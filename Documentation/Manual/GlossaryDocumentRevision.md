DocumentRevision {#GlossaryDocumentRevision}
============================================

@GE{Document Revision}: As ArangoDB supports MVCC, documents can exist
in more than one revision.  The document revision is the MVCC token
used to identify a particular revision of a document. It is an integer
and unique within the list of document revision for a single
document. Earlier revisions of a document have smaller numbers.
Document revisions can be used to conditionally update, replace or
delete documents in the database. In order to find a particular
revision of a document, you need the document handle and the document
revision.

ArangoDB currently uses 64bit unsigned integer values for document
revisions. As this datatype is not portable to all client languages,
clients should rather treat the revision as an opaque string when they
store or use document revision values locally.
