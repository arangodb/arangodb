class DatabasePermission : public Permission, DatabaseResource {
 private:
  std::map<CollectionResource, CollectionPermission> _collections;
};
