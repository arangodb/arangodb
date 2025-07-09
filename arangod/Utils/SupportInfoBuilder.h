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
/// @author Julia Puget
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "RestServer/arangod.h"

namespace arangodb {

namespace velocypack {
class Builder;
class Slice;
}  // namespace velocypack

/// @brief Utility class for building comprehensive support information messages
///
/// This class provides static methods to collect and format system information
/// that is useful for support and diagnostic purposes. It builds structured
/// VelocyPack messages containing database, server, and host information that
/// can be used for telemetrics, support tickets, and system monitoring.
///
/// The SupportInfoBuilder provides:
/// - Comprehensive server and database information collection
/// - Telemetrics-specific data formatting and normalization
/// - Host system information gathering
/// - Database server data storage statistics
/// - Consistent structured output format
///
/// @note This class contains only static methods and cannot be instantiated
/// @note All methods work with VelocyPack format for efficient serialization
/// @note Designed for both local and remote support information collection
/// @note Used for telemetrics reporting and support diagnostics
class SupportInfoBuilder {
 public:
  /// @brief Deleted default constructor
  ///
  /// This class is designed to be used as a utility class with static methods
  /// only. Direct instantiation is not allowed or necessary.
  SupportInfoBuilder() = delete;

  /// @brief Build comprehensive support information message
  ///
  /// Creates a structured VelocyPack message containing comprehensive support
  /// information including database details, server configuration, and host
  /// system information. The message can be customized for local or remote
  /// collection and for telemetrics purposes.
  ///
  /// @param result VelocyPack builder to store the support information
  /// @param dbName Name of the database to include information about
  /// @param server Reference to the ArangodServer instance
  /// @param isLocal Whether this is a local information collection
  /// @param isTemeletricsReq Whether this is for telemetrics purposes (defaults to false)
  ///
  /// @note The result builder will contain structured support information
  /// @note Telemetrics requests may have normalized keys and filtered data
  /// @note Database name is used to include database-specific information
  /// @note Server reference provides access to configuration and runtime data
  static void buildInfoMessage(velocypack::Builder& result,
                               std::string const& dbName, ArangodServer& server,
                               bool isLocal, bool isTemeletricsReq = false);

  /// @brief Build database server data storage information
  ///
  /// Collects and formats information about data storage on database servers,
  /// including statistics about collections, indexes, and storage usage.
  /// This information is useful for capacity planning and performance analysis.
  ///
  /// @param result VelocyPack builder to store the data storage information
  /// @param server Reference to the ArangodServer instance
  ///
  /// @note Provides detailed storage statistics for database servers
  /// @note Includes collection and index information
  /// @note Used for capacity planning and performance monitoring
  /// @note Results are formatted in VelocyPack for efficient processing
  static void buildDbServerDataStoredInfo(velocypack::Builder& result,
                                          ArangodServer& server);

 private:
  /// @brief Add database-specific information to the result
  ///
  /// Processes and adds database-specific information from the provided
  /// info slice to the result builder. This includes database configuration,
  /// statistics, and other database-related metadata.
  ///
  /// @param result VelocyPack builder to store the database information
  /// @param infoSlice VelocyPack slice containing raw database information
  /// @param server Reference to the ArangodServer instance
  ///
  /// @note Processes raw database information into structured format
  /// @note Includes database configuration and statistics
  /// @note Used internally by buildInfoMessage
  static void addDatabaseInfo(velocypack::Builder& result,
                              velocypack::Slice infoSlice,
                              ArangodServer& server);

  /// @brief Build host system information
  ///
  /// Collects and formats information about the host system including
  /// hardware specifications, operating system details, and runtime
  /// environment information. The information can be customized for
  /// telemetrics purposes.
  ///
  /// @param result VelocyPack builder to store the host information
  /// @param server Reference to the ArangodServer instance
  /// @param isTelemetricsReq Whether this is for telemetrics purposes
  ///
  /// @note Includes hardware and OS information
  /// @note Telemetrics requests may have normalized keys
  /// @note Used for system diagnostics and support
  /// @note Provides runtime environment details
  static void buildHostInfo(velocypack::Builder& result, ArangodServer& server,
                            bool isTelemetricsReq);

  /// @brief Normalize key names for telemetrics reporting
  ///
  /// Transforms key names to be suitable for telemetrics reporting by
  /// applying consistent naming conventions and removing potentially
  /// sensitive information. This ensures consistent data format for
  /// telemetrics processing.
  ///
  /// @param key Key name to normalize (modified in-place)
  ///
  /// @note Modifies the key in-place for efficiency
  /// @note Applies consistent naming conventions for telemetrics
  /// @note May remove or transform sensitive information
  /// @note Used internally when building telemetrics data
  static void normalizeKeyForTelemetrics(std::string& key);
};

}  // namespace arangodb
