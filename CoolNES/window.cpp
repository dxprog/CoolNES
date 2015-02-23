/**********************************************
* Platformer                                  *
* Copyright (C) 2005 Matt Hackmann            *
* All Rights Reserved.                        *
*                                             *
* Window class functions                      *
**********************************************/

#include "window.h"

HWND Window::Create (HINSTANCE h_Inst, char *s_Class, char *s_Title, int i_Mode)
{

	/* The window class and window handle */
	HWND h_Wnd;
	WNDCLASSEX g_WndClass;
	
	/* Set the program instance and class info... */
	g_WndClass.cbSize = sizeof (WNDCLASSEX);
	g_WndClass.hInstance = h_Inst;
	g_WndClass.lpszClassName = s_Class;
	g_WndClass.lpfnWndProc = WindowProc;
	g_WndClass.style = 0;
	
	/* ...icons, cursors, and background color... */
	g_WndClass.hIcon = LoadIcon (NULL, IDI_APPLICATION);
	g_WndClass.hIconSm = LoadIcon (NULL, IDI_APPLICATION);
	g_WndClass.hCursor = LoadCursor (NULL, IDC_ARROW);
	g_WndClass.hbrBackground = (HBRUSH) GetStockObject (BLACK_BRUSH);
	
	/* and menu. */
	g_WndClass.lpszMenuName = NULL;
	g_WndClass.cbClsExtra = 0;
	g_WndClass.cbWndExtra = 0;
	
	/* Register the window class */
	if (!RegisterClassEx (&g_WndClass))
		return 0;
		
	/* Now that that's done we can create the actual window */
	h_Wnd = CreateWindow (s_Class, s_Title, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 300, 300, HWND_DESKTOP, NULL, h_Inst, NULL);
	
	/* Show the window */
	ShowWindow (h_Wnd, i_Mode);

	/* Return the window handle */
	return h_Wnd;

}