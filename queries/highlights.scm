
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

; verify::mauve/config
(verify_statement
  "::" @operator
  (path) @string.special)

; import vite::{defineConfig}
(import_statement
  (path) @string.special
  "::" @operator
  "{" @punctuation.bracket
  (identifier) @property
  "}" @punctuation.bracket)


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
(variable_dollar) @punctuation.special
(variable_qualifier) @property
(variable_dot) @punctuation.delimiter
(variable_segment) @variable


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
(multiline_fold) @string

(preserve_delimiter) @punctuation ; was @operator
(multiline_preserve) @string


;
;
;
; ————————————————————————
(raw_value) @string
