LoginUserResult LoginManager::loginUser(LoginCredentials const& credentials) {
  LoginUserResult user = findCachedCredentials(credentials);

  if (user.found()) {
    return user;
  }

  user = storeCachedCredentials(credentials);

  return user;
}

LoginResult LoginManager::validate(LoginUser& user) {
  if (user.isValidated()) {
    return LoginResult{user};
  }

  for (auto const& method : _methods) {
    AuthenticationResult result = _methods.authenticator().authenticate(user);

    if (result.notResponsible()) {
      continue;
    }

    if (result.denied()) {
      return LoginResult{result};
    }

    return LoginResult{_methods.authorizator().authorize(user)};
  }

  return LoginResult::accessDenied(user, method);
}
