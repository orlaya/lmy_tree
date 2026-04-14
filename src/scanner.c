#include "tree_sitter/parser.h"

enum TokenType {
  FOLD_OPEN,           // <<
  FOLD_CLOSE,          // >>
  PRESERVE_DELIMITER,  // ||
};

void *tree_sitter_lmy_external_scanner_create() { return NULL; }
void tree_sitter_lmy_external_scanner_destroy(void *payload) {}
unsigned tree_sitter_lmy_external_scanner_serialize(void *payload, char *buffer) { return 0; }
void tree_sitter_lmy_external_scanner_deserialize(void *payload, const char *buffer, unsigned length) {}

bool tree_sitter_lmy_external_scanner_scan(
  void *payload,
  TSLexer *lexer,
  const bool *valid_symbols
) {
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
