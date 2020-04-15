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

#include <ctype.h>
#include "breakpoint.h"
#include "util.h"
#include "dwarf_helper.h"
#include "ptrace.h"
#include "function.h"
#include "line.h"

/* Offset memcmp pointer. */
int64_t (*offmemcmp)(
	void *src, void *dest, size_t block_size, size_t length);

/**
 * @brief Dump all variables found in the target function.
 *
 * @param vars Variables array.
 */
void var_dump(struct array *vars)
{
	for (int i = 0; i < (int) array_size(&vars); i++)
	{
		struct dw_variable *v;
		v = array_get(&vars, i, NULL);

		printf("    Variable found: %s\n", v->name);
		printf("        scope: %d\n", v->scope);

		/* Location. */
		if (v->scope == VLOCAL)
			printf("        location: %d\n", (int)v->location.fp_offset);
		else
			printf("        location: %" PRIx64 "\n", v->location.address);

		printf("        size (bytes): %zu\n", v->byte_size);
		printf("        var type:     %d\n", v->type.var_type);
		printf("        var encoding: %d\n", v->type.encoding);

		/* Check if array. */
		if (v->type.array.dimensions > 0)
		{
			printf("        array (%d dimensions) (size per element: %zu) (type: %d): \n",
				v->type.array.dimensions, v->type.array.size_per_element,
				v->type.array.var_type);

			printf("            ");
			for (int i = 0; i < v->type.array.dimensions; i++)
				printf("[%d], ", v->type.array.elements_per_dimension[i]);

			printf("\n");
		}

		printf("\n");
	}
}

/**
 * @brief For a given value, buffer, encoding and size, prepares a
 * formatted string with its content. It's important to note, that
 * this function only formats TBASE_TYPE, TENUM and partially
 * TPOINTERSs.
 *
 * @param buffer Destination buffer, that will holds the formatted
 * string.
 *
 * @param v Variable value.
 * @param encoding Variable encoding (signed, unsigned...).
 * @param byte_size Variable size, in bytes.
 *
 * @return If success, returns the buffer containing the formatted
 * string, otherwise, returns NULL.
 */
char *var_format_value(char *buffer, union var_value *v, int encoding, size_t byte_size)
{
	/* Check encoding. */
	switch (encoding)
	{
		/* Signed values. */
		case ENC_SIGNED:
			switch (byte_size)
			{
				case 1:
					/* Special case if printable character =). */
					if ( isprint( ((int8_t)v->u64_value[0]) ) )
						snprintf(buffer, BS, "%" PRId8 " (%c)", (int8_t) v->u64_value[0],
							(int8_t) v->u64_value[0]);
					else
						snprintf(buffer, BS, "%" PRId8, (int8_t)  v->u64_value[0]);
					break;
				case 2:
					snprintf(buffer, BS, "%" PRId16, (int16_t) v->u64_value[0]);
					break;
				case 4:
					snprintf(buffer, BS, "%" PRId32, (int32_t) v->u64_value[0]);
					break;
				case 8:
					snprintf(buffer, BS, "%" PRId64, (int64_t) v->u64_value[0]);
					break;
			}
			break;

		/* Unsigned values. */
		case ENC_UNSIGNED:
			switch (byte_size)
			{
				case 1:
					/* Special case if printable character =). */
					if ( isprint( ((uint8_t)v->u64_value[0]) ) )
						snprintf(buffer, BS, "%" PRIu8 " (%c)", (uint8_t) v->u64_value[0],
							(uint8_t) v->u64_value[0]);
					else
						snprintf(buffer, BS, "%" PRIu8, (uint8_t)  v->u64_value[0]);
					break;
				case 2:
					snprintf(buffer, BS, "%" PRIu16, (uint16_t) v->u64_value[0]);
					break;
				case 4:
					snprintf(buffer, BS, "%" PRIu32, (uint32_t) v->u64_value[0]);
					break;
				case 8:
					snprintf(buffer, BS, "%" PRIu64, (uint64_t) v->u64_value[0]);
					break;
			}
			break;

		/* Floating point values, that includes float and double. */
		case ENC_FLOAT:
			switch (byte_size)
			{
				case 4:
					snprintf(buffer, BS, "%f", v->f_value);
					break;
				case 8:
					snprintf(buffer, BS, "%f", v->d_value);
					break;
				case 16:
					snprintf(buffer, BS, "%Lf", v->ld_value);
					break;
			}
			break;

		/* Pointers. */
		case ENC_POINTER:
			switch (byte_size)
			{
				case 4:
					snprintf(buffer, BS, "0x%" PRIX32, (uint32_t) v->u64_value[0]);
					break;
				case 8:
					snprintf(buffer, BS, "0x%" PRIX64, (uint64_t) v->u64_value[0]);
					break;
			}
			break;

		default:
			return (NULL);
	}

	return (buffer);
}

/**
 * @brief Given two memory blocks (arrays), compares each position
 * and returns the offset that differs.
 *
 * @param v1 First memory block to be compared.
 * @param v2 Second memory block to be compared.
 * @param block_size Size per element.
 * @param n Memory block size.
 *
 * @return Returns the byte offset of the change (0 inclusive), or -1 if
 * there is no change.
 *
 * @note Its *very* important to note that this function is endianness
 * sensitive and is unlikely to work on big-endian targets. Since PBD
 * only supports x86_64 (at the moment), this should not be a problem.
 */
int64_t offmemcmp_generic(void *v1, void *v2,
	size_t block_size, size_t n)
{
	const uint64_t *u64p1; /* 8-byte pointer, first memory block.  */
	const uint64_t *u64p2; /* 8-byte pointer, second memory block. */
	const uint8_t  *u8p1;  /* 1-byte pointer, first memory block.  */
	const uint8_t  *u8p2;  /* 1-byte pointer, second memory block. */
	size_t size;           /* Memory block size.                   */
	int trailing_zeros;    /* Trailing zeros of a 64-bit word.     */

	u64p1 = v1;
	u64p2 = v2;
	size  = n;

	/* Multiples of word. */
	while (n >= 8)
	{
		if (*u64p1 != *u64p2)
		{
			trailing_zeros = __builtin_ctzll(*u64p1 ^ *u64p2);
			return ( (trailing_zeros / (block_size << 3)) * block_size ) + (size - n);
		}

		u64p1++;
		u64p2++;
		n -= 8;
	}

	u8p1 = (const uint8_t *) u64p1;
	u8p2 = (const uint8_t *) u64p2;

	/* Remaining bytes. */
	while (n)
	{
		if (*u8p1++ != *u8p2++)
			return (size - n);
		n--;
	}

	return (-1);
}

/**
 * @brief Allocates a new function context.
 *
 * Allocates a new function context by carefully duplicating
 * and filling the variable structure.
 *
 * @param prev_ctx Previous context.
 * @param curr_ctx Current context pointer.
 * @param ctx_list Context list.
 *
 * @return Returns 0 if success.
 */
int var_new_context(
	struct function *prev_ctx,
	struct function **curr_ctx,
	struct array *ctx_list)
{
	struct dw_variable *prev_v; /* Previous variable. */
	struct dw_variable *v;      /* Current variable.  */
	int size;                   /* Variable size.     */

	size = (int) array_size(&prev_ctx->vars);

	/* Allocates a new function context. */
	*curr_ctx = calloc(1, sizeof(struct function));
	array_init(&((*curr_ctx)->vars));

	/*
	 * Copies all the old variables into the new context.
	 */
	for (int i = 0; i < size; i++)
	{
		prev_v = array_get(&prev_ctx->vars, i, NULL);
		v = calloc(1, sizeof(struct dw_variable));

		/* Do a quick n' dirty shallow copy. */
		memcpy(v, prev_v, sizeof(struct dw_variable));

		/* Change the remaining important items:
		 * - Name
		 * - Value
		 */
		v->name = malloc(sizeof(char) * (strlen(prev_v->name) + 1));
		strcpy(v->name, prev_v->name);

		v->value.u64_value[0] = 0;
		v->value.u64_value[1] = 0;

		/* Add into array. */
		array_add(&((*curr_ctx)->vars), v);
	}

	/*
	 * Adds the new context into the context list.
	 *
	 * Note: Its important to note that the new context _is_ also
	 * the current context from now on.
	 */
	array_add(&ctx_list, *curr_ctx);

	return (0);
}

/**
 * @brief By a given variable, context and depth, deallocates
 * all the variables and arrays for that context.
 *
 * @param vars Current variables list.
 * @param context Context list.
 * @param depth Current depth.
 *
 * @return Returns 0 if success and a negative number otherwise.
 */
int var_deallocate_context(
	struct array *vars,
	struct array *context,
	int depth)
{
	/**
	 * Deallocates each array and if depth is greater than 1,
	 * also deallocates the var and its context.
	 */
	for (int i = 0; i < (int) array_size(&vars); i++)
	{
		struct dw_variable *v;
		v = array_get(&vars, i, NULL);

		if (v->type.var_type == TARRAY)
		{
			free(v->value.p_value);
			v->value.p_value = NULL;
		}

		if (depth > 1)
		{
			free(v->name);
			free(v);
		}
	}

	/* Deallocates array and remove the last level. */
	if (depth > 1)
	{
		array_finish(&vars);
		free( array_remove_last(&context, NULL) );
	}

	return (0);
}

/**
 * @brief Reads the current variable value for a given variable.
 *
 * @param value Value union.
 * @param v Variable to be read.
 * @param child Child process.
 *
 * @return Returns 0 if success and a negative number otherwise.
 */
int var_read(union var_value *value, struct dw_variable *v, pid_t child)
{
	uint64_t base_pointer; /* Base Pointer Value.            */
	uint64_t location;     /* Base Pointer Relative Address. */

	/*
	 * Very important note:
	 * Since I'm assuming that this code will run (at least initially)
	 * in x86_64 archs, sizeof(long) must be 64 bits.
	 */
	COMPILE_TIME_ASSERT(sizeof(long) == 8);

	/*
	 * If a primitive type, read a u64 should be
	 * enough, otherwise, read an arbitrary amount
	 * of bytes.
	 */
	if (v->type.var_type & (TBASE_TYPE|TENUM|TPOINTER))
	{
		/* Global or Static. */
		if (v->scope == VGLOBAL)
		{
			if (v->byte_size <= 8)
				value->u64_value[0] = pt_readmemory64(child, v->location.address);
			else
			{
				/*
				 * Long double types?.
				 */
				if (v->byte_size == 16)
				{
					value->u64_value[0] = pt_readmemory64(child, v->location.address);
					value->u64_value[1] = pt_readmemory64(child, v->location.address + 8);
				}
				else
					return (-1);
			}
		}

		/* Local variables. */
		else
		{
			base_pointer = pt_readregister_bp(child);
			location = base_pointer + v->location.fp_offset;

			if (v->byte_size <= 8)
				value->u64_value[0] = pt_readmemory64(child, location);
			else
			{
				/*
				 * Long double types?.
				 */
				if (v->byte_size == 16)
				{
					value->u64_value[0] = pt_readmemory64(child, location);
					value->u64_value[1] = pt_readmemory64(child, location + 8);
				}
				else
					return (-1);
			}
		}
	}

	/* Arrays. */
	else if (v->type.var_type == TARRAY)
	{
		/*
		 * At the moment, PBD only cares about base types, so we need
		 * to check first.
		 *
		 * TODO: Implement other types.
		 */
		if (v->type.array.var_type & (TBASE_TYPE|TENUM|TPOINTER))
		{
			/* Global or Static. */
			if (v->scope == VGLOBAL)
				value->p_value = pt_readmemory(child, v->location.address, v->byte_size);

			/* Local. */
			else
			{
				base_pointer = pt_readregister_bp(child);
				location = base_pointer + v->location.fp_offset;
				value->p_value = pt_readmemory(child, location, v->byte_size);
			}
		}

		else
			return (-1);
	}

	/* TODO: Implement the other cases here. */
	else
		return (-1);

	return (0);
}

/**
 * @brief Initializes all the variables for the current
 * context by reading its initial values in the stack.
 *
 * @param vars Variables list, for current context.
 * @param child Child process.
 *
 * @TODO: Implement the initialization for other types,
 * other than TBASE_TYPE, TENUM and TPOINTER.
 */
void var_initialize(struct array *vars, pid_t child)
{
	/*
	 * For each variable, check its type and read it
	 * from the stack.
	 */
	for (int i = 0; i < (int) array_size(&vars); i++)
	{
		struct dw_variable *v;
		v = array_get(&vars, i, NULL);

		/* Base types. */
		if (v->type.var_type & (TBASE_TYPE|TENUM|TPOINTER))
		{
			/*
			 * While initializing the variables, we do not know in advance
			 * when the variable will first be initialized, which means that
			 * before this happens, the stack/variable will maintain garbagge
			 * values, that do not reflect reality.
			 *
			 * To work around this, 'scratch_value' will keep the first value
			 * right after the prologue and, as long as the variable is not
			 * initialized, all comparisons will be against scratch_value
			 * instead of value (see var_check_changes()) and if and only if a
			 * difference is detected, PBD will assume that the local variable
			 * has been initialized and thus properly print an appropriate
			 * 'before' value (i.e: 0 or 0.0 for floating-point), instead of
			 * garbagge value.
			 *
			 * This assures us that the output will be consistent regardless
			 * of the stack organization before the value is set correctly, ;-).
			 */
			if (v->scope != VGLOBAL)
			{
				if (var_read(&v->scratch_value, v, child))
					QUIT(EXIT_FAILURE, "wrong size type!, var name: %s / "
						"var size: %d\n", v->name, v->byte_size);

				v->initialized = 0;
			}

			/* If global, there's no need to initialize them. */
			else
			{
				if (var_read(&v->value, v, child))
					QUIT(EXIT_FAILURE, "wrong size type!, var name: %s / "
						"var size: %d\n", v->name, v->byte_size);

				v->initialized = 1;
			}
		}

		/* Arrays. */
		else if (v->type.var_type == TARRAY)
		{
			/*
			 * At the moment, PBD only cares about base types, so we need
			 * to check first.
			 *
			 * TODO: Implement other types.
			 */
			if (v->type.array.var_type & (TBASE_TYPE|TENUM|TPOINTER))
			{
				/* Initialize. */
				if (var_read(&v->value, v, child))
					QUIT(EXIT_FAILURE, "wrong size type!, var name: %s / "
						"var size: %d\n", v->name, v->byte_size);

				v->initialized = 1;
			}
		}
	}
}

/**
 * @brief Checks if there is a change for all variables
 * in the current context, if so, updates its value and
 * exihibits the changed value.
 *
 * @param b Current breakpoint.
 * @param vars Variables list, for current context.
 * @param child Child process.
 * @param depth Function depth.
 *
 * @TODO: Check variable change for other types, other than
 * TBASE_TYPE, TENUM and TPOINTER.
 */
void var_check_changes(struct breakpoint *b, struct array *vars, pid_t child, int depth)
{
	union var_value value;                                /* Variable value.      */
	int index_per_dimension[MATRIX_MAX_DIMENSIONS] = {0}; /* Index per dimension. */

	/* For each variable. */
	for (int i = 0; i < (int) array_size(&vars); i++)
	{
		struct dw_variable *v;
		v = array_get(&vars, i, NULL);

		/* If base type. */
		if (v->type.var_type & (TBASE_TYPE|TENUM|TPOINTER))
		{
			/* Read and compares its value. */
			var_read(&value, v, child);

			/*
			 * Checks if the variable was initialized, if not compare with its
			 * scratch value, and if differs, the variable was finally initialized.
			 */
			if (!v->initialized)
			{
				if (memcmp(&value.u64_value, &v->scratch_value.u64_value, v->byte_size))
				{
					v->value.u64_value[0] = value.u64_value[0];
					v->value.u64_value[1] = value.u64_value[1];

					if (v->type.encoding != ENC_FLOAT)
					{
						v->scratch_value.u64_value[0] = 0;
						v->scratch_value.u64_value[0] = 0;
					}
					else
						v->scratch_value.ld_value = 0.0;

					/* Output changes using the current printer. */
					line_output(depth, b->line_no, v, &v->scratch_value, &value, NULL);
					v->initialized = 1;
				}
				continue;
			}

			/* If the variable was already initialized, lets check normally. */
			if (memcmp(&value.u64_value, &v->value.u64_value, v->byte_size))
			{
				/* Output changes using the current printer. */
				line_output(depth, b->line_no, v, &v->value, &value, NULL);

				v->value.u64_value[0] = value.u64_value[0];
				v->value.u64_value[1] = value.u64_value[1];
			}
		}

		/* If array. */
		else if (v->type.var_type == TARRAY)
		{
			/*
			 * At the moment, PBD only cares about base types, so we need
			 * to check first.
			 *
			 * TODO: Implement other types.
			 */
			if (v->type.array.var_type & (TBASE_TYPE|TENUM|TPOINTER))
			{
				int changed;             /* Variable status.  */
				char *v1, *cmp1;         /* Variable old.     */
				char *v2, *cmp2;         /* Variable new.     */
				size_t size_per_element; /* Size per element. */
				int64_t byte_offset;     /* Byte offset.      */
				size_t size;

				/* Read and compares its value. */
				var_read(&value, v, child);

				/* Setup pointers and data. */
				v1 = (char *)v->value.p_value;
				v2 = (char *)value.p_value;
				size_per_element = v->type.array.size_per_element;
				size = v->byte_size;
				changed = 0;

				/* Compares each position */
				cmp1 = v1;
				cmp2 = v2;
				while ( ((size_t)(cmp1 - v1)) < v->byte_size
					&& (byte_offset = offmemcmp(cmp1, cmp2, size_per_element, size)) >= 0 )
				{
					union var_value value1;
					union var_value value2;
					changed = 1;

					cmp1 += byte_offset;
					cmp2 += byte_offset;

					/*
					 * Fill the value1 and 2 with the element
					 * read in the iteration.
					 */
					memcpy(value1.u8_value, cmp1, size_per_element);
					memcpy(value2.u8_value, cmp2, size_per_element);

					/* If one dimension. */
					if (v->type.array.dimensions == 1)
					{
						index_per_dimension[0] = (cmp1 - v1) / size_per_element;

						/* Output changes using the current printer. */
						line_output(depth, b->line_no, v, &value1, &value2,
							index_per_dimension);
					}

					/* If multiple dimensions. */
					else
					{
						int div;
						int idx_dim;

						div = (cmp1 - v1) / size_per_element;
						idx_dim = v->type.array.dimensions - 1;

						/* Calculate indexes. */
						for (int j = 0; j < v->type.array.dimensions && div; j++)
						{
							index_per_dimension[idx_dim] =
								div % v->type.array.elements_per_dimension[idx_dim];

							div /= v->type.array.elements_per_dimension[idx_dim];
							idx_dim--;
						}

						/* Output changes using the current printer. */
						line_output(depth, b->line_no, v, &value1, &value2,
							index_per_dimension);
					}

					cmp1 += size_per_element;
					cmp2 += size_per_element;
					size -= byte_offset + size_per_element;
				}

				/*
				 * If there is any change, deallocates the old buffer and
				 * points to the new allocated ;-).
				 *
				 * Important note: Maybe there's some overhead of allocating/
				 * deallocating everytime but at least the memory footprint is
				 * smaller than keeping two buffers.
				 */
				if (changed)
				{
					free(v->value.p_value);
					v->value.p_value = value.p_value;
				}
				else
					free(value.p_value);
			}
		}
	}
}

/**
 * @brief Deallocates all the remaining variables in the
 * last function context.
 *
 * @param vars Variables array.
 */
void var_array_free(struct array *vars)
{
	int size;

	size = (int) array_size(&vars);
	for (int i = 0; i < size; i++)
	{
		struct dw_variable *v;
		v = array_remove(&vars, 0, NULL);

		if (v->type.var_type == TARRAY)
			if (v->value.p_value != NULL)
				free(v->value.p_value);

		free(v->name);
		free(v);
	}
	array_finish(&vars);
}
