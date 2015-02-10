
#include "INC_INCLUDES.h"

ST_TS_RINGBUFF g_stRingBuff_Fic;
ST_TS_RINGBUFF g_stRingBuff_Dmb;
ST_TS_RINGBUFF g_stRingBuff_Dab;
ST_TS_RINGBUFF g_stRingBuff_Data;

//TS_HEADER_INFO g_stHeaderInfo;

//extern DWORD g_DebugMsgEnable;
//extern DWORD g_StatusMsgEnable;


bool INC_Buffer_Init(PST_TS_RINGBUFF pstBuffPtr)
{
	int nLoop;
	bool bResult = true;
	
	if(pstBuffPtr == NULL)
    {
		bResult = false;
    }

	pstBuffPtr->uiIndexHead = 0;
	pstBuffPtr->uiIndexTail = 0;
	pstBuffPtr->bBufferEmpty = true;
	pstBuffPtr->bBufferFull = false;
	pstBuffPtr->uiGetBuffCount = 0;	

	for(nLoop = 0; nLoop < MAX_RINGBUFF_COUNT; nLoop++)
	{
		pstBuffPtr->astChannelData[nLoop].uiDataSize = 0;
	}

	return bResult;
}

bool INC_Buffer_Empty(PST_TS_RINGBUFF pstBuffPtr)
{
	if(pstBuffPtr->uiIndexHead == pstBuffPtr->uiIndexTail)
		pstBuffPtr->bBufferEmpty = true;
	else 
		pstBuffPtr->bBufferEmpty = false;

	return pstBuffPtr->bBufferEmpty;
}

bool INC_Buffer_Full(PST_TS_RINGBUFF pstBuffPtr)
{
	if(((pstBuffPtr->uiIndexHead + 1) & (MAX_RINGBUFF_COUNT - 1)) == pstBuffPtr->uiIndexTail)
		pstBuffPtr->bBufferFull = true;
	else 
		pstBuffPtr->bBufferFull = false;

	return pstBuffPtr->bBufferFull;
}

unsigned int INC_Get_Buffer_Count(PST_TS_RINGBUFF pstBuffPtr)
{
	if(pstBuffPtr->uiIndexHead < pstBuffPtr->uiIndexTail){
		pstBuffPtr->uiGetBuffCount = (pstBuffPtr->uiIndexHead + MAX_RINGBUFF_COUNT) - pstBuffPtr->uiIndexTail;
	}
	else{
		pstBuffPtr->uiGetBuffCount = pstBuffPtr->uiIndexHead - pstBuffPtr->uiIndexTail;
	}

	return pstBuffPtr->uiGetBuffCount;
}

unsigned char* INC_Put_Ptr_Buffer(PST_TS_RINGBUFF pstBuffPtr, unsigned int uiWriteSize, unsigned int uiTabSize)
{
	unsigned char* pSourceBuff = NULL;

	if(uiTabSize > MAX_BUFF_SIZE)
	{
		return pSourceBuff;
	}
	
	if( (pstBuffPtr->astChannelData[pstBuffPtr->uiIndexHead].uiDataSize + uiWriteSize) > uiTabSize )
	{
		pstBuffPtr->uiIndexHead++;
		pstBuffPtr->uiIndexHead &= (MAX_RINGBUFF_COUNT- 1);		
		pstBuffPtr->astChannelData[pstBuffPtr->uiIndexHead].uiDataSize = 0;
	}
	
	pSourceBuff = pstBuffPtr->astChannelData[pstBuffPtr->uiIndexHead].aucBuff + pstBuffPtr->astChannelData[pstBuffPtr->uiIndexHead].uiDataSize;
	pstBuffPtr->astChannelData[pstBuffPtr->uiIndexHead].uiDataSize += uiWriteSize;
	
	return pSourceBuff;
}

unsigned char* INC_Get_Ptr_Buffer(PST_TS_RINGBUFF pstBuffPtr, unsigned int* pDataSize)
{
	unsigned char* pDestBuff = NULL;

	pDestBuff = pstBuffPtr->astChannelData[pstBuffPtr->uiIndexTail].aucBuff;
	*pDataSize = pstBuffPtr->astChannelData[pstBuffPtr->uiIndexTail].uiDataSize;

	pstBuffPtr->uiIndexTail++;
	pstBuffPtr->uiIndexTail &= (MAX_RINGBUFF_COUNT- 1);		
	
	return pDestBuff;
}

#if 0
bool INC_Header_Parsing(unsigned char* pBuff)
{
	unsigned int uiLoop, uiBuffCnt;
	
	PST_TS_RINGBUFF pTsRingBuff = NULL;	
	PTS_HEADER_INFO pstHeaderInfo = &g_stHeaderInfo;

	uiBuffCnt = 0;
	
	pstHeaderInfo->uiStartCmd = (((pBuff[0] << 8) | pBuff[1]) & 0xffff);
	
	if(pstHeaderInfo->uiStartCmd != 0x3300)
	{
		return false;
	}
	
	pstHeaderInfo->uiCifCount = (((pBuff[2] << 8) | pBuff[3]) & 0xffff);
	pstHeaderInfo->uiCifSize = (((pBuff[4] << 8) | pBuff[5]) & 0xffff);
	pstHeaderInfo->uiTiiSize = (((pBuff[6] << 8) | pBuff[7]) & 0xffff);

	if(pstHeaderInfo->uiCifSize & 0x8000)
	{
		pstHeaderInfo->uiCifSize *= 2;
	}

	//RETAILMSG(1, (TEXT("uiStartCmd = 0x%x \r\n"),pstHeaderInfo->uiStartCmd));
	//RETAILMSG(1, (TEXT("uiCifSize = 0x%x \r\n"),pstHeaderInfo->uiCifSize));

	for(uiLoop = 0; uiLoop < MAX_SUBCH_COUNT; uiLoop++)
	{
		pstHeaderInfo->auiSubChId[uiLoop] = 0xff;
		pstHeaderInfo->auiSubChSize[uiLoop] = 0;
	
		pstHeaderInfo->auiSubChId[uiLoop] = ((pBuff[8+(uiLoop*2)] >> 2) & 0x3f);
		pstHeaderInfo->auiSubChSize[uiLoop] = (((pBuff[8+(uiLoop*2)] << 8) | pBuff[9+(uiLoop*2)]) & 0x03ff) * 2;
		
		//RETAILMSG(1, (TEXT("auiSubChId[%d] = 0x%x \r\n"), uiLoop, pstHeaderInfo->auiSubChId[uiLoop]));
		//RETAILMSG(1, (TEXT("auiSubChSize[%d] = %d \r\n"), uiLoop, pstHeaderInfo->auiSubChSize[uiLoop]));
	}

	return true;
}

bool INC_Fifo_Process(unsigned char* pBuff, unsigned int uiSize, PST_SUBCH_INFO pChInfo)
{
	unsigned int uiLoop, uiIndex, uiBuffCnt;
	unsigned char* pSourceBuff;
	
	PST_TS_RINGBUFF pTsRingBuff = NULL;	
	PTS_HEADER_INFO pstHeaderInfo = &g_stHeaderInfo;

	uiBuffCnt = 0;
		
	pstHeaderInfo->uiStartCmd = (((pBuff[0] << 8) | pBuff[1]) & 0xffff);
	pstHeaderInfo->uiCifCount = (((pBuff[2] << 8) | pBuff[3]) & 0xffff);
	pstHeaderInfo->uiCifSize = (((pBuff[4] << 8) | pBuff[5]) & 0xffff);
	pstHeaderInfo->uiTiiSize = (((pBuff[6] << 8) | pBuff[7]) & 0xffff);

	//RETAILMSG(1, (TEXT("uiStartCmd = 0x%x \r\n"),pstHeaderInfo->uiStartCmd));
	//RETAILMSG(1, (TEXT("uiCifSize = 0x%x \r\n"),pstHeaderInfo->uiCifSize));

	for(uiLoop = 0; uiLoop < MAX_SUBCH_COUNT; uiLoop++)
	{
		pstHeaderInfo->auiSubChId[uiLoop] = 0xff;
		pstHeaderInfo->auiSubChSize[uiLoop] = 0;
	
		pstHeaderInfo->auiSubChId[uiLoop] = ((pBuff[8+uiLoop] >> 2) & 0x3f);
		pstHeaderInfo->auiSubChSize[uiLoop] = (((pBuff[8+uiLoop] << 8) | pBuff[9+uiLoop]) & 0x03ff) << 1;

		//RETAILMSG(1, (TEXT("auiSubChId[%d] = 0x%x \r\n"), uiLoop, pstHeaderInfo->auiSubChId[uiLoop]));
		//RETAILMSG(1, (TEXT("auiSubChSize[%d] = 0x%x \r\n"), uiLoop, pstHeaderInfo->auiSubChSize[uiLoop]));
	}

	uiBuffCnt = INC_TS_HEADER_SIZE;
	
	//pstHeaderInfo->auiSubChId[0] = ((pBuff[8] >> 2) & 0x3f);
	//pstHeaderInfo->auiSubChId[1] = ((pBuff[10] >> 2) & 0x3f);
	//pstHeaderInfo->auiSubChId[2] = ((pBuff[12] >> 2) & 0x3f);
	//pstHeaderInfo->auiSubChId[3] = ((pBuff[14] >> 2) & 0x3f);

	//pstHeaderInfo->auiSubChSize[0] = (((pBuff[8] << 8) | pBuff[9]) & 0x03ff) << 1;
	//pstHeaderInfo->auiSubChSize[1] = (((pBuff[10] << 8) | pBuff[11]) & 0x03ff) << 1;
	//pstHeaderInfo->auiSubChSize[2] = (((pBuff[12] << 8) | pBuff[13]) & 0x03ff) << 1;
	//pstHeaderInfo->auiSubChSize[3] = (((pBuff[14] << 8) | pBuff[15]) & 0x03ff) << 1;
	
	if(pstHeaderInfo->uiStartCmd == 0x3300)
	{
		for(uiIndex = 0; uiIndex < (unsigned int)pChInfo->nSetCnt; uiIndex++)
		{
			for(uiLoop = 1; uiLoop < MAX_SUBCH_COUNT; uiLoop++)
			{
				if(!pstHeaderInfo->auiSubChSize[uiLoop])
				{
					continue;
				}
				
				if(pChInfo->astSubChInfo[uiIndex].ucSubChID == pstHeaderInfo->auiSubChId[uiLoop])
				{
					if(pChInfo->astSubChInfo[uiIndex].uiTmID == TMID_1)
					{
						pTsRingBuff = &g_stRingBuff_Dmb;
					}
					else if(pChInfo->astSubChInfo[uiIndex].uiTmID == TMID_0)
					{
						pTsRingBuff = &g_stRingBuff_Dab;
					}
					else
					{
						pTsRingBuff = &g_stRingBuff_Data;
					}

					if(!INC_Buffer_Full(pTsRingBuff))
					{
						pSourceBuff = INC_Put_Ptr_Buffer(pTsRingBuff, pstHeaderInfo->auiSubChSize[uiLoop], MAX_GET_FIFO_SIZE); // MSC1
						memcpy(pSourceBuff, pBuff+uiBuffCnt, pstHeaderInfo->auiSubChSize[uiLoop]);
						uiBuffCnt += pstHeaderInfo->auiSubChSize[uiLoop];
					}
					else
					{
						INC_Buffer_Init(pTsRingBuff);
					}
				}
			}
		}
		return true;
	}
	else
	{
		return false;
	}
}
#endif

