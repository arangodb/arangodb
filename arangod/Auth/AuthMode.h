////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Auth/Common.h"
#include "Auth/Permissions.h"
#include "Basics/Result.h"

#include <variant>

namespace arangodb::auth {
class UserManager;
}
namespace arangodb {
class AuthenticationFeature;
namespace rbac {
struct Service;
}

// TODO It is becoming apparent that we need to be able to keep information
//      related to the original request, like user and path, in the ExecContext.
//      And that we need to keep it for all kinds of AuthMode; e.g. including
//      Superuser or Unauthenticated, probably even Disabled.
//      That probably means removing the username again from AuthMode/IAuth,
//      and instead keeping it in the ExecContext.
//      Also, creating a Superuser ExecContextScope needs to be changed in a
//      way that keeps the original request/user information, and just
//      "upgrades" the AuthMode to Superuser, or something along these lines.

// Different modes of authentication and authorization, depending on server
// configuration, and the specific user. Used to handle authorization in
// ExecContext.
struct AuthMode {
  // Common interface for the following classes
  // TODO Implement this interface
  struct IAuth {
    virtual ~IAuth() = default;
    [[nodiscard]] virtual auto username() const noexcept
        -> std::string_view = 0;

    // this is a suboptimal interface; it should be batched. but it's closer to
    // the existing one, and simplifies the migration a little.
    // this is allowed to throw (e.g. network errors, etc.).
    [[nodiscard]] virtual auto canUse(Permission) const -> Result = 0;
  };

  // Superuser; may do anything, without further checks.
  struct Superuser : IAuth {
    [[nodiscard]] auto username() const noexcept -> std::string_view override;
    [[nodiscard]] auto canUse(Permission permission) const -> Result override;
  };

  // Classic, arangodb-internal authorization, based on permissions in _users.
  struct Classic : IAuth {
    auth::UserManager& _userManager;
    std::string const _username;

    Classic(auth::UserManager& userManager, std::string username);

    [[nodiscard]] auto username() const noexcept -> std::string_view override;

    [[nodiscard]] auto canUse(Permission permission) const -> Result override;

   protected:
    // has _system RW access
    [[nodiscard]] Result isAdmin() const;
  };

  // Role-based access control, based on an external authorization service.
  struct Rbac : IAuth {
    AuthenticationFeature& _authenticationFeature;
    rbac::Service& _rbacService;
    std::string const _username;
    std::string const _jwtToken;

    Rbac(AuthenticationFeature& authenticationFeature,
         rbac::Service& rbacService, std::string username, std::string jwtToken)
        : _authenticationFeature(authenticationFeature),
          _rbacService(rbacService),
          _username(std::move(username)),
          _jwtToken(std::move(jwtToken)) {}

    [[nodiscard]] auto username() const noexcept -> std::string_view override;

    [[nodiscard]] auto canUse(Permission permission) const -> Result override;
  };

  // Authentication is on, but the current user is without authentication.
  // Has basically no permissions.
  struct Unauthenticated : IAuth {
    std::string _username;

    // TODO Assuming we keep _username: Drop the default constructor.
    //      I've added it just so I could compile the tests again quickly.
    Unauthenticated() = default;
    explicit Unauthenticated(std::string username);

    [[nodiscard]] auto username() const noexcept -> std::string_view override;

    [[nodiscard]] auto canUse(Permission permission) const -> Result override;
  };

  // Authentication is disabled, barely any restrictions.
  struct Disabled : IAuth {
    std::string _username;

    explicit Disabled(std::string username);

    [[nodiscard]] auto username() const noexcept -> std::string_view override;

    [[nodiscard]] auto canUse(Permission permission) const -> Result override;
  };

#ifdef ARANGODB_USE_GOOGLE_TESTS
  struct Mockable : IAuth {
    // Use pImpl and delegate in order to avoid having to include gmock
    std::shared_ptr<IAuth> mock;

    // If you don't want Mockable to own the mock, pass a raw pointer to this
    // constructor:
    Mockable(IAuth* mock) : mock(mock, [](auto*) {}) {}

    // construct from any compatible unique_ptr:
    template<typename T, typename D>
    requires std::derived_from<T, IAuth> Mockable(std::unique_ptr<T, D> mock)
        : mock(std::move(mock)) {}

    // construct from a shared_ptr
    Mockable(std::shared_ptr<IAuth> mock) : mock(std::move(mock)) {}

    [[nodiscard]] auto username() const noexcept -> std::string_view override;
    [[nodiscard]] auto canUse(Permission permission) const -> Result override;
  };
#define MOCKABLE , Mockable
#else
#define MOCKABLE
#endif

  using Any = std::variant<Superuser, Classic, Rbac, Unauthenticated,
                           Disabled MOCKABLE>;

  [[nodiscard]] auto getIAuth() -> IAuth&;
  [[nodiscard]] auto getIAuth() const -> IAuth const&;

  Any authMode;

  [[nodiscard]] bool isRbac() const noexcept;
  [[nodiscard]] bool isSuperuser() const noexcept;
  [[nodiscard]] bool isDisabled() const noexcept;

  template<typename T, typename... Args>
  void reset(Args&&... args) {
    authMode.emplace<T>(std::forward<Args>(args)...);
  }
};

}  // namespace arangodb
