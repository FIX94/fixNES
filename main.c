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

#define DEBUG_HZ 0
#define DEBUG_KEY 0
#define DEBUG_LOAD_INFO 1

static const char *VERSION_STRING = "fixNES Alpha v0.3";

static void nesEmuDisplayFrame(void);
static void nesEmuMainLoop(void);
static void nesEmuDeinit(void);

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

static bool inPause = false;
static bool inOverscanToggle = false;
static bool inResize = false;

#if WINDOWS_BUILD
#include <windows.h>
typedef bool (APIENTRY *PFNWGLSWAPINTERVALEXTPROC) (int interval);
PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT = NULL;
static DWORD emuFrameStart = 0;
static DWORD emuTimesCalled = 0;
static DWORD emuTotalElapsed = 0;
#endif

#define DOTS 341

#define VISIBLE_DOTS 256
#define VISIBLE_LINES 240

static const int visibleImg = VISIBLE_DOTS*VISIBLE_LINES*4;
static int scaleFactor = 2;
static bool emuSaveEnabled = false;
static int emuApuClockCycles;

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
		if(emuNesROM[6] & 8)
			ppuScreenMode = PPU_MODE_FOURSCREEN;
		else if(emuNesROM[6] & 1)
			ppuScreenMode = PPU_MODE_VERTICAL;
		else
			ppuScreenMode = PPU_MODE_HORIZONTAL;
		uint32_t prgROMsize = emuNesROM[4] * 0x4000;
		uint32_t chrROMsize = emuNesROM[5] * 0x2000;
		emuPrgRAMsize = emuNesROM[8] * 0x2000;
		if(emuPrgRAMsize == 0) emuPrgRAMsize = 0x2000;
		emuPrgRAM = malloc(emuPrgRAMsize);
		#if DEBUG_LOAD_INFO
		printf("Read in %s\n", argv[1]);
		printf("Used Mapper: %i\n", mapper);
		printf("PRG: 0x%x bytes PRG RAM: 0x%x bytes CHR: 0x%x bytes\n", prgROMsize, emuPrgRAMsize, chrROMsize);
		printf("Trainer: %i Saving: %i VRAM Mode: %s\n", trainer, emuSaveEnabled, (ppuScreenMode == PPU_MODE_VERTICAL) ? "Vertical" : 
			((ppuScreenMode == PPU_MODE_HORIZONTAL) ? "Horizontal" : "4-Screen"));
		#endif
		uint8_t *prgROM = emuNesROM+16;
		if(trainer) prgROM += 512;
		uint8_t *chrROM = NULL;
		if(chrROMsize)
		{
			chrROM = emuNesROM+16+prgROMsize;
			if(trainer) chrROM += 512;
		}
		cpuInit();
		ppuInit();
		memInit();
		apuInit();
		inputInit();
		if(!mapperInit(mapper, prgROM, prgROMsize, emuPrgRAM, emuPrgRAMsize, chrROM, chrROMsize))
		{
			printf("Mapper init failed!\n");
			free(emuNesROM);
			return EXIT_SUCCESS;
		}
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
		#if WINDOWS_BUILD
		emuFrameStart = GetTickCount();
		#endif
		textureImage = malloc(visibleImg);
		memset(textureImage,0,visibleImg);
		//make sure image is visible
		int i;
		for(i = 0; i < visibleImg; i+=4)
			textureImage[i+3] = 0xFF;
	}
	if(emuNesROM == NULL)
		return EXIT_SUCCESS;
	emuApuClockCycles = nesPAL ? 8313 : 7457;
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
	apuDeinit();
	if(emuNesROM != NULL)
		free(emuNesROM);
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

//static int mCycles = 0;
static bool emuApuDoCycle = false;
static int emuApuClock = 0;
//do one scanline per idle loop
#define MAIN_LOOP_RUNS 341
static int mainClock = 0, mainLoopPos = MAIN_LOOP_RUNS;
static int ppuExtraCycles = 0;
static void nesEmuMainLoop(void)
{
	do
	{
		if((!emuSkipVsync && emuRenderFrame) || nesPause)
			return;
		if(mainClock == 2)
		{
			//runs every second cpu clock
			if(emuApuDoCycle && !apuCycle())
				return;
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
			if(emuApuClock == emuApuClockCycles)
			{
				apuLenCycle();
				emuApuClock = 0;
			}
			else
				emuApuClock++;
			mainClock = 0;
		}
		else
			mainClock++;
		//3 PPU dots per CPU cycle
		if(!ppuCycle())
			exit(EXIT_SUCCESS);
		if(nesPAL)
		{
			// Extra PPU cycle every 15 to get to 3.2
			// dots every cpu cycle to match 50Hz timing
			if(ppuExtraCycles == 15)
			{
				if(!ppuCycle())
					exit(EXIT_SUCCESS);
				ppuExtraCycles = 0;
			}
			else
				ppuExtraCycles++;
		}
		if(ppuDrawDone())
		{
			//printf("%i\n",mCycles);
			//mCycles = 0;
			emuRenderFrame = true;
			if(fm2playRunning())
				fm2playUpdate();
			#if WINDOWS_BUILD
			emuTimesCalled++;
			DWORD end = GetTickCount();
			emuTotalElapsed += end - emuFrameStart;
			if(emuTotalElapsed >= 1000)
			{
				#if DEBUG_HZ
				printf("\r%iHz   ", emuTimesCalled);
				#endif
				emuTimesCalled = 0;
				emuTotalElapsed = 0;
			}
			emuFrameStart = end;
			#endif
			glutPostRedisplay();
			if(ppuDebugPauseFrame)
				nesPause = true;
		}
	}
	while(mainLoopPos--);
	mainLoopPos = MAIN_LOOP_RUNS;
}

void nesEmuResetApuClock(void)
{
	emuApuClock = 0;
}

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
