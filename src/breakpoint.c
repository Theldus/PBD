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

#include "breakpoint.h"
#include "dwarf_helper.h"

/**
 * @brief Creates a breakpoint list by the given @P lines array
 * and the @p pid child process.
 *
 * @param lines Lines array.
 * @param pid Child process.
 *
 * @return Returns a breakpoint list.
 */
struct array *bp_createlist(struct array *lines, pid_t pid)
{
	struct array *breakpoints; /* Breakpoints list. */
	struct breakpoint *bp;     /* Breakpoint.       */

	/* Initialize our list. */
	array_init(&breakpoints);

	for (int i = 0; i < (int) array_size(&lines); i++)
	{
		struct dw_line *l;
		l = array_get(&lines, i, NULL);

		if (l->line_type != LBEGIN_STMT)
			continue;
		
		/* Allocates a new break point. */
		bp = malloc(sizeof(struct breakpoint));
		bp->addr = l->addr;
		bp->original_byte = pt_readmemory_long(pid, bp->addr) & 0xFF;
		bp->line_no = l->line_no;

		//printf("read: %x / addr: %x\n", bp->original_byte, bp->addr);

		/* Add to our array. */
		array_add(&breakpoints, bp);
	}

	return (breakpoints);
}

/**
 * Insert a breakpoint into a given address.
 * @param bp Breakpoint to be added.
 * @param child Child process.
 * @return Returns 0 if success and a negative number
 * otherwise.
 */
int bp_insertbreakpoint(struct breakpoint *bp, pid_t child)
{
	long insn; /* Address opcode. */

	if (bp->addr == 0)
		return (-1);

	insn = pt_readmemory_long(child, bp->addr);
	insn = (insn & ~0xFF) | BP_OPCODE;
	pt_writememory_long(child, bp->addr, insn);

	return (0);
}

/**
 * Insert breakpoints into the child process memory.
 * @param bp Breakpoints list.
 * @param child Child process.
 * @return ...
 */
int bp_insertbreakpoints(struct array *bp, pid_t child)
{
	struct breakpoint *b; /* Current breakpoint. */

	if (bp == NULL || array_size(&bp) <= 0)
		return (-1);

	for (int i = 0; i < (int) array_size(&bp); i++)
	{
		b = array_get(&bp, i, NULL);
		if (bp_insertbreakpoint(b, child))
			return (-1);
	}

	return (0);
}

/**
 * @brief By a given address @p addr, tries to find a breakpoint
 * into the breakpoint list @p bp_list.
 *
 * @param addr Address that will be searched.
 * @param bp_list Target breakpoint list.
 *
 * @return Returns the underlying breakpoint structure if found,
 * or NULL otherwise.
 *
 * @note TODO: Use hashtable instead of array, this function is
 * one of the bottlenecks.
 */
struct breakpoint *bp_findbreakpoint(
	uint64_t addr, struct array *bp_list)
{
	struct breakpoint *b;

	for (int i = 0; i < (int) array_size(&bp_list); i++)
	{
		b = array_get(&bp_list, i, NULL);
		if (b->addr == addr)
			return (b);
	}

	return (NULL);
}

/**
 * @brief If the child process if stopped inside a breakpoint,
 * executes the original instruction and enables to breakpoint
 * again.
 *
 * @param bp_list Breakpoint list.
 * @param child Child process.
 */
void bp_skipbreakpoint(struct breakpoint *bp, pid_t child)
{
	uint64_t insn;

	/* If a valid breakpoint, restart instruction. */
	pt_setregister_pc(child, bp->addr);

	/* Insert the original instruction. */
	insn = pt_readmemory_long(child, bp->addr);
	insn = (insn & ~0xFF) | bp->original_byte;
	pt_writememory_long(child, bp->addr, insn);

	/* Execute and wait. */
	pt_continue_single_step(child);
	pt_waitchild();

	/* Enables the breakpoint again. */
	insn = (insn & ~0xFF) | BP_OPCODE;
	pt_writememory_long(child, bp->addr, insn);
}
