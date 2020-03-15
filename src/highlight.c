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

/**
 * This is Neko (cat in Japanese): A very simple syntax highlighter made in C
 * for C language.
 *
 * Despite its simplicity, it has some interesting features:
 * - Can work as a standalone program or as a library
 *   with only one major function `highlight_line()`.
 *
 * - Fast! Neko manages to catch up with the classic `cat` or even surpass it
 *   for certain types of source code. Also, it is *much* faster than
 *   similar programs, such as bat (https://github.com/sharkdp/bat).
 *
 * - Bloat free!. Neko has no additional dependencies and the only thing you
 *   need is the standard libc.
 *
 * - Not so bad/decent syntax highlight. Although with some limitations,
 *   Neko manages to deliver a visually pleasing, and similar or superior
 *   output to that found in programs that use GtkSourceView, such as
 *   Mousepad, GEdit.
 *
 * - Themes. Neko makes available a simple configuration scheme file that allows
 *   users to define your own themes.
 */

#define _POSIX_C_SOURCE 200809L
#include "highlight.h"

/* Run standalone?. */
#define RUN_STANDALONE 0

#if RUN_STANDALONE == 1
#define OPTPARSE_IMPLEMENTATION
#include "optparse.h"
#endif

#if defined(__GNUC__) || defined(__GNUG__) || defined(__clang__)
#define INLINE  __attribute__((always_inline)) inline
#else
#define INLINE inline
#endif

/* States. */
#define HL_DEFAULT                     0
#define HL_KEYWORD                     1
#define HL_NUMBER                      3
#define HL_CHAR                        4
#define HL_STRING                      5
#define HL_COMMENT_MULTI               6
#define HL_PREPROCESSOR                7
#define HL_PREPROCESSOR_INCLUDE        8
#define HL_PREPROCESSOR_INCLUDE_STRING 9

/* Themes. */
#define COLOR_8       0  /* 8-colors theme.     */
#define ELF_DEITY     8  /* Elf Deity theme.    */
#define USER_DEF     16  /* User-defined theme. */

/* Current theme. */
static int CURRENT_THEME = ELF_DEITY;

/* Color map. */
char *COLORS[] = {
	/*
	 * 8 colors theme.
	 */
	"\033[31m",  /* LIGHT_RED      */
	"\033[32m",  /* LIGHT_GREEN    */
	"\033[33m",  /* LIGHT_YELLOW   */
	"\033[34m",  /* LIGHT_BLUE     */
	"\033[35m",  /* LIGHT_PURPLE   */
	"\033[36m",  /* LIGHT_CYAN     */
	"\033[37m",  /* LIGHT_GRAY     */
	"\033[39m",  /* DEFAULT        */

	/*
	 * 256 Colors.
	 *
	 * Elf Deity Theme.
	 *
	 * I don't really like design stuff and so on, and if I were
	 * to name colors myself, I could get a maximum of 16 names.
	 * However, since we are dealing with 256 colors and we can
	 * have several shades of the same color, the names below
	 * have been extracted from the website
	 * (http://chir.ag/projects/name-that-color) from their RGB
	 * values corresponding.
	 */
	"\033[38;5;63m",   /* CORNFLOWER_BLUE_256  */
	"\033[38;5;83m",   /* SCREAMIN_GREEN_256   */
	"\033[38;5;227m",  /* LASER_FLAMINGO_256   */
	"\033[38;5;214m",  /* YELLOW_SEA_256       */
	"\033[38;5;207m",  /* PINK_FLAMINGO_256    */
	"\033[38;5;102m",  /* GRAY_256             */
	"\033[38;5;193m",  /* REEF_256             */
	"\033[38;5;101m",  /* CLAY_CREEK_256       */

	/* Placeholder for external theme. */
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	""
};

/* Colors constants. */
#define RESET_COLOR   "\033[0m"
#define PREPROC_COLOR    0
#define TYPES_COLOR      1
#define KWRDS_COLOR      2
#define NUMBER_COLOR     3
#define STRING_COLOR     4
#define COMMENT_COLOR    5
#define FUNC_CALL_COLOR  6
#define SYMBOL_COLOR     7

/* Global state. */
struct global_state
{
	int state;
} gs = {
	.state = HL_DEFAULT
};

/* Keyword. */
struct keyword
{
	char *keyword;
	int color;
};

/* Keywords. */
struct keyword keywords_list[] = {
	/* C Types. */
	{"double",   TYPES_COLOR}, {"int",    TYPES_COLOR}, {"long",  TYPES_COLOR},
	{"char",     TYPES_COLOR}, {"float",  TYPES_COLOR}, {"short", TYPES_COLOR},
	{"unsigned", TYPES_COLOR}, {"signed", TYPES_COLOR},

	/* Common typedefs. */
	{"int8_t",  TYPES_COLOR}, {"uint8_t",  TYPES_COLOR},
	{"int16_t", TYPES_COLOR}, {"uint16_t", TYPES_COLOR},
	{"int32_t", TYPES_COLOR}, {"uint32_t", TYPES_COLOR},
	{"int64_t", TYPES_COLOR}, {"uint64_t", TYPES_COLOR},

	{"size_t", TYPES_COLOR}, {"ssize_t", TYPES_COLOR}, {"off_t", TYPES_COLOR},
	{"int8_t", TYPES_COLOR}, {"NULL",   NUMBER_COLOR}, /* Why not NULL?. */

	/* Other keywords. */
	{"auto",    KWRDS_COLOR}, {"struct",   KWRDS_COLOR}, {"break",   KWRDS_COLOR},
	{"else",    KWRDS_COLOR}, {"switch",   KWRDS_COLOR}, {"case",    KWRDS_COLOR},
	{"enum",    KWRDS_COLOR}, {"register", KWRDS_COLOR}, {"typedef", KWRDS_COLOR},
	{"extern",  KWRDS_COLOR}, {"return",   KWRDS_COLOR}, {"union",   KWRDS_COLOR},
	{"const",   KWRDS_COLOR}, {"continue", KWRDS_COLOR}, {"for",     KWRDS_COLOR},
	{"void",    KWRDS_COLOR}, {"default",  KWRDS_COLOR}, {"goto",    KWRDS_COLOR},
	{"sizeof",  KWRDS_COLOR}, {"volatile", KWRDS_COLOR}, {"do",      KWRDS_COLOR},
	{"if",      KWRDS_COLOR}, {"static",   KWRDS_COLOR}, {"while",   KWRDS_COLOR}
};

/* Hashtable keywords. */
struct hashtable *ht_keywords = NULL;

/*
 * Allowed symbols table.
 *
 * This is a simple lookup table that contains all symbols that
 * will be highlighted.
 */
unsigned char symbols_table[256] =
{
	['['] = 1, [']'] = 1, ['('] = 1, [')'] = 1, ['{'] = 1, ['}'] = 1,
	['*'] = 1, [':'] = 1, ['='] = 1, [';'] = 1, ['-'] = 1, ['>'] = 1,
	['&'] = 1, ['+'] = 1, ['~'] = 1, ['!'] = 1, ['/'] = 1, ['%'] = 1,
	['<'] = 1, ['^'] = 1, ['|'] = 1, ['?'] = 1
};

/**
 * Allocates a new Highlighted Buffer line.
 *
 * A Highlighted Buffer Line is a facility that transparently allows
 * the user to append chars and strings inside a buffer without having
 * to worry about space, reallocs and so on, something similiar to
 * SDS strings.
 *
 * @returns Return a pointer to the highlighted line.
 */
char* highlight_alloc_line(void)
{
	struct highlighted_line *hl;
	hl = calloc(1, sizeof(struct highlighted_line) + (sizeof(char) * 80));
	hl->idx = 0;
	hl->size = 80;
	return ((char*)(hl+1));
}

/**
 * Appends a single char @p c into the highlighted buffer
 * @p line.
 *
 * @param line Highlighted Buffer.
 * @param c Char to be highlighted.
 *
 * @return Returns a pointer to the highlighted buffer containing
 * the appended character,
 */
INLINE static char* add_char_to_hl(char *line, char c)
{
	struct highlighted_line *hl;
	hl = ((struct highlighted_line *)line - 1);

	if (hl->idx >= hl->size)
	{
		hl->size += 32;
		hl = realloc(hl, sizeof(struct highlighted_line) +
			(sizeof(char) * hl->size));
	}
	line = (char*)(hl+1);
	line[hl->idx++] = c;
	return (line);
}

/**
 * Appends a new string @p str of size @p size into the
 * highlighted buffer @p line.
 *
 * @param line Highlighted Buffer.
 * @param str String to be appended.
 * @param size Size of the given string @p str.
 *
 * @return Returns a pointer to the highlighted buffer containing
 * the appended string.
 */
INLINE static char* add_str_to_hl(char *line, const char *str, size_t size)
{
	struct highlighted_line *hl;
	hl = ((struct highlighted_line *)line - 1);

	if (!size)
		size = strlen(str);

	if ( (hl->size - hl->idx) < size )
	{
		/* Make room for the string and adds 32 extra chars. */
		hl->size += (size - (hl->size - hl->idx)) + 32;
		hl = realloc(hl, sizeof(struct highlighted_line) +
			(sizeof(char) * hl->size));
	}
	line = (char*)(hl+1);
	memcpy(line+hl->idx, str, size);
	hl->idx += size;
	return (line);
}

/**
 * Deallocate a Highlighted Line Buffer.
 *
 * @param line Highlighted Line Buffer to be deallocated.
 */
void highlight_free(char *line)
{
	struct highlighted_line *hl;
	hl = ((struct highlighted_line *)line - 1);
	free(hl);
}

/**
 * Checks if the given character belongs to a valid keyword
 * or not.
 *
 * @param c Character to be checked.
 *
 * @return Returns 1 if the character belongs to a valid
 * keyword, 0 otherwise.
 */
INLINE static int is_char_keyword(char c)
{
	return (isalpha(c) || isdigit(c) || c == '_');
}

/**
 * Checks if the given keyword @p key is one of the keywords
 * allowed, if so, returns the structure that belongs to the
 * given keyword, otherwise, returns NULL.
 *
 * @param key Keyword to be checked.
 * @param size Keyword length.
 *
 * @return Returns a keyword structure, otherwise, returns NULL.
 */
static struct keyword* is_keyword(const char *key, size_t size)
{
	struct keyword *k;

	/* This should occur in most of the cases. */
	if (size < 80)
	{
		char nkey[80];
		memcpy(nkey, key, size);
		nkey[size] = '\0';
		return (hashtable_get(&ht_keywords, nkey));
	}

	/*
	 * Otherwise, we should temporarily allocate a new string
	 * in order to use as parameter of hashtable.
	 *
	 * I hope the overhead copy (in order to use hashtable) will
	 * compensate the O(n) iterations through the list.
	 */
	char *nkey = malloc(sizeof(char) * (size+1));
	memcpy(nkey, key, size);
	nkey[size] = '\0';

	/*
	 * Check if the at the given range belongs to
	 * some keyword.
	 */
	k = hashtable_get(&ht_keywords, nkey);
	free(nkey);
	return (k);
}

/**
 * Highlight (or not) a given symbol @p c.
 *
 * @param c Symbol to be highlighted.
 * @param hl Highlighted Line Buffer.
 */
INLINE static int highlight_symbol(int c, char **hl)
{
	if (symbols_table[c])
	{
		*hl = add_str_to_hl(*hl, COLORS[CURRENT_THEME+SYMBOL_COLOR], 0);
		*hl = add_char_to_hl(*hl, c);
		*hl = add_str_to_hl(*hl, RESET_COLOR, 4);
		return (1);
	}
	return (0);
}

/**
 * For a given line @p line and a (already) allocated
 * highlighted line buffer @p hl, highlights the
 * line and returns @p hl with the highlighted line.
 *
 * @param line Line (null terminated string) to be highlighted.
 * @param hl Pre-allocated Highlighted Line buffer.
 *
 * @return Returns a Highlighted Line Buffer.
 */
char *highlight_line(const char *line, char *hl)
{
	struct highlighted_line *high_line;
	size_t str_size;
	size_t tok_size;
	int keyword_start;
	int keyword_end;

	/* Reset indexes. */
	if (hl != NULL)
	{
		high_line = ((struct highlighted_line *)hl - 1);
		high_line->idx = 0;
	}
	else
		hl = highlight_alloc_line();

	keyword_start = 0;
	keyword_end = 0;
	str_size = strlen(line);

	/* For each char, including the null terminated. */
	for (size_t i = 0; i < str_size+1; i++)
	{
		switch (gs.state)
		{
			/* Default state. */
			case HL_DEFAULT:
			{
				/*
				 * If potential keyword.
				 *
				 * A valid C keyword may contain numbers, but *not*
				 * as a suffix.
				 */
				if (is_char_keyword(line[i]) && !isdigit(line[i]))
				{
					keyword_start = i;
					gs.state = HL_KEYWORD;
					continue;
				}

				/* If potential number. */
				else if (isdigit(line[i]))
				{
					keyword_start = i;
					gs.state = HL_NUMBER;
					continue;
				}

				/* If potential char. */
				else if (line[i] == '\'')
				{
					keyword_start = i;
					gs.state = HL_CHAR;
					continue;
				}

				/* If potential string. */
				else if (line[i] == '"')
				{
					keyword_start = i;
					gs.state = HL_STRING;
					continue;
				}

				/* Line or multiline comment. */
				else if (line[i] == '/' && i+1 < str_size)
				{
					/* If one of them, skip next char and puts the color. */
					if (line[i+1] == '/')
					{
						/*
						 * Line comment is pretty 'hacky':
						 * After a line comment is identified correctly, all remaining
						 * characters (to the end of the line) *will* be comments for
						 * sure, so we can safely analyze this and abort the loop.
						 */
						tok_size = str_size - i;
						hl = add_str_to_hl(hl, COLORS[CURRENT_THEME+COMMENT_COLOR], 0);
						hl = add_str_to_hl(hl, line+i, tok_size);
						hl = add_str_to_hl(hl, RESET_COLOR, 4);

						/* string terminator \'0', =). */
						hl = add_char_to_hl(hl, '\0');

						/* Abort loop. */
						i = str_size;
						continue;
					}

					/* Multiline comments should be handled as usual. */
					else if (line[i+1] == '*')
					{
						gs.state = HL_COMMENT_MULTI;
						keyword_start = i;
						i += 1;
						continue;
					}

					/* Something else, maybe a symbol?. */
					highlight_symbol(line[i], &hl);
					continue;
				}

				/* Preprocessor. */
				else if (line[i] == '#')
				{
					keyword_start = i;
					gs.state = HL_PREPROCESSOR;
					continue;
				}

				/* If any symbol supported. */
				else if (highlight_symbol(line[i], &hl))
					continue;

				hl = add_char_to_hl(hl, line[i]);
			}
			break;

			/* Keyword state. */
			case HL_KEYWORD:
			{
				/* End of keyword, check if it really is a valid keyword. */
				if (!is_char_keyword(line[i]))
				{
					struct keyword *keyword;
					keyword_end = i - 1;
					tok_size = keyword_end - keyword_start + 1;
					gs.state = HL_DEFAULT;

					/* If keyword, highlight. */
					if ( (keyword = is_keyword(line+keyword_start, tok_size)) != NULL )
					{
						hl = add_str_to_hl(hl, COLORS[CURRENT_THEME+keyword->color], 0);
						hl = add_str_to_hl(hl, line+keyword_start, tok_size);
						hl = add_str_to_hl(hl, RESET_COLOR, 4);

						/* Maybe we should highlight this remaining char. */
						if (!highlight_symbol(line[i], &hl))
							hl = add_char_to_hl(hl, line[i]);
						continue;
					}

					/*
					 * If not keyword, maybe its a function call.
					 *
					 * Important to note that this is hacky and will only work
					 * if there is no space between keyword and '('.
					 */
					if (line[i] == '(')
					{
						keyword_end = i;
						tok_size = keyword_end - keyword_start;
						gs.state = HL_DEFAULT;
						hl = add_str_to_hl(hl, COLORS[CURRENT_THEME+FUNC_CALL_COLOR], 0);
						hl = add_str_to_hl(hl, line+keyword_start, tok_size);
						hl = add_str_to_hl(hl, RESET_COLOR, 4);

						/* Opening parenthesis will always be highlighted */
						highlight_symbol(line[i], &hl);
						continue;
					}

					hl = add_str_to_hl(hl, line+keyword_start, tok_size);

					/* Maybe we should highlight this remaining char. */
					if (!highlight_symbol(line[i], &hl))
						hl = add_char_to_hl(hl, line[i]);
					continue;
				}
			}
			break;

			/* Number state. */
			case HL_NUMBER:
			{
				char c = tolower(line[i]);

				/*
				 * Should we end the state?.
				 *
				 * Very important observation:
				 * Although the number highlight works fine for most (if not all)
				 * of the possible cases, it also assumes that the code is written
				 * correctly and the source is able to compile, meaning that:
				 *
				 * Numbers like: 123, 0xABC123, 12.3e4f, 123ULL....
				 * will be correctly identified and highlighted
				 *
				 * But, 'numbers' like: 123ABC, 0xxxxABCxx123, 123UUUUU....
				 * will also be highlighted.
				 *
				 * It also assumes that no keyword will start with a number
				 * and everything starting with a number (except inside strings or
				 * comments) will be a number.
				 */
				if (!isdigit(c) && (c < 'a' || c > 'f') && c != 'b' &&
					c != 'x' && c != 'u' && c != 'l' && c != '.')
				{
					keyword_end = i - 1;
					tok_size = keyword_end - keyword_start + 1;
					gs.state = HL_DEFAULT;

					/* If not a valid char keyword: valid number. */
					if (!is_char_keyword(line[i]))
					{
						hl = add_str_to_hl(hl, COLORS[CURRENT_THEME+NUMBER_COLOR], 0);
						hl = add_str_to_hl(hl, line+keyword_start, tok_size);
						hl = add_str_to_hl(hl, RESET_COLOR, 4);

						/* Maybe we should highlight this remaining char. */
						if (!highlight_symbol(line[i], &hl))
							hl = add_char_to_hl(hl, line[i]);
						continue;
					}

					/* Otherwise, something else. */
					hl = add_str_to_hl(hl, line+keyword_start, tok_size);

					/* Maybe we should highlight this remaining char. */
					if (!highlight_symbol(line[i], &hl))
						hl = add_char_to_hl(hl, line[i]);
					continue;
				}
			}
			break;

			/* Char state. */
			case HL_CHAR:
			{
				/* Should we end char state?. */
				if (line[i] == '\'' && line[i + 1] != '\'')
				{
					keyword_end = i - 1;
					tok_size = keyword_end - keyword_start + 1;
					gs.state = HL_DEFAULT;

					hl = add_str_to_hl(hl, COLORS[CURRENT_THEME+STRING_COLOR], 0);
					hl = add_str_to_hl(hl, line+keyword_start, tok_size);
					hl = add_char_to_hl(hl, line[i]);
					hl = add_str_to_hl(hl, RESET_COLOR, 4);
					continue;
				}
			}
			break;

			/* String state. */
			case HL_STRING:
			{
				/* Should we end char state?. */
				if (line[i] == '"' && line[i - 1] != '\\')
				{
					keyword_end = i - 1;
					tok_size = keyword_end - keyword_start + 1;
					gs.state = HL_DEFAULT;

					hl = add_str_to_hl(hl, COLORS[CURRENT_THEME+STRING_COLOR], 0);
					hl = add_str_to_hl(hl, line+keyword_start, tok_size);
					hl = add_char_to_hl(hl, line[i]);
					hl = add_str_to_hl(hl, RESET_COLOR, 4);
					continue;
				}
			}
			break;

			/* Multiline comment. */
			case HL_COMMENT_MULTI:
			{
				/*
				 * If we are at the end of line _or_ have identified
				 * an end of comment...
				 */
				if (i == str_size || (line[i] == '*' && i+1 < str_size &&
					line[i+1] == '/'))
				{
					if (i == str_size)
						keyword_end = i;
					else
					{
						gs.state = HL_DEFAULT;
						keyword_end = i + 1;
						i += 1;
					}

					tok_size = keyword_end - keyword_start + 1;
					hl = add_str_to_hl(hl, COLORS[CURRENT_THEME+COMMENT_COLOR], 0);
					hl = add_str_to_hl(hl, line+keyword_start, tok_size);
					hl = add_str_to_hl(hl, RESET_COLOR, 4);
					continue;
				}
			}
			break;

			/* Preprocessor. */
			case HL_PREPROCESSOR:
			{
				if (!isspace(line[i]))
				{
					char temp[7 + 1];

					/*
					 * Maybe include?
					 * 6 = nclude, chars remaining.
					 */
					if (line[i] == 'i' && i+6 < str_size)
					{
						memcpy(temp, line+i, 7);
						temp[7] = '\0';

						/* If include, lets set or include state. */
						if (!strcmp(temp, "include"))
						{
							gs.state = HL_PREPROCESSOR_INCLUDE;
							i += 6;
							continue;
						}
					}
				}

				if (i == str_size-1)
				{
					gs.state = HL_DEFAULT;
					keyword_end = i;
					tok_size = keyword_end - keyword_start + 1;

					hl = add_str_to_hl(hl, COLORS[CURRENT_THEME+PREPROC_COLOR], 0);
					hl = add_str_to_hl(hl, line+keyword_start, tok_size);
					hl = add_str_to_hl(hl, RESET_COLOR, 4);
				}
			}
			break;

			/*
			 * Preprocessor/Preprocessor include
			 *
			 * This is a 'dumb' preprocessor highlighter:
			 * it highlights everything with the same color
			 * and if and only if an '#include' is detected
			 * the included header will be handled as string
			 * and thus, will have the same color as the string.
			 *
			 * In fact, it is somehow similar to what GtkSourceView
			 * does (Mousepad, Gedit...) but with one silly difference:
			 * single-line/multi-line comments will not be handled
			 * while inside the preprocessor state, meaning that
			 * comments will also have the same color as the remaining
			 * of the line, yeah, ugly.
			 */
			case HL_PREPROCESSOR_INCLUDE:
			case HL_PREPROCESSOR_INCLUDE_STRING:
			{
				/* If start of string, it means that we advanced enough
				 * to now colorify the '#include' keyword. */
				if (gs.state == HL_PREPROCESSOR_INCLUDE)
				{
					if (line[i] == '<' || line[i] == '"' || i == str_size)
					{
						tok_size = i - keyword_start;
						hl = add_str_to_hl(hl, COLORS[CURRENT_THEME+PREPROC_COLOR], 0);
						hl = add_str_to_hl(hl, line+keyword_start, tok_size);
						hl = add_str_to_hl(hl, RESET_COLOR, 4);
						keyword_start = i;
						gs.state = HL_PREPROCESSOR_INCLUDE_STRING;
					}
					continue;
				}

				/* End of string. */
				if (line[i] == '>' || line[i] == '"' || i == str_size)
				{
					keyword_end = i;
					tok_size = keyword_end - keyword_start + 1;
					gs.state = HL_DEFAULT;

					hl = add_str_to_hl(hl, COLORS[CURRENT_THEME+STRING_COLOR], 0);
					hl = add_str_to_hl(hl, line+keyword_start, tok_size);
					hl = add_str_to_hl(hl, RESET_COLOR, 4);
					continue;
				}
			}
			break;

			default:
				break;
		}
	}
	return (hl);
}

/**
 * Safe string-to-int routine that takes into account:
 * - Overflow and Underflow
 * - No undefined behaviour
 *
 * Taken from https://stackoverflow.com/a/12923949/3594716
 * and slightly adapted: no error classification, because
 * I dont need to know, error is error.
 *
 * @param out Pointer to integer.
 * @param s String to be converted.
 *
 * @return Returns 0 if success and a negative number otherwise.
 */
int str2int(int *out, char *s)
{
	char *end;
	if (s[0] == '\0' || isspace(s[0]))
		return (-1);
    errno = 0;

    long l = strtol(s, &end, 10);

    /* Both checks are needed because INT_MAX == LONG_MAX is possible. */
    if (l > INT_MAX || (errno == ERANGE && l == LONG_MAX))
        return (-1);
    if (l < INT_MIN || (errno == ERANGE && l == LONG_MIN))
        return (-1);
    if (*end != '\0')
        return (-1);

    *out = l;
    return (0);
}

/**
 * Initialize the syntax highlight engine.
 *
 * @param theme_file Theme file, if NULL, will use
 * the internal theme.
 *
 * @return Returns 0 if success and 1 otherwise.
 */
int highlight_init(const char *theme_file)
{
	int num;        /* Integer color.        */
	int idx;        /* Color vector index.   */
	FILE *fp;       /* File pointer.         */
	char *file;     /* File buffer.          */
	char *str_num;  /* String number buffer. */
	size_t kw_size; /* Keyword size.         */
	long file_size; /* Theme file size.      */

	kw_size = sizeof(keywords_list)/sizeof(struct keyword);

	/* Configure themes. */
	if (theme_file != NULL)
	{
		char *p;  /* strtok pointer. */
		idx = 0;

		fp = fopen(theme_file, "r");
		if (fp == NULL)
		{
			fprintf(stderr, "highlight: cannot open the theme file %s, is it"
				" really exists?\n", theme_file);
			return (-1);
		}

		/* File size. */
		fseek(fp, 0, SEEK_END);
		file_size = ftell(fp);
		fseek(fp, 0, SEEK_SET);

		/* Allocate and read the file. */
		file = malloc(sizeof(char) * (file_size+1));
		if (fread(file, file_size, 1, fp) != 1)
		{
			fprintf(stderr, "highlight: an error has ocurred while"
				" trying to read %s!\n", theme_file);

			free(file);
			fclose(fp);
			return (-1);
		}

		file[file_size] = '\0';
		p = file;

		/* Parse each number. */
		for (str_num = strtok(p, ", \t\n"); str_num != NULL && idx < 8;
			str_num = strtok(NULL, ", \t\n"), idx++ )
		{
			char *color;

			if (str2int(&num, str_num) < 0 || num < 0 || num > 255)
			{
				fprintf(stderr, "highlight: cannot proceed, invalid"
					" number: %s, valid numbers must be between 0-255\n", str_num);
				break;
			}

			CURRENT_THEME = USER_DEF;

			/*
			 * Color string size:
			 * 12 = length of \033[38;5; + dddm + '\0'
			 *
			 * Moreover, we have ensured that num is at most 3 chars, i.e: 255,
			 * so there is no worries about sprintf =).
			 */
			color = malloc(sizeof(char) * 12);
			sprintf(color, "\033[38;5;%dm", num);

			/* Copy to the list. */
			COLORS[CURRENT_THEME+idx] = color;
		}

		/* If something goes wrong. */
		if (idx != 8)
		{
			/* Deallocate previously configured pointers. */
			for (int i = 0; i < idx; i++)
				free(COLORS[CURRENT_THEME+i]);

			fprintf(stderr, "highlight: wrong theme, maybe a wrong number of colors? (%d/8)\n"
				"           colors should be exactly 8 and between 0-255!\n", idx);

			free(file);
			fclose(fp);
			return (-1);
		}

		free(file);
		fclose(fp);
	}

	/* Initialize hashtable. */
	hashtable_init(&ht_keywords, hashtable_sdbm_setup);

	for (size_t i = 0; i < kw_size; i++)
		hashtable_add(&ht_keywords, keywords_list[i].keyword,
			&keywords_list[i]);

	return (0);
}

/**
 * Finishes the highlight 'engine'.
 */
void highlight_finish(void)
{
	/* Finish hashtable. */
	hashtable_finish(&ht_keywords, 0);

	/* If user-defined theme. */
	if (CURRENT_THEME == USER_DEF)
	{
		for (int i = 0; i < 8; i++)
			free(COLORS[CURRENT_THEME+i]);

		CURRENT_THEME = ELF_DEITY;
	}
}


#if RUN_STANDALONE == 1
/**
 * Show usae.
 *
 * @param code Return code.
 */
void usage(int code)
{
	printf("Usage: highlight [file-name] [options]\n");
	printf("Options:\n");
	printf("-h --help          Show options available\n");
	printf("-t --theme [file]  Set a theme file\n");
	exit(code);
}

/**
 * Test
 */
int main(int argc, char **argv)
{
	FILE *fp;                /* File pointer.               */
	char *line;              /* Current line.               */
	size_t len;              /* Allocated size.             */
	ssize_t read;            /* Bytes read.                 */
	char *hl;                /* Currently highlighted line. */
	char *buff;              /* Buffer to be dumped.        */
	int option;              /* Current option.             */
	struct optparse options; /* Optparse options.           */
	char *theme_file = NULL; /* Theme file.                 */
	char *targ_file = NULL;  /* Target file, if any.        */

	fp = stdin;

	if (argc > 1)
	{
		/* Current arguments list. */
		struct optparse_long longopts[] = {
			{"help",   'h',   OPTPARSE_NONE},
			{"theme",  't',   OPTPARSE_REQUIRED},
			{0,0,0}
		};

		optparse_init(&options, argv);
		while ((option = optparse_long(&options, longopts, NULL)) != -1)
		{
			switch (option)
			{
				case 'h':
					usage(0);
					break;
				case 't':
					theme_file = options.optarg;
					break;
				case '?':
					fprintf(stderr, "%s: %s\n\n", argv[0], options.errmsg);
					usage(1);
					break;
			}
		}

		/* Try to read the source. */
		targ_file = optparse_arg(&options);
		if (targ_file != NULL && strcmp(targ_file, "-") != 0)
		{
			fp = fopen(targ_file, "r");
			if (fp == NULL)
			{
				fprintf(stderr, "%s: cannot open the file %s, is it really exists?\n",
					argv[0], argv[1]);
				return (1);
			}
		}
	}

	line = NULL;
	buff = NULL;
	len  = 0;

	if (highlight_init(theme_file) < 0)
	{
		if (fp != stdin)
			fclose(fp);
		return (-1);
	}

	hl = highlight_alloc_line();
	buff = highlight_alloc_line();

	/* Read the entire file. */
	while ((read = getline(&line, &len, fp)) != -1)
	{
		/* Remove line break. */
		line[strlen(line)-1] = '\0';

		/* Highlight. */
		hl = highlight_line(line, hl);

		/* Replace '\0' to '\n'. */
		hl[strlen(hl)] = '\n';

		/* Add '\0' into our string. */
		hl = add_char_to_hl(hl, '\0');

		/* Add our string into our buffer. */
		buff = add_str_to_hl(buff, hl, 0 );
	}

	buff = add_char_to_hl(buff, '\0');
	puts(buff);

	highlight_free(hl);
	highlight_free(buff);
	free(line);

	if (fp != stdin)
		fclose(fp);

	highlight_finish();
}
#endif /* RUN_STANDALONE. */
