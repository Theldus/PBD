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

#include <stdint.h>
#include <stdio.h>
#include "cpudisp.h"

/* Masks. */
#define OSXSAVE_MASK       (1 << 27)
#define AVX2_MASK          (1 <<  5)
#define XCR0_SSE           (1 <<  1)
#define XCR0_AVX           (1 <<  2)
#define XCR0_SIMD_SUPPORT  (XCR0_SSE|XCR0_AVX)

/**
 * CPUID instruction.
 *
 * @param eax EAX Register.
 * @param ecx ECX Register.
 * @param abcd EAX, EBX, ECX and EDX registers.
 *
 */
static void __cpuid(uint32_t eax, uint32_t ecx, uint32_t* abcd)
{
	uint32_t ebx, edx;
	ebx = 0;
	edx = 0;
	__asm__ __volatile__
	(
		"cpuid"
		: "+b" (ebx), "+a" (eax), "+c" (ecx), "=d" (edx)
	);

	abcd[0] = eax;
	abcd[1] = ebx;
	abcd[2] = ecx;
	abcd[3] = edx;
}

/**
 * Checks if SSE and AVX are enabled in the OS.
 *
 * @return Returns 1 if enabled and 0 otherwise.
 */
int check_xcr0()
{
	uint32_t xcr0;
	__asm__ __volatile__
	(
		"xgetbv"
		: "=a" (xcr0)
		: "c" (0)
		: "%edx"
	);
	return ((xcr0 & XCR0_SIMD_SUPPORT) == XCR0_SIMD_SUPPORT);
}

/**
 * Checks if the CPU supports AVX2 and the OS
 * is able to use.
 *
 * @return Returns 1 if AVX2 is supported and 0
 * otherwise.
 */
int supports_avx2()
{
	uint32_t abcd[4];

	/* OSXSAVE bit, EAX = 01h, ECX = 0h, result in ECX. */
	__cpuid(1, 0, abcd);
	if ((abcd[2] & OSXSAVE_MASK) != OSXSAVE_MASK)
		return (0);

	/* AVX and SSE must be enabled. */
	if (!check_xcr0())
		return (0);

	/*
	 * Structured Extended Feature Flags
	 * AVX2 Support, EAX = 07h, ECX = 0h, result in EBX, bit 5.
	 */
	__cpuid(7, 0, abcd);
	if ((abcd[1] & AVX2_MASK) != AVX2_MASK)
		return (0);

	return (1);
}

/**
 * Select the appropriate function pointers
 * accordingly with the host CPU.
 */
void select_cpu(void)
{
	/* If AVX2 Enabled. */
#ifdef CAN_BUILD_AVX2
	if (supports_avx2())
		offmemcmp = offmemcmp_avx2;
	else
#endif
		offmemcmp = offmemcmp_sse2;
}
