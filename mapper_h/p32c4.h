/*
 * Copyright (C) 2017 FIX94
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#ifndef p32c4_h_
#define p32c4_h_

void p32c4init(uint8_t *prgROM, uint32_t prgROMsize, 
			uint8_t *prgRAM, uint32_t prgRAMsize,
			uint8_t *chrROM, uint32_t chrROMsize);
uint8_t p32c4get8(uint16_t addr);
void p32c4set8(uint16_t addr, uint8_t val);
uint8_t p32c4chrGet8(uint16_t addr);
void p32c4chrSet8(uint16_t addr, uint8_t val);

#endif
