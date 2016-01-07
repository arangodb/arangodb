////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock serverAuthenticationDisable
/// @brief disable authentication for requests via UNIX domain sockets
/// `--server.disable-authentication-unix-sockets value`
///
/// Setting *value* to true will turn off authentication on the server side
/// for requests coming in via UNIX domain sockets. With this flag enabled,
/// clients located on the same host as the ArangoDB server can use UNIX
/// domain
/// sockets to connect to the server without authentication.
/// Requests coming in by other means (e.g. TCP/IP) are not affected by this
/// option.
///
/// The default value is *false*.
///
/// **Note**: this option is only available on platforms that support UNIX
/// domain
/// sockets.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////