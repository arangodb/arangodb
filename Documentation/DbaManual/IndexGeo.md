Geo Indexes{#IndexGeo}
======================

@NAVIGATE_IndexGeo
@EMBEDTOC{IndexGeoTOC}

Introduction to Geo Indexes{#IndexGeoIntro}
===========================================

This is an introduction to ArangoDB's geo indexes.

ArangoDB uses Hilbert curves to implement geo-spatial indexes. See this <a
href="http://www.arangodb.org/2012/03/31/using-hilbert-curves-and-polyhedrons-for-geo-indexing">blog</a>
for details.

A geo-spatial index assumes that the latitude is between -90 and 90 degree and
the longitude is between -180 and 180 degree. A geo index will ignore all
documents which do not fulfill these requirements.

A geo-spatial constraint makes the same assumptions, but documents not
fulfilling these requirements are rejected.

Accessing Geo Indexes from the Shell{#IndexGeoShell}
====================================================

@anchor IndexGeoShellEnsureGeoIndex
@copydetails JS_EnsureGeoIndexVocbaseCol

@CLEARPAGE
@anchor IndexGeoShellEnsureGeoConstraint
@copydetails JS_EnsureGeoConstraintVocbaseCol

@CLEARPAGE
@anchor IndexGeoShellGeo
@copydetails JSF_ArangoCollection_prototype_geo

@CLEARPAGE
@anchor IndexGeoShellNear
@copydetails JSF_ArangoCollection_prototype_near

@CLEARPAGE
@anchor IndexGeoShellWithin
@copydetails JSF_ArangoCollection_prototype_within
