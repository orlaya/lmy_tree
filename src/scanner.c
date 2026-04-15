#include "tree_sitter/parser.h"

enum TokenType {
  FOLD_OPEN,           // <<
  FOLD_CLOSE,          // >>
  PRESERVE_DELIMITER,  // ||
  VARIABLE_DOLLAR,     // $
  VARIABLE_QUALIFIER,  // namespace segment before dot (sv in $sv.FOO)
  VARIABLE_SEGMENT,    // final identifier segment (FOO in $sv.FOO, or BAR in $BAR)
  VARIABLE_DOT,        // . between segments
  KEY,                 // assignment key (wider charset than identifier)
  VERSION_PREFIX,      // ^ or ~
  VERSION_DIGITS,      // digit runs (1, 0, 3)
  VERSION_DOT,         // . between digit groups
  VERSION_DASH,        // - before pre-release tag
  VERSION_TAG,         // pre-release tag (beta, f92627f, etc)
  NUMBER,              // 34 or 45.70
  ERROR_SENTINEL,
};

static bool is_segment_start(int32_t c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static bool is_segment_char(int32_t c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
         (c >= '0' && c <= '9') || c == '_' || c == '@' || c == '-';
}

static bool is_digit(int32_t c) {
  return c >= '0' && c <= '9';
}

static bool is_version_tag_char(int32_t c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
         (c >= '0' && c <= '9') || c == '_';
}

static bool is_key_start(int32_t c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c == '#' ||
         c == '*' || c == '.' || c == '@';
}

static bool is_key_char(int32_t c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
         (c >= '0' && c <= '9') || c == '_' || c == '@' || c == '#' ||
         c == '/' || c == '.' || c == '-' || c == '*';
}

void *tree_sitter_lmy_external_scanner_create() { return NULL; }
void tree_sitter_lmy_external_scanner_destroy(void *payload) {}
unsigned tree_sitter_lmy_external_scanner_serialize(void *payload, char *buffer) { return 0; }
void tree_sitter_lmy_external_scanner_deserialize(void *payload, const char *buffer, unsigned length) {}

bool tree_sitter_lmy_external_scanner_scan(
  void *payload,
  TSLexer *lexer,
  const bool *valid_symbols
) {
  if (valid_symbols[ERROR_SENTINEL]) return false;

  // ── Variable internals: no whitespace skip (must be adjacent) ──

  if (valid_symbols[VARIABLE_DOT] && lexer->lookahead == '.') {
    lexer->advance(lexer, false);
    lexer->mark_end(lexer);
    if (is_segment_start(lexer->lookahead)) {
      lexer->result_symbol = VARIABLE_DOT;
      return true;
    }
    return false;
  }

  if ((valid_symbols[VARIABLE_SEGMENT] || valid_symbols[VARIABLE_QUALIFIER]) &&
      is_segment_start(lexer->lookahead)) {
    lexer->advance(lexer, false);
    while (is_segment_char(lexer->lookahead)) {
      lexer->advance(lexer, false);
    }
    lexer->mark_end(lexer);

    // Colon-separated parts (gist:check) — colon is part of segment
    // only if followed by another identifier start
    while (lexer->lookahead == ':') {
      lexer->advance(lexer, false);
      if (is_segment_start(lexer->lookahead)) {
        lexer->advance(lexer, false);
        while (is_segment_char(lexer->lookahead)) {
          lexer->advance(lexer, false);
        }
        lexer->mark_end(lexer);
      } else {
        break;
      }
    }

    // Peek ahead: if `.` + segment_start follows, this is a qualifier (namespace),
    // not the final segment
    if (valid_symbols[VARIABLE_QUALIFIER] && lexer->lookahead == '.') {
      lexer->advance(lexer, false);
      if (is_segment_start(lexer->lookahead)) {
        lexer->result_symbol = VARIABLE_QUALIFIER;
        return true;
      }
    }

    lexer->result_symbol = VARIABLE_SEGMENT;
    return true;
  }

  // ── Version internals: no whitespace skip (must be adjacent) ──

  if (valid_symbols[VERSION_DOT] && lexer->lookahead == '.') {
    lexer->advance(lexer, false);
    lexer->mark_end(lexer);
    if (is_digit(lexer->lookahead) || is_version_tag_char(lexer->lookahead)) {
      lexer->result_symbol = VERSION_DOT;
      return true;
    }
    return false;
  }

  if (valid_symbols[VERSION_DASH] && lexer->lookahead == '-') {
    lexer->advance(lexer, false);
    lexer->mark_end(lexer);
    if (is_version_tag_char(lexer->lookahead)) {
      lexer->result_symbol = VERSION_DASH;
      return true;
    }
    return false;
  }

  if (valid_symbols[VERSION_TAG] && is_version_tag_char(lexer->lookahead)) {
    while (is_version_tag_char(lexer->lookahead)) {
      lexer->advance(lexer, false);
    }
    lexer->mark_end(lexer);
    lexer->result_symbol = VERSION_TAG;
    return true;
  }

  // ── Whitespace skip for remaining tokens ──

  if (lexer->eof(lexer)) return false;
  uint32_t column_before_skip = lexer->get_column(lexer);

  while (lexer->lookahead == ' ' || lexer->lookahead == '\t') {
    lexer->advance(lexer, true);
  }

  if (valid_symbols[KEY] && column_before_skip == 0) {
    // Quoted key: "something":
    if (lexer->lookahead == '"') {
      lexer->advance(lexer, false);
      while (lexer->lookahead != '"' && lexer->lookahead != '\n' && !lexer->eof(lexer)) {
        lexer->advance(lexer, false);
      }
      if (lexer->lookahead != '"') return false;
      lexer->advance(lexer, false);
      lexer->mark_end(lexer);
      if (lexer->lookahead == ':') {
        lexer->advance(lexer, false);
        if (lexer->lookahead != ':') {
          lexer->result_symbol = KEY;
          return true;
        }
      }
      return false;
    }

    // Bare key: something:
    if (is_key_start(lexer->lookahead)) {
      lexer->advance(lexer, false);
      while (is_key_char(lexer->lookahead)) {
        lexer->advance(lexer, false);
      }
      lexer->mark_end(lexer);

      bool found_trailing_colon = false;
      while (lexer->lookahead == ':') {
        lexer->advance(lexer, false);
        if (is_key_start(lexer->lookahead)) {
          lexer->advance(lexer, false);
          while (is_key_char(lexer->lookahead)) {
            lexer->advance(lexer, false);
          }
          lexer->mark_end(lexer);
        } else {
          found_trailing_colon = true;
          break;
        }
      }

      if (found_trailing_colon && lexer->lookahead != ':') {
        lexer->result_symbol = KEY;
        return true;
      }
      return false;
    }
  }

  if (valid_symbols[VERSION_PREFIX] && (lexer->lookahead == '^' || lexer->lookahead == '~')) {
    lexer->advance(lexer, false);
    lexer->mark_end(lexer);
    if (is_digit(lexer->lookahead)) {
      lexer->result_symbol = VERSION_PREFIX;
      return true;
    }
    return false;
  }

  if ((valid_symbols[VERSION_DIGITS] || valid_symbols[NUMBER]) && is_digit(lexer->lookahead)) {
    while (is_digit(lexer->lookahead) || lexer->lookahead == '_') {
      lexer->advance(lexer, false);
    }
    lexer->mark_end(lexer);

    // At value start (VERSION_PREFIX still valid): peek ahead to decide
    // version vs number
    if (valid_symbols[VERSION_PREFIX]) {
      if (lexer->lookahead == '.') {
        lexer->advance(lexer, false);
        if (is_digit(lexer->lookahead)) {
          while (is_digit(lexer->lookahead) || lexer->lookahead == '_') lexer->advance(lexer, false);
          if (lexer->lookahead == '.') {
            // Two dots confirmed — this is a version
            if (valid_symbols[VERSION_DIGITS]) {
              lexer->result_symbol = VERSION_DIGITS;
              return true;
            }
          }
          // One dot only — this is a decimal number (45.70)
          // mark_end to include the .digits we just consumed
          lexer->mark_end(lexer);
        }
        // Dot not followed by digit — mark_end stays at initial digits
      }

      // Number must be the whole value — reject if followed by letters (45.88kg)
      // or if followed by space+text (5 inches)
      if (valid_symbols[NUMBER] && !is_segment_start(lexer->lookahead)) {
        if (lexer->lookahead == ' ' || lexer->lookahead == '\t') {
          // Peek past whitespace to check what follows
          while (lexer->lookahead == ' ' || lexer->lookahead == '\t') {
            lexer->advance(lexer, false);
          }
          // Only emit NUMBER if line ends or comment follows
          if (lexer->lookahead == '\n' || lexer->lookahead == '\r' ||
              lexer->lookahead == '/' || lexer->eof(lexer)) {
            lexer->result_symbol = NUMBER;
            return true;
          }
        } else if (lexer->lookahead == '\n' || lexer->lookahead == '\r' ||
                   lexer->lookahead == '/' || lexer->eof(lexer)) {
          lexer->result_symbol = NUMBER;
          return true;
        }
      }
      return false;
    }

    // Inside a version (VERSION_PREFIX not valid) — emit version digits
    if (valid_symbols[VERSION_DIGITS]) {
      lexer->result_symbol = VERSION_DIGITS;
      return true;
    }

    if (valid_symbols[NUMBER]) {
      lexer->result_symbol = NUMBER;
      return true;
    }
  }

  if (valid_symbols[VARIABLE_DOLLAR] && lexer->lookahead == '$') {
    lexer->advance(lexer, false);
    lexer->mark_end(lexer);
    if (is_segment_start(lexer->lookahead)) {
      lexer->result_symbol = VARIABLE_DOLLAR;
      return true;
    }
    return false;
  }

  if (valid_symbols[FOLD_OPEN] && lexer->lookahead == '<') {
    lexer->advance(lexer, false);
    if (lexer->lookahead == '<') {
      lexer->advance(lexer, false);
      lexer->mark_end(lexer);
      lexer->result_symbol = FOLD_OPEN;
      return true;
    }
    return false;
  }

  if (valid_symbols[FOLD_CLOSE] && lexer->lookahead == '>') {
    lexer->advance(lexer, false);
    if (lexer->lookahead == '>') {
      lexer->advance(lexer, false);
      lexer->mark_end(lexer);
      lexer->result_symbol = FOLD_CLOSE;
      return true;
    }
    return false;
  }

  if (valid_symbols[PRESERVE_DELIMITER] && lexer->lookahead == '|') {
    lexer->advance(lexer, false);
    if (lexer->lookahead == '|') {
      lexer->advance(lexer, false);
      lexer->mark_end(lexer);
      lexer->result_symbol = PRESERVE_DELIMITER;
      return true;
    }
    return false;
  }

  return false;
}
