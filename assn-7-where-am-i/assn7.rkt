;; The Where-Am-I helper functions:
;; originally written by Nick Parlante

;;
;;  the 2-D POINT type  (x and y coordinates)
;;  impelemented as a list length 2
;;
;;  the CIRCLE type (a radius and a center point)
;;  implemented as a list length 2-- first element is the radius and
;;  the second is the center point
;;
;;  For convenience, I do not insist that you treat these as ADT's-
;;  so if you want to use CAR to get the x coordinate, or build you own
;;  circles without going through MAKE-CIRCLE that will be ok.
;; 
;; POINT functions:
;;  make-pt   create a new point
;;  x         get the x coordinate of a point
;;  y         get the y coordinate of a point
;;  dist      return the distance between two points
;;
;; CIRCLE functions
;;  make-circle   create a new circle
;;  radius        get the radius of a circle
;;  center        get the center of a circle
;;  intersect     given two circles, returns a list of the points
;;                of intersection for those circles.
;;                For the purposes of this program I have
;;                bastardized the definition of 'intersect' a little
;;                to give better results when the measurements are
;;                inexact.  Don't worry about that, just use the points
;;                returned.  Someone who is interested in Math or Geometry
;;                might be interested to see how I compute the intersection.

#lang racket
(provide (all-defined-out)) ;; so we can put tests in a second file

(define (make-pt x y) 
  (list x y))

(define (x pt) 
  (car pt))

(define (y pt) 
  (cadr pt))

(define (dist pt1 pt2) 
  (let ((dx (- (x pt1) (x pt2)))
        (dy (- (y pt1) (y pt2))))
    (sqrt (+ (* dx dx) (* dy dy)))))

;;
;; 2D vector operations - used by the intersection function
;; vectors are a lot like points.  You won't need these.
;; 

(define (add v1 v2)
  (list (+ (car v1) (car v2))
	(+ (cadr v1) (cadr v2))))

(define (sub v1 v2)
  (list (- (car v1) (car v2))
	(- (cadr v1) (cadr v2))))

(define (len v)
  (sqrt (+ (* (car v) (car v))
	   (* (cadr v) (cadr v)))))

(define (scale v factor)
  (list (* (car v) factor) 
	(* (cadr v) factor)))

(define (normalize v)
  (scale (list (- (cadr v)) (car v)) (/ (len v))))


(define (make-circle radius center) 
  (list radius center))

(define (radius circle) 
  (car circle))

(define (center circle) 
  (cadr circle))

;;
;; Function: intersect
;; -------------------
;; Return a list of the points of intersection of the two circles.
;; The circles may not have the same center point
;;

(define (intersect circle1 circle2)
  (if (equal? (center circle1) (center circle2))
      (error "Intersect cannont handle circles with the same center point.")
      (let* ((c1 (center circle1))
	     (r1 (radius circle1))
	     (c2 (center circle2))
	     (r2 (radius circle2))
	     (d (dist c1 c2)))
      ;; first check to see if the circles are too far apart to intersect,
      ;; or if one circle is within another.
	(if (or (> d (+ r1 r2)) (> r1 (+ d r2)) (> r2 (+ d r1)))
	    ;; if there is no real intersection, use the closest tangent points on each
	    ;; circle.  This is the bastardization above.
	    (list (add c1 (scale (sub c2 c1) (/ r1 d)))  ;; c1-> towards c2
		  (add c2 (scale (sub c1 c2) (/ r2 d)))) ;; c2-> towards c1
    ;;otherwise the circles intersect normally, and I did some hairy
    ;;geometry to show that the following computes the two points
    ;;of intersection.
    (let* ((r12 (* r1 r1))
           (r22 (* r2 r2))
           (d2 (* d d))
           (d1 (/ (+ r12 (- r22) d2) 2 d))
           (h (sqrt (- r12 (* d1 d1))))
           (towards (scale (sub c2 c1) (/ d1 d))) ;;vector c1->c2
           (perp (scale (normalize towards) h)))
      (list (add c1 (add towards perp))
            (add c1 (add towards (scale perp -1)))))))))

;;
;; Function: prefix-of-list
;; ------------------------
;; Accepts the incoming list and returns one
;; with the same first k elements and nothing more.
;;

(define (prefix-of-list ls k)
  (if (or (zero? k) (null? ls)) '()
      (cons (car ls) (prefix-of-list (cdr ls) (- k 1)))))

;;
;; Function: partition
;; -------------------
;; Takes a pivot and a list and produces a pair two lists.
;; The first of the two lists contains all of those element less than the 
;; pivot, and the second contains everything else.  Notice that
;; the first list pair every produced is (() ()), and as the
;; recursion unwinds exactly one of the two lists gets a new element
;; cons'ed to the front of it.  
;; 

(define (partition pivot num-list cmpfn)
  (if (null? num-list) '(() ())
      (let ([split-of-rest (partition pivot (cdr num-list) cmpfn)])
	(if (cmpfn (car num-list) pivot)
	    (list (cons (car num-list) (car split-of-rest)) (cadr split-of-rest))
	    (list (car split-of-rest) (cons (car num-list) (car (cdr split-of-rest))))))))

;;
;; Function: quicksort
;; -------------------
;; Implements the quicksort algorithm to sort lists of numbers from
;; high to low.  If a list is of length 0 or 1, then it is trivially
;; sorted.  Otherwise, we partition to cdr of the list around the car
;; to generate two lists: those in the cdr that are smaller than the car,
;; and those in the cdr that are greater than or equal to the car.  
;; We then recursively quicksort the two lists, and then splice everything
;; together in the proper order.
;;
;; Takes a comparator cmpfn which specifies how elements are ordered.
;;

(define (quicksort num-list cmpfn)
  (if (<= (length num-list) 1) num-list
      (let ([split (partition (car num-list) (cdr num-list) cmpfn)])
	(append (quicksort (car split) cmpfn) 
		(list (car num-list)) 
		(quicksort (cadr split) cmpfn)))))

;;
;; Function: remove
;; ----------------
;; Generates a copy of the specified list, except that all
;; instances that match the specified elem in the equal? sense
;; are excluded.
;;

(define (remove elem ls)
  (cond ((null? ls) '())
	((equal? (car ls) elem) (remove elem (cdr ls)))
	(else (cons (car ls) (remove elem (cdr ls))))))
                  
;; 
;; Function: all-guesses
;; ---------------------
;; Given a list of distances and a list of stars, return a list of all
;; the possible guesses.  A single guess is a list of circles which pairs
;; each distance with one of the stars.
;; 

(define (all-guesses distances stars)
  (if (or (null? distances) (null? stars)) '(())
      (apply append 
	     (map (lambda (star)
		    (map (lambda (pair) 
			   (cons (list (car distances) star) pair))
			 (all-guesses (cdr distances) (remove star stars))
			 )
		    )
		  stars))))

(define *distances-1* '(2.65 5.55 5.25))
(define *stars-1* '((0 0) (4 6) (10 0) (7 4) (12 5)))

(define *distances-2* '(2.5 11.65 7.75))
(define *stars-2* '((0 0) (4 4) (10 0)))

;; 
;; Include your code below.. make sure
;; write and test each function independently.  This
;; will require that you repeatedly type in
;; (load "where-am-i.scm") at the command prompt, allowing
;; it to override exisiting definitions and including
;; the most recently implemented into the lot.
;;

(define (my-flatten xs) 
  (if (null? xs) '() 
     (append (car xs) (my-flatten (cdr xs)))))

;; Function: intersection-points
;; -----------------------------
;; intersection-points takes a list of circles and returns a list of
;; all the points where the circles intersect. Intersection-points uses 
;; the intersect function (given), which takes two circles and returns a 
;; list of the points where the circles intersect. Each circle in the list 
;; may intersect with each of the other circles in the list.

;; intersection-points checks all possibilities by intersecting the first circle in the
;; list with all of the ones to the right, then intersecting the second circle with all of the
;; ones to its right and so on. 
	 
(define (intersection-points xs) 
  (if (< (length xs) 2) '()
      (append (my-flatten (map (lambda (x) (intersect (car xs) x)) (cdr xs)))
            (intersection-points (cdr xs)))))

;; Function: distance-product
;; --------------------------			
;; distance-product takes a point and a list of points and returns
;; the product of the distances between that point and all the points in the list. The
;; point itself may be in the list. In that case, the point is removed so that it doesn't
;; force the product to be zero. Uses the dist function provided as part of the point
;; type. 
			
(define (distance-product x xs)
  (foldl * 1 (map (lambda (y) (dist x y)) (remove x xs))))

 

;; Function: rate-points
;; ---------------------
;; rate-points calculates how far each intersection point is from all the other
;; intersection points. rate-points takes a list of points and returns a list where 
;; each point is annotated to show its distance-product from the other points. 
  
(define (rate-points xs)
  (map (lambda (x) (list (distance-product x xs) x)) xs))

;; Function: cmp-pair
;; ------------------
;; cmp-pair is a binary comparator that takes two pairs and return 
;; true if the first element of the first pair is less than than the first
;; element of the second element, false otherwise. 
  
(define (cmp-pair xs ys) 
  (< (car xs) (car ys)))

  
  
  
;; Function: sort-points
;; ---------------------
;; sort-points takes a list of rated points, and sorts them in ascending order of rating. 
  
(define (sort-points xs) 
  (quicksort xs cmp-pair))

  
;; Function: clumped-points
;; ------------------------
;; clumped-points takes a list of points, rates them, sorts them, and then returns the half 
;; of the points with the smallest ratings. clumped-points returns the points without the ratings.    
  
(define (clumped-points xs)
  (let ([xlen (quotient (length xs) 2)])
     (prefix-of-list (map (lambda (x) (cadr x)) (sort-points (rate-points xs))) xlen )))

	 
;; Function: sumfst
;; ----------------
;; sumfst takes a list of pairs and returns the sum of the first 
;; element of the pair	 
	 
(define (sumfst xs)
  (foldl + 0 (map (lambda (x) (car x)) xs)))

;; Function: sumsnd
;; ----------------
;; sumsnd takes a list of pairs and returns the sum of the second 
;; element of the pair	   
  
(define (sumsnd xs)
  (foldl + 0 (map (lambda (x) (cadr x)) xs)))

;; Function: dist-sum
;; ------------------
;; dist-sum takea a number x and a list of numbers and returns the 
;; sum of the product of x and each element in the list
  
(define (dist-sum x xs) 
  (foldl + 0 (map (lambda (y) (dist y x)) xs )))

  
  
;; Function: average-point
;; -----------------------
;; average-point takes a list of points and averages them down to a single
;; point. The average point is obtained by averaging all the x values to get an x value
;; and all the y values to get a y value. average-point also includes the distance
;; rating indicating how far the average point was from all the points. 
  
(define (average-point xs)
	(let ([avgpt (cons (/ (sumfst xs) (length xs)) (cons (/ (sumsnd xs) (length xs)) '()))])
    (cons (distance-product avgpt xs) (cons avgpt '()))))

	
;; Function: best-estimate
;; -----------------------
;; best-estimate takes a guess (a list of circles), computes all the points of intersection, 
;; winnows those points down to those which are most clumped, and returns their average point.	
		
(define (best-estimate xs) 
  (average-point (clumped-points (intersection-points xs))))

;; Function: myzip
;; ---------------
;; myzip takes two lists and returns a single list of tuples 
;; made from the input lists
  
(define (myzip xs ys) 
  (if (null? xs) '()
     (cons (list (car xs) (car ys)) (myzip (cdr xs) (cdr ys)))))

;; Function: where-am-i
;; --------------------
;; where-am-i takes list of distances and a list of star locations, and computes all the 
;; possible guesses, usomg best-estimate to get an answer out of each one, and sorts the 
;; estimates in increasing order of distance rating. 
;; where-am-i returns a list of rated points. 
	 
(define (where-am-i rs cs) 
  (map best-estimate (all-guesses rs cs)))
