/*
 * Copyright (C) 2017 FIX94
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#ifndef vrc6_h_
#define vrc6_h_

void vrc6init(uint8_t *prgROM, uint32_t prgROMsize, 
			uint8_t *prgRAM, uint32_t prgRAMsize,
			uint8_t *chrROM, uint32_t chrROMsize);
uint8_t vrc6get8(uint16_t addr);
void m24_set8(uint16_t addr, uint8_t val);
void m26_set8(uint16_t addr, uint8_t val);
uint8_t vrc6chrGet8(uint16_t addr);
void vrc6chrSet8(uint16_t addr, uint8_t val);
void vrc6cycle();

#endif
