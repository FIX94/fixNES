/*
 * Copyright (C) 2017 FIX94
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#ifndef _mapper_h_
#define _mapper_h_

typedef void (*initFuncT)(uint8_t*, uint32_t, uint8_t*, uint32_t, uint8_t*, uint32_t);
typedef uint8_t (*get8FuncT)(uint16_t);
typedef void (*set8FuncT)(uint16_t, uint8_t);
typedef uint8_t (*chrGet8FuncT)(uint16_t);
typedef void (*chrSet8FuncT)(uint16_t, uint8_t);
typedef uint8_t* (*getChrFuncT)();
typedef void (*cycleFuncT)();

typedef struct _mapperList_t {
	initFuncT initF;
	get8FuncT get8F;
	set8FuncT set8F;
	chrGet8FuncT chrGet8F;
	chrSet8FuncT chrSet8F;
	cycleFuncT cycleFuncF;
} mapperList_t;

bool mapperInit(uint8_t mapper, uint8_t *prgROM, uint32_t prgROMsize, uint8_t *prgRAM, uint32_t prgRAMsize, uint8_t *chrROM, uint32_t chrROMsize);

extern get8FuncT mapperGet8;
extern set8FuncT mapperSet8;
extern chrGet8FuncT mapperChrGet8;
extern chrSet8FuncT mapperChrSet8;
extern cycleFuncT mapperCycle;

#endif
