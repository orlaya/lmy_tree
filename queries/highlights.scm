
; Comments
; ; ————————————————————————
(comment) @comment


;
;
; Keywords
; ————————————————————————
"verify" @keyword
"import" @keyword


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
  (identifier) @type
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
; Values
; ————————————————————————
(string) @string
(boolean) @boolean
(version) @number
(number) @number
(path_value) @string.special


;
;
;
; ————————————————————————
; raw_value intentionally unstyled - it's the catch-all
; identifier as a value (not as a key) gets variable styling
(assignment
  value: (identifier) @variable)

(list_item
  (identifier) @variable)
