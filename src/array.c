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

#include "array.h"

/**
 * Initializes the array.
 *
 * @param array Array structure pointer to be
 * initialized.
 *
 * @return Returns 0 if success and a negative number
 * otherwise.
 */
int array_init(struct array **array)
{
	struct array *out;
	out = calloc(1, sizeof(struct array));
	if (out == NULL)
		return (-1);
	
	out->capacity = ARRAY_UTILS_DEFAULT_SIZE;
	out->elements = 0;
	out->buf = calloc(out->capacity, sizeof(void *));
	if (out->buf == NULL)
		return (-1);
	
	*array = out;
	return (0);
}

/**
 * Deallocates the array.
 *
 * @return Returns 0 if success.
 *
 * @note The elements present are not freed and its
 * up to the user to free them.
 */
int array_finish(struct array **array)
{
	/* Invalid array. */
	if (*array == NULL)
		return (-1);
		
	free((*array)->buf);
	free(*array);
	return (0);
}

/**
 * Adds a void pointer into the array.
 *
 * @param ar Array structure.
 * @param e Element to be added.
 *
 * @return Returns 0 if success and a negative
 * number otherwise.
 */
int array_add(struct array **ar, void *e)
{
	struct array *array;  /* Array.        */
	void **new_buf;       /* New buffer.   */
	size_t old_capacity;  /* Old capacity. */
	size_t new_capacity;  /* New capacity. */
	
	array = *ar;

	/* If bigger than capacity. */
	if (array->elements >= array->capacity)
	{
		old_capacity = array->capacity;
		new_capacity = old_capacity * 2;

		/* New memory. */
		new_buf = realloc(array->buf, new_capacity * sizeof(void *));
		if (new_buf)
			array->buf = new_buf;
		else
			return (-1);

		array->capacity = new_capacity;
	}
	
	/* Stores the element. */
	array->buf[array->elements] = e;
	array->elements++;
	return (0);
}

/**
 * Removes an element from a given position.
 *
 * @param ar Array structure pointer.
 * @param pos Position to be removed.
 * @param e Element to be returned.
 *
 * @return Returns the removed element or NULL if
 * invalid position.
 */
void* array_remove(struct array **ar, size_t pos, void **e)
{
	struct array *array; /* Array.            */
	void *ret;           /* Returned element. */
	
	array = *ar;

	/* Invalid position. */
	if (pos >= array->elements)
		return (NULL);

	ret = array->buf[pos];
	if (e != NULL)
		*e = ret;

	/* If middle. */
	if (pos != array->elements - 1)
	{
		memmove(&(array->buf[pos]), &(array->buf[pos + 1]),
			(array->elements - 1 - pos) * sizeof(void *));
	}

	array->elements--;
	return (ret);
}

/**
 * Gets the element from the given position.
 *
 * @param array Array structure pointer.
 * @param pos Position desired.
 * @param e Element to be returned.
 *
 * @return Returns the element from the given position
 * and NULL if invalid position.
 */
void* array_get(struct array **array, size_t pos, void **e)
{
	/* Valid positions. */
	if (pos > (*array)->elements)
		return (NULL);
	
	/* Valid pointer. */
	if (e != NULL)
		*e = (*array)->buf[pos];
	
	return ( (*array)->buf[pos] );
}

/**
 * Gets the number of elements currently present in
 * the array.
 *
 * @param array Array structure pointer.
 *
 * @return Returns the number of elements
 * in the array.
 */
size_t array_size(struct array **array)
{
	return ( (*array)->elements );
}

/*============================================================================*
 * Tests                                                                      *
 *============================================================================*/

/**
 * @brief Tries to add @p size elements into the array.
 * @param array Target array.
 * @param size Elements amount.
 * @return Returns 0 if success and a negative number otherwise.
 */
int array_addtest(struct array *array, int size)
{
	/* Initialize with sequencial data. */
	for (int i = 0; i < size; i++)
	{
		int *num = malloc(sizeof(int));
		if (num != NULL)
		{
			*num = i;
			if (array_add(&array, num))
			{
				fprintf(stderr, "array_utils: error while adding: %d\n", *num);
				fprintf(stderr, "             at pos: %zu / capacity: %zu\n",
					array->elements, array->capacity);

				return (-1);
			}
		}
		else
		{
			fprintf(stderr, "array_utils: error while malloc'ing\n");
			return (-1);
		}
	}

	return (0);
}

/**
 * @brief Checks if the previously added elements are correct.
 * @param array Target array.
 * @param size Elements amount.
 * @return Returns 0 if success and a negative number otherwise.
 */
int array_integritytest(struct array *array, int size)
{
	/* Checks if all elements was insert correctly. */
	for (int i = 0; i < size; i++)
	{
		int *el;
		if ( array_get(&array, i, (void *)&el) != NULL )
		{
			if (*el != i)
			{
				fprintf(stderr, "array_utils: element (%d) at pos %d is "
					"different from", *el, i);
				fprintf(stderr, "             expected (%d)\n", i);
				return (-1);
			}
		}
		else
		{
			fprintf(stderr, "array_utils: element does not exist at given"
				" pos: %d\n", i);
			return (-1);
		}
	}

	return (0);
}

/**
 * @brief Tries to remove the first half of the array.
 * @param array Target array.
 * @param size Elements amount.
 * @return Returns 0 if success and a negative number otherwise.
 */
int array_removehalf(struct array *array, int size)
{
	/* Removes the first half. */
	for (int i = 0; i < size/2; i++)
	{
		int *el = (int *) array_remove(&array, 0, NULL);
		
		if (el != NULL)
			free(el);
		else
		{
			fprintf(stderr, "array_utils: was not possible to remove at"
				" position %d\n", i);
		}
	}

	return (0);
}

/**
 * @brief Checks if the first half remaining is valid.
 * @param array Target array.
 * @param size Elements amount.
 * @return Returns 0 if success and a negative number otherwise.
 */
int array_checkhalf(struct array *array, int size)
{
	/* Checks if the remaining is correct. */
	int start_element = size/2;
	for (int i = 0; i < size/2; i++)
	{
		int *el;
		if (array_get(&array, i, (void *)&el) != NULL )
		{
			if (*el != start_element + i)
			{
				fprintf(stderr, "array_utils: element (%d) at pos %d"
					" is different from", *el, i);
				fprintf(stderr, "             expected (%d)\n", start_element + i);
				return (-1);
			}
		}
		else
		{
			fprintf(stderr, "array_utils: element does not exist at"
				" given pos: %d\n", i);
			return (-1);
		}
	}

	return (0);
}

/**
 * @brief Tries to remove the remaining half of the array.
 * @param array Target array.
 * @param size Elements amount.
 * @return Returns 0 if success and a negative number otherwise.
 */
int array_remove_rem_half(struct array *array, int size)
{
	/* Remove the remaining half. */
	for (int i = 0; i < size/2; i++)
	{
		int *el = (int *) array_remove(&array, 0, NULL);
		
		if (el != NULL)
			free(el);
		else
		{
			fprintf(stderr, "array_utils: was not possible to remove at"
				" position %d\n", i);
		}
	}

	return (0);
}

/**
 * Runs a series of checks in the available array functions
 * in order to test all the funcionalities.
 *
 * Current tests:
 * - Heavy insertions
 * - Check for consistent values
 * - Remove values
 *   - Check for consistent values
 *
 * @return Returns 0 if success and a negative number
 * otherwise.
 */
int array_selftest(void)
{
	#define ARRAY_SIZE 32000
	struct array *array;
	
	/* Initializes array. */
	array_init(&array);

	/* Tests. */
	printf("Initialize array [%s]\n",
		!array_addtest(array, ARRAY_SIZE) ? "PASSED" : "FAILED");
	printf("Checking elements [%s]\n",
		!array_integritytest(array, ARRAY_SIZE) ? "PASSED" : "FAILED");
	printf("Remove first half [%s]\n",
		!array_removehalf(array, ARRAY_SIZE) ? "PASSED" : "FAILED");
	printf("First half integrity [%s]\n",
		!array_checkhalf(array, ARRAY_SIZE) ? "PASSED" : "FAILED");
	printf("Remove half remaining [%s]\n",
		!array_remove_rem_half(array, ARRAY_SIZE) ? "PASSED" : "FAILED");

	/* Free array. */
	array_finish(&array);

	/* Awesome =). */
	return (0);
}

#if 0
/**
 * @brief Execute tests
 */
int main()
{
	array_selftest();
	return (0);
}
#endif
