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

#include "breakpoint.h"
#include "util.h"
#include "dwarf_helper.h"
#include "ptrace.h"

#define BS 64

static char before[BS];
static char after[BS];

/**
 *
 */
char *var_format_value(char *buffer, union var_value *v, int encoding, size_t byte_size)
{
	switch (encoding)
	{
		case DW_ATE_signed:
		case DW_ATE_signed_char:
			switch(byte_size)
			{
				case 1:
					snprintf(buffer, BS, "%" PRId8,  (int8_t)  v->u64_value);
					break;
				case 2:
					snprintf(buffer, BS, "%" PRId16, (int16_t) v->u64_value);
					break;
				case 4:
					snprintf(buffer, BS, "%" PRId32, (int32_t) v->u64_value);
					break;
				case 8:
					snprintf(buffer, BS, "%" PRId64, (int64_t) v->u64_value);
					break;
			}
			break;

		case DW_ATE_unsigned:
		case DW_ATE_unsigned_char:
			switch(byte_size)
			{
				case 1:
					snprintf(buffer, BS, "%" PRIu8,  (uint8_t)  v->u64_value);
					break;
				case 2:
					snprintf(buffer, BS, "%" PRIu16, (uint16_t) v->u64_value);
					break;
				case 4:
					snprintf(buffer, BS, "%" PRIu32, (uint32_t) v->u64_value);
					break;
				case 8:
					snprintf(buffer, BS, "%" PRIu64, (uint64_t) v->u64_value);
					break;
			}
			break;

		case DW_ATE_float:
			snprintf(buffer, BS, "%f", (byte_size == sizeof(double)) ?
				v->d_value : v->f_value);
			break;

		default:
			return (NULL);
	}

	return (buffer);
}

/**
 *
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
	if (v->type.var_type == TBASE_TYPE)
	{
		/* Global or Static. */
		if (v->scope == VGLOBAL)
		{
			if (v->byte_size <= sizeof(long))
				value->u64_value = pt_readmemory_long(child, v->location.address);
			else
				return (-1);
		}

		/* Local variables. */
		else
		{
			if (v->byte_size <= sizeof(long))
			{
				base_pointer = pt_readregister_bp(child);
				location = base_pointer + v->location.fp_offset;
				value->u64_value = pt_readmemory_long(child, location);
			}
			else
				return (-1);
		}
	}

	/* TODO: Implement the other cases here. */
	else
		return (-1);

	return (0);
}

/**
 *
 */
void var_initialize(struct array *vars, pid_t child)
{
	for (int i = 0; i < (int) array_size(&vars); i++)
	{
		struct dw_variable *v;
		v = array_get(&vars, i, NULL);

		if (v->type.var_type == TBASE_TYPE)
		{
			if (var_read(&v->value, v, child))
				quit(-1, "var_initialize: wrong size type!, var name: %s\n", v->name);
		}
	}
}

/**
 *
 */
void var_check_changes(struct breakpoint *b, struct array *vars, pid_t child)
{
	union var_value value;

	for (int i = 0; i < (int) array_size(&vars); i++)
	{
		struct dw_variable *v;
		v = array_get(&vars, i, NULL);

		if (v->type.var_type == TBASE_TYPE)
		{
			var_read(&value, v, child);

			if (memcmp(&value.u64_value, &v->value.u64_value, v->byte_size))
			{
				printf("[Line: %d] [%s] [%s] has changed!, before: %s, after: %s\n",
					b->line_no,
					(v->scope == VGLOBAL) ? "global" : "local",
					v->name,
					var_format_value(before, &v->value, v->type.encoding, v->byte_size),
					var_format_value(after,  &value, v->type.encoding, v->byte_size)
				);
				
				v->value.u64_value = value.u64_value;
			}
		}
	}
}
