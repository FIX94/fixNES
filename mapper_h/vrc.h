/*
 * Copyright (C) 2017 FIX94
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#ifndef vrc_h_
#define vrc_h_

void vrcinit(uint8_t *prgROM, uint32_t prgROMsize, 
			uint8_t *prgRAM, uint32_t prgRAMsize,
			uint8_t *chrROM, uint32_t chrROMsize);
uint8_t vrcget8(uint16_t addr, uint8_t val);
void m21_set8(uint16_t addr, uint8_t val);
void m22_set8(uint16_t addr, uint8_t val);
void m23_set8(uint16_t addr, uint8_t val);
void m25_set8(uint16_t addr, uint8_t val);
uint8_t vrcchrGet8(uint16_t addr);
uint8_t m22_chrGet8(uint16_t addr);
void vrcchrSet8(uint16_t addr, uint8_t val);
void vrccycle();

#endif
