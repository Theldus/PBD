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

#include "ptrace.h"
#include "util.h"

/**
 * @brief Creates a new process and executes a file
 * pointed to by @p file.
 *
 * @param file File to be executed.
 *
 * @return In case of success, returns the child PID,
 * otherwise, a negative number.
 */
int pt_spawnprocess(const char *file, char **argv)
{
	pid_t child; /* Child Process. */

	child = fork();
	if (child == 0)
	{
		ptrace(PTRACE_TRACEME, 0, NULL, NULL);
		execv(file, (char *const *)argv);
	}

	return (child);
}

/**
 * @brief Executes until the child process has been
 * stopped (breakpoint, signal...).
 *
 * @return Returns PT_CHILD_EXIT if the child process
 * was terminated, otherwise, returns 0.
 */
int pt_waitchild(void)
{
	int status;    /* Status Code. */
	wait(&status);

	if (WIFEXITED(status) || WIFSIGNALED(status))
		return (PT_CHILD_EXIT);

	return (0);
}

/**
 * @brief Continues the child execution.
 *
 * @param child Child to be resumed.
 */
int pt_continue(pid_t child)
{
	ptrace(PTRACE_CONT, child, NULL, NULL);
	return (0);
}

/**
 * @brief Continues the child execution in single
 * step mode.
 *
 * @param child Child to be 'singlestepped'.
 */
int pt_continue_single_step(pid_t child)
{
	ptrace(PTRACE_SINGLESTEP, child, NULL, NULL);
	return (0);
}

/**
 * @brief Reads the current program counter (RIP in x86_64) from
 * the child process.
 *
 * @param child Child process.
 *
 * @return Returns the child program counter.
 */
uint64_t pt_readregister_pc(pid_t child)
{
	return (ptrace(PTRACE_PEEKUSER, child, 8 * RIP, NULL));
}

/**
 * @brief Sets the program counter @p pc for the specified
 * @p child.
 *
 * @param child Child process.
 * @param pc New program counter.
 */
void pt_setregister_pc(pid_t child, uint64_t pc)
{
	struct user_regs_struct regs;
	ptrace(PTRACE_GETREGS, child, NULL, &regs);
	regs.rip = pc;
	ptrace(PTRACE_SETREGS, child, NULL, &regs);
}

/**
 * @brief Reads the current base pointer (RBP in x86_64) from
 * the child process.
 *
 * @param child Child process.
 *
 * @return Returns the child base pointer.
 */
uint64_t pt_readregister_bp(pid_t child)
{
	return (ptrace(PTRACE_PEEKUSER, child, 8 * RBP, NULL));
}

/**
 * @brief Considering the child process is inside
 * the function prologue, retrieves the returning
 * address.
 *
 * @param child Child process.
 *
 * @return Returns the 'return' address.
 */
uint64_t pt_readreturn_address(pid_t child)
{
	uint64_t sp;
	sp = ptrace(PTRACE_PEEKUSER, child, 8 * RSP, NULL);
	return (ptrace(PTRACE_PEEKDATA, child, sp, NULL));
}

/**
 * @brief Reads a uint64_t value (usually 64-bit in x86_64) from
 * a given processs @p child and address @p addr.
 *
 * @param child Child process.
 * @param addr Address to be read.
 *
 * @return Returns a uint64_t containing a value for the specified
 * address.
 */
uint64_t pt_readmemory64(pid_t child, uint64_t addr)
{
	uint64_t data; /* Data returned. */

	/* Expected in 64-bit systems. */
	if (sizeof(long) == 8)
		return (ptrace(PTRACE_PEEKDATA, child, addr, NULL));

	/* Expected in 32-bit systems. */
	if (sizeof(long) == 4)
	{
		data  =  ptrace(PTRACE_PEEKDATA, child, addr, NULL);
		data |= ((uint64_t) ptrace(PTRACE_PEEKDATA, child, addr + 4, NULL)) << 32;
		return (data);
	}
	else
		QUIT(EXIT_FAILURE, "unexpected long size: %zu", sizeof(long));
}

/**
 * @brief Writes a uint64_t value into a given processs @p child
 * and address @p addr.
 *
 * @param child Child process.
 * @param addr Address to be written.
 */
void pt_writememory64(pid_t child, uint64_t addr, uint64_t data)
{
	/* Expected in 64-bit systems. */
	if (sizeof(long) == 8)
		ptrace(PTRACE_POKEDATA, child, addr, data);

	/* Expected in 32-bit systems. */
	else if (sizeof(long) == 4)
	{
		ptrace(PTRACE_POKEDATA, child, addr    , data);
		ptrace(PTRACE_POKEDATA, child, addr + 4, ((data >> 32) & 0xFFFFFFFF));
	}
	else
		QUIT(EXIT_FAILURE, "unexpected long size: %zu", sizeof(long));
}

/**
 * @brief Reads an arbitrary amount of bytes @p len of the given
 * process @p child in the address @p addr.
 *
 * @param child Child process.
 * @param addr Address to be read.
 * @param len How many bytes will be read.

 * @return Returns a pointer containing the the memory read.
 *
 * @note Its up to the caller function to free the returned
 * pointer.
 */
char *pt_readmemory(pid_t child, uint64_t addr, size_t len)
{
	char *data;      /* Return pointer.   */

	/* Allocates enough room for the data to be read. */
	if (posix_memalign((void *)&data, sizeof(void *), sizeof(char) * len) != 0)
		return (NULL);

	/*
	 * Note: process_vm_readv() is only supported by GNU libc with
	 * versions >= 2.15, moreover, this function is Linux-specific
	 * and supported only on kernels >= 3.12 (2012-ish).
	 *
	 * So if we're not using GLIBC or the version is inferior, lets
	 * use the tradional (and way slower) ptrace approach.
	 */
#if defined(__linux__) && defined(__GLIBC__) \
	&& (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 15))

	struct iovec local[1];   /* IO Vector Local.  */
	struct iovec remote[1];  /* IO Vector Remote. */

	/* Prepare arguments for readv. */
	local[0].iov_base  = data;
	local[0].iov_len   = len;
	remote[0].iov_base = (void *) addr;
	remote[0].iov_len  = len;

	if (process_vm_readv(child, local, 1, remote, 1, 0) != (ssize_t)len)
		return (NULL);
	else
		return (data);

/* Ptrace approach. */
#else
	char *laddr;     /* Auxiliar pointer. */
	int i;           /* Address index.    */
	int j;           /* Block counter.    */
	int long_size;   /* Long size.        */

	long_size = sizeof(long);
	union u
	{
		long val;
		char chars[sizeof(long)];
	} temp_data;

	i = 0;
	j = len / long_size;

	/* Assigns memory. */
	laddr = data;

	/* While there are 'blocks' remaining, keep reading. */
	while (i < j)
	{
		temp_data.val = ptrace(PTRACE_PEEKDATA, child, addr + i * sizeof(void *), NULL);
		memcpy(laddr, temp_data.chars, long_size);
		i++;
		laddr += long_size;
	}

	/* If few bytes remaining, read them. */
	j = len % long_size;
	if (j != 0)
	{
		temp_data.val = ptrace(PTRACE_PEEKDATA, child, addr + i * sizeof(void *), NULL);
		memcpy(laddr, temp_data.chars, j);
	}

	return (data);
#endif
}

/**
 * @brief Writes an arbitrary amount of bytes @p len into the given
 * process @p child in the address @p addr.
 *
 * @param child Child process.
 * @param addr Address to be written.
 * @param data Data to be write.
 * @param len How many bytes will be written.
 */
void pt_writememory(pid_t child, uint64_t addr, char *data, size_t len)
{
	int i;         /* Address index.    */
	int j;         /* Block counter.    */
	char *laddr;   /* Auxiliar pointer. */
	int long_size; /* Long size.        */

	long_size = sizeof(long);
	union u
	{
		long val;
		char chars[sizeof(long)];
	} temp_data;

	i = 0;
	j = len / long_size;
	laddr = data;

	/* While there are 'blocks' remaining, keep writing. */
	while (i < j)
	{
		memcpy(temp_data.chars, laddr, long_size);
		ptrace(PTRACE_POKEDATA, child, addr + i * sizeof(char *), temp_data.val);
		i++;
		laddr += long_size;
	}

	/* If few bytes remaining, write them. */
	j = len % long_size;
	if (j != 0)
	{
		memcpy(temp_data.chars, laddr, j);
		ptrace(PTRACE_POKEDATA, child, addr + i * sizeof(char *), temp_data.val);
	}
}
