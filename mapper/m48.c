/*
 * Copyright (C) 2017 FIX94
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#include <stdio.h>
#include <inttypes.h>
#include <stdbool.h>
#include "../ppu.h"

static uint8_t *m48_prgROM;
static uint8_t *m48_prgRAM;
static uint8_t *m48_chrROM;
static uint32_t m48_prgROMsize;
static uint32_t m48_prgRAMsize;
static uint32_t m48_chrROMsize;
static uint8_t m48_chrRAM[0x2000];
static uint32_t m48_curPRGBank0;
static uint32_t m48_curPRGBank1;
static uint32_t m48_lastM1PRGBank;
static uint32_t m48_lastPRGBank;
static uint32_t m48_CHRBank[6];
static uint8_t m48_writeAddr;
static uint8_t m48_tmpAddr;
static uint8_t m48_irqCtr;
static bool m48_irqEnable;
static uint8_t m48_irqReloadVal;
static uint8_t m48_irqCooldown;
static uint8_t m48_irqStart;
extern bool mapper_interrupt;
static uint16_t m48_prevAddr;
//used externally
uint32_t m48_prgROMadd;
uint32_t m48_chrROMadd;
uint32_t m48_prgROMand;
uint32_t m48_chrROMand;

void m48init(uint8_t *prgROMin, uint32_t prgROMsizeIn, 
			uint8_t *prgRAMin, uint32_t prgRAMsizeIn,
			uint8_t *chrROMin, uint32_t chrROMsizeIn)
{
	m48_prgROM = prgROMin;
	m48_prgROMsize = prgROMsizeIn;
	m48_prgRAM = prgRAMin;
	m48_prgRAMsize = prgRAMsizeIn;
	m48_prgROMand = prgROMsizeIn-1;
	m48_curPRGBank0 = 0;
	m48_curPRGBank1 = 0x2000;
	m48_lastPRGBank = (prgROMsizeIn - 0x2000);
	m48_lastM1PRGBank = m48_lastPRGBank - 0x2000;
	if(chrROMsizeIn > 0)
	{
		m48_chrROM = chrROMin;
		m48_chrROMsize = chrROMsizeIn;
		m48_chrROMand = chrROMsizeIn-1;
	}
	else
	{
		m48_chrROM = m48_chrRAM;
		m48_chrROMsize = 0x2000;
		m48_chrROMand = 0x1FFF;
	}
	memset(m48_chrRAM,0,0x2000);
	memset(m48_CHRBank,0,6*sizeof(uint32_t));
	m48_tmpAddr = 0;
	m48_irqCtr = 0;
	m48_irqStart = 0;
	m48_writeAddr = 0;
	m48_irqEnable = false;
	m48_irqReloadVal = 0xFF;
	m48_irqCooldown = 0;
	m48_prevAddr = 0;
	m48_prgROMadd = 0;
	m48_chrROMadd = 0;
	printf("Mapper 33/48 inited\n");
}

uint8_t m48get8(uint16_t addr, uint8_t val)
{
	if(addr >= 0x6000 && addr < 0x8000)
		return m48_prgRAM[addr&0x1FFF];
	else if(addr >= 0x8000)
	{
		if(addr < 0xA000)
			return m48_prgROM[(((m48_curPRGBank0<<13)+(addr&0x1FFF))&m48_prgROMand)+m48_prgROMadd];
		else if(addr < 0xC000)
			return m48_prgROM[(((m48_curPRGBank1<<13)+(addr&0x1FFF))&m48_prgROMand)+m48_prgROMadd];
		else if(addr < 0xE000)
			return m48_prgROM[((m48_lastM1PRGBank+(addr&0x1FFF))&m48_prgROMand)+m48_prgROMadd];
		return m48_prgROM[((m48_lastPRGBank+(addr&0x1FFF))&m48_prgROMand)+m48_prgROMadd];
	}
	return val;
}

void m33set8(uint16_t addr, uint8_t val)
{
	//printf("m33set8 %04x %02x\n", addr, val);
	if(addr >= 0x6000 && addr < 0x8000)
		m48_prgRAM[addr&0x1FFF] = val;
	else if(addr == 0x8000)
	{
		m48_curPRGBank0 = (val&0x3F);
		if(!ppu4Screen)
		{
			if((val&0x40) == 0)
			{
				//printf("Vertical mode\n");
				ppuSetNameTblVertical();
			}
			else
			{
				//printf("Horizontal mode\n");
				ppuSetNameTblHorizontal();
			}
		}
	}
	else if(addr == 0x8001)
		m48_curPRGBank1 = (val&0x3F);
	else if(addr == 0x8002)
		m48_CHRBank[0] = val;
	else if(addr == 0x8003)
		m48_CHRBank[1] = val;
	else if(addr == 0xA000)
		m48_CHRBank[2] = val;
	else if(addr == 0xA001)
		m48_CHRBank[3] = val;
	else if(addr == 0xA002)
		m48_CHRBank[4] = val;
	else if(addr == 0xA003)
		m48_CHRBank[5] = val;
}

void m48set8(uint16_t addr, uint8_t val)
{
	//printf("m48set8 %04x %02x\n", addr, val);
	if(addr >= 0x6000 && addr < 0x8000)
		m48_prgRAM[addr&0x1FFF] = val;
	else if(addr == 0x8000)
		m48_curPRGBank0 = (val&0x3F);
	else if(addr == 0x8001)
		m48_curPRGBank1 = (val&0x3F);
	else if(addr == 0x8002)
		m48_CHRBank[0] = val;
	else if(addr == 0x8003)
		m48_CHRBank[1] = val;
	else if(addr == 0xA000)
		m48_CHRBank[2] = val;
	else if(addr == 0xA001)
		m48_CHRBank[3] = val;
	else if(addr == 0xA002)
		m48_CHRBank[4] = val;
	else if(addr == 0xA003)
		m48_CHRBank[5] = val;
	else if(addr == 0xC000)
		m48_irqReloadVal = val^0xFF;
	else if(addr == 0xC001)
		m48_irqCtr = 0;
	else if(addr == 0xC002)
	{
		m48_irqStart = 0;
		m48_irqEnable = true;
		//printf("Interrupt enabled\n");
	}
	else if(addr == 0xC003)
	{
		m48_irqEnable = false;
		mapper_interrupt = false;
		m48_irqStart = 0;
		//printf("Interrupt disabled\n");
	}
	else if(addr == 0xE000)
	{
		if(!ppu4Screen)
		{
			if((val&0x40) == 0)
			{
				//printf("Vertical mode\n");
				ppuSetNameTblVertical();
			}
			else
			{
				//printf("Horizontal mode\n");
				ppuSetNameTblHorizontal();
			}
		}
	}
}

void m48clock(uint16_t addr)
{
	if(addr & 0x1000)
	{
		if(m48_irqCooldown == 0)
		{
			//printf("MMC3 Beep at %i %i\n", curLine, curDot);
			if(m48_irqCtr == 0)
				m48_irqCtr = m48_irqReloadVal;
			else
				m48_irqCtr--;
			if(m48_irqCtr == 0)
			{
				if(m48_irqEnable)
				{
					//printf("MMC3 Tick at %i %i\n", curLine, curDot);
					m48_irqStart = 5; //takes a bit before trigger
					m48_irqEnable = false;
				}
			}
		}
		//make sure to pass all other sprites
		//before counting up again
		m48_irqCooldown = 20;
	}
}

uint8_t m48chrGet8(uint16_t addr)
{
	//printf("%04x\n",addr);
	m48clock(addr);
	addr &= 0x1FFF;
	if(addr < 0x800)
		return m48_chrROM[(((m48_CHRBank[0]<<11)+(addr&0x7FF))&m48_chrROMand)+m48_chrROMadd];
	else if(addr < 0x1000)
		return m48_chrROM[(((m48_CHRBank[1]<<11)+(addr&0x7FF))&m48_chrROMand)+m48_chrROMadd];
	else if(addr < 0x1400)
		return m48_chrROM[(((m48_CHRBank[2]<<10)+(addr&0x3FF))&m48_chrROMand)+m48_chrROMadd];
	else if(addr < 0x1800)
		return m48_chrROM[(((m48_CHRBank[3]<<10)+(addr&0x3FF))&m48_chrROMand)+m48_chrROMadd];
	else if(addr < 0x1C00)
		return m48_chrROM[(((m48_CHRBank[4]<<10)+(addr&0x3FF))&m48_chrROMand)+m48_chrROMadd];
	return m48_chrROM[(((m48_CHRBank[5]<<10)+(addr&0x3FF))&m48_chrROMand)+m48_chrROMadd];
}

void m48chrSet8(uint16_t addr, uint8_t val)
{
	//printf("m48chrSet8 %04x %02x\n", addr, val);
	if(m48_chrROM == m48_chrRAM) //Writable
		m48_chrROM[addr&0x1FFF] = val;
}

void m48cycle()
{
	uint16_t curAddr = ppuGetCurVramAddr();
	if((curAddr & 0x1000) && !(m48_prevAddr & 0x1000))
		m48clock(curAddr);
	m48_prevAddr = curAddr;
	if(m48_irqCooldown)
		m48_irqCooldown--;
	if(m48_irqStart == 1)
	{
		mapper_interrupt = true;
		m48_irqStart = 0;
	}
	else if(m48_irqStart > 1)
		m48_irqStart--;
}

