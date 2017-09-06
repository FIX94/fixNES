/*
 * Copyright (C) 2017 FIX94
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#ifndef m206h_
#define m206h_

void m206init(uint8_t *prgROM, uint32_t prgROMsize, 
			uint8_t *prgRAM, uint32_t prgRAMsize,
			uint8_t *chrROM, uint32_t chrROMsize);
uint8_t m112get8(uint16_t addr, uint8_t val);
uint8_t m206get8(uint16_t addr, uint8_t val);
void m95set8(uint16_t addr, uint8_t val);
void m112set8(uint16_t addr, uint8_t val);
void m154set8(uint16_t addr, uint8_t val);
void m206set8(uint16_t addr, uint8_t val);
uint8_t m76chrGet8(uint16_t addr);
uint8_t m88chrGet8(uint16_t addr);
uint8_t m206chrGet8(uint16_t addr);
void m206chrSet8(uint16_t addr, uint8_t val);

#endif
