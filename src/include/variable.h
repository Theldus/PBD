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

#ifndef VARIABLE_H
#define VARIABLE_H

	#include "array.h"
	#include "dwarf_helper.h"
	#include "breakpoint.h"
	#include "function.h"
	#include  <sys/types.h>

	extern void var_dump(struct array *vars);

	extern int var_new_context(struct function *prev_ctx,
		struct function **curr_ctx, struct array *ctx_list);

	extern int var_deallocate_context(struct array *vars,
		struct array *context, int depth);

	extern int var_read(union var_value *value, struct dw_variable *v,
		pid_t child);

	extern void var_initialize(struct array *vars, pid_t child);

	extern void var_check_changes(struct breakpoint *bp, struct array *vars,
		pid_t child, int depth);

	extern void var_array_free(struct array *vars);

#endif /* VARIABLE_H */
