#!/bin/bash

cd ~/Desktop/Silph
rm main.o

rm bg.c
rm flower.c

make
mv cube.elf ~/Dropbox/cube.elf
rm main.o