/*
 * Copyright (C) 2017 FIX94
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#ifndef _apu_h_
#define _apu_h_

//#define APU_FREQUENCY 1789773.f/2.f
//our actually effective output at 60hz
#define APU_FREQUENCY 893415.f
#define APU_BUF_SIZE ((int)(APU_FREQUENCY/60.f))
#define APU_BUF_SIZE_BYTES APU_BUF_SIZE*sizeof(float)
#define NUM_BUFFERS 8

void apuInit();
void apuDeinit();
int apuCycle();
void apuClockTimers();
uint8_t *apuGetBuf();
void apuSet8(uint8_t reg, uint8_t val);
uint8_t apuGet8(uint8_t reg);
void apuLenCycle();

#endif
