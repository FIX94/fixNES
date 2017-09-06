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

static uint8_t *m4_prgROM;
static uint8_t *m4_prgRAM;
static uint8_t *m4_chrROM;
static uint32_t m4_prgROMsize;
static uint32_t m4_prgRAMsize;
static uint32_t m4_chrROMsize;
static uint8_t m4_chrRAM[0x2000];
static uint32_t m4_curPRGBank0;
static uint32_t m4_curPRGBank1;
static uint32_t m4_lastM1PRGBank;
static uint32_t m4_lastPRGBank;
static uint32_t m4_CHRBank[6];
static uint8_t m4_writeAddr;
static uint8_t m4_tmpAddr;
static bool m4_chr_bank_flip;
static bool m4_prg_bank_flip;
static uint8_t m4_irqCtr;
static bool m4_irqEnable;
static bool m4_altirq;
static bool m4_clear;
static uint8_t m4_irqReloadVal;
static uint8_t m4_irqStart;
extern bool mapper_interrupt;
static uint16_t m4_prevAddr;
//used externally
uint32_t m4_prgROMadd;
uint32_t m4_chrROMadd;
uint32_t m4_prgROMand;
uint32_t m4_chrROMand;

void m4init(uint8_t *prgROMin, uint32_t prgROMsizeIn, 
			uint8_t *prgRAMin, uint32_t prgRAMsizeIn,
			uint8_t *chrROMin, uint32_t chrROMsizeIn)
{
	m4_prgROM = prgROMin;
	m4_prgROMsize = prgROMsizeIn;
	m4_prgRAM = prgRAMin;
	m4_prgRAMsize = prgRAMsizeIn;
	m4_prgROMand = mapperGetAndValue(prgROMsizeIn);
	m4_curPRGBank0 = 0;
	m4_curPRGBank1 = 0x2000;
	m4_lastPRGBank = (prgROMsizeIn - 0x2000);
	m4_lastM1PRGBank = m4_lastPRGBank - 0x2000;
	if(chrROMsizeIn > 0)
	{
		m4_chrROM = chrROMin;
		m4_chrROMsize = chrROMsizeIn;
		m4_chrROMand = mapperGetAndValue(chrROMsizeIn);
	}
	else
	{
		m4_chrROM = m4_chrRAM;
		m4_chrROMsize = 0x2000;
		m4_chrROMand = 0x1FFF;
	}
	memset(m4_chrRAM,0,0x2000);
	memset(m4_CHRBank,0,6*sizeof(uint32_t));
	m4_tmpAddr = 0;
	m4_irqCtr = 0;
	m4_irqStart = 0;
	m4_writeAddr = 0;
	m4_irqEnable = false;
	//can enable MMC3A IRQ behaviour
	m4_altirq = false;
	m4_clear = false;
	m4_irqReloadVal = 0xFF;
	m4_chr_bank_flip = false;
	m4_prg_bank_flip = false;
	m4_prevAddr = 0;
	m4_prgROMadd = 0;
	m4_chrROMadd = 0;
	printf("Mapper 4 inited\n");
}

static uint32_t m12_chrROMadd0;
static uint32_t m12_chrROMadd1;
void m12init(uint8_t *prgROMin, uint32_t prgROMsizeIn, 
			uint8_t *prgRAMin, uint32_t prgRAMsizeIn,
			uint8_t *chrROMin, uint32_t chrROMsizeIn)
{
	m4init(prgROMin, prgROMsizeIn, prgRAMin, prgRAMsizeIn, chrROMin, chrROMsizeIn);
	m4_altirq = true; //needs to be MMC3A
	m4_chrROMand = 0x3FFFF; //forced CHR size
	m12_chrROMadd0 = 0;
	m12_chrROMadd1 = 0;
	printf("Mapper 12 (Pirate Mapper 4) inited\n");
}

static uint16_t m118nt[6];
void m118init(uint8_t *prgROMin, uint32_t prgROMsizeIn, 
			uint8_t *prgRAMin, uint32_t prgRAMsizeIn,
			uint8_t *chrROMin, uint32_t chrROMsizeIn)
{
	m4init(prgROMin, prgROMsizeIn, prgRAMin, prgRAMsizeIn, chrROMin, chrROMsizeIn);
	memset(m118nt,0,6*sizeof(uint16_t));
	printf("Mapper 118 (Mapper 4 Variant) inited\n");
}

void m119init(uint8_t *prgROMin, uint32_t prgROMsizeIn, 
			uint8_t *prgRAMin, uint32_t prgRAMsizeIn,
			uint8_t *chrROMin, uint32_t chrROMsizeIn)
{
	m4init(prgROMin, prgROMsizeIn, prgRAMin, prgRAMsizeIn, chrROMin, chrROMsizeIn);
	printf("Mapper 119 (Mapper 4 Variant) inited\n");
}

uint8_t m4get8(uint16_t addr, uint8_t val)
{
	if(addr >= 0x6000 && addr < 0x8000)
		return m4_prgRAM[addr&0x1FFF];
	else if(addr >= 0x8000)
	{
		if(addr < 0xA000)
		{
			if(m4_prg_bank_flip)
				return m4_prgROM[(((m4_lastM1PRGBank+(addr&0x1FFF)))&m4_prgROMand)|m4_prgROMadd];
			else
				return m4_prgROM[(((m4_curPRGBank0<<13)+(addr&0x1FFF))&m4_prgROMand)|m4_prgROMadd];
		}
		else if(addr < 0xC000)
			return m4_prgROM[(((m4_curPRGBank1<<13)+(addr&0x1FFF))&m4_prgROMand)|m4_prgROMadd];
		else if(addr < 0xE000)
		{
			if(m4_prg_bank_flip)
				return m4_prgROM[(((m4_curPRGBank0<<13)+(addr&0x1FFF))&m4_prgROMand)|m4_prgROMadd];
			else
				return m4_prgROM[((m4_lastM1PRGBank+(addr&0x1FFF))&m4_prgROMand)|m4_prgROMadd];
		}
		return m4_prgROM[((m4_lastPRGBank+(addr&0x1FFF))&m4_prgROMand)|m4_prgROMadd];
	}
	return val;
}

extern bool cpuWriteTMP;
void m4set8(uint16_t addr, uint8_t val)
{
	if(addr >= 0x6000 && addr < 0x8000)
	{
		//printf("m4set8 %04x %02x\n", addr, val);
		m4_prgRAM[addr&0x1FFF] = val;
	}
	else if(addr >= 0x8000)
	{
		// mmc1 regs cant be written to
		// with just 1 cpu cycle delay
		//if(cpuWriteTMP)
		//	return;
		//printf("m4set8 %04x %02x\n", addr, val);
		if(addr < 0xA000)
		{
			if((addr&1) == 0)
			{
				m4_chr_bank_flip = ((val&(1<<7)) != 0);
				m4_prg_bank_flip = ((val&(1<<6)) != 0);
				m4_writeAddr = (val&7);
			}
			else
			{
				switch(m4_writeAddr)
				{
					case 0:
						m4_CHRBank[0] = val;
						break;
					case 1:
						m4_CHRBank[1] = val;
						break;
					case 2:
						m4_CHRBank[2] = val;
						break;
					case 3:
						m4_CHRBank[3] = val;
						break;
					case 4:
						m4_CHRBank[4] = val;
						break;
					case 5:
						m4_CHRBank[5] = val;
						break;
					case 6:
						m4_curPRGBank0 = val;
						break;
					case 7:
						m4_curPRGBank1 = val;
						break;
				}
			}
		}
		else if(addr < 0xC000)
		{
			if((addr&1) == 0)
			{
				if(!ppu4Screen)
				{
					if((val&1) == 0)
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
		else if(addr < 0xE000)
		{
			if((addr&1) == 0)
			{
				//printf("Reload value set to %i\n", val);
				m4_irqReloadVal = val;
				if(m4_altirq)
					m4_clear = true;
			}
			else
			{
				//printf("Reset counter\n");
				m4_irqCtr = 0;
			}
		}
		else
		{
			if((addr&1) == 0)
			{
				m4_irqEnable = false;
				mapper_interrupt = false;
				m4_irqStart = 0;
				//printf("Interrupt disabled\n");
			}
			else
			{
				m4_irqStart = 0;
				m4_irqEnable = true;
				//printf("Interrupt enabled\n");
			}
		}
	}
}

void m12set8(uint16_t addr, uint8_t val)
{
	//do normal m4 sets
	m4set8(addr, val);
	//special pirate regs
	if(addr >= 0x4020 && addr < 0x6000)
	{
		m12_chrROMadd0 = (val&0x01)?0x40000:0;
		m12_chrROMadd1 = (val&0x10)?0x40000:0;
	}
}

void m118set8(uint16_t addr, uint8_t val)
{
	//Bypass standard mirroring controls
	if(addr >= 0xA000 && addr < 0xC000)
		return;
	//do normal m4 sets
	m4set8(addr, val);
	//Set up special mirroring based on low ppu regs
	if(addr >= 0x8000 && addr < 0xA000 && (addr&1) && (m4_writeAddr <= 5))
		m118nt[m4_writeAddr] = (val&0x80)?0x400:0;
	if(m4_chr_bank_flip) //upper bits used for low ppu
		ppuSetNameTblCustom(m118nt[2],m118nt[3],m118nt[4],m118nt[5]);
	else //lower bits used for low ppu
		ppuSetNameTblCustom(m118nt[0],m118nt[0],m118nt[1],m118nt[1]);
}

extern void ppuPrintCurLineDot();
void m4clock(uint16_t addr)
{
	if((addr & 0x1000) && !(m4_prevAddr & 0x1000))
	{
		//printf("MMC3 Beep at %i %i\n", curLine, curDot);
		uint8_t oldCtr = m4_irqCtr;
		if(m4_irqCtr == 0 || m4_clear)
			m4_irqCtr = m4_irqReloadVal;
		else
			m4_irqCtr--;
		if((!m4_altirq || oldCtr != 0 || m4_clear) && m4_irqCtr == 0 && m4_irqEnable)
		{
			//ppuPrintCurLineDot();
			//printf("MMC3 Tick at %i %i\n", curLine, curDot);
			//m4_irqStart = 2; //takes a bit before trigger
			mapper_interrupt = true;
			m4_irqEnable = false;
		}
		m4_clear = false;
	}
	m4_prevAddr = addr;
}

uint8_t m4chrGet8(uint16_t addr)
{
	//printf("%04x\n",addr);
	m4clock(addr);
	addr &= 0x1FFF;
	if(m4_chr_bank_flip)
		addr ^= 0x1000;
	if(addr < 0x800)
		return m4_chrROM[((((m4_CHRBank[0]&~1)<<10)+(addr&0x7FF))&m4_chrROMand)|m4_chrROMadd];
	else if(addr < 0x1000)
		return m4_chrROM[((((m4_CHRBank[1]&~1)<<10)+(addr&0x7FF))&m4_chrROMand)|m4_chrROMadd];
	else if(addr < 0x1400)
		return m4_chrROM[(((m4_CHRBank[2]<<10)+(addr&0x3FF))&m4_chrROMand)|m4_chrROMadd];
	else if(addr < 0x1800)
		return m4_chrROM[(((m4_CHRBank[3]<<10)+(addr&0x3FF))&m4_chrROMand)|m4_chrROMadd];
	else if(addr < 0x1C00)
		return m4_chrROM[(((m4_CHRBank[4]<<10)+(addr&0x3FF))&m4_chrROMand)|m4_chrROMadd];
	return m4_chrROM[(((m4_CHRBank[5]<<10)+(addr&0x3FF))&m4_chrROMand)|m4_chrROMadd];
}

uint8_t m12chrGet8(uint16_t addr)
{
	//printf("%04x\n",addr);
	m4clock(addr);
	addr &= 0x1FFF;
	if(m4_chr_bank_flip)
		addr ^= 0x1000;
	if(addr < 0x800)
		return m4_chrROM[((((m4_CHRBank[0]&~1)<<10)+(addr&0x7FF))&m4_chrROMand)|m12_chrROMadd0];
	else if(addr < 0x1000)
		return m4_chrROM[((((m4_CHRBank[1]&~1)<<10)+(addr&0x7FF))&m4_chrROMand)|m12_chrROMadd0];
	else if(addr < 0x1400)
		return m4_chrROM[(((m4_CHRBank[2]<<10)+(addr&0x3FF))&m4_chrROMand)|m12_chrROMadd1];
	else if(addr < 0x1800)
		return m4_chrROM[(((m4_CHRBank[3]<<10)+(addr&0x3FF))&m4_chrROMand)|m12_chrROMadd1];
	else if(addr < 0x1C00)
		return m4_chrROM[(((m4_CHRBank[4]<<10)+(addr&0x3FF))&m4_chrROMand)|m12_chrROMadd1];
	return m4_chrROM[(((m4_CHRBank[5]<<10)+(addr&0x3FF))&m4_chrROMand)|m12_chrROMadd1];
}

uint8_t m119chrGet8(uint16_t addr)
{
	m4clock(addr);
	addr &= 0x1FFF;
	if(m4_chr_bank_flip)
		addr ^= 0x1000;
	//check for RAM first
	if(addr < 0x800)
	{
		if(m4_CHRBank[0]&0x40)
			return m4_chrRAM[(((m4_CHRBank[0]&~1)<<10)+(addr&0x7FF))&0x1FFF];
		return m4_chrROM[((((m4_CHRBank[0]&~1)<<10)+(addr&0x7FF))&m4_chrROMand)|m4_chrROMadd];
	}
	else if(addr < 0x1000)
	{
		if(m4_CHRBank[1]&0x40)
			return m4_chrRAM[(((m4_CHRBank[1]&~1)<<10)+(addr&0x7FF))&0x1FFF];
		return m4_chrROM[((((m4_CHRBank[1]&~1)<<10)+(addr&0x7FF))&m4_chrROMand)|m4_chrROMadd];
	}
	else if(addr < 0x1400)
	{
		if(m4_CHRBank[2]&0x40)
			return m4_chrRAM[((m4_CHRBank[2]<<10)+(addr&0x3FF))&0x1FFF];
		return m4_chrROM[(((m4_CHRBank[2]<<10)+(addr&0x3FF))&m4_chrROMand)|m4_chrROMadd];
	}
	else if(addr < 0x1800)
	{
		if(m4_CHRBank[3]&0x40)
			return m4_chrRAM[((m4_CHRBank[3]<<10)+(addr&0x3FF))&0x1FFF];
		return m4_chrROM[(((m4_CHRBank[3]<<10)+(addr&0x3FF))&m4_chrROMand)|m4_chrROMadd];
	}
	else if(addr < 0x1C00)
	{
		if(m4_CHRBank[4]&0x40)
			return m4_chrRAM[((m4_CHRBank[4]<<10)+(addr&0x3FF))&0x1FFF];
		return m4_chrROM[(((m4_CHRBank[4]<<10)+(addr&0x3FF))&m4_chrROMand)|m4_chrROMadd];
	}
	//1C00-2000
	if(m4_CHRBank[5]&0x40)
		return m4_chrRAM[((m4_CHRBank[5]<<10)+(addr&0x3FF))&0x1FFF];
	return m4_chrROM[(((m4_CHRBank[5]<<10)+(addr&0x3FF))&m4_chrROMand)|m4_chrROMadd];
}

void m4chrSet8(uint16_t addr, uint8_t val)
{
	m4clock(addr);
	addr &= 0x1FFF;
	if(m4_chr_bank_flip)
		addr ^= 0x1000;
	//printf("m4chrSet8 %04x %02x\n", addr, val);
	if(m4_chrROM == m4_chrRAM) //Writable
	{
		if(addr < 0x800)
			m4_chrRAM[(((m4_CHRBank[0]&~1)<<10)+(addr&0x7FF))&0x1FFF] = val;
		else if(addr < 0x1000)
			m4_chrRAM[(((m4_CHRBank[1]&~1)<<10)+(addr&0x7FF))&0x1FFF] = val;
		else if(addr < 0x1400)
			m4_chrRAM[((m4_CHRBank[2]<<10)+(addr&0x3FF))&0x1FFF] = val;
		else if(addr < 0x1800)
			m4_chrRAM[((m4_CHRBank[3]<<10)+(addr&0x3FF))&0x1FFF] = val;
		else if(addr < 0x1C00)
			m4_chrRAM[((m4_CHRBank[4]<<10)+(addr&0x3FF))&0x1FFF] = val;
		else //1C00-2000
			m4_chrRAM[((m4_CHRBank[5]<<10)+(addr&0x3FF))&0x1FFF] = val;
	}
}

void m119chrSet8(uint16_t addr, uint8_t val)
{
	m4clock(addr);
	addr &= 0x1FFF;
	if(m4_chr_bank_flip)
		addr ^= 0x1000;
	//printf("m119chrSet8 %04x %02x\n", addr, val);
	if(addr < 0x800)
	{
		if(m4_CHRBank[0]&0x40)
			m4_chrRAM[(((m4_CHRBank[0]&~1)<<10)+(addr&0x7FF))&0x1FFF] = val;
	}
	else if(addr < 0x1000)
	{
		if(m4_CHRBank[1]&0x40)
			m4_chrRAM[(((m4_CHRBank[1]&~1)<<10)+(addr&0x7FF))&0x1FFF] = val;
	}
	else if(addr < 0x1400)
	{
		if(m4_CHRBank[2]&0x40)
			m4_chrRAM[((m4_CHRBank[2]<<10)+(addr&0x3FF))&0x1FFF] = val;
	}
	else if(addr < 0x1800)
	{
		if(m4_CHRBank[3]&0x40)
			m4_chrRAM[((m4_CHRBank[3]<<10)+(addr&0x3FF))&0x1FFF] = val;
	}
	else if(addr < 0x1C00)
	{
		if(m4_CHRBank[4]&0x40)
			m4_chrRAM[((m4_CHRBank[4]<<10)+(addr&0x3FF))&0x1FFF] = val;
	}
	else //1C00-2000
	{
		if(m4_CHRBank[5]&0x40)
			m4_chrRAM[((m4_CHRBank[5]<<10)+(addr&0x3FF))&0x1FFF] = val;
	}
}

void m4cycle()
{
	if(m4_irqStart == 1)
	{
		mapper_interrupt = true;
		m4_irqStart = 0;
	}
	else if(m4_irqStart > 1)
		m4_irqStart--;
}

