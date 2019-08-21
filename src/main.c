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
#include "ptrace.h"
#include "util.h"
#include "variable.h"
#include <inttypes.h>

/**
 * @brief Current function context.
 */
struct context
{
	/* Depth, useful for recursive analysis. */
	int depth;

	/* Dwarf Utils current context. */
	struct dw_utils dw;

	/* All variables found. */
	struct array *vars;

	/* Program lines. */
	struct array *lines;

	/* Breakpoints. */
	struct array *breakpoints;

	/* Return address. */
	uint64_t return_addr;
} context;

/**
 * @brief Dump all variables found in the target function.
 *
 * @param vars Variables array.
 */
void dump_vars(struct array *vars)
{
	for (int i = 0; i < (int) array_size(&vars); i++)
	{
		struct dw_variable *v;
		v = array_get(&vars, i, NULL);

		printf("Variable found: %s\n", v->name);
		printf("  Scope: %d\n", v->scope);

		/* Location. */
		if (v->scope == VLOCAL)
			printf("  Location: %d\n", (int)v->location.fp_offset);
		else
			printf("  Location: %" PRIx64 "\n", v->location.address);

		printf("  Size (bytes): %zu\n", v->byte_size);
		printf("  Var type:     %d\n", v->type.var_type);
		printf("  Var encoding: %d\n", v->type.encoding);

		/* Check if array. */
		if (v->type.array.dimensions > 0)
		{
			printf("  Array (%d dimensions) (size per element: %zu) (type: %d): \n",
				v->type.array.dimensions, v->type.array.size_per_element,
				v->type.array.var_type);
			
			printf("    ");
			for (int i = 0; i < v->type.array.dimensions; i++)
				printf("[%d], ", v->type.array.elements_per_dimension[i]);

			printf("\n");
		}

		printf("\n");
	}
}

/**
 * @brief Dump all lines found in the target function.
 *
 * @param lines Lines array.
 */
void dump_lines(struct array *lines)
{
	for (int i = 0; i < (int) array_size(&lines); i++)
	{
		struct dw_line *l;
		l = array_get(&lines, i, NULL);
		printf("line: %d / address: %" PRIx64 " / type: %d\n",
			l->line_no, l->addr, l->line_type);
	}
}

/**
 * @brief Parses all the lines and variables for the target
 * file and function.
 *
 * @param file Target file to be analyzed.
 * @param function Target function to be analyzed.
 */
int setup(const char *file, const char *function)
{
	/* Initializes dwarf. */
	dw_init(file, &context.dw);

	/* Searches for the target function */
	get_address_by_function(&context.dw, function);

	/* Parses all variables and lines. */
	context.vars = get_all_variables(&context.dw);
	context.lines = get_all_lines(&context.dw);
	context.depth = 0;

	/* 
	 * Since all dwarf analysis are static, when the analysis
	 * is over, we can already free the dwarf context.
	 */
	dw_finish(&context.dw);
	return (0);
}

/**
 * @brief Deallocates everything.
 */
void finish(void)
{
	/* Deallocate variables. */
	int size = (int) array_size(&context.vars);
	for (int i = 0; i < size; i++)
	{
		struct dw_variable *v;
		v = array_remove(&context.vars, 0, NULL);

		if (v->type.var_type == TARRAY)
			if (v->value.p_value != NULL)
				free(v->value.p_value);

		free(v->name);
		free(v);
	}
	array_finish(&context.vars);

	/* Deallocate lines. */
	size = (int) array_size(&context.lines);
	for (int i = 0; i < size; i++)
	{
		struct dw_lines *l;
		l = array_remove(&context.lines, 0, NULL);
		free(l);
	}
	array_finish(&context.lines);

	/* Deallocate breakpoints. */
	size = (int) array_size(&context.breakpoints);
	for (int i = 0; i < size; i++)
	{
		struct breakpoint *p;
		p = array_remove(&context.breakpoints, 0, NULL);
		free(p);
	}
	array_finish(&context.breakpoints);
}

/**
 * Main routine
 *
 * @param file File to be analyzed.
 * @param function Function to be analyzed.
 */
void do_analysis(const char *file, const char *function)
{
	pid_t child;                /* Spawned child process. */
	int init_vars;              /* Initialize vars flags. */
	struct breakpoint *prev_bp; /* Previous breakpoint. */

	/*
	 * Setup everything and get ready to analyze.
	 * If something fails, the program will abort
	 * before return from this function.
	 */
	setup(file, function);

	/* Tries to spawn the process. */
	if ((child = pt_spawnprocess(file)) < 0)
		quit(-1, "do_analysis: error while spawning the child process!\n");
	
	/* Wait child and create the breakpoint list. */
	pt_waitchild();
	{
		context.breakpoints = bp_createlist(context.lines, child);
		if (bp_insertbreakpoints(context.breakpoints, child))
			quit(-1, "do_analysis: error while inserting breakpoints!\n");
	}
	pt_continue_single_step(child);
	init_vars = 0;
	prev_bp = NULL;

	printf("Debugging function %s:\n", function);

	/* Main loop. */
	while (pt_waitchild() != PT_CHILD_EXIT)
	{
		uint64_t pc;
		struct breakpoint *bp;
		
		pc = pt_readregister_pc(child) - 1;
		bp = bp_findbreakpoint(pc, context.breakpoints);

		/* If not valid breakpoint, continues. */
		if (bp == NULL)
		{
			pt_continue(child);
			continue;
		}

		/* 
		 * Since we are the very first instruction of the
		 * function, there is nothing to analyze here, yet.
		 */
		if (pc == context.dw.dw_func.low_pc)
		{
			if (context.depth++ > 0)
				quit(-1, "do_analysis: recursive analysis not supported yet!\n");

			/*
			 * It is important to set a breakpoint on the next instruction
			 * right after returning from the function, so it is easier
			 * to know when to enter or exit the function. Especially useful
			 * for recursive analysis.
			 */
			bp_createbreakpoint(context.return_addr = pt_readreturn_address(child),
				context.breakpoints, child);

			/* Executes that breakpoint. */
			bp_skipbreakpoint(bp, child);

			prev_bp = bp;
			init_vars = 1;
			pt_continue(child);
			continue;
		}

		/*
		 * Returning from a previous call.
		 */
		if (pc == context.return_addr)
		{
			/*
			 * Since we're returning from an previous call, we also
			 * need to free all (possible) arrays allocated first.
			 */
			for (int i = 0; i < (int) array_size(&context.vars); i++)
			{
				struct dw_variable *v;
				v = array_get(&context.vars, i, NULL);
				if (v->type.var_type == TARRAY)
				{
					free(v->value.p_value);
					v->value.p_value = NULL;
				}
			}

			/* Decrements the context and continues. */
			context.depth--;
			bp_skipbreakpoint(bp, child);
			pt_continue(child);
			continue;
		}

		/*
		 * If init_vars is set, means that its time to finally
		 * initialize the vars right after the prologue in the
		 * previous iteration.
		 *
		 * Even if the values are 'wrong', this ensures that we
		 * have a knowlable value in beforehand before start
		 * comparing values.
		 */
		if (init_vars)
		{
			printf("\n[depth: %d] Entering function...\n", context.depth);
			init_vars = 0;
			var_initialize(context.vars, child);
		}

		/* Do something. */
		if (prev_bp != NULL)
			var_check_changes(prev_bp, context.vars, child);

		prev_bp = bp;

		/* Executes that breakpoint. */
		bp_skipbreakpoint(bp, child);

		/* Continue. */
		pt_continue(child);
	}

	/* Finish everything. */
	finish();
}

/**
 * Entry point.
 */
int main(int argc, char **argv)
{
	do_analysis("tests/test", "func1");

	return (0);
}
