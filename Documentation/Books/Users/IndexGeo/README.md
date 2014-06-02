<a name="geo_indexes"></a>
# Geo Indexes

<a name="introduction_to_geo_indexes"></a>
### Introduction to Geo Indexes

This is an introduction to ArangoDB's geo indexes.

ArangoDB uses Hilbert curves to implement geo-spatial indexes. See this [blog](http://www.arangodb.org/2012/03/31/using-hilbert-curves-and-polyhedrons-for-geo-indexing)
for details.

A geo-spatial index assumes that the latitude is between -90 and 90 degree and
the longitude is between -180 and 180 degree. A geo index will ignore all
documents which do not fulfill these requirements.

A geo-spatial constraint makes the same assumptions, but documents not
fulfilling these requirements are rejected.

<a name="accessing_geo_indexes_from_the_shell"></a>
## Accessing Geo Indexes from the Shell

@anchor IndexGeoShellEnsureGeoIndex
@copydetails JSF_ArangoCollection_prototype_ensureGeoIndex

@CLEARPAGE
@anchor IndexGeoShellEnsureGeoConstraint
@copydetails JSF_ArangoCollection_prototype_ensureGeoConstraint

@CLEARPAGE
@anchor IndexGeoShellGeo
@copydetails JSF_ArangoCollection_prototype_geo

@CLEARPAGE
@anchor IndexGeoShellNear
@copydetails JSF_ArangoCollection_prototype_near

@CLEARPAGE
@anchor IndexGeoShellWithin
@copydetails JSF_ArangoCollection_prototype_within
