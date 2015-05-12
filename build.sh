#!/bin/bash

cd ~/Desktop/Silph
rm main.o
make
mv cube.elf ~/Dropbox/cube.elf
rm main.o