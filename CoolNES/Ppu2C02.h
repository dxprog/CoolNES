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

#ifndef _PPU2C02_H_
#define _PPU2C02_H_

#include <windows.h>
#include <stdio.h>
#include <ddraw.h>

#define VIDEO_RAM     0x4000
#define SPRITE_RAM    0x100

#define MAX_TILES     512
#define MIR_HORZ      0
#define MIR_VERT      1
#define MIR_FOUR      3

#define SET_VBLANK(status) (status = (status | 0x80))
#define CLR_VBLANK(status) (status = (status & 0x7F))

//#define _SHOW_NAMETABLES_

struct Tile {
	unsigned char a_Pixels[0x40];
	unsigned int i_PixelVal;
};

extern unsigned char i_Mirroring;

class Ppu2C02 {

	public:

		Ppu2C02 (HWND hWnd);
		~Ppu2C02 ();
		void Clk ();
		unsigned char ReadRam (unsigned short int i_Addr);
		void WriteMem (unsigned short int i_Addr, unsigned char i_Value);

		HWND h_Window;
		bool b_NMI;

		unsigned char a_SprRam[SPRITE_RAM];

	private:

		void CacheTile (unsigned char i_Value, unsigned short int i_Addr);
		
		// VRAM and sprite RAM
		unsigned char a_VRam[VIDEO_RAM];
		unsigned long int a_Palette[63];

		unsigned long int a_Screen[65536];

		// Tile cache
		Tile a_Cache[MAX_TILES];

		// Address and register stuffs
		unsigned short int i_VRam1;
		unsigned short int i_VRam2;
		unsigned char i_SprPtr;
		bool b_SpriteLine;
		unsigned char i_Sprites;
		unsigned char a_Sprites[64];
		
		
		unsigned short int i_NameTable;
		unsigned char i_AddrInc;
		unsigned char i_SprAddr;
		unsigned char i_BgAddr;
		unsigned char i_SpritesDrawn;
		unsigned char i_LastSprite;

		// PPU scrolling registers
		unsigned char i_LatchX, i_LatchY;

		bool b_BgOn;
		bool b_SpritesOn;
		unsigned long int i_BgColor;
		unsigned char i_Line;

		unsigned char i_SprRam;
		unsigned char i_Status;
		unsigned char i_Latch;

		unsigned char i_VScroll;
		unsigned char i_HScroll;

		unsigned char i_PPUCtrl1;

		unsigned short int i_Attr, i_LastTile;

		// Double buffer stuff
		HDC h_Buffer;
		HBITMAP h_Bitmap;

		unsigned long int i_LastTick, i_RenderTime;

		// DirectDraw stuff
		bool InitDDraw (HWND hWnd);
		LPDIRECTDRAW7 g_DDraw;
		LPDIRECTDRAWSURFACE7 g_Primary;
		LPDIRECTDRAWSURFACE7 g_BackBuffer;
		LPDIRECTDRAWCLIPPER g_Clipper;
		bool b_DDraw;

		void Blit ();

		bool b_Write;


		// Counters
		unsigned short int i_HLine;
		unsigned short int i_ScanLine;

		// Window graphics handle
		HDC hDC;

};

#endif