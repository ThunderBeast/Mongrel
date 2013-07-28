/*
 * Copyright (C) 1997-2001 Id Software, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 */
// vid_null.c -- null video driver to aid porting efforts
// this assumes that one of the refs is statically linked to the executable

#include "../client/client.h"

 // Console variables that we need to access from this module
cvar_t      *vid_gamma;
cvar_t      *vid_ref;           // Name of Refresh DLL loaded
cvar_t      *vid_xpos;          // X coordinate of window position
cvar_t      *vid_ypos;          // Y coordinate of window position
cvar_t      *vid_fullscreen;

viddef_t viddef;                                // global video state

refexport_t re;

static qboolean reflib_active = 0;

extern refexport_t GetRefAPI(refimport_t rimp);

/*
 * ==========================================================================
 *
 * DIRECT LINK GLUE
 *
 * ==========================================================================
 */

#define MAXPRINTMSG    4096
void VID_Printf(int print_level, char *fmt, ...)
{
    va_list argptr;
    char    msg[MAXPRINTMSG];

    va_start(argptr, fmt);
    vsprintf(msg, fmt, argptr);
    va_end(argptr);

    if (print_level == PRINT_ALL)
    {
        Com_Printf("%s", msg);
    }
    else
    {
        Com_DPrintf("%s", msg);
    }
}


void VID_Error(int err_level, char *fmt, ...)
{
    va_list argptr;
    char    msg[MAXPRINTMSG];

    va_start(argptr, fmt);
    vsprintf(msg, fmt, argptr);
    va_end(argptr);

    Com_Error(err_level, "%s", msg);
}


void VID_NewWindow(int width, int height)
{
    viddef.width  = width;
    viddef.height = height;
}


/*
** VID_GetModeInfo
*/
typedef struct vidmode_s
{
    const char *description;
    int        width, height;
    int        mode;
} vidmode_t;

vidmode_t vid_modes[] =
{
    { "Mode 0: 320x240",     320,  240,  0 },
    { "Mode 1: 400x300",     400,  300,  1 },
    { "Mode 2: 512x384",     512,  384,  2 },
    { "Mode 3: 640x480",     640,  480,  3 },
    { "Mode 4: 800x600",     800,  600,  4 },
    { "Mode 5: 960x720",     960,  720,  5 },
    { "Mode 6: 1024x768",   1024,  768,  6 },
    { "Mode 7: 1152x864",   1152,  864,  7 },
    { "Mode 8: 1280x960",   1280,  960,  8 },
    { "Mode 9: 1600x1200",  1600, 1200,  9 },
    { "Mode 10: 2048x1536", 2048, 1536, 10 }
};
#define VID_NUM_MODES    (sizeof(vid_modes) / sizeof(vid_modes[0]))

qboolean VID_GetModeInfo(int *width, int *height, int mode)
{
    if ((mode < 0) || (mode >= VID_NUM_MODES))
    {
        return false;
    }

    *width  = vid_modes[mode].width;
    *height = vid_modes[mode].height;

    return true;
}


void VID_Init(void)
{
    /* Create the video variables so we know how to start the graphics drivers */
    vid_ref = Cvar_Get ("vid_ref", "opengl", CVAR_ARCHIVE);
    vid_xpos = Cvar_Get ("vid_xpos", "3", CVAR_ARCHIVE);
    vid_ypos = Cvar_Get ("vid_ypos", "22", CVAR_ARCHIVE);
    vid_fullscreen = Cvar_Get ("vid_fullscreen", "0", CVAR_ARCHIVE);
    vid_gamma = Cvar_Get( "vid_gamma", "1", CVAR_ARCHIVE );
        
    /* Start the graphics mode and load refresh DLL */
    VID_CheckChanges();

}


void VID_Shutdown(void)
{
    if (re.Shutdown)
    {
        re.Shutdown();
    }
}

/*
==============
VID_LoadRefresh
==============
*/
qboolean VID_LoadRefresh( char *name )
{
    refimport_t ri;
    
    if ( reflib_active )
    {
        re.Shutdown();
    }

    Com_Printf( "------- Loading %s -------\n", name );

    ri.Cmd_AddCommand = Cmd_AddCommand;
    ri.Cmd_RemoveCommand = Cmd_RemoveCommand;
    ri.Cmd_Argc = Cmd_Argc;
    ri.Cmd_Argv = Cmd_Argv;
    ri.Cmd_ExecuteText = Cbuf_ExecuteText;
    ri.Con_Printf = VID_Printf;
    ri.Sys_Error = VID_Error;
    ri.FS_LoadFile = FS_LoadFile;
    ri.FS_FreeFile = FS_FreeFile;
    ri.FS_Gamedir = FS_Gamedir;
    ri.Cvar_Get = Cvar_Get;
    ri.Cvar_Set = Cvar_Set;
    ri.Cvar_SetValue = Cvar_SetValue;
    ri.Vid_GetModeInfo = VID_GetModeInfo;
    ri.Vid_MenuInit = VID_MenuInit;
    ri.Vid_NewWindow = VID_NewWindow;

    re = GetRefAPI( ri );

    if (re.api_version != API_VERSION)
    {
        Com_Error (ERR_FATAL, "%s has incompatible api_version", name);
    }

    if ( re.Init( NULL, NULL ) == -1 )
    {
        re.Shutdown();
        return false;
    }

    Com_Printf( "------------------------------------\n");
    reflib_active = true;

//======
//PGM
    vidref_val = VIDREF_OTHER;
    if(vid_ref)
    {
        if(!strcmp (vid_ref->string, "gl"))
            vidref_val = VIDREF_GL;
        else if(!strcmp(vid_ref->string, "soft"))
            vidref_val = VIDREF_SOFT;
    }
//PGM
//======

    return true;
}



void VID_CheckChanges (void)
{
    char name[100];

    if ( vid_ref->modified )
    {
        cl.force_refdef = true;     // can't use a paused refdef
        S_StopAllSounds();
    }
    while (vid_ref->modified)
    {
        /*
        ** refresh has changed
        */
        vid_ref->modified = false;
        vid_fullscreen->modified = true;
        cl.refresh_prepped = false;
        cls.disable_screen = true;

        Com_sprintf( name, sizeof(name), "ref_%s.dll", vid_ref->string );
        if ( !VID_LoadRefresh( name ) )
        {
            if ( strcmp (vid_ref->string, "soft") == 0 )
                Com_Error (ERR_FATAL, "Couldn't fall back to software refresh!");
            Cvar_Set( "vid_ref", "soft" );

            /*
            ** drop the console if we fail to load a refresh
            */
            if ( cls.key_dest != key_console )
            {
                Con_ToggleConsole_f();
            }
        }
        cls.disable_screen = false;
    }

}


void VID_MenuInit(void)
{
}


void VID_MenuDraw(void)
{
}


const char *VID_MenuKey(int k)
{
    return NULL;
}
