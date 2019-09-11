class ImplicitAuthenticator : public Authorizator {
 public:
  AuthorizationResult authorize(LoginUser&) override;
};

AuthorizationResult ImplicitAuthenticator::authorize(LoginUser& user) {
  if (!user.isAuthenticated()) {
    return AuthorizationResult{NOT_AUTHENTICATED};
  }

  user.addRole(user.internalName());
  return AuthorizationResult{AUTHORIZED_COMPLETED};
}
