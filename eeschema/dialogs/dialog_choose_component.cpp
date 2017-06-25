/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2014 Henner Zeller <h.zeller@acm.org>
 * Copyright (C) 2017 Chris Pavlina <pavlina.chris@gmail.com>
 * Copyright (C) 2016-2017 KiCad Developers, see AUTHORS.txt for contributors.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you may find one here:
 * http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
 * or you may search the http://www.gnu.org website for the version 2 license,
 * or you may write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#include <dialog_choose_component.h>

#include <algorithm>
#include <set>
#include <wx/utils.h>

#include <wx/button.h>
#include <wx/dataview.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/splitter.h>
#include <wx/timer.h>
#include <wx/utils.h>

#include <class_library.h>
#include <sch_base_frame.h>
#include <template_fieldnames.h>
#include <widgets/component_tree.h>
#include <widgets/footprint_preview_widget.h>
#include <widgets/footprint_select_widget.h>


FOOTPRINT_ASYNC_LOADER          DIALOG_CHOOSE_COMPONENT::m_fp_loader;
std::unique_ptr<FOOTPRINT_LIST> DIALOG_CHOOSE_COMPONENT::m_fp_list;

DIALOG_CHOOSE_COMPONENT::DIALOG_CHOOSE_COMPONENT( SCH_BASE_FRAME* aParent, const wxString& aTitle,
        CMP_TREE_MODEL_ADAPTER::PTR& aAdapter, int aDeMorganConvert, bool aAllowFieldEdits )
        : DIALOG_SHIM( aParent, wxID_ANY, aTitle, wxDefaultPosition, wxSize( 800, 650 ),
                  wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER ),
          m_parent( aParent ),
          m_deMorganConvert( aDeMorganConvert >= 0 ? aDeMorganConvert : 0 ),
          m_allow_field_edits( aAllowFieldEdits ),
          m_external_browser_requested( false )
{
    wxBusyCursor busy_while_loading;

    auto sizer = new wxBoxSizer( wxVERTICAL );

    auto splitter = new wxSplitterWindow(
            this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxSP_LIVE_UPDATE );
    m_tree = new COMPONENT_TREE( splitter, aAdapter );
    auto right_panel = ConstructRightPanel( splitter );
    auto buttons = new wxStdDialogButtonSizer();
    m_dbl_click_timer = new wxTimer( this );

    splitter->SetSashGravity( 0.9 );
    splitter->SetMinimumPaneSize( 1 );
    splitter->SplitVertically( m_tree, right_panel, -300 );

    buttons->AddButton( new wxButton( this, wxID_OK ) );
    buttons->AddButton( new wxButton( this, wxID_CANCEL ) );
    buttons->Realize();

    sizer->Add( splitter, 1, wxEXPAND | wxALL, 5 );
    sizer->Add( buttons, 0, wxEXPAND | wxBOTTOM, 10 );
    SetSizer( sizer );

    Bind( wxEVT_INIT_DIALOG, &DIALOG_CHOOSE_COMPONENT::OnInitDialog, this );
    Bind( wxEVT_TIMER, &DIALOG_CHOOSE_COMPONENT::OnCloseTimer, this, m_dbl_click_timer->GetId() );
    Bind( COMPONENT_PRESELECTED, &DIALOG_CHOOSE_COMPONENT::OnComponentPreselected, this );
    Bind( COMPONENT_SELECTED, &DIALOG_CHOOSE_COMPONENT::OnComponentSelected, this );

    m_sch_view_ctrl->Bind( wxEVT_LEFT_DCLICK, &DIALOG_CHOOSE_COMPONENT::OnSchViewDClick, this );
    m_sch_view_ctrl->Bind( wxEVT_PAINT, &DIALOG_CHOOSE_COMPONENT::OnSchViewPaint, this );

    if( m_fp_sel_ctrl )
        m_fp_sel_ctrl->Bind(
                EVT_FOOTPRINT_SELECTED, &DIALOG_CHOOSE_COMPONENT::OnFootprintSelected, this );

    Layout();
}


DIALOG_CHOOSE_COMPONENT::~DIALOG_CHOOSE_COMPONENT()
{
    // I am not sure the following two lines are necessary,
    // but they will not hurt anyone
    m_dbl_click_timer->Stop();
    Unbind( wxEVT_TIMER, &DIALOG_CHOOSE_COMPONENT::OnCloseTimer, this );

    delete m_dbl_click_timer;
}


wxPanel* DIALOG_CHOOSE_COMPONENT::ConstructRightPanel( wxWindow* aParent )
{
    auto panel = new wxPanel( aParent );
    auto sizer = new wxBoxSizer( wxVERTICAL );

    m_sch_view_ctrl = new wxPanel( panel, wxID_ANY, wxDefaultPosition, wxSize( -1, -1 ),
            wxFULL_REPAINT_ON_RESIZE | wxSUNKEN_BORDER | wxTAB_TRAVERSAL );
    m_sch_view_ctrl->SetLayoutDirection( wxLayout_LeftToRight );

    if( m_allow_field_edits )
        m_fp_sel_ctrl = new FOOTPRINT_SELECT_WIDGET( panel, m_fp_loader, m_fp_list, true );
    else
        m_fp_sel_ctrl = nullptr;

    m_fp_view_ctrl = new FOOTPRINT_PREVIEW_WIDGET( panel, Kiway() );


    sizer->Add( m_sch_view_ctrl, 1, wxEXPAND | wxALL, 5 );

    if( m_fp_sel_ctrl )
        sizer->Add( m_fp_sel_ctrl, 0, wxEXPAND | wxALL, 5 );

    sizer->Add( m_fp_view_ctrl, 1, wxEXPAND | wxALL, 5 );


    panel->SetSizer( sizer );
    panel->Layout();
    sizer->Fit( panel );

    return panel;
}


void DIALOG_CHOOSE_COMPONENT::OnInitDialog( wxInitDialogEvent& aEvent )
{
    if( m_fp_view_ctrl->IsInitialized() )
    {
        // This hides the GAL panel and shows the status label
        m_fp_view_ctrl->SetStatusText( wxEmptyString );
    }

    if( m_fp_sel_ctrl )
        m_fp_sel_ctrl->Load( Kiway(), Prj() );
}


LIB_ALIAS* DIALOG_CHOOSE_COMPONENT::GetSelectedAlias( int* aUnit ) const
{
    return m_tree->GetSelectedAlias( aUnit );
}


void DIALOG_CHOOSE_COMPONENT::OnCloseTimer( wxTimerEvent& aEvent )
{
    // Hack handler because of eaten MouseUp event. See
    // DIALOG_CHOOSE_COMPONENT::OnDoubleClickTreeActivation for the beginning
    // of this spaghetti noodle.

    auto state = wxGetMouseState();

    if( state.LeftIsDown() )
    {
        // Mouse hasn't been raised yet, so fire the timer again. Otherwise the
        // purpose of this timer is defeated.
        m_dbl_click_timer->StartOnce( DIALOG_CHOOSE_COMPONENT::DblClickDelay );
    }
    else
    {
        EndQuasiModal( wxID_OK );
    }
}


void DIALOG_CHOOSE_COMPONENT::OnSchViewDClick( wxMouseEvent& aEvent )
{
    m_external_browser_requested = true;
    EndQuasiModal( wxID_OK );
}


void DIALOG_CHOOSE_COMPONENT::ShowFootprintFor( LIB_ALIAS* aAlias )
{
    if( !m_fp_view_ctrl->IsInitialized() )
        return;

    LIB_FIELD* fp_field = aAlias->GetPart()->GetField( FOOTPRINT );
    wxString   fp_name = fp_field ? fp_field->GetFullText() : wxString( "" );

    ShowFootprint( fp_name );
}


void DIALOG_CHOOSE_COMPONENT::ShowFootprint( wxString const& aName )
{
    if( aName == wxEmptyString )
    {
        m_fp_view_ctrl->SetStatusText( _( "No footprint specified" ) );
    }
    else
    {
        LIB_ID lib_id( aName );

        m_fp_view_ctrl->ClearStatus();
        m_fp_view_ctrl->CacheFootprint( lib_id );
        m_fp_view_ctrl->DisplayFootprint( lib_id );
    }
}


void DIALOG_CHOOSE_COMPONENT::PopulateFootprintSelector( LIB_ALIAS* aAlias )
{
    if( !m_fp_sel_ctrl )
        return;

    m_fp_sel_ctrl->ClearFilters();

    if( aAlias )
    {
        LIB_PINS   temp_pins;
        LIB_FIELD* fp_field = aAlias->GetPart()->GetField( FOOTPRINT );
        wxString   fp_name = fp_field ? fp_field->GetFullText() : wxString( "" );

        aAlias->GetPart()->GetPins( temp_pins );

        m_fp_sel_ctrl->FilterByPinCount( temp_pins.size() );
        m_fp_sel_ctrl->FilterByFootprintFilters( aAlias->GetPart()->GetFootPrints(), true );
        m_fp_sel_ctrl->SetDefaultFootprint( fp_name );
        m_fp_sel_ctrl->UpdateList();
        m_fp_sel_ctrl->Enable();
    }
    else
    {
        m_fp_sel_ctrl->UpdateList();
        m_fp_sel_ctrl->Disable();
    }
}


void DIALOG_CHOOSE_COMPONENT::OnSchViewPaint( wxPaintEvent& aEvent )
{
    int unit = 0;
    LIB_ALIAS* alias = m_tree->GetSelectedAlias( &unit );
    LIB_PART*  part = alias ? alias->GetPart() : nullptr;

    // Don't draw anything (not even the background) if we don't have
    // a part to show
    if( !part )
        return;

    if( alias->IsRoot() )
    {
        // just show the part directly
        RenderPreview( part, unit );
    }
    else
    {
        // switch out the name temporarily for the alias name
        wxString tmp( part->GetName() );
        part->SetName( alias->GetName() );

        RenderPreview( part, unit );

        part->SetName( tmp );
    }
}


void DIALOG_CHOOSE_COMPONENT::OnFootprintSelected( wxCommandEvent& aEvent )
{
    m_fp_override = aEvent.GetString();

    m_field_edits.erase(
            std::remove_if( m_field_edits.begin(), m_field_edits.end(),
                    []( std::pair<int, wxString> const& i ) { return i.first == FOOTPRINT; } ),
            m_field_edits.end() );

    m_field_edits.push_back( std::make_pair( FOOTPRINT, m_fp_override ) );

    ShowFootprint( m_fp_override );
}


void DIALOG_CHOOSE_COMPONENT::OnComponentPreselected( wxCommandEvent& aEvent )
{
    int unit = 0;
    LIB_ALIAS* alias = m_tree->GetSelectedAlias( &unit );

    m_sch_view_ctrl->Refresh();

    if( alias )
    {
        ShowFootprintFor( alias );
        PopulateFootprintSelector( alias );
    }
    else
    {
        if( m_fp_view_ctrl->IsInitialized() )
            m_fp_view_ctrl->SetStatusText( wxEmptyString );

        PopulateFootprintSelector( nullptr );
    }
}


void DIALOG_CHOOSE_COMPONENT::OnComponentSelected( wxCommandEvent& aEvent )
{
    if( m_tree->GetSelectedAlias() )
    {
        // Got a selection. We can't just end the modal dialog here, because
        // wx leaks some events back to the parent window (in particular, the
        // MouseUp following a double click).
        //
        // NOW, here's where it gets really fun. wxTreeListCtrl eats MouseUp.
        // This isn't really feasible to bypass without a fully custom
        // wxDataViewCtrl implementation, and even then might not be fully
        // possible (docs are vague). To get around this, we use a one-shot
        // timer to schedule the dialog close.
        //
        // See DIALOG_CHOOSE_COMPONENT::OnCloseTimer for the other end of this
        // spaghetti noodle.
        m_dbl_click_timer->StartOnce( DIALOG_CHOOSE_COMPONENT::DblClickDelay );
    }
}


void DIALOG_CHOOSE_COMPONENT::RenderPreview( LIB_PART* aComponent, int aUnit )
{
    wxPaintDC dc( m_sch_view_ctrl );

    const wxSize dc_size = dc.GetSize();

    // Avoid rendering when either dimension is zero
    if( dc_size.x == 0 || dc_size.y == 0 )
        return;

    GRResetPenAndBrush( &dc );

    COLOR4D bgColor = m_parent->GetDrawBgColor();

    dc.SetBackground( wxBrush( bgColor.ToColour() ) );
    dc.Clear();

    if( !aComponent )
        return;

    if( aUnit <= 0 )
        aUnit = 1;

    dc.SetDeviceOrigin( dc_size.x / 2, dc_size.y / 2 );

    // Find joint bounding box for everything we are about to draw.
    EDA_RECT     bBox = aComponent->GetUnitBoundingBox( aUnit, m_deMorganConvert );
    const double xscale = (double) dc_size.x / bBox.GetWidth();
    const double yscale = (double) dc_size.y / bBox.GetHeight();
    const double scale = std::min( xscale, yscale ) * 0.85;

    dc.SetUserScale( scale, scale );

    wxPoint offset = -bBox.Centre();

    auto opts = PART_DRAW_OPTIONS::Default();
    opts.draw_hidden_fields = false;
    aComponent->Draw( nullptr, &dc, offset, aUnit, m_deMorganConvert, opts );
}
