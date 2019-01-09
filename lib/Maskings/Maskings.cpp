////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include "Maskings.h"

#include <iostream>

#include "Basics/FileUtils.h"
#include "Logger/Logger.h"
#include "Random/RandomGenerator.h"

#include <velocypack/Iterator.h>
#include <velocypack/Parser.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::maskings;

MaskingsResult Maskings::fromFile(std::string const& filename) {
  std::string definition;

  try {
    definition = basics::FileUtils::slurp(filename);
  } catch (std::exception const& e) {
    std::string msg = "cannot read maskings file '" + filename + "': " + e.what();
    LOG_TOPIC(DEBUG, Logger::CONFIG) << msg;

    return MaskingsResult(MaskingsResult::CANNOT_READ_FILE, msg);
  }

  LOG_TOPIC(DEBUG, Logger::CONFIG) << "found maskings file '" << filename;

  if (definition.empty()) {
    std::string msg = "maskings file '" + filename + "' is empty";
    LOG_TOPIC(DEBUG, Logger::CONFIG) << msg;
    return MaskingsResult(MaskingsResult::CANNOT_READ_FILE, msg);
  }

  std::unique_ptr<Maskings> maskings(new Maskings{});

  maskings.get()->_randomSeed = RandomGenerator::interval(UINT64_MAX);

  try {
    std::shared_ptr<VPackBuilder> parsed = velocypack::Parser::fromJson(definition);

    ParseResult<Maskings> res = maskings->parse(parsed->slice());

    if (res.status != ParseResult<Maskings>::VALID) {
      return MaskingsResult(MaskingsResult::ILLEGAL_DEFINITION, res.message);
    }

    return MaskingsResult(std::move(maskings));
  } catch (velocypack::Exception const& e) {
    std::string msg = "cannot parse maskings file '" + filename + "': " + e.what();
    LOG_TOPIC(DEBUG, Logger::CONFIG) << msg << ". file content: " << definition;

    return MaskingsResult(MaskingsResult::CANNOT_PARSE_FILE, msg);
  }
}

ParseResult<Maskings> Maskings::parse(VPackSlice const& def) {
  if (!def.isObject()) {
    return ParseResult<Maskings>(ParseResult<Maskings>::DUPLICATE_COLLECTION,
                                 "expecting an object for masking definition");
  }

  for (auto const& entry : VPackObjectIterator(def, false)) {
    std::string key = entry.key.copyString();
    LOG_TOPIC(TRACE, Logger::CONFIG) << "masking collection '" << key << "'";

    if (_collections.find(key) != _collections.end()) {
      return ParseResult<Maskings>(ParseResult<Maskings>::DUPLICATE_COLLECTION,
                                   "duplicate collection entry '" + key + "'");
    }

    ParseResult<Collection> c = Collection::parse(this, entry.value);

    if (c.status != ParseResult<Collection>::VALID) {
      return ParseResult<Maskings>((ParseResult<Maskings>::StatusCode)(int)c.status,
                                   c.message);
    }

    _collections[key] = c.result;
  }

  return ParseResult<Maskings>(ParseResult<Maskings>::VALID);
}

bool Maskings::shouldDumpStructure(std::string const& name) {
  auto const itr = _collections.find(name);

  if (itr == _collections.end()) {
    return false;
  }

  switch (itr->second.selection()) {
    case CollectionSelection::FULL:
      return true;
    case CollectionSelection::MASKED:
      return true;
    case CollectionSelection::EXCLUDE:
      return false;
    case CollectionSelection::STRUCTURE:
      return true;
  }

  // should not get here. however, compiler warns about it
  TRI_ASSERT(false);
  return false;
}

bool Maskings::shouldDumpData(std::string const& name) {
  auto const itr = _collections.find(name);

  if (itr == _collections.end()) {
    return false;
  }

  switch (itr->second.selection()) {
    case CollectionSelection::FULL:
      return true;
    case CollectionSelection::MASKED:
      return true;
    case CollectionSelection::EXCLUDE:
      return false;
    case CollectionSelection::STRUCTURE:
      return false;
  }

  // should not get here. however, compiler warns about it
  TRI_ASSERT(false);
  return false;
}

VPackValue Maskings::maskedItem(Collection& collection, std::vector<std::string>& path,
                                std::string& buffer, VPackSlice const& data) {
  static std::string xxxx("xxxx");

  if (path.size() == 1) {
    if (path[0] == "_key" || path[0] == "_id" || path[0] == "_rev") {
      if (data.isString()) {
        velocypack::ValueLength length;
        char const* c = data.getString(length);
        buffer = std::string(c, length);
        return VPackValue(buffer);
      } else if (data.isInteger()) {
        return VPackValue(data.getInt());
      }
    }
  }

  MaskingFunction* func = collection.masking(path);

  if (func == nullptr) {
    if (data.isBool()) {
      return VPackValue(data.getBool());
    } else if (data.isString()) {
      velocypack::ValueLength length;
      char const* c = data.getString(length);
      buffer = std::string(c, length);
      return VPackValue(buffer);
    } else if (data.isInteger()) {
      return VPackValue(data.getInt());
    } else if (data.isDouble()) {
      return VPackValue(data.getDouble());
    } else {
      return VPackValue(VPackValueType::Null);
    }
  } else {
    if (data.isBool()) {
      return func->mask(data.getBool());
    } else if (data.isString()) {
      velocypack::ValueLength length;
      char const* c = data.getString(length);
      return func->mask(std::string(c, length), buffer);
    } else if (data.isInteger()) {
      return func->mask(data.getInt());
    } else if (data.isDouble()) {
      return func->mask(data.getDouble());
    } else {
      return VPackValue(VPackValueType::Null);
    }
  }

  return VPackValue(xxxx);
}

void Maskings::addMaskedArray(Collection& collection, VPackBuilder& builder,
                              std::vector<std::string>& path, VPackSlice const& data) {
  for (auto const& entry : VPackArrayIterator(data)) {
    if (entry.isObject()) {
      VPackObjectBuilder ob(&builder);
      addMaskedObject(collection, builder, path, entry);
    } else if (entry.isArray()) {
      VPackArrayBuilder ap(&builder);
      addMaskedArray(collection, builder, path, entry);
    } else {
      std::string buffer;
      builder.add(maskedItem(collection, path, buffer, entry));
    }
  }
}

void Maskings::addMaskedObject(Collection& collection, VPackBuilder& builder,
                               std::vector<std::string>& path, VPackSlice const& data) {
  for (auto const& entry : VPackObjectIterator(data, false)) {
    std::string key = entry.key.copyString();
    VPackSlice const& value = entry.value;

    path.push_back(key);

    if (value.isObject()) {
      VPackObjectBuilder ob(&builder, key);
      addMaskedObject(collection, builder, path, value);
    } else if (value.isArray()) {
      VPackArrayBuilder ap(&builder, key);
      addMaskedArray(collection, builder, path, value);
    } else {
      std::string buffer;
      builder.add(key, maskedItem(collection, path, buffer, value));
    }

    path.pop_back();
  }
}

void Maskings::addMasked(Collection& collection, VPackBuilder& builder,
                         VPackSlice const& data) {
  if (!data.isObject()) {
    return;
  }

  std::vector<std::string> path;
  std::string dataStr("data");
  VPackObjectBuilder ob(&builder, dataStr);

  addMaskedObject(collection, builder, path, data);
}

void Maskings::addMasked(Collection& collection, basics::StringBuffer& data,
                         VPackSlice const& slice) {
  if (!slice.isObject()) {
    return;
  }

  velocypack::StringRef dataStrRef("data");

  VPackBuilder builder;

  {
    VPackObjectBuilder ob(&builder);

    for (auto const& entry : VPackObjectIterator(slice, false)) {
      velocypack::StringRef key = entry.key.stringRef();

      if (key.equals(dataStrRef)) {
        addMasked(collection, builder, entry.value);
      } else {
        builder.add(key, entry.value);
      }
    }
  }

  std::string masked = builder.toJson();
  data.appendText(masked);
  data.appendText("\n");
}

void Maskings::mask(std::string const& name, basics::StringBuffer const& data,
                    basics::StringBuffer& result) {
  result.clear();

  auto const itr = _collections.find(name);

  if (itr == _collections.end()) {
    result.copy(data);
    return;
  }

  if (itr->second.selection() == CollectionSelection::FULL) {
    result.copy(data);
    return;
  }

  result.reserve(data.length());

  char const* p = data.c_str();
  char const* e = p + data.length();
  char const* q = p;

  while (p < e) {
    while (p < e && (*p != '\n' && *p != '\r')) {
      ++p;
    }

    std::shared_ptr<VPackBuilder> builder = VPackParser::fromJson(q, p - q);

    addMasked(itr->second, result, builder->slice());

    while (p < e && (*p == '\n' || *p == '\r')) {
      ++p;
    }

    q = p;
  }
}
