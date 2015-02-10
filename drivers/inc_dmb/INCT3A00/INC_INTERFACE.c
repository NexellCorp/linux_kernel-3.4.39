//#include "StdAfx.h"
#include "INC_INCLUDES.h"
#include "INC_SPI.h"
#ifdef INC_WINDOWS_SPACE // Windows XP
#include "USB_Driver.h"
USB_Driver	g_pUsbDriver;
CRITICAL_SECTION	gCS_INC;
FILE* m_hINC_LogFile = NULL;
#endif

#define INC_INTERRUPT_LOCK()		
#define INC_INTERRUPT_FREE()


//static DEFINE_MUTEX(INCio_mutex);


ST_SUBCH_INFO		g_stDmbInfo;
ST_SUBCH_INFO		g_stDabInfo;
ST_SUBCH_INFO		g_stDataInfo;
ST_SUBCH_INFO		g_stFIDCInfo;
ST_SUBCH_INFO		g_stCHInfo; 


/*********************************************************************************/
/*	RF Band Select																 */
/*																				 */
/*	INC_UINT8 m_ucRfBand = KOREA_BAND_ENABLE,									 */
/*				 BANDIII_ENABLE,												 */
/*				 LBAND_ENABLE,													 */
/*				 CHINA_ENABLE,													 */
/*				 EXTERNAL_ENABLE,												 */
/*********************************************************************************/
ENSEMBLE_BAND 		m_ucRfBand 		= KOREA_BAND_ENABLE;

/*********************************************************************************/
/*	MPI Chip Select and Clock Setup Part										 */
/*	                                                                             */
/*	m_ucCommandMode = INC_I2C_CTRL, INC_SPI_CTRL, INC_EBI_CTRL                   */
/*	m_ucUploadMode  = STREAM_UPLOAD_MASTER_SERIAL,                               */
/*				      STREAM_UPLOAD_SLAVE_PARALLEL,                              */
/*				      STREAM_UPLOAD_TS,                                          */
/*				      STREAM_UPLOAD_SPI,                                         */
/*																				 */
/*	m_ucClockSpeed = INC_OUTPUT_CLOCK_4096,                                      */
/*				     INC_OUTPUT_CLOCK_2048,                                      */
/*				     INC_OUTPUT_CLOCK_1024,                                      */
/*********************************************************************************/
CTRL_MODE 			m_ucCommandMode 		= INC_SPI_CTRL;
ST_TRANSMISSION		m_ucTransMode			= TRANSMISSION_MODE1;
UPLOAD_MODE_INFO	m_ucUploadMode 			= STREAM_UPLOAD_SPI;
CLOCK_SPEED			m_ucClockSpeed 			= INC_OUTPUT_CLOCK_4096;
INC_ACTIVE_MODE		m_ucMPI_CS_Active 		= INC_ACTIVE_LOW;
INC_ACTIVE_MODE		m_ucMPI_CLK_Active 		= INC_ACTIVE_LOW;

#ifdef  POLLING_DUMP_METHOD
INC_UINT16			m_unIntCtrl				= (INC_INTERRUPT_ACTIVE_POLALITY_LOW | \
											   INC_INTERRUPT_LEVEL | \
											   INC_INTERRUPT_AUTOCLEAR_DISABLE| \
											   (INC_INTERRUPT_PULSE_COUNT & INC_INTERRUPT_PULSE_COUNT_MASK));
#else
INC_UINT16			m_unIntCtrl				= (INC_INTERRUPT_ACTIVE_POLALITY_LOW | \
											   INC_INTERRUPT_PULSE | \
											   INC_INTERRUPT_AUTOCLEAR_ENABLE| \
											   (INC_INTERRUPT_PULSE_COUNT & INC_INTERRUPT_PULSE_COUNT_MASK));
#endif

/*********************************************************************************/
/* PLL_MODE			m_ucPLL_Mode                                                 */
/*T3A00  Input Clock Setting                                                     */
/*********************************************************************************/
//PLL_MODE			m_ucPLL_Mode		= INPUT_CLOCK_19200KHZ; 
PLL_MODE			m_ucPLL_Mode		= INPUT_CLOCK_24576KHZ; 



/*********************************************************************************/
/* INC_DPD_MODE		m_ucDPD_Mode                                                 */
/* T3A00  Power Saving mode setting                                              */
/*********************************************************************************/
INC_DPD_MODE		m_ucDPD_Mode		= INC_DPD_OFF;


/*********************************************************************************/
/*  MPI Chip Select and Clock Setup Part                                         */
/*                                                                               */
/*  INC_UINT8 m_ucCommandMode = INC_I2C_CTRL, INC_SPI_CTRL, INC_EBI_CTRL         */
/*                                                                               */
/*  INC_UINT8 m_ucUploadMode = STREAM_UPLOAD_MASTER_SERIAL,                      */
/*                 STREAM_UPLOAD_SLAVE_PARALLEL,                                 */
/*                 STREAM_UPLOAD_TS,                                             */
/*                 STREAM_UPLOAD_SPI,                                            */
/*                                                                               */
/*  INC_UINT8 m_ucClockSpeed = INC_OUTPUT_CLOCK_4096,                            */
/*                 INC_OUTPUT_CLOCK_2048,                                        */
/*                 INC_OUTPUT_CLOCK_1024,                                        */
/*********************************************************************************/

void INC_DELAY(INC_UINT16 uiDelay)
{
#ifdef INC_KERNEL_SPACE
        msleep(uiDelay);
#else
	Sleep(uiDelay);
#endif
}

void INC_MSG_PRINTF(INC_INT8 nFlag, INC_INT8* pFormat, ...)
{
	va_list Ap;
	INC_UINT16 nSize;
	INC_INT8 acTmpBuff[1000] = {0};

	va_start(Ap, pFormat);
	nSize = vsprintf(acTmpBuff, pFormat, Ap);
	va_end(Ap);

#ifdef INC_KERNEL_SPACE
	if(nFlag)
		printk("%s", acTmpBuff);

#elif defined  (INC_WINDOWS_SPACE) // Windows XP
	if(nFlag)
		TRACE("%s", acTmpBuff);

	if(m_hINC_LogFile && nFlag){
		CString s;
		CTime	t;
		t = CTime::GetCurrentTime();

		s.Format("[%s] %s", t.Format("%d %H:%M:%S"), acTmpBuff);
		fwrite(s, sizeof(char), strlen(s), m_hINC_LogFile);
		fflush( (FILE*) m_hINC_LogFile );
	}
#else //WinCE
	///////////////////////////////////////////////////
	// .NET 버전일 경우 wchar_t로 변환
	wchar_t wcstring[1024] = {0};
	mbstowcs(wcstring, acTmpBuff, strlen(acTmpBuff)+1);
	RETAILMSG(nFlag, (TEXT("%s"), wcstring));
	///////////////////////////////////////////////////
	if(m_hINC_LogFile && nFlag){
		SYSTEMTIME time;
		GetLocalTime(&time);

		sprintf(logstr, "[%02d.%02d %02d:%02d:%02d] %s",
			time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond, acTmpBuff);
		fwrite(logstr, sizeof(char), strlen(logstr)+1, m_hINC_LogFile);
		fflush( (FILE*) m_hINC_LogFile ); 
	}
#endif
}


INC_UINT8 INC_DRIVER_OPEN(INC_UINT8 ucI2CID)
{
#ifdef INC_WINDOWS_SPACE
	if(!g_pUsbDriver.USB_Open()){
		return INC_ERROR;
	}
#elif defined(INC_KERNEL_SPACE) //Android
	if(!INC_SPI_DRV_OPEN()){
		return INC_ERROR;
	}
#else //WinCE
	InitializeCriticalSection(&gCS_INC);
#endif

	return INC_SUCCESS;
}

INC_UINT8 INC_DRIVER_CLOSE(INC_UINT8 ucI2CID)
{
#ifdef INC_WINDOWS_SPACE
	g_pUsbDriver.USB_Close();
#elif defined(INC_KERNEL_SPACE) //Android
	INC_SPI_DRV_CLOSE();
#else //WinCE
	DeleteCriticalSection(&gCS_INC);
#endif

	return INC_SUCCESS;
}

INC_UINT8 INC_DRIVER_RESET(INC_UINT8 ucI2CID)
{
    INC_SPI_DRV_RESET();
	return INC_SUCCESS;
}


INC_UINT16 INC_I2C_READ(INC_UINT8 ucI2CID, INC_UINT16 uiAddr)
{
#ifdef INC_I2C_GPIO_CTRL_ENABLE	
	INC_UINT8 acBuff[2];
	INC_UINT16 wData;
	INC_I2C_ACK AckStatus;
	
	AckStatus = INC_GPIO_CTRL_READ(ucI2CID, uiAddr, acBuff, 2);
	if(AckStatus == I2C_ACK_SUCCESS){
		wData = ((INC_UINT16)acBuff[0] << 8) | (INC_UINT16)acBuff[1];
		return wData;
	}
	return INC_ERROR;
#elif defined(INC_WINDOWS_SPACE)
	//TODO I2C Read code here...

	INC_UINT16 uiRcvData = 0;
	uiRcvData = g_pUsbDriver.I2C_Read(uiAddr);
	return uiRcvData;
#endif
	return INC_SUCCESS;
}

INC_UINT8 INC_I2C_WRITE(INC_UINT8 ucI2CID, INC_UINT16 uiAddr, INC_UINT16 uiData)
{
#ifdef INC_I2C_GPIO_CTRL_ENABLE	
	INC_UINT8 acBuff[2];
	INC_UINT8 ucCnt = 0;
	INC_I2C_ACK AckStatus;

	acBuff[ucCnt++] = (uiData >> 8) & 0xff;
	acBuff[ucCnt++] = uiData & 0xff;
	
	AckStatus = INC_GPIO_CTRL_WRITE(ucI2CID, uiAddr, acBuff, ucCnt);
	if(AckStatus == I2C_ACK_SUCCESS)
		return INC_SUCCESS;
	return INC_ERROR;
#elif defined(INC_WINDOWS_SPACE)
	//TODO I2C write code here...
	g_pUsbDriver.I2C_Write(uiAddr, uiData);
	return INC_SUCCESS;
#endif
	return INC_SUCCESS;
}

INC_UINT8 INC_I2C_READ_BURST(INC_UINT8 ucI2CID,  INC_UINT16 uiAddr, INC_UINT8* pData, INC_UINT16 nSize)
{
#ifdef INC_I2C_GPIO_CTRL_ENABLE	
	INC_I2C_ACK AckStatus;
	AckStatus = INC_GPIO_CTRL_READ(ucI2CID, uiAddr, pData, nSize);

	if(AckStatus == I2C_ACK_SUCCESS)
		return INC_SUCCESS;
	return INC_ERROR;
#elif defined(INC_WINDOWS_SPACE)
	//TODO I2C Read code here...
	g_pUsbDriver.I2C_Dump(uiAddr, nSize, pData);
	return INC_SUCCESS;
#endif
	return INC_SUCCESS;
}

INC_UINT8 INC_EBI_WRITE(INC_UINT8 ucI2CID, INC_UINT16 uiAddr, INC_UINT16 uiData)
{
	INC_UINT16 uiCMD = INC_REGISTER_CTRL(SPI_REGWRITE_CMD) | 1;
	INC_UINT16 uiNewAddr = (ucI2CID == INC_I2C_ID82) ? (uiAddr | 0x8000) : uiAddr;

	INC_INTERRUPT_LOCK();

	*(volatile INC_UINT8*)STREAM_PARALLEL_ADDRESS = uiNewAddr >> 8;
	*(volatile INC_UINT8*)STREAM_PARALLEL_ADDRESS = uiNewAddr & 0xff;
	*(volatile INC_UINT8*)STREAM_PARALLEL_ADDRESS = uiCMD >> 8;
	*(volatile INC_UINT8*)STREAM_PARALLEL_ADDRESS = uiCMD & 0xff;

	*(volatile INC_UINT8*)STREAM_PARALLEL_ADDRESS = (uiData >> 8) & 0xff;
	*(volatile INC_UINT8*)STREAM_PARALLEL_ADDRESS =  uiData & 0xff;

	INC_INTERRUPT_FREE();

	return INC_SUCCESS;
}

INC_UINT16 INC_EBI_READ(INC_UINT8 ucI2CID, INC_UINT16 uiAddr)
{
	INC_UINT16 uiRcvData = 0;
	INC_UINT16 uiCMD = INC_REGISTER_CTRL(SPI_REGREAD_CMD) | 1;
	INC_UINT16 uiNewAddr = (ucI2CID == INC_I2C_ID82) ? (uiAddr | 0x8000) : uiAddr;

	INC_INTERRUPT_LOCK();
	*(volatile INC_UINT8*)STREAM_PARALLEL_ADDRESS = uiNewAddr >> 8;
	*(volatile INC_UINT8*)STREAM_PARALLEL_ADDRESS = uiNewAddr & 0xff;
	*(volatile INC_UINT8*)STREAM_PARALLEL_ADDRESS = uiCMD >> 8;
	*(volatile INC_UINT8*)STREAM_PARALLEL_ADDRESS = uiCMD & 0xff;

	uiRcvData  = (*(volatile INC_UINT8*)STREAM_PARALLEL_ADDRESS  & 0xff) << 8;
	uiRcvData |= (*(volatile INC_UINT8*)STREAM_PARALLEL_ADDRESS & 0xff);
	
	INC_INTERRUPT_FREE();

	return uiRcvData;
}

INC_UINT8 INC_EBI_READ_BURST(INC_UINT8 ucI2CID, INC_UINT16 uiAddr, INC_UINT8* pData, INC_UINT16 nSize)
{
	INC_UINT16 uiLoop, nIndex = 0, anLength[2], uiCMD, unDataCnt;
	INC_UINT16 uiNewAddr = (ucI2CID == INC_I2C_ID82) ? (uiAddr | 0x8000) : uiAddr;

	if(nSize > INC_MPI_MAX_BUFF) return INC_ERROR;
	memset((INC_INT8*)anLength, 0, sizeof(anLength));

	if(nSize > INC_TDMB_LENGTH_MASK) {
		anLength[nIndex++] = INC_TDMB_LENGTH_MASK;
		anLength[nIndex++] = nSize - INC_TDMB_LENGTH_MASK;
	}
	else anLength[nIndex++] = nSize;

	INC_INTERRUPT_LOCK();
	for(uiLoop = 0; uiLoop < nIndex; uiLoop++){

		uiCMD = INC_REGISTER_CTRL(SPI_MEMREAD_CMD) | (anLength[uiLoop] & INC_TDMB_LENGTH_MASK);

		*(volatile INC_UINT8*)STREAM_PARALLEL_ADDRESS = uiNewAddr >> 8;
		*(volatile INC_UINT8*)STREAM_PARALLEL_ADDRESS = uiNewAddr & 0xff;
		*(volatile INC_UINT8*)STREAM_PARALLEL_ADDRESS = uiCMD >> 8;
		*(volatile INC_UINT8*)STREAM_PARALLEL_ADDRESS = uiCMD & 0xff;

		for(unDataCnt = 0 ; unDataCnt < anLength[uiLoop]; unDataCnt++){
			*pData++ = *(volatile INC_UINT8*)STREAM_PARALLEL_ADDRESS & 0xff;
		}
	}
	INC_INTERRUPT_FREE();

	return INC_SUCCESS;
}

INC_UINT16 INC_SPI_REG_READ(INC_UINT8 ucI2CID, INC_UINT16 uiAddr)
{
	INC_UINT16 uiRcvData = 0;

	//mutex_lock(&INCio_mutex);
    uiRcvData = INC_SPI_DRV_REG_READ(uiAddr);    
	//mutex_unlock(&INCio_mutex);
	return uiRcvData;
}

INC_UINT8 INC_SPI_REG_WRITE(INC_UINT8 ucI2CID, INC_UINT16 uiAddr, INC_UINT16 uiData)
{
	//mutex_lock(&INCio_mutex);
	INC_SPI_DRV_REG_WRITE(uiAddr, uiData);
	//mutex_unlock(&INCio_mutex);
	return INC_SUCCESS;
}

INC_UINT8 INC_SPI_READ_BURST(INC_UINT8 ucI2CID, INC_UINT16 uiAddr, INC_UINT8* pBuff, INC_UINT16 wSize)
{
	//mutex_lock(&INCio_mutex);
	INC_SPI_DRV_MEM_READ(uiAddr, pBuff, wSize);
	//mutex_unlock(&INCio_mutex);
	return INC_SUCCESS;
}

INC_UINT8 INTERFACE_DBINIT(void)
{
	memset(&g_stDmbInfo,	0, sizeof(ST_SUBCH_INFO));
	memset(&g_stDabInfo,	0, sizeof(ST_SUBCH_INFO));
	memset(&g_stDataInfo,	0, sizeof(ST_SUBCH_INFO));
	memset(&g_stFIDCInfo,	0, sizeof(ST_SUBCH_INFO));
    
    memset(&g_stCHInfo,   0, sizeof(ST_SUBCH_INFO)); // by kdk    
	return INC_SUCCESS;
}

void INTERFACE_UPLOAD_MODE(INC_UINT8 ucI2CID, UPLOAD_MODE_INFO ucUploadMode)
{
	m_ucUploadMode = ucUploadMode;
	INC_UPLOAD_MODE(ucI2CID);
}

INC_UINT8 INTERFACE_PLL_MODE(INC_UINT8 ucI2CID, PLL_MODE ucPllMode)
{
	m_ucPLL_Mode = ucPllMode;
	return INC_PLL_SET(ucI2CID);
}

// 초기 전원 입력시 호출
INC_UINT8 INTERFACE_INIT(INC_UINT8 ucI2CID)
{
	return INC_INIT(ucI2CID);
}

// 에러 발생시 에러코드 읽기
INC_ERROR_INFO INTERFACE_ERROR_STATUS(INC_UINT8 ucI2CID)
{
	ST_BBPINFO* pInfo;
	pInfo = INC_GET_STRINFO(ucI2CID);
	return pInfo->nBbpStatus;
}

/*********************************************************************************/
/* 단일 채널 선택하여 시작하기....                                               */  
/* pChInfo->uiTmID, pChInfo->ucSubChID, pChInfo->ulRFFreq 는                     */
/* 반드시 넘겨주어야 한다.                                                       */
/* DMB채널 선택시 pChInfo->uiTmID = TMID_1                                       */
/* DAB채널 선택시 pChInfo->uiTmID = TMID_0 으로 설정을 해야함.                   */
/*********************************************************************************/
INC_UINT8 INTERFACE_START(INC_UINT8 ucI2CID, ST_SUBCH_INFO* pChInfo)
{
	return INC_CHANNEL_START(ucI2CID, pChInfo);
}

/*********************************************************************************/
/* 스캔시  호출한다.                                                             */
/* 주파수 값은 반드시 넘겨주어야 한다.                                           */
/*********************************************************************************/
INC_UINT8 INTERFACE_SCAN(INC_UINT8 ucI2CID, INC_UINT32 ulFreq)
{
	INC_INT16 nIndex;
	INC_CHANNEL_INFO* pChInfo;

	INTERFACE_DBINIT();
	memset(&g_stCHInfo, 0, sizeof(ST_SUBCH_INFO));

	if(!INC_ENSEMBLE_SCAN(ucI2CID, ulFreq)) return INC_ERROR;
	INC_DB_UPDATE(ulFreq, &g_stCHInfo);
	INC_BUBBLE_SORT(&g_stCHInfo,  INC_SUB_CHANNEL_ID);

	for(nIndex = 0; nIndex < g_stCHInfo.nSetCnt; nIndex++)
	{                                                    
		switch(g_stCHInfo.astSubChInfo[nIndex].uiTmID)
		{
		case TMID_1 : pChInfo = &g_stDmbInfo.astSubChInfo[g_stDmbInfo.nSetCnt++]; break;
		case TMID_0 : pChInfo = &g_stDabInfo.astSubChInfo[g_stDabInfo.nSetCnt++]; break;
		default   : pChInfo = &g_stDataInfo.astSubChInfo[g_stDataInfo.nSetCnt++]; break;
		}
		memcpy(pChInfo, &g_stCHInfo.astSubChInfo[nIndex], sizeof(INC_CHANNEL_INFO));	
	}

/*	for(nIndex=0; nIndex<g_stDmbInfo.nSetCnt; nIndex++)
	{
		INC_MSG_PRINTF(INC_DEBUG_LEVEL, " ==============================================\r\n");
		pChInfo = &g_stDmbInfo.astSubChInfo[nIndex];
		INC_MSG_PRINTF(1, " EL[%s] Sub channel label[%s]\r\n", pChInfo->aucEnsembleLabel, pChInfo->aucLabel);
		INC_MSG_PRINTF(1, " EL[%s] Sub Channel Service   ID[0x%.8X]\r\n", pChInfo->aucEnsembleLabel, pChInfo->ulServiceID);
		INC_MSG_PRINTF(1, " EL[%s] Sub Channel Service Type[0x%.4X]\r\n", pChInfo->aucEnsembleLabel, pChInfo->ucServiceType);
		INC_MSG_PRINTF(1, " EL[%s] Sub Channel           ID[0x%.4X]\r\n", pChInfo->aucEnsembleLabel, pChInfo->ucSubChID);
		INC_MSG_PRINTF(1, " EL[%s] Sub Channel         TmID[0x%.4X]\r\n", pChInfo->aucEnsembleLabel, pChInfo->uiTmID);
		INC_MSG_PRINTF(1, " EL[%s] Sub Channel     Bit Rate[0x%.4X]\r\n", pChInfo->aucEnsembleLabel, pChInfo->uiBitRate);
		INC_MSG_PRINTF(1, " EL[%s] Sub Channel   PacketAddr[0x%.4X]\r\n", pChInfo->aucEnsembleLabel, pChInfo->uiPacketAddr);
		INC_MSG_PRINTF(1, " EL[%s] Sub Channel     ucSlFlag[0x%.4X]\r\n", pChInfo->aucEnsembleLabel, pChInfo->ucSlFlag);
		INC_MSG_PRINTF(1, " EL[%s] Sub Channel   ucProtectionLevel[0x%.4X]\r\n", pChInfo->aucEnsembleLabel, pChInfo->ucProtectionLevel);
		INC_MSG_PRINTF(1, " EL[%s] Sub Channel   uiDifferentRate[0x%.4X]\r\n", pChInfo->aucEnsembleLabel, pChInfo->uiDifferentRate);
		INC_MSG_PRINTF(1, " EL[%s] Sub Channel   ucOption[0x%.4X]\r\n", pChInfo->aucEnsembleLabel, pChInfo->ucOption);
	}
*/	
	return INC_SUCCESS;
}

/*********************************************************************************/
/* 스캔이 완료되면 앙상블 label을 리턴한다.                                      */
/*********************************************************************************/
INC_INT8* INTERFACE_GET_ENSEMBLE_LABEL(void)
{
	ST_FICDB_LIST*		pList;
	pList = INC_GET_FICDB_LIST();
	return (INC_INT8*)pList->aucEnsembleName;
}

/*********************************************************************************/
/* 스캔이 완료되면 앙상블 ID를 리턴한다.                                         */
/*********************************************************************************/
INC_UINT16 INTERFACE_GET_ENSEMBLE_ID(void)
{
	ST_FICDB_LIST*		pList;
	pList = INC_GET_FICDB_LIST();
	return pList->unEnsembleID;
}

/*********************************************************************************/
/* 스캔이 완료되면 전채 서브채널 개수를 리턴한다.                                */
/*********************************************************************************/
INC_UINT16 INTERFACE_GET_SUBCHANNEL_CNT(void)
{
	return INTERFACE_GETDMB_CNT() + INTERFACE_GETDAB_CNT() + INTERFACE_GETDATA_CNT();
}

/*********************************************************************************/
/* 단일채널 스캔이 완료되면 DMB채널 개수를 리턴한다.                             */
/*********************************************************************************/
INC_UINT16 INTERFACE_GETDMB_CNT(void)
{
	return (INC_UINT16)g_stDmbInfo.nSetCnt;
}

/*********************************************************************************/
/* 단일채널 스캔이 완료되면 DAB채널 개수를 리턴한다.                             */
/*********************************************************************************/
INC_UINT16 INTERFACE_GETDAB_CNT(void)
{
	return (INC_UINT16)g_stDabInfo.nSetCnt;
}

/*********************************************************************************/
/* 단일채널 스캔이 완료되면 DATA채널 개수를 리턴한다.                            */
/*********************************************************************************/
INC_UINT16 INTERFACE_GETDATA_CNT(void)
{
	return (INC_UINT16)g_stDataInfo.nSetCnt;
}

/*********************************************************************************/
/* 단일채널 스캔이 완료되면 Ensemble label을 리턴한다.                           */
/*********************************************************************************/
INC_UINT8* INTERFACE_GETENSEMBLE_LABEL(INC_UINT8 ucI2CID)
{
	ST_FICDB_LIST*	pList;
	pList = INC_GET_FICDB_LIST();
	return pList->aucEnsembleName;
}

/*********************************************************************************/
/* 단일채널 검색이 완료되면, 검색된 모든 정보를 리턴한다.						 */
/*********************************************************************************/
ST_SUBCH_INFO* INTERFACE_GETDB_CHANNEL(void)
{
	return &g_stCHInfo;
}

/*********************************************************************************/
/* DMB 채널 정보를 리턴한다.                                                     */
/*********************************************************************************/
INC_CHANNEL_INFO* INTERFACE_GETDB_DMB(INC_INT16 uiPos)
{
	if(uiPos >= MAX_SUBCH_SIZE) return INC_NULL;
	if(uiPos >= g_stDmbInfo.nSetCnt) return INC_NULL;
	return &g_stDmbInfo.astSubChInfo[uiPos];
}

/*********************************************************************************/
/* DAB 채널 정보를 리턴한다.                                                     */
/*********************************************************************************/
INC_CHANNEL_INFO* INTERFACE_GETDB_DAB(INC_INT16 uiPos)
{
	if(uiPos >= MAX_SUBCH_SIZE) return INC_NULL;
	if(uiPos >= g_stDabInfo.nSetCnt) return INC_NULL;
	return &g_stDabInfo.astSubChInfo[uiPos];
}

/*********************************************************************************/
/* DATA 채널 정보를 리턴한다.                                                    */
/*********************************************************************************/
INC_CHANNEL_INFO* INTERFACE_GETDB_DATA(INC_INT16 uiPos)
{
	if(uiPos >= MAX_SUBCH_SIZE) return INC_NULL;
	if(uiPos >= g_stDataInfo.nSetCnt) return INC_NULL;
	return &g_stDataInfo.astSubChInfo[uiPos];
}

// 시청 중 FIC 정보 변경되었는지를 체크
INC_UINT8 INTERFACE_RECONFIG(INC_UINT8 ucI2CID)
{
	return INC_FIC_RECONFIGURATION_HW_CHECK(ucI2CID);
}

// Check the strength of Signal
INC_UINT8 INTERFACE_STATUS_CHECK(INC_UINT8 ucI2CID)
{
	return INC_STATUS_CHECK(ucI2CID);
}


/*********************************************************************************/
/* Scan, 채널 시작시에 강제로 중지시 호출한다.                                      */
/*********************************************************************************/
void INTERFACE_USER_STOP(INC_UINT8 ucI2CID)
{
	ST_BBPINFO* pInfo;
	pInfo = INC_GET_STRINFO(ucI2CID);
	pInfo->ucStop = 1;
	INC_STOP(ucI2CID);
}

// Interrupt Start...
void INTERFACE_INT_ENABLE(INC_UINT8 ucI2CID, INC_UINT16 unSet)
{
	INC_INTERRUPT_ENABLE(ucI2CID, unSet);
}

// Use when polling mode
INC_UINT8 INTERFACE_INT_CHECK(INC_UINT8 ucI2CID)
{
	INC_UINT16 nValue = 0;

	nValue = INC_CMD_READ(ucI2CID, APB_INT_BASE+ 0x01);
	if(!(nValue & INC_MPI_INTERRUPT_ENABLE))
		return INC_ERROR;

	return INC_SUCCESS;
}

// Interrupt clear.
void INTERFACE_INT_CLEAR(INC_UINT8 ucI2CID, INC_UINT16 unClr)
{
	INC_INTERRUPT_CLEAR(ucI2CID, unClr);
}

// Interrupt Service Routine... // SPI Slave Mode or MPI Slave Mode
// It's sample function..
INC_UINT8 INTERFACE_ISR(INC_UINT8 ucI2CID, INC_UINT8* pBuff)
{
	INC_UINT16 unData;
	unData = INC_CMD_READ(ucI2CID, APB_MPI_BASE+ 0x6);
	if(unData < INC_INTERRUPT_SIZE) return INC_ERROR;

	INC_CMD_READ_BURST(ucI2CID, APB_STREAM_BASE, pBuff, INC_INTERRUPT_SIZE);

	if((m_unIntCtrl & INC_INTERRUPT_LEVEL) && (!(m_unIntCtrl & INC_INTERRUPT_AUTOCLEAR_ENABLE)))
		INTERFACE_INT_CLEAR(ucI2CID, INC_MPI_INTERRUPT_ENABLE);

	return INC_SUCCESS;
}

INC_UINT8 SAVE_CHANNEL_INFO(char* pStr)
{
#ifdef INC_WINDOWS_SPACE
	FILE* pFile = fopen(pStr, "wb+");
	if(pFile == NULL)
		return INC_ERROR;

	DWORD dwWriteLen = 0;

	dwWriteLen = fwrite(&g_stDabInfo, sizeof(char), sizeof(ST_SUBCH_INFO), pFile);
	if( dwWriteLen != sizeof(ST_SUBCH_INFO) )
	{
		memset(&g_stDabInfo, 0x00, sizeof(ST_SUBCH_INFO));
	}

	dwWriteLen = fwrite(&g_stDmbInfo, sizeof(char), sizeof(ST_SUBCH_INFO), pFile);
	if( dwWriteLen != sizeof(ST_SUBCH_INFO) )
	{
		memset(&g_stDmbInfo, 0x00, sizeof(ST_SUBCH_INFO));
	}

	dwWriteLen = fwrite(&g_stDataInfo, sizeof(char), sizeof(ST_SUBCH_INFO), pFile);
	if( dwWriteLen != sizeof(ST_SUBCH_INFO) )
	{
		memset(&g_stDataInfo, 0x00, sizeof(ST_SUBCH_INFO));
	}

	fclose(pFile);
#endif
	return INC_SUCCESS;
}

INC_UINT8 LOAD_CHANNEL_INFO(char* pStr)
{
#ifdef INC_WINDOWS_SPACE
	FILE* pFile = fopen(pStr, "rb");
	if(pFile == NULL)
		return INC_ERROR;

	DWORD dwReadLen = 0;

	dwReadLen = fread(&g_stDabInfo, sizeof(char), sizeof(ST_SUBCH_INFO), pFile);
	if( dwReadLen != sizeof(ST_SUBCH_INFO) )
	{
		memset(&g_stDabInfo, 0x00, sizeof(ST_SUBCH_INFO));
	}

	dwReadLen = fread(&g_stDmbInfo, sizeof(char), sizeof(ST_SUBCH_INFO), pFile);
	if( dwReadLen != sizeof(ST_SUBCH_INFO) )
	{
		memset(&g_stDmbInfo, 0x00, sizeof(ST_SUBCH_INFO));
	}

	dwReadLen = fread(&g_stDataInfo, sizeof(char), sizeof(ST_SUBCH_INFO), pFile);
	if( dwReadLen != sizeof(ST_SUBCH_INFO) )
	{
		memset(&g_stDataInfo, 0x00, sizeof(ST_SUBCH_INFO));
	}
	fclose(pFile);
#endif
	return INC_SUCCESS;
}

void INTERFACE_INC_DEBUG(INC_UINT8 ucI2CID)
{
	INC_UINT16 nLoop = 0;
	for(nLoop = 0; nLoop < 3; nLoop++)
	{
		INC_MSG_PRINTF(INC_DEBUG_LEVEL, " APB_DEINT_BASE+ 0x02+%d : 0x%X \r\n", nLoop*2, INC_CMD_READ(INC_I2C_ID80, APB_DEINT_BASE+ 0x02 + (nLoop*2)));
		INC_MSG_PRINTF(INC_DEBUG_LEVEL, " APB_DEINT_BASE+ 0x03+%d : 0x%X \r\n", nLoop*2, INC_CMD_READ(INC_I2C_ID80, APB_DEINT_BASE+ 0x03 + (nLoop*2)));
		INC_MSG_PRINTF(INC_DEBUG_LEVEL, " APB_VTB_BASE  + 0x02+%d : 0x%X \r\n", nLoop, INC_CMD_READ(INC_I2C_ID80, APB_VTB_BASE+ 0x02 + nLoop));
	}

	INC_MSG_PRINTF(INC_DEBUG_LEVEL, " APB_MPI_BASE+ 0x00 : 0x%X \r\n", INC_CMD_READ(INC_I2C_ID80, APB_MPI_BASE+ 0x00));
	INC_MSG_PRINTF(INC_DEBUG_LEVEL, " APB_MPI_BASE+ 0x01 : 0x%X \r\n", INC_CMD_READ(INC_I2C_ID80, APB_MPI_BASE+ 0x01));
	INC_MSG_PRINTF(INC_DEBUG_LEVEL, " APB_MPI_BASE+ 0x02 : 0x%X \r\n", INC_CMD_READ(INC_I2C_ID80, APB_MPI_BASE+ 0x02));
	INC_MSG_PRINTF(INC_DEBUG_LEVEL, " APB_MPI_BASE+ 0x03 : 0x%X \r\n", INC_CMD_READ(INC_I2C_ID80, APB_MPI_BASE+ 0x03));
	INC_MSG_PRINTF(INC_DEBUG_LEVEL, " APB_MPI_BASE+ 0x04 : 0x%X \r\n", INC_CMD_READ(INC_I2C_ID80, APB_MPI_BASE+ 0x04));
	INC_MSG_PRINTF(INC_DEBUG_LEVEL, " APB_MPI_BASE+ 0x05 : 0x%X \r\n", INC_CMD_READ(INC_I2C_ID80, APB_MPI_BASE+ 0x05));
	INC_MSG_PRINTF(INC_DEBUG_LEVEL, " APB_MPI_BASE+ 0x06 : 0x%X \r\n", INC_CMD_READ(INC_I2C_ID80, APB_MPI_BASE+ 0x06));
	INC_MSG_PRINTF(INC_DEBUG_LEVEL, " APB_MPI_BASE+ 0x07 : 0x%X \r\n", INC_CMD_READ(INC_I2C_ID80, APB_MPI_BASE+ 0x07));
	INC_MSG_PRINTF(INC_DEBUG_LEVEL, " APB_MPI_BASE+ 0x08 : 0x%X \r\n", INC_CMD_READ(INC_I2C_ID80, APB_MPI_BASE+ 0x08));


	// INIT
	INC_MSG_PRINTF(INC_DEBUG_LEVEL, " APB_INT_BASE+ 0x00 : 0x%X \r\n", INC_CMD_READ(INC_I2C_ID80, APB_INT_BASE+ 0x00));
	INC_MSG_PRINTF(INC_DEBUG_LEVEL, " APB_INT_BASE+ 0x01 : 0x%X \r\n", INC_CMD_READ(INC_I2C_ID80, APB_INT_BASE+ 0x01));
	INC_MSG_PRINTF(INC_DEBUG_LEVEL, " APB_INT_BASE+ 0x02 : 0x%X \r\n", INC_CMD_READ(INC_I2C_ID80, APB_INT_BASE+ 0x02));
	INC_MSG_PRINTF(INC_DEBUG_LEVEL, " APB_INT_BASE+ 0x03 : 0x%X \r\n", INC_CMD_READ(INC_I2C_ID80, APB_INT_BASE+ 0x03));

	INC_MSG_PRINTF(INC_DEBUG_LEVEL, " APB_PHY_BASE+ 0x3B : 0x%X \r\n", INC_CMD_READ(INC_I2C_ID80, APB_PHY_BASE+ 0x3B));
	INC_MSG_PRINTF(INC_DEBUG_LEVEL, " APB_PHY_BASE+ 0x00 : 0x%X \r\n", INC_CMD_READ(INC_I2C_ID80, APB_PHY_BASE+ 0x00));
	INC_MSG_PRINTF(INC_DEBUG_LEVEL, " APB_PHY_BASE+ 0x84 : 0x%X \r\n", INC_CMD_READ(INC_I2C_ID80, APB_PHY_BASE+ 0x84));
	INC_MSG_PRINTF(INC_DEBUG_LEVEL, " APB_PHY_BASE+ 0x86 : 0x%X \r\n", INC_CMD_READ(INC_I2C_ID80, APB_PHY_BASE+ 0x86));
	INC_MSG_PRINTF(INC_DEBUG_LEVEL, " APB_PHY_BASE+ 0xB4 : 0x%X \r\n", INC_CMD_READ(INC_I2C_ID80, APB_PHY_BASE+ 0xB4));
	INC_MSG_PRINTF(INC_DEBUG_LEVEL, " APB_PHY_BASE+ 0x1A : 0x%X \r\n", INC_CMD_READ(INC_I2C_ID80, APB_PHY_BASE+ 0x1A));
	INC_MSG_PRINTF(INC_DEBUG_LEVEL, " APB_PHY_BASE+ 0x8A : 0x%X \r\n", INC_CMD_READ(INC_I2C_ID80, APB_PHY_BASE+ 0x8A));
	INC_MSG_PRINTF(INC_DEBUG_LEVEL, " APB_PHY_BASE+ 0xC4 : 0x%X \r\n", INC_CMD_READ(INC_I2C_ID80, APB_PHY_BASE+ 0xC4));
	INC_MSG_PRINTF(INC_DEBUG_LEVEL, " APB_PHY_BASE+ 0x24 : 0x%X \r\n", INC_CMD_READ(INC_I2C_ID80, APB_PHY_BASE+ 0x24));
	INC_MSG_PRINTF(INC_DEBUG_LEVEL, " APB_PHY_BASE+ 0xBE : 0x%X \r\n", INC_CMD_READ(INC_I2C_ID80, APB_PHY_BASE+ 0xBE));
	INC_MSG_PRINTF(INC_DEBUG_LEVEL, " APB_PHY_BASE+ 0xB0 : 0x%X \r\n", INC_CMD_READ(INC_I2C_ID80, APB_PHY_BASE+ 0xB0));
	INC_MSG_PRINTF(INC_DEBUG_LEVEL, " APB_PHY_BASE+ 0xC0 : 0x%X \r\n", INC_CMD_READ(INC_I2C_ID80, APB_PHY_BASE+ 0xC0));
	INC_MSG_PRINTF(INC_DEBUG_LEVEL, " APB_PHY_BASE+ 0x8C : 0x%X \r\n", INC_CMD_READ(INC_I2C_ID80, APB_PHY_BASE+ 0x8C));
	INC_MSG_PRINTF(INC_DEBUG_LEVEL, " APB_PHY_BASE+ 0xA8 : 0x%X \r\n", INC_CMD_READ(INC_I2C_ID80, APB_PHY_BASE+ 0xA8));
	INC_MSG_PRINTF(INC_DEBUG_LEVEL, " APB_PHY_BASE+ 0xAA : 0x%X \r\n", INC_CMD_READ(INC_I2C_ID80, APB_PHY_BASE+ 0xAA));
	INC_MSG_PRINTF(INC_DEBUG_LEVEL, " APB_PHY_BASE+ 0x80 : 0x%X \r\n", INC_CMD_READ(INC_I2C_ID80, APB_PHY_BASE+ 0x80));
	INC_MSG_PRINTF(INC_DEBUG_LEVEL, " APB_PHY_BASE+ 0x88 : 0x%X \r\n", INC_CMD_READ(INC_I2C_ID80, APB_PHY_BASE+ 0x88));
	INC_MSG_PRINTF(INC_DEBUG_LEVEL, " APB_PHY_BASE+ 0xC8 : 0x%X \r\n", INC_CMD_READ(INC_I2C_ID80, APB_PHY_BASE+ 0xC8));
	INC_MSG_PRINTF(INC_DEBUG_LEVEL, " APB_PHY_BASE+ 0xBC : 0x%X \r\n", INC_CMD_READ(INC_I2C_ID80, APB_PHY_BASE+ 0xBC));
	INC_MSG_PRINTF(INC_DEBUG_LEVEL, " APB_PHY_BASE+ 0x90 : 0x%X \r\n", INC_CMD_READ(INC_I2C_ID80, APB_PHY_BASE+ 0x90));
	INC_MSG_PRINTF(INC_DEBUG_LEVEL, " APB_PHY_BASE+ 0xCA : 0x%X \r\n", INC_CMD_READ(INC_I2C_ID80, APB_PHY_BASE+ 0xCA));
	INC_MSG_PRINTF(INC_DEBUG_LEVEL, " APB_PHY_BASE+ 0x40 : 0x%X \r\n", INC_CMD_READ(INC_I2C_ID80, APB_PHY_BASE+ 0x40));
	INC_MSG_PRINTF(INC_DEBUG_LEVEL, " APB_PHY_BASE+ 0x24 : 0x%X \r\n", INC_CMD_READ(INC_I2C_ID80, APB_PHY_BASE+ 0x24));
	INC_MSG_PRINTF(INC_DEBUG_LEVEL, " APB_PHY_BASE+ 0x41 : 0x%X \r\n", INC_CMD_READ(INC_I2C_ID80, APB_PHY_BASE+ 0x41));
	INC_MSG_PRINTF(INC_DEBUG_LEVEL, " APB_PHY_BASE+ 0xC6 : 0x%X \r\n", INC_CMD_READ(INC_I2C_ID80, APB_PHY_BASE+ 0xC6));

	INC_MSG_PRINTF(INC_DEBUG_LEVEL, " APB_VTB_BASE+ 0x05 : 0x%X \r\n", INC_CMD_READ(INC_I2C_ID80, APB_VTB_BASE+ 0x05));
	INC_MSG_PRINTF(INC_DEBUG_LEVEL, " APB_VTB_BASE+ 0x01 : 0x%X \r\n", INC_CMD_READ(INC_I2C_ID80, APB_VTB_BASE+ 0x01));
	INC_MSG_PRINTF(INC_DEBUG_LEVEL, " APB_VTB_BASE+ 0x00 : 0x%X \r\n", INC_CMD_READ(INC_I2C_ID80, APB_VTB_BASE+ 0x00));

	INC_MSG_PRINTF(INC_DEBUG_LEVEL, " APB_RS_BASE+ 0x00 : 0x%X \r\n", INC_CMD_READ(INC_I2C_ID80, APB_RS_BASE+ 0x00));
	INC_MSG_PRINTF(INC_DEBUG_LEVEL, " APB_RS_BASE+ 0x07 : 0x%X \r\n", INC_CMD_READ(INC_I2C_ID80, APB_RS_BASE+ 0x07));
	INC_MSG_PRINTF(INC_DEBUG_LEVEL, " APB_RS_BASE+ 0x0A : 0x%X \r\n", INC_CMD_READ(INC_I2C_ID80, APB_RS_BASE+ 0x0A));
	INC_MSG_PRINTF(INC_DEBUG_LEVEL, " APB_RS_BASE+ 0x01 : 0x%X \r\n", INC_CMD_READ(INC_I2C_ID80, APB_RS_BASE+ 0x01));
}

