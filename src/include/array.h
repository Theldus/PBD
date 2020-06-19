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

#ifndef ARRAY_UTILS_H
#define ARRAY_UTILS_H

	#include <stdio.h>
	#include <string.h>
	#include <stdlib.h>

	/**
	 * Array initial size.
	 */
	#define ARRAY_UTILS_DEFAULT_SIZE 16

	/**
	 * Array structure.
	 */
	struct array
	{
		void **buf;
		size_t capacity;
		size_t elements;
	};

	/* External functions. */
	extern int array_init(struct array **array);
	extern int array_finish(struct array **array);
	extern int array_add(struct array **ar, void *e);
	extern void* array_remove(struct array **ar, size_t pos, void **e);
	extern void *array_remove_last(struct array **ar, void **e);
	extern void* array_get(struct array **array, size_t pos, void **e);
	extern void *array_get_last(struct array **array, void **e);
	extern size_t array_size(struct array **array);
	extern int array_sort(struct array **ar,
		int (*cmp)(const void*,const void*));
	extern int array_selftest(void);

#endif /* ARRAY_UTILS_H */
