/// <reference types="./_config/tree-sitter-types.d.ts" />
// @ts-check

export default grammar({
  name: 'lmy',

  extras: $ => [
    /\s/,
    $.comment,
  ],

  rules: {
    source_file: $ => repeat($._definition),

    _definition: $ => choice(
      $.section,
      $.verify_statement,
      $.import_statement,
      $.scope_entry,
      $.scope_return,
      $.assignment,
      $.list_item,
    ),

    //
    //
    //
    // ────────────────────────────────

    // [WORKSPACE]
    section: $ => seq('[', $.identifier, ']'),

    // server:: (entering nested scope)
    scope_entry: $ => seq(field('name', $.identifier), '::'),

    // :: (back to section root)
    scope_return: $ => '::',

    //
    //
    //
    // ────────────────────────────────

    // verify::mauve/config
    verify_statement: $ => seq('verify', '::', $.path),

    // import vite::{defineConfig}
    import_statement: $ => seq(
      'import',
      $.path,
      '::',
      '{',
      commaSep($.identifier),
      '}',
    ),

    // name: orlaya  OR  catalogs: (value optional for list headers)
    // prec.right = prefer to grab the value when there's ambiguity
    assignment: $ => prec.right(seq(
      field('key', $.identifier),
      ':',
      optional(field('value', $._value)),
    )),

    // -- coreWorkspace
    list_item: $ => seq('--', $._value),

    // // comment text
    comment: $ => seq('//', /.*/),

    //
    //
    //
    // ────────────────────────────────

    _value: $ => choice(
      $.string,
      $.boolean,
      $.version,
      $.number,
      $.path_value,
      $.identifier,
      $.raw_value,
    ),

    // Catch-all: anything else up to end of line
    // This handles shell commands, expressions with parentheses, etc.
    // Low precedence so other value types match first
    raw_value: $ => prec(-1, /[^\n\r]+/),

    // "quoted string"
    string: $ => /"[^"]*"/,

    // true / false
    boolean: $ => choice('true', 'false'),

    // 0.0.0 or 1.2.3.4 (digits with dots, at least one dot)
    version: $ => /\d+(?:\.\d+)+/,

    // 3001
    number: $ => /\d+/,

    // /Users/sarah/whatever or mauve/config (paths with slashes)
    path_value: $ => /\/?[a-zA-Z_][\w\-\/.]*/,

    // Path for imports/verify (allows @ prefix for npm scopes)
    path: $ => /@?[a-zA-Z_][\w\-\/]*/,

    // ─────────────────────────────────────────────
    // Identifier - the tricky one
    // ─────────────────────────────────────────────
    //
    //
    //
    // ────────────────────────────────
    // Must handle:
    //   aft:dev     → single identifier (colon between words)
    //   name: value → identifier, then assignment operator, then value
    //
    // Rule: colon allowed BETWEEN word segments, never at the end.
    // Pattern breakdown:
    //   [a-zA-Z_]      start with letter or underscore
    //   [\w@-]*        then word chars, @, or hyphens
    //   (?:            optionally, one or more times:
    //     :              a colon
    //     [a-zA-Z_]      followed immediately by letter/underscore
    //     [\w@-]*        then more word chars
    //   )*
    //
    identifier: $ => /[a-z_][\w@-]*(?::[a-z_][\w@-]*)*/i,
  },
})

/**
 * @param {RuleOrLiteral} rule
 * Custom Funtion -- jsdoc needed to appease ts check
 */
function commaSep(rule) {
  return optional(seq(rule, repeat(seq(',', rule))))
}
