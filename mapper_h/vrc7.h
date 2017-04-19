/*
 * Copyright (C) 2017 FIX94
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#ifndef vrc7_h_
#define vrc7_h_

void vrc7init(uint8_t *prgROM, uint32_t prgROMsize, 
			uint8_t *prgRAM, uint32_t prgRAMsize,
			uint8_t *chrROM, uint32_t chrROMsize);
uint8_t vrc7get8(uint16_t addr);
void vrc7set8(uint16_t addr, uint8_t val);
uint8_t vrc7chrGet8(uint16_t addr);
void vrc7chrSet8(uint16_t addr, uint8_t val);
void vrc7cycle();

#endif
