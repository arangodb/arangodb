#ifdef DO_NOT_USE_FOR_NOW

AuthenticationResult InternalAuthenticator::authenticate(LoginUser& user) {
  if (user.isAuthenticated()) {
    return AuthenticationResult{ALREADY_AUTHENTICATED, user};
  }

  return _manager->authenticate(user);
}

#endif
