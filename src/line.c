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

#include "line.h"
#include "array.h"
#include "highlight.h"
#include "dwarf_helper.h"
#include "pbd.h"
#include <ctype.h>
#include <libgen.h>
#include <math.h>

/* Compile unit source code. */
struct array *source_lines = NULL;
struct array *source_lines_highlighted = NULL;

/*
 * Buffer
 *
 * This buffer holds the value from before and
 * after the value is changed for a single
 * TBASE_TYPE, TENUM and TPOINTER.
 */
static char before[BS];
static char after[BS];

/* Current function pointer. */
void (*line_output)(
	int depth, unsigned line_no,
	struct dw_variable *v, union var_value *v_before,
	union var_value *v_after, int *array_idxs) = line_default_printer;

/* Base file name. */
char *base_file_name = NULL;

/**
 * @brief Compares two lines accordingly with the
 * line number and returns if there is a difference.
 *
 * @param a First line.
 * @param b Second line.
 *
 * @return Returns a negative, integer or zero if the first
 * line is less than, equal to, or greater than the second
 * line.
 */
int line_cmp(const void *a, const void *b)
{
	struct dw_line *l1 = *(void **)a;
	struct dw_line *l2 = *(void **)b;
	return ((int)l1->line_no - (int)l2->line_no);
}

/**
 * @brief Check if the parameter @p c is a valid variable
 * and/or function name character.
 *
 * @param c Character to be checked.
 *
 * @return Returns 1 if valid or 0 otherwise.
 */
inline static int line_is_valid_variable_name(char c)
{
	return (isalpha(c) || isdigit(c) || c == '_');
}

/**
 * @brief For a given line @p line, gets the index of the variable
 * (if one) inside the string or no one if found, gets the index
 * of the first printable character.
 *
 * @param line Line to be checked.
 * @param var_name Target variable name.
 *
 * @return Returns the index of the variable (if applicable) or
 * the index of the first printable character found.
 */
static int line_first_printable_or_var
(
	const char * const line,
	const char * const var_name)
{
	const char *p; /* Substring variable.   */
	const char *e; /* End pointer.          */
	int i;         /* Loop index.           */
	size_t len;    /* Variable name length. */

	len = strlen(var_name);
	p   = line;
	e   = line + strlen(line);

	/* Searches for the variable name. */
	while ((p = strstr(p, var_name)) != NULL)
	{
		if ( (size_t)(e - p + 1) >= len
			&& line_is_valid_variable_name( p[len] ) )
		{
			p += len;
			continue;
		}
		else
			break;
	}

	/* If found. */
	if (p != NULL)
		return (p - line);


	/* Not found, gets the index of first printable char. */
	for (i = 0; line[i] != '\0' && isspace(line[i]); i++);
	return (i);
}

/**
 * @brief Reindent a given line @p line by replacing all tabs
 * for spaces, this is needed for the proper alignment in
 * line_detailed_printer() routine.
 *
 * @param line Line to be reindented.
 *
 * @return Returns a new allocated line (user should free with
 * free()) with all tabs replaced with spaces.
 *
 */
static char *line_reindent(const char *line)
{
	char *reindented_line; /* Target reindented line. */
	int n_tabs;            /* Amount of tabs.         */
	int n_spaces;          /* Amount of spaces.       */
	int i;                 /* Loop index.             */

	/* Amount of tabs and spaces. */
	n_tabs   = 0;
	n_spaces = 0;
	for (i = 0; line[i] != '\0'; i++)
	{
		if (line[i] == '\t')
			n_tabs++;
		else if (line[i] == ' ')
			n_spaces++;
		else
			break;
	}

	/* Total space amount. */
	n_spaces = n_spaces + (n_tabs * FUNCTION_INDENT_LEVEL);
	reindented_line = calloc(sizeof(char), (n_spaces + strlen(line) - i + 1));

	/* Assemble the final line. */
	memset(reindented_line, ' ', n_spaces);
	strcat(reindented_line, line + i);

	return (reindented_line);
}

/**
 * @brief Given a complete source file name @p filename,
 * read the entire file and saves into an array.
 *
 * @param filename File to be read.
 * @param highlight Enable (or not) syntax highlight.
 *
 * @return Returns 0 if success and a negative number
 * otherwise.
 */
int line_read_source(const char *filename, int highlight, char *theme_file)
{
	FILE *fp;         /* File pointer.   */
	char *line;       /* Current line.   */
	char *tmp;        /* Tmp line.       */
	size_t len;       /* Allocated size. */
	ssize_t read;     /* Bytes read.     */
	char *reindented; /* Reindent line. */

	line = NULL;
	len  = 0;

	/* Try to read the source. */
	fp = fopen(filename, "r");
	if (fp == NULL)
		return (-1);

	/* Initialize array. */
	array_init(&source_lines);

	/* If syntax highlight on.
	 *
	 * Note that using syntax highlight ends up using memory memory,
	 * since we still need the original source code in order to
	 * calculate the 'cursor' positioning, in the 'line_detailed_printer'.
	 */
	if (highlight)
	{
		/* Initialize syntax highlighting. */
		if (highlight_init(theme_file) < 0)
		{
			fclose(fp);
			array_finish(&source_lines);
			source_lines = NULL;
			return (-1);
		}

		array_init(&source_lines_highlighted);

		/* Read the entire file. */
		while ((read = getline(&line, &len, fp)) != -1)
		{
			array_add(&source_lines,
				(reindented = line_reindent(line)));

			array_add(&source_lines_highlighted,
				highlight_line(reindented, NULL));
		}

		highlight_finish();
	}
	else
	{
		/* Read the entire file. */
		while ((read = getline(&line, &len, fp)) != -1)
			array_add(&source_lines, line_reindent(line));
	}

	/* Set base name. */
	tmp = basename((char *)filename);
	base_file_name = malloc(sizeof(char) * (strlen(tmp) + 1));
	strcpy(base_file_name, tmp);

	free(line);
	fclose(fp);
	return (0);
}

/**
 * @brief Free all the allocated resources for line usage,
 * including the array lines and base_file_name.
 */
void line_free_source(int highlight)
{
	size_t size; /* Amout of lines. */

	/* If there is something to be removed. */
	if (source_lines == NULL)
		return;

	size = array_size(&source_lines);

	if (highlight)
	{
		for (size_t i = 0; i < size; i++)
		{
			free( array_remove(&source_lines, 0, NULL) );
			highlight_free(
				array_remove(&source_lines_highlighted, 0, NULL) );
		}
		array_finish(&source_lines_highlighted);
	}
	else
	{
		for (size_t i = 0; i < size; i++)
			free( array_remove(&source_lines, 0, NULL) );
	}

	array_finish(&source_lines);

	/* Free the base name. */
	free(base_file_name);
}

/**
 * @brief Dummy function that does nothing...
 * useful for debugging purposes only.
 */
void line_null_printer(int depth, unsigned line_no,
	struct dw_variable *v, union var_value *v_before,
	union var_value *v_after, int *array_idxs)
{
	/* Avoid unused args. */
	((void)depth);
	((void)line_no);
	((void)v);
	((void)v_before);
	((void)v_after);
	((void)array_idxs);
}

/**
 * @brief Prints the line number changed, the variable name, its
 * context and before/after values.
 *
 * @param depth Current function depth.
 * @param line_no Current line number.
 * @param v Variable analized.
 * @param v_before Value before being changed.
 * @param v_after Value after being changed.
 * @param array_idxs Computed index, only applicable
 *        if variable is an array, otherwise,
 *        this value can safely be NULL.
 */
void line_default_printer(int depth, unsigned line_no,
	struct dw_variable *v, union var_value *v_before,
	union var_value *v_after, int *array_idxs)
{
	/* If base type. */
	if (v->type.var_type & (TBASE_TYPE|TENUM|TPOINTER))
	{
		fn_printf(depth, 0,
			"[Line: %d] [%s] (%s) %s!, before: %s, after: %s\n",
			line_no,
			(v->scope == VGLOBAL) ? "global" : "local",
			v->name,
			(!v->initialized ? "initialized" : "has changed"),
			var_format_value(before, v_before, v->type.encoding, v->byte_size),
			var_format_value(after,  v_after, v->type.encoding, v->byte_size)
		);
	}

	/* If array. */
	else if (v->type.var_type == TARRAY)
	{
		/* Print indexes and values. */
		fn_printf(depth, 0,
			"[Line: %d] [%s] (%s",
			line_no,
			(v->scope == VGLOBAL) ? "global" : "local",
			v->name
		);

		for (int j = 0; j < v->type.array.dimensions; j++)
			printf("[%d]", array_idxs[j]);

		printf(") has changed!, before: %s, after: %s\n",
			var_format_value(before, v_before, v->type.encoding,
				v->type.array.size_per_element),

			var_format_value(after,  v_after, v->type.encoding,
				v->type.array.size_per_element)
		);
	}
}

/**
 * @brief Prints the file name, line number changed, the line,
 * variable name and before/after values.
 *
 * @param depth Current function depth.
 * @param line_no Current line number.
 * @param v Variable analized.
 * @param v_before Value before being changed.
 * @param v_after Value after being changed.
 * @param array_idxs Computed index, only applicable
 *        if variable is an array, otherwise,
 *        this value can safely be NULL.
 */
void line_detailed_printer(int depth, unsigned line_no,
	struct dw_variable *v, union var_value *v_before,
	union var_value *v_after, int *array_idxs)
{
	struct array *line_ptr;
	char *line;
	size_t size;

	line_ptr = source_lines;
	line = array_get(&line_ptr, line_no - 1, NULL);
	size = array_size(&line_ptr);

	/*
	 * The line alignment is pretty trickier, it takes:
	 * - the base filename
	 * - the amount of digits of line number
	 * - the adjustment of the 'cursor "^"' based in the variable name (if one)
	 * plus the cosmetic symbols: "[", ":", "]" and ":".
	 *
	 * Pay attention while changing this.
	 *
	 * TODO: Think of a better solution.
	 */
	int predicted_offset =
		strlen(base_file_name)                     +
		line_first_printable_or_var(line, v->name) +
		(log10(line_no) + 1)                       +
		4;

	/* Read highlighted line if available. */
	if (source_lines_highlighted != NULL)
	{
		line = array_get(&source_lines_highlighted, line_no - 1, NULL);
		line_ptr = source_lines_highlighted;
	}

	/* If base type. */
	if (v->type.var_type & (TBASE_TYPE|TENUM|TPOINTER|TARRAY))
	{
		int start;
		int end;

		/* Print lines before. */
		if (args.context)
		{
			printf("----------------------------------------------------------"
				"---------------------\n");

			if (line_no - args.context > 0)
				start = line_no - args.context - 1;
			else
				start = 0;

			end = line_no - 1;

			while (start < end)
			{
				fn_printf(depth, 0, "[%s:%d]:%s", base_file_name, start+1,
					array_get(&line_ptr, start, NULL));
				start++;
			}
		}

		/* Line changed. */
		fn_printf(depth, 0, "[%s:%d]:%s", base_file_name, line_no, line);

		/* If not array, lets proceed normally. */
		if (v->type.var_type != TARRAY)
		{
			fn_printf(depth, predicted_offset,
				"^----- (%s) before: %s, after: %s\n",
				v->name,
				var_format_value(before, v_before, v->type.encoding, v->byte_size),
				var_format_value(after,  v_after, v->type.encoding, v->byte_size)
			);
		}

		/* If array, we also need to print the indexes. */
		else
		{
			fn_printf(depth, predicted_offset,
				"^----- (%s",
				v->name,
				var_format_value(before, v_before, v->type.encoding, v->byte_size),
				var_format_value(after,  v_after, v->type.encoding, v->byte_size)
			);

			for (int j = 0; j < v->type.array.dimensions; j++)
				printf("[%d]", array_idxs[j]);

			printf("), before: %s, after: %s\n",
				var_format_value(before, v_before, v->type.encoding,
					v->type.array.size_per_element),

				var_format_value(after,  v_after, v->type.encoding,
					v->type.array.size_per_element)
			);
		}

		/* Print lines after. */
		if (args.context)
		{

			if (line_no + args.context <= size)
				end = line_no + args.context;
			else
				end = size - 1;

			start = line_no;
			while (start < end)
			{
				fn_printf(depth, 0, "[%s:%d]:%s", base_file_name, start+1,
					array_get(&line_ptr, start, NULL));
				start++;
			}

			printf("----------------------------------------------------------"
				"---------------------\n\n");
		}
		printf("\n");
	}
}
