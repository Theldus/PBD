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

#include "dwarf_helper.h"
#include "hashtable.h"
#include "util.h"
#include "pbd.h"
#include <inttypes.h>
#include <limits.h>

/**
 * Initializes the use of libdwarf library.
 *
 * @param file Elf file to be read.
 * @param Dwarf Utils structure.
 *
 * @return If success, returns 0.
 */
int *dw_init(const char *file, struct dw_utils *dw)
{
	Dwarf_Error error;      /* Error code.      */
	Dwarf_Handler errhand;  /* Error handler.   */
	Dwarf_Ptr errarg;       /* Error argument.  */
	int res;                /* Return code.     */

	/* Clear structure. */
	memset(dw, 0, sizeof(struct dw_utils));

	/* Open file. */
	dw->fd = open(file, O_RDONLY);
	if (dw->fd < 0)
		QUIT(EXIT_FAILURE, "File not found\n");

	/* Initializes dwarf. */
	errhand = NULL;
	errarg  = NULL;
	res = dwarf_init(dw->fd, DW_DLC_READ, errhand, errarg, &dw->dbg, &error);
	if(res != DW_DLV_OK)
		QUIT(EXIT_FAILURE, "Cannot process file\n");

	dw->initialized = 1;
	return (0);
}

/**
 * Finalizes the use of libdwarf library.
 *
 * @param dw Dwarf Utils structure.
 */
void dw_finish(struct dw_utils *dw)
{
	Dwarf_Error error; /* Error code. */

	if (dw == NULL)
		return;

	if (dw->initialized)
		dwarf_finish(dw->dbg, &error);

	if (dw->fd >= 0)
		close(dw->fd);

	/* Invalidate flags to avoid double finishing. */
	dw->initialized = 0;
	dw->fd = -1;
}

/**
 * Reads the next Compile Unit present in the
 * context of dw.
 *
 * @param dw Dwarf Utils structure pointer.
 * @param die Returned CU Die
 *
 * @return Returns 1 if success, a negative number if
 * error and 0 if there is no more CU Header available.
 */
int dw_next_cu_die(struct dw_utils *dw, Dwarf_Die *die)
{
	Dwarf_Error error; /* Error code.  */
	Dwarf_Die no_die;  /* No die.      */
	int res;           /* Return code. */

	Dwarf_Unsigned cu_header_length;
	Dwarf_Half version_stamp;
	Dwarf_Unsigned abbrev_offset;
	Dwarf_Half address_size;
	Dwarf_Unsigned next_cu_header;
	no_die = 0;

	/* Read next Compile Unit Header. */
	res = dwarf_next_cu_header(dw->dbg, &cu_header_length,
		&version_stamp, &abbrev_offset, &address_size,
		&next_cu_header, &error);

	/* Error. */
	if (res == DW_DLV_ERROR)
		return (-1);

	/* No more entries =). */
	if (res == DW_DLV_NO_ENTRY)
		return (0);

	/* Read a CU die from the CU. */
	res = dwarf_siblingof(dw->dbg, no_die, die, &error);
	if(res == DW_DLV_ERROR)
		QUIT(EXIT_FAILURE, "No CU die found!!\n");

	return (1);
}

/**
 * Parses the variable location for the the specified
 * parameter and returns it in the var.
 *
 * @param var_die Variable DIE to be analyzed.
 * @param dw Structure Dwarf_Utils.
 * @param var Pointer to the target variable strutucture.
 */
static int dw_parse_variable_location
(
	Dwarf_Die *var_die,
	struct dw_utils *dw,
	struct dw_variable *var
)
{
	Dwarf_Error error;      /* Error code.              */
	Dwarf_Attribute attr;   /* Attribute.               */
	Dwarf_Locdesc **llbuf;  /* Location descriptor buf. */
	Dwarf_Signed lcnt;      /* Location count.          */
	Dwarf_Loc *lr;          /* Location.                */
	Dwarf_Bool battr;       /* Boolean.                 */
	int retcode;            /* Return code.             */

	retcode = -1;

	/* Location. */
	if (!dwarf_hasattr(*var_die, DW_AT_location, &battr, &error) && battr)
	{
		if (dwarf_attr(*var_die, DW_AT_location, &attr, &error))
			goto err0;
		if (dwarf_loclist_n(attr, &llbuf, &lcnt, &error))
			goto err0;

		/* Only one location supported for now. */
		if (lcnt > 1)
		{
			fprintf(stderr, "dw_parse_variable: location greater than 1\n"
				"make sure you're building with -O0\n");
			goto dealloc;
		}

		if (llbuf[0]->ld_cents > 1)
		{
			fprintf(stderr, "dw_parse_variable: location entries grater than 1\n"
				"make sure you're building with -O0\n");
			goto dealloc;
		}

		/* Finally, location. */
		lr = &llbuf[0]->ld_s[0];

		/* Check if valid operands. */
		if (lr->lr_atom == DW_OP_addr)
		{
			var->scope = VGLOBAL;
			var->location.address = lr->lr_number;
		}
		else if (lr->lr_atom == DW_OP_fbreg)
		{
			var->scope = VLOCAL;
			var->location.fp_offset = lr->lr_number + dw->dw_func.bp_offset;
		}
		else
		{
			fprintf(stderr, "dw_parse_variable: operand not supported!, make sure\n"
				"you're building with -O0\n");
			goto dealloc;
		}

		retcode = 0;
	}
	else
		goto err0;

	/* Deallocates everything. */
dealloc:
	for (int i = 0; i < lcnt; i++)
	{
		dwarf_dealloc(dw->dbg, llbuf[i]->ld_s, DW_DLA_LOC_BLOCK);
		dwarf_dealloc(dw->dbg, llbuf[i], DW_DLA_LOCDESC);
	}
	dwarf_dealloc(dw->dbg, llbuf, DW_DLA_LIST);

err0:
	return (retcode);
}

/**
 * For a given variable DIE, analyzes the DIE until find the
 * base type in it and stores the information found in the
 * pointers: byte_size, var_type, encoding and type_die.
 *
 * @param var_die Target variable DIE.
 * @param dw Dwarf Utils structure pointer.
 * @param byte_size Pointer to the variable size, in bytes.
 * @param var_type Pointer to the variable type.
 * @param encoding Pointer to the variable encoding.
 * @param type_die Pointer to the variable DIE.
 *
 * @note type_die may no be the same as var_die, since var_die
 * can be a typedef, for instance.
 */
static int dw_parse_variable_base_type
(
	Dwarf_Die *var_die,
	struct dw_utils *dw,
	size_t *byte_size,
	int *var_type,
	int *encoding,
	Dwarf_Die *type_die
)
{
	Dwarf_Error error;          /* Error code.   */
	Dwarf_Attribute attr;       /* Attribute.    */
	Dwarf_Off offset;           /* Offset.       */
	Dwarf_Bool battr;           /* Boolean.      */
	Dwarf_Half tag;             /* Tag.          */
	Dwarf_Unsigned dwarf_unsig; /* Byte size.    */

	/* Type. */
	if (!dwarf_hasattr(*var_die, DW_AT_type, &battr, &error) && battr)
	{
		*type_die = *var_die;

		/*
		 * If the type is a typedef, loop until we get the 'base' type.
		 *
		 * - Structures
		 * - Unions
		 * - Enumerations
		 * - Arrays
		 * ...
		 * Will be handled case by case.
		 */
		do
		{
			if (dwarf_attr(*type_die, DW_AT_type, &attr, &error))
				goto err0;
			if (dwarf_global_formref(attr, &offset, &error))
				goto err0;
			if (dwarf_offdie_b(dw->dbg, offset, 1, type_die, &error))
				goto err0;
			if (dwarf_tag(*type_die, &tag, &error))
				goto err0;

		} while (tag == DW_TAG_typedef);

		/*
		 * - Base types: int, char, double, float....
		 * - Structures and Unions.
		 * - Enums
		 */
		if (tag == DW_TAG_base_type
			|| tag == DW_TAG_structure_type
			|| tag == DW_TAG_union_type
			|| tag == DW_TAG_enumeration_type)
		{
			/* Get the byte size. */
			if (dwarf_hasattr(*type_die, DW_AT_byte_size, &battr, &error))
				goto err0;
			if (dwarf_attr(*type_die, DW_AT_byte_size, &attr, &error))
				goto err0;
			if (dwarf_formudata(attr, &dwarf_unsig, &error))
				goto err0;

			/* Size \o/. */
			*byte_size = dwarf_unsig;

			/*
			 * Until now, structures/unions
			 * will be tracked globally and not
			 * per element.
			 */
			if (tag == DW_TAG_structure_type)
				*var_type = TSTRUCTURE;
			else if (tag == DW_TAG_union_type)
				*var_type = TUNION;

			/*
			 * Although GCC outputs the type used in an
			 * enumeration, CLANG does not.. so the only
			 * relevant attribute that I could get here
			 * is the byte size.
			 */
			else if (tag == DW_TAG_enumeration_type)
				*var_type = TENUM;

			/* Base types. */
			else
			{
				*var_type = TBASE_TYPE;

				/* Get encoding. */
				if (dwarf_hasattr(*type_die, DW_AT_encoding, &battr, &error))
					goto err0;
				if (dwarf_attr(*type_die, DW_AT_encoding, &attr, &error))
					goto err0;
				if (dwarf_formudata(attr, &dwarf_unsig, &error))
					goto err0;

				*encoding = dwarf_unsig;
			}

			return (0);
		}

		/*
		 * Pointers:
		 * Pointers is a special case here, because I do not
		 * want to track the pointers content but only the
		 * address the pointer is holding, like the base
		 * types.
		 */
		else if (tag == DW_TAG_pointer_type)
		{
			*var_type = TPOINTER;

			/*
			 * Clang does not output the field DW_AT_byte_size
			 * in pointer types, so I'm assuming the size would
			 * be the same as sizeof(void *), ;-).
			 */
			*byte_size = sizeof(void *);

			return (0);
		}

		/*
		 * Array types: It's important to note here that
		 * everything with 1 or more dimensions have the
		 * same tag: DW_TAG_array_type.
		 */
		else if (tag == DW_TAG_array_type)
		{
			*var_type = TARRAY;
			return (0);
		}

		/*
		 * Variable not recognized.
		 */
		else
			goto err0;
	}

	/* Not a type die. */
	else
		goto err0;

err0:
	return (-1);
}

/**
 * For a given variable DIE, parses the DIE and checks for knowable
 * type in it.
 *
 * @param var_die Variable DIE to be analyzed.
 * @param dw Structure Dwarf_Utils.
 * @param var Pointer to the target variable strutucture.
 */
static int dw_parse_variable_type
(
	Dwarf_Die *var_die,
	struct dw_utils *dw,
	struct dw_variable *var
)
{
	Dwarf_Error error;          /* Error code.   */
	Dwarf_Attribute attr;       /* Attribute.    */
	Dwarf_Die type_die;         /* Type die.     */
	Dwarf_Die type_child;       /* Type child.   */
	Dwarf_Bool battr;           /* Boolean.      */
	Dwarf_Half tag;             /* Tag.          */
	Dwarf_Unsigned dwarf_unsig; /* Byte size.    */

	size_t byte_size;
	int var_type;
	int encoding;

	byte_size = 0;
	var_type  = 0;
	encoding  = 0;

	if (!dw_parse_variable_base_type(var_die, dw, &byte_size,
		&var_type, &encoding, &type_die))
	{
		var->byte_size = byte_size;
		var->type.var_type = var_type;
		var->type.encoding = encoding;

		/* If array, lets parse its type and dimensions. */
		if (var_type == TARRAY)
		{
			Dwarf_Die child;
			int dim_count;

			/* Type. */
			if (dw_parse_variable_base_type(&type_die, dw, &byte_size,
				&var_type, &encoding, &type_child))
				goto err0;

			var->byte_size = 1;
			var->type.array.size_per_element = byte_size;
			var->type.array.var_type = var_type;
			var->type.encoding = encoding;

			/* Dimensions. (aka DW_TAG_subrange_type) */
			if (dwarf_child(type_die, &child, &error) != DW_DLV_OK)
				goto err0;

			dim_count = 0;
			do
			{
				if (dwarf_tag(child, &tag, &error) != DW_DLV_OK)
					goto err0;

		        /* If not subrange, skip. */
		        if (tag != DW_TAG_subrange_type)
		        	continue;

				/*
		         * Dimension size:
		         * GCC uses the tag DW_AT_upper_bound, while clang
		         * uses DW_AT_count, so we need to check for both.
		         */
		        if (!dwarf_hasattr(child, DW_AT_upper_bound, &battr, &error) && battr)
		        {
					if (dwarf_attr(child, DW_AT_upper_bound, &attr, &error))
						goto err0;
					if (dwarf_formudata(attr, &dwarf_unsig, &error))
						goto err0;

					var->byte_size *=  (dwarf_unsig + 1);
					var->type.array.elements_per_dimension[dim_count] =
						(dwarf_unsig + 1);
					dim_count++;

					var->type.array.dimensions++;
		        }

		        /* Maybe clang ?. */
		        else if (!dwarf_hasattr(child, DW_AT_count, &battr, &error) && battr)
		        {
					if (dwarf_attr(child, DW_AT_count, &attr, &error))
						goto err0;
					if (dwarf_formudata(attr, &dwarf_unsig, &error))
						goto err0;

					var->byte_size *= dwarf_unsig;
					var->type.array.elements_per_dimension[dim_count] =
						dwarf_unsig;
					dim_count++;

					var->type.array.dimensions++;
		        }

		        /* Error. */
		        else
					return (-1);

			} while (dwarf_siblingof(dw->dbg, child, &child, &error) == DW_DLV_OK);

			/* Updates with the correct size (elements * size_per_element). */
			var->byte_size *= var->type.array.size_per_element;
		}

		return (0);
	}

err0:
	return (-1);
}

/**
 * For a given variable DIE, parse the DIE and saves the
 * useful information into the dw_variable structure.
 *
 * @param var_die Target variable DIE to be parsed.
 * @param dw Dwarf Utils Structure.
 *
 * @return In case of success, returns a dw_variable structure
 * filled with the information found in the DIE, otherwise,
 * returns NULL.
 */
struct dw_variable *dw_parse_variable(Dwarf_Die *var_die, struct dw_utils *dw)
{
	Dwarf_Error error;        /* Dwarf Error.     */
	struct dw_variable *var;  /* Variable pointer */
	char *name;               /* Variable name.   */

	var = calloc(1, sizeof(struct dw_variable));

	/* Variable name. */
	if (dwarf_diename(*var_die, &name, &error) == DW_DLV_OK)
	{
		/* Copies to the right place. */
		var->name = malloc(sizeof(char) * (strlen(name) + 1) );
		strcpy(var->name, name);

		/* Deallocates. */
		dwarf_dealloc(dw->dbg, name, DW_DLA_STRING);

		/* Check if this variable is elegible to be added or not. */
		if (args.flags & FLG_IGNR_LIST)
		{
			if (hashtable_get(&args.iw_list.ht_list, var->name) != NULL)
				goto err0;
		}
		else if (args.flags & FLG_WATCH_LIST)
		{
			if (hashtable_get(&args.iw_list.ht_list, var->name) == NULL)
				goto err0;
		}

		/* Location. */
		if (dw_parse_variable_location(var_die, dw, var))
			goto err0;

		/* Base type, size... */
		if (dw_parse_variable_type(var_die, dw, var))
			goto err0;

		return (var);
	}
	else
		goto dealloc;

err0:
	free(var->name);
	dwarf_dealloc(dw->dbg, name, DW_DLA_STRING);

dealloc:
	free(var);
	return (NULL);
}

/**
 * Returns a high_pc and low_pc for a given function
 * name.
 *
 * @param dw Dwarf Utils structure pointer.
 * @param func Function name
 * @param low_pc Returned low_pc
 * @param high_pc Returned high_pc
 *
 * @note This function does not take into account the
 * existence of static functions, which, in turn, may
 * have functions of the same name in different Compile
 * Units.
 */
int dw_get_address_by_function
(
	struct dw_utils *dw,
	const char *func
)
{
	char *name;                /* Die name.        */
	Dwarf_Die die;             /* Current die.     */
	Dwarf_Die child;           /* Die child.       */
	Dwarf_Error error;         /* Error code.      */
	Dwarf_Half tag;            /* Tag.             */
	Dwarf_Attribute attr;      /* Attribute.       */
	Dwarf_Addr alow_pc;        /* Address low pc.  */
	Dwarf_Addr ahigh_pc;       /* Address high pc. */
	Dwarf_Unsigned uhigh_pc;   /* Address high pc. */

	/* Clear addresses. */
	dw->dw_func.high_pc = 0;
	dw->dw_func.low_pc = 0;

	/*
	 * Loop through all compile units and searchs
	 * for the function specified.
	 */
	while (dw_next_cu_die(dw, &die) > 0)
	{
		if (dwarf_child(die, &child, &error) != DW_DLV_OK)
			continue;

		/* Loop through all the siblings. */
		do
		{
			if (dwarf_tag(child, &tag, &error) != DW_DLV_OK)
		        QUIT(EXIT_FAILURE, "Error in dwarf_tag\n");

			/* Only interested in subprogram DIEs here */
			if (tag != DW_TAG_subprogram)
				continue;

		    /*
		     * Let us check if belongs to the target
		     * function name.
		     */
			if (dwarf_diename(child, &name, &error) == DW_DLV_OK)
			{
				if (!strcmp(name, func))
				{
					/* Low PC. */
					if (!dwarf_attr(child, DW_AT_low_pc, &attr, &error) &&
						!dwarf_formaddr(attr, &alow_pc, &error))
					{
						dw->dw_func.low_pc = alow_pc;

						/*
						 * High PC:
						 * In DWARF4, the high pc have the attribute changed from FORM_addr
						 * to data8.. so we need to check the attribute type before
						 * proceed.
						 */
						if (dwarf_attr(child, DW_AT_high_pc, &attr, &error) == DW_DLV_OK)
						{
							Dwarf_Half form;
							dwarf_whatform(attr, &form, &error);

							if (form == DW_FORM_addr)
							{
								if (!dwarf_formaddr(attr, &ahigh_pc, &error))
									dw->dw_func.high_pc = ahigh_pc - 1;
								else
									QUIT(EXIT_FAILURE, "Error while getting high pc from formaddr");
							}
							else
							{
								if (!dwarf_formudata(attr, &uhigh_pc, &error))
									dw->dw_func.high_pc = uhigh_pc + alow_pc - 1;
								else
									QUIT(EXIT_FAILURE, "Error while getting high pc from formudata");
							}

							dw->cu_die = die;
							dw->fn_die = child;
							dwarf_dealloc(dw->dbg, name, DW_DLA_STRING);

							/*
							 * Here we purposely iterate until the end, in order to be
							 * able to re-iterate again through the cu_die again in
							 * other opportunity.
							 */
						}
					}
				}
			}

		} while (dwarf_siblingof(dw->dbg, child, &child, &error) == DW_DLV_OK);
	}

	return (0);
}

/**
 * In order to find the correct variable location, one of the
 * required steps is evaluate the field DW_AT_frame_base
 * from the target function, in order to find the correct
 * offset from the base/stack pointer.
 *
 * Thus, get_base_pointer_offset() gets the appropriate
 * base pointer offset and saves it into the Dwarf Utils
 * structure.
 *
 * @param dw Dwarf Utils structure pointer.
 *
 * @return Returns 0 if success and a negative number
 * otherwise.
 */
static int dw_get_base_pointer_offset(struct dw_utils *dw)
{
	Dwarf_Error error;      /* Error code.              */
	Dwarf_Attribute attr;   /* Attribute.               */
	Dwarf_Locdesc **llbuf;  /* Location descriptor buf. */
	Dwarf_Signed lcnt;      /* Location count.          */
	Dwarf_Loc *lr;          /* Location.                */
	Dwarf_Bool battr;       /* Boolean.                 */
	int retcode;            /* Return code.             */

	retcode = -1;

	/* Location. */
	if (!dwarf_hasattr(dw->fn_die, DW_AT_frame_base, &battr, &error) && battr)
	{
		if (dwarf_attr(dw->fn_die, DW_AT_frame_base, &attr, &error))
			goto err0;
		if (dwarf_loclist_n(attr, &llbuf, &lcnt, &error))
			goto err0;

		/* Only one location entry. */
		if (llbuf[0]->ld_cents > 1)
		{
			fprintf(stderr, "dw_parse_variable: location entries greater than 1\n"
				"  make sure you're building your target with: \n"
				"  -O0 -gdwarf-2 -fno-omit-frame-pointer\n");
			goto dealloc;
		}

		/*
		 * Loop through all locations and searchs one that uses
		 * the base pointer. If no one can be found, a negative
		 * number is returned.
		 *
		 * Its important to note here that when the target
		 * is compiled with -gdwarf-2, GCC emmits a
		 * location list with multiples entries relating
		 * each address with a location.
		 *
		 * However, while building with clang, the compiler
		 * uses only one location referring directly to the
		 * base pointer DW_OP_reg6.
		 */
		dw->dw_func.bp_offset = INT_MIN;

		for (int i = 0; i < lcnt; i++)
		{
			/* Current location. */
			lr = &llbuf[i]->ld_s[0];

			if (lr->lr_atom == DW_OP_reg6 || lr->lr_atom == DW_OP_breg6)
			{
				/* GCC approach. */
				if (lcnt > 1)
					dw->dw_func.bp_offset = lr->lr_number;

				/* clang approach. */
				else
					dw->dw_func.bp_offset = 0;

				break;
			}
		}

		/* If cannot find any base pointer, error. */
		if (dw->dw_func.bp_offset == INT_MIN)
			QUIT(EXIT_FAILURE, "cannot find any base pointer!\n"
				"  make sure you're building your target with: \n"
				"  -O0 -gdwarf-2 -fno-omit-frame-pointer\n");

		retcode = 0;
	}
	else
		goto err0;

	/* Deallocates everything. */
dealloc:
	for (int i = 0; i < lcnt; i++)
	{
		dwarf_dealloc(dw->dbg, llbuf[i]->ld_s, DW_DLA_LOC_BLOCK);
		dwarf_dealloc(dw->dbg, llbuf[i], DW_DLA_LOCDESC);
	}
	dwarf_dealloc(dw->dbg, llbuf, DW_DLA_LIST);

err0:
	return (retcode);
}

/**
 * Gets all the global variables found in the target file
 * and all the local variables from the given function
 * found in dw->fn_die.
 *
 * @param dw Dwarf Utils Structure Pointer.
 *
 * @return If success, returns an array containing
 * all the variables found, otherwise, returns NULL.
 */
struct array *dw_get_all_variables(struct dw_utils *dw)
{
	Dwarf_Die die;             /* Current CU.      */
	Dwarf_Die child0, child1;  /* Siblings.        */
	Dwarf_Half tag;            /* Tag.             */
	Dwarf_Error error;         /* Error code.      */

	struct dw_variable *var;   /* Variable.        */
	struct array *vars;        /* Variables array. */

	/* Invalid Compile Unit. */
	if (!dw->cu_die)
		QUIT(EXIT_FAILURE, "Compile Unit not found!\n");

	/*
	 * Base pointer offset
	 * In order to get the appropriate variable location
	 * is necessary to first read the attribute
	 * DW_AT_frame_base from the subprogram present in
	 * dw->fn_die.
	 */
	dw_get_base_pointer_offset(dw);

	/* Initializes array. */
	array_init(&vars);

	/* If globals enabled. */
	if (args.flags & FLG_ONLY_GLOBALS)
	{
		/*
		 * Loop through all compile units and searchs
		 * for all global variables.
		 */
		while (dw_next_cu_die(dw, &die) > 0)
		{
			if (dwarf_child(die, &child1, &error) != DW_DLV_OK)
				continue;

			child0 = NULL;

			/* Loop through all the siblings. */
			do
			{
				if (child0 != NULL)
					dwarf_dealloc(dw->dbg, child0, DW_DLA_DIE);

				child0 = child1;

				if (dwarf_tag(child1, &tag, &error) != DW_DLV_OK)
				{
					array_finish(&vars);
					QUIT(EXIT_FAILURE, "Error in dwarf_tag\n");
				}

				/* Only interested in global variables DIEs here */
				if (tag != DW_TAG_variable)
					continue;

				var = dw_parse_variable(&child1, dw);
				if (var)
					array_add(&vars, var);

			} while (dwarf_siblingof(dw->dbg, child0, &child1, &error) == DW_DLV_OK);

			dwarf_dealloc(dw->dbg, die, DW_DLA_DIE);
			dwarf_dealloc(dw->dbg, child1, DW_DLA_DIE);
		}
	}

	/* If locals enabled. */
	if (args.flags & FLG_ONLY_LOCALS)
	{
		/*
		 * Loop through all the subprogram childs and get
		 * the local variables.
		 */
		child0 = NULL;

		if (dwarf_child(dw->fn_die, &child1, &error) != DW_DLV_OK)
		{
			array_finish(&vars);
			QUIT(EXIT_FAILURE, "subprogram not found\n");
		}

		do
		{
			if (child0 != NULL)
				dwarf_dealloc(dw->dbg, child0, DW_DLA_DIE);

			child0 = child1;

			if (dwarf_tag(child1, &tag, &error) != DW_DLV_OK)
			{
				array_finish(&vars);
				QUIT(EXIT_FAILURE, "Error in dwarf_tag\n");
			}

			/* Only interested in local variables DIEs here */
			if (tag != DW_TAG_variable && tag != DW_TAG_formal_parameter)
				continue;

			var = dw_parse_variable(&child1, dw);
			if (var)
				array_add(&vars, var);

		} while (dwarf_siblingof(dw->dbg, child0, &child1, &error) == DW_DLV_OK);

		dwarf_dealloc(dw->dbg, child1, DW_DLA_DIE);
	}

	return (vars);
}

/**
 * @brief Gets all the lines for the pre-configured
 * function found in @p dw dw_utils structure and
 * returns an array of lines.
 *
 * @param dw Dwarf Utils Structure Pointer.
 *
 * @return Returns an array of lines, contaning the address,
 * line type and line number for each entry.
 */
struct array *dw_get_all_lines(struct dw_utils *dw)
{
	Dwarf_Line *lines;         /* Lines.                  */
	Dwarf_Signed nlines;       /* Amount of lines per CU. */
	Dwarf_Addr lineaddr;       /* Current adddress.       */
	Dwarf_Unsigned lineno;     /* Line number.            */
	Dwarf_Bool dbool;          /* Boolean.                */
	Dwarf_Error error;         /* Error.                  */

	struct dw_line *line;      /* Line.                   */
	struct array *array_lines; /* Line array.             */

	/* Invalid Compile Unit. */
	if (!dw->cu_die)
		QUIT(EXIT_FAILURE, "Compile Unit not found!\n");

	/* Invalid Function. */
	if (dw->dw_func.low_pc == 0 || dw->dw_func.high_pc == 0
		|| (dw->dw_func.high_pc <= dw->dw_func.low_pc) )
		QUIT(EXIT_FAILURE, "Invalid Function Range!\n");

	/* Initializes array. */
	array_init(&array_lines);

	/* Get the lines from the Compile Unit specified. */
	if (dwarf_srclines(dw->cu_die, &lines, &nlines, &error) != DW_DLV_OK)
	{
		array_finish(&array_lines);
		QUIT(EXIT_FAILURE, "Error while getting the lines!\n");
	}

	/*
	 * Loop through all the lines and searchs the lines that
	 * belongs to the function.
	 */
	for (int i = 0; i < nlines; i++)
	{
		/* Retrieve the virtual address for this line. */
		if (dwarf_lineaddr(lines[i], &lineaddr, &error))
		{
			array_finish(&array_lines);
			QUIT(EXIT_FAILURE, "Cannot retrieve the line address!\n");
		}

		/* Skips lines that do not belongs to the target function. */
		if (lineaddr < dw->dw_func.low_pc || lineaddr > dw->dw_func.high_pc)
			continue;

		/* Retrieve the line number in the source file. */
		if (dwarf_lineno(lines[i], &lineno, &error))
		{
			array_finish(&array_lines);
			QUIT(EXIT_FAILURE, "Cannot retrieve the line number"
				"from address %x\n", lineaddr);
		}

		/* Everything went fine until here, lets allocate our line. */
		line = malloc(sizeof(struct dw_line));
		line->addr = lineaddr;
		line->line_no = lineno;
		line->line_type = 0;

		/* Line type. */
		if (!dwarf_linebeginstatement(lines[i], &dbool, &error) && dbool)
			line->line_type |= LBEGIN_STMT;
		if (!dwarf_lineendsequence(lines[i], &dbool, &error) && dbool)
			line->line_type |= LEND_SEQ;
		if (!dwarf_lineblock(lines[i], &dbool, &error) && dbool)
			line->line_type |= LBLOCK;

		/* Add to our array. */
		array_add(&array_lines, line);
	}

	return (array_lines);
}

/**
 * @brief Gets the complete qualified path for the source file
 * belonging to the current Compile Unit.
 *
 * @param dw Dwarf Utils Structure Pointer.
 *
 * @return Returns a string containing the complete qualified
 * path for the debugged Compile Unit.
 *
 * @note This does not guarantee that the path *will* exist
 * in the filesystem while debugging, this should be checked.
 */
char *dw_get_source_file(struct dw_utils *dw)
{
	char *comp_dir;        /* Compile dir.       */
	char *file;            /* CU file.           */
	char *filename;        /* Complete filename. */
	Dwarf_Error error;     /* Error code.        */
	Dwarf_Bool battr;      /* Boolean.           */
	Dwarf_Attribute attr;  /* Attribute.         */

	/* Invalid Compile Unit. */
	if (!dw->cu_die)
		QUIT(EXIT_FAILURE, "Compile Unit not found!\n");

	filename = NULL;

	/* Get file name. */
	if (dwarf_diename(dw->cu_die, &file, &error) == DW_DLV_OK)
	{
		/* Get compile directory. */
		if (!dwarf_hasattr(dw->cu_die, DW_AT_comp_dir, &battr, &error) && battr)
		{
			if (dwarf_attr(dw->cu_die, DW_AT_comp_dir, &attr, &error))
				goto out1;
			if (dwarf_formstring(attr, &comp_dir, &error))
				goto out1;

			/* Room enough for filename ;-). */
			filename = malloc(sizeof(char)*(strlen(comp_dir)+strlen(file)+2));
			strcpy(filename, comp_dir);
			strcat(filename, "/");
			strcat(filename, file);
		}
		else
			goto out1;
	}
	else
		goto out0;

	dwarf_dealloc(dw->dbg, comp_dir, DW_DLA_STRING);
out1:
	dwarf_dealloc(dw->dbg, file, DW_DLA_STRING);
out0:
	return (filename);
}

/**
 * @brief Asserts if the current compile unit was build in a C
 * language.
 *
 * Since PBD only supports C (and have no intention to support others),
 * it's important to do this check, in order to avoid trying to debug
 * unsupported languages, like C++.
 *
 * @param dw Dwarf Utils Structure Pointer.
 *
 * @return Returns 0 if not C and 1 if the Compile Unit is C.
 */
int dw_is_c_language(struct dw_utils *dw)
{
	Dwarf_Error error;    /* Error code.  */
	Dwarf_Bool battr;     /* Boolean.     */
	Dwarf_Attribute attr; /* Attribute.   */
	Dwarf_Unsigned lang;  /* Language.    */
	int ret;              /* Return code. */

	ret = 0;

	/* Invalid Compile Unit. */
	if (!dw->cu_die)
		QUIT(EXIT_FAILURE, "Compile Unit not found!\n");

	/* Get build language. */
	if (!dwarf_hasattr(dw->cu_die, DW_AT_language, &battr, &error) && battr)
	{
		if (dwarf_attr(dw->cu_die, DW_AT_language, &attr, &error))
			goto out;
		if (dwarf_formudata(attr, &lang, &error))
			goto out;

		/* Check if C. */
		switch (lang)
		{
			case DW_LANG_C89:
			case DW_LANG_C:
			case DW_LANG_C99:
			case DW_LANG_C11:
				ret = 1;
				break;
			default:
				break;
		}
	}
out:
	return (ret);
}

/**
 * @brief Dump all lines found in the target function.
 *
 * @param lines Lines array.
 */
void dw_lines_dump(struct array *lines)
{
	for (int i = 0; i < (int) array_size(&lines); i++)
	{
		struct dw_line *l;
		l = array_get(&lines, i, NULL);
		printf("    line: %03d / address: %" PRIx64 " / type: %d\n",
			l->line_no, l->addr, l->line_type);
	}
}

/**
 * @brief Deallocates all the lines remaining.
 *
 * @param lines Lines list.
 */
void dw_lines_array_free(struct array *lines)
{
	int size;

	size = (int) array_size(&lines);
	for (int i = 0; i < size; i++)
	{
		struct dw_lines *l;
		l = array_remove(&lines, 0, NULL);
		free(l);
	}
	array_finish(&lines);
}
