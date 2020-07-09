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

#define _POSIX_C_SOURCE 200809L
#include <sys/types.h>
#include <signal.h>
#include <ctype.h>
#include <inttypes.h>

#include "analysis.h"
#include "breakpoint.h"
#include "cpudisp.h"
#include "dwarf_helper.h"
#include "function.h"
#include "ptrace.h"
#include "util.h"
#include "variable.h"
#include "hashtable.h"
#include "line.h"
#include "highlight.h"

#define OPTPARSE_IMPLEMENTATION
#include "optparse.h"

#include "pbd.h"

/* PBD outputfile. */
FILE *pbd_output;

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
struct args args = {0,0,{0,0},0,0,0,0,0};

/* Forward definition. */
extern int str2int(int *out, char *s);

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
		exit(EXIT_FAILURE);
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
		if (line_read_source(filename,
			args.flags & FLG_SYNTAX_HIGHLIGHT,
			args.theme_file))
		{
			fprintf(stderr, "PBD: Source code/theme file %s not found, please\n"
				"check if the file exists in your system!\n", filename);
			finish();
			exit(EXIT_FAILURE);
		}
		line_output = line_detailed_printer;
	}

	/* Check if static analysis enabled. */
	if (args.flags & FLG_STATIC_ANALYSIS &&
		(!filename || access(filename, R_OK) == -1))
	{
		fprintf(stderr, "PBD: Source code (%s) not found!, static analysis (-S)"
			"\nexpects the source code is available!\n", filename);
		finish();
		exit(EXIT_FAILURE);
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
	line_free_source(args.flags & FLG_SYNTAX_HIGHLIGHT);
	free(filename);

	/* Deallocate breakpoints. */
	bp_list_free(breakpoints);

	/* Deallocate watch or ignore list, if available. */
	if (args.iw_list.ht_list != NULL)
		hashtable_finish(&args.iw_list.ht_list, 1);

	/* Deallocate theme file, if any. */
	if (args.theme_file != NULL)
		free(args.theme_file);

	/* Deallocate static analysis data structures. */
	static_analysis_finish();

	/* Deallocate and close output, if any. */
	if (args.output_file)
	{
		free(args.output_file);
		fclose(pbd_output);
	}
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
		QUIT(EXIT_FAILURE, "error while spawning the child process!\n");

	/* Wait child, create the breakpoint list and insert them. */
	if (pt_waitchild() != 0)
	{
		finish();
		exit(EXIT_FAILURE);
	}

	/*
	 * Create the breakpoint list accordingly with the analysis type:
	 * - Normal
	 * - Static analysis
	 */
	breakpoints = (args.flags & FLG_STATIC_ANALYSIS) ?
		static_analysis(filename, function, lines, dw.dw_func.low_pc) :
		bp_createlist(lines);

	/* Insert them. */
	bp_insertbreakpoints(breakpoints, child);

	/* Proceed execution. */
	pt_continue_single_step(child);
	init_vars = 0;
	prev_bp = NULL;

	fprintf(pbd_output, "PBD (Printf Based Debugger) v%d.%d%s\n", MAJOR_VERSION, MINOR_VERSION,
		RLSE_VERSION);
	fprintf(pbd_output, "---------------------------------------\n");

	fprintf(pbd_output, "Debugging function %s:\n", function);

	/* Main loop. */
	while (pt_waitchild() != PT_CHILD_EXIT)
	{
		int current_depth;
		uintptr_t pc;
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
			fputc('\n', pbd_output);
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
	struct hashtable *breakpoints;  /* Breakpoints list.     */
	struct breakpoint *b_k, *b_v;   /* Breakpoint key/value. */
	pid_t child;                    /* Child process pid.    */
	int i;                          /* Loop index.           */
	((void)b_k);

	/* Executable and function name should always be passed as parameters! */
	if (args.executable == NULL || args.function == NULL)
	{
		fprintf(stderr, "%s: executable and/or function name not found!\n\n", prg_name);
		usage(EXIT_FAILURE, prg_name);
	}

	/* Setup and spawns cihld. */
	setup(args.executable, args.function);
	if ((child = pt_spawnprocess(args.executable, NULL)) < 0)
		QUIT(EXIT_FAILURE, "error while spawning the child process!\n");

	/* Wait for child process. */
	pt_waitchild();

	fprintf(pbd_output, "PBD (Printf Based Debugger) v%d.%d%s\n", MAJOR_VERSION,
		MINOR_VERSION, RLSE_VERSION);
	fprintf(pbd_output, "---------------------------------------\n");

	/* File name. */
	fprintf(pbd_output, "Filename: %s\n", filename);

	/* Dump vars. */
	fprintf(pbd_output, "\nVariables:\n");
	var_dump( ((struct function *)array_get(&context, 0, NULL))->vars );

	/* Dump lines. */
	fprintf(pbd_output, "Lines:\n");
	dw_lines_dump(lines);

	/* Break point list. */
	fprintf(pbd_output, "\nBreakpoint list:\n");
	breakpoints = (args.flags & FLG_STATIC_ANALYSIS) ?
		static_analysis(filename, args.function, lines, dw.dw_func.low_pc) :
		bp_createlist(lines);

	i = 0;
	HASHTABLE_FOREACH(breakpoints, b_k, b_v,
	{
		fprintf(pbd_output,
			"    Breakpoint #%03d, line: %03d / addr: %" PRIxPTR
			" / orig_byte: %" PRIx64"\n",
			i++,
			b_v->line_no,
			b_v->addr,
			(pt_readmemory64(child, b_v->addr) & 0xFF)
		);
	});

	kill(child, SIGKILL);
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
	/* Deallocate maybe allocated resources. */
	static_analysis_finish();
	if (args.iw_list.list != NULL)
		free(args.iw_list.list);
	if (args.theme_file != NULL)
		free(args.theme_file);

	/* Show options. */
	printf("Usage: %s [options] executable function_name [executable_options]\n",
		prg_name);
	printf("Options:\n");
	printf("--------\n");
	printf("  -h --help           Display this information\n");
	printf("  -v --version        Display the PBD version\n");
	printf("  -s --show-lines     Shows the debugged source code portion in the output\n");
	printf("  -x --context <num>  Shows num lines before and after the code portion.\n");
	printf("                      This option is meant to be used in conjunction with\n");
	printf("                      -s option\n");
	printf("\n");
	printf("  -l --only-locals   Monitors only local variables (default: global + local)\n");
	printf("  -g --only-globals  Monitors only global variables (default: global + local)\n");
	printf("  -i --ignore-list <var1, ...> Ignores a specified list of variables names\n");
	printf("  -w --watch-list  <var1, ...> Monitors a specified list of variables names\n");
	printf("  -o --output <output-file>    Sets an output file for PBD output. Useful to\n");
	printf("                               not mix PBD and executable outputs\n");
	printf("     --args          Delimits executable arguments from this point. All\n");
	printf("                     arguments onwards will be treated as executable\n");
	printf("                     program arguments.");

	printf("\nStatic Analysis options:\n");
	printf("------------------------\n");
	printf("PBD is able to do a previous static analysis in the C source code that\n");
	printf("belongs to the monitored function, and thus, greatly improving the\n");
	printf("debugging time. Note however, that this is an experimental feature.\n");
	printf("\n");
	printf("  -S --static                Enables static analysis\n");
	printf("\nOptional flags:\n");
	printf("  -D sym[=val]               Defines 'sym' with value 'val'\n");
	printf("  -U sym                     Undefines 'sym'\n");
	printf("  -I dir                     Add 'dir' to the include path\n");
	printf("  --std=<std>                Defines the language standard, supported values\n");
	printf("                             are: c89, gnu89, c99, gnu99, c11 and gnu11.\n");
	printf("                             (Default: gnu11)\n");

	printf("\nSyntax highlighting options:\n");
	printf("----------------------------\n");
	printf("  -c --color                 Enables syntax highlight, this option only takes\n");
	printf("                             effect while used together with --show-lines, Also\n");
	printf("                             note that this option requires a 256-color\n");
	printf("                             compatible terminal\n");
	printf("\n");
	printf("  -t  --theme <theme-file>   Select a theme file for the highlighting\n");

	printf("\nNotes:\n");
	printf("------\n");
	printf("  - Options -i and -w are mutually exclusive!\n\n");
	printf("  - The executable *must* be built without any optimization and with at\n"
		   "    least -gdwarf-2 and no PIE! (if PIE enabled by default)");

	printf("\n\nThe following options are for PBD internals:\n");
	printf("  -d --dump-all    Dump all information gathered by the executable\n");

	printf("\n\n'Unsafe' options:\n");
	printf("-----------------\n");
	printf("  The options below are meant to be used with caution, since they could lead\n"
		   "  to wrong output.\n\n");

	printf("  --avoid-equal-statements  If enabled, PBD will ignore all line statements\n"
		   "                            that are 'duplicated', i.e: belongs to the same\n"
		   "                            liner number, regardless its address.\n\n");
	exit(retcode);
}

/**
 * Program version.
 */
static void version(void)
{
	printf("PBD (Printf Based Debugger) v%d.%d%s\n", MAJOR_VERSION, MINOR_VERSION,
		RLSE_VERSION);
	printf("MIT License - Copyright (C) 2019-2020 Davidson Francis\n");
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
		{"version",                'v',     OPTPARSE_NONE},
		{"help",                   'h',     OPTPARSE_NONE},
		{"show-lines",             's',     OPTPARSE_NONE},
		{"context",                'x', OPTPARSE_REQUIRED},
		{"only-locals",            'l',     OPTPARSE_NONE},
		{"only-globals",           'g',     OPTPARSE_NONE},
		{"ignore-list",            'i', OPTPARSE_REQUIRED},
		{"watch-list",             'w', OPTPARSE_REQUIRED},
		{"output",                 'o', OPTPARSE_REQUIRED},
		{"args",                   253,     OPTPARSE_NONE},
		{"static",                 'S',     OPTPARSE_NONE},
		{0,                        'D', OPTPARSE_REQUIRED},
		{0,                        'U', OPTPARSE_REQUIRED},
		{0,                        'I', OPTPARSE_REQUIRED},
		{"std",                    254, OPTPARSE_REQUIRED},
		{"color",                  'c',     OPTPARSE_NONE},
		{"theme",                  't', OPTPARSE_REQUIRED},
		{"dump-all",               'd',     OPTPARSE_NONE},
		{"avoid-equal-statements", 255,     OPTPARSE_NONE},
		{0,0,0}
	};

	/* Parse options. */
	optparse_init(&options, argv);
	while ((option = optparse_long(&options, longopts, NULL)) != -1)
	{
		switch (option)
		{
			/* Version. */
			case 'v':
				version();
				break;

			/* Help/Usage. */
			case 'h':
				usage(EXIT_SUCCESS, argv[0]);
				break;

			/* Show lines. */
			case 's':
				args.flags |= FLG_SHOW_LINES;
				break;

			/* Context number, used together with --show-lines. */
			case 'x':
				if (str2int(&args.context, options.optarg) < 0 || args.context < 0)
				{
					fprintf(stderr, "%s: --context: number (%s) cannot be "
						"parsed!\n", argv[0], options.optarg);
					usage(EXIT_FAILURE, argv[0]);
				}
				break;

			/* Show only locals variables. */
			case 'l':
				args.flags |= FLG_ONLY_LOCALS;
				break;

			/* Show only globals variables. */
			case 'g':
				args.flags |= FLG_ONLY_GLOBALS;
				break;

			/* Ignore list: variables list to ignore. */
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

			/* Watch list: variables list to watch. */
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

			/* Set output file. */
			case 'o':
				if (args.output_file != NULL)
					free(args.output_file);

				args.output_file = malloc(sizeof(char) *
					(strlen(options.optarg) + 1));

				strcpy(args.output_file, options.optarg);
				break;

			/* Delimit program arguments: --args. */
			case 253:
				goto out;

			/* Enable static analysis. */
			case 'S':
				args.flags |= FLG_STATIC_ANALYSIS;
				break;

			/* Define a new symbol. (used together with -S). */
			case 'D':
				if (!(args.flags & FLG_STATIC_ANALYSIS))
				{
					fprintf(stderr, "%s: static analysis (-S) "
						"should be enabled first, before using -D\n\n", argv[0]);
					usage(EXIT_FAILURE, argv[0]);
				}
				static_analysis_add_arg("-D ", options.optarg);
				break;

			/* Undefines a symbol. (used together with -S). */
			case 'U':
				if (!(args.flags & FLG_STATIC_ANALYSIS))
				{
					fprintf(stderr, "%s: static analysis (-S) "
						"should be enabled first, before using -U\n\n", argv[0]);
					usage(EXIT_FAILURE, argv[0]);
				}
				static_analysis_add_arg("-U ", options.optarg);
				break;

			/* Include path. (used together with -S). */
			case 'I':
				if (!(args.flags & FLG_STATIC_ANALYSIS))
				{
					fprintf(stderr, "%s: static analysis (-S) "
						"should be enabled first, before using -I\n\n", argv[0]);
					usage(EXIT_FAILURE, argv[0]);
				}
				static_analysis_add_arg("-I", options.optarg);
				break;

			/* Set C standard (--std). (used together with -S). */
			case 254:
				if (!(args.flags & FLG_STATIC_ANALYSIS))
				{
					fprintf(stderr, "%s: static analysis (-S) "
						"should be enabled first, before using --std\n\n", argv[0]);
					usage(EXIT_FAILURE, argv[0]);
				}
				args.flags |= FLG_SANALYSIS_SETSTD;
				static_analysis_add_arg("-std=", options.optarg);
				break;

			/* Enables syntax highlighting. */
			case 'c':
				args.flags |= FLG_SYNTAX_HIGHLIGHT;
				break;

			/* Set theme file. (used together with -c). */
			case 't':
				if (args.theme_file != NULL)
					free(args.theme_file);

				args.theme_file = malloc(sizeof(char) *
					(strlen(options.optarg) + 1));

				strcpy(args.theme_file, options.optarg);
				break;

			/* Dumps all information gathered by the executable. */
			case 'd':
				args.flags |= FLG_DUMP_ALL;
				break;

			/*
			 * Ignore equal statements, i.e: statements that are in the same
			 * line  number.
			 */
			case 255:
				args.flags |= FLG_IGNR_EQSTAT;
				break;

			/* Unknown command. */
			case '?':
				fprintf(stderr, "%s: %s\n\n", argv[0], options.errmsg);
				usage(EXIT_FAILURE, argv[0]);
		}
	}

out:
	/* Enable syntax highlight?. */
	if ((args.flags & FLG_SYNTAX_HIGHLIGHT) && !(args.flags & FLG_SHOW_LINES))
	{
		fprintf(stderr, "%s: option -c only work if used"
			" together with -s!\n\n", argv[0]);
		usage(EXIT_FAILURE, argv[0]);
	}

	/* Custom theme?. */
	if ((args.theme_file != NULL) && !(args.flags & FLG_SYNTAX_HIGHLIGHT))
	{
		fprintf(stderr, "%s: option -t only works if used"
			" together with -s _and_ -c!\n\n", argv[0]);
		usage(EXIT_FAILURE, argv[0]);
	}

	/* Check if context enabled. */
	if (args.context != 0 && !(args.flags & FLG_SHOW_LINES))
	{
		fprintf(stderr, "%s: option -x only work if used"
			" together with -s!\n\n", argv[0]);
		usage(EXIT_FAILURE, argv[0]);
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

	/* PBD output. */
	if (args.output_file != NULL)
	{
		pbd_output = fopen(args.output_file, "w");
		if (!pbd_output)
		{
			fprintf(stderr, "%s: cannot open %s to write!\n", argv[0],
				args.output_file);
			usage(EXIT_FAILURE, argv[0]);
		}
	}
}

/**
 * Entry point.
 */
int main(int argc, char **argv)
{
	pbd_output = stdout;

	/*
	 * Argument list of static analysis.
	 * This argument list should be built _before_ the arguments
	 * parsing.
	 */
	static_analysis_init();

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

	/* CPU dispatcher. */
	select_cpu();

	/* Analyze. */
	do_analysis(args.executable, args.function, args.argv);
	return (EXIT_SUCCESS);
}
