; COMMAND-LINE: --finite-model-find
; EXPECT: unsat
(set-logic ALL)
(declare-sort $$unsorted 0)
(declare-fun sorti1 ($$unsorted) Bool)
(declare-fun op1 ($$unsorted $$unsorted) $$unsorted)
(declare-fun sorti2 ($$unsorted) Bool)
(declare-fun op2 ($$unsorted $$unsorted) $$unsorted)
(declare-fun h ($$unsorted) $$unsorted)
(declare-fun j ($$unsorted) $$unsorted)
(assert (forall ((U $$unsorted) (BOUND_VARIABLE_345 $$unsorted)) (or (not (sorti1 U)) (not (sorti1 BOUND_VARIABLE_345)) (sorti1 (op1 U BOUND_VARIABLE_345))) ))
(assert (forall ((U $$unsorted) (BOUND_VARIABLE_364 $$unsorted)) (or (not (sorti2 U)) (not (sorti2 BOUND_VARIABLE_364)) (sorti2 (op2 U BOUND_VARIABLE_364))) ))
(assert (forall ((U $$unsorted)) (or (not (sorti1 U)) (= U (op1 U U))) ))
(assert (not (forall ((U $$unsorted)) (or (not (sorti2 U)) (= U (op2 U U))) )))
(assert (not (=> (and (forall ((U $$unsorted)) (or (not (sorti1 U)) (sorti2 (h U))) ) (forall ((V $$unsorted)) (or (not (sorti2 V)) (sorti1 (j V))) )) (not (and (forall ((W $$unsorted) (BOUND_VARIABLE_406 $$unsorted)) (or (not (sorti1 W)) (not (sorti1 BOUND_VARIABLE_406)) (= (op2 (h W) (h BOUND_VARIABLE_406)) (h (op1 W BOUND_VARIABLE_406)))) ) (forall ((Y $$unsorted) (BOUND_VARIABLE_431 $$unsorted)) (or (not (sorti2 Y)) (not (sorti2 BOUND_VARIABLE_431)) (= (op1 (j Y) (j BOUND_VARIABLE_431)) (j (op2 Y BOUND_VARIABLE_431)))) ) (forall ((X1 $$unsorted)) (or (not (sorti2 X1)) (= X1 (h (j X1)))) ) (forall ((X2 $$unsorted)) (or (not (sorti1 X2)) (= X2 (j (h X2)))) ))))))
(assert (and (forall ((U $$unsorted)) (or (not (sorti1 U)) (sorti2 (h U))) ) (forall ((V $$unsorted)) (or (not (sorti2 V)) (sorti1 (j V))) ) (forall ((W $$unsorted) (BOUND_VARIABLE_406 $$unsorted)) (or (not (sorti1 W)) (not (sorti1 BOUND_VARIABLE_406)) (= (op2 (h W) (h BOUND_VARIABLE_406)) (h (op1 W BOUND_VARIABLE_406)))) ) (forall ((Y $$unsorted) (BOUND_VARIABLE_431 $$unsorted)) (or (not (sorti2 Y)) (not (sorti2 BOUND_VARIABLE_431)) (= (op1 (j Y) (j BOUND_VARIABLE_431)) (j (op2 Y BOUND_VARIABLE_431)))) ) (forall ((X1 $$unsorted)) (or (not (sorti2 X1)) (= X1 (h (j X1)))) ) (forall ((X2 $$unsorted)) (or (not (sorti1 X2)) (= X2 (j (h X2)))) )))
(check-sat)
