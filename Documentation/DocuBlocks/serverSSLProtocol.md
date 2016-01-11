////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock serverSSLProtocol
/// @brief SSL protocol type to use
/// `--server.ssl-protocolvalue`
///
/// Use this option to specify the default encryption protocol to be used.
/// The following variants are available:
/// - 1: SSLv2
/// - 2: SSLv23
/// - 3: SSLv3
/// - 4: TLSv1
///
/// The default *value* is 4 (i.e. TLSv1).
///
/// **Note**: this option is only relevant if at least one SSL endpoint is
/// used.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////