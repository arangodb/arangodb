#ifdef DO_NOT_USE_FOR_NOW

class DatabasePermission : public Permission, DatabaseResource {
 private:
  std::map<CollectionResource, CollectionPermission> _collections;
};

#endif
