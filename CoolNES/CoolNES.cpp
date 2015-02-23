/********************************************************
* CoolNES                                               *
* Copyright (C) 2005-2006 Matt "dxprog" Hackmann        *
*                                                       *
* Assuming I ever release the source code for this and  *
* it gets viewed by other people and somebody decides   *
* to use it in one of their projects, all I ask is that *
* you give me a little credit. Otherwise, it's yours to *
* use. Happy programming :-)                            *
********************************************************/

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <stdio.h>
#include "window.h"
#include "cpu6502.h"
#include "Cart.h"

iNES t_RomInfo;
unsigned char *a_PrgBanks;
unsigned char *a_ChrBanks;
unsigned char i_Mapper;
unsigned char i_Mirroring;
Cpu6502 g_CPU;

int WINAPI WinMain (HINSTANCE hThisInst, HINSTANCE hPrevInst, LPSTR lpszArgs, int nWinMode)
{

	HWND hWnd;
	Window g_Window;
	Cart t_ROM;
	FILE *h_File;

	// Create the window
	hWnd = g_Window.Create(hThisInst, "COOLNES", "CoolNES", nWinMode);
	if (!hWnd)
		return 1;

	// Create the PPU and CPU
	Ppu2C02 g_PPU (hWnd);
	g_CPU.Init (&g_PPU);

	// Load the ROM
	if (!t_ROM.LoadCart("C:\\Roms\\smb.nes")) {
		MessageBox (hWnd, "Unable to load ROM image!", "Bummer", MB_OK);
		return 1;
	}

	g_CPU.Reset (false);
	if (t_RomInfo.i_Control1 & 8)
		i_Mirroring = MIR_FOUR;
	else
		i_Mirroring = t_RomInfo.i_Control1 & 1;

	// If there was CHRROM in the ROM load it into memory and cache the tiles
	if (t_RomInfo.i_ChrBanks > 0) {

		// heh. This is interesting. I will initialize the vram pointer to 0 (using valid PPU calls) and write all the
		// CHRROM into the first $2000 bytes of vram. It'd actually work on a real PPU... creepy
		g_PPU.WriteMem (0x2006, 0);
		g_PPU.WriteMem (0x2006, 0);
		
		// Load the CHR data into vram
		for (int i = 0; i < 0x2000; i++)
			g_PPU.WriteMem (0x2007, a_ChrBanks[i]);

	}

	MSG msg;
	bool b_Loop = false;

	/*
	// Start the message loop
	PeekMessage (&msg, hWnd, 0, 0, PM_REMOVE);
	while (msg.message != WM_QUIT) {

		// If there's a message, process it otherwise run the CPU
		if (PeekMessage (&msg, hWnd, 0, 0, PM_REMOVE)) {
			TranslateMessage (&msg);
			DispatchMessage (&msg);
		}
		else {
			// Run the PPU/CPU clk
			g_CPU.Clk ();
			for (int i = 0; i < 3; i++) {
				g_PPU.Clk ();
				if (g_PPU.b_NMI == true)
					g_CPU.RaiseInt (I_NMI);
			}
		}

	} */

	int i_Clock = GetTickCount (), i_Ops = 0, i_Count = 0;
	int i_Ticks = GetTickCount (), a_Speed[1000];
	while (i_Clock + 10000 > GetTickCount ()) {
		g_CPU.Clk ();
		for (int i = 0; i < 3; i++) {
			g_PPU.Clk ();
			if (g_PPU.b_NMI == true)
				g_CPU.RaiseInt (I_NMI);
		}
		i_Ops++;
		if (i_Ticks + 100 < GetTickCount ()) {
			i_Ticks = GetTickCount ();
			a_Speed[i_Count] = i_Ops;
			i_Count++;
			i_Ops = 0;
		}
	}

	FILE *h_Temp; fopen_s (&h_Temp, "C:\\performance.csv", "w");
	for (int i = 0; i < i_Count; i++)
		fprintf (h_Temp, "%d\n", a_Speed[i]);
	fclose (h_Temp);

	return 0;

}

/* The main window loop */
LRESULT CALLBACK WindowProc (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	
	switch (msg) {
	case WM_KEYDOWN:
		switch (wParam) {
		case VK_RETURN:
			g_CPU.SetKey (KEY_START, true);
			break;
		case VK_LEFT:
			g_CPU.SetKey (KEY_LEFT, true);
			break;
		case VK_RIGHT:
			g_CPU.SetKey (KEY_RIGHT, true);
			break;
		case VK_UP:
			g_CPU.SetKey (KEY_UP, true);
			break;
		case VK_DOWN:
			g_CPU.SetKey (KEY_DOWN, true);
			break;
		case 0x41:
			g_CPU.SetKey (KEY_A, true);
			break;
		case 0x53:
			g_CPU.SetKey (KEY_B, true);
			break;
		case 0x42:
			g_CPU.SetKey (KEY_SELECT, true);
			break;
		}
		break;
	case WM_KEYUP:
		switch (wParam) {
		case VK_RETURN:
			g_CPU.SetKey (KEY_START, false);
			break;
		case VK_LEFT:
			g_CPU.SetKey (KEY_LEFT, false);
			break;
		case VK_RIGHT:
			g_CPU.SetKey (KEY_RIGHT, false);
			break;
		case VK_UP:
			g_CPU.SetKey (KEY_UP, false);
			break;
		case VK_DOWN:
			g_CPU.SetKey (KEY_DOWN, false);
			break;
		case 0x41:
			g_CPU.SetKey (KEY_A, false);
			break;
		case 0x53:
			g_CPU.SetKey (KEY_B, false);
			break;
		case 0x42:
			g_CPU.SetKey (KEY_SELECT, false);
			break;
		}
		break;
	case WM_CLOSE:
		PostQuitMessage (0);
		break;
	default:
		return DefWindowProc (hwnd, msg, wParam, lParam);
	}
	
	return 0;
	
}