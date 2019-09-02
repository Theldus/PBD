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
#include <inttypes.h>

/* Depth, useful for recursive analysis. */
int depth;

/* Dwarf Utils current context. */
struct dw_utils dw;

/* Program lines. */
struct array *lines;

/* Breakpoints. */
struct array *breakpoints;

/**
 * @brief Current function context.
 */
struct function
{
	/* All variables found. */
	struct array *vars;

	/* Return address. */
	uint64_t return_addr;
};

/* Function context. */
static struct array *context;

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
	struct function *f; /* First function context. */

	/* Initializes dwarf. */
	dw_init(file, &dw);

	/* Searches for the target function */
	get_address_by_function(&dw, function);

	/* Initialize first function context. */
	array_init(&context);
		f = calloc(1, sizeof(struct function));
	array_add(&context, f);

	/* Parses all variables and lines. */
	f->vars = get_all_variables(&dw);
	lines = get_all_lines(&dw);
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
	struct function *f; /* Context function. */

	/* Deallocate variables. */
	f = array_get(&context, 0, NULL);
	int size = (int) array_size(&f->vars);

	for (int i = 0; i < size; i++)
	{
		struct dw_variable *v;
		v = array_remove(&f->vars, 0, NULL);

		if (v->type.var_type == TARRAY)
			if (v->value.p_value != NULL)
				free(v->value.p_value);

		free(v->name);
		free(v);
	}
	array_finish(&f->vars);
	free(f);

	/* Deallocate context. */
	array_finish(&context);

	/* Deallocate lines. */
	size = (int) array_size(&lines);
	for (int i = 0; i < size; i++)
	{
		struct dw_lines *l;
		l = array_remove(&lines, 0, NULL);
		free(l);
	}
	array_finish(&lines);

	/* Deallocate breakpoints. */
	size = (int) array_size(&breakpoints);
	for (int i = 0; i < size; i++)
	{
		struct breakpoint *p;
		p = array_remove(&breakpoints, 0, NULL);
		free(p);
	}
	array_finish(&breakpoints);
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
		quit(-1, "do_analysis: error while spawning the child process!\n");

	/* Wait child and create the breakpoint list. */
	pt_waitchild();
	{
		breakpoints = bp_createlist(lines, child);
		if (bp_insertbreakpoints(breakpoints, child))
			quit(-1, "do_analysis: error while inserting breakpoints!\n");
	}

	pt_continue_single_step(child);
	init_vars = 0;
	prev_bp = NULL;

	printf("Debugging function %s:\n", function);

	/* Main loop. */
	while (pt_waitchild() != PT_CHILD_EXIT)
	{
		char *indent_buff;
		int current_depth;
		uint64_t pc;
		struct breakpoint *bp;

		f  = array_get_last(&context, NULL);
		pc = pt_readregister_pc(child) - 1;
		bp = bp_findbreakpoint(pc, breakpoints);
		current_depth = array_size(&context);
		indent_buff   = NULL;

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
			if (current_depth > 0 && depth > 0)
			{
				struct dw_variable *prev_v, *v;
				struct function *prev_f;
				int size;

				prev_f = array_get(&context, current_depth - 1, NULL);
				size = (int) array_size(&prev_f->vars);

				/* Allocates a new function context. */
				f = calloc(1, sizeof(struct function));
				array_init(&f->vars);

				/*
				 * Copies all the old variables into the new context.
				 */
				for (int i = 0; i < size; i++)
				{
					prev_v = array_get(&prev_f->vars, i, NULL);
					v = calloc(1, sizeof(struct dw_variable));

					/* Do a quick n' dirty shallow copy. */
					memcpy(v, prev_v, sizeof(struct dw_variable));

					/* Change the remaining important items:
					 * - Name
					 * - Value
					 */
					v->name = malloc(sizeof(char) * (strlen(prev_v->name) + 1));
					strcpy(v->name, prev_v->name);

					v->value.u64_value[0] = 0;
					v->value.u64_value[1] = 0;

					/* Add into array. */
					array_add(&f->vars, v);
				}

				array_add(&context, f);
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
			printf("%s[depth: %d] Returning to function...\n\n",
				(indent_buff = fn_get_indent(current_depth)), current_depth);

			fn_free_indent(indent_buff);

			/*
			 * Since we're returning from an previous call, we also
			 * need to free all (possible) arrays allocated first.
			 */
			for (int i = 0; i < (int) array_size(&f->vars); i++)
			{
				struct dw_variable *v;
				v = array_get(&f->vars, i, NULL);

				if (v->type.var_type == TARRAY)
				{
					free(v->value.p_value);
					v->value.p_value = NULL;
				}

				if (current_depth > 1)
				{
					free(v->name);
					free(v);
				}
			}

			/* Deallocates array and remove the last level. */
			if (current_depth > 1)
			{
				array_finish(&f->vars);
				free( array_remove_last(&context, NULL) );
			}

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
			printf("\n%s[depth: %d] Entering function...\n",
				(indent_buff = fn_get_indent(current_depth)), current_depth);

			fn_free_indent(indent_buff);

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
	do_analysis("tests/test", "func1");

	return (0);
}
