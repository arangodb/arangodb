!CHAPTER Working with Documents using REST

<!-- arangod/RestHandler/RestDocumentHandler.cpp -->
@startDocuBlock REST_DOCUMENT_READ

!SUBSUBSECTION Changes in 3.0 from 2.8:

The *rev* query parameter has been withdrawn. The same effect can be
achieved with the *If-Match* HTTP header.

<!-- arangod/RestHandler/RestDocumentHandler.cpp -->
@startDocuBlock REST_DOCUMENT_READ_HEAD

!SUBSUBSECTION Changes in 3.0 from 2.8:

The *rev* query parameter has been withdrawn. The same effect can be
achieved with the *If-Match* HTTP header.

<!-- arangod/RestHandler/RestDocumentHandler.cpp -->
@startDocuBlock REST_DOCUMENT_READ_ALL

!SUBSUBSECTION Changes in 3.0 from 2.8:

The collection name should now be specified in the URL path. The old
way with the URL path */_api/document* and the required query parameter
*collection* still works.

<!-- arangod/RestHandler/RestDocumentHandler.cpp -->
@startDocuBlock REST_DOCUMENT_CREATE

!SUBSUBSECTION Changes in 3.0 from 2.8:

The collection name should now be specified in the URL path. The old
way with the URL path */_api/document* and the required query parameter
*collection* still works. The possibility to insert multiple documents
with one operation is new and the query parameter *returnNew* has been added.


<!-- arangod/RestHandler/RestDocumentHandler.cpp -->
@startDocuBlock REST_DOCUMENT_REPLACE

!SUBSUBSECTION Changes in 3.0 from 2.8:

There are quite some changes in this in comparison to Version 2.8, but
few break existing usage:

  - the *rev* query parameter is gone (was duplication of If-Match)
  - the *policy* query parameter is gone (was non-sensical)
  - the *ignoreRevs* query parameter is new, the default *true* gives 
    the traditional behavior as in 2.8
  - the *returnNew* and *returnOld* query parameters are new

There should be very few changes to behavior happening in real-world
situations or drivers. Essentially, one has to replace usage of the
*rev* query parameter by usage of the *If-Match* header. The non-sensical
combination of *If-Match* given and *policy=last* no longer works, but can
easily be achieved by leaving out the *If-Match* header.

The collection name should now be specified in the URL path. The old
way with the URL path */_api/document* and the required query parameter
*collection* still works.

<!-- arangod/RestHandler/RestDocumentHandler.cpp -->
@startDocuBlock REST_DOCUMENT_REPLACE_MULTI

!SUBSUBSECTION Changes in 3.0 from 2.8:

The multi document version is new in 3.0.

<!-- arangod/RestHandler/RestDocumentHandler.cpp -->
@startDocuBlock REST_DOCUMENT_UPDATE

!SUBSUBSECTION Changes in 3.0 from 2.8:

There are quite some changes in this in comparison to Version 2.8, but
few break existing usage:

  - the *rev* query parameter is gone (was duplication of If-Match)
  - the *policy* query parameter is gone (was non-sensical)
  - the *ignoreRevs* query parameter is new, the default *true* gives 
    the traditional behavior as in 2.8
  - the *returnNew* and *returnOld* query parameters are new

There should be very few changes to behavior happening in real-world
situations or drivers. Essentially, one has to replace usage of the
*rev* query parameter by usage of the *If-Match* header. The non-sensical
combination of *If-Match* given and *policy=last* no longer works, but can
easily be achieved by leaving out the *If-Match* header.

The collection name should now be specified in the URL path. The old
way with the URL path */_api/document* and the required query parameter
*collection* still works.

<!-- arangod/RestHandler/RestDocumentHandler.cpp -->
@startDocuBlock REST_DOCUMENT_UPDATE_MULTI

!SUBSUBSECTION Changes in 3.0 from 2.8:

The multi document version is new in 3.0.

<!-- arangod/RestHandler/RestDocumentHandler.cpp -->
@startDocuBlock REST_DOCUMENT_DELETE

!SUBSUBSECTION Changes in 3.0 from 2.8:

There are only very few changes in this in comparison to Version 2.8:

  - the *rev* query parameter is gone (was duplication of If-Match)
  - the *policy* query parameter is gone (was non-sensical)
  - the *returnOld* query parameter is new

There should be very few changes to behavior happening in real-world
situations or drivers. Essentially, one has to replace usage of the
*rev* query parameter by usage of the *If-Match* header. The non-sensical
combination of *If-Match* given and *policy=last* no longer works, but can
easily be achieved by leaving out the *If-Match* header.

<!-- arangod/RestHandler/RestDocumentHandler.cpp -->
@startDocuBlock REST_DOCUMENT_DELETE_MULTI

!SUBSUBSECTION Changes in 3.0 from 2.8:

This variant is new in 3.0. Note that it requires a body in the DELETE
request.
