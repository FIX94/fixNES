#!/bin/sh

#Need to replace this with a makefile

gcc -DZIPSUPPORT main.c apu.c audio.c audio_fds.c audio_mmc5.c audio_vrc6.c audio_vrc7.c audio_n163.c audio_s5b.c alhelpers.c cpu.c ppu.c mem.c input.c mapper.c mapperList.c fm2play.c vrc_irq.c mapper/*.c unzip/*.c -DFREEGLUT_STATIC -lglut -lopenal -lGL -lGLU -lm -Wall -lz -Wextra -DDO_INLINE_ATTRIBS=1 -Wno-attributes -O3 -flto -s -o fixNES
echo "Succesfully built fixNES"

