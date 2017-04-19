/*
 * Copyright (C) 2017 FIX94
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#ifndef _AUDIO_VRC7_H_
#define _AUDIO_VRC7_H_

void vrc7AudioInit();
void vrc7AudioCycle();
void vrc7AudioSet8(uint8_t addr, uint8_t val);

extern bool vrc7enabled;
extern int vrc7Out;

#endif
