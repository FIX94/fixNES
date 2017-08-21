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
#include "mapper.h"
#include "ppu.h"

//sprite byte 2
#define PPU_SPRITE_PRIO (1<<5)
#define PPU_SPRITE_FLIP_X (1<<6)
#define PPU_SPRITE_FLIP_Y (1<<7)

//2000
#define PPU_INC_AMOUNT (1<<2)
#define PPU_SPRITE_ADDR (1<<3)
#define PPU_BACKGROUND_ADDR (1<<4)
#define PPU_SPRITE_8_16 (1<<5)
#define PPU_FLAG_NMI (1<<7)

//2001
#define PPU_GRAY (1<<0)
#define PPU_BG_8PX (1<<1)
#define PPU_SPRITE_8PX (1<<2)
#define PPU_BG_ENABLE (1<<3)
#define PPU_SPRITE_ENABLE (1<<4)

//2002
#define PPU_FLAG_OVERFLOW (1<<5)
#define PPU_FLAG_SPRITEZERO (1<<6)
#define PPU_FLAG_VBLANK (1<<7)

#define DOTS 341

#define VISIBLE_DOTS 256
#define VISIBLE_LINES 240

#define PPU_VRAM_HORIZONTAL_MASK 0x41F
#define PPU_VRAM_VERTICAL_MASK (~PPU_VRAM_HORIZONTAL_MASK)

#define PPU_DEBUG_ULTRA 0

#define PPU_DEBUG_VSYNC 0

//set or used externally
bool ppu4Screen = false;
bool ppu816Sprite = false;
bool ppuInFrame = false;
bool ppuScanlineDone = false;
uint8_t ppuDrawnXTile = 0;

//from main.c
extern uint32_t textureImage[0xF000];
extern bool nesPause;
extern bool ppuDebugPauseFrame;
extern bool doOverscan;

static uint8_t ppuDoBackground(uint8_t color, uint8_t dot);
static uint8_t ppuDoSprites(uint8_t color, uint8_t dot);

// BMF Final 2
static const uint8_t PPU_Pal[192] =
{
    0x52, 0x52, 0x52, 0x00, 0x00, 0x80, 0x08, 0x00, 0x8A, 0x2C, 0x00, 0x7E, 0x4A, 0x00, 0x4E, 0x50, 0x00, 0x06, 0x44, 0x00, 0x00, 0x26, 0x08, 0x00, 
    0x0A, 0x20, 0x00, 0x00, 0x2E, 0x00, 0x00, 0x32, 0x00, 0x00, 0x26, 0x0A, 0x00, 0x1C, 0x48, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0xA4, 0xA4, 0xA4, 0x00, 0x38, 0xCE, 0x34, 0x16, 0xEC, 0x5E, 0x04, 0xDC, 0x8C, 0x00, 0xB0, 0x9A, 0x00, 0x4C, 0x90, 0x18, 0x00, 0x70, 0x36, 0x00, 
    0x4C, 0x54, 0x00, 0x0E, 0x6C, 0x00, 0x00, 0x74, 0x00, 0x00, 0x6C, 0x2C, 0x00, 0x5E, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0xFF, 0xFF, 0xFF, 0x4C, 0x9C, 0xFF, 0x7C, 0x78, 0xFF, 0xA6, 0x64, 0xFF, 0xDA, 0x5A, 0xFF, 0xF0, 0x54, 0xC0, 0xF0, 0x6A, 0x56, 0xD6, 0x86, 0x10, 
    0xBA, 0xA4, 0x00, 0x76, 0xC0, 0x00, 0x46, 0xCC, 0x1A, 0x2E, 0xC8, 0x66, 0x34, 0xC2, 0xBE, 0x3A, 0x3A, 0x3A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0xFF, 0xFF, 0xFF, 0xB6, 0xDA, 0xFF, 0xC8, 0xCA, 0xFF, 0xDA, 0xC2, 0xFF, 0xF0, 0xBE, 0xFF, 0xFC, 0xBC, 0xEE, 0xFA, 0xC2, 0xC0, 0xF2, 0xCC, 0xA2, 
    0xE6, 0xDA, 0x92, 0xCC, 0xE6, 0x8E, 0xB8, 0xEE, 0xA2, 0xAE, 0xEA, 0xBE, 0xAE, 0xE8, 0xE2, 0xB0, 0xB0, 0xB0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
} ;

static uint8_t PPU_Reg[8];
static uint8_t PPU_VRAM[0x1000];

static uint8_t PPU_OAM[0x100];
static uint8_t PPU_OAM2[0x20];

static uint8_t PPU_PALRAM[0x20];
static uint32_t PPU_BGRLUT[0x200];
//internally processed, ready to draw
static uint8_t PPU_Sprites[0x20];

static uint16_t ppuNameTbl[4] = { 0, 0, 0, 0 };

//static uint32_t ppuCycles;
static uint16_t curLine;
static uint16_t ppuLinesTotal;
static uint16_t ppuPreRenderLine;
static uint16_t curDot;
static uint16_t ppuVramAddr;
static uint16_t ppuTmpVramAddr;
static uint8_t ppuToDraw;
static uint8_t ppuVramReadBuf;
static uint8_t ppuOAMpos;
static uint8_t ppuOAM2pos;
static uint8_t ppuFineXScroll;
static uint8_t ppuSpriteTilePos;
static uint16_t ppuBGValA;
static uint16_t ppuBGValB;
static uint16_t ppuBGRegA;
static uint16_t ppuBGRegB;
static uint8_t ppuBGAttribValA;
static uint8_t ppuBGAttribValB;
static uint8_t ppuBGAttribRegA;
static uint8_t ppuBGAttribRegB;
static uint8_t ppuCurOverflowAdd;
static uint8_t ppulastVal;
static bool ppuHasZSprite;
static bool ppuNextHasZSprite;
static bool ppuFrameDone;
static bool ppuTmpWrite;
static bool ppuNMITriggered;
static bool ppuSpriteSearch;
static bool ppuCurVBlankStat;
static bool ppuCurNMIStat;
static bool ppuOddFrame;
static bool ppuReadReg2;

extern bool nesPAL;

void ppuInit()
{
	memset(PPU_Reg,0,8);
	memset(PPU_VRAM,0,0x1000);
	memset(PPU_OAM,0,0x100);
	memset(PPU_PALRAM,0,0x20);
	memset(PPU_OAM2,0xFF,0x20);
	memset(PPU_Sprites,0xFF,0x20);
	//ppuCycles = 0;
	//start out being in vblank
	PPU_Reg[2] |= PPU_FLAG_VBLANK;
	ppuLinesTotal = nesPAL ? 312 : 262;
	ppuPreRenderLine = ppuLinesTotal - 1;
	curLine = ppuLinesTotal - 11;
	curDot = 0;
	ppuVramAddr = 0;
	ppuToDraw = 0;
	ppuVramReadBuf = 0;
	ppuOAMpos = 0;
	ppuOAM2pos = 0;
	ppuFineXScroll = 0;
	ppuSpriteTilePos = 0;
	ppuBGRegA = 0;
	ppuBGRegB = 0;
	ppuBGAttribRegA = 0;
	ppuBGAttribRegB = 0;
	ppuCurOverflowAdd = 0;
	ppulastVal = 0;
	ppuHasZSprite = false;
	ppuNextHasZSprite = false;
	ppuFrameDone = false;
	ppuTmpWrite = false;
	ppuNMITriggered = false;
	ppuSpriteSearch = false;
	ppuCurVBlankStat = false;
	ppuCurNMIStat = false;
	ppuOddFrame = false;
	ppuReadReg2 = false;
	//generate full BGR LUT
	uint8_t rtint = nesPAL ? (1<<6) : (1<<5);
	uint8_t gtint = nesPAL ? (1<<5) : (1<<6);
	uint8_t btint = (1<<7);
	int32_t i;
	for(i = 0; i < 0x200; i++)
	{
		uint8_t palpos = (i&0x3F)*3;
		uint8_t r = PPU_Pal[palpos];
		uint8_t g = PPU_Pal[palpos+1];
		uint8_t b = PPU_Pal[palpos+2];
		if((i & 0xF) <= 0xD)
		{
			//reduce red
			if((i>>1) & btint) r = (uint8_t)(((float)r)*0.75f);
			if((i>>1) & gtint) r = (uint8_t)(((float)r)*0.75f);
			//reduce green
			if((i>>1) & rtint) g = (uint8_t)(((float)g)*0.75f);
			if((i>>1) & btint) g = (uint8_t)(((float)g)*0.75f);
			//reduce blue
			if((i>>1) & rtint) b = (uint8_t)(((float)b)*0.75f);
			if((i>>1) & gtint) b = (uint8_t)(((float)b)*0.75f);
		}
		//save new color into LUT
		PPU_BGRLUT[i] = 
			(b) //Blue
			| (g<<8) //Green
			| (r<<16) //Red
			| (0xFF<<24); //Alpha
	}
}

extern uint8_t m5_exMode;
extern uint8_t m5exGetAttrib(uint16_t addr);
static inline uint16_t ppuGetVramTbl(uint16_t tblStart)
{
	return ppuNameTbl[(tblStart>>10)&3];
}
extern bool nesEmuNSFPlayback;
static uint8_t ppuSprite0hit = 0;
bool ppuCycle()
{
	if(nesEmuNSFPlayback)
		goto ppuIncreasePos;
	ppuCurVBlankStat = !!(PPU_Reg[2] & PPU_FLAG_VBLANK);
	ppuCurNMIStat = !!(PPU_Reg[0] & PPU_FLAG_NMI);

	bool pictureOutput = (PPU_Reg[1] & (PPU_BG_ENABLE | PPU_SPRITE_ENABLE)) != 0;

	/* For MMC5 Scanline Detect */
	if(curDot == 0)
	{
		if((curLine <= VISIBLE_LINES) && pictureOutput)
		{
			ppuInFrame = true;
			ppuScanlineDone = true;
		}
		else
			ppuInFrame = false;
	}
	/* Do Background Updates */
	if((curLine == ppuPreRenderLine || curLine < VISIBLE_LINES))
	{
		/* Do BG Reg Updates every 8 dots */
		if((curDot & 7) == 0 && (curDot >= 319 || (curDot && curDot <= VISIBLE_DOTS)))
		{
			/* MMC5 Scroll Related */
			if(curDot == 320)
				ppuDrawnXTile = 0;
			else
				ppuDrawnXTile++;
			if(m5_exMode == 1)
			{
				/* MMC5 Ex Mode 1 has different Attribute for every Tile */
				uint8_t cPalByte = m5exGetAttrib(ppuVramAddr);
				ppuBGAttribValA = (cPalByte)&1;
				ppuBGAttribValB = (cPalByte>>1)&1;
			}
			else
			{
				uint16_t cPpuTbl = ppuGetVramTbl(ppuVramAddr);
				/* Select new BG Background Attribute */
				uint8_t cAttrib = ((ppuVramAddr>>4)&0x38) | ((ppuVramAddr>>2)&7);
				uint16_t attributeAddr = cPpuTbl | (0x3C0 | cAttrib);
				uint8_t cPalByte = mapperVramGet8(attributeAddr);
				bool left = ((ppuVramAddr&2) == 0);
				//top tiles
				if((ppuVramAddr&0x40) == 0)
				{
					if(left)
					{
						ppuBGAttribValA = (cPalByte)&1;
						ppuBGAttribValB = (cPalByte>>1)&1;
					}
					else
					{
						ppuBGAttribValA = (cPalByte>>2)&1;
						ppuBGAttribValB = (cPalByte>>3)&1;
					}
				}
				else //bottom tiles
				{
					if(left)
					{
						ppuBGAttribValA = (cPalByte>>4)&1;
						ppuBGAttribValB = (cPalByte>>5)&1;
					}
					else
					{
						ppuBGAttribValA = (cPalByte>>6)&1;
						ppuBGAttribValB = (cPalByte>>7)&1;
					}
				}
			}
			if(curDot == 328)
			{
				uint8_t i;
				for(i = 0; i < 8; i++)
				{
					ppuBGAttribRegA <<= 1;
					ppuBGAttribRegB <<= 1;
					ppuBGAttribRegA |= ppuBGAttribValA;
					ppuBGAttribRegB |= ppuBGAttribValB;
				}
			}
		}
		if(pictureOutput)
		{
			/* Update tile address every 8 dots */
			if((curDot & 7) == 0 && (curDot >= 328 || (curDot >= 8 && curDot <= VISIBLE_DOTS)))
			{
				if((ppuVramAddr & 0x1F) == 0x1F)
				{
					ppuVramAddr &= ~0x1F;
					ppuVramAddr ^= 0x400;
				}
				else
					ppuVramAddr++;
			}
			/* update Y position for writes */
			if(curDot == VISIBLE_DOTS)
			{
				if(((ppuVramAddr>>12)&7) != 7)
					ppuVramAddr += (1<<12);
				else
				{
					ppuVramAddr &= ~0x7000;
					uint8_t coarseY = (ppuVramAddr&0x3E0)>>5;
					if(coarseY == 29)
					{
						coarseY = 0;
						ppuVramAddr ^= 0x800;
					}
					else if(coarseY == 31)
						coarseY = 0;
					else
						coarseY++;
					ppuVramAddr &= ~0x3E0;
					ppuVramAddr |= (coarseY<<5);
				}
			} /* Update horizontal values after scanline */
			else if(curDot == VISIBLE_DOTS+1)
			{
				ppuVramAddr &= ~PPU_VRAM_HORIZONTAL_MASK;
				ppuVramAddr |= (ppuTmpVramAddr & PPU_VRAM_HORIZONTAL_MASK);
			}
		}
		/* Do BG Reg Updates every 8 dots */
		if((curDot & 7) == 4 && (curDot >= 319 || curDot <= VISIBLE_DOTS))
		{
			uint16_t cPpuTbl = ppuGetVramTbl(ppuVramAddr);
			/* Select new BG Tiles */
			uint16_t chrROMBG = (PPU_Reg[0] & PPU_BACKGROUND_ADDR) ? 0x1000 : 0;
			uint16_t workAddr = cPpuTbl | (ppuVramAddr & 0x3FF);
			uint8_t curBGtileReg = mapperVramGet8(workAddr);
			uint8_t curTileY = (ppuVramAddr>>12)&7;
			uint16_t curBGTile = chrROMBG+(curBGtileReg<<4)+curTileY;
			mapperChrMode = 0;
			ppuBGValA = mapperChrGet8(curBGTile);
			ppuBGValB = mapperChrGet8(curBGTile+8);
		}
		if((curDot&7) == 0 && curDot >= 8 && curDot <= VISIBLE_DOTS)
		{
			ppuBGRegA |= ppuBGValA;
			ppuBGRegB |= ppuBGValB;
		}
		else if(curDot == 328)
		{
			ppuBGRegA = ppuBGValA<<8;
			ppuBGRegB = ppuBGValB<<8;
		}
		else if(curDot == 336)
		{
			ppuBGRegA |= ppuBGValA;
			ppuBGRegB |= ppuBGValB;
		}
	}

	/* Only render visible dots after pre-render line and before post-render line */
	if(curDot < VISIBLE_DOTS && curLine < VISIBLE_LINES)
	{
		/* Grab color to render from BG and Sprites */
		uint8_t curCol = ppuDoBackground(0, curDot);
		ppuBGRegA <<= 1;
		ppuBGRegB <<= 1;
		ppuBGAttribRegA <<= 1;
		ppuBGAttribRegB <<= 1;
		ppuBGAttribRegA |= ppuBGAttribValA;
		ppuBGAttribRegB |= ppuBGAttribValB;
		curCol = ppuDoSprites(curCol, curDot);
		if((curCol&3) == 0) //0,4,8 and C wrap
			curCol &= ~0x10; //down to 0x00
		/* make sure to to clip top and bottom if requested */
		if(doOverscan && (curLine < 8 || curLine >= 232))
			curCol = 0xFF;
		/* If left is clipped, also clip right */
		if((!(PPU_Reg[1] & PPU_BG_8PX) || !(PPU_Reg[1] & PPU_SPRITE_8PX)) && (curDot < 8 || curDot > 248))
			curCol = 0xFF;
		/* Draw current dot on screen */
		size_t drawPos = (curDot)+(curLine<<8);
		if(curCol != 0xFF)
		{
			uint16_t cPalIdx;
			if(pictureOutput) //use color from bg or sprite input
				cPalIdx = (PPU_PALRAM[curCol&0x1F]&((PPU_Reg[1]&PPU_GRAY)?0x30:0x3F))|((PPU_Reg[1]&0xE0)<<1);
			else if(ppuVramAddr >= 0x3F00 && ppuVramAddr < 0x4000) //bg and sprite disabled but address within PALRAM
				cPalIdx = (PPU_PALRAM[ppuVramAddr&0x1F]&((PPU_Reg[1]&PPU_GRAY)?0x30:0x3F))|((PPU_Reg[1]&0xE0)<<1);
			else //bg and sprite disabled and address not within PALRAM
				cPalIdx = PPU_PALRAM[0];
			textureImage[drawPos] = PPU_BGRLUT[cPalIdx];
		}
		else /* Draw clipped area as black */
			textureImage[drawPos] = 0xFF000000;
	}

	/* set to 0 during not visible dots up to post-render line */
	if(pictureOutput && curDot > 257 && curLine < VISIBLE_LINES)
	{
		ppuOAMpos = 0;
		ppuSpriteSearch = true;
		ppuCurOverflowAdd = 0;
	}
	/* Update vertical values on pre-render scanline */
	if(pictureOutput && curLine == ppuPreRenderLine && curDot >= 280 && curDot <= 304)
	{
		ppuVramAddr &= ~PPU_VRAM_VERTICAL_MASK;
		ppuVramAddr |= (ppuTmpVramAddr & PPU_VRAM_VERTICAL_MASK);
	}
	/* Clear out OAM2 for next eval */
	if(curDot < 64 && ((curDot&1) == 0) && (curLine == ppuPreRenderLine || curLine < VISIBLE_LINES))
	{
		PPU_OAM2[ppuOAM2pos] = 0xFF;
		ppuOAM2pos++; ppuOAM2pos &= 0x1F;
	} /* Start sprite eval for next line */
	else if(curDot >= 64 && curDot < 192 && ((curDot&1) == 0) && curLine < VISIBLE_LINES && ppuSpriteSearch && pictureOutput)
	{
		uint8_t cSpriteLn = PPU_OAM[(ppuOAMpos+ppuCurOverflowAdd)&0xFF];
		/* nes sprite overflow bug */
		if(ppuOAM2pos == 0x20)
		{
			if(ppuCurOverflowAdd < 3)
				ppuCurOverflowAdd++;
			else
				ppuCurOverflowAdd = 0;
		}
		uint8_t cSpriteAdd = (PPU_Reg[0] & PPU_SPRITE_8_16) ? 16 : 8;
		if(ppuSpriteSearch && cSpriteLn <= curLine && (cSpriteLn+cSpriteAdd) > curLine)
		{
			if(ppuOAM2pos != 0x20)
			{
				if(ppuOAM2pos == 0 && ppuOAMpos == 0)
					ppuNextHasZSprite = true;
				*(uint32_t*)(PPU_OAM2+ppuOAM2pos) = *(uint32_t*)(PPU_OAM+ppuOAMpos);
				//printf("Copying sprite with line %i at line %i pos oam %i oam2 %i\n", cSpriteLn, curLine, ppuOAMpos, ppuOAM2pos);
				ppuOAM2pos += 4;
			}
			else
			{
				//if(!(PPU_Reg[2] & PPU_FLAG_OVERFLOW))
				//	printf("Overflow on line %i\n", curLine);
				PPU_Reg[2] |= PPU_FLAG_OVERFLOW;
				ppuSpriteSearch = false;
			}
		}
		ppuOAMpos += 4;
		//if(ppuOAMpos == 0)
		//	ppuSpriteSearch = false;
		//printf("%i\n", ppuOAMpos);
	}
	/* Grab Sprite Tiles to be drawn next line */
	if(curDot >= 260 && curDot <= 316 && (curDot&7) == 4 && (curLine == ppuPreRenderLine || curLine < VISIBLE_LINES) && pictureOutput)
	{
		uint8_t cSpriteLn = PPU_OAM2[ppuSpriteTilePos];
		uint8_t cSpriteIndex = PPU_OAM2[ppuSpriteTilePos+1];
		uint8_t cSpriteByte2 = PPU_OAM2[ppuSpriteTilePos+2];
		uint8_t cSpriteByte3 = PPU_OAM2[ppuSpriteTilePos+3];
		uint8_t cSpriteAnd = (PPU_Reg[0] & PPU_SPRITE_8_16) ? 15 : 7;
		uint8_t cSpriteAdd = 0; //used to select which 8 by 16 tile
		uint8_t cSpriteY = (curLine - cSpriteLn)&cSpriteAnd;
		if(cSpriteY > 7) //8 by 16 select
		{
			cSpriteAdd = 16;
			cSpriteY &= 7;
		}
		uint16_t chrROMSpriteAdd = 0;
		if(PPU_Reg[0] & PPU_SPRITE_8_16)
		{
			chrROMSpriteAdd = ((cSpriteIndex & 1) << 12);
			cSpriteIndex &= ~1;
		}
		else if(PPU_Reg[0] & PPU_SPRITE_ADDR)
			chrROMSpriteAdd = 0x1000;
		if(cSpriteByte2 & PPU_SPRITE_FLIP_Y)
		{
			cSpriteY ^= 7;
			if(PPU_Reg[0] & PPU_SPRITE_8_16)
				cSpriteAdd ^= 16; //8 by 16 select
		}
		/* write processed values into internal draw buffer */
		mapperChrMode = 1; 
		PPU_Sprites[ppuSpriteTilePos] = mapperChrGet8(((chrROMSpriteAdd+(cSpriteIndex<<4)+cSpriteY+cSpriteAdd)&0xFFF) | chrROMSpriteAdd);
		PPU_Sprites[ppuSpriteTilePos+1] = mapperChrGet8(((chrROMSpriteAdd+(cSpriteIndex<<4)+cSpriteY+8+cSpriteAdd)&0xFFF) | chrROMSpriteAdd);
		PPU_Sprites[ppuSpriteTilePos+2] = cSpriteByte2;
		PPU_Sprites[ppuSpriteTilePos+3] = cSpriteByte3;
		ppuSpriteTilePos+=4;
	}
ppuIncreasePos:
	/* increase pos */
	curDot++;
	if(curDot == 340 && curLine == ppuPreRenderLine && !nesPAL)
	{
		ppuOddFrame ^= true;
		if(ppuOddFrame && (PPU_Reg[1] & PPU_BG_ENABLE))
			curDot++;
	}
	if(curDot == DOTS)
	{
		curDot = 0;
		curLine++;
		ppuHasZSprite = ppuNextHasZSprite;
		ppuNextHasZSprite = false; //reset
		ppuSpriteTilePos = 0; //reset
		ppuToDraw = ppuOAM2pos;
		ppuOAM2pos = 0;
		//printf("Line done\n");
	}
	if(ppuSprite0hit == 1)
	{
		PPU_Reg[2] |= PPU_FLAG_SPRITEZERO;
		ppuSprite0hit = 0;
	}
	else if(ppuSprite0hit > 1)
		ppuSprite0hit--;
	/* VBlank start at first dot after post-render line */
	/* Though results are better when starting it a bit later */
	if(curDot == 5 && curLine == 241)
	{
		ppuNMITriggered = false;
		if(!ppuReadReg2)
			PPU_Reg[2] |= PPU_FLAG_VBLANK;
		#if PPU_DEBUG_VSYNC
		printf("PPU Start VBlank\n");
		#endif
	}
	ppuReadReg2 = false;
	/* VBlank ends at first dot of the pre-render line */
	/* Though results are better when clearing it a bit later */
	if(curDot == 5 && curLine == ppuPreRenderLine)
	{
		#if PPU_DEBUG_VSYNC
		printf("PPU End VBlank\n");
		#endif
		PPU_Reg[2] &= ~(PPU_FLAG_VBLANK | PPU_FLAG_SPRITEZERO | PPU_FLAG_OVERFLOW);
	}
	/* Wrap back down after pre-render line */
	if(curLine == ppuLinesTotal)
	{
		ppuFrameDone = true;
		curLine = 0;
	}
	//ppuCycles++;
	return true;
}

static uint8_t ppuDoBackground(uint8_t color, uint8_t dot)
{
	if((dot < 8 && !(PPU_Reg[1] & PPU_BG_8PX)) || !(PPU_Reg[1] & PPU_BG_ENABLE))
		return color;

	if(ppuBGRegA & (0x8000>>ppuFineXScroll))
		color |= 1;
	if(ppuBGRegB & (0x8000>>ppuFineXScroll))
		color |= 2;
	if(color != 0)
	{
		if(ppuBGAttribRegA & 0x80>>ppuFineXScroll)
			color |= 4;
		if(ppuBGAttribRegB & 0x80>>ppuFineXScroll)
			color |= 8;
	}

	#if PPU_DEBUG_ULTRA
	//if(dot == 90 && color){ printf("%02x\n", color);/* color = 0xD;*/ }
	#endif
	return color;
}

static uint8_t ppuDoSprites(uint8_t color, uint8_t dot)
{
	if((dot < 8 && !(PPU_Reg[1] & PPU_SPRITE_8PX)) || !(PPU_Reg[1] & PPU_SPRITE_ENABLE))
		return color;

	uint8_t i;
	for(i = 0; i < ppuToDraw; i+=4)
	{
		uint8_t cSpriteDot = PPU_Sprites[i+3];
		if(cSpriteDot <= dot && (cSpriteDot+8) > dot)
		{
			uint8_t cSpriteByte2 = PPU_Sprites[i+2];
			uint8_t cSpriteColor = cSpriteByte2&3;
			uint8_t cSpriteX = (dot - cSpriteDot)&7;
			if(cSpriteByte2 & PPU_SPRITE_FLIP_X)
				cSpriteX ^= 7;
			uint8_t sprCol = 0;
			if(PPU_Sprites[i] & (0x80>>cSpriteX))
				sprCol |= 1;
			if(PPU_Sprites[i+1] & (0x80>>cSpriteX))
				sprCol |= 2;
			if(i == 0 && ppuHasZSprite && dot < 255 && ((color&3) != 0) && (sprCol != 0) && !(PPU_Reg[2] & PPU_FLAG_SPRITEZERO) && !ppuSprite0hit)
			{
				ppuSprite0hit = 5;
				#if PPU_DEBUG_ULTRA
				printf("Zero sprite hit at x %i y %i cSpriteDot %i "
							"table %04x color %02x sprCol %02x\n", dot, curLine, cSpriteDot, ppuGetVramTbl((PPU_Reg[0]&3)<<10), color, sprCol);
				#endif
				//if(line != 30)
				//	ppuDebugPauseFrame = true;
			}
			//done looking at sprites, we have to
			//always return the first one we find
			if(sprCol != 0)
			{
				//sprite has highest priority, return sprite
				if((cSpriteByte2 & PPU_SPRITE_PRIO) == 0)
				{
					color = (sprCol | cSpriteColor<<2 | 0x10);
					break;
				} //sprite has low priority and BG is not 0, return BG
				else if((color&3) != 0)
					break;
				//background is 0 so return sprite
				color = (sprCol | cSpriteColor<<2 | 0x10);
				break;
			}
			//Sprite is 0, keep looking for sprites
		}
	}
	return color;
}

bool ppuDrawDone()
{
	if(ppuFrameDone)
	{
		//printf("%i\n",ppuCycles);
		//ppuCycles = 0;
		ppuFrameDone = false;
		return true;
	}
	return false;
}

void ppuSet8(uint8_t reg, uint8_t val)
{
	ppulastVal = val;
	if(reg == 0)
	{
		PPU_Reg[reg] = val;
		ppuTmpVramAddr &= ~0xC00;
		ppuTmpVramAddr |= ((val&3)<<10);
		ppu816Sprite = (PPU_Reg[0] & PPU_SPRITE_8_16) != 0;
		//printf("%d %d %d\n", (PPU_Reg[0] & PPU_BACKGROUND_ADDR) != 0, (PPU_Reg[0] & PPU_SPRITE_ADDR) != 0, (PPU_Reg[0] & PPU_SPRITE_8_16) != 0);
	}
	else if(reg == 3)
	{
		#if PPU_DEBUG_ULTRA
		printf("ppuOAMpos was %02x set to %02x\n", ppuOAMpos, val);
		#endif
		ppuOAMpos = val;
	}
	else if(reg == 4)
	{
		#if PPU_DEBUG_ULTRA
		printf("Setting OAM at %02x to %02x\n", ppuOAMpos, val);
		#endif
		PPU_OAM[ppuOAMpos++] = val;
	}
	else if(reg == 5)
	{
		#if PPU_DEBUG_ULTRA
		printf("ppuScrollWrite (%d) %02x pc %04x\n", ppuTmpWrite, val, cpuGetPc());
		#endif
		if(!ppuTmpWrite)
		{
			ppuTmpWrite = true;
			ppuFineXScroll = val&7;
			ppuTmpVramAddr &= ~0x1F;
			ppuTmpVramAddr |= ((val>>3)&0x1F);
		}
		else
		{
			ppuTmpWrite = false;
			ppuTmpVramAddr &= ~0x73E0;
			ppuTmpVramAddr |= ((val&7)<<12) | ((val>>3)<<5);
		}
	}
	else if(reg == 6)
	{
		#if PPU_DEBUG_ULTRA
		printf("ppuVramAddrWrite (%d) %02x pc %04x\n", ppuTmpWrite, val, cpuGetPc());
		#endif
		if(!ppuTmpWrite)
		{
			ppuTmpWrite = true;
			ppuTmpVramAddr &= 0xFF;
			ppuTmpVramAddr |= ((val&0x3F)<<8);
		}
		else
		{
			ppuTmpWrite = false;
			ppuTmpVramAddr &= ~0xFF;
			ppuTmpVramAddr |= val;
			ppuVramAddr = ppuTmpVramAddr;
		}
	}
	else if(reg == 7)
	{
		uint16_t writeAddr = (ppuVramAddr & 0x3FFF);
		if(writeAddr < 0x2000)
		{
			mapperChrSet8(writeAddr, val);
		}
		else if(writeAddr < 0x3F00)
		{
			uint16_t workAddr = ppuGetVramTbl(writeAddr) | (writeAddr & 0x3FF);
			//printf("ppuVRAMwrite %04x %02x\n", workAddr, val);
			mapperVramSet8(workAddr, val);
		}
		else
		{
			uint8_t palRamAddr = (writeAddr&0x1F);
			if((palRamAddr&3) == 0)
				palRamAddr &= ~0x10;
			PPU_PALRAM[palRamAddr] = val;
		}
		ppuVramAddr += (PPU_Reg[0] & PPU_INC_AMOUNT) ? 32 : 1;
	}
	else if(reg != 2)
	{
		PPU_Reg[reg] = val;
		//printf("ppuSet8 %04x %02x\n", reg, val);
	}
}

uint8_t ppuGet8(uint8_t reg)
{
	uint8_t ret = ppulastVal;
	//if(ret & PPU_FLAG_VBLANK)
	//printf("ppuGet8 %04x:%02x\n",reg,ret);
	if(reg == 2)
	{
		ret = PPU_Reg[reg];
		PPU_Reg[reg] &= ~PPU_FLAG_VBLANK;
		ppuTmpWrite = false;
		ppuReadReg2 = true;
	}
	else if(reg == 4)
		ret = PPU_OAM[ppuOAMpos];
	else if(reg == 7)
	{
		uint16_t writeAddr = (ppuVramAddr & 0x3FFF);
		if(writeAddr < 0x2000)
		{
			ret = ppuVramReadBuf;
			mapperChrMode = 2;
			ppuVramReadBuf = mapperChrGet8(writeAddr);
		}
		else if(writeAddr < 0x3F00)
		{
			ret = ppuVramReadBuf;
			uint16_t workAddr = ppuGetVramTbl(writeAddr) | (writeAddr & 0x3FF);
			ppuVramReadBuf = mapperVramGet8(workAddr);
			//printf("ppuVRAMread pc %04x addr %04x ret %02x\n", cpuGetPc(), workAddr, ret);
		}
		else
		{
			uint8_t palRamAddr = (writeAddr&0x1F);
			if((palRamAddr&3) == 0)
				palRamAddr &= ~0x10;
			ret = PPU_PALRAM[palRamAddr]&((PPU_Reg[1]&PPU_GRAY)?0x30:0x3F);
			//shadow read
			uint16_t workAddr = ppuGetVramTbl(writeAddr) | (writeAddr & 0x3FF);
			ppuVramReadBuf = mapperVramGet8(workAddr);
		}
		ppuVramAddr += (PPU_Reg[0] & PPU_INC_AMOUNT) ? 32 : 1;
	}
	ppulastVal = ret;
	return ret;
}

bool ppuNMI()
{
	if(ppuCurVBlankStat && ppuCurNMIStat)
	{
		if(ppuNMITriggered == false)
		{
			ppuNMITriggered = true;
			return true;
		}
		else
			return false;
	}
	ppuNMITriggered = false;
	return false;
}

void ppuLoadOAM(uint8_t val)
{
	#if PPU_DEBUG_ULTRA
	printf("ppuLoadOAM\n");
	#endif
	PPU_OAM[ppuOAMpos] = val;
	ppuOAMpos++;
}

void ppuDumpMem()
{
	FILE *f = fopen("PPU_VRAM.bin","wb");
	if(f)
	{
		fwrite(PPU_VRAM,1,0x1000,f);
		fclose(f);
	}
	f = fopen("PPU_OAM.bin","wb");
	if(f)
	{
		fwrite(PPU_OAM,1,0x100,f);
		fclose(f);
	}
	f = fopen("PPU_Sprites.bin","wb");
	if(f)
	{
		fwrite(PPU_Sprites,1,0x20,f);
		fclose(f);
	}
}

uint16_t ppuGetCurVramAddr()
{
	return ppuVramAddr;
}

void ppuSetNameTblSingleLower()
{
	ppuNameTbl[0] = 0; ppuNameTbl[1] = 0; ppuNameTbl[2] = 0; ppuNameTbl[3] = 0;
}

void ppuSetNameTblSingleUpper()
{
	ppuNameTbl[0] = 0x400; ppuNameTbl[1] = 0x400; ppuNameTbl[2] = 0x400; ppuNameTbl[3] = 0x400;
}

void ppuSetNameTblVertical()
{
	ppuNameTbl[0] = 0; ppuNameTbl[1] = 0x400; ppuNameTbl[2] = 0; ppuNameTbl[3] = 0x400;
}

void ppuSetNameTblHorizontal()
{
	ppuNameTbl[0] = 0; ppuNameTbl[1] = 0; ppuNameTbl[2] = 0x400; ppuNameTbl[3] = 0x400;
}

void ppuSetNameTbl4Screen()
{
	ppu4Screen = true;
	ppuNameTbl[0] = 0; ppuNameTbl[1] = 0x400; ppuNameTbl[2] = 0x800; ppuNameTbl[3] = 0xC00;
}

void ppuSetNameTblCustom(uint16_t addrA, uint16_t addrB, uint16_t addrC, uint16_t addrD)
{
	ppuNameTbl[0] = addrA; ppuNameTbl[1] = addrB; ppuNameTbl[2] = addrC; ppuNameTbl[3] = addrD;
}


//64x12 1BPP "Track"
static const uint8_t ppuNSFTextTrack[96] =
{
	0x0C, 0x1C, 0x03, 0xD8, 0x7C, 0x71, 0xC0, 0x00, 0x0C, 0x1C, 0x07, 0xF8, 0xFE, 0x73, 0x80, 0x00, 
	0x0C, 0x1C, 0x06, 0x39, 0xE2, 0x77, 0x00, 0x00, 0x0C, 0x1C, 0x07, 0x39, 0xC0, 0x7E, 0x00, 0x00, 
	0x0C, 0x1C, 0x07, 0xF9, 0xC0, 0x7C, 0x00, 0x00, 0x0C, 0x1C, 0x01, 0xF9, 0xC0, 0x7C, 0x00, 0x00, 
	0x0C, 0x1E, 0x60, 0x38, 0xE2, 0x7E, 0x00, 0x00, 0x0C, 0x1F, 0xE3, 0xF8, 0xFE, 0x77, 0x00, 0x00, 
	0x0C, 0x1D, 0xC3, 0xF0, 0x3C, 0x73, 0xC0, 0x00, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x70, 0x00, 0x00, 
	0xFF, 0xC0, 0x00, 0x00, 0x00, 0x70, 0x00, 0x00, 0xFF, 0xC0, 0x00, 0x00, 0x00, 0x70, 0x00, 0x00, 
};

//128x12 1BPP "0123456789/"
static const uint8_t ppuNsfTextRest[192] =
{
	0x0E, 0x1F, 0xF7, 0xF8, 0xF8, 0x03, 0x9F, 0x01, 0xF0, 0x60, 0x1F, 0x0F, 0x83, 0x00, 0x00, 0x00, 
	0x3F, 0x9F, 0xF7, 0xF9, 0xFC, 0x03, 0x9F, 0xC3, 0xF8, 0x70, 0x3F, 0x8F, 0xC3, 0x00, 0x00, 0x00, 
	0x3B, 0x83, 0x83, 0x80, 0x0E, 0x7F, 0xC1, 0xE7, 0x1C, 0x70, 0x71, 0xC0, 0xE1, 0x80, 0x00, 0x00, 
	0x71, 0xC3, 0x81, 0xC0, 0x0E, 0x7F, 0xC0, 0xE7, 0x1C, 0x30, 0x71, 0xC0, 0x71, 0x80, 0x00, 0x00, 
	0x79, 0xC3, 0x80, 0xE0, 0x0E, 0x63, 0x80, 0xE7, 0x1C, 0x38, 0x71, 0xC7, 0x70, 0xC0, 0x00, 0x00, 
	0x7D, 0xC3, 0x80, 0x70, 0x7E, 0x33, 0x81, 0xE7, 0x1C, 0x18, 0x3F, 0x8F, 0xF0, 0xC0, 0x00, 0x00, 
	0x77, 0xC3, 0x80, 0x70, 0x7C, 0x13, 0x9F, 0xC7, 0xF8, 0x1C, 0x1F, 0x1C, 0x70, 0x60, 0x00, 0x00, 
	0x73, 0xC3, 0x80, 0x38, 0x0E, 0x1B, 0x9F, 0x87, 0x70, 0x1C, 0x31, 0x9C, 0x70, 0x60, 0x00, 0x00, 
	0x71, 0xC3, 0x80, 0x38, 0x0E, 0x0B, 0x9C, 0x07, 0x00, 0x0C, 0x71, 0xDC, 0x70, 0x30, 0x00, 0x00, 
	0x3B, 0x9F, 0x80, 0x38, 0x0E, 0x0F, 0x9C, 0x03, 0x80, 0x0E, 0x71, 0xDC, 0x70, 0x30, 0x00, 0x00, 
	0x3F, 0x8F, 0x83, 0xF1, 0xFC, 0x07, 0x9F, 0xC1, 0xF9, 0xFE, 0x3F, 0x8F, 0xE0, 0x18, 0x00, 0x00, 
	0x0E, 0x03, 0x81, 0xE0, 0xF8, 0x03, 0x9F, 0xC0, 0xF9, 0xFE, 0x1F, 0x07, 0xC0, 0x18, 0x00, 0x00, 
};

static void ppuDrawRest(uint8_t curX, uint8_t sym)
{
	uint8_t i, j;
	for(i = 0; i < 12; i++)
	{
		for(j = 0; j < 10; j++)
		{
			size_t drawPos = (j+curX)+((i+9)*256);
			uint8_t xSel = (j+(sym*10));
			if(ppuNsfTextRest[((11-i)<<4)+(xSel>>3)]&(0x80>>(xSel&7)))
				textureImage[drawPos] = 0xFFFFFFFF; //White
			else
				textureImage[drawPos] = 0xFF000000; //Black
		}
	}
}

void ppuDrawNSFTrackNum(uint8_t cTrack, uint8_t trackTotal)
{
	memset(textureImage,0,0x16800);
	uint8_t curX = 4;
	//draw "Track"
	uint8_t i, j;
	for(i = 0; i < 12; i++)
	{
		for(j = 0; j < 50; j++)
		{
			size_t drawPos = (j+curX)+((i+9)*256);
			if(ppuNSFTextTrack[((11-i)<<3)+(j>>3)]&(0x80>>(j&7)))
				textureImage[drawPos] = 0xFFFFFFFF; //White
			else
				textureImage[drawPos] = 0xFF000000; //Black
		}
	}
	//"Track" len+space
	curX+=60;
	//draw current num
	if(cTrack > 99)
	{
		ppuDrawRest(curX, (cTrack/100)%10);
		curX+=10;
	}
	if(cTrack > 9)
	{
		ppuDrawRest(curX, (cTrack/10)%10);
		curX+=10;
	}
	ppuDrawRest(curX, cTrack%10);
	curX+=10;
	//draw the "/"
	ppuDrawRest(curX, 10);
	curX+=10;
	//draw total num
	if(trackTotal > 99)
	{
		ppuDrawRest(curX, (trackTotal/100)%10);
		curX+=10;
	}
	if(trackTotal > 9)
	{
		ppuDrawRest(curX, (trackTotal/10)%10);
		curX+=10;
	}
	ppuDrawRest(curX, trackTotal%10);
	curX+=10;
}

uint8_t ppuVRAMGet8(uint16_t addr)
{
	return PPU_VRAM[addr&0xFFF];
}

void ppuVRAMSet8(uint16_t addr, uint8_t val)
{
	PPU_VRAM[addr&0xFFF] = val;
}
