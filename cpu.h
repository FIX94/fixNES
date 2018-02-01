/*
 * Copyright (C) 2017 FIX94
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#ifndef _cpu_h_
#define _cpu_h_

void cpuInit();
void cpuSetupActionArr();
void cpuInitNSF(uint16_t addr, uint8_t newA, uint8_t newX);
void cpuStartPlayNSF();
void cpuEndPlayNSF();
void cpuSoftReset();
bool cpuCycle();
void cpuIncWaitCycles(uint32_t inc);
uint16_t cpuGetPc();

#endif
