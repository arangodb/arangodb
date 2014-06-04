<a name="geo_queries"></a>
# Geo Queries

The ArangoDB allows to select documents based on geographic coordinates. In
order for this to work, a geo-spatial index must be defined.  This index will
use a very elaborate algorithm to lookup neighbors that is a magnitude faster
than a simple R* index.

In general a geo coordinate is a pair of latitude and longitude.  This can
either be a list with two elements like `[-10, +30]` (latitude first, followed
by longitude) or an object like `{lon: -10, lat: +30`}.  In order to find all
documents within a given radius around a coordinate use the *within*
operator. In order to find all documents near a given document use the *near*
operator.

It is possible to define more than one geo-spatial index per collection.  In
this case you must give a hint using the *geo* operator which of indexes
should be used in a query.

`collection.near( latitude, longitude)`

The default will find at most 100 documents near the coordinate ( latitude, longitude). The returned list is sorted according to the distance, with the nearest document coming first. If there are near documents of equal distance, documents are chosen randomly from this set until the limit is reached. It is possible to change the limit using the limit operator.

In order to use the near operator, a geo index must be defined for the collection. This index also defines which attribute holds the coordinates for the document. If you have more then one geo-spatial index, you can use the geo operator to select a particular index.

Note: near does not support negative skips. However, you can still use limit followed to skip.

`collection.near( latitude, longitude).limit( limit)`

Limits the result to limit documents instead of the default 100.

Note: Unlike with multiple explicit limits, limit will raise the implicit default limit imposed by within.
collection.near( latitude, longitude).distance()
This will add an attribute distance to all documents returned, which contains the distance between the given point and the document in meter.

`collection.near( latitude, longitude).distance( name)`
This will add an attribute name to all documents returned, which contains the distance between the given point and the document in meter.

*Examples*

To get the nearest two locations:

	arango> db.geo.near(0,0).limit(2).toArray();
	[ { "_id" : "geo/24773376", "_key" : "24773376", "_rev" : "24773376", "name" : "Name/0/0", "loc" : [ 0, 0 ] }, 
	  { "_id" : "geo/22348544", "_key" : "22348544", "_rev" : "22348544", "name" : "Name/-10/0", "loc" : [ -10, 0 ] } ]

If you need the distance as well, then you can use the distance operator:

	arango> db.geo.near(0,0).distance().limit(2).toArray();
	[ 
	  { "_id" : geo/24773376", "_key" : "24773376", "_rev" : "24773376", "distance" : 0, "name" : "Name/0/0", "loc" : [ 0, 0 ] },
	  { "_id" : geo/22348544", "_key" : "22348544", "_rev" : "22348544", "distance" : 1111949.3, "name" : "Name/-10/0", "loc" : [ -10, 0 ] } 
	  ]

`collection.within( latitude, longitude, radius)`

This will find all documents with in a given radius around the coordinate ( latitude, longitude). The returned list is sorted by distance.

In order to use the within operator, a geo index must be defined for the collection. This index also defines which attribute holds the coordinates for the document. If you have more then one geo-spatial index, you can use the geo operator to select a particular index.

`collection.within( latitude, longitude, radius).distance()`

This will add an attribute _distance to all documents returned, which contains the distance between the given point and the document in meter.

`collection.within( latitude, longitude, radius).distance( name)`

This will add an attribute name to all documents returned, which contains the distance between the given point and the document in meter.

*Examples*

To find all documents within a radius of 2000 km use:

	arango> db.geo.within(0, 0, 2000 * 1000).distance().toArray();
	[ { "_id" : "geo/24773376", "_key" : "24773376", "_rev" : "24773376", "distance" : 0, "name" : "Name/0/0", "loc" : [ 0, 0 ] }, 
	  { "_id" : "geo/24707840", "_key" : "24707840", "_rev" : "24707840", "distance" : 1111949.3, "name" : "Name/0/-10", "loc" : [ 0, -10 ] },
	  { "_id" : "geo/24838912", "_key" : "24838912", "_rev" : "24838912", "distance" : 1111949.3, "name" : "Name/0/10", "loc" : [ 0, 10 ] },
	  { "_id" : "geo/22348544", "_key" : "22348544", "_rev" : "22348544", "distance" : 1111949.3, "name" : "Name/-10/0", "loc" : [ -10, 0 ] },
	  { "_id" : "geo/27198208", "_key" : "27198208", "_rev" : "27198208", "distance" : 1111949.3, "name" : "Name/10/0", "loc" : [ 10, 0 ] },
	  { "_id" : "geo/22414080", "_key" : "22414080", "_rev" : "22414080", "distance" : 1568520.6, "name" : "Name/-10/10", "loc" : [ -10, 10 ] },
	  { "_id" : "geo/27263744", "_key" : "27263744", "_rev" : "27263744", "distance" : 1568520.6, "name" : "Name/10/10", "loc" : [ 10, 10 ] },
	  { "_id" : "geo/22283008", "_key" : "22283008", "_rev" : "22283008", "distance" : 1568520.6, "name" : "Name/-10/-10", "loc" : [ -10, -10 ] },
	  { "_id" : "geo/27132672", "_key" : "27132672", "_rev" : "27132672", "distance" : 1568520.6, "name" : "Name/10/-10", "loc" : [ 10, -10 ] } ]

`collection.geo( location)`

The next near or within operator will use the specific geo-spatial index.

`collection.geo( location, true)`

The next near or within operator will use the specific geo-spatial index.

`collection.geo( latitude, longitude)`

The next near or within operator will use the specific geo-spatial index.

*Examples*

Assume you have a location stored as list in the attribute home and a destination stored in the attribute work. Than you can use the geo operator to select, which coordinates to use in a near query.

	arango> for (i = -90;  i <= 90;  i += 10) {
	.......>   for (j = -180;  j <= 180;  j += 10) {
	.......>     db.complex.save({ name : "Name/" + i + "/" + j, 
	.......>                       home : [ i, j ], 
	.......>                       work : [ -i, -j ] });
	.......>   }
	.......> }
	
	arango> db.complex.near(0, 170).limit(5);
	exception in file '/simple-query' at 1018,5: a geo-index must be known
	
	arango> db.complex.ensureGeoIndex(""home"");
	arango> db.complex.near(0, 170).limit(5).toArray();
	[ { "_id" : "complex/74655276", "_key" : "74655276", "_rev" : "74655276", "name" : "Name/0/170", "home" : [ 0, 170 ], "work" : [ 0, -170 ] },
	  { "_id" : "complex/74720812", "_key" : "74720812", "_rev" : "74720812", "name" : "Name/0/180", "home" : [ 0, 180 ], "work" : [ 0, -180 ] }, 
	  { "_id" : "complex/77080108", "_key" : "77080108", "_rev" : "77080108", "name" : "Name/10/170", "home" : [ 10, 170 ], "work" : [ -10, -170 ] },
	  { "_id" : "complex/72230444", "_key" : "72230444", "_rev" : "72230444", "name" : "Name/-10/170", "home" : [ -10, 170 ], "work" : [ 10, -170 ] },
	  { "_id" : "complex/72361516", "_key" : "72361516", "_rev" : "72361516", "name" : "Name/0/-180", "home" : [ 0, -180 ], "work" : [ 0, 180 ] } ]      

	arango> db.complex.geo("work").near(0, 170).limit(5);
	exception in file '/simple-query' at 1018,5: a geo-index must be known
	
	arango> db.complex.ensureGeoIndex("work");
	arango> db.complex.geo("work").near(0, 170).limit(5).toArray();
	[ { "_id" : "complex/72427052", "_key" : "72427052", "_rev" : "72427052", "name" : "Name/0/-170", "home" : [ 0, -170 ], "work" : [ 0, 170 ] }, 
	  { "_id" : "complex/72361516", "_key" : "72361516", "_rev" : "72361516", "name" : "Name/0/-180", "home" : [ 0, -180 ], "work" : [ 0, 180 ] }, 
	  { "_id" : "complex/70002220", "_key" : "70002220", "_rev" : "70002220", "name" : "Name/-10/-170", "home" : [ -10, -170 ], "work" : [ 10, 170 ] }, 
	  { "_id" : "complex/74851884", "_key" : "74851884", "_rev" : "74851884", "name" : "Name/10/-170", "home" : [ 10, -170 ], "work" : [ -10, 170 ] }, 
	  { "_id" : "complex/74720812", "_key" : "74720812", "_rev" : "74720812", "name" : "Name/0/180", "home" : [ 0, 180 ], "work" : [ 0, -180 ] } ]


<!--
@anchor SimpleQueryNear
@copydetails JSF_ArangoCollection_prototype_near

@CLEARPAGE
@anchor SimpleQueryWithin
@copydetails JSF_ArangoCollection_prototype_within

@CLEARPAGE
@anchor SimpleQueryGeo
@copydetails JSF_ArangoCollection_prototype_geo
-->