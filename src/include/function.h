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

#ifndef FUNCTION_H
#define FUNCTION_H

	#include <stdio.h>
	#include <inttypes.h>

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

	/* Amount of spaces to indent. */
	#define FUNCTION_INDENT_LEVEL 4

	extern char *fn_get_indent(size_t depth);

	extern int fn_printf(size_t depth, int extra_space, const char* fmt, ...);

	extern void fn_free_indent(char *buff);

	extern void fn_free(struct function *f);

#endif /* BREAKPOINT_H */
