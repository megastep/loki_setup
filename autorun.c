/* Simple AUTORUN.EXE file to display a README file */

#include <windows.h>

/* This is where execution begins [windowed apps] */
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR szCmdLine, int sw)
{
	ShellExecute(GetDesktopWindow(), "open", "win32\\README.htm",
					NULL, NULL, SW_SHOWNORMAL);
	return(TRUE);
}
