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
#include "../cpu.h"
#include "../ppu.h"
#include "../mapper.h"

static uint8_t *s3_prgROM;
static uint8_t *s3_prgRAM;
static uint8_t *s3_chrROM;
static uint32_t s3_prgROMsize;
static uint32_t s3_prgRAMsize;
static uint32_t s3_chrROMsize;
static uint32_t s3_prgROMand;
static uint32_t s3_chrROMand;
static uint32_t s3_curPRGBank;
static uint32_t s3_lastPRGBank;
static uint32_t s3_CHRBank[4];
static uint16_t s3_irqCtr;
static bool s3_TmpWrite;
static bool s3_enableRAM;
static bool s3_irqCtrEnable;
extern uint8_t interrupt;

void s3init(uint8_t *prgROMin, uint32_t prgROMsizeIn, 
			uint8_t *prgRAMin, uint32_t prgRAMsizeIn,
			uint8_t *chrROMin, uint32_t chrROMsizeIn)
{
	s3_prgROM = prgROMin;
	s3_prgROMsize = prgROMsizeIn;
	s3_prgROMand = mapperGetAndValue(prgROMsizeIn);
	s3_prgRAM = prgRAMin;
	s3_prgRAMsize = prgRAMsizeIn;
	s3_curPRGBank = 0;
	s3_lastPRGBank = (prgROMsizeIn - 0x4000);
	if(chrROMsizeIn > 0)
	{
		s3_chrROM = chrROMin;
		s3_chrROMsize = chrROMsizeIn;
		s3_chrROMand = mapperGetAndValue(chrROMsizeIn);
	}
	else
		printf("s3???\n");
	memset(s3_CHRBank,0,4*sizeof(uint32_t));
	s3_irqCtr = 0;
	s3_TmpWrite = false;
	s3_enableRAM = false;
	s3_irqCtrEnable = false;
	printf("Sunsoft 3 Mapper inited\n");
}

uint8_t s3get8(uint16_t addr, uint8_t val)
{
	if(addr >= 0x8000)
	{
		if(addr < 0xC000)
			return s3_prgROM[((s3_curPRGBank<<14)+(addr&0x3FFF))&s3_prgROMand];
		return s3_prgROM[(s3_lastPRGBank+(addr&0x3FFF))&s3_prgROMand];
	}
	return val;
}

void s3set8(uint16_t addr, uint8_t val)
{
	if(addr >= 0x8000)
	{
		if(addr < 0x9000)
			s3_CHRBank[0] = val;
		else if(addr < 0xA000)
			s3_CHRBank[1] = val;
		else if(addr < 0xB000)
			s3_CHRBank[2] = val;
		else if(addr < 0xC000)
			s3_CHRBank[3] = val;
		else if(addr < 0xD000)
		{
			if(!s3_TmpWrite)
			{
				s3_TmpWrite = true;
				s3_irqCtr &= 0xFF;
				s3_irqCtr |= (val<<8);
			}
			else
			{
				s3_TmpWrite = false;
				s3_irqCtr &= ~0xFF;
				s3_irqCtr |= val;
			}
		}
		else if(addr < 0xE000)
		{
			s3_TmpWrite = false;
			interrupt &= ~MAPPER_IRQ;
			s3_irqCtrEnable = !!(val&0x10);
		}
		else if(addr < 0xF000)
		{
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
		}
		else
			s3_curPRGBank = val;
	}
}

uint8_t s3chrGet8(uint16_t addr)
{
	//printf("%04x\n",addr);
	addr &= 0x1FFF;
	if(addr < 0x800)
		return s3_chrROM[((s3_CHRBank[0]<<11)+(addr&0x7FF))&s3_chrROMand];
	else if(addr < 0x1000)
		return s3_chrROM[((s3_CHRBank[1]<<11)+(addr&0x7FF))&s3_chrROMand];
	else if(addr < 0x1800)
		return s3_chrROM[((s3_CHRBank[2]<<11)+(addr&0x7FF))&s3_chrROMand];
	return s3_chrROM[((s3_CHRBank[3]<<11)+(addr&0x7FF))&s3_chrROMand];
}

void s3chrSet8(uint16_t addr, uint8_t val)
{
	//printf("s3chrSet8 %04x %02x\n", addr, val);
	(void)addr;
	(void)val;
}

void s3cycle()
{
	if(s3_irqCtrEnable)
	{
		s3_irqCtr--;
		if(s3_irqCtr == 0xFFFF)
		{
			s3_irqCtrEnable = false;
			interrupt |= MAPPER_IRQ;
		}
	}
}
