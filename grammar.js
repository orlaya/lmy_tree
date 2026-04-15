/// <reference types="./_config/tree-sitter-types.d.ts" />
// @ts-check

export default grammar({
  name: 'lmy',

  externals: $ => [
    $.fold_open,
    $.fold_close,
    $.preserve_delimiter,
    $.variable_dollar,
    $.variable_qualifier,
    $.variable_segment,
    $.variable_dot,
    $.key,
    $.version_prefix,
    $.version_digits,
    $.version_dot,
    $.version_dash,
    $.version_tag,
    $.number,
    $.error_sentinel,
  ],

  extras: $ => [
    /[ \t]/,
    $.comment,
  ],

  rules: {
    source_file: $ => repeat(choice($._definition, '}', /\r?\n/)),

    _definition: $ => choice(
      $.section,
      $.verify_statement,
      $.import_statement,
      $.scope_block,
      $.scope_entry,
      $.scope_return,
      $.assignment,
      $.list_item,
    ),

    //
    //
    //
    // ────────────────────────────────

    // [WORKSPACE] or [SCRIPT_VARIABLES] aka [sv]
    section: $ => seq(
      '[', $.identifier, ']',
      optional(seq('aka', '[', $.identifier, ']')),
    ),

    // OUTPUT::{ ... }
    scope_block: $ => seq(
      field('name', $.identifier),
      '::',
      '{',
      repeat(choice($._definition, /\r?\n/)),
      '}',
    ),

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
      field('key', $.key),
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

    multiline_fold: $ => seq(
      $.fold_open,
      optional($._line_content),
      repeat(seq(/\r?\n/, optional($._line_content))),
      $.fold_close,
    ),

    multiline_preserve: $ => seq(
      $.preserve_delimiter,
      optional($._line_content),
      repeat(seq(/\r?\n/, optional($._line_content))),
      $.preserve_delimiter,
    ),

    _line_content: $ => repeat1(choice(
      $.variable,
      /[^$*\/\n\r]+/,
      '/',
      '$',
      '*',
    )),

    _value: $ => choice(
      $.multiline_fold,
      $.multiline_preserve,
      $.array,
      $.string,
      $.boolean,
      $.version,
      $.number,
      $.raw_value,
    ),

    array: $ => prec(1, seq('[', commaSep($._value), ']')),

    variable: $ => seq(
      $.variable_dollar,
      repeat(seq($.variable_qualifier, $.variable_dot)),
      $.variable_segment,
    ),

    raw_value: $ => prec(-1, seq(
      choice(
        $.variable,
        /[^ \t$*\/\n\r\[\]]+/,
        '/',
        '$',
        '*',
      ),
      repeat(choice(
        $.variable,
        /[^$*\/\n\r\[\]]+/,
        '/',
        '$',
        '*',
      )),
    )),

    // "quoted string"
    string: $ => /"[^"]*"/,

    // true / false
    boolean: $ => choice('true', 'false'),

    // 1.0.0, ^1.1.0, ~2.3.0, 1.0.0-beta.1-f92627f
    version: $ => seq(
      optional($.version_prefix),
      $.version_digits,
      $.version_dot,
      $.version_digits,
      repeat(seq($.version_dot, $.version_digits)),
      repeat(seq(
        $.version_dash,
        $.version_tag,
        repeat(seq($.version_dot, choice($.version_digits, $.version_tag))),
      )),
    ),

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
