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
#include "function.h"
#include "ptrace.h"
#include "util.h"
#include "variable.h"
#include "hashtable.h"
#include "line.h"
#include <inttypes.h>

/* Depth, useful for recursive analysis. */
int depth;

/* Dwarf Utils current context. */
struct dw_utils dw;

/* Program lines. */
struct array *lines;

/* Breakpoints. */
struct hashtable *breakpoints;

/* Function context. */
static struct array *context;

/* Debugged file name. */
static char *filename;

/**
 * @brief Parses all the lines and variables for the target
 * file and function.
 *
 * @param file Target file to be analyzed.
 * @param function Target function to be analyzed.
 */
int setup(const char *file, const char *function)
{
	struct function *f; /* First function context. */

	/* Initializes dwarf. */
	dw_init(file, &dw);

	/* Searches for the target function */
	dw_get_address_by_function(&dw, function);

	/* Initialize first function context. */
	array_init(&context);
		f = calloc(1, sizeof(struct function));
	array_add(&context, f);

	/* Parses all variables, lines and filename. */
	f->vars  = dw_get_all_variables(&dw);
	lines    = dw_get_all_lines(&dw);
	filename = dw_get_source_file(&dw);

	line_read_source(filename);

	depth = 0;

	/*
	 * Since all dwarf analysis are static, when the analysis
	 * is over, we can already free the dwarf context.
	 */
	dw_finish(&dw);
	return (0);
}

/**
 * @brief Deallocates everything.
 */
void finish(void)
{
	/* Deallocate variables. */
	fn_free( array_get(&context, 0, NULL) );

	/* Deallocate context. */
	array_finish(&context);

	/* Deallocate lines and filename. */
	dw_lines_array_free(lines);
	line_free_source();
	free(filename);

	/* Deallocate breakpoints. */
	bp_list_free(breakpoints);
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
	struct breakpoint *prev_bp; /* Previous breakpoint.   */
	struct function *f;         /* Context function.      */

	/*
	 * Setup everything and get ready to analyze.
	 * If something fails, the program will abort
	 * before return from this function.
	 */
	setup(file, function);

	/* Tries to spawn the process. */
	if ((child = pt_spawnprocess(file)) < 0)
		QUIT(-1, "error while spawning the child process!\n");

	/* Wait child, create the breakpoint list and insert them. */
	pt_waitchild();
	{
		breakpoints = bp_createlist(lines, child);
	}

	pt_continue_single_step(child);
	init_vars = 0;
	prev_bp = NULL;

	printf("Debugging function %s:\n", function);

	/* Main loop. */
	while (pt_waitchild() != PT_CHILD_EXIT)
	{
		int current_depth;
		uint64_t pc;
		struct breakpoint *bp;

		f  = array_get_last(&context, NULL);
		pc = pt_readregister_pc(child) - 1;
		bp = bp_findbreakpoint(pc, breakpoints);
		current_depth = array_size(&context);

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
		if (pc == dw.dw_func.low_pc)
		{
			/* Allocates a new function context. */
			if (current_depth > 0 && depth > 0)
			{
				var_new_context(
					array_get(&context, current_depth - 1, NULL),
					&f,
					context
				);
			}

			/*
			 * It is important to set a breakpoint on the next instruction
			 * right after returning from the function, so it is easier
			 * to know when to enter or exit the function. Especially useful
			 * for recursive analysis.
			 */
			bp_createbreakpoint(f->return_addr = pt_readreturn_address(child),
				breakpoints, child);

			/* Executes that breakpoint. */
			bp_skipbreakpoint(bp, child);

			depth++;
			prev_bp = bp;
			init_vars = 1;
			pt_continue(child);
			continue;
		}

		/*
		 * Returning from a previous call.
		 */
		if (pc == f->return_addr)
		{
			fn_printf(current_depth, 0, "[depth: %d] Returning to function...\n\n",
				current_depth);

			/*
			 * Since we're returning from an previous call, we also
			 * need to free all (possible) arrays allocated first.
			 */
			var_deallocate_context(f->vars, context, current_depth);

			/* Decrements the context and continues. */
			depth--;
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
			putchar('\n');
			fn_printf(current_depth, 0, "[depth: %d] Entering function...\n",
				current_depth);

			init_vars = 0;
			var_initialize(f->vars, child);
		}

		/* Do something. */
		if (prev_bp != NULL)
			var_check_changes(prev_bp, f->vars, child, current_depth);

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
	do_analysis(argv[1], argv[2]);

	return (0);
}
