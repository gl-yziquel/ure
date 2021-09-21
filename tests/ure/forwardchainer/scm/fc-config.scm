;
; Configuration file for the example crisp rule base system (used by
; fc.scm)

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Load required modules and utils ;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(use-modules (opencog))
(use-modules (opencog ure))

;; Useful to run the unit tests without having to install opencog
(load-from-path "ure-utils.scm")

;;;;;;;;;;;;;;;;
;; Load rules ;;
;;;;;;;;;;;;;;;;
(load-from-path "tests/ure/rules/crisp-modus-ponens-rule.scm")
(load-from-path "tests/ure/rules/fc-deduction-rule.scm")

; Define a new rule base (aka rule-based system)
(define fc-rbs (ConceptNode "fc-rule-base"))

; Associate the rules to the rule base (with weights, their semantics
; is currently undefined, we might settled with probabilities but it's
; not sure)
(define crisp-modus-ponens-tv (stv 0.4 0.9))
(define crisp-deduction-tv (stv 0.6 0.9))
(define fc-rules (list (list crisp-modus-ponens-rule-name crisp-modus-ponens-tv)
                       (list fc-deduction-rule-name crisp-deduction-tv)))
(format #t "duuude pre uer-add-ru\n")
(ure-add-rules fc-rbs fc-rules)

; Termination criteria parameters
(ure-set-num-parameter fc-rbs "URE:maximum-iterations" 20)
