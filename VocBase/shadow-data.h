typedef struct TRI_shadow_store_s {
  TRI_mutex_t _lock;

  TRI_associative_pointer_t _data;

  void* (*parse) (struct TRI_shadow_store_s*, struct TRI_doc_collection_s*, struct TRI_doc_mptr_s*);
}
TRI_shadow_store_t;



typdef struct TRI_shadow_s {
  int64_t _rc;
  void* _data;
}
TRI_shadow_t;
