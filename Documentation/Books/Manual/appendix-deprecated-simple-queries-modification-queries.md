---
layout: default
description: It is recommended to use AQL instead, see Data Modification Queries
---
Modification Queries
====================

{% hint 'warning' %}
It is recommended to use AQL instead, see [**Data Modification Queries**](aql/data-queries.html#data-modification-queries).
{% endhint %}

ArangoDB also allows removing, replacing, and updating documents based 
on an example document. Every document in the collection will be 
compared against the specified example document and be deleted/replaced/
updated if all attributes match.

These method should be used with caution as they are intended to remove or
modify lots of documents in a collection.

All methods can optionally be restricted to a specific number of operations.
However, if a limit is specific but is less than the number of matches, it
will be undefined which of the matching documents will get removed/modified.
[Remove by Example](data-modeling-documents-document-methods.html#remove-by-example),
 [Replace by Example](data-modeling-documents-document-methods.html#replace-by-example) and 
[Update by Example](data-modeling-documents-document-methods.html#update-by-example)
 are described with examples in the subchapter 
[Collection Methods](data-modeling-documents-document-methods.html).  