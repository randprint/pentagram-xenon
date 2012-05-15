/*
Copyright (C) 2002-2009 The Pentagram team

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#include "pent_include.h"

#include <SDL.h>
#include "GUIApp.h"
#include "MemoryManager.h"
#include "version.h"
#include "filesys/FileSystem.h"
#ifndef XENON
#include "filesys/OutputLogger.h"
#else
#include <xetypes.h>
#include <xenos/xe.h>
#include <xenos/xenos.h>
#include <xenos/edram.h>
#include <xenos/xenos.h>
#include <libfat/fat.h>
#include <usb/usbmain.h>
#include <diskio/ata.h>
#include <console/console.h>
#include <xenon_smc/xenon_smc.h>
#include <xenon_soc/xenon_power.h>
#include <sys/iosupport.h>
#include <xenon_sound/sound.h>
#endif
#ifdef _WIN32
// Disable SDLMain in Windows
#ifdef main
#undef main
#endif
#endif
#ifdef LIBXENON
#define TR {printf("[Trace] in function %s, line %d, file %s\n",__FUNCTION__,__LINE__,__FILE__);}

static void findDevices()
{
	int device_list_size = 0;
	char device_list[STD_MAX][10];
	int i;
	
	for (i = 3; i < STD_MAX; i++)
	{
		if (devoptab_list[i]->structSize)
		{
			printf("findDevices : %s\r\n", devoptab_list[i]->name);
		}
	}
}
#endif
#ifndef XENON
int main(int argc, char* argv[])
{
	
	const std::string &home = FileSystem::getHomePath();

	OutputLogger	stdoutLogger(stdout,home + "/pstdout.txt");
	OutputLogger	stderrLogger(stderr,home + "/pstderr.txt");

#else
#ifdef XENON
int main()
{
	TR;
	const std::string &home = "uda:/pentagram";
	const int argc=0;
	const char* argv[]={};
	xenon_make_it_faster(XENON_SPEED_FULL);
	xenos_init(VIDEO_MODE_AUTO);
	console_init();
    xenon_sound_init();
	// usb	
	usb_init();
	xenon_ata_init();
	usb_do_poll();
	fatInitDefault();
	findDevices();
	TR;
#endif
#endif
	// Initialize Memory Manager here to avoid extra tools depending on it
	MemoryManager mm;
	GUIApp app(argc, argv);

#ifdef SAFE_CONSOLE_STREAMS
	console_streambuf<char> fb;
	ppout = new console_ostream<char>(&fb);

	console_err_streambuf<char> err_fb;
	pperr = new console_err_ostream<char>(&err_fb);

#endif

	pout << "Pentagram version " << PentagramVersion::version << std::endl;
	pout << "Built: " << PentagramVersion::buildtime << std::endl;
	pout << "Optional features: " << PentagramVersion::features << std::endl;
	pout << std::endl;

	app.startup();
	app.run();

	return 0;
}

#ifdef _WIN32
#include <windows.h>
#include <cstdio>

 PCHAR*
 CommandLineToArgvA(
     PCHAR CmdLine,
     int* _argc
     )
 {
     PCHAR* argv;
     PCHAR  _argv;
     ULONG   len;
     ULONG   argc;
     CHAR   a;
     ULONG   i, j;

     BOOLEAN  in_QM;
     BOOLEAN  in_TEXT;
     BOOLEAN  in_SPACE;

     len = strlen(CmdLine);
     i = ((len+2)/2)*sizeof(PVOID) + sizeof(PVOID);

	 argv = (PCHAR*)LocalAlloc(LMEM_FIXED, i + (len+2)*sizeof(CHAR));

     _argv = (PCHAR)(((PUCHAR)argv)+i);

     argc = 0;
     argv[argc] = _argv;
     in_QM = FALSE;
     in_TEXT = FALSE;
     in_SPACE = TRUE;
     i = 0;
     j = 0;

     while( a = CmdLine[i] ) {
         if(in_QM) {
             if(a == '\"') {
                 in_QM = FALSE;
             } else {
                 _argv[j] = a;
                 j++;
             }
         } else {
             switch(a) {
             case '\"':
                 in_QM = TRUE;
                 in_TEXT = TRUE;
                 if(in_SPACE) {
                     argv[argc] = _argv+j;
                     argc++;
                 }
                 in_SPACE = FALSE;
                 break;
             case ' ':
             case '\t':
             case '\n':
             case '\r':
                 if(in_TEXT) {
                     _argv[j] = '\0';
                     j++;
                 }
                 in_TEXT = FALSE;
                 in_SPACE = TRUE;
                 break;
             default:
                 in_TEXT = TRUE;
                 if(in_SPACE) {
                     argv[argc] = _argv+j;
                     argc++;
                 }
                 _argv[j] = a;
                 j++;
                 in_SPACE = FALSE;
                 break;
             }
         }
         i++;
     }
     _argv[j] = '\0';
     argv[argc] = NULL;

     (*_argc) = argc;
     return argv;
 }


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
	int argc;
	char **argv = CommandLineToArgvA(GetCommandLineA(), &argc);

	int res = main(argc, argv);

	// Free memory
	LocalFree((HLOCAL)argv);

	return  res;
}
#endif
