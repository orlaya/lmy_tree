;
;
; Assignment values
; ————————————————————————

(assignment
  (language_tag) @injection.language
  (raw_value) @injection.content)

(assignment
  (language_tag) @injection.language
  (multiline_fold (fold_body) @injection.content))

(assignment
  (language_tag) @injection.language
  (multiline_preserve (preserve_body) @injection.content))


;
;
; List items
; ————————————————————————

(list_item
  (language_tag) @injection.language
  (raw_value) @injection.content)

(list_item
  (language_tag) @injection.language
  (multiline_fold (fold_body) @injection.content))

(list_item
  (language_tag) @injection.language
  (multiline_preserve (preserve_body) @injection.content))


;
;
; Done items
; ————————————————————————

(done_item
  (language_tag) @injection.language
  (raw_value) @injection.content)

(done_item
  (language_tag) @injection.language
  (multiline_fold (fold_body) @injection.content))

(done_item
  (language_tag) @injection.language
  (multiline_preserve (preserve_body) @injection.content))


;
;
; Fenced code blocks (inside tilde blocks)
; ————————————————————————

(fenced_code_block
  (fenced_code_language) @injection.language
  (fenced_code_body) @injection.content)
