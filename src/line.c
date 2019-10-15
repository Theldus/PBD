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

/*
 * Buffer
 *
 * This buffer holds the value from before and
 * after the value is changed for a single
 * TBASE_TYPE.
 */
static char before[BS];
static char after[BS];

/* Current function pointer. */
void (*line_output)(
	int depth, unsigned line_no,
	struct dw_variable *v, union var_value *v_before,
	union var_value *v_after, int *array_idxs) = line_default_printer;

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
	if (v->type.var_type == TBASE_TYPE)
	{
		fn_printf(depth,
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
		fn_printf(depth,
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
