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
#include <string.h>

// DLL specific
#include <olectl.h>
#include "actctx.h"
#include <assert.h>
#include <Python.h>

BOOL have_init = FALSE;
HANDLE g_ctypes = 0;
HMODULE gInstance = 0;

CRITICAL_SECTION csInit; // protecting our init code


extern int init_with_instance(HMODULE hmod_exe, char *frozen, int argc, wchar_t **argv);
extern void fini();
extern int run_script(void);
extern wchar_t libfilename[_MAX_PATH + _MAX_FNAME + _MAX_EXT];
// end DLL specific

int check_init(int argc, wchar_t ** arglist);

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

//    char modulename[6] = "hello";
	wchar_t wmodulename[6]  = L"hello";
	wchar_t *arg1;
    wchar_t **arglist;
	int result;
	arg1 = (wchar_t *) wmodulename;
	arglist = &arg1;
	//result = init("shared_dll", 0, arglist);
	result = check_init(1, arglist);
	printf("initialization: %d\n", (int) result);
    return result;
}

__declspec(dllexport) int WINAPI runPython ()
{
	int result;

    printf(stdout, "running");
	result = initPython();
	printf("initialization: %d\n", (int) result);
	start();

	return 0;
}

__declspec(dllexport) int WINAPI closePython ()
{
    printf(stdout, "running");

	initPython();
	fini();

	return 0;
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

int check_init(int argc, wchar_t ** arglist)
{
	if (!have_init) {
//		if (IDYES == MessageBox(NULL, "Attach Debugger?", "run_ctypes_dll", MB_YESNO))
//			DebugBreak();
		EnterCriticalSection(&csInit);
		// Check the flag again - another thread may have beat us to it!
		if (!have_init) {
			PyObject *frozen;
			/* If Python had previously been initialized, we must use
			   PyGILState_Ensure normally.  If Python has not been initialized,
			   we must leave the thread state unlocked, so other threads can
			   call in.
			*/
			PyGILState_STATE restore_state = PyGILState_UNLOCKED;
#if 0
			if (!Py_IsInitialized) {
				// Python function pointers are yet to be loaded
				// - force that now. See above for why we *must*
				// know about the initialized state of Python so
				// we can setup the thread-state correctly
				// before calling init_with_instance(); This is
				// wasteful, but fixing it would involve a
				// restructure of init_with_instance()
				_LocateScript(gInstance);
				_LoadPythonDLL(gInstance);
			}
#endif
			// initialize, and set sys.frozen = 'dll'
			init_with_instance(gInstance, "dll", argc, arglist);
/*
			if (Py_IsInitialized && Py_IsInitialized()) {
				restore_state = PyGILState_Ensure();
			}
*/
#if 0
			// a little DLL magic.  Set sys.frozen='dll'
			init_with_instance(gInstance, "dll");
			init_memimporter();
#endif
			frozen = PyLong_FromVoidPtr(gInstance);
			if (frozen) {
				PySys_SetObject("frozendllhandle", frozen);
				Py_DECREF(frozen);
			}
			// Now run the generic script - this always returns in a DLL.
			run_script();
			have_init = TRUE;
			if (g_ctypes == NULL)
				//load_ctypes();
			// Reset the thread-state, so any thread can call in
			PyGILState_Release(restore_state);
		}
		LeaveCriticalSection(&csInit);
	}
	return g_ctypes != NULL;
}

