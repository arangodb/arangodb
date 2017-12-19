!CHAPTER Geo Queries

The ArangoDB allows to select documents based on geographic coordinates. In
order for this to work, a geo-spatial index must be defined.  This index will
use a very elaborate algorithm to lookup neighbors that is a magnitude faster
than a simple R* index.

In general a geo coordinate is a pair of latitude and longitude, which must
both be specified as numbers. A geo index can be created on coordinates that
are stored in a single list attribute with two elements like *[-10, +30]* 
(latitude first, followed by longitude) or on coordinates stored in two 
separate attributes.

For example, to index the following documents, an index can be created on the
*position* attribute of the documents:

    db.test.save({ position: [ -10, 30 ] });
    db.test.save({ position: [ 10, 45.5 ] });

    db.test.ensureIndex({ type: "geo", fields: [ "position" ] });

If coordinates are stored in two distinct attributes, the index must be created
on the two attributes:

    db.test.save({ latitude: -10, longitude: 30 });
    db.test.save({ latitude: 10, longitude: 45.5 });

    db.test.ensureIndex({ type: "geo", fields: [ "latitude", "longitude" ] });

In order to find all documents within a given radius around a coordinate use 
the *within* operator. In order to find all documents near a given document 
use the *near* operator.

It is possible to define more than one geo-spatial index per collection.  In
this case you must give a hint using the *geo* operator which of indexes
should be used in a query.

!SUBSECTION Near 
<!-- js/common/modules/@arangodb/arango-collection-common.js-->
@startDocuBlock collectionNear

!SUBSECTION Within
<!-- js/common/modules/@arangodb/arango-collection-common.js-->
@startDocuBlock collectionWithin

!SUBSECTION Geo
<!-- js/common/modules/@arangodb/arango-collection-common.js-->
@startDocuBlock collectionGeo

!SUBSECTION Related topics

Other ArangoDB geographic features are described in: 
- [AQL Geo functions](../../../AQL/Functions/Geo.html)
- [Geo indexes](../../Indexing/Geo.md)  

