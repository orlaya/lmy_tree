;
;
; Assignment values
; ————————————————————————

(assignment
  value: (language_tag) @injection.language
  value: (raw_value) @injection.content)

(assignment
  value: (language_tag) @injection.language
  value: (multiline_fold) @injection.content)

(assignment
  value: (language_tag) @injection.language
  value: (multiline_preserve) @injection.content)


;
;
; List items
; ————————————————————————

(list_item
  (language_tag) @injection.language
  (raw_value) @injection.content)

(list_item
  (language_tag) @injection.language
  (multiline_fold) @injection.content)

(list_item
  (language_tag) @injection.language
  (multiline_preserve) @injection.content)
