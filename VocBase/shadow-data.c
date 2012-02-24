TRI_shadow_t* TRI_CreateShadowData (TRI_shadow_store_t* store, TRI_vocbase_t* vocbase, TRI_voc_cid_t cid, TRI_voc_did_t did) {
  TRI_vocbase_col_t const* col;

  // extract the collection
  col = TRI_LookupCollectionByIdVocBase(vocbase, cid);

  if (col == NULL ...) {
    return NULL;
  }

  collection = col->...;

  // lock the collection
  collection->beginRead(collection);

  // find the document
  mptr = collection->read(collection, did);

  if (mptr == NULL) {
    collection->endRead(collection);
    return NULL;
  }

  // check if we already know a parsed version
  TRI_LockMutex(&store->_lock);

  element = LookupElement(store, cid, did);

  if (element != NULL) {
    ok = store->verify(store, collection, mptr, element);

    if (ok) {
      ++element->_counter;

      TRI_UnlockMutex(&store->_lock);
      collection->endRead(collection);

      return element;
    }
    else {
      RemoveElement(store, element);
    }
  }

  // parse the document
  parsed = store->parse(store, collection, mptr);
  
  if (parsed == NULL) {
    TRI_UnlockMutex(&store->_lock);
    collection->endRead(collection);
    return NULL;
  }

  // enter the parsed document into the store
  element = CreateElement(store, parsed);

  // use element, unlock the store and the collection
  ++element->_counter;

  TRI_UnlockMutex(&store->_lock);
  collection->endRead(collection);

  // and return the element
  return element;
}



void TRI_ReleaseShadowData (TRI_shadow_store_t* store, TRI_shadow_t* element) {
  TRI_LockMutex(&store->_lock);

  // release the element
  --element->_counter;

  // need to destroy the element
  if (element->_counter < 1) {
    RemoveElement(store, element);
    DestroyElement(store, element);
  }

  TRI_UnlockMutex(&store->_lock);
}
  
