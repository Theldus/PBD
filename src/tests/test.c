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

#include <stdio.h>
#include <string.h>
#include <inttypes.h>

/*
 * == Note ==
 * I'm assuming that PBD will not be used in *exotic*
 * systems, so I will resume all the C types into the
 * following sizes below:
 */
int8_t   gi8;
uint8_t  gu8;
int16_t  gi16;
uint16_t gu16;
int32_t  gi32;
uint32_t gu32;
int64_t  gi64;
uint64_t gu64;

/* Arrays Test. */
char array1dim[10] = {0};
int array10x10[10][10][10];

/* Pointer test. */
int *integer_pointer;

/* Enums test, */
enum animals{cat = 1, dog = 1, elephant = 3, monkey = 4};
enum animals anim_vect[4];

/* Stub struct. */
struct stub_struct
{
	int foo;
	int bar;
};

/*===========================================================================*
 * Iterative analysis                                                        *
 *===========================================================================*/

/*
 * Changing vars outside the monitored function.
 */
void func2(int *func2_local_argument1)
{
	/* Change a global var from a side function. */
	gi64 += 1;

	/* Change a pointer. */
	(*func2_local_argument1)++;
}

/*
 * General tests, with local vars, global, loops
 * and blocks.
 */
int func1(int func1_local_argument1)
{
	int         func1_local_a = 3;
	int         func1_local_b;
	float       func1_local_c;
	double      func1_local_d;
	long double func1_local_e;

	/* Enums. */
	anim_vect[0] = cat;
	anim_vect[1] = dog + anim_vect[0];
	anim_vect[2] = elephant;
	anim_vect[3] = monkey;

	/* Pointer. */
	integer_pointer = (int *) 0xdeadbeeb;
	integer_pointer++;

	/* Arrays single dimension test. */
	for (size_t i = 0; i < 10; i++)
		array1dim[i] = i+1;

	array1dim[9] += 9;

	/* Fill test. */
	memset(array1dim, 5, sizeof(array1dim));

	/* Array multiple dimension test. */
	array10x10[5][7][6]++;

	func1_local_b = 5 + func1_local_a;

	/* Change an argument/local var. */
	func1_local_argument1++;

	/*
	 * If a function changes a local or global
	 * variable, this change will also be tracked.
	 */
	func2(&func1_local_b);

	/* Floating-point variables. */
	func1_local_d = 2.03;
	func1_local_c = func1_local_d + 0.11;
	func1_local_c++;

	/* Long double test. */
	func1_local_e = 1.1234;
	func1_local_e++;

	/*
	 * Variables defined inside a block scope are
	 * _purposely_ not tracked.
	 */
	{
		int func1_block_a;
		int func1_block_b;

		func1_block_a = 42;
		func1_block_b = func1_block_a + 1;
		func1_block_b++;
	}

	/*
	 * Which is useful for loop indexes:
	 * If the purpose _is_ to debug the loop index,
	 * please make it at the function scope.
	 */
	for (int i = 0; i < 5; i++)
		func1_local_d = (i * 5);

	/* Set all global vars. */
	 gi8 = INT8_MAX;
	 gu8 = -1;
	gi16 = INT16_MAX;
	gu16 = -1;
	gi32 = INT32_MAX;
	gu32 = -1;
	gi64 = INT64_MAX;
	gu64 = -1;

	return (0);
}

/*===========================================================================*
 * Static Analysis                                                           *
 *===========================================================================*/

/**
 * This *very* ugly routine tests multiples cases that the
 * static analysis parser should watch/ignore, ranging from
 * trivial use cases to more unusual.
 *
 * @note This function is not meant to be compiled and/or
 * executed, it just serves as a target function to the
 * static analyzer.
 */
void static_analysis_func3(int param)
{
	((void)param);
#ifdef STATIC_ANALYSIS
	typedef int mytype_t;

	/* Mixed declarations and initializers. */
	int var_a;
	mytype_t var_b = 3;

	/* Enumerations. */
	enum animals anim;
	anim = cat;

	/* Structs, should not track. */
	struct stub_struct ss;
	ss.foo = 1;

	/*
	 * Scopes:
	 * - Global
	 * - Function scope
	 * - Function outside scope.
	 */
	var_a = 0; /* Local scope, simplest assignment.   */
	gi64  = 0; /* Global scope.                       */
	param = 0; /* Parameter is equals function scope. */
	{
		/* Declaration and assignment outside scope. */
		int var_c = 3;
		var_c++;
	}

	/*
	 * Assignment inside declaration initializer.
	 * var_a++ will be ignored, since ptr_a is enough to track
	 * the line. If ptr_a is outside the scope, var_a would be
	 * analyzed.
	 */
	char *ptr_a = (char *)var_a++;
	{
		char *ptr_a = (char *)var_a++;
	}

	/* Pointer change, content change and multiples assignments. */
	ptr_a++;
	*ptr_a = 'F';
	ptr_a[5] = 'O';
	ptr_a[var_a = 3] = 'O';

	/* Nested scopes/compound statements. */
	{
		var_a++;
		{
			int vet_a[10];
			{
				vet_a[var_a + 1] = 2;
			}
		}
	}

	/* Multiples dimensions array. */
	char array_a[10][5][2];
	array_a[2][3][4] = 'B';

	/* For Loops. */
	for (int i = 0; i < 10; i++) {        /* With compound statement. */
		var_a++;
	}
	for (int i = 0; i < 10; i++)
		var_a++;

	for (var_a = 0; var_a < 10;)          /* Assign inside init.      */
		{ int dummy; dummy++; }
	for (;var_a = 0;)                     /*        inside condition. */
		{ int dummy; dummy++; }
	for (; var_a < 10; var_a = var_a + 1) /*        inside loop expr. */
		{ int dummy; dummy++; }

	/* While. */
	while (var_a < 10) /* Not assign.       */
		{ int dummy; dummy++; }
	while (var_a = 5)  /* Inside condition. */
		{ int dummy; dummy++; }
	while (var_a) {
		int dummy = 1;
		(var_a < 10) ? var_b++ : 0; /* Ternary operator. */
	}

	/* Do..While. */
	do
	{
		var_b += 1;
	} while (var_a < 10);

	/* Assignment inside condition. */
	do
	{
		var_b = var_b + 1;
	} while (var_a < 10 && var_a++);

	/* Function call. */
	func1(0xbeef);

	/* Switch/case. */
	switch (var_b = 3) /* Inside var. */
	{
		case 0:
			var_a = 3;
			break;
		case 1:
			func1(0xcafe);
			break;
		case 2:
			{
				int var_a; /* Local scope of var_a, should not watch. */
				var_a = 10;
				var_a = (char)(var_b++); /* Assign with cast expr. */
			}
			break;
		default:
			/* Comma expr. */
			var_a = 3, var_b = 5;
			break;
	}

	/* If statements. */
	if (var_a)
	{
		if (var_b)
			var_b = var_a + 1;
		else
		{
			if (gi64 > (var_b = 1))
				gi64++;
		}
	}
#endif
}

/*===========================================================================*
 * Recursive analysis                                                        *
 *===========================================================================*/

/**
 * Factorial function.
 *
 * @param Current factorial number.
 *
 * @return Returns the factorial of n.
 */
uint64_t factorial(uint64_t n)
{
	uint64_t result;
	if(n == 0)
	{
		result = 1;
		return (result);
	}
	else
	{
		result = n;
		result = n * factorial(n - 1);
		return (result);
	}
}

/**
 * Entry point
 */
int main()
{
	/* Multiples function calls. */
	for (int i = 0; i < 2; i++)
		func1(i);

	factorial(10);

	return (0);
}
