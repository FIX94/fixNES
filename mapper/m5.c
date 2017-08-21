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
#include "../audio_mmc5.h"

static uint8_t *m5_prgROM;
static uint8_t *m5_prgRAM;
static uint8_t *m5_chrROM;
static uint32_t m5_prgROMsize;
static uint32_t m5_prgRAMsize;
static uint32_t m5_chrROMsize;
static uint8_t m5_chrRAM[0x2000];
static uint8_t m5_VRAM[0x800];
static uint8_t m5_exRAM[0x400];
static uint32_t m5_PRGRAMBank0;
static uint32_t m5_PRGBank[4];
static uint8_t m5_PRGBankType[3];
static uint32_t m5_CHRBank[12];
static uint8_t m5_prg_bank_mode;
static uint8_t m5_chr_bank_mode;
static uint8_t m5_irqCtr;
static uint8_t m5_irqVal;
static uint8_t m5_fillTile;
static uint8_t m5_fillAttr;
static bool m5_irqEnable;
static bool m5_inFrame;
static bool m5_irqPending;
static bool m5_chrSet;
static bool m5_split;
static bool m5_splitRight;
static uint8_t m5_splitTile;
static uint8_t m5_splitBank;
extern bool mapper_interrupt;
static uint16_t m5_prevAddr;
static uint8_t m5_mulA, m5_mulB;
static uint16_t m5_mulRes;
static uint32_t m5_prgROMand;
static uint32_t m5_prgRAMand;
static uint32_t m5_chrROMand;
static uint32_t m5_prgRAMBank0add;
static uint32_t m5_prgRAMadd[4];
//used externally
uint8_t m5_exMode;

void m5init(uint8_t *prgROMin, uint32_t prgROMsizeIn, 
			uint8_t *prgRAMin, uint32_t prgRAMsizeIn,
			uint8_t *chrROMin, uint32_t chrROMsizeIn)
{
	m5_prgROM = prgROMin;
	m5_prgROMsize = prgROMsizeIn;
	m5_prgRAM = prgRAMin;
	m5_prgRAMsize = prgRAMsizeIn;
	m5_prgROMand = prgROMsizeIn-1;
	m5_prgRAMand = prgRAMsizeIn-1;
	if(chrROMsizeIn > 0)
	{
		m5_chrROM = chrROMin;
		m5_chrROMsize = chrROMsizeIn;
		m5_chrROMand = chrROMsizeIn-1;
	}
	else
	{
		m5_chrROM = m5_chrRAM;
		m5_chrROMsize = 0x2000;
		m5_chrROMand = 0x1FFF;
	}
	m5_PRGRAMBank0 = 0;
	memset(m5_PRGBank,0,3*sizeof(uint32_t));
	m5_PRGBank[3] = 0xFF;
	memset(m5_PRGBankType,0,3*sizeof(uint8_t));
	memset(m5_chrRAM,0,0x2000);
	memset(m5_VRAM,0,0x800);
	memset(m5_exRAM,0,0x400);
	memset(m5_CHRBank,0,12*sizeof(uint32_t));
	m5_irqCtr = 0;
	m5_irqEnable = false;
	m5_inFrame = false;
	m5_irqPending = false;
	m5_chrSet = false;
	m5_split = false;
	m5_splitRight = false;
	m5_splitTile = 0;
	m5_splitBank = 0;
	m5_exMode = 0;
	m5_irqVal = 0;
	m5_fillTile = 0;
	m5_fillAttr = 0;
	m5_prg_bank_mode = 3;
	m5_chr_bank_mode = 3;
	m5_prevAddr = 0;
	m5_mulA = 0, m5_mulB = 0;
	m5_mulRes = 0;
	m5_prgRAMBank0add = 0;
	memset(m5_prgRAMadd,0,4*sizeof(uint32_t));
	mmc5AudioInit();
	printf("Mapper 5 inited\n");
}

extern bool ppuInFrame;
extern bool ppuScanlineDone;
extern bool ppu816Sprite;

uint8_t m5get8(uint16_t addr, uint8_t val)
{
	if(addr >= 0x5000 && addr < 0x5016)
		return mmc5AudioGet8(addr&0x1F);
	if(addr >= 0x5200 && addr < 0x5207)
	{
		//printf("m5get8 %04x\n", addr);
		uint8_t val;
		switch(addr&7)
		{
			case 3:
				return m5_irqVal;
			case 4:
				val = (m5_irqPending<<7)|(m5_inFrame<<6);
				m5_irqPending = false;
				mapper_interrupt = false;
				//printf("%08x\n",val);
				return val;
			case 5:
				return m5_mulRes&0xFF;
			case 6:
				return m5_mulRes>>8;
		}
	}
	else if(addr >= 0x5C00 && addr < 0x6000 && m5_exMode >= 2)
		return m5_exRAM[addr&0x3FF];
	else if(addr >= 0x6000 && addr < 0x8000)
		return m5_prgRAM[(((m5_PRGRAMBank0<<13)+(addr&0x1FFF))&m5_prgRAMand)|m5_prgRAMBank0add];
	else if(addr >= 0x8000)
	{
		switch(m5_prg_bank_mode)
		{
			case 0:
				return m5_prgROM[(((m5_PRGBank[3]&~3)<<13)+(addr&0x7FFF))&m5_prgROMand];
			case 1:
				if(addr < 0xC000)
				{
					if(m5_PRGBankType[1] == 1) //ROM
						return m5_prgROM[(((m5_PRGBank[1]&~1)<<13)+(addr&0x3FFF))&m5_prgROMand];
					else //RAM
						return m5_prgRAM[((((m5_PRGBank[1]&~1)<<13)+(addr&0x3FFF))&m5_prgRAMand)|m5_prgRAMadd[1]];
				}
				else
					return m5_prgROM[(((m5_PRGBank[3]&~1)<<13)+(addr&0x3FFF))&m5_prgROMand];
			case 2:
				if(addr < 0xC000)
				{
					if(m5_PRGBankType[1] == 1) //ROM
						return m5_prgROM[(((m5_PRGBank[1]&~1)<<13)+(addr&0x3FFF))&m5_prgROMand];
					else //RAM
						return m5_prgRAM[((((m5_PRGBank[1]&~1)<<13)+(addr&0x3FFF))&m5_prgRAMand)|m5_prgRAMadd[1]];
				}
				else if(addr < 0xE000)
				{
					if(m5_PRGBankType[2] == 1) //ROM
						return m5_prgROM[((m5_PRGBank[2]<<13)+(addr&0x1FFF))&m5_prgROMand];
					else //RAM
						return m5_prgRAM[(((m5_PRGBank[2]<<13)+(addr&0x1FFF))&m5_prgRAMand)|m5_prgRAMadd[2]];
				}
				else
					return m5_prgROM[((m5_PRGBank[3]<<13)+(addr&0x1FFF))&m5_prgROMand];
			case 3:
				if(addr < 0xA000)
				{
					if(m5_PRGBankType[0] == 1) //ROM
						return m5_prgROM[((m5_PRGBank[0]<<13)+(addr&0x1FFF))&m5_prgROMand];
					else //RAM
						return m5_prgRAM[(((m5_PRGBank[0]<<13)+(addr&0x1FFF))&m5_prgRAMand)|m5_prgRAMadd[0]];
				}
				else if(addr < 0xC000)
				{
					if(m5_PRGBankType[1] == 1) //ROM
						return m5_prgROM[((m5_PRGBank[1]<<13)+(addr&0x1FFF))&m5_prgROMand];
					else //RAM
						return m5_prgRAM[(((m5_PRGBank[1]<<13)+(addr&0x1FFF))&m5_prgRAMand)|m5_prgRAMadd[1]];
				}
				else if(addr < 0xE000)
				{
					if(m5_PRGBankType[2] == 1) //ROM
						return m5_prgROM[((m5_PRGBank[2]<<13)+(addr&0x1FFF))&m5_prgROMand];
					else //RAM
						return m5_prgRAM[(((m5_PRGBank[2]<<13)+(addr&0x1FFF))&m5_prgRAMand)|m5_prgRAMadd[2]];
				}
				else
					return m5_prgROM[((m5_PRGBank[3]<<13)+(addr&0x1FFF))&m5_prgROMand];
			default:
				break;
		}
	}
	return val;
}

extern bool cpuWriteTMP;
void m5set8(uint16_t addr, uint8_t val)
{
	//printf("m5set8 %04x %02x\n", addr, val);
	if(addr >= 0x5000 && addr < 0x5016)
		mmc5AudioSet8(addr&0x1F, val);
	else if(addr >= 0x5100 && addr < 0x512C)
	{
		//printf("m5set8 %04x %02x\n", addr, val);
		uint16_t v1,v2,v3,v4;
		switch(addr&0x3F)
		{
			case 0:
				m5_prg_bank_mode = val&3;
				break;
			case 1:
				m5_chr_bank_mode = val&3;
				break;
			case 4:
				m5_exMode = val&3;
				break;
			case 5:
				v1 = (val&3)<<10;
				v2 = ((val>>2)&3)<<10;
				v3 = ((val>>4)&3)<<10;
				v4 = ((val>>6)&3)<<10;
				ppuSetNameTblCustom(v1,v2,v3,v4);
				break;
			case 6:
				m5_fillTile = val;
				break;
			case 7:
				m5_fillAttr = val&3;
				break;
			case 0x13:
				m5_PRGRAMBank0 = val&3;
				m5_prgRAMBank0add = (val&4)?0x8000:0;
				break;
			case 0x14:
				m5_PRGBankType[0] = (val&0x80)!=0;
				if(m5_PRGBankType[0] == 1) //ROM
					m5_PRGBank[0] = val&0x7F;
				else //RAM
				{
					m5_PRGBank[0] = val&3;
					m5_prgRAMadd[0] = (val&4)?0x8000:0;
				}
				break;
			case 0x15:
				m5_PRGBankType[1] = (val&0x80)!=0;
				if(m5_PRGBankType[1] == 1) //ROM
					m5_PRGBank[1] = val&0x7F;
				else //RAM
				{
					m5_PRGBank[1] = val&3;
					m5_prgRAMadd[1] = (val&4)?0x8000:0;
				}
				break;
			case 0x16:
				m5_PRGBankType[2] = (val&0x80)!=0;
				if(m5_PRGBankType[2] == 1) //ROM
					m5_PRGBank[2] = val&0x7F;
				else //RAM
				{
					m5_PRGBank[2] = val&3;
					m5_prgRAMadd[2] = (val&4)?0x8000:0;
				}
				break;
			case 0x17:
				//always ROM
				m5_PRGBank[3] = val&0x7F;
				break;
			case 0x20:
				m5_CHRBank[0] = val;
				m5_chrSet = false;
				break;
			case 0x21:
				m5_CHRBank[1] = val;
				m5_chrSet = false;
				break;
			case 0x22:
				m5_CHRBank[2] = val;
				m5_chrSet = false;
				break;
			case 0x23:
				m5_CHRBank[3] = val;
				m5_chrSet = false;
				break;
			case 0x24:
				m5_CHRBank[4] = val;
				m5_chrSet = false;
				break;
			case 0x25:
				m5_CHRBank[5] = val;
				m5_chrSet = false;
				break;
			case 0x26:
				m5_CHRBank[6] = val;
				m5_chrSet = false;
				break;
			case 0x27:
				m5_CHRBank[7] = val;
				m5_chrSet = false;
				break;
			case 0x28:
				m5_CHRBank[0x8] = val;
				m5_chrSet = true;
				break;
			case 0x29:
				m5_CHRBank[0x9] = val;
				m5_chrSet = true;
				break;
			case 0x2A:
				m5_CHRBank[0xA] = val;
				m5_chrSet = true;
				break;
			case 0x2B:
				m5_CHRBank[0xB] = val;
				m5_chrSet = true;
				break;
		}
	}
	else if(addr >= 0x5200 && addr < 0x5207)
	{
		//printf("m5set8 %04x %02x\n", addr, val);
		switch(addr&7)
		{
			case 0:
				m5_split = (val&0x80)!=0;
				m5_splitRight = (val&0x40)!=0;
				m5_splitTile = val&0x1F;
				break;
			case 1:
				//ignored for now
				break;
			case 2:
				m5_splitBank = val;
				break;
			case 3:
				m5_irqVal = val;
				break;
			case 4:
				m5_irqEnable = (val&0x80)!=0;
				break;
			case 5:
				m5_mulA = val;
				m5_mulRes = m5_mulA*m5_mulB;
				break;
			case 6:
				m5_mulB = val;
				m5_mulRes = m5_mulA*m5_mulB;
				break;
		}
	}
	else if(addr >= 0x5C00 && addr < 0x6000)
		m5_exRAM[addr&0x3FF] = val;
	else if(addr >= 0x6000 && addr < 0x8000)
		m5_prgRAM[(((m5_PRGRAMBank0<<13)+(addr&0x1FFF))&m5_prgRAMand)|m5_prgRAMBank0add] = val;
	else if(addr >= 0x8000)
	{
		switch(m5_prg_bank_mode)
		{
			case 0:
				//ROM Only
				break;
			case 1:
				if(addr < 0xC000)
				{
					if(m5_PRGBankType[1] == 0) //RAM
						m5_prgRAM[((((m5_PRGBank[1]&~1)<<13)+(addr&0x3FFF))&m5_prgRAMand)|m5_prgRAMadd[1]] = val;
				}
			case 2:
				if(addr < 0xC000)
				{
					if(m5_PRGBankType[1] == 0) //RAM
						m5_prgRAM[((((m5_PRGBank[1]&~1)<<13)+(addr&0x3FFF))&m5_prgRAMand)|m5_prgRAMadd[1]] = val;
				}
				else if(addr < 0xE000)
				{
					if(m5_PRGBankType[2] == 0) //RAM
						m5_prgRAM[(((m5_PRGBank[2]<<13)+(addr&0x1FFF))&m5_prgRAMand)|m5_prgRAMadd[2]] = val;
				}
			case 3:
				if(addr < 0xA000)
				{
					if(m5_PRGBankType[0] == 0) //RAM
						m5_prgRAM[(((m5_PRGBank[0]<<13)+(addr&0x1FFF))&m5_prgRAMand)|m5_prgRAMadd[0]] = val;
				}
				else if(addr < 0xC000)
				{
					if(m5_PRGBankType[1] == 0) //RAM
						m5_prgRAM[(((m5_PRGBank[1]<<13)+(addr&0x1FFF))&m5_prgRAMand)|m5_prgRAMadd[1]] = val;
				}
				else if(addr < 0xE000)
				{
					if(m5_PRGBankType[2] == 0) //RAM
						m5_prgRAM[(((m5_PRGBank[2]<<13)+(addr&0x1FFF))&m5_prgRAMand)|m5_prgRAMadd[2]] = val;
				}
			default:
				break;
		}
	}
}
extern uint8_t ppuDrawnXTile;
uint8_t m5chrGet8(uint16_t addr)
{
	//printf("%04x\n",addr);
	if(mapperChrMode == 0)
	{
		if(m5_split && m5_exMode < 2)
		{
			if((!m5_splitRight && ppuDrawnXTile < m5_splitTile) ||
				(m5_splitRight && ppuDrawnXTile >= m5_splitTile))
				return m5_chrROM[(((m5_splitBank<<12)+(addr&0xFFF))&m5_chrROMand)];
		}
		else if(m5_exMode == 1) //Special ROM Bank per tile
			return m5_chrROM[((((m5_exRAM[ppuGetCurVramAddr()&0x3FF]&0x3F)<<12)+(addr&0xFFF))&m5_chrROMand)];
	}
	bool readLowRegs = true;
	if(ppu816Sprite)
	{
		if(mapperChrMode == 0) //BG
			readLowRegs = false;
		else if(mapperChrMode == 1) //Sprite
			readLowRegs = true;
		else if(mapperChrMode == 2) //2007
		{
			if(m5_chrSet)
				readLowRegs = false;
			else
				readLowRegs = true;
		}
	}
	if(readLowRegs)
	{
		switch(m5_chr_bank_mode)
		{
			case 0: //8KB Pages
				return m5_chrROM[(((m5_CHRBank[7]<<13)+(addr&0x1FFF))&m5_chrROMand)];
			case 1: //4KB Pages
				if(addr < 0x1000)
					return m5_chrROM[(((m5_CHRBank[3]<<12)+(addr&0xFFF))&m5_chrROMand)];
				return m5_chrROM[(((m5_CHRBank[7]<<12)+(addr&0xFFF))&m5_chrROMand)];
			case 2: //2KB Pages
				if(addr < 0x800)
					return m5_chrROM[(((m5_CHRBank[1]<<11)+(addr&0x7FF))&m5_chrROMand)];
				if(addr < 0x1000)
					return m5_chrROM[(((m5_CHRBank[3]<<11)+(addr&0x7FF))&m5_chrROMand)];
				if(addr < 0x1800)
					return m5_chrROM[(((m5_CHRBank[5]<<11)+(addr&0x7FF))&m5_chrROMand)];
				return m5_chrROM[(((m5_CHRBank[7]<<11)+(addr&0x7FF))&m5_chrROMand)];
			case 3: //1KB Pages
				if(addr < 0x400)
					return m5_chrROM[(((m5_CHRBank[0]<<10)+(addr&0x3FF))&m5_chrROMand)];
				else if(addr < 0x800)
					return m5_chrROM[(((m5_CHRBank[1]<<10)+(addr&0x3FF))&m5_chrROMand)];
				else if(addr < 0xC00)
					return m5_chrROM[(((m5_CHRBank[2]<<10)+(addr&0x3FF))&m5_chrROMand)];
				else if(addr < 0x1000)
					return m5_chrROM[(((m5_CHRBank[3]<<10)+(addr&0x3FF))&m5_chrROMand)];
				else if(addr < 0x1400)
					return m5_chrROM[(((m5_CHRBank[4]<<10)+(addr&0x3FF))&m5_chrROMand)];
				else if(addr < 0x1800)
					return m5_chrROM[(((m5_CHRBank[5]<<10)+(addr&0x3FF))&m5_chrROMand)];
				else if(addr < 0x1C00)
					return m5_chrROM[(((m5_CHRBank[6]<<10)+(addr&0x3FF))&m5_chrROMand)];
				return m5_chrROM[(((m5_CHRBank[7]<<10)+(addr&0x3FF))&m5_chrROMand)];
			default:
				break;
		}
	}
	else
	{
		switch(m5_chr_bank_mode)
		{
			case 0: //8KB Pages
				return m5_chrROM[(((m5_CHRBank[0xB]<<13)+(addr&0x1FFF))&m5_chrROMand)];
			case 1: //4KB Pages
				if(addr < 0x1000)
					return m5_chrROM[(((m5_CHRBank[0xB]<<12)+(addr&0xFFF))&m5_chrROMand)];
				return m5_chrROM[(((m5_CHRBank[0xB]<<12)+(addr&0xFFF))&m5_chrROMand)];
			case 2: //2KB Pages
				if(addr < 0x800)
					return m5_chrROM[(((m5_CHRBank[0x9]<<11)+(addr&0x7FF))&m5_chrROMand)];
				if(addr < 0x1000)
					return m5_chrROM[(((m5_CHRBank[0xB]<<11)+(addr&0x7FF))&m5_chrROMand)];
				if(addr < 0x1800)
					return m5_chrROM[(((m5_CHRBank[0x9]<<11)+(addr&0x7FF))&m5_chrROMand)];
				return m5_chrROM[(((m5_CHRBank[0xB]<<11)+(addr&0x7FF))&m5_chrROMand)];
			case 3: //1KB Pages
				if(addr < 0x400)
					return m5_chrROM[(((m5_CHRBank[0x8]<<10)+(addr&0x3FF))&m5_chrROMand)];
				else if(addr < 0x800)
					return m5_chrROM[(((m5_CHRBank[0x9]<<10)+(addr&0x3FF))&m5_chrROMand)];
				else if(addr < 0xC00)
					return m5_chrROM[(((m5_CHRBank[0xA]<<10)+(addr&0x3FF))&m5_chrROMand)];
				else if(addr < 0x1000)
					return m5_chrROM[(((m5_CHRBank[0xB]<<10)+(addr&0x3FF))&m5_chrROMand)];
				else if(addr < 0x1400)
					return m5_chrROM[(((m5_CHRBank[0x8]<<10)+(addr&0x3FF))&m5_chrROMand)];
				else if(addr < 0x1800)
					return m5_chrROM[(((m5_CHRBank[0x9]<<10)+(addr&0x3FF))&m5_chrROMand)];
				else if(addr < 0x1C00)
					return m5_chrROM[(((m5_CHRBank[0xA]<<10)+(addr&0x3FF))&m5_chrROMand)];
				return m5_chrROM[(((m5_CHRBank[0xB]<<10)+(addr&0x3FF))&m5_chrROMand)];
			default:
				break;
		}
	}
	return 0;
}

void m5chrSet8(uint16_t addr, uint8_t val)
{
	//printf("m5chrSet8 %04x %02x\n", addr, val);
	if(m5_chrROM == m5_chrRAM) //Writable
		m5_chrROM[addr&0x1FFF] = val;
}

uint8_t m5vramGet8(uint16_t addr)
{
	if(m5_split && m5_exMode < 2)
	{
		if((!m5_splitRight && ppuDrawnXTile < m5_splitTile) ||
			(m5_splitRight && ppuDrawnXTile >= m5_splitTile))
		{
			if((addr&0x3FF) < 0x3C0)
				return m5_exRAM[ppuDrawnXTile+((m5_irqCtr&~7)<<2)];
			else
				return m5_exRAM[0x3C0|((ppuDrawnXTile>>3)+((m5_irqCtr&~15)>>2))];
		}
	}
	if(addr < 0x800)
		return m5_VRAM[addr];
	else if(addr < 0xC00)
	{
		if(m5_exMode < 2)
			return m5_exRAM[addr&0x3FF];
		return 0;
	}
	else if(addr < 0xFC0)
		return m5_fillTile;
	return (m5_fillAttr<<0)|(m5_fillAttr<<2)|(m5_fillAttr<<4)|(m5_fillAttr<<6);
}

void m5vramSet8(uint16_t addr, uint8_t val)
{
	if(addr < 0x800)
		m5_VRAM[addr] = val;
	else if(addr < 0xC00 && m5_exMode < 2)
		m5_exRAM[addr&0x3FF] = val;
}

uint8_t m5exGetAttrib(uint16_t addr)
{
	return m5_exRAM[addr&0x3FF]>>6;
}

void m5cycle()
{
	mmc5AudioClockTimers();
	if(ppuInFrame)
	{
		if(ppuScanlineDone)
		{
			ppuScanlineDone = false;
			if(!m5_inFrame)
			{
				m5_inFrame = true;
				m5_irqCtr = 0;
				m5_irqPending = false;
			}
			else
			{
				m5_irqCtr++;
				if(m5_irqCtr == m5_irqVal)
				{
					m5_irqPending = true;
					//printf("MMC5 IRQ at %i\n", m5_irqCtr);
				}
			}
		}
	}
	else
		m5_inFrame = false;

	if(m5_irqPending && m5_irqEnable)
	{
		mapper_interrupt = true;
		//printf("Beep\n");
	}
}
