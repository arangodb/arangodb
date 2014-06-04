<a name="importing_into_edge_collections"></a>
# Importing into Edge Collections

Please note that when importing documents into an edge collection, it is 
mandatory that all imported documents contain the `_from` and `_to` attributes,
and that these contain references to existing collections.

Please also note that it is not possible to create a new edge collection on the
fly using the `createCollection` parameter.
