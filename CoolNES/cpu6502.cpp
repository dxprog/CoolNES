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

#include "cpu6502.h"

void Cpu6502::Init (Ppu2C02 *t_PPU)
{

	g_PPU = t_PPU;
	b_Stop = false;

}

Cpu6502::~Cpu6502 ()
{	

	FILE *h_File;
	fopen_s (&h_File, "C:\\sys_ram.bin", "wb");
	fwrite (&a_SysRam, sizeof (unsigned char), SYSTEM_RAM, h_File);
	fclose (h_File);

	fopen_s (&h_File, "C:\\performance.txt", "w");
	fprintf (h_File, "Operations executed: %d\n", i_Ops);
	fprintf (h_File, "Average operations per millisecond: %f\n", (double) (i_Ops / (GetTickCount () - i_Speed)));
	fclose (h_File);

}

/********************** MEMORY FETCHING ROUTINES ***********************/
inline void Cpu6502::SetFlags (unsigned char i_Value) 
{ 
	if (!i_Value) SET_Z(reg_P); else CLEAR_Z(reg_P); if (i_Value & 0x80) SET_N(reg_P); else CLEAR_N(reg_P);
}

#define FM_ZeroPage ReadRam (ReadOp)
#define FM_ZeroPageMem ReadOp

#define FM_ZeroPageX ReadRam ((ReadOp + reg_X) & 0xFF)
#define FM_ZeroPageXMem (ReadOp + reg_X) & 0xFF

#define FM_ZeroPageY ReadRam ((ReadOp + reg_Y) & 0xFF)
#define FM_ZeroPageYMem (ReadOp + reg_Y) & 0xFF

#define FM_Absolute ReadRam (ReadOp + (ReadOp << 8))
#define FM_AbsoluteMem ReadOp + (ReadOp << 8)

#define FM_AbsoluteX ReadRam (ReadOp + (ReadOp << 8) + reg_X)
#define FM_AbsoluteXMem (ReadOp + (ReadOp << 8) + reg_X)

#define FM_AbsoluteY ReadRam (ReadOp + (ReadOp << 8) + reg_Y)
#define FM_AbsoluteYMem (ReadOp + (ReadOp << 8) + reg_Y)

/*************************************************************
* Function: ReadOp                                           *
* Description: Reads the value at the program counter,       *
*              returns it and increments the counter         *
*************************************************************/
#define ReadOp ReadBank (reg_C++)

/*************************************************************
* Function: ReadRam                                          *
* Description: Reads a value from memory and invokes special *
*              handling on PPU and controller addresses      *
*************************************************************/
inline unsigned char Cpu6502::ReadRam(unsigned short int i_Addr)
{
	
	/* If this value is between 2000 and 2007 throw it all over to the PPU */
	if (i_Addr >= 0x2000 && i_Addr <= 0x3FFF)
		return g_PPU->ReadRam (i_Addr);

	/* If the address is 4016 return controller info */
	if (i_Addr == 0x4016) {
		// Increment the key counter
		i_KeyCounter &= 7;
		i_KeyCounter++;
		return a_Keys[i_KeyCounter - 1] + 0x40;
	}

	/* If it's reading from PRG ROM return the value form ReadBank */
	if (i_Addr > 0x7FFF)
		return ReadBank (i_Addr);

	/* Used for mirroring */
	if (i_Addr < 0x1800)
		i_Addr &= 0x07FF;

	/* Return the value at the address requested */
	return a_SysRam[i_Addr];

}

/*************************************************************
* Function: WriteRam                                         *
* Description: Writes a value to memory and invokes special  *
*              handling on PPU and controller addresses      *
*************************************************************/
inline void Cpu6502::WriteRam(unsigned short i_Addr, unsigned char i_Value)
{

	if (i_Addr < 0x8000) {
		
		// Write the value
		a_SysRam[i_Addr] = i_Value;
		
		// Write the value to RAM
		if (i_Addr < 0x1800) {
			i_Addr &= 0x7FF;
			a_SysRam[i_Addr] = i_Value;
		}

		// Invoke the PPU if necessary
		else if (i_Addr >= 0x2000 && i_Addr <= 0x3FFF)
			g_PPU->WriteMem (i_Addr, i_Value);

		// If the address is 4014 initiate a DMA transfer of the data starting at i_Value to sprite memory
		else if (i_Addr == 0x4014)
			memcpy (&g_PPU->a_SprRam, &a_SysRam[0x100 * i_Value], 0xFF);

		// If the address is 4016 reset the controller counter
		else if (i_Addr == 0x4016)
			i_KeyCounter = 0;

	}
	else
		HandleMapper (i_Addr, i_Value);

}

/*************************************************************
* Function: Clk                                              *
* Description: The main CPU clock cycle routine              *
*************************************************************/
void Cpu6502::Clk()
{

	/* Only continue if the CPU hasn't been halted */
	if (b_Stop == false) {

		/* Get the next opcode and execute it */
		//i_Ops++;
		ExecuteOp (ReadOp);

	}

}

/*************************************************************
* Function: ReadBank                                         *
* Description: Reads a value from PRG ROM                    *
*************************************************************/
inline unsigned char Cpu6502::ReadBank (unsigned short int i_Addr)
{

	// Figure out which bank to choose from depending on the address
	switch (i_Addr & 0xF000) {
	case 0x8000:
	case 0x9000:
		return a_PrgBanks[(i_Addr & 0x1FFF) + i_Bank1];
	case 0xA000:
	case 0xB000:
		return a_PrgBanks[(i_Addr & 0x1FFF) + i_Bank2];
	case 0xC000:
	case 0xD000:
		return a_PrgBanks[(i_Addr & 0x1FFF) + i_Bank3];
	case 0xE000:
	case 0xF000:
		return a_PrgBanks[(i_Addr & 0x1FFF) + i_Bank4];
	}

}

/*************************************************************
* Function: Reset                                            *
* Description: Resets the game                               *
*************************************************************/
void Cpu6502::Reset(bool b_Soft)
{
	
	unsigned char i_HiByte, i_LoByte;

	LoadPrg ();

	/* Set the program counter to the address in the reset vector */
	i_LoByte = ReadRam(RESET_VECTOR);
	i_HiByte = ReadRam(RESET_VECTOR + 1);
	reg_C = (i_HiByte << 8) + i_LoByte;

	/* If this is a hard reset blank out memory and reset the stack pointer */
	if (!b_Soft) {
		reg_S = 0xFF;
		reg_P = 0x20;
	}

}

/*************************************************************
* Function: LoadPrg                                          *
* Description: Loads PRG data into memory                    *
*************************************************************/
void Cpu6502::LoadPrg()
{

	/* Load the banks according to the mapper type (MMC and NROM only at the moment) */
	switch (i_Mapper) {
	case NROM:

		/* If there's only one bank of PRG-ROM load it into 8000 and C000... */
		if (t_RomInfo.i_PrgBanks == 1) {
			i_Bank1 = 0;
			i_Bank2 = 0x2000;
			i_Bank3 = 0;
			i_Bank4 = 0x2000;
		}
		/* ... otherwise just load in the first two banks */
		else {
			i_Bank1 = 0;
			i_Bank2 = 0x2000;
			i_Bank3 = 0x4000;
			i_Bank4 = 0x6000;
		}

		break;

	case MMC1:
	case MMC3:
	case UNROM:

		/* Set 8000-BFFF to the first bank and C000-FFFF to the last */
		i_Bank1 = 0;
		i_Bank2 = 0x2000;
		i_Bank3 = (t_RomInfo.i_PrgBanks - 1) * PRG_BANK_SIZE;
		i_Bank4 = i_Bank3 + 0x2000;
		
		break;

	default:

		MessageBox (NULL, "This mapper is not supported yet!", "BAD MAPPER", MB_OK);
		PostQuitMessage (i_Mapper);

	}

}

/*************************************************************
* Function: Push                                             *
* Description: Pushes a value onto the stack                 *
*************************************************************/
#define Push(i_Value) a_SysRam[STACK_RAM + reg_S] = i_Value; reg_S--;

/*************************************************************
* Function: Pop                                              *
* Description: Pops a value off the stack                    *
*************************************************************/
#define Pop a_SysRam[STACK_RAM + ++reg_S]
/* inline unsigned char Cpu6502::Pop()
{

	/* Get the value and increment the stack pointer
	reg_S++;
	return a_SysRam[STACK_RAM + reg_S];

} */


/*************************************************************
* Function: SetKey                                           *
* Description: Sets a controller button state                *
*************************************************************/
void Cpu6502::SetKey (unsigned char i_Key, bool i_State)
{
	if (i_State)
		a_Keys[i_Key] = 1;
	else
		a_Keys[i_Key] = 0;
}

/*************************************************************
* Function: RaiseInt                                         *
* Description: Raises an interrupt                           *
*************************************************************/
void Cpu6502::RaiseInt(INT_Type t_Int)
{

	// Push the counter and status to the stack
	Push (reg_C >> 8);
	Push (reg_C & 0xFF);
	Push (reg_P);

	// Get the interrupt address vector
	switch (t_Int)  {
	case I_NMI:
		reg_C = (ReadRam (0xFFFB) << 8) + ReadRam (0xFFFA);
		break;
	case I_BRK:
		reg_C = (ReadRam (0xFFFF) << 8) + ReadRam (0xFFFE);
		SET_B (reg_P);
		break;
	case I_RST:
		Reset (true);
		break;
	}

	i_Clocks += 7;

}

/*************************************************************
* Function: ExecuteOp                                        *
* Description: Executes the specified opcode                 *
*************************************************************/
void Cpu6502::ExecuteOp(unsigned char i_OpCode)
{
	
	unsigned char i_HiByte, i_LoByte;
	unsigned short int i_Addr;
	
	//fopen_s (&h_Debug, "C:\\debug.out", "a");
	//fprintf (h_Debug, "%X: %X - A: %X\tP: %X\n", reg_C - 1, i_OpCode, reg_A, reg_P);
	//fclose (h_Debug);

	i_Executed++;

	/* Pick which instruction we need */
	switch (i_OpCode) {
		
		/*** FLAG SET ROUTINES ***/
		case 0xF8:
			SET_D (reg_P);
			break;

		case 0x78:
			SET_I (reg_P);
			break;

		case 0x38:
			SET_C (reg_P);
			break;

		case 0xB8:
			CLEAR_V (reg_P);
			break;

		case 0x58:
			CLEAR_I (reg_P);
			break;

		case 0xD8:
			CLEAR_D (reg_P);
			break;

		case 0x18:
			CLEAR_C (reg_P);
			break;

		/* REGISTER TRANSFER ROUTINES */
		case 0x9A:
			reg_S = reg_X; // TXS
			i_Clocks = 2;
			break;

		case 0xBA:
			reg_X = reg_S; // TSX
			SetFlags (reg_X);
			i_Clocks = 2;
			break;

		case 0x8A:
			reg_A = reg_X; // TXA
			SetFlags (reg_A);
			i_Clocks = 2;
			break;

		case 0x98:
			reg_A = reg_Y; // TYA
			SetFlags (reg_A);
			i_Clocks = 2;
			break;

		case 0xAA: // TAX
			reg_X = reg_A;
			SetFlags (reg_X);
			i_Clocks = 2;
			break;

		case 0xA8: // TAY
			reg_Y = reg_A;
			SetFlags (reg_Y);
			i_Clocks = 2;
			break;


		/* PHP */
		case 0x08:
			Push (reg_P);
			break;
			
		case 0x28: // PLP
			reg_P = Pop;
			break;

		/* PLA */
		case 0x68:
			reg_A = Pop;
			SetFlags (reg_A);
			break;

		/* JUMP ROUTINES */
		case 0x4C:
			reg_C = FM_AbsoluteMem;
			break;
		case 0x6C:
			reg_C = FM_Indirect ();
			break;
		case 0x20: // JSR
			i_Addr = FM_AbsoluteMem;
			reg_C--;
			Push (reg_C >> 8);
			Push (reg_C & 0xFF);
			reg_C = i_Addr;
			i_Clocks = 6;
			break;

		/* BRK */
		case 0x00:
			RaiseInt (I_BRK);
			i_Clocks = 2;
			break;

		/* RTS */
		case 0x60:
			i_LoByte = Pop;
			i_HiByte = Pop;
			reg_C = (i_HiByte << 8) + i_LoByte + 1;
			i_Clocks = 6;
			break;

		/* RTI */
		case 0x40:
			reg_P = Pop;
			i_LoByte = Pop;
			i_HiByte = Pop;
			reg_C = (i_HiByte << 8) + i_LoByte;
			CLEAR_B (reg_P);
			i_Clocks = 6;
			break;

		/* NOP */
		case 0xEA:
			i_Clocks = 2;
			break;

		/* PHA */
		case 0x48:
			Push (reg_A);
			break;

		/* BRANCH ROUTINES */
		case 0x10: // BPL
			Branch (GET_N(reg_P), 0);
			break;
		case 0xD0: // BNE
			Branch (GET_Z(reg_P), 0);
			break;
		case 0x30: // BMI
			Branch (GET_N(reg_P), 1);
			break;
		case 0xF0: // BEQ
			Branch (GET_Z(reg_P), 1);
			break;
		case 0xB0: // BCS
			Branch (GET_C(reg_P), 1);
			break;
		case 0x90: // BCC
			Branch (GET_C(reg_P), 0);
			break;
		case 0x50: // BVC
			Branch (GET_V(reg_P), 0);
			break;
		case 0x70: // BVS
			Branch (GET_V(reg_P), 1);
			break;

		/* ADC */
		case 0x69:
			ADC (ReadOp);
			break;
		case 0x65:
			ADC (FM_ZeroPage);
			break;
		case 0x75:
			ADC (FM_ZeroPageX);
			break;
		case 0x6D:
			ADC (FM_Absolute);
			break;
		case 0x7D:
			ADC (FM_AbsoluteX);
			break;
		case 0x79:
			ADC (FM_AbsoluteY);
			break;
		case 0x61:
			ADC (FM_IndexedIndirect (false));
			break;
		case 0x71:
			ADC (FM_IndirectIndexed (false));
			break;

		/* LDA */
		case 0xA9:
			LDA (ReadOp);
			break;
		case 0xA5:
			LDA (FM_ZeroPage);
			break;
		case 0xB5:
			LDA (FM_ZeroPageX);
			break;
		case 0xAD:
			LDA (FM_Absolute);
			break;
		case 0xBD:
			LDA (FM_AbsoluteX);
			break;
		case 0xB9:
			LDA (FM_AbsoluteY);
			break;
		case 0xA1:
			LDA (FM_IndexedIndirect (false));
			break;
		case 0xB1:
			LDA (FM_IndirectIndexed (false));
			break;

		/* LDX */
		case 0xA2:
			LDX (ReadOp);
			break;
		case 0xA6:
			LDX (FM_ZeroPage);
			break;
		case 0xB6:
			LDX (FM_ZeroPageY);
			break;
		case 0xAE:
			LDX (FM_Absolute);
			break;
		case 0xBE:
			LDX (FM_AbsoluteY);
			break;

		/* LDY */
		case 0xA0:
			LDY (ReadOp);
			break;
		case 0xA4:
			LDY (FM_ZeroPage);
			break;
		case 0xB4:
			LDY (FM_ZeroPageX);
			break;
		case 0xAC:
			LDY (FM_Absolute);
			break;
		case 0xBC:
			LDY (FM_AbsoluteX);
			break;

		/* STA */
		case 0x85:
			STR (reg_A, FM_ZeroPageMem);
			break;
		case 0x95:
			STR (reg_A, FM_ZeroPageXMem);
			break;
		case 0x8D:
			STR (reg_A, FM_AbsoluteMem);
			break;
		case 0x9D:
			STR (reg_A, FM_AbsoluteXMem);
			break;
		case 0x99:
			STR (reg_A, FM_AbsoluteYMem);
			break;
		case 0x81:
			STR (reg_A, FM_IndexedIndirect (true));
			break;
		case 0x91:
			STR (reg_A, FM_IndirectIndexed (true));
			break;

		/* STX */
		case 0x86:
			STR (reg_X, FM_ZeroPageMem);
			break;
		case 0x96:
			STR (reg_X, FM_ZeroPageYMem);
			break;
		case 0x8E:
			STR (reg_X, FM_AbsoluteMem);
			break;

		case 0xCA: // DEX
			reg_X--;
			SetFlags (reg_X);
			i_Clocks = 2;
			break;
		case 0x88: // DEY
			reg_Y--;
			SetFlags (reg_Y);
			i_Clocks = 2;
			break;
		case 0xE8: // INX
			reg_X++;
			SetFlags (reg_X);
			i_Clocks = 2;
			break;
		case 0xC8: // INY
			reg_Y++;
			SetFlags (reg_Y);
			i_Clocks = 2;
			break;

		/* STY */
		case 0x84:
			STR (reg_Y, FM_ZeroPageMem);
			break;
		case 0x94:
			STR (reg_Y, FM_ZeroPageXMem);
			break;
		case 0x8C:
			STR (reg_Y, FM_AbsoluteMem);
			break;

		/* SBC */
		case 0xE9:
			SBC (ReadOp);
			break;
		case 0xE5:
			SBC (FM_ZeroPage);
			break;
		case 0xF5:
			SBC (FM_ZeroPageX);
			break;
		case 0xED:
			SBC (FM_Absolute);
			break;
		case 0xFD:
			SBC (FM_AbsoluteX);
			break;
		case 0xF9:
			SBC (FM_AbsoluteY);
			break;
		case 0xE1:
			SBC (FM_IndexedIndirect (false));
			break;
		case 0xF1:
			SBC (FM_IndirectIndexed (false));
			break;

		/* COMPARE ROUTINES */

		/* CPX */
		case 0xE0:
			Compare (reg_X, ReadOp);
			break;
		case 0xE4:
			Compare (reg_X, FM_ZeroPage);
			break;
		case 0xEC:
			Compare (reg_X, FM_Absolute);
			break;

		/* BIT */
		case 0x24:
			BIT (FM_ZeroPage);
			break;
		case 0x2C:
			BIT (FM_Absolute);
			break;

		/* CPY */
		case 0xC0:
			Compare (reg_Y, ReadOp);
			break;
		case 0xC4:
			Compare (reg_Y, FM_ZeroPage);
			break;
		case 0xCC:
			Compare (reg_Y, FM_Absolute);
			break;

		/* CMP */
		case 0xC9:
			Compare (reg_A, ReadOp);
			break;
		case 0xC5:
			Compare (reg_A, FM_ZeroPage);
			break;
		case 0xD5:
			Compare (reg_A, FM_ZeroPageX);
			break;
		case 0xCD:
			Compare (reg_A, FM_Absolute);
			break;
		case 0xDD:
			Compare (reg_A, FM_AbsoluteX);
			break;
		case 0xD9:
			Compare (reg_A, FM_AbsoluteY);
			break;
		case 0xC1:
			Compare (reg_A, FM_IndexedIndirect (false));
			break;
		case 0xD1:
			Compare (reg_A, FM_IndirectIndexed (false));
			break;

		/* LSR */
		case 0x4A:
			LSR (0xFFFF);
			break;
		case 0x46:
			LSR (FM_ZeroPageMem);
			break;
		case 0x56:
			LSR (FM_ZeroPageXMem);
			break;
		case 0x4E:
			LSR (FM_AbsoluteMem);
			break;
		case 0x5E:
			LSR (FM_AbsoluteXMem);
			break;

		/* ROL */
		case 0x2A:
			ROL (0xFFFF);
			break;
		case 0x26:
			ROL (FM_ZeroPageMem);
			break;
		case 0x36:
			ROL (FM_ZeroPageXMem);
			break;
		case 0x2E:
			ROL (FM_AbsoluteMem);
			break;
		case 0x3E:
			ROL (FM_AbsoluteXMem);
			break;

		/* EOR */
		case 0x49:
			EOR (ReadOp);
			break;
		case 0x45:
			EOR (FM_ZeroPage);
			break;
		case 0x55:
			EOR (FM_ZeroPageX);
			break;
		case 0x4D:
			EOR (FM_Absolute);
			break;
		case 0x5D:
			EOR (FM_AbsoluteX);
			break;
		case 0x59:
			EOR (FM_AbsoluteY);
			break;
		case 0x41:
			EOR (FM_IndexedIndirect (false));
			break;
		case 0x51:
			EOR (FM_IndirectIndexed (false));
			break;

		/* AND */
		case 0x29:
			AND (ReadOp);
			break;
		case 0x25:
			AND (FM_ZeroPage);
			break;
		case 0x35:
			AND (FM_ZeroPageX);
			break;
		case 0x2D:
			AND (FM_Absolute);
			break;
		case 0x3D:
			AND (FM_AbsoluteX);
			break;
		case 0x39:
			AND (FM_AbsoluteY);
			break;
		case 0x21:
			AND (FM_IndexedIndirect (false));
			break;
		case 0x31:
			AND (FM_IndirectIndexed (false));
			break;

		/* AND */
		case 0x09:
			ORA (ReadOp);
			break;
		case 0x05:
			ORA (FM_ZeroPage);
			break;
		case 0x15:
			ORA (FM_ZeroPageX);
			break;
		case 0x0D:
			ORA (FM_Absolute);
			break;
		case 0x1D:
			ORA (FM_AbsoluteX);
			break;
		case 0x19:
			ORA (FM_AbsoluteY);
			break;
		case 0x01:
			ORA (FM_IndexedIndirect (false));
			break;
		case 0x11:
			ORA (FM_IndirectIndexed (false));
			break;

		/* ROR */
		case 0x6A:
			ROR (0xFFFF);
			break;
		case 0x66:
			ROR (FM_ZeroPageMem);
			break;
		case 0x76:
			ROR (FM_ZeroPageXMem);
			break;
		case 0x6E:
			ROR (FM_AbsoluteMem);
			break;
		case 0x7E:
			ROR (FM_AbsoluteXMem);
			break;

		/* ASL */
		case 0x0A:
			ASL (0xFFFF);
			break;
		case 0x06:
			ASL (FM_ZeroPageMem);
			break;
		case 0x16:
			ASL (FM_ZeroPageXMem);
			break;
		case 0x0E:
			ASL (FM_AbsoluteMem);
			break;
		case 0x1E:
			ASL (FM_AbsoluteXMem);
			break;


		/* INC */
		case 0xE6:
			INC (FM_ZeroPageMem);
			break;
		case 0xF6:
			INC (FM_ZeroPageXMem);
			break;
		case 0xEE:
			INC (FM_AbsoluteMem);
			break;
		case 0xFE:
			INC (FM_AbsoluteXMem);
			break;

		/* DEC */
		case 0xC6:
			DEC (FM_ZeroPageMem);
			break;
		case 0xD6:
			DEC (FM_ZeroPageXMem);
			break;
		case 0xCE:
			DEC (FM_AbsoluteMem);
			break;
		case 0xDE:
			DEC (FM_AbsoluteXMem);
			break;


		default:

			char s_Text[255] = "";
			sprintf (s_Text, "Unknown opcode: %X\nAddress: %X", i_OpCode, reg_C);
			MessageBox (NULL, s_Text, "HALT", MB_OK);
			b_Stop = true;
			i_Executed--;

	}

}

inline unsigned short int Cpu6502::FM_Indirect()
{
	
	unsigned char i_LoByte = ReadOp, i_HiByte = ReadOp;
	unsigned short int i_Addr = (i_HiByte << 8) + i_LoByte;

	/* Due to an error in the 6502, when an indirect is called on the page boundry (0xnnFF) it gets the high
	   byte from FF bytes earlier (0xnn00) */
	if (i_LoByte == 0xFF) {
		i_LoByte = ReadRam(i_Addr);
		i_HiByte = ReadRam(i_Addr - 0xFF);
	}
	else {
		i_LoByte = ReadRam(i_Addr);
		i_HiByte = ReadRam(i_Addr + 1);
	}

	return (i_HiByte << 8) + i_LoByte;

}

inline unsigned short int Cpu6502::FM_IndexedIndirect(bool b_RetMem)
{

	unsigned char i_HiByte, i_LoByte;
	unsigned short int i_Addr = ReadOp;

	i_Clocks = 5;

	/* Get the lo and hi bytes stored at the address (plus X) */
	i_LoByte = ReadRam((i_Addr + reg_X) & 0xFF);
	i_HiByte = ReadRam(((i_Addr + reg_X) + 1) & 0xFF);
	
	if (b_RetMem)
		return (i_HiByte << 8) + i_LoByte;
	else
		return ReadRam((i_HiByte << 8) + i_LoByte);

}

inline unsigned short int Cpu6502::FM_IndirectIndexed(bool b_RetMem)
{

	unsigned char i_HiByte, i_LoByte;
	unsigned short int i_Addr = ReadOp;

	i_Clocks = 5;

	/* Get the lo and hi bytes stored at the address */
	i_LoByte = ReadRam(i_Addr);
	i_HiByte = ReadRam((i_Addr + 1) & 0xFF);
	
	if (b_RetMem)
		return (i_HiByte << 8) + i_LoByte + reg_Y;
	else
		return ReadRam((i_HiByte << 8) + i_LoByte + reg_Y);

}

/***************************** OP ROUTINES *****************************/

inline void Cpu6502::ADC(unsigned char i_Value)
{
	
	/* Add everything together */
	unsigned int i_Result = reg_A + i_Value + (reg_P & 1);

	/* Set the various flags */
	if (i_Result > 0xFF) SET_C(reg_P); else	CLEAR_C(reg_P);
	
	if (~(reg_A ^ i_Value) & (reg_A ^ i_Result) & 0x80)
		SET_V(reg_P);
	else
		CLEAR_V(reg_P);

	/* Store the value in the accumulator */
	reg_A = i_Result & 0xFF;
	if (!reg_A) SET_Z (reg_P); else CLEAR_Z (reg_P);
	if (reg_A & 0x80) SET_N (reg_P); else CLEAR_N (reg_P);
	i_Clocks += 1;

}

inline void Cpu6502::DEC (unsigned short int i_Addr)
{

	unsigned char i_Temp = ReadRam(i_Addr) - 1;
	WriteRam (i_Addr, i_Temp);
	SetFlags (i_Temp);
	i_Clocks = 2;

}

inline void Cpu6502::INC (unsigned short int i_Addr)
{

	unsigned char i_Temp = ReadRam(i_Addr) + 1;
	WriteRam (i_Addr, i_Temp);
	SetFlags (i_Temp);
	i_Clocks = 2;

}

inline void Cpu6502::LDA(unsigned char i_Value)
{

	/* Store the value in the accumulator and set the flags */
	reg_A = i_Value;
	SetFlags(i_Value);
	i_Clocks += 1;

}

inline void Cpu6502::LDX(unsigned char i_Value)
{

	/* Store the value in the accumulator and set the flags */
	reg_X = i_Value;
	SetFlags(i_Value);
	i_Clocks += 1;

}

inline void Cpu6502::LDY(unsigned char i_Value)
{

	/* Store the value in the accumulator and set the flags */
	reg_Y = i_Value;
	SetFlags(i_Value);
	i_Clocks += 1;

}

inline void Cpu6502::STR(unsigned char i_Value, unsigned short int i_Addr)
{

	/* Store the accumulator in the address supplied */
	WriteRam(i_Addr, i_Value);
	i_Clocks += 1;

}

inline void Cpu6502::SBC(unsigned char i_Value)
{
	
	/* Subtract the value from the accumulator and the not of the carry bit */
	unsigned int i_Result = reg_A - i_Value - (GET_C (reg_P) ? 0 : 1);

	if (i_Result < 0x100) SET_C(reg_P);	else CLEAR_C(reg_P);

	if (((reg_A ^ i_Result) & 0x80) && ((reg_A ^ i_Value) & 0x80))
		SET_V(reg_P);
	else
		CLEAR_V(reg_P);

	if (!i_Result) SET_Z (reg_P); else CLEAR_Z (reg_P);
	if (i_Result & 0x80) SET_N (reg_P); else CLEAR_N (reg_P);

	reg_A = i_Result & 0xFF;
	// SetFlags (reg_A);
	i_Clocks += 1;

}

inline void Cpu6502::Compare(unsigned char i_Reg, unsigned char i_Value)
{

	unsigned int i_Temp = i_Reg - i_Value;

	if (i_Temp < 0x100) SET_C (reg_P); else CLEAR_C (reg_P);
	if (!(i_Temp & 0xFF)) SET_Z (reg_P); else CLEAR_Z (reg_P);
	if (i_Temp & 0x80) SET_N (reg_P); else CLEAR_N (reg_P);

}

inline void Cpu6502::Branch(unsigned char i_Reg, unsigned char i_Value)
{

	/* Figure up the absolute address to jump to */
	signed char i_Jump = (signed char) ReadOp;

	/* If the condition evaluates true, adjust the program counter to the jump position */
	i_Clocks = 2;
	if (i_Reg == i_Value) {
		reg_C += i_Jump;
		i_Clocks++;
	}

}

inline void Cpu6502::BIT (unsigned char i_Value)
{

	SetFlags (i_Value & reg_A);
	reg_P = (reg_P & 0x3F) + (i_Value & 0xC0);

}

inline void Cpu6502::LSR (unsigned short int i_Addr)
{

	unsigned char i_Var;
	if (i_Addr == 0xFFFF)
		i_Var = reg_A;
	else
		i_Var = ReadRam(i_Addr);

	// Copy bit 0 into the carry flag
	reg_P = (reg_P & 0xFE) | (i_Var & 1);
	i_Var = i_Var >> 1;
	if (i_Addr == 0xFFFF)
		reg_A = i_Var;
	else
		WriteRam (i_Addr, i_Var);
	SetFlags (i_Var);
	i_Clocks += 1;

}

inline void Cpu6502::ROL (unsigned short int i_Addr)
{

	unsigned char i_OldC = reg_P, i_Var;
	if (i_Addr == 0xFFFF)
		i_Var = reg_A;
	else
		i_Var = ReadRam(i_Addr);

	// Copy bit 7 into the carry flag
	reg_P = (reg_P & 0xFE) | (i_Var >> 7);

	// Shift the bits left and plop the old carry bit into bit 0
	i_Var = ((i_Var << 1) & 0xFE) | (i_OldC & 1);
	if (i_Addr == 0xFFFF)
		reg_A = i_Var;
	else
		WriteRam (i_Addr, i_Var);
	SetFlags (i_Var);
	i_Clocks += 1;

}

inline void Cpu6502::ASL (unsigned short int i_Addr)
{

	unsigned char i_Var;
	if (i_Addr == 0xFFFF)
		i_Var = reg_A;
	else
		i_Var = ReadRam (i_Addr);

	// Copy bit 7 into the carry flag
	reg_P = (reg_P & 0xFE) | (i_Var >> 7);

	// Shift the bits left
	i_Var = ((i_Var << 1) & 0xFF);
	if (i_Addr == 0xFFFF)
		reg_A = i_Var;
	else
		WriteRam (i_Addr, i_Var);
	SetFlags (i_Var);
	i_Clocks += 1;

}

inline void Cpu6502::ROR (unsigned short int i_Addr)
{

	unsigned char i_OldC = reg_P, i_Var;

	if (i_Addr == 0xFFFF)
		i_Var = reg_A;
	else
		i_Var = ReadRam (i_Addr);

	// Save the old bit 0 in the carry flag
	reg_P = (reg_P & 0xFE) | (i_Var & 1);

	// Shift the bits right and stuff the old carry bit up front
	i_Var = (i_Var >> 1) | ((i_OldC & 1) << 7);
	if (i_Addr == 0xFFFF)
		reg_A = i_Var;
	else
		WriteRam (i_Addr, i_Var);
	SetFlags (i_Var);
	i_Clocks += 1;

}

inline void Cpu6502::EOR (unsigned char i_Val)
{

	// XOR the value in mem with the accumulator
	reg_A ^= i_Val;
	SetFlags (reg_A);

}

inline void Cpu6502::AND (unsigned char i_Val)
{

	// AND the value in mem with the accumulator
	reg_A &= i_Val;
	SetFlags (reg_A);

}

inline void Cpu6502::ORA (unsigned char i_Val)
{

	// OR the value in mem with the accumulator
	reg_A |= i_Val;
	SetFlags (reg_A);

}

void Cpu6502::HandleMapper (unsigned short i_Addr, unsigned char i_Value)
{

	unsigned char i_Temp;
	unsigned short i_PrgRom, i_ChrRom;

	// See what mapper we've got
	switch (i_Mapper) {
	case MMC1:

		i_MMCWrites++;
		i_ShiftReg += ((i_Value & 1) << (i_MMCWrites - 1));

		// If bit 7 of the value passed is set reset the writes and shift registers
		if (i_Value & 0x80) {
			i_MMCWrites = 0;
			i_ShiftReg = 0;
		}

		if (i_MMCWrites == 5) {

			i_Value = i_ShiftReg;

			if (i_Addr < 0xA000) {
				
				// If bit 3 is set we're loading 16K from the address specified at bit 2
				if (i_Value & 8) {
					i_SwapSize = 1;
					if (i_Value & 4)
						i_SwapDest = 0x8000;
					else
						i_SwapDest = 0xC000;
				}
				else {
					i_SwapSize = 2;
					i_SwapDest = 0x8000;
				}

				// Set the mirroring
				switch (i_Value & 3) {
				case 0:
				case 1:
					break;
				case 2:
					i_Mirroring = MIR_VERT;
					break;
				case 3:
					i_Mirroring = MIR_HORZ;
					break;
				}

			}
			else if (i_Addr < 0xC000) {

			}
			else if (i_Addr < 0xE000) {

			}
			else {

				// Swap out the data from the position specified
				if (i_SwapSize > 0) {
					i_SwapSrc = i_Value & 7;
					switch (i_SwapDest) {
					case 0x8000:
						i_Bank1 = i_SwapSrc * PRG_BANK_SIZE;
						i_Bank2 = (i_SwapSrc * PRG_BANK_SIZE) + 0x2000;
						if (i_SwapSize == 2) {
							i_Bank3 = ((i_SwapSrc * PRG_BANK_SIZE) + 0x2000) + 0x2000;
							i_Bank4 = (((i_SwapSrc * PRG_BANK_SIZE) + 0x2000) + 0x2000) + 0x2000;
						}
						break;
					case 0xC000:
						i_Bank3 = i_SwapSrc * PRG_BANK_SIZE;
						i_Bank4 = (i_SwapSrc * PRG_BANK_SIZE) + 0x2000;
						break;
					}
				}

			}
			
			i_ShiftReg = 0;
			i_MMCWrites = 0;

		}

		break;
	case MMC3:

		switch (i_Addr) {
		case 0x8000:
			
			// Used to tell what bank of what needs to be swapped
			i_Temp = i_Value & 7;

			// Save the swap locations of PRG and CHR roms
			if (i_Value & 0x40) {
				i_PrgRom = 0xC000;
				// Load the second to last bank into 8000
				for (int i = 0; i < 0x2000; i++)
					a_SysRam[0x8000 + i] = a_PrgBanks[((t_RomInfo.i_PrgBanks * PRG_BANK_SIZE) - 0x2000) + i];

			}
			else
				i_PrgRom = 0x8000;
			
			// Based on what we got earlier, set info for swapping
			switch (i_Temp) {
			case 0:
			case 1:
			case 2:
			case 3:
			case 4:
			case 5:
				i_SwapSize = 0;
				i_SwapDest = 0;
				break;

			case 6:
				i_SwapSize = 0x2000;
				i_SwapDest = i_PrgRom;
				break;
			case 7:
				i_SwapSize = 0x2000;
				i_SwapDest = 0xA000;
				break;
			}
			break;

		case 0x8001:

			// Transfer the data from the bank to the locaiton setup earlier
			if (i_SwapSize > 0) {

			}


			break;
		}
		break;

	case UNROM:

		// Swap the bank specified into 8000
		i_Bank1 = (i_Value * PRG_BANK_SIZE);
		i_Bank2 = i_Bank1 + 0x2000;

		break;

	}

}