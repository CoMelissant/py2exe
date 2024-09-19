/*
 *	   Copyright (c) 2000-2013 Thomas Heller
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <windows.h>
#include <stdio.h>

// DLL specific
#include <olectl.h>
#include "actctx.h"
#include <assert.h>
#include <Python.h>

BOOL have_init = FALSE;
HANDLE g_ctypes = 0;
HMODULE gInstance = 0;

CRITICAL_SECTION csInit; // protecting our init code

extern wchar_t libfilename[_MAX_PATH + _MAX_FNAME + _MAX_EXT];
// end DLL specific

void SystemError(int error, char *msg)
{
	char Buffer[1024];

	if (msg)
		fprintf(stderr, msg);
	if (error) {
		LPVOID lpMsgBuf;
		FormatMessageA(
			FORMAT_MESSAGE_ALLOCATE_BUFFER |
			FORMAT_MESSAGE_FROM_SYSTEM,
			NULL,
			error,
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			(LPSTR)&lpMsgBuf,
			0,
			NULL
			);
		strncpy(Buffer, lpMsgBuf, sizeof(Buffer));
		LocalFree(lpMsgBuf);
		fprintf(stderr, Buffer);
	}
}

/* imported from start.c 
   init calls init_with_instance */
extern int init(char *, int, wchar_t **);
extern int start();


/*
  The main function for our exe.
*/
__declspec(dllexport) int WINAPI initPython ()
{
    wchar_t **arglist = NULL;
	int result;
	result = init("shared_dll", 0, arglist);
	
    return result;
}

__declspec(dllexport) int WINAPI runPython ()
{
    //fprintf(stdout, "running");
	return start();
}




// *****************************************************************
// All the public entry points needed for COM, Windows, and anyone
// else who wants their piece of the action.
// *****************************************************************
BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
{
	OutputDebugStringA("DllMain");
    if ( dwReason == DLL_PROCESS_ATTACH) {
		gInstance = hInstance;
		InitializeCriticalSection(&csInit);
		_MyLoadActCtxPointers();
		if (pfnGetCurrentActCtx && pfnAddRefActCtx)
		  if ((*pfnGetCurrentActCtx)(&PyWin_DLLhActivationContext))
			if (!(*pfnAddRefActCtx)(PyWin_DLLhActivationContext))
			  OutputDebugStringA("Python failed to load the default activation context\n");
	}
	else if ( dwReason == DLL_PROCESS_DETACH ) {
		gInstance = 0;
		DeleteCriticalSection(&csInit);
		if (pfnReleaseActCtx)
		  (*pfnReleaseActCtx)(PyWin_DLLhActivationContext);
		// not much else safe to do here
	}
	return TRUE; 
}
