Geo functions
=============

AQL offers the following functions to filter data based on [geo indexes](../Glossary/README.md#geo-index):

- *NEAR(collection, latitude, longitude, limit, distancename)*: 
  Returns at most *limit* documents from collection *collection* that are near
  *latitude* and *longitude*. The result contains at most *limit* documents, returned in
  any order. If more than *limit* documents qualify, it is undefined which of the qualifying
  documents are returned. Optionally, the distances between the specified coordinate
  (*latitude* and *longitude*) and the document coordinates can be returned as well.
  To make use of that, an attribute name for the distance result has to be specified in
  the *distancename* argument. The result documents will contain the distance value in
  an attribute of that name.
  *limit* is an optional parameter since ArangoDB 1.3. If it is not specified or null, a limit
  value of 100 will be applied.

- *WITHIN(collection, latitude, longitude, radius, distancename)*: 
  Returns all documents from collection *collection* that are within a radius of
  *radius* around that specified coordinate (*latitude* and *longitude*). The order
  in which the result documents are returned is undefined. Optionally, the distance between the
  coordinate and the document coordinates can be returned as well.
  To make use of that, an attribute name for the distance result has to be specified in
  the *distancename* argument. The result documents will contain the distance value in
  an attribute of that name.

* *WITHIN_RECTANGLE(collection, latitude1, longitude1, latitude2, longitude2)*:
  Returns all documents from collection *collection* that are positioned inside the bounding
  rectangle with the points (*latitude1*, *longitude1*) and (*latitude2*, *longitude2*).

Note: these functions require the collection *collection* to have at least
one geo index.  If no geo index can be found, calling this function will fail
with an error.

- *IS_IN_POLYGON(polygon, latitude, longitude)*:
  Returns `true` if the point (*latitude*, *longitude*) is inside the polygon specified in the
  *polygon* parameter. The result is undefined (may be `true` or `false`) if the specified point
  is exactly on a boundary of the polygon.

  *latitude* can alternatively be specified as an array with two values. By default,
  the first array element will be interpreted as the latitude value and the second array element
  as the longitude value. This can be changed by setting the 3rd parameter to `true`.
  *polygon* needs to be an array of points, with each point being an array with two values. The
  first value of each point is considered to be the latitude value and the second to be the
  longitude value, unless the 3rd parameter has a value of `true`. That means latitude and
  longitude need to be specified in the same order in the *points* parameter as they are in
  the search coordinate.

  Examples:

      /* will check if the point (lat 4, lon 7) is contained inside the polygon */
      RETURN IS_IN_POLYGON([ [ 0, 0 ], [ 0, 10 ], [ 10, 10 ], [ 10, 0 ] ], 4, 7)

      /* will check if the point (lat 4, lon 7) is contained inside the polygon */
      RETURN IS_IN_POLYGON([ [ 0, 0 ], [ 0, 10 ], [ 10, 10 ], [ 10, 0 ] ], [ 4, 7 ])
      
      /* will check if the point (lat 4, lon 7) is contained inside the polygon */
      RETURN IS_IN_POLYGON([ [ 0, 0 ], [ 10, 0 ], [ 10, 10 ], [ 0, 10 ] ], [ 7, 4 ], true)
