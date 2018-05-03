Geo utility functions
---------------------

The following helper functions **can** use geo indexes, but do not have to in
all cases. You can use all of these functions in combination with each other,
and if you have  configured a geo index it may be used. For more information see
the [geo index page](../../Manual/Indexing/Geo.html)

### GEO_DISTANCE()

`GEO_DISTANCE(geoJSONA, geoJSONB) → double`

Return the distance between two GeoJSON objects, measured from the **centroid**
of each shape (For a list of supported Types see [geo index
page](../../Manual/Indexing/Geo.html#geojson)).
- **geoJSONA** first GeoJSON object
- **geoJSONB** second GeoJSON object.
- returns **double**: the distance between the centroid points of the two
  objects.

```js
LET polygon = {
  type:"Polygon",
  coordinates: [[[-11.5, 23.5], [-10.5, 26.1], [-11.2, 27.1], [-11.5, 23.5]]]
}
FOR doc IN collectionName
  LET distance = GEO_DISTANCE(doc.geometry, polygon) // calculates the distance
  RETURN distance
```

### DISTANCE()

`DISTANCE(latitude1, longitude1, latitude2, longitude2) → distance`

Calculate the distance between two arbitrary coordinates in meters (as birds
would fly). The value is computed using the haversine formula, which is based
on a spherical Earth model. It's fast to compute and is accurate to around 0.3%,
which is sufficient for most use cases such as location-aware services.

- **latitude1** (number): the latitude portion of the first coordinate
- **longitude1** (number): the longitude portion of the first coordinate
- **latitude2** (number): the latitude portion of the second coordinate
- **longitude2** (number): the longitude portion of the second coordinate
- returns **distance** (number): the distance between both coordinates in **meters**

```js
// Distance from Brandenburg Gate (Berlin) to ArangoDB headquarters (Cologne)
DISTANCE(52.5163, 13.3777, 50.9322, 6.94) // 476918.89688380965 (~477km)

// Sort a small number of documents based on distance to Central Park (New York)
FOR doc IN doc // e.g. documents returned by a traversal
  SORT DISTANCE(doc.latitude, doc.longitude, 40.78, -73.97)
  RETURN doc
```

### GEO_CONTAINS()

`GEO_CONTAINS(geoJSONA, geoJSONB) → bool`

Checks whether the [GeoJSON object](../../Manual/Indexing/Geo.html#geojson) `geoJSONA`
fully contains `geoJSONB` (Every point in B is also in A). The object `geoJSONA` has to be of type 
`Polygon` or `MultiPolygon`, other types are not supported because containment is ill defined. 
This function can be **optimized** via a S2 based geospatial index.

- **geoJSONA** first GeoJSON object or coordinate array (in longitude, latitude order)
- **geoJSONB** second GeoJSON object or coordinate array (in longitude, latitude order)
- returns **bool**: true when every point in B is also contained in A, false otherwise

### GEO_INTERSECTS()

`GEO_INTERSECTS(geoJSONA, geoJSONB) → bool`

Checks whether the [GeoJSON object](../../Manual/Indexing/Geo.html#geojson) `geoJSONA`
intersects with `geoJSONB` (At least one point in B is also A or vice-versa). 
This function can be **optimized** via a S2 based geospatial index, please look at the [relevant](../../Manual/Indexing/Geo.html) manual section for more information.

- **geoJSONA** first GeoJSON object
- **geoJSONB** second GeoJSON object.
- returns **bool**: true if B intersects A, false otherwise

### GEO_EQUALS()

`GEO_EQUALS(geoJSONA, geoJSONB) → bool`

Checks whether two GeoJSON objects are equal or not. (For a list of supported
Types see [geo index page](../../Manual/Indexing/Geo.html#geojson)).

- **geoJSONA** first GeoJSON object
- **geoJSONB** second GeoJSON object.
- returns **bool**: true for equality.

```js
LET polygonA = GEO_POLYGON([
  [-11.5, 23.5], [-10.5, 26.1], [-11.2, 27.1], [-11.5, 23.5]
])
LET polygonB = GEO_POLYGON([
  [-11.5, 23.5], [-10.5, 26.1], [-11.2, 27.1], [-11.5, 23.5]
])
RETURN GEO_EQUALS(polygonA, polygonB)
```

```
// true
```

```js
LET polygonA = GEO_POLYGON([
  [-11.1, 24.0], [-10.5, 26.1], [-11.2, 27.1], [-11.1, 24.0]
])
LET polygonB = GEO_POLYGON([
  [-11.5, 23.5], [-10.5, 26.1], [-11.2, 27.1], [-11.5, 23.5]
])
RETURN GEO_EQUALS(polygonA, polygonB)
```

```
// false
```

### IS_IN_POLYGON()

Determine whether a coordinate is inside a polygon.

Note: the *IS_IN_POLYGON* AQL function is **deprecated** as of ArangoDB 3.4.0 in
favor of the new `GEO_CONTAINS` AQL function, which works with
[GeoJSON](https://tools.ietf.org/html/rfc7946) Polygons and MultiPolygons.

`IS_IN_POLYGON(polygon, latitude, longitude) → bool`

- **polygon** (array): an array of arrays with 2 elements each, representing the
  points of the polygon in the format *[lat, lon]*
- **latitude** (number): the latitude portion of the search coordinate
- **longitude** (number): the longitude portion of the search coordinate
- returns **bool** (bool): *true* if the point (*latitude*, *longitude*) is
  inside the *polygon* or *false* if it's not. The result is undefined (can be
  *true* or *false*) if the specified point is exactly on a boundary of the
  polygon.

```js
// will check if the point (lat 4, lon 7) is contained inside the polygon
IS_IN_POLYGON( [ [ 0, 0 ], [ 0, 10 ], [ 10, 10 ], [ 10, 0 ] ], 4, 7 )
```

`IS_IN_POLYGON(polygon, coord, useLonLat) → bool`

The 2nd parameter can alternatively be specified as an array with two values.

By default, each array element in *polygon* is expected to be in the format
*[lat, lon]*. This can be changed by setting the 3rd parameter to *true* to
interpret the points as *[lon, lat]*. *coord* will then also be interpreted in
the same way.

- **polygon** (array): an array of arrays with 2 elements each, representing the
  points of the polygon
- **coord** (array): the search coordinate as a number array with two elements
- **useLonLat** (bool, *optional*): if set to *true*, the coordinates in
  *polygon* and the search coordinate *coord* will be interpreted as
  *[lon, lat]* (GeoJSON). The default is *false* and the format *[lat, lon]* is
  expected.
- returns **bool** (bool): *true* if the point *coord* is inside the *polygon*
  or *false* if it's not. The result is undefined (can be *true* or *false*) if
  the specified point is exactly on a boundary of the polygon.

```js
// will check if the point (lat 4, lon 7) is contained inside the polygon
IS_IN_POLYGON( [ [ 0, 0 ], [ 0, 10 ], [ 10, 10 ], [ 10, 0 ] ], [ 4, 7 ] )

// will check if the point (lat 4, lon 7) is contained inside the polygon
IS_IN_POLYGON( [ [ 0, 0 ], [ 10, 0 ], [ 10, 10 ], [ 0, 10 ] ], [ 7, 4 ], true )
```

Geo Index Functions **(Deprecated)**
-------------------

Note: Below AQL functions are *deprecated*, please use above geo utility
functions.

AQL offers the following functions to filter data based on [geo
indexes](../../Manual/Indexing/Geo.html). These functions require the collection
to have at least one geo index. If no geo index can be found, calling this
function will fail with an error at runtime. There is no error when explaining
the query however.

### NEAR()

`NEAR(coll, latitude, longitude, limit, distanceName) → docArray`

Return at most *limit* documents from collection *coll* that are near *latitude*
and *longitude*. The result contains at most *limit* documents, returned sorted
by distance, with closest distances being returned first. If more than *limit*
documents qualify, with the distance being exactly the same among multiple
documents around the limit, it is undefined which of the qualifying documents
are returned. Optionally, the distances in meters between the specified
coordinate (*latitude* and *longitude*) and the document coordinates can be
returned as well. To make use of that, the desired attribute  name for the
distance result has to be specified in the *distanceName* argument. The result
documents will contain the distance value in an attribute of that name.

- **coll** (collection): a collection
- **latitude** (number): the latitude portion of the search coordinate
- **longitude** (number): the longitude portion of the search coordinate
- **limit** (number, *optional*): cap the result to at most this number of
  documents. The default is 100. If more documents than *limit* are found, it is
  undefined which ones will be returned.
- **distanceName** (string, *optional*): include the distance to the search
  coordinate in each document in the result (in meters), using the attribute
  name *distanceName*
- returns **docArray** (array): an array of documents, sorted by distance
  (shortest distance first)


**Note:** `NEAR` is a *deprecated* AQL function, instead use a query like this:

```js
FOR doc IN doc
  SORT DISTANCE(doc.latitude, doc.longitude, paramLatitude, paramLongitude) ASC
  RETURN doc
```
Assuming there exists a geo-type index on `latitude` and `longitude`, the
optimizer will recognize it and accelerate the query.

### WITHIN()

`WITHIN(coll, latitude, longitude, radius, distanceName) → docArray`

Return all documents from collection *coll* that are within a radius of *radius*
around the specified coordinate (*latitude* and *longitude*). The documents
returned are sorted by distance to the search coordinate, with the closest
distances being returned first. Optionally, the distance in meters between the
search coordinate and the document coordinates can be returned as well. To make
use of that, an attribute name for the distance result has to be specified in
the *distanceName* argument. The result documents will contain the distance
value in an attribute of that name.

- **coll** (collection): a collection
- **latitude** (number): the latitude portion of the search coordinate
- **longitude** (number): the longitude portion of the search coordinate
- **radius** (number): radius in meters
- **distanceName** (string, *optional*): include the distance to the search
  coordinate in each document in the result (in meters), using the attribute
  name *distanceName*
- returns **docArray** (array): an array of documents, sorted by distance
  (shortest distance first)

**Note:** `WITHIN` is a *deprecated* AQL function, instead use a query like this:
```js
FOR doc IN doc
  LET d = DISTANCE(doc.latitude, doc.longitude, paramLatitude, paramLongitude)
	FILTER d <= radius
	SORT d ASC     
  RETURN doc
```

Assuming there exists a geo-type index on `latitude` and `longitude`, the
optimizer will recognize it and accelerate the query.

### WITHIN_RECTANGLE()

`WITHIN_RECTANGLE(coll, latitude1, longitude1, latitude2, longitude2) → docArray`

Return all documents from collection *coll* that are positioned inside the
bounding rectangle with the points (*latitude1*, *longitude1*) and (*latitude2*,
*longitude2*). There is no guaranteed order in which the documents are returned.

- **coll** (collection): a collection
- **latitude1** (number): the bottom-left latitude portion of the search
  coordinate
- **longitude1** (number): the bottom-left longitude portion of the search
  coordinate
- **latitude2** (number): the top-right latitude portion of the search
  coordinate
- **longitude2** (number): the top-right longitude portion of the search
  coordinate
- returns **docArray** (array): an array of documents, in random order

**Note:** `WITHIN_RECTANGLE` is a *deprecated* AQL function, instead use a query
using a GeoJSON polygon:

```js
LET rect = {type:"Polygon", coordinates:[[[longitude1, latitude1], ...]]]}
FOR doc IN doc
  FILTER GEO_CONTAINS(poly, [doc.longitude, doc.latitude])
  RETURN doc
```
Assuming there exists a geo-type index on `latitude` and `longitude`, the
optimizer will recognize it and accelerate the query.
