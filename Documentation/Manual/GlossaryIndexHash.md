HashIndex {#GlossaryIndexHash}
==============================

@GE{Hash Index}: A hash index is used to find documents based on
examples. A hash index can be created for one or multiple document
attributes. 

A hash index will only be used by queries if all indexed attributes 
are present in the example or search query, and if all attributes are
compared using the equality (`==` operator). That means the hash index
does not support range queries. 

If the index is declared `unique`, then access to the indexed attributes
should be fast. The performance degrades if the indexed attribute(s)
contain(s) only very few distinct values.
