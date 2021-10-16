/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/** \file
 * \ingroup sptext
 */

#include <string.h>

#include "BLI_blenlib.h"

#include "DNA_space_types.h"
#include "DNA_text_types.h"

#include "BKE_text.h"

#include "text_format.h"

/* *** Local Functions (for format_line) *** */

static int txtfmt_hy_find_builtinfunc(const char *string)
{
  int i, len;

  /* Keep aligned args for readability. */
  /* clang-format off */

  if        (STR_LITERAL_STARTSWITH(string, ")",                 len)) { i = len;
  } else                                                               { i = 0;
  }

  /* clang-format on */

  /* If next source char is an identifier (eg. 'i' in "definite") no match */
  if (i == 0 || text_check_identifier(string[i])) {
    return -1;
  }
  return i;
}

static int txtfmt_hy_find_reserved(const char *string)
{
  int i, len;

  /* Keep aligned args for readability. */
  /* clang-format off */

  if        (STR_LITERAL_STARTSWITH(string, "True",      len)) { i = len;
  } else if (STR_LITERAL_STARTSWITH(string, "False",     len)) { i = len;
  } else if (STR_LITERAL_STARTSWITH(string, "None",      len)) { i = len;
  } else                                                       { i = 0;
  }

  /* clang-format on */

  /* If next source char is an identifier (eg. 'i' in "definite") no match */
  if (i == 0 || text_check_identifier(string[i]) || string[i] == '-') {
    return -1;
  }
  return i;
}

/* Checks the specified source string for a hy special name. This name must
 * start at the beginning of the source string and must be followed by a non-
 * identifier (see text_check_identifier(char)) or null character.
 *
 * If a special name is found, the length of the matching name is returned.
 * Otherwise, -1 is returned. */

static int txtfmt_hy_find_specialvar(const char *string)
{
  int i, len;

  /* Keep aligned args for readability. */
  /* clang-format off */

  /* hy shader types */
  if        (STR_LITERAL_STARTSWITH(string, "&optional", len)) { i = len;
  } else if (STR_LITERAL_STARTSWITH(string, "&rest",     len)) { i = len;
  } else if (STR_LITERAL_STARTSWITH(string, "&kwonly",   len)) { i = len;
  } else if (STR_LITERAL_STARTSWITH(string, "&kwargs",   len)) { i = len;
  } else if (STR_LITERAL_STARTSWITH(string, "#@",        len)) { i = len;
  } else if (STR_LITERAL_STARTSWITH(string, "#*",        len)) { i = len;
  } else if (STR_LITERAL_STARTSWITH(string, "#**",       len)) { i = len;
  } else if (STR_LITERAL_STARTSWITH(string, "~",         len)) { i = len;
  } else if (STR_LITERAL_STARTSWITH(string, "@",         len)) { i = len;
  } else if (STR_LITERAL_STARTSWITH(string, "`",         len)) { i = len;
  } else                                                          { i = 0;
  }

  /* clang-format on */

  /* If next source char is an identifier (eg. 'i' in "definite") no match */
  if (i == 0 || text_check_identifier(string[i])) {
    return -1;
  }
  return i;
}

/* Type hint */
static int txtfmt_hy_find_preprocessor(const char *string)
{
  if (string[0] == '^') {
    int i = 1;
    /*                                         Dot is ok '^kek.bur' */
    while (text_check_identifier(string[i]) || string[i] == '.') {
      i++;
    }
    return i;
  }
  return -1;
}

static bool hy_text_check_delim(const char ch)
{
  int a;
  char delims[] = "\"\' []{}.\t";

  for (a = 0; a < (sizeof(delims) - 1); a++) {
    if (ch == delims[a]) {
      return true;
    }
  }
  return false;
}

static bool hy_text_check_wrap_delim(const char ch) {
  int a;
  char delims[] = "()[]{}";

  for (a = 0; a < (sizeof(delims) - 1); a++) {
    if (ch == delims[a]) {
      return true;
    }
  }
  return false;
}

/* Function call - Check for opening paren with any word/symbol.
    v------v
    (kek.bur lol) <- force exit on closing paren.
*/
static int txtfmt_hy_find_funcall(const char *string)
{
  
  if (string[0] == '(') {
    int i = 1;
    while(string[i] >= '!') {
        i++;
        if (string[i] == ')') {
          break;
        }
    }
    return i;
  }
  return -1;
}

/* Tag call - Check for hashtag with any word/symbol.
    #kek 
    force exit if a delimiter is found.
    ----v
    #kek(bur)
*/
static int txtfmt_hy_find_tagcall(const char *string)
{ 
  if (string[0] == '#') {
    int i = 1;
    while(string[i] >= '!') {
        if (hy_text_check_wrap_delim(string[i])) {
          return i;
        }
        i++;
    }
    return i;
  }
  return -1;
}

/* Keyword
    :kek
*/
static int txtfmt_hy_find_keyword(const char *string)
{
  if (string[0] == ':') {
    int i = 1;
    while(string[i] >= '!') {
        i++;
    }
    return i;
  }
  return -1;
}

static char txtfmt_hy_format_identifier(const char *str)
{
  char fmt;

  /* Keep aligned args for readability. */
  /* clang-format off */

  if        ((txtfmt_hy_find_specialvar(str))   != -1) { fmt = FMT_TYPE_SPECIAL;
  } else if ((txtfmt_hy_find_builtinfunc(str))  != -1) { fmt = FMT_TYPE_KEYWORD;
  } else if ((txtfmt_hy_find_reserved(str))     != -1) { fmt = FMT_TYPE_RESERVED;
  } else if ((txtfmt_hy_find_funcall(str))      != -1) { fmt = FMT_TYPE_KEYWORD;
  } else if ((txtfmt_hy_find_tagcall(str))      != -1) { fmt = FMT_TYPE_SPECIAL;
  } else if ((txtfmt_hy_find_keyword(str))      != -1) { fmt = FMT_TYPE_SPECIAL;
  } else if ((txtfmt_hy_find_preprocessor(str)) != -1) { fmt = FMT_TYPE_DIRECTIVE;
  } else                                               { fmt = FMT_TYPE_DEFAULT;
  }

  /* clang-format on */

  return fmt;
}



int paren_search = -1;
char bracket_type_open;
char bracket_type_close;


static void txtfmt_hy_format_line(SpaceText *st, TextLine *line, const bool do_next)
{
  FlattenString fs;
  const char *str;
  char *fmt;
  char cont_orig, cont, find, prev = ' ';
  int len, i;

  /* Get continuation from previous line */
  if (line->prev && line->prev->format != NULL) {
    fmt = line->prev->format;
    cont = fmt[strlen(fmt) + 1]; /* Just after the null-terminator */
    BLI_assert((FMT_CONT_ALL & cont) == cont);
  }
  else {
    cont = FMT_CONT_NOP;
  }

  /* Get original continuation from this line */
  if (line->format != NULL) {
    fmt = line->format;
    cont_orig = fmt[strlen(fmt) + 1]; /* Just after the null-terminator */
    BLI_assert((FMT_CONT_ALL & cont_orig) == cont_orig);
  }
  else {
    cont_orig = 0xFF;
  }

  len = flatten_string(st, &fs, line->line);
  str = fs.buf;
  if (!text_check_format_len(line, len)) {
    flatten_string_free(&fs);
    return;
  }
  fmt = line->format;

  while (*str) {
    /* Handle escape sequences by skipping both \ and next char */
    if (*str == '\\') {
      *fmt = prev;
      fmt++;
      str++;
      if (*str == '\0') {
        break;
      }
      *fmt = prev;
      fmt++;
      str += BLI_str_utf8_size_safe(str);
      continue;
    }
    /* Handle continuations */
    else if (cont) {
      /* Hy Contextual comments */
      if (cont & FMT_CONT_COMMENT_C) {
        if (*str == bracket_type_close) {
            paren_search --;
            *fmt = FMT_TYPE_COMMENT;
            /*Terminate contextual comment*/
            if (paren_search == 0) {
                cont = FMT_CONT_NOP;
                paren_search = -1;
            }
        } 
        else if (*str == bracket_type_open) {
            paren_search++;
            *fmt = FMT_TYPE_COMMENT;
        }
        else {
          *fmt = FMT_TYPE_COMMENT;
        }
        /* Handle other comments */
      }
      else {
        find = (cont & FMT_CONT_QUOTEDOUBLE) ? '"' : '\'';
        if (*str == find) {
          cont = 0;
        }
        *fmt = FMT_TYPE_STRING;
      }

      str += BLI_str_utf8_size_safe(str) - 1;
    }
    /* Not in a string... */
    else {
      /* Deal with comments first */
      if (*str == ';') {
        /* fill the remaining line */
        text_format_fill(&str, &fmt, FMT_TYPE_COMMENT, len - (int)(fmt - line->format));
      }
      /* Hy Contextual comments */
     else if (*str == '(' && *(str + 1) == 'c' 
                          && *(str + 2) == 'o' 
                          && *(str + 3) == 'm' 
                          && *(str + 4) == 'm' 
                          && *(str + 5) == 'e' 
                          && *(str + 6) == 'n' 
                          && *(str + 7) == 't') {
        cont = FMT_CONT_COMMENT_C;
        *fmt = FMT_TYPE_COMMENT;
        fmt++;
        str++;
        *fmt = FMT_TYPE_COMMENT;
        fmt++;
        str++;
        *fmt = FMT_TYPE_COMMENT;
        fmt++;
        str++;
        *fmt = FMT_TYPE_COMMENT;
        fmt++;
        str++;
        *fmt = FMT_TYPE_COMMENT;
        fmt++;
        str++;
        *fmt = FMT_TYPE_COMMENT;
        fmt++;
        str++;
        *fmt = FMT_TYPE_COMMENT;

        paren_search = 1;
        bracket_type_open  = '(';
        bracket_type_close = ')';
      }
      else if (*str == '#' && *(str + 1) == '_' && *(str + 2) == '(') {
        cont = FMT_CONT_COMMENT_C;
        *fmt = FMT_TYPE_COMMENT;
        fmt++;
        str++;
        *fmt = FMT_TYPE_COMMENT;
        fmt++;
        str++;
        *fmt = FMT_TYPE_COMMENT;

        paren_search = 1;
        bracket_type_open  = '(';
        bracket_type_close = ')';
      }
      else if (*str == '#' && *(str + 1) == '_' && *(str + 2) == '[') {
        cont = FMT_CONT_COMMENT_C;
        *fmt = FMT_TYPE_COMMENT;
        fmt++;
        str++;
        *fmt = FMT_TYPE_COMMENT;
        fmt++;
        str++;
        *fmt = FMT_TYPE_COMMENT;

        paren_search = 1;
        bracket_type_open  = '[';
        bracket_type_close = ']';
      }
      else if (*str == '#' && *(str + 1) == '_' && *(str + 2) == '{') {
        cont = FMT_CONT_COMMENT_C;
        *fmt = FMT_TYPE_COMMENT;
        fmt++;
        str++;
        *fmt = FMT_TYPE_COMMENT;
        fmt++;
        str++;
        *fmt = FMT_TYPE_COMMENT;

        paren_search = 1;
        bracket_type_open  = '{';
        bracket_type_close = '}';
      }
      else if (*str == '#' && *(str + 1) == '_' && *(str + 2) == '#' && *(str + 3) == '{') {
        cont = FMT_CONT_COMMENT_C;
        *fmt = FMT_TYPE_COMMENT;
        fmt++;
        str++;
        *fmt = FMT_TYPE_COMMENT;
        fmt++;
        str++;
        *fmt = FMT_TYPE_COMMENT;
        fmt++;
        str++;
        *fmt = FMT_TYPE_COMMENT;

        paren_search = 1;
        bracket_type_open  = '{';
        bracket_type_close = '}';
      }
      else if (*str == '"') {
        /* Strings */
        find = *str;
        //cont = (*str == '"') ? FMT_CONT_QUOTEDOUBLE : FMT_CONT_QUOTESINGLE;
        cont = FMT_CONT_QUOTEDOUBLE;
        *fmt = FMT_TYPE_STRING;
      }
      /* Whitespace (all ws. has been converted to spaces) */
      else if (*str == ' ') {
        *fmt = FMT_TYPE_WHITESPACE;
      }
      /* Numbers (special case where number is part of symbol combined with '-') */
      else if (prev == '-' && text_check_digit(*str)) {
          *fmt = FMT_TYPE_SYMBOL;
      }
      /* Numbers (digits not part of an identifier and periods followed by digits) */
      else if ((prev != FMT_TYPE_DEFAULT && text_check_digit(*str)) ||
               (*str == '.' && text_check_digit(*(str + 1)))) {
        *fmt = FMT_TYPE_NUMERAL;
      }
      /* Punctuation */
      else if (hy_text_check_delim(*str)) {
        *fmt = FMT_TYPE_SYMBOL;
      } 
      /* Closing Paren (Assign to KEYWORD) */
      else if (*str == ')') {
        *fmt = FMT_TYPE_KEYWORD;
      }
      /* Literal Set (Assign to KEYWORD) */
      else if (*str == '#' && *(str + 1) == '{') {
        *fmt = FMT_TYPE_SYMBOL;
      }
      /* Identifiers and other text (no previous ws. or delims. so text continues) */
      else if (prev == FMT_TYPE_DEFAULT) {
        str += BLI_str_utf8_size_safe(str) - 1;
        *fmt = FMT_TYPE_DEFAULT;
      }
      /* Not ws, a digit, punct, or continuing text. Must be new, check for special words */
      else {
        /* Keep aligned args for readability. */
        /* clang-format off */

        /* Special vars(v) or built-in keywords(b) */
        /* keep in sync with 'txtfmt_hy_format_identifier()' */
        if        ((i = txtfmt_hy_find_specialvar(str))   != -1) { prev = FMT_TYPE_SPECIAL;
        } else if ((i = txtfmt_hy_find_builtinfunc(str))  != -1) { prev = FMT_TYPE_KEYWORD;
        } else if ((i = txtfmt_hy_find_reserved(str))     != -1) { prev = FMT_TYPE_RESERVED;
        } else if ((i = txtfmt_hy_find_funcall(str))      != -1) { prev = FMT_TYPE_KEYWORD;
        } else if ((i = txtfmt_hy_find_tagcall(str))      != -1) { prev = FMT_TYPE_SPECIAL;
        } else if ((i = txtfmt_hy_find_keyword(str))      != -1) { prev = FMT_TYPE_SPECIAL;
        } else if ((i = txtfmt_hy_find_preprocessor(str)) != -1) { prev = FMT_TYPE_DIRECTIVE;
        }

        /* clang-format on */

        if (i > 0) {
          if (prev == FMT_TYPE_DIRECTIVE) { /* can contain utf8 */
            text_format_fill(&str, &fmt, prev, i);
          }
          else if (prev == FMT_TYPE_KEYWORD) { /* can contain utf8 */
            text_format_fill(&str, &fmt, prev, i);
          }
          else {
            text_format_fill_ascii(&str, &fmt, prev, i);
          }
        }
        else {
          str += BLI_str_utf8_size_safe(str) - 1;
          *fmt = FMT_TYPE_DEFAULT;
        }
      }
    }
    prev = *fmt;
    fmt++;
    str++;
  }

  /* Terminate and add continuation char */
  *fmt = '\0';
  fmt++;
  *fmt = cont;

  /* If continuation has changed and we're allowed, process the next line */
  if (cont != cont_orig && do_next && line->next) {
    txtfmt_hy_format_line(st, line->next, do_next);
  }

  flatten_string_free(&fs);
}

void ED_text_format_register_hy(void)
{
  static TextFormatType tft = {NULL};
  static const char *ext[] = {"hy", NULL};

  tft.format_identifier = txtfmt_hy_format_identifier;
  tft.format_line = txtfmt_hy_format_line;
  tft.ext = ext;

  ED_text_format_register(&tft);
}
