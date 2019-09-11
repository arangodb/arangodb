class AuthenticationResult {
 public:
  enum class Type {
    ALREADY_AUTHENTICATED,
    AUTHENTICATED,
    DENIED,
    NOT_RESPONSIBLE,
    RESOURCE_TEMPORARLY_UNAVAILABLE
  };
};
