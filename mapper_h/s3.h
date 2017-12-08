/*
 * Copyright (C) 2017 FIX94
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#ifndef s3_h_
#define s3_h_

void s3init(uint8_t *prgROM, uint32_t prgROMsize, 
			uint8_t *prgRAM, uint32_t prgRAMsize,
			uint8_t *chrROM, uint32_t chrROMsize);
uint8_t s3get8(uint16_t addr, uint8_t val);
void s3set8(uint16_t addr, uint8_t val);
uint8_t s3chrGet8(uint16_t addr);
void s3chrSet8(uint16_t addr, uint8_t val);
void s3cycle();

#endif
