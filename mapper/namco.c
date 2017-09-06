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
#include "../audio_n163.h"

enum {
	T_N163 = 0,
	T_N175,
	T_N340,
	T_UNK,
};

static uint8_t *namco_prgROM;
static uint8_t *namco_prgRAM;
static uint8_t *namco_chrROM;
static uint32_t namco_prgROMsize;
static uint32_t namco_prgRAMsize;
static uint32_t namco_chrROMsize;
static uint32_t namco_curPRGBank0;
static uint32_t namco_curPRGBank1;
static uint32_t namco_curPRGBank2;
static uint32_t namco_lastPRGBank;
static uint32_t namco_CHRBank[8];
static uint8_t namco_VRAM[0x800];
static bool namco_CHRBankIsNT0;
static bool namco_CHRBankIsNT1;
static uint32_t namco_NTAddr[4];
static uint16_t namco_irqCtr;
static bool namco_irqEnable;
extern bool mapper_interrupt;
static uint8_t namco_type;
//used externally
uint32_t namco_prgROMand;
uint32_t namco_chrROMand;

void namco_init(uint8_t *prgROMin, uint32_t prgROMsizeIn, 
			uint8_t *prgRAMin, uint32_t prgRAMsizeIn,
			uint8_t *chrROMin, uint32_t chrROMsizeIn)
{
	namco_prgROM = prgROMin;
	namco_prgROMsize = prgROMsizeIn;
	namco_prgRAM = prgRAMin;
	namco_prgRAMsize = prgRAMsizeIn;
	namco_prgROMand = mapperGetAndValue(prgROMsizeIn);
	namco_curPRGBank0 = 0;
	namco_curPRGBank1 = 0x2000;
	namco_curPRGBank2 = 0x4000;
	namco_lastPRGBank = (prgROMsizeIn - 0x2000);
	namco_chrROM = chrROMin;
	namco_chrROMsize = chrROMsizeIn;
	namco_chrROMand = mapperGetAndValue(chrROMsizeIn);
	memset(namco_CHRBank,0,8*sizeof(uint32_t));
	memset(namco_VRAM,0,0x800);
	namco_CHRBankIsNT0 = false;
	namco_CHRBankIsNT1 = false;
	memset(namco_NTAddr,0,4*sizeof(uint32_t));
	namco_irqCtr = 0;
	namco_irqEnable = false;
	namco_type = T_UNK;
	ppuBackUpTbl();
	printf("Namco Mapper inited\n");
}

static void namcoSetN163()
{
	if(namco_type != T_N163)
	{
		printf("Should be Namco N163\n");
		namco_type = T_N163;
		n163AudioInit();
		ppuSetNameTbl4Screen();
	}
}

static void namcoGuessN175()
{
	if((namco_type == T_N340) || (namco_type == T_UNK))
	{
		printf("Guessing Namco N175\n");
		namco_type = T_N175;
		ppuRestoreTbl();
	}
}

static void namcoGuessN340()
{
	if(namco_type == T_UNK)
	{
		printf("Guessing Namco N340\n");
		namco_type = T_N340;
	}
}

uint8_t namco_get8(uint16_t addr, uint8_t val)
{
	if(addr >= 0x4800 && addr < 0x6000)
	{
		namcoSetN163();
		if(addr < 0x5000)
			return n163AudioGet8(addr, val);
		else if(addr < 0x5800)
			return namco_irqCtr&0xFF;
		else //if addr < 0x6000
			return (((namco_irqCtr>>8)&0x7F)|(namco_irqEnable<<7));
	}
	else if(addr >= 0x6000 && addr < 0x8000)
	{
		namcoGuessN175();
		return namco_prgRAM[addr&0x1FFF];
	}
	else if(addr >= 0x8000)
	{
		if(addr < 0xA000)
			return namco_prgROM[((namco_curPRGBank0<<13)|(addr&0x1FFF))&namco_prgROMand];
		else if(addr < 0xC000)
			return namco_prgROM[((namco_curPRGBank1<<13)|(addr&0x1FFF))&namco_prgROMand];
		else if(addr < 0xE000)
			return namco_prgROM[((namco_curPRGBank2<<13)|(addr&0x1FFF))&namco_prgROMand];
		return namco_prgROM[(namco_lastPRGBank+(addr&0x1FFF))&namco_prgROMand];
	}
	return val;
}

void namco_set8(uint16_t addr, uint8_t val)
{
	//printf("namco_set8 %04x %02x\n", addr, val);
	if(addr >= 0x4800 && addr < 0x6000)
	{
		namcoSetN163();
		if(addr < 0x5000)
			n163AudioSet8(addr, val);
		else if(addr < 0x5800)
		{
			namco_irqCtr &= ~0xFF;
			namco_irqCtr |= val;
			mapper_interrupt = false;
		}
		else //if addr < 0x6000
		{
			namco_irqCtr &= 0xFF;
			namco_irqCtr |= (val&0x7F)<<8;
			namco_irqEnable = (val&0x80)!=0;
			mapper_interrupt = false;
		}
	}
	else if(addr >= 0x6000 && addr < 0x8000)
	{
		namcoGuessN175();
		namco_prgRAM[addr&0x1FFF] = val;
	}
	else if(addr >= 0x8000)
	{
		if(addr < 0xE000)
		{
			if(addr >= 0xC000)
			{
				if(addr >= 0xC800)
					namcoSetN163();
				else if(addr < 0xC800)
					namcoGuessN175();
				//set these always just in case
				if(addr < 0xC800)
					namco_NTAddr[0] = val;
				else if(addr < 0xD000)
					namco_NTAddr[1] = val;
				else if(addr < 0xD800)
					namco_NTAddr[2] = val;
				else //if addr < 0xE000
					namco_NTAddr[3] = val;
			}
			else
				namco_CHRBank[(addr>>11)&7] = val;
		}
		else if(addr < 0xE800)
		{
			namco_curPRGBank0 = val&0x3F;
			if((val&0xC0) != 0)
				namcoGuessN340();
			if(namco_type == T_N340)
			{
				switch((val>>6)&3)
				{
					case 0:
						ppuSetNameTblSingleLower();
						break;
					case 1:
						ppuSetNameTblVertical();
						break;
					case 2:
						ppuSetNameTblSingleUpper();
						break;
					case 3:
						ppuSetNameTblHorizontal();
						break;
				}
			}
		}
		else if(addr < 0xF000)
		{
			namco_curPRGBank1 = val&0x3F;
			namco_CHRBankIsNT0 = (val&0x40) == 0;
			namco_CHRBankIsNT1 = (val&0x80) == 0;
		}
		else if(addr < 0xF800)
			namco_curPRGBank2 = val&0x3F;
		else if(addr >= 0xF800)
		{
			//cant use this set, dream master writes
			//0x40 into this reg on bootup...
			//namcoSetN163();
			n163AudioSet8(addr, val);
		}
	}
}

uint8_t namco_chrGet8(uint16_t addr)
{
	//printf("%04x\n",addr);
	addr &= 0x1FFF;
	if(namco_type == T_N163)
	{
		if(addr < 0x1000 && namco_CHRBankIsNT0)
		{
			if(addr < 0x400)
			{
				if(namco_CHRBank[0] >= 0xE0)
					return namco_VRAM[((namco_CHRBank[0]&1)?0x400:0)|(addr&0x3FF)];
			}
			else if(addr < 0x800)
			{
				if(namco_CHRBank[1] >= 0xE0)
					return namco_VRAM[((namco_CHRBank[1]&1)?0x400:0)|(addr&0x3FF)];
			}
			else if(addr < 0xC00)
			{
				if(namco_CHRBank[2] >= 0xE0)
					return namco_VRAM[((namco_CHRBank[2]&1)?0x400:0)|(addr&0x3FF)];
			}
			else if(addr < 0x1000)
			{
				if(namco_CHRBank[3] >= 0xE0)
					return namco_VRAM[((namco_CHRBank[3]&1)?0x400:0)|(addr&0x3FF)];
			}
		}
		else if(addr >= 0x1000 && namco_CHRBankIsNT1)
		{
			if(addr < 0x1400)
			{
				if(namco_CHRBank[4] >= 0xE0)
					return namco_VRAM[((namco_CHRBank[4]&1)?0x400:0)|(addr&0x3FF)];
			}
			else if(addr < 0x1800)
			{
				if(namco_CHRBank[5] >= 0xE0)
					return namco_VRAM[((namco_CHRBank[5]&1)?0x400:0)|(addr&0x3FF)];
			}
			else if(addr < 0x1C00)
			{
				if(namco_CHRBank[6] >= 0xE0)
					return namco_VRAM[((namco_CHRBank[6]&1)?0x400:0)|(addr&0x3FF)];
			}
			else if(addr < 0x2000)
			{
				if(namco_CHRBank[7] >= 0xE0)
					return namco_VRAM[((namco_CHRBank[7]&1)?0x400:0)|(addr&0x3FF)];
			}
		}
	}
	if(addr < 0x400)
		return namco_chrROM[((namco_CHRBank[0]<<10)|(addr&0x3FF))&namco_chrROMand];
	else if(addr < 0x800)
		return namco_chrROM[((namco_CHRBank[1]<<10)|(addr&0x3FF))&namco_chrROMand];
	else if(addr < 0xC00)
		return namco_chrROM[((namco_CHRBank[2]<<10)|(addr&0x3FF))&namco_chrROMand];
	else if(addr < 0x1000)
		return namco_chrROM[((namco_CHRBank[3]<<10)|(addr&0x3FF))&namco_chrROMand];
	else if(addr < 0x1400)
		return namco_chrROM[((namco_CHRBank[4]<<10)|(addr&0x3FF))&namco_chrROMand];
	else if(addr < 0x1800)
		return namco_chrROM[((namco_CHRBank[5]<<10)|(addr&0x3FF))&namco_chrROMand];
	else if(addr < 0x1C00)
		return namco_chrROM[((namco_CHRBank[6]<<10)|(addr&0x3FF))&namco_chrROMand];
	return namco_chrROM[((namco_CHRBank[7]<<10)|(addr&0x3FF))&namco_chrROMand];
}

void namco_chrSet8(uint16_t addr, uint8_t val)
{
	//printf("namco_chrSet8 %04x %02x\n", addr, val);
	(void)addr;
	(void)val;
}

uint8_t namco_vramGet8(uint16_t addr)
{
	if(namco_type == T_N163)
	{
		if(addr < 0x400)
		{
			if(namco_NTAddr[0] >= 0xE0)
				return namco_VRAM[((namco_NTAddr[0]&1)?0x400:0)|(addr&0x3FF)];
			else
				return namco_chrROM[((namco_NTAddr[0]<<10)|(addr&0x3FF))&namco_chrROMand];
		}
		else if(addr < 0x800)
		{
			if(namco_NTAddr[1] >= 0xE0)
				return namco_VRAM[((namco_NTAddr[1]&1)?0x400:0)|(addr&0x3FF)];
			else
				return namco_chrROM[((namco_NTAddr[1]<<10)|(addr&0x3FF))&namco_chrROMand];
		}
		else if(addr < 0xC00)
		{
			if(namco_NTAddr[2] >= 0xE0)
				return namco_VRAM[((namco_NTAddr[2]&1)?0x400:0)|(addr&0x3FF)];
			else
				return namco_chrROM[((namco_NTAddr[2]<<10)|(addr&0x3FF))&namco_chrROMand];
		}
		else if(addr < 0x1000)
		{
			if(namco_NTAddr[3] >= 0xE0)
				return namco_VRAM[((namco_NTAddr[3]&1)?0x400:0)|(addr&0x3FF)];
			else
				return namco_chrROM[((namco_NTAddr[3]<<10)|(addr&0x3FF))&namco_chrROMand];
		}
	}
	return namco_VRAM[addr&0x7FF];
}

void namco_vramSet8(uint16_t addr, uint8_t val)
{
	if(namco_type == T_N163)
	{
		if(addr < 0x400)
		{
			if(namco_NTAddr[0] >= 0xE0)
				namco_VRAM[((namco_NTAddr[0]&1)?0x400:0)|(addr&0x3FF)] = val;
		}
		else if(addr < 0x800)
		{
			if(namco_NTAddr[1] >= 0xE0)
				namco_VRAM[((namco_NTAddr[1]&1)?0x400:0)|(addr&0x3FF)] = val;
		}
		else if(addr < 0xC00)
		{
			if(namco_NTAddr[2] >= 0xE0)
				namco_VRAM[((namco_NTAddr[2]&1)?0x400:0)|(addr&0x3FF)] = val;
		}
		else if(addr < 0x1000)
		{
			if(namco_NTAddr[3] >= 0xE0)
				namco_VRAM[((namco_NTAddr[3]&1)?0x400:0)|(addr&0x3FF)] = val;
		}
	}
	else
		namco_VRAM[addr&0x7FF] = val;
}

void namco_cycle()
{
	if(n163enabled)
		n163AudioClockTimers();
	if(namco_irqEnable)
	{
		if(namco_irqCtr < 0x7FFF)
			namco_irqCtr++;
		if(namco_irqCtr >= 0x7FFF)
		{
			namco_irqEnable = false;
			if(!mapper_interrupt)
			{
				mapper_interrupt = true;
				//printf("Namco Interrupt\n");
			}
		}
	}
}
