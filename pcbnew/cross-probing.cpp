/*****************************************************************/
/* Cross probing function: handle communication to/from eeschema */
/*****************************************************************/

/* cross-probing.cpp */

/** Handle messages between pcbnew and eeschema via a socket,
 *  the port number is
 * KICAD_PCB_PORT_SERVICE_NUMBER (currently 4242) (eeschema to pcbnew)
 * KICAD_SCH_PORT_SERVICE_NUMBER (currently 4243) (pcbnew to eeschema)
 * Note: these ports must be enabled for firewall protection
 */

#include "fctsys.h"
#include "appl_wxstruct.h"
#include "common.h"
#include "pcbnew.h"
#include "wxPcbStruct.h"
#include "eda_dde.h"
#include "pcbnew_id.h"
#include "collectors.h"
#include "protos.h"


/*******************************************/
void RemoteCommand(  const char* cmdline )
/*******************************************/

/** Read a remote command send by eeschema via a socket,
 *  port KICAD_PCB_PORT_SERVICE_NUMBER (currently 4242)
 * @param cmdline = received command from eeschema
 * Commands are
 * $PART: "reference"   put cursor on component
 * $PIN: "pin name"  $PART: "reference" put cursor on the footprint pin
 */
{
    char             line[1024];
    wxString         msg;
    wxString         modName;
    char*            idcmd;
    char*            text;
    MODULE*          module = 0;
    WinEDA_PcbFrame* frame  = (WinEDA_PcbFrame*)wxGetApp().GetTopWindow();
    wxPoint          pos;

    strncpy( line, cmdline, sizeof(line) - 1 );

    idcmd = strtok( line, " \n\r" );
    text  = strtok( NULL, " \n\r" );

    if( !idcmd || !text )
        return;

    if( strcmp( idcmd, "$PART:" ) == 0 )
    {
        modName = CONV_FROM_UTF8( text );

        module = frame->GetBoard()->FindModuleByReference( modName );

        if( module )
            msg.Printf( _( "%s found" ), GetChars( modName ) );
        else
            msg.Printf( _( "%s not found" ), GetChars( modName ) );

        frame->SetStatusText( msg );

        if( module )
            pos = module->GetPosition();
    }
    else if( strcmp( idcmd, "$PIN:" ) == 0 )
    {
        wxString pinName;
        D_PAD*   pad     = NULL;
        int      netcode = -1;

        pinName = CONV_FROM_UTF8( text );

        text = strtok( NULL, " \n\r" );
        if( text && strcmp( text, "$PART:" ) == 0 )
            text = strtok( NULL, "\n\r" );

        modName = CONV_FROM_UTF8( text );

        module = frame->GetBoard()->FindModuleByReference( modName );

        if( module )
            pad = module->FindPadByName( pinName );

        if( pad )
        {
            netcode = pad->GetNet();

            // put cursor on the pad:
            pos = pad->GetPosition();
        }

        if( netcode > 0 )               /* highlight the pad net*/
        {
            g_HighLight_Status   = 1;
            g_HighLight_NetCode = netcode;
        }
        else
        {
            g_HighLight_Status   = 0;
            g_HighLight_NetCode = 0;
        }

        if( module == NULL )
            msg.Printf( _( "%s not found" ), GetChars( modName ) );
        else if( pad == NULL )
        {
            msg.Printf( _( "%s pin %s not found" ), GetChars( modName ), GetChars( pinName ) );
            frame->SetCurItem( module );
        }
        else
        {
            msg.Printf( _( "%s pin %s found" ), GetChars( modName ), GetChars( pinName ) );
            frame->SetCurItem( pad );
        }

        frame->SetStatusText( msg );
    }

    if( module )  // if found, center the module on screen, and redraw the screen.
        frame->RedrawScreen( pos, false );
}


// see wxstruct.h
/**************************************************************************/
void WinEDA_PcbFrame::SendMessageToEESCHEMA( BOARD_ITEM* objectToSync )
/**************************************************************************/

/** Send a remote command to eeschema via a socket,
 * @param objectToSync = item to be located on schematic (module, pin or text)
 * Commands are
 * $PART: "reference"   put cursor on component anchor
 * $PART: "reference" $PAD: "pad number" put cursor on the component pin
 * $PART: "reference" $REF: "reference" put cursor on the component ref
 * $PART: "reference" $VAL: "value" put cursor on the component value
 */
{
    char          cmd[1024];
    const char*   text_key;
    MODULE*       module = NULL;
    D_PAD*        pad;
    TEXTE_MODULE* text_mod;
    wxString      msg;

    if( objectToSync == NULL )
        return;

    switch( objectToSync->Type() )
    {
    case TYPE_MODULE:
        module = (MODULE*) objectToSync;
        sprintf( cmd, "$PART: \"%s\"",
                 CONV_TO_UTF8( module->m_Reference->m_Text ) );
        break;

    case TYPE_PAD:
        module = (MODULE*) objectToSync->GetParent();
        pad    = (D_PAD*) objectToSync;
        msg    = pad->ReturnStringPadName();
        sprintf( cmd, "$PART: \"%s\" $PAD: \"%s\"",
                 CONV_TO_UTF8( module->m_Reference->m_Text ),
                 CONV_TO_UTF8( msg ) );
        break;

    case TYPE_TEXTE_MODULE:
        #define REFERENCE 0
        #define VALUE     1
        module   = (MODULE*) objectToSync->GetParent();
        text_mod = (TEXTE_MODULE*) objectToSync;
        if( text_mod->m_Type == REFERENCE )
            text_key = "$REF:";
        else if( text_mod->m_Type == VALUE )
            text_key = "$VAL:";
        else
            break;

        sprintf( cmd, "$PART: \"%s\" %s \"%s\"",
                 CONV_TO_UTF8( module->m_Reference->m_Text ),
                 text_key,
                 CONV_TO_UTF8( text_mod->m_Text ) );
        break;

    default:
        break;
    }

    if( module )
    {
        SendCommand( MSG_TO_SCH, cmd );
    }
}
