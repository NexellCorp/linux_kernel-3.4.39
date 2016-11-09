#ifndef __NXB110TV_FICDEC_INTERNAL_H__
#define __NXB110TV_FICDEC_INTERNAL_H__

/********************/
/*   Header Files   */
/********************/
#include <string.h>

#include "nxb110tv_ficdec.h"


#define RTV_FAIL       (-1)
#define RTV_UNLOCK     (-10)
#define RTV_OK         0
#define RTV_SCAN_OK    1
#define RTV_LOCK_CHECK 10
#define RTV_NOTHING    2


#define FIC_GOING       0
#define FIC_LABEL       0x01
#define FIC_APP_TYPE    0x10
#define FIC_DONE        0x11
#define FIC_CRC_ERR       0x44


/**************************/
/*     FIC Definition     */
/**************************/
#define MSC_STREAM_AUDIO   0x00
#define MSC_STREAM_DATA    0x01
#define FIDC               0x02
#define MSC_PACKET_DATA    0x03

#define LABEL_NUM          17    // Ensemble, service, service component label number
#define MAX_SERV_COMP      64    // maximum service component number in Ensemble channel
#define MAX_SUB_CH_NUM     12    // maximum service component number in one service
#define MAX_SERVICE_NUM    20    // maximum service number in one ensemble
#define USER_APP_NUM       6     // maximum number of user applications

#define ALL_FLAG_MATCH	   0x11  // get FIC Time information complete
#define TIME_FLAG          0x01  // get FIC type0 extension10 complete
#define LTO_FLAG           0x10  // get FIC type0 extension9 complete

#define Unspec_DATA        0
#define TMC_Type           1
#define EWS_Type           2
#define ITTS_Type          3
#define Paging_Type        4
#define TDC_Type           5
#define KDMB_Type          24
#define	Em_IP_Type         59
#define	MOT_Type           60
#define	PS_noDSCTy         61

/****************************/
/*    FIC Data structure    */
/****************************/
/*! Date and Time information for Application user */
typedef struct 
{
	U32 MJD;
	U8  LSI;
	U8  conf_ind;
	U8  utc_flag;
	U8  apm_flag;
	U8  time_flag;
	U8  get_flag;
	U16 years;
	U8  months_dec;
	char months_ste[4];
	char weeks[4];
	U8  days;
	U8  hours;
	U8  minutes;
	U8  seconds;
	U16 milliseconds;
	U8  LTO;          ///Local Time Offset
}DATE_TIME_INFO;

/*! Service Component Descriptor for Application user */
typedef struct
{
	///MCI Information 
	S32 BIT_RATE;   ///T0/1 Service Component BitRate(kbit/s) 
	S32 P_L;        ///TO/1 Service Component Protection Level 
	S32 START_Addr; ///TO/1 Service Component CU Start Address 
	S32 SUB_CH_Size;///TO/1 Service Component CU Size
	U8  TMID;       ///T0/2 Transport Mechanism Identifier. 00:stream audio, 01:stream data, 10:FIDC, 11:packet data
	U8  ASCTy;      ///T0/2 Audio Service Component type 00:foreground, 01:background, 10:multi-channel 
	U8  SubChid;    ///T0/2 Sub-channel Identifier
	U8  P_S;        ///T0/2 1:primary, 0:secondary 
	U8  CA_flag;    ///T0/2 flag shall indicate whether access control 0:no access, 1:access
	U8  DSCTy;      ///T0/2, T0/3 Data Service Component Type
	U8  FIDCid;     ///T0/2 Fast Information Data Channel 
	U8  TCid;       ///T0/2 FIDC Type Component Identifier
	U8  Ext;        ///T0/2 FIDC Extension
	U16 SCid;       ///T0/2 Service Component Identifier
	U8  DG_flag;    ///T0/3 Data group flag
	U16 Packet_add; ///T0/3 The address of packet in which the service component is carried
	U16 CA_Org;     ///T0/3 Conditional Access Organization
	U8  FEC_scheme; ///T0/14 FEC Frame
	//SI Information
	U8	language;   ///T0/5 Service Component Language. TS 101 576, table 9 and 10
	U8  charset;    ///T1/4 Service component label identify a character set
	char Label[LABEL_NUM]; ///T1/4 Service component label
	//Service componet structure
	U32 Sid;        ///T0/2 Service Identifier for service component linkage
	U8  SCidS;      ///T0/8 Service Component Identifier within the service
	//User application information
	U8  Num_User_App;                       ///T0/13 Number of User Application
	U16 User_APP_Type[USER_APP_NUM];	    ///T0/13 User Application Type
	U8  User_APP_data_length[USER_APP_NUM]; ///T0/13 User Application data length
	U8  User_APP_data[24];                  ///T0/13 User Application data
}SVR_COM_DESP;

/*! Service Descriptor for Application user */
typedef struct
{
	//MCI Information
	U32 Sid;            ///T0/2 Service Identifier
	U8  Country_id;     ///T0/2 TS 101 756[23], tables 3 to 7
	U32 Service_ref;    ///T0/2 Indicate the number of the service
	U8  ECC;            ///T0/2 Extended country code
	U8  Local_flag;     ///T0/2 0:whole ensemble 1:partial ensemble
	U8  CAID;           ///T0/2 Conditional Access Id
	U8  Num_ser_comp;   ///T0/2 Max 12 for 16-bits SIds, Max 11 for 32-bits SIds
	U8  P_D;            ///Type0 Programme(16-bit SId)/Data(32-bit SId) service 
	//SI Information
	U8  label_flag;     ///T1/1 Character flag field
	U8  charset;        ///T1/1, T1/5 identify a character set
	char  Label[LABEL_NUM]; ///T1/1, T1/5 Service label
	//Program type
	U8	int_code;       ///T0/17 basic Programme Type category. TS 101 576, International table
	U8  ser_comp_num[MAX_SUB_CH_NUM]; /// Service Component Index number in a Service
}SERVICE_DESC;

/*! FIDC Descriptor for Application user */
typedef struct
{
	U8  Sub_Region[11];
}FIDC_EWS_Region;

typedef struct
{
	U8  EWS_current_segmemt;
	U8  EWS_total_segmemt;
	U8  EWS_Message_ID;
	SS8  EWS_category[4];
	U8  EWS_priority;
	U32 EWS_time_MJD;
	U8  EWS_time_Hours;
	U8  EWS_time_Minutes;
	U8  EWS_region_form;
	U8  EWS_region_num;
	U8  EWS_Rev;
	FIDC_EWS_Region  EWS_Region[15];
	U8  EWS_short_sentence[409];
}FIDC_DESC;

/*! Ensemble Descriptor for Application user */
typedef struct
{
	//COMMON Information
	U32 svr_num;        ///Service number in a Ensemble
	U32 svr_comp_num;   ///Service Component number in a Ensemble
	U32 label_num;      ///Service Label number in a Ensemble
	//MCI Information
	U16 id;             ///T0/0 Ensemble Identifier
	U8  change_flag;    ///T0/0 Indicator to be change in the sub-channel or service organization
	U8  Alarm_flag;     ///T0/0 Ensemble Alarm message
	//SI Information
	U8  charset;        ///T1/0 Character flag field
	char  Label[LABEL_NUM]; ///T1/0 Ensemble label
	U32 freq;           ///Ensemble Frequency
	U8  label_flag;     ///Ensemble label Flag

	DATE_TIME_INFO date_time_info;
	SERVICE_DESC svr_desc[MAX_SERVICE_NUM];
	SVR_COM_DESP svr_comp[MAX_SERV_COMP];
	FIDC_DESC fidc_desc;

} ENSEMBLE_DESC;


/*****************/
/*    EXTERNS    */
/*****************/
extern char *PROGRAM_TYPE_CODE16[32];
extern char *USER_APP_TYPE_CODE[11];
extern char *FIDC_EXT_CODE[3];
extern char *ASCTy[3];
extern char *DSCTy[11];
extern char *ANNOUNCEMENT_TYPE_CODE[12];
extern S32 SUBCH_UEP_TABLE[64][3];
extern char *WEEK_TABLE[8];
extern char *MONTH_TABLE[12];
extern ENSEMBLE_DESC ENS_DESC[], NEXT_ENS_DESC[];
extern ENSEMBLE_DESC ENS_DESC1, NEXT_ENS_DESC1;
extern ENSEMBLE_DESC ENS_DESC2, NEXT_ENS_DESC2;
extern char *EWS_CATEGORY[67][3];
extern char *EWS_PRIORITY_TABLE[4];
extern char *EWS_REGION_FORM_TABLE[4];
extern char *EWS_OFFICIAL_ORGANIZATION_TABLE[4];


//////////////////
/********************/
/*  FIC Definition  */
/********************/
#define PN_FIB_END_MARKER (0xFF)

/****************************/
/*   FIC Parser function    */
/****************************/
S32  FIB_Init_Dec(int demod_no, U8 *);
S32  MCI_SI_DEC(int demod_no, U8 );
S32  SI_LABEL_DEC1(int demod_no, U8 );
S32  SI_LABEL_DEC2(int demod_no, U8 );
S32  FIDC_DEC(int demod_no, U8 );
S32  CA_DEC(int demod_no, U8 );
S32  RESERVED1(int demod_no, U8 );
S32  RESERVED2(int demod_no, U8 );
S32  RESERVED3(int demod_no, U8 );

/*****************************/
/*  FIG Data Type structure  */
/*****************************/
typedef struct  
{
	U8 type; 
	U8 length;
	U8 *data; 
	U8 byte_cnt;
	U8 bit_cnt;
}FIG_DATA;

/****************************/
/*  FIG type 0 data field   */
/****************************/
typedef struct
{
	U8 C_N;	///current or the next version of the multiplex configuration. 0:current, 1:next
	U8 OE;	///Other Ensemble. 0:this ensemble, 1:other ensemble
	U8 P_D;	///Porgramme/data service Id. 0:16-bit SId, 1:32-bit SId
	U8 Ext;	///Extension
}FIG_TYPE0;

/* Ensemble Information */
typedef struct
{
	U16 Eid;              ///Ensemble Id
	U8  Country_ID;       ///Country Id
	U32 Ensemble_Ref;     ///Ensemble reference
	U8  Change_flag;      ///Change flag. 00:no, 01:sub-channel, 10:service, 11:sub-channel & service
	U8  AI_flag;          ///Alarm announcement	
	U8  CIF_Count0;       ///High part module-20 counter
	U8  CIF_Count1;       ///Low part module-250 counter
	U8  Occurence_Change; ///Indicate the value of the lower part of the CIF counter from which the new configuration
}FIG_TYPE0_Ext0;

/* Structure of the sub-channel organization field */
typedef struct
{
	U8  SubChid;         ///Sub-channel Id
	U32 StartAdd;        ///Address the first CU of the sub-channel
	U8  S_L_form;        ///Indicate whether the short or the long form. 0:short, 1:long
	U32 Size_Protection;
	U8  Table_sw;        ///Table 7 of UEP
	U8  Table_index;	
	U8  Option;          ///000: data rate 8n kbit/s. 001: data rate 32n kbit/s
	U8  Protection_Level;
	U32 Sub_ch_size;
}FIG_TYPE0_Ext1;

/* Structure of the service organization field */
typedef struct
{
	U8  TMID;      ///Transport mechanism Id
	U8  ASCTy;     ///Audio Service component type
	U8  SubChid;   ///Sub-channel Id
	U8  P_S;       ///Primary/Secondary
	U8  CA_flag;   ///CAS flag
	U8  DSCTy;     ///Data Service Component type
	U8  FIDCid;    ///FIDC Id
	U8  TCid;      ///FIDC Type component Id
	U8  Ext;       ///FIDC Extension
	U16 SCid;      ///Service component Id
}FIG_TYPE0_Ext2_ser_comp_des;

/* Basic service and service component definition structure  */
typedef struct
{
	U32 Sid;         ///Service Id
	U8  Country_id;  ///Country Id
	U32 Service_ref; ///Service reference 
	U8  ECC;         ///Extended country code
	U8  Local_flag;  ///Local flag
	U8  CAID;        ///Conditional Access Id
	U8  Num_ser_comp;///Number of service components
	FIG_TYPE0_Ext2_ser_comp_des svr_comp_des[MAX_SERV_COMP];
}FIG_TYPE0_Ext2;

/* Structure of the service component in packet mode */
typedef struct
{
	U16 SCid;        ///Service Component Id
	U8  CA_Org_flag; ///Conditional Access flag
	U8  DG_flag;     ///Data Group flag
	U8  DSCTy;       ///Data Service Component type		
	U8  SubChid;     ///Sub-channel Id
	U16 Packet_add;  ///Packet address
	U16 CA_Org;      ///Conditional Access Organization  
}FIG_TYPE0_Ext3;

/* Structure of the service component field in Stream mode or FIC */
typedef struct
{
	U8  M_F;         ///0:MSC and SubChId, 1:FIC and FIDCId
	U8  SubChid;
	U8  FIDCid;
	U16 CA_Org;		
}FIG_TYPE0_Ext4;

/* Structure of the service component language field */
typedef struct
{
	U8  L_S_flag;     ///0:short form, 1:long form
	U8  MSC_FIC_flag; ///0:MSC in stream mode, 1:FIC
	U8  SubChid;
	U8  FIDCid;
	U16 SCid;
	U8  Language;     ///Audio/data service component language (Ex. Korean = 0x65)
}FIG_TYPE0_Ext5;			

/* Service Linking Information */
typedef struct
{
	U8  id_list_flag;
	U8  LA;
	U8  S_H;
	U8  ILS;
	U32 LSN;
	U8  id_list_usage;
	U8  idLQ;
	U8  Shd;
	U8  Num_ids;
	U16 id[12];
	U8  ECC[12];
	U32 Sid[12];
}FIG_TYPE0_Ext6;

/* Structure of the service component global definition field */
typedef struct
{
	U32 Sid;
	U8  Ext_flag;
	U8  SCidS;
	U8  L_S_flag;
	U8  MSC_FIC_flag;
	U8  SubChid;
	U8  FIDCid;
	U32 SCid;
}FIG_TYPE0_Ext8;

/* Structure of Country, LTO International field */
typedef struct
{
	U8  Ext_flag;
	U8  LTO_unique;
	U8  Ensemble_LTO;   ///Ensemble Local Time Offset
	U8  Ensemble_ECC;   ///Ensemble Extended Country Code
	U8  Inter_Table_ID; ///International Table ID
	U8  Num_Ser;        ///Number of Service	
	U8  LTO;            ///Local Time Offset
	U8  ECC;            ///Extended Country Code
	U32 Sid[11];
}FIG_TYPE0_Ext9;

/* Structure of the data and time field */
typedef struct
{
	U32 MJD;           ///Modified Julian Date
	U8  LSI;           ///Leap Second Indicator
	U8  Conf_ind;      ///Confidence Indicator
	U8  UTC_flag;
	U32 UTC;           ///Co-ordinated Universal Time
	U8  Hours;
	U8  Minutes;
	U8  Seconds;
	U16 Milliseconds;
}FIG_TYPE0_Ext10;

/* Structure of the Region Definition */
typedef struct
{
	U8  GATy;
	U8  G_E_flag;
	U8  Upper_part;
	U8  Lower_part;
	U8  length_TII_list;
	U8  Mainid[12];
	U8  Length_Subid_list;
	U8  Subid[36];
	U32 Latitude_Coarse;
	U32 Longitude_coarse;
	U32 Extent_Latitude;
	U32 Extent_Longitude;
}FIG_TYPE0_Ext11;

/* Structure of the User Application Information */
typedef struct
{
	U32 Sid;
	U8  SCidS;
	U8  Num_User_App;
	U16 User_APP_Type[6];
	U8  User_APP_data_length[6];
	U8  CA_flag;
	U8  CA_Org_flag;
	U8  X_PAD_App_Ty;
	U8  DG_flag;
	U8  DSCTy;
	U16 CA_Org;
	U8  User_APP_data[24];
}FIG_TYPE0_Ext13;

/* FEC sub-channel organization */
typedef struct
{
	U8  SubChid;     ///Sub-channel Id
	U8  FEC_scheme;
}FIG_TYPE0_Ext14;

/* Program Number structure  */
typedef struct
{
	U16 Sid;
	U16 PNum;
	U8  Continuation_flag;
	U8  Update_flag;
	U16 New_Sid;
	U16 New_PNum;
}FIG_TYPE0_Ext16;

/*	Program Type structure	*/
typedef struct
{
	U16 Sid;        ///Service Id
	U8  S_D;        ///Static or Dynamic
	U8  P_S;        ///Primary or Secondary
	U8  L_flag;     ///Language Flag
	U8  CC_flag;    ///Complimentary Code Flag
	U8  Language;   ///language of Audio
	U8  Int_code;   ///Basic Program Type
	U8  Comp_code;  ///Specific Program Type
}FIG_TYPE0_Ext17;

/* Announcement support */
typedef struct
{
	U16 Sid;
	U16 ASU_flags;
	U8  Num_clusters;
	U8  Cluster_ID[23];
}FIG_TYPE0_Ext18;

/* Announcement switching */
typedef struct
{
	U8  Cluster_ID;
	U16 ASW_flags;
	U8  New_flag;
	U8  Region_flag;
	U8  SubChid;
	U8  Regionid_Lower_Part;
}FIG_TYPE0_Ext19;

/* Frequency Information */
typedef struct
{
	U16 ResionID;
	U8  Length_of_FI_list;
	U16 id_field;
	U8  R_M;
	U8  Continuity_flag;
	U8  Length_Freq_list;
	U8  Control_field[5];
	U8  id_field2[4];
	U32 Freq_a[5];
	U8  Freq_b[17];
	U16 Freq_c[8];
	U16 Freq_d[7];
}FIG_TYPE0_Ext21;

/* Transmitter Identification Information (TII) database */
typedef struct
{
	U8  M_S;
	U8  Mainid;
	U32 Latitude_coarse;
	U32 Longitude_coarse;
	U8  Latitude_fine;
	U8  Longitude_fine;
	U8  Num_Subid_fields;
	U8  Subid[4];
	U16 TD[4];
	U16 Latitude_offset[4];
	U16 Longitude_offset[4];
}FIG_TYPE0_Ext22;

/* Other Ensemble Service */
typedef struct
{
	U32 Sid;
	U8  CAid;
	U8  Number_Eids;
	U16 Eid[12];
}FIG_TYPE0_Ext24;

/* Other Ensemble Announcement support */
typedef struct
{
	U32 Sid;
	U32 ASU_flag;
	U8  Number_Eids;
	U8  Eid[12];
}FIG_TYPE0_Ext25;

/* Other Ensemble Announcement switching */
typedef struct
{
	U8  Cluster_id_Current_Ensemble;
	U32 Asw_flags;
	U8  New_flag;
	U8  Region_flag;
	U8  Region_id_current_Ensemble;
	U32 Eid_Other_Ensemble;
	U8  Cluster_id_other_Ensemble;
	U8  Region_id_Oter_Ensemble;
}FIG_TYPE0_Ext26;

/* FM Announcement support */
typedef struct
{
	U32 Sid;
	U8  Number_PI_Code;
	U32 PI[12];
}FIG_TYPE0_Ext27;

/* FM Announcement switching */
typedef struct
{
	U8  Cluster_id_Current_Ensemble;
	U8  New_flag;
	U8  Region_id_Current_Ensemble;
	U32 PI;
}FIG_TYPE0_Ext28;

/* FIC re-direction */
typedef struct
{
	U32 FIG0_flag_field;
	U8  FIG1_flag_field;
	U8  FIG2_flag_field;
}FIG_TYPE0_Ext31;

/****************************/
/*  FIG type 5 data field   */
/****************************/
typedef struct
{
	U8 D1;
	U8 D2;
	U8 TCid;
	U8 Ext;
}FIG_TYPE5;

/* Paging */
typedef struct
{
	U8  SubChid;
	U16 Packet_add;
	U8  F1;
	U8  F2;
	U16 LFN;
	U8  F3;
	U16 Time;
	U8  CAid;
	U16 CA_Org;
	U32 Paging_user_group;
}FIG_TYPE5_Ext0;

/* TMC */
typedef struct
{
	U32 TMC_User_Message[30];
	U16 TMC_System_Message[30];
}FIG_TYPE5_Ext1;

/* EWS */
typedef struct
{
	U8  Sub_Region[11];
}FIG_EWS_Region;

typedef struct
{
	U8  current_segmemt;
	U8  total_segmemt;
	U8  Message_ID;
	SS8  category[4];
	U8  priority;
	U32 time_MJD;
	U8  time_Hours;
	U8  time_Minutes;
	U8  region_form;
	U8  region_num;
	U8  Rev;
	FIG_EWS_Region Region[15];
	U8  data[409];
}FIG_TYPE5_Ext2;

/****************************/
/*  FIG type 6 data field   */
/****************************/
typedef struct
{
	U8  C_N;
	U8  OE;
	U8  P_D;
	U8  LEF;
	U8  ShortCASysId;
	U32 Sid;
	U16 CASysId;
	U16 CAIntChar;
}FIG_TYPE6;

/***************************/
/*  Function declarations  */
/***************************/

// FIG TYPE 0 function 
S32 Get_FIG0_EXT0(int demod_no, U8 fic_cmd, U8 P_D, U8 C_N);
S32 Get_FIG0_EXT1(int demod_no, U8 fic_cmd, U8 P_D, U8 C_N);
S32 Get_FIG0_EXT2(int demod_no, U8 fic_cmd, U8 P_D, U8 C_N);
S32 Get_FIG0_EXT3(int demod_no, U8 fic_cmd, U8 P_D, U8 C_N);
S32 Get_FIG0_EXT4(int demod_no, U8 fic_cmd, U8 P_D, U8 C_N);
S32 Get_FIG0_EXT5(int demod_no, U8 fic_cmd, U8 P_D, U8 C_N);
S32 Get_FIG0_EXT6(int demod_no, U8 fic_cmd, U8 P_D, U8 C_N);
S32 Get_FIG0_EXT7(int demod_no, U8 fic_cmd, U8 P_D, U8 C_N);
S32 Get_FIG0_EXT8(int demod_no, U8 fic_cmd, U8 P_D, U8 C_N);
S32 Get_FIG0_EXT9(int demod_no, U8 fic_cmd, U8 P_D, U8 C_N);
S32 Get_FIG0_EXT10(int demod_no, U8 fic_cmd, U8 P_D, U8 C_N);
S32 Get_FIG0_EXT11(int demod_no, U8 fic_cmd, U8 P_D, U8 C_N);
S32 Get_FIG0_EXT12(int demod_no, U8 fic_cmd, U8 P_D, U8 C_N);
S32 Get_FIG0_EXT13(int demod_no, U8 fic_cmd, U8 P_D, U8 C_N);
S32 Get_FIG0_EXT14(int demod_no, U8 fic_cmd, U8 P_D, U8 C_N);
S32 Get_FIG0_EXT15(int demod_no, U8 fic_cmd, U8 P_D, U8 C_N);
S32 Get_FIG0_EXT16(int demod_no, U8 fic_cmd, U8 P_D, U8 C_N);
S32 Get_FIG0_EXT17(int demod_no, U8 fic_cmd, U8 P_D, U8 C_N);
S32 Get_FIG0_EXT18(int demod_no, U8 fic_cmd, U8 P_D, U8 C_N);
S32 Get_FIG0_EXT19(int demod_no, U8 fic_cmd, U8 P_D, U8 C_N);
S32 Get_FIG0_EXT20(int demod_no, U8 fic_cmd, U8 P_D, U8 C_N);
S32 Get_FIG0_EXT21(int demod_no, U8 fic_cmd, U8 P_D, U8 C_N);
S32 Get_FIG0_EXT22(int demod_no, U8 fic_cmd, U8 P_D, U8 C_N);
S32 Get_FIG0_EXT23(int demod_no, U8 fic_cmd, U8 P_D, U8 C_N);
S32 Get_FIG0_EXT24(int demod_no, U8 fic_cmd, U8 P_D, U8 C_N);
S32 Get_FIG0_EXT25(int demod_no, U8 fic_cmd, U8 P_D, U8 C_N);
S32 Get_FIG0_EXT26(int demod_no, U8 fic_cmd, U8 P_D, U8 C_N);
S32 Get_FIG0_EXT27(int demod_no, U8 fic_cmd, U8 P_D, U8 C_N);
S32 Get_FIG0_EXT28(int demod_no, U8 fic_cmd, U8 P_D, U8 C_N);
S32 Get_FIG0_EXT29(int demod_no, U8 fic_cmd, U8 P_D, U8 C_N);
S32 Get_FIG0_EXT30(int demod_no, U8 fic_cmd, U8 P_D, U8 C_N);
S32 Get_FIG0_EXT31(int demod_no, U8 fic_cmd, U8 P_D, U8 C_N);

// FIG TYPE 1 function 
S32 Get_FIG1_EXT0(int demod_no, U8 fic_cmd, U8 Char_Set);
S32 Get_FIG1_EXT1(int demod_no, U8 fic_cmd, U8 Char_Set);
S32 Get_FIG1_EXT2(int demod_no, U8 fic_cmd, U8 Char_Set);
S32 Get_FIG1_EXT3(int demod_no, U8 fic_cmd, U8 Char_Set);
S32 Get_FIG1_EXT4(int demod_no, U8 fic_cmd, U8 Char_Set);
S32 Get_FIG1_EXT5(int demod_no, U8 fic_cmd, U8 Char_Set);
S32 Get_FIG1_EXT6(int demod_no, U8 fic_cmd, U8 Char_Set);
S32 Get_FIG1_EXT7(int demod_no, U8 fic_cmd, U8 Char_Set);

// FIG TYPE 2 function 
S32 Get_FIG2_EXT0(int demod_no, U8 fic_cmd, U8 Seg_Index);
S32 Get_FIG2_EXT1(int demod_no, U8 fic_cmd, U8 Seg_Index);
S32 Get_FIG2_EXT2(int demod_no, U8 fic_cmd, U8 Seg_Index);
S32 Get_FIG2_EXT3(int demod_no, U8 fic_cmd, U8 Seg_Index);
S32 Get_FIG2_EXT4(int demod_no, U8 fic_cmd, U8 Seg_Index);
S32 Get_FIG2_EXT5(int demod_no, U8 fic_cmd, U8 Seg_Index);
S32 Get_FIG2_EXT6(int demod_no, U8 fic_cmd, U8 Seg_Index);
S32 Get_FIG2_EXT7(int demod_no, U8 fic_cmd, U8 Seg_Index);

// FIG TYPE 5 function 
S32 Get_FIG5_EXT0(int demod_no, U8 D1, U8 D2, U8 fic_cmd, U8 TCid);
S32 Get_FIG5_EXT1(int demod_no, U8 D1, U8 D2, U8 fic_cmd, U8 TCid);
S32 Get_FIG5_EXT2(int demod_no, U8 D1, U8 D2, U8 fic_cmd, U8 TCid);



U8 GET_SUBCH_INFO(FIG_TYPE0_Ext1 *type0_ext1, S32 *BIT_RATE, S32 *SUB_CH_Size, S32 *P_L);
U8 GET_DATE_TIME(int demod_no, DATE_TIME_INFO *time_desc);
U8 GET_EWS_TIME(DATE_TIME_INFO *time_desc);



#endif /* __NXB110TV_FICDEC_INTERNAL_H__ */
