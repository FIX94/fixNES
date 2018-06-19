/*
 * Copyright (C) 2017 FIX94
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#ifndef _AUDIO_VRC6_H_
#define _AUDIO_VRC6_H_

#include "common.h"

void vrc6AudioInit();
FIXNES_NOINLINE void vrc6AudioCycle();
void vrc6AudioClockTimers();
void vrc6AudioSet8(uint16_t addr, uint8_t val);

extern uint8_t vrc6Out;

#endif
