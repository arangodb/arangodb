// Tests for the /_open/auth and /_open/auth/renew endpoints.
//
// Both endpoints are marked OPEN in the permission table, meaning they do not
// gate access on the caller's _system permission level.  All admin users
// (AU, AN, AR, AW) and the superuser should receive HTTP 200 in both tests.

export default [
  {
    name: "Obtain a JWT token with valid credentials (POST /_open/auth)",
    type: "admin",
    method: "POST",
    path: "/_open/auth",
    // Use the AR user's credentials (ro on _system, password == username).
    // The endpoint is OPEN so the server must accept this request regardless
    // of the Authorization header that the test runner attaches.
    body: { username: "AR", password: "AR" },
  },

  {
    name: "Renew a JWT token (POST /_open/auth/renew) with proper token",
    type: "admin",
    method: "POST",
    path: "/_open/auth/renew",
    // The renew endpoint requires JWT authentication with a non-empty username.
    // The runner's default auth (Basic Auth for admin users, superuser JWT for
    // the last column) would both yield 404, so we override the Authorization
    // header with a real user JWT obtained in setup.  All five columns should
    // return 200, demonstrating that the endpoint is OPEN and does not check
    // the caller's _system permission level.
    headers: { "Authorization": "Bearer ${ctx.data.jwt}" },

    setup: async (ctx) => {
      // POST to /_open/auth with AR's credentials to obtain a user-scoped JWT.
      // The AR user has ro access to _system and its password equals its name.
      // /_open/auth is itself OPEN and inspects only the request body, so the
      // superuser Authorization header added by ctx.request is ignored.
      const r = await ctx.request('POST', '/_open/auth', { username: "AR", password: "AR" });
      return { jwt: r.body.jwt };
    },
  },

  {
    name: "Renew a JWT token (POST /_open/auth/renew) without proper token",
    type: "admin",
    method: "POST",
    path: "/_open/auth/renew",
    // The renew endpoint requires JWT authentication with a non-empty username.
    // The runner's default auth (Basic Auth for admin users, superuser JWT for
    // the last column) must yield 404.
  },
];
