/*
 * Copyright (C) 2017 FIX94
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#ifndef p16_h_
#define p16_h_

void p16init(uint8_t *prgROM, uint32_t prgROMsize, 
			uint8_t *prgRAM, uint32_t prgRAMsize,
			uint8_t *chrROM, uint32_t chrROMsize);
uint8_t p16get8(uint16_t addr);
void m2_set8(uint16_t addr, uint8_t val);
void m71_set8(uint16_t addr, uint8_t val);
uint8_t p16chrGet8(uint16_t addr);
void p16chrSet8(uint16_t addr, uint8_t val);

#endif
