/*
 * Copyright (C) 2017 FIX94
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#ifndef _AUDIO_n163_H_
#define _AUDIO_n163_H_

void n163AudioInit();
void n163AudioClockTimers();
void n163AudioSet8(uint16_t addr, uint8_t val);
uint8_t n163AudioGet8(uint16_t addr, uint8_t val);

extern bool n163enabled;
extern int16_t n163Out;

#endif
