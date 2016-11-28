/*
 *
 * File name: nxb220_rf.h
 *
 * Description : NXB220 RF services header file.
 *
 * Copyright (C) (2013, NEXELL)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation version 2.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __NXB220_RF_H__
#define __NXB220_RF_H__

#include "nxb220_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

extern UINT g_dwRtvPrevChFreqKHz;

INT nxtvRF_SetFrequency_FULLSEG(enum E_NXTV_SERVICE_TYPE eServiceType,
	enum E_NXTV_BANDWIDTH_TYPE eLpfBwType, U32 dwChFreqKHz);
INT nxtvRF_Initilize_FULLSEG(enum E_NXTV_BANDWIDTH_TYPE eBandwidthType);

#ifdef __cplusplus
}
#endif

#endif /* __NXB220_RF_H__ */

