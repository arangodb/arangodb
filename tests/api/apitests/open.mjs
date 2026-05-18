// Tests for the /_open/auth endpoint.
//
// This endpoint is marked OPEN in the permission table, meaning it does not
// require any incoming authentication.  All admin users (AU, AN, AR, AW) and
// the superuser should receive HTTP 200 when valid credentials are supplied in
// the request body, because access is never gated on the caller's permission
// level.

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
];
