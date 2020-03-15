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

#ifndef HIGHLIGHT_H
#define HIGHLIGHT_H

	#include <stdio.h>
	#include <string.h>
	#include <ctype.h>
	#include <limits.h>
	#include <errno.h>
	#include "hashtable.h"

	/* Highlighted line. */
	struct highlighted_line
	{
		size_t idx;
		size_t size;
	};

	/**
	 * Allocates a new Highlighted Buffer line.
	 *
	 * A Highlighted Buffer Line is a facility that transparently allows
	 * the user to append chars and strings inside a buffer without having
	 * to worry about space, reallocs and so on, something similiar to
	 * SDS strings.
	 *
	 * @returns Return a pointer to the highlighted line.
	 */
	extern char* highlight_alloc_line(void);

	/**
	 * Deallocate a Highlighted Line Buffer.
	 *
	 * @param line Highlighted Line Buffer to be deallocated.
	 */
	extern void highlight_free(char *line);

	/**
	 * For a given line @p line and a (already) allocated
	 * highlighted line buffer @p hl, highlights the
	 * line and returns @p hl with the highlighted line.
	 *
	 * @param line Line (null terminated string) to be highlighted.
	 * @param hl Pre-allocated Highlighted Line buffer.
	 *
	 * @return Returns a Highlighted Line Buffer.
	 */
	extern char *highlight_line(const char *line, char *hl);

	/**
	 * Initialize the syntax highlight engine.
	 *
	 * @param theme_file Theme file, if NULL, will use
	 * the internal theme.
	 *
	 * @return Returns 0 if success and 1 otherwise.
	 */
	extern int highlight_init(const char *theme_file);

	/**
	 * Finishes the highlight 'engine'.
	 */
	extern void highlight_finish(void);

#endif /* HIGHLIGHT_H */
