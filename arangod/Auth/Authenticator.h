class Authenticator {
 public:
  virtual AuthenticationResult authenticate(LoginUser&) = 0;
};

