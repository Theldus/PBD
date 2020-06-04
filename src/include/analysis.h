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

#ifndef ANALYSIS_H
#define ANALYSIS_H

	#include <stdio.h>
	#include <string.h>
	#include "dissect.h"
	#include "scope.h"
	#include "ptrlist.h"
	#include "breakpoint.h"
	#include "dwarf_helper.h"

	/*
	 * Arguments structure.
	 *
	 */
	struct analysis_struct
	{
		struct array *args;
	};

	extern int static_analysis_add_arg(
		const char *arg1,
		const char *arg2);

	extern int static_analysis_init(void);
	extern void static_analysis_finish(void);

	extern struct hashtable *static_analysis(
		const char *file,
		const char *func,
		struct array *lines);


#endif /* ANALYSIS_H */
