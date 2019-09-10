struct CollectionResource : public DatabaseResource {
  CollectionResource(std::string const& database, 
		     std::string const& collection)
    : DatabaseResource(database), _collection(collection) {}

  CollectionResource(std::string&& database, 
		     std::string&& collection)
    : DatabaseResource(std::move(database)), _collection(std::move(collection)) {}

  std::string const _collection;
};

