////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
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

#include <cstdint>
#include <optional>
#include <variant>

#include <string>
#include <utility>
#include <vector>

namespace arangodb {
namespace url {

// TODO Add string validation

/// @brief Represents the scheme component of a URL
///
/// This class encapsulates the scheme part of a URL (e.g., "http", "https", "ftp").
/// The scheme identifies the protocol or method used to access the resource.
///
/// @note The scheme is stored as-is without validation
/// @note Common schemes include "http", "https", "ftp", "file", etc.
/// @note The scheme is case-insensitive according to RFC standards
class Scheme {
 public:
  /// @brief Create a scheme with the specified value
  ///
  /// Creates a new Scheme object with the given scheme string.
  /// The scheme is typically a protocol identifier like "http" or "https".
  ///
  /// @param value The scheme string (e.g., "http", "https")
  ///
  /// @note The scheme is stored as-is without validation
  /// @note Case sensitivity depends on the protocol specification
  explicit Scheme(std::string value);

  /// @brief Get the scheme value
  ///
  /// Returns the scheme string that was provided during construction.
  /// This is the protocol identifier part of the URL.
  ///
  /// @return The scheme string
  ///
  /// @note Does not throw exceptions
  /// @note Returns a const reference for efficiency
  std::string const& value() const noexcept;

 private:
  /// @brief The scheme string
  ///
  /// Stores the scheme component of the URL (e.g., "http", "https").
  /// This is the protocol identifier that determines how the resource
  /// should be accessed.
  std::string _value;
};

/// @brief Represents the user component of URL user information
///
/// This class encapsulates the username part of the user information
/// component in a URL. The user information appears before the host
/// in the authority section of a URL.
///
/// @note The user information is optional in URLs
/// @note No validation is performed on the username
/// @note Should be URL-encoded if it contains special characters
class User {
 public:
  /// @brief Create a user with the specified username
  ///
  /// Creates a new User object with the given username string.
  /// The username is part of the user information in the URL's authority section.
  ///
  /// @param username The username string
  ///
  /// @note The username is stored as-is without validation
  /// @note Special characters should be URL-encoded
  explicit User(std::string username);

  /// @brief Get the username value
  ///
  /// Returns the username string that was provided during construction.
  /// This is the user identifier part of the URL's authority section.
  ///
  /// @return The username string
  ///
  /// @note Does not throw exceptions
  /// @note Returns a const reference for efficiency
  std::string const& value() const noexcept;

 private:
  /// @brief The username string
  ///
  /// Stores the username component of the URL's user information.
  /// This identifies the user for authentication purposes.
  std::string _value;
};

/// @brief Represents the password component of URL user information
///
/// This class encapsulates the password part of the user information
/// component in a URL. The password appears after the username and
/// is separated by a colon in the authority section.
///
/// @note Passwords in URLs are generally discouraged for security reasons
/// @note No validation is performed on the password
/// @note Should be URL-encoded if it contains special characters
class Password {
 public:
  /// @brief Create a password with the specified value
  ///
  /// Creates a new Password object with the given password string.
  /// The password is part of the user information in the URL's authority section.
  ///
  /// @param password The password string
  ///
  /// @note The password is stored as-is without validation
  /// @note Special characters should be URL-encoded
  /// @note Passwords in URLs are generally discouraged for security
  explicit Password(std::string password);

  /// @brief Get the password value
  ///
  /// Returns the password string that was provided during construction.
  /// This is the password part of the URL's authority section.
  ///
  /// @return The password string
  ///
  /// @note Does not throw exceptions
  /// @note Returns a const reference for efficiency
  std::string const& value() const noexcept;

 private:
  /// @brief The password string
  ///
  /// Stores the password component of the URL's user information.
  /// This is used for authentication purposes.
  std::string _value;
};

/// @brief Represents the user information component of a URL
///
/// This class combines the username and optional password components
/// that appear in the authority section of a URL. The user information
/// is formatted as "username:password" or just "username" if no password.
///
/// @note User information is optional in URLs
/// @note Passwords in URLs are generally discouraged for security reasons
/// @note The password component is optional even when user information is present
class UserInfo {
 public:
  /// @brief Create user information with username and password
  ///
  /// Creates a new UserInfo object with both username and password components.
  /// This represents the full user information that can appear in a URL.
  ///
  /// @param user The username component
  /// @param password The password component
  ///
  /// @note Both username and password are stored
  /// @note Passwords in URLs are generally discouraged for security
  UserInfo(User user, Password password);

  /// @brief Create user information with only username
  ///
  /// Creates a new UserInfo object with only the username component.
  /// No password is associated with this user information.
  ///
  /// @param user The username component
  ///
  /// @note Only the username is stored
  /// @note This is more secure than including passwords in URLs
  explicit UserInfo(User user);

  /// @brief Get the username component
  ///
  /// Returns the username component of the user information.
  /// This is always present in UserInfo objects.
  ///
  /// @return The username component
  ///
  /// @note Does not throw exceptions
  /// @note Returns a const reference for efficiency
  User const& user() const noexcept;

  /// @brief Get the optional password component
  ///
  /// Returns the password component of the user information if present.
  /// The password is optional and may not be set.
  ///
  /// @return Optional password component
  ///
  /// @note Does not throw exceptions
  /// @note Returns nullopt if no password was set
  /// @note Returns a const reference for efficiency
  std::optional<Password> const& password() const noexcept;

 private:
  /// @brief The username component
  ///
  /// Stores the username part of the user information.
  /// This is always present in UserInfo objects.
  User _user;

  /// @brief The optional password component
  ///
  /// Stores the password part of the user information if provided.
  /// This is optional and may not be set for security reasons.
  std::optional<Password> _password;
};

/// @brief Represents the host component of a URL
///
/// This class encapsulates the host part of the authority section in a URL.
/// The host can be a domain name, IPv4 address, or IPv6 address.
///
/// @note The host is a required component of the authority section
/// @note No validation is performed on the host format
/// @note IPv6 addresses should be enclosed in brackets
class Host {
 public:
  /// @brief Create a host with the specified value
  ///
  /// Creates a new Host object with the given host string.
  /// The host can be a domain name, IPv4 address, or IPv6 address.
  ///
  /// @param hostname The host string (domain name or IP address)
  ///
  /// @note The host is stored as-is without validation
  /// @note IPv6 addresses should be enclosed in brackets
  explicit Host(std::string hostname);

  /// @brief Get the host value
  ///
  /// Returns the host string that was provided during construction.
  /// This is the host identifier part of the URL's authority section.
  ///
  /// @return The host string
  ///
  /// @note Does not throw exceptions
  /// @note Returns a const reference for efficiency
  std::string const& value() const noexcept;

 private:
  /// @brief The host string
  ///
  /// Stores the host component of the URL's authority section.
  /// This can be a domain name, IPv4 address, or IPv6 address.
  std::string _value;
};

/// @brief Represents the port component of a URL
///
/// This class encapsulates the port number part of the authority section
/// in a URL. The port specifies which network port to connect to on the host.
///
/// @note The port is optional in URLs
/// @note Default ports are defined for common schemes (80 for HTTP, 443 for HTTPS)
/// @note Port numbers range from 1 to 65535
class Port {
 public:
  /// @brief Create a port with the specified number
  ///
  /// Creates a new Port object with the given port number.
  /// The port specifies which network port to connect to.
  ///
  /// @param portNumber The port number (1-65535)
  ///
  /// @note Port numbers must be in the range 1-65535
  /// @note Common ports: 80 (HTTP), 443 (HTTPS), 21 (FTP), 22 (SSH)
  explicit Port(uint16_t portNumber);

  /// @brief Get the port number
  ///
  /// Returns the port number that was provided during construction.
  /// This is the network port to connect to on the host.
  ///
  /// @return The port number
  ///
  /// @note Does not throw exceptions
  /// @note Returns a const reference for efficiency
  uint16_t const& value() const noexcept;

 private:
  /// @brief The port number
  ///
  /// Stores the port component of the URL's authority section.
  /// This specifies which network port to connect to.
  uint16_t _value;
};

/// @brief Represents the authority component of a URL
///
/// This class combines the optional user information, required host, and
/// optional port components that form the authority section of a URL.
/// The authority section appears after the scheme and before the path.
///
/// @note The authority is optional in some URL schemes
/// @note The host is required when authority is present
/// @note User information and port are optional components
class Authority {
 public:
  /// @brief Create an authority with all components
  ///
  /// Creates a new Authority object with the specified user information,
  /// host, and port components. The authority represents the network
  /// location where the resource can be accessed.
  ///
  /// @param userInfo Optional user information (username and password)
  /// @param host The host component (required)
  /// @param port Optional port number
  ///
  /// @note The host is required and must be provided
  /// @note User information and port are optional
  Authority(std::optional<UserInfo> userInfo, Host host,
            std::optional<Port> port);

  /// @brief Get the optional user information
  ///
  /// Returns the user information component of the authority if present.
  /// User information contains username and optional password.
  ///
  /// @return Optional user information
  ///
  /// @note Does not throw exceptions
  /// @note Returns nullopt if no user information was set
  /// @note Returns a const reference for efficiency
  std::optional<UserInfo> const& userInfo() const noexcept;

  /// @brief Get the host component
  ///
  /// Returns the host component of the authority.
  /// This is always present in Authority objects.
  ///
  /// @return The host component
  ///
  /// @note Does not throw exceptions
  /// @note Returns a const reference for efficiency
  Host const& host() const noexcept;

  /// @brief Get the optional port component
  ///
  /// Returns the port component of the authority if present.
  /// If no port is specified, the default port for the scheme is used.
  ///
  /// @return Optional port component
  ///
  /// @note Does not throw exceptions
  /// @note Returns nullopt if no port was set
  /// @note Returns a const reference for efficiency
  std::optional<Port> const& port() const noexcept;

 private:
  /// @brief Optional user information
  ///
  /// Stores the user information component (username and password).
  /// This is optional and may not be set.
  std::optional<UserInfo> _userInfo;

  /// @brief The host component
  ///
  /// Stores the host part of the authority.
  /// This is required when authority is present.
  Host _host;

  /// @brief Optional port component
  ///
  /// Stores the port number if specified.
  /// This is optional and defaults to the scheme's default port.
  std::optional<Port> _port;
};

/// @brief Represents the path component of a URL
///
/// This class encapsulates the path part of a URL, which identifies
/// the specific resource within the host. The path appears after
/// the authority and before the query string.
///
/// @note The path is always present in URLs (may be empty)
/// @note Paths should be URL-encoded if they contain special characters
/// @note The path includes the leading slash in absolute URLs
class Path {
 public:
  /// @brief Create a path with the specified value
  ///
  /// Creates a new Path object with the given path string.
  /// The path identifies the specific resource within the host.
  ///
  /// @param pathString The path string (e.g., "/api/v1/users")
  ///
  /// @note The path is stored as-is without validation
  /// @note Special characters should be URL-encoded
  /// @note The path may be empty for root resources
  explicit Path(std::string pathString);

  /// @brief Get the path value
  ///
  /// Returns the path string that was provided during construction.
  /// This is the resource identifier part of the URL.
  ///
  /// @return The path string
  ///
  /// @note Does not throw exceptions
  /// @note Returns a const reference for efficiency
  std::string const& value() const noexcept;

 private:
  /// @brief The path string
  ///
  /// Stores the path component of the URL.
  /// This identifies the specific resource within the host.
  std::string _value;
};

/// @brief Represents a query string component of a URL
///
/// This class encapsulates a raw query string that appears after the path
/// and before the fragment in a URL. The query string is preceded by a
/// question mark and contains parameters for the resource.
///
/// @note The query string is optional in URLs
/// @note Query strings should be URL-encoded
/// @note This class stores the raw query string without parsing
class QueryString {
 public:
  /// @brief Create a query string with the specified value
  ///
  /// Creates a new QueryString object with the given query string.
  /// The query string contains parameters for the resource.
  ///
  /// @param queryString The raw query string (e.g., "foo=bar&baz=qux")
  ///
  /// @note The query string is stored as-is without validation
  /// @note Special characters should be URL-encoded
  /// @note Do not include the leading question mark
  explicit QueryString(std::string queryString);

  /// @brief Get the query string value
  ///
  /// Returns the raw query string that was provided during construction.
  /// This is the parameter string part of the URL.
  ///
  /// @return The raw query string
  ///
  /// @note Does not throw exceptions
  /// @note Returns a const reference for efficiency
  std::string const& value() const noexcept;

 private:
  /// @brief The raw query string
  ///
  /// Stores the query string component of the URL.
  /// This contains parameters for the resource.
  std::string _value;
};

/// @brief Represents structured query parameters for a URL
///
/// This class provides a structured way to build query parameters
/// for URLs. It stores key-value pairs and handles URL encoding
/// automatically when converting to string format.
///
/// @note Keys and values are automatically URL-encoded as necessary
/// @note Multiple values for the same key are supported
/// @note The class handles the formatting of the query string
class QueryParameters {
 public:
  /// @brief Add a key-value pair to the query parameters
  ///
  /// Adds a new parameter to the query string with the specified key and value.
  /// The key and value will be URL-encoded automatically when the query
  /// string is generated.
  ///
  /// @param key The parameter key
  /// @param value The parameter value
  ///
  /// @note Keys and values are automatically URL-encoded
  /// @note Multiple values for the same key are supported
  /// @note The order of parameters is preserved
  void add(std::string const& key, std::string const& value);

  /// @brief Check if the query parameters are empty
  ///
  /// Returns whether any parameters have been added to the query.
  /// Empty query parameters will not generate a query string.
  ///
  /// @return true if no parameters have been added, false otherwise
  ///
  /// @note Does not throw exceptions
  /// @note Used to determine if query string should be included
  bool empty() const noexcept;

  /// @brief Write the query parameters to a stream
  ///
  /// Formats the query parameters as a query string and writes it to
  /// the provided output stream. Keys and values are URL-encoded.
  ///
  /// @param stream The output stream to write to
  ///
  /// @return Reference to the output stream
  ///
  /// @note Keys and values are automatically URL-encoded
  /// @note Parameters are separated by '&' characters
  /// @note Key-value pairs are separated by '=' characters
  std::ostream& toStream(std::ostream& stream) const;

 private:
  /// @brief Storage for key-value pairs
  ///
  /// Vector of key-value pairs that represent the query parameters.
  /// The order of parameters is preserved as they were added.
  std::vector<std::pair<std::string, std::string>> _pairs;
};

/// @brief Represents the query component of a URL
///
/// This class can hold either a raw query string or structured query
/// parameters. It provides a unified interface for working with URL
/// query components regardless of their internal representation.
///
/// @note Can hold either a raw query string or structured parameters
/// @note The representation is chosen based on how the query was created
/// @note Empty queries are supported and will not generate output
class Query {
 public:
  /// @brief Create a query from a raw query string
  ///
  /// Creates a new Query object that holds a raw query string.
  /// The query string is used as-is without parsing.
  ///
  /// @param queryString The raw query string to use
  ///
  /// @note The query string is stored without parsing
  /// @note Use this when you have a pre-formatted query string
  explicit Query(QueryString queryString);

  /// @brief Create a query from structured parameters
  ///
  /// Creates a new Query object that holds structured query parameters.
  /// The parameters will be formatted into a query string when needed.
  ///
  /// @param params The structured query parameters to use
  ///
  /// @note The parameters are formatted automatically
  /// @note Use this when building queries programmatically
  explicit Query(QueryParameters params);

  /// @brief Check if the query is empty
  ///
  /// Returns whether the query has any content. Empty queries will
  /// not generate a query string in the URL.
  ///
  /// @return true if the query is empty, false otherwise
  ///
  /// @note Does not throw exceptions
  /// @note Used to determine if query string should be included
  bool empty() const noexcept;

  /// @brief Write the query to a stream
  ///
  /// Formats the query content and writes it to the provided output stream.
  /// The format depends on whether the query holds a raw string or parameters.
  ///
  /// @param stream The output stream to write to
  ///
  /// @return Reference to the output stream
  ///
  /// @note Handles both raw strings and structured parameters
  /// @note URL encoding is applied as appropriate
  std::ostream& toStream(std::ostream& stream) const;

 private:
  /// @brief Storage for query content
  ///
  /// Variant that can hold either a raw query string or structured
  /// query parameters. The actual type depends on how the query was created.
  std::variant<QueryString, QueryParameters> _content;
};

/// @brief Represents the fragment component of a URL
///
/// This class encapsulates the fragment part of a URL, which appears
/// after the query string and is preceded by a hash (#) character.
/// Fragments are used to identify specific parts of a resource.
///
/// @note The fragment is optional in URLs
/// @note Fragments are not sent to the server in HTTP requests
/// @note Fragments should be URL-encoded if they contain special characters
class Fragment {
 public:
  /// @brief Create a fragment with the specified value
  ///
  /// Creates a new Fragment object with the given fragment string.
  /// The fragment identifies a specific part of the resource.
  ///
  /// @param fragmentString The fragment string (e.g., "section1")
  ///
  /// @note The fragment is stored as-is without validation
  /// @note Special characters should be URL-encoded
  /// @note Do not include the leading hash character
  explicit Fragment(std::string fragmentString);

  /// @brief Get the fragment value
  ///
  /// Returns the fragment string that was provided during construction.
  /// This is the fragment identifier part of the URL.
  ///
  /// @return The fragment string
  ///
  /// @note Does not throw exceptions
  /// @note Returns a const reference for efficiency
  std::string const& value() const noexcept;

 private:
  /// @brief The fragment string
  ///
  /// Stores the fragment component of the URL.
  /// This identifies a specific part of the resource.
  std::string _value;
};

/// @brief Represents a complete URL
///
/// This class combines all components of a URL into a complete representation.
/// It includes the scheme, optional authority, path, optional query, and
/// optional fragment components.
///
/// @note The scheme and path are required components
/// @note Authority, query, and fragment are optional
/// @note Provides methods to access individual components and generate strings
class Url {
 public:
  /// @brief Create a complete URL with all components
  ///
  /// Creates a new Url object with the specified components.
  /// The URL represents a complete resource identifier.
  ///
  /// @param scheme The URL scheme (required)
  /// @param authority Optional authority component
  /// @param path The resource path (required)
  /// @param query Optional query component
  /// @param fragment Optional fragment component
  ///
  /// @note The scheme and path are required
  /// @note Other components are optional
  Url(Scheme scheme, std::optional<Authority> authority, Path path, 
      std::optional<Query> query, std::optional<Fragment> fragment);

  /// @brief Convert the URL to a string
  ///
  /// Generates a complete URL string from all the components.
  /// The string follows standard URL format and encoding rules.
  ///
  /// @return The complete URL string
  ///
  /// @note All components are properly formatted and encoded
  /// @note Optional components are omitted if not present
  std::string toString() const;

  /// @brief Get the scheme component
  ///
  /// Returns the scheme component of the URL.
  /// This is always present in URL objects.
  ///
  /// @return The scheme component
  ///
  /// @note Does not throw exceptions
  /// @note Returns a const reference for efficiency
  Scheme const& scheme() const noexcept;

  /// @brief Get the optional authority component
  ///
  /// Returns the authority component of the URL if present.
  /// The authority contains host and optional user/port information.
  ///
  /// @return Optional authority component
  ///
  /// @note Does not throw exceptions
  /// @note Returns nullopt if no authority was set
  /// @note Returns a const reference for efficiency
  std::optional<Authority> const& authority() const noexcept;

  /// @brief Get the path component
  ///
  /// Returns the path component of the URL.
  /// This is always present in URL objects.
  ///
  /// @return The path component
  ///
  /// @note Does not throw exceptions
  /// @note Returns a const reference for efficiency
  Path const& path() const noexcept;

  /// @brief Get the optional query component
  ///
  /// Returns the query component of the URL if present.
  /// The query contains parameters for the resource.
  ///
  /// @return Optional query component
  ///
  /// @note Does not throw exceptions
  /// @note Returns nullopt if no query was set
  /// @note Returns a const reference for efficiency
  std::optional<Query> const& query() const noexcept;

  /// @brief Get the optional fragment component
  ///
  /// Returns the fragment component of the URL if present.
  /// The fragment identifies a specific part of the resource.
  ///
  /// @return Optional fragment component
  ///
  /// @note Does not throw exceptions
  /// @note Returns nullopt if no fragment was set
  /// @note Returns a const reference for efficiency
  std::optional<Fragment> const& fragment() const noexcept;

 private:
  /// @brief The scheme component
  ///
  /// Stores the scheme part of the URL.
  /// This is always present in URL objects.
  Scheme _scheme;

  /// @brief Optional authority component
  ///
  /// Stores the authority part of the URL if present.
  /// This contains host and optional user/port information.
  std::optional<Authority> _authority;

  /// @brief The path component
  ///
  /// Stores the path part of the URL.
  /// This is always present in URL objects.
  Path _path;

  /// @brief Optional query component
  ///
  /// Stores the query part of the URL if present.
  /// This contains parameters for the resource.
  std::optional<Query> _query;

  /// @brief Optional fragment component
  ///
  /// Stores the fragment part of the URL if present.
  /// This identifies a specific part of the resource.
  std::optional<Fragment> _fragment;
};

/// @brief Represents a URL location without scheme and authority
///
/// This class represents an artificial part of a URL that includes
/// the path and optionally query and fragment components, but omits
/// the scheme and authority. This is useful for relative URLs.
///
/// @note This is an artificial construct not part of URL standards
/// @note Useful for representing relative URLs or URL suffixes
/// @note Contains path and optional query/fragment components
class Location {
 public:
  /// @brief Create a location with the specified components
  ///
  /// Creates a new Location object with the specified path and
  /// optional query and fragment components.
  ///
  /// @param path The resource path (required)
  /// @param query Optional query component
  /// @param fragment Optional fragment component
  ///
  /// @note The path is required
  /// @note Query and fragment are optional
  Location(Path path, std::optional<Query> query, std::optional<Fragment> fragment);

  /// @brief Convert the location to a string
  ///
  /// Generates a location string from the path and optional components.
  /// The string follows URL format for the included components.
  ///
  /// @return The location string
  ///
  /// @note All components are properly formatted and encoded
  /// @note Optional components are omitted if not present
  std::string toString() const;

  /// @brief Get the path component
  ///
  /// Returns the path component of the location.
  /// This is always present in Location objects.
  ///
  /// @return The path component
  ///
  /// @note Does not throw exceptions
  /// @note Returns a const reference for efficiency
  Path const& path() const noexcept;

  /// @brief Get the optional query component
  ///
  /// Returns the query component of the location if present.
  /// The query contains parameters for the resource.
  ///
  /// @return Optional query component
  ///
  /// @note Does not throw exceptions
  /// @note Returns nullopt if no query was set
  /// @note Returns a const reference for efficiency
  std::optional<Query> const& query() const noexcept;

  /// @brief Get the optional fragment component
  ///
  /// Returns the fragment component of the location if present.
  /// The fragment identifies a specific part of the resource.
  ///
  /// @return Optional fragment component
  ///
  /// @note Does not throw exceptions
  /// @note Returns nullopt if no fragment was set
  /// @note Returns a const reference for efficiency
  std::optional<Fragment> const& fragment() const noexcept;

 private:
  /// @brief The path component
  ///
  /// Stores the path part of the location.
  /// This is always present in Location objects.
  Path _path;

  /// @brief Optional query component
  ///
  /// Stores the query part of the location if present.
  /// This contains parameters for the resource.
  std::optional<Query> _query;

  /// @brief Optional fragment component
  ///
  /// Stores the fragment part of the location if present.
  /// This identifies a specific part of the resource.
  std::optional<Fragment> _fragment;
};

/// @brief Encode a string for use in URLs
///
/// Applies URL encoding (percent-encoding) to the input string to make it
/// safe for use in URLs. Reserved and non-ASCII characters are encoded
/// as percent-encoded sequences.
///
/// @param input The string to encode
///
/// @return The URL-encoded string
///
/// @note Uses percent-encoding for reserved and non-ASCII characters
/// @note Follows RFC 3986 encoding rules
/// @note Safe to use in any part of a URL
std::string uriEncode(std::string const& input);

/// @brief Check if a character is unreserved in URLs
///
/// Determines if the specified character is considered unreserved
/// according to URL standards. Unreserved characters do not need
/// to be percent-encoded in URLs.
///
/// @param ch The character to check
///
/// @return true if the character is unreserved, false otherwise
///
/// @note Unreserved characters include alphanumeric and some symbols
/// @note Based on RFC 3986 unreserved character set
/// @note Does not throw exceptions
bool isUnreserved(char ch);

/// @brief Check if a character is reserved in URLs
///
/// Determines if the specified character is considered reserved
/// according to URL standards. Reserved characters have special
/// meaning in URLs and may need percent-encoding in certain contexts.
///
/// @param ch The character to check
///
/// @return true if the character is reserved, false otherwise
///
/// @note Reserved characters include delimiters and sub-delimiters
/// @note Based on RFC 3986 reserved character set
/// @note Does not throw exceptions
bool isReserved(char ch);

/// @brief Stream output operator for Authority
///
/// Outputs the authority component to the specified stream in
/// standard URL format. Includes user info, host, and port as appropriate.
///
/// @param stream The output stream
/// @param authority The authority to output
///
/// @return Reference to the output stream
///
/// @note Formats according to URL standards
/// @note Handles optional components appropriately
std::ostream& operator<<(std::ostream& stream, Authority const& authority);

/// @brief Stream output operator for Query
///
/// Outputs the query component to the specified stream in
/// standard URL format. Handles both raw strings and structured parameters.
///
/// @param stream The output stream
/// @param query The query to output
///
/// @return Reference to the output stream
///
/// @note Formats according to URL standards
/// @note Handles URL encoding as needed
std::ostream& operator<<(std::ostream& stream, Query const& query);

/// @brief Stream output operator for QueryParameters
///
/// Outputs the query parameters to the specified stream in
/// standard URL format. Keys and values are URL-encoded.
///
/// @param stream The output stream
/// @param params The query parameters to output
///
/// @return Reference to the output stream
///
/// @note Formats according to URL standards
/// @note Applies URL encoding to keys and values
std::ostream& operator<<(std::ostream& stream, QueryParameters const& params);

/// @brief Stream output operator for Location
///
/// Outputs the location component to the specified stream in
/// standard URL format. Includes path and optional query/fragment.
///
/// @param stream The output stream
/// @param location The location to output
///
/// @return Reference to the output stream
///
/// @note Formats according to URL standards
/// @note Handles optional components appropriately
std::ostream& operator<<(std::ostream& stream, Location const& location);

/// @brief Stream output operator for Url
///
/// Outputs the complete URL to the specified stream in
/// standard URL format. Includes all components as appropriate.
///
/// @param stream The output stream
/// @param url The URL to output
///
/// @return Reference to the output stream
///
/// @note Formats according to URL standards
/// @note Handles optional components appropriately
std::ostream& operator<<(std::ostream& stream, Url const& url);

/// @brief Stream output operator for UserInfo
///
/// Outputs the user information to the specified stream in
/// standard URL format. Includes username and optional password.
///
/// @param stream The output stream
/// @param userInfo The user information to output
///
/// @return Reference to the output stream
///
/// @note Formats according to URL standards
/// @note Handles optional password component
std::ostream& operator<<(std::ostream& stream, UserInfo const& userInfo);

}  // namespace url
}  // namespace arangodb
