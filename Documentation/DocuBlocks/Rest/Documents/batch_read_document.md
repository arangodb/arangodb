
@startDocuBlock batch_read_document
@brief read a list of documents

@RESTHEADER{POST /_api/batch/document/{collection}/read, Read documents}

@RESTHEADERPARAMETERS

@RESTHEADERPARAM{Content-type,string,required}
```
Content-type: application/json
```

@RESTURLPARAMETERS

@RESTURLPARAM{collection,string,required}
Name of the collection to read from.

@RESTALLBODYPARAM{array,json,required}
A JSON array of strings or documents.

@RESTSTRUCT

@RESTDESCRIPTION
TODO

@RESTRETURNCODES

@RESTRETURNCODE{200}
is returned if the document was found

@RESTRETURNCODE{404}
is returned if the document or collection was not found

@endDocuBlock
