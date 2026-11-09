EE_BIN = punto-cero.elf
EE_OBJS = src/ps2_platform.o src/game.o src/audio.o src/audsrv_irx.o
EE_INCS += -I$(GSKIT)/include -I$(PS2SDK)/ports/include
EE_LDFLAGS += -L$(GSKIT)/lib
EE_LIBS = -lgskit -ldmakit -lpad -laudsrv -lm -lc -lkernel

all: $(EE_BIN)

clean:
	rm -f $(EE_BIN) $(EE_OBJS) src/audsrv_irx.c

# Embebe el modulo de audio del IOP dentro del ELF (no hace falta ningun archivo externo)
src/audsrv_irx.c: $(PS2SDK)/iop/irx/audsrv.irx
	$(PS2SDK)/bin/bin2c $< $@ audsrv_irx

run: $(EE_BIN)
	ps2client execee host:$(EE_BIN)

reset:
	ps2client reset

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
