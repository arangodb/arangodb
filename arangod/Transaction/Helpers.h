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

#include <string_view>
#include "Transaction/CountCache.h"
#include "Utils/OperationResult.h"
#include "VocBase/Identifiers/DataSourceId.h"
#include "VocBase/Identifiers/LocalDocumentId.h"
#include "VocBase/Identifiers/RevisionId.h"
#include "VocBase/voc-types.h"

namespace arangodb {

class CollectionNameResolver;
class LogicalCollection;
struct OperationOptions;

namespace velocypack {

class Builder;
class Slice;

}  // namespace velocypack
namespace transaction {

struct BatchOptions;
class Context;
class Methods;

namespace helpers {
/// @brief Extract the _key attribute from a VelocyPack slice
///
/// This function extracts the _key attribute from a VelocyPack document slice.
/// It provides information about whether the key was present in the document.
///
/// @param slice The VelocyPack slice containing the document
/// @param keyPresent Output parameter set to true if _key was found, false otherwise
/// @return String view of the key value, or empty string_view if not present
///
/// @note The returned string_view is valid only as long as the input slice remains valid
/// @note For performance reasons, this function assumes the _key attribute is at a predictable position
///
/// Example:
/// @code
/// velocypack::Slice document = ...;
/// bool keyPresent = false;
/// std::string_view key = extractKeyPart(document, keyPresent);
/// if (keyPresent) {
///   std::cout << "Document key: " << key << std::endl;
/// }
/// @endcode
std::string_view extractKeyPart(velocypack::Slice slice, bool& keyPresent);

/// @brief Extract the _key attribute from a VelocyPack slice
///
/// This is a simplified version of extractKeyPart that doesn't provide information
/// about whether the key was present. It returns an empty string_view if the key
/// is not found.
///
/// @param slice The VelocyPack slice containing the document
/// @return String view of the key value, or empty string_view if not present
///
/// @note The returned string_view is valid only as long as the input slice remains valid
/// @note This function is optimized for performance and assumes standard document structure
///
/// Example:
/// @code
/// velocypack::Slice document = ...;
/// std::string_view key = extractKeyPart(document);
/// if (!key.empty()) {
///   std::cout << "Document key: " << key << std::endl;
/// }
/// @endcode
std::string_view extractKeyPart(velocypack::Slice slice);

/// @brief Extract the key part from a document ID string
///
/// Given a document ID string in the format "collection/key", this function
/// returns the key part (everything after the first '/') or the whole string
/// if no '/' is found.
///
/// @param id The document ID string to parse
/// @return String view of the key part, or the entire string if no '/' is found
///
/// @note The returned string_view is valid only as long as the input string remains valid
/// @note This function is used to extract keys from full document IDs
///
/// Example:
/// @code
/// std::string_view docId = "users/john_doe";
/// std::string_view key = extractKeyPart(docId);
/// // key will be "john_doe"
/// 
/// std::string_view justKey = "simple_key";
/// std::string_view extracted = extractKeyPart(justKey);
/// // extracted will be "simple_key"
/// @endcode
std::string_view extractKeyPart(std::string_view);

/// @brief Extract a document ID string from VelocyPack slices
///
/// This function constructs a document ID string from a VelocyPack slice and base slice.
/// It uses the CollectionNameResolver to resolve collection names and handles both
/// regular document IDs and custom ID formats.
///
/// @param resolver Collection name resolver for converting collection IDs to names
/// @param slice The VelocyPack slice containing the document or ID information
/// @param base The base VelocyPack slice for context (may be the document itself)
/// @return A string containing the full document ID in "collection/key" format
///
/// @note The resolver parameter must not be null
/// @note This function handles both standard and custom ID formats
/// @note The returned string is a newly allocated string, not a view
///
/// Example:
/// @code
/// CollectionNameResolver resolver = ...;
/// velocypack::Slice document = ...;
/// velocypack::Slice base = ...;
/// std::string docId = extractIdString(&resolver, document, base);
/// // docId might be "users/john_doe"
/// @endcode
std::string extractIdString(CollectionNameResolver const* resolver,
                            VPackSlice slice, VPackSlice base);

/// @brief Extract the _key attribute from a database document
///
/// This function provides optimized access to the _key attribute in a database document.
/// It assumes the document follows ArangoDB's standard structure where _key is the first
/// attribute in the document.
///
/// @param slice The VelocyPack slice containing the database document
/// @return VPackSlice pointing to the _key value
///
/// @note The document must have at least two attributes with _key as the first one
/// @note This is a performance-optimized function that bypasses generic attribute lookup
/// @note The returned slice is valid only as long as the input slice remains valid
/// @note Do not use this function for arbitrary objects - only for ArangoDB documents
///
/// @warning This function makes assumptions about document structure for performance.
///          Use only with verified ArangoDB documents.
///
/// Example:
/// @code
/// velocypack::Slice document = ...;  // ArangoDB document
/// velocypack::Slice keySlice = extractKeyFromDocument(document);
/// std::string_view key = keySlice.stringView();
/// @endcode
VPackSlice extractKeyFromDocument(VPackSlice);

/// @brief Extract the _id attribute from a database document
///
/// This function provides optimized access to the _id attribute in a database document.
/// It assumes the document follows ArangoDB's standard structure where _id is the second
/// attribute in the document.
///
/// @param slice The VelocyPack slice containing the database document
/// @return VPackSlice pointing to the _id value (may be of type Custom)
///
/// @note The document must have at least two attributes with _id as the second one
/// @note This may return a Slice of type Custom for efficiency reasons
/// @note The returned slice is valid only as long as the input slice remains valid
/// @note Do not use this function for arbitrary objects - only for ArangoDB documents
///
/// @warning Do NOT call this method when:
/// - The input slice is not a database document
/// - You are not willing to deal with slices of type Custom
/// @warning This function makes assumptions about document structure for performance.
///          Use only with verified ArangoDB documents.
///
/// Example:
/// @code
/// velocypack::Slice document = ...;  // ArangoDB document
/// velocypack::Slice idSlice = extractIdFromDocument(document);
/// // Handle Custom type appropriately
/// if (idSlice.isCustom()) {
///   // Process custom ID format
/// } else {
///   std::string_view id = idSlice.stringView();
/// }
/// @endcode
VPackSlice extractIdFromDocument(VPackSlice);

/// @brief Extract the _from attribute from an edge document
///
/// This function provides optimized access to the _from attribute in an edge document.
/// It assumes the document follows ArangoDB's standard edge structure where attributes
/// are ordered as: _key, _id, _from, _to, _rev.
///
/// @param slice The VelocyPack slice containing the edge document
/// @return VPackSlice pointing to the _from value
///
/// @note The document must have at least five attributes in the expected order
/// @note This function is specifically designed for edge documents
/// @note The returned slice is valid only as long as the input slice remains valid
/// @note Do not use this function for regular documents - only for edge documents
///
/// @warning This function makes assumptions about edge document structure for performance.
///          Use only with verified ArangoDB edge documents.
///
/// Example:
/// @code
/// velocypack::Slice edgeDoc = ...;  // ArangoDB edge document
/// velocypack::Slice fromSlice = extractFromFromDocument(edgeDoc);
/// std::string_view fromVertex = fromSlice.stringView();
/// // fromVertex might be "vertices/vertex1"
/// @endcode
VPackSlice extractFromFromDocument(VPackSlice);

/// @brief Extract the _to attribute from an edge document
///
/// This function provides optimized access to the _to attribute in an edge document.
/// It assumes the document follows ArangoDB's standard edge structure where attributes
/// are ordered as: _key, _id, _from, _to, _rev.
///
/// @param slice The VelocyPack slice containing the edge document
/// @return VPackSlice pointing to the _to value
///
/// @note The document must have at least five attributes in the expected order
/// @note This function is specifically designed for edge documents
/// @note The returned slice is valid only as long as the input slice remains valid
/// @note Do not use this function for regular documents - only for edge documents
///
/// @warning This function makes assumptions about edge document structure for performance.
///          Use only with verified ArangoDB edge documents.
///
/// Example:
/// @code
/// velocypack::Slice edgeDoc = ...;  // ArangoDB edge document
/// velocypack::Slice toSlice = extractToFromDocument(edgeDoc);
/// std::string_view toVertex = toSlice.stringView();
/// // toVertex might be "vertices/vertex2"
/// @endcode
VPackSlice extractToFromDocument(VPackSlice);

/// @brief Extract both _key and _rev from a document in one optimized operation
///
/// This function provides optimized access to both the _key and _rev attributes
/// from a database document in a single operation. It's specifically designed for
/// high-performance scenarios like collection loading, WAL operations, and compaction.
///
/// @param slice The VelocyPack slice containing the database document
/// @param keySlice Output parameter that will contain the _key VPackSlice
/// @param revisionId Output parameter that will contain the parsed RevisionId
///
/// @note This is an optimized version used in performance-critical operations
/// @note The function assumes standard ArangoDB document structure
/// @note keySlice will be valid only as long as the input slice remains valid
/// @note revisionId will be properly constructed from the document's _rev attribute
///
/// @warning Use only with verified ArangoDB documents that contain both _key and _rev
///
/// Example:
/// @code
/// velocypack::Slice document = ...;  // ArangoDB document
/// velocypack::Slice keySlice;
/// RevisionId revisionId;
/// extractKeyAndRevFromDocument(document, keySlice, revisionId);
/// 
/// std::string_view key = keySlice.stringView();
/// uint64_t rev = revisionId.id();
/// @endcode
void extractKeyAndRevFromDocument(VPackSlice slice, VPackSlice& keySlice,
                                  RevisionId& revisionId);

/// @brief Extract the collection name from a full document ID
///
/// This function extracts the collection name from a document ID string.
/// Document IDs in ArangoDB have the format "collection/key", and this function
/// returns the collection part (everything before the first '/').
///
/// @param id The document ID string to parse
/// @return String view of the collection name, or empty string_view if no '/' is found
///
/// @note The returned string_view is valid only as long as the input string remains valid
/// @note If no '/' is found, the function returns an empty string_view
///
/// Example:
/// @code
/// std::string_view docId = "users/john_doe";
/// std::string_view collection = extractCollectionFromId(docId);
/// // collection will be "users"
/// 
/// std::string_view justKey = "simple_key";
/// std::string_view extracted = extractCollectionFromId(justKey);
/// // extracted will be empty string_view
/// @endcode
std::string_view extractCollectionFromId(std::string_view id);

/// @brief Extract the _rev attribute from a database document
///
/// This function extracts the revision ID from a database document and returns
/// it as a RevisionId object. The RevisionId provides access to both numeric
/// and HLC (Hybrid Logical Clock) revision formats.
///
/// @param slice The VelocyPack slice containing the database document
/// @return RevisionId object containing the document's revision
///
/// @note The function handles both numeric and HLC revision formats
/// @note Returns a properly constructed RevisionId that can be used for comparisons
/// @note The function assumes standard ArangoDB document structure
///
/// Example:
/// @code
/// velocypack::Slice document = ...;  // ArangoDB document
/// RevisionId revisionId = extractRevFromDocument(document);
/// uint64_t rev = revisionId.id();
/// bool isSet = revisionId.isSet();
/// @endcode
RevisionId extractRevFromDocument(VPackSlice slice);

/// @brief Extract the _rev attribute as a VPackSlice from a database document
///
/// This function extracts the revision attribute from a database document and returns
/// it as a VPackSlice without parsing it into a RevisionId. This is useful when you
/// need to work with the raw revision data or pass it to other functions.
///
/// @param slice The VelocyPack slice containing the database document
/// @return VPackSlice pointing to the _rev value
///
/// @note The returned slice is valid only as long as the input slice remains valid
/// @note This function provides access to the raw revision data without parsing
/// @note The function assumes standard ArangoDB document structure
///
/// Example:
/// @code
/// velocypack::Slice document = ...;  // ArangoDB document
/// velocypack::Slice revSlice = extractRevSliceFromDocument(document);
/// // Use revSlice for further processing or comparison
/// @endcode
VPackSlice extractRevSliceFromDocument(VPackSlice slice);

/// @brief Build a count result from shard-level count data
///
/// This function constructs an OperationResult containing count information from
/// multiple shards or data sources. It handles different count types and formats
/// the result appropriately for cluster or single-server environments.
///
/// @param options Operation options containing configuration for the count operation
/// @param count Vector of shard name/count pairs containing individual shard counts
/// @param type Type of count result to generate (kNormal, kTryCache, or kDetailed)
/// @param total Output parameter that will contain the total count across all shards
/// @return OperationResult containing the formatted count result
///
/// @note CountType::kNormal and kTryCache return a single number
/// @note CountType::kDetailed returns an object with per-shard counts
/// @note The total parameter is always populated with the sum of all shard counts
/// @note This function is used internally by count operations in cluster environments
///
/// Count Types:
/// - kNormal: Returns actual and accurate count as a single number
/// - kTryCache: May return cached result, formatted as single number
/// - kDetailed: Returns per-shard detailed results as an object
///
/// Example:
/// @code
/// OperationOptions options;
/// std::vector<std::pair<std::string, uint64_t>> shardCounts = {
///   {"shard1", 100}, {"shard2", 200}, {"shard3", 150}
/// };
/// uint64_t total = 0;
/// 
/// // For detailed count:
/// OperationResult result = buildCountResult(options, shardCounts, CountType::kDetailed, total);
/// // result.slice() will be: {"shard1": 100, "shard2": 200, "shard3": 150}
/// // total will be 450
/// 
/// // For normal count:
/// OperationResult result = buildCountResult(options, shardCounts, CountType::kNormal, total);
/// // result.slice() will be: 450
/// // total will be 450
/// @endcode
OperationResult buildCountResult(
    OperationOptions const& options,
    std::vector<std::pair<std::string, uint64_t>> const& count, CountType type,
    uint64_t& total);

/// @brief Create a document ID string from a custom _id value and _key
///
/// This function creates a document ID string from a custom _id VPackSlice and a _key VPackSlice.
/// Custom _id values are used internally by ArangoDB for efficiency and contain encoded collection
/// information. This function extracts the collection ID from the custom _id and combines it with
/// the key to create a full document ID.
///
/// @param resolver Collection name resolver for converting collection IDs to names
/// @param idPart The VPackSlice containing the custom _id value (must be Custom type with head 0xf3)
/// @param keyPart The VPackSlice containing the document key (must be String type)
/// @return A string containing the full document ID in "collection/key" format
///
/// @note idPart must be a Custom type slice with head value 0xf3
/// @note keyPart must be a String type slice
/// @note The resolver parameter must not be null
/// @note Custom _id values contain encoded collection information for efficiency
/// @note This function is used internally when processing documents with custom _id formats
///
/// @warning This function makes assumptions about the custom _id format and should only be used
///          with verified custom _id values from ArangoDB documents
///
/// Example:
/// @code
/// CollectionNameResolver resolver = ...;
/// velocypack::Slice customId = ...;  // Custom _id from ArangoDB document
/// velocypack::Slice key = ...;       // String key slice
/// 
/// std::string docId = makeIdFromCustom(&resolver, customId, key);
/// // docId will be "collection_name/key_value"
/// @endcode
std::string makeIdFromCustom(CollectionNameResolver const* resolver,
                             VPackSlice idPart, VPackSlice keyPart);

/// @brief Create a document ID string from collection ID and key
///
/// This function creates a document ID string from a DataSourceId (collection ID) and a key.
/// It uses the CollectionNameResolver to convert the collection ID to a collection name,
/// then combines it with the key to create a full document ID.
///
/// @param resolver Collection name resolver for converting collection IDs to names
/// @param cid The DataSourceId identifying the collection
/// @param keyPart The VPackSlice containing the document key (must be String type)
/// @return A string containing the full document ID in "collection/key" format
///
/// @note keyPart must be a String type slice
/// @note The resolver parameter must not be null
/// @note The cid parameter must be a valid DataSourceId
/// @note This function handles both single-server and cluster environments
///
/// Example:
/// @code
/// CollectionNameResolver resolver = ...;
/// DataSourceId collectionId = ...;   // Valid collection ID
/// velocypack::Slice key = ...;       // String key slice
/// 
/// std::string docId = makeIdFromParts(&resolver, collectionId, key);
/// // docId will be "collection_name/key_value"
/// @endcode
std::string makeIdFromParts(CollectionNameResolver const* resolver,
                            DataSourceId const& cid, VPackSlice keyPart);

/// @brief Create a new document object for insert operation
///
/// This function creates a new document object for insertion into a collection.
/// It adds all required system attributes (_key, _id, _from, _to, _rev) and
/// processes the user-provided document value according to the operation options.
///
/// @param trx The transaction methods object providing transaction context
/// @param collection The logical collection where the document will be inserted
/// @param key The document key (must be valid and unique within the collection)
/// @param value The user-provided document value to insert
/// @param revisionId Output parameter that will contain the generated revision ID
/// @param builder VelocyPack builder where the final document will be constructed
/// @param options Operation options controlling insert behavior (waitForSync, validation, etc.)
/// @param batchOptions Batch processing options (schema validation, computed values, etc.)
/// @return Result indicating success or failure of the operation
///
/// @note The value parameter must have _key set correctly
/// @note For edge collections, _from and _to attributes are required in the value
/// @note The builder will contain the complete document with all system attributes
/// @note The revisionId will be set to the generated revision for the new document
/// @note Schema validation and computed values are applied if configured
///
/// @warning This function assumes the key is valid and unique - validation should be done beforehand
///
/// Example:
/// @code
/// transaction::Methods trx = ...;
/// LogicalCollection collection = ...;
/// std::string_view key = "user123";
/// velocypack::Slice userDoc = ...;  // {"name": "John", "age": 30}
/// RevisionId revisionId;
/// velocypack::Builder builder;
/// OperationOptions options;
/// BatchOptions batchOptions;
/// 
/// Result result = newObjectForInsert(trx, collection, key, userDoc, 
///                                   revisionId, builder, options, batchOptions);
/// if (result.ok()) {
///   // builder now contains: {"_key": "user123", "_id": "users/user123", 
///   //                       "_rev": "...", "name": "John", "age": 30}
/// }
/// @endcode
Result newObjectForInsert(Methods& trx, LogicalCollection& collection,
                          std::string_view key, velocypack::Slice value,
                          RevisionId& revisionId, velocypack::Builder& builder,
                          OperationOptions const& options,
                          BatchOptions& batchOptions);

/// @brief Merge old and new document objects for update operation
///
/// This function merges an existing document with update data to create the final
/// updated document. It handles partial updates, null value processing, object
/// merging, and maintains system attributes from the original document.
///
/// @param trx The transaction methods object providing transaction context
/// @param collection The logical collection containing the document
/// @param oldValue The existing document to be updated
/// @param newValue The update data to merge with the existing document
/// @param isNoOpUpdate Whether this is a no-operation update (version conflicts, etc.)
/// @param previousRevisionId The revision ID of the existing document
/// @param revisionId Output parameter that will contain the new revision ID
/// @param builder VelocyPack builder where the final updated document will be constructed
/// @param options Operation options controlling update behavior (keepNull, mergeObjects, etc.)
/// @param batchOptions Batch processing options (schema validation, computed values, etc.)
/// @return Result indicating success or failure of the operation
///
/// @note The oldValue must be a complete existing document with system attributes
/// @note The newValue contains the fields to update (partial document)
/// @note System attributes (_key, _id, _from, _to) are preserved from oldValue
/// @note Null handling is controlled by options.keepNull
/// @note Object merging is controlled by options.mergeObjects
/// @note Schema validation and computed values are applied if configured
///
/// Example:
/// @code
/// velocypack::Slice oldDoc = ...;    // {"_key": "user123", "_id": "users/user123", 
///                                    //  "_rev": "1", "name": "John", "age": 30}
/// velocypack::Slice updateData = ...; // {"age": 31, "city": "New York"}
/// RevisionId oldRevId = RevisionId::fromString("1");
/// RevisionId newRevId;
/// velocypack::Builder builder;
/// 
/// Result result = mergeObjectsForUpdate(trx, collection, oldDoc, updateData, 
///                                      false, oldRevId, newRevId, builder, 
///                                      options, batchOptions);
/// if (result.ok()) {
///   // builder contains: {"_key": "user123", "_id": "users/user123", 
///   //                   "_rev": "2", "name": "John", "age": 31, "city": "New York"}
/// }
/// @endcode
Result mergeObjectsForUpdate(Methods& trx, LogicalCollection& collection,
                             velocypack::Slice oldValue,
                             velocypack::Slice newValue, bool isNoOpUpdate,
                             RevisionId previousRevisionId,
                             RevisionId& revisionId,
                             velocypack::Builder& builder,
                             OperationOptions const& options,
                             BatchOptions& batchOptions);

/// @brief Create a new document object for replace operation
///
/// This function creates a new document object for replacing an existing document.
/// It preserves system attributes from the old document and replaces all user
/// attributes with the new values.
///
/// @param trx The transaction methods object providing transaction context
/// @param collection The logical collection containing the document
/// @param oldValue The existing document to be replaced
/// @param newValue The new document data to replace the existing document
/// @param isNoOpReplace Whether this is a no-operation replace (version conflicts, etc.)
/// @param previousRevisionId The revision ID of the existing document
/// @param revisionId Output parameter that will contain the new revision ID
/// @param builder VelocyPack builder where the final replaced document will be constructed
/// @param options Operation options controlling replace behavior (validation, etc.)
/// @param batchOptions Batch processing options (schema validation, computed values, etc.)
/// @return Result indicating success or failure of the operation
///
/// @note The oldValue must be a complete existing document with system attributes
/// @note The newValue contains the complete replacement document (without system attributes)
/// @note System attributes (_key, _id, _from, _to) are preserved from oldValue
/// @note For edge collections, _from and _to must be provided in newValue
/// @note All user attributes are completely replaced (unlike update operations)
/// @note Schema validation and computed values are applied if configured
///
/// Example:
/// @code
/// velocypack::Slice oldDoc = ...;    // {"_key": "user123", "_id": "users/user123", 
///                                    //  "_rev": "1", "name": "John", "age": 30}
/// velocypack::Slice newDoc = ...;    // {"name": "Jane", "email": "jane@example.com"}
/// RevisionId oldRevId = RevisionId::fromString("1");
/// RevisionId newRevId;
/// velocypack::Builder builder;
/// 
/// Result result = newObjectForReplace(trx, collection, oldDoc, newDoc, 
///                                    false, oldRevId, newRevId, builder, 
///                                    options, batchOptions);
/// if (result.ok()) {
///   // builder contains: {"_key": "user123", "_id": "users/user123", 
///   //                   "_rev": "2", "name": "Jane", "email": "jane@example.com"}
///   // Note: "age" field is removed as it's not in newDoc
/// }
/// @endcode
Result newObjectForReplace(Methods& trx, LogicalCollection& collection,
                           velocypack::Slice oldValue,
                           velocypack::Slice newValue, bool isNoOpReplace,
                           RevisionId previousRevisionId,
                           RevisionId& revisionId, velocypack::Builder& builder,
                           OperationOptions const& options,
                           BatchOptions& batchOptions);

/// @brief Validate an edge attribute (_from or _to) value
///
/// This function validates that a VelocyPack slice contains a valid edge attribute
/// value. Edge attributes must be strings containing valid document IDs in the
/// format "collection/key". The function performs comprehensive validation of
/// the document ID format including collection name and key validation.
///
/// @param slice The VelocyPack slice to validate (should contain the _from or _to value)
/// @param allowExtendedNames Whether to allow extended collection names (unicode, etc.)
/// @return true if the slice contains a valid edge attribute, false otherwise
///
/// @note The slice must be a String type to be valid
/// @note The string must contain a valid document ID in "collection/key" format
/// @note Collection names and keys are validated according to ArangoDB naming rules
/// @note Extended names support unicode characters and relaxed naming constraints
/// @note This function is used internally during edge document operations
///
/// @warning This function assumes the slice represents an edge attribute value
///          and should only be used in edge document validation contexts
///
/// Validation Rules:
/// - Must be a string type
/// - Must contain a valid document ID (collection/key format)
/// - Collection name must be valid according to ArangoDB naming rules
/// - Key must be valid according to ArangoDB key naming rules
/// - Extended names allow unicode and relaxed constraints when enabled
///
/// Example:
/// @code
/// velocypack::Slice fromAttribute = ...;  // Contains "_from" value
/// bool allowExtended = true;
/// 
/// if (isValidEdgeAttribute(fromAttribute, allowExtended)) {
///   // Valid edge attribute, can be used in edge document
///   std::string_view docId = fromAttribute.stringView();
///   // docId might be "vertices/vertex123" or "उपयोगकर्ता/user123" (if extended names enabled)
/// } else {
///   // Invalid edge attribute - not a string or invalid document ID format
/// }
/// @endcode
bool isValidEdgeAttribute(velocypack::Slice slice, bool allowExtendedNames);

}  // namespace helpers

/// @brief RAII std::string resource manager for transaction contexts
///
/// This class provides automatic management of std::string objects from a transaction
/// context's resource pool. It implements RAII (Resource Acquisition Is Initialization)
/// pattern to ensure proper cleanup of string resources.
///
/// The StringLeaser automatically acquires a string from the transaction context's
/// pool upon construction and returns it upon destruction. This avoids repeated
/// memory allocations and improves performance during transaction operations.
///
/// @note The leased string is automatically returned to the pool when the StringLeaser
///       is destroyed, ensuring no memory leaks
/// @note Multiple StringLeaser objects can be used simultaneously
/// @note The transaction context maintains a pool of reusable string objects
/// @note The leased string is cleared before being handed out for reuse
///
/// Example:
/// @code
/// transaction::Methods trx = ...;
/// 
/// {
///   StringLeaser leaser(&trx);
///   std::string* str = leaser.get();
///   str->append("collection_name/");
///   str->append(keyValue);
///   
///   // Use the string...
///   std::string docId = *str;
/// } // String is automatically returned to pool here
/// @endcode
class StringLeaser {
 public:
  /// @brief Construct StringLeaser from transaction Methods
  /// @param trx Transaction methods providing access to transaction context
  explicit StringLeaser(Methods* trx);
  
  /// @brief Construct StringLeaser from transaction Context
  /// @param transactionContext Transaction context that manages the string pool
  explicit StringLeaser(Context* transactionContext);
  
  /// @brief Destructor - automatically returns string to pool
  ~StringLeaser();

  /// @brief Release ownership of the string and return it as unique_ptr
  /// @return unique_ptr to the string (caller takes ownership)
  /// @note After calling this, the StringLeaser no longer manages the string
  auto release() {
    return std::unique_ptr<std::string>{std::exchange(_string, nullptr)};
  }
  
  /// @brief Acquire a string from unique_ptr and take ownership
  /// @param r unique_ptr to string to acquire
  /// @note The StringLeaser must not currently hold a string
  void acquire(std::unique_ptr<std::string> r) {
    TRI_ASSERT(_string == nullptr);
    _string = r.release();
  }

  /// @brief Get raw pointer to the managed string
  /// @return Pointer to the managed string
  std::string* string() const { return _string; }
  
  /// @brief Arrow operator for accessing string methods
  /// @return Pointer to the managed string
  std::string* operator->() const { return _string; }
  
  /// @brief Dereference operator for accessing string
  /// @return Reference to the managed string
  std::string& operator*() { return *_string; }
  
  /// @brief Const dereference operator for accessing string
  /// @return Const reference to the managed string
  std::string const& operator*() const { return *_string; }
  
  /// @brief Get raw pointer to the managed string
  /// @return Pointer to the managed string
  std::string* get() const { return _string; }

 private:
  Context* _transactionContext;
  std::string* _string;
};

/// @brief RAII VelocyPack Builder resource manager for transaction contexts
///
/// This class provides automatic management of VelocyPack Builder objects from a
/// transaction context's resource pool. It implements RAII (Resource Acquisition Is
/// Initialization) pattern to ensure proper cleanup of builder resources.
///
/// The BuilderLeaser automatically acquires a VelocyPack Builder from the transaction
/// context's pool upon construction and returns it upon destruction. This avoids repeated
/// memory allocations and improves performance during transaction operations.
///
/// @note The leased builder is automatically returned to the pool when the BuilderLeaser
///       is destroyed, ensuring no memory leaks
/// @note Multiple BuilderLeaser objects can be used simultaneously
/// @note The transaction context maintains a pool of reusable builder objects
/// @note The leased builder is cleared before being handed out for reuse
/// @note This class is move-only (no copy constructor/assignment)
///
/// Example:
/// @code
/// transaction::Methods trx = ...;
/// 
/// {
///   BuilderLeaser leaser(&trx);
///   leaser->openObject();
///   leaser->add("_key", VPackValue("user123"));
///   leaser->add("name", VPackValue("John Doe"));
///   leaser->close();
///   
///   VPackSlice document = leaser->slice();
///   // Use the document...
/// } // Builder is automatically returned to pool here
/// @endcode
class BuilderLeaser {
 public:
  /// @brief Construct BuilderLeaser from transaction Context
  /// @param transactionContext Transaction context that manages the builder pool
  explicit BuilderLeaser(Context* transactionContext);
  
  /// @brief Construct BuilderLeaser from transaction Methods
  /// @param trx Transaction methods providing access to transaction context
  explicit BuilderLeaser(Methods* trx);

  /// @brief Copy constructor - deleted (move-only class)
  BuilderLeaser(BuilderLeaser const&) = delete;
  
  /// @brief Copy assignment - deleted (move-only class)
  BuilderLeaser& operator=(BuilderLeaser const&) = delete;
  
  /// @brief Move assignment - deleted (move-only class)
  BuilderLeaser& operator=(BuilderLeaser&&) = delete;

  /// @brief Move constructor - transfers ownership of builder
  /// @param source Source BuilderLeaser to move from
  BuilderLeaser(BuilderLeaser&& source)
      : _transactionContext(source._transactionContext),
        _builder(source.steal()) {}

  /// @brief Destructor - automatically returns builder to pool
  ~BuilderLeaser();

  /// @brief Get raw pointer to the managed builder
  /// @return Pointer to the managed VelocyPack Builder
  velocypack::Builder* builder() const noexcept { return _builder; }
  
  /// @brief Arrow operator for accessing builder methods
  /// @return Pointer to the managed VelocyPack Builder
  velocypack::Builder* operator->() const noexcept { return _builder; }
  
  /// @brief Dereference operator for accessing builder
  /// @return Reference to the managed VelocyPack Builder
  velocypack::Builder& operator*() noexcept { return *_builder; }
  
  /// @brief Const dereference operator for accessing builder
  /// @return Const reference to the managed VelocyPack Builder
  velocypack::Builder& operator*() const noexcept { return *_builder; }
  
  /// @brief Get raw pointer to the managed builder
  /// @return Pointer to the managed VelocyPack Builder
  velocypack::Builder* get() const noexcept { return _builder; }
  
  /// @brief Steal ownership of the builder and return raw pointer
  /// @return Pointer to the builder (caller takes ownership)
  /// @note After calling this, the BuilderLeaser no longer manages the builder
  velocypack::Builder* steal() { return std::exchange(_builder, nullptr); }

  /// @brief Manually return builder to pool and clear this leaser
  /// @note This can be called multiple times safely
  /// @note After calling this, the BuilderLeaser no longer manages a builder
  void clear();

 private:
  Context* _transactionContext;
  velocypack::Builder* _builder;
};

}  // namespace transaction
}  // namespace arangodb
