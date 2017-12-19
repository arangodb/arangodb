!CHAPTER Attribute Names 

Users can pick attribute names for document attributes as desired, provided the
following attribute naming constraints are not violated:

- Attribute names starting with an underscore are considered to be system
  attributes for ArangoDB's internal use. Such attribute names are already used
  by ArangoDB for special purposes:
  - *_id* is used to contain a document's handle
  - *_key* is used to contain a document's user-defined key
  - *_rev* is used to contain the document's revision number
  - In edge collections, the
    - *_from*
    - *_to*

    attributes are used to reference other documents.

  More system attributes may be added in the future without further notice so
  end users should try to avoid using their own attribute names starting with
  underscores.

* Theoretically, attribute names can include punctuation and special characters
  as desired, provided the name is a valid UTF-8 string.  For maximum
  portability, special characters should be avoided though.  For example,
  attribute names may contain the dot symbol, but the dot has a special meaning
  in JavaScript and also in AQL, so when using such attribute names in one of
  these languages, the attribute name would need to be quoted by the end
  user. This will work but requires more work so it might be better to use
  attribute names which don't require any quoting/escaping in all languages
  used. This includes languages used by the client (e.g. Ruby, PHP) if the
  attributes are mapped to object members there.
* Attribute names starting with an at-mark (*@*) will need to be enclosed in
  backticks when used in an AQL query to tell them apart from bind variables.
  Therefore we do not encourage the use of attributes starting with at-marks,
  though they will work when used properly.
* ArangoDB does not enforce a length limit for attribute names. However, long
  attribute names may use more memory in result sets etc. Therefore the use
  of long attribute names is discouraged.
* As ArangoDB saves document attribute names separate from the actual document
  attribute value data, the combined length of all attribute names for a
  document must fit into an ArangoDB shape structure. The maximum combined names
  length is variable and depends on the number and data types of attributes
  used.
* Attribute names are case-sensitive.
* Attribute names should not include the NUL byte (`\u0000`), as names will be
  truncated at the first NUL byte.
* Attributes with empty names (the empty string) are removed from the document 
  when saving it. 

  When the document is later requested, it will be returned without these 
  attributes. For example, if this document is saved:

      { "a" : 1, "" : 2, "b": 3 }

  and later requested, it will be returned like this:
      
      { "a" : 1, "b": 3 }

