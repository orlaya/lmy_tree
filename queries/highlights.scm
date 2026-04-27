
; Comments
; ; ————————————————————————
(comment) @comment


;
;
; Keywords
; ————————————————————————
"verify" @keyword
"import" @keyword
"aka" @keyword


;
;
; Sections - the [WORKSPACE] style headers
; ————————————————————————

(section
  "[" @punctuation.bracket
  (identifier) @title
  "]" @punctuation.bracket)


;
;
;  Scope operators
; ————————————————————————

; OUTPUT::{ ... }
(scope_block
  name: (identifier) @label
  "::" @operator
  "{" @punctuation.bracket
  "}" @punctuation.bracket)

; server:: (entering a scope)
(scope_entry
  name: (identifier) @label
  "::" @operator)

; :: (returning to root)
(scope_return) @operator

"}" @punctuation.bracket


;
;
; Statements
; ————————————————————————

; Path pieces (used by both verify and import)
(path
  (path_body
    (path_segment) @string.special))
(path
  (path_body
    "/" @punctuation.delimiter))
(path
  (path_alias_prefix) @operator)
(path
  (path_relative_prefix) @punctuation.special)

; verify ombre::{ PNPM_CATALOGS, PNPM_SETTINGS }
(verify_statement "::" @operator)
(verify_statement "{" @punctuation.bracket)
(verify_statement "}" @punctuation.bracket)
(verify_statement "," @punctuation.delimiter)
(verify_statement (identifier) @title)

; import vite::{defineConfig}
(import_statement "::" @operator)
(import_statement "{" @punctuation.bracket)
(import_statement "}" @punctuation.bracket)
(import_statement "," @punctuation.delimiter)
(import_statement (identifier) @variable)


;
;
; Assignments
; ————————————————————————

; name: orlaya
(assignment
  key: (key) @property
  ":" @punctuation.delimiter)


;
;
; List items
; ————————————————————————

; -- coreWorkspace
(list_item
  "--" @punctuation)

; xx done!
(done_item
  (done_marker) @punctuation.special)


;
;
; Arrays
; ————————————————————————

(array "[" @punctuation.bracket)
(array "]" @punctuation.bracket)
(array "," @punctuation.delimiter)


;
;
; Variables
; ————————————————————————
(variable_dollar) @punctuation
; (variable_dollar) @punctuation.special
(variable_qualifier) @label
; (variable_qualifier) @property
(variable_dot) @punctuation.delimiter
(variable_segment) @variable


;
;
; Language injection
; ————————————————————————
(language_tag) @punctuation
(injection_delimiter) @punctuation


;
;
; Inline formatting
; ————————————————————————
(inline_code "`" @punctuation.delimiter)
(inline_code) @string.special


;
;
; Values
; ————————————————————————
(string) @string
(boolean) @boolean
(number) @number
"*" @punctuation.special

; Version parts
(version_prefix) @operator
(version_digits) @string
(version_dot) @punctuation.delimiter
(version_dash) @punctuation.delimiter
(version_tag) @string.special


;
;
; Multiline blocks
; ————————————————————————
(fold_open) @punctuation
(fold_close) @punctuation
(fold_body) @string

(preserve_delimiter) @punctuation
(preserve_body) @string

(tilde_delimiter) @punctuation
(tilde_body) @string


;
;
;
; ————————————————————————
(raw_value) @string
(array_raw_value) @string
