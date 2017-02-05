#!/bin/sh

#Need to replace this with a makefile

gcc main.c apu.c audio.c alhelpers.c cpu.c ppu.c mem.c input.c mapper.c fm2play.c mapper/*.c -DFREEGLUT_STATIC -lglut -lopenal -lGL -lGLU -lm -Wall -Wextra -O3 -s -o fixNES
echo "Succesfully built fixNES"

