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

#ifndef _CART_H_
#define _CART_H_

#include <stdio.h>
#include <stdlib.h>

/* Constants */
#define PRG_BANK_SIZE 0x4000
#define CHR_BANK_SIZE 0x2000

/* iNES file header */
struct iNES {
	char s_Header[4];
	unsigned char i_PrgBanks;
	unsigned char i_ChrBanks;
	unsigned char i_Control1;
	unsigned char i_Control2;
	unsigned char i_RamBanks;
	unsigned char i_Unused;
	char s_Reserved[6];
};

extern iNES t_RomInfo;
extern unsigned char *a_PrgBanks;
extern unsigned char *a_ChrBanks;
extern unsigned char i_Mapper;

#define HEADER_SIZE 0xA0

class Cart {

	public:

		bool LoadCart (char *s_FileName);
		unsigned char * ReadChr (int i_Bank);
		~Cart();

};

#endif