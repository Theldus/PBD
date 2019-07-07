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

#ifndef UTIL_H
#define UTIL_H

	#include <stdarg.h>
	#include <stdio.h>
	#include <stdlib.h>

	/**
	 * Triggers a compile time error if the expression
	 * evaluates to 0.
	 */
	#define COMPILE_TIME_ASSERT(expr)  \
    	switch(0){case 0:case expr:;}

	/**
	 * Quits with a return code and a formatted string.
	 *
	 * @param code Error code to be returned
	 * @param fmt Formatted string to be printed.
	 */
	static void quit(int code, const char* fmt, ...)
	{
	    va_list args;
	    
	    va_start(args, fmt);
	    vfprintf(stderr, fmt, args);
	    va_end(args);
	    
	    exit(code);
	}

#endif /* UTIL_H */
