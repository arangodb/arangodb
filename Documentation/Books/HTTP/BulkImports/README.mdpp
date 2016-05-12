!CHAPTER HTTP Interface for Bulk Imports

ArangoDB provides an HTTP interface to import multiple documents at once into a
collection. This is known as a bulk import.

The data uploaded must be provided in JSON format. There are two mechanisms to
import the data:

* self-contained JSON documents: in this case, each document contains all 
  attribute names and values. Attribute names may be completely different
  among the documents uploaded
* attribute names plus document data: in this case, the first array must 
  contain the attribute names of the documents that follow. The following arrays
  containing only the attribute values. Attribute values will be mapped to the 
  attribute names by positions.

The endpoint address is */_api/import* for both input mechanisms. Data must be
sent to this URL using an HTTP POST request. The data to import must be
contained in the body of the POST request.

The *collection* query parameter must be used to specify the target collection for
the import. Importing data into a non-existing collection will produce an error. 

The *waitForSync* query parameter can be set to *true* to make the import only 
return if all documents have been synced to disk.

The *complete* query parameter can be set to *true* to make the entire import fail if
any of the uploaded documents is invalid and cannot be imported. In this case,
no documents will be imported by the import run, even if a failure happens at the
end of the import. 

If *complete* has a value other than *true*, valid documents will be imported while 
invalid documents will be rejected, meaning only some of the uploaded documents 
might have been imported.

The *details* query parameter can be set to *true* to make the import API return
details about documents that could not be imported. If *details* is *true*, then
the result will also contain a *details* attribute which is an array of detailed
error messages. If the *details* is set to *false* or omitted, no details will be
returned.



@startDocuBlock JSF_import_document
@startDocuBlock JSF_import_json
