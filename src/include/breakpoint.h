/*
 * MIT License
 *
 * Copyright (c) 2019 Davidson Francis <davidsondfgl@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef BREAKPOINT_H
#define BREAKPOINT_H

	#include "ptrace.h"
	#include <stdio.h>
	#include <string.h>
	#include <stdlib.h>

	/*
	 * Break point opcode
	 * Note that this is highly architecture
	 * dependent.. think a generic way to do
	 * this.
	 */
	#define BP_OPCODE 0xCC

	/**
	 * @brief Breakpoint structure.
	 */
	struct breakpoint
	{
		uint64_t addr;
		uint8_t original_byte;
		unsigned line_no;
	};

	extern struct array *bp_createlist(struct array *lines, pid_t pid);
	
	extern int bp_createbreakpoint(uint64_t addr, struct array *bp, pid_t child);

	extern int bp_insertbreakpoint(struct breakpoint *bp, pid_t child);

	extern int bp_insertbreakpoints(struct array *bp, pid_t child);
	
	extern struct breakpoint *bp_findbreakpoint(uint64_t addr,
		struct array *bp_list);
	
	extern void bp_skipbreakpoint(struct breakpoint *bp, pid_t child);

#endif /* BREAKPOINT_H */
