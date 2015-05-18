#!/bin/bash

cd ~/Desktop/Silph

make
mv cube.elf ~/Dropbox/cube.elf

rm main.o
rm bg.c
rm flower.c
rm player_0_0.c