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

#include "function.h"
#include <stdlib.h>
#include <string.h>

/* Static buffer for indent level. */
static char fn_indent_buff[64 + 1] = {0};

/**
 * @brief Returns a constant indented string for the
 * current indentation level.
 *
 * @param depth Current indentation level.
 *
 * @return Returns a constant string indented by
 * FUNCTION_INDENT_LEVEL times.
 *
 * @note Its important to note that for 64 characters (or 16
 * levels by using the default FUNCTION_INDENT_LEVEL size, i.e: 4)
 * buffer is already pre-allocated, thus, reducing the overhead
 * of memory allocation.
 *
 * For more than 64 characters, fn_get_indent() will allocate a buffer
 * with enough room; in this case, its up to the caller to free the
 * memory by calling fn_free_indent() which will adequately free
 * the memory if thats the case.
 */
char *fn_get_indent(size_t depth)
{
	char *buff;

	/* Sanity checking. */
	if (depth < 1)
		return (NULL);

	/* Pre-allocated buffer. */
	if (depth < (sizeof(fn_indent_buff) - 1) / FUNCTION_INDENT_LEVEL)
	{
		depth--;
		memset(fn_indent_buff, ' ', depth * FUNCTION_INDENT_LEVEL);
		fn_indent_buff[depth * FUNCTION_INDENT_LEVEL] = '\0';
		return (fn_indent_buff);
	}

	/* Allocate a new buffer. */
	else
	{
		depth--;
		buff = calloc(1, sizeof(char) * (depth * FUNCTION_INDENT_LEVEL + 1));
		memset(buff, ' ', depth * FUNCTION_INDENT_LEVEL);
		buff[depth * FUNCTION_INDENT_LEVEL] = '\0';
		return (buff);
	}
}

/**
 * @brief Prints a formatted string with already indentation.
 *
 * This function is recommended whenever a formatted print
 * with indentation is required, since fn_printf() also
 * deallocates the buffer provided by fn_get_indent().
 *
 * @param depth Function depth.
 * @param fmt Formatted string to be printed.
 */
void fn_printf(size_t depth, const char* fmt, ...)
{
	char *buffer;
    va_list args;

    /* Indent level. */
    fputs( (buffer = fn_get_indent(depth)), stdout );

    /* Print formatted string. */
    va_start(args, fmt);
    vfprintf(stdout, fmt, args);
    va_end(args);

    /* Free buffer. */
    fn_free_indent(buffer);
}

/**
 * @brief Free the memory alocated by the fn_get_indent()
 * regardless of what buffer was used.
 *
 * @param buff Buffer to be freed.
 */
void fn_free_indent(char *buff)
{
	if (fn_indent_buff >= buff &&
		buff < fn_indent_buff+sizeof(fn_indent_buff))
		return;

	free(buff);
}

/**
 * @brief Deallocates the remaining variables and the
 * last function context.
 *
 * @param f Function context.
 */
void fn_free(struct function *f)
{
	var_array_free(f->vars);
	free(f);
}
