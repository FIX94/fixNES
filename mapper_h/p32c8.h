/*
 * Copyright (C) 2017 FIX94
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#ifndef p32c8_h_
#define p32c8_h_

void p32c8init(uint8_t *prgROM, uint32_t prgROMsize, 
			uint8_t *prgRAM, uint32_t prgRAMsize,
			uint8_t *chrROM, uint32_t chrROMsize);
uint8_t p32c8get8(uint16_t addr, uint8_t val);
uint8_t m36_p32c8get8(uint16_t addr, uint8_t val);
void m0_set8(uint16_t addr, uint8_t val);
void m11_set8(uint16_t addr, uint8_t val);
void m36_set8(uint16_t addr, uint8_t val);
void m38_set8(uint16_t addr, uint8_t val);
void m46_set8(uint16_t addr, uint8_t val);
void m66_set8(uint16_t addr, uint8_t val);
void m79_set8(uint16_t addr, uint8_t val);
void m113_set8(uint16_t addr, uint8_t val);
void m133_set8(uint16_t addr, uint8_t val);
void m140_set8(uint16_t addr, uint8_t val);
void m144_set8(uint16_t addr, uint8_t val);
void m147_set8(uint16_t addr, uint8_t val);
void m148_set8(uint16_t addr, uint8_t val);
void m240_set8(uint16_t addr, uint8_t val);
void m242_set8(uint16_t addr, uint8_t val);
uint8_t p32c8chrGet8(uint16_t addr);
void p32c8chrSet8(uint16_t addr, uint8_t val);

#endif
