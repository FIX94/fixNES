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

static uint8_t *vrc_prgROM;
static uint8_t *vrc_prgRAM;
static uint8_t *vrc_chrROM;
static uint32_t vrc_prgROMsize;
static uint32_t vrc_prgRAMsize;
static uint32_t vrc_chrROMsize;
static uint32_t vrc_curPRGBank0;
static uint32_t vrc_curPRGBank1;
static uint32_t vrc_lastM1PRGBank;
static uint32_t vrc_lastPRGBank;
static uint32_t vrc_CHRBank[8];
static uint32_t vrc_prgROMand;
static uint32_t vrc_chrROMand;
static uint8_t vrc_irqCtr;
static uint8_t vrc_irqCurCtr;
static uint8_t vrc_irqCurCycleCtr;
static uint8_t vrc_scanTblPos;
static bool vrc_irq_enabled;
static bool vrc_irq_enable_after_ack;
static bool vrc_irq_cyclemode;
static bool vrc_prg_bank_flip;
extern bool mapper_interrupt;

static uint8_t vrc_scanTbl[3] = { 113, 113, 112 };

void vrcinit(uint8_t *prgROMin, uint32_t prgROMsizeIn, 
			uint8_t *prgRAMin, uint32_t prgRAMsizeIn,
			uint8_t *chrROMin, uint32_t chrROMsizeIn)
{
	vrc_prgROM = prgROMin;
	vrc_prgROMsize = prgROMsizeIn;
	vrc_prgRAM = prgRAMin;
	vrc_prgRAMsize = prgRAMsizeIn;
	vrc_curPRGBank0 = 0;
	vrc_curPRGBank1 = 0x2000;
	vrc_lastPRGBank = (prgROMsizeIn - 0x2000);
	vrc_lastM1PRGBank = vrc_lastPRGBank - 0x2000;
	vrc_prgROMand = prgROMsizeIn-1;
	vrc_chrROM = chrROMin;
	vrc_chrROMsize = chrROMsizeIn;
	vrc_chrROMand = chrROMsizeIn-1;
	memset(vrc_CHRBank, 0, 8*sizeof(uint32_t));
	vrc_prg_bank_flip = false;
	vrc_irqCtr = 0;
	vrc_irqCurCtr = 0;
	vrc_irqCurCycleCtr = 0;
	vrc_irq_enabled = false;
	vrc_irq_enable_after_ack = false;
	vrc_irq_cyclemode = false;
	vrc_scanTblPos = 0;
	printf("VRC Mapper inited\n");
}

uint8_t vrcget8(uint16_t addr)
{
	if(addr >= 0x6000 && addr < 0x8000)
		return vrc_prgRAM[addr&0x1FFF];
	else if(addr >= 0x8000)
	{
		if(addr < 0xA000)
		{
			if(vrc_prg_bank_flip)
				return vrc_prgROM[(((vrc_lastM1PRGBank+(addr&0x1FFF)))&vrc_prgROMand)];
			else
				return vrc_prgROM[(((vrc_curPRGBank0<<13)+(addr&0x1FFF))&vrc_prgROMand)];
		}
		else if(addr < 0xC000)
			return vrc_prgROM[(((vrc_curPRGBank1<<13)+(addr&0x1FFF))&vrc_prgROMand)];
		else if(addr < 0xE000)
		{
			if(vrc_prg_bank_flip)
				return vrc_prgROM[(((vrc_curPRGBank0<<13)+(addr&0x1FFF))&vrc_prgROMand)];
			else
				return vrc_prgROM[((vrc_lastM1PRGBank+(addr&0x1FFF))&vrc_prgROMand)];
		}
		return vrc_prgROM[((vrc_lastPRGBank+(addr&0x1FFF))&vrc_prgROMand)];
	}
	return 0;
}

void vrcset8(uint16_t addr, uint8_t val)
{
	//select all possible addresses
	addr &= 0xF003;
	if(addr == 0x8000 || addr == 0x8001 || addr == 0x8002 || addr == 0x8003)
		vrc_curPRGBank0 = (val&0x1F);
	else if(addr == 0x9000 || addr == 0x9001)
	{
		if(!ppu4Screen)
		{
			val &= 3;
			switch(val)
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
				default: //case 3
					ppuSetNameTblSingleUpper();
					break;
			}
		}
	}
	else if(addr == 0x9002 || addr == 0x9003)
		vrc_prg_bank_flip = ((val&2) != 0);
	else if(addr == 0xA000 || addr == 0xA001 || addr == 0xA002 || addr == 0xA003)
		vrc_curPRGBank1 = (val&0x1F);
	else if(addr == 0xB000)
		vrc_CHRBank[0] = (vrc_CHRBank[0]&~0xF) | (val&0xF);
	else if(addr == 0xB001)
		vrc_CHRBank[0] = (vrc_CHRBank[0]&0xF) | ((val&0x1F)<<4);
	else if(addr == 0xB002)
		vrc_CHRBank[1] = (vrc_CHRBank[1]&~0xF) | (val&0xF);
	else if(addr == 0xB003)
		vrc_CHRBank[1] = (vrc_CHRBank[1]&0xF) | ((val&0x1F)<<4);
	else if(addr == 0xC000)
		vrc_CHRBank[2] = (vrc_CHRBank[2]&~0xF) | (val&0xF);
	else if(addr == 0xC001)
		vrc_CHRBank[2] = (vrc_CHRBank[2]&0xF) | ((val&0x1F)<<4);
	else if(addr == 0xC002)
		vrc_CHRBank[3] = (vrc_CHRBank[3]&~0xF) | (val&0xF);
	else if(addr == 0xC003)
		vrc_CHRBank[3] = (vrc_CHRBank[3]&0xF) | ((val&0x1F)<<4);
	else if(addr == 0xD000)
		vrc_CHRBank[4] = (vrc_CHRBank[4]&~0xF) | (val&0xF);
	else if(addr == 0xD001)
		vrc_CHRBank[4] = (vrc_CHRBank[4]&0xF) | ((val&0x1F)<<4);
	else if(addr == 0xD002)
		vrc_CHRBank[5] = (vrc_CHRBank[5]&~0xF) | (val&0xF);
	else if(addr == 0xD003)
		vrc_CHRBank[5] = (vrc_CHRBank[5]&0xF) | ((val&0x1F)<<4);
	else if(addr == 0xE000)
		vrc_CHRBank[6] = (vrc_CHRBank[6]&~0xF) | (val&0xF);
	else if(addr == 0xE001)
		vrc_CHRBank[6] = (vrc_CHRBank[6]&0xF) | ((val&0x1F)<<4);
	else if(addr == 0xE002)
		vrc_CHRBank[7] = (vrc_CHRBank[7]&~0xF) | (val&0xF);
	else if(addr == 0xE003)
		vrc_CHRBank[7] = (vrc_CHRBank[7]&0xF) | ((val&0x1F)<<4);
	else if(addr == 0xF000)
		vrc_irqCtr = (vrc_irqCtr&~0xF) | (val&0xF);
	else if(addr == 0xF001)
		vrc_irqCtr = (vrc_irqCtr&0xF) | ((val&0xF)<<4);
	else if(addr == 0xF002)
	{
		mapper_interrupt = false;
		vrc_irq_enable_after_ack = ((val&1) != 0);
		vrc_irq_cyclemode = ((val&4) != 0);
		if((val&2) != 0)
		{
			vrc_irq_enabled = true;
			vrc_irqCurCtr = vrc_irqCtr;
			vrc_irqCurCycleCtr = 0;
			vrc_scanTblPos = 0;
			//printf("VRC IRQ Reload with in %02x ctr %i\n", val, vrc_irqCtr);
		}
	}
	else if(addr == 0xF003)
	{
		mapper_interrupt = false;
		if(vrc_irq_enable_after_ack)
		{
			vrc_irq_enabled = true;
			//printf("VRC ACK IRQ Reload\n");
		}
		//else
		//	printf("VRC ACK No Reload\n");
	}
}

void m21_set8(uint16_t addr, uint8_t val)
{
	if(addr < 0x6000)
		return;
	if(addr < 0x8000)
	{
		vrc_prgRAM[addr&0x1FFF] = val;
		return;
	}
	//printf("m21_set8 %04x %02x\n", addr, val);
	//VRC4 M21 Address Setup
	uint8_t tmp1 = (addr>>1)&3;
	uint8_t tmp2 = (addr>>6)&3;
	addr &= 0xF000; addr |= (tmp1 | tmp2);
	vrcset8(addr, val);
}

void m22_set8(uint16_t addr, uint8_t val)
{
	if(addr < 0x8000)
		return;
	//printf("m22_set8 %04x %02x\n", addr, val);
	//VRC2 only supports vertical/horizonal
	if(addr == 0x9000 || addr == 0x9002)
		val &= 1;
	//VRC2 M22 Address Setup
	uint8_t tmp = addr&3;
	addr &= ~0x3; addr |= (((tmp&1)<<1) | ((tmp&2)>>1));
	vrcset8(addr, val);
}

void m23_set8(uint16_t addr, uint8_t val)
{
	if(addr < 0x6000)
		return;
	if(addr < 0x8000)
	{
		vrc_prgRAM[addr&0x1FFF] = val;
		return;
	}
	//printf("m23_set8 %04x %02x\n", addr, val);
	//VRC2 Wai Wai World, avoid wrong mirroring
	if(addr == 0x9000 && val == 0xFF)
		val &= 1; //only supports vertical/horizonal
	//VRC4 M23 Address Setup
	uint8_t tmp = (addr>>2)&3;
	addr &= ~0xC; addr |= tmp;
	vrcset8(addr, val);
}

void m25_set8(uint16_t addr, uint8_t val)
{
	if(addr < 0x6000)
		return;
	if(addr < 0x8000)
	{
		vrc_prgRAM[addr&0x1FFF] = val;
		return;
	}
	//printf("m25_set8 %04x %02x\n", addr, val);
	//VRC4 M25 Address Setup
	uint8_t tmp1 = (((addr&1)<<1) | ((addr&2)>>1));
	uint8_t tmp2 = ((((addr>>2)&1)<<1) | (((addr>>2)&2)>>1));
	addr &= 0xF000; addr |= (tmp1 | tmp2);
	vrcset8(addr, val);
}

uint8_t vrcchrGet8(uint16_t addr)
{
	if(addr < 0x400)
		return vrc_chrROM[(((vrc_CHRBank[0]<<10)+(addr&0x3FF))&vrc_chrROMand)];
	else if(addr < 0x800)
		return vrc_chrROM[(((vrc_CHRBank[1]<<10)+(addr&0x3FF))&vrc_chrROMand)];
	else if(addr < 0xC00)
		return vrc_chrROM[(((vrc_CHRBank[2]<<10)+(addr&0x3FF))&vrc_chrROMand)];
	else if(addr < 0x1000)
		return vrc_chrROM[(((vrc_CHRBank[3]<<10)+(addr&0x3FF))&vrc_chrROMand)];
	else if(addr < 0x1400)
		return vrc_chrROM[(((vrc_CHRBank[4]<<10)+(addr&0x3FF))&vrc_chrROMand)];
	else if(addr < 0x1800)
		return vrc_chrROM[(((vrc_CHRBank[5]<<10)+(addr&0x3FF))&vrc_chrROMand)];
	else if(addr < 0x1C00)
		return vrc_chrROM[(((vrc_CHRBank[6]<<10)+(addr&0x3FF))&vrc_chrROMand)];
	else // < 0x2000
		return vrc_chrROM[(((vrc_CHRBank[7]<<10)+(addr&0x3FF))&vrc_chrROMand)];
}

uint8_t m22_chrGet8(uint16_t addr)
{
	if(addr < 0x400)
		return vrc_chrROM[((((vrc_CHRBank[0]>>1)<<10)+(addr&0x3FF))&vrc_chrROMand)];
	else if(addr < 0x800)
		return vrc_chrROM[((((vrc_CHRBank[1]>>1)<<10)+(addr&0x3FF))&vrc_chrROMand)];
	else if(addr < 0xC00)
		return vrc_chrROM[((((vrc_CHRBank[2]>>1)<<10)+(addr&0x3FF))&vrc_chrROMand)];
	else if(addr < 0x1000)
		return vrc_chrROM[((((vrc_CHRBank[3]>>1)<<10)+(addr&0x3FF))&vrc_chrROMand)];
	else if(addr < 0x1400)
		return vrc_chrROM[((((vrc_CHRBank[4]>>1)<<10)+(addr&0x3FF))&vrc_chrROMand)];
	else if(addr < 0x1800)
		return vrc_chrROM[((((vrc_CHRBank[5]>>1)<<10)+(addr&0x3FF))&vrc_chrROMand)];
	else if(addr < 0x1C00)
		return vrc_chrROM[((((vrc_CHRBank[6]>>1)<<10)+(addr&0x3FF))&vrc_chrROMand)];
	else // < 0x2000
		return vrc_chrROM[((((vrc_CHRBank[7]>>1)<<10)+(addr&0x3FF))&vrc_chrROMand)];
}

void vrcchrSet8(uint16_t addr, uint8_t val)
{
	(void)addr;
	(void)val;
}

void vrcctrcycle()
{
	if(vrc_irqCurCtr == 0xFF)
	{
		if(vrc_irq_enabled)
		{
			//printf("VRC Cycle Interrupt\n");
			mapper_interrupt = true;
			vrc_irq_enabled = false;
		}
		vrc_irqCurCtr = vrc_irqCtr;
	}
	else
		vrc_irqCurCtr++;
}

void vrccycle()
{
	if(vrc_irq_cyclemode)
		vrcctrcycle();
	else
	{
		if(vrc_irqCurCycleCtr >= vrc_scanTbl[vrc_scanTblPos])
		{
			vrcctrcycle();
			vrc_scanTblPos++;
			if(vrc_scanTblPos >= 3)
				vrc_scanTblPos = 0;
			vrc_irqCurCycleCtr = 0;
		}
		else
			vrc_irqCurCycleCtr++;
	}
}
