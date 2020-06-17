/*
 * MIT License
 *
 * Copyright (c) 2020 Davidson Francis <davidsondfgl@gmail.com>
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

#include "analysis.h"
#include "pbd.h"

/* Arguments. */
struct analysis_struct analysis_arguments;

/* Compound statement function scope. */
static struct scope *f_scope;

/* Target function. */
static const char *function;
static const char *filename;

/**
 * Hash table breakpoints list.
 */
static struct hashtable *breakpoints;

/**
 * Lines list.
 */
static struct array *lines;

/**
 * If dump_all flag is enabled, verbose_assign()
 * will output to stdout all the assignment/declaration
 * statements that is found during the file parsing.
 *
 * @param line_no stmt/expr line number.
 * @param name variable name.
 * @param is_assign 1 if assignment statement, 0 otherwise.
 * @param is_decl 1 if declaration statement, 0 otherwise.
 * @param is_ignored 1 if the statement/expr is ignored*,
 *        0 otherwise.
 *
 * @note *An statement/expression will be ignored if the
 * variable is not one of the types tracked (currently:
 * basetype, arrays and pointers) or if outside the valid
 * scopes, i.e: not global and/or not in the function
 * scope.
 */
static inline void verbose_assign(
	int line_no,
	const char *name,
	int is_assign,
	int is_decl,
	int is_ignored)
{
	if (args.flags & FLG_DUMP_ALL)
	{
		printf("===static=analysis=== [%03d] %15s (is_assign: %d) %s%s\n",
			line_no,
			name,
			is_assign,
			(is_decl ? "(decl) " : ""),
			(is_ignored ? "(ignored) " : "")
		);
	}
}

/**
 * If dump_all flag is enabled, verbose_function_call()
 * will output to stdout all the function call statements
 * that is found during the file parsing.
 *
 * @param line_no Function call line number.
 */
static inline void verbose_function_call(int line_no)
{
	if (args.flags & FLG_DUMP_ALL)
	{
		printf("===static=analysis=== [%03d] %15s (func call)\n",
			line_no, "");
	}
}

/**
 * Do a binary search on the lines list looking for a given @p
 * line_no and returns the leftmost ocurrence, if there is more
 * than one.
 *
 * @param lines Lines list.
 * @param line_no Line to be searched.
 *
 * @return Returns the line index, if one.
 */
static int binsearch_lines(struct array *lines, unsigned line_no)
{
	struct dw_line *line;
	size_t size;
	int l;
	int r;
	int m;

	size = array_size(&lines);
	l = 0;
	r = size - 1;

	/* Returns the leftmost element if duplicate. */
	while (l < r)
	{
		m = (r + l) / 2;
		line = array_get(&lines, m, NULL);

		if (line->line_no < line_no)
			l = m + 1;
		else
			r = m;
	}

	line = array_get(&lines, l, NULL);
	if (line && line->line_no == line_no)
		return (l);
	else
		return (-1);
}

/**
 * For a given symbol @p sym, checks if the symbol is global
 * or local.
 *
 * @param sym Symbol to be checked.
 *
 * @return Returns 1 if global and 0 otherwise.
 *
 * @note Adapted from storage() in test-dissect.c,
 * lib sparse source code.
 */
static inline int is_symbol_global(struct symbol *sym)
{
	unsigned m;
	m = sym->ctype.modifiers;

	/*
	 * Since static variables do not resides in the stack,
	 * they're equals to global variables, from the PBD
	 * point-of-view.
	 */
	if (m & MOD_STATIC || m & MOD_NONLOCAL)
		return (1);
	else
		return (0);
}

/**
 * Checks if the symbol @p sym is one of the types supported
 * to be monitored.
 *
 * @param sym Symbol to be checked.
 *
 * @return Returns 1 if supported and 0 otherwise.
 */
static inline int is_var_symbol(struct symbol *sym)
{
	int type;
	type = get_sym_type(sym);
	return (type == SYM_BASETYPE || type == SYM_ARRAY
		|| type == SYM_PTR);
}

/**
 * For a given symbol @p sym, checks if the symbol elegible
 * to be monitored or not.
 *
 * @param sym Symbol to be checked.
 *
 * @return Returns 1 if elegible and 0 otherwise.
 */
static inline int should_watch_sym(struct symbol *sym)
{
	return
	(
		is_var_symbol(sym) &&
		(is_symbol_global(sym) || sym->scope == f_scope)
	);
}

/**
 * Given a line number @p line_no, tries to add the all the
 * matching lines in the line list in to the breakpoint
 * list.
 *
 * @param line_no Line to be added.
 */
static void try_add_symbol(unsigned line_no)
{
	int idx;
	int size;
	struct dw_line *line;
	struct breakpoint *bp;

	idx = binsearch_lines(lines, line_no);
	size = (int) array_size(&lines);

	/* Skip lines that do not exist in dwarf structures. */
	if (idx == -1)
		return;

	/* Loop through all (possible) repeated lines. */
	while (idx < size)
	{
		line = array_get(&lines, idx, NULL);
		idx++;

		/* If different, we should break, since the list is ordered. */
		if (line->line_no != line_no)
			break;

		/* Skip lines that are not begin of statement. */
		if (line->line_type != LBEGIN_STMT)
			continue;

		/*
		 * Check if the value already exists, break* if so.
		 *
		 * *We can safely break the loop here because the addresses
		 * is (I really hope) guaranteed to not repeat, so, if the
		 * first line ocurrence already exists for the same address
		 * we can break the entire loop, instead of just skip.
		 */
		if (hashtable_get(&breakpoints, (void*)line->addr) != NULL)
			break;

		/* Allocates a new breakpoint. */
		bp = malloc(sizeof(struct breakpoint));
		bp->addr = line->addr;
		bp->original_byte = 0;
		bp->line_no = line->line_no;

		/* Add to our hashtable. */
		hashtable_add(&breakpoints, (void*)bp->addr, bp);
	}
}

/**
 * Recursively expands a given expression until find a EXPR_SYMBOL,
 * EXPR_CALL or a non-supported expression type.
 *
 * @param expr Expression to be expanded.
 *
 * @param is_assignment Signals if a previous expression of type
 * EXPR_ASSIGNMENT was found. The first call to handle_expr()
 * is_assignment should be '0'.
 */
void handle_expr(struct expression *expr, int is_assignment)
{
	/* Skip NULL expressions. */
	if (!expr)
		return;

	switch (expr->type)
	{
		default:
			break;

		/* Pre-operator. */
		case EXPR_PREOP:
			handle_expr(expr->unop, is_assignment);
			break;

		/*
		 * In my tests, postop seems to be used to handle things
		 * like: foo++, foo--. So, lets consider this as an assignment
		 * expression.
		 */
		case EXPR_POSTOP:
			handle_expr(expr->unop, 1);
			break;

		/* Left/Right expressions. */
		case EXPR_COMPARE:
		case EXPR_LOGICAL:
		case EXPR_BINOP:
		case EXPR_COMMA:
			handle_expr(expr->left, is_assignment);

			/*
			 * Since we're looking for the left-most symbol that
			 * belongs to an assignment expression, if the next
			 * left expression is of type EXPR_SYMBOL _and_ we're
			 * inside an assignment, we walk through it as usual
			 * and reset the assignment state before proceed to
			 * the right side.
			 */
			if (expr->left->type == EXPR_SYMBOL && is_assignment)
				handle_expr(expr->right, 0);
			else
				handle_expr(expr->right, is_assignment);
			break;

		/* We can have assignments inside cast expressions. */
		case EXPR_CAST:
		case EXPR_IMPLIED_CAST:
		case EXPR_FORCE_CAST:
			handle_expr(expr->cast_expression, is_assignment);
			break;

		/* Almost there. */
		case EXPR_ASSIGNMENT:
			handle_expr(expr->left, 1);
			handle_expr(expr->right, 0);
			break;

		/* Ternary operator, I think... */
		case EXPR_CONDITIONAL:
		case EXPR_SELECT:
			handle_expr(expr->conditional, 0);
			handle_expr(expr->cond_true, 0);
			handle_expr(expr->cond_false, 0);
			break;

		/* Function call, no need to proceed further. */
		case EXPR_CALL:
			verbose_function_call(expr->pos.line);
			try_add_symbol(expr->pos.line);
			break;

		/* Symbol found, yay. */
		case EXPR_SYMBOL:
			if (is_assignment && should_watch_sym(expr->symbol))
			{
				verbose_assign(expr->pos.line, expr->symbol_name->name,
					is_assignment, 0, 0);

				try_add_symbol(expr->pos.line);
			}
			else
			{
				verbose_assign(expr->pos.line, expr->symbol_name->name,
					is_assignment, 0, 1);
			}
			break;
	}
}

/**
 * Recursively expands a given statement until all statements are
 * expanded or a non-supported statement is found.
 *
 * @param stmt Statement to be expanded.
 */
void handle_stmt(struct statement *stmt)
{
	struct symbol *sym;
	struct statement *s;

	/* Skip NULL statements. */
	if (!stmt)
		return;

	switch (stmt->type)
	{
		default:
			break;

		/* Expressions. */
		case STMT_EXPRESSION:
			handle_expr(stmt->expression, 0);
			break;

		case STMT_IF:
			handle_expr(stmt->if_conditional, 0);
			handle_stmt(stmt->if_true);
			handle_stmt(stmt->if_false);
			break;

		/*
		 * Loops.
		 *
		 * I'm not worried here of which kind of loop is being
		 * analyzed, so I'm just handling all statements and
		 * expressions inside.
		 *
		 * Nulled pointers are handled nicely at the top of
		 * each handle, so I'm also not worried with it.
		 */
		case STMT_ITERATOR:
			/* Expressions. */
			handle_expr(stmt->iterator_pre_condition, 0);
			handle_expr(stmt->iterator_post_condition, 0);
			/* Statements. */
			handle_stmt(stmt->iterator_pre_statement);
			handle_stmt(stmt->iterator_statement);
			handle_stmt(stmt->iterator_post_statement);
			break;

		/* Switch statements are composed of switch, label,
		 * and goto statements. */
		case STMT_SWITCH:
			handle_expr(stmt->switch_expression, 0);
			handle_stmt(stmt->switch_statement);
			break;
		case STMT_CASE:
			handle_expr(stmt->case_expression, 0);
			handle_expr(stmt->case_to, 0);
			handle_stmt(stmt->case_statement);
			break;
		case STMT_LABEL:
			handle_stmt(stmt->label_statement);
			break;
		case STMT_GOTO:
			handle_expr(stmt->goto_expression, 0);
			break;

		/* Declarations. */
		case STMT_DECLARATION:
			/*
			 * If a symbol has an initializer _and_ is one of the
			 * should be watched varibles _or_ has an assignment
			 * expression inside initializer.
			 */
			FOR_EACH_PTR(stmt->declaration, sym) {
				if (sym->initializer)
				{
					/*
					 * If declared symbol is:
					 * global _or_ function scope variable
					 * of type: basetype, arrays or pointers.
					 */
					if (should_watch_sym(sym))
					{
						verbose_assign(sym->pos.line, sym->ident->name, 1, 1, 0);
						try_add_symbol(sym->pos.line);
					}

					/*
					 * Cases like:
					 * int foo = 0xbeef;
					 * {
					 *    int bar = foo++;
					 * }
					 * since foo is at function scope and being changed.
					 */
					else
					{
						verbose_assign(sym->pos.line, sym->ident->name, 1, 1, 1);
						handle_expr(sym->initializer, 0);
					}
				}
				else
					verbose_assign(sym->pos.line, sym->ident->name, 0, 1, 1);

			} END_FOR_EACH_PTR(sym);
			break;

		/* Compound statements. */
		case STMT_COMPOUND:
			FOR_EACH_PTR(stmt->stmts, s) {
				handle_stmt(s);
			} END_FOR_EACH_PTR(s);
			break;
	}
}

/**
 * For a symbol @p sym belonging to our target function,
 * save it's scope and handles all it's statements.
 *
 * @param sym Function symbol.
 */
static void process_function(struct symbol *sym)
{
	struct symbol *type;
	struct statement *stmt;

	type = sym->ctype.base_type;
	stmt = sym->ctype.modifiers & MOD_INLINE
			? type->inline_stmt : type->stmt;

	/* Check if we have a compound statement and a return. */
	if (stmt->type == STMT_COMPOUND && type->stmt->ret)
	{
		f_scope = type->stmt->ret->scope;
		struct statement *s;
		FOR_EACH_PTR(stmt->stmts, s) {
			handle_stmt(s);
		} END_FOR_EACH_PTR(s);
	}
}

/**
 * Given a symmbol list @p sym_list, iterates through
 * all the symbols and searches for the target
 * function.
 *
 * @param sym_list Symbol list.
 */
static void process(struct symbol_list *sym_list)
{
	struct symbol *sym;

	FOR_EACH_PTR(sym_list, sym) {
		if (sym->type == SYM_NODE && get_base_type(sym)->type == SYM_FN)
		{
			if (strcmp(sym->ident->name, function) == 0)
			{
				process_function(sym);
				break;
			}
		}
	} END_FOR_EACH_PTR(sym);
}

/**
 * Init the static analysis internal structure argument.
 */
int static_analysis_init(void)
{
	/* Initialize argument list. */
	if (array_init(&analysis_arguments.args) < 0)
		return (-1);

	/* Initial arguments. */
	array_add(&analysis_arguments.args, "pbd");
	array_add(&analysis_arguments.args, "-Wno-strict-prototypes");
	array_add(&analysis_arguments.args, "-Wno-decl");
	return (0);
}

/**
 * Adds the argument @p arg and its value @p value into the
 * argument list for the static analysis.
 *
 * @param arg Argument.
 * @param value Argument value.
 *
 * @return Returns 0 if success and a negative value
 * otherwise.
 */
int static_analysis_add_arg(const char *arg, const char *value)
{
	char *str;

	/* If not valid. */
	if (!arg || !value)
		return (-1);

	/* Make room and copy. */
	str = malloc(sizeof(char) * (strlen(arg) + strlen(value) + 1));
	if (!str)
		return (-1);

	strcpy(str, arg);
	strcat(str, value);
	return ( array_add(&analysis_arguments.args, str) );
}

/**
 * Finishes all allocated resources.
 *
 * @note There is no problem to call this function twice, since
 * array_finish nicely handle NULLed arguments.
 */
void static_analysis_finish(void)
{
	size_t args_count;
	size_t args_end;
	args_count = array_size(&analysis_arguments.args);

	/*
	 * If exists extra parameters, i.e, more than 5:
	 * [0] 1st: pbd
	 * [1] 2nd: -Wno-strict-prototypes
	 * [2] 3rd: -Wno-decl
	 * [.] (optional arguments here)
	 * [3] 4th: filename
	 * [4] 5th: (NULL)
	 * lets deallocate them.
	 */
	if (args_count > 5)
	{
		args_end = (args_count - 5) + 3;
		for (size_t i = 3; i < args_end; i++)
			free( array_get(&analysis_arguments.args, i, NULL) );
	}
	array_finish(&analysis_arguments.args);
	analysis_arguments.args = NULL;
}

/**
 * Do a static analysis in the C source code and returns a
 * hashtable with the breakpoint list.
 *
 * @param file Source to be analyzed.
 * @param func Function to be analyzed.
 * @param lines_l Lines list.
 * @param flags Program flags.
 *
 * @return Returns a breakpoint list containing all the lines
 * that PBD should break.
 *
 * @note This function is intended to be similar of what bp_createlist()
 * does, but in a 'smarter' way.
 */
struct hashtable *static_analysis(const char *file, const char *func,
	struct array *lines_l, uint64_t firstbreak)
{
	char *file_cur;
	struct dw_line *line;
	struct breakpoint *b;
	struct symbol_list *list;
	struct string_list *filelist;

	/*
	 * Enable our default standard if none is set.
	 * 'gnu11' standard seems to be the most used
	 * by default between multiples versions of GCC
	 * and Clang.
	 */
	if (!(args.flags & FLG_SANALYSIS_SETSTD))
		static_analysis_add_arg("-std=", "gnu11");

	/* Append our target file into the list. */
	array_add(&analysis_arguments.args, (char *)file);
	array_add(&analysis_arguments.args, NULL);

	/* Initialize breakpoint hashtable. */
	hashtable_init(&breakpoints, NULL);

	/*
	 * Allocate our first breakpoint to the first function
	 * instruction. Since this is an special case, there is
	 * no need to known the line number.
	 */
	b = malloc(sizeof(struct breakpoint));
	b->addr = firstbreak;
	b->original_byte = 0;
	b->line_no = 0;

	/*
	 * Set our first breakpoint to the very
	 * first instruction, PBD's main loop
	 * expect this.
	 */
	hashtable_add(&breakpoints, (void*)b->addr, b);

	/* Last line. */
	line = array_get(&lines_l, array_size(&lines_l) - 1, NULL);

	/* Allocate our second breakpoint. */
	b = malloc(sizeof(struct breakpoint));
	b->addr = line->addr;
	b->original_byte = 0;
	b->line_no = line->line_no;

	/* Last instruction, or so. */
	hashtable_add(&breakpoints, (void*)b->addr, b);

	/* Fill some infos. */
	filename = file;
	function = func;
	lines = lines_l;

	/* Init Sparse. */
	filelist = NULL;
	sparse_initialize(array_size(&analysis_arguments.args),
		(char **)analysis_arguments.args->buf, &filelist);

	/* Analyze. */
	FOR_EACH_PTR(filelist, file_cur) {
		list = sparse(file_cur);
		process(list);
	} END_FOR_EACH_PTR(file_cur);

	/* Since analysis is done, we can safely
	 * dealloc our arguments. */
	static_analysis_finish();
	return (breakpoints);
}
