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
CFLAGS  := -Wall -Wextra
CFLAGS  += -std=c99
CFLAGS  += -fno-omit-frame-pointer -O0 -gdwarf-2

#
# PIE Support check
#
# In order to PBD works as expected, every source intended to be analyzed
# _should_ be built with at least -O0 and -gdwarf-2, this is not optional,
# nonetheless, it's also highly desirable that the frame pointer is not
# ommited, thus: -fno-omit-frame-pointer.
#
# Last but not least, PBD doesn't support 'Position Independent Executables'
# and all the addresses need to be known from before execution. Since some
# compilers build 'PIE' executables (yeah, redundant ;-)) by default, we
# also need to check this in execution time.
#
# ---
# This solution is not the best but at least cover GCC and CLANG...
#
PIE_SUPPORT := $(shell echo "int main(){}" | $(CC) -x c -; file a.out \
	| grep -Ec "shared|pie"; rm a.out &> /dev/null)

ifeq ($(PIE_SUPPORT), 1)
	CFLAGS += -no-pie
endif

# Source
C_SRC = $(wildcard *.c)
OBJ = $(C_SRC:.c=.o)

%.o: %.c
	$(CC) $< $(CFLAGS) -c -o $@

all: bench run_benchs

bench: $(OBJ)
	$(CC) $^ $(CFLAGS) -o $@

run_benchs: bench
	@bash run-benchs.sh

clean:
	@rm -f $(OBJ) bench
