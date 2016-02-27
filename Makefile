CC=sdcc
CFLAGS=-mz80 --std-sdcc99 --reserve-regs-iy --max-allocs-per-node 30000
LKFLAGS=--code-loc 0x9D9B --data-loc 0 --no-std-crt0
EXEC=brainfk
LDIR=lib

.PHONY: all $(LDIR) clean

all: $(EXEC).8xp $(LDIR)

$(EXEC).8xp: $(EXEC).bin
	binpac8x.py -O brainfk $(EXEC).bin

$(EXEC).bin: $(EXEC).ihx
	hex2bin $(EXEC).ihx

$(EXEC).ihx: $(EXEC).c $(LDIR)
	$(CC) $(CFLAGS) $(LKFLAGS) $(LDIR)/tios_crt0.rel $(LDIR)/c_ti83p.lib $(EXEC).c

$(LDIR):
	$(MAKE) -C $(LDIR)

clean:
	rm -f *.8xp *.bin *.ihx *.asm *.lst *.sym *.lk *.map *.noi *.rel
	$(MAKE) -C $(LDIR) clean
