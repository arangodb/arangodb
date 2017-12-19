!CHAPTER Queries without collections


Following is a query that returns a string value. The result string is contained in an array
because the result of every valid query is an array:

```js
RETURN "this will be returned"
[ 
  "this will be returned" 
]
```

Here is a query that creates the cross products of two arrays and runs a projection 
on it, using a few of AQL's built-in functions:

```js
FOR year in [ 2011, 2012, 2013 ]
  FOR quarter IN [ 1, 2, 3, 4 ]
    RETURN { 
      "y" : "year", 
      "q" : quarter, 
      "nice" : CONCAT(quarter, "/", year) 
    }
[ 
  { "y" : "year", "q" : 1, "nice" : "1/2011" }, 
  { "y" : "year", "q" : 2, "nice" : "2/2011" }, 
  { "y" : "year", "q" : 3, "nice" : "3/2011" }, 
  { "y" : "year", "q" : 4, "nice" : "4/2011" }, 
  { "y" : "year", "q" : 1, "nice" : "1/2012" }, 
  { "y" : "year", "q" : 2, "nice" : "2/2012" }, 
  { "y" : "year", "q" : 3, "nice" : "3/2012" }, 
  { "y" : "year", "q" : 4, "nice" : "4/2012" }, 
  { "y" : "year", "q" : 1, "nice" : "1/2013" }, 
  { "y" : "year", "q" : 2, "nice" : "2/2013" }, 
  { "y" : "year", "q" : 3, "nice" : "3/2013" }, 
  { "y" : "year", "q" : 4, "nice" : "4/2013" } 
]
```
