/*
 * Copyright (C) 2017 FIX94
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#ifndef _MAPPERLIST_H_
#define _MAPPERLIST_H_

typedef struct _mapperList_t {
	initFuncT initF;
	get8FuncT get8F;
	set8FuncT set8F;
	chrGet8FuncT chrGet8F;
	chrSet8FuncT chrSet8F;
	cycleFuncT cycleFuncF;
} mapperList_t;

extern mapperList_t mapperList[256];

#endif
