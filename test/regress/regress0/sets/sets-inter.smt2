(set-logic ALL_SUPPORTED)
(set-info :status unsat)
(define-sort SetInt () (Set Int))
(declare-fun a () (Set Int))
(declare-fun b () (Set Int))
(declare-fun x () Int)
;(assert (not (member x a)))
(assert (member x (intersection a b)))
(assert (not (member x b)))
(check-sat)
(exit)
