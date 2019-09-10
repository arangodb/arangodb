class Credential {
 public:
  static void registerAuthenticator(std::string const&, Authenticator*);
  static void cacheCredential(Credential*);
  static std::shared_ptr<Crendetial> lookupCredential(std::string const& type,
                                                      std::string const& externalUsername,
                                                      std::string const& credential);

 private:
  static std::hash_map<std::string, Authenticator*> _authenticators;

 public:
  Credential(std::string const& type, std::string const& internalUsername,
             std::string const& externalUsername, std::string const& credential);

 private:
  std::string const _internalUsername;
  std::string const _externalUsername;
  std::string const _credential;
};
