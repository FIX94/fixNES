/*
 * Copyright (C) 2017 - 2018 FIX94
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <malloc.h>
#include <inttypes.h>
#include <ctype.h>
#ifndef __LIBRETRO__
#include <GL/glut.h>
#include <GL/glext.h>
#endif
#include <time.h>
#include <math.h>
#include "mapper.h"
#include "cpu.h"
#include "ppu.h"
#include "mem.h"
#include "input.h"
#include "fm2play.h"
#include "apu.h"
#include "audio.h"
#include "audio_fds.h"
#include "audio_vrc7.h"
#include "mapper_h/nsf.h"
#include "mapper_h/fds.h"
#if ZIPSUPPORT
#include "unzip/unzip.h"
#endif
#define DEBUG_HZ 0
#define DEBUG_MAIN_CALLS 0
#define DEBUG_KEY 0
#define DEBUG_LOAD_INFO 1

const char *VERSION_STRING = "fixNES Alpha v1.2.1";
static char window_title[256];
static char window_title_pause[256];

enum {
	FTYPE_UNK = 0,
	FTYPE_NES,
	FTYPE_NSF,
	FTYPE_FDS,
	FTYPE_QD,
#if ZIPSUPPORT
	FTYPE_ZIP,
#endif
};

static void nesEmuFileOpen(const char *name);
static bool nesEmuFileRead();
static void nesEmuFileClose();

static void nesEmuDisplayFrame(void);
void nesEmuMainLoop(void);
void nesEmuDeinit(void);
static void nesEmuFdsSetup(uint8_t *src, uint8_t *dst);

static void nesEmuHandleKeyDown(unsigned char key, int x, int y);
static void nesEmuHandleKeyUp(unsigned char key, int x, int y);
static void nesEmuHandleSpecialDown(int key, int x, int y);
static void nesEmuHandleSpecialUp(int key, int x, int y);
#if WINDOWS_BUILD
static void nesEmuSetWindowsVSync(int vsync);
#endif
static int emuFileType = FTYPE_UNK;
static char emuFileName[1024];
uint8_t *emuNesROM = NULL;
uint32_t emuNesROMsize = 0;
#ifndef __LIBRETRO__
static char emuSaveName[1024];
#endif
uint8_t *emuPrgRAM = NULL;
uint32_t emuPrgRAMsize = 0;
//used externally
#ifdef COL_32BIT
uint32_t textureImage[0xF000];
#define TEXIMAGE_LEN VISIBLE_DOTS*VISIBLE_LINES*4
#ifdef COL_GL_BSWAP
#define GL_TEX_FMT GL_UNSIGNED_INT_8_8_8_8_REV
#else //no REVerse
#define GL_TEX_FMT GL_UNSIGNED_INT_8_8_8_8
#endif
#else //COL_16BIT
uint16_t textureImage[0xF000];
#define TEXIMAGE_LEN VISIBLE_DOTS*VISIBLE_LINES*2
#ifdef COL_GL_BSWAP
#define GL_TEX_FMT GL_UNSIGNED_SHORT_5_6_5_REV
#else //no REVerse
#define GL_TEX_FMT GL_UNSIGNED_SHORT_5_6_5
#endif
#endif
bool nesPause = false;
bool ppuDebugPauseFrame = false;
bool doOverscan = true;
bool nesPAL = false;
bool nesEmuNSFPlayback = false;
uint8_t emuInitialNT = NT_UNKNOWN;

#ifndef __LIBRETRO__
static bool inPause = false;
static bool inOverscanToggle = false;
static bool inResize = false;
static bool inDiskSwitch = false;
static bool inReset = false;

#if WINDOWS_BUILD
#include <windows.h>
typedef bool (APIENTRY *PFNWGLSWAPINTERVALEXTPROC) (int interval);
PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT = NULL;
#if DEBUG_HZ
static DWORD emuFrameStart = 0;
static DWORD emuTimesCalled = 0;
static DWORD emuTotalElapsed = 0;
#endif
#if DEBUG_MAIN_CALLS
static DWORD emuMainFrameStart = 0;
static DWORD emuMainTimesCalled = 0;
static DWORD emuMainTimesSkipped = 0;
static DWORD emuMainTotalElapsed = 0;
#endif
#endif
#endif // __LIBRETRO__

#define DOTS 341

#define VISIBLE_DOTS 256
#define VISIBLE_LINES 240

static uint32_t linesToDraw = VISIBLE_LINES;
static uint8_t scaleFactor = 2;
bool emuSaveEnabled = false;
bool emuFdsHasSideB = false;

//static uint16_t ppuCycleTimer;
uint32_t cpuCycleTimer;
uint32_t vrc7CycleTimer;
//from input.c
extern uint8_t inValReads[8];
//from m32.c
extern bool m32_singlescreen;
//from p16c8.c
extern bool m78_m78a;
//from ppu.c
extern bool ppuMapper5;

#ifdef __LIBRETRO__
int nesEmuLoadGame(const char* filename)
{
	int argc = 2;
	const char* argv[] = {"fixNES", filename};
#else
int main(int argc, char** argv)
{
#endif
	puts(VERSION_STRING);
	strcpy(window_title, VERSION_STRING);
	memset(textureImage,0,TEXIMAGE_LEN);
	emuFileType = FTYPE_UNK;
	linesToDraw = VISIBLE_LINES;
	scaleFactor = 2;
	emuSaveEnabled = false;
	emuFdsHasSideB = false;
	nesPause = false;
	ppuDebugPauseFrame = false;
	doOverscan = true;
	nesPAL = false;
	nesEmuNSFPlayback = false;
	m32_singlescreen = false;
	m78_m78a = false;
	ppuMapper5 = false;
	emuInitialNT = NT_UNKNOWN;
	memset(emuFileName,0,1024);
#ifndef __LIBRETRO__
	memset(emuSaveName,0,1024);
#endif
	if(argc >= 2)
		nesEmuFileOpen(argv[1]);
	if(emuFileType == FTYPE_NES)
	{
		if(!nesEmuFileRead())
		{
			nesEmuFileClose();
			printf("Main: Could not read %s!\n", emuFileName);
			puts("Press enter to exit");
			getc(stdin);
			return EXIT_FAILURE;
		}
		nesEmuFileClose();
		nesPAL = (strstr(emuFileName,"(E)") != NULL) || (strstr(emuFileName,"(Europe)") != NULL) || (strstr(emuFileName,"(Australia)") != NULL)
			|| (strstr(emuFileName,"(France)") != NULL) || (strstr(emuFileName,"(Germany)") != NULL) || (strstr(emuFileName,"(Italy)") != NULL)
			|| (strstr(emuFileName,"(Spain)") != NULL) || (strstr(emuFileName,"(Sweden)") != NULL);
		uint8_t mapper = ((emuNesROM[6] & 0xF0) >> 4) | ((emuNesROM[7] & 0xF0));
		emuSaveEnabled = (emuNesROM[6] & (1<<1)) != 0;
		bool trainer = (emuNesROM[6] & (1<<2)) != 0;
		uint32_t ROMsize = emuNesROMsize-16;
		uint32_t prgROMsize = emuNesROM[4] * 0x4000;
		if(prgROMsize > ROMsize) //ensure we read in bounds
			prgROMsize = ROMsize;
		ROMsize -= prgROMsize;
		uint32_t chrROMsize = emuNesROM[5] * 0x2000;
		if(chrROMsize > ROMsize) //ensure we read in bounds
			chrROMsize = ROMsize;
		if(mapper == 5) //just to be on the safe side
			emuPrgRAMsize = 0x10000;
		else
		{
			emuPrgRAMsize = emuNesROM[8] * 0x2000;
			if(emuPrgRAMsize == 0) emuPrgRAMsize = 0x2000;
		}
		emuPrgRAM = malloc(emuPrgRAMsize);
		uint8_t *prgROM = emuNesROM+16;
		if(trainer)
		{
			memcpy(emuPrgRAM+0x1000,prgROM,0x200);
			prgROM += 512;
		}
		uint8_t *chrROM = NULL;
		if(chrROMsize)
		{
			chrROM = emuNesROM+16+prgROMsize;
			if(trainer) chrROM += 512;
		}
		apuInitBufs();
		cpuInit();
		ppuInit();
		memInit();
		apuInit();
		inputInit();
		if(emuNesROM[6] & 8)
		{
			emuInitialNT = NT_4SCREEN;
			ppuSetNameTbl4Screen();
		}
		else if(emuNesROM[6] & 1)
		{
			emuInitialNT = NT_VERTICAL;
			ppuSetNameTblVertical();
		}
		else
		{
			emuInitialNT = NT_HORIZONTAL;
			ppuSetNameTblHorizontal();
		}
		#if DEBUG_LOAD_INFO
		printf("Used Mapper: %i\n", mapper);
		printf("PRG: 0x%x bytes PRG RAM: 0x%x bytes CHR: 0x%x bytes\n", prgROMsize, emuPrgRAMsize, chrROMsize);
		#endif
		if(mapper == 5)
			ppuMapper5 = true;
		else if(mapper == 32)
		{
			if(strstr(emuFileName,"Major League") != NULL)
			{
				printf("Using Single Screen for Major League Mapper 32\n");
				m32_singlescreen = true;
			}
			else
				m32_singlescreen = false;
		}
		else if(mapper == 78)
		{
			if(strstr(emuFileName,"Holy Diver") != NULL)
			{
				printf("Using Holy Diver Variant for Mapper 78\n");
				m78_m78a = true;
			}
			else
				m78_m78a = false;
		}
		if(!mapperInit(mapper, prgROM, prgROMsize, emuPrgRAM, emuPrgRAMsize, chrROM, chrROMsize))
		{
			printf("Mapper init failed!\n");
			free(emuNesROM);
			puts("Press enter to exit");
			getc(stdin);
			return EXIT_FAILURE;
		}
		#if DEBUG_LOAD_INFO
		printf("Trainer: %i Saving: %i VRAM Mode: %s\n", trainer, emuSaveEnabled, (emuNesROM[6] & 8) ? "4-Screen" : 
			((emuNesROM[6] & 1) ? "Vertical" : "Horizontal"));
		#endif
		sprintf(window_title, "%s NES - %s\n", nesPAL ? "PAL" : "NTSC", VERSION_STRING);
#ifndef __LIBRETRO__
		if(emuSaveEnabled)
		{
			memcpy(emuSaveName, emuFileName, 1024);
			memcpy(emuSaveName+strlen(emuSaveName)-3,"sav",3);
			printf("Save Path: %s\n",emuSaveName);
			FILE *save = fopen(emuSaveName, "rb");
			if(save)
			{
				fread(emuPrgRAM,1,emuPrgRAMsize,save);
				fclose(save);
			}
		}
#endif
		#if 0
		if(argc == 5 && (strstr(argv[2],".fm2") != NULL || strstr(argv[2],".FM2") != NULL))
			fm2playInit(argv[2], atoi(argv[3]), !!atoi(argv[4]));
		#endif
	}
	else if(emuFileType == FTYPE_NSF)
	{
		if(!nesEmuFileRead())
		{
			nesEmuFileClose();
			printf("Main: Could not read %s!\n", emuFileName);
			puts("Press enter to exit");
			getc(stdin);
			return EXIT_FAILURE;
		}
		nesEmuFileClose();
		emuPrgRAMsize = 0x2000;
		emuPrgRAM = malloc(emuPrgRAMsize);
		if(!mapperInitNSF(emuNesROM, emuNesROMsize, emuPrgRAM, emuPrgRAMsize))
		{
			printf("NSF init failed!\n");
			free(emuNesROM);
			puts("Press enter to exit");
			getc(stdin);
			return EXIT_FAILURE;
		}
		if(emuNesROM[0xE] != 0)
			sprintf(window_title, "%.32s (%s NSF) - %s\n", (char*)(emuNesROM+0xE), nesPAL ? "PAL" : "NTSC", VERSION_STRING);
		nesEmuNSFPlayback = true;
		linesToDraw = 30;
		scaleFactor = 3;
	}
	else if(emuFileType == FTYPE_FDS || emuFileType == FTYPE_QD)
	{
		bool saveValid = false;
#ifndef __LIBRETRO__
		memcpy(emuSaveName, emuFileName, 1024);
		if(emuFileType == FTYPE_FDS)
			memcpy(emuSaveName+strlen(emuSaveName)-3,"sav",3);
		else //.qd has one less character
			memcpy(emuSaveName+strlen(emuSaveName)-2,"sav",3);
		printf("Save Path: %s\n",emuSaveName); 
		FILE *save = fopen(emuSaveName, "rb");
		if(save)
		{
			fseek(save,0,SEEK_END);
			size_t saveSize = ftell(save);
			if(saveSize == 0x10000 || saveSize == 0x20000)
			{
				emuNesROM = malloc(saveSize);
				rewind(save);
				fread(emuNesROM,1,saveSize,save);
				saveValid = true;
				if(saveSize == 0x20000)
					emuFdsHasSideB = true;
			}
			else
				printf("Save file ignored\n");
			fclose(save);
		}
#endif
		if(saveValid) //can use save as ROM
			nesEmuFileClose();
		else //no (valid) save, parse file
		{
			if(!nesEmuFileRead())
			{
				nesEmuFileClose();
				printf("Main: Could not read %s!\n", emuFileName);
				puts("Press enter to exit");
				getc(stdin);
				return EXIT_FAILURE;
			}
			nesEmuFileClose();
			uint8_t *nesFread = emuNesROM;
			uint32_t nesFread_len = emuNesROMsize;
			emuNesROM = NULL;
			emuNesROMsize = 0;
			uint8_t *fds_src;
			uint32_t fds_src_len;
			if(nesFread[0] == 0x46 && nesFread[1] == 0x44 && nesFread[2] == 0x53)
			{
				fds_src = nesFread+0x10;
				fds_src_len = nesFread_len-0x10;
			}
			else
			{
				fds_src = nesFread;
				fds_src_len = nesFread_len;
			}
			bool fds_no_crc = (fds_src[0x38] == 0x02 && fds_src[0x3A] == 0x03 
							&& fds_src[0x3A] != 0x02 && fds_src[0x3E] != 0x03);
			if(fds_no_crc)
			{
				if(fds_src_len == 0x1FFB8)
				{
					emuFdsHasSideB = true;
					emuNesROMsize = 0x20000;
					emuNesROM = malloc(emuNesROMsize);
					memset(emuNesROM, 0, emuNesROMsize);
					nesEmuFdsSetup(fds_src, emuNesROM); //setup individually
					nesEmuFdsSetup(fds_src+0xFFDC, emuNesROM+0x10000);
				}
				else if(fds_src_len == 0xFFDC)
				{
					emuFdsHasSideB = false;
					emuNesROMsize = 0x10000;
					emuNesROM = malloc(emuNesROMsize);
					memset(emuNesROM, 0, emuNesROMsize);
					nesEmuFdsSetup(fds_src, emuNesROM);
				}
				else
					printf("Unknown FDS Length: %x\n", fds_src_len);
			}
			else
			{
				if(fds_src_len == 0x20000)
				{
					emuFdsHasSideB = true;
					emuNesROMsize = 0x20000;
					emuNesROM = malloc(emuNesROMsize);
					memcpy(emuNesROM, fds_src, emuNesROMsize);
				}
				else if(fds_src_len == 0x10000)
				{
					emuFdsHasSideB = false;
					emuNesROMsize = 0x10000;
					emuNesROM = malloc(emuNesROMsize);
					memcpy(emuNesROM, fds_src, emuNesROMsize);
				}
				else
					printf("Unknown FDS Length: %x\n", fds_src_len);
			}
			free(nesFread);
		}
		if(emuNesROM != NULL)
		{
			emuPrgRAMsize = 0x8000;
			emuPrgRAM = malloc(emuPrgRAMsize);
			apuInitBufs();
			cpuInit();
			ppuInit();
			memInit();
			apuInit();
			inputInit();
			if(!mapperInitFDS(emuNesROM, emuFdsHasSideB, emuPrgRAM, emuPrgRAMsize))
			{
				printf("FDS init failed!\n");
				free(emuNesROM);
				puts("Press enter to exit");
				getc(stdin);
				return EXIT_FAILURE;
			}
			sprintf(window_title, "Famicom Disk System - %s\n", VERSION_STRING);
		}
	}
	if(emuNesROM == NULL)
	{
#if ZIPSUPPORT
		printf("Main: No File to Open! Make sure to call fixNES with a .nes/.nsf/.fds/.qd/.zip File as Argument.\n");
#else
		printf("Main: No File to Open! Make sure to call fixNES with a .nes/.nsf/.fds/.qd File as Argument.\n");
#endif
		puts("Press enter to exit");
		getc(stdin);
		return EXIT_FAILURE;
	}
	sprintf(window_title_pause, "%s (Pause)", window_title);
	#if WINDOWS_BUILD
	#if DEBUG_HZ
	emuFrameStart = GetTickCount();
	#endif
	#if DEBUG_MAIN_CALLS
	emuMainFrameStart = GetTickCount();
	#endif
	#endif
	cpuCycleTimer = nesPAL ? 16 : 12;
	vrc7CycleTimer = 432 / cpuCycleTimer;
	//do one scanline per idle loop
	//ppuCycleTimer = nesPAL ? 5 : 4;
	//mainLoopRuns = nesPAL ? DOTS*ppuCycleTimer : DOTS*ppuCycleTimer;
	//mainLoopPos = mainLoopRuns;
#ifndef __LIBRETRO__
	glutInit(&argc, argv);
	glutInitWindowSize(VISIBLE_DOTS*scaleFactor, linesToDraw*scaleFactor);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA);
	glutCreateWindow(nesPause ? window_title_pause : window_title);
	audioInit();
	atexit(&nesEmuDeinit);
	glutKeyboardFunc(&nesEmuHandleKeyDown);
	glutKeyboardUpFunc(&nesEmuHandleKeyUp);
	glutSpecialFunc(&nesEmuHandleSpecialDown);
	glutSpecialUpFunc(&nesEmuHandleSpecialUp);
	glutDisplayFunc(&nesEmuDisplayFrame);
	glutIdleFunc(&nesEmuMainLoop);
	#if WINDOWS_BUILD
	/* Enable OpenGL VSync */
	wglSwapIntervalEXT = (PFNWGLSWAPINTERVALEXTPROC)wglGetProcAddress("wglSwapIntervalEXT");
	nesEmuSetWindowsVSync(1);
	#endif
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
#ifdef COL_32BIT
#ifdef COL_BGRA
	printf("Drawing onto BGRA8 Texture\n");
	glTexImage2D(GL_TEXTURE_2D, 0, 4, VISIBLE_DOTS, linesToDraw, 0, GL_BGRA, GL_TEX_FMT, textureImage);
#else //case RGBA
	printf("Drawing onto RGBA8 Texture\n");
	glTexImage2D(GL_TEXTURE_2D, 0, 4, VISIBLE_DOTS, linesToDraw, 0, GL_RGBA, GL_TEX_FMT, textureImage);
#endif //end COL_BGRA
#else //case COL_16BIT
	printf("Drawing onto RGB565 Texture\n");
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, VISIBLE_DOTS, linesToDraw, 0, GL_RGB, GL_TEX_FMT, textureImage);
#endif //end COL_32BIT
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glEnable(GL_TEXTURE_2D);
	glShadeModel(GL_FLAT);

	glutMainLoop();
	#endif // __LIBRETRO__
	return EXIT_SUCCESS;
}

static FILE *nesEmuFilePointer = NULL;
#if ZIPSUPPORT
static bool nesEmuFileIsZip = false;
static uint8_t *nesEmuZipBuf = NULL;
static uint32_t nesEmuZipLen = 0;
static unzFile nesEmuZipObj;
static unz_file_info nesEmuZipObjInfo;
#endif
static int nesEmuGetFileType(const char *name)
{
	int nLen = strlen(name);
	if(nLen > 4 && name[nLen-4] == '.')
	{
		if(tolower(name[nLen-3]) == 'n' && tolower(name[nLen-2]) == 'e' && tolower(name[nLen-1]) == 's')
			return FTYPE_NES;
		else if(tolower(name[nLen-3]) == 'n' && tolower(name[nLen-2]) == 's' && tolower(name[nLen-1]) == 'f')
			return FTYPE_NSF;
		else if(tolower(name[nLen-3]) == 'f' && tolower(name[nLen-2]) == 'd' && tolower(name[nLen-1]) == 's')
			return FTYPE_FDS;
#if ZIPSUPPORT
		else if(tolower(name[nLen-3]) == 'z' && tolower(name[nLen-2]) == 'i' && tolower(name[nLen-1]) == 'p')
			return FTYPE_ZIP;
#endif
	}
	else if(nLen > 3 && name[nLen-3] == '.')
	{
		if(tolower(name[nLen-2]) == 'q' && tolower(name[nLen-1]) == 'd')
			return FTYPE_QD;
	}
	return FTYPE_UNK;
}

static void nesEmuFileOpen(const char *name)
{
	emuFileType = FTYPE_UNK;
	memset(emuFileName,0,1024);
#ifndef __LIBRETRO__
	memset(emuSaveName,0,1024);
#endif
	int baseType = nesEmuGetFileType(name);
#if ZIPSUPPORT
	if(baseType == FTYPE_ZIP)
	{
		printf("Base ZIP File: %s\n", name);
		FILE *tmp = fopen(name,"rb");
		if(!tmp)
		{
			printf("Main: Could not open %s!\n", name);
			return;
		}
		fseek(tmp,0,SEEK_END);
		nesEmuZipLen = ftell(tmp);
		rewind(tmp);
		nesEmuZipBuf = malloc(nesEmuZipLen);
		if(!nesEmuZipBuf)
		{
			printf("Main: Could not allocate ZIP buffer!\n");
			fclose(tmp);
			return;
		}
		fread(nesEmuZipBuf,1,nesEmuZipLen,tmp);
		fclose(tmp);
		char filepath[20];
		snprintf(filepath,20,"%x+%x",(unsigned int)nesEmuZipBuf,nesEmuZipLen);
		nesEmuZipObj = unzOpen(filepath);
		int err = unzGoToFirstFile(nesEmuZipObj);
		while (err == UNZ_OK)
		{
			char tmpName[256];
			err = unzGetCurrentFileInfo(nesEmuZipObj,&nesEmuZipObjInfo,tmpName,256,NULL,0,NULL,0);
			if(err == UNZ_OK)
			{
				int curInZipType = nesEmuGetFileType(tmpName);
				if(curInZipType != FTYPE_ZIP && curInZipType != FTYPE_UNK)
				{
					emuFileType = curInZipType;
					nesEmuFileIsZip = true;
					if(strchr(name,'/') != NULL || strchr(name,'\\') != NULL)
					{
						const char *nPath = name;
						if(strchr(nPath,'/') != NULL)
							nPath = (strrchr(nPath,'/')+1);
						if(strchr(nPath,'\\') != NULL)
							nPath = (strrchr(nPath,'\\')+1);
						strncpy(emuFileName, name, nPath-name);
					}
					const char *zName = tmpName;
					if(strchr(zName,'/') != NULL)
						zName = (strrchr(zName,'/')+1);
					if(strchr(zName,'\\') != NULL)
						zName = (strrchr(zName,'\\')+1);
					strcat(emuFileName, zName);
					printf("File in ZIP Type: %s\n", emuFileType == FTYPE_NES ? "NES" : (emuFileType == FTYPE_NSF ? "NSF" : "FDS"));
					printf("Full Path from ZIP: %s\n", emuFileName);
					break;
				}
				else
					err = unzGoToNextFile(nesEmuZipObj);
			}
		}
		if(emuFileType == FTYPE_UNK)
		{
			printf("Found no usable file in ZIP\n");
			unzClose(nesEmuZipObj);
			if(nesEmuZipBuf)
				free(nesEmuZipBuf);
			nesEmuZipBuf = NULL;
			nesEmuZipLen = 0;
		}
	}
	else if(baseType != FTYPE_UNK)
#else
	if(baseType != FTYPE_UNK)
#endif
	{
		nesEmuFilePointer = fopen(name,"rb");
		if(!nesEmuFilePointer)
			printf("Main: Could not open %s!\n", name);
		else
		{
			emuFileType = baseType;
			strncpy(emuFileName, name, 1024);
			printf("File Type: %s\n", baseType == FTYPE_NES ? "NES" : (baseType == FTYPE_NSF ? "NSF" : "FDS"));
			printf("Full Path: %s\n", emuFileName);
		}
	}
}

static bool nesEmuFileRead()
{
#if ZIPSUPPORT
	if(nesEmuFileIsZip)
	{
		unzOpenCurrentFile(nesEmuZipObj);
		emuNesROMsize = nesEmuZipObjInfo.uncompressed_size;
		emuNesROM = malloc(emuNesROMsize);
		if(emuNesROM)
			unzReadCurrentFile(nesEmuZipObj,emuNesROM,emuNesROMsize);
		unzCloseCurrentFile(nesEmuZipObj);
	}
	else
#endif
	{
		fseek(nesEmuFilePointer,0,SEEK_END);
		emuNesROMsize = ftell(nesEmuFilePointer);
		rewind(nesEmuFilePointer);
		emuNesROM = malloc(emuNesROMsize);
		if(emuNesROM)
			fread(emuNesROM,1,emuNesROMsize,nesEmuFilePointer);
	}
	if(emuNesROM)
		return true;
	//else
	printf("Main: Could not allocate ROM buffer!\n");
	return false;
}

//cleans up vars from read
static void nesEmuFileClose()
{
#if ZIPSUPPORT
	if(nesEmuFileIsZip)
		unzClose(nesEmuZipObj);
	nesEmuFileIsZip = false;
	if(nesEmuZipBuf)
		free(nesEmuZipBuf);
	nesEmuZipBuf = NULL;
	nesEmuZipLen = 0;
#endif
	if(nesEmuFilePointer)
		fclose(nesEmuFilePointer);
	nesEmuFilePointer = NULL;
}

#ifndef __LIBRETRO__
static volatile bool emuRenderFrame = false;
#endif
extern uint8_t audioExpansion;
void nesEmuDeinit(void)
{
	//printf("\n");
	#ifndef __LIBRETRO__
	emuRenderFrame = false;
	audioDeinit();
	#endif
	apuDeinitBufs();
	if(emuNesROM != NULL)
	{
#ifndef __LIBRETRO__
		if(!nesEmuNSFPlayback && (audioExpansion&EXP_FDS))
		{
			FILE *save = fopen(emuSaveName, "wb");
			if(save)
			{
				if(emuFdsHasSideB)
					fwrite(emuNesROM,1,0x20000,save);
				else
					fwrite(emuNesROM,1,0x10000,save);
				fclose(save);
			}
		}
#endif
		free(emuNesROM);
	}
	audioExpansion = 0;
	nesEmuNSFPlayback = false;
	emuNesROM = NULL;
	emuNesROMsize = 0;
	if(emuPrgRAM != NULL)
	{
#ifndef __LIBRETRO__
		if(emuSaveEnabled)
		{
			FILE *save = fopen(emuSaveName, "wb");
			if(save)
			{
				fwrite(emuPrgRAM,1,emuPrgRAMsize,save);
				fclose(save);
			}
		}
#endif
		free(emuPrgRAM);
	}
	emuSaveEnabled = false;
	emuPrgRAM = NULL;
	emuPrgRAMsize = 0;
	//printf("Bye!\n");
}

//used externally
#ifndef __LIBRETRO__
bool emuSkipFrame = false;
#endif

//static uint32_t mCycles = 0;
void nesEmuMainLoop(void)
{
#ifndef __LIBRETRO__
	if(emuRenderFrame || nesPause)
	{
		#if (WINDOWS_BUILD && DEBUG_MAIN_CALLS)
		emuMainTimesSkipped++;
		#endif
		audioSleep();
		return;
	}
#endif
	while(1)
	{
		//main CPU clock
		if(!cpuCycle())
			exit(EXIT_SUCCESS);
		//run graphics
		ppuCycle();
		//run audio
		apuCycle();
		//mapper related irqs
		mapperCycle();
		//mCycles++;
		if(ppuDrawDone())
		{
			//printf("%i\n",mCycles);
			//mCycles = 0;
		#ifndef __LIBRETRO__
			emuRenderFrame = true;
			#if 0
			if(fm2playRunning())
				fm2playUpdate();
			#endif
			#if (WINDOWS_BUILD && DEBUG_HZ)
			emuTimesCalled++;
			DWORD end = GetTickCount();
			emuTotalElapsed += end - emuFrameStart;
			if(emuTotalElapsed >= 1000)
			{
				printf("\r%iHz   ", emuTimesCalled);
				emuTimesCalled = 0;
				emuTotalElapsed = 0;
			}
			emuFrameStart = end;
			#endif
			//update audio before drawing
			while(!apuUpdate()) audioSleep();
			glutPostRedisplay();
			#if 0
			if(ppuDebugPauseFrame)
			{
				ppuDebugPauseFrame = false;
				nesPause = true;
			}
			#endif
		#endif
			if(nesEmuNSFPlayback)
				nsfVsync(); //for next song checks
			else if(audioExpansion&EXP_FDS)
				fdsupdatedisc(); //for possible disc insert/switch
			break;
		}
	}
	#if (WINDOWS_BUILD && DEBUG_MAIN_CALLS)
	emuMainTimesCalled++;
	DWORD end = GetTickCount();
	emuMainTotalElapsed += end - emuMainFrameStart;
	if(emuMainTotalElapsed >= 1000)
	{
		printf("\r%i calls, %i skips   ", emuMainTimesCalled, emuMainTimesSkipped);
		emuMainTimesCalled = 0;
		emuMainTimesSkipped = 0;
		emuMainTotalElapsed = 0;
	}
	emuMainFrameStart = end;
	#endif
}

#ifndef __LIBRETRO__
extern bool fdsSwitch;
static void nesEmuHandleKeyDown(unsigned char key, int x, int y)
{
	(void)x;
	(void)y;
	switch (key)
	{
		case 'y':
		case 'z':
		case 'Y':
		case 'Z':
			if(fm2playRunning())
				break;
			#if DEBUG_KEY
			if(inValReads[BUTTON_A]==0)
				printf("a\n");
			#endif
			inValReads[BUTTON_A]=1;
			break;
		case 'x':
		case 'X':
			if(fm2playRunning())
				break;
			#if DEBUG_KEY
			if(inValReads[BUTTON_B]==0)
				printf("b\n");
			#endif
			inValReads[BUTTON_B]=1;
			break;
		case 's':
		case 'S':
			if(fm2playRunning())
				break;
			#if DEBUG_KEY
			if(inValReads[BUTTON_SELECT]==0)
				printf("sel\n");
			#endif
			inValReads[BUTTON_SELECT]=1;
			break;
		case 'a':
		case 'A':
			if(fm2playRunning())
				break;
			#if DEBUG_KEY
			if(inValReads[BUTTON_START]==0)
				printf("start\n");
			#endif
			inValReads[BUTTON_START]=1;
			break;
		case '\x1B': //Escape
			memDumpMainMem();
			exit(EXIT_SUCCESS);
			break;
		case 'b':
		case 'B':
			if(!inDiskSwitch)
			{
				fdsSwitch = true;
				inDiskSwitch = true;
			}
			break;
		case 'p':
		case 'P':
			if(!inPause)
			{
				#if DEBUG_KEY
				printf("pause\n");
				#endif
				inPause = true;
				nesPause ^= true;
				glutSetWindowTitle(nesPause ? window_title_pause : window_title);
			}
			break;
		case '1':
			if(!inResize)
			{
				inResize = true;
				glutReshapeWindow(VISIBLE_DOTS*1, linesToDraw*1);
			}
			break;
		case '2':
			if(!inResize)
			{
				inResize = true;
				glutReshapeWindow(VISIBLE_DOTS*2, linesToDraw*2);
			}
			break;
		case '3':
			if(!inResize)
			{
				inResize = true;
				glutReshapeWindow(VISIBLE_DOTS*3, linesToDraw*3);
			}
			break;
		case '4':
			if(!inResize)
			{
				inResize = true;
				glutReshapeWindow(VISIBLE_DOTS*4, linesToDraw*4);
			}
			break;
		case '5':
			if(!inResize)
			{
				inResize = true;
				glutReshapeWindow(VISIBLE_DOTS*5, linesToDraw*5);
			}
			break;
		case '6':
			if(!inResize)
			{
				inResize = true;
				glutReshapeWindow(VISIBLE_DOTS*6, linesToDraw*6);
			}
			break;
		case '7':
			if(!inResize)
			{
				inResize = true;
				glutReshapeWindow(VISIBLE_DOTS*7, linesToDraw*7);
			}
			break;
		case '8':
			if(!inResize)
			{
				inResize = true;
				glutReshapeWindow(VISIBLE_DOTS*8, linesToDraw*8);
			}
			break;
		case '9':
			if(!inResize)
			{
				inResize = true;
				glutReshapeWindow(VISIBLE_DOTS*9, linesToDraw*9);
			}
			break;
		case 'o':
		case 'O':
			if(!inOverscanToggle)
			{
				inOverscanToggle = true;
				doOverscan ^= true;
			}
			break;
		case '\x12': //ctrl-R
			if(!inReset)
			{
				inReset = true;
				//will be used at the end of a frame
				if(!nesEmuNSFPlayback)
					ppuSoftReset();
			}
			break;
		default:
			break;
	}
}

static void nesEmuHandleKeyUp(unsigned char key, int x, int y)
{
	(void)x;
	(void)y;
	switch (key)
	{
		case 'y':
		case 'z':
		case 'Y':
		case 'Z':
			if(fm2playRunning())
				break;
			#if DEBUG_KEY
			printf("a up\n");
			#endif
			inValReads[BUTTON_A]=0;
			break;
		case 'x':
		case 'X':
			if(fm2playRunning())
				break;
			#if DEBUG_KEY
			printf("b up\n");
			#endif
			inValReads[BUTTON_B]=0;
			break;
		case 's':
		case 'S':
			if(fm2playRunning())
				break;
			#if DEBUG_KEY
			printf("sel up\n");
			#endif
			inValReads[BUTTON_SELECT]=0;
			break;
		case 'a':
		case 'A':
			if(fm2playRunning())
				break;
			#if DEBUG_KEY
			printf("start up\n");
			#endif
			inValReads[BUTTON_START]=0;
			break;
		case 'b':
		case 'B':
			inDiskSwitch = false;
			break;
		case 'p':
		case 'P':
			#if DEBUG_KEY
			printf("pause up\n");
			#endif
			inPause=false;
			break;
		case '1': case '2':	case '3':
		case '4': case '5':	case '6':
		case '7': case '8':	case '9':
			inResize = false;
			break;
		case 'o':
		case 'O':
			inOverscanToggle = false;
			break;
		case '\x12': //ctrl-R
			inReset = false;
			break;
		default:
			break;
	}
}

static void nesEmuHandleSpecialDown(int key, int x, int y)
{
	(void)x;
	(void)y;
	switch(key)
	{
		case GLUT_KEY_UP:
			if(fm2playRunning())
				break;
			#if DEBUG_KEY
			if(inValReads[BUTTON_UP]==0)
				printf("up\n");
			#endif
			inValReads[BUTTON_UP]=1;
			break;	
		case GLUT_KEY_DOWN:
			if(fm2playRunning())
				break;
			#if DEBUG_KEY
			if(inValReads[BUTTON_DOWN]==0)
				printf("down\n");
			#endif
			inValReads[BUTTON_DOWN]=1;
			break;
		case GLUT_KEY_LEFT:
			if(fm2playRunning())
				break;
			#if DEBUG_KEY
			if(inValReads[BUTTON_LEFT]==0)
				printf("left\n");
			#endif
			inValReads[BUTTON_LEFT]=1;
			break;
		case GLUT_KEY_RIGHT:
			if(fm2playRunning())
				break;
			#if DEBUG_KEY
			if(inValReads[BUTTON_RIGHT]==0)
				printf("right\n");
			#endif
			inValReads[BUTTON_RIGHT]=1;
			break;
		default:
			break;
	}
}

static void nesEmuHandleSpecialUp(int key, int x, int y)
{
	(void)x;
	(void)y;
	switch(key)
	{
		case GLUT_KEY_UP:
			if(fm2playRunning())
				break;
			#if DEBUG_KEY
			printf("up up\n");
			#endif
			inValReads[BUTTON_UP]=0;
			break;	
		case GLUT_KEY_DOWN:
			if(fm2playRunning())
				break;
			#if DEBUG_KEY
			printf("down up\n");
			#endif
			inValReads[BUTTON_DOWN]=0;
			break;
		case GLUT_KEY_LEFT:
			if(fm2playRunning())
				break;
			#if DEBUG_KEY
			printf("left up\n");
			#endif
			inValReads[BUTTON_LEFT]=0;
			break;
		case GLUT_KEY_RIGHT:
			if(fm2playRunning())
				break;
			#if DEBUG_KEY
			printf("right up\n");
			#endif
			inValReads[BUTTON_RIGHT]=0;
			break;
		default:
			break;
	}
}

static void nesEmuDisplayFrame()
{
	if(emuRenderFrame)
	{
		#if WINDOWS_BUILD
		//temporarily disable vsync to resync
		//with audio output
		if(emuSkipFrame)
			nesEmuSetWindowsVSync(0);
		else
			nesEmuSetWindowsVSync(1);
		#else
		//vsync on other systems not in our control,
		//so if it wants to skip frames to resync
		//with audio output let it do so
		if(emuSkipFrame)
		{
			emuRenderFrame = false;
			return;
		}
		#endif
#ifdef COL_32BIT
#ifdef COL_BGRA
	glTexImage2D(GL_TEXTURE_2D, 0, 4, VISIBLE_DOTS, linesToDraw, 0, GL_BGRA, GL_TEX_FMT, textureImage);
#else //case RGBA
	glTexImage2D(GL_TEXTURE_2D, 0, 4, VISIBLE_DOTS, linesToDraw, 0, GL_RGBA, GL_TEX_FMT, textureImage);
#endif //end COL_BGRA
#else //case COL_16BIT
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, VISIBLE_DOTS, linesToDraw, 0, GL_RGB, GL_TEX_FMT, textureImage);
#endif //end COL_32BIT
		emuRenderFrame = false;

		glClear(GL_COLOR_BUFFER_BIT);

		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glOrtho(0, glutGet(GLUT_WINDOW_WIDTH), 0, glutGet(GLUT_WINDOW_HEIGHT), -1, 1);
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();

		double upscaleVal = round((((double)glutGet(GLUT_WINDOW_HEIGHT))/((double)linesToDraw))*20.0)/20.0;
		double windowMiddle = ((double)glutGet(GLUT_WINDOW_WIDTH))/2.0;
		double drawMiddle = (((double)VISIBLE_DOTS)*upscaleVal)/2.0;
		double drawHeight = ((double)linesToDraw)*upscaleVal;

		glBegin(GL_QUADS);
			glTexCoord2f(0,0); glVertex2f(windowMiddle-drawMiddle,drawHeight);
			glTexCoord2f(1,0); glVertex2f(windowMiddle+drawMiddle,drawHeight);
			glTexCoord2f(1,1); glVertex2f(windowMiddle+drawMiddle,0);
			glTexCoord2f(0,1); glVertex2f(windowMiddle-drawMiddle,0);
		glEnd();

		glutSwapBuffers();
	}
}
#endif

static void nesEmuFdsSetup(uint8_t *src, uint8_t *dst)
{
	memcpy(dst, src, 0x38);
	memcpy(dst+0x3A, src+0x38, 2);
	uint16_t cDiskPos = 0x3E;
	uint16_t cROMPos = 0x3A;
	do
	{
		if(src[cROMPos] != 0x03)
			break;
		memcpy(dst+cDiskPos, src+cROMPos, 0x10);
		uint16_t copySize = ((src[cROMPos+0xD])|(src[cROMPos+0xE]<<8))+1;
		cDiskPos+=0x12;
		cROMPos+=0x10;
		memcpy(dst+cDiskPos, src+cROMPos, copySize);
		cDiskPos+=copySize+2;
		cROMPos+=copySize;
	} while(cROMPos < 0xFFDC && cDiskPos < 0xFFFF);
	printf("%04x -> %04x\n", cROMPos, cDiskPos);
}

#if WINDOWS_BUILD
static void nesEmuSetWindowsVSync(int vsync)
{
	if(wglSwapIntervalEXT) wglSwapIntervalEXT(vsync);
}
#endif
