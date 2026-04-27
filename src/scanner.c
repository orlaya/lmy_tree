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
  LANGUAGE_TAG,        // sh, py, js, sql etc (before ~~)
  INJECTION_DELIMITER, // ~~
  DONE_MARKER,         // xx (done list item prefix)
  TILDE_DELIMITER,     // ~~~
  TILDE_BODY,          // content between ~~~ markers (opaque for injection)
  EMPHASIS_OPEN,               // * or ** opening — single-line (peeks for close)
  EMPHASIS_CLOSE,              // * or ** closing — single-line
  EMPHASIS_OPEN_MULTILINE,     // * or ** opening — multi-line (no peek)
  EMPHASIS_CLOSE_MULTILINE,    // * or ** closing — multi-line
  LAST_TOKEN_WHITESPACE,       // phantom — never emitted, context via valid_symbols
  LAST_TOKEN_PUNCTUATION,      // phantom — never emitted, context via valid_symbols
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

static bool is_punctuation(int32_t c) {
  return (c >= '!' && c <= '/') || (c >= ':' && c <= '@') ||
         (c >= '[' && c <= '`') || (c >= '{' && c <= '~');
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

// ── Emphasis state ──
// Current delimiter run is an opening run
static const uint8_t STATE_EMPHASIS_DELIMITER_IS_OPEN = 0x04;

typedef struct {
  uint8_t state;
  uint8_t num_emphasis_delimiters_left;
} Scanner;

void *tree_sitter_lmy_external_scanner_create() {
  Scanner *s = calloc(1, sizeof(Scanner));
  return s;
}

void tree_sitter_lmy_external_scanner_destroy(void *payload) {
  free(payload);
}

unsigned tree_sitter_lmy_external_scanner_serialize(void *payload, char *buffer) {
  Scanner *s = (Scanner *)payload;
  buffer[0] = (char)s->state;
  buffer[1] = (char)s->num_emphasis_delimiters_left;
  return 2;
}

void tree_sitter_lmy_external_scanner_deserialize(void *payload, const char *buffer, unsigned length) {
  Scanner *s = (Scanner *)payload;
  s->state = 0;
  s->num_emphasis_delimiters_left = 0;
  if (length >= 2) {
    s->state = (uint8_t)buffer[0];
    s->num_emphasis_delimiters_left = (uint8_t)buffer[1];
  }
}

// ── Emphasis open/close detection ──
// Ported from tree-sitter-markdown's parse_star.
// Context about what preceded the * comes via valid_symbols:
//   LAST_TOKEN_WHITESPACE  — grammar places optional($._last_token_whitespace)
//                            after whitespace-producing positions
//   LAST_TOKEN_PUNCTUATION — grammar places optional($._last_token_punctuation)
//                            after punctuation-producing positions
// These phantom tokens never actually match input; they just signal context.

// Peek ahead (without consuming) to check if there's a closing * on this line.
// Returns true if a matching * is found before line end.
static bool has_closing_star_on_line(TSLexer *lexer) {
  // We're positioned right after the opening star(s), looking at the first
  // content char. Scan forward for a * that isn't immediately followed by
  // whitespace (which would make it a valid right-flanking delimiter).
  while (lexer->lookahead != '\n' && lexer->lookahead != '\r' &&
         !lexer->eof(lexer)) {
    if (lexer->lookahead == '*') {
      return true;
    }
    lexer->advance(lexer, false);
  }
  return false;
}

static bool parse_emphasis(Scanner *s, TSLexer *lexer, const bool *valid_symbols) {
  bool is_multi = valid_symbols[EMPHASIS_OPEN_MULTILINE] ||
                  valid_symbols[EMPHASIS_CLOSE_MULTILINE];
  int open_sym  = is_multi ? EMPHASIS_OPEN_MULTILINE  : EMPHASIS_OPEN;
  int close_sym = is_multi ? EMPHASIS_CLOSE_MULTILINE : EMPHASIS_CLOSE;

  lexer->advance(lexer, false);

  // Continuing a delimiter run — we already decided open vs close
  if (s->num_emphasis_delimiters_left > 0) {
    if ((s->state & STATE_EMPHASIS_DELIMITER_IS_OPEN) &&
        valid_symbols[open_sym]) {
      s->state &= ~STATE_EMPHASIS_DELIMITER_IS_OPEN;
      lexer->result_symbol = open_sym;
      s->num_emphasis_delimiters_left--;
      return true;
    }
    if (valid_symbols[close_sym]) {
      lexer->result_symbol = close_sym;
      s->num_emphasis_delimiters_left--;
      return true;
    }
  }

  lexer->mark_end(lexer);

  // Count consecutive stars
  uint8_t star_count = 1;
  while (lexer->lookahead == '*') {
    star_count++;
    lexer->advance(lexer, false);
  }

  s->num_emphasis_delimiters_left = star_count - 1;

  bool line_end = lexer->lookahead == '\n' || lexer->lookahead == '\r' ||
                  lexer->eof(lexer);
  bool next_whitespace = line_end || lexer->lookahead == ' ' ||
                         lexer->lookahead == '\t';
  bool next_punctuation = is_punctuation(lexer->lookahead);

  // Closing (right-flanking): previous token was NOT whitespace, and either
  // previous wasn't punctuation, or next is punctuation/whitespace.
  // Close takes precedence over open.
  if (valid_symbols[close_sym] &&
      !valid_symbols[LAST_TOKEN_WHITESPACE] &&
      (!valid_symbols[LAST_TOKEN_PUNCTUATION] ||
       next_punctuation || next_whitespace)) {
    s->state &= ~STATE_EMPHASIS_DELIMITER_IS_OPEN;
    lexer->result_symbol = close_sym;
    return true;
  }

  // Opening (left-flanking): next char is not whitespace, and either next
  // is not punctuation, or previous was punctuation/whitespace.
  if (valid_symbols[open_sym] &&
      !next_whitespace &&
      (!next_punctuation ||
       valid_symbols[LAST_TOKEN_PUNCTUATION] ||
       valid_symbols[LAST_TOKEN_WHITESPACE])) {
    // Single-line: only commit if there's a closing * on this line.
    // Multi-line: always commit (close can be on a later line).
    if (!is_multi && !has_closing_star_on_line(lexer)) {
      s->num_emphasis_delimiters_left = 0;
      return false;
    }
    s->state |= STATE_EMPHASIS_DELIMITER_IS_OPEN;
    lexer->result_symbol = open_sym;
    return true;
  }

  return false;
}

bool tree_sitter_lmy_external_scanner_scan(
  void *payload,
  TSLexer *lexer,
  const bool *valid_symbols
) {
  Scanner *s = (Scanner *)payload;

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

  // ── Tilde body: consume everything between ~~~ markers ──
  // Must be before whitespace skip — body includes its own whitespace.

  if (valid_symbols[TILDE_BODY]) {
    // We're right after the grammar matched the opening ~~~ and its newline.
    // Consume lines until we find one that is just ~~~.
    bool has_content = false;

    while (!lexer->eof(lexer)) {
      // Mark at start of each line — if this line is the closing ~~~,
      // the body ends here (just before this line).
      lexer->mark_end(lexer);

      // Skip leading whitespace
      while (lexer->lookahead == ' ' || lexer->lookahead == '\t') {
        lexer->advance(lexer, false);
      }

      // Check for closing ~~~
      if (lexer->lookahead == '~') {
        lexer->advance(lexer, false);
        if (lexer->lookahead == '~') {
          lexer->advance(lexer, false);
          if (lexer->lookahead == '~') {
            lexer->advance(lexer, false);
            // Exactly three tildes (not four+)
            if (lexer->lookahead != '~') {
              if (has_content) {
                lexer->result_symbol = TILDE_BODY;
                return true;
              }
              return false;
            }
          }
        }
      }

      // Not the closing ~~~ — consume rest of line
      while (lexer->lookahead != '\n' && lexer->lookahead != '\r' && !lexer->eof(lexer)) {
        lexer->advance(lexer, false);
      }
      if (lexer->lookahead == '\r') lexer->advance(lexer, false);
      if (lexer->lookahead == '\n') lexer->advance(lexer, false);

      has_content = true;
    }

    return false;
  }

  // ── Whitespace skip for remaining tokens ──

  if (lexer->eof(lexer)) return false;
  uint32_t column_before_skip = lexer->get_column(lexer);

  while (lexer->lookahead == ' ' || lexer->lookahead == '\t') {
    lexer->advance(lexer, true);
  }

  if ((valid_symbols[KEY] || valid_symbols[DONE_MARKER]) && column_before_skip == 0) {
    // Quoted key: "something":
    if (valid_symbols[KEY] && lexer->lookahead == '"') {
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

    // Bare key or done marker — both start with alpha at column 0.
    // Read the identifier, then decide: trailing colon → KEY, exactly "xx" + whitespace → DONE_MARKER.
    if (is_key_start(lexer->lookahead)) {
      int32_t first_char = lexer->lookahead;
      int char_count = 0;
      int32_t second_char = 0;

      lexer->advance(lexer, false);
      char_count++;
      if (char_count == 1) second_char = lexer->lookahead;

      while (is_key_char(lexer->lookahead)) {
        lexer->advance(lexer, false);
        char_count++;
      }
      lexer->mark_end(lexer);

      // Try KEY first: look for trailing colon
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
        if (valid_symbols[KEY]) {
          lexer->result_symbol = KEY;
          return true;
        }
      }

      // Not a key — check for done marker: exactly "xx" followed by whitespace
      if (valid_symbols[DONE_MARKER] &&
          char_count == 2 && first_char == 'x' && second_char == 'x' &&
          (lexer->lookahead == ' ' || lexer->lookahead == '\t')) {
        lexer->result_symbol = DONE_MARKER;
        return true;
      }

      return false;
    }
  }

  // ── Emphasis open/close ──
  if ((valid_symbols[EMPHASIS_OPEN] || valid_symbols[EMPHASIS_CLOSE] ||
       valid_symbols[EMPHASIS_OPEN_MULTILINE] || valid_symbols[EMPHASIS_CLOSE_MULTILINE]) &&
      lexer->lookahead == '*') {
    return parse_emphasis(s, lexer, valid_symbols);
  }

  if (valid_symbols[TILDE_DELIMITER] && lexer->lookahead == '~') {
    lexer->advance(lexer, false);
    if (lexer->lookahead == '~') {
      lexer->advance(lexer, false);
      if (lexer->lookahead == '~') {
        lexer->advance(lexer, false);
        lexer->mark_end(lexer);
        // Exactly three tildes — not four+
        if (lexer->lookahead != '~') {
          lexer->result_symbol = TILDE_DELIMITER;
          return true;
        }
      }
    }
    return false;
  }

  if (valid_symbols[LANGUAGE_TAG] && lexer->lookahead >= 'a' && lexer->lookahead <= 'z') {
    lexer->advance(lexer, false);
    while (lexer->lookahead >= 'a' && lexer->lookahead <= 'z') {
      lexer->advance(lexer, false);
    }
    lexer->mark_end(lexer);
    if (lexer->lookahead == '~') {
      lexer->advance(lexer, false);
      if (lexer->lookahead == '~') {
        lexer->result_symbol = LANGUAGE_TAG;
        return true;
      }
    }
    return false;
  }

  if (valid_symbols[INJECTION_DELIMITER] && lexer->lookahead == '~') {
    lexer->advance(lexer, false);
    if (lexer->lookahead == '~') {
      lexer->advance(lexer, false);
      lexer->mark_end(lexer);
      lexer->result_symbol = INJECTION_DELIMITER;
      return true;
    }
    return false;
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
