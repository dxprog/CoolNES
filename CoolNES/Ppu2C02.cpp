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

#include "Ppu2C02.h"

Ppu2C02::Ppu2C02 (HWND hWnd)
{

	unsigned char rgb[3], i_Pal = 0;

	/* Initialize the scanline and hline counters */
	i_ScanLine = 241;
	i_HLine = 0;
	i_Status = 0;
	h_Window = hWnd;
	b_Write = false;

	// If DirectDraw was unable to initialize, setup GDI
	if (!(b_DDraw = InitDDraw (hWnd))) {
		
		MessageBox (NULL, "Unable to initialize DirectDraw. Using GDI", "ERROR", MB_OK);

		/* Create the bitmap and device context */
		h_Buffer = CreateCompatibleDC (GetDC (hWnd));
		h_Bitmap = CreateCompatibleBitmap (GetDC (hWnd), 256, 240);
		SelectObject (h_Buffer, h_Bitmap);

	}

	/* Read in the palette */
	FILE *h_File;
	fopen_s (&h_File, "C:\\rgb.pal", "rb");
	while (!feof (h_File)) {
		fread (&rgb, sizeof (unsigned char), 3, h_File);
		a_Palette[i_Pal] = RGB(rgb[2], rgb[1], rgb[0]);
		i_Pal++;
	}
	fclose (h_File);

	WriteMem (0x2000, 0);
	WriteMem (0x2001, 0);

	i_RenderTime = 0;

}

Ppu2C02::~Ppu2C02()
{

	// If DirectDraw's been initialized, can the objects
	if (b_DDraw) {
		g_Clipper->Release ();
		g_Primary->Release ();
		g_DDraw->Release ();
	}
	
	// Clean up the double buffer stuff
	DeleteObject (h_Bitmap);
	DeleteDC (h_Buffer);
	
		FILE *h_File;
	fopen_s (&h_File, "C:\\video_ram.debug", "wb");
	fwrite (&a_VRam, sizeof (unsigned char), VIDEO_RAM, h_File);
	fclose (h_File);

}

/********************************************************
* Function: Clk                                         *
* Description: PPU clk sequence                         *
********************************************************/
void Ppu2C02::Clk()
{

	unsigned short int i_TileX, i_TileY, i_Tile, i_Temp = 0, i_NTable = i_NameTable, i_Pattern;
	unsigned long int i_Color = 0, i_BgColor = 0, i_Temp2, i_Render;
	unsigned char i_AttrBit, i_Sprite;

	i_LastTick = GetTickCount ();

	// Increment the hline counter
	i_HLine++;
	b_NMI = false;

	// If we're not in H/VBlank draw pixels baby (as long as the PPU is on)!
	if (i_HLine < 256 && i_ScanLine < 241) {

		if (b_BgOn) {
			// Get the tile
			i_TileY = (i_ScanLine + i_VScroll) >> 3;
			i_TileX = (i_HLine + i_HScroll) >> 3;
			if (i_TileY > 0x1D || i_TileX > 0x1F) {

				// Go to the next name table depending on mirroring
				switch (i_Mirroring) {
				case MIR_HORZ:
					if (i_NameTable > 0x2000)
						i_NTable = 0x2000;
					else
						i_NTable = 0x2800;
					break;
				case MIR_VERT:
					if (i_NameTable > 0x2000)
						i_NTable = 0x2000;
					else
						i_NTable = 0x2400;
					break;
				case MIR_FOUR:
					i_NTable = 0x2000;
					break;
				}

				// Get the tile x and y back within the nametable's area
				if (i_TileX > 0x1F)
					i_TileX -= 0x20;
				if (i_TileY > 0x1D)
					i_TileY -= 0x1E;

			}

			i_Tile = (i_TileY * 32) + i_TileX;
			i_Pattern = a_VRam[i_Tile + i_NTable] + (i_BgAddr * 256);

			// If this tile isn't tranparent, draw it
			if (a_Cache[i_Pattern].i_PixelVal) {

				// Fetch the attribute data once every eight pixels (at the leading edge of each new tile)
				if ((i_HLine + i_HScroll) % 8 == 0) {
					i_Attr = ((i_TileY >> 2) * 8) | (i_TileX >> 2);
					i_AttrBit = ((i_TileY & 2) | ((i_TileX >> 1) & 1)) << 1;
					i_Attr = (a_VRam[i_NTable + 0x3C0 + i_Attr] & (3 << i_AttrBit)) >> i_AttrBit;
				}

				i_BgColor = a_Cache[i_Pattern].a_Pixels[(((i_ScanLine + i_VScroll) & 7) * 8) + ((i_HLine + i_HScroll)  & 7)];
				if (i_BgColor != 0)
					a_Screen[(i_ScanLine * 256) + i_HLine] = a_Palette[a_VRam[i_BgColor + 0x3F00 + (i_Attr << 2)]];
				else
					a_Screen[(i_ScanLine * 256) + i_HLine] = a_Palette[a_VRam[0x3F10]];

			}
			else
				a_Screen[(i_ScanLine * 256) + i_HLine] = a_Palette[a_VRam[0x3F10]];

		}

		// Draw the sprite
		if (b_SpritesOn && b_SpriteLine) {
			// Check to see if there are any sprites on this line
			for (int i = 0; i < i_Sprites; i++) {

				// Figure up the coordinate to get the pixel
				i_Sprite = a_Sprites[i];

				// Get the X and Y offsets the the current scanning position of the next pixel to draw
				i_TileX = (i_HLine - a_SprRam[i_Sprite + 3]);
				i_TileY = (i_ScanLine - a_SprRam[i_Sprite] - 1);

				// If the pixel is within drawing distance, draw it
				if (i_TileX < 8 && i_TileY < 8) {

					// Flip the sprite if need be
					if (a_SprRam[i_Sprite + 2] & 0x80)
						i_TileY = abs((signed char) i_TileY - 7);
					if (a_SprRam[i_Sprite + 2] & 0x40)
						i_TileX = abs((signed char) i_TileX - 7);

					//i_SpritesDrawn++;
					
					// If the sprites are 8x16 select the proper pattern table using bit 0 of the index number
					if (i_PPUCtrl1 & 0x20)
						i_SprAddr = i_Sprite & 1;
					i_Tile = a_SprRam[i_Sprite + 1] + (i_SprAddr * 256);
					
					// If this pixel is not transparent, draw it
					if (a_Cache[i_Tile].a_Pixels[(i_TileY * 8) + i_TileX] != 0) {
						
						// If the background pixel was also non-transparent and the sprite is 0 set the sprite
						// zero hit flag
						if (i_BgColor && i_Sprite == 0)
							i_Status |= 0x40;

						// Draw the pixel if it is supposed to overlap the background
						if (!(a_SprRam[i_Sprite + 2] & 0x20) || !i_BgColor)
							a_Screen[(i_ScanLine * 256) + i_HLine] = a_Palette[a_VRam[((a_SprRam[i_Sprite + 2] & 3) << 2) + a_Cache[i_Tile].a_Pixels[(i_TileY * 8) + i_TileX] + 0x3F10]];

					}
				}
			}
		}
	}

	 // If we've hit 341 (the end of the scanline row), move to the next scanline and start over
	 if (i_ScanLine < 241) {
		if (i_HLine == 256) {

			i_SpritesDrawn = 0;
			i_Sprites = 0;
			i_LastSprite = 0;
			b_SpriteLine = false;

		}
		
		if (i_HLine > 256 && i_HLine < 341 && i_LastSprite < 256 && i_Sprites < 8) {

			if ((a_SprRam[i_LastSprite] + 1) <= (i_ScanLine + 1) && (a_SprRam[i_LastSprite] + 9) >= (i_ScanLine + 1)) {
				a_Sprites[i_Sprites] = i_LastSprite;
				i_Sprites++;
				b_SpriteLine = true;
				
				// If we have eight sprites set the flag
				if (i_Sprites == 8) {
					i_LastSprite = 256;
					i_Status &= 0xDF;
					i_Status |= 0x20;
				}
			}
			i_LastSprite += 4;
		}
	}
	
	if (i_HLine == 341) {

		i_HLine = 0;
		i_ScanLine++;
		i_HScroll = i_LatchX;
		i_Status &= 0xDF;

		// If we've hit scanline 241, set vblank
		if (i_ScanLine == 241) {
			
			SET_VBLANK (i_Status);
			i_VScroll = i_LatchY;
			
			// If NMIs are enabled call one
			if (i_PPUCtrl1 & 0x80)
				b_NMI = true;

			// Blit the buffered image to the screen
			#ifdef _SHOW_NAMETABLES_
			for (int j = 0; j < 4; j++) {
				for (int y = 0; y < 30; y++) {
					for (int x = 0; x < 32; x++) {
						i_Temp2 = 0x2000 + (j * 0x400);

						i_Temp2 = (((((j & 2) >> 1) * 32) + y) * 256) + (((j & 1) * 32) + x);
						if (a_Cache[a_VRam[0x2000 + (j * 0x400) + ((y * 32) + x)] + 256].i_PixelVal == 0)
							a_Screen[i_Temp2] = 0;
						else
							a_Screen[i_Temp2] = RGB(255,255,255);
						
					}
				}
			}
			#endif
			Blit ();

		}
		// If scanline 261, move to the top
		else if (i_ScanLine == 261) {
			i_ScanLine = 0;
			CLR_VBLANK (i_Status);
			i_Status &= 0xBF;
		}
	}
}

unsigned char Ppu2C02::ReadRam(unsigned short i_Addr)
{

	unsigned char i_Return;
	i_Addr &= 7;

	// See what address is being read and return the appropriate value
	switch (i_Addr) {
	case 2:
		i_Return = i_Status;
		CLR_VBLANK(i_Status);
		i_Latch = 0;
		b_Write = false;
		break;
	case 7:

		// Return the old latch data and read the new stuff in
		if (i_VRam1 < 0x3F00)
			i_Return = i_Latch;
		else
			i_Return = a_VRam[i_VRam1];
		i_Latch = a_VRam[i_VRam1];
		i_VRam1++;

	}

	return i_Return;

}

void Ppu2C02::WriteMem(unsigned short int i_Addr, unsigned char i_Value)
{

	int i_Temp;
	i_Addr &= 7;

	// See what to write to depending on the address
	switch (i_Addr) {
	case 0:
		i_PPUCtrl1 = i_Value;

		// Set individual properties
		i_NameTable = ((i_PPUCtrl1 & 3) * 0x400) + 0x2000;
		if (!(i_PPUCtrl1 & 4)) i_AddrInc = 1; else i_AddrInc = 32;
		i_SprAddr = (i_PPUCtrl1 & 8) >> 3;
		i_BgAddr = (i_PPUCtrl1 & 16) >> 4;

		break;
	
	case 1:

		// Set the bg/sprite enabled properties
		b_BgOn = i_Value & 8;
		b_SpritesOn = i_Value & 0x10;

		break;

	case 5:

		if (b_Write) {
			if (i_Value < 240)
				i_LatchY = i_Value;
			b_Write = false;
		}
		else {
			i_LatchX = i_Value;
			b_Write = true;
		}

		break;

	case 3:
		i_SprPtr = i_Value;
		break;
	
	case 4:
		a_SprRam[i_SprPtr] = i_Value;
		i_SprPtr++;
		break;

	case 6:

		// If this is the first write, shift the bits up
		if (!b_Write) {
			//i_LatchY = ((i_Value & 3) << 3) | ((i_Value & 30) >> 4);
			i_VRam2 = (i_Value & 0x3F) << 8;
			b_Write = true;
		}
		else {
			i_VRam1 = i_VRam2 + i_Value;
			i_NameTable = (((i_VRam1 & 0xC00) >> 10) * 0x400) + 0x2000;
			i_HScroll = i_LatchX;
			i_VScroll = i_LatchY;
			b_Write = false;
		}
		break;

	case 7:

		// Write the value to the address specififed by 2006
		i_VRam1 &= 0x3FFF;

		// Mirror if needed
		if (i_VRam1 > 0x1FFF && i_VRam1 < 0x3000) {
			
			i_Temp = i_VRam1 & 0x3C00;

			switch (i_Mirroring) {
			case MIR_HORZ:
				if (i_Temp == 0x2000 || i_Temp == 0x2400) {
					a_VRam[(i_VRam1 & 0x3FF) + 0x2000] = i_Value;
					a_VRam[(i_VRam1 & 0x3FF) + 0x2400] = i_Value;
				}
				else {
					a_VRam[(i_VRam1 & 0x3FF) + 0x2800] = i_Value;
					a_VRam[(i_VRam1 & 0x3FF) + 0x2C00] = i_Value;
				}
				break;
			case MIR_VERT:
				if (i_Temp == 0x2000 || i_Temp == 0x2800) {
					a_VRam[(i_VRam1 & 0x3FF) + 0x2000] = i_Value;
					a_VRam[(i_VRam1 & 0x3FF) + 0x2800] = i_Value;
				}
				else {
					a_VRam[(i_VRam1 & 0x3FF) + 0x2400] = i_Value;
					a_VRam[(i_VRam1 & 0x3FF) + 0x2C00] = i_Value;
				}
				break;
			}
		}
		else 
			a_VRam[i_VRam1] = i_Value;

		// If the address is within the character memory portion of vram adjust the tile cache
		if ((i_VRam1) < 0x2000)
			CacheTile (i_Value, i_VRam1);

		i_VRam1 += i_AddrInc;
		break;

	}

}

bool Ppu2C02::InitDDraw (HWND hWnd)
{
	
	LPDIRECTDRAW g_DD;
	DDSURFACEDESC2 g_Ddsd;

	// Fix up the surface descriptor
	ZeroMemory (&g_Ddsd, sizeof (DDSURFACEDESC2));
	g_Ddsd.dwSize = sizeof (DDSURFACEDESC2);

	long i_RetVal = DirectDrawCreate (NULL, &g_DD, NULL);
	if (i_RetVal != DD_OK)
		return false;

	// Create the dd7 object
	i_RetVal = g_DD->QueryInterface (IID_IDirectDraw7, (LPVOID *) &g_DDraw);
	if (i_RetVal != DD_OK)
		return false;

	// Kill the ddraw creator
	g_DD->Release ();

	// Set the cooperative level
	i_RetVal = g_DDraw->SetCooperativeLevel (hWnd, DDSCL_NORMAL);
	if (i_RetVal != DD_OK)
		return false;

	// Set up the primary surface
	g_Ddsd.dwFlags = DDSD_CAPS;
	g_Ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;
	i_RetVal = g_DDraw->CreateSurface (&g_Ddsd, &g_Primary, NULL);
	if (i_RetVal != DD_OK) {
		g_DDraw->Release ();
		return false;
	}

	// Create the clipper
	i_RetVal = g_DDraw->CreateClipper (0, &g_Clipper, NULL);
	if (i_RetVal != DD_OK) {
		g_Primary->Release ();
		g_DDraw->Release ();
		return false;
	}
	
	// Feed the clipper our window handle
	i_RetVal = g_Clipper->SetHWnd (0, hWnd);
	if (i_RetVal != DD_OK) {
		g_Primary->Release ();
		g_DDraw->Release ();
		return false;
	}

	// Attach the clipper to the primary surface
	i_RetVal = g_Primary->SetClipper (g_Clipper);
	if (i_RetVal != DD_OK) {
		g_Clipper->Release ();
		g_DDraw->Release ();
		g_Primary->Release ();
		return false;
	}

	// Set up the back buffer descriptor
	ZeroMemory (&g_Ddsd, sizeof (DDSURFACEDESC2));
	g_Ddsd.dwSize = sizeof (DDSURFACEDESC2);
	g_Ddsd.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH;
	g_Ddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN;
	g_Ddsd.dwHeight = g_Ddsd.dwWidth = 256;

	// Create the backbuffer
	i_RetVal = g_DDraw->CreateSurface (&g_Ddsd, &g_BackBuffer, NULL);
	if (i_RetVal != DD_OK) {
		g_Clipper->Release ();
		g_Primary->Release ();
		g_DDraw->Release ();
		return false;
	}
	
	// Everything's been created, we're good
	return true;

}

void Ppu2C02::Blit ()
{

	RECT t_Dest, t_Src;
	POINT t_Point = {0, 0};

	// If it's enabled, blit using DirectDraw
	if (b_DDraw) {

		DDSURFACEDESC2 g_Ddsd;
		ZeroMemory (&g_Ddsd, sizeof (DDSURFACEDESC2));
		g_Ddsd.dwSize = sizeof (DDSURFACEDESC2);

		// Get the position/area of our window
		ClientToScreen (h_Window, &t_Point);
		GetClientRect (h_Window, &t_Dest);
		OffsetRect (&t_Dest, t_Point.x, t_Point.y);
		SetRect (&t_Src, 0, 0, 256, 240);

		// Lock down the surface and transfer our pixels to it
		int i_RetVal = g_BackBuffer->Lock (NULL, &g_Ddsd, DDLOCK_WAIT, NULL);
		if (i_RetVal != DD_OK) {
			MessageBox (h_Window, "Unable to lock the back buffer. Quitting app!", "ERROR", MB_OK);
			PostQuitMessage (i_RetVal);
			return;
		}

		// Transfer the pixel data
		//g_Ddsd.lpSurface = &a_Screen;
		unsigned long *g_Screen = (unsigned long *) g_Ddsd.lpSurface;
		memcpy (g_Screen, &a_Screen, sizeof (unsigned long) * 65536);

		// Unlock the surface and blit it to the screen
		g_BackBuffer->Unlock (NULL);
		g_Primary->Blt (&t_Dest, g_BackBuffer, &t_Src, DDBLT_WAIT, NULL);

	}
	else {
		// Blit using GDI
		SetBitmapBits (h_Bitmap, 0x40000, &a_Screen);
		hDC = GetDC (h_Window);
		BitBlt (hDC, 0, 0, 256, 240, h_Buffer, 0, 0, SRCCOPY);
		ReleaseDC (h_Window, hDC);
	}

}

void Ppu2C02::CacheTile(unsigned char i_Value, unsigned short i_Addr)
{

	// Get the tile number
	unsigned short int i_Tile = i_Addr >> 4;

	i_Addr &= 15;
	a_Cache[i_Tile].i_PixelVal += i_Value;

	// If the first four bytes of the address are less than eight write the low byte	
	if (i_Addr < 8) {
		a_Cache[i_Tile].a_Pixels[(i_Addr * 8)] = (a_Cache[i_Tile].a_Pixels[(i_Addr * 8)] & 2) | ((i_Value & 128) >> 7);
		a_Cache[i_Tile].a_Pixels[(i_Addr * 8) + 1] = (a_Cache[i_Tile].a_Pixels[(i_Addr * 8) + 1] & 2) | ((i_Value & 64) >> 6);
		a_Cache[i_Tile].a_Pixels[(i_Addr * 8) + 2] = (a_Cache[i_Tile].a_Pixels[(i_Addr * 8) + 2] & 2) | ((i_Value & 32) >> 5);
		a_Cache[i_Tile].a_Pixels[(i_Addr * 8) + 3] = (a_Cache[i_Tile].a_Pixels[(i_Addr * 8) + 3] & 2) | ((i_Value & 16) >> 4);
		a_Cache[i_Tile].a_Pixels[(i_Addr * 8) + 4] = (a_Cache[i_Tile].a_Pixels[(i_Addr * 8) + 4] & 2) | ((i_Value & 8) >> 3);
		a_Cache[i_Tile].a_Pixels[(i_Addr * 8) + 5] = (a_Cache[i_Tile].a_Pixels[(i_Addr * 8) + 5] & 2) | ((i_Value & 4) >> 2);
		a_Cache[i_Tile].a_Pixels[(i_Addr * 8) + 6] = (a_Cache[i_Tile].a_Pixels[(i_Addr * 8) + 6] & 2) | ((i_Value & 2) >> 1);
		a_Cache[i_Tile].a_Pixels[(i_Addr * 8) + 7] = (a_Cache[i_Tile].a_Pixels[(i_Addr * 8) + 7] & 2) | (i_Value & 1);
	}
	else {

		i_Addr &= 7;
		a_Cache[i_Tile].a_Pixels[(i_Addr * 8)] = (a_Cache[i_Tile].a_Pixels[(i_Addr * 8)] & 1) | ((i_Value & 128) >> 6);
		a_Cache[i_Tile].a_Pixels[(i_Addr * 8) + 1] = (a_Cache[i_Tile].a_Pixels[(i_Addr * 8) + 1] & 1) | ((i_Value & 64) >> 5);
		a_Cache[i_Tile].a_Pixels[(i_Addr * 8) + 2] = (a_Cache[i_Tile].a_Pixels[(i_Addr * 8) + 2] & 1) | ((i_Value & 32) >> 4);
		a_Cache[i_Tile].a_Pixels[(i_Addr * 8) + 3] = (a_Cache[i_Tile].a_Pixels[(i_Addr * 8) + 3] & 1) | ((i_Value & 16) >> 3);
		a_Cache[i_Tile].a_Pixels[(i_Addr * 8) + 4] = (a_Cache[i_Tile].a_Pixels[(i_Addr * 8) + 4] & 1) | ((i_Value & 8) >> 2);
		a_Cache[i_Tile].a_Pixels[(i_Addr * 8) + 5] = (a_Cache[i_Tile].a_Pixels[(i_Addr * 8) + 5] & 1) | ((i_Value & 4) >> 1);
		a_Cache[i_Tile].a_Pixels[(i_Addr * 8) + 6] = (a_Cache[i_Tile].a_Pixels[(i_Addr * 8) + 6] & 1) | (i_Value & 2);
		a_Cache[i_Tile].a_Pixels[(i_Addr * 8) + 7] = (a_Cache[i_Tile].a_Pixels[(i_Addr * 8) + 7] & 1) | ((i_Value & 1) << 1);

	}

}