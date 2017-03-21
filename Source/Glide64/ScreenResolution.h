/****************************************************************************
*                                                                           *
* Project64 - A Nintendo 64 emulator.                                       *
* http://www.pj64-emu.com/                                                  *
* Copyright (C) 2012 Project64. All rights reserved.                        *
*                                                                           *
* License:                                                                  *
* GNU/GPLv2 http://www.gnu.org/licenses/gpl-2.0.html                        *
*                                                                           *
****************************************************************************/
#pragma once
#include <Common/stdtypes.h>

uint32_t GetScreenResolutionCount();
uint32_t GetDefaultScreenRes();
uint32_t GetScreenResWidth(uint32_t index);
uint32_t GetScreenResHeight(uint32_t index);
const char * GetScreenResolutionName(uint32_t index);

int GetCurrentResIndex(void);
uint32_t GetFullScreenResWidth(uint32_t index);
uint32_t GetFullScreenResHeight(uint32_t index);
bool EnterFullScreen(uint32_t index);
