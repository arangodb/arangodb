class LoginMethod {
 private:
  Authenticator _authenticator;
  Authorizator _authorizator;
};

// ================================================================================

class LoginUser {
 public:
  bool isValidated() const;
  std::string const& externalName() const;
  std::string const& internalName() const;

 private:
  std::set<std::string> _roles;
};

// ================================================================================

class LoginManager {
 public:
  LoginUserResult loginUser(Credentials const&);
  LoginResult validate(LoginUser&);

 private:
  std::vector<LoginMethod> _methods;
};
