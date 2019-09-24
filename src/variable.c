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
#include "function.h"

/*
 * Buffer
 *
 * This buffer holds the value from before and
 * after the value is changed for a single
 * TBASE_TYPE.
 */
#define BS 64
static char before[BS];
static char after[BS];

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

		printf("Variable found: %s\n", v->name);
		printf("  Scope: %d\n", v->scope);

		/* Location. */
		if (v->scope == VLOCAL)
			printf("  Location: %d\n", (int)v->location.fp_offset);
		else
			printf("  Location: %" PRIx64 "\n", v->location.address);

		printf("  Size (bytes): %zu\n", v->byte_size);
		printf("  Var type:     %d\n", v->type.var_type);
		printf("  Var encoding: %d\n", v->type.encoding);

		/* Check if array. */
		if (v->type.array.dimensions > 0)
		{
			printf("  Array (%d dimensions) (size per element: %zu) (type: %d): \n",
				v->type.array.dimensions, v->type.array.size_per_element,
				v->type.array.var_type);

			printf("    ");
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
 * this function only formats TBASE_TYPEs.
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
		case DW_ATE_signed:
		case DW_ATE_signed_char:
			switch (byte_size)
			{
				case 1:
					snprintf(buffer, BS, "%" PRId8,  (int8_t)  v->u64_value[0]);
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
		case DW_ATE_unsigned:
		case DW_ATE_unsigned_char:
			switch (byte_size)
			{
				case 1:
					snprintf(buffer, BS, "%" PRIu8,  (uint8_t)  v->u64_value[0]);
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
		case DW_ATE_float:
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

		default:
			return (NULL);
	}

	return (buffer);
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
	if (v->type.var_type == TBASE_TYPE)
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
		if (v->type.array.var_type == TBASE_TYPE)
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
 * other than TBASE_TYPE.
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
		if (v->type.var_type == TBASE_TYPE)
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
					quit(-1, "var_initialize: wrong size type!, var name: %s / "
						"var size: %d\n", v->name, v->byte_size);

				v->initialized = 0;
			}

			/* If global, there's no need to initialize them. */
			else
			{
				if (var_read(&v->value, v, child))
					quit(-1, "var_initialize: wrong size type!, var name: %s / "
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
			if (v->type.array.var_type == TBASE_TYPE)
			{
				/* Initialize. */
				if (var_read(&v->value, v, child))
					quit(-1, "var_initialize: wrong size type!, var name: %s / "
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
 * TBASE_TYPE.
 */
void var_check_changes(struct breakpoint *b, struct array *vars, pid_t child, int depth)
{
	union var_value value;
	char *indent_buff;

	/* For each variable. */
	for (int i = 0; i < (int) array_size(&vars); i++)
	{
		struct dw_variable *v;
		v = array_get(&vars, i, NULL);

		/* If base type. */
		if (v->type.var_type == TBASE_TYPE)
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
					v->initialized = 1;

					if (v->type.encoding != DW_ATE_float)
					{
						v->scratch_value.u64_value[0] = 0;
						v->scratch_value.u64_value[0] = 0;
					}
					else
						v->scratch_value.ld_value = 0.0;

					printf("%s[Line: %d] [%s] (%s) initialized!, before: %s, after: %s\n",
						(indent_buff = fn_get_indent(depth)),
						b->line_no,
						(v->scope == VGLOBAL) ? "global" : "local",
						v->name,
						var_format_value(before, &v->scratch_value, v->type.encoding, v->byte_size),
						var_format_value(after,  &value, v->type.encoding, v->byte_size)
					);

					fn_free_indent(indent_buff);
				}
				continue;
			}

			/* If the variable was already initialized, lets check normally. */
			if (memcmp(&value.u64_value, &v->value.u64_value, v->byte_size))
			{
				printf("%s[Line: %d] [%s] (%s) has changed!, before: %s, after: %s\n",
					(indent_buff = fn_get_indent(depth)),
					b->line_no,
					(v->scope == VGLOBAL) ? "global" : "local",
					v->name,
					var_format_value(before, &v->value, v->type.encoding, v->byte_size),
					var_format_value(after,  &value, v->type.encoding, v->byte_size)
				);

				fn_free_indent(indent_buff);

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
			if (v->type.array.var_type == TBASE_TYPE)
			{
				int changed;
				char *v1;
				char *v2;
				size_t size_per_element;

				/* Read and compares its value. */
				var_read(&value, v, child);

				/* Setup pointers and data. */
				v1 = (char *)v->value.p_value;
				v2 = (char *)value.p_value;
				size_per_element = v->type.array.size_per_element;
				changed = 0;

				/* Compares each position */
				for (size_t i = 0; i < v->byte_size; i += size_per_element)
				{
					if (memcmp(v1, v2, size_per_element))
					{
						union var_value value1;
						union var_value value2;
						changed = 1;

						/*
						 * Fill the value1 and 2 with the element
						 * read in the iteration.
						 */
						memcpy(value1.u8_value, v1, size_per_element);
						memcpy(value2.u8_value, v2, size_per_element);

						/* If one dimension. */
						if (v->type.array.dimensions == 1)
						{
							printf("%s[Line: %d] [%s] (%s[%zu]) has changed!, before: %s, after: %s\n",
								(indent_buff = fn_get_indent(depth)),
								b->line_no,
								(v->scope == VGLOBAL) ? "global" : "local",
								v->name,
								i / size_per_element,
								var_format_value(before, &value1, v->type.encoding, size_per_element),
								var_format_value(after,  &value2, v->type.encoding, size_per_element)
							);

							fn_free_indent(indent_buff);
						}

						/* If multiple dimensions. */
						else
						{
							int div;
							int index_per_dimension[MATRIX_MAX_DIMENSIONS] = {0};
							int idx_dim;

							div = i / size_per_element;
							idx_dim = v->type.array.dimensions - 1;

							/* Calculate indexes. */
							for (int j = 0; j < v->type.array.dimensions && div; j++)
							{
								index_per_dimension[idx_dim] =
									div % v->type.array.elements_per_dimension[idx_dim];

								div /= v->type.array.elements_per_dimension[idx_dim];
								idx_dim--;
							}

							/* Print indexes and values. */
							printf("%s[Line: %d] [%s] (%s",
								(indent_buff = fn_get_indent(depth)),
								b->line_no,
								(v->scope == VGLOBAL) ? "global" : "local",
								v->name
							);

							for (int j = 0; j < v->type.array.dimensions; j++)
								printf("[%d]", index_per_dimension[j]);

							printf(") has changed!, before: %s, after: %s\n",
								var_format_value(before, &value1, v->type.encoding, size_per_element),
								var_format_value(after,  &value2, v->type.encoding, size_per_element)
							);

							fn_free_indent(indent_buff);
						}
					}

					v1 += size_per_element;
					v2 += size_per_element;
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
