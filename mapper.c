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
#include "mapperList.h"
#include "mapper_h/nsf.h"
#include "mapper_h/fds.h"
#include "ppu.h"

get8FuncT mapperGet8;
set8FuncT mapperSet8;
chrGet8FuncT mapperChrGet8;
chrSet8FuncT mapperChrSet8;
vramGet8FuncT mapperVramGet8;
vramSet8FuncT mapperVramSet8;
cycleFuncT mapperCycle;
uint8_t mapperChrMode;

bool mapperInit(uint8_t mapper, uint8_t *prgROM, uint32_t prgROMsize, uint8_t *prgRAM, uint32_t prgRAMsize, uint8_t *chrROM, uint32_t chrROMsize)
{
	if(mapperList[mapper].initF == NULL)
	{
		printf("Unsupported Mapper %i!\n", mapper);
		return false;
	}
	mapperList[mapper].initF(prgROM, prgROMsize, prgRAM, prgRAMsize, chrROM, chrROMsize);
	mapperGet8 = mapperList[mapper].get8F;
	mapperSet8 = mapperList[mapper].set8F;
	mapperChrGet8 = mapperList[mapper].chrGet8F;
	mapperChrSet8 = mapperList[mapper].chrSet8F;
	mapperCycle = mapperList[mapper].cycleFuncF;
	//some mappers re-route VRAM
	if(mapperList[mapper].vramGet8F == NULL)
		mapperVramGet8 = ppuVRAMGet8;
	else
		mapperVramGet8 = mapperList[mapper].vramGet8F;
	if(mapperList[mapper].vramSet8F == NULL)
		mapperVramSet8 = ppuVRAMSet8;
	else
		mapperVramSet8 = mapperList[mapper].vramSet8F;
	mapperChrMode = 0;
	return true;
}

bool mapperInitNSF(uint8_t *nsfBIN, uint32_t nsfBINsize, uint8_t *prgRAM, uint32_t prgRAMsize)
{
	nsfinit(nsfBIN, nsfBINsize, prgRAM, prgRAMsize);
	mapperGet8 = nsfget8;
	mapperSet8 = nsfset8;
	mapperChrGet8 = nsfchrGet8;
	mapperChrSet8 = nsfchrSet8;
	mapperCycle = nsfcycle;
	mapperVramGet8 = ppuVRAMGet8;
	mapperVramSet8 = ppuVRAMSet8;
	mapperChrMode = 0;
	return true;
}

static uint8_t fdsBIOS[0x2000];
bool mapperInitFDS(uint8_t *fdsFile, bool fdsSideB, uint8_t *prgRAM, uint32_t prgRAMsize)
{
	if(fdsFile == NULL)
	{
		printf("No FDS loaded!\n");
		return false;
	}
	FILE *f = fopen("disksys.rom","rb");
	if(f == NULL)
	{
		printf("disksys.rom not found!\n");
		return false;
	}
	fseek(f,0,SEEK_END);
	size_t fsize = ftell(f);
	rewind(f);
	if(fsize != 0x2000)
	{
		printf("disksys.rom has a wrong size, is %i bytes, should be 8192 bytes!\n", fsize);
		fclose(f);
		return false;
	}
	fread(fdsBIOS, 1, 0x2000, f);
	fclose(f);
	fdsinit(fdsBIOS, fsize, fdsFile, fdsSideB, prgRAM, prgRAMsize);
	mapperGet8 = fdsget8;
	mapperSet8 = fdsset8;
	mapperChrGet8 = fdschrGet8;
	mapperChrSet8 = fdschrSet8;
	mapperVramGet8 = ppuVRAMGet8;
	mapperVramSet8 = ppuVRAMSet8;
	mapperCycle = fdscycle;
	mapperChrMode = 0;
	return true;
}

uint32_t mapperGetAndValue(uint32_t v)
{
	v--;
	v |= v >> 1;
	v |= v >> 2;
	v |= v >> 4;
	v |= v >> 8;
	v |= v >> 16;
	return v;
}
