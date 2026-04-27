;
;
; Tilde blocks — always markdown
; ————————————————————————

((tilde_block
  (tilde_body) @injection.content)
  (#set! injection.language "markdown"))


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
