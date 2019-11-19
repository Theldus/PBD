# MIT License
#
# Copyright (c) 2019 Davidson Francis <davidsondfgl@gmail.com>
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

# Some distros like Debian Buster (10) and Ubuntu Bionic (18.04)
# uses a different path for libdwarf, so lets use it.
INCLUDE_DWARF = /usr/include/libdwarf/

CFLAGS   = -Wall -Wextra
CFLAGS  += -I $(INCLUDE) -I $(INCLUDE_DWARF)
CFLAGS  += -std=c99 -g
LDFLAGS  = -ldwarf -lm

C_SRC = $(wildcard *.c)
OBJ = $(C_SRC:.c=.o)

# Prefix
ifeq ($(PREFIX),)
    PREFIX := /usr/local
endif

# Paths
BINDIR = $(PREFIX)/bin

%.o: %.c
	$(CC) $< $(CFLAGS) -c -o $@

all: pbd

# Main program
pbd: $(OBJ)
	$(CC) $^ $(CFLAGS) $(LDFLAGS) -o $@

# Tests
test:
	$(MAKE) -C tests/

# Install rules
install: pbd
	@# Binary file
	install -d $(DESTDIR)$(BINDIR)
	install -m 755 pbd $(DESTDIR)$(BINDIR)

# Uninstall rules
uninstall:
	rm -f $(DESTDIR)$(BINDIR)/pbd

clean:
	@rm -f $(OBJ) pbd
	@$(MAKE) clean -C tests/
