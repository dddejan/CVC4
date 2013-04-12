(set-logic QF_LRA)
(set-info :smt-lib-version 2.0)
(set-info :status sat)
(declare-fun x0 () Bool)
(declare-fun x1 () Bool)
(declare-fun x2 () Bool)
(assert (or x0 x1 x2))
(assert (or x0 x1 (not x2)))
(assert (or x0 (not x1) x2))
(assert (or x0 (not x1) (not x2)))
(assert (or (not x0) x1 x2))
(assert (or (not x0) x1 (not x2)))
(assert (or (not x0) (not x1) x2))
(assert (or (not x0) (not x1) (not x2)))
(check-sat)
(exit)