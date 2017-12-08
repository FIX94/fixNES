/*
 * Copyright (C) 2017 FIX94
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#ifndef _AUDIO_s5B_H_
#define _AUDIO_s5B_H_

void s5BAudioInit();
void s5BAudioClockTimers();
void s5BAudioSet8(uint16_t addr, uint8_t val);
void s5BAudioCycle();

extern bool s5Benabled;
extern uint16_t s5BOut;

#endif
