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
#include "Basics/StaticStrings.h"
#include "IResearch/SystemDatabaseFeature.h"
#include "Logger/Logger.h"
#include "Logger/LogMacros.h"
#include "StorageEngine/DocumentIdentifierToken.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/OperationOptions.h"
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
  
  arangodb::velocypack::Builder builder;

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

  return arangodb::aql::AqlValue(builder);
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
    ".,.", // argument variable names
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
   _refCount(0) { // no references yet
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
  startsAfter("SystemDatabase"); // used for getting the system database containing the persisted configuration
}

std::pair<IResearchAnalyzerFeature::AnalyzerPool::ptr, bool> IResearchAnalyzerFeature::emplace(
  irs::string_ref const& name,
  irs::string_ref const& type,
  irs::string_ref const& properties
) noexcept {
  return emplace(name, type, properties, true);
}

std::pair<IResearchAnalyzerFeature::AnalyzerPool::ptr, bool> IResearchAnalyzerFeature::emplace(
  irs::string_ref const& name,
  irs::string_ref const& type,
  irs::string_ref const& properties,
  bool initAndPersist
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

    if (!pool) {
      LOG_TOPIC(WARN, Logger::FIXME) << "failure creating an IResearch analyzer instance for name '" << name << "' type '" << type << "' properties '" << properties << "'";
      TRI_set_errno(TRI_ERROR_BAD_PARAMETER);

      return std::make_pair(AnalyzerPool::ptr(), false);
    }

    // skip initialization and persistance
    if (!initAndPersist && pool) {
      erase = false;

      return std::make_pair(pool, itr.second);
    }

    if (itr.second) { // new pool
      if (!pool->init(type, properties)) {
        LOG_TOPIC(WARN, Logger::FIXME) << "failure initializing an IResearch analyzer instance for name '" << name << "' type '" << type << "' properties '" << properties << "'";
        TRI_set_errno(TRI_ERROR_BAD_PARAMETER);

        return std::make_pair(AnalyzerPool::ptr(), false);
      }

      if (!storeConfiguration(*pool)) {
        LOG_TOPIC(WARN, Logger::FIXME) << "failure persisting an IResearch analyzer instance for name '" << name << "' type '" << type << "' properties '" << properties << "'";
        TRI_set_errno(TRI_ERROR_BAD_PARAMETER);

        return std::make_pair(AnalyzerPool::ptr(), false);
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
    WriteMutex mutex(_mutex);
    SCOPED_LOCK(mutex);

    auto itr = _analyzers.find(irs::make_hashed_ref(name, std::hash<irs::string_ref>()));

    if (itr == _analyzers.end()) {
      return 0; // nothing to erase
    }

    auto pool = itr->second;

    if (!pool) {
      LOG_TOPIC(WARN, Logger::FIXME) << "removal of an unset IResearch analizer name '" << name << "'";
      _analyzers.erase(itr);

      return 0; // no actual valid analyzer was removed (this is definitly a bug somewhere)
    }

    if (!force && pool->_refCount) {
      LOG_TOPIC(WARN, Logger::FIXME) << "outstanding reservation requests preventing removal of IResearch analizer name '" << name << "'";

      return 0;
    }

    if (_started) {
      auto* database = getFeature<arangodb::iresearch::SystemDatabaseFeature>();

      if (!database) {
        LOG_TOPIC(WARN, Logger::FIXME) << "failure to find feature 'SystemDatabase' while removing IResearch analyzer name '" << pool->name() << "'";

        return false;
      }

      auto vocbase = database->use();

      if (!vocbase) {
        LOG_TOPIC(WARN, Logger::FIXME) << "failure to get system database while removing IResearch analyzer name '" << pool->name() << "'";

        return false;
      }

      arangodb::SingleCollectionTransaction trx(
        arangodb::transaction::StandaloneContext::Create(vocbase.get()),
        ANALYZER_COLLECTION_NAME,
        arangodb::AccessMode::Type::WRITE
      );
      auto res = trx.begin();

      if (!res.ok()) {
        LOG_TOPIC(WARN, Logger::FIXME) << "failure to start transaction while removing configuration for IResearch analyzer name '" << pool->name() << "'";

        return false;
      }

      arangodb::velocypack::Builder builder;
      arangodb::OperationOptions options;

      builder.openObject();
      builder.add(arangodb::StaticStrings::KeyString, arangodb::velocypack::Value(pool->_key));
      builder.close();
      options.waitForSync = true;

      auto result = trx.remove(ANALYZER_COLLECTION_NAME, builder.slice(), options);

      // stataic analyzers may be not persisted if their '_refCount' did not change
      if (!result.successful() && TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND != result.code) {
        LOG_TOPIC(WARN, Logger::FIXME) << "failure to persist AnalyzerPool configuration while removing IResearch analyzer name '" << pool->name() << "'";
        trx.abort();

        return false;
      }

      if (!trx.commit().ok()) {
        LOG_TOPIC(WARN, Logger::FIXME) << "failure to commit AnalyzerPool configuration while removing IResearch analyzer name '" << pool->name() << "'";
        trx.abort();

        return false;
      }
    }

    if (force) {
      LOG_TOPIC(WARN, Logger::FIXME) << "outstanding reservation requests while removal of IResearch analizer name '" << name << "'";
    }

    // OK to erase if !_started because on start() the persisted configuration will be loaded
    _analyzers.erase(itr);

    return 1;
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
    // insert dummy (uninitialized) placeholders if this feature has not been
    // started to break the dependency loop on DatabaseFeature
    // placeholders will be loaded/validation during start()/loadConfiguration()
    if (!_started) {
      return const_cast<IResearchAnalyzerFeature*>(this)->emplace(name, irs::string_ref::nil, irs::string_ref::nil, false).first;
    }

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

void IResearchAnalyzerFeature::loadConfiguration(
    std::unordered_set<irs::string_ref> const& preinitialized
) {
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

  std::unordered_map<irs::string_ref, std::pair<AnalyzerPool::ptr, int64_t>> initialized;
  auto visitor = [this, &trx, collection, &initialized, &preinitialized](
      DocumentIdentifierToken const& token
  )->bool {
    ManagedDocumentResult result;

    if (!collection->readDocument(&trx, token, result)) {
      LOG_TOPIC(WARN, Logger::FIXME) << "skipping failed read of an IResearch analyzer persisted configuration token: " << token._data;

      return true; // failed to read document, skip
    }

    arangodb::velocypack::Slice slice(result.vpack());

    if (!slice.isObject()
        || !slice.hasKey(arangodb::StaticStrings::KeyString)
        || !slice.get(arangodb::StaticStrings::KeyString).isString()
        || !slice.hasKey("name") || !slice.get("name").isString()
        || !slice.hasKey("type") || !slice.get("type").isString()
        || !slice.hasKey("properties")
        || !slice.hasKey("ref_count") || !slice.get("ref_count").isNumber<uint64_t>()) {
      LOG_TOPIC(WARN, Logger::FIXME) << "skipping invalid IResearch analyzer persisted configuration entry: " << slice.toJson();

      return true; // not a valid configuration, skip
    }

    auto key = getStringRef(slice.get(StaticStrings::KeyString));
    auto name = getStringRef(slice.get("name"));
    auto type = getStringRef(slice.get("type"));
    auto properties = slice.get("properties").toJson(); // format as jSON config
    auto count = slice.get("ref_count").getNumber<uint64_t>();

    WriteMutex mutex(_mutex);
    SCOPED_LOCK(mutex); // the cache could return the same pool asynchronously before '_key'/'_refCount' updated below
    auto entry = emplace(name, type, properties, false); // do not persit since this config is already coming from the persisted store
    auto& pool = entry.first;

    if (!pool) {
      LOG_TOPIC(WARN, Logger::FIXME) << "failure creating an IResearch analyzer instance for name '" << name << "' type '" << type << "' properties '" << properties << "'";

      THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        std::string("failure creating an IResearch analyzer instance for name '") + std::string(name) + "' type '" + std::string(type) + "' properties '" + properties + "'"
      );
    }

    if (!entry.second && initialized.find(name) != initialized.end()) {
      LOG_TOPIC(WARN, Logger::FIXME) << "name collision detected while registering an IResearch analizer name '" << name << "' type '" << type << "' properties '" << properties << "', previous registration type '" << pool->_type << "' properties '" << pool->_properties << "'";

      THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        std::string("name collision detected while registering an IResearch analizer name '") + std::string(name) + "' type '" + std::string(type) + "' properties '" + properties + "', previous registration type '" + pool->_type + "' properties '" + pool->_properties + "'"
      );
    }

    // check if able to add an int64_t 'count' to '_refCount'
    if (pool->_refCount >= std::numeric_limits<int64_t>::max()) {
      LOG_TOPIC(WARN, Logger::FIXME) << "overflow detected while registering an IResearch analyzer name '" << name << "' type '" << type << "' properties '" << properties << "', previous registration type '" << pool->_type << "' properties '" << pool->_properties << "'";

      THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL,
        std::string("overflow detected while registering an IResearch analyzer name '") + std::string(name) + "' type '" + std::string(type) + "' properties '" + properties + "', previous registration type '" + pool->_type + "' properties '" + pool->_properties + "'"
      );
    }

    if (!entry.second && preinitialized.find(name) != preinitialized.end()) {
      if (type != pool->_type || properties != pool->_properties) {
        LOG_TOPIC(WARN, Logger::FIXME) << "name collision detected while registering an IResearch analizer name '" << name << "' type '" << type << "' properties '" << properties << "', previous registration type '" << pool->_type << "' properties '" << pool->_properties << "'";

        THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_INTERNAL,
          std::string("name collision detected while registering an IResearch analizer name '") + std::string(name) + "' type '" + std::string(type) + "' properties '" + properties + "', previous registration type '" + pool->_type + "' properties '" + pool->_properties + "'"
        );
      }
    } else if (!pool->init(type, properties)) {
      LOG_TOPIC(WARN, Logger::FIXME) << "failure initializing an IResearch analyzer instance for name '" << name << "' type '" << type << "' properties '" << properties << "'";

      THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        std::string("failure initializing an IResearch analyzer instance for name '") + std::string(name) + "' type '" + std::string(type) + "' properties '" + properties + "'"
      );
    }

    initialized.emplace(
      std::piecewise_construct,
      std::forward_as_tuple(pool->name()), // emplace with ref at sting in pool
      std::forward_as_tuple(pool, int64_t(count))
    );
    pool->_key = key;
    pool->_refCount += count; // for new entries refCount == 0, for dummy placeholder value should be summed and persisted

    return true; // process next
  };

  try {
    collection->invokeOnAllElements(&trx, visitor);

    // ensure all records were initialized
    for (auto& entry: _analyzers) {
      if (initialized.find(entry.first) == initialized.end()
          && preinitialized.find(entry.first) == preinitialized.end()) {
        LOG_TOPIC(WARN, Logger::FIXME) << "uninitialized AnalyzerPool deletected while loading persisted configuration, IResearch analyzer name '" << entry.first << "'";

        THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_INTERNAL,
          std::string("uninitialized AnalyzerPool deletected while loading persisted configuration, IResearch analyzer name '") + std::string(entry.first) + "'"
        );
      }
    }

    // persist refCount changes
    for (auto& entry: initialized) {
      auto& name = entry.first;
      auto count = entry.second.second;
      auto& pool = entry.second.first;

      if (count && (!pool || !updateConfiguration(trx, *pool, count))) {
        LOG_TOPIC(WARN, Logger::FIXME) << "failure to persist AnalyzerPool configuration while updating ref_count of IResearch analyzer name '" << name << "'";

        THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_INTERNAL,
          std::string("failure to persist AnalyzerPool configuration while updating ref_count of IResearch analyzer name '") + std::string(name) + "'"
        );
      }
    }

    if (!trx.commit().ok()) {
      LOG_TOPIC(WARN, Logger::FIXME) << "failure to commit AnalyzerPool configuration while updating ref_count of IResearch analyzer name '" << name << "'";
      trx.abort();

      THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL,
        "failure to commit AnalyzerPool configuration while updating ref_count of IResearch analyzer"
      );
    }
  } catch (...) {
    for (auto& entry: initialized) {
      auto& pool = entry.second.first;

      if (pool) {
        pool->_type.clear(); // noexcept
        pool->_properties.clear(); // noexcept
        pool->_refCount -= entry.second.second; // noexcept
      }
    }

    throw;
  }
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
  if (!pool) {
    return false; // ignore release requests on uninitialized pools
  }

  if (_started) {
    return updateConfiguration(*pool, -1); // -1 for decrement
  }

  if (!pool->_refCount) {
    return false;
  }

  --pool->_refCount;

  return true;
}

bool IResearchAnalyzerFeature::reserve(AnalyzerPool::ptr const& pool) noexcept {
  if (!pool) {
    return false; // ignore reservation requests on uninitialized pools
  }

  if (_started) {
    return updateConfiguration(*pool, 1); // +1 for increment
  }

  ++pool->_refCount;

  return true;
}

void IResearchAnalyzerFeature::start() {
  ApplicationFeature::start();

  // register the indentity analyzer (before loading configuration)
  auto identity = emplace(
    IdentityTokenizer::type().name(), // use name same as type for convenience
    IdentityTokenizer::type().name(),
    irs::string_ref::nil,
    false // do not persist since it's a static analyzer always available after start()
  ).first;
  std::unordered_set<irs::string_ref> initialized;

  // initialize the identity analyzer pool
  if (identity
      && identity->init(IdentityTokenizer::type().name(), irs::string_ref::nil)) {
    initialized.emplace(identity->name());
  }

  // load persisted configuration
  loadConfiguration(initialized);

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

bool IResearchAnalyzerFeature::storeConfiguration(AnalyzerPool& pool) {
  auto* database = getFeature<arangodb::iresearch::SystemDatabaseFeature>();

  if (!database) {
    LOG_TOPIC(WARN, Logger::FIXME) << "failure to find feature 'SystemDatabase' while persisting configuration IResearch analyzer name '" << pool.name() << "'";

    return false;
  }

  auto vocbase = database->use();

  if (!vocbase) {
    LOG_TOPIC(WARN, Logger::FIXME) << "failure to get system database while persisting configuration IResearch analyzer name '" << pool.name() << "'";

    return false;
  }

  try {
    arangodb::SingleCollectionTransaction trx(
      arangodb::transaction::StandaloneContext::Create(vocbase.get()),
      ANALYZER_COLLECTION_NAME,
      arangodb::AccessMode::Type::WRITE
    );
    auto res = trx.begin();

    if (!res.ok()) {
      LOG_TOPIC(WARN, Logger::FIXME) << "failure to start transaction while persisting configuration for IResearch analyzer name '" << pool.name() << "'";

      return false;
    }

    arangodb::velocypack::Builder builder;
    arangodb::OperationOptions options;

    builder.openObject();
    builder.add("name", arangodb::velocypack::Value(pool.name()));
    builder.add("type", arangodb::velocypack::Value(pool._type));
    builder.add("properties", arangodb::velocypack::Value(pool._properties));
    builder.add("ref_count", arangodb::velocypack::Value(pool._refCount));
    builder.close();
    options.waitForSync = true;

    auto result = trx.insert(ANALYZER_COLLECTION_NAME, builder.slice(), options);

    if (!result.successful()) {
      LOG_TOPIC(WARN, Logger::FIXME) << "failure to persist AnalyzerPool configuration while persisting configuration for IResearch analyzer name '" << pool.name() << "'";
      trx.abort();

      return false;
    }

    auto key = result.slice().get(arangodb::StaticStrings::KeyString);

    if (!key.isString()) {
      LOG_TOPIC(WARN, Logger::FIXME) << "failure to find the resulting key field while persisting configuration for IResearch analyzer name '" << pool.name() << "'";
      trx.abort();

      return false;
    }

    if (!trx.commit().ok()) {
      LOG_TOPIC(WARN, Logger::FIXME) << "failure to commit AnalyzerPool configuration while persisting configuration for IResearch analyzer name '" << pool.name() << "'";
      trx.abort();

      return false;
    }

    pool._key = getStringRef(key);

    return true;
  } catch (std::exception& e) {
    LOG_TOPIC(WARN, Logger::FIXME) << "caught exception during persist of an AnalyzerPool configuration while persisting configuration for IResearch analyzer name '" << pool.name() << "': " << e.what();
    IR_EXCEPTION();
  } catch (...) {
    LOG_TOPIC(WARN, Logger::FIXME) << "caught exception during persist of an AnalyzerPool configuration while persisting configuration for IResearch analyzer name '" << pool.name() << "'";
    IR_EXCEPTION();
  }

  return false;
}

bool IResearchAnalyzerFeature::updateConfiguration(
    AnalyzerPool& pool,
    int64_t delta
) {
  auto* database = getFeature<arangodb::iresearch::SystemDatabaseFeature>();

  if (!database) {
    LOG_TOPIC(WARN, Logger::FIXME) << "failure to find feature 'SystemDatabase' while updating ref_count of IResearch analyzer name '" << pool.name() << "'";

    return false;
  }

  auto vocbase = database->use();

  if (!vocbase) {
    LOG_TOPIC(WARN, Logger::FIXME) << "failure to get system database while updating ref_count of IResearch analyzer name '" << pool.name() << "'";

    return false;
  }

  try {
    arangodb::SingleCollectionTransaction trx(
      arangodb::transaction::StandaloneContext::Create(vocbase.get()),
      ANALYZER_COLLECTION_NAME,
      arangodb::AccessMode::Type::WRITE
    );
    auto res = trx.begin();

    if (!res.ok()) {
      LOG_TOPIC(WARN, Logger::FIXME) << "failure to start transaction while updating ref_count of IResearch analyzer name '" << pool.name() << "'";

      return false;
    }

    WriteMutex mutex(_mutex);
    SCOPED_LOCK(mutex);

    if (!updateConfiguration(trx, pool, delta)) {
      LOG_TOPIC(WARN, Logger::FIXME) << "failure to update AnalyzerPool configuration while updating ref_count of IResearch analyzer name '" << pool.name() << "'";
      trx.abort();

      return false;
    }

    if (!trx.commit().ok()) {
      LOG_TOPIC(WARN, Logger::FIXME) << "failure to commit AnalyzerPool configuration while updating ref_count of IResearch analyzer name '" << pool.name() << "'";
      trx.abort();

      return false;
    }

    pool._refCount += delta;
  } catch (std::exception& e) {
    LOG_TOPIC(WARN, Logger::FIXME) << "caught exception during persist of an AnalyzerPool configuration while updating ref_count of IResearch analyzer name '" << pool.name() << "': " << e.what();
    IR_EXCEPTION();
  } catch (...) {
    LOG_TOPIC(WARN, Logger::FIXME) << "caught exception during persist of an AnalyzerPool configuration while updating ref_count of IResearch analyzer name '" << pool.name() << "'";
    IR_EXCEPTION();
  }

  return false;
}

bool IResearchAnalyzerFeature::updateConfiguration(
    arangodb::transaction::Methods& trx,
    AnalyzerPool& pool,
    int64_t delta
) {
  if ((delta < 0 && decltype(pool._refCount)(0 - delta) > pool._refCount)
      || (delta >0 && std::numeric_limits<decltype(pool._refCount)>::max() - pool._refCount < decltype(pool._refCount)(delta))) {
    LOG_TOPIC(WARN, Logger::FIXME) << "overflow detected while updating ref_count of IResearch analyzer name '" << pool.name() << "'";

    return false;
  }

  arangodb::velocypack::Builder builder;
  arangodb::OperationOptions options;

  builder.openObject();
  builder.add(arangodb::StaticStrings::KeyString, arangodb::velocypack::Value(pool._key));
  builder.add("ref_count", arangodb::velocypack::Value(pool._refCount + delta));
  builder.close();
  options.waitForSync = true;
  options.mergeObjects = true;

  return trx.update(ANALYZER_COLLECTION_NAME, builder.slice(), options).successful();
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
