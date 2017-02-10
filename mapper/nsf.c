/*
 * Copyright (C) 2017 FIX94
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#include <stdio.h>
#include <stdbool.h>
#include <inttypes.h>
#include "../ppu.h"
#include "../cpu.h"
#include "../input.h"
#include "../mem.h"

static uint8_t *nsf_prgROM;
static uint8_t *nsf_prgRAM;
static uint32_t nsf_prgROMsize;
static uint32_t nsf_prgRAMsize;
static uint32_t nsf_PRGBank[8];
static uint16_t nsf_loadAddr;
static uint16_t nsf_initAddr;
static uint16_t nsf_playAddr;
static uint8_t nsf_trackTotal;
static uint8_t nsf_curTrack;
static bool nsf_bankEnable;
static bool nsf_playing;
static uint8_t nsf_chrRAM[0x2000];
extern bool nesPAL;
static uint8_t nsf_prevValReads[8];

void nsfinit(uint8_t *nsfBIN, uint32_t nsfBINsize, uint8_t *prgRAMin, uint32_t prgRAMsizeIn)
{
	nsf_prgROM = nsfBIN+0x80;
	nsf_prgROMsize = nsfBINsize-0x80;
	nsf_prgRAM = prgRAMin;
	nsf_prgRAMsize = prgRAMsizeIn;
	nsf_loadAddr = (*(uint16_t*)(nsfBIN+0x8))&0x7FFF;
	nsf_initAddr = *(uint16_t*)(nsfBIN+0xA);
	nsf_playAddr = *(uint16_t*)(nsfBIN+0xC);
	nesPAL = ((nsfBIN[0x7A]&1) != 0);
	nsf_bankEnable = false;
	int i;
	for(i = 0; i < 8; i++)
	{
		nsf_PRGBank[i] = (nsfBIN[0x70+i]<<12);
		if(nsf_PRGBank[i] != 0)
			nsf_bankEnable = true;
	}
	nsf_curTrack = 1;
	nsf_trackTotal = nsfBIN[6];
	memset(nsf_prevValReads, 0, 8);
	printf("NSF Player inited in %s Mode %s banking\n", nesPAL ? "PAL" : "NTSC", nsf_bankEnable ? "with" : "without");
	if(nsfBIN[0xE] != 0) printf("Playing back %.32s\n", nsfBIN+0xE);
	printf("Track %i/%i         ", nsf_curTrack, nsf_trackTotal);
	nsf_playing = true;
	memset(nsf_prgRAM, 0, nsf_prgRAMsize);
	cpuInitNSF(nsf_initAddr, nsf_curTrack-1, nesPAL ? 1 : 0);
}

uint8_t nsfget8(uint16_t addr)
{
	if(addr < 0x6000)
	{
		if(addr == 0x4567)
		{
			nsf_playing = false;
			return 0xEA; //NOP
		}
		return 0;
	}
	else if(addr < 0x8000)
		return nsf_prgRAM[addr&0x1FFF];
	else
	{
		uint32_t romAddr;
		if(!nsf_bankEnable)
			romAddr = (addr&0x7FFF);
		else if(addr < 0x9000)
			romAddr = nsf_PRGBank[0]+(addr&0xFFF);
		else if(addr < 0xA000)
			romAddr = nsf_PRGBank[1]+(addr&0xFFF);
		else if(addr < 0xB000)
			romAddr = nsf_PRGBank[2]+(addr&0xFFF);
		else if(addr < 0xC000)
			romAddr = nsf_PRGBank[3]+(addr&0xFFF);
		else if(addr < 0xD000)
			romAddr = nsf_PRGBank[4]+(addr&0xFFF);
		else if(addr < 0xE000)
			romAddr = nsf_PRGBank[5]+(addr&0xFFF);
		else if(addr < 0xF000)
			romAddr = nsf_PRGBank[6]+(addr&0xFFF);
		else
			romAddr = nsf_PRGBank[7]+(addr&0xFFF);
		if(romAddr >= nsf_loadAddr && (romAddr-nsf_loadAddr) < nsf_prgROMsize)
			return nsf_prgROM[romAddr-nsf_loadAddr];
		else
			return 0;
	}
}

void nsfset8(uint16_t addr, uint8_t val)
{
	//printf("nsfset8 %04x %02x\n", addr, val);
	if(addr >= 0x5FF8 && addr < 0x6000)
	{
		if(addr == 0x5FF8)
			nsf_PRGBank[0] = val<<12;
		else if(addr == 0x5FF9)
			nsf_PRGBank[1] = val<<12;
		else if(addr == 0x5FFA)
			nsf_PRGBank[2] = val<<12;
		else if(addr == 0x5FFB)
			nsf_PRGBank[3] = val<<12;
		else if(addr == 0x5FFC)
			nsf_PRGBank[4] = val<<12;
		else if(addr == 0x5FFD)
			nsf_PRGBank[5] = val<<12;
		else if(addr == 0x5FFE)
			nsf_PRGBank[6] = val<<12;
		else if(addr == 0x5FFF)
			nsf_PRGBank[7] = val<<12;
	}
	else if(addr >= 0x6000 && addr < 0x8000)
		nsf_prgRAM[addr&0x1FFF] = val;
}

uint8_t nsfchrGet8(uint16_t addr)
{
	return nsf_chrRAM[addr&0x1FFF];
}

void nsfchrSet8(uint16_t addr, uint8_t val)
{
	nsf_chrRAM[addr&0x1FFF] = val;
}

extern uint8_t inValReads[8];

void nsfcycle()
{
	if(inValReads[BUTTON_RIGHT] && !nsf_prevValReads[BUTTON_RIGHT])
	{
		nsf_prevValReads[BUTTON_RIGHT] = inValReads[BUTTON_RIGHT];
		nsf_curTrack++;
		if(nsf_curTrack > nsf_trackTotal)
			nsf_curTrack = 1;
		printf("\rTrack %i/%i         ", nsf_curTrack, nsf_trackTotal);
		nsf_playing = true;
		memset(nsf_prgRAM, 0, nsf_prgRAMsize);
		cpuInitNSF(nsf_initAddr, nsf_curTrack-1, nesPAL ? 1 : 0);
	}
	else if(!inValReads[BUTTON_RIGHT])
		nsf_prevValReads[BUTTON_RIGHT] = 0;
	
	if(inValReads[BUTTON_LEFT] && !nsf_prevValReads[BUTTON_LEFT])
	{
		nsf_prevValReads[BUTTON_LEFT] = inValReads[BUTTON_LEFT];
		nsf_curTrack--;
		if(nsf_curTrack < 1)
			nsf_curTrack = nsf_trackTotal;
		printf("\rTrack %i/%i         ", nsf_curTrack, nsf_trackTotal);
		nsf_playing = true;
		memset(nsf_prgRAM, 0, nsf_prgRAMsize);
		cpuInitNSF(nsf_initAddr, nsf_curTrack-1, nesPAL ? 1 : 0);
	}
	else if(!inValReads[BUTTON_LEFT])
		nsf_prevValReads[BUTTON_LEFT] = 0;

	if(nsf_playing)
		return;

	if(ppuDrawDone())
	{
		cpuPlayNSF(nsf_playAddr);
		nsf_playing = true;
	}
	else //cpu wait loop
		cpuIncWaitCycles(1);
}
