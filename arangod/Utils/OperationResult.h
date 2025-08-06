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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Basics/debugging.h"
#include "Basics/Result.h"
#include "Utils/OperationOptions.h"

#include <velocypack/Buffer.h>
#include <velocypack/Options.h>
#include <velocypack/Slice.h>

#include <unordered_map>

namespace arangodb {

/// @brief Result of a database operation with operation-specific details
///
/// This structure combines operation success/failure status with detailed
/// operation results and metadata. It wraps a Result object and extends it
/// with operation-specific information including document data, operation
/// options, and error counts for batch operations.
///
/// The OperationResult provides:
/// - Operation success/failure status via embedded Result
/// - Document data or operation results via VelocyPack buffer
/// - Operation options and configuration used during the operation
/// - Error counts for batch operations that can partially fail
///
/// @note This struct is move-only for efficiency and safety
/// @note The buffer must remain valid for the lifetime of this object
/// @note Used extensively in transaction operations and document manipulation
struct OperationResult final {
  /// @brief Create OperationResult from existing Result with options
  ///
  /// Creates an OperationResult by copying an existing Result object and
  /// associating it with specific operation options. This is typically used
  /// when an operation completes but doesn't return data.
  ///
  /// @param other Result object to copy status from
  /// @param options Operation options that were used for this operation
  ///
  /// @note The buffer will be null since no data is provided
  explicit OperationResult(Result const& other, OperationOptions options)
      : result(other), options(std::move(options)) {}

  /// @brief Create OperationResult from moved Result with options
  ///
  /// Creates an OperationResult by moving an existing Result object and
  /// associating it with specific operation options. This is the preferred
  /// constructor when you have a temporary Result object.
  ///
  /// @param other Result object to move from
  /// @param options Operation options that were used for this operation
  ///
  /// @note The buffer will be null since no data is provided
  explicit OperationResult(Result&& other, OperationOptions options)
      : result(std::move(other)), options(std::move(options)) {}

  /// @brief Deleted copy constructor
  ///
  /// Copy construction is disabled to prevent accidental copying of
  /// potentially large buffers and to enforce move semantics.
  OperationResult(OperationResult const& other) = delete;

  /// @brief Deleted copy assignment operator
  ///
  /// Copy assignment is disabled to prevent accidental copying of
  /// potentially large buffers and to enforce move semantics.
  OperationResult& operator=(OperationResult const& other) = delete;

  /// @brief Default move constructor
  ///
  /// Enables efficient transfer of OperationResult objects. All members
  /// are moved, leaving the source object in a valid but unspecified state.
  ///
  /// @param other OperationResult to move from
  OperationResult(OperationResult&& other) = default;

  /// @brief Move assignment operator
  ///
  /// Efficiently transfers ownership of an OperationResult object.
  /// All members are moved individually to ensure proper resource management.
  ///
  /// @param other OperationResult to move from
  /// @return Reference to this object
  ///
  /// @note Self-assignment is handled safely
  OperationResult& operator=(OperationResult&& other) noexcept {
    if (this != &other) {
      result = std::move(other.result);
      buffer = std::move(other.buffer);
      options = std::move(other.options);
      countErrorCodes = std::move(other.countErrorCodes);
    }
    return *this;
  }

  /// @brief Create OperationResult with complete operation details
  ///
  /// Creates an OperationResult with all operation details including result
  /// status, data buffer, options, and error counts. This is the primary
  /// constructor for operations that return data.
  ///
  /// @param result Operation result status and error information
  /// @param buffer Shared buffer containing VelocyPack data
  /// @param options Operation options that were used
  /// @param countErrorCodes Error count summary for batch operations
  ///
  /// @note For successful operations, buffer must not be null and must contain valid data
  /// @note Buffer data pointer is verified for successful operations
  OperationResult(Result result, std::shared_ptr<VPackBuffer<uint8_t>> buffer,
                  OperationOptions options,
                  std::unordered_map<ErrorCode, size_t> countErrorCodes = {})
      : result(std::move(result)),
        buffer(std::move(buffer)),
        options(std::move(options)),
        countErrorCodes(std::move(countErrorCodes)) {
    if (this->result.ok()) {
      TRI_ASSERT(this->buffer != nullptr);
      TRI_ASSERT(this->buffer->data() != nullptr);
    }
  }

  /// @brief Default destructor
  ///
  /// Cleans up the OperationResult. Since the buffer is managed by shared_ptr,
  /// no manual cleanup is required.
  ~OperationResult() = default;

  /// @brief Check if the operation succeeded
  ///
  /// Provides a convenient way to check if the operation completed successfully
  /// without any errors. This is the primary success indicator.
  ///
  /// @return true if operation succeeded, false otherwise
  ///
  /// @note This delegates to the embedded Result object
  bool ok() const noexcept { return result.ok(); }

  /// @brief Check if the operation failed
  ///
  /// Provides a convenient way to check if the operation failed with any error.
  /// This is the complement of ok() and is useful for error handling.
  ///
  /// @return true if operation failed, false otherwise
  ///
  /// @note This delegates to the embedded Result object
  bool fail() const noexcept { return result.fail(); }

  /// @brief Get the error code for failed operations
  ///
  /// Returns the specific error code that caused the operation to fail.
  /// For successful operations, this returns TRI_ERROR_NO_ERROR.
  ///
  /// @return Error code indicating the type of failure
  ///
  /// @note This delegates to the embedded Result object
  ErrorCode errorNumber() const noexcept { return result.errorNumber(); }

  /// @brief Check if the operation failed with a specific error
  ///
  /// Determines whether the operation failed with the specified error code.
  /// This is useful for handling specific error conditions.
  ///
  /// @param errorNumber Error code to check for
  /// @return true if operation failed with this error, false otherwise
  ///
  /// @note This delegates to the embedded Result object
  bool is(ErrorCode errorNumber) const noexcept {
    return result.is(errorNumber);
  }

  /// @brief Check if the operation did not fail with a specific error
  ///
  /// Determines whether the operation either succeeded or failed with a
  /// different error code than the one specified.
  ///
  /// @param errorNumber Error code to check against
  /// @return true if operation did not fail with this error, false otherwise
  ///
  /// @note This delegates to the embedded Result object
  bool isNot(ErrorCode errorNumber) const noexcept {
    return result.isNot(errorNumber);
  }

  /// @brief Get the error message for failed operations
  ///
  /// Returns a human-readable error message describing why the operation failed.
  /// For successful operations, this may be empty or contain a success message.
  ///
  /// @return Error message string view
  ///
  /// @note This delegates to the embedded Result object
  /// @note The returned string view is only valid as long as the Result exists
  std::string_view errorMessage() const { return result.errorMessage(); }

  /// @brief Check if operation result contains data
  ///
  /// Determines whether this operation result contains actual data in its
  /// VelocyPack buffer. This is useful for distinguishing between operations
  /// that return data and those that don't.
  ///
  /// @return true if buffer contains data, false if no data available
  ///
  /// @note A null buffer indicates no data was returned by the operation
  /// @note Some successful operations may not return data (e.g., DELETE operations)
  bool hasSlice() const noexcept { return buffer != nullptr; }

  /// @brief Get the VelocyPack slice containing operation data
  ///
  /// Returns the VelocyPack slice that contains the operation result data.
  /// This is typically used to access document data, query results, or
  /// operation metadata returned by the database operation.
  ///
  /// @return VelocyPack slice containing operation data
  ///
  /// @note Buffer must not be null when calling this method
  /// @note The slice is only valid as long as the buffer exists
  /// @note Assertion will fail if buffer is null in debug builds
  velocypack::Slice slice() const noexcept {
    TRI_ASSERT(buffer != nullptr);
    return VPackSlice(buffer->data());
  }

  /// @brief Reset the operation result to default state
  ///
  /// Clears all operation result data and resets the object to its default
  /// state. This is useful for reusing OperationResult objects in loops
  /// or when you need to clear previous operation results.
  ///
  /// @note All data will be lost after calling this method
  /// @note The buffer reference will be released
  void reset() noexcept {
    result.reset();
    buffer.reset();
    options = OperationOptions();
    countErrorCodes.clear();
  }

  /// @brief Operation result status and error information
  ///
  /// Contains the success/failure status of the operation along with any
  /// error codes and messages. This is the primary indicator of operation
  /// success and provides detailed error information for failed operations.
  Result result;

  /// @brief Shared buffer containing VelocyPack operation data
  ///
  /// Contains the actual data returned by the operation in VelocyPack format.
  /// This may include document data, query results, or operation metadata.
  /// The buffer is shared to enable efficient passing without copying.
  ///
  /// @note May be null for operations that don't return data
  /// @note Buffer lifetime must exceed the lifetime of any slices derived from it
  std::shared_ptr<velocypack::Buffer<uint8_t>> buffer;

  /// @brief Operation options that were used for this operation
  ///
  /// Contains the configuration options that were applied during the operation.
  /// This includes settings like write concern, timeout values, and other
  /// operation-specific parameters that influenced the operation behavior.
  OperationOptions options;

  /// @brief Error count summary for batch operations
  ///
  /// Executive summary for batch operations that reports all errors that
  /// occurred during the operation. Each error code is mapped to the number
  /// of times it occurred. Detailed error information for individual
  /// documents is stored in the respective positions of the result buffer.
  ///
  /// @note This is primarily used for batch operations where some documents
  ///       may succeed while others fail
  /// @note Empty for single-document operations or fully successful batch operations
  std::unordered_map<ErrorCode, size_t> countErrorCodes;
};

}  // namespace arangodb
