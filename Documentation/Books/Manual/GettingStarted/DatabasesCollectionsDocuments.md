Databases, Collections and Documents
====================================

Databases are sets of collections. Collections store records, which are referred
to as documents. Collections are the equivalent of tables in RDBMS, and
documents can be thought of as rows in a table. The difference is that you don't
define what columns (or rather attributes) there will be in advance. Every
document in any collection can have arbitrary attribute keys and
values. Documents in a single collection will likely have a similar structure in
practice however, but the database system itself does not impose it and will
operate stable and fast no matter how your data looks like.

Read more in the [data-model concepts](../DataModeling/Concepts.md) chapter.

For now, you can stick with the default `_system` database and use the web
interface to create collections and documents. Start by clicking the
*COLLECTIONS* menu entry, then the *Add Collection* tile. Give it a name, e.g.
*users*, leave the other settings unchanged (we want it to be a document
collection) and *Save* it. A new tile labeled *users* should show up, which
you can click to open.

There will be *No documents* yet. Click the green circle with the white plus
on the right-hand side to create a first document in this collection. A dialog
will ask you for a `_key`. You can leave the field blank and click *Create* to
let the database system assign an automatically generated (unique) key. Note
that the `_key` property is immutable, which means you can not change it once
the document is created. What you can use as document key is described in the
[naming conventions](../DataModeling/NamingConventions/DocumentKeys.md).

An automatically generated key could be `"9883"` (`_key` is always a string!),
and the document `_id` would be `"users/9883"` in that case. Aside from a few
system attributes, there is nothing in this document yet. Let's add a custom
attribute by clicking the icon to the left of *(empty object)*, then *Append*.
Two input fields will become available, *FIELD* (attribute key) and *VALUE*
(attribute value). Type `name` as key and your name as value. *Append* another
attribute, name it `age` and set it to your age. Click *Save* to persist the
changes. If you click on *Collection: users* at the top on the right-hand side
of the ArangoDB logo, the document browser will show the documents in the
*users* collection and you will see the document you just created in the list.
