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
#include "../apu.h"
#include "../audio_fds.h"
#include "../audio_mmc5.h"
#include "../audio_vrc6.h"
#include "../audio_vrc7.h"

static uint8_t *nsf_prgROM;
static uint8_t *nsf_prgRAM;
static uint8_t nsf_FillRAM[0x8000];
static uint32_t nsf_prgROMsize;
static uint32_t nsf_prgRAMsize;
static uint32_t nsf_PRGBank[8];
static uint32_t nsf_RAMBank[2];
static uint16_t nsf_loadAddr;
static uint16_t nsf_initAddr;
static uint16_t nsf_playAddr;
static uint16_t nsf_retAddr;
static uint8_t nsf_trackTotal;
static uint8_t nsf_curTrack;
static bool nsf_bankEnable;
static bool nsf_playing;
static bool nsf_init;
static uint8_t nsf_vrc7_audioReg;
static uint8_t nsf_init_timeout;
static uint8_t nsf_chrRAM[0x2000];
extern bool nesPAL;
static uint8_t nsf_prevValReads[8];

static void nsfInitPlayback()
{
	nsf_playing = false;
	nsf_init = true;
	nsf_vrc7_audioReg = 0;
	nsf_init_timeout = 10; //give it a couple frames
	memset(nsf_prgRAM, 0, nsf_prgRAMsize);
	memset(nsf_FillRAM, 0, 0x8000);
	cpuInitNSF(nsf_initAddr, nsf_curTrack-1, nesPAL ? 1 : 0);
	if(vrc6enabled)
		vrc6AudioInit();
	if(mmc5enabled)
		mmc5AudioInit();
	if(fdsEnabled)
		fdsAudioInit();
}

#define onOff(x) (x ? "On" : "Off")

void nsfinit(uint8_t *nsfBIN, uint32_t nsfBINsize, uint8_t *prgRAMin, uint32_t prgRAMsizeIn)
{
	nsf_prgROM = nsfBIN+0x80;
	nsf_prgROMsize = nsfBINsize-0x80;
	nsf_prgRAM = prgRAMin;
	nsf_prgRAMsize = prgRAMsizeIn;
	nsf_loadAddr = (*(uint16_t*)(nsfBIN+0x8))&0x7FFF;
	nsf_initAddr = *(uint16_t*)(nsfBIN+0xA);
	nsf_playAddr = *(uint16_t*)(nsfBIN+0xC);
	nsf_retAddr = 0x456A;
	nesPAL = ((nsfBIN[0x7A]&1) != 0);
	apuInitBufs();
	if((nsfBIN[0x7B]&1) != 0)
		vrc6AudioInit();
	if((nsfBIN[0x7B]&2) != 0)
		vrc7AudioInit();
	if((nsfBIN[0x7B]&4) != 0)
		fdsAudioInit();
	if((nsfBIN[0x7B]&8) != 0)
		mmc5AudioInit();
	nsf_bankEnable = false;
	int i;
	for(i = 0; i < 8; i++)
	{
		nsf_PRGBank[i] = (nsfBIN[0x70+i]<<12);
		if(i == 6) nsf_RAMBank[0] = nsf_PRGBank[i];
		if(i == 7) nsf_RAMBank[1] = nsf_PRGBank[i];
		if(nsf_PRGBank[i] != 0)
			nsf_bankEnable = true;
	}
	if(nsf_bankEnable) nsf_loadAddr &= 0xFFF;
	nsf_curTrack = 1;
	nsf_trackTotal = nsfBIN[6];
	memset(nsf_prevValReads, 0, 8);
	printf("NSF Player inited in %s Mode (VRC6 %s, VRC7 %s, FDS %s, MMC5 %s) %s banking\n", nesPAL ? "PAL" : "NTSC", 
		onOff(vrc6enabled), onOff(vrc7enabled), onOff(fdsEnabled), onOff(mmc5enabled), nsf_bankEnable ? "with" : "without");
	if(nsfBIN[0xE] != 0) printf("Playing back %.32s\n", nsfBIN+0xE);
	printf("Track %i/%i         ", nsf_curTrack, nsf_trackTotal);
	inputInit();
	nsfInitPlayback();
}

static uint32_t nsfgetromAddr(uint16_t addr)
{
	uint32_t romAddr;
	if(nsf_bankEnable)
	{
		switch(addr>>12)
		{
			case 0x6:
				romAddr = nsf_RAMBank[0]+(addr&0xFFF);
				break;
			case 0x7:
				romAddr = nsf_RAMBank[1]+(addr&0xFFF);
				break;
			case 0x8:
				romAddr = nsf_PRGBank[0]+(addr&0xFFF);
				break;
			case 0x9:
				romAddr = nsf_PRGBank[1]+(addr&0xFFF);
				break;
			case 0xA:
				romAddr = nsf_PRGBank[2]+(addr&0xFFF);
				break;
			case 0xB:
				romAddr = nsf_PRGBank[3]+(addr&0xFFF);
				break;
			case 0xC:
				romAddr = nsf_PRGBank[4]+(addr&0xFFF);
				break;
			case 0xD:
				romAddr = nsf_PRGBank[5]+(addr&0xFFF);
				break;
			case 0xE:
				romAddr = nsf_PRGBank[6]+(addr&0xFFF);
				break;
			default: //0xF
				romAddr = nsf_PRGBank[7]+(addr&0xFFF);
				break;
		}
	}
	else
		romAddr = (addr&0x7FFF);
	return romAddr;
}

static uint8_t nsf_mmc5_mul1 = 0, nsf_mmc5_mul2 = 0;
static uint16_t nsf_mmc5_mulRes = 0;

uint8_t nsfget8(uint16_t addr)
{
	//printf("nsfget8 %04x\n", addr);
	if(addr < 0x6000)
	{
		/* FDS Audio Regs */
		if(fdsEnabled)
		{
			if(addr >= 0x4040 && addr <= 0x407F)
				return fdsAudioGetWave(addr&0x3F);
			else if(addr == 0x4090 || addr == 0x4092)
				return fdsAudioGet8(addr&3);
		}
		/* MMC5 Regs */
		if(mmc5enabled)
		{
			if(addr == 0x5015)
				return mmc5AudioGet8(0x15);
			else if(addr == 0x5205)
				return (nsf_mmc5_mulRes&0xFF);
			else if(addr == 0x5206)
				return (nsf_mmc5_mulRes>>8);
		}
		/* Init Loop Routine */
		if(addr == 0x4567)
			return 0x4C; //JMP Absolute
		else if(addr == 0x4568)
			return 0x67; //low addr, 0x67
		else if(addr == 0x4569)
		{
			//if(nsf_init)
			//	printf("Init return\n");
			nsf_init = false;
			return 0x45; //high addr, 0x4567
		}
		/* Play Return Routine */
		if(addr == 0x456A)
			return 0x4C; //JMP Absolute
		else if(addr == 0x456B)
			return (nsf_retAddr&0xFF); //low addr
		else if(addr == 0x456C)
		{
			//if(nsf_playing)
			//	printf("Play return\n");
			nsf_playing = false;
			return (nsf_retAddr>>8); //high addr
		}
		return 0;
	}
	else 
	{
		if(addr < 0x8000 && (!fdsEnabled || !nsf_bankEnable))
			return nsf_prgRAM[addr&0x1FFF];
		uint32_t romAddr = nsfgetromAddr(addr);
		if(romAddr >= nsf_loadAddr && (romAddr-nsf_loadAddr) < nsf_prgROMsize)
		{
			uint8_t ret = nsf_prgROM[romAddr-nsf_loadAddr];
			//printf("Ret from ROM %04x with %02x\n", romAddr-nsf_loadAddr, ret);
			if(addr < 0xE000 && fdsEnabled)
				nsf_FillRAM[addr-0x6000] = ret;
			return ret;
		}
		else if(addr < 0xE000 && fdsEnabled)
			return nsf_FillRAM[addr-0x6000];
		else
			return 0;
	}
}

void nsfset8(uint16_t addr, uint8_t val)
{
	//printf("nsfset8 %04x %02x\n", addr, val);
	/* FDS Audio Regs */
	if(fdsEnabled)
	{
		if(addr >= 0x4040 && addr <= 0x407F)
			fdsAudioSetWave(addr&0x3F, val);
		else if(addr >= 0x4080 && addr <= 0x408A)
			fdsAudioSet8(addr&0x1F, val);
	}
	/* MMC5 Regs */
	if(mmc5enabled)
	{
		if(addr >= 0x5000 && addr <= 0x5015)
			mmc5AudioSet8(addr&0x1F, val);
		else if(addr == 0x5205)
		{
			nsf_mmc5_mul1 = val;
			nsf_mmc5_mulRes = nsf_mmc5_mul1*nsf_mmc5_mul2;
		}
		else if(addr == 0x5206)
		{
			nsf_mmc5_mul2 = val;
			nsf_mmc5_mulRes = nsf_mmc5_mul1*nsf_mmc5_mul2;
		}
	}
	if(addr >= 0x5FF6 && addr < 0x6000)
	{
		if(addr == 0x5FF6)
			nsf_RAMBank[0] = val<<12;
		else if(addr == 0x5FF7)
			nsf_RAMBank[1] = val<<12;
		else if(addr == 0x5FF8)
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
	else
	{
		if(addr < 0x8000 && (!fdsEnabled || !nsf_bankEnable))
			nsf_prgRAM[addr&0x1FFF] = val;
		else if(addr < 0xE000 && fdsEnabled)
			nsf_FillRAM[addr-0x6000] = val;
		else if(vrc6enabled && ((addr >= 0x9000 && addr <= 0x9003) ||
								(addr >= 0xA000 && addr <= 0xA002) ||
								(addr >= 0xB000 && addr <= 0xB002)))
			vrc6AudioSet8(addr, val);
		else if(vrc7enabled)
		{
			if(addr == 0x9010)
				nsf_vrc7_audioReg = (val&0x3F);
			else if(addr == 0x9030)
				vrc7AudioSet8(nsf_vrc7_audioReg, val);
		}
	}
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
	if(vrc6enabled)
		vrc6AudioClockTimers();
	if(fdsEnabled)
		fdsAudioClockTimers();
	if(mmc5enabled)
		mmc5AudioClockTimers();

	if(inValReads[BUTTON_RIGHT] && !nsf_prevValReads[BUTTON_RIGHT])
	{
		nsf_prevValReads[BUTTON_RIGHT] = inValReads[BUTTON_RIGHT];
		nsf_curTrack++;
		if(nsf_curTrack > nsf_trackTotal)
			nsf_curTrack = 1;
		printf("\rTrack %i/%i         ", nsf_curTrack, nsf_trackTotal);
		nsfInitPlayback();
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
		nsfInitPlayback();
	}
	else if(!inValReads[BUTTON_LEFT])
		nsf_prevValReads[BUTTON_LEFT] = 0;

	if(nsf_playing)
		return;

	if(ppuDrawDone())
	{
		//wait for init return
		if(nsf_init_timeout)
		{
			nsf_init_timeout--;
			return;
		}
		//init finished/timeout or next playback frame
		uint16_t lastAddr = cpuPlayNSF(nsf_playAddr);
		//still in init, make sure to jump back to it after play
		if(nsf_init) //ret used after play RTS
			nsf_retAddr = lastAddr;
		else //regular play jump loop
			nsf_retAddr = 0x456A;
		nsf_playing = true;
	}
}
