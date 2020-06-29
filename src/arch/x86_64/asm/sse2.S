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
	#define MOVDQx movdqa
#else
	#define MOVDQx movdqu
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

.macro sse2_comparison reg2
	add $16, %r9

	MOVDQx -16(%rdi,%r9,1), %\reg2
	pcmpeqb %xmm0, %\reg2
	pmovmskb %\reg2, %r8
	cmp $0xFFFF, %r8
	jne .found_16byte
.endm

/*
 * Memory Comparison (with byte offset) routine (SSE2 Version)
 *
 * int64_t offmemcmp_sse2 (void *src, void *dest, size_t block_size,
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
 * that this routine uses SIMD registers, (SSE2/128-bit registers)
 * in order to speedup the comparison.
 *
 * Although pretty simple, this function tries to align both pointers,
 * whether 8-byte per iteration or 1-byte. When aligned (or not),
 * the remaining bytes are compared by a 32-byte loop with 2x16-bytes
 * comparisons. If there are remaining bytes, 8-bytes/1-byte comparisons
 * are made.
 *
 * Performance
 * -----------
 *
 * This implementation is ~ 41% faster than the original C implementation
 * of offmemcmp with -O3. However, glibc's memcmp is ~ 94% faster than the
 * C code, so there is a lot room for improvements here.
 *
 * Requirements
 * ------------
 *
 * Requires a SSE2-compatible processor, i.e: Pentium 4 or later.
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
.globl offmemcmp_sse2
.type offmemcmp_sse2, @function
offmemcmp_sse2:
	mov $-1, %rax

	# Check if we can align
	mov %rsi, %r9
	mov %rdi, %r10
	and $0xF, %r9
	and $0xF, %r10
	cmp %r9, %r10
	je  .align_16byte

	xor %r9,  %r9
	jmp .cond_loop_16byte # Not 'alignable' =(

# ------- Align both pointers before SIMD -------
.align_16byte:
	mov $0xF, %r10
	sub %r9 , %r10  # Iteration counter
	cmp $0xF, %r10
	je .cond_loop_16byte # If already aligned

	xor %r9, %r9
	cmp $8,  %rcx # Size should greater or equal 8-byte
	jl  .cond_loop_align_16byte_1byte # Otherwise, 1-byte comparisons
	jmp .cond_loop_align_16byte_8byte # should be made

# ------- Align 16-byte using 8-byte pointers
.loop_align_16byte_8byte:
	eight_byte_comparison
	sub $8, %rcx
	sub $8, %r10

.cond_loop_align_16byte_8byte:
	cmp $8, %rcx
	jl  .cond_loop_align_16byte_1byte
	cmp $8, %r10
	jge .loop_align_16byte_8byte

	# Maybe we need one-byte alignment?
	jmp .cond_loop_align_16byte_1byte

# ------- Align 16-byte using 1-byte pointers
.loop_align_16byte_1byte:
	one_byte_comparison
	sub $1, %rcx
	sub $1, %r10

.cond_loop_align_16byte_1byte:
	cmp $0, %rcx
	je  .out
	cmp $0, %r10
	jge .loop_align_16byte_1byte

	# Addresses are aligned
	jmp .cond_loop_16byte

# ------- SIMD 2x16-byte loop -------
.loop_16byte:
	MOVDQx  0(%rsi,%r9,1), %xmm0
	sse2_comparison xmm1

	MOVDQx  0(%rsi,%r9,1), %xmm0
	sse2_comparison xmm1

	sub  $32, %rcx
.cond_loop_16byte:
	cmp  $32, %rcx
	jge .loop_16byte
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
.found_16byte:
	not   %r8
	tzcnt %r8,  %rax
	sub   $16,  %r9
	sub   $1, %rdx
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
