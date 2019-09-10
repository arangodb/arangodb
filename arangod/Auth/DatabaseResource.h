struct DatabaseResource : public Resource {
  DatabaseResource(std::string const& database)
    : _database(database) {}

  DatabaseResource(std::string&& database)
    : _database(std::move(database)) {}

  std::string const _database;
};
