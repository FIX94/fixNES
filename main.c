/*
 * Copyright (C) 2017 FIX94
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <malloc.h>
#include <inttypes.h>
#include <GL/glut.h>
#include <GL/glext.h>
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

#define DEBUG_HZ 0
#define DEBUG_MAIN_CALLS 0
#define DEBUG_KEY 0
#define DEBUG_LOAD_INFO 1

static const char *VERSION_STRING = "fixNES Alpha v0.8.2";

static void nesEmuDisplayFrame(void);
static void nesEmuMainLoop(void);
static void nesEmuDeinit(void);
static void nesEmuFdsSetup(uint8_t *src, uint8_t *dst);

static void nesEmuHandleKeyDown(unsigned char key, int x, int y);
static void nesEmuHandleKeyUp(unsigned char key, int x, int y);
static void nesEmuHandleSpecialDown(int key, int x, int y);
static void nesEmuHandleSpecialUp(int key, int x, int y);

static uint8_t *emuNesROM = NULL;
static char *emuSaveName = NULL;
static uint8_t *emuPrgRAM = NULL;
static uint32_t emuPrgRAMsize = 0;
//used externally
uint8_t *textureImage = NULL;
bool nesPause = false;
bool ppuDebugPauseFrame = false;
bool doOverscan = true;
bool nesPAL = false;
bool nesEmuNSFPlayback = false;

static bool inPause = false;
static bool inOverscanToggle = false;
static bool inResize = false;
static bool inDiskSwitch = false;

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

#define DOTS 341

#define VISIBLE_DOTS 256
#define VISIBLE_LINES 240

static const uint32_t visibleImg = VISIBLE_DOTS*VISIBLE_LINES*4;
static uint8_t scaleFactor = 2;
static bool emuSaveEnabled = false;
static bool emuFdsHasSideB = false;
static uint16_t mainLoopRuns;
static uint16_t mainLoopPos;
static uint16_t ppuCycleTimer;
static uint16_t cpuCycleTimer;
//from input.c
extern uint8_t inValReads[8];

int main(int argc, char** argv)
{
	puts(VERSION_STRING);
	if(argc >= 2 && (strstr(argv[1],".nes") != NULL || strstr(argv[1],".NES") != NULL))
	{
		nesPAL = (strstr(argv[1],"(E)") != NULL);
		FILE *nesF = fopen(argv[1],"rb");
		if(!nesF) return EXIT_SUCCESS;
		fseek(nesF,0,SEEK_END);
		size_t fsize = ftell(nesF);
		rewind(nesF);
		emuNesROM = malloc(fsize);
		fread(emuNesROM,1,fsize,nesF);
		fclose(nesF);
		uint8_t mapper = ((emuNesROM[6] & 0xF0) >> 4) | ((emuNesROM[7] & 0xF0));
		emuSaveEnabled = (emuNesROM[6] & (1<<1)) != 0;
		bool trainer = (emuNesROM[6] & (1<<2)) != 0;
		uint32_t prgROMsize = emuNesROM[4] * 0x4000;
		uint32_t chrROMsize = emuNesROM[5] * 0x2000;
		emuPrgRAMsize = emuNesROM[8] * 0x2000;
		if(emuPrgRAMsize == 0) emuPrgRAMsize = 0x2000;
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
		#if DEBUG_LOAD_INFO
		printf("Read in %s\n", argv[1]);
		printf("Used Mapper: %i\n", mapper);
		printf("PRG: 0x%x bytes PRG RAM: 0x%x bytes CHR: 0x%x bytes\n", prgROMsize, emuPrgRAMsize, chrROMsize);
		#endif
		if(!mapperInit(mapper, prgROM, prgROMsize, emuPrgRAM, emuPrgRAMsize, chrROM, chrROMsize))
		{
			printf("Mapper init failed!\n");
			free(emuNesROM);
			emuNesROM = NULL;
			return EXIT_SUCCESS;
		}
		if(emuNesROM[6] & 8)
			ppuSetNameTbl4Screen();
		else if(emuNesROM[6] & 1)
			ppuSetNameTblVertical();
		else
			ppuSetNameTblHorizontal();
		#if DEBUG_LOAD_INFO
		printf("Trainer: %i Saving: %i VRAM Mode: %s\n", trainer, emuSaveEnabled, (emuNesROM[6] & 1) ? "Vertical" : 
			((!(emuNesROM[6] & 1)) ? "Horizontal" : "4-Screen"));
		#endif
		if(emuSaveEnabled)
		{
			emuSaveName = strdup(argv[1]);
			memcpy(emuSaveName+strlen(emuSaveName)-3,"sav",3);
			FILE *save = fopen(emuSaveName, "rb");
			if(save)
			{
				fread(emuPrgRAM,1,emuPrgRAMsize,save);
				fclose(save);
			}
		}
		if(argc == 5 && (strstr(argv[2],".fm2") != NULL || strstr(argv[2],".FM2") != NULL))
			fm2playInit(argv[2], atoi(argv[3]), !!atoi(argv[4]));
	}
	else if(argc >= 2 && (strstr(argv[1],".nsf") != NULL || strstr(argv[1],".NSF") != NULL))
	{
		FILE *nesF = fopen(argv[1],"rb");
		if(!nesF) return EXIT_SUCCESS;
		fseek(nesF,0,SEEK_END);
		size_t fsize = ftell(nesF);
		rewind(nesF);
		emuNesROM = malloc(fsize);
		fread(emuNesROM,1,fsize,nesF);
		fclose(nesF);
		emuPrgRAMsize = 0x2000;
		emuPrgRAM = malloc(emuPrgRAMsize);
		if(!mapperInitNSF(emuNesROM, fsize, emuPrgRAM, emuPrgRAMsize))
		{
			printf("NSF init failed!\n");
			free(emuNesROM);
			return EXIT_SUCCESS;
		}
		nesEmuNSFPlayback = true;
	}
	else if(argc >= 2 && (strstr(argv[1],".fds") != NULL || strstr(argv[1],".FDS") != NULL
						|| strstr(argv[1],".qd") != NULL || strstr(argv[1],".QD") != NULL))
	{
		emuSaveName = strdup(argv[1]);
		memcpy(emuSaveName+strlen(emuSaveName)-3,"sav",3);
		bool saveValid = false;
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
		if(!saveValid)
		{
			FILE *nesF = fopen(argv[1],"rb");
			if(!nesF) return EXIT_SUCCESS;
			fseek(nesF,0,SEEK_END);
			size_t fsize = ftell(nesF);
			rewind(nesF);
			uint8_t *nesFread = malloc(fsize);
			fread(nesFread,1,fsize,nesF);
			fclose(nesF);
			uint8_t *fds_src;
			uint32_t fds_src_len;
			if(nesFread[0] == 0x46 && nesFread[1] == 0x44 && nesFread[2] == 0x53)
			{
				fds_src = nesFread+0x10;
				fds_src_len = fsize-0x10;
			}
			else
			{
				fds_src = nesFread;
				fds_src_len = fsize;
			}
			bool fds_no_crc = (fds_src[0x38] == 0x02 && fds_src[0x3A] == 0x03 
							&& fds_src[0x3A] != 0x02 && fds_src[0x3E] != 0x03);
			if(fds_no_crc)
			{
				if(fds_src_len == 0x1FFB8)
				{
					emuFdsHasSideB = true;
					emuNesROM = malloc(0x20000);
					memset(emuNesROM, 0, 0x20000);
					nesEmuFdsSetup(fds_src, emuNesROM); //setup individually
					nesEmuFdsSetup(fds_src+0xFFDC, emuNesROM+0x10000);
				}
				else if(fds_src_len == 0xFFDC)
				{
					emuNesROM = malloc(0x10000);
					memset(emuNesROM, 0, 0x10000);
					nesEmuFdsSetup(fds_src, emuNesROM);
				}
			}
			else
			{
				if(fds_src_len == 0x20000)
				{
					emuFdsHasSideB = true;
					emuNesROM = malloc(0x20000);
					memcpy(emuNesROM, fds_src, 0x20000);
				}
				else if(fds_src_len == 0x10000)
				{
					emuNesROM = malloc(0x10000);
					memcpy(emuNesROM, fds_src, 0x10000);
				}
			}
			free(nesFread);
		}
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
			emuNesROM = NULL;
			return EXIT_SUCCESS;
		}
	}
	if(emuNesROM == NULL)
		return EXIT_SUCCESS;
	#if WINDOWS_BUILD
	#if DEBUG_HZ
	emuFrameStart = GetTickCount();
	#endif
	#if DEBUG_MAIN_CALLS
	emuMainFrameStart = GetTickCount();
	#endif
	#endif
	textureImage = malloc(visibleImg);
	memset(textureImage,0,visibleImg);
	//make sure image is visible
	uint32_t i;
	for(i = 0; i < visibleImg; i+=4)
		textureImage[i+3] = 0xFF;
	cpuCycleTimer = nesPAL ? 16 : 12;
	//do one scanline per idle loop
	ppuCycleTimer = nesPAL ? 5 : 4;
	mainLoopRuns = nesPAL ? DOTS*ppuCycleTimer : DOTS*ppuCycleTimer;
	mainLoopPos = mainLoopRuns;
	glutInit(&argc, argv);
	glutInitWindowSize(VISIBLE_DOTS*scaleFactor, VISIBLE_LINES*scaleFactor);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA);
	glutCreateWindow(VERSION_STRING);
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
	wglSwapIntervalEXT(1);
	#endif
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage2D(GL_TEXTURE_2D, 0, 4, VISIBLE_DOTS, VISIBLE_LINES, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, textureImage);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glEnable(GL_TEXTURE_2D);
	glShadeModel(GL_FLAT);

	glutMainLoop();

	return EXIT_SUCCESS;
}

static volatile bool emuRenderFrame = false;

static void nesEmuDeinit(void)
{
	//printf("\n");
	emuRenderFrame = false;
	audioDeinit();
	apuDeinitBufs();
	if(emuNesROM != NULL)
	{
		if(!nesEmuNSFPlayback && fdsEnabled)
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
		free(emuNesROM);
	}
	emuNesROM = NULL;
	if(emuPrgRAM != NULL)
	{
		if(emuSaveEnabled)
		{
			FILE *save = fopen(emuSaveName, "wb");
			if(save)
			{
				fwrite(emuPrgRAM,1,emuPrgRAMsize,save);
				fclose(save);
			}
		}
		free(emuPrgRAM);
	}
	emuPrgRAM = NULL;
	if(textureImage != NULL)
		free(textureImage);
	textureImage = NULL;
	//printf("Bye!\n");
}

//used externally
bool emuSkipVsync = false;
bool emuSkipFrame = false;

//static uint32_t mCycles = 0;
static bool emuApuDoCycle = false;

static uint16_t mainClock = 1;
static uint16_t ppuClock = 1;
static uint16_t vrc7Clock = 1;

static void nesEmuMainLoop(void)
{
	do
	{
		if((!emuSkipVsync && emuRenderFrame) || nesPause)
		{
			#if (WINDOWS_BUILD && DEBUG_MAIN_CALLS)
			emuMainTimesSkipped++;
			#endif
			audioSleep();
			return;
		}
		if(mainClock == cpuCycleTimer)
		{
			//runs every second cpu clock
			if(emuApuDoCycle && !apuCycle())
			{
				#if (WINDOWS_BUILD && DEBUG_MAIN_CALLS)
				emuMainTimesSkipped++;
				#endif
				audioSleep();
				return;
			}
			emuApuDoCycle ^= true;
			//runs every cpu cycle
			apuClockTimers();
			//main CPU clock
			if(!cpuCycle())
				exit(EXIT_SUCCESS);
			//mapper related irqs
			if(mapperCycle != NULL)
				mapperCycle();
			//mCycles++;
			//channel timer updates
			apuLenCycle();
			mainClock = 1;
		}
		else
			mainClock++;
		if(ppuClock == ppuCycleTimer)
		{
			if(!ppuCycle())
				exit(EXIT_SUCCESS);
			if(!nesEmuNSFPlayback && ppuDrawDone())
			{
				//printf("%i\n",mCycles);
				//mCycles = 0;
				emuRenderFrame = true;
				if(fm2playRunning())
					fm2playUpdate();
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
				glutPostRedisplay();
				if(ppuDebugPauseFrame)
					nesPause = true;
			}
			ppuClock = 1;
		}
		else
			ppuClock++;
		if(fdsEnabled)
			fdsAudioMasterUpdate();
		if(vrc7enabled)
		{
			if(vrc7Clock == 432)
			{
				vrc7AudioCycle();
				vrc7Clock = 1;
			}
			else
				vrc7Clock++;
		}
	}
	while(mainLoopPos--);
	mainLoopPos = mainLoopRuns;
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
			}
			break;
		case '1':
			if(!inResize)
			{
				inResize = true;
				glutReshapeWindow(VISIBLE_DOTS*1, VISIBLE_LINES*1);
			}
			break;
		case '2':
			if(!inResize)
			{
				inResize = true;
				glutReshapeWindow(VISIBLE_DOTS*2, VISIBLE_LINES*2);
			}
			break;
		case '3':
			if(!inResize)
			{
				inResize = true;
				glutReshapeWindow(VISIBLE_DOTS*3, VISIBLE_LINES*3);
			}
			break;
		case '4':
			if(!inResize)
			{
				inResize = true;
				glutReshapeWindow(VISIBLE_DOTS*4, VISIBLE_LINES*4);
			}
			break;
		case '5':
			if(!inResize)
			{
				inResize = true;
				glutReshapeWindow(VISIBLE_DOTS*5, VISIBLE_LINES*5);
			}
			break;
		case '6':
			if(!inResize)
			{
				inResize = true;
				glutReshapeWindow(VISIBLE_DOTS*6, VISIBLE_LINES*6);
			}
			break;
		case '7':
			if(!inResize)
			{
				inResize = true;
				glutReshapeWindow(VISIBLE_DOTS*7, VISIBLE_LINES*7);
			}
			break;
		case '8':
			if(!inResize)
			{
				inResize = true;
				glutReshapeWindow(VISIBLE_DOTS*8, VISIBLE_LINES*8);
			}
			break;
		case '9':
			if(!inResize)
			{
				inResize = true;
				glutReshapeWindow(VISIBLE_DOTS*9, VISIBLE_LINES*9);
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
		if(emuSkipFrame)
		{
			emuRenderFrame = false;
			return;
		}
		if(textureImage != NULL)
			glTexImage2D(GL_TEXTURE_2D, 0, 4, VISIBLE_DOTS, VISIBLE_LINES, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, textureImage);
		emuRenderFrame = false;
	}

	glClear(GL_COLOR_BUFFER_BIT);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, glutGet(GLUT_WINDOW_WIDTH), 0, glutGet(GLUT_WINDOW_HEIGHT), -1, 1);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	double upscaleVal = round((((double)glutGet(GLUT_WINDOW_HEIGHT))/((double)VISIBLE_LINES))*20.0)/20.0;
	double windowMiddle = ((double)glutGet(GLUT_WINDOW_WIDTH))/2.0;
	double drawMiddle = (((double)VISIBLE_DOTS)*upscaleVal)/2.0;
	double drawHeight = ((double)VISIBLE_LINES)*upscaleVal;

	glBegin(GL_QUADS);
		glTexCoord2f(0,0); glVertex2f(windowMiddle-drawMiddle,drawHeight);
		glTexCoord2f(1,0); glVertex2f(windowMiddle+drawMiddle,drawHeight);
		glTexCoord2f(1,1); glVertex2f(windowMiddle+drawMiddle,0);
		glTexCoord2f(0,1); glVertex2f(windowMiddle-drawMiddle,0);
	glEnd();

	glutSwapBuffers();
}

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
		uint16_t copySize = (*(uint16_t*)(src+cROMPos+0xD))+1;
		cDiskPos+=0x12;
		cROMPos+=0x10;
		memcpy(dst+cDiskPos, src+cROMPos, copySize);
		cDiskPos+=copySize+2;
		cROMPos+=copySize;
	} while(cROMPos < 0xFFDC && cDiskPos < 0xFFFF);
	printf("%04x -> %04x\n", cROMPos, cDiskPos);
}
