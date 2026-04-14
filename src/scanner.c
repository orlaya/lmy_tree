#include "tree_sitter/parser.h"

enum TokenType {
  FOLD_OPEN,           // <<
  FOLD_CLOSE,          // >>
  PRESERVE_DELIMITER,  // ||
  VARIABLE_DOLLAR,     // $
  VARIABLE_QUALIFIER,  // namespace segment before dot (sv in $sv.FOO)
  VARIABLE_SEGMENT,    // final identifier segment (FOO in $sv.FOO, or BAR in $BAR)
  VARIABLE_DOT,        // . between segments
  GLOB,                // * or **
  ERROR_SENTINEL,
};

static bool is_segment_start(int32_t c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static bool is_segment_char(int32_t c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
         (c >= '0' && c <= '9') || c == '_' || c == '@' || c == '-';
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

  // ── Whitespace skip for remaining tokens ──

  while (lexer->lookahead == ' ' || lexer->lookahead == '\t') {
    lexer->advance(lexer, true);
  }

  if (valid_symbols[GLOB] && lexer->lookahead == '*') {
    lexer->advance(lexer, false);
    if (lexer->lookahead == '*') {
      lexer->advance(lexer, false);
    }
    lexer->mark_end(lexer);
    lexer->result_symbol = GLOB;
    return true;
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
