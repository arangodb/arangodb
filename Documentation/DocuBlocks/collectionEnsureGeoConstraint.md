

@brief ensures that a geo index exists
`collection.ensureIndex({ type: "geo", fields: [ "location" ] })`

Since ArangoDB 2.5, this method is an alias for *ensureGeoIndex* since 
geo indexes are always sparse, meaning that documents that do not contain
the index attributes or have non-numeric values in the index attributes
will not be indexed.

