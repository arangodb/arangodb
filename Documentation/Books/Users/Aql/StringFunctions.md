String functions
================

For string processing, AQL offers the following functions:

- *CONCAT(value1, value2, ... valueN)*: Concatenate the strings
  passed as in *value1* to *valueN*. *null* values are ignored. Array value arguments
  are expanded automatically, and their individual members will be concatenated.

      /* "foobarbaz" */
      CONCAT('foo', 'bar', 'baz')

      /* "foobarbaz" */
      CONCAT([ 'foo', 'bar', 'baz' ])

- *CONCAT_SEPARATOR(separator, value1, value2, ... valueN)*:
  Concatenate the strings passed as arguments *value1* to *valueN* using the
  *separator* string. *null* values are ignored. Array value arguments
  are expanded automatically, and their individual members will be concatenated.

      /* "foo, bar, baz" */
      CONCAT_SEPARATOR(', ', 'foo', 'bar', 'baz')

      /* "foo, bar, baz" */
      CONCAT_SEPARATOR(', ', [ 'foo', 'bar', 'baz' ])

- *CHAR_LENGTH(value)*: Return the number of characters in *value*.

  Note: This functions returns the number of utf8 code points not the number of characters.

- *LOWER(value)*: Lower-case *value*

- *UPPER(value)*: Upper-case *value*

- *SUBSTITUTE(value, search, replace, limit)*: Replaces search values in the string
  *value*. If *search* is a string, all occurrences of *search* will be replaced in
  *value*. If *search* is a list, each occurrence of a value contained in *search*
  will be replaced by the corresponding list item in *replace*. If *replace* has less
  list items than *search*, occurrences of unmapped *search* items will be replaced
  by the empty string. The number of replacements can optionally be limited using the
  *limit* parameter. If the *limit* is reached, no further occurrences of the search
  values will be replaced.

      /* "the lazy brown foxx" */ 
      SUBSTITUTE("the quick brown foxx", "quick", "lazy")

      /* "the slow brown dog" */ 
      SUBSTITUTE("the quick brown foxx", [ "quick", "foxx" ], [ "slow", "dog" ])       

      /* "A VOID! brown " */ 
      SUBSTITUTE("the quick brown foxx", [ "the", "quick", "foxx" ], [ "A", "VOID!" ])

      /* "the xx brown xx" */
      SUBSTITUTE("the quick brown foxx", [ "quick", "foxx" ], "xx" )                   

  Alternatively, *search* and *replace* can be specified in a combined value:

      /* "the small slow ant" */ 
      SUBSTITUTE("the quick brown foxx", { 
        "quick" : "small", 
        "brown" : "slow", 
        "foxx" : "ant" 
      }) 

- *SUBSTRING(value, offset, length)*: Return a substring of *value*,
  starting at *offset* and with a maximum length of *length* characters. Offsets
  start at position 0. Length is optional and if omitted the substring from *offset*
  to the end of the string will be returned.

- *LEFT(value, LENGTH)*: Returns the *LENGTH* leftmost characters of
  the string *value*

- *RIGHT(value, LENGTH)*: Returns the *LENGTH* rightmost characters of
  the string *value*

- *TRIM(value, type)*: Returns the string *value* with whitespace stripped 
  from the start and/or end. The optional *type* parameter specifies from which parts
  of the string the whitespace is stripped:

  - *type* 0 will strip whitespace from the start and end of the string
  - *type* 1 will strip whitespace from the start of the string only
  - *type* 2 will strip whitespace from the end of the string only

- *TRIM(value, chars)*: Returns the string *value* with whitespace stripped 
  from the start and end. The optional *chars* parameter can be used to override the
  characters that should be removed from the string. It defaults to `\r\n\t `
  (i.e. `0x0d`, `0x0a`, `0x09` and `0x20`).

      /* "foobar" */
      TRIM("  foobar\t \r\n ")         

      /* "foo;bar;baz" */
      TRIM(";foo;bar;baz, ", ",; ")     

- *LTRIM(value, chars)*: Returns the string *value* with whitespace stripped 
  from the start only. The optional *chars* parameter can be used to override the
  characters that should be removed from the string. It defaults to `\r\n\t `
  (i.e. `0x0d`, `0x0a`, `0x09` and `0x20`).

- *RTRIM(value, chars)*: Returns the string *value* with whitespace stripped 
  from the end only. The optional *chars* parameter can be used to override the
  characters that should be removed from the string. It defaults to `\r\n\t `
  (i.e. `0x0d`, `0x0a`, `0x09` and `0x20`).

- *SPLIT(value, separator, limit)*: Splits the given string *value* into a list of
  strings, using the *separator*. The *separator* can either be a string or a
  list of strings. If the *separator* is the empty string, *value* will be split
  into a list of characters. If no *separator* is specified, *value* will be
  returned inside a list.
  The optional parameter *limit* can be used to limit the number of split values in
  the result. If no *limit* is given, the number of splits returned is not bounded. 

- *REVERSE(value)*: Returns the reverse of the string *value*

- *CONTAINS(text, search, return-index)*: Checks whether the string
  *search* is contained in the string *text*. By default, this function returns 
  *true* if *search* is contained in *text*, and *false* otherwise. By
  passing *true* as the third function parameter *return-index*, the function
  will return the position of the first occurrence of *search* within *text*, 
  starting at offset 0, or *-1* if *search* is not contained in *text*.

  The string matching performed by *CONTAINS* is case-sensitive.

* *FIND_FIRST(text, search, start, end)*: Returns the position of the first
  occurrence of the string *search* inside the string *text*. Positions start at 
  zero. If *search* is not contained in *text*, -1 is returned. The search can 
  optionally be limited to a subset of *text* using the *start* and *end* arguments.

* *FIND_LAST(text, search, start, end)*: Returns the position of the last
  occurrence of the string *search* inside the string *text*. Positions start at 
  zero. If *search* is not contained in *text*, -1 is returned. The search can 
  optionally be limited to a subset of *text* using the *start* and *end* arguments.

- *LIKE(text, search, case-insensitive)*: Checks whether the pattern
  *search* is contained in the string *text*, using wildcard matching. 
  Returns *true* if the pattern is contained in *text*, and *false* otherwise. 
  The *pattern* string can contain the wildcard characters *%* (meaning any
  sequence of characters) and *_* (any single character).

  The string matching performed by *LIKE* is case-sensitive by default, but by
  passing *true* as the third parameter, the matching will be case-insensitive.

  The value for *search* cannot be a variable or a document attribute. The actual 
  value must be present at query parse time already.

- *MD5(text)*: calculates the MD5 checksum for *text* and returns it in a 
  hexadecimal string representation.

- *SHA1(text)*: calculates the SHA1 checksum for *text* and returns it in a 
  hexadecimal string representation.

- *RANDOM_TOKEN(length)*: generates a pseudo-random token string with the 
  specified length. *length* must be greater than zero and at most 65536. The
  generated token may consist of lower and uppercase letters and numbers. The
  algorithm for token generation should be treated as opaque.

