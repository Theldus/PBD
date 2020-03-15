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

#ifndef LINE_H
#define LINE_H

	#define _POSIX_C_SOURCE 200809L
	#define _XOPEN_SOURCE 700

	#if defined(__GLIBC__) && \
		(__GLIBC__ < 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ < 10) )

		#define _GNU_SOURCE
	#endif

	#include "dwarf_helper.h"
	#include "variable.h"
	#include <stdio.h>

	/* Output buffer size. */
	#define BS 64

	void (*line_output)(
		int depth, unsigned line_no,
		struct dw_variable *v, union var_value *v_before,
		union var_value *v_after, int *array_idxs);

	extern int line_read_source(const char *filename, int highlight,
		char *theme_file);

	extern void line_free_source(int highlight);

	extern void line_null_printer(int depth, unsigned line_no,
		struct dw_variable *v, union var_value *v_before,
		union var_value *v_after, int *array_idxs);

	extern void line_default_printer(int depth, unsigned line_no,
		struct dw_variable *v, union var_value *v_before,
		union var_value *v_after, int *array_idxs);

	extern void line_detailed_printer(int depth, unsigned line_no,
		struct dw_variable *v, union var_value *v_before,
		union var_value *v_after, int *array_idxs);

#endif /* LINE_H */
