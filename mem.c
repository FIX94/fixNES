/*
 * Copyright (C) 2017 FIX94
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#include <stdio.h>
#include <stdbool.h>
#include <inttypes.h>
#include "mapper.h"
#include "ppu.h"
#include "cpu.h"
#include "input.h"
#include "fm2play.h"
#include "apu.h"

static uint8_t Main_Mem[0x800];
static uint8_t memLastVal;

void memInit()
{
	memset(Main_Mem,0,0x800);
	memLastVal = 0;
}

uint8_t memGet8(uint16_t addr)
{
	uint8_t val = memLastVal;
	//printf("memGet8 %04x\n", addr);
	if(addr >= 0x4020)
		val = mapperGet8(addr);
	else if(addr >= 0x4000)
	{
		if(addr == 0x4015)
			val = apuGet8(0x15);
		else if(addr == 0x4016)
		{
			val &= ~0x1F; //player 1
			val |= inputGet();
		}
		else if(addr == 0x4017)
			val &= ~0x1F; //player 2
	}
	else if(addr >= 0x2000)
		val = ppuGet8(addr&7);
	else
		val = Main_Mem[addr&0x7FF];

	memLastVal = val;
	return val;
}
extern uint32_t cpu_oam_dma;
void memSet8(uint16_t addr, uint8_t val)
{
	//printf("memSet8 %04x %02x\n", addr, val);
	if(addr >= 0x4000)
	{
		//everything starting from 0x4000 has to
		//go to mapper, even if used later on
		mapperSet8(addr, val);
		//all other devices
		if(addr == 0x4014)
		{
			uint16_t dmaAddr = (val<<8);
			//printf("ppuLoadOAM %04x\n", dmaAddr);
			if(!fm2playRunning() || (fm2playRunning() && fm2playWaitDMAcycles()))
				cpu_oam_dma = 514;
			uint16_t i;
			if(dmaAddr < 0x2000)
			{
				for(i = 0; i < 0x100; i++)
					ppuLoadOAM(Main_Mem[(dmaAddr+i)&0x7FF]);
			}
			else if(dmaAddr >= 0x6000)
			{
				for(i = 0; i < 0x100; i++)
					ppuLoadOAM(mapperGet8(dmaAddr+i));
			}
			else
				printf("WARNING: Invalid ppuLoadOAM at %04x!\n", dmaAddr);
		}
		else if(addr == 0x4016)
			inputSet(val);
		else if(addr < 0x4018)
			apuSet8(addr&0x1F, val);
	}
	else if(addr >= 0x2000)
		ppuSet8(addr&7, val);
	else
		Main_Mem[addr&0x7FF] = val;
	memLastVal = val;
}

#define DEBUG_MEM_DUMP 0

void memDumpMainMem()
{
	#if DEBUG_MEM_DUMP
	FILE *f = fopen("MainMem.bin","wb");
	if(f)
	{
		fwrite(Main_Mem,1,0x800,f);
		fclose(f);
	}
	ppuDumpMem();
	#endif
}

