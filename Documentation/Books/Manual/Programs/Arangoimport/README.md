Arangoimport
============

This manual describes the ArangoDB importer _arangoimport_, which can be used for
bulk imports.

The most convenient method to import a lot of data into ArangoDB is to use the
*arangoimport* command-line tool. It allows you to import data records from a file
into an existing database collection.

It is possible to import [document keys](../../Appendix/Glossary.md#document-key) with the documents using the *_key*
attribute. When importing into an [edge collection](../../Appendix/Glossary.md#edge-collection), it is mandatory that all
imported documents have the *_from* and *_to* attributes, and that they contain
valid references.

Let's assume for the following examples you want to import user data into an
existing collection named "users" on the server.
