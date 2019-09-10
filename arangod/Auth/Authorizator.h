class Authorizator {
 public:
  virtual AuthorizationResult authorize(LoginUser&) = 0;
};

