/*
 * Copyright (C) 2017 FIX94
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#ifndef m4_h_
#define m4_h_

void m4init(uint8_t *prgROM, uint32_t prgROMsize, 
			uint8_t *prgRAM, uint32_t prgRAMsize,
			uint8_t *chrROM, uint32_t chrROMsize);
void m12init(uint8_t *prgROM, uint32_t prgROMsize, 
			uint8_t *prgRAM, uint32_t prgRAMsize,
			uint8_t *chrROM, uint32_t chrROMsize);
void m118init(uint8_t *prgROM, uint32_t prgROMsize, 
			uint8_t *prgRAM, uint32_t prgRAMsize,
			uint8_t *chrROM, uint32_t chrROMsize);
void m119init(uint8_t *prgROM, uint32_t prgROMsize, 
			uint8_t *prgRAM, uint32_t prgRAMsize,
			uint8_t *chrROM, uint32_t chrROMsize);
uint8_t m4get8(uint16_t addr, uint8_t val);
void m4set8(uint16_t addr, uint8_t val);
void m12set8(uint16_t addr, uint8_t val);
void m118set8(uint16_t addr, uint8_t val);
uint8_t m4chrGet8(uint16_t addr);
uint8_t m12chrGet8(uint16_t addr);
uint8_t m119chrGet8(uint16_t addr);
void m4chrSet8(uint16_t addr, uint8_t val);
void m119chrSet8(uint16_t addr, uint8_t val);
void m4cycle();

#endif
