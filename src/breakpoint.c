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
struct hashtable *bp_createlist(struct array *lines)
{
	struct hashtable *breakpoints; /* Breakpoints list. */
	struct breakpoint *bp;         /* Breakpoint.       */

	/* Initialize our list. */
	hashtable_init(&breakpoints, NULL);

	for (int i = 0; i < (int) array_size(&lines); i++)
	{
		struct dw_line *l;
		l = array_get(&lines, i, NULL);

		if (l->line_type != LBEGIN_STMT)
			continue;

		/* Allocates a new break point. */
		bp = malloc(sizeof(struct breakpoint));
		bp->addr = l->addr;
		bp->original_byte = 0;
		bp->line_no = l->line_no;

		/* Add to our hashmap. */
		hashtable_add(&breakpoints, (void*)bp->addr, bp);
	}

	return (breakpoints);
}

/**
 * Create and adds a breakpoint given the address, the breakpoint list
 * and the child process into the breakpoint list.
 * @param addr Target breakpoint address.
 * @param bp Breakpoint list.
 * @param child Child process.
 */
int bp_createbreakpoint(uint64_t addr, struct hashtable *bp, pid_t child)
{
	struct breakpoint *b; /* Breakpoint. */

	/* Check if already exists. */
	if (bp_findbreakpoint(addr, bp) != NULL)
		return (-1);

	/* Allocate our new breakpoint. */
	b = malloc(sizeof(struct breakpoint));
	b->addr = addr;
	b->original_byte = pt_readmemory64(child, b->addr) & 0xFF;
	b->line_no = 0;

	/* Add to our list. */
	hashtable_add(&bp, (void*)b->addr, b);

	/* Insert breakpoint. */
	bp_insertbreakpoint(b, child);

	return (0);
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

	insn = pt_readmemory64(child, bp->addr);
	insn = (insn & ~0xFF) | BP_OPCODE;
	pt_writememory64(child, bp->addr, insn);

	return (0);
}

/**
 * Insert breakpoints into the child process memory.
 *
 * @param bp Breakpoints list.
 * @param child Child process.
 *
 * @return Returns 0 if success and a negative number
 * otherwise.
 *
 * @note This function should not be confused with his
 * sister bp_insertbreakpoint() (singular) that adds
 * a single breakpoint into memory.
 */
int bp_insertbreakpoints(struct hashtable *bp, pid_t child)
{
	struct breakpoint *b_k; /* Current breakpoint key.   */
	struct breakpoint *b_v; /* Current breakpoint value. */
	((void)b_k);

	/* If invalid. */
	if (bp == NULL)
		return (-1);

	HASHTABLE_FOREACH(bp, b_k, b_v,
	{
		b_v->original_byte = pt_readmemory64(child, b_v->addr) & 0xFF;
		if (bp_insertbreakpoint(b_v, child))
			return (-1);
	});

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
 */
struct breakpoint *bp_findbreakpoint(
	uint64_t addr, struct hashtable *bp_list)
{
	struct breakpoint *b;
	b = hashtable_get(&bp_list, (void*)addr);
	return (b);
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
	insn = pt_readmemory64(child, bp->addr);
	insn = (insn & ~0xFF) | bp->original_byte;
	pt_writememory64(child, bp->addr, insn);

	/* Execute and wait. */
	pt_continue_single_step(child);
	pt_waitchild();

	/* Enables the breakpoint again. */
	insn = (insn & ~0xFF) | BP_OPCODE;
	pt_writememory64(child, bp->addr, insn);
}

/**
 * @brief Deallocates all the breakpoints remaining.
 *
 * @param breakpoints Breakpoints list.
 */
void bp_list_free(struct hashtable *breakpoints)
{
	hashtable_finish(&breakpoints, 1);
}
