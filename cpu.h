/*
 * Copyright (C) 2017 FIX94
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#ifndef _cpu_c_
#define _cpu_h_

void cpuInit();
void cpuInitNSF(uint16_t addr, uint8_t newA, uint8_t newX);
uint16_t cpuPlayNSF(uint16_t addr);
bool cpuCycle();
void cpuIncWaitCycles(uint32_t inc);
uint16_t cpuGetPc();

#endif
