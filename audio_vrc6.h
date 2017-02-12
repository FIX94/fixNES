/*
 * Copyright (C) 2017 FIX94
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#ifndef _AUDIO_VRC6_H_
#define _AUDIO_VRC6_H_

void vrc6Init();
void vrc6Cycle();
void vrc6ClockTimers();
void vrc6Set8(uint16_t addr, uint8_t val);

extern bool vrc6enabled;
extern uint8_t vrc6Out;

#endif
