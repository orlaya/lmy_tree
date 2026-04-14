
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

; server:: (entering a scope)
(scope_entry
  name: (identifier) @label
  "::" @operator)

; :: (returning to root)
(scope_return) @operator


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
  key: (identifier) @property
  ":" @punctuation.delimiter)


;
;
; List items
; ————————————————————————

; -- coreWorkspace
(list_item
  "--" @punctuation.list_marker)


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
(version) @number
(number) @number
"*" @punctuation.special


;
;
; Multiline blocks
; ————————————————————————
(fold_open) @operator
(fold_close) @operator
(multiline_fold) @string

(preserve_delimiter) @operator
(multiline_preserve) @string


;
;
;
; ————————————————————————
(raw_value) @string

(assignment
  value: (identifier) @string)

(list_item
  (identifier) @string)
