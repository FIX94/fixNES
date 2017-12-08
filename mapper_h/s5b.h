/*
 * Copyright (C) 2017 FIX94
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#ifndef s5b_h_
#define s5b_h_

void s5Binit(uint8_t *prgROM, uint32_t prgROMsize, 
			uint8_t *prgRAM, uint32_t prgRAMsize,
			uint8_t *chrROM, uint32_t chrROMsize);
uint8_t s5Bget8(uint16_t addr, uint8_t val);
void s5Bset8(uint16_t addr, uint8_t val);
uint8_t s5BchrGet8(uint16_t addr);
void s5BchrSet8(uint16_t addr, uint8_t val);
void s5Bcycle();

#endif
