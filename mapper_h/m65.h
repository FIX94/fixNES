/*
 * Copyright (C) 2017 FIX94
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#ifndef m65_h_
#define m65_h_

void m65init(uint8_t *prgROM, uint32_t prgROMsize, 
			uint8_t *prgRAM, uint32_t prgRAMsize,
			uint8_t *chrROM, uint32_t chrROMsize);
uint8_t m65get8(uint16_t addr, uint8_t val);
void m65set8(uint16_t addr, uint8_t val);
uint8_t m65chrGet8(uint16_t addr);
void m65chrSet8(uint16_t addr, uint8_t val);
void m65cycle();

#endif
