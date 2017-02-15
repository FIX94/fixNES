#!/bin/sh

#Need to replace this with a makefile

gcc main.c apu.c audio.c audio_fds.c audio_mmc5.c audio_vrc6.c alhelpers.c cpu.c ppu.c mem.c input.c mapper.c mapperList.c fm2play.c mapper/*.c -DFREEGLUT_STATIC -lglut -lopenal -lGL -lGLU -lm -Wall -Wextra -O3 -msse -mfpmath=sse -ffast-math -s -o fixNES
echo "Succesfully built fixNES"

