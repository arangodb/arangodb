DocumentRevision {#GlossaryDocumentRevision}
============================================

@GE{Document Revision}: As ArangoDB supports MVCC, documents can exist
in more than one revision.  The document revision is the MVCC token
used to identify a particular revision of a document. It is a string
value currently containing an integer number and is unique within the 
list of document revisions for a single document. 
Document revisions can be used to conditionally update, replace or
delete documents in the database. In order to find a particular
revision of a document, you need the document handle and the document
revision.

ArangoDB currently uses 64bit unsigned integer values to maintain
document revisions internally. When returning document revisions to 
clients, ArangoDB will put them into a string to ensure the revision id
is not clipped by clients that do not support big integers.
Clients should treat the revision id returned by ArangoDB as an 
opaque string when they store or use it locally. This will allow ArangoDB 
to change the format of revision ids later if this should be required.
Clients can use revisions ids to perform simple equality/non-equality 
comparisons (e.g. to check whether a document has changed or not), but 
they should not use revision ids to perform greater/less than comparisons
with them to check if a document revision is older than one another,
even if this might work for some cases. 

Note: Revision ids have been returned as integers up to including 
ArangoDB 1.1
