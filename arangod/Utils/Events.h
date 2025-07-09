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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <velocypack/Slice.h>

#include "Rest/CommonDefines.h"
#include "Utils/OperationOptions.h"
#include "VocBase/LogicalCollection.h"

namespace arangodb {
class GeneralRequest;
class GeneralResponse;
struct OperationResult;

namespace aql {
class Query;
}

/// @brief Event logging functions for system monitoring and auditing
///
/// This namespace contains functions for logging various system events
/// in ArangoDB. These events are used for monitoring, auditing, and
/// debugging purposes. Each function logs a specific type of event
/// with relevant context and metadata.
///
/// The events are typically used by:
/// - Security monitoring systems
/// - Audit logging systems
/// - System monitoring and alerting
/// - Performance analysis tools
///
/// @note Events are logged asynchronously to avoid blocking operations
/// @note Event logging can be configured and filtered based on system settings
/// @note Events may be stored in log files, databases, or external systems
namespace events {

/// @brief Log an unknown authentication method event
///
/// Records an event when an unknown or unsupported authentication method
/// is encountered during a request. This typically indicates a configuration
/// issue or an attempt to use an unsupported authentication mechanism.
///
/// @param request The request that triggered this event
///
/// @note This event is useful for identifying authentication configuration issues
/// @note May indicate security-related problems or client misconfigurations
void UnknownAuthenticationMethod(GeneralRequest const&);

/// @brief Log a missing credentials event
///
/// Records an event when a request requires authentication but no credentials
/// are provided. This is a common security event that indicates unauthorized
/// access attempts or client configuration issues.
///
/// @param request The request that triggered this event
///
/// @note This event helps identify unauthorized access attempts
/// @note May indicate client applications missing authentication configuration
void CredentialsMissing(GeneralRequest const&);

/// @brief Log a successful login event
///
/// Records an event when a user successfully logs in to the system.
/// This is an important security event for tracking user access patterns
/// and session management.
///
/// @param request The request that triggered this event
/// @param username The username of the user who logged in
///
/// @note This event is useful for tracking user access patterns
/// @note Helps with security monitoring and session management
void LoggedIn(GeneralRequest const&, std::string const& username);

/// @brief Log a bad credentials event for specific user
///
/// Records an event when authentication fails due to incorrect credentials
/// for a specific user. This is a security event that may indicate
/// brute-force attacks or password issues.
///
/// @param request The request that triggered this event
/// @param username The username for which authentication failed
///
/// @note This event helps identify potential security threats
/// @note May indicate brute-force attacks or password problems
void CredentialsBad(GeneralRequest const&, std::string const& username);

/// @brief Log a bad credentials event for authentication method
///
/// Records an event when authentication fails due to incorrect credentials
/// for a specific authentication method. This helps identify issues with
/// specific authentication mechanisms.
///
/// @param request The request that triggered this event
/// @param method The authentication method that failed
///
/// @note This event helps identify issues with specific authentication methods
/// @note Useful for debugging authentication configuration problems
void CredentialsBad(GeneralRequest const&, rest::AuthenticationMethod);

/// @brief Log a successful authentication event
///
/// Records an event when a user successfully authenticates using a specific
/// authentication method. This is distinct from login and tracks the
/// authentication mechanism used.
///
/// @param request The request that triggered this event
/// @param method The authentication method that was used
///
/// @note This event tracks which authentication methods are being used
/// @note Helps with security monitoring and method effectiveness analysis
void Authenticated(GeneralRequest const&, rest::AuthenticationMethod);

/// @brief Log an authorization failure event
///
/// Records an event when a user is authenticated but lacks authorization
/// to perform the requested operation. This is a security event that
/// indicates insufficient permissions.
///
/// @param request The request that triggered this event
///
/// @note This event helps identify permission-related issues
/// @note May indicate privilege escalation attempts or misconfiguration
void NotAuthorized(GeneralRequest const&);

/// @brief Log a collection creation event
///
/// Records an event when a collection is created in the database.
/// This is an important administrative event for tracking database
/// structure changes.
///
/// @param db Database name where the collection was created
/// @param name Name of the collection that was created
/// @param result Result code indicating success or failure
///
/// @note This event tracks database structure changes
/// @note Useful for auditing administrative operations
void CreateCollection(std::string const& db, std::string const& name,
                      ErrorCode result);

/// @brief Log a collection creation event with string view
///
/// Records an event when a collection is created in the database.
/// This overload accepts a string view for the collection name,
/// providing better performance for temporary strings.
///
/// @param db Database name where the collection was created
/// @param name Name of the collection that was created (string view)
/// @param result Result code indicating success or failure
///
/// @note This event tracks database structure changes
/// @note Optimized version for temporary string handling
void CreateCollection(std::string const& db, std::string_view name,
                      ErrorCode result);

/// @brief Log a collection deletion event
///
/// Records an event when a collection is deleted from the database.
/// This is an important administrative event for tracking database
/// structure changes and potential data loss.
///
/// @param db Database name where the collection was deleted
/// @param name Name of the collection that was deleted
/// @param result Result code indicating success or failure
///
/// @note This event tracks database structure changes
/// @note Critical for auditing data deletion operations
void DropCollection(std::string const& db, std::string const& name,
                    ErrorCode result);

/// @brief Log a collection property update event
///
/// Records an event when collection properties are modified.
/// This includes changes to collection settings, schema, or other
/// metadata that affects collection behavior.
///
/// @param db Database name containing the collection
/// @param collectionName Name of the collection that was modified
/// @param result Operation result containing success/failure information
///
/// @note This event tracks collection configuration changes
/// @note Useful for auditing administrative modifications
void PropertyUpdateCollection(std::string const& db,
                              std::string const& collectionName,
                              OperationResult const&);

/// @brief Log a collection truncation event
///
/// Records an event when a collection is truncated (all documents removed).
/// This is a critical administrative event as it represents potential
/// data loss and significant database changes.
///
/// @param db Database name containing the collection
/// @param name Name of the collection that was truncated
/// @param result Operation result containing success/failure information
///
/// @note This event tracks data deletion operations
/// @note Critical for auditing bulk data removal
void TruncateCollection(std::string const& db, std::string const& name,
                        OperationResult const& result);

/// @brief Log a database creation event
///
/// Records an event when a new database is created in the system.
/// This is a high-level administrative event that affects the entire
/// system structure.
///
/// @param name Name of the database that was created
/// @param result Result indicating success or failure of the operation
/// @param context Execution context containing user and authorization information
///
/// @note This event tracks system-level administrative changes
/// @note Critical for auditing database lifecycle management
void CreateDatabase(std::string const& name, Result const& result,
                    ExecContext const& context);

/// @brief Log a database deletion event
///
/// Records an event when a database is deleted from the system.
/// This is a critical administrative event that represents significant
/// data loss and system structure changes.
///
/// @param name Name of the database that was deleted
/// @param result Result indicating success or failure of the operation
/// @param context Execution context containing user and authorization information
///
/// @note This event tracks system-level administrative changes
/// @note Critical for auditing database lifecycle management and data loss
void DropDatabase(std::string const& name, Result const& result,
                  ExecContext const& context);

/// @brief Log the start of an index creation operation
///
/// Records an event when an index creation operation begins.
/// This is paired with CreateIndexEnd to track the complete lifecycle
/// of index creation operations.
///
/// @param db Database name containing the collection
/// @param col Collection name where the index is being created
/// @param slice VelocyPack slice containing index definition
///
/// @note This event tracks the start of index creation operations
/// @note Used to monitor index creation performance and lifecycle
void CreateIndexStart(std::string const& db, std::string const& col,
                      VPackSlice slice);

/// @brief Log the completion of an index creation operation
///
/// Records an event when an index creation operation completes.
/// This is paired with CreateIndexStart to track the complete lifecycle
/// of index creation operations.
///
/// @param db Database name containing the collection
/// @param col Collection name where the index was created
/// @param slice VelocyPack slice containing index definition
/// @param result Result code indicating success or failure
///
/// @note This event tracks the completion of index creation operations
/// @note Used to monitor index creation performance and success rates
void CreateIndexEnd(std::string const& db, std::string const& col,
                    VPackSlice slice, ErrorCode result);

/// @brief Log an index deletion event
///
/// Records an event when an index is deleted from a collection.
/// This is an administrative event that affects query performance
/// and database structure.
///
/// @param db Database name containing the collection
/// @param col Collection name where the index was deleted
/// @param idx Index identifier or name that was deleted
/// @param result Result code indicating success or failure
///
/// @note This event tracks database structure changes
/// @note Important for auditing performance-related modifications
void DropIndex(std::string const& db, std::string const& col,
               std::string const& idx, ErrorCode result);

/// @brief Log a view creation event
///
/// Records an event when a view is created in the database.
/// Views are virtual collections that provide filtered or aggregated
/// access to underlying data.
///
/// @param db Database name where the view was created
/// @param name Name of the view that was created
/// @param result Result code indicating success or failure
///
/// @note This event tracks database structure changes
/// @note Important for auditing view lifecycle management
void CreateView(std::string const& db, std::string const& name,
                ErrorCode result);

/// @brief Log a view deletion event
///
/// Records an event when a view is deleted from the database.
/// This affects applications that depend on the view for data access.
///
/// @param db Database name where the view was deleted
/// @param name Name of the view that was deleted
/// @param result Result code indicating success or failure
///
/// @note This event tracks database structure changes
/// @note Important for auditing view lifecycle management
void DropView(std::string const& db, std::string const& name, ErrorCode result);

/// @brief Log a document creation event
///
/// Records an event when a document is created in a collection.
/// This is a data-level event that tracks document lifecycle and
/// can be used for auditing data changes.
///
/// @param db Database name containing the collection
/// @param collection Collection name where the document was created
/// @param document VelocyPack slice containing the document data
/// @param options Operation options that were used
/// @param code Result code indicating success or failure
///
/// @note This event tracks document lifecycle changes
/// @note May be used for data auditing and change tracking
void CreateDocument(std::string const& db, std::string const& collection,
                    VPackSlice const& document, OperationOptions const& options,
                    ErrorCode code);

/// @brief Log a document deletion event
///
/// Records an event when a document is deleted from a collection.
/// This is a data-level event that tracks document lifecycle and
/// represents potential data loss.
///
/// @param db Database name containing the collection
/// @param collection Collection name where the document was deleted
/// @param document VelocyPack slice containing the document data
/// @param options Operation options that were used
/// @param code Result code indicating success or failure
///
/// @note This event tracks document lifecycle changes
/// @note Critical for auditing data deletion operations
void DeleteDocument(std::string const& db, std::string const& collection,
                    VPackSlice const& document, OperationOptions const& options,
                    ErrorCode code);

/// @brief Log a document read event
///
/// Records an event when a document is read from a collection.
/// This is a data-level event that can be used for access monitoring
/// and security auditing.
///
/// @param db Database name containing the collection
/// @param collection Collection name where the document was read
/// @param document VelocyPack slice containing the document data
/// @param options Operation options that were used
/// @param code Result code indicating success or failure
///
/// @note This event tracks document access patterns
/// @note May be used for security monitoring and access auditing
void ReadDocument(std::string const& db, std::string const& collection,
                  VPackSlice const& document, OperationOptions const& options,
                  ErrorCode code);

/// @brief Log a document replacement event
///
/// Records an event when a document is replaced in a collection.
/// This is a data-level event that tracks document lifecycle and
/// represents data modification.
///
/// @param db Database name containing the collection
/// @param collection Collection name where the document was replaced
/// @param document VelocyPack slice containing the document data
/// @param options Operation options that were used
/// @param code Result code indicating success or failure
///
/// @note This event tracks document lifecycle changes
/// @note Important for auditing data modification operations
void ReplaceDocument(std::string const& db, std::string const& collection,
                     VPackSlice const& document,
                     OperationOptions const& options, ErrorCode code);

/// @brief Log a document modification event
///
/// Records an event when a document is modified (partially updated) in a collection.
/// This is a data-level event that tracks document lifecycle and
/// represents data modification.
///
/// @param db Database name containing the collection
/// @param collection Collection name where the document was modified
/// @param document VelocyPack slice containing the document data
/// @param options Operation options that were used
/// @param code Result code indicating success or failure
///
/// @note This event tracks document lifecycle changes
/// @note Important for auditing data modification operations
void ModifyDocument(std::string const& db, std::string const& collection,
                    VPackSlice const& document, OperationOptions const& options,
                    ErrorCode code);

/// @brief Log an illegal document operation event
///
/// Records an event when an illegal or unauthorized document operation
/// is attempted. This is a security event that may indicate malicious
/// activity or application errors.
///
/// @param request The request that triggered this event
/// @param result HTTP response code indicating the type of error
///
/// @note This event helps identify security threats and application errors
/// @note May indicate malicious activity or incorrect API usage
void IllegalDocumentOperation(GeneralRequest const& request,
                              rest::ResponseCode result);

/// @brief Log an AQL query execution event
///
/// Records an event when an AQL query is executed. This is important
/// for performance monitoring, security auditing, and system analysis.
///
/// @param query The AQL query that was executed
///
/// @note This event tracks query execution patterns
/// @note Important for performance monitoring and security auditing
/// @note May contain sensitive information depending on query content
void AqlQuery(aql::Query const& query);

/// @brief Log a hot backup creation event
///
/// Records an event when a hot backup is created. Hot backups are
/// consistent snapshots of the database that can be used for recovery
/// and disaster protection.
///
/// @param id Identifier of the hot backup that was created
/// @param result Result code indicating success or failure
///
/// @note This event tracks backup operations
/// @note Critical for auditing data protection operations
void CreateHotbackup(std::string const& id, ErrorCode result);

/// @brief Log a hot backup restoration event
///
/// Records an event when a hot backup is restored. This is a critical
/// administrative operation that replaces current data with backup data.
///
/// @param id Identifier of the hot backup that was restored
/// @param result Result code indicating success or failure
///
/// @note This event tracks backup restoration operations
/// @note Critical for auditing data recovery operations
void RestoreHotbackup(std::string const& id, ErrorCode result);

/// @brief Log a hot backup deletion event
///
/// Records an event when a hot backup is deleted. This affects the
/// system's ability to recover from that backup point.
///
/// @param id Identifier of the hot backup that was deleted
/// @param result Result code indicating success or failure
///
/// @note This event tracks backup management operations
/// @note Important for auditing backup lifecycle management
void DeleteHotbackup(std::string const& id, ErrorCode result);

}  // namespace events
}  // namespace arangodb
