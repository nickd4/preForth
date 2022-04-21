# Makefile for preForth and seedForth
#
# make bootstrap should produce two identical files: preForth1$(DOTASM) and preForth$(DOTASM) 

# ------------------------------------------------------------------------

# Set HOSTFORTH to the Forth system that generates the initial preForth
HOSTFORTH=gforth
# HOSTFORTH=sf   # SwiftForth >3.7

#UNIXFLAVOUR=$(shell uname -s)
#DOTASM=.asm
#DOTEXE=
#DOTSLASH=./

UNIXFLAVOUR=z80
DOTASM=.asm
DOTEXE=.bin
# note: below definition has a trailing space
DOTSLASH=LD_LIBRARY_PATH=lib ./z80 

# for compiling Z80 emulator
CFLAGS=-g -Wall -Iinclude
LDFLAGS=-g -Llib

# ------------------------------------------------------------------------

.PHONY=all
all: z80 preForth$(DOTEXE) seedForth$(DOTEXE) seedForthDemo.seed seedForthInteractive.seed

.PHONY=test
test: runseedforthdemo runseedforthinteractive

.PHONY=runseedforthdemo
runseedforthdemo: seedForth$(DOTEXE) seedForthDemo.seed
	cat seedForthDemo.seed | $(DOTSLASH)seedForth$(DOTEXE)

.PHONY=runseedfortinteractive
runseedforthinteractive: seedForth$(DOTEXE) seedForthInteractive.seed
	./seed$(DOTEXE)

z80: z80.o
	${CC} ${LDFLAGS} -o $@ $^ -lz80

seedForth-z80$(DOTASM): seedForth-z80.pre preForth$(DOTEXE)
	cat seedForth-z80.pre | $(DOTSLASH)preForth$(DOTEXE) >seedForth-z80$(DOTASM)

# preForth connected to stdin - output to preForth$(DOTASM)
preForth$(DOTASM): preForth.pre preForth-z80-backend.pre load-z80-preForth.fs
	cat preForth-z80-rts.pre preForth-rts.pre preForth-z80-backend.pre preForth.pre \
	    | $(HOSTFORTH) load-z80-preForth.fs >preForth$(DOTASM)

%$(DOTASM): %.pre preForth$(DOTEXE) preForth-z80-rts.pre preForth-rts.pre
	cat preForth-z80-rts.pre preForth-rts.pre $< | $(DOTSLASH)preForth$(DOTEXE) >$@

%$(DOTEXE): %-$(UNIXFLAVOUR)
	cp $< $@

# assemble and link executable on linux
%-Linux: %$(DOTASM)
	fasm $< $@.o
	LDEMULATION=elf_i386 ld -arch i386 -o $@ \
	   -dynamic-linker /lib32/ld-linux.so.2 \
	   /usr/lib/i386-linux-gnu/crt1.o /usr/lib/i386-linux-gnu/crti.o \
	   $@.o \
	   -lc /usr/lib/i386-linux-gnu/crtn.o
	# rm $@.o

# assemble and link executable on MacOS
%-Darwin: %$(DOTASM)
	fasm $< $@.o
	objconv -fmacho32 -nu $@.o $@_m.o
	ld -arch i386 -macosx_version_min 10.6 -o $@ \
	  $@_m.o /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/usr/lib/crt1.o /usr/lib/libc.dylib
	# rm $@.o $@_m.o

# assemble and link executable on z80
%-z80: %$(DOTASM)
	cp $< $@$(DOTASM)
	bin/asz80 -l -o $@$(DOTASM)
	bin/aslink -n -m -u -i $@.ihx $@.rel
	python3 ~/.local/bin/hex2bin.py $@.ihx $@
	# rm $@.ihx $@.map $@.rst $@.hlr $@.lst $@.rel $@$(DOTASM)

# run preForth on its own source code to perform a bootstrap 
# should produce identical results
bootstrap: preForth preForth-z80-backend.pre preForth.pre preForth$(DOTASM)
	cat preForth-z80-rts.pre preForth-rts.pre preForth-z80-backend.pre preForth.pre\
           | $(DOTSLASH)preForth$(DOTEXE) >preForth1$(DOTASM)
	cmp preForth$(DOTASM) preForth1$(DOTASM)

# preForth connected to stdin - output to stdout
.PHONY=visible-bootstrap
visible-bootstrap: preForth$(DOTEXE) preForth-z80-backend.pre preForth.pre 
	cat preForth-z80-backend.pre preForth.pre | $(DOTSLASH)preForth$(DOTEXE)

# ------------------------------------------------------------------------
# Docker support (for Linux version)
# ------------------------------------------------------------------------
# create a linux image based on Dockerfile
.PHONY=docker-image
docker-image: Dockerfile
	docker build -t preforth .

# run the docker image
.PHONY=run
rundocker: docker-image
	docker run -i -t --rm preforth /preForth/seed
# ------------------------------------------------------------------------

# ------------------------------------------------------------------------
# seedForth
# ------------------------------------------------------------------------
seedForth$(DOTASM): seedForth-z80.pre preForth$(DOTEXE)
	cat seedForth-z80.pre | $(DOTSLASH)preForth$(DOTEXE) >seedForth$(DOTASM)

seedForth: seedForth.$(UNIXFLAVOUR)
	cp seedForth.$(UNIXFLAVOUR) seedForth

%.seed: %.seedsource seedForth-tokenizer.fs
	gforth seedForth-tokenizer.fs $<


.PHONY=clean
clean:
	rm -f *$(DOTASM) *.o *.fas *.s *.c *-Darwin *-Linux preForthdemo preForth forth seedForth seedForthDemo.seed seedForthInteractive.seed
