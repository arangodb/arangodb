;; Copyright (C) 2016 and later: Unicode, Inc. and others. License & terms of use: http://www.unicode.org/copyright.html
;; Copyright (c) 2014 IBM Corporation and others, all rights reserved
;; Thx: http://www.ergoemacs.org/emacs/elisp_syntax_coloring.html
;; Thx: http://repo.or.cz/w/emacs.git/blob/HEAD:/lisp/progmodes/sh-script.el
;;
;; load this with M-x eval-buffer
;;
;; TODO: .*

(setq icuDepKeywords
      '(("group:\\|system_symbols:\\|library:" . font-lock-function-name-face)
        ("deps" . font-lock-constant-face)
        ("#.*" . font-lock-comment-face)
        ("[a-zA-Z0-9_]+\\.o" . font-lock-doc-face)
    )
)

(define-derived-mode icu-dependencies-mode fundamental-mode
  (setq font-lock-defaults '(icuDepKeywords))
  (setq mode-name "icu dependencies.txt")
  (setq comment-start "# ")
)
