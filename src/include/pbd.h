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

#ifndef PBD_H
#define PBD_H

	/* Current version. */
	#define MAJOR_VERSION 0
	#define MINOR_VERSION 6
	#define RLSE_VERSION ""

	/* Arguments flags. */
	#define FLG_SHOW_LINES       0x01
	#define FLG_ONLY_LOCALS      0x02
	#define FLG_ONLY_GLOBALS     0x04
	#define FLG_IGNR_LIST        0x08
	#define FLG_WATCH_LIST       0x10
	#define FLG_DUMP_ALL         0x20
	#define FLG_IGNR_EQSTAT      0x40
	#define FLG_SYNTAX_HIGHLIGHT 0x80

	/* Experimental features.
	 *
	 * If something goes wrong, the following experimental flags
	 * can be disabled.
	 */
	/* none yet. */

	/* Program arguments. */
	struct args
	{
		uint8_t flags;
		int context;
		struct iw_list
		{
			char *list;
			struct hashtable *ht_list;
		} iw_list;
		char *executable;
		char *function;
		char *theme_file;
		char **argv;
	};

	extern struct args args;
	extern void finish(void);
	extern void usage(int retcode, const char *prg_name);

#endif /* PDB_H */
