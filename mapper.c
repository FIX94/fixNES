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

get8FuncT mapperGet8;
set8FuncT mapperSet8;
getChrFuncT mapperGetChr;
chrGet8FuncT mapperChrGet8;
chrSet8FuncT mapperChrSet8;
cycleFuncT mapperCycle;

bool mapperInit(uint8_t mapper, uint8_t *prgROM, uint32_t prgROMsize, uint8_t *prgRAM, uint32_t prgRAMsize, uint8_t *chrROM, uint32_t chrROMsize)
{
	if(mapperList[mapper].initF == NULL)
	{
		printf("Unsupported Mapper!\n");
		return false;
	}
	mapperList[mapper].initF(prgROM, prgROMsize, prgRAM, prgRAMsize, chrROM, chrROMsize);
	mapperGet8 = mapperList[mapper].get8F;
	mapperSet8 = mapperList[mapper].set8F;
	mapperChrGet8 = mapperList[mapper].chrGet8F;
	mapperChrSet8 = mapperList[mapper].chrSet8F;
	mapperCycle = mapperList[mapper].cycleFuncF;
	return true;
}
