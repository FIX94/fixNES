/*
 * Copyright (C) 2017 FIX94
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#ifndef p8c8_h_
#define p8c8_h_

void p8c8init(uint8_t *prgROM, uint32_t prgROMsize, 
			uint8_t *prgRAM, uint32_t prgRAMsize, 
			uint8_t *chrROM, uint32_t chrROMsize);
uint8_t p8c8get8(uint16_t addr, uint8_t val);
void m3_set8(uint16_t addr, uint8_t val);
void m87_set8(uint16_t addr, uint8_t val);
void m99_set8(uint16_t addr, uint8_t val);
void m101_set8(uint16_t addr, uint8_t val);
void m145_set8(uint16_t addr, uint8_t val);
void m149_set8(uint16_t addr, uint8_t val);
void m185_set8(uint16_t addr, uint8_t val);
uint8_t p8c8chrGet8(uint16_t addr);
uint8_t m185_chrGet8(uint16_t addr);
void p8c8chrSet8(uint16_t addr, uint8_t val);

#endif
