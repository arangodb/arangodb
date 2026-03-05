////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Auth/Rbac/Backend.h"

#include "Inspection/Status.h"
#include "Network/Methods.h"

#include <string>
#include <vector>

namespace arangodb {
namespace rbac {

struct BackendImpl : Backend {
  BackendImpl(network::ConnectionPool& pool, std::string authorizationEndpoint);
  BackendImpl(network::Sender sendRequest, std::string authorizationEndpoint);

  auto evaluateTokenMany(JwtToken const& token, RequestItems const& items)
      -> futures::Future<ResultT<EvaluateResponseMany>> override;
  auto evaluateMany(PlainUser const& user, RequestItems const& items)
      -> futures::Future<ResultT<EvaluateResponseMany>> override;

 protected:
  auto sendRequest(arangodb::fuerte::RestVerb type, std::string path,
                   std::string_view payload)
      -> futures::Future<network::Response>;

 private:
  network::Sender _sendRequest;
  std::string _authorizationEndpoint;
};

// Transformer that maps a flat std::vector<std::string> to the nested JSON
// shape: { "parameters": { "attribute": { "values": [...] } } }
// This is used for the "context" field of RequestItem.
struct AttributeValuesTransformer {
  // { "values": [...] }
  struct Values {
    std::vector<std::string> values;
    template<class Inspector>
    friend auto inspect(Inspector& f, Values& x) {
      return f.object(x).fields(f.field("values", x.values));
    }
  };
  //{ "attribute": { "values": [...] } }
  struct Attribute {
    Values attribute;
    template<class Inspector>
    friend auto inspect(Inspector& f, Attribute& x) {
      return f.object(x).fields(f.field("attribute", x.attribute));
    }
  };
  // { "parameters": { "attribute": { "values": [...] } } }
  struct Parameters {
    Attribute parameters;
    template<class Inspector>
    friend auto inspect(Inspector& f, Parameters& x) {
      return f.object(x).fields(f.field("parameters", x.parameters));
    }
  };

  using MemoryType = std::vector<std::string>;
  using SerializedType = Parameters;

  inspection::Status toSerialized(MemoryType const& src,
                                  SerializedType& dst) const {
    dst.parameters.attribute.values = src;
    return {};
  }
  inspection::Status fromSerialized(SerializedType const& src,
                                    MemoryType& dst) const {
    dst = src.parameters.attribute.values;
    return {};
  }
};

template<class Inspector>
auto inspect(Inspector& f, Backend::RequestItem& v) {
  return f.object(v).fields(f.field("action", v.action),
                            f.field("resource", v.resource),
                            f.field("context", v.attributeValues)
                                .transformWith(AttributeValuesTransformer{}));
}

// Transformer that maps Backend::Effect ↔ "Allow"/"Deny" string.
struct EffectTransformer {
  using MemoryType = Backend::Effect;
  using SerializedType = std::string;

  inspection::Status toSerialized(MemoryType const& src,
                                  SerializedType& dst) const {
    dst = (src == MemoryType::Allow) ? "Allow" : "Deny";
    return {};
  }
  inspection::Status fromSerialized(SerializedType const& src,
                                    MemoryType& dst) const {
    if (src == "Allow") {
      dst = MemoryType::Allow;
      return {};
    }
    if (src == "Deny") {
      dst = MemoryType::Deny;
      return {};
    }
    return {"unknown effect value: " + src};
  }
};

template<class Inspector>
auto inspect(Inspector& f, Backend::ResponseItem& v) {
  return f.object(v).fields(
      f.field("effect", v.effect).transformWith(EffectTransformer{}),
      f.field("message", v.message).fallback(std::string{}));
}

template<class Inspector>
auto inspect(Inspector& f, Backend::EvaluateResponseMany& v) {
  return f.object(v).fields(
      f.field("effect", v.effect).transformWith(EffectTransformer{}),
      f.field("message", v.message).fallback(std::string{}),
      f.field("items", v.items));
}

// Request body for POST /_integration/authorization/v1/evaluate-many
struct EvaluateManyRequest {
  std::string user;
  std::vector<std::string> roles;
  std::vector<Backend::RequestItem> items;
};

template<class Inspector>
auto inspect(Inspector& f, EvaluateManyRequest& v) {
  return f.object(v).fields(f.field("user", v.user), f.field("roles", v.roles),
                            f.field("items", v.items));
}

// Request body for POST /_integration/authorization/v1/evaluate-token-many
struct EvaluateTokenManyRequest {
  std::string token;
  std::vector<Backend::RequestItem> items;
};

template<class Inspector>
auto inspect(Inspector& f, EvaluateTokenManyRequest& v) {
  return f.object(v).fields(f.field("token", v.token),
                            f.field("items", v.items));
}

}  // namespace rbac
}  // namespace arangodb
