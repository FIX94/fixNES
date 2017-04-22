/*
 * Copyright (C) 2017 FIX94
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#ifndef nsf_h_
#define nsf_h_

void nsfinit(uint8_t *nsfBIN, uint32_t nsfBINsize,
			uint8_t *prgRAM, uint32_t prgRAMsize);
uint8_t nsfget8(uint16_t addr, uint8_t val);
void nsfset8(uint16_t addr, uint8_t val);
uint8_t nsfchrGet8(uint16_t addr);
void nsfchrSet8(uint16_t addr, uint8_t val);
void nsfcycle();

#endif
