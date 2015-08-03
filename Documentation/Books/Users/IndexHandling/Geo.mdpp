!CHAPTER Geo Indexes

!SUBSECTION Introduction to Geo Indexes

This is an introduction to ArangoDB's geo indexes.

ArangoDB uses Hilbert curves to implement geo-spatial indexes. 
See this [blog](https://www.arangodb.com/2012/03/31/using-hilbert-curves-and-polyhedrons-for-geo-indexing)
for details.

A geo-spatial index assumes that the latitude is between -90 and 90 degree and
the longitude is between -180 and 180 degree. A geo index will ignore all
documents which do not fulfill these requirements.

!SECTION Accessing Geo Indexes from the Shell

<!-- js/server/modules/org/arangodb/arango-collection.js-->
@startDocuBlock collectionEnsureGeoIndex

<!-- js/common/modules/org/arangodb/arango-collection-common.js-->
@startDocuBlock collectionGeo

<!-- js/common/modules/org/arangodb/arango-collection-common.js-->
@startDocuBlock collectionNear

<!-- js/common/modules/org/arangodb/arango-collection-common.js-->
@startDocuBlock collectionWithin


@startDocuBlock collectionEnsureGeoConstraint
