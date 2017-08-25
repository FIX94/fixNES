/*
 * Copyright (C) 2017 FIX94
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#ifndef _audio_mmc5_h_
#define _audio_mmc5_h_

void mmc5AudioInit();
void mmc5AudioCycle();
void mmc5AudioClockTimers();
void mmc5AudioSet8(uint8_t reg, uint8_t val);
uint8_t mmc5AudioGet8(uint8_t reg);
void mmc5AudioLenCycle();
void mmc5AudioPCMWrite(uint8_t val);

extern bool mmc5enabled;
extern uint8_t mmc5Out;
extern uint8_t mmc5pcm;
extern bool mmc5_dmcreadmode;

#endif
