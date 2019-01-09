/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2007 G. Harland
 * Copyright (C) 1992-2018 KiCad Developers, see AUTHORS.txt for contributors.
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

#ifndef WIDGET_EESCHEMA_COLOR_CONFIG_H_
#define WIDGET_EESCHEMA_COLOR_CONFIG_H_

#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/colordlg.h>
#include <wx/clrpicker.h>

#include <wx/choice.h>
#include <wx/stattext.h>
#include <wx/fileconf.h>
#include <memory>

class wxBoxSizer;
class wxStaticLine;
class wxStdDialogButtonSizer;


/***********************************************/
/* Derived class for the frame color settings. */
/***********************************************/

class WIDGET_EESCHEMA_COLOR_CONFIG : public wxPanel
{
private:
    EDA_DRAW_FRAME*         m_drawFrame;
    wxBoxSizer*             m_mainBoxSizer;
    wxBoxSizer*             m_colorSchemeBoxSizer;
    wxBoxSizer*             m_controlsBoxSizer;
    wxSize                  m_butt_size_pix;
    wxSize                  m_butt_border_pix;

    wxStaticText*           m_labelColorScheme;
    wxChoice*               m_choiceColorScheme;
    wxButton*               m_buttonCoppyColorScheme;
    wxButton*               m_buttonDeleteColorScheme;
    std::unique_ptr<wxFileConfig>   m_colorSchemeConfigFile;

    // Creates color scheme check list and sizers
    wxBoxSizer* CreateColorSchemeList();
    // Creates the controls and sizers
    wxBoxSizer* CreateControls();

    void    SetColor( wxCommandEvent& aEvent );

    void     OnChoice( wxCommandEvent& aEvent );
    void     OnButtonCopyClick( wxCommandEvent& aEvent );
    void     OnButtonDeleteClick( wxCommandEvent& aEvent );
    bool     InitColorSchemeFile( void );
    bool     GetColorsFromTempFile( const wxString& aScheme );
    wxString GetCurrentColorSchemeFromFile( void );
    bool     SetCurrentColorSchemeInTempFile( const wxString& aScheme );
    bool     ChangeColorShemaInTempFile( const wxString& aKey, const wxString& aValue );
    bool     CopyColorSchemeInTempFile( const wxString& aNewScheme );
    bool     DeleteColorSchemeInTempFile( const wxString& aDeletedScheme );
    bool     CreateColorSchemeFile( const wxString& aFilePath );
//    bool     CreateColorSchemeTempFile( void );
    bool     GetColorSchemeListFromFile( wxArrayString& aColorSchemeList );
    bool     SaveColorSchemeChangesToFile( void );

    virtual EDA_DRAW_FRAME* GetDrawFrame() { return m_drawFrame; }

public:
    // Constructors and destructor
    WIDGET_EESCHEMA_COLOR_CONFIG( wxWindow* aParent, EDA_DRAW_FRAME* aDrawFrame );

    bool TransferDataFromControl();
};


class PANEL_EESCHEMA_COLOR_CONFIG : public wxPanel
{
public:
    PANEL_EESCHEMA_COLOR_CONFIG( EDA_DRAW_FRAME* aFrame, wxWindow* aParent );

protected:
    bool TransferDataToWindow() override;
    bool TransferDataFromWindow() override;

    WIDGET_EESCHEMA_COLOR_CONFIG* m_colorConfig;
};

#endif    // WIDGET_EESCHEMA_COLOR_CONFIG_H_
