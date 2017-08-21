/*
 * Copyright (C) 2017 FIX94
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#ifndef m5_h_
#define m5_h_

void m5init(uint8_t *prgROM, uint32_t prgROMsize, 
			uint8_t *prgRAM, uint32_t prgRAMsize,
			uint8_t *chrROM, uint32_t chrROMsize);
uint8_t m5get8(uint16_t addr, uint8_t val);
void m5set8(uint16_t addr, uint8_t val);
uint8_t m5chrGet8(uint16_t addr);
void m5chrSet8(uint16_t addr, uint8_t val);
uint8_t m5vramGet8(uint16_t addr);
void m5vramSet8(uint16_t addr, uint8_t val);
void m5cycle();

#endif
