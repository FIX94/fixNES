/*
 * Copyright (C) 2017 FIX94
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#ifndef _AUDIO_VRC7_H_
#define _AUDIO_VRC7_H_

void vrc7AudioInit();
__attribute__((noinline)) void vrc7AudioCycle();
void vrc7AudioSet8(uint8_t addr, uint8_t val);

extern int32_t vrc7Out;

#endif
