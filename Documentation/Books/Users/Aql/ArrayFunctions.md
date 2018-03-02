Array functions
===============

AQL supports the following functions to operate on array values:

- *LENGTH(array)*: Returns the length (number of array elements) of *array*. If 
  *array* is an object / document, returns the number of attribute keys of the document, 
  regardless of their values. If *array* is a collection, returns the number of documents
  in it.

  Note: In v8 mode, this functions returns the number of utf8 code points not the number of characters.

- *FLATTEN(array, depth)*: Turns an array of arrays into a flat array. All 
  array elements in *array* will be expanded in the result array. Non-array elements 
  are added as they are. The function will recurse into sub-arrays up to a depth of
  *depth*. *depth* has a default value of 1.

*Examples*
  
      FLATTEN([ 1, 2, [ 3, 4 ], 5, [ 6, 7 ], [ 8, [ 9, 10 ] ] ])

  will produce:

      [ 1, 2, 3, 4, 5, 6, 7, 8, [ 9, 10 ] ]

  To fully flatten the array, use a *depth* of 2:
      
      FLATTEN([ 1, 2, [ 3, 4 ], 5, [ 6, 7 ], [ 8, [ 9, 10 ] ] ], 2)

  This will produce:
      
      [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ]

- *MIN(array)*: Returns the smallest element of *array*. *null* values
  are ignored. If the array is empty or only *null* values are contained in the array, the
  function will return *null*.

- *MAX(array)*: Returns the greatest element of *array*. *null* values
  are ignored. If the array is empty or only *null* values are contained in the array, the
  function will return *null*.

- *AVERAGE(array)*: Returns the average (arithmetic mean) of the values in *array*. 
  This requires the elements in *array* to be numbers. *null* values are ignored. 
  If the array is empty or only *null* values are contained in the array, the function 
  will return *null*.

- *SUM(array)*: Returns the sum of the values in *array*. This
  requires the elements in *array* to be numbers. *null* values are ignored. 

- *MEDIAN(array)*: Returns the median value of the values in *array*. This 
  requires the elements in *array* to be numbers. *null* values are ignored. If the 
  array is empty or only *null* values are contained in the array, the function will return 
  *null*.

- *PERCENTILE(array, n, method)*: Returns the *n*th percentile of the values in *array*. 
  This requires the elements in *array* to be numbers. *null* values are ignored. *n* must
  be between 0 (excluded) and 100 (included). *method* can be *rank* or *interpolation*.
  The function will return null if the array is empty or only *null* values are contained 
  in it or the percentile cannot be calculated.

- *VARIANCE_POPULATION(array)*: Returns the population variance of the values in 
  *array*. This requires the elements in *array* to be numbers. *null* values 
  are ignored. If the array is empty or only *null* values are contained in the array, 
  the function will return *null*.

- *VARIANCE_SAMPLE(array)*: Returns the sample variance of the values in 
  *array*. This requires the elements in *array* to be numbers. *null* values 
  are ignored. If the array is empty or only *null* values are contained in the array, 
  the function will return *null*.

- *STDDEV_POPULATION(array)*: Returns the population standard deviation of the 
  values in *array*. This requires the elements in *array* to be numbers. *null* 
  values are ignored. If the array is empty or only *null* values are contained in the array, 
  the function will return *null*.

- *STDDEV_SAMPLE(array)*: Returns the sample standard deviation of the values in 
  *array*. This requires the elements in *array* to be numbers. *null* values 
  are ignored. If the array is empty or only *null* values are contained in the array, 
  the function will return *null*.

- *REVERSE(array)*: Returns the elements in *array* in reversed order.

- *FIRST(array)*: Returns the first element in *array* or *null* if the
  array is empty.

- *LAST(array)*: Returns the last element in *array* or *null* if the
  array is empty.

- *NTH(array, position)*: Returns the array element at position *position*.
  Positions start at 0. If *position* is negative or beyond the upper bound of the array
  specified by *array*, then *null* will be returned.

- *POSITION(array, search, return-index)*: Returns the position of the
  element *search* in *array*. Positions start at 0. If the element is not 
  found, then *-1* is returned. If *return-index* is *false*, then instead of the
  position only *true* or *false* are returned, depending on whether the sought element
  is contained in the array.

- *SLICE(array, start, length)*: Extracts a slice of the array specified
  by *array*. The extraction will start at array element with position *start*. 
  Positions start at 0. Up to *length* elements will be extracted. If *length* is
  not specified, all array elements starting at *start* will be returned.
  If *start* is negative, it can be used to indicate positions from the end of the
  array.

*Examples*

      SLICE([ 1, 2, 3, 4, 5 ], 0, 1)
      
  will return *[ 1 ]*
  
      SLICE([ 1, 2, 3, 4, 5 ], 1, 2)
      
  will return *[ 2, 3 ]*
  
      SLICE([ 1, 2, 3, 4, 5 ], 3) 
  
  will return *[ 4, 5 ]*
  
      SLICE([ 1, 2, 3, 4, 5 ], 1, -1) 
      
  will return *[ 2, 3, 4 ]*
  
      SLICE([ 1, 2, 3, 4, 5 ], 0, -2)
      
  will return *[ 1, 2, 3 ]*

- *UNIQUE(array)*: Returns all unique elements in *array*. To determine
  uniqueness, the function will use the comparison order.
  Calling this function may return the unique elements in any order.

- *UNION(array1, array2, ...)*: Returns the union of all arrays specified.
  The function expects at least two array values as its arguments. The result is an array
  of values in an undefined order.

  Note: No duplicates will be removed. In order to remove duplicates, please use either
  *UNION_DISTINCT* function or apply the *UNIQUE* on the result of *union*.

*Examples*

      RETURN UNION(
        [ 1, 2, 3 ],
        [ 1, 2 ]
      )

  will produce:

      [ [ 1, 2, 3, 1, 2 ] ]

  with duplicate removal:

      RETURN UNIQUE(
        UNION(
          [ 1, 2, 3 ],
          [ 1, 2 ]
        )
      )
  
  will produce:

      [ [ 1, 2, 3 ] ]

- *UNION_DISTINCT(array1, array2, ...)*: Returns the union of distinct values of
  all arrays specified. The function expects at least two array values as its arguments. 
  The result is an array of values in an undefined order.

- *MINUS(array1, array2, ...)*: Returns the difference of all arrays specified.
  The function expects at least two array values as its arguments.
  The result is an array of values that occur in the first array but not in any of the
  subsequent arrays. The order of the result array is undefined and should not be relied on.
  Note: duplicates will be removed.

- *INTERSECTION(array1, array2, ...)*: Returns the intersection of all arrays specified.
  The function expects at least two array values as its arguments.
  The result is an array of values that occur in all arguments. The order of the result array
  is undefined and should not be relied on.
  
  Note: Duplicates will be removed.

- *APPEND(array, values, unique)*: Adds all elements from the array *values* to the array
  specified by *array*. If *unique* is set to true, then only those *values* will be added
  that are not already contained in *array*.
  The modified array is returned. All values are added at the end of the array (right side).

      /* [ 1, 2, 3, 5, 6, 9 ] */
      APPEND([ 1, 2, 3 ], [ 5, 6, 9 ])

      /* [ 1, 2, 3, 4, 5, 9 ] */
      APPEND([ 1, 2, 3 ], [ 3, 4, 5, 2, 9 ], true)

- *PUSH(array, value, unique)*: Adds *value* to the array specified by *array*. If
  *unique* is set to true, then *value* is not added if already present in the array. 
  The modified array is returned. The value is added at the end of the array (right side).

  Note: non-unique elements will not be removed from the array if they were already present 
  before the call to `PUSH`. The *unique* flag will only control if the value will
  be added again to the array if already present. To make a array unique, use the `UNIQUE` 
  function.

      /* [ 1, 2, 3, 4 ] */
      PUSH([ 1, 2, 3 ], 4)

      /* [ 1, 2, 3 ] */
      PUSH([ 1, 2, 3 ], 2, true)

- *UNSHIFT(array, value, unique)*: Adds *value* to the array specified by *array*. If
  *unique* is set to true, then *value* is not added if already present in the array.
  The modified array is returned. The value is added at the start of the array (left side).
  
  Note: non-unique elements will not be removed from the array if they were already present 
  before the call to `UNSHIFT`. The *unique* flag will only control if the value will
  be added again to the array if already present. To make a array unique, use the `UNIQUE` 
  function.

      /* [ 4, 1, 2, 3 ] */
      UNSHIFT([ 1, 2, 3 ], 4)

      /* [ 1, 2, 3 ] */
      UNSHIFT([ 1, 2, 3 ], 2, true)

- *POP(array)*: Removes the element at the end (right side) of *array*. The modified array 
  is returned. If the array is already empty or *null*, an empty array is returned. 

      /* [ 1, 2, 3 ] */
      POP([ 1, 2, 3, 4 ])

- *SHIFT(array)*: Removes the element at the start (left side) of *array*. The modified array 
  is returned. If the array is already empty or *null*, an empty array is returned. 

      /* [ 2, 3, 4 ] */
      SHIFT([ 1, 2, 3, 4 ])

- *REMOVE_VALUE(array, value, limit)*: Removes all occurrences of *value* in the array
  specified by *array*. If the optional *limit* is specified, only *limit* occurrences
  will be removed.

      /* [ "b", "b", "c" ] */
      REMOVE_VALUE([ "a", "b", "b", "a", "c" ], "a")

      /* [ "b", "b", "a", "c" ] */
      REMOVE_VALUE([ "a", "b", "b", "a", "c" ], "a", 1)

- *REMOVE_VALUES(array, values)*: Removes all occurrences of any of the values specified
  in array *values* from the array specified by *array*.
 
      /* [ "b", "c", "e", "g" ] */
      REMOVE_VALUES([ "a", "b", "c", "d", "e", "f", "g" ], [ "a", "f", "d" ])

- *REMOVE_NTH(array, position)*: Removes the element at position *position* from the
  array specified by *array*. Positions start at 0. Negative positions are supported, 
  with -1 being the last array element. If *position* is out of bounds, the array is
  returned unmodified. Otherwise, the modified array is returned.
 
      /* [ "a", "c", "d", "e" ] */
      REMOVE_NTH([ "a", "b", "c", "d", "e" ], 1)

      /* [ "a", "b", "c", "e" ] */
      REMOVE_NTH([ "a", "b", "c", "d", "e" ], -2)

Apart from these functions, AQL also offers several language constructs (e.g.
*FOR*, *SORT*, *LIMIT*, *COLLECT*) to operate on arrays.
