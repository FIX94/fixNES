#!/bin/sh

#Need to replace this with a makefile

gcc *.c mapper/*.c -DFREEGLUT_STATIC -lglut -lopenal -lGL -lGLU -lm -Wall -Wextra -O3 -s -o fixNES
echo "Succesfully built fixNES"

