# MIT License
#
# Copyright (c) 2019-2020 Davidson Francis <davidsondfgl@gmail.com>
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

CC ?= gcc
INCLUDE = $(CURDIR)/include

# Sparse lib
INCLUDE_SPARSE += $(CURDIR)/sparse/

# Some distros like Debian Buster (10) and Ubuntu Bionic (18.04)
# uses a different path for libdwarf, so lets use it.
INCLUDE_DWARF = /usr/include/libdwarf/

CFLAGS   = -Wall -Wextra -Werror
CFLAGS  += -I $(INCLUDE) -I $(INCLUDE_DWARF) -I $(INCLUDE_SPARSE)
CFLAGS  += -std=c99 -g -O3
LDFLAGS  = -ldwarf -lm

# Machine architecture
ARCH := $(shell uname -m)
ifeq ($(ARCH), x86_64)
    ifneq ($(filter -m32,$(CFLAGS)),)
        $(warning Enabling cross-compiling mode (x86_64 building 32 bit...))
        ARCH = x86
    else
        ARCH = x86_64
    endif
else
    ifneq ($(filter %86,$(ARCH)),)
        ARCH = x86
    else
        $(error PBD currently only supports x86/x86_64 architectures!)
    endif
endif

INCLUDE += -I $(CURDIR)/arch/
INCLUDE += -I $(CURDIR)/arch/$(ARCH)/
C_SRC =  $(wildcard *.c)
C_SRC += $(wildcard arch/$(ARCH)/*.c)

# Conditionally add ASM files to the identified arch
ifeq ($(ARCH), x86_64)
	AVX2_SUPPORT := $(shell printf "vpcmpeqb %%ymm1, %%ymm1, %%ymm1\n" \
		| if (gcc -x assembler -c -o .avx2.o -) &>/dev/null;           \
			then            \
				echo "yes"; \
			else            \
				echo "no";  \
			fi;             \
			rm .avx2.o 2> /dev/null)

	ifeq ($(AVX2_SUPPORT), yes)
		CFLAGS += -DCAN_BUILD_AVX2
		ASM_SRC += $(wildcard arch/x86_64/asm/*.S)
	else
		ASM_SRC += $(wildcard arch/x86_64/asm/sse2.S)
	endif
endif

OBJ =  $(C_SRC:.c=.o)
OBJ += $(ASM_SRC:.S=.o)

# Paths
PREFIX ?= /usr/local
BINDIR  = $(PREFIX)/bin
MANDIR  = $(PREFIX)/man

# Pretty print
Q := @
ifeq ($(V), 1)
	Q :=
endif

# C Files
%.o: %.c
	@echo "  CC      $@"
	$(Q)$(CC) $< $(CFLAGS) -c -o $@

# ASM Files
%.o: %.S
	@echo "  CC      $@"
	$(Q)$(CC) $< $(CFLAGS) -c -o $@

all: pbd

# Sparse library
sparse/libsparse.a: sparse/Makefile
	$(MAKE) -C sparse/ HAVE_SQLITE=no libsparse.a

sparse/Makefile:
	$(warning Sparse submodule not fetched, fetching...)
	git submodule update --init

# Main program
pbd: $(OBJ) sparse/libsparse.a
	@echo "  LD      $@"
	$(Q)$(CC) $^ $(CFLAGS) $(LDFLAGS) -o $@

# Tests
test: pbd
	$(MAKE) -C tests/

# Benchs
bench: pbd
	$(MAKE) -C benchs/

# Install rules
install: pbd
	@# Binary file
	install -d $(DESTDIR)$(BINDIR)
	install -m 755 pbd $(DESTDIR)$(BINDIR)
	@#Manpages
	install -d $(DESTDIR)$(MANDIR)/man1
	install -m 0644 $(CURDIR)/man/man1/pbd.1 $(DESTDIR)$(MANDIR)/man1/
	gzip $(DESTDIR)$(MANDIR)/man1/pbd.1

# Uninstall rules
uninstall:
	rm -f $(DESTDIR)$(BINDIR)/pbd
	rm -f $(DESTDIR)$(MANDIR)/man1/pbd.1.gz

clean:
	@echo "  CLEAN"
	@rm -f $(OBJ) pbd
	@$(MAKE) clean -C tests/
	@$(MAKE) clean -C benchs/
	@$(MAKE) clean -C sparse/ HAVE_SQLITE=no
