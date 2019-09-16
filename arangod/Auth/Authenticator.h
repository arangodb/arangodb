#ifdef DO_NOT_USE_FOR_NOW

class Authenticator {
 public:
  virtual AuthenticationResult authenticate(LoginUser&) = 0;
};

#endif
