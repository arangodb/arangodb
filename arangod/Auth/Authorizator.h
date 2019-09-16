#ifdef DO_NOT_USE_FOR_NOW

class Authorizator {
 public:
  virtual AuthorizationResult authorize(LoginUser&) = 0;
};

#endif
