#ifdef DO_NOT_USE_FOR_NOW

class Permissions {
 public:
  static Permissions* find(std::set<Role> const& roles);

 private:
  static Permissions create(std::vector<Permission> const&);

  static Permissions merge(Permissions const&, Permissions const&);

  Level authLevel(std::string const& username, DatabaseResource const&, bool configured);

  Level authLevel(std::string const& username, CollectionResource const&, bool configured);
};

#endif
