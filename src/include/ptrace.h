/*
 * MIT License
 *
 * Copyright (c) 2019-2020 Davidson Francis <davidsondfgl@gmail.com>
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

#ifndef PTRACE_H
#define PTRACE_H

	#define _GNU_SOURCE
	#include <sys/ptrace.h>
#ifdef __GLIBC__
	#include <sys/uio.h>
#endif
	#include <sys/reg.h>
	#include <sys/user.h>
	#include <sys/types.h>
	#include <sys/wait.h>
	#include <inttypes.h>
	#include <unistd.h>

	#include <stdio.h>
	#include <string.h>
	#include <stdlib.h>

	/* Child Exit Signal. */
	#define PT_CHILD_EXIT 1

	extern int pt_spawnprocess(const char *file, char **argv);
	extern int pt_waitchild(void);
	extern int pt_continue(pid_t child);
	extern int pt_continue_single_step(pid_t child);
	extern uint64_t pt_readregister_pc(pid_t child);
	extern void pt_setregister_pc(pid_t child, uint64_t pc);
	extern uint64_t pt_readregister_bp(pid_t child);
	extern uint64_t pt_readreturn_address(pid_t child);
	extern char *pt_readmemory(pid_t child, uint64_t addr, size_t len);
	extern void pt_writememory(pid_t child, uint64_t addr, char *data, size_t len);
	extern uint64_t pt_readmemory64(pid_t child, uint64_t addr);
	extern void pt_writememory64(pid_t child, uint64_t addr, uint64_t data);

#endif /* PTRACE_H */
