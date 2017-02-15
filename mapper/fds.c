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
#include "../audio_fds.h"

bool fdsSwitch;

static uint8_t *fds_BIOS;
static uint8_t *fds_File;
static uint8_t *fds_prgRAM;
static uint32_t fds_BIOSsize;
static uint32_t fds_FileLoc;
static uint32_t fds_prgRAMsize;
static uint8_t fds_chrRAM[0x2000];
static bool fds_irq_enable;
static bool fds_transfer_irq_enable;
static bool fds_disk_ready;
static bool fds_transfer_done;
static bool fds_data_read;
static bool fds_cur_data_read;
static bool fds_has_disk_sideB;
static bool fds_disk_sideB;
static bool fds_crc_check;
static bool fds_disk_start;
static bool fds_disk_active;
static uint8_t fds_transfer_val;
static uint8_t fds_switch_delay;
static uint16_t fds_irq_timer;
static uint16_t fds_cur_irq_timer;
static uint16_t fds_transfer_timer;
static uint16_t fds_disk_position;
static uint32_t fds_disk_ready_timer;

extern bool fds_interrupt;
extern bool fds_transfer_interrupt;

static void fdsLoadDisk(bool req_side_b)
{
	if(req_side_b)
		fds_FileLoc = 0x10000;
	else
		fds_FileLoc = 0;
	fds_disk_sideB = req_side_b;
	fds_disk_ready = false;
	//give it 1 second to get ready
	fds_disk_ready_timer = 1789773;
}


void fdsinit(uint8_t *fdsBIOS, uint32_t fdsBIOSsize, uint8_t *fdsFile, bool fdsSideB, uint8_t *prgRAMin, uint32_t prgRAMsizeIn)
{
	fds_BIOS = fdsBIOS;
	fds_BIOSsize = fdsBIOSsize;
	fds_File = fdsFile;
	fds_FileLoc = 0;
	fds_has_disk_sideB = fdsSideB;
	fdsLoadDisk(false);
	fdsSwitch = false;
	//give it another 6 seconds to get ready
	fds_disk_ready_timer += 10738638;
	fds_prgRAM = prgRAMin;
	fds_prgRAMsize = prgRAMsizeIn;
	fds_irq_enable = false;
	fds_transfer_irq_enable = false;
	fds_transfer_done = false;
	fds_data_read = true;
	fds_disk_active = false;
	fds_crc_check = false;
	fds_disk_start = true;
	fds_cur_data_read = false;
	fds_transfer_val = 0;
	fds_switch_delay = 0;
	fds_irq_timer = 0;
	fds_cur_irq_timer = 0;
	fds_transfer_timer = 0;
	fds_disk_position = 0;
	fdsAudioInit();
	memset(fds_chrRAM, 0, 0x2000);
	printf("FDS Inited\n");
}

uint8_t fdsget8(uint16_t addr)
{
	//printf("fdsget8 %04x\n", addr);
	if(addr < 0x6000)
	{
		/* FDS Disk Regs */
		if(addr == 0x4030)
		{
			uint8_t ret = (fds_interrupt&1) | ((fds_transfer_done&1)<<1) | (((!fds_disk_active)&1)<<6) | 0x80;
			fds_transfer_done = false;
			fds_interrupt = false;
			return ret;
		}
		else if(addr == 0x4031)
		{
			//printf("Returning %02x\n", fds_transfer_val);
			return fds_transfer_val;
		}
		else if(addr == 0x4032)
		{
			return (((!fds_disk_ready)&1) | ((!fds_disk_active)&1)<<1) | 0x40;
		}
		else if(addr == 0x4033)
			return 0x80;
		/* FDS Audio Regs */
		if(addr >= 0x4040 && addr <= 0x407F)
			return fdsAudioGetWave(addr&0x3F);
		else if(addr == 0x4090 || addr == 0x4092)
			return fdsAudioGet8(addr&3);
		return 0;
	}
	else //if addr >= 0x6000
	{
		if(addr < 0xE000)
			return fds_prgRAM[addr-0x6000];
		return fds_BIOS[addr&0x1FFF];
	}
}

void fdsset8(uint16_t addr, uint8_t val)
{
	//printf("fdsset8 %04x %02x\n", addr, val);
	/* FDS Disk Regs */
	if(addr == 0x4020)
		fds_irq_timer = ((fds_irq_timer&~0xFF)|val);
	else if(addr == 0x4021)
		fds_irq_timer = ((fds_irq_timer&0xFF)|(val<<8));
	else if(addr == 0x4022)
	{
		fds_irq_enable = ((val&2) != 0);
		if(fds_irq_enable)
			fds_cur_irq_timer = fds_irq_timer;
		else
			fds_cur_irq_timer = 0;
	}
	else if(addr == 0x4024)
	{
		//fds_transfer_interrupt = false;
		fds_transfer_done = false;
		if(!fds_data_read)
		{
			//printf("Writing %02x\n", val);
			fds_transfer_val = val;
		}
		else
		{
			//printf("Writing nothing\n");
		}
	}
	else if(addr == 0x4025)
	{
		if(val&2)
		{
			//printf("Resetting disc position\n");
			fds_disk_position = 0;
			fds_disk_start = true;
			fds_crc_check = false;
			fds_disk_active = false;
			fds_transfer_timer = 0;
		}
		else
		{
			fds_disk_active = true;
			if((val&4) == 0)
			{
				if(fds_data_read != false)
				{
					//printf("Write mode\n");
					fds_data_read = false;
					fds_switch_delay = 2;
				}
			}
			else
			{
				if(fds_data_read != true)
				{
					//printf("Read mode\n");
					fds_data_read = true;
				}
			}
			if(val&0x40)
			{
				if(fds_transfer_timer == 0)
					fds_transfer_timer = 145;
			}
			else
				fds_transfer_timer = 0;
		}
		if((val&8) == 0)
		{
			//printf("Vertical mode\n");
			ppuSetNameTblVertical();
		}
		else
		{
			//printf("Horizontal mode\n");
			ppuSetNameTblHorizontal();
		}
		if(val&0x10 && !fds_data_read)
		{
			if(fds_crc_check != true)
			{
				//printf("In CRC Write check\n");
				fds_disk_position+=2;
			}
			fds_crc_check = true;
		}
		else
		{
			if(fds_crc_check != false)
			{
				//printf("Finished CRC Write check\n");
				fds_transfer_val = fds_File[fds_FileLoc+fds_disk_position];
			}
			fds_crc_check = false;
		}
		if(val&0x80)
			fds_transfer_irq_enable = true;
		else
		{
			fds_transfer_irq_enable = false;
			//fds_transfer_interrupt = false;
		}
	}
	/* FDS Audio Regs */
	if(addr >= 0x4040 && addr <= 0x407F)
		fdsAudioSetWave(addr&0x3F, val);
	else if(addr >= 0x4080 && addr <= 0x408A)
		fdsAudioSet8(addr&0x1F, val);
	/* FDS RAM */
	if(addr >= 0x6000 && addr < 0xE000)
		fds_prgRAM[addr-0x6000] = val;
}

uint8_t fdschrGet8(uint16_t addr)
{
	return fds_chrRAM[addr&0x1FFF];
}

void fdschrSet8(uint16_t addr, uint8_t val)
{
	fds_chrRAM[addr&0x1FFF] = val;
}

void fdscycle()
{
	fdsAudioClockTimers();
	if(fds_cur_irq_timer == 1)
	{
		if(fds_irq_enable)
			fds_interrupt = true;
		fds_cur_irq_timer = 0;
	}
	else if(fds_cur_irq_timer > 1)
		fds_cur_irq_timer--;
	if(fds_disk_ready_timer == 1)
	{
		fds_disk_ready = true;
		fds_disk_ready_timer = 0;
	}
	else if(fds_disk_ready_timer > 1)
		fds_disk_ready_timer--;

	if(fdsSwitch)
	{
		if(fds_has_disk_sideB)
			fdsLoadDisk(!fds_disk_sideB);
		fdsSwitch = false;
	}
	if(!fds_disk_active || fds_crc_check)
		return;

	if(fds_transfer_timer == 1)
	{
		if(fds_switch_delay == 0)
		{
			if(!fds_disk_start)
				fds_disk_position++;
			//printf("Transfer done, giving pos %04x\n", fds_disk_position);
			if(fds_data_read)
				fds_transfer_val = fds_File[fds_FileLoc+fds_disk_position];
			else
				fds_File[fds_FileLoc+fds_disk_position] = fds_transfer_val;
		}
		else if(fds_switch_delay > 0)
			fds_switch_delay--;
		fds_transfer_done = true;
		fds_transfer_timer = 145;
		fds_transfer_interrupt = true;
		fds_disk_start = false;
	}
	else if(fds_transfer_timer > 1)
		fds_transfer_timer--;
}
