/////////////////////////////////
#include "INC_INCLUDES.h"
/////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//#define BOARD_CONFIG_AESOP		//Insignal V210 EVK
//#define BOARD_CONFIG_MANGO		//crz-tech V210 EVK
//#define BOARD_CONFIG_THINKWARE	//Thinkware V210 K9-A
//#define BOARD_CONFIG_ORIGEN  		//Insignal exynos4210(V310) EVK
//#define BOARD_CONFIG_MV_V310		//MicroVision:exynos4210
//#define BOARD_CONFIG_MV_4412  	//MicroVision:exynos4412

///////////////////////////////////////////////////////////////////////////////

#include <mach/gpio.h>
#include <mach/irqs.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <asm-generic/gpio.h>
#include "INC_SPI.h"
#include <linux/mutex.h>
#include <linux/module.h>

static DEFINE_MUTEX(INCio_mutex);


#define	INC_DEV_NAME	"Tdmb"
#define	INC_DEV_MAJOR      125
#define INC_DEV_MINOR      0

#define	INT_OCCUR_SIG	0x0A
#define INT_EXIT_SIG	0x01

extern ST_SUBCH_INFO	g_stCHInfo;
struct spi_device	 	gInc_spi;
struct task_struct*  	g_pThread;
ST_INC_Interrupt	 	g_stINCInt;

wait_queue_head_t 	 	WaitQueue_Read;
//struct mutex 			inc_mutex;		// <asj>2/28/2014 오후 4:23:19

unsigned int	g_TaskFlag = 0;
unsigned char 	g_StreamBuff[INC_MPI_MAX_BUFF] = {0};	// 1024*8
unsigned int  	g_nThreadcnt = 0;
unsigned short	g_ReadDataLength = 0;  
unsigned char 	g_ReadQ;
unsigned char	g_DEV_MAJOR_NUM;
unsigned short	g_DataLength = 0;


///////////////////////////////////////////////////////////////////////////////
// ioctl message
///////////////////////////////////////////////////////////////////////////////
#define TDMB_MAGIC 'T'
#define TDMB_IOCTL_MAX 8

#define DMB_T_STOP 		_IOW(TDMB_MAGIC,  0, int)
#define DMB_T_SCAN 		_IOWR(TDMB_MAGIC, 1, BB_CH_INFO)
#define DMB_T_PLAY 		_IOW(TDMB_MAGIC,  2, BB_CH_PLAY)
#define DMB_T_SIGNAL		_IOR(TDMB_MAGIC,  3, BB_CH_SIGNAL)
#define DMB_T_DATASIZE 	_IOWR(TDMB_MAGIC, 4, BB_CH_DATASIZE)
#define DMB_T_TEST1		_IOR(TDMB_MAGIC,  5, int)
#define DMB_T_VERSION		_IOR(TDMB_MAGIC,  6, BB_CH_VERSION)
#define DMB_T_CHECKSYNC	_IOW(TDMB_MAGIC,  7, int)

///////////////////////////////////////////////////////////////////////////////
// ioctl Data
///////////////////////////////////////////////////////////////////////////////

typedef struct
{
	unsigned short	nBbpStatus;
	unsigned int	uiRFFreq;
	unsigned short	uiEnsembleID;
	unsigned char	ucSubChCnt;
	unsigned char	aucSubChID[MAX_SUBCHANNEL];
	unsigned short	auiBitRate[MAX_SUBCHANNEL];
	unsigned char	aucTMID[MAX_SUBCHANNEL];
	unsigned char	aucServiceType[MAX_SUBCHANNEL];
	unsigned short	auiUserAppType[MAX_SUBCHANNEL];
	unsigned int	aulServiceID[MAX_SUBCHANNEL];
	unsigned char	aucEnsembleLabel[MAX_LABEL_CHAR];
	unsigned char	aucServiceLabel[MAX_SUBCHANNEL][MAX_LABEL_CHAR];
#ifdef INC_CAS_ENABLE
	unsigned char	ucCAFlag[MAX_SUBCHANNEL];
	unsigned short	uiSCId[MAX_SUBCHANNEL];
	unsigned char	ucDGFlag[MAX_SUBCHANNEL];
	unsigned char	ucDSCType[MAX_SUBCHANNEL];
	unsigned short	uiPacketAddr[MAX_SUBCHANNEL];
	unsigned short	uiCAOrg[MAX_SUBCHANNEL];
#endif
} BB_CH_INFO;


typedef struct
{
	unsigned short	nBbpStatus;
	unsigned int	uiRFFreq;
	unsigned char	ucSubChCnt;
	unsigned char	ucSubChID[3];
	unsigned char	ucTMID[3];
	unsigned short	nTempValue[3];
} BB_CH_PLAY;

typedef struct
{
	unsigned int	uiPreBER;
	unsigned int	uiPostBER;
	unsigned short	uiCER;
	unsigned short	uiCERAvg;
	unsigned short	uiRSSI;
	unsigned char	ucSNR;
} BB_CH_SIGNAL;

typedef struct
{
	unsigned char	aucSubChID[3];
	unsigned short	nDataSize[3];
} BB_CH_DATASIZE;

typedef struct
{
	unsigned char	aucMajor[32];
	unsigned char	aucMinor[32];
	unsigned char	aucDate[32];
} BB_CH_VERSION;


#ifdef USE_INC_EWS
typedef struct {
	unsigned char  organ;                  // create organization 
	unsigned char  id;                     // msg ID
	unsigned char  priority;               // emergency priority : unknown/normal/emergency/very emergency
	unsigned char  areaType;               // emergency area type : Korea/government code/address code 
	unsigned int   genTime;                // generation time(28 Bit); (MJT:17bit + UTC short:11Bit(Hour:5Bit+Min:6Bit))
	unsigned char  emgType[3];             // emergency type
	unsigned char  numOfArea;              // number of emergency area
	unsigned int   dataSize;               // size of emergency message) // data + areaCode = dataSize
	unsigned char  data[16*26];              // emergency message // 16 * 26
} EWS_AEAS_MSG_T, *LPEWS_AEAS_MSG_T;  // Automatic Emergency Alert Service :: sizeof(432)
#endif

typedef struct _tagST_DUMP_DATA
{
#ifdef USE_INC_EWS
	EWS_AEAS_MSG_T	stFdEwsMsg;
#endif

	unsigned int    uiDmbChSize;
	unsigned char   aucDmbBuff[MAX_BUFF_SIZE];
	unsigned int    uiDabChSize;
	unsigned char   aucDabBuff[MAX_BUFF_SIZE];
	unsigned int    uiDataChSize;
	unsigned char   aucDataBuff[MAX_BUFF_SIZE];
} ST_DUMP_DATA, *PST_DUMP_DATA;

extern ST_TS_RINGBUFF g_stRingBuff_Fic;
extern ST_TS_RINGBUFF g_stRingBuff_Dmb;
extern ST_TS_RINGBUFF g_stRingBuff_Dab;
extern ST_TS_RINGBUFF g_stRingBuff_Data;


unsigned int  INC_TickCnt(void)
{
	struct timeval tick;
	do_gettimeofday(&tick);

	// return msec tick
	return (tick.tv_sec*1000 + tick.tv_usec/1000);
}

int INC_dump_thread(void *kthread)
{
	ST_FIFO* 		pMultiFF;
	PST_TS_RINGBUFF pstGetTSBuff = NULL;
	unsigned char* 	pSourceBuff = NULL;
	unsigned short  uiLoop = 0, uiIndex = 0, uiDataLength = 0;
	
	long nTimeStamp = 0;
	unsigned char  	bFirstLoop = 1;
	unsigned int   	nTickCnt = 0;
	unsigned short	nStatus = 0;
	ST_BBPINFO*		pInfo;
	
	g_nThreadcnt++;
	INC_MSG_PRINTF(1, "\n[%s] : INC_dump_thread start [%d]===========>> \r\n",
		__func__, g_nThreadcnt);

#ifdef INC_MULTI_CHANNEL_ENABLE
		INC_MULTI_SORT_INIT();
#endif

	//INTERFACE_INT_ENABLE(INC_I2C_ID80,INC_MPI_INTERRUPT_ENABLE); 
	
	while (!kthread_should_stop())
	{
		if(!g_TaskFlag)	break;
		if( (INC_TickCnt() - nTimeStamp ) > INC_CER_PERIOD_TIME )
		{
			nStatus = INTERFACE_STATUS_CHECK(INC_I2C_ID80);
			if( nStatus == 0xFF )
			{
				////////////////////////////////////
				// Reset!!!!! 
				////////////////////////////////////
				INC_GPIO_Reset(1);
				INC_MSG_PRINTF(1, "[%s]: SPI Reset !!!!\r\n", __func__);
			
				if(INC_SUCCESS == INC_CHIP_STATUS(INC_I2C_ID80))
				{
					INC_INIT(INC_I2C_ID80);
					INC_READY(INC_I2C_ID80, g_stCHInfo.astSubChInfo[0].ulRFFreq);
					INC_START(INC_I2C_ID80, &g_stCHInfo, 0); 
				}

				pInfo = INC_GET_STRINFO(INC_I2C_ID80);
				pInfo->ulFreq = g_stCHInfo.astSubChInfo[0].ulRFFreq;
				bFirstLoop = 1;
			}
			else if(nStatus == INC_ERROR){
				INC_MSG_PRINTF(1, "[%s] ReSynced..... !!!!\r\n",   __func__);
				bFirstLoop = 1;
			}
			nTimeStamp = INC_TickCnt();
		}

		if(!g_TaskFlag)	break;
		if(bFirstLoop)
		{
			nTickCnt = 0;
			bFirstLoop = 0;
			init_completion(&g_stINCInt.comp);			
			INTERFACE_INT_ENABLE(INC_I2C_ID80,INC_MPI_INTERRUPT_ENABLE); 
			INTERFACE_INT_CLEAR(INC_I2C_ID80, INC_MPI_INTERRUPT_ENABLE);
			INC_INIT_MPI(INC_I2C_ID80);	
			INC_MSG_PRINTF(1, "[%s] [%d]: nTickCnt(%d) INC_INIT_MPI!!!!!! \r\n", __func__, __LINE__, nTickCnt);
		}

		if(!g_TaskFlag)	break;
		
#ifdef POLLING_DUMP_METHOD
		if(!INTERFACE_INT_CHECK(INC_I2C_ID80)){
			INC_DELAY(5);
			//INC_MSG_PRINTF(1, "[%s] INTERFACE_INT_CHECK() fail\r\n", __func__);	
			continue;
		}
		//INC_MSG_PRINTF(1, "[%s] INTERFACE_INT_CHECK() success\r\n", __func__);		
#else
///////////////////////////////////////////////////////////////////////////////////////////
#ifdef INC_WINCE_SPACE
		ulStatus = WaitForSingleObject(g_stBBinfo.hINTEvent, 1000);
		if(ulStatus != WAIT_OBJECT_0){
			INC_MSG_PRINTF(1, " ==> INTR TimeOut : nTickCnt[%d]\r\n", nTickCnt);
			bFirstLoop = TRUE;
			continue;
		}		
#elif defined(INC_KERNEL_SPACE)
		// HZ:256
		if(!wait_for_completion_timeout(&g_stINCInt.comp, 1000*HZ/1000)){ // 1000msec 
			INC_MSG_PRINTF(1, "[%s] INTR TimeOut : nTickCnt[%d]\r\n", __func__, nTickCnt);
			bFirstLoop = 1;
			continue;
		}
		//INC_MSG_PRINTF(1, "I");
#endif
//////////////////////////////////////////////////////////////////////////////////////////
#endif

		if(!g_TaskFlag)	break;
		
		////////////////////////////////////////////////////////////////
		// Read the dump size
		////////////////////////////////////////////////////////////////
		uiDataLength = INC_CMD_READ(INC_I2C_ID80, APB_MPI_BASE+ 0x08);
		//INC_MSG_PRINTF(1, "[%s] nDataLength[%d]\r\n", __func__, nDataLength);
		
		if( uiDataLength & 0x4000 ){
			bFirstLoop = 1;
			INC_MSG_PRINTF(1, "[%s]==> FIFO FULL   : 0x%X nTickCnt(%d)\r\n", __func__, uiDataLength, nTickCnt);
			continue;
		}
		else if( !(uiDataLength & 0x3FFF ))	{
			uiDataLength = 0;
			INTERFACE_INT_CLEAR(INC_I2C_ID80, INC_MPI_INTERRUPT_ENABLE);
			INC_MSG_PRINTF(1, "[%s]==> FIFO Empty   : 0x%X nTickCnt(%d)\r\n", __func__, uiDataLength, nTickCnt);
			continue;
		}
		else{
			uiDataLength &= 0x3FFF;
			if(uiDataLength < INC_INTERRUPT_SIZE)
  				continue;			  
			
			uiDataLength = INC_INTERRUPT_SIZE;
		}

		//uiDataLength = ((INC_UINT16)(uiDataLength /188))*188;      /* 188 byte align*/
		//if(uiDataLength > 7520){
		//	INC_MSG_PRINTF(1, "[%s] SPI Read Burst size(%d), limit(MPI_CS_SIZE * 5)\r\n", __func__, uiDataLength);	
		//	uiDataLength = 7520;
		//}
		
#ifndef INC_MULTI_CHANNEL_ENABLE
		g_DataLength = uiDataLength;
#endif

		if(!g_TaskFlag)	break;
		////////////////////////////////////////////////////////////////
		// dump the stream
		////////////////////////////////////////////////////////////////
		if(uiDataLength >0){
			INC_CMD_READ_BURST(INC_I2C_ID80, APB_STREAM_BASE, g_StreamBuff, uiDataLength);
			//INC_MSG_PRINTF(1, "[%s] uiDataLength   = %d\r\n", __func__, uiDataLength);
			//g_ReadQ = INT_OCCUR_SIG; 
		}
		nTickCnt++;
#ifdef POLLING_DUMP_METHOD		
		INTERFACE_INT_CLEAR(INC_I2C_ID80, INC_MPI_INTERRUPT_ENABLE);
#endif

#ifdef INC_MULTI_CHANNEL_ENABLE
		if(!INC_MULTI_FIFO_PROCESS(g_StreamBuff, uiDataLength))
		{
			INC_MSG_PRINTF(1, "[%s] [FAIL]uiDataLength   = %d\r\n", __func__, uiDataLength);
			//INC_MSG_PRINTF(1, "[0x%02x][0x%02x][0x%02x][0x%02x][0x%02x][0x%02x][0x%02x][0x%02x][0x%02x][0x%02x][0x%02x][0x%02x][0x%02x][0x%02x][0x%02x][0x%02x]\r\n", 
			//	g_StreamBuff[0], g_StreamBuff[1], g_StreamBuff[2], g_StreamBuff[3],
			//	g_StreamBuff[4], g_StreamBuff[5], g_StreamBuff[6], g_StreamBuff[7],
			//	g_StreamBuff[8], g_StreamBuff[9], g_StreamBuff[10], g_StreamBuff[11],
			//	g_StreamBuff[12], g_StreamBuff[13], g_StreamBuff[14], g_StreamBuff[15]);
			continue;
		}
		
		pMultiFF = INC_GET_CHANNEL_FIFO((MULTI_CHANNEL_INFO)FIC_STREAM_DATA);
		uiDataLength = INC_QFIFO_GET_SIZE(pMultiFF);
		if(uiDataLength)
		{	
			INC_QFIFO_BRING(pMultiFF, g_StreamBuff, uiDataLength);		
			//RETAILMSG(1,(TEXT("[dmb_t3600.cpp %d] Fic Lengthe = %d\r\n"), __LINE__, uiDataLength));

			pstGetTSBuff = &g_stRingBuff_Fic;
			if(!INC_Buffer_Full(pstGetTSBuff))
			{
				pSourceBuff = INC_Put_Ptr_Buffer(pstGetTSBuff, uiDataLength, MAX_BUFF_FIC_DMUP_SIZE);
				memcpy(pSourceBuff, g_StreamBuff, uiDataLength);
			}
			else
			{
				INC_Buffer_Init(pstGetTSBuff);
				//RETAILMSG(1, (TEXT("[dmb_t3600.cpp %d] INC_Buffer_Full(FIC) and INC_Buffer_Init(FIC)\r\n"), __LINE__));
			}				
		}
		
        for(uiLoop = 0; uiLoop < g_stCHInfo.nSetCnt; uiLoop++)
		{
            pMultiFF = INC_GET_CHANNEL_FIFO((MULTI_CHANNEL_INFO)(CHANNEL1_STREAM_DATA + uiLoop));
			uiDataLength = INC_QFIFO_GET_SIZE(pMultiFF);

            if( !uiDataLength ) continue;

            INC_QFIFO_BRING(pMultiFF, g_StreamBuff, uiDataLength);
			
			if(g_stCHInfo.astSubChInfo[uiLoop].uiTmID == TMID_1)	
			{
				pstGetTSBuff = &g_stRingBuff_Dmb;
				
				for(uiIndex = 0; uiIndex < (uiDataLength/188); uiIndex++)	
				{
					if(g_StreamBuff[188*uiIndex] == 0x47) 
					{
						if(g_StreamBuff[188*uiIndex+1] == 0x80)
						{
							
						}
						else
						{
							if(!INC_Buffer_Full(pstGetTSBuff))
							{
								pSourceBuff = INC_Put_Ptr_Buffer(pstGetTSBuff, 188, MAX_BUFF_DMB_DMUP_SIZE); // MSC1	
								memcpy(pSourceBuff, &g_StreamBuff[188*uiIndex], 188);
							}
							else
							{
								INC_Buffer_Init(pstGetTSBuff);
								INC_MSG_PRINTF(1, "[tdmb.c %s] INC_Buffer_Full(DMB) and INC_Buffer_Init(DMB)\r\n",   __LINE__);
							}							
						}
					}
					else
					{
						
					}
				}

				///////////////////////////////////////////////////////
				//fwrite(aucCifBuff, sizeof(char), nDataLength, m_hFile);
				///////////////////////////////////////////////////////
			}
			else if(g_stCHInfo.astSubChInfo[uiLoop].uiTmID == TMID_0)
			{
				pstGetTSBuff = &g_stRingBuff_Dab;
				if(!INC_Buffer_Full(pstGetTSBuff))
				{
					pSourceBuff = INC_Put_Ptr_Buffer(pstGetTSBuff, uiDataLength, MAX_BUFF_DAB_DMUP_SIZE);
					memcpy(pSourceBuff, g_StreamBuff, uiDataLength);
				}
				else
				{
					INC_Buffer_Init(pstGetTSBuff);
					INC_MSG_PRINTF(1, "[tdmb.c %s] INC_Buffer_Full(DAB) and INC_Buffer_Init(DAB)\r\n",   __LINE__);
				}				
			}
			else if(g_stCHInfo.astSubChInfo[uiLoop].uiTmID == TMID_3)
			{
				pstGetTSBuff = &g_stRingBuff_Data;
				if(!INC_Buffer_Full(pstGetTSBuff))
				{
					pSourceBuff = INC_Put_Ptr_Buffer(pstGetTSBuff, uiDataLength, MAX_BUFF_DATA_DMUP_SIZE);
					memcpy(pSourceBuff, g_StreamBuff, uiDataLength);			
					//RETAILMSG(g_DebugMsgEnable, (TEXT("[Get TPEG = %d]\r\n"), uiDataLength));
				}
				else
				{
					INC_Buffer_Init(pstGetTSBuff);
					INC_MSG_PRINTF(1, "[tdmb.c %s] INC_Buffer_Full(DATA) and INC_Buffer_Init(DATA)\r\n",   __LINE__);
				}				
			}
			else
			{
				
			}
        }

		g_ReadQ = INT_OCCUR_SIG;         
		wake_up_interruptible(&WaitQueue_Read);
        //INC_MSG_PRINTF(1, "[%s()] wake_up_interruptible [%d]\r\n",   __func__, ucDevNum);
	} 
#endif

	g_ReadQ = INT_EXIT_SIG;
	wake_up_interruptible(&WaitQueue_Read);
	
	g_nThreadcnt--;
	INC_MSG_PRINTF(1, "[%s] : INC_dump_thread end [%d]=============<< \r\n\r\n",
					__func__, g_nThreadcnt);
	return 0;
}


int INC_scan(BB_CH_INFO* pChInfo)
{
	INC_UINT16 	nLoop = 0;
	INC_UINT32 	uiFreq;

#ifdef INC_T3A00_CHIP
	ST_SUBCH_INFO* pstCHInfo;
#endif

	//INC_MSG_PRINTF(1, "[%s] %s : freq[%d] start \r\n", __FILE__, __func__, pChInfo->uiRFFreq);
	uiFreq = pChInfo->uiRFFreq;
	if(uiFreq == 0xffff || uiFreq == 0){
		INC_MSG_PRINTF(1, "[%s] : freq[%d] Error!!! \r\n", __func__, uiFreq);
		return -1;
	}

	////////////////////////////////////////////////
	// Start Single Scan
	////////////////////////////////////////////////
	if(!INTERFACE_SCAN(INC_I2C_ID80, uiFreq)){
		pChInfo->nBbpStatus = (INC_UINT16)INTERFACE_ERROR_STATUS(INC_I2C_ID80);
		INC_MSG_PRINTF(1, "[%s] : [Scan Fail] Freq:%d, status:0x%X\r\n", 
			__func__, uiFreq, pChInfo->nBbpStatus);

		if( pChInfo->nBbpStatus == ERROR_STOP || pChInfo->nBbpStatus == ERROR_INIT)
		{
			////////////////////////////////////
			// Reset!!!!! 
			////////////////////////////////////
			INC_GPIO_Reset(1);
			INC_MSG_PRINTF(1, "[%s] : SPI Reset !!!![%d]\r\n", __func__,pChInfo->nBbpStatus);
			INTERFACE_INIT(INC_I2C_ID80);
		}		
		return -1;
	}else{
		INC_MSG_PRINTF(1, "[%s] : [Scan Success] Freq:%d\r\n", __func__, uiFreq);
	}

#ifdef INC_T3A00_CHIP
	pstCHInfo = INTERFACE_GETDB_CHANNEL();
	INC_MSG_PRINTF(1, "[%s] : [INC_T3A00_CHIP] setcnt : %d\r\n", __func__, pstCHInfo->nSetCnt);
	pChInfo->ucSubChCnt = pstCHInfo->nSetCnt;
	pChInfo->uiEnsembleID = pstCHInfo->astSubChInfo[0].uiEnsembleID;
	memcpy(pChInfo->aucEnsembleLabel, pstCHInfo->astSubChInfo[0].aucEnsembleLabel, MAX_LABEL_CHAR);

	for(nLoop=0; nLoop<pstCHInfo->nSetCnt; nLoop++)
	{
		pChInfo->aucSubChID[nLoop] 		= pstCHInfo->astSubChInfo[nLoop].ucSubChID;
		pChInfo->aucTMID[nLoop] 		= pstCHInfo->astSubChInfo[nLoop].uiTmID;
		pChInfo->auiBitRate[nLoop]		= pstCHInfo->astSubChInfo[nLoop].uiBitRate;
		pChInfo->aucServiceType[nLoop] 	= pstCHInfo->astSubChInfo[nLoop].ucServiceType;
		pChInfo->auiUserAppType[nLoop] 	= pstCHInfo->astSubChInfo[nLoop].stUsrApp.astUserApp[0].unUserAppType;
		pChInfo->aulServiceID[nLoop] 	= pstCHInfo->astSubChInfo[nLoop].ulServiceID;
		memcpy(pChInfo->aucServiceLabel[nLoop], pstCHInfo->astSubChInfo[nLoop].aucLabel, MAX_LABEL_CHAR);	
	}
#else // T3600
	
	pChInfo->ucSubChCnt = g_stCHInfo.nSetCnt;
	pChInfo->uiEnsembleID = g_stCHInfo.astSubChInfo[0].uiEnsembleID;
	memcpy(pChInfo->aucEnsembleLabel, g_stCHInfo.astSubChInfo[0].aucEnsembleLabel, MAX_LABEL_CHAR);

	for(nLoop=0; nLoop<g_stCHInfo.nSetCnt; nLoop++)
	{
		INC_CHANNEL_INFO*  tempCHInfo = &g_stCHInfo.astSubChInfo[nLoop];
		pChInfo->aucSubChID[nLoop] 		= tempCHInfo->ucSubChID;
		pChInfo->auiBitRate[nLoop] 		= tempCHInfo->uiBitRate;
		pChInfo->aucTMID[nLoop] 		= tempCHInfo->uiTmID;
		pChInfo->aucServiceType[nLoop] 	= tempCHInfo->ucServiceType;
		pChInfo->auiUserAppType[nLoop] 	= tempCHInfo->uiUserAppType; 
		pChInfo->aulServiceID[nLoop] 	= tempCHInfo->ulServiceID;
#ifdef INC_CAS_ENABLE
		pChInfo->uiSCId[nLoop]			= tempCHInfo->uiSCId;
		pChInfo->ucDGFlag[nLoop]		= tempCHInfo->ucDGFlag;
		pChInfo->ucDSCType[nLoop]		= tempCHInfo->ucDSCType;
		pChInfo->uiPacketAddr[nLoop]	= tempCHInfo->uiPacketAddr;
		pChInfo->uiCAOrg[nLoop]			= tempCHInfo->uiSCId;
#endif
		memcpy(pChInfo->aucServiceLabel[nLoop], tempCHInfo->aucLabel, MAX_LABEL_CHAR);
	}
#endif
	return 0;
}


int INC_start(BB_CH_PLAY* pstSetChInfo)
{
	INC_UINT16	nLoop;
	g_TaskFlag = 0;
	
	INC_MSG_PRINTF(1, "[%s] : freq[%d] start\r\n", 	__func__, pstSetChInfo->uiRFFreq);
	
	if(pstSetChInfo->ucSubChCnt > 3){
		INC_MSG_PRINTF(1, "[%s] :   subChcnt Error[%d]!!! \r\n", 
			__func__, pstSetChInfo->ucSubChCnt);
		return -1;
	}

	memset(&g_stCHInfo, 0, sizeof(ST_SUBCH_INFO));
	pstSetChInfo->nBbpStatus = ERROR_NON;
	////////////////////////////////////////////////
	// setting the channel
	////////////////////////////////////////////////
	for(nLoop=0; nLoop<pstSetChInfo->ucSubChCnt; nLoop++)
	{
		g_stCHInfo.astSubChInfo[nLoop].ulRFFreq = pstSetChInfo->uiRFFreq;
		g_stCHInfo.astSubChInfo[nLoop].ucSubChID = pstSetChInfo->ucSubChID[nLoop];
		g_stCHInfo.astSubChInfo[nLoop].uiTmID = pstSetChInfo->ucTMID[nLoop];
		g_stCHInfo.nSetCnt++;
	}

	INC_MSG_PRINTF(1, "[%s] : nSetCnt        = %d\r\n", __func__, g_stCHInfo.nSetCnt);
	INC_MSG_PRINTF(1, "[%s] : ucSubChID[0] = %d\r\n", 	__func__, g_stCHInfo.astSubChInfo[0].ucSubChID);
	INC_MSG_PRINTF(1, "[%s] : uiTmID   [0] = %d\r\n", 	__func__, g_stCHInfo.astSubChInfo[0].uiTmID);
	INC_MSG_PRINTF(1, "[%s] : ucSubChID[1] = %d\r\n", 	__func__, g_stCHInfo.astSubChInfo[1].ucSubChID);
	INC_MSG_PRINTF(1, "[%s] : uiTmID   [1] = %d\r\n", 	__func__, g_stCHInfo.astSubChInfo[1].uiTmID);

	////////////////////////////////////////////////
	// Start channel
	////////////////////////////////////////////////
	if(!INTERFACE_START(INC_I2C_ID80, &g_stCHInfo))
	{
		pstSetChInfo->nBbpStatus = (INC_UINT16)INTERFACE_ERROR_STATUS(INC_I2C_ID80);
		INC_MSG_PRINTF(1, "[%s] : INTERFACE_START() Error : 0x%X \r\n", 
			__func__, pstSetChInfo->nBbpStatus);

		if( pstSetChInfo->nBbpStatus == ERROR_STOP || pstSetChInfo->nBbpStatus == ERROR_INIT)
		{
			////////////////////////////////////
			// Reset!!!!! 
			////////////////////////////////////
			INC_GPIO_Reset(1);
			INC_MSG_PRINTF(1, "[%s] : SPI Reset !!!!\r\n", __func__);
			INTERFACE_INIT(INC_I2C_ID80);
		}
		return 0;
		
	}else{
		INC_MSG_PRINTF(1, "[%s] : INTERFACE_START() Success !!!!\r\n", __func__);
	}

	g_TaskFlag = 1;
	g_pThread = kthread_run(INC_dump_thread, NULL, "kidle_timeout");
	if(IS_ERR(g_pThread) ) {// no error	
		INC_MSG_PRINTF(1, "[%s] : cann't create the INC_dump_thread !!!! \n", __func__);
		return -1;	
	}
	return 0;
}


int INC_interface_test(void)
{
	unsigned short nLoop = 0, nIndex = 0;
	unsigned short nData = 0;
	unsigned int nTick = 0;
	unsigned int TestCnt = 0;
					
	/////////////////////////////////////////////
	// SPI interface Test
	/////////////////////////////////////////////
	nTick = INC_TickCnt();
	while(nIndex < 1)
	{
		for(nLoop=1; nLoop<256; nLoop++)
		{
			INC_CMD_WRITE(INC_I2C_ID80, APB_MPI_BASE + 5, nLoop) ;
			nData = INC_CMD_READ(INC_I2C_ID80, APB_MPI_BASE + 5);

			if(nLoop != nData){
				INC_MSG_PRINTF(1, " [Interface Test : %03d] Error: WData[0x%02X], RData[0x%02X] \r\n", 
					(nIndex*256)+nLoop, nLoop, nData);
				//msleep(10);
				
				if(TestCnt++ > 16)
					break;
			}
		}
		nIndex++;
	}
	if(TestCnt){
		INC_MSG_PRINTF(1, "[%s] FAIL ==> %d msec \r\n", __func__, INC_TickCnt()-nTick);
		return 0;
	}
	
	INC_MSG_PRINTF(1, "[%s] SUCCESS ==> %d msec \r\n", __func__, INC_TickCnt()-nTick);
	return 1;
}


int INC_drv_open (struct inode *inode, struct file *filp)
{
	INC_MSG_PRINTF(1,"\r\n##################################################\n");
	INC_MSG_PRINTF(1,"######## [%s]   Start!! ##################\n", __func__);
	INC_MSG_PRINTF(1,"######## [%s, %s, %s]\n", INC_FW_VERSION_MAJOR, INC_FW_VERSION_MINOR, INC_FW_VERSION_DATE);
	INC_MSG_PRINTF(1,"##################################################\n");

	///////////////////////////////////////
	// by jin 10/11/04 : RESET PIN Setting
	///////////////////////////////////////
	INC_GPIO_DMBEnable();
	INC_GPIO_Reset(1);
	msleep(10);
	
	if(INC_ERROR == INC_DRIVER_OPEN(INC_I2C_ID80)){
		INC_MSG_PRINTF(1, "[%s] INC_DRIVER_OPEN() FAIL !!!\n", __func__);
		//return -1;
		goto out;
	}
	
	INC_MSG_PRINTF(1, "[%s] INC_DRIVER_OPEN() SUCCESS \n", __func__);
// Test the spi communication
	if(!INC_interface_test()){

		INC_GPIO_DMBEnable();
		INC_GPIO_Reset(1);
		msleep(100);
	
		if(!INC_interface_test())
			goto out;		
	}
	
	// Initialize INC chip
	if(!INTERFACE_INIT(INC_I2C_ID80)){
		INC_MSG_PRINTF(1, "[%s] INTERFACE_INIT() FAIL !!!\n", __func__);
		goto out;
	}
	INC_MSG_PRINTF(1, "[%s] INTERFACE_INIT() SUCCESS \n", __func__);
	

#ifndef POLLING_DUMP_METHOD	
	///////////////////////////////////////
	// Interrupt setting
	///////////////////////////////////////
	if(INC_GPIO_set_Interrupt()){
		return -1;
	}
#endif

	INC_MSG_PRINTF(1, "######## [%s] Success!! ################\n\n", __func__);
	return 0;
	
out :
	g_pThread = NULL;
	g_nThreadcnt = 0;
	INC_GPIO_Reset(0);
	INC_GPIO_DMBDisable();
	INC_MSG_PRINTF(1, "######## [%s] FAIL!! ################\n\n", __func__);
	return -1;
	
}

ssize_t INC_drv_read(struct file *filp, char *buf, size_t count, loff_t *f_pos)
{
#if 1
		int res = -1;
		struct tdmb_data	*tdmbdev;
		ST_DUMP_DATA		*st_data;
		unsigned char* pBuff;
		
		unsigned char dev_num, dev_id;
		unsigned int uiDataSize = 0;

		int read_flag = 0;
	
		st_data = (ST_DUMP_DATA*)buf;
		
		if(INC_Get_Buffer_Count(&g_stRingBuff_Dmb))
		{
			pBuff = INC_Get_Ptr_Buffer(&g_stRingBuff_Dmb, &uiDataSize);
			res = copy_to_user(st_data->aucDmbBuff, pBuff, sizeof(unsigned char) * uiDataSize);
			if (res > 0) {
				res = -EFAULT;
				INC_MSG_PRINTF(1, "[%s] : copy_to_user(buffer) Error!!! \n", __func__);
				return -1;
			}
			#if 1
			res = copy_to_user(&st_data->uiDmbChSize, &uiDataSize, sizeof(unsigned int));
			if (res > 0) {
				res = -EFAULT;
				INC_MSG_PRINTF(1, "[%s] : copy_to_user(length) Error!! \n", __func__);
				return -1;
			}
			#endif			
			//st_data->uiDmbChSize = uiDataSize;
			//INC_MSG_PRINTF(1, "[%s] : st_data->aucDmbBuff   = %d\r\n", __func__, st_data->uiDmbChSize);
			read_flag = 1;
		}	
	
		if(INC_Get_Buffer_Count(&g_stRingBuff_Dab))
		{
			pBuff = INC_Get_Ptr_Buffer(&g_stRingBuff_Dab, &uiDataSize);
			res = copy_to_user(st_data->aucDabBuff, pBuff, sizeof(unsigned char) * uiDataSize);
			if (res > 0) {
				res = -EFAULT;
				INC_MSG_PRINTF(1, "[%s] : copy_to_user(buffer) Error!!! \n", __func__);
				return -1;
			}
			#if 1
			res = copy_to_user(&st_data->uiDabChSize, &uiDataSize, sizeof(unsigned int));
			if (res > 0) {
				res = -EFAULT;
				INC_MSG_PRINTF(1, "[%s] : copy_to_user(length) Error!! \n", __func__);
				return -1;
			}
			#endif
			//st_data->uiDabChSize = uiDataSize;
			//INC_MSG_PRINTF(1, "[%s] : st_data->uiDabChSize   = %d\r\n", __func__, st_data->uiDabChSize);
			read_flag = 1;
		}
	
		if(INC_Get_Buffer_Count(&g_stRingBuff_Data))
		{
			pBuff = INC_Get_Ptr_Buffer(&g_stRingBuff_Data, &uiDataSize);
			res = copy_to_user(st_data->aucDataBuff, pBuff, sizeof(unsigned char) * uiDataSize);
			if (res > 0) {
				res = -EFAULT;
				INC_MSG_PRINTF(1, "[%s] : copy_to_user(buffer) Error!!! \n", __func__);
				return -1;
			}
			#if 1
			res = copy_to_user(&st_data->uiDataChSize, &uiDataSize, sizeof(unsigned int));
			if (res > 0) {
				res = -EFAULT;
				INC_MSG_PRINTF(1, "[%s] : copy_to_user(length) Error!! \n", __func__);
				return -1;
			}
			#endif
			//st_data->uiDataChSize = uiDataSize;
			//INC_MSG_PRINTF(1, "[%s] : st_data->uiDataChSize   = %d\r\n", __func__, st_data->uiDataChSize);
			//st_data->uiDataChSize = uiDataSize;
			read_flag = 1;
		}

		if(read_flag) {
			//g_ReadQ = INT_OCCUR_SIG;         
			//wake_up_interruptible(&WaitQueue_Read);
		}
	
		return sizeof(ST_DUMP_DATA);
#else
		int res = -1;
		unsigned int	nReadSize = 0;
		
#ifdef INC_MULTI_CHANNEL_ENABLE	
		unsigned int	nFifoSize = 0;
		unsigned short	nIndex = 0;
		ST_FIFO*		pMultiFF;
		unsigned char*	pTempBuff;
		unsigned char	ucSubChID = 0;
	
		pTempBuff = kmalloc(INC_FIFO_DEPTH, GFP_KERNEL);
		memset(pTempBuff, 0, INC_FIFO_DEPTH);
		
		nReadSize = count & 0xFFFF;
		ucSubChID = (count >> 16) & 0xFF;
	
		//INC_MSG_PRINTF(1, "[%s] %s : count(0x%X), nReadSize(%d), ucSubChID(%d)\n", 
		//				__FILE__, __func__, count, nReadSize, ucSubChID);
	
		for(nIndex=0; nIndex<3; nIndex++)
		{
#ifdef	INC_MULTI_CHANNEL_FIC_UPLOAD
			if(ucSubChID == INC_FIC_SUBCHID){
				pMultiFF = INC_GET_CHANNEL_FIFO((MULTI_CHANNEL_INFO)(FIC_STREAM_DATA));
				nFifoSize = INC_QFIFO_GET_SIZE(pMultiFF);
				break;
			}
#endif			
			pMultiFF = INC_GET_CHANNEL_FIFO((MULTI_CHANNEL_INFO)(CHANNEL1_STREAM_DATA + nIndex));
			if(ucSubChID != (unsigned char)pMultiFF->unSubChID){
				continue;
			}
			nFifoSize = INC_QFIFO_GET_SIZE(pMultiFF);
			break;
		}
	
		if(nIndex >= 3){
			INC_MSG_PRINTF(1, "[%s] : Can't find subCHId[0x%X]\n", __func__, ucSubChID);
			nFifoSize = 0;
			return -1;
		}
	
		if(nReadSize != nFifoSize){
			INC_MSG_PRINTF(1, "[%s] ReadSize mismatch [%d:%d]\n", __func__, nReadSize, nFifoSize);
		}
	
		if(nReadSize > INC_FIFO_DEPTH){
			kfree(pTempBuff);
			return -EMSGSIZE;
		}
	
		INC_QFIFO_BRING(pMultiFF, pTempBuff, nReadSize);
		res = copy_to_user(buf, pTempBuff, nReadSize);
		if (res > 0) {
			res = -EFAULT;
			INC_MSG_PRINTF(1, "[%s] : Error!! \n", __func__);
			kfree(pTempBuff);			
			return -1;
		}
		kfree(pTempBuff);
#else
		nReadSize = count & 0xFFFF;
		res = copy_to_user(buf, g_StreamBuff, nReadSize);
		if (res > 0) {
			res = -EFAULT;
			INC_MSG_PRINTF(1, "[%s] : Error!! \n", __func__);
			return -1;
		}
#endif
		return nReadSize;
#endif	
}

ssize_t INC_drv_write (struct file *filp, const char *buf, size_t count, loff_t *f_pos)
{
	INC_MSG_PRINTF(1, "[%s]  \n", __func__);
	return 0;
}

int INC_drv_poll( struct file *filp, poll_table *wait )
{
	int mask = 0;
	poll_wait( filp, &WaitQueue_Read, wait );

	if(g_ReadQ == INT_OCCUR_SIG){
		mask |= (POLLIN);
	}else if(g_ReadQ != 0){
		mask |= POLLERR;
	}else{
	}

	g_ReadQ = 0x00;
	return mask;
}	

//int tdmb_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg)
int INC_drv_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int nIndex = 0;
	INC_MSG_PRINTF(0, "[%s] : enter-0 [cmd:%X]\n", __func__, cmd);
		
	if(_IOC_TYPE(cmd) != TDMB_MAGIC)
		return -EINVAL;

	if(_IOC_NR(cmd) >= TDMB_IOCTL_MAX)
		return -EINVAL;

	mutex_lock(&INCio_mutex);
	switch(cmd)
	{
	case DMB_T_STOP:
		{
			INC_MSG_PRINTF(1, "[%s] : TDMB_STOP_CH \n", __func__);
			INTERFACE_USER_STOP(INC_I2C_ID80);
			if(g_pThread != NULL){
				g_TaskFlag = 0;
				complete(&g_stINCInt.comp);
				//kthread_stop(g_pTS);
				msleep(30);
				g_pThread= NULL;
			}
			if(copy_from_user((void*)&nIndex, (const void*)arg, sizeof(int)))
				INC_MSG_PRINTF(1, "[%s] : TDMB_STOP_CH : data[0x%x]4\n", __func__, nIndex);
		}
		break;	
	case DMB_T_SCAN:
		{
			BB_CH_INFO ChInfo;
			if(copy_from_user((void*)&ChInfo, (const void*)arg, sizeof(BB_CH_INFO)))
				INC_MSG_PRINTF(1, "[%s] : DMB_T_SCAN 1 : Freq [%d]\n", __func__, ChInfo.uiRFFreq);

			INC_MSG_PRINTF(0, "[%s] : DMB_T_SCAN : Freq [%d] : Start\n", __func__, ChInfo.uiRFFreq);
			INC_scan(&ChInfo);

			if(copy_to_user((void*)arg, (const void*)&ChInfo, sizeof(BB_CH_INFO)))
				INC_MSG_PRINTF(1, "[%s] : DMB_T_SCAN 2 : Freq [%d]\n", __func__, ChInfo.uiRFFreq);
			
			//INC_MSG_PRINTF(1, "[%s] %s : DMB_T_SCAN   : Freq [%d] : End\n", __FILE__, __func__, ChInfo.uiRFFreq);
		}			
		break;
	case DMB_T_PLAY:
		{
			BB_CH_PLAY stSetChInfo;
			if(copy_from_user((void*)&stSetChInfo, (const void*)arg, sizeof(BB_CH_PLAY)))
				INC_MSG_PRINTF(1, "[%s] : TDMB_SET_CH 111 : Freq [%d]\n", __func__, stSetChInfo.uiRFFreq);

			INC_MSG_PRINTF(0, "[%s] : TDMB_SET_CH : Freq [%d]\n", __func__, stSetChInfo.uiRFFreq);
			INC_start(&stSetChInfo);
			
			if(copy_to_user((void*)arg, (const void*)&stSetChInfo, sizeof(BB_CH_PLAY)))
				INC_MSG_PRINTF(1, "[%s] : TDMB_SET_CH 222: Freq [%d]\n", __func__, stSetChInfo.uiRFFreq);				
		}
		break;
	case DMB_T_SIGNAL:
		{
			BB_CH_SIGNAL ChRFinfo;
			ST_BBPINFO*	pInfo;
			pInfo = INC_GET_STRINFO(INC_I2C_ID80);
						
			if(copy_from_user((void*)&ChRFinfo, (const void*)arg, sizeof(BB_CH_SIGNAL)))
				INC_MSG_PRINTF(1, "[%s] : TDMB_GET_CER   =========== \n", __func__);

			memset(&ChRFinfo, 0, sizeof(BB_CH_SIGNAL));

			ChRFinfo.uiCER = pInfo->uiCER;
			ChRFinfo.uiCERAvg = pInfo->uiInCERAvg;
			//ChRFinfo.uiPostBER = pInfo->uiPostBER;
			//ChRFinfo.uiPreBER = pInfo->uiPreBER;
			ChRFinfo.uiPostBER = pInfo->ucVber;
			ChRFinfo.uiPreBER = pInfo->ucAntLevel;
			ChRFinfo.ucSNR = pInfo->ucSnr;
			ChRFinfo.uiRSSI = pInfo->uiRssi;

			if(copy_to_user((void*)arg, (const void*)&ChRFinfo, sizeof(BB_CH_SIGNAL)))
				INC_MSG_PRINTF(1, "[%s] : TDMB_GET_CER   : [%d]\n", __func__, ChRFinfo.uiCER);
		}
		break;
	case DMB_T_DATASIZE:
		{
#if 0
			BB_CH_DATASIZE	stDataSize;
			unsigned short	nLoop = 0, nIndex = 0;
			ST_FIFO*		pMultiFF;
			
			if(copy_from_user((void*)&stDataSize, (const void*)arg, sizeof(BB_CH_DATASIZE)))
				INC_MSG_PRINTF(1, "[%s] : DMB_T_DATASIZE 1 : subCHId[0x%X,0x%X]\n", 
								__func__, stDataSize.aucSubChID[0], stDataSize.aucSubChID[1]);

			INC_MSG_PRINTF(0, "[%s] %s : DMB_T_DATASIZE   : subCHId[0x%X,0x%X]\n", 
							__FILE__, __func__, stDataSize.aucSubChID[0], stDataSize.aucSubChID[1]);
			
#ifdef INC_MULTI_CHANNEL_ENABLE 
			for(nIndex=0; nIndex<3; nIndex++)
			{
				for(nLoop=0; nLoop<3; nLoop++)
				{
#ifdef	INC_MULTI_CHANNEL_FIC_UPLOAD
					if(stDataSize.aucSubChID[nIndex] == INC_FIC_SUBCHID){
						pMultiFF = INC_GET_CHANNEL_FIFO((MULTI_CHANNEL_INFO)(FIC_STREAM_DATA));
						stDataSize.nDataSize[nIndex] = INC_QFIFO_GET_SIZE(pMultiFF);
						break;
					}
#endif
					pMultiFF = INC_GET_CHANNEL_FIFO((MULTI_CHANNEL_INFO)(CHANNEL1_STREAM_DATA + nLoop));
					if(stDataSize.aucSubChID[nIndex] != (unsigned char)pMultiFF->unSubChID)
						continue;
					
					stDataSize.nDataSize[nIndex] = INC_QFIFO_GET_SIZE(pMultiFF);
					break;
				}
			} 
			INC_MSG_PRINTF(0, "[%s]   DMB_T_DATASIZE => [0x%X, %d], [0x%X, %d], [0x%X, %d]\n", __func__, 
							stDataSize.aucSubChID[0], stDataSize.nDataSize[0],
							stDataSize.aucSubChID[1], stDataSize.nDataSize[1],
							stDataSize.aucSubChID[2], stDataSize.nDataSize[2]);
#else
			if(stDataSize.aucSubChID[0] != g_stCHInfo.astSubChInfo[0].ucSubChID){
				INC_MSG_PRINTF(0, "[%s] : DMB_T_DATASIZE	: subCHId[0x%X:0x%X], %d\n", 
							__func__, stDataSize.aucSubChID[0], 
							g_stCHInfo.astSubChInfo[0].ucSubChID, g_stCHInfo.nSetCnt);
			}
//			stDataSize.nDataSize[0] = INC_INTERRUPT_SIZE;
			stDataSize.nDataSize[0] = g_DataLength;
#endif
			if(copy_to_user((void*)arg, (const void*)&stDataSize, sizeof(BB_CH_DATASIZE)))
				INC_MSG_PRINTF(1, "[%s] : DMB_T_DATASIZE 2 : subCHId[0x%X,0x%X]\n", 
								__func__, stDataSize.aucSubChID[0], stDataSize.aucSubChID[1]);
#endif
		}
		break;		
	case DMB_T_TEST1:
		{
			int reval = 0;
			INC_MSG_PRINTF(1, "[%s] : DMB_T_TEST1 \n", __func__);
			
			if(copy_from_user((void*)&reval, (const void*)arg, sizeof(int)))
				INC_MSG_PRINTF(1, "[%s] : TDMB_INTERFACE_TEST : copy_from_user Error\n", __func__);
			
			// Test the spi communication
			reval = INC_interface_test();
			
			if(!reval){
				INC_MSG_PRINTF(1, "[%s] INC_INTERFACE_TEST() FAIL !!!\n", __func__);
			}else{
				INC_MSG_PRINTF(1, "[%s]   INC_INTERFACE_TEST() SUCCESS \n", __func__);
			}
			if(copy_to_user((void*)arg, (const void*)&reval, sizeof(int)))
				INC_MSG_PRINTF(1, "[%s] : TDMB_INTERFACE_TEST : copy_to_user Error\n", __func__);
		}
		break;
	case DMB_T_VERSION:
		{
			BB_CH_VERSION versionInfo;
			INC_MSG_PRINTF(1, "[%s] : DMB_T_VERSION 1 \n", __func__);
			
			memset(&versionInfo, 0, sizeof(BB_CH_VERSION));
			
			if(copy_from_user((void*)&versionInfo, (const void*)arg, sizeof(BB_CH_VERSION)))
				INC_MSG_PRINTF(1, "[%s] : DMB_T_VERSION 2 \n", __func__);

			strcpy(versionInfo.aucMajor, INC_FW_VERSION_MAJOR);
			strcpy(versionInfo.aucMinor, INC_FW_VERSION_MINOR);
			strcpy(versionInfo.aucDate, INC_FW_VERSION_DATE);

			if(copy_to_user((void*)arg, (const void*)&versionInfo, sizeof(BB_CH_VERSION)))
				INC_MSG_PRINTF(1, "[%s] : DMB_T_VERSION 3 : %s, %s, %s\n", __func__, 
					versionInfo.aucMajor, versionInfo.aucMinor, versionInfo.aucDate);
		}
		break;
	case DMB_T_CHECKSYNC:
		{
			int freq=0, reval=0;
			if(copy_from_user((void*)&freq, (const void*)arg, sizeof(int)))
				INC_MSG_PRINTF(1, "[%s] : copy_from_user Error, Freq [%d]\n", __func__, freq);

#ifdef INC_T3A00_CHIP
			reval = INC_SYNCDETECTOR_CHECK(INC_I2C_ID80, freq);
#endif			
			if(reval == INC_SUCCESS){
				INC_MSG_PRINTF(1, "[%s] INC_CHECKSYNC() SUCCESS !!!\n", __func__);
			}else{
				INC_MSG_PRINTF(1, "[%s]   INC_CHECKSYNC() Fail \n", __func__);
			}		

			if(copy_to_user((void*)arg, (const void*)&reval, sizeof(int)))
				INC_MSG_PRINTF(1, "[%s] : copy_to_user Error, Freq [%d]\n", __func__, freq);
			break;		
		}
		break;
	default :
		INC_MSG_PRINTF(1, "[%s] : Unknown cmd [%d] \n", __func__, cmd);
	}
	mutex_unlock(&INCio_mutex);
	
	return 0;
}

int INC_drv_release (struct inode *inode, struct file *filp)
{
	INC_MSG_PRINTF(1, "######## [%s] :  start\n", __func__, g_stINCInt.irq);

	INTERFACE_USER_STOP(INC_I2C_ID80);
	if(g_TaskFlag){
		g_TaskFlag = 0;
		complete(&g_stINCInt.comp);
		msleep(30);
	}

#ifndef POLLING_DUMP_METHOD
	//free_irq(g_stINCInt.irq, NULL);
	INC_GPIO_free_Interrupt();
#endif
	
	INC_GPIO_Reset(0);
	INC_GPIO_DMBDisable();
	
	
	INC_MSG_PRINTF(1, "######## [%s] : irq[0x%X] end!! \n", __func__, g_stINCInt.irq);
	
	INC_MSG_PRINTF(1, "######## [%s] : %s, cs[%d], mod[%d], %dkhz, %dbit, %d\n", 
			__func__, gInc_spi.modalias, gInc_spi.chip_select, gInc_spi.mode, gInc_spi.max_speed_hz/1000, gInc_spi.bits_per_word, gInc_spi.master->bus_num);
	
	
	g_pThread = NULL;

	INC_DRIVER_CLOSE(INC_I2C_ID80);
	
	return 0;
}


static int __init INC_drv_probe(struct spi_device *spi)
{
	if(spi == NULL) {
		INC_MSG_PRINTF(1, "[%s]   SPI Device is NULL !!! SPI Open Error...", __func__);
		return -1;
	}
	
	INC_MSG_PRINTF(1, "######## [%s] %s, bus[%d], cs[%d], mod[%d], %dkhz, %dbit \n", 
		__func__, spi->modalias, spi->master->bus_num, spi->chip_select, spi->mode, spi->max_speed_hz/1000, spi->bits_per_word);
	
	///////////////////////////////////////
	// SPI setup to I&C
	///////////////////////////////////////
	memcpy(&gInc_spi, spi, sizeof(struct spi_device));

	gInc_spi.mode = 0;
	gInc_spi.bits_per_word = 8;
	gInc_spi.max_speed_hz = 4*1000*1000;

	//if(INC_SPI_SETUP()   == INC_ERROR) {
	//	return -1;
	//}
	
	return 0;
}


static int __exit INC_drv_remove(struct spi_device *spi)
{
	INC_MSG_PRINTF(1, "[%s]:   %s, bus[%d], cs[%d], mod[%d], %dkhz, %dbit, \n", 
		__func__, spi->modalias, spi->master->bus_num, spi->chip_select, spi->mode, spi->max_speed_hz/1000, spi->bits_per_word);

	return 0;
}

/*-------------------------------------------------------------------------*/
/* The main reason to have this class is to make mdev/udev create the
 * /dev/spidevB.C character device nodes exposing our userspace API.
 * It also simplifies memory management.
 */
static struct class *incdev_class;
/*-------------------------------------------------------------------------*/

static struct spi_driver tdmb_spi = {
	.driver = {
		.name = "INC_SPI",		
	//	.name = "spidev",
	//	.name = "s3c64xx-spi",
		.bus  = &spi_bus_type,
		.owner=	THIS_MODULE,
	},
	.probe = INC_drv_probe,
	.remove = __exit_p(INC_drv_remove),
	.suspend = NULL,
	.resume = NULL,

	/* NOTE:  suspend/resume methods are not necessary here.
	 * We don't do anything except pass the requests to/from
	 * the underlying controller.  The refrigerator handles
	 * most issues; the controller driver handles the rest.
	 */
};

struct file_operations inc_fops =
{
	.owner	  = THIS_MODULE,
	.unlocked_ioctl	  = INC_drv_ioctl,  // use after kernel 3.0 version
	.read	  = INC_drv_read,
	.write	  = INC_drv_write,
	.poll	  = INC_drv_poll,
	.open	  = INC_drv_open,
	.release  = INC_drv_release,	
};

int INC_drv_init(void)
{
	int nIndex = 0;
	int result = 0;
	struct device *dev;
	g_DEV_MAJOR_NUM = 0;

	INC_MSG_PRINTF(1, "\n\n\n\n\n\n\n\n######## [%s]   Start!!! ##################\n", __func__);
	while(1){
		nIndex++;
		if(INC_DEV_MAJOR + nIndex > 250)
			return result;
		result = register_chrdev( INC_DEV_MAJOR + nIndex, INC_DEV_NAME, &inc_fops);
		if (result < 0) {
			INC_MSG_PRINTF(1, "[%s] unable to get major %d for fb devs\n\n", __func__, INC_DEV_MAJOR + nIndex);
			continue;
		}else{
			INC_MSG_PRINTF(1, "[%s] success to get major %d for fb devs\n\n", __func__, INC_DEV_MAJOR + nIndex);
			break;
		}
	}
	g_DEV_MAJOR_NUM = INC_DEV_MAJOR + nIndex;
	INC_MSG_PRINTF(1, "######## [%s] register_chrdev : OK!!! major num[%d]\n", __func__, g_DEV_MAJOR_NUM);

	incdev_class = class_create(THIS_MODULE, INC_DEV_NAME);
	if (IS_ERR(incdev_class)) {
		INC_MSG_PRINTF(1, "[%s] Unable to create incdev_class; errno = %ld\n", 
			__func__, PTR_ERR(incdev_class));
		unregister_chrdev(g_DEV_MAJOR_NUM, INC_DEV_NAME);
		return PTR_ERR(incdev_class);
	}
	INC_MSG_PRINTF(1, "######## [%s] class_create : OK!!!\n", __func__);

	result = spi_register_driver(&tdmb_spi);
	if (result < 0) {
		INC_MSG_PRINTF(1, "[%s] Unable spi_register_driver; result = %ld\n", 
			__func__, result);
		class_destroy(incdev_class);
		unregister_chrdev(g_DEV_MAJOR_NUM, INC_DEV_NAME);
	}
	INC_MSG_PRINTF(1, "######## [%s] spi_register_driver : OK!!!\n", __func__);

	dev = device_create(incdev_class, NULL, MKDEV(g_DEV_MAJOR_NUM, INC_DEV_MINOR),
						NULL, "tdmb%d.%d", 0, 1);

	INC_MSG_PRINTF(1, "######## [%s] device_create : OK!!!\n", __func__);

	if (IS_ERR(dev)) {
		INC_MSG_PRINTF(1, "[%s] Unable to create device for framebuffer ; errno = %ld\n",
			__func__, PTR_ERR(dev));
		unregister_chrdev(g_DEV_MAJOR_NUM, INC_DEV_NAME);
		return PTR_ERR(dev);
	}
	
	//if(INC_GPIO_CONFIGURE()) INC_MSG_PRINTF(1, "[%s] GPIO Request Fail ....!!\n", __func__);
	//else INC_MSG_PRINTF(1, "[%s] GPIOs Request Success ....!!\n", __func__);
		
	init_waitqueue_head(&WaitQueue_Read);		// [S5PV210_Kernel], 20101221, ASJ, 
	INC_MSG_PRINTF(1, "######## [%s] Success!! ################\n\n\n\n\n\n", __func__);

	return 0;
}

void INC_drv_exit(void)
{
	INC_MSG_PRINTF(1, "[%s]   enter... \n", __func__);

	//mutex_destroy(&inc_mutex);		// <asj>2/28/2014 오후 4:31:25
	spi_unregister_driver(&tdmb_spi);
	device_destroy(incdev_class, MKDEV(g_DEV_MAJOR_NUM, INC_DEV_MINOR));
	class_destroy(incdev_class);
	unregister_chrdev( INC_DEV_MAJOR, INC_DEV_NAME );
	
	INC_MSG_PRINTF(1, "[%s]   success! \n", __func__);
	
}

module_init(INC_drv_init);
module_exit(INC_drv_exit);

MODULE_AUTHOR("I&C Technology, SATEAM");
#if defined (INC_T3A00_CHIP)
MODULE_DESCRIPTION("TDMB Driver with T3A00");
#elif defined (INC_J3000_CHIP)
MODULE_DESCRIPTION("ISDBT Driver with J3000");
#else
MODULE_DESCRIPTION("TDMB Driver with T3600&T3700");
#endif
MODULE_LICENSE("GPL");
