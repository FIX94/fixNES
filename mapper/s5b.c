/*
 * Copyright (C) 2017 FIX94
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#include <stdio.h>
#include <inttypes.h>
#include <stdbool.h>
#include <string.h>
#include "../ppu.h"
#include "../mapper.h"
#include "../audio_s5b.h"

static uint8_t *s5B_prgROM;
static uint8_t *s5B_prgRAM;
static uint8_t *s5B_chrROM;
static uint32_t s5B_prgROMsize;
static uint32_t s5B_prgRAMsize;
static uint32_t s5B_chrROMsize;
static uint32_t s5B_prgROMand;
static uint32_t s5B_prgRAMand;
static uint32_t s5B_chrROMand;
static uint32_t s5B_lastPRGBank;
static uint32_t s5B_PRGBank[4];
static uint32_t s5B_CHRBank[8];
static uint16_t s5B_irqCtr;
static uint8_t s5B_CurReg;
static bool s5B_lowRAM;
static bool s5B_enableRAM;
static bool s5B_irqEnable;
static bool s5B_irqCtrEnable;
extern bool mapper_interrupt;

void s5Binit(uint8_t *prgROMin, uint32_t prgROMsizeIn, 
			uint8_t *prgRAMin, uint32_t prgRAMsizeIn,
			uint8_t *chrROMin, uint32_t chrROMsizeIn)
{
	s5B_prgROM = prgROMin;
	s5B_prgROMsize = prgROMsizeIn;
	s5B_prgROMand = mapperGetAndValue(prgROMsizeIn);
	s5B_prgRAM = prgRAMin;
	s5B_prgRAMsize = prgRAMsizeIn;
	s5B_prgRAMand = mapperGetAndValue(prgRAMsizeIn);
	s5B_lastPRGBank = (prgROMsizeIn - 0x2000);
	if(chrROMsizeIn > 0)
	{
		s5B_chrROM = chrROMin;
		s5B_chrROMsize = chrROMsizeIn;
		s5B_chrROMand = mapperGetAndValue(chrROMsizeIn);
	}
	else
		printf("S5B???\n");
	memset(s5B_PRGBank,0,4*sizeof(uint32_t));
	memset(s5B_CHRBank,0,8*sizeof(uint32_t));
	s5B_irqCtr = 0;
	s5B_CurReg = 0;
	s5B_lowRAM = false;
	s5B_enableRAM = false;
	s5B_irqEnable = false;
	s5B_irqCtrEnable = false;
	s5BAudioInit();
	printf("Sunsoft 5B Mapper inited\n");
}

uint8_t s5Bget8(uint16_t addr, uint8_t val)
{
	if(addr >= 0x6000 && addr < 0x8000)
	{
		if(!s5B_lowRAM)
			return s5B_prgROM[((s5B_PRGBank[0]<<13)+(addr&0x1FFF))&s5B_prgROMand];
		else if(s5B_lowRAM && s5B_enableRAM)
			return s5B_prgRAM[((s5B_PRGBank[0]<<13)+(addr&0x1FFF))&s5B_prgRAMand];
	}
	else if(addr >= 0x8000)
	{
		if(addr < 0xA000)
			return s5B_prgROM[((s5B_PRGBank[1]<<13)+(addr&0x1FFF))&s5B_prgROMand];
		else if(addr < 0xC000)
			return s5B_prgROM[((s5B_PRGBank[2]<<13)+(addr&0x1FFF))&s5B_prgROMand];
		else if(addr < 0xE000)
			return s5B_prgROM[((s5B_PRGBank[3]<<13)+(addr&0x1FFF))&s5B_prgROMand];
		return s5B_prgROM[(s5B_lastPRGBank+(addr&0x1FFF))&s5B_prgROMand];
	}
	return val;
}

void s5Bset8(uint16_t addr, uint8_t val)
{
	if(addr >= 0x6000 && addr < 0x8000)
	{
		//printf("s5Bset8 %04x %02x\n", addr, val);
		if(s5B_lowRAM && s5B_enableRAM)
			s5B_prgRAM[((s5B_PRGBank[0]<<13)+(addr&0x1FFF))&s5B_prgRAMand] = val;
	}
	else if(addr >= 0x8000)
	{
		if(addr < 0xA000)
			s5B_CurReg = (val&0xF);
		else if(addr < 0xC000)
		{
			switch(s5B_CurReg)
			{
				case 0x0:
					s5B_CHRBank[0] = val;
					break;
				case 0x1:
					s5B_CHRBank[1] = val;
					break;
				case 0x2:
					s5B_CHRBank[2] = val;
					break;
				case 0x3:
					s5B_CHRBank[3] = val;
					break;
				case 0x4:
					s5B_CHRBank[4] = val;
					break;
				case 0x5:
					s5B_CHRBank[5] = val;
					break;
				case 0x6:
					s5B_CHRBank[6] = val;
					break;
				case 0x7:
					s5B_CHRBank[7] = val;
					break;
				case 0x8:
					s5B_PRGBank[0] = val&0x3F;
					s5B_lowRAM = !!(val&0x40);
					s5B_enableRAM = !!(val&0x80);
					break;
				case 0x9:
					s5B_PRGBank[1] = val&0x3F;
					break;
				case 0xA:
					s5B_PRGBank[2] = val&0x3F;
					break;
				case 0xB:
					s5B_PRGBank[3] = val&0x3F;
					break;
				case 0xC:
					switch(val&3)
					{
						case 0:
							ppuSetNameTblVertical();
							break;
						case 1:
							ppuSetNameTblHorizontal();
							break;
						case 2:
							ppuSetNameTblSingleLower();
							break;
						case 3:
							ppuSetNameTblSingleUpper();
							break;
						default:
							break;
					}
					break;
				case 0xD:
					s5B_irqEnable = !!(val&1);
					s5B_irqCtrEnable = !!(val&0x80);
					mapper_interrupt = false;
					break;
				case 0xE:
					s5B_irqCtr = (s5B_irqCtr&~0xFF) | val;
					break;
				case 0xF:
					s5B_irqCtr = (s5B_irqCtr&0xFF) | (val<<8);
					break;
				default:
					break;
			}
		}
		else
			s5BAudioSet8(addr, val);
	}
}

uint8_t s5BchrGet8(uint16_t addr)
{
	//printf("%04x\n",addr);
	addr &= 0x1FFF;
	if(addr < 0x400)
		return s5B_chrROM[((s5B_CHRBank[0]<<10)+(addr&0x3FF))&s5B_chrROMand];
	else if(addr < 0x800)
		return s5B_chrROM[((s5B_CHRBank[1]<<10)+(addr&0x3FF))&s5B_chrROMand];
	else if(addr < 0xC00)
		return s5B_chrROM[((s5B_CHRBank[2]<<10)+(addr&0x3FF))&s5B_chrROMand];
	else if(addr < 0x1000)
		return s5B_chrROM[((s5B_CHRBank[3]<<10)+(addr&0x3FF))&s5B_chrROMand];
	else if(addr < 0x1400)
		return s5B_chrROM[((s5B_CHRBank[4]<<10)+(addr&0x3FF))&s5B_chrROMand];
	else if(addr < 0x1800)
		return s5B_chrROM[((s5B_CHRBank[5]<<10)+(addr&0x3FF))&s5B_chrROMand];
	else if(addr < 0x1C00)
		return s5B_chrROM[((s5B_CHRBank[6]<<10)+(addr&0x3FF))&s5B_chrROMand];
	return s5B_chrROM[((s5B_CHRBank[7]<<10)+(addr&0x3FF))&s5B_chrROMand];
}

void s5BchrSet8(uint16_t addr, uint8_t val)
{
	//printf("s5BchrSet8 %04x %02x\n", addr, val);
	(void)addr;
	(void)val;
}

void s5Bcycle()
{
	s5BAudioClockTimers();
	if(s5B_irqCtrEnable && !mapper_interrupt)
	{
		s5B_irqCtr--;
		if(s5B_irqCtr == 0xFFFF && s5B_irqEnable)
			mapper_interrupt = true;
	}
}
