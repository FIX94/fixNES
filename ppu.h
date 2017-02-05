/*
 * Copyright (C) 2017 FIX94
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#ifndef _ppu_h_
#define _ppu_h_

void ppuInit();
bool ppuCycle();
bool ppuDrawDone();
uint8_t ppuGet8(uint8_t reg);
void ppuSet8(uint8_t reg, uint8_t val);
void ppuLoadOAM(uint8_t val);
bool ppuNMI();
void ppuDumpMem();
uint16_t ppuGetCurVramAddr();

extern uint8_t ppuScreenMode;

#define PPU_MODE_SINGLE 0
#define PPU_MODE_VERTICAL 1
#define PPU_MODE_HORIZONTAL 2
#define PPU_MODE_FOURSCREEN 3

#endif