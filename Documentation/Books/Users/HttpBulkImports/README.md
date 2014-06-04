<a name="http_interface_for_bulk_imports"></a>
# HTTP Interface for Bulk Imports

ArangoDB provides an HTTP interface to import multiple documents at once into a
collection. This is known as a bulk import.

The data uploaded must be provided in JSON format. There are two mechanisms to
import the data:
- self-contained JSON documents: in this case, each document contains all 
  attribute names and values. Attribute names may be completely different
  among the documents uploaded

- attribute names plus document data: in this case, the first document must 
  be a JSON list containing the attribute names of the documents that follow.
  The following documents must be lists containing only the document data.
  Data will be mapped to the attribute names by attribute positions.

The endpoint address is `/_api/import` for both input mechanisms. Data must be
sent to this URL using an HTTP POST request. The data to import must be
contained in the body of the POST request.

The `collection` URL parameter must be used to specify the target collection for
the import. The optional URL parameter `createCollection` can be used to create
a non-existing collection during the import. If not used, importing data into a
non-existing collection will produce an error. Please note that the `createCollection`
flag can only be used to create document collections, not edge collections.

The `waitForSync` URL parameter can be set to `true` to make the import only 
return if all documents have been synced to disk.

The `complete` URL parameter can be set to `true` to make the entire import fail if
any of the uploaded documents is invalid and cannot be imported. In this case,
no documents will be imported by the import run, even if a failure happens at the
end of the import. 

If `complete` has a value other than `true`, valid documents will be imported while 
invalid documents will be rejected, meaning only some of the uploaded documents 
might have been imported.

The `details` URL parameter can be set to `true` to make the import API return
details about documents that could not be imported. If `details` is `true`, then
the result will also contain a `details` attribute which is a list of detailed
error messages. If the `details` is set to `false` or omitted, no details will be
returned.
