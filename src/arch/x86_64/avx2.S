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

#define ALIGNED 0

#if ALIGNED == 1
	#define VMOVDQx vmovdqa
#else
	#define VMOVDQx vmovdqu
#endif

.macro eight_byte_comparison
	mov (%rsi, %r9, 1), %r8
	cmp %r8, (%rdi, %r9, 1)
	jne .found_8byte

	add $8, %r9
.endm

.macro one_byte_comparison
	movb (%rsi, %r9, 1), %r8b
	cmpb %r8b, (%rdi, %r9, 1)
	jne .found_1byte

	add $1, %r9
.endm

.macro avx2_comparison reg2
	add $32, %r9

	VMOVDQx -32(%rdi,%r9,1), %\reg2
	vpcmpeqb %ymm1, %\reg2, %\reg2
	vpmovmskb %\reg2, %r8
	cmp %r11, %r8
	jne .found_32byte
.endm

/*
 * Memory Comparison (with byte offset) routine (SSE2 Version)
 *
 * int64_t offmemcmp_avx2 (void *src, void *dest, size_t block_size,
 *	size_length)
 *
 * @p src Src pointer
 * @p dst Dst pointer
 * @p block_size Memory block size, in bytes.
 * @p length Memory length.
 *
 * This function performs similar as the usual memcmp but returns
 * the offset @p block_size aligned.
 *
 * For instance, given two integers (4-byte) arrays that differs
 * in the third position, this function would return '12', meaning
 * that from the 12 byte, there is a difference. Note also that,
 * if the byte 12,13,14 or 15 differs between the two pointers, this
 * function still returns '12'.
 *
 * For block_size equals '1', this function will return
 * the exact byte offset.
 *
 * If both pointers are equal, -1 is returned, otherwise,
 * a number greater than or equals 0 if returned.
 *
 * Implementation details:
 * -----------------------
 *
 * As can be easily noted, this is an assembly version of offmemcmp
 * written in C. This version differs from the former in the sense
 * that this routine uses SIMD registers, (AVX2/256-bit registers)
 * in order to speedup the comparison.
 *
 * Although pretty simple, this function tries to align both pointers,
 * whether 8-byte per iteration or 1-byte. When aligned (or not),
 * the remaining bytes are compared by a 64-byte loop with 2x32-bytes
 * comparisons. If there are remaining bytes, 8-bytes/1-byte comparisons
 * are made.
 *
 * Performance
 * -----------
 *
 * In contrast to the SSE2 version, the AVX2 manages to keep on par with
 * (glibc) memcmp, staying at most 1-5% of performance difference. This
 * does not means that offmemcmp_avx2 can completely substitute memcmp
 * (in terms of performance) for all kinds of inputs, but I think its a
 * good replacement =).
 *
 * Requirements
 * ------------
 *
 * Requires an AVX2-compatible processor, i.e: a fourth Intel generation
 * processor/Haswell (2013) or equivalent.
 *
 *
 * Register usage:
 * ---------------
 * rdi = v1
 * rsi = v2
 * rdx = block_size
 * rcx = n
 *
 * Our 'variables'
 * rax = return value, duh
 * r8 = temp
 * r9 = temp / offset
 * r10 = temp / skip counter (for align loop)
 * r11 = temp
 */
.globl offmemcmp_avx2
.type offmemcmp_avx2, @function
offmemcmp_avx2:
	mov $-1, %rax
	mov $0xFFFFFFFF, %r11

	# Check if we can align
	mov %rsi,  %r9
	mov %rdi,  %r10
	and $0x1F, %r9
	and $0x1F, %r10
	cmp %r9, %r10
	je  .align_32byte

	xor %r9,  %r9
	jmp .cond_loop_32byte # Not 'alignable' =(

# ------- Align both pointers before SIMD -------
.align_32byte:
	mov $0x1F, %r10
	sub %r9 ,  %r10  # Iteration counter
	cmp $0x1F, %r10
	je .cond_loop_32byte # If already aligned

	xor %r9, %r9
	cmp $8,  %rcx # Size should greater or equal 8-byte
	jl  .cond_loop_align_32byte_1byte # Otherwise, 1-byte comparisons
	jmp .cond_loop_align_32byte_8byte # should be made

# ------- Align 32-byte using 8-byte pointers
.loop_align_32byte_8byte:
	eight_byte_comparison
	sub $8, %rcx
	sub $8, %r10

.cond_loop_align_32byte_8byte:
	cmp $8, %rcx
	jl  .cond_loop_align_32byte_1byte
	cmp $8, %r10
	jge .loop_align_32byte_8byte

	# Maybe we need one-byte alignment?
	jmp .cond_loop_align_32byte_1byte

# ------- Align 32-byte using 1-byte pointers
.loop_align_32byte_1byte:
	one_byte_comparison
	sub $1, %rcx
	sub $1, %r10

.cond_loop_align_32byte_1byte:
	cmp $0, %rcx
	je  .out
	cmp $0, %r10
	jge .loop_align_32byte_1byte

	# Addresses are aligned
	jmp .cond_loop_32byte

# ------- SIMD 2x32-byte loop -------
.loop_32byte:
	VMOVDQx  0(%rsi,%r9,1), %ymm1
	avx2_comparison ymm2

	VMOVDQx  0(%rsi,%r9,1), %ymm1
	avx2_comparison ymm2

	sub  $64, %rcx
.cond_loop_32byte:
	cmp  $64, %rcx
	jge .loop_32byte
	jmp .cond_loop_8byte

# ------- 8-byte remaining bytes loop -------
.loop_8byte:
	eight_byte_comparison
	sub $8, %rcx

.cond_loop_8byte:
	cmp $8, %rcx
	jge .loop_8byte
	jmp .cond_loop_1byte

# ------- 1-byte remaining bytes loop -------
.loop_1byte:
	one_byte_comparison
	sub $1, %rcx

.cond_loop_1byte:
	cmp $0, %rcx
	jne .loop_1byte
	jmp .out

# ------- Return values -------
.found_32byte:
	not   %r8
	tzcnt %r8,  %rax
	sub   $32,  %r9
	sub   $1,   %rdx
	not   %rdx
	and   %rdx, %rax
	add   %r9,  %rax
	ret
.found_8byte:
	mov  (%rsi, %r9, 1), %r8
	mov  (%rdi, %r9, 1), %rax
	xor   %r8,  %rax
	tzcnt %rax, %rax # Trailing zeros = a

	mov  %rdx, %r11
	mov  %rdx, %r8
	xor  %rdx, %rdx
	shl  $3,   %r8   # block_size << 3 = b

	div  %r8         # Trailing zeros / (block_size << 3) = c
	mul  %r11        # c * block_size = d
	add  %r9, %rax   # off = d + a
	ret
.found_1byte:
	mov   %r9,  %rax
	sub   $1, %rdx
	not   %rdx
	and   %rdx, %rax
.out:
	ret
