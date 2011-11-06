////////////////////////////////////////////////////////////////////////////////
/// @brief High-Performance Database Framework made by triagens
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2011 triagens GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Martin Schoenert
/// @author Copyright 2009-2011, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @page FruitSort Fruit Sort
///
/// @section Pre-processor directive FSRT_TYPE
///
/// The directive @ec FSRT_TYPE refers to the type of each element which is to
/// be sorted.  If this directive is undefined, then the type @c char* is used
/// instead, which may result in a reduction in the speed of the sort
/// algorithm. For example,
///
/// If you define @c FSRT_TYPE the corresponding macros functions will have the
/// following signatures:
///
/// <tt>void FSRT_NAME( FSRT_TYPE * fs_b, FSRT_TYPE * fs_e )</tt>                 @n
/// <tt>int  FSRT_LT( FSRT_TYPE * fs_x, FSRT_TYPE * fs_y, size_t fs_s )</tt>      @n
/// <tt>int  FSRT_EQ( FSRT_TYPE * fs_x, FSRT_TYPE * fs_y, size_t fs_s )</tt>      @n
/// <tt>int  FSRT_COMP( FSRT_TYPE * fs_x, FSRT_TYPE * fs_y, size_t fs_s )</tt>    @n
/// <tt>void FSRT_COPY( FSRT_TYPE * fs_x, FSRT_TYPE * fs_y, size_t fs_s )</tt>    @n
///
/// If @c FSRT_TYPE is undefined the corresponding macros functions will macros
/// have the following signatures:
///
/// <tt>void FSRT_NAME( char * fs_b, char * fs_e, size_t fs_s, char * fs_t )</tt> @n
/// <tt>int  FSRT_LT( char * fs_x, char * fs_y, size_t fs_s )</tt>                @n
/// <tt>int  FSRT_EQ( char * fs_x, char * fs_y, size_t fs_s )</tt>                @n
/// <tt>int  FSRT_COMP( char * fs_x, char * fs_y, size_t fs_s )</tt>              @n
/// <tt>void FSRT_COPY( char * fs_x, char * fs_y, size_t fs_s )</tt>              @n
///
/// @section Pre-processor directive FSRT_NAME
///
/// The pre-processor directive @c FSRT_NAME must be defined.
///
/// The function @c FSRT_NAME(fs_b,fs_e) sorts an array of statically sized
/// elements.  The parameters @c fs_b and @c fs_e are pointers or iterators
/// which access the first and last - 1 element.
///
/// The function @c FSRT_NAME(fs_b,fs_e,fs_s,fs_t) can be used to sort elements
/// whose complete type and size is unkown at compile time.  The additional
/// parameter @c fs_s refers to the size of the elements, while the parameter
/// @c fs_t refers to a location in memory used to store a temporary element.
///
/// @section Pre-processor directive FSRT_UNIQ
///
/// The directive @c FSRT_UNIQ provides a hint that the elements to be sorted
/// are unique (or little repetitions). In this case, the sorting algorithm will
/// use a 2-way partitioning of the elements into <= and >=. The default
/// alogorithm is a 3-way partitioning of the elements into <, = and >.
///
/// Define this directive when know that the number of repetitions is small.
///
/// @section Pre-processor directive FSRT_INSR
///
/// The directive @c FSRT_INSR represents the cutoff where the algorithm
/// switches from quick sort to insertion sort, i.e. subranges shorter than
/// @c FSRT_INSR will be sorted with Insertion sort algorithm rather than the
/// Quicksort algorithm.
///
/// Thus optimal value for @c FSRT_INSR is dependent on the costs associated
/// with the comparisons copies of elements.  The default value of 8 (when
/// @c FSRT_INSR is undefined) is reasonable most of the time.  If the cost of
/// comparisons and copies is small, you can use larger values (up to about 40)
/// to increase performance. Consider reducing the value of @c FSRT_INSR where
/// the cost of comparisons and copies is high (down to 4).
///
/// @section Pre-processor directive FSRT_THREE
///
/// The directive @c FSRT_THREE represents the cutoff for median of 3 pivot
/// selection.  Defaults to 10 unless otherwise defined. The directive
/// @c FSRT_NINE represents the cutoff for median of 9 pivot selection.
/// Defaults to 10 unless otherwise defined.
///
/// @section Pre-processor directive FSRT_EXTRA_ARGS
///
/// The directive @c FSRT_EXTRA_ARGS provides a list of additional arguments
/// which may be required by the copy and comparison functions.
/// @c FSRT_EXTRA_ARGS is a comma separated list of names of the additional
/// function arguments required. By default no additional arguments are defined.
/// The directive @c FSRT_EXTRA_DECL provides additional declarations for any
/// extra arguments which may be required. @c FSRT_EXTRA_DECL is a comman
/// separated list of declarations.
///
/// By default no additional declarations are defined. For example:
///
/// <tt>#define FSRT_EXTRA_ARGS     dsd, loc</tt>                             @n
/// <tt>#define FSRT_EXTRA_DECL     DataSetDescription* dsd, Locale* loc</tt> @n
/// <tt>#define FSRT_LT(x,y,s,d,l)  my_str_cmp(x,y,d,l)</tt>                  @n
///
/// @section Required Directives
///
/// @c FSRT_NAME and @c FSRT_NAM2.
///
/// @section Required Macros
///
/// @c FSRT_LT and @c FSRT_EQ or @c FSRT_COMP.
///
/// @section Optional Macros
///
/// @c FSRT_COPY, @c FSRT_TYPE, @c FSRT_UNIQ, @c FSRT_INSR, @c FSRT_THREE,
/// @c FSRT_NINE @c FSRT_EXTRA_ARGS, @c FSRT_EXTRA_DECL, @c FSRT_COUNT, @c
/// FSRT_DEBUG, @c FSRT_COUNT.
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup FruitSort Fruit Sort
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @def FSRT_COUNT
/// @brief statistics buffer
////////////////////////////////////////////////////////////////////////////////

#ifdef FSRT_COUNT
static uint32_t FSRT_Count [20];
#endif

////////////////////////////////////////////////////////////////////////////////
/// @def FSRT__TYPE
/// @brief the type of the elements which are begin sorted
///
/// Optional. Unless specified the type defaults to a @c char.
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @def FSRT__UNIT
/// @brief the size of an element specified in terms of the @c FSRT_TYPE
///
/// When the default value of @c FSRT_TYPE is @c char, this defaults
/// to the size of the elements in bytes. That is, @c fs_s.
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @def FSRT__SIZE
/// @brief the size of an element given in bytes
///
/// That is, @a fs_s.
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @def FSRT__TEMP
/// @brief the address of some memory storage set aside for an element
///
/// The address of some memory storage set aside at least as large as the size
/// of an element. This temporary storage is used by the function @c FSRT_NAME.
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @def FSRT__DIST
/// @brief The number of elements between a given two elements.
////////////////////////////////////////////////////////////////////////////////

#ifdef FSRT_TYPE

#define FSRT__TYPE                      FSRT_TYPE
#define FSRT__UNIT                      1
#define FSRT__SIZE                      sizeof(FSRT_TYPE)
#define FSRT__TEMP                      &fs_t
#define FSRT__DIST(fs_x,fs_y,fs_s)      ((fs_x) - (fs_y))

#else

#define FSRT__TYPE                      char
#define FSRT__UNIT                      (fs_s)
#define FSRT__SIZE                      (fs_s)
#define FSRT__TEMP                      fs_t
#define FSRT__DIST(fs_x,fs_y,fs_s)      (((fs_x) - (fs_y)) / (fs_s))

#endif

////////////////////////////////////////////////////////////////////////////////
/// @def FSRT__LT(x,y,s,d)
/// @brief less-than comparison
///
/// Returns true if @c x is less than @c y. Otherwise, returns false.
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @def FSRT__EQ(x,y,s,d)
/// @brief equal-than comparison
///
/// Returns the integer 0 if the elements @c x and @c y both of size @c s are
/// equal. A non-zero value is returned otherwise.
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @def FSRT__LEC(x,y,s,d)
/// @brief comparison
///
/// Sets the var @c fs_c to the result of a comparision of @c x and @c y.  A
/// value of 0 indicates equality between @c x and @c y. This variable is used
/// as part of the sorting algorithm.
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @def FSRT__EQC(x,y,s,d)
/// @brief equalitity test
///
/// Performs a test of equality between @c x and @c y. The value of the variable
/// @a fs_c contains an integer which is the result of a @em previous
/// comparision between @a x and @a y.
////////////////////////////////////////////////////////////////////////////////

#ifdef FSRT_LT

#ifndef FSRT__COUNT
#define FSRT__LT(fs_x,fs_y,fs_s,fs_d)   FSRT_LT((fs_x),(fs_y),(fs_s) FSRT__EXTRA_ARGS)
#define FSRT__EQ(fs_x,fs_y,fs_s,fs_d)   FSRT_EQ((fs_x),(fs_y),(fs_s) FSRT__EXTRA_ARGS)
#define FSRT__LEC(fs_x,fs_y,fs_s,fs_d)  (! FSRT_LT((fs_y),(fs_x),(fs_s) FSRT__EXTRA_ARGS))
#define FSRT__EQC(fs_x,fs_y,fs_s,fs_d)  FSRT_EQ((fs_x),(fs_y),(fs_s) FSRT__EXTRA_ARGS)
#else
#define FSRT__LT(fs_x,fs_y,fs_s,fs_d)   (FSRT_Count[(fs_d)]++, FSRT_LT((fs_x),(fs_y),(fs_s) FSRT__EXTRA_ARGS))
#define FSRT__EQ(fs_x,fs_y,fs_s,fs_d)   (FSRT_Count[(fs_d)]++, FSRT_EQ((fs_x),(fs_y),(fs_s) FSRT__EXTRA_ARGS))
#define FSRT__LEC(fs_x,fs_y,fs_s,fs_d)  (FSRT_Count[(fs_d)]++, (! FSRT_LT((fs_y),(fs_x),(fs_s) FSRT__EXTRA_ARGS)))
#define FSRT__EQC(fs_x,fs_y,fs_s,fs_d)  (FSRT_Count[(fs_d)]++, FSRT_EQ((fs_x),(fs_y),(fs_s) FSRT__EXTRA_ARGS))
#endif

#else

#ifndef FSRT__COUNT
#define FSRT__LT(fs_x,fs_y,fs_s,fs_d)   (FSRT_COMP((fs_x),(fs_y),(fs_s) FSRT__EXTRA_ARGS) < 0)
#define FSRT__EQ(fs_x,fs_y,fs_s,fs_d)   (FSRT_COMP((fs_x),(fs_y),(fs_s) FSRT__EXTRA_ARGS) == 0)
#define FSRT__LEC(fs_x,fs_y,fs_s,fs_d)  ((fs_c = FSRT_COMP((fs_x),(fs_y),(fs_s) FSRT__EXTRA_ARGS)) <= 0)
#define FSRT__EQC(fs_x,fs_y,fs_s,fs_d)  (fs_c == 0)
#else
#define FSRT__LT(fs_x,fs_y,fs_s,fs_d)   (FSRT_Count[(fs_d)]++, (FSRT_COMP((fs_x),(fs_y),(fs_s) FSRT__EXTRA_ARGS) < 0))
#define FSRT__EQ(fs_x,fs_y,fs_s,fs_d)   (FSRT_Count[(fs_d)]++, (FSRT_COMP((fs_x),(fs_y),(fs_s) FSRT__EXTRA_ARGS) == 0))
#define FSRT__LEC(fs_x,fs_y,fs_s,fs_d)  (FSRT_Count[(fs_d)]++, ((fs_c = FSRT_COMP((fs_x),(fs_y),(fs_s) FSRT__EXTRA_ARGS)) <= 0))
#define FSRT__EQC(fs_x,fs_y,fs_s,fs_d)  (FSRT_Count[(fs_d)]++, (fs_c == 0))
#endif

#endif

////////////////////////////////////////////////////////////////////////////////
/// @def FSRT__COPY
/// @brief copies one element onto another
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @def FSRT__SWAP
/// @brief swaps two elements
////////////////////////////////////////////////////////////////////////////////

#ifdef FSRT_COPY

#ifndef FSRT__COUNT
#define FSRT__COPY(fs_x,fs_y,fs_s,fs_d) FSRT_COPY((fs_x),(fs_y),(fs_s) FSRT__EXTRA_ARGS)
#else
#define FSRT__COPY(fs_x,fs_y,fs_s,fs_d) (FSRT_Count[(fs_d)]++, FSRT_COPY((fs_x),(fs_y),(fs_s) FSRT__EXTRA_ARGS))
#endif

#else

#ifndef FSRT__COUNT
#define FSRT__COPY(fs_x,fs_y,fs_s,fs_d) (memcpy( (char*)(fs_x), (char*)(fs_y), (fs_s) ))
#else
#define FSRT__COPY(fs_x,fs_y,fs_s,fs_d) (FSRT_Count[(fs_d)]++, (memcpy( (char*)(fs_x), (char*)(fs_y), (fs_s) )))
#endif

#endif

#define FSRT__SWAP(fs_x,fs_y,fs_s,fs_d) (FSRT__COPY( FSRT__TEMP, (fs_x), (fs_s), (fs_d) ), \
                                         FSRT__COPY( (fs_x), (fs_y), (fs_s), (0) ), \
                                         FSRT__COPY( (fs_y), FSRT__TEMP, (fs_s), (0) ))

////////////////////////////////////////////////////////////////////////////////
/// @def FSRT__INSR
/// @brief Cutoff point when to use the insertion short algorithm.
////////////////////////////////////////////////////////////////////////////////

#ifdef FSRT_INSR
#define FSRT__INSR                      FSRT_INSR
#else
#define FSRT__INSR                      8
#endif

////////////////////////////////////////////////////////////////////////////////
/// @def FSRT__THREE
/// @brief Used for sorting short arrays.
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @def FSRT__NINE
/// @brief Used for sorting short arrays.
////////////////////////////////////////////////////////////////////////////////

#ifdef FSRT_THREE
#define FSRT__THREE                     FSRT_THREE
#else
#define FSRT__THREE                     10
#endif
#ifdef FSRT_NINE
#define FSRT__NINE                      FSRT_NINE
#else
#define FSRT__NINE                      40
#endif

////////////////////////////////////////////////////////////////////////////////
/// @def FSRT__EXTRA_ARGS
/// @brief additional arguments which may be required for the algorithm
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @def FSRT__EXTRA_DECL
/// @brief additional declarations which may be required for the algorithm
////////////////////////////////////////////////////////////////////////////////

#ifdef FSRT_EXTRA_ARGS
#define FSRT__EXTRA_ARGS                , FSRT_EXTRA_ARGS
#define FSRT__EXTRA_DECL                , FSRT_EXTRA_DECL
#else
#define FSRT__EXTRA_ARGS                // none
#define FSRT__EXTRA_DECL                // none
#endif

////////////////////////////////////////////////////////////////////////////////
/// @fn FSRT__TYPE * FSRT_NAM2 (
///    FSRT__TYPE *        fs_x,
///    FSRT__TYPE *        fs_y,
///    FSRT__TYPE *        fs_z
///    FSRT__EXTRA_DECL )
///
/// @brief returns the middle element given three elements
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @def FSRT__MED3
/// @brief an alias for the FSRT_NAM2 function
////////////////////////////////////////////////////////////////////////////////

#ifdef FSRT_TYPE

#define FSRT__MED3(fs_x,fs_y,fs_z,fs_s) FSRT_NAM2((fs_x),(fs_y),(fs_z) FSRT__EXTRA_ARGS)

FSRT__TYPE * FSRT_NAM2 (
    FSRT__TYPE *        fs_x,
    FSRT__TYPE *        fs_y,
    FSRT__TYPE *        fs_z
    FSRT__EXTRA_DECL ) {

#else

#define FSRT__MED3(fs_x,fs_y,fs_z,fs_s) FSRT_NAM2((fs_x),(fs_y),(fs_z),(fs_s) FSRT__EXTRA_ARGS)

FSRT__TYPE * FSRT_NAM2 (
    FSRT__TYPE *        fs_x,
    FSRT__TYPE *        fs_y,
    FSRT__TYPE *        fs_z,
    size_t              fs_s
    FSRT__EXTRA_DECL ) {

#endif

  if ( FSRT__LT( fs_x, fs_y, FSRT__SIZE, 1 ) ) {
    if      ( FSRT__LT( fs_y, fs_z, FSRT__SIZE, 1 ) ) { return fs_y; }
    else if ( FSRT__LT( fs_x, fs_z, FSRT__SIZE, 1 ) ) { return fs_z; }
    else                                              { return fs_x; }
  }
  else {
    if      ( FSRT__LT( fs_x, fs_z, FSRT__SIZE, 1 ) ) { return fs_x; }
    else if ( FSRT__LT( fs_y, fs_z, FSRT__SIZE, 1 ) ) { return fs_z; }
    else                                              { return fs_y; }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @var FSRT_Rand
/// @brief stores the position of the current random element within the array
////////////////////////////////////////////////////////////////////////////////

#ifndef FSRT__RAND
uint32_t FSRT_Rand;
#endif

////////////////////////////////////////////////////////////////////////////////
/// @def FSRT__RAND
/// @brief computes a quasi-random element
////////////////////////////////////////////////////////////////////////////////

#ifndef FSRT__RAND
#define FSRT__RAND \
  ((fs_b) + FSRT__UNIT * ((FSRT_Rand = FSRT_Rand * 31415 + 27818) % FSRT__DIST(fs_e,fs_b,FSRT__SIZE)))
#endif

////////////////////////////////////////////////////////////////////////////////
/// @fn void FSRT_NAME (
///    FSRT__TYPE *        fs_b,
///    FSRT__TYPE *        fs_e
///    FSRT__EXTRA_DECL )
///
/// @brief The actual sorting algorithm.
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @def FSRT__SORT
///
/// @brief An alias for the sorting algorithm FSRT_NAME.
////////////////////////////////////////////////////////////////////////////////

#ifdef FSRT_TYPE

#define FSRT__SORT(fs_x,fs_y,fs_s,fs_t) FSRT_NAME((fs_x),(fs_y) FSRT__EXTRA_ARGS)

void FSRT_NAME (
    FSRT__TYPE *        fs_b,
    FSRT__TYPE *        fs_e
    FSRT__EXTRA_DECL ) {
    FSRT__TYPE *        fs_l;
    FSRT__TYPE *        fs_r;
    FSRT__TYPE *        fs_p;
#ifndef FSRT_UNIQ
    FSRT__TYPE *        fs_m;
    FSRT__TYPE *        fs_q;
#endif
    FSRT__TYPE          fs_t;

#else

#define FSRT__SORT(fs_x,fs_y,fs_s,fs_t) FSRT_NAME((fs_x),(fs_y),(fs_s),(fs_t) FSRT__EXTRA_ARGS)

void FSRT_NAME (
    FSRT__TYPE *        fs_b,
    FSRT__TYPE *        fs_e,
    size_t              fs_s,
    FSRT__TYPE *        fs_t
    FSRT__EXTRA_DECL ) {

  FSRT__TYPE *        fs_l;
  FSRT__TYPE *        fs_r;
  FSRT__TYPE *        fs_p;
#ifndef FSRT_UNIQ
  FSRT__TYPE *        fs_m;
  FSRT__TYPE *        fs_q;
#endif

#endif

#ifndef FSRT_LT
  int                 fs_c;
#endif

  // iterate while the number of elements in the range is large enough
  while ( FSRT__INSR <= FSRT__DIST(fs_e,fs_b,FSRT__SIZE) ) {

    // for larger ranges take the median-of-median of nine random elms
    if ( FSRT__NINE <= FSRT__DIST(fs_e,fs_b,FSRT__SIZE) ) {
      fs_l = FSRT__MED3( FSRT__RAND, FSRT__RAND, FSRT__RAND, FSRT__SIZE );
      fs_r = FSRT__MED3( FSRT__RAND, FSRT__RAND, FSRT__RAND, FSRT__SIZE );
      fs_p = FSRT__MED3( FSRT__RAND, FSRT__RAND, FSRT__RAND, FSRT__SIZE );
      fs_p = FSRT__MED3( fs_l, fs_r, fs_p, FSRT__SIZE );
    }

    // for smaller ranges take the median of three random elms
    else if ( FSRT__THREE <= FSRT__DIST(fs_e,fs_b,FSRT__SIZE) ) {
      fs_p = FSRT__MED3( FSRT__RAND, FSRT__RAND, FSRT__RAND, FSRT__SIZE );
    }

    // and for very small ranges take one random element
    else {
      fs_p = FSRT__RAND;
    }

    // swap the pivot to the beg pos (out of the way for partitioning)
    if ( fs_b != fs_p ) {
      FSRT__SWAP( fs_b, fs_p, FSRT__SIZE, 4 );
    }

#ifdef FSRT_UNIQ

    // 2-way partitioning into elements le and ge.
    // Swap elements smaller (or equal) to the pivot to the left,
    // and  elements larger  (or equal) to the pivot to the right.
    // Note that scanning pointers stop on and exchange equal elements
    // This is important if we sort arrays with few different keys
    // because after a few iterations the subranges contain only
    // elements with equal keys, and if we would scan over elements
    // equal to the pivot, the right pointer would scan all the way
    // down and the quick sort would degenerate to a quadratic algo.

#define FSRT__UNIQ__ALG1
#ifdef FSRT__UNIQ__ALG1

    // Note that this code is a lot trickier than it appears ;-)
    // It is faster than other more obvious formulations, because
    // it compares every element exactly once to the pivot and
    // it keeps the number of statements (especially jumps) minimal
    // so the optimizer can generate compact and fast code.

    fs_p = fs_b;
    fs_l = fs_b;
    fs_r = fs_e;

    // *(b==l) <= *p and *p <= *r .. *(e-1) since the set is empty
    while ( fs_l < fs_r ) {

      // *b .. *(l-1) *l <= *p  *p <= *r *(r+1) .. *(e-1)
      do { fs_r -= FSRT__UNIT; }
      while ( fs_l < fs_r && FSRT__LT( fs_p, fs_r, FSRT__SIZE, 2 ) );

      // *p >= *r  *p <= *(r+1) .. *(e-1)
      do { fs_l += FSRT__UNIT; }
      while ( fs_l < fs_r && FSRT__LT( fs_l, fs_p, FSRT__SIZE, 2 ) );

      // *b .. *(l-1) <= *p  and  ((l < r & *l >= *p) or (l >= r))
      if ( fs_l < fs_r ) {

        // .. *(l-1) <= *p  *l >= *p  *p >= *r  *p <= *(r+1) ..
        FSRT__SWAP( fs_l, fs_r, FSRT__SIZE, 4 );
        // *b .. *(l-1) *l <= *p  *p >= *r *(r+1) .. *(e-1)
      }
    }

    // l == r   and *b .. *(l-1) *(l==r) <= *p <= *(r+1) .. *(e-1)
    // l == r+1 and *b .. *(l-1==r) <= *p <= *(l==r+1) .. *(e-1)

    // swap the pivot to the middle
    FSRT__SWAP( fs_b, fs_r, FSRT__SIZE, 4 );
    fs_l = fs_r;
    fs_r = fs_l + FSRT__UNIT;

#endif

#ifdef FSRT__UNIQ__ALG2

    // This is a 2-way partitioning algorithm.
    // It performs exactely the same comparisons and swaps as alg1.
    // However it is formulated slightly different.
    // This is bad for the performance (the code is bit slower).
    // But I leave it in here, because this algorithm serves as a good
    // starting point for the 3-way partitioning algorithm below.

    fs_p = fs_b;
    fs_l = fs_b+FSRT__UNIT;
    fs_r = fs_e-FSRT__UNIT;

    while ( 1 ) {
      while ( fs_l <= fs_r && FSRT__LT( fs_p, fs_r, FSRT__SIZE, 2 ) ) {
        fs_r -= FSRT__UNIT;
      }

      while ( fs_l <  fs_r && FSRT__LT( fs_l, fs_p, FSRT__SIZE, 2 ) ) {
        fs_l += FSRT__UNIT;
      }

      if ( fs_r <= fs_l ) { break; }
      FSRT__SWAP( fs_l, fs_r, FSRT__SIZE, 4 );
      fs_r -= FSRT__UNIT;
      fs_l += FSRT__UNIT;
    }

    fs_l = fs_r + FSRT__UNIT;

    if ( fs_b < fs_r ) { FSRT__SWAP( fs_b, fs_r, FSRT__SIZE, 4 ); }
    fs_l -= FSRT__UNIT;
    fs_r += FSRT__UNIT;

#endif

#ifdef FSRT_DEBUG
    for ( fs_p = fs_b; fs_p < fs_e; fs_p++ ) {
      if ( fs_p < fs_l ) {
        if ( FSRT__LT( fs_l, fs_p, FSRT__SIZE, 0 ) ) {
          printf("fs_p %08x not less equal after partitioning\n",fs_p);
          break;
        }
      }
      else if ( fs_p < fs_r ) {
        if ( ! FSRT__EQ( fs_l, fs_p, FSRT__SIZE, 0 ) ) {
          printf("fs_p %08x not equal after partitioning\n",fs_p);
          break;
        }
      }
      else {
        if ( FSRT__LT( fs_p, fs_l, FSRT__SIZE, 0 ) ) {
          printf("fs_p %08x not greater equal after partitioning\n",fs_p);
          break;
        }
      }
    }
#endif

#else

    // 3-way partitioning into elements lt, eq, and gt
    // This is a bit slower than 2-way partitioning but a lot faster
    // if the range contains only elements with few different keys.
    // In the first step this does a split-end partitioning,
    // i.e. the elements equal to the pivot end up at the very left
    // (left of m) or at the very right (right of q) end of the range.
    // In the second step (down below) they are swapped to the middle.
    // This has the advantage, that it executes fewer swaps
    // (which is quite counter to the intuition).
    // It executes about 1/4 N swaps for the not-equal elements and
    // 2 E swaps for the equal elements (and even fewer if E > N).
    // The alternative (that keeps the equal elements during the
    // partitioning in the middle) executes about 3/4 N swaps for the
    // not-equal elements and 1 E swaps for the equal elements.

    fs_p = fs_b;
    fs_l = fs_b+FSRT__UNIT;
    fs_m = fs_b+FSRT__UNIT;
    fs_r = fs_e-FSRT__UNIT;
    fs_q = fs_e-FSRT__UNIT;

    while ( 1 ) {
      while ( fs_l <= fs_r && FSRT__LEC( fs_p, fs_r, FSRT__SIZE, 2 ) ) {
        if ( FSRT__EQC( fs_p, fs_r, FSRT__SIZE, 0 ) ) {
          if ( fs_r < fs_q ) { FSRT__SWAP( fs_r, fs_q, FSRT__SIZE, 4 ); }
          fs_q -= FSRT__UNIT;
        }
        fs_r -= FSRT__UNIT;
      }

      while ( fs_l <  fs_r && FSRT__LEC( fs_l, fs_p, FSRT__SIZE, 2 ) ) {
        if ( FSRT__EQC( fs_p, fs_l, FSRT__SIZE, 0 ) ) {
          if ( fs_m < fs_l ) { FSRT__SWAP( fs_m, fs_l, FSRT__SIZE, 4 ); }
          fs_m += FSRT__UNIT;
        }
        fs_l += FSRT__UNIT;
      }

      if ( fs_r <= fs_l ) { break; }

      FSRT__SWAP( fs_l, fs_r, FSRT__SIZE, 4 );
      fs_r -= FSRT__UNIT;
        fs_l += FSRT__UNIT;
    }

    fs_l = fs_r + FSRT__UNIT;

#ifdef FSRT_DEBUG
    // printf("%08x %08x %08x %08x %08x %08x\n",fs_b,fs_m,fs_l,fs_r,fs_q,fs_e);
    for ( fs_p = fs_b; fs_p < fs_e; fs_p++ ) {
      if ( fs_p < fs_m ) {
        if ( ! FSRT__EQ( fs_p, fs_b, FSRT__SIZE, 0 ) ) {
          printf("fs_p %08x < fs_m not equal after partitioning\n",fs_p);
          break;
        }
      }
      else if ( fs_p < fs_l ) {
        if ( ! FSRT__LT( fs_p, fs_b, FSRT__SIZE, 0 ) ) {
          printf("fs_p %08x < fs_l not less after partitioning\n",fs_p);
          break;
        }
      }
      if ( fs_q < fs_p ) {
        if ( ! FSRT__EQ( fs_b, fs_p, FSRT__SIZE, 0 ) ) {
          printf("fs_q < fs_p %08x not equal after partitioning\n",fs_p);
          break;
        }
      }
      else if ( fs_r < fs_p ) {
        if ( ! FSRT__LT( fs_b, fs_p, FSRT__SIZE, 0 ) ) {
          printf("fs_r < fs_p %08x not greater after partitioning\n",fs_p);
          break;
        }
      }
    }
#endif

    // Swap the equal elements (including the pivot) to the middle.
    // I found it not so easy to get this code correct, so beware ;-)
    fs_p = fs_b;

    while ( fs_p < fs_m && fs_m < fs_l ) {
      fs_l -= FSRT__UNIT;
      FSRT__SWAP( fs_p, fs_l, FSRT__SIZE, 4 );
      fs_p += FSRT__UNIT;
    }

    if ( fs_l == fs_m ) {
      fs_l = fs_p;
    }

    fs_p = fs_e-FSRT__UNIT;

    while ( fs_q < fs_p && fs_r < fs_q ) {
      fs_r += FSRT__UNIT;
      FSRT__SWAP( fs_r, fs_p, FSRT__SIZE, 4 );
      fs_p -= FSRT__UNIT;
    }

    if ( fs_r == fs_q ) {
      fs_r = fs_p;
    }

    fs_r = fs_r + FSRT__UNIT;

#ifdef FSRT_DEBUG

    for ( fs_p = fs_b; fs_p < fs_e; fs_p++ ) {
      if ( fs_p < fs_l ) {
        if ( ! FSRT__LT( fs_p, fs_l, FSRT__SIZE, 0 ) ) {
          printf("fs_p %08x < fs_l not less after swapping\n",fs_p);
          break;
        }
      }
      else if ( fs_p < fs_r ) {
        if ( ! FSRT__EQ( fs_l, fs_p, FSRT__SIZE, 0 ) ) {
          printf("fs_p %08x < fs_r not equal after swapping\n",fs_p);
          break;
        }
      }
      else {
        if ( ! FSRT__LT( fs_l, fs_p, FSRT__SIZE, 0 ) ) {
          printf("fs_r <= fs_p %08x not greater after swapping\n",fs_p);
          break;
        }
      }
    }
#endif

#endif

    // Now sort the range before and the range after the pivot.
    // Sort the smaller range recursively and the larger iteratively.
    // This limits the maximal redursion depth to log2(N).
    // To avoid extra variables we modify the arguments of the func.

    if ( fs_l-fs_b < fs_e-fs_r ) {
      FSRT__SORT( fs_b, fs_l, FSRT__SIZE, FSRT__TEMP );
      fs_b = fs_r;
    }
    else {
      FSRT__SORT( fs_r, fs_e, FSRT__SIZE, FSRT__TEMP );
      fs_e = fs_l;
    }
  }

  // Finally the remaining short subrange is sorted with insertion sort.
  // Sorting each individual short subrange instead of executing one
  // insertion sort over the entire range at the end is more efficient,
  // because we know that all the pivots (and all the elements equal to
  // them) are at the right location, so we need not compare them with
  // their predecessor and successor.
  // It may also be a bit more cache friendly.
  // I tried other algorithms (shell sort, special cases, etc.)
  // but in the end this algorithm always was the fastest
  // (probably because the subranges are so small).

  for ( fs_l = fs_b+FSRT__UNIT; fs_l < fs_e; fs_l += FSRT__UNIT ) {
    if ( FSRT__LT( fs_l, fs_l-FSRT__UNIT, FSRT__SIZE, 3 ) ) {
      FSRT__COPY( FSRT__TEMP, fs_l, FSRT__SIZE, 5 );
      FSRT__COPY( fs_l, fs_l-FSRT__UNIT, FSRT__SIZE, 5 );
      for ( fs_r = fs_l-2*FSRT__UNIT;
            fs_b <= fs_r && FSRT__LT( FSRT__TEMP, fs_r, FSRT__SIZE, 3 );
            fs_r -= FSRT__UNIT ) {
        FSRT__COPY( fs_r+FSRT__UNIT, fs_r, FSRT__SIZE, 5 );
      }
      FSRT__COPY( fs_r+FSRT__UNIT, FSRT__TEMP, FSRT__SIZE, 5 );
    }
  }
}

#undef FSRT__TYPE
#undef FSRT__UNIT
#undef FSRT__SIZE
#undef FSRT__DIST
#undef FSRT__TEMP
#undef FSRT__LT
#undef FSRT__EQ
#undef FSRT__LEC
#undef FSRT__EQC
#undef FSRT__COPY
#undef FSRT__SWAP
#undef FSRT__INSR
#undef FSRT__THREE
#undef FSRT__NINE
#undef FSRT__EXTRA_ARGS
#undef FSRT__EXTRA_DECL
#undef FSRT__MED3
// #undef FSRT__RAND
#undef FSRT__SORT

#undef FSRT_NAME
#undef FSRT_NAM2
#undef FSRT_LT
#undef FSRT_EQ
#undef FSRT_COMP
#undef FSRT_TYPE
#undef FSRT_COPY
#undef FSRT_UNIQ
#undef FSRT_INSR
#undef FSRT_THREE
#undef FSRT_NINE
#undef FSRT_EXTRA_ARGS
#undef FSRT_EXTRA_DECL

////////////////////////////////////////////////////////////////////////////////
//
// fsrt.h . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .  ends here
//
//  0.90 2008-11-20 mschoene initial version
//
//  0.95 2009-03-24 mschoene file is now called fsrt.h
//                           changed prefix to FSRT_
//                           parameters and local variables begin with fs_
//                           changed INSR default to 8
//                           added description for split-end partitioning
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\)"
// End:
