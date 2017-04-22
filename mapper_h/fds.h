/*
 * Copyright (C) 2017 FIX94
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#ifndef fds_h_
#define fds_h_

void fdsinit(uint8_t *fdsBIOS, uint32_t fdsBIOSsize, 
			uint8_t *fdsFile, bool fdsSideB,
			uint8_t *prgRAMin, uint32_t prgRAMsizeIn);
uint8_t fdsget8(uint16_t addr, uint8_t val);
void fdsset8(uint16_t addr, uint8_t val);
uint8_t fdschrGet8(uint16_t addr);
void fdschrSet8(uint16_t addr, uint8_t val);
void fdscycle();

#endif
