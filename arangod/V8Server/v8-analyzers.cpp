////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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

#include "v8-analyzers.h"
#include "Basics/StringUtils.h"
#include "IResearch/IResearchAnalyzerFeature.h"
#include "IResearch/VelocyPackHelper.h"
#include "RestServer/SystemDatabaseFeature.h"
#include "Transaction/V8Context.h"
#include "V8/v8-conv.h"
#include "V8/v8-globals.h"
#include "V8/v8-vpack.h"
#include "V8Server/v8-externals.h"
#include "V8Server/v8-vocbaseprivate.h"
#include "VocBase/vocbase.h"
#include "velocypack/Parser.h"
#include "velocypack/StringRef.h"

namespace {

////////////////////////////////////////////////////////////////////////////////
/// @brief unwraps an analyser wrapped via WrapAnalyzer(...)
/// @return collection or nullptr on failure
////////////////////////////////////////////////////////////////////////////////
arangodb::iresearch::IResearchAnalyzerFeature::AnalyzerPool* UnwrapAnalyzer( // unwrap
   v8::Isolate* isolate, // isolate
    v8::Local<v8::Object> const& holder // holder
) {
  return TRI_UnwrapClass<arangodb::iresearch::IResearchAnalyzerFeature::AnalyzerPool>( // unwrap class
    holder, WRP_IRESEARCH_ANALYZER_TYPE, TRI_IGETC // args
  );
}

////////////////////////////////////////////////////////////////////////////////
/// @brief wraps an Analyzer
////////////////////////////////////////////////////////////////////////////////
v8::Handle<v8::Object> WrapAnalyzer( // wrap analyzer
    v8::Isolate* isolate, // isolate
    arangodb::iresearch::IResearchAnalyzerFeature::AnalyzerPool::ptr const& analyzer // analyzer
) {
  v8::EscapableHandleScope scope(isolate);
  TRI_GET_GLOBALS();
  TRI_GET_GLOBAL(IResearchAnalyzerTempl, v8::ObjectTemplate);
  auto result = IResearchAnalyzerTempl->NewInstance();

  if (result.IsEmpty()) {
    return scope.Escape<v8::Object>(result);
  }

  auto itr = TRI_v8_global_t::SharedPtrPersistent::emplace(*isolate, analyzer);
  auto& entry = itr.first;

  result->SetInternalField( // required for TRI_UnwrapClass(...)
    SLOT_CLASS_TYPE, v8::Integer::New(isolate, WRP_IRESEARCH_ANALYZER_TYPE) // args
  );
  result->SetInternalField(SLOT_CLASS, entry.get());

  return scope.Escape<v8::Object>(result);
}

void JS_AnalyzerFeatures(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);
  auto* analyzer = UnwrapAnalyzer(isolate, args.Holder());

  if (!analyzer) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract analyzer");
  }

  // ...........................................................................
  // end of parameter parsing
  // ...........................................................................

  if (!arangodb::iresearch::IResearchAnalyzerFeature::canUse(analyzer->name(), arangodb::auth::Level::RO)) {
    TRI_V8_THROW_EXCEPTION_MESSAGE( // exception
      TRI_ERROR_FORBIDDEN, // code
      "insufficient rights to get analyzer" // message
    );
  }

  try {
    auto i = 0;
    auto result = v8::Array::New(isolate);

    for (auto& feature: analyzer->features()) {
      if (feature) { // valid
        if (feature->name().null()) {
          result->Set(i++, v8::Null(isolate));
        } else {
          result->Set( // set value
            i++, TRI_V8_STD_STRING(isolate, std::string(feature->name())) // args
          );
        }
      }
    }

    TRI_V8_RETURN(result);
  } catch (arangodb::basics::Exception const& ex) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(ex.code(), ex.what());
  } catch (std::exception const& ex) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, ex.what());
  } catch (...) {
    TRI_V8_THROW_EXCEPTION_MESSAGE( // exception
      TRI_ERROR_INTERNAL, // code
      "cannot access analyzer features" // message
    );
  }

  TRI_V8_TRY_CATCH_END
}

void JS_AnalyzerName(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);
  auto* analyzer = UnwrapAnalyzer(isolate, args.Holder());

  if (!analyzer) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract analyzer");
  }

  // ...........................................................................
  // end of parameter parsing
  // ...........................................................................

  if (!arangodb::iresearch::IResearchAnalyzerFeature::canUse(analyzer->name(), arangodb::auth::Level::RO)) {
    TRI_V8_THROW_EXCEPTION_MESSAGE( // exception
      TRI_ERROR_FORBIDDEN, // code
      "insufficient rights to get analyzer" // message
    );
  }

  try {
    auto result = TRI_V8_STD_STRING(isolate, analyzer->name());

    TRI_V8_RETURN(result);
  } catch (arangodb::basics::Exception const& ex) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(ex.code(), ex.what());
  } catch (std::exception const& ex) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, ex.what());
  } catch (...) {
    TRI_V8_THROW_EXCEPTION_MESSAGE( // exception
      TRI_ERROR_INTERNAL, // code
      "cannot access analyzer name" // message
    );
  }

  TRI_V8_TRY_CATCH_END
}

void JS_AnalyzerProperties(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);
  auto* analyzer = UnwrapAnalyzer(isolate, args.Holder());

  if (!analyzer) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract analyzer");
  }

  // ...........................................................................
  // end of parameter parsing
  // ...........................................................................

  if (!arangodb::iresearch::IResearchAnalyzerFeature::canUse(analyzer->name(), arangodb::auth::Level::RO)) {
    TRI_V8_THROW_EXCEPTION_MESSAGE( // exception
      TRI_ERROR_FORBIDDEN, // code
      "insufficient rights to get analyzer" // message
    );
  }

  try {
    if (analyzer->properties().null()) {
      TRI_V8_RETURN(v8::Null(isolate));
    }

    try {
      auto& properties = analyzer->properties();
      auto json = arangodb::velocypack::Parser::fromJson( // json properties object
        properties.c_str(), properties.size() // args
      );
      auto slice = json->slice();

      if (slice.isObject()) {
        auto result = TRI_VPackToV8(isolate, slice);

        TRI_V8_RETURN( // return as json
          result->ToObject(TRI_IGETC).FromMaybe(v8::Local<v8::Object>()) // args
        );
      }
    } catch (...) { // parser may throw exceptions for valid properties
      // NOOP
    }

    auto result = // result
      TRI_V8_STD_STRING(isolate, std::string(analyzer->properties()));

    TRI_V8_RETURN(result);
  } catch (arangodb::basics::Exception const& ex) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(ex.code(), ex.what());
  } catch (std::exception const& ex) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, ex.what());
  } catch (...) {
    TRI_V8_THROW_EXCEPTION_MESSAGE( // exception
      TRI_ERROR_INTERNAL, // code
      "cannot access analyzer properties" // message
    );
  }

  TRI_V8_TRY_CATCH_END
}

void JS_AnalyzerType(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);
  auto* analyzer = UnwrapAnalyzer(isolate, args.Holder());

  if (!analyzer) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract analyzer");
  }

  // ...........................................................................
  // end of parameter parsing
  // ...........................................................................

  if (!arangodb::iresearch::IResearchAnalyzerFeature::canUse(analyzer->name(), arangodb::auth::Level::RO)) {
    TRI_V8_THROW_EXCEPTION_MESSAGE( // exception
      TRI_ERROR_FORBIDDEN, // code
      "insufficient rights to get analyzer" // message
    );
  }

  try {
    if (analyzer->type().null()) {
      TRI_V8_RETURN(v8::Null(isolate));
    }

    auto result = TRI_V8_STD_STRING(isolate, std::string(analyzer->type()));

    TRI_V8_RETURN(result);
  } catch (arangodb::basics::Exception const& ex) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(ex.code(), ex.what());
  } catch (std::exception const& ex) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, ex.what());
  } catch (...) {
    TRI_V8_THROW_EXCEPTION_MESSAGE( // exception
      TRI_ERROR_INTERNAL, // code
      "cannot access analyzer type" // message
    );
  }

  TRI_V8_TRY_CATCH_END
}

void JS_Create(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);
  auto& vocbase = GetContextVocBase(isolate);

  if (vocbase.isDangling()) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  // we require at least 2 but no more than 4 arguments
  // save(name: <string>, type: <string>[, parameters: <json>[, features: <string-array>]]);
  if (args.Length() < 2 // too few args
      || args.Length() > 4 // too many args
      || !args[0]->IsString() // not a string
      || !args[1]->IsString() // not a string
     ) {
    TRI_V8_THROW_EXCEPTION_USAGE("save(<name>, <type>[, <properties>[, <features>]])");
  }

  PREVENT_EMBEDDED_TRANSACTION();

  auto* analyzers = arangodb::application_features::ApplicationServer::getFeature<
    arangodb::iresearch::IResearchAnalyzerFeature
  >();

  TRI_ASSERT(analyzers);

  auto* sysDatabase = arangodb::application_features::ApplicationServer::lookupFeature<
    arangodb::SystemDatabaseFeature
  >();
  auto sysVocbase = sysDatabase ? sysDatabase->use() : nullptr;

  auto name = TRI_ObjectToString(isolate, args[0]);

  if (!TRI_vocbase_t::IsAllowedName(false, arangodb::velocypack::StringRef(name))) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(
      TRI_ERROR_BAD_PARAMETER,
      "invalid characters in analyzer name '" + name + "'"
    );

    return;
  }

  std::string nameBuf;

  if (sysVocbase) {
    nameBuf = arangodb::iresearch::IResearchAnalyzerFeature::normalize( // normalize
      name, vocbase, *sysVocbase // args
    );
    name = nameBuf;
  };

  auto type = TRI_ObjectToString(isolate, args[1]);

  irs::string_ref properties;
  std::string propertiesBuf;

  if (args.Length() > 2) { // have properties
    if (args[2]->IsString()) {
      propertiesBuf = TRI_ObjectToString(isolate, args[2]);
      properties = propertiesBuf;
    } else if (args[2]->IsObject()) {
      auto value = // value
        args[2]->ToObject(TRI_IGETC).FromMaybe(v8::Local<v8::Object>());
      arangodb::velocypack::Builder builder;
      auto res = TRI_V8ToVPack(isolate, builder, value, false);

      if (TRI_ERROR_NO_ERROR != res) {
        TRI_V8_THROW_EXCEPTION(res);
      }

      propertiesBuf = builder.toString();
      properties = propertiesBuf;
    } else if (!args[2]->IsNull()) {
      TRI_V8_THROW_TYPE_ERROR("<properties> must be an object");
    }
  }

  irs::flags features;

  if (args.Length() > 3) { // have features
    if (!args[3]->IsArray()) {
      TRI_V8_THROW_TYPE_ERROR("<features> must be an array");
    }

    auto value = v8::Local<v8::Array>::Cast(args[3]);

    for (uint32_t i = 0, count = value->Length(); i < count;  ++i) {
      auto subValue = value->Get(i);

      if (!subValue->IsString()) {
        TRI_V8_THROW_TYPE_ERROR("<feature> must be a string");
      }

      auto* feature = // feature
        irs::attribute::type_id::get(TRI_ObjectToString(isolate, subValue));

      if (!feature) {
        TRI_V8_THROW_TYPE_ERROR("<feature> not supported");
      }

      features.add(*feature);
    }
  }

  // ...........................................................................
  // end of parameter parsing
  // ...........................................................................

  if (!arangodb::iresearch::IResearchAnalyzerFeature::canUse(name, arangodb::auth::Level::RW)) {
    TRI_V8_THROW_EXCEPTION_MESSAGE( // exception
      TRI_ERROR_FORBIDDEN, // code
      "insufficient rights to create analyzer" // message
    );
  }

  try {
    arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
    auto res = analyzers->emplace(result, name, type, properties, features);

    if (!res.ok()) {
      TRI_V8_THROW_EXCEPTION(res);
    }

    if (!result.first) {
      TRI_V8_THROW_EXCEPTION_MESSAGE( // exception
        TRI_ERROR_INTERNAL, // code
        "problem creating view" // message
      );
    }

    auto v8Result = WrapAnalyzer(isolate, result.first);

    if (v8Result.IsEmpty()) {
      TRI_V8_THROW_EXCEPTION_MEMORY();
    }

    TRI_V8_RETURN(v8Result);
  } catch (arangodb::basics::Exception const& ex) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(ex.code(), ex.what());
  } catch (std::exception const& ex) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, ex.what());
  } catch (...) {
    TRI_V8_THROW_EXCEPTION_MESSAGE( // exception
      TRI_ERROR_INTERNAL, // code
      "cannot create analyzer" // message
    );
  }

  TRI_V8_TRY_CATCH_END
}

void JS_Get(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);
  auto& vocbase = GetContextVocBase(isolate);

  if (vocbase.isDropped()) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  // expecting one argument
  // analyzer(name: <string>);
  if (args.Length() != 1 || !args[0]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE("analyzer(<name>)");
  }

  PREVENT_EMBEDDED_TRANSACTION();

  auto* analyzers = arangodb::application_features::ApplicationServer::getFeature<
    arangodb::iresearch::IResearchAnalyzerFeature
  >();

  TRI_ASSERT(analyzers);

  auto* sysDatabase = arangodb::application_features::ApplicationServer::lookupFeature< // find feature
    arangodb::SystemDatabaseFeature // featue type
  >();
  auto sysVocbase = sysDatabase ? sysDatabase->use() : nullptr;

  auto name = TRI_ObjectToString(isolate, args[0]);
  std::string nameBuf;

  if (sysVocbase) {
    nameBuf = arangodb::iresearch::IResearchAnalyzerFeature::normalize( // normalize
      name, vocbase, *sysVocbase // args
    );
    name = nameBuf;
  };

  // ...........................................................................
  // end of parameter parsing
  // ...........................................................................

  if (!arangodb::iresearch::IResearchAnalyzerFeature::canUse(name, arangodb::auth::Level::RO)) {
    TRI_V8_THROW_EXCEPTION_MESSAGE( // exception
      TRI_ERROR_FORBIDDEN, // code
      "insufficient rights to get analyzer" // message
    );
  }

  try {
    auto analyzer = analyzers->get(name);

    if (!analyzer) {
      TRI_V8_RETURN_NULL();
    }

    auto result = WrapAnalyzer(isolate, analyzer);

    if (result.IsEmpty()) {
      TRI_V8_THROW_EXCEPTION_MEMORY();
    }

    TRI_V8_RETURN(result);
  } catch (arangodb::basics::Exception const& ex) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(ex.code(), ex.what());
  } catch (std::exception const& ex) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, ex.what());
  } catch (...) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "cannot get analyzer");
  }

  TRI_V8_TRY_CATCH_END
}

void JS_List(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);
  auto& vocbase = GetContextVocBase(isolate);

  if (vocbase.isDropped()) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  auto* analyzers = arangodb::application_features::ApplicationServer::getFeature<
    arangodb::iresearch::IResearchAnalyzerFeature
  >();

  TRI_ASSERT(analyzers);

  auto* sysDatabase = arangodb::application_features::ApplicationServer::lookupFeature< // find feature
    arangodb::SystemDatabaseFeature // featue type
  >();
  auto sysVocbase = sysDatabase ? sysDatabase->use() : nullptr;

  // ...........................................................................
  // end of parameter parsing
  // ...........................................................................

  typedef arangodb::iresearch::IResearchAnalyzerFeature::AnalyzerPool::ptr AnalyzerPoolPtr;
  static const auto nameLess = []( // analyzer name less
    AnalyzerPoolPtr const& lhs, // left hand side
    AnalyzerPoolPtr const& rhs // right hand side
  )->bool {
    return arangodb::basics::StringUtils::tolower(lhs->name()) < arangodb::basics::StringUtils::tolower(rhs->name());
  };
  std::vector<AnalyzerPoolPtr> result;
  auto visitor = [&result](AnalyzerPoolPtr const& analyzer)->bool {
    if (analyzer) {
      result.emplace_back(analyzer);
    }

    return true; // continue with next analyzer
  };

  try {
    analyzers->visit(visitor, nullptr); // include static analyzers

    if (arangodb::iresearch::IResearchAnalyzerFeature::canUse(vocbase, arangodb::auth::Level::RO)) {
      analyzers->visit(visitor, &vocbase);
    }

    // include analyzers from the system vocbase if possible
    if (sysVocbase // have system vocbase
       && sysVocbase->name() != vocbase.name() // not same vocbase as current
       && arangodb::iresearch::IResearchAnalyzerFeature::canUse(*sysVocbase, arangodb::auth::Level::RO)) {
      analyzers->visit(visitor, sysVocbase.get());
    }

    std::sort(result.begin(), result.end(), nameLess);

    auto v8Result = v8::Array::New(isolate);

    for (size_t i = 0, count = result.size(); i < count; ++i) {
      auto analyzer = WrapAnalyzer(isolate, result[i]);

      if (analyzer.IsEmpty() || i > std::numeric_limits<uint32_t>::max()) {
        TRI_V8_THROW_EXCEPTION_MEMORY();
      }

      v8Result->Set(static_cast<uint32_t>(i), analyzer); // cast safe because of check above
    }

    TRI_V8_RETURN(v8Result);
  } catch (arangodb::basics::Exception const& ex) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(ex.code(), ex.what());
  } catch (std::exception const& ex) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, ex.what());
  } catch (...) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "cannot list analyzers");
  }

  TRI_V8_TRY_CATCH_END
}

void JS_Remove(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);
  auto& vocbase = GetContextVocBase(isolate);

  if (vocbase.isDangling()) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  // we require 1 string argument and an optional boolean argument
  // remove(name: <string>[, force: <bool>])
  if (args.Length() < 1 // too few args
      || args.Length() > 2 // too many args
      || !args[0]->IsString() // not a string
      ) {
    TRI_V8_THROW_EXCEPTION_USAGE("remove(<name> [, <force>])");
  }

  PREVENT_EMBEDDED_TRANSACTION();

  auto* analyzers = arangodb::application_features::ApplicationServer::getFeature<
    arangodb::iresearch::IResearchAnalyzerFeature
  >();

  TRI_ASSERT(analyzers);

  auto* sysDatabase = arangodb::application_features::ApplicationServer::lookupFeature< // find feature
    arangodb::SystemDatabaseFeature // featue type
  >();
  auto sysVocbase = sysDatabase ? sysDatabase->use() : nullptr;

  auto name = TRI_ObjectToString(isolate, args[0]);
  std::string nameBuf;

  if (sysVocbase) {
    nameBuf = arangodb::iresearch::IResearchAnalyzerFeature::normalize( // normalize
      name, vocbase, *sysVocbase // args
    );
    name = nameBuf;
  };

  bool force = false;

  if (args.Length() > 1) {
    if (!args[1]->IsBoolean() && !args[1]->IsBooleanObject()) {
      TRI_V8_THROW_TYPE_ERROR("<force> must be a boolean");
    }

    force = TRI_ObjectToBoolean(isolate, args[1]);
  }

  // ...........................................................................
  // end of parameter parsing
  // ...........................................................................

  if (!arangodb::iresearch::IResearchAnalyzerFeature::canUse(name, arangodb::auth::Level::RW)) {
    TRI_V8_THROW_EXCEPTION_MESSAGE( // exception
      TRI_ERROR_FORBIDDEN, // code
      "insufficient rights to remove analyzer" // message
    );
  }

  try {
    auto res = analyzers->remove(name, force);

    if (!res.ok()) {
      TRI_V8_THROW_EXCEPTION(res);
    }

    TRI_V8_RETURN_UNDEFINED();
  } catch (arangodb::basics::Exception const& ex) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(ex.code(), ex.what());
  } catch (std::exception const& ex) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, ex.what());
  } catch (...) {
    TRI_V8_THROW_EXCEPTION_MESSAGE( // exception
      TRI_ERROR_INTERNAL, // code
      "cannot remove analyzer" // message
    );
  }

  TRI_V8_TRY_CATCH_END
}

}

namespace arangodb {
namespace iresearch {

void TRI_InitV8Analyzers(TRI_v8_global_t& v8g, v8::Isolate* isolate) {
  // 'analyzers' feature functions
  {
    auto fnTemplate = v8::FunctionTemplate::New(isolate);

    fnTemplate->SetClassName(TRI_V8_ASCII_STRING(isolate, "ArangoAnalyzersCtor"));

    auto objTemplate = fnTemplate->InstanceTemplate();

    objTemplate->SetInternalFieldCount(0);
    TRI_AddMethodVocbase(isolate, objTemplate, TRI_V8_ASCII_STRING(isolate, "analyzer"), JS_Get);
    TRI_AddMethodVocbase(isolate, objTemplate, TRI_V8_ASCII_STRING(isolate, "remove"), JS_Remove);
    TRI_AddMethodVocbase(isolate, objTemplate, TRI_V8_ASCII_STRING(isolate, "save"), JS_Create);
    TRI_AddMethodVocbase(isolate, objTemplate, TRI_V8_ASCII_STRING(isolate, "toArray"), JS_List);

    v8g.IResearchAnalyzersTempl.Reset(isolate, objTemplate);

    auto instance = objTemplate->NewInstance();

    // register the global object accessable via JavaScipt
    if (!instance.IsEmpty()) {
      TRI_AddGlobalVariableVocbase(isolate, TRI_V8_ASCII_STRING(isolate, "ArangoAnalyzers"), instance);
    }
  }

  // individual analyzer functions
  {
    auto fnTemplate = v8::FunctionTemplate::New(isolate);

    fnTemplate->SetClassName(TRI_V8_ASCII_STRING(isolate, "ArangoAnalyzer"));

    auto objTemplate = fnTemplate->InstanceTemplate();

    objTemplate->SetInternalFieldCount(2); // SLOT_CLASS_TYPE + SLOT_CLASS
    TRI_AddMethodVocbase(isolate, objTemplate, TRI_V8_ASCII_STRING(isolate, "features"), JS_AnalyzerFeatures);
    TRI_AddMethodVocbase(isolate, objTemplate, TRI_V8_ASCII_STRING(isolate, "name"), JS_AnalyzerName);
    TRI_AddMethodVocbase(isolate, objTemplate, TRI_V8_ASCII_STRING(isolate, "properties"), JS_AnalyzerProperties);
    TRI_AddMethodVocbase(isolate, objTemplate, TRI_V8_ASCII_STRING(isolate, "type"), JS_AnalyzerType);

    v8g.IResearchAnalyzerTempl.Reset(isolate, objTemplate);
    TRI_AddGlobalFunctionVocbase( // required only for pretty-printing via JavaScript (must to be defined AFTER v8g.IResearchAnalyzerTempl.Reset(...))
      isolate, // isolate
      TRI_V8_ASCII_STRING(isolate, "ArangoAnalyzer"), // name
      fnTemplate->GetFunction() // impl
    );
  }
}

} // iresearch
} // arangodb

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
