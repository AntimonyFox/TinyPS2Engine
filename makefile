# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# Copyright 2001-2004, ps2dev - http://www.ps2dev.org
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
#

PS2DEV=/usr/local/ps2dev
PATH=$PATH:$PS2DEV/bin
PATH=$PATH:$PS2DEV/ee/bin
PATH=$PATH:$PS2DEV/iop/bin
PATH=$PATH:$PS2DEV/dvp/bin
PS2SDK=$PS2DEV/ps2sdk
PATH=$PATH:$PS2SDK/bin
PATH=$PATH:/bin

SPRITES = $(foreach number, $(wildcard sprites/*), $(notdir $(number)))
EE_BIN = cube.elf
EE_OBJS = main.o
EE_LIBS = -ldraw -lgraph -lmath3d -lmf -lpacket -ldma -lpad -lc -lm

all: $(SPRITES) $(EE_BIN)
	ee-strip --strip-all $(EE_BIN)

%.raw:
	@echo "compile $@"
	@bin2c sprites/$*.raw $*.ci $*
	@echo "#include \"$*.ci\"" >> sprites.c
	@echo "extern unsigned char $*[];" >> sprites.c


clean:
	rm -f *.elf *.o *.a *.ci sprites.c

run: $(EE_BIN)
	ps2client execee host:$(EE_BIN)

reset:
	ps2client reset

include Makefile.pref
include Makefile.eeglobal
