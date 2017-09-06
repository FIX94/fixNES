/*
 * Copyright (C) 2017 FIX94
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#ifndef namco_h_
#define namco_h_

void namco_init(uint8_t *prgROM, uint32_t prgROMsize, 
			uint8_t *prgRAM, uint32_t prgRAMsize,
			uint8_t *chrROM, uint32_t chrROMsize);
uint8_t namco_get8(uint16_t addr, uint8_t val);
void namco_set8(uint16_t addr, uint8_t val);
uint8_t namco_chrGet8(uint16_t addr);
void namco_chrSet8(uint16_t addr, uint8_t val);
uint8_t namco_vramGet8(uint16_t addr);
void namco_vramSet8(uint16_t addr, uint8_t val);
void namco_cycle();

#endif
