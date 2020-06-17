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

#include <stdlib.h>
#include <stdint.h>

static uint64_t numb1 = 1;
static uint64_t numb2 = 2;
static uint64_t numb3 = 3;
static uint64_t numb4 = 4;
static uint64_t numb5 = 5;

static char array[40000];
static int n;
static int i;

/**
 * Dummy function that just do some 'heavy' computation
 * and changes the variables in the last moment.
 *
 * This routine exercises the best case scenario for
 * PBD: the assignment statements will be executed only
 * once, since the for loop and if will be ignored. The
 * execution time will be nearly identical to the one
 * without debugger.
 */
static void do_work1(void)
{
	for (int i = 0; i < n; i++)
	{
		if (i == n - 1)
		{
			numb1 = 7;
			numb2 = 8;
			numb3 = 9;
			numb4 = 10;
			numb5 = 11;
			array[39999] = numb1;
		}
	}
}

/**
 * Do some random 'heavy' computation in a 'real-world'-like
 * scenario: monitored breakpoints executes very often _and_
 * there are also statements that will be ignored.
 */
static void do_work2(void)
{
	for (int i = 0; i < n; i++)
	{
		/*
		 * This loop and the if statement inside will be
		 * completely ignored by PBD, since all the
		 * variables changes is outside the function scope.
		 */
		for (int j = 0; j < 10; j++)
		{
			if (j % 100 == 0)
			{
				int var;
				var = j * 50;
				var++;
			}
		}

		/* Comparison ignored. */
		if (i % 2 == 0)
		{
			/* Not ignored. */
			numb1 = 7;
			numb2 = 8;
			numb3 = 9;
			numb4 = 10;
			numb5 = 11;
			array[39999] = numb1;
		}
	}
}

/**
 * Random 'heavy' computation in the PBD's worst case
 * scenario: nothing or almost nothing will be ignored.
 */
static void do_work3(void)
{
	for (i = 0; i < n; i++)
	{
		/* Not ignored. */
		numb1 = 7;
		numb2 = 8;
		numb3 = 9;
		numb4 = 10;
		numb5 = 11;
		array[39999] = numb1;
	}
}

int main(int argc, char **argv)
{
	int f;

	if (argc < 2)
		return (1);

	n = atoi(argv[1]);
	f = atoi(argv[2]);

	if (f == 1)
		do_work1();
	else if (f == 2)
		do_work2();
	else
		do_work3();
}
