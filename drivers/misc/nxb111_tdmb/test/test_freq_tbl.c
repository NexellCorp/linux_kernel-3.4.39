#include "test.h"


static const DAB_FREQ_TBL_INFO dab_korea_metropolitan_tbl[] =
{
	{"8A", 181280}, {"8B", 183008}, {"8C", 184736},
	{"12A", 205280}, {"12B", 207008}, {"12C", 208736}
};

static const DAB_FREQ_TBL_INFO dab_korea_national_tbl[] =
{
	{"7A", 175280}, {"7B", 177008}, {"7A", 178736},
	{"8A", 181280}, {"8B", 183008}, {"8C", 184736},
	{"9A", 187280}, {"9B", 189008}, {"9C", 190736},
	{"10A", 193280}, {"10B", 195008}, {"10C", 196736},
	{"11A", 199280}, {"11B", 201008}, {"11C", 202736},
	{"12A", 205280}, {"12B", 207008}, {"12C", 208736},
	{"13A", 211280}, {"13B", 213008}, {"13C", 214736}
};

static const DAB_FREQ_TBL_INFO dab_band3_tbl[]=
{
	{"5A", 174928}, {"5B", 176640}, {"5C", 178352}, {"5D", 180064},
	{"6A", 181936}, {"6B", 183648}, {"6C", 185360}, {"6D", 187072},
	{"7A", 188928}, {"7B", 190640}, {"7C", 192352}, {"7D", 194064},
	{"8A", 195936}, {"8B", 197648}, {"8C", 199360}, {"8D", 201072},
	{"9A", 202928}, {"9B", 204640}, {"9C", 206352}, {"9D", 208064},
	{"10A", 209936}, {"10N", 210096}, {"10B", 211648}, {"10C", 213360},
	{"10D", 215072}, {"11A", 216928}, {"11N", 217088}, {"11B", 218640},
	{"11C", 220352}, {"11D", 222064}, {"12A", 223936}, {"12N", 224096},
	{"12B", 225648}, {"12C", 227360}, {"12D", 229072}, {"13A", 230784},
	{"13B", 232496}, {"13C", 234208}, {"13D", 235776},{"13E", 237488},
	{"13F", 239200}
};		  

#ifdef RTV_DAB_LBAND_ENABLED
static const DAB_FREQ_TBL_INFO  dab_lband_tbl[]=
{
	{"LA", 1452960}, {"LB", 1454672}, {"LC", 1456384}, {"LD", 1458096},
	{"LE", 1459808}, {"LF", 1461520}, {"LG", 1463232}, {"LH", 1464944},
	{"LI", 1466656}, {"LJ", 1468368}, {"LK", 1470080}, {"LL", 1471792},
	{"LM", 1473504}, {"LN", 1475216}, {"LO", 1476928}, {"LP", 1478640},
	{"LQ", 1480352}, {"LR", 1482064}, {"LS", 1483776}, {"LT", 1485488},
	{"LU", 1487200}, {"LV", 1488912}, {"LW", 1490624}
};
#endif


/*============== DAB ====================================*/
BOOL is_valid_dab_freq(unsigned int ch_freq_khz)
{
	int i, num_ch;
	
	num_ch = sizeof(dab_korea_national_tbl) / sizeof(DAB_FREQ_TBL_INFO);
	for(i=0; i<num_ch; i++)
	{
		if(ch_freq_khz == dab_korea_national_tbl[i].freq)
			return TRUE;
	}

#ifdef RTV_DAB_ENABLE
	num_ch = sizeof(dab_band3_tbl) / sizeof(DAB_FREQ_TBL_INFO);
	for(i=0; i<num_ch; i++)
	{
		if(ch_freq_khz == dab_band3_tbl[i].freq)
			return TRUE;
	}

	#ifdef RTV_DAB_LBAND_ENABLED
	num_ch = sizeof(dab_lband_tbl) / sizeof(DAB_FREQ_TBL_INFO);
	for(i=0; i<num_ch; i++)
	{
		if(ch_freq_khz == dab_lband_tbl[i].freq)
			return TRUE;
	}
	#endif
#endif /* #ifdef RTV_DAB_ENABLE */

	return FALSE;
}


unsigned int get_dab_freq_from_user(void)
{
	unsigned int ch_freq_khz;
	
	while(1)
	{
		DMSG0("Input Channel freq(ex. 174928):");
		scanf("%u", &ch_freq_khz);			
		CLEAR_STDIN;
	
		if(is_valid_dab_freq(ch_freq_khz) == TRUE)
			break;
	
		DMSG0("[DAB] Invalid frequency\n");
	}

	return ch_freq_khz;
}


const DAB_FREQ_TBL_INFO *get_dab_freq_table_from_user(unsigned int *num_freq)
{
	int key;
	const DAB_FREQ_TBL_INFO *freq_tbl_ptr;
		
	while(1)
	{
		DMSG0("=========== Select DAB Frequency ============\n");
		DMSG0("\t0: Korea Metropolitan\n");
		DMSG0("\t1: Korea National\n");
#ifdef RTV_DAB_ENABLE		
		DMSG0("\t2: DAB Band III\n");
	#ifdef RTV_DAB_LBAND_ENABLED		
		DMSG0("\t3: DAB L-Band\n");
	#endif
#endif
		DMSG0("\tq or Q: return\n");
		DMSG0("======================================\n");

		key = getc(stdin);				
		CLEAR_STDIN;

		switch(key)
		{
			case '0' :
				*num_freq = sizeof(dab_korea_metropolitan_tbl) / sizeof(DAB_FREQ_TBL_INFO);
				freq_tbl_ptr = dab_korea_metropolitan_tbl;
				return freq_tbl_ptr;

			case '1' :
				*num_freq = sizeof(dab_korea_national_tbl) / sizeof(DAB_FREQ_TBL_INFO);
				freq_tbl_ptr = dab_korea_national_tbl;
				return freq_tbl_ptr;

#ifdef RTV_DAB_ENABLE
			case '2' :
				*num_freq = sizeof(dab_band3_tbl) / sizeof(DAB_FREQ_TBL_INFO);
				freq_tbl_ptr = dab_band3_tbl;
				return freq_tbl_ptr;

	#ifdef RTV_DAB_LBAND_ENABLED
			case '3' :
				*num_freq = sizeof(dab_lband_tbl) / sizeof(DAB_FREQ_TBL_INFO);
				freq_tbl_ptr = dab_lband_tbl;
				return freq_tbl_ptr;
	#endif
#endif /* RTV_DAB_ENABLE */

			case 'q':
			case 'Q':
				return NULL;
		}
	}

	return NULL;
}
/*============== end DAB ====================================*/



