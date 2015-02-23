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

#ifndef _CPU6502_H_
#define _CPU6502_H_

#include <windows.h>
#include <stdio.h>
#include "cart.h"
#include "Ppu2C02.h"

/* Processor status macros */
#define GET_C(reg)   (reg & 0x01)
#define GET_Z(reg)   ((reg & 0x02) >> 1)
#define GET_N(reg)   ((reg & 0x80) >> 7)
#define GET_V(reg)   ((reg & 0x40) >> 6)

#define SET_C(reg)   (reg = (reg | 0x01))
#define SET_Z(reg)   (reg = (reg | 0x02))
#define SET_N(reg)   (reg = (reg | 0x80))
#define SET_V(reg)   (reg = (reg | 0x40))
#define SET_B(reg)   (reg = (reg | 0x10))
#define SET_I(reg)   (reg = (reg | 0x04))
#define SET_D(reg)   (reg = (reg | 0x08))

#define CLEAR_C(reg) (reg = (reg & 0xFE))
#define CLEAR_Z(reg) (reg = (reg & 0xFD))
#define CLEAR_N(reg) (reg = (reg & 0x7F))
#define CLEAR_V(reg) (reg = (reg & 0xBF))
#define CLEAR_I(reg) (reg = (reg & 0xFB))
#define CLEAR_D(reg) (reg = (reg & 0xF7))
#define CLEAR_B(reg) (reg = (reg & 0xEF))

enum INT_Type {
	I_NMI = 0,
	I_BRK,
	I_RST
};

/* Constants */
const unsigned long int SYSTEM_RAM = 0x8000;
const unsigned long int RESET_VECTOR = 0xFFFC;
#define STACK_RAM 0x100

#define KEY_A        0
#define KEY_B        1
#define KEY_SELECT   2
#define KEY_START    3
#define KEY_UP       4
#define KEY_DOWN     5
#define KEY_LEFT     6
#define KEY_RIGHT    7

extern iNES t_RomInfo;
extern unsigned char *a_PrgBanks;
extern unsigned char *a_ChrBanks;
extern unsigned char i_Mapper;
extern unsigned char i_Mirroring;

/* Mapper types */
#define NROM     0
#define MMC1     1
#define UNROM    2
#define MMC3     4

/* 6502 Class Definition */
class Cpu6502 {

	public:

		/* Registers */
		unsigned long int reg_C;
		unsigned char reg_A;
		unsigned char reg_X;
		unsigned char reg_Y;
		unsigned char reg_S;
		unsigned char reg_P;

		unsigned long int i_Executed;

		/* Functions */
		void Init (Ppu2C02 *t_PPU);
		~Cpu6502 ();
		void LoadPrg();
		void Reset(bool b_Soft);
		void Clk();
		inline unsigned char ReadRam(unsigned short int i_Addr);
		inline unsigned char ReadBank (unsigned short int i_Addr);
		inline void WriteRam(unsigned short int i_Addr, unsigned char i_Value);
		//inline unsigned char Pop();
		void RaiseInt (INT_Type t_Int);
		void SetKey (unsigned char i_Key, bool i_State);

		/* Stops CPU execution */
		bool b_Stop;

		// Sound stuff
		unsigned long int i_CpuClks;

	private:
unsigned long int i_Ops, i_Speed;
		/* RAM */
		unsigned char a_SysRam[SYSTEM_RAM];

		/* PRG banks */
		unsigned long int i_Bank1, i_Bank2, i_Bank3, i_Bank4;

		FILE *h_Debug;

		/* The PPU */
		Ppu2C02 *g_PPU;

		/* This keeps track of how many clocks the last op would take on a _real_ 6502 */
		unsigned short int i_Clocks;
		
		// Controller stuff
		unsigned char i_KeyCounter;
		unsigned char a_Keys[8];

		/* A few handy functions to make life just a tad easier */
		void ExecuteOp (unsigned char i_OpCode);

		// Mapper junk
		unsigned char i_SwapSrc;
		unsigned short i_SwapSize;
		unsigned short i_SwapDest;
		unsigned char i_ShiftReg;
		unsigned char i_MMCWrites;

		/* Memfetch and op routines */
		//inline unsigned char FM_ZeroPage(bool b_RetMem);
		//inline unsigned char FM_ZeroPageX(bool b_RetMem);
		//inline unsigned char FM_ZeroPageY(bool b_RetMem);
		//inline unsigned short int FM_Absolute(bool b_RetMem);
		//inline unsigned short int FM_AbsoluteX(bool b_RetMem);
		inline unsigned short int FM_AbsoluteY(bool b_RetMem);
		inline unsigned short int FM_Indirect();
		inline unsigned short int FM_IndexedIndirect(bool b_RetMem);
		inline unsigned short int FM_IndirectIndexed(bool b_RetMem);

		inline void ADC(unsigned char i_Value);
		
		inline void LDA(unsigned char i_Value);
		inline void LDX(unsigned char i_Value);
		inline void LDY(unsigned char i_Value);

		inline void SBC(unsigned char i_Value);
		inline void STR(unsigned char i_Value, unsigned short int i_Addr);
		inline void DEC(unsigned short int i_Addr);
		inline void INC(unsigned short int i_Addr);

		inline void BIT(unsigned char i_Value);
		
		inline void LSR(unsigned short int i_Addr);
		inline void ROL(unsigned short int i_Addr);
		inline void ROR(unsigned short int I_Addr);
		inline void ASL(unsigned short int i_Addr);

		inline void EOR(unsigned char i_Val);
		inline void AND(unsigned char i_Val);
		inline void ORA(unsigned char i_Val);

		inline void Compare (unsigned char i_Reg, unsigned char i_Value);
		inline void Branch (unsigned char i_Reg, unsigned char i_Value);
		inline void SetFlags(unsigned char i_Value);

		void HandleMapper (unsigned short i_Addr, unsigned char i_Value);

};

#endif