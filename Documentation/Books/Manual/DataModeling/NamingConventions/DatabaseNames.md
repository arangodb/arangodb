!CHAPTER Database Names

ArangoDB will always start up with a default database, named *_system*.
Users can create additional databases in ArangoDB, provided the database
names conform to the following constraints:

* Database names must only consist of the letters *a* to *z* (both lower and
  upper case allowed), the numbers *0* to *9*, and the underscore (*_*) or 
  dash (*-*) symbols
  This also means that any non-ASCII database names are not allowed
* Database names must always start with a letter. Database names starting 
  with an underscore are considered to be system databases, and users should 
  not create or delete those
* The maximum allowed length of a database name is 64 bytes
* Database names are case-sensitive

