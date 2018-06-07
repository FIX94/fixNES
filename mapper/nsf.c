/*
 * Copyright (C) 2017 FIX94
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#include <stdio.h>
#include <stdbool.h>
#include <inttypes.h>
#include <string.h>
#include "../ppu.h"
#include "../cpu.h"
#include "../input.h"
#include "../mem.h"
#include "../apu.h"
#include "../ppu.h"
#include "../audio_fds.h"
#include "../audio_vrc6.h"
#include "../audio_vrc7.h"
#include "../audio_mmc5.h"
#include "../audio_n163.h"
#include "../audio_s5b.h"

static uint8_t *nsf_prgROM;
static uint8_t *nsf_prgRAM;
static uint8_t nsf_FillRAM[0x8000];
static uint32_t nsf_prgROMsize;
static uint32_t nsf_prgRAMsize;
static uint32_t nsf_InitPRGBank[8];
static uint32_t nsf_InitRAMBank[2];
static uint32_t nsf_PRGBank[8];
static uint32_t nsf_RAMBank[2];
static uint16_t nsf_loadAddr;
static uint16_t nsf_initAddr;
static uint16_t nsf_playAddr;
static uint8_t nsf_trackTotal;
static uint8_t nsf_curTrack;
static bool nsf_bankEnable;
static bool nsf_playing;
static bool nsf_init;
static uint8_t nsf_vrc7_audioReg;
static uint8_t nsf_init_timeout;
static uint8_t nsf_chrRAM[0x2000];
static uint8_t nsf_MMC5ExRAM[0x400];
extern bool nesPAL;
extern uint8_t audioExpansion;
static uint8_t nsf_prevValReads[8];

//used externally
bool nsf_startPlayback;
bool nsf_endPlayback;

static void nsfInitPlayback()
{
	nsf_playing = false;
	nsf_startPlayback = false;
	nsf_endPlayback = false;
	nsf_init = true;
	nsf_vrc7_audioReg = 0;
	nsf_init_timeout = 10; //give it a couple frames
	memset(nsf_prgRAM, 0, nsf_prgRAMsize);
	memset(nsf_FillRAM, 0, 0x8000);
	memset(nsf_MMC5ExRAM, 0, 0x400);
	memcpy(nsf_PRGBank, nsf_InitPRGBank, 8*sizeof(uint32_t));
	memcpy(nsf_RAMBank, nsf_InitRAMBank, 2*sizeof(uint32_t));
	cpuInitNSF(nsf_initAddr, nsf_curTrack-1, nesPAL ? 1 : 0);
	if(audioExpansion & EXP_VRC6)
		vrc6AudioInit();
	if(audioExpansion & EXP_VRC7)
		vrc7AudioInit();
	if(audioExpansion & EXP_FDS)
		fdsAudioInit();
	if(audioExpansion & EXP_MMC5)
		mmc5AudioInit();
	if(audioExpansion & EXP_N163)
		n163AudioInit();
	if(audioExpansion & EXP_S5B)
		s5BAudioInit();
}

#define onOff(x) (x ? "On" : "Off")

void nsfinit(uint8_t *nsfBIN, uint32_t nsfBINsize, uint8_t *prgRAMin, uint32_t prgRAMsizeIn)
{
	nsf_prgROM = nsfBIN+0x80;
	nsf_prgROMsize = nsfBINsize-0x80;
	nsf_prgRAM = prgRAMin;
	nsf_prgRAMsize = prgRAMsizeIn;
	nsf_loadAddr = ((nsfBIN[0x8])|(nsfBIN[0x9]<<8))&0x7FFF;
	nsf_initAddr = ((nsfBIN[0xA])|(nsfBIN[0xB]<<8));
	nsf_playAddr = ((nsfBIN[0xC])|(nsfBIN[0xD]<<8));
	nesPAL = ((nsfBIN[0x7A]&1) != 0);
	apuInitBufs(); //audioExpansion=0
	//the inits will set audioExpansion
	if(nsfBIN[0x7B] & EXP_VRC6)
		vrc6AudioInit();
	if(nsfBIN[0x7B] & EXP_VRC7)
		vrc7AudioInit();
	if(nsfBIN[0x7B] & EXP_FDS)
		fdsAudioInit();
	if(nsfBIN[0x7B] & EXP_MMC5)
		mmc5AudioInit();
	if(nsfBIN[0x7B] & EXP_N163)
		n163AudioInit();
	if(nsfBIN[0x7B] & EXP_S5B)
		s5BAudioInit();
	nsf_bankEnable = false;
	uint8_t i;
	for(i = 0; i < 8; i++)
	{
		nsf_InitPRGBank[i] = (nsfBIN[0x70+i]<<12);
		if(i == 6) nsf_InitRAMBank[0] = nsf_InitPRGBank[i];
		if(i == 7) nsf_InitRAMBank[1] = nsf_InitPRGBank[i];
		if(nsf_InitPRGBank[i] != 0)
			nsf_bankEnable = true;
	}
	if(nsf_bankEnable) nsf_loadAddr &= 0xFFF;
	nsf_curTrack = 1;
	nsf_trackTotal = nsfBIN[6];
	memset(nsf_prevValReads, 0, 8);
	printf("NSF Player inited in %s Mode (VRC6 %s, VRC7 %s, FDS %s, MMC5 %s, N163 %s, S5B %s) %s banking\n", nesPAL ? "PAL" : "NTSC", 
		onOff(audioExpansion&EXP_VRC6), onOff(audioExpansion&EXP_VRC7), onOff(audioExpansion&EXP_FDS), onOff(audioExpansion&EXP_MMC5),
		onOff(audioExpansion&EXP_N163), onOff(audioExpansion&EXP_S5B), nsf_bankEnable ? "with" : "without");
	if(nsfBIN[0xE] != 0) printf("Playing back %.32s\n", nsfBIN+0xE);
	//printf("Track %i/%i         ", nsf_curTrack, nsf_trackTotal);
	ppuDrawNSFTrackNum(nsf_curTrack, nsf_trackTotal);
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

uint8_t nsfget8(uint16_t addr, uint8_t val)
{
	uint8_t aExp = audioExpansion;
	//printf("nsfget8 %04x\n", addr);
	if(addr < 0x6000)
	{
		/* FDS Audio Regs */
		if(aExp&EXP_FDS)
		{
			if(addr >= 0x4040 && addr <= 0x407F)
				return fdsAudioGetWave(addr&0x3F);
			else if(addr == 0x4090 || addr == 0x4092)
				return fdsAudioGet8(addr&3);
		}
		/* N163 Regs */
		if(aExp&EXP_N163)
		{
			if(addr >= 0x4800 && addr < 0x5000)
				return n163AudioGet8(addr, val);
		}
		/* MMC5 Regs */
		if(aExp&EXP_MMC5)
		{
			if(addr == 0x5015)
				return mmc5AudioGet8(0x15);
			else if(addr == 0x5205)
				return (nsf_mmc5_mulRes&0xFF);
			else if(addr == 0x5206)
				return (nsf_mmc5_mulRes>>8);
			else if(addr >= 0x5C00 && addr < 0x5FF6)
				return nsf_MMC5ExRAM[addr&0x3FF];
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
			return 0x6A; //low addr, 0x6A
		else if(addr == 0x456C)
		{
			//if(nsf_playing)
			//	printf("Play return\n");
			//will end on next CPU_GET_INSTRUCTION state
			nsf_endPlayback = true;
			nsf_playing = false;
			return 0x45; //high addr, 0x456A
		}
	}
	else 
	{
		if(addr < 0x8000 && (!(aExp&EXP_FDS) || !nsf_bankEnable))
			val = nsf_prgRAM[addr&0x1FFF];
		else
		{
			uint32_t romAddr = nsfgetromAddr(addr);
			if(romAddr >= nsf_loadAddr && (romAddr-nsf_loadAddr) < nsf_prgROMsize)
			{
				val = nsf_prgROM[romAddr-nsf_loadAddr];
				//printf("Ret from ROM %04x with %02x\n", romAddr-nsf_loadAddr, val);
				if(addr < 0xE000 && (aExp&EXP_FDS))
					nsf_FillRAM[addr-0x6000] = val;
			}
			else if(addr < 0xE000 && (aExp&EXP_FDS))
				val = nsf_FillRAM[addr-0x6000];
			else //ROM data not available so return 0
				val = 0;
			if((aExp&EXP_MMC5) && addr >= 0x8000 && addr < 0xC000 && mmc5_dmcreadmode)
				mmc5AudioPCMWrite(val);
		}
	}
	return val;
}

void nsfset8(uint16_t addr, uint8_t val)
{
	uint8_t aExp = audioExpansion;
	//printf("nsfset8 %04x %02x\n", addr, val);
	if(addr < 0x6000)
	{
		/* FDS Audio Regs */
		if(aExp&EXP_FDS)
		{
			if(addr >= 0x4040 && addr <= 0x407F)
				fdsAudioSetWave(addr&0x3F, val);
			else if(addr >= 0x4080 && addr <= 0x408A)
				fdsAudioSet8(addr&0x1F, val);
		}
		/* N163 Regs */
		if(aExp&EXP_N163)
		{
			if(addr >= 0x4800 && addr < 0x5000)
				n163AudioSet8(addr, val);
		}
		/* MMC5 Regs */
		if(aExp&EXP_MMC5)
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
			else if(addr >= 0x5C00 && addr < 0x5FF6)
				nsf_MMC5ExRAM[addr&0x3FF] = val;
		}
		/* NSF Bank Regs */
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
		if(addr < 0x8000 && (!(aExp&EXP_FDS) || !nsf_bankEnable))
			nsf_prgRAM[addr&0x1FFF] = val;
		else if(addr < 0xE000 && (aExp&EXP_FDS))
			nsf_FillRAM[addr-0x6000] = val;
		else if((aExp&EXP_N163) && addr >= 0xF800)
			n163AudioSet8(addr, val);
		else if((aExp&EXP_S5B) && addr >= 0xC000)
			s5BAudioSet8(addr, val);
		else if((aExp&EXP_VRC6) && ((addr >= 0x9000 && addr <= 0x9003) ||
											(addr >= 0xA000 && addr <= 0xA002) ||
											(addr >= 0xB000 && addr <= 0xB002)))
			vrc6AudioSet8(addr, val);
		else if(aExp&EXP_VRC7)
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
	if(audioExpansion&EXP_VRC6)
		vrc6AudioClockTimers();
	if(audioExpansion&EXP_FDS)
		fdsAudioClockTimers();
	if(audioExpansion&EXP_MMC5)
		mmc5AudioClockTimers();
	if(audioExpansion&EXP_N163)
		n163AudioClockTimers();
	if(audioExpansion&EXP_S5B)
		s5BAudioClockTimers();

	if(inValReads[BUTTON_RIGHT] && !nsf_prevValReads[BUTTON_RIGHT])
	{
		nsf_prevValReads[BUTTON_RIGHT] = inValReads[BUTTON_RIGHT];
		nsf_curTrack++;
		if(nsf_curTrack > nsf_trackTotal)
			nsf_curTrack = 1;
		//printf("\rTrack %i/%i         ", nsf_curTrack, nsf_trackTotal);
		ppuDrawNSFTrackNum(nsf_curTrack, nsf_trackTotal);
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
		//printf("\rTrack %i/%i         ", nsf_curTrack, nsf_trackTotal);
		ppuDrawNSFTrackNum(nsf_curTrack, nsf_trackTotal);
		nsfInitPlayback();
	}
	else if(!inValReads[BUTTON_LEFT])
		nsf_prevValReads[BUTTON_LEFT] = 0;
}

void nsfVsync()
{
	if(nsf_playing)
		return;

	//wait for init return
	if(nsf_init_timeout)
	{
		nsf_init_timeout--;
		return;
	}
	//will get started on next CPU_GET_INSTRUCTION state
	nsf_startPlayback = true;
	nsf_playing = true;
}

uint16_t nsfGetPlayAddr()
{
	return nsf_playAddr;
}
