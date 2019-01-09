/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2015 Jean-Pierre Charras, jp.charras at wanadoo.fr
 * Copyright (C) 2016-2018 KiCad Developers, see AUTHORS.txt for contributors.
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

/* Set up colors to draw items in Eeschema
 */

#include <fctsys.h>
#include <draw_frame.h>
#include <sch_draw_panel.h>
#include <sch_view.h>
#include <general.h>

#include "widget_eeschema_color_config.h"
#include <dialogs/dialog_color_picker.h>

#include <wx/filefn.h>

//The names of the color scheme configuration files
#define COLOR_SCHEME_FILE_NAME      wxT( "eeschema.colors" )
#define COLOR_SCHEME_TEMP_FILE_NAME wxT( "~eeschema.colors" )

// Width and height of every (color-displaying / bitmap) button in dialog units
const wxSize BUTT_SIZE( 10, 6 );
const wxSize BUTT_BORDER( 4, 4 );

/********************/
/* Layer menu list. */
/********************/

struct COLORBUTTON
{
    wxString        m_Name;
    wxString        m_ConfigName;
    int             m_Layer;
};

struct BUTTONINDEX
{
    wxString        m_Name;
    COLORBUTTON*    m_Buttons;
};

static COLORBUTTON generalColorButtons[] = {
    { _( "Wire" ),               wxT( "WIRE" ),             LAYER_WIRE },
    { _( "Bus" ),                wxT( "BUS" ),              LAYER_BUS },
    { _( "Junction" ),           wxT( "JUNCTION" ),         LAYER_JUNCTION },
    { _( "Label" ),              wxT( "LOCLABEL" ),         LAYER_LOCLABEL },
    { _( "Global label" ),       wxT( "GLOBLABEL" ),        LAYER_GLOBLABEL },
    { _( "Net name" ),           wxT( "NETNAM" ),           LAYER_NETNAM },
    { _( "Notes" ),              wxT( "NOTES" ),            LAYER_NOTES },
    { _( "No connect symbol" ),  wxT( "NOCONNECT" ),        LAYER_NOCONNECT },
    { wxT( "" ), wxT( "" ), -1 }                           // Sentinel marking end of list.
};

static COLORBUTTON componentColorButtons[] = {
    { _( "Body outline" ),       wxT( "DEVICE" ),           LAYER_DEVICE },
    { _( "Body background" ),    wxT( "DEVICE_BG" ),        LAYER_DEVICE_BACKGROUND },
    { _( "Pin" ),                wxT( "PIN" ),              LAYER_PIN },
    { _( "Pin number" ),         wxT( "PINNUM" ),           LAYER_PINNUM },
    { _( "Pin name" ),           wxT( "PINNAM" ),           LAYER_PINNAM },
    { _( "Reference" ),          wxT( "REFERENCEPART" ),    LAYER_REFERENCEPART },
    { _( "Value" ),              wxT( "VALUEPART" ),        LAYER_VALUEPART },
    { _( "Fields" ),             wxT( "FIELDS" ),           LAYER_FIELDS },
    { wxT( "" ), wxT( "" ), -1 }                           // Sentinel marking end of list.
};

static COLORBUTTON sheetColorButtons[] = {
    { _( "Sheet" ),              wxT( "SHEET" ),            LAYER_SHEET },
    { _( "Sheet file name" ),    wxT( "SHEETFILENAME" ),    LAYER_SHEETFILENAME },
    { _( "Sheet name" ),         wxT( "SHEETNAME" ),        LAYER_SHEETNAME },
    { _( "Sheet label" ),        wxT( "SHEETLABEL" ),       LAYER_SHEETLABEL },
    { _( "Hierarchical label" ), wxT( "HIERLABEL" ),        LAYER_HIERLABEL },
    { wxT( "" ), wxT( "" ), -1 }                           // Sentinel marking end of list.
};

static COLORBUTTON miscColorButtons[] = {
    { _( "ERC warning" ),        wxT( "ERC_WARN" ),         LAYER_ERC_WARN },
    { _( "ERC error" ),          wxT( "ERC_ERR" ),          LAYER_ERC_ERR },
    { _( "Brightened" ),         wxT( "BRIGHTENED" ),       LAYER_BRIGHTENED },
    { _( "Hidden items" ),       wxT( "HIDDEN" ),           LAYER_HIDDEN },
    { _( "Worksheet" ),          wxT( "WORKSHEET" ),        LAYER_WORKSHEET },
    { _( "Cursor" ),             wxT( "SCHEMATIC_CURSOR" ), LAYER_SCHEMATIC_CURSOR },
    { _( "Grid" ),               wxT( "SCHEMATIC_GRID" ),   LAYER_SCHEMATIC_GRID },
    { _( "Background" ),         wxT( "SCHEMATIC_BG" ),     LAYER_SCHEMATIC_BACKGROUND },
    { wxT( "" ), wxT( "" ), -1 }                           // Sentinel marking end of list.
};


static BUTTONINDEX buttonGroups[] = {
    { _( "General" ),           generalColorButtons },
    { _( "Component" ),         componentColorButtons },
    { _( "Sheet" ),             sheetColorButtons },
    { _( "Miscellaneous" ),     miscColorButtons },
    { wxT( "" ), NULL }
};

static COLORBUTTON bgColorButton = { "", "", LAYER_SCHEMATIC_BACKGROUND };

static COLOR4D currentColors[ LAYER_ID_COUNT ];


WIDGET_EESCHEMA_COLOR_CONFIG::WIDGET_EESCHEMA_COLOR_CONFIG( wxWindow* aParent, EDA_DRAW_FRAME* aDrawFrame ) :
    wxPanel( aParent ), m_drawFrame( aDrawFrame )
{
    m_butt_size_pix = ConvertDialogToPixels( BUTT_SIZE );
    m_butt_border_pix = ConvertDialogToPixels( BUTT_BORDER );

    m_mainBoxSizer = new wxBoxSizer( wxVERTICAL );
    SetSizer( m_mainBoxSizer );

    m_colorSchemeBoxSizer = CreateColorSchemeList();
    m_mainBoxSizer->Add( m_colorSchemeBoxSizer, 0, wxEXPAND, 10 );
    m_controlsBoxSizer = CreateControls();
    m_mainBoxSizer->Add( m_controlsBoxSizer, 1, wxEXPAND, 5 );
}


wxBoxSizer* WIDGET_EESCHEMA_COLOR_CONFIG::CreateControls()
{
    wxStaticText*   label;
    int             buttonId = 1800;
    wxBoxSizer*     retBoxSizer = new wxBoxSizer( wxHORIZONTAL );

    BUTTONINDEX* groups = buttonGroups;
    wxBoxSizer* columnBoxSizer = NULL;

    while( groups->m_Buttons != NULL )
    {
        COLORBUTTON* buttons = groups->m_Buttons;

        columnBoxSizer = new wxBoxSizer( wxVERTICAL );
        retBoxSizer->Add( columnBoxSizer, 1, wxALIGN_TOP | wxLEFT, 5 );
        wxBoxSizer* rowBoxSizer = new wxBoxSizer( wxHORIZONTAL );
        columnBoxSizer->Add( rowBoxSizer, 0, wxGROW | wxLEFT | wxRIGHT | wxBOTTOM, 5 );

        // Add a text string to identify the column of color select buttons.
        label = new wxStaticText( this, wxID_ANY, groups->m_Name );

        // Make the column label font bold.
        wxFont font( label->GetFont() );
        font.SetWeight( wxFONTWEIGHT_BOLD );
        label->SetFont( font );

        rowBoxSizer->Add( label, 1, wxALIGN_CENTER_VERTICAL | wxALL, 5 );

        while( buttons->m_Layer >= 0 )
        {
            rowBoxSizer = new wxBoxSizer( wxHORIZONTAL );
            columnBoxSizer->Add( rowBoxSizer, 0, wxGROW | wxALL, 0 );

            COLOR4D color = GetLayerColor( SCH_LAYER_ID( buttons->m_Layer ) );
            currentColors[ buttons->m_Layer ] = color;

            wxMemoryDC iconDC;
            wxBitmap   bitmap( m_butt_size_pix );

            iconDC.SelectObject( bitmap );
            iconDC.SetPen( *wxBLACK_PEN );

            wxBrush brush;
            brush.SetColour( color.ToColour() );
            brush.SetStyle( wxBRUSHSTYLE_SOLID );
            iconDC.SetBrush( brush );
            iconDC.DrawRectangle( 0, 0, m_butt_size_pix.x, m_butt_size_pix.y );

            wxBitmapButton* bitmapButton = new wxBitmapButton(
                                    this, buttonId, bitmap, wxDefaultPosition,
                                    m_butt_size_pix + m_butt_border_pix + wxSize( 1, 1 ) );
            bitmapButton->SetClientData( (void*) buttons );

            rowBoxSizer->Add( bitmapButton, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT | wxBOTTOM, 5 );

            label = new wxStaticText( this, wxID_ANY, wxGetTranslation( buttons->m_Name ) );
            rowBoxSizer->Add( label, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT | wxBOTTOM, 5 );
            buttonId += 1;
            buttons++;
        }

        groups++;
    }

    Connect( 1800, buttonId, wxEVT_COMMAND_BUTTON_CLICKED,
            wxCommandEventHandler( WIDGET_EESCHEMA_COLOR_CONFIG::SetColor ) );

    // Dialog now needs to be resized, but the associated command is found elsewhere.

    return retBoxSizer;
}


void WIDGET_EESCHEMA_COLOR_CONFIG::SetColor( wxCommandEvent& event )
{
    wxBitmapButton* button = (wxBitmapButton*) event.GetEventObject();

    wxCHECK_RET( button != NULL, wxT( "Color button event object is NULL." ) );

    COLORBUTTON* colorButton = (COLORBUTTON*) button->GetClientData();

    wxCHECK_RET( colorButton != NULL, wxT( "Client data not set for color button." ) );
    COLOR4D oldColor = currentColors[ colorButton->m_Layer ];
    COLOR4D newColor = COLOR4D::UNSPECIFIED;
    DIALOG_COLOR_PICKER dialog( this, oldColor, false );

    if( dialog.ShowModal() == wxID_OK )
    {
        newColor = dialog.GetColor();
    }

    if( newColor == COLOR4D::UNSPECIFIED || oldColor == newColor )
        return;

    currentColors[ colorButton->m_Layer ] = newColor;

    wxMemoryDC iconDC;

    wxBitmap bitmap = button->GetBitmapLabel();
    iconDC.SelectObject( bitmap );
    iconDC.SetPen( *wxBLACK_PEN );

    wxBrush  brush;
    brush.SetColour( newColor.ToColour() );
    brush.SetStyle( wxBRUSHSTYLE_SOLID );

    iconDC.SetBrush( brush );
    iconDC.DrawRectangle( 0, 0, m_butt_size_pix.x, m_butt_size_pix.y );
    button->SetBitmapLabel( bitmap );
    button->Refresh();

    Refresh( false );

    wxString strNewColor = newColor.ToWxString( wxC2S_NAME | wxC2S_CSS_SYNTAX );
    ChangeColorShemaInTempFile( colorButton->m_ConfigName, strNewColor );
}


bool WIDGET_EESCHEMA_COLOR_CONFIG::TransferDataFromControl()
{
    // Check for color conflicts with background color to give user a chance to bail
    // out before making changes.

    COLOR4D bgcolor = currentColors[ LAYER_SCHEMATIC_BACKGROUND ];

    for( SCH_LAYER_ID clyr = SCH_LAYER_ID_START; clyr < SCH_LAYER_ID_END; ++clyr )
    {
        if( bgcolor == currentColors[ clyr ] && clyr != LAYER_SCHEMATIC_BACKGROUND )
        {
            wxString msg = _( "Some items have the same color as the background\n"
                              "and they will not be seen on the screen.  Are you\n"
                              "sure you want to use these colors?" );

            if( wxMessageBox( msg,  _( "Warning" ), wxYES_NO | wxICON_QUESTION, this ) == wxNO )
                return false;

            break;
        }
    }

    for( SCH_LAYER_ID clyr = SCH_LAYER_ID_START; clyr < SCH_LAYER_ID_END; ++clyr )
        SetLayerColor( currentColors[ clyr ], clyr );

    SetLayerColor( currentColors[ LAYER_WORKSHEET ], (SCH_LAYER_ID) LAYER_WORKSHEET );

    m_colorSchemeConfigFile->Flush();
    SaveColorSchemeChangesToFile();

    return true;
}


wxBoxSizer* WIDGET_EESCHEMA_COLOR_CONFIG::CreateColorSchemeList()
{
    wxString        configPath = wxEmptyString;
    wxArrayString   arrayStringChoices;
    wxBoxSizer*     retBoxSizer = new wxBoxSizer( wxHORIZONTAL );
    wxString        currentColorScheme = wxEmptyString;
    int             selection;

    if ( InitColorSchemeFile() )
    {
        GetColorSchemeListFromFile( arrayStringChoices );

        currentColorScheme = GetCurrentColorSchemeFromFile();

        m_labelColorScheme = new wxStaticText(
                this, wxID_ANY, _( "Color scheme: " ), wxDefaultPosition, wxDefaultSize, 0 );
        m_labelColorScheme->Wrap( -1 );
        retBoxSizer->Add( m_labelColorScheme, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5 );

        m_choiceColorScheme =
                new wxChoice( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, arrayStringChoices, 0 );
        selection = m_choiceColorScheme->FindString(currentColorScheme, true);
        m_choiceColorScheme->SetSelection( selection );
        retBoxSizer->Add( m_choiceColorScheme, 1, wxALL | wxALIGN_CENTER_VERTICAL, 5 );

        m_buttonCoppyColorScheme =
                new wxButton( this, wxID_ANY, _( "Copy..." ), wxDefaultPosition, wxDefaultSize, 0 );
        retBoxSizer->Add( m_buttonCoppyColorScheme, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5 );

        m_buttonDeleteColorScheme =
                new wxButton( this, wxID_ANY, _( "Delete" ), wxDefaultPosition, wxDefaultSize, 0 );
        retBoxSizer->Add( m_buttonDeleteColorScheme, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5 );

        m_choiceColorScheme->Connect( wxEVT_COMMAND_CHOICE_SELECTED,
                wxCommandEventHandler( WIDGET_EESCHEMA_COLOR_CONFIG::OnChoice ), NULL, this );
        m_buttonCoppyColorScheme->Connect( wxEVT_COMMAND_BUTTON_CLICKED,
                wxCommandEventHandler( WIDGET_EESCHEMA_COLOR_CONFIG::OnButtonCopyClick ), NULL, this );
        m_buttonDeleteColorScheme->Connect( wxEVT_COMMAND_BUTTON_CLICKED,
                wxCommandEventHandler( WIDGET_EESCHEMA_COLOR_CONFIG::OnButtonDeleteClick ), NULL,
                this );
    }

    return retBoxSizer;
}


void WIDGET_EESCHEMA_COLOR_CONFIG::OnChoice( wxCommandEvent& event )
{
    wxChoice* choiceList = static_cast<wxChoice*>( event.GetEventObject() );
    wxString  scheme =
            choiceList->GetString( static_cast<unsigned int>( choiceList->GetCurrentSelection() ) );

    SetCurrentColorSchemeInTempFile( scheme );
    GetColorsFromTempFile( scheme );
}


void WIDGET_EESCHEMA_COLOR_CONFIG::OnButtonCopyClick( wxCommandEvent& event )
{
    wxString msg = _( "Please enter a new name for the copied color scheme" );
    wxString textFromUser = wxEmptyString;
    wxString proposedName = wxEmptyString;
    int      index;
    bool     repeat = true;

    index = m_choiceColorScheme->GetSelection();

    if( index >= 0 )
    {
        proposedName = m_choiceColorScheme->GetString( static_cast<unsigned int>( index ) )
                       + wxT( " (copy)" );
    }

    while( repeat )
    {
        repeat = false;

        textFromUser = wxGetTextFromUser( msg, _( "Copy Color Scheme" ), proposedName, this );

        if( textFromUser == wxEmptyString )
        {
            return;
        }
        else
        {
            for( unsigned int i = 0; i < m_choiceColorScheme->GetCount(); ++i )
            {
                if( m_choiceColorScheme->GetString( i ) == textFromUser )
                {
                    wxMessageBox( _( "The name you entered exists. Pleas enter another one." ) );
                    repeat = true;
                }
            }
        }
    }

    CopyColorSchemeInTempFile( textFromUser );
    SetCurrentColorSchemeInTempFile( textFromUser );

    m_choiceColorScheme->Insert( textFromUser, ( m_choiceColorScheme->GetCount() ) );
    m_choiceColorScheme->SetSelection( static_cast<int>( m_choiceColorScheme->GetCount() - 1 ) );

    Refresh( false );

    event.Skip();
}


void WIDGET_EESCHEMA_COLOR_CONFIG::OnButtonDeleteClick( wxCommandEvent& event )
{
    int selection = m_choiceColorScheme->GetSelection();

    if( selection < 0 )
    {
        wxMessageBox( _( "Please chose color scheme you want to delete" ) );
    }
    else
    {
        wxString deletedScheme =
                m_choiceColorScheme->GetString( static_cast<unsigned int>( selection ) );
        wxString msg = _( "Are you sure you want to delete this color scheme?" );

        if( wxMessageBox( msg, _( "Delete Color Scheme" ), wxYES_NO | wxICON_QUESTION, this )
                == wxYES )
        {
            m_choiceColorScheme->Delete( static_cast<unsigned int>( selection ) );
            m_choiceColorScheme->SetSelection( 0 );

            GetColorsFromTempFile( m_choiceColorScheme->GetString( 0 ) );

            DeleteColorSchemeInTempFile( deletedScheme );

            Refresh( false );
        }
    }

    event.Skip();
}


bool WIDGET_EESCHEMA_COLOR_CONFIG::InitColorSchemeFile( void )
{
    wxFileName fileName;

    fileName.AssignDir( GetKicadConfigPath() );
    fileName.SetFullName( COLOR_SCHEME_FILE_NAME );

    wxString filePath = fileName.GetFullPath();

    if( wxFileExists( filePath ) == false )
    {
        if( CreateColorSchemeFile( fileName.GetFullPath() ) == false )
        {
            return false;
        }
    }

    //Create temporary color scheme config file
    fileName.SetFullName( COLOR_SCHEME_TEMP_FILE_NAME );
    wxString tempFilePath = fileName.GetFullPath();

    if (wxCopyFile( filePath, tempFilePath ) == false)
    {
        return false;
    }

    m_colorSchemeConfigFile = std::make_unique<wxFileConfig>( wxEmptyString, wxEmptyString,
            tempFilePath, wxEmptyString, wxCONFIG_USE_LOCAL_FILE | wxCONFIG_USE_RELATIVE_PATH );

    return true;
}


bool WIDGET_EESCHEMA_COLOR_CONFIG::GetColorsFromTempFile( const wxString& aScheme )
{
    int           buttonId = 1800;
    BUTTONINDEX*  groups = buttonGroups;
    wxString      configGroup = '/' + aScheme;


    if( m_colorSchemeConfigFile->HasGroup( configGroup ) == false )
    {
        return false;
    }

    m_colorSchemeConfigFile->SetPath( '/' );
    m_colorSchemeConfigFile->Write( wxT( "current" ), aScheme );
    m_colorSchemeConfigFile->SetPath( configGroup );

    while( groups->m_Buttons != NULL )
    {
        COLORBUTTON* buttons = groups->m_Buttons;

        while( buttons->m_Layer >= 0 )
        {
            COLOR4D  newColor;
            wxString colorStr = wxEmptyString;

            if( m_colorSchemeConfigFile->Read( buttons->m_ConfigName, &colorStr ) )
            {
                newColor.SetFromWxString( colorStr );
            }

            //            COLOR4D color = GetLayerColor( SCH_LAYER_ID( buttons->m_Layer ) );

            if( currentColors[buttons->m_Layer] != newColor )
            {
                wxBitmapButton* bitmapButton =
                        static_cast<wxBitmapButton*>( FindWindowById( buttonId ) );

                wxMemoryDC iconDC;
                wxBitmap   bitmap = bitmapButton->GetBitmapLabel();

                iconDC.SelectObject( bitmap );
                iconDC.SetPen( *wxBLACK_PEN );

                wxBrush brush;

                brush.SetColour( newColor.ToColour() );
                brush.SetStyle( wxBRUSHSTYLE_SOLID );
                iconDC.SetBrush( brush );
                iconDC.DrawRectangle( 0, 0, m_butt_size_pix.x, m_butt_size_pix.y );

                bitmapButton->SetBitmapLabel( bitmap );
                bitmapButton->Refresh();

                currentColors[buttons->m_Layer] = newColor;
            }

            buttonId++;
            buttons++;
        }

        groups++;
    }

    Refresh( false );

    return true;
}


wxString WIDGET_EESCHEMA_COLOR_CONFIG::GetCurrentColorSchemeFromFile( void )
{
    wxString retScheme = wxEmptyString;

    m_colorSchemeConfigFile->SetPath( '/' );
    m_colorSchemeConfigFile->Read( wxT( "current" ), &retScheme );

    return retScheme;
}


bool WIDGET_EESCHEMA_COLOR_CONFIG::SetCurrentColorSchemeInTempFile( const wxString& aScheme )
{
    m_colorSchemeConfigFile->SetPath( '/' );

    return m_colorSchemeConfigFile->Write( wxT( "current" ), aScheme );
}


bool WIDGET_EESCHEMA_COLOR_CONFIG::ChangeColorShemaInTempFile(
        const wxString& aKey, const wxString& aValue )
{
    int           index;
    wxString      selection;

    index = m_choiceColorScheme->GetSelection();

    if( index < 0 )
    {
        return false;
    }

    selection = m_choiceColorScheme->GetString( static_cast<unsigned int>( index ) );
    m_colorSchemeConfigFile->SetPath( '/' + selection );

    return m_colorSchemeConfigFile->Write( aKey, aValue );
}


bool WIDGET_EESCHEMA_COLOR_CONFIG::CopyColorSchemeInTempFile( const wxString& aNewScheme )
{
    m_colorSchemeConfigFile->SetPath( '/' + aNewScheme );

    BUTTONINDEX* groups = buttonGroups;

    while( groups->m_Buttons != NULL )
    {
        COLORBUTTON* buttons = groups->m_Buttons;

        while( buttons->m_Layer >= 0 )
        {
            wxString rgbString =
                    currentColors[buttons->m_Layer].ToWxString( wxC2S_NAME | wxC2S_CSS_SYNTAX );

            m_colorSchemeConfigFile->Write( buttons->m_ConfigName, rgbString );

            buttons++;
        }

        groups++;
    }

    return true; //TODO void??
}


bool WIDGET_EESCHEMA_COLOR_CONFIG::DeleteColorSchemeInTempFile( const wxString& aDeletedScheme )
{
    wxString      group = wxEmptyString;

    group = '/' + aDeletedScheme;

    if( m_colorSchemeConfigFile->HasGroup( group ) == false )
    {
        return false;
    }

    m_colorSchemeConfigFile->DeleteGroup( group );

    return true;
}


bool WIDGET_EESCHEMA_COLOR_CONFIG::CreateColorSchemeFile(const wxString& aFilePath)
{
    std::unique_ptr<wxFileConfig> configFile =
            std::make_unique<wxFileConfig>( wxEmptyString, wxEmptyString, aFilePath, wxEmptyString,
                    wxCONFIG_USE_LOCAL_FILE | wxCONFIG_USE_RELATIVE_PATH );

    BUTTONINDEX* groups = buttonGroups;
    configFile->Write( wxT( "current" ), wxT( "Default" ) );
    configFile->SetPath( wxT( "/Default" ) );

    while( groups->m_Buttons != NULL )
    {
        COLORBUTTON* buttons = groups->m_Buttons;

        while( buttons->m_Layer >= 0 )
        {
            wxString rgbString = GetLayerColor( SCH_LAYER_ID( buttons->m_Layer ) )
                                         .ToWxString( wxC2S_NAME | wxC2S_CSS_SYNTAX );

            configFile->Write( buttons->m_ConfigName, rgbString );

            buttons++;
        }

        groups++;
    }

    return true; //TODO void??
}


bool WIDGET_EESCHEMA_COLOR_CONFIG::GetColorSchemeListFromFile( wxArrayString& aColorSchemeList )
{
    wxString      group;
    long          index = 0;

    if( m_colorSchemeConfigFile->GetFirstGroup( group, index ) )
    {
        aColorSchemeList.Add( group );

        while( m_colorSchemeConfigFile->GetNextGroup( group, index ) )
        {
            aColorSchemeList.Add( group );
        }
    }
    else
    {
        return false;
    }

    return true;
}


bool WIDGET_EESCHEMA_COLOR_CONFIG::SaveColorSchemeChangesToFile()
{
    bool        retVal = false;
    wxString    configPath = wxEmptyString;
    wxFileName  fileName;

    fileName.AssignDir( GetKicadConfigPath() );
    fileName.SetFullName( COLOR_SCHEME_FILE_NAME );

    wxString filePath = fileName.GetFullPath();

    fileName.SetFullName( COLOR_SCHEME_TEMP_FILE_NAME );
    wxString tempFilePath = fileName.GetFullPath();


    retVal = wxCopyFile(tempFilePath, filePath);
    //wxRemoveFile(m_colorSchemeTempFile);

    return retVal;
}


PANEL_EESCHEMA_COLOR_CONFIG::PANEL_EESCHEMA_COLOR_CONFIG( EDA_DRAW_FRAME* aFrame,
                                                          wxWindow* aParent ) :
    wxPanel( aParent )
{
    auto sizer = new wxBoxSizer( wxVERTICAL );
    SetSizer( sizer );

    m_colorConfig = new WIDGET_EESCHEMA_COLOR_CONFIG( this, aFrame );
    sizer->Add( m_colorConfig, 1, wxEXPAND | wxLEFT | wxRIGHT, 5 );
}


bool PANEL_EESCHEMA_COLOR_CONFIG::TransferDataToWindow()
{
    return true;
}


bool PANEL_EESCHEMA_COLOR_CONFIG::TransferDataFromWindow()
{
    return m_colorConfig->TransferDataFromControl();
}
