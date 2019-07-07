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

#ifndef DWARF_UTILS_H
#define DWARF_UTILS_H

	#include <sys/types.h>
	#include <sys/stat.h>
	#include <fcntl.h>
	#include <stdlib.h>
	#include <unistd.h>
	#include <stdio.h>
	#include <stdlib.h>
	#include <stdarg.h>
	#include <string.h>
	#include <stdint.h>
	#include <dwarf.h>
	#include <libdwarf.h>

	#include "array.h"
	
	/* Variables. */
	enum VAR_SCOPE {VLOCAL, VGLOBAL};
	enum VAR_TYPE {TBASE_TYPE, TARRAY, TSTRUCTURE, TUNION, TENUM, TPOINTER};

	/* Line types. */
	enum LINE_TYPE {LBEGIN_STMT = 1, LEND_SEQ = 2, LBLOCK = 4};

	/*
	 * Who uses more than 8 dimensions? more than enough,
	 * right?
	 */
	#define MATRIX_MAX_DIMENSIONS 8

	/**
	 * Function structure.
	 */
	struct dw_function
	{
		/*
		 * Points to the first byte of memory containing
		 * the function.
		 */
		uint64_t low_pc;

		/*
		 * Points to the last by of memory containing
		 * the function.
		 */
		uint64_t high_pc;

		/*
		 * Base pointer offset.
		 *
		 * Since GCC and CLANG (and perhaps others compilers too)
		 * uses different schemes to refer to the local variable
		 * location, bp_offset contains the offset to be added
		 * into the base pointer in order to get the right
		 * location.
		 *
		 * Since CLANG (until now) do not use loclists (with -gdwarf-2)
		 * bp_offset will always point to 0.
		 */
		int bp_offset;
	};

	/**
	 * Line structure.
	 */
	struct dw_line
	{
		/* Line address. */
		uint64_t addr;

		/* Line number. */
		unsigned line_no;

		/*
		 * Line type:
		 * A line can be one (or more) of the three
		 * types below:
		 * - Begin statement (that's the one we want)
		 * - End sequence
		 * - Line block
		 */
		unsigned line_type;
	};

	/**
	 * Dwarf Utils structure.
	 */
	struct dw_utils
	{
		int fd;
		Dwarf_Debug dbg;
		Dwarf_Die cu_die;
		Dwarf_Die fn_die;

		/*
		 * dw_function structure
		 */
		struct dw_function dw_func;
	};

	/**
	 * Variable structure
	 */
	struct dw_variable
	{
		char *name;
		enum VAR_SCOPE scope;

		/*
		 * For TBASE_TYPE variables, the u64_value
		 * is more than enough to hold the type. If
		 * not TBASE_TYPE, p_value should be used to
		 * hold a pointer to the current location.
		 */
		union var_value
		{
			uint64_t u64_value;
			double d_value;
			float f_value;
			void *p_value;
		} value;

		/*
		 * If the variable is global or static,
		 * the address should be used, if local,
		 * fp_offset.
		 */
		union location
		{
			off_t  fp_offset;
			uint64_t address;
		} location;

		size_t byte_size;

		struct type
		{
			int var_type;
			int encoding;

			/*
			 * Not all variables are arrays, but when so,
			 * we need some space to it, right?.
			 *
			 * Kinda waste of space here, but...
			 */
			struct arrayt
			{
				size_t size_per_element;
				int dimensions;
				int elements_per_dimension[MATRIX_MAX_DIMENSIONS];
			} array;
		} type;
	};

	extern int *dw_init(const char *file, struct dw_utils *dw);
	
	extern void dw_finish(struct dw_utils *dw);
	
	extern int dw_next_cu_die(struct dw_utils *dw, Dwarf_Die *die);
	
	extern int get_address_by_function(struct dw_utils *dw, const char *func);

	extern struct array *get_all_variables(struct dw_utils *dw);

	extern struct array *get_all_lines(struct dw_utils *dw);

#endif /* DWARF_UTILS_H */
