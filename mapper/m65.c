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

static uint8_t *m65_prgROM;
static uint8_t *m65_prgRAM;
static uint8_t *m65_chrROM;
static uint32_t m65_prgROMsize;
static uint32_t m65_prgRAMsize;
static uint32_t m65_chrROMsize;
static uint32_t m65_prgROMand;
static uint32_t m65_prgRAMand;
static uint32_t m65_chrROMand;
static uint32_t m65_lastPRGBank;
static uint32_t m65_PRGBank[3];
static uint32_t m65_CHRBank[8];
static uint16_t m65_irqCtr;
static uint16_t m65_irqReloadCtr;
static bool m65_irqEnable;
extern uint8_t interrupt;

void m65init(uint8_t *prgROMin, uint32_t prgROMsizeIn, 
			uint8_t *prgRAMin, uint32_t prgRAMsizeIn,
			uint8_t *chrROMin, uint32_t chrROMsizeIn)
{
	m65_prgROM = prgROMin;
	m65_prgROMsize = prgROMsizeIn;
	m65_prgROMand = mapperGetAndValue(prgROMsizeIn);
	m65_prgRAM = prgRAMin;
	m65_prgRAMsize = prgRAMsizeIn;
	m65_prgRAMand = mapperGetAndValue(prgRAMsizeIn);
	m65_lastPRGBank = (prgROMsizeIn - 0x2000);
	if(chrROMsizeIn > 0)
	{
		m65_chrROM = chrROMin;
		m65_chrROMsize = chrROMsizeIn;
		m65_chrROMand = mapperGetAndValue(chrROMsizeIn);
	}
	else
		printf("M65???\n");
	m65_PRGBank[0] = 0;
	m65_PRGBank[1] = 1;
	m65_PRGBank[2] = 0xFE;
	memset(m65_CHRBank,0,8*sizeof(uint32_t));
	m65_irqCtr = 0;
	m65_irqReloadCtr = 0;
	m65_irqEnable = false;
	printf("Mapper 65 inited\n");
}

uint8_t m65get8(uint16_t addr, uint8_t val)
{
	if(addr >= 0x6000 && addr < 0x8000)
		return m65_prgRAM[addr&0x1FFF];
	else if(addr >= 0x8000)
	{
		if(addr < 0xA000)
			return m65_prgROM[((m65_PRGBank[0]<<13)+(addr&0x1FFF))&m65_prgROMand];
		else if(addr < 0xC000)
			return m65_prgROM[((m65_PRGBank[1]<<13)+(addr&0x1FFF))&m65_prgROMand];
		else if(addr < 0xE000)
			return m65_prgROM[((m65_PRGBank[2]<<13)+(addr&0x1FFF))&m65_prgROMand];
		return m65_prgROM[(m65_lastPRGBank+(addr&0x1FFF))&m65_prgROMand];
	}
	return val;
}

void m65set8(uint16_t addr, uint8_t val)
{
	if(addr >= 0x6000 && addr < 0x8000)
	{
		//printf("m65set8 %04x %02x\n", addr, val);
		m65_prgRAM[addr&0x1FFF] = val;
	}
	else if(addr >= 0x8000)
	{
		if(addr < 0x9000)
			m65_PRGBank[0] = val;
		else if(addr < 0xA000)
		{
			switch(addr&7)
			{
				case 1:
					if(val&0x80)
						ppuSetNameTblHorizontal();
					else
						ppuSetNameTblVertical();
					break;
				case 3:
					m65_irqEnable = !!(val&0x80);
					interrupt &= ~MAPPER_IRQ;
					break;
				case 4:
					m65_irqCtr = m65_irqReloadCtr;
					interrupt &= ~MAPPER_IRQ;
					break;
				case 5:
					m65_irqReloadCtr = (m65_irqReloadCtr&0xFF) | (val<<8);
					break;
				case 6:
					m65_irqReloadCtr = (m65_irqReloadCtr&~0xFF) | val;
					break;
				default:
					break;
			}
		}
		else if(addr < 0xB000)
			m65_PRGBank[1] = val;
		else if(addr < 0xC000)
		{
			switch(addr&7)
			{
				case 0x0:
					m65_CHRBank[0] = val;
					break;
				case 0x1:
					m65_CHRBank[1] = val;
					break;
				case 0x2:
					m65_CHRBank[2] = val;
					break;
				case 0x3:
					m65_CHRBank[3] = val;
					break;
				case 0x4:
					m65_CHRBank[4] = val;
					break;
				case 0x5:
					m65_CHRBank[5] = val;
					break;
				case 0x6:
					m65_CHRBank[6] = val;
					break;
				case 0x7:
					m65_CHRBank[7] = val;
					break;
				default:
					break;
			}
		}
		else if(addr < 0xD000)
			m65_PRGBank[2] = val;
	}
}

uint8_t m65chrGet8(uint16_t addr)
{
	//printf("%04x\n",addr);
	addr &= 0x1FFF;
	if(addr < 0x400)
		return m65_chrROM[((m65_CHRBank[0]<<10)+(addr&0x3FF))&m65_chrROMand];
	else if(addr < 0x800)
		return m65_chrROM[((m65_CHRBank[1]<<10)+(addr&0x3FF))&m65_chrROMand];
	else if(addr < 0xC00)
		return m65_chrROM[((m65_CHRBank[2]<<10)+(addr&0x3FF))&m65_chrROMand];
	else if(addr < 0x1000)
		return m65_chrROM[((m65_CHRBank[3]<<10)+(addr&0x3FF))&m65_chrROMand];
	else if(addr < 0x1400)
		return m65_chrROM[((m65_CHRBank[4]<<10)+(addr&0x3FF))&m65_chrROMand];
	else if(addr < 0x1800)
		return m65_chrROM[((m65_CHRBank[5]<<10)+(addr&0x3FF))&m65_chrROMand];
	else if(addr < 0x1C00)
		return m65_chrROM[((m65_CHRBank[6]<<10)+(addr&0x3FF))&m65_chrROMand];
	return m65_chrROM[((m65_CHRBank[7]<<10)+(addr&0x3FF))&m65_chrROMand];
}

void m65chrSet8(uint16_t addr, uint8_t val)
{
	//printf("m65chrSet8 %04x %02x\n", addr, val);
	(void)addr;
	(void)val;
}

void m65cycle()
{
	if(m65_irqCtr && m65_irqEnable)
	{
		m65_irqCtr--;
		if(m65_irqCtr == 0)
			interrupt |= MAPPER_IRQ;
	}
}
