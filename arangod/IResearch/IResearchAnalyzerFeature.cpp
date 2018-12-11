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

// otherwise define conflict between 3rdParty\date\include\date\date.h and 3rdParty\iresearch\core\shared.hpp
#if defined(_MSC_VER)
  #include "date/date.h"
  #undef NOEXCEPT
#endif

#include "analysis/analyzers.hpp"
#include "analysis/token_attributes.hpp"
#include "utils/hash_utils.hpp"
#include "utils/object_pool.hpp"

#include "ApplicationServerHelper.h"
#include "IResearchAnalyzerFeature.h"
#include "IResearchCommon.h"
#include "VelocyPackHelper.h"
#include "Aql/AqlFunctionFeature.h"
#include "Aql/ExpressionContext.h"
#include "Basics/StaticStrings.h"
#include "Cluster/ServerState.h"
#include "Logger/LogMacros.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/SystemDatabaseFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/OperationOptions.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/LocalDocumentId.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ManagedDocumentResult.h"
#include "VocBase/vocbase.h"

NS_LOCAL

static std::string const ANALYZER_COLLECTION_NAME("_iresearch_analyzers");
static size_t const DEFAULT_POOL_SIZE = 8; // arbitrary value
static std::string const FEATURE_NAME("IResearchAnalyzer");
static irs::string_ref const IDENTITY_ANALYZER_NAME("identity");

struct IdentityValue : irs::term_attribute {
  void value(irs::bytes_ref const& data) noexcept {
    value_ = data;
  }
};

class IdentityAnalyzer: public irs::analysis::analyzer {
 public:
  DECLARE_ANALYZER_TYPE();
  DECLARE_FACTORY(irs::string_ref const& args); // args ignored

  IdentityAnalyzer();
  virtual irs::attribute_view const& attributes() const NOEXCEPT override;
  virtual bool next() override;
  virtual bool reset(irs::string_ref const& data) override;

 private:
  irs::attribute_view _attrs;
  IdentityValue _term;
  irs::increment _inc;
  irs::string_ref _value;
  bool _empty;
};

DEFINE_ANALYZER_TYPE_NAMED(IdentityAnalyzer, IDENTITY_ANALYZER_NAME);
REGISTER_ANALYZER_JSON(IdentityAnalyzer, IdentityAnalyzer::make);

/*static*/ irs::analysis::analyzer::ptr IdentityAnalyzer::make(
    irs::string_ref const& args
) {
  UNUSED(args);
  PTR_NAMED(IdentityAnalyzer, ptr);
  return ptr;
}

IdentityAnalyzer::IdentityAnalyzer()
  : irs::analysis::analyzer(IdentityAnalyzer::type()), _empty(true) {
  _attrs.emplace(_term);
  _attrs.emplace(_inc);
}

irs::attribute_view const& IdentityAnalyzer::attributes() const NOEXCEPT {
  return _attrs;
}

bool IdentityAnalyzer::next() {
  auto empty = _empty;

  _term.value(irs::ref_cast<irs::byte_type>(_value));
  _empty = true;
  _value = irs::string_ref::NIL;

  return !empty;
}

bool IdentityAnalyzer::reset(irs::string_ref const& data) {
  _empty = false;
  _value = data;

  return !_empty;
}

arangodb::aql::AqlValue aqlFnTokens(
    arangodb::aql::ExpressionContext* expressionContext,
    arangodb::transaction::Methods* trx,
    arangodb::aql::VPackFunctionParameters const& args
) {
  if (2 != args.size() || !args[0].isString() || !args[1].isString()) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "invalid arguments passed while computing result for function 'TOKENS'";
    TRI_set_errno(TRI_ERROR_BAD_PARAMETER);

    return arangodb::aql::AqlValue();
  }

  auto data = arangodb::iresearch::getStringRef(args[0].slice());
  auto name = arangodb::iresearch::getStringRef(args[1].slice());
  auto* analyzers = arangodb::application_features::ApplicationServer::lookupFeature<
    arangodb::iresearch::IResearchAnalyzerFeature
  >();

  if (!analyzers) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "failure to find feature 'arangosearch' while computing result for function 'TOKENS'";
    TRI_set_errno(TRI_ERROR_INTERNAL);

    return arangodb::aql::AqlValue();
  }

  auto pool = analyzers->get(name);

  if (!pool) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "failure to find arangosearch analyzer pool name '" << name << "' while computing result for function 'TOKENS'";
    TRI_set_errno(TRI_ERROR_BAD_PARAMETER);

    return arangodb::aql::AqlValue();
  }

  auto analyzer = pool->get();

  if (!analyzer) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "failure to find arangosearch analyzer name '" << name << "' while computing result for function 'TOKENS'";
    TRI_set_errno(TRI_ERROR_BAD_PARAMETER);

    return arangodb::aql::AqlValue();
  }

  if (!analyzer->reset(data)) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "failure to reset arangosearch analyzer name '" << name << "' while computing result for function 'TOKENS'";
    TRI_set_errno(TRI_ERROR_INTERNAL);

    return arangodb::aql::AqlValue();
  }

  auto& values = analyzer->attributes().get<irs::term_attribute>();

  if (!values) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "failure to retrieve values from arangosearch analyzer name '" << name << "' while computing result for function 'TOKENS'";
    TRI_set_errno(TRI_ERROR_INTERNAL);

    return arangodb::aql::AqlValue();
  }

  // to avoid copying Builder's default buffer when initializing AqlValue
  // create the buffer externally and pass ownership directly into AqlValue
  auto buffer = irs::memory::make_unique<arangodb::velocypack::Buffer<uint8_t>>();

  if (!buffer) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "failure to allocate result buffer while computing result for function 'TOKENS'";

    return arangodb::aql::AqlValue();
  }

  arangodb::velocypack::Builder builder(*buffer);

  builder.openArray();

  while (analyzer->next()) {
    auto value = irs::ref_cast<char>(values->value());

    builder.add(arangodb::iresearch::toValuePair(value));
  }

  builder.close();

  bool bufOwner = true; // out parameter from AqlValue denoting ownership aquisition (must be true initially)
  auto release = irs::make_finally([&buffer, &bufOwner]()->void {
    if (!bufOwner) {
      buffer.release();
    }
  });

  return arangodb::aql::AqlValue(buffer.get(), bufOwner);
}

void addFunctions(arangodb::aql::AqlFunctionFeature& functions) {
  arangodb::iresearch::addFunction(functions, arangodb::aql::Function{
    "TOKENS", // name
    ".,.", // positional arguments (data,analyzer)
    // deterministic (true == called during AST optimization and will be used to calculate values for constant expressions)
    arangodb::aql::Function::makeFlags(
      arangodb::aql::Function::Flags::Deterministic, 
      arangodb::aql::Function::Flags::Cacheable,
      arangodb::aql::Function::Flags::CanRunOnDBServer
    ), 
    &aqlFnTokens // function implementation
  });
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ensure configuration collection is present in the specified vocbase
////////////////////////////////////////////////////////////////////////////////
void ensureConfigCollection(TRI_vocbase_t& vocbase) {
  static const std::string json =
    std::string("{\"isSystem\": true, \"name\": \"") + ANALYZER_COLLECTION_NAME + "\", \"type\": 2}";

  if (!arangodb::ServerState::instance()->isCoordinator()) {
    try {
      vocbase.createCollection(arangodb::velocypack::Parser::fromJson(json)->slice());
    } catch(arangodb::basics::Exception& e) {
      if (TRI_ERROR_ARANGO_DUPLICATE_NAME != e.code()) {
        throw;
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a pointer to the system database or nullptr on error
////////////////////////////////////////////////////////////////////////////////
arangodb::SystemDatabaseFeature::ptr getSystemDatabase() {
  auto* database = arangodb::application_features::ApplicationServer::lookupFeature<
    arangodb::SystemDatabaseFeature
  >();

  if (!database) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "failure to find feature '" << arangodb::SystemDatabaseFeature::name() << "' while getting the system database";

    return nullptr;
  }

  return database->use();
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
  if (type.empty()) {
    // in ArangoSearch we don't allow to have analyzers with empty type string
    return nullptr;
  }

  // ArangoDB, for API consistency, only supports analyzers configurable via jSON
  return irs::analysis::analyzers::get(type, irs::text_format::json, properties);
}

IResearchAnalyzerFeature::AnalyzerPool::AnalyzerPool(
    irs::string_ref const& name
): _cache(DEFAULT_POOL_SIZE),
   _name(name) {
}

bool IResearchAnalyzerFeature::AnalyzerPool::init(
    irs::string_ref const& type,
    irs::string_ref const& properties,
    irs::flags const& features /*= irs::flags::empty_instance()*/
) {
  try {
    _cache.clear(); // reset for new type/properties

    auto instance = _cache.emplace(type, properties);

    if (instance) {
      _config.clear();
      _config.append(type).append(properties);
      _key = irs::string_ref::NIL;
      _type = irs::string_ref::NIL;
      _properties = irs::string_ref::NIL;

      if (!type.null()) {
        _type = irs::string_ref(&(_config[0]), type.size());
      }

      if (!properties.null()) {
        _properties = irs::string_ref(&(_config[0]) + _type.size(), properties.size());
      }

      _features = features ; // store only requested features

      return true;
    }
  } catch (arangodb::basics::Exception& e) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "caught exception while initializing an arangosearch analizer type '" << _type << "' properties '" << _properties << "': " << e.code() << " " << e.what();
    IR_LOG_EXCEPTION();
  } catch (std::exception& e) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "caught exception while initializing an arangosearch analizer type '" << _type << "' properties '" << _properties << "': " << e.what();
    IR_LOG_EXCEPTION();
  } catch (...) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "caught exception while initializing an arangosearch analizer type '" << _type << "' properties '" << _properties << "'";
    IR_LOG_EXCEPTION();
  }

  _config.clear(); // set as uninitialized
  _key = irs::string_ref::NIL; // set as uninitialized
  _type = irs::string_ref::NIL; // set as uninitialized
  _properties = irs::string_ref::NIL; // set as uninitialized
  _features.clear(); // set as uninitialized

  return false;
}

void IResearchAnalyzerFeature::AnalyzerPool::setKey(
    irs::string_ref const& key
) {
  if (key.null()) {
    _key = irs::string_ref::NIL;

    return; // nothing more to do
  }

  auto keyOffset = _config.size(); // append at end
  auto typeOffset = _type.null() ? 0 : (_type.c_str() - &(_config[0])); // start of type
  auto propertiesOffset = _properties.null() ? 0 : (_properties.c_str() - &(_config[0])); // start of properties

  _config.append(key.c_str(), key.size());
  _key = irs::string_ref(&(_config[0]) + keyOffset, key.size());

  // update '_type' since '_config' might have been reallocated during append(...)
  if (!_type.null()) {
    TRI_ASSERT(typeOffset + _type.size() <= _config.size());
    _type = irs::string_ref(&(_config[0]) + typeOffset, _type.size());
  }

  // update '_properties' since '_config' might have been reallocated during append(...)
  if (!_properties.null()) {
    TRI_ASSERT(propertiesOffset + _properties.size() <= _config.size());
    _properties = irs::string_ref(&(_config[0]) + propertiesOffset, _properties.size());
  }
}

irs::flags const& IResearchAnalyzerFeature::AnalyzerPool::features() const noexcept {
  return _features;
}

irs::analysis::analyzer::ptr IResearchAnalyzerFeature::AnalyzerPool::get() const noexcept {
  try {
    // FIXME do not use shared_ptr
    return _cache.emplace(_type, _properties).release();
  } catch (arangodb::basics::Exception const& e) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "caught exception while instantiating an arangosearch analizer type '" << _type << "' properties '" << _properties << "': " << e.code() << " " << e.what();
    IR_LOG_EXCEPTION();
  } catch (std::exception& e) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "caught exception while instantiating an arangosearch analizer type '" << _type << "' properties '" << _properties << "': " << e.what();
    IR_LOG_EXCEPTION();
  } catch (...) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "caught exception while instantiating an arangosearch analizer type '" << _type << "' properties '" << _properties << "'";
    IR_LOG_EXCEPTION();
  }

  return nullptr;
}

std::string const& IResearchAnalyzerFeature::AnalyzerPool::name() const noexcept {
  return _name;
}

IResearchAnalyzerFeature::IResearchAnalyzerFeature(
    arangodb::application_features::ApplicationServer& server
): ApplicationFeature(server, IResearchAnalyzerFeature::name()),
  _analyzers(getStaticAnalyzers()), // load static analyzers
  _started(false) {
  setOptional(true);
  startsAfter("V8Phase");

  startsAfter("AQLFunctions"); // used for registering IResearch analyzer functions
  startsAfter("SystemDatabase"); // used for getting the system database containing the persisted configuration
}

std::pair<IResearchAnalyzerFeature::AnalyzerPool::ptr, bool> IResearchAnalyzerFeature::emplace(
  irs::string_ref const& name,
  irs::string_ref const& type,
  irs::string_ref const& properties,
  irs::flags const& features /*= irs::flags::empty_instance()*/
) noexcept {
  return emplace(name, type, properties, true, features);
}

std::pair<IResearchAnalyzerFeature::AnalyzerPool::ptr, bool> IResearchAnalyzerFeature::emplace(
  irs::string_ref const& name,
  irs::string_ref const& type,
  irs::string_ref const& properties,
  bool initAndPersist,
  irs::flags const& features /*= irs::flags::empty_instance()*/
) noexcept {
  try {
    WriteMutex mutex(_mutex);
    SCOPED_LOCK(mutex);

    auto generator = [](
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
      LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
        << "failure creating an arangosearch analyzer instance for name '" << name << "' type '" << type << "' properties '" << properties << "'";
      TRI_set_errno(TRI_ERROR_BAD_PARAMETER);

      return std::make_pair(AnalyzerPool::ptr(), false);
    }

    // skip initialization and persistance
    if (!initAndPersist) {
      if (itr.second) {
        _customAnalyzers[itr.first->first] = itr.first->second; // mark as custom if insertion took place
      }

      erase = false;

      return std::make_pair(pool, itr.second);
    }

    if (itr.second) { // new pool
      if (!_started) {
        LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
          << "cannot garantee collision-free persistance while creating an arangosearch analyzer instance for name '" << name << "' type '" << type << "' properties '" << properties << "'";
        TRI_set_errno(TRI_ERROR_ARANGO_ILLEGAL_STATE);

        return std::make_pair(AnalyzerPool::ptr(), false);
      }

      if (!pool->init(type, properties, features)) {
        LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
          << "failure initializing an arangosearch analyzer instance for name '" << name << "' type '" << type << "' properties '" << properties << "'";
        TRI_set_errno(TRI_ERROR_BAD_PARAMETER);

        return std::make_pair(AnalyzerPool::ptr(), false);
      }

      if (!storeConfiguration(*pool)) {
        LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
          << "failure persisting an arangosearch analyzer instance for name '" << name << "' type '" << type << "' properties '" << properties << "'";
        TRI_set_errno(TRI_ERROR_BAD_PARAMETER);

        return std::make_pair(AnalyzerPool::ptr(), false);
      }

      _customAnalyzers[itr.first->first] = itr.first->second; // mark as custom
      erase = false;
    } else if (type != pool->_type || properties != pool->_properties) {
      LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
        << "name collision detected while registering an arangosearch analizer name '" << name << "' type '" << type << "' properties '" << properties << "', previous registration type '" << pool->_type << "' properties '" << pool->_properties << "'";
      TRI_set_errno(TRI_ERROR_BAD_PARAMETER);

      return std::make_pair(AnalyzerPool::ptr(), false);
    } else if (pool->_key.null() && !storeConfiguration(*pool)) { // not yet persisted
      LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
        << "failure persisting an arangosearch analyzer instance for name '" << name << "' type '" << type << "' properties '" << properties << "'";
      TRI_set_errno(TRI_ERROR_BAD_PARAMETER);

      return std::make_pair(AnalyzerPool::ptr(), false);
    }

    return std::make_pair(pool, itr.second);
  } catch (arangodb::basics::Exception& e) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "caught exception while registering an arangosearch analizer name '" << name << "' type '" << type << "' properties '" << properties << "': " << e.code() << " " << e.what();
    IR_LOG_EXCEPTION();
  } catch (std::exception& e) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "caught exception while registering an arangosearch analizer name '" << name << "' type '" << type << "' properties '" << properties << "': " << e.what();
    IR_LOG_EXCEPTION();
  } catch (...) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "caught exception while registering an arangosearch analizer name '" << name << "' type '" << type << "' properties '" << properties << "'";
    IR_LOG_EXCEPTION();
  }

  return std::make_pair(AnalyzerPool::ptr(), false);
}

IResearchAnalyzerFeature::AnalyzerPool::ptr IResearchAnalyzerFeature::ensure(
    irs::string_ref const& name
) {
  // insert dummy (uninitialized) placeholders if this feature has not been
  // started to break the dependency loop on DatabaseFeature
  // placeholders will be loaded/validation during start()/loadConfiguration()
  return _started
    ? get(name)
    : emplace(name, irs::string_ref::NIL, irs::string_ref::NIL, false).first;
}

size_t IResearchAnalyzerFeature::erase(irs::string_ref const& name) noexcept {
  try {
    WriteMutex mutex(_mutex);
    SCOPED_LOCK(mutex);

    auto itr = _customAnalyzers.find(irs::make_hashed_ref(name, std::hash<irs::string_ref>()));

    if (itr == _customAnalyzers.end()) {
      return 0; // nothing to erase
    }

    auto pool = itr->second;

    if (!pool) {
      LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
        << "removal of an unset arangosearch analizer name '" << name << "'";
      _analyzers.erase(itr->first); // ok to erase since found in '_customAnalyzers'
      _customAnalyzers.erase(itr);

      return 0; // no actual valid analyzer was removed (this is definitly a bug somewhere)
    }

    if (_started) {
      auto vocbase = getSystemDatabase();

      if (!vocbase) {
        LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
          << "failure to get system database while removing arangosearch analyzer name '" << pool->name() << "'";

        return 0;
      }

      arangodb::SingleCollectionTransaction trx(
        arangodb::transaction::StandaloneContext::Create(*vocbase),
        ANALYZER_COLLECTION_NAME,
        arangodb::AccessMode::Type::WRITE
      );
      auto res = trx.begin();

      if (!res.ok()) {
        LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
          << "failure to start transaction while removing configuration for arangosearch analyzer name '" << pool->name() << "'";

        return 0;
      }

      arangodb::velocypack::Builder builder;
      arangodb::OperationOptions options;

      builder.openObject();
      builder.add(arangodb::StaticStrings::KeyString, toValuePair(pool->_key));
      builder.close();
      options.waitForSync = true;

      auto result = trx.remove(ANALYZER_COLLECTION_NAME, builder.slice(), options);

      // stataic analyzers are not persisted
      if (!result.ok() && result.isNot(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND)) {
        LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
          << "failure to persist AnalyzerPool configuration while removing arangosearch analyzer name '" << pool->name() << "'";
        trx.abort();

        return 0;
      }

      if (!trx.commit().ok()) {
        LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
          << "failure to commit AnalyzerPool configuration while removing arangosearch analyzer name '" << pool->name() << "'";
        trx.abort();

        return 0;
      }
    }

    // OK to erase if !_started because on start() the persisted configuration will be loaded
    _analyzers.erase(itr->first); // ok to erase since found in '_customAnalyzers'
    _customAnalyzers.erase(itr);

    return 1;
  } catch (arangodb::basics::Exception& e) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "caught exception while removing an arangosearch analizer name '" << name << "': " << e.code() << " " << e.what();
    IR_LOG_EXCEPTION();
  } catch (std::exception& e) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "caught exception while removing an arangosearch analizer name '" << name << "': " << e.what();
    IR_LOG_EXCEPTION();
  } catch (...) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "caught exception while removing an arangosearch analizer name '" << name << "'";
    IR_LOG_EXCEPTION();
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
      LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
        << "failure to find arangosearch analyzer name '" << name << "'";

      return nullptr;
    }

    auto pool = itr->second;

    if (pool) {
      return pool;
    }

    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "failure to get arangosearch analyzer name '" << name << "'";
    TRI_set_errno(TRI_ERROR_INTERNAL);
  } catch (arangodb::basics::Exception& e) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "caught exception while retrieving an arangosearch analizer name '" << name << "': " << e.code() << " "  << e.what();
    IR_LOG_EXCEPTION();
  } catch (std::exception& e) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "caught exception while retrieving an arangosearch analizer name '" << name << "': " << e.what();
    IR_LOG_EXCEPTION();
  } catch (...) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "caught exception while retrieving an arangosearch analizer name '" << name << "'";
    IR_LOG_EXCEPTION();
  }

  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a container of statically defined/initialized analyzers
////////////////////////////////////////////////////////////////////////////////
/*static*/ IResearchAnalyzerFeature::Analyzers const& IResearchAnalyzerFeature::getStaticAnalyzers() {
  struct Instance {
    Analyzers analyzers;
    Instance() {
      // register the indentity analyzer
      {
        static const irs::flags extraFeatures = { irs::frequency::type(), irs::norm::type() };
        //static const irs::flags extraFeatures = { }; // FIXME TODO use above once tfidf/bm25 sort is fixed
        static const irs::string_ref name("identity");
        PTR_NAMED(AnalyzerPool, pool, name);

        if (!pool
            || !pool->init(IdentityAnalyzer::type().name(), irs::string_ref::NIL, extraFeatures)) {
          LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
            << "failure creating an arangosearch static analyzer instance for name '" << name << "'";
          throw irs::illegal_state(); // this should never happen, treat as an assertion failure
        }

        analyzers.emplace(
          irs::make_hashed_ref(name, std::hash<irs::string_ref>()),
          pool
        );
      }

      // register the text analyzers
      {
        // Note: ArangoDB strings coming from JavaScript user input are UTF-8 encoded
        static const std::vector<std::pair<irs::string_ref, irs::string_ref>> textAnalzyers = {
          {"text_de", "{ \"locale\": \"de.UTF-8\", \"ignored_words\": [ ] }" }, // empty stop word list
          {"text_en", "{ \"locale\": \"en.UTF-8\", \"ignored_words\": [ ] }" }, // empty stop word list
          {"text_es", "{ \"locale\": \"es.UTF-8\", \"ignored_words\": [ ] }" }, // empty stop word list
          {"text_fi", "{ \"locale\": \"fi.UTF-8\", \"ignored_words\": [ ] }" }, // empty stop word list
          {"text_fr", "{ \"locale\": \"fr.UTF-8\", \"ignored_words\": [ ] }" }, // empty stop word list
          {"text_it", "{ \"locale\": \"it.UTF-8\", \"ignored_words\": [ ] }" }, // empty stop word list
          {"text_nl", "{ \"locale\": \"nl.UTF-8\", \"ignored_words\": [ ] }" }, // empty stop word list
          {"text_no", "{ \"locale\": \"no.UTF-8\", \"ignored_words\": [ ] }" }, // empty stop word list
          {"text_pt", "{ \"locale\": \"pt.UTF-8\", \"ignored_words\": [ ] }" }, // empty stop word list
          {"text_ru", "{ \"locale\": \"ru.UTF-8\", \"ignored_words\": [ ] }" }, // empty stop word list
          {"text_sv", "{ \"locale\": \"sv.UTF-8\", \"ignored_words\": [ ] }" }, // empty stop word list
          {"text_zh", "{ \"locale\": \"zh.UTF-8\", \"ignored_words\": [ ] }" }, // empty stop word list
        };
        static const irs::flags extraFeatures = { irs::frequency::type(), irs::norm::type(), irs::position::type() }; // add norms + frequency/position for by_phrase
        static const irs::string_ref type("text");

        for (auto& entry: textAnalzyers) {
          auto& name = entry.first;
          auto& args = entry.second;
          PTR_NAMED(AnalyzerPool, pool, name);

          if (!pool
              || !pool->init(type, args, extraFeatures)) {
            LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
              << "failure creating an arangosearch static analyzer instance for name '" << name << "'";
            throw irs::illegal_state(); // this should never happen, treat as an assertion failure
          }


          analyzers.emplace(
            irs::make_hashed_ref(name, std::hash<irs::string_ref>()),
            pool
          );
        }
      }
    }
  };
  static const Instance instance;

  return instance.analyzers;
}

/*static*/ IResearchAnalyzerFeature::AnalyzerPool::ptr IResearchAnalyzerFeature::identity() noexcept {
  struct Identity {
    AnalyzerPool::ptr instance;
    Identity() {
      // find the 'identity' analyzer pool in the static analyzers
      auto& staticAnalyzers = getStaticAnalyzers();
      irs::string_ref name = "identity"; // hardcoded name of the identity analyzer pool
      auto key = irs::make_hashed_ref(name, std::hash<irs::string_ref>());
      auto itr = staticAnalyzers.find(key);

      if (itr != staticAnalyzers.end()) {
        instance = itr->second;
      }
    }
  };
  static const Identity identity;

  return identity.instance;
}

bool IResearchAnalyzerFeature::loadConfiguration() {
  if (arangodb::ServerState::instance()->isRunningInCluster()) {
    // the following code will not be working in the cluster
    // safe to access since loadConfiguration(...) is called from start() which is single-thread
    return _customAnalyzers.empty();
  }

  auto vocbase = getSystemDatabase();

  if (!vocbase) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "failure to get system database while loading arangosearch analyzer persisted configuration";

    return false;
  }

  arangodb::SingleCollectionTransaction trx(
    arangodb::transaction::StandaloneContext::Create(*vocbase),
    ANALYZER_COLLECTION_NAME,
    arangodb::AccessMode::Type::READ
  );
  auto res = trx.begin();

  if (!res.ok()) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "failure to start transaction while loading arangosearch analyzer persisted configuration";

    return false;
  }

  auto* collection = trx.documentCollection();

  if (!collection) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "failure to get collection while loading arangosearch analyzer persisted configuration";
    trx.abort();

    return false;
  }

  std::unordered_map<irs::string_ref, AnalyzerPool::ptr> initialized;
  auto visitor = [this, &trx, collection, &initialized](
      LocalDocumentId const& token
  )->bool {
    ManagedDocumentResult result;

    if (!collection->readDocument(&trx, token, result)) {
      LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
        << "skipping failed read of an arangosearch analyzer persisted configuration token: " << token.id();

      return true; // failed to read document, skip
    }

    arangodb::velocypack::Slice slice(result.vpack());

    if (!slice.isObject()
        || !slice.hasKey(arangodb::StaticStrings::KeyString)
        || !slice.get(arangodb::StaticStrings::KeyString).isString()
        || !slice.hasKey("name") || !slice.get("name").isString()
        || !slice.hasKey("type") || !slice.get("type").isString()
        || !slice.hasKey("properties")
        || !(
          slice.get("properties").isNull()
          || slice.get("properties").isString()
          || slice.get("properties").isArray()
          || slice.get("properties").isObject()
        )) {
      LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
        << "skipping invalid arangosearch analyzer persisted configuration entry: " << slice.toJson();

      return true; // not a valid configuration, skip
    }

    auto key = getStringRef(slice.get(arangodb::StaticStrings::KeyString));
    auto name = getStringRef(slice.get("name"));
    auto type = getStringRef(slice.get("type"));
    irs::string_ref properties;
    auto propertiesSlice = slice.get("properties");
    std::string tmpString;

    // encode jSON array/object as a string for analyzers that support jSON
    if (propertiesSlice.isArray() || propertiesSlice.isObject()) {
      tmpString = propertiesSlice.toJson(); // format as a jSON encoded string
      properties = tmpString;
    } else {
      properties = getStringRef(propertiesSlice);
    }

    auto entry = emplace(name, type, properties, false); // do not persist since this config is already coming from the persisted store
    auto& pool = entry.first;

    if (!pool) {
      LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
        << "failure creating an arangosearch analyzer instance for name '" << name << "' type '" << type << "' properties '" << properties << "'";

      THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        std::string("failure creating an arangosearch analyzer instance for name '") + std::string(name) + "' type '" + std::string(type) + "' properties '" + std::string(properties) + "'"
      );
    }

    if (!entry.second && initialized.find(name) != initialized.end()) {
      LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
        << "name collision detected while registering an arangosearch analizer name '" << name << "' type '" << type << "' properties '" << properties << "', previous registration type '" << pool->_type << "' properties '" << pool->_properties << "'";

      THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        std::string("name collision detected while registering an arangosearch analizer name '") + std::string(name) + "' type '" + std::string(type) + "' properties '" + std::string(properties) + "', previous registration type '" + std::string(pool->_type) + "' properties '" + std::string(pool->_properties) + "'"
      );
    }

    static auto& staticAnalyzers = getStaticAnalyzers();

    if (!entry.second
        && staticAnalyzers.find(irs::make_hashed_ref(name, std::hash<irs::string_ref>())) != staticAnalyzers.end()) {
      if (type != pool->_type || properties != pool->_properties) {
        LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
          << "name collision with a static analyzer detected while registering an arangosearch analizer name '" << name << "' type '" << type << "' properties '" << properties << "', previous registration type '" << pool->_type << "' properties '" << pool->_properties << "'";

        THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_INTERNAL,
          std::string("name collision with a static analyzer detected while registering an arangosearch analizer name '") + std::string(name) + "' type '" + std::string(type) + "' properties '" + std::string(properties) + "', previous registration type '" + std::string(pool->_type) + "' properties '" + std::string(pool->_properties) + "'"
        );
      }

      LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
        << "name collision with a static analyzer detected while registering an arangosearch analizer name '" << name << "' type '" << type << "' properties '" << properties << "'";

      THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL,
        std::string("name collision with a static analyzer detected while registering an arangosearch analizer name '") + std::string(name) + "' type '" + std::string(type) + "' properties '" + std::string(properties) + "'"
      );
    } else if (!pool->init(type, properties)) {
      LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
        << "failure initializing an arangosearch analyzer instance for name '" << name << "' type '" << type << "' properties '" << properties << "'";

      THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        std::string("failure initializing an arangosearch analyzer instance for name '") + std::string(name) + "' type '" + std::string(type) + "' properties '" + std::string(properties) + "'"
      );
    }

    initialized.emplace(
      std::piecewise_construct,
      std::forward_as_tuple(pool->name()), // emplace with ref at string in pool
      std::forward_as_tuple(pool)
    );
    pool->setKey(key);

    return true; // process next
  };

  WriteMutex mutex(_mutex);
  SCOPED_LOCK(mutex); // the cache could return the same pool asynchronously before '_key' updated/rolled back below
  bool revert = true;
  auto cleanup = irs::make_finally([&revert, &initialized]()->void{
    if (revert) {

      for (auto& entry: initialized) {
        auto& pool = entry.second;

        // reset pool configuration back to uninitialized
        // safe to reset since loadConfiguration(...) is called from start() which is single-thread
        if (pool) {
          pool->_config.clear(); // noexcept
          pool->_key = irs::string_ref::NIL; // noexcept
          pool->_type = irs::string_ref::NIL; // noexcept
          pool->_properties = irs::string_ref::NIL; // noexcept
        }
      }
   }
  });

  try {
    collection->invokeOnAllElements(&trx, visitor);

    // ensure all records were initialized
    for (auto& entry: _customAnalyzers) {
      if (initialized.find(entry.first) == initialized.end()) {
        LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
          << "uninitialized AnalyzerPool deletected while validating analyzers, arangosearch analyzer name '" << entry.first << "'";
        return false; // found an uninitialized analyzer
      }
    }

    if (!trx.commit().ok()) {
      LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
        << "failure to commit transaction while loading AnalyzerFeature configuration";
      return false;
    }

    revert = false;

    return true;
  } catch (arangodb::basics::Exception& e) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "caught exception while loading configuration: " << e.code() << " "  << e.what();
    IR_LOG_EXCEPTION();
  } catch (std::exception& e) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "caught exception while loading configuration: " << e.what();
    IR_LOG_EXCEPTION();
  } catch (...) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "caught exception while loading configuration";
    IR_LOG_EXCEPTION();
  }

  return false;
}

/*static*/ std::string const& IResearchAnalyzerFeature::name() noexcept {
  return FEATURE_NAME;
}

void IResearchAnalyzerFeature::prepare() {
  ApplicationFeature::prepare();

  // load all known analyzers
  ::iresearch::analysis::analyzers::init();
}

void IResearchAnalyzerFeature::start() {
  ApplicationFeature::start();

  // register analyzer functions
  {
    auto* functions = arangodb::application_features::ApplicationServer::lookupFeature<
      arangodb::aql::AqlFunctionFeature
    >("AQLFunctions");

    if (functions) {
      addFunctions(*functions);
    } else {
      LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
        << "failure to find feature 'AQLFunctions' while registering IResearch functions";
    }
  }

  // ensure that the configuration collection is present before loading configuration
  // for the case of inRecovery() if there is no collection then obviously no
  // custom analyzer configurations were persisted (so missing analyzer is failure)
  // if there is a configuration collection then just load analizer configurations
  {
    auto vocbase = getSystemDatabase();

    if (!vocbase) {
      LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
        << "failure to get system database while starting feature 'IResearchAnalyzer'";
      // assume configuration collection exists
    } else {
      auto collection = vocbase->lookupCollection(ANALYZER_COLLECTION_NAME);

      if (!collection) {
        auto* engine = arangodb::EngineSelectorFeature::ENGINE;

        if (!engine) {
          LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
            << "failure to get storage engine while starting feature 'IResearchAnalyzer'";
          // assume not inRecovery(), create collection immediately
        } else if (engine->inRecovery()) {
          auto* feature = arangodb::application_features::ApplicationServer::lookupFeature<
            arangodb::DatabaseFeature
          >("Database");

          if (!feature) {
            LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
              << "failure to find feature 'Database' while starting feature 'IResearchAnalyzer'";
            // can't register post-recovery callback, create collection immediately
          } else {
            std::shared_ptr<TRI_vocbase_t> sharedVocbase(std::move(vocbase));

            feature->registerPostRecoveryCallback([this, sharedVocbase]()->arangodb::Result {
              ensureConfigCollection(*sharedVocbase); // ensure configuration collection exists

              WriteMutex mutex(_mutex);
              SCOPED_LOCK(mutex); // '_started' can be asynchronously read

              // ensure all records were initialized
              if (!_customAnalyzers.empty()) {
                return arangodb::Result(
                  TRI_ERROR_INTERNAL,
                  "uninitialized AnalyzerPool detected while validating analyzers"
                );
              }

              _started = true;

              return arangodb::Result();
            });

            return; // nothing more to do while inRecovery()
          }
        }

        ensureConfigCollection(*vocbase); // ensure configuration collection exists

        WriteMutex mutex(_mutex);
        SCOPED_LOCK(mutex); // '_customAnalyzers' can be asynchronously modified, '_started' can be asynchronously read

        // ensure all records were initialized
        if (!_customAnalyzers.empty()) {
          THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_INTERNAL,
            "uninitialized AnalyzerPool detected while validating analyzers"
          );
        }

        _started = true;

        return; // no persisted configurations to load since just created collection
      }
    }

    // load persisted configuration
    if (!loadConfiguration()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL,
        "uninitialized AnalyzerPool detected while validating analyzers"
      );
    }
  }

  WriteMutex mutex(_mutex);
  SCOPED_LOCK(mutex); // '_started' can be asynchronously read

  _started = true;
}

void IResearchAnalyzerFeature::stop() {
  {
    WriteMutex mutex(_mutex);
    SCOPED_LOCK(mutex); // '_analyzers'/_customAnalyzers/'_started' can be asynchronously read

    _started = false;
    _analyzers = getStaticAnalyzers(); // clear cache and reload static analyzers
    _customAnalyzers.clear(); // clear cache
  }

  ApplicationFeature::stop();
}

bool IResearchAnalyzerFeature::storeConfiguration(AnalyzerPool& pool) {
  if (pool._type.null()) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
        << "failure to persist arangosearch analyzer '" << pool.name()
        << "' configuration with 'null' type";

    return false;
  }

  auto vocbase = getSystemDatabase();

  if (!vocbase) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "failure to get system database while persisting configuration arangosearch analyzer name '" << pool.name() << "'";

    return false;
  }

  try {
    arangodb::SingleCollectionTransaction trx(
      arangodb::transaction::StandaloneContext::Create(*vocbase),
      ANALYZER_COLLECTION_NAME,
      arangodb::AccessMode::Type::WRITE
    );
    auto res = trx.begin();

    if (!res.ok()) {
      LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
        << "failure to start transaction while persisting configuration for arangosearch analyzer name '" << pool.name() << "'";

      return false;
    }

    arangodb::velocypack::Builder builder;
    arangodb::OperationOptions options;

    builder.openObject();
    builder.add("name", arangodb::velocypack::Value(pool.name()));
    builder.add("type", toValuePair(pool._type));

    // do not allow to pass null properties since it causes undefined
    // behavior in `arangodb::velocypack::Builder`
    if (pool._properties.null()) {
      builder.add("properties", arangodb::velocypack::Value(arangodb::velocypack::ValueType::Null));
    } else {
      builder.add("properties", toValuePair(pool._properties));
    }

    builder.close();
    options.waitForSync = true;

    auto result = trx.insert(ANALYZER_COLLECTION_NAME, builder.slice(), options);

    if (!result.ok()) {
      LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
        << "failure to persist AnalyzerPool configuration while persisting configuration for arangosearch analyzer name '" << pool.name() << "'";
      trx.abort();

      return false;
    }

    auto key = result.slice().get(arangodb::StaticStrings::KeyString);

    if (!key.isString()) {
      LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
        << "failure to find the resulting key field while persisting configuration for arangosearch analyzer name '" << pool.name() << "'";
      trx.abort();

      return false;
    }

    if (!trx.commit().ok()) {
      LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
        << "failure to commit AnalyzerPool configuration while persisting configuration for arangosearch analyzer name '" << pool.name() << "'";
      trx.abort();

      return false;
    }

    pool.setKey(getStringRef(key));

    return true;
  } catch (arangodb::basics::Exception& e) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "caught exception during persist of an AnalyzerPool configuration while persisting configuration for arangosearch analyzer name '" << pool.name() << "': " << e.code() << " "  << e.what();
    IR_LOG_EXCEPTION();
  } catch (std::exception& e) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "caught exception during persist of an AnalyzerPool configuration while persisting configuration for arangosearch analyzer name '" << pool.name() << "': " << e.what();
    IR_LOG_EXCEPTION();
  } catch (...) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "caught exception during persist of an AnalyzerPool configuration while persisting configuration for arangosearch analyzer name '" << pool.name() << "'";
    IR_LOG_EXCEPTION();
  }

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