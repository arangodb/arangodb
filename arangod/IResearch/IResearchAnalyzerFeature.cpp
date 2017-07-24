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

#include "analysis/token_attributes.hpp"

#include "ApplicationServerHelper.h"
#include "VelocyPackHelper.h"

#include "Aql/AqlFunctionFeature.h"
#include "Logger/Logger.h"
#include "Logger/LogMacros.h"

#include "IResearchAnalyzerFeature.h"

NS_LOCAL

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

void addFunctions(arangodb::aql::AqlFunctionFeature& functions) {
  static auto tokens_impl = [](
      arangodb::aql::Query* query,
      arangodb::transaction::Methods* trx,
      arangodb::aql::VPackFunctionParameters const& args
  )->arangodb::aql::AqlValue {
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

    auto analyzer = pool.get();

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
  };
  arangodb::aql::Function tokens(
    "TOKENS", // external name (AQL function external names are always in upper case)
    "tokens", // internal name
    "data analyzer", // argument variable names
    false, // cacheable
    false, // deterministic
    true, // can throw
    true, // can be run on server
    true, // can pass arguments by reference
    tokens_impl
  );

  functions.add(tokens);
}

NS_END

NS_BEGIN(arangodb)
NS_BEGIN(iresearch)

/*static*/ IResearchAnalyzerFeature::AnalyzerPool::Builder::ptr IResearchAnalyzerFeature::AnalyzerPool::Builder::make(
    irs::string_ref const& type,
    irs::string_ref const& properties
) {
  return irs::analysis::analyzers::get(type, properties);
}

IResearchAnalyzerFeature::AnalyzerPool::AnalyzerPool() {
  static const Meta meta;
  _meta = (&meta);
}

IResearchAnalyzerFeature::AnalyzerPool::AnalyzerPool(
    Meta const& meta,
    std::shared_ptr<Cache>const& pool
): _meta(&meta), _pool(pool) {
}

IResearchAnalyzerFeature::AnalyzerPool::AnalyzerPool(
    AnalyzerPool const& other
) {
  *this = other;
}

IResearchAnalyzerFeature::AnalyzerPool::AnalyzerPool(
    AnalyzerPool&& other
) noexcept {
  *this = std::move(other);
}

IResearchAnalyzerFeature::AnalyzerPool& IResearchAnalyzerFeature::AnalyzerPool::operator=(
    AnalyzerPool const& other
) {
  if (this != &other) {
    _meta = other._meta;
    _pool = other._pool;
  }

  return *this;
}

IResearchAnalyzerFeature::AnalyzerPool& IResearchAnalyzerFeature::AnalyzerPool::operator=(
    AnalyzerPool&& other
) noexcept {
  if (this != &other) {
    _meta = std::move(other._meta);
    _pool = std::move(other._pool);
  }

  return *this;
}

IResearchAnalyzerFeature::AnalyzerPool::operator bool() const noexcept {
  return false == !_pool;
}

irs::flags const& IResearchAnalyzerFeature::AnalyzerPool::features() const noexcept {
  assert(_meta);

  return _meta->_features;
}

irs::analysis::analyzer::ptr IResearchAnalyzerFeature::AnalyzerPool::get() const noexcept {
  assert(_meta);

  try {
    return _pool->emplace(_meta->_type, _meta->_properties);
  } catch (std::exception& e) {
    LOG_TOPIC(WARN, Logger::FIXME) << "caught exception while instantiating an IResearch analizer type '" << _meta->_type << "' properties '" << _meta->_properties << "': " << e.what();
    IR_EXCEPTION();
  } catch (...) {
    LOG_TOPIC(WARN, Logger::FIXME) << "caught exception while instantiating an IResearch analizer type '" << _meta->_type << "' properties '" << _meta->_properties << "'";
    IR_EXCEPTION();
  }

  return nullptr;
}

IResearchAnalyzerFeature::IResearchAnalyzerFeature(
    arangodb::application_features::ApplicationServer* server
): ApplicationFeature(server, IResearchAnalyzerFeature::name()), _started(false) {
  setOptional(true);
  requiresElevatedPrivileges(false);
  startsAfter("AQLFunctions"); // used for registering IResearch analyzer functions
}

IResearchAnalyzerFeature::AnalyzerPool IResearchAnalyzerFeature::emplace(
  irs::string_ref const& name,
  irs::string_ref const& type,
  irs::string_ref const& properties
) noexcept {
  try {
    irs::async_utils::read_write_mutex::write_mutex mutex(_mutex);
    SCOPED_LOCK(mutex);

    auto itr = _analyzers.emplace(
      std::piecewise_construct,
      std::forward_as_tuple(irs::make_hashed_ref(name, std::hash<irs::string_ref>())),
      std::forward_as_tuple()
    );

    bool erase = itr.second;
    auto cleanup = irs::make_finally([&erase, this, &itr]()->void {
      if (erase) {
        _analyzers.erase(itr.first); // ensure no broken analyzers are left behind
      }
    });

    auto& entry = itr.first->second;
    auto pool = get(entry);

    if (!pool) {
      LOG_TOPIC(WARN, Logger::FIXME) << "failure to get IResearch analyzer name '" << name << "' type '" << type << "' properties '" << properties << "'";
      TRI_set_errno(TRI_ERROR_INTERNAL);

      return AnalyzerPool();
    }

    if (itr.second) {
      entry._meta._properties = properties;
      entry._meta._type = type;

      AnalyzerPool analyzer(entry._meta, pool);
      auto instance = analyzer.get();

      if (!instance) {
        LOG_TOPIC(WARN, Logger::FIXME) << "failure creating an IResearch analyzer instance for name '" << name << "' type '" << type << "' properties '" << properties << "'";
        TRI_set_errno(TRI_ERROR_BAD_PARAMETER);

        return AnalyzerPool();
      }

      entry._meta._features = instance->attributes().features();

      // FIXME TODO store definition in persisted mappings

      erase = false;
    } else if (type != entry._meta._type || properties != entry._meta._properties) {
      LOG_TOPIC(WARN, Logger::FIXME) << "name collision detected while registering an IResearch analizer name '" << name << "' type '" << type << "' properties '" << properties << "', previous registration type '" << entry._meta._type << "' properties '" << entry._meta._properties << "'";
      TRI_set_errno(TRI_ERROR_BAD_PARAMETER);

      return AnalyzerPool();
    }

    return AnalyzerPool(entry._meta, pool);
  } catch (std::exception& e) {
    LOG_TOPIC(WARN, Logger::FIXME) << "caught exception while registering an IResearch analizer name '" << name << "' type '" << type << "' properties '" << properties << "': " << e.what();
    IR_EXCEPTION();
  } catch (...) {
    LOG_TOPIC(WARN, Logger::FIXME) << "caught exception while registering an IResearch analizer name '" << name << "' type '" << type << "' properties '" << properties << "'";
    IR_EXCEPTION();
  }

  return AnalyzerPool();
}

IResearchAnalyzerFeature::AnalyzerPool IResearchAnalyzerFeature::get(
    irs::string_ref const& name
) const noexcept {
  try {
    irs::async_utils::read_write_mutex::read_mutex mutex(_mutex);
    SCOPED_LOCK(mutex);
    auto itr = _analyzers.find(irs::make_hashed_ref(name, std::hash<irs::string_ref>()));

    if (itr == _analyzers.end()) {
      LOG_TOPIC(WARN, Logger::FIXME) << "failure to find IResearch analyzer name '" << name << "'";

      return AnalyzerPool();
    }

    auto& entry = itr->second;
    auto pool = get(entry);

    if (!pool) {
      LOG_TOPIC(WARN, Logger::FIXME) << "failure to get IResearch analyzer name '" << name << "' type '" << entry._meta._type << "' properties '" << entry._meta._properties << "'";
      TRI_set_errno(TRI_ERROR_INTERNAL);

      return AnalyzerPool();
    }

    return AnalyzerPool(entry._meta, pool);
  } catch (std::exception& e) {
    LOG_TOPIC(WARN, Logger::FIXME) << "caught exception while retrieving an IResearch analizer name '" << name << "': " << e.what();
    IR_EXCEPTION();
  } catch (...) {
    LOG_TOPIC(WARN, Logger::FIXME) << "caught exception while retrieving an IResearch analizer name '" << name << "'";
    IR_EXCEPTION();
  }

  return AnalyzerPool();
}

std::shared_ptr<IResearchAnalyzerFeature::AnalyzerPool::Cache> IResearchAnalyzerFeature::get(
    AnalyzerEntry const& entry
) const {
  SCOPED_LOCK(entry._mutex); // no thread safety between lock() and update of weak_ptr
  auto pool = entry._pool.lock();

  if (!pool) {
    pool = irs::memory::make_unique<AnalyzerPool::Cache>(DEFAULT_POOL_SIZE);
    entry._pool = pool;
  }

  return pool;
}

irs::string_ref const& IResearchAnalyzerFeature::identity() const noexcept {
  return IDENTITY_TOKENIZER_NAME;
}

/*static*/ std::string const& IResearchAnalyzerFeature::name() {
  return FEATURE_NAME;
}

void IResearchAnalyzerFeature::prepare() {
  ApplicationFeature::prepare();

  // load all known analyzers
  ::iresearch::analysis::analyzers::init();
}

void IResearchAnalyzerFeature::start() {
  ApplicationFeature::start();

  // FIXME TODO load persisted mappings

  auto* functions = getFeature<arangodb::aql::AqlFunctionFeature>("AQLFunctions");

  if (functions) {
    addFunctions(*functions);
  } else {
    LOG_TOPIC(WARN, Logger::FIXME) << "failure to find feature 'AQLFunctions' while registering IResearch functions";
  }

  _started = true;
}

void IResearchAnalyzerFeature::stop() {
  _started = false;
  ApplicationFeature::stop();
}

size_t IResearchAnalyzerFeature::remove(
    irs::string_ref const& name,
    bool force /*= false*/
) noexcept {
  try {
    // FIXME TODO do not remove name if it is still referenced (unless force)
    // FIXME TODO remove definition from persisted mappings
    irs::async_utils::read_write_mutex::write_mutex mutex(_mutex);
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

bool IResearchAnalyzerFeature::visit(
    std::function<bool(irs::string_ref const& name, irs::string_ref const& type, irs::string_ref const& properties)> const& visitor
) {
  irs::async_utils::read_write_mutex::read_mutex mutex(_mutex);
  SCOPED_LOCK(mutex);

  for (auto& entry: _analyzers) {
    if (!visitor(entry.first, entry.second._meta._type, entry.second._meta._properties)) {
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