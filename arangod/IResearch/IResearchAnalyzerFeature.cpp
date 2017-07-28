////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "analysis/analyzers.hpp"
#include "analysis/token_attributes.hpp"
#include "utils/hash_utils.hpp"
#include "utils/object_pool.hpp"

#include "ApplicationServerHelper.h"
#include "VelocyPackHelper.h"

#include "Aql/AqlFunctionFeature.h"
#include "IResearch/SystemDatabaseFeature.h"
#include "Logger/Logger.h"
#include "Logger/LogMacros.h"
#include "StorageEngine/DocumentIdentifierToken.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ManagedDocumentResult.h"
#include "VocBase/vocbase.h"

#include "IResearchAnalyzerFeature.h"

NS_LOCAL

static std::string const ANALYZER_COLLECTION_NAME("_iresearch_analyzers");
static size_t const DEFAULT_POOL_SIZE = 8; // arbitrary value
static std::string const FEATURE_NAME("IResearchAnalyzer");
static irs::string_ref const IDENTITY_TOKENIZER_NAME("identity");

class IdentityValue: public irs::term_attribute {
public:
  DECLARE_FACTORY_DEFAULT();

  virtual ~IdentityValue() {}

  virtual void clear() override {
    _value = irs::bytes_ref::nil;
  }

  virtual const irs::bytes_ref& value() const {
    return _value;
  }

  void value(irs::bytes_ref const& data) {
    _value = data;
  }

 private:
  iresearch::bytes_ref _value;
};

DEFINE_FACTORY_DEFAULT(IdentityValue);

class IdentityTokenizer: public irs::analysis::analyzer {
 public:
  DECLARE_ANALYZER_TYPE();
  DECLARE_FACTORY_DEFAULT(irs::string_ref const& args); // args ignored

  IdentityTokenizer();
  virtual irs::attribute_store const& attributes() const NOEXCEPT override;
  virtual bool next() override;
  virtual bool reset(irs::string_ref const& data) override;

 private:
  irs::attribute_store _attrs;
  bool _empty;
  irs::string_ref _value;
};

DEFINE_ANALYZER_TYPE_NAMED(IdentityTokenizer, IDENTITY_TOKENIZER_NAME);
REGISTER_ANALYZER(IdentityTokenizer);

/*static*/ irs::analysis::analyzer::ptr IdentityTokenizer::make(irs::string_ref const& args) {
  PTR_NAMED(IdentityTokenizer, ptr);
  return ptr;
}

IdentityTokenizer::IdentityTokenizer()
  : irs::analysis::analyzer(IdentityTokenizer::type()), _empty(true) {
  _attrs.emplace<IdentityValue>();
  _attrs.emplace<irs::increment>();
}

irs::attribute_store const& IdentityTokenizer::attributes() const NOEXCEPT {
  return _attrs;
}

bool IdentityTokenizer::next() {
  auto empty = _empty;

  const_cast<const irs::attribute_store&>(_attrs).get<IdentityValue>()->value(irs::ref_cast<irs::byte_type>(_value));
  _empty = true;
  _value = irs::string_ref::nil;

  return !empty;
}

bool IdentityTokenizer::reset(irs::string_ref const& data) {
  _empty = false;
  _value = data;

  return !_empty;
}

arangodb::aql::AqlValue aqlFnTokens(
    arangodb::aql::Query* query,
    arangodb::transaction::Methods* trx,
    arangodb::aql::VPackFunctionParameters const& args
) {
  if (2 != args.size() || !args[0].isString() || !args[1].isString()) {
    LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "invalid arguments passed while computing result for function 'TOKENS'";
    TRI_set_errno(TRI_ERROR_BAD_PARAMETER);

    return arangodb::aql::AqlValue();
  }

  auto data = arangodb::iresearch::getStringRef(args[0].slice());
  auto name = arangodb::iresearch::getStringRef(args[1].slice());
  auto analyzers = arangodb::iresearch::getFeature<arangodb::iresearch::IResearchAnalyzerFeature>("IResearchAnalyzer");

  if (!analyzers) {
    LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "failure to find feature 'IResearch' while computing result for function 'TOKENS'";
    TRI_set_errno(TRI_ERROR_INTERNAL);

    return arangodb::aql::AqlValue();
  }

  auto pool = analyzers->get(name);

  if (!pool) {
    LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "failure to find IResearch analyzer pool name '" << name << "' while computing result for function 'TOKENS'";
    TRI_set_errno(TRI_ERROR_BAD_PARAMETER);

    return arangodb::aql::AqlValue();
  }

  auto analyzer = pool->get();

  if (!analyzer) {
    LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "failure to find IResearch analyzer name '" << name << "' while computing result for function 'TOKENS'";
    TRI_set_errno(TRI_ERROR_BAD_PARAMETER);

    return arangodb::aql::AqlValue();
  }

  if (!analyzer->reset(data)) {
    LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "failure to reset IResearch analyzer name '" << name << "' while computing result for function 'TOKENS'";
    TRI_set_errno(TRI_ERROR_INTERNAL);

    return arangodb::aql::AqlValue();
  }

  auto& values = analyzer->attributes().get<irs::term_attribute>();

  if (!values) {
    LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "failure to retrieve values from IResearch analyzer name '" << name << "' while computing result for function 'TOKENS'";
    TRI_set_errno(TRI_ERROR_INTERNAL);

    return arangodb::aql::AqlValue();
  }

  auto buffer = irs::memory::make_unique<arangodb::velocypack::Buffer<uint8_t>>();

  if (!buffer) {
    LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "failure to allocate result buffer while computing result for function 'TOKENS'";

    return arangodb::aql::AqlValue();
  }

  arangodb::velocypack::Builder builder(*buffer);

  builder.openArray();

  while (analyzer->next()) {
    auto value = values->value();

    builder.add(arangodb::velocypack::ValuePair(
      value.c_str(),
      value.size(),
      arangodb::velocypack::ValueType::String
    ));
  }

  builder.close();

  return arangodb::aql::AqlValue(buffer.release());
}

void addFunction(
    arangodb::aql::AqlFunctionFeature& functions,
    arangodb::aql::Function const& function
) {
  // check that a function by the given name is not registred to avoid
  // triggering an assert inside AqlFunctionFeature::add(...)
  try {
    if (functions.byName(function.externalName)) {
      return; // already have a function with this name
    }
  } catch (arangodb::basics::Exception& e) {
    if (TRI_ERROR_QUERY_FUNCTION_NAME_UNKNOWN != e.code()) {
      throw; // not a duplicate instance exception
    }
  }

  functions.add(function);
}

void addFunctions(arangodb::aql::AqlFunctionFeature& functions) {
  arangodb::aql::Function tokens(
    "TOKENS", // external name (AQL function external names are always in upper case)
    "tokens", // internal name
    "data analyzer", // argument variable names
    false, // cacheable
    false, // deterministic
    true, // can throw
    true, // can be run on server
    true, // can pass arguments by reference
    aqlFnTokens // function implementation
  );

  addFunction(functions, tokens);
}

void ensureConfigCollection(TRI_vocbase_t& vocbase) {
  static const std::string json =
    std::string("{\"isSystem\": true, \"name\": \"") + ANALYZER_COLLECTION_NAME + "\"}";

  try {
    vocbase.createCollection(arangodb::velocypack::Parser::fromJson(json)->slice());
  } catch(arangodb::basics::Exception& e) {
    if (TRI_ERROR_ARANGO_DUPLICATE_NAME != e.code()) {
      throw e;
    }
  }
}

typedef irs::async_utils::read_write_mutex::read_mutex ReadMutex;
typedef irs::async_utils::read_write_mutex::write_mutex WriteMutex;

NS_END

NS_BEGIN(arangodb)
NS_BEGIN(iresearch)

/*static*/ IResearchAnalyzerFeature::AnalyzerPool::Builder::ptr IResearchAnalyzerFeature::AnalyzerPool::Builder::make(
    irs::string_ref const& type,
    irs::string_ref const& properties
) {
  return irs::analysis::analyzers::get(type, properties);
}

IResearchAnalyzerFeature::AnalyzerPool::AnalyzerPool(
    irs::string_ref const& name
): _cache(DEFAULT_POOL_SIZE),
   _name(name),
   _refCount(0), // no references yet
   _rid(0) { // no rid int the persisted configuration yet
}

bool IResearchAnalyzerFeature::AnalyzerPool::init(
    irs::string_ref const& type,
    irs::string_ref const& properties
) noexcept {
  try {
    _cache.clear(); // reset for new type/properties

    auto instance = _cache.emplace(type, properties);

    if (instance) {
      _type = type;
      _properties = properties;
      _features = instance->attributes().features();

      return true;
    }
  } catch (std::exception& e) {
    LOG_TOPIC(WARN, Logger::FIXME) << "caught exception while initializing an IResearch analizer type '" << _type << "' properties '" << _properties << "': " << e.what();
    IR_EXCEPTION();
  } catch (...) {
    LOG_TOPIC(WARN, Logger::FIXME) << "caught exception while initializing an IResearch analizer type '" << _type << "' properties '" << _properties << "'";
    IR_EXCEPTION();
  }

  _type.clear(); // set as uninitialized
  _properties.clear(); // set as uninitialized
  _features.clear(); // set as uninitialized

  return false;
}
irs::flags const& IResearchAnalyzerFeature::AnalyzerPool::features() const noexcept {
  return _features;
}

irs::analysis::analyzer::ptr IResearchAnalyzerFeature::AnalyzerPool::get() const noexcept {
  try {
    return _cache.emplace(_type, _properties);
  } catch (std::exception& e) {
    LOG_TOPIC(WARN, Logger::FIXME) << "caught exception while instantiating an IResearch analizer type '" << _type << "' properties '" << _properties << "': " << e.what();
    IR_EXCEPTION();
  } catch (...) {
    LOG_TOPIC(WARN, Logger::FIXME) << "caught exception while instantiating an IResearch analizer type '" << _type << "' properties '" << _properties << "'";
    IR_EXCEPTION();
  }

  return nullptr;
}

std::string const& IResearchAnalyzerFeature::AnalyzerPool::name() const noexcept {
  return _name;
};

IResearchAnalyzerFeature::IResearchAnalyzerFeature(
    arangodb::application_features::ApplicationServer* server
): ApplicationFeature(server, IResearchAnalyzerFeature::name()), _started(false) {
  setOptional(true);
  requiresElevatedPrivileges(false);
  startsAfter("AQLFunctions"); // used for registering IResearch analyzer functions
/*FIXME TODO enable
  startsAfter("SystemDatabase"); // used for getting the system database containing the persisted configuration
*/
}

std::pair<IResearchAnalyzerFeature::AnalyzerPool::ptr, bool> IResearchAnalyzerFeature::emplace(
  irs::string_ref const& name,
  irs::string_ref const& type,
  irs::string_ref const& properties
) noexcept {
  /* FIXME TODO enable persistence
  return emplace(name, type, properties, true);
  */
  return emplace(name, type, properties, false);
}

std::pair<IResearchAnalyzerFeature::AnalyzerPool::ptr, bool> IResearchAnalyzerFeature::emplace(
  irs::string_ref const& name,
  irs::string_ref const& type,
  irs::string_ref const& properties,
  bool persist
) noexcept {
  try {
    WriteMutex mutex(_mutex);
    SCOPED_LOCK(mutex);

    static auto generator = [](
        irs::hashed_string_ref const& key,
        AnalyzerPool::ptr const& value
    )->irs::hashed_string_ref {
      PTR_NAMED(AnalyzerPool, pool, key);
      const_cast<AnalyzerPool::ptr&>(value) = pool;// lazy-instantiate pool
      return pool ? irs::hashed_string_ref(key.hash(), pool->name()) : key; // reuse hash but point ref at value in pool
    };
    auto itr = irs::map_utils::try_emplace_update_key(
      _analyzers,
      generator,
      irs::make_hashed_ref(name, std::hash<irs::string_ref>())
    );
    bool erase = itr.second;
    auto cleanup = irs::make_finally([&erase, this, &itr]()->void {
      if (erase) {
        _analyzers.erase(itr.first); // ensure no broken analyzers are left behind
      }
    });

    auto pool = itr.first->second;

    if (itr.second) { // new pool
      if (!_started && persist) {
        LOG_TOPIC(WARN, Logger::FIXME) << "cannot garantee collision-free persistance while creating an IResearch analyzer instance for name '" << name << "' type '" << type << "' properties '" << properties << "'";
        TRI_set_errno(TRI_ERROR_ARANGO_ILLEGAL_STATE);

        return std::make_pair(AnalyzerPool::ptr(), false);
      }

      if (!pool || !pool->init(type, properties)) {
        LOG_TOPIC(WARN, Logger::FIXME) << "failure creating an IResearch analyzer instance for name '" << name << "' type '" << type << "' properties '" << properties << "'";
        TRI_set_errno(TRI_ERROR_BAD_PARAMETER);

        return std::make_pair(AnalyzerPool::ptr(), false);
      }

      if (persist) {
        // FIXME TODO store definition in persisted mappings
      }

      erase = false;
    } else if (type != pool->_type || properties != pool->_properties) {
      LOG_TOPIC(WARN, Logger::FIXME) << "name collision detected while registering an IResearch analizer name '" << name << "' type '" << type << "' properties '" << properties << "', previous registration type '" << pool->_type << "' properties '" << pool->_properties << "'";
      TRI_set_errno(TRI_ERROR_BAD_PARAMETER);

      return std::make_pair(AnalyzerPool::ptr(), false);
    }

    return std::make_pair(pool, itr.second);
  } catch (std::exception& e) {
    LOG_TOPIC(WARN, Logger::FIXME) << "caught exception while registering an IResearch analizer name '" << name << "' type '" << type << "' properties '" << properties << "': " << e.what();
    IR_EXCEPTION();
  } catch (...) {
    LOG_TOPIC(WARN, Logger::FIXME) << "caught exception while registering an IResearch analizer name '" << name << "' type '" << type << "' properties '" << properties << "'";
    IR_EXCEPTION();
  }

  return std::make_pair(AnalyzerPool::ptr(), false);
}

size_t IResearchAnalyzerFeature::erase(
    irs::string_ref const& name,
    bool force /*= false*/
) noexcept {
  try {
    // FIXME TODO do not remove name if it is still referenced (unless force)
    // FIXME TODO remove definition from persisted mappings
    WriteMutex mutex(_mutex);
    SCOPED_LOCK(mutex);

    return _analyzers.erase(irs::make_hashed_ref(name, std::hash<irs::string_ref>()));
  } catch (std::exception& e) {
    LOG_TOPIC(WARN, Logger::FIXME) << "caught exception while removing an IResearch analizer name '" << name << "': " << e.what();
    IR_EXCEPTION();
  } catch (...) {
    LOG_TOPIC(WARN, Logger::FIXME) << "caught exception while removing an IResearch analizer name '" << name << "'";
    IR_EXCEPTION();
  }

  return 0;
}

IResearchAnalyzerFeature::AnalyzerPool::ptr IResearchAnalyzerFeature::get(
    irs::string_ref const& name
) const noexcept {
  try {
    ReadMutex mutex(_mutex);
    SCOPED_LOCK(mutex);
    auto itr = _analyzers.find(irs::make_hashed_ref(name, std::hash<irs::string_ref>()));

    if (itr == _analyzers.end()) {
      LOG_TOPIC(WARN, Logger::FIXME) << "failure to find IResearch analyzer name '" << name << "'";

      return nullptr;
    }

    auto pool = itr->second;

    if (pool) {
      return pool;
    }

    LOG_TOPIC(WARN, Logger::FIXME) << "failure to get IResearch analyzer name '" << name << "'";
    TRI_set_errno(TRI_ERROR_INTERNAL);
  } catch (std::exception& e) {
    LOG_TOPIC(WARN, Logger::FIXME) << "caught exception while retrieving an IResearch analizer name '" << name << "': " << e.what();
    IR_EXCEPTION();
  } catch (...) {
    LOG_TOPIC(WARN, Logger::FIXME) << "caught exception while retrieving an IResearch analizer name '" << name << "'";
    IR_EXCEPTION();
  }

  return nullptr;
}

/*static*/ IResearchAnalyzerFeature::AnalyzerPool::ptr IResearchAnalyzerFeature::identity() noexcept {
  struct Identity {
    AnalyzerPool::ptr instance;
    Identity() {
      PTR_NAMED(AnalyzerPool, ptr, IDENTITY_TOKENIZER_NAME);

      // name (use same as 'type' for convenience)
      if (!ptr || !ptr->init(IDENTITY_TOKENIZER_NAME, irs::string_ref::nil)) {
        LOG_TOPIC(FATAL, Logger::FIXME) << "failed to initialize 'identity' analyzer";

        throw irs::illegal_state(); // this should never happen, treat as an assertion failure
      }

      instance = ptr;
    }
  };
  static const Identity identity;

  return identity.instance;
}

void IResearchAnalyzerFeature::loadConfiguration() {
  auto* database = getFeature<arangodb::iresearch::SystemDatabaseFeature>();

  if (!database) {
    LOG_TOPIC(WARN, Logger::FIXME) << "failure to find feature 'SystemDatabase' while loading IResearch analyzer persisted configuration";

    return;
  }

  auto vocbase = database->use();

  if (!vocbase) {
    LOG_TOPIC(WARN, Logger::FIXME) << "failure to get system database while loading IResearch analyzer persisted configuration";

    return;
  }

  // ensure that the configuration collection is present before using it in a transaction
  ensureConfigCollection(*vocbase); // assume the collection is not dropped before transaction uses it

  arangodb::SingleCollectionTransaction trx(
    arangodb::transaction::StandaloneContext::Create(vocbase.get()),
    ANALYZER_COLLECTION_NAME,
    arangodb::AccessMode::Type::WRITE
  );
  auto res = trx.begin();

  if (!res.ok()) {
    LOG_TOPIC(WARN, Logger::FIXME) << "failure to start transaction while loading IResearch analyzer persisted configuration";

    return;
  }

  auto* collection = trx.documentCollection();

  if (!collection) {
    LOG_TOPIC(WARN, Logger::FIXME) << "failure to get collection while loading IResearch analyzer persisted configuration";
    trx.abort();

    return;
  }

  collection->invokeOnAllElements(
    &trx,
    [this, &trx, collection](
        DocumentIdentifierToken const& token
    )->bool {
      ManagedDocumentResult result;

      if (!collection->readDocument(&trx, token, result)) {
        LOG_TOPIC(WARN, Logger::FIXME) << "skipping failed read of an IResearch analyzer persisted configuration token: " << token._data;

        return true; // failed to read document, skip
      }

      auto rid = result.lastRevisionId();
      arangodb::velocypack::Slice slice(result.vpack());

      if (!slice.isObject()
          || !slice.hasKey("name") || !slice.get("name").isString()
          || !slice.hasKey("type") || !slice.get("type").isString()
          || !slice.hasKey("properties")
          || !slice.hasKey("ref_count") || !slice.get("ref_count").isNumber<uint64_t>()) {
        LOG_TOPIC(WARN, Logger::FIXME) << "skipping invalid IResearch analyzer persisted configuration entry: " << slice.toJson();

        return true; // not a valid configuration, skip
      }

      auto name = getStringRef(slice.get("name"));
      auto type = getStringRef(slice.get("type"));
      auto properties = slice.get("properties").toJson();
      // FIXME TODO check how toJson() converts 'null' and plain string values
      auto count = slice.get("ref_count").getNumber<uint64_t>();
      auto entry = emplace(name, type, properties, false); // do not persit since this config is already coming from the persisted store

      if (!entry.first) {
        LOG_TOPIC(WARN, Logger::FIXME) << "skipping failed caching of an IResearch analyzer persisted configuration entry: " << slice.toJson();

        return true; // analizer configuration already in cache, skip duplicate
      }

      WriteMutex mutex(_mutex);
      SCOPED_LOCK(mutex); // the cache could return the same pool asynchronously before '_rid'/'_refCount' updated below

      if (!entry.second) {
        LOG_TOPIC(WARN, Logger::FIXME) << "skipping duplicate IResearch analyzer persisted configuration entry: " << slice.toJson();

        return true; // analizer configuration already in cache, skip duplicate
      }

      entry.first->_rid = rid;
      entry.first->_refCount = count;

      return true; // process next
    }
  );
}

/*static*/ std::string const& IResearchAnalyzerFeature::name() noexcept {
  return FEATURE_NAME;
}

void IResearchAnalyzerFeature::prepare() {
  ApplicationFeature::prepare();

  // load all known analyzers
  ::iresearch::analysis::analyzers::init();
}

bool IResearchAnalyzerFeature::release(AnalyzerPool::ptr const& pool) noexcept {
  // FIXME TODO update persisted mappings
  return true;
}

bool IResearchAnalyzerFeature::reserve(AnalyzerPool::ptr const& pool) noexcept {
  // FIXME TODO update persisted mappings
  return true;
}

void IResearchAnalyzerFeature::start() {
  ApplicationFeature::start();

  // register the indentity analyzer (before loading configuration)
  emplace(
    IdentityTokenizer::type().name(), // use name same as type for convenience
    IdentityTokenizer::type().name(),
    irs::string_ref::nil,
    false // do not persist since it's a static analyzer always available after start()
  );
/* FIXME TODO
  // load persisted configuration
  loadConfiguration();
*/
  auto* functions = getFeature<arangodb::aql::AqlFunctionFeature>("AQLFunctions");

  if (functions) {
    addFunctions(*functions);
  } else {
    LOG_TOPIC(WARN, Logger::FIXME) << "failure to find feature 'AQLFunctions' while registering IResearch functions";
  }

  WriteMutex mutex(_mutex);
  SCOPED_LOCK(mutex); // '_started' can be asynchronously read

  _started = true;
}

void IResearchAnalyzerFeature::stop() {
  {
    WriteMutex mutex(_mutex);
    SCOPED_LOCK(mutex); // '_analyzers'/'_started' can be asynchronously read

    _started = false;
    _analyzers.clear(); // clear cache
  }

  ApplicationFeature::stop();
}

bool IResearchAnalyzerFeature::storeConfiguration(AnalyzerPool const& pool) {
  // FIXME TODO implement
  return false;
}

bool IResearchAnalyzerFeature::visit(
    std::function<bool(irs::string_ref const& name, irs::string_ref const& type, irs::string_ref const& properties)> const& visitor
) {
  ReadMutex mutex(_mutex);
  SCOPED_LOCK(mutex);

  for (auto& entry: _analyzers) {
    if (entry.second && !visitor(entry.first, entry.second->_type, entry.second->_properties)) {
      return false;
    }
  }

  return true;
}

NS_END // iresearch
NS_END // arangodb

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------