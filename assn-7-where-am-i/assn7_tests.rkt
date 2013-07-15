#lang racket
(require "assn7.rkt") 


(define ti-1 '((1 (0 0)) (1 (1 0))))
(define ti-2 '((1 (0 0)) (1 (1 0)) (1 (1 1))))

(define ti-3 '((0 0) (2 0) (6 0)) )
(define ti-4 '((2 5) (7 8) (10 1) (3 2)) )

(define ti-5 '((0 0) (2 0) (6 0) (1 0)))
(define ti-6 '((0 0) (2 0) (6 0) (1 0) (5 4) (4 5)))



(pretty-print "Intersection points:")
(pretty-print (intersection-points ti-1))
(pretty-print "" )
(pretty-print (intersection-points ti-2))
(pretty-print "" )

(pretty-print "Distance product:")
(pretty-print (distance-product '(2 0) ti-3))
(pretty-print "" )
(pretty-print (distance-product '(3 3) ti-4))
(pretty-print "" )

(pretty-print "Rate points:")
(pretty-print (rate-points ti-3))
(pretty-print "" )
(pretty-print (rate-points ti-4))
(pretty-print "" )

(pretty-print "Sort points:")
(pretty-print (sort-points (rate-points ti-4)))
(pretty-print "" )
(pretty-print (sort-points (rate-points ti-3)))
(pretty-print "" )

(pretty-print "Clumped points:")
(pretty-print (clumped-points ti-3))
(pretty-print "")
(pretty-print (clumped-points ti-5))
(pretty-print "") 

(pretty-print "Sumfst: 8, 22")
(pretty-print (sumfst ti-3))
(pretty-print "")
(pretty-print (sumfst ti-4))
(pretty-print "") 

(pretty-print "Sumsnd: 0, 16")
(pretty-print (sumsnd ti-3))
(pretty-print "")
(pretty-print (sumsnd ti-4))
(pretty-print "") 

(pretty-print "average-point")
(pretty-print (average-point ti-3))
(pretty-print "")
(pretty-print (average-point ti-6))
(pretty-print "")


(define ti-7 '( (1 (0 0)) (1 (2 0)) (0.1 (1 0)) ))
(pretty-print "best-estimate")
(pretty-print (best-estimate ti-7))
(pretty-print "")

(define ti-8 '(2.5 11.65 7.75))
(define ti-9 '((0 0) (4 4) (10 0)))

(pretty-print (where-am-i ti-8 ti-9))