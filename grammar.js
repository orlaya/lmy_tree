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
    $.language_tag,
    $.injection_delimiter,
    $.done_marker,
    $.tilde_delimiter,
    $.tilde_body,
    $.emphasis_open,
    $.emphasis_close,
    // Phantom tokens — never emitted by the scanner, but their presence in
    // valid_symbols tells the scanner what preceded the current position.
    // The grammar places optional($._last_token_whitespace) after whitespace
    // and optional($._last_token_punctuation) after punctuation so the
    // scanner can check valid_symbols[LAST_TOKEN_*] for context.
    $._last_token_whitespace,
    $._last_token_punctuation,
    $.error_sentinel,
  ],

  extras: $ => [
    /[ \t]/,
    $.comment,
  ],

  conflicts: $ => [
    [$.fold_body, $.emphasis_multiline],
    [$.fold_body, $.strong_emphasis_multiline],
    [$.preserve_body, $.emphasis_multiline],
    [$.preserve_body, $.strong_emphasis_multiline],
    [$.raw_value, $.emphasis],
    [$.raw_value, $.strong_emphasis],
    [$.raw_value, $.emphasis, $.strong_emphasis],
    [$.array_raw_value, $.emphasis],
    [$.array_raw_value, $.strong_emphasis],
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
      $.done_item,
      $.tilde_block,
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

    // verify ombre::{ PNPM_CATALOGS, PNPM_SETTINGS }
    verify_statement: $ => prec.right(seq(
      'verify',
      $.path,
      optional(seq(
        '::',
        optional(seq(
          '{',
          commaSep($.identifier),
          optional('}'),
        )),
      )),
    )),

    // import vite::{defineConfig}
    import_statement: $ => prec.right(seq(
      'import',
      $.path,
      optional(seq(
        '::',
        optional(seq(
          '{',
          commaSep($.identifier),
          optional('}'),
        )),
      )),
    )),

    // name: orlaya  OR  catalogs: (value optional for list headers)
    // prec.right = prefer to grab the value when there's ambiguity
    assignment: $ => prec.right(seq(
      field('key', $.key),
      ':',
      optional(field('value', $._value)),
    )),

    // -- coreWorkspace
    list_item: $ => seq('--', $._value),

    // xx done!
    done_item: $ => seq($.done_marker, $._value),

    // // comment text
    comment: $ => seq('//', /.*/),

    //
    //
    //
    // ────────────────────────────────

    multiline_fold: $ => seq(
      $.fold_open,
      optional($.fold_body),
      $.fold_close,
    ),

    fold_body: $ => seq(optional($._last_token_whitespace), repeat1(choice(
      $.strong_emphasis_multiline,
      $.emphasis_multiline,
      $.inline_code,
      $.variable,
      /[^$*\/`\n\r]+/,
      seq('/', optional($._last_token_punctuation)),
      '$',
      $.emphasis_open,
      '`',
      seq(/\r?\n/, optional($._last_token_whitespace)),
    ))),

    multiline_preserve: $ => seq(
      $.preserve_delimiter,
      optional($.preserve_body),
      $.preserve_delimiter,
    ),

    preserve_body: $ => seq(optional($._last_token_whitespace), repeat1(choice(
      $.strong_emphasis_multiline,
      $.emphasis_multiline,
      $.inline_code,
      $.variable,
      /[^$*\/`\n\r]+/,
      seq('/', optional($._last_token_punctuation)),
      '$',
      $.emphasis_open,
      '`',
      seq(/\r?\n/, optional($._last_token_whitespace)),
    ))),

    // ~~~ markdown block ~~~
    // Always injects markdown. Body is an opaque scanner token
    // so comments and variables inside aren't interpreted.
    tilde_block: $ => seq(
      $.tilde_delimiter,
      /\r?\n/,
      optional($.tilde_body),
      $.tilde_delimiter,
    ),

    _line_content: $ => repeat1(choice(
      $.strong_emphasis,
      $.emphasis,
      $.inline_code,
      $.variable,
      /[^$*\/`\n\r]+/,
      '/',
      '$',
      $.emphasis_open,
      '`',
    )),

    _value: $ => choice(
      seq($.language_tag, $.injection_delimiter, $._injectable_value),
      $._injectable_value,
    ),

    _injectable_value: $ => choice(
      $.multiline_fold,
      $.multiline_preserve,
      $.array,
      $.string,
      $.boolean,
      $.version,
      $.number,
      $.raw_value,
    ),

    array: $ => prec(1, seq('[', commaSep($._array_value), ']')),

    _array_value: $ => choice(
      seq($.language_tag, $.injection_delimiter, $._injectable_array_value),
      $._injectable_array_value,
    ),

    _injectable_array_value: $ => choice(
      $.array,
      $.string,
      $.boolean,
      $.version,
      $.number,
      $.array_raw_value,
    ),

    array_raw_value: $ => prec(-1, seq(
      optional($._last_token_whitespace),
      repeat1(choice(
        $.strong_emphasis,
        $.emphasis,
        $.inline_code,
        $.variable,
        /[^,$*\/~`\n\r\[\]]+/,
        seq('/', optional($._last_token_punctuation)),
        '$',
        $.emphasis_open,
        '~',
        '`',
      )),
    )),

    variable: $ => seq(
      $.variable_dollar,
      repeat(seq($.variable_qualifier, $.variable_dot)),
      $.variable_segment,
    ),

    raw_value: $ => prec(-1, seq(
      optional($._last_token_whitespace),
      choice(
        $.strong_emphasis,
        $.emphasis,
        $.inline_code,
        $.variable,
        /[^ \t$*\/~`\n\r\[]+/,
        seq('/', optional($._last_token_punctuation)),
        '$',
        $.emphasis_open,
        '~',
        '`',
      ),
      repeat(choice(
        $.strong_emphasis,
        $.emphasis,
        $.inline_code,
        $.variable,
        /[^$*\/~`\n\r]+/,
        seq('/', optional($._last_token_punctuation)),
        '$',
        $.emphasis_open,
        '~',
        '`',
      )),
    )),

    // ── Single-line emphasis (raw_value, array_raw_value) ──

    // *italic*
    emphasis: $ => prec.dynamic(1, seq(
      alias($.emphasis_open, $.emphasis_delimiter),
      optional($._last_token_punctuation),
      $._emphasis_content,
      alias($.emphasis_close, $.emphasis_delimiter),
    )),

    // **bold**
    strong_emphasis: $ => prec.dynamic(2, seq(
      alias($.emphasis_open, $.emphasis_delimiter),
      alias($.emphasis_open, $.emphasis_delimiter),
      optional($._last_token_punctuation),
      $._emphasis_content,
      alias($.emphasis_close, $.emphasis_delimiter),
      alias($.emphasis_close, $.emphasis_delimiter),
    )),

    // Single-line only — no newlines allowed
    _emphasis_content: $ => repeat1(choice(
      $.inline_code,
      $.variable,
      /[^$*\/`\n\r]+/,
      seq('/', optional($._last_token_punctuation)),
      '$',
      '`',
    )),

    // ── Multi-line emphasis (fold_body, preserve_body) ──

    // *italic* (can span lines)
    emphasis_multiline: $ => prec.dynamic(1, seq(
      alias($.emphasis_open, $.emphasis_delimiter),
      optional($._last_token_punctuation),
      $._emphasis_content_multiline,
      alias($.emphasis_close, $.emphasis_delimiter),
    )),

    // **bold** (can span lines)
    strong_emphasis_multiline: $ => prec.dynamic(2, seq(
      alias($.emphasis_open, $.emphasis_delimiter),
      alias($.emphasis_open, $.emphasis_delimiter),
      optional($._last_token_punctuation),
      $._emphasis_content_multiline,
      alias($.emphasis_close, $.emphasis_delimiter),
      alias($.emphasis_close, $.emphasis_delimiter),
    )),

    // Multi-line — allows newlines
    _emphasis_content_multiline: $ => repeat1(choice(
      $.inline_code,
      $.variable,
      /[^$*\/`\n\r]+/,
      seq('/', optional($._last_token_punctuation)),
      '$',
      '`',
      seq(/\r?\n/, optional($._last_token_whitespace)),
    )),

    // `inline code`
    inline_code: $ => token(prec(2, seq('`', /[^`\n\r]+/, '`'))),

    // "quoted string"
    string: $ => /"[^"\n]*"/,

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

    // Path for imports/verify.
    //   file/path                 — regular
    //   ./file/path, ../file/path — relative (. or .. max)
    //   #/aliased/path            — alias (# highlighted specially)
    // Prefixes separate so each highlights distinctly; slashes split out as
    // their own tokens so they colour as punctuation, not body text.
    // Tolerates mid-typing states: bare `#`, `./`, trailing slashes, etc.
    path: $ => choice(
      seq($.path_alias_prefix, optional($.path_body)),
      seq($.path_relative_prefix, optional($.path_body)),
      $.path_body,
    ),

    path_alias_prefix: $ => '#',
    path_relative_prefix: $ => /\.\.?/,
    path_body: $ => choice(
      seq('/', optional(seq($.path_segment, repeat(seq('/', optional($.path_segment)))))),
      seq($.path_segment, repeat(seq('/', optional($.path_segment)))),
    ),
    path_segment: $ => /[\w@][\w\-.@]*/,

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
