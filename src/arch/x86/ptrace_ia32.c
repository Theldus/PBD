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

#include "ptrace.h"

/**
 * Architecture dependent ptrace helper functions.
 */

#if !defined(__i386__)
	#error "ptrace_ia32.c should only be included on x86 builds!"
#endif

/**
 * @brief Reads the current program counter (EIP in x86) from
 * the child process.
 *
 * @param child Child process.
 *
 * @return Returns the child program counter.
 */
uintptr_t pt_readregister_pc(pid_t child)
{
	return (ptrace(PTRACE_PEEKUSER, child, 4 * EIP, NULL));
}

/**
 * @brief Sets the program counter @p pc for the specified
 * @p child.
 *
 * @param child Child process.
 * @param pc New program counter.
 */
void pt_setregister_pc(pid_t child, uintptr_t pc)
{
	ptrace(PTRACE_POKEUSER, child, 4 * EIP, pc);
}

/**
 * @brief Reads the current base pointer (EBP in x86) from
 * the child process.
 *
 * @param child Child process.
 *
 * @return Returns the child base pointer.
 */
uintptr_t pt_readregister_bp(pid_t child)
{
	return (ptrace(PTRACE_PEEKUSER, child, 4 * EBP, NULL));
}

/**
 * @brief Considering the child process is inside
 * the function prologue, retrieves the returning
 * address.
 *
 * @param child Child process.
 *
 * @return Returns the 'return' address.
 */
uintptr_t pt_readreturn_address(pid_t child)
{
	uintptr_t sp;
	sp = ptrace(PTRACE_PEEKUSER, child, 4 * UESP, NULL);
	return (ptrace(PTRACE_PEEKDATA, child, sp, NULL));
}
