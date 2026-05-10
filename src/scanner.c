#include "tree_sitter/parser.h"

enum TokenType {
  FOLD_OPEN,           // <<
  FOLD_CLOSE,          // >>
  PRESERVE_DELIMITER,  // ||
  VARIABLE_DOLLAR,     // $
  INTERPOLATION_DOLLAR, // $ in ${...}
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
  LIST_MARKER,         // -- (list item prefix)
  IN_PROGRESS_MARKER,  // == (in-progress item prefix)
  TILDE_DELIMITER,     // ~~~
  HEADING_MARKER,      // # through ###### at column 0 followed by space
  FENCED_CODE_DELIMITER, // ``` (exactly three backticks)
  FENCED_CODE_BODY,      // opaque blob inside fenced code block
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

// Keys can start with # * . @ for things like: *.test: value, #/alias: value
// Because # is here, heading_marker MUST be checked before key detection.
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
  while (lexer->lookahead != '\n' && lexer->lookahead != '\r' &&
         !lexer->eof(lexer)) {
    if (lexer->lookahead == '*') {
      return true;
    }
    lexer->advance(lexer, false);
  }
  return false;
}

// Peek ahead for a closing * before a blank line or EOF.
// Allows single newlines (line wrapping) but a blank line (two consecutive
// newlines with only whitespace between) means the paragraph ended.
static bool has_closing_star_before_blank_line(TSLexer *lexer) {
  while (!lexer->eof(lexer)) {
    if (lexer->lookahead == '*') {
      return true;
    }
    if (lexer->lookahead == '\n' || lexer->lookahead == '\r') {
      // Consume the newline
      if (lexer->lookahead == '\r') lexer->advance(lexer, false);
      if (lexer->lookahead == '\n') lexer->advance(lexer, false);
      // Skip whitespace on the next line
      while (lexer->lookahead == ' ' || lexer->lookahead == '\t') {
        lexer->advance(lexer, false);
      }
      // If the next line is also a newline (or EOF), that's a blank line — bail
      if (lexer->lookahead == '\n' || lexer->lookahead == '\r' ||
          lexer->eof(lexer)) {
        return false;
      }
      // Also bail if we hit a closing delimiter (~~~ for tilde blocks,
      // >> for folds, || for preserves)
      if (lexer->lookahead == '~' || lexer->lookahead == '>' ||
          lexer->lookahead == '|') {
        return false;
      }
      continue;
    }
    lexer->advance(lexer, false);
  }
  return false;
}

static bool parse_emphasis(Scanner *s, TSLexer *lexer, const bool *valid_symbols) {
  // Prefer single-line when both are valid (GLR has heading path + tilde_body
  // path active simultaneously — single-line feeds heading_content, multiline
  // feeds tilde_body. Picking single-line lets the heading path survive.)
  bool has_single = valid_symbols[EMPHASIS_OPEN] || valid_symbols[EMPHASIS_CLOSE];
  bool has_multi  = valid_symbols[EMPHASIS_OPEN_MULTILINE] ||
                    valid_symbols[EMPHASIS_CLOSE_MULTILINE];
  bool is_multi = has_multi && !has_single;
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
    // Multi-line: only commit if there's a closing * before a blank line.
    // Emphasis never spans across paragraph boundaries.
    if (!is_multi && !has_closing_star_on_line(lexer)) {
      s->num_emphasis_delimiters_left = 0;
      return false;
    }
    if (is_multi && !has_closing_star_before_blank_line(lexer)) {
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

  // ── Fenced code body: opaque blob, consumes until ```+ at start of line ──
  // The closing fence may be preceded by any amount of whitespace (spaces or
  // tabs) — fenced blocks inside indented folds/preserves naturally indent
  // their close. Only whitespace is allowed before the fence on that line.
  // Three or more backticks closes the fence — many editors auto-insert
  // extra backticks, so we don't insist on exactly three.
  if (valid_symbols[FENCED_CODE_BODY]) {
    while (!lexer->eof(lexer)) {
      if (lexer->lookahead == '\n' || lexer->lookahead == '\r') {
        // Consume the newline
        if (lexer->lookahead == '\r') lexer->advance(lexer, false);
        if (lexer->lookahead == '\n') lexer->advance(lexer, false);
        // Mark end of body BEFORE consuming any leading whitespace on the
        // candidate close line — the whitespace and backticks belong to the
        // following FENCED_CODE_DELIMITER token, not to the body.
        lexer->mark_end(lexer);
        // Skip leading whitespace on the new line
        while (lexer->lookahead == ' ' || lexer->lookahead == '\t') {
          lexer->advance(lexer, false);
        }
        // Count backticks — three or more closes the fence
        if (lexer->lookahead == '`') {
          uint8_t backtick_count = 0;
          while (lexer->lookahead == '`') {
            lexer->advance(lexer, false);
            backtick_count++;
          }
          if (backtick_count >= 3) {
            // Confirmed close — body ends at the start of this line
            // (mark_end was already set above).
            lexer->result_symbol = FENCED_CODE_BODY;
            return true;
          }
        }
        continue;
      }
      lexer->advance(lexer, false);
    }
    // Hit EOF without closing — emit what we have
    lexer->mark_end(lexer);
    lexer->result_symbol = FENCED_CODE_BODY;
    return true;
  }

  // ── Whitespace skip for remaining tokens ──

  if (lexer->eof(lexer)) return false;
  uint32_t column_before_skip = lexer->get_column(lexer);

  while (lexer->lookahead == ' ' || lexer->lookahead == '\t') {
    lexer->advance(lexer, true);
  }

  // ── Column-0 markers: headings, lists, keys ──
  // Order matters — heading/list/in_progress checks MUST come before key
  // detection because # is a valid key_start char and would get swallowed
  // by the key path otherwise.
  if (column_before_skip == 0 && (
      valid_symbols[KEY] || valid_symbols[DONE_MARKER] ||
      valid_symbols[LIST_MARKER] || valid_symbols[IN_PROGRESS_MARKER] ||
      valid_symbols[HEADING_MARKER])) {

    // ── Heading marker: # through ###### followed by space/tab ──
    if (valid_symbols[HEADING_MARKER] && lexer->lookahead == '#') {
      uint8_t hash_count = 0;
      while (lexer->lookahead == '#' && hash_count < 7) {
        lexer->advance(lexer, false);
        hash_count++;
      }
      lexer->mark_end(lexer);
      if (hash_count >= 1 && hash_count <= 6 &&
          (lexer->lookahead == ' ' || lexer->lookahead == '\t')) {
        lexer->result_symbol = HEADING_MARKER;
        return true;
      }
      return false;
    }

    // ── List marker: -- followed by space/tab ──
    if (valid_symbols[LIST_MARKER] && lexer->lookahead == '-') {
      lexer->advance(lexer, false);
      if (lexer->lookahead == '-') {
        lexer->advance(lexer, false);
        lexer->mark_end(lexer);
        if (lexer->lookahead == ' ' || lexer->lookahead == '\t') {
          lexer->result_symbol = LIST_MARKER;
          return true;
        }
      }
      return false;
    }

    // ── In-progress marker: == followed by space/tab ──
    if (valid_symbols[IN_PROGRESS_MARKER] && lexer->lookahead == '=') {
      lexer->advance(lexer, false);
      if (lexer->lookahead == '=') {
        lexer->advance(lexer, false);
        lexer->mark_end(lexer);
        if (lexer->lookahead == ' ' || lexer->lookahead == '\t') {
          lexer->result_symbol = IN_PROGRESS_MARKER;
          return true;
        }
      }
      return false;
    }

    // ── Quoted key: "something": ──
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

    // ── Bare key or done marker ──
    // Both start with alpha at column 0.
    // Read the identifier, then decide: trailing colon → KEY, exactly "xx" + whitespace → DONE_MARKER.
    if (is_key_start(lexer->lookahead)) {
      int32_t first_char = lexer->lookahead;
      int char_count = 0;

      lexer->advance(lexer, false);
      char_count++;
      int32_t second_char = lexer->lookahead;

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

  // ── Fenced code delimiter: three or more backticks ──
  // Only fires at the start of a line (possibly indented). A bare ``` mid-
  // prose stays plain text — otherwise inline mentions like "no ```xyz code
  // blocks here" inside a fold body would be misparsed as an opening fence.
  // Accepts any run of 3+ backticks because many editors auto-insert extra
  // backticks when typing fences, and the count carries no meaning here.
  if (valid_symbols[FENCED_CODE_DELIMITER] && lexer->lookahead == '`' &&
      column_before_skip == 0) {
    uint8_t backtick_count = 0;
    while (lexer->lookahead == '`') {
      lexer->advance(lexer, false);
      backtick_count++;
    }
    if (backtick_count >= 3) {
      lexer->mark_end(lexer);
      lexer->result_symbol = FENCED_CODE_DELIMITER;
      return true;
    }
    return false;
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

  if ((valid_symbols[INTERPOLATION_DOLLAR] || valid_symbols[VARIABLE_DOLLAR]) &&
      lexer->lookahead == '$') {
    lexer->advance(lexer, false);
    lexer->mark_end(lexer);

    if (valid_symbols[INTERPOLATION_DOLLAR] && lexer->lookahead == '{') {
      lexer->result_symbol = INTERPOLATION_DOLLAR;
      return true;
    }

    if (valid_symbols[VARIABLE_DOLLAR] && is_segment_start(lexer->lookahead)) {
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
