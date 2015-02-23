/**********************************************
* Platformer                                  *
* Copyright (C) 2005 Matt Hackmann            *
* All Rights Reserved.                        *
*                                             *
* Window class definition                     *
**********************************************/

#ifndef __H_WINDOW__
#define __H_WINDOW__

#include <windows.h>

/* The window function */
LRESULT CALLBACK WindowProc (HWND, UINT, WPARAM, LPARAM);

class Window {

	public:
		
		HWND Create (HINSTANCE h_Inst, char *s_Class, char *s_Title, int i_Mode);
	
	protected:
	
		HWND h_Wnd;
	
	private:

};

#endif