/*
 * Copyright (C) 2017 FIX94
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#include <stdio.h>
#include <stdbool.h>
#include <inttypes.h>
#include "mapper/m0.h"
#include "mapper/m1.h"
#include "mapper/m2.h"
#include "mapper/m3.h"
#include "mapper/m4.h"
#include "mapper/m7.h"
#include "mapper/m9.h"
#include "mapper/m10.h"
#include "mapper.h"

get8FuncT mapperGet8;
set8FuncT mapperSet8;
getChrFuncT mapperGetChr;
chrGet8FuncT mapperChrGet8;
chrSet8FuncT mapperChrSet8;
cycleFuncT mapperCycle;

mapperList_t mapperList[11] = {
	{ m0init,  m0get8,  m0set8,  m0chrGet8,  m0chrSet8,  NULL },
	{ m1init,  m1get8,  m1set8,  m1chrGet8,  m1chrSet8,  NULL },
	{ m2init,  m2get8,  m2set8,  m2chrGet8,  m2chrSet8,  NULL },
	{ m3init,  m3get8,  m3set8,  m3chrGet8,  m3chrSet8,  NULL },
	{ m4init,  m4get8,  m4set8,  m4chrGet8,  m4chrSet8,  m4cycle },
	{ NULL,    NULL,    NULL,    NULL,       NULL,       NULL },
	{ NULL,    NULL,    NULL,    NULL,       NULL,       NULL },
	{ m7init,  m7get8,  m7set8,  m7chrGet8,  m7chrSet8,  NULL },
	{ NULL,    NULL,    NULL,    NULL,       NULL,       NULL },
	{ m9init,  m9get8,  m9set8,  m9chrGet8,  m9chrSet8,  NULL },
	{ m10init, m10get8, m10set8, m10chrGet8, m10chrSet8, NULL },
};

bool mapperInit(uint8_t mapper, uint8_t *prgROM, uint32_t prgROMsize, uint8_t *prgRAM, uint32_t prgRAMsize, uint8_t *chrROM, uint32_t chrROMsize)
{
	if(mapper > 10 || mapperList[mapper].initF == NULL)
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
