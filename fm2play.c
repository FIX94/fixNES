/*
 * Copyright (C) 2017 FIX94
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <malloc.h>
#include "fm2play.h"

static char *fm2playIn = NULL;
static char *fm2playCur = NULL;
static size_t fm2playSize = 0;
bool waitDMAcycles = true;

bool fm2playInit(char *fName, int fstart, bool wait)
{
	FILE *f = fopen(fName,"rb");
	if(!f)
	{
		printf("Huh\n");
		return false;
	}
	fseek(f,0,SEEK_END);
	fm2playSize = ftell(f);
	fm2playIn = (char*)malloc(fm2playSize+1);
	fm2playIn[fm2playSize] = '\0';
	rewind(f);
	fread(fm2playIn,1,fm2playSize,f);
	fclose(f);
	fm2playCur = fm2playIn;

	if(!wait)
		waitDMAcycles = false;

	int i;
	for(i = 0; i < fstart; i++)
		fm2playUpdate();

	return true;
}

//from input.c
extern uint8_t inValReads[8];

void fm2playUpdate()
{
	if(!fm2playIn) return;
	if(*fm2playCur == '\0')
	{
		free(fm2playIn);
		fm2playIn = NULL;
		return;
	}
	fm2playCur = strstr(fm2playCur,"|0|");
	if(!fm2playCur)
	{
		free(fm2playIn);
		fm2playIn = NULL;
		return;
	}
	fm2playCur+=3;
	//printf("%.8s\n",fm2playCur);
	int i;
	for(i = 0; i < 8; i++)
	{
		char cChar = *fm2playCur;
		fm2playCur++;
		if(cChar != 0x20 && cChar != 0x2E)
			inValReads[7-i] = 1;
		else
			inValReads[7-i] = 0;
	}
}

bool fm2playRunning()
{
	return (fm2playIn != NULL);
}

bool fm2playWaitDMAcycles()
{
	return waitDMAcycles;
}

