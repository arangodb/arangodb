////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// @author Kaveh Vahedipour
////////////////////////////////////////////////////////////////////////////////

#include "Store.h"

#include "Agency/Agent.h"
#include "Agency/Node.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/ReadLocker.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/WriteLocker.h"
#include "Basics/debugging.h"
#include "Logger/LogMacros.h"
#include "RestServer/arangod.h"
#include "Network/Methods.h"
#include "Network/NetworkFeature.h"

#include <velocypack/Buffer.h>
#include <velocypack/Compare.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>

#include <ctime>
#include <iomanip>
#include <string_view>

using namespace arangodb::consensus;
using namespace arangodb::basics;
using namespace arangodb::velocypack;

/// Ctor with name
Store::Store(std::string const& name)
    : _node(std::make_shared<Node>(name, this)) {}

/// Copy assignment operator
Store& Store::operator=(Store const& rhs) {
  if (&rhs != this) {
    std::lock_guard otherLock{rhs._storeLock};
    std::lock_guard lock{_storeLock};
    _node = rhs._node;
  }
  return *this;
}

/// Move assignment operator
Store& Store::operator=(Store&& rhs) {
  if (&rhs != this) {
    std::lock_guard otherLock{rhs._storeLock};
    std::lock_guard lock{_storeLock};
    _node = std::move(rhs._node);
  }
  return *this;
}

/// Default dtor
Store::~Store() = default;

index_t Store::applyTransactions(std::vector<log_t> const& queries) {
  std::lock_guard storeLocker{_storeLock};

  for (auto const& query : queries) {
    applies(Slice(query.entry->data()));
  }
  return queries.empty() ? 0 : queries.back().index;
}

/// Apply array of transactions multiple queries to store
/// Return vector of according success
std::vector<apply_ret_t> Store::applyTransactions(
    VPackSlice query, Agent::WriteMode const& wmode) {
  std::vector<apply_ret_t> success;

  if (query.isArray()) {
    try {
      for (auto const& i : VPackArrayIterator(query)) {
        if (!wmode.privileged()) {
          bool found = false;
          for (auto const& o : VPackObjectIterator(i[0])) {
            size_t pos = o.key.stringView().find(RECONFIGURE);
            if (pos != std::string::npos && (pos == 0 || pos == 1)) {
              found = true;
              break;
            }
          }
          if (found) {
            success.push_back(FORBIDDEN);
            continue;
          }
        }

        std::lock_guard storeLocker{_storeLock};
        switch (i.length()) {
          case 1:  // No precondition
            success.push_back(applies(i[0]) ? APPLIED : UNKNOWN_ERROR);
            break;
          case 2:  // precondition + uuid
          case 3:
            if (check(i[1]).successful()) {
              success.push_back(applies(i[0]) ? APPLIED : UNKNOWN_ERROR);
            } else {  // precondition failed
              LOG_TOPIC("f6873", TRACE, Logger::AGENCY)
                  << "Precondition failed!";
              success.push_back(PRECONDITION_FAILED);
            }
            break;
          default:  // Wrong
            LOG_TOPIC("795d6", ERR, Logger::AGENCY)
                << "We can only handle log entry with or without precondition! "
                << " however, We received " << i.toJson();
            success.push_back(UNKNOWN_ERROR);
            break;
        }
      }
    } catch (std::exception const& e) {  // Catch any errors
      LOG_TOPIC("8264b", ERR, Logger::AGENCY) << e.what();
      success.push_back(UNKNOWN_ERROR);
    }

  } else {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_AGENCY_MALFORMED_TRANSACTION,
                                   "Agency request syntax is [[<queries>]]");
  }

  return success;
}

/// Apply single transaction
check_ret_t Store::applyTransaction(VPackSlice query) {
  check_ret_t ret(true);

  std::lock_guard storeLocker{_storeLock};

  if (query.isObject()) {
    ret.successful(applies(query.get("query")));
  } else {
    switch (query.length()) {
      case 1:  // No precondition
        ret.successful(applies(query[0]));
        break;
      case 2:  // precondition
      case 3:  // precondition + clientId
        ret = check(query[1], CheckMode::FULL);
        if (ret.successful()) {
          ret.successful(applies(query[0]));
        } else {  // precondition failed
          LOG_TOPIC("ded9e", TRACE, Logger::AGENCY) << "Precondition failed!";
        }
        break;
      default:  // Wrong
        LOG_TOPIC("18f6d", ERR, Logger::AGENCY)
            << "We can only handle log entry with or without precondition! "
            << "However we received " << query.toJson();
        break;
    }
  }

  return ret;
}

/// template<class T, class U> std::multimap<std::string, std::string>
std::ostream& operator<<(
    std::ostream& os,
    std::unordered_multimap<std::string, std::string> const& m) {
  for (auto const& i : m) {
    os << i.first << ": " << i.second << std::endl;
  }

  return os;
}

/// Notification type
struct notify_t {
  std::string key;
  std::string modified;
  std::string oper;
  notify_t(std::string const& k, std::string m, std::string o)
      : key(k), modified(std::move(m)), oper(std::move(o)) {}
};

/// Apply (from logs)
std::vector<bool> Store::applyLogEntries(
    arangodb::velocypack::Builder const& queries, index_t index, term_t term) {
  std::vector<bool> applied;

  // Apply log entries
  {
    std::lock_guard storeLocker{_storeLock};

    for (auto it : VPackArrayIterator(queries.slice())) {
      applied.push_back(applies(it.value()));
    }
  }

  return applied;
}

/// Check precodition object
check_ret_t Store::check(VPackSlice slice, CheckMode mode) const {
  TRI_ASSERT(slice.isObject());
  check_ret_t ret;
  ret.open();

  for (auto const& precond : VPackObjectIterator(slice)) {  // Preconditions
    std::vector<std::string> pv = split(precond.key.stringView());

    Node const* node = &Node::dummyNode();

    // Check is guarded in ::apply
    bool found = false;
    if (auto n = _node->get(pv); n) {
      found = true;
      node = n;
    }

    if (precond.value.isObject()) {
      for (auto const& op : VPackObjectIterator(precond.value)) {
        std::string_view oper = op.key.stringView();
        if (oper == "old") {  // old
          if (*node != op.value) {
            ret.push_back(precond.key);
            if (mode == FIRST_FAIL) {
              break;
            }
          }
        } else if (oper == "oldNot") {  // oldNot
          if (*node == op.value) {
            ret.push_back(precond.key);
            if (mode == FIRST_FAIL) {
              break;
            }
          }
        } else if (oper == "isArray") {  // isArray
          if (!op.value.isBoolean()) {
            LOG_TOPIC("4516b", ERR, Logger::AGENCY)
                << "Non boolean expression for 'isArray' precondition";
            ret.push_back(precond.key);
            if (mode == FIRST_FAIL) {
              break;
            }
          }
          bool isArray = (node->type() == LEAF && node->slice().isArray());
          if (op.value.getBool() ? !isArray : isArray) {
            ret.push_back(precond.key);
            if (mode == FIRST_FAIL) {
              break;
            }
          }
        } else if (oper == "oldEmpty") {  // isEmpty
          if (!op.value.isBoolean()) {
            LOG_TOPIC("9e1c8", ERR, Logger::AGENCY)
                << "Non boolsh expression for 'oldEmpty' precondition";
            ret.push_back(precond.key);
            if (mode == FIRST_FAIL) {
              break;
            }
          }
          if (op.value.getBool() ? found : !found) {
            ret.push_back(precond.key);
            if (mode == FIRST_FAIL) {
              break;
            }
          }
        } else if (oper == "in") {  // in
          if (found) {
            if (node->slice().isArray()) {
              bool found = false;
              for (auto const& i : VPackArrayIterator(node->slice())) {
                if (basics::VelocyPackHelper::equal(i, op.value, false)) {
                  found = true;
                  break;
                }
              }
              if (found) {
                continue;
              }
              ret.push_back(precond.key);
            }
          }
          ret.push_back(precond.key);
          if (mode == FIRST_FAIL) {
            break;
          }
        } else if (oper == "notin") {  // in
          if (!found) {
            continue;
          }
          if (node->slice().isArray()) {
            bool found = false;
            for (auto const& i : VPackArrayIterator(node->slice())) {
              if (basics::VelocyPackHelper::equal(i, op.value, false)) {
                found = true;
                break;
              }
            }
            if (!found) {
              continue;
            }
            ret.push_back(precond.key);
          }
          ret.push_back(precond.key);
          if (mode == FIRST_FAIL) {
            break;
          }
        } else if (oper == PREC_IS_WRITE_LOCKED) {  // is-write-locked
          // the lock is write locked by the given entity, if node and the value
          // are strings and both are equal.
          if (found && op.value.isString() && node->slice().isString()) {
            if (node->slice().isEqualString(op.value.stringView())) {
              continue;
            }
          }
          ret.push_back(precond.key);
          if (mode == FIRST_FAIL) {
            break;
          }
        } else if (oper == PREC_IS_READ_LOCKED) {  // is-read-locked
          // the lock is read locked by the given entity, if node is an
          // array of strings and the value is a string and if the value is
          // contained in the node array.
          if (found && op.value.isString() && node->slice().isArray()) {
            bool isValid = false;
            for (auto const& i : VPackArrayIterator(node->slice())) {
              if (!i.isString()) {
                isValid = false;
                break;  // invalid, only strings allowed
              }
              if (i.isEqualString(op.value.stringView())) {
                isValid = true;
              }
            }
            if (isValid) {
              continue;
            }
          }
          ret.push_back(precond.key);
          if (mode == FIRST_FAIL) {
            break;
          }
        } else if (oper == PREC_CAN_READ_LOCK) {  // can-read-lock
          // a lock can be read locked if the node is either not found
          // or the node is readLockable.
          if (!found) {
            continue;
          }
          if (!op.value.isString() ||
              !node->isReadLockable(op.value.stringView())) {
            ret.push_back(precond.key);
            if (mode == FIRST_FAIL) {
              break;
            }
          }
        } else if (oper == PREC_CAN_WRITE_LOCK) {  // can-write-lock
          // a lock can be write locked if the node is either not found
          // or the node is writeLockable.
          if (!found) {
            continue;
          }
          if (!op.value.isString() ||
              !node->isWriteLockable(op.value.stringView())) {
            ret.push_back(precond.key);
            if (mode == FIRST_FAIL) {
              break;
            }
          }
        } else if (oper == "intersectionEmpty") {  // in
          auto const nslice = node->slice();
          if (!op.value.isArray()) {  // right hand side must be array will
            ret.push_back(precond.key);
            if (mode == FIRST_FAIL) {
              break;
            }
          } else {
            if (!found) {
              continue;
            }
            if (nslice.isArray()) {
              bool found_ = false;
              std::unordered_set<VPackSlice,
                                 arangodb::velocypack::NormalizedCompare::Hash,
                                 arangodb::velocypack::NormalizedCompare::Equal>
                  elems;
              Slice shorter, longer;

              if (nslice.length() <= op.value.length()) {
                longer = op.value;
                shorter = nslice;
              } else {
                shorter = nslice;
                longer = op.value;
              }

              for (auto const i : VPackArrayIterator(shorter)) {
                elems.emplace(i);
              }
              for (auto const i : VPackArrayIterator(longer)) {
                if (elems.find(i) != elems.end()) {
                  found_ = true;
                  break;
                }
              }
              if (!found_) {
                continue;
              }
              ret.push_back(precond.key);
              if (mode == FIRST_FAIL) {
                break;
              }
            }
          }
        } else {
          ret.push_back(precond.key);
          if (mode == FIRST_FAIL) {
            break;
          }
          LOG_TOPIC("44419", WARN, Logger::AGENCY)
              << "Malformed object-type precondition was failed: "
              << "key: " << precond.key.toJson()
              << " value: " << precond.value.toJson();
        }
      }
    } else {
      if (*node != precond.value) {
        ret.push_back(precond.key);
        if (mode == FIRST_FAIL) {
          break;
        }
      }
    }
  }

  ret.close();
  return ret;
}

/// Read queries into result
std::vector<bool> Store::readMultiple(VPackSlice queries,
                                      Builder& result) const {
  std::vector<bool> success;
  if (queries.isArray()) {
    VPackArrayBuilder r(&result);
    for (auto const& query : VPackArrayIterator(queries)) {
      success.push_back(read(query, result));
    }
  } else {
    LOG_TOPIC("fec72", ERR, Logger::AGENCY)
        << "Read queries to stores must be arrays";
  }
  return success;
}

/// Read single query into ret
bool Store::read(VPackSlice query, Builder& ret) const {
  bool success = true;
  bool showHidden = false;

  // Collect all paths
  if (!query.isArray()) {
    return false;
  }

  std::vector<std::string> query_strs;
  for (auto const& sub_query : VPackArrayIterator(query)) {
    query_strs.emplace_back(sub_query.copyString());
    showHidden |= (query_strs.back().find('.') == 0 ||
                   (query_strs.back().find("/.") != std::string::npos));
  }

  // Remove double ranges (inclusion / identity)
  std::sort(query_strs.begin(), query_strs.end());  // sort paths
  for (auto i = query_strs.begin(), j = i; i != query_strs.end(); ++i) {
    if (i != j && i->compare(0, i->size(), *j) == 0) {
      *i = "";
    } else {
      j = i;
    }
  }
  auto cut =
      std::remove_if(query_strs.begin(), query_strs.end(),
                     [](std::string const& s) -> bool { return s.empty(); });
  query_strs.erase(cut, query_strs.end());

  // Distinguish two cases:
  //   a fast path for exactly one path, in which we do not have to copy all
  //   a slow path for more than one path

  std::lock_guard storeLocker{_storeLock};  // Freeze KV-Store for read
  if (query_strs.size() == 1) {
    auto const& path = query_strs[0];
    std::vector<std::string> pv = split(path);
    // Build surrounding object structure:
    size_t e = _node->exists(pv).size();  // note: e <= pv.size()!
    size_t i = 0;
    for (auto it = pv.begin(); i < e; ++i, ++it) {
      ret.openObject();
      ret.add(VPackValue(*it));
    }
    if (e == pv.size()) {  // existing
      _node->get(pv)->toBuilder(ret, showHidden);
    } else {
      VPackObjectBuilder guard(&ret);
    }
    // And close surrounding object structures:
    for (i = 0; i < e; ++i) {
      ret.close();
    }
  } else {  // slow path for 0 or more than 2 paths:
    // Create response tree
    Node copy("copy");
    for (auto const& path : query_strs) {
      std::vector<std::string> pv = split(path);
      size_t e = _node->exists(pv).size();
      if (e == pv.size()) {  // existing
        copy.getOrCreate(pv) = *_node->get(pv);
      } else {  // non-existing
        for (size_t i = 0; i < pv.size() - e + 1; ++i) {
          pv.pop_back();
        }
        if (copy.getOrCreate(pv).type() == LEAF &&
            copy.getOrCreate(pv).slice().isNone()) {
          copy.getOrCreate(pv) =
              arangodb::velocypack::Slice::emptyObjectSlice();
        }
      }
    }

    // Into result builder
    copy.toBuilder(ret, showHidden);
  }

  return success;
}

/// Dump internal data to builder
void Store::dumpToBuilder(Builder& builder) const {
  std::lock_guard storeLocker{_storeLock};
  toBuilder(builder, true);
}

/// Apply transaction to key value store. Guarded by caller
bool Store::applies(arangodb::velocypack::Slice const& transaction) {
  auto it = VPackObjectIterator(transaction);

  std::vector<std::string> abskeys;
  abskeys.reserve(it.size());

  std::vector<std::pair<size_t, VPackSlice>> idx;
  idx.reserve(it.size());

  size_t counter = 0;
  while (it.valid()) {
    std::string_view key = it.key().stringView();

    // push back an empty string first, so we can avoid a later move
    abskeys.emplace_back();

    // and now work on this string
    std::string& absoluteKey = abskeys.back();
    absoluteKey.reserve(key.size());

    // turn the path into an absolute path, collapsing all duplicate
    // forward slashes into a single forward slash
    char const* p = key.data();
    char const* e = key.data() + key.size();
    char last = '\0';
    if (p == e || *p != '/') {
      // key must start with '/'
      absoluteKey.push_back('/');
      last = '/';
    }
    while (p < e) {
      char current = *p;
      if (current != '/' || last != '/') {
        // avoid repeated forward slashes
        absoluteKey.push_back(current);
        last = current;
      }
      ++p;
    }

    TRI_ASSERT(!absoluteKey.empty());
    TRI_ASSERT(absoluteKey[0] == '/');
    TRI_ASSERT(absoluteKey.find("//") == std::string::npos);

    idx.emplace_back(counter++, it.value());

    it.next();
  }

  std::sort(idx.begin(), idx.end(),
            [&abskeys](std::pair<size_t, VPackSlice> const& i1,
                       std::pair<size_t, VPackSlice> const& i2) noexcept {
              return abskeys[i1.first] < abskeys[i2.first];
            });

  bool success = true;
  for (const auto& i : idx) {
    Slice value = i.second;

    if (value.isObject() && value.hasKey("op")) {
      std::string_view const op = value.get("op").stringView();

      if (op == "delete" || op == "replace" || op == "erase") {
        if (!_node->has(abskeys.at(i.first))) {
          continue;
        }
      }
      auto uri = Store::normalize(abskeys.at(i.first));
      auto ret = _node->hasAsWritableNode(abskeys.at(i.first)).applyOp(value);
      success &= ret.ok();
      // check for triggers here
      callTriggers(abskeys.at(i.first), op, value);
    } else {
      success &= _node->hasAsWritableNode(abskeys.at(i.first)).applies(value);
    }
  }

  return success;
}

// Clear my data
void Store::clear() {
  std::lock_guard storeLocker{_storeLock};
  _node->clear();
}

/// Apply a request to my key value store
Store& Store::loadFromVelocyPack(VPackSlice s) {
  TRI_ASSERT(s.isObject());
  TRI_ASSERT(s.hasKey("readDB"));
  auto const& slice = s.get("readDB");

  std::lock_guard storeLocker{_storeLock};
  if (slice.isArray()) {
    TRI_ASSERT(slice.length() >= 1);
    _node->applies(slice[0]);

  } else if (slice.isObject()) {
    _node->applies(slice);
  }
  return *this;
}

/// Put key value store in velocypack, guarded by caller
void Store::toBuilder(Builder& b, bool showHidden) const {
  _node->toBuilder(b, showHidden);
}

/// Get node at path under mutex and store it in velocypack
void Store::get(std::string const& path, arangodb::velocypack::Builder& b,
                bool showHidden) const {
  std::lock_guard storeLocker{_storeLock};
  if (auto node = _node->hasAsNode(path); node) {
    node->toBuilder(b, showHidden);
  } else {
    // Backwards compatibility of a refactoring. Would be better to communicate
    // this clearly via a return code or so.
    b.add(arangodb::velocypack::Slice::emptyObjectSlice());
  }
}

/// Get node at path under mutex
Node Store::get(std::string const& path) const {
  std::lock_guard storeLocker{_storeLock};
  return *_node->hasAsNode(path);
}

/// Get node at path under mutex
bool Store::has(std::string const& path) const {
  std::lock_guard storeLocker{_storeLock};
  return _node->has(path);
}

std::string Store::normalize(char const* key, size_t length) {
  std::string_view const path(key, length);

  std::string normalized;
  normalized.reserve(path.size() + 1);
  size_t offset = 0;

  while (true) {
    if (normalized.empty() || normalized.back() != '/') {
      normalized.push_back('/');
    }
    size_t pos = path.find('/', offset);
    if (pos != std::string::npos) {
      normalized.append(path.data() + offset, pos - offset);
      offset = pos + 1;
    } else {
      normalized.append(path.data() + offset, path.size() - offset);
      break;
    }
  }

  TRI_ASSERT(!normalized.empty());
  TRI_ASSERT(normalized.front() == '/');
  if (normalized.size() > 1 && normalized.back() == '/') {
    normalized.pop_back();
  }
  TRI_ASSERT(!normalized.empty());
  TRI_ASSERT(normalized.front() == '/');
  TRI_ASSERT(normalized.find("//") == std::string::npos);
  return normalized;
}

/**
 * @brief Unguarded pointer to a node path in this store.
 *        Caller must enforce locking.
 */
Node const* Store::nodePtr(std::string const& path) const {
  return _node->get(path);
}

void Store::callTriggers(std::string_view key, std::string_view op,
                         VPackSlice trx) {
  std::unique_lock guard(_triggersMutex);
  // search for all keys that are a prefix of key
  auto iter = _triggers.upper_bound(key);

  while (iter != _triggers.begin()) {
    iter--;
    if (key.starts_with(iter->first)) {
      // call trigger
      iter->second.callback(key, trx);
    }
  }
}

void Store::registerPrefixTrigger(std::string const& prefix,
                                  AgencyTriggerCallback cb) {
  std::unique_lock guard(_triggersMutex);
  auto normalized = normalize(prefix);
  _triggers.emplace(std::move(normalized), AgencyTrigger(std::move(cb)));
}

std::vector<std::string> Store::split(std::string_view str) {
  return Node::split(str);
}
