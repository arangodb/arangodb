!CHAPTER Attribute Names 

Users can pick attribute names for document attributes as desired, provided the
following attribute naming constraints are not violated:
- Attribute names starting with an underscore are considered to be system
  attributes for ArangoDB's internal use. Such attribute names are already used
  by ArangoDB for special purposes, e.g. `_id` is used to contain a document's
  handle, `_key` is used to contain a document's user-defined key, and `_rev` is
  used to contain the document's revision number. In edge collections, the
  `_from` and `_to` attributes are used to reference other documents.

  More system attributes may be added in the future without further notice so
  end users should not use attribute names starting with an underscore for their
  own attributes.

- Attribute names should not start with the at-mark (`\@`). The at-mark
  at the start of attribute names is reserved in ArangoDB for future use cases.
- Theoretically, attribute names can include punctuation and special characters
  as desired, provided the name is a valid UTF-8 string.  For maximum
  portability, special characters should be avoided though.  For example,
  attribute names may contain the dot symbol, but the dot has a special meaning
  in Javascript and also in AQL, so when using such attribute names in one of
  these languages, the attribute name would need to be quoted by the end
  user. This will work but requires more work so it might be better to use
  attribute names which don't require any quoting/escaping in all languages
  used. This includes languages used by the client (e.g. Ruby, PHP) if the
  attributes are mapped to object members there.
- ArangoDB does not enforce a length limit for attribute names. However, long
  attribute names may use more memory in result sets etc. Therefore the use
  of long attribute names is discouraged.
- As ArangoDB saves document attribute names separate from the actual document
  attribute value data, the combined length of all attribute names for a
  document must fit into an ArangoDB shape structure. The maximum combined names
  length is variable and depends on the number and data types of attributes
  used.
- Attribute names are case-sensitive.

- Attributes with empty names (the empty string) and attributes with names that
  start with an underscore and don't have a special meaning (system attributes)
  are removed from the document when saving it. 

  When the document is later requested, it will be returned without these 
  attributes. For example, if this document is saved

      { "a" : 1, "" : 2, "_test" : 3, "b": 4 }

  and later requested, it will be returned like this:
      
      { "a" : 1, "b": 4 }

