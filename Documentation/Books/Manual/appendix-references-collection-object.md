---
layout: default
description: The following methods exist on the collection object (returned by db
---
The "collection" Object
=======================

The following methods exist on the collection object (returned by *db.name*):

*Collection*

* [collection.checksum()](data-modeling-collections-collection-methods.html#checksum)
* [collection.compact()](data-modeling-collections-collection-methods.html#compact)
* [collection.count()](data-modeling-documents-document-methods.html#count)
* [collection.drop()](data-modeling-collections-collection-methods.html#drop)
* [collection.figures()](data-modeling-collections-collection-methods.html#figures)
* [collection.getResponsibleShard()](data-modeling-collections-collection-methods.html#getresponsibleshard)
* [collection.load()](data-modeling-collections-collection-methods.html#load)
* [collection.properties(options)](data-modeling-collections-collection-methods.html#properties)
* [collection.revision()](data-modeling-collections-collection-methods.html#revision)
* [collection.rotate()](data-modeling-collections-collection-methods.html#rotate)
* [collection.shards()](data-modeling-collections-collection-methods.html#shards)
* [collection.toArray()](data-modeling-documents-document-methods.html#toarray)
* [collection.truncate()](data-modeling-collections-collection-methods.html#truncate)
* [collection.type()](data-modeling-documents-document-methods.html#collection-type)
* [collection.unload()](data-modeling-collections-collection-methods.html#unload)

*Indexes*

* [collection.dropIndex(index)](indexing-working-with-indexes.html#dropping-an-index-via-a-collection-handle)
* [collection.ensureIndex(description)](indexing-working-with-indexes.html#creating-an-index)
* [collection.getIndexes(name)](indexing-working-with-indexes.html#listing-all-indexes-of-a-collection)
* [collection.index(index)](indexing-working-with-indexes.html#index-identifiers-and-handles)

*Document*

* [collection.all()](data-modeling-documents-document-methods.html#all)
* [collection.any()](data-modeling-documents-document-methods.html#any)
* [collection.byExample(example)](data-modeling-documents-document-methods.html#query-by-example)
* [collection.closedRange(attribute, left, right)](data-modeling-documents-document-methods.html#closed-range)
* [collection.document(object)](data-modeling-documents-document-methods.html#document)
* [collection.documents(keys)](data-modeling-documents-document-methods.html#lookup-by-keys)
* [collection.edges(vertex-id)](data-modeling-documents-document-methods.html#edges)
* [collection.exists(object)](data-modeling-documents-document-methods.html#exists)
* [collection.firstExample(example)](data-modeling-documents-document-methods.html#first-example)
* [collection.inEdges(vertex-id)](data-modeling-documents-document-methods.html#edges)
* [collection.insert(data)](data-modeling-documents-document-methods.html#insert--save)
* [collection.edges(vertices)](data-modeling-documents-document-methods.html#edges)
* [collection.iterate(iterator,options)](data-modeling-documents-document-methods.html#misc)
* [collection.outEdges(vertex-id)](data-modeling-documents-document-methods.html#edges)
* [collection.range(attribute, left, right)](data-modeling-documents-document-methods.html#range)
* [collection.remove(selector)](data-modeling-documents-document-methods.html#remove)
* [collection.removeByExample(example)](data-modeling-documents-document-methods.html#remove-by-example)
* [collection.removeByKeys(keys)](data-modeling-documents-document-methods.html#remove-by-keys)
* [collection.rename()](data-modeling-collections-collection-methods.html#rename)
* [collection.replace(selector, data)](data-modeling-documents-document-methods.html#replace)
* [collection.replaceByExample(example, data)](data-modeling-documents-document-methods.html#replace-by-example)
* [collection.save(data)](data-modeling-documents-document-methods.html#insert--save)
* [collection.update(selector, data)](data-modeling-documents-document-methods.html#update)
* [collection.updateByExample(example, data)](data-modeling-documents-document-methods.html#update-by-example)
