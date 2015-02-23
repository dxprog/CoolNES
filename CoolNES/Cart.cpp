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

#include "Cart.h"

bool Cart::LoadCart(char *s_FileName)
{

	/* Blank out the PRG and CHR banks */

	/* Open the file */
	FILE *h_File;
	if (fopen_s (&h_File, s_FileName, "rb"))
		return false;

	/* Read the header */
	fread (&t_RomInfo, sizeof (iNES), 1, h_File);
	if (t_RomInfo.s_Header[0] != 'N' || t_RomInfo.s_Header[3] != 0x1A)
		return false;

	/* Get the mapper type */
	i_Mapper = ((t_RomInfo.i_Control1 & 0xF0) >> 4) | (t_RomInfo.i_Control2 & 0xF0);

	/* Resize the prg banks and read them in */
	a_PrgBanks = (unsigned char *) malloc ((sizeof (unsigned char *) * PRG_BANK_SIZE) * t_RomInfo.i_PrgBanks);
	fread (a_PrgBanks, sizeof (unsigned char), PRG_BANK_SIZE * t_RomInfo.i_PrgBanks, h_File);
	
	/* Do the same for the chr banks */
	a_ChrBanks = (unsigned char *) malloc ((sizeof (unsigned char *) * CHR_BANK_SIZE) * t_RomInfo.i_ChrBanks);
	fread (a_ChrBanks, sizeof (unsigned char), CHR_BANK_SIZE * t_RomInfo.i_ChrBanks, h_File);

	fclose (h_File);
	return true;

}

Cart::~Cart ()
{
	free (a_PrgBanks);
	free (a_ChrBanks);
}