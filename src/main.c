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

#define OPTPARSE_IMPLEMENTATION
#include "optparse.h"

#include "pbd.h"
#include <ctype.h>
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

/* Arguments list. */
struct args args = {0};

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

	/* Ensure we're debugging a C program. */
	if (!dw_is_c_language(&dw))
	{
		fprintf(stderr, "PBD: Unsupported language, languages supported: \n"
			"  -> C, standards: C89, C99 and C11\n");
		dw_finish(&dw);
		exit(-1);
	}

	/* Initialize first function context. */
	array_init(&context);
		f = calloc(1, sizeof(struct function));
	array_add(&context, f);

	/* Parses all variables, lines and filename. */
	f->vars  = dw_get_all_variables(&dw);
	lines    = dw_get_all_lines(&dw);
	filename = dw_get_source_file(&dw);

	/* Should we read the source?. */
	if (args.flags & FLG_SHOW_LINES)
	{
		if (line_read_source(filename))
		{
			fprintf(stderr, "PBD: Source code %s not found, please check if the file "
				"exists in your system!\n", filename);
			finish();
			exit(EXIT_FAILURE);
		}
		line_output = line_detailed_printer;
	}

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
	/* Free dwarf structures. */
	dw_finish(&dw);

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

	/* Deallocate watch or ignore list, if available. */
	if (args.iw_list.ht_list != NULL)
		hashtable_finish(&args.iw_list.ht_list, 1);
}

/**
 * Main routine
 *
 * @param file File to be analyzed.
 * @param function Function to be analyzed.
 */
void do_analysis(const char *file, const char *function, char **argv)
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
	if ((child = pt_spawnprocess(file, argv)) < 0)
		QUIT(-1, "error while spawning the child process!\n");

	/* Wait child, create the breakpoint list and insert them. */
	pt_waitchild();
	{
		breakpoints = bp_createlist(lines, child);
	}

	pt_continue_single_step(child);
	init_vars = 0;
	prev_bp = NULL;

	printf("PBD (Printf Based Debugger) v%d.%d%s\n", MAJOR_VERSION, MINOR_VERSION,
		RLSE_VERSION);
	printf("---------------------------------------\n");

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
 * @brief Dumps all information gathered by the executable.
 *
 * @param prg_name PBD argument name.
 */
static void dump_all(const char *prg_name)
{
	pid_t child; /* Child process pid. */

	/* Executable and function name should always be passed as parameters! */
	if (args.executable == NULL || args.function == NULL)
	{
		printf("%s: executable and/or function name not found!\n\n", prg_name);
		usage(EXIT_FAILURE, prg_name);
	}

	/* Setup and spawns cihld. */
	setup(args.executable, args.function);
	if ((child = pt_spawnprocess(args.executable, NULL)) < 0)
		QUIT(-1, "error while spawning the child process!\n");

	printf("PBD (Printf Based Debugger) v%d.%d%s\n", MAJOR_VERSION, MINOR_VERSION,
		RLSE_VERSION);
	printf("---------------------------------------\n");

	/* File name. */
	printf("Filename: %s\n", filename);

	/* Dump vars. */
	printf("\nVariables:\n");
	var_dump( ((struct function *)array_get(&context, 0, NULL))->vars );

	/* Dump lines. */
	printf("Lines:\n");
	dw_lines_dump(lines);

	/* Break point list. */
	printf("\nBreakpoint list:\n");
	for (int i = 0; i < (int) array_size(&lines); i++)
	{
		struct dw_line *l;
		l = array_get(&lines, i, NULL);

		if (l->line_type != LBEGIN_STMT)
			continue;

		printf("    Breakpoint #%03d, line: %03d / addr: %" PRIx64" / orig_byte: %" PRIx64"\n",
			i, l->line_no, l->addr, (pt_readmemory64(child, l->addr) & 0xFF));
	}

	finish();
	exit(EXIT_SUCCESS);
}

/**
 * @brief Program usage.
 *
 * @param retcode Return code.
 * @param prg_name Program name.
 */
void usage(int retcode, const char *prg_name)
{
	printf("Usage: %s [options] executable function_name [executable_options]\n",
		prg_name);
	printf("Options:\n");
	printf("  -h --help          Display this information\n");
	printf("  -v --version       Display the PBD version\n");
	printf("  -s --show-lines    Shows the debugged source code portion in the output\n");
	printf("  -l --only-locals   Monitors only local variables (default: global + local)\n");
	printf("  -g --only-globals  Monitors only global variables (default: global + local)\n");
	printf("  -i --ignore-list <var1, ...> Ignores a specified list of variables names\n");
	printf("  -w --watch-list  <var1, ...> Monitors a specified list of variables names\n");

	printf("\nNotes:\n");
	printf("  - Options -i and -w are mutually exclusive!\n\n");
	printf("  - The executable *must* be built without any optimization and with at\n"
		   "    least -gdwarf-2 and no PIE! (if PIE enabled by default)");

	printf("\n\nThe following options are for PBD internals:\n");
	printf("  -d --dump-all    Dump all information gathered by the executable\n");
	exit(retcode);
}

/**
 * Program version.
 */
static void version(void)
{
	printf("PBD (Printf Based Debugger) v%d.%d%s\n", MAJOR_VERSION, MINOR_VERSION,
		RLSE_VERSION);
	printf("MIT License - Copyright (C) 2019 Davidson Francis\n");
	exit(EXIT_SUCCESS);
}

/**
 * @brief Parses the watch- and ignore-list and creates
 * a hashtable with each variable name inside.
 *
 * @param list watch- or ignore-list to be parsed.
 *
 * @return Returns a hashtable with each variable name
 * parsed.
 */
static struct hashtable *parse_list(char *list)
{
	struct hashtable *ht; /* Hashtable.        */
	char *s;              /* Temporary string. */
	char *var;            /* Variable name.    */
	char *trimmed;        /* Trimmed list.     */
	int idx;              /* Loop index.       */

	/*
	 * Since the user could add extra-spaces inside the list
	 * lets remove them.
	 */
	idx = 0;
	trimmed = malloc(sizeof(char) * (strlen(list) + 1));

	for (int i = 0; list[i] != '\0'; i++)
		if (!isblank(list[i]))
			trimmed[idx++] = list[i];

	trimmed[idx] = '\0';

	/* Initialize hash table. */
	hashtable_init(&ht, hashtable_sdbm_setup);
	s = trimmed;

	/* For each var, adds into a hashtable. */
	for (s = strtok(s, ","); s != NULL; s = strtok(NULL, ","))
	{
		var = malloc(sizeof(char) * (strlen(s) + 1));
		strcpy(var, s);
		hashtable_add(&ht, var, var);
	}

	free(trimmed);
	free(list);
	return (ht);
}

/**
 * Parses the command-line arguments.
 *
 * @param argc Argument counter.
 * @param argv Argument list.
 */
static void readargs(int argc, char **argv)
{
	int option;              /* Current option.   */
	struct optparse options; /* Optparse options. */

	((void)argc);

	/* Current arguments list. */
	struct optparse_long longopts[] = {
		{"version",      'v',     OPTPARSE_NONE},
		{"help",         'h',     OPTPARSE_NONE},
		{"show-lines",   's',     OPTPARSE_NONE},
		{"only-locals",  'l',     OPTPARSE_NONE},
		{"only-globals", 'g',     OPTPARSE_NONE},
		{"ignore-list",  'i', OPTPARSE_REQUIRED},
		{"watch-list",   'w', OPTPARSE_REQUIRED},
		{"dump-all",     'd',     OPTPARSE_NONE},
		{0}
	};

	optparse_init(&options, argv);
	while ((option = optparse_long(&options, longopts, NULL)) != -1)
	{
		switch (option)
		{
			case 'v':
				version();
				break;
			case 'h':
				usage(EXIT_SUCCESS, argv[0]);
				break;
			case 's':
				args.flags |= FLG_SHOW_LINES;
				break;
			case 'l':
				args.flags |= FLG_ONLY_LOCALS;
				break;
			case 'g':
				args.flags |= FLG_ONLY_GLOBALS;
				break;
			case 'i':
				if (args.flags & FLG_WATCH_LIST)
				{
					fprintf(stderr, "%s: options -i and -w"
						" are mutually exclusive!\n\n", argv[0]);
					usage(EXIT_FAILURE, argv[0]);
				}

				args.flags |= FLG_IGNR_LIST;
				if (args.iw_list.list != NULL)
					free(args.iw_list.list);

				args.iw_list.list = malloc(sizeof(char) *
					(strlen(options.optarg) + 1));

				strcpy(args.iw_list.list, options.optarg);
				break;
			case 'w':
				if (args.flags & FLG_IGNR_LIST)
				{
					fprintf(stderr, "%s: options -i and -w"
						" are mutually exclusive!\n\n", argv[0]);
					usage(EXIT_FAILURE, argv[0]);
				}

				args.flags |= FLG_WATCH_LIST;
				if (args.iw_list.list != NULL)
					free(args.iw_list.list);

				args.iw_list.list = malloc(sizeof(char) *
					(strlen(options.optarg) + 1));

				strcpy(args.iw_list.list, options.optarg);
				break;
			case 'd':
				args.flags |= FLG_DUMP_ALL;
				break;
			case '?':
				fprintf(stderr, "%s: %s\n\n", argv[0], options.errmsg);
				usage(EXIT_FAILURE, argv[0]);
		}
	}

	/* Print remaining arguments. */
	args.executable = optparse_arg(&options);
	args.function = optparse_arg(&options);
	args.argv = options.argv + options.optind - 1;

	/*
	 * Reverse arguments order.
	 * Instead of creating another list, it is easier to
	 * invert the function name with the executable name
	 * and pass the same argv that the PBD receives, ;-).
	 */
	if (args.executable != NULL)
		options.argv[options.optind - 1] = args.executable;

	/* If no one variable options set, set all. */
	if ( !(args.flags & (FLG_ONLY_GLOBALS|FLG_ONLY_LOCALS)) )
		args.flags |= FLG_ONLY_GLOBALS|FLG_ONLY_LOCALS;

	/* If ignore list, lets parse each variable. */
	if (args.flags & (FLG_IGNR_LIST|FLG_WATCH_LIST))
		args.iw_list.ht_list = parse_list(args.iw_list.list);
}

/**
 * Entry point.
 */
int main(int argc, char **argv)
{
	/* Read arguments. */
	readargs(argc, argv);

	if (args.flags & FLG_DUMP_ALL)
		dump_all(argv[0]);

	/* Ensure we have the minimal necessary. */
	if (args.executable == NULL || args.function == NULL)
	{
		printf("%s: executable and/or function name not found!\n\n", argv[0]);
		usage(EXIT_FAILURE, argv[0]);
	}

	/* Analyze. */
	do_analysis(args.executable, args.function, args.argv);
	return (0);
}
