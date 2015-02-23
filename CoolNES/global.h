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


class Global {

	iNES t_RomInfo;
	unsigned char *a_PrgBanks;
	unsigned char *a_ChrBanks;

};