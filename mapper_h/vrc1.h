/*
 * Copyright (C) 2017 FIX94
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#ifndef vrc1_h_
#define vrc1_h_

void vrc1init(uint8_t *prgROM, uint32_t prgROMsize, 
			uint8_t *prgRAM, uint32_t prgRAMsize,
			uint8_t *chrROM, uint32_t chrROMsize);
uint8_t vrc1get8(uint16_t addr, uint8_t val);
void vrc1set8(uint16_t addr, uint8_t val);
uint8_t vrc1chrGet8(uint16_t addr);
void vrc1chrSet8(uint16_t addr, uint8_t val);

#endif
