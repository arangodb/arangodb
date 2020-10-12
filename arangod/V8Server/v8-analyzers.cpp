////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "v8-analyzers.h"

#include <velocypack/Parser.h>
#include <velocypack/StringRef.h>

#include "Basics/StringUtils.h"
#include "Basics/StaticStrings.h"
#include "Cluster/ClusterTypes.h"
#include "Cluster/ServerState.h"
#include "IResearch/IResearchCommon.h"
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

namespace {

////////////////////////////////////////////////////////////////////////////////
/// @brief unwraps an analyser wrapped via WrapAnalyzer(...)
/// @return collection or nullptr on failure
////////////////////////////////////////////////////////////////////////////////
arangodb::iresearch::AnalyzerPool* UnwrapAnalyzer(
    v8::Isolate* isolate,
    v8::Local<v8::Object> const& holder) {
  return TRI_UnwrapClass<arangodb::iresearch::AnalyzerPool>(
    holder, WRP_IRESEARCH_ANALYZER_TYPE, TRI_IGETC);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief wraps an Analyzer
////////////////////////////////////////////////////////////////////////////////
v8::Handle<v8::Object> WrapAnalyzer(
    v8::Isolate* isolate,
    arangodb::iresearch::AnalyzerPool::ptr const& analyzer) {
  v8::EscapableHandleScope scope(isolate);
  TRI_GET_GLOBALS();
  TRI_GET_GLOBAL(IResearchAnalyzerInstanceTempl, v8::ObjectTemplate);
  auto result = IResearchAnalyzerInstanceTempl->NewInstance(TRI_IGETC).FromMaybe(v8::Local<v8::Object>());

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
  auto context = TRI_IGETC;
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
        if (feature().name().null()) {
          result->Set(context, i++, v8::Null(isolate)).FromMaybe(false);
        } else {
          result->Set(context,  // set value
                      i++, TRI_V8_STD_STRING(isolate, std::string(feature().name()))).FromMaybe(false); // args
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
    auto result = TRI_VPackToV8(isolate, analyzer->properties());

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
  auto context = TRI_IGETC;
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

  TRI_GET_GLOBALS();
  auto& analyzers =
      v8g->_server.getFeature<arangodb::iresearch::IResearchAnalyzerFeature>();

  auto nameFromArgs = TRI_ObjectToString(isolate, args[0]);
  auto splittedAnalyzerName =
    arangodb::iresearch::IResearchAnalyzerFeature::splitAnalyzerName(nameFromArgs);
  if (!arangodb::iresearch::IResearchAnalyzerFeature::analyzerReachableFromDb(
         splittedAnalyzerName.first, vocbase.name())) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(
      TRI_ERROR_FORBIDDEN,
      "Database in analyzer name does not match current database");
    return;
  }

  if (!TRI_vocbase_t::IsAllowedName(false, arangodb::velocypack::StringRef(splittedAnalyzerName.second.c_str(),
                                                                           splittedAnalyzerName.second.size()))) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(
      TRI_ERROR_BAD_PARAMETER,
      std::string("invalid characters in analyzer name '").append(splittedAnalyzerName.second.c_str()).append("'")
    );

    return;
  }

  auto name = arangodb::iresearch::IResearchAnalyzerFeature::normalize(splittedAnalyzerName.second, vocbase.name());

  auto type = TRI_ObjectToString(isolate, args[1]);

  VPackSlice propertiesSlice = VPackSlice::emptyObjectSlice();
  VPackBuilder propertiesBuilder;

  if (args.Length() > 2) { // have properties
    if (args[2]->IsString()) {
      std::string const propertiesBuf = TRI_ObjectToString(isolate, args[2]);
      arangodb::velocypack::Parser(propertiesBuilder).parse(propertiesBuf);
      propertiesSlice = propertiesBuilder.slice();
    } else if (args[2]->IsObject()) {
      auto value = args[2]->ToObject(TRI_IGETC).FromMaybe(v8::Local<v8::Object>());
      TRI_V8ToVPack(isolate, propertiesBuilder, value, false);
      propertiesSlice = propertiesBuilder.slice();
    } else if (!args[2]->IsNull()) {
      TRI_V8_THROW_TYPE_ERROR("<properties> must be an object");
    }
  }
  // properties at the end should be parsed into object
  if (!propertiesSlice.isObject()) {
    TRI_V8_THROW_TYPE_ERROR("<properties> must be an object");
  }

  irs::flags features;

  if (args.Length() > 3) { // have features
    if (!args[3]->IsArray()) {
      TRI_V8_THROW_TYPE_ERROR("<features> must be an array");
    }

    auto value = v8::Local<v8::Array>::Cast(args[3]);

    for (uint32_t i = 0, count = value->Length(); i < count;  ++i) {
      auto subValue = value->Get(context, i).FromMaybe(v8::Local<v8::Value>());

      if (!subValue->IsString()) {
        TRI_V8_THROW_TYPE_ERROR("<feature> must be a string");
      }

      const auto feature = irs::attributes::get(TRI_ObjectToString(isolate, subValue), false);

      if (!feature) {
        TRI_V8_THROW_TYPE_ERROR("<feature> not supported");
      }

      features.add(feature.id());
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
    auto res = analyzers.emplace(result, name, type, propertiesSlice, features);

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

  TRI_GET_GLOBALS();
  auto& analyzers =
      v8g->_server.getFeature<arangodb::iresearch::IResearchAnalyzerFeature>();


  auto name = arangodb::iresearch::IResearchAnalyzerFeature::normalize(
    TRI_ObjectToString(isolate, args[0]), vocbase.name());

  // ...........................................................................
  // end of parameter parsing
  // ...........................................................................

  const auto analyzerVocbase = arangodb::iresearch::IResearchAnalyzerFeature::extractVocbaseName(name);
  if (!arangodb::iresearch::IResearchAnalyzerFeature::analyzerReachableFromDb(
          analyzerVocbase, vocbase.name(), true)) {
    std::string errorMessage("Analyzer '");
    errorMessage.append(name)
      .append("' is not accessible. Only analyzers from current database ('")
      .append(vocbase.name())
      .append("')");
    if (vocbase.name() != arangodb::StaticStrings::SystemDatabase) {
      errorMessage.append(" or system database");
    }
    errorMessage.append(" are available");
    TRI_V8_THROW_EXCEPTION_MESSAGE(
      TRI_ERROR_FORBIDDEN, // code
      errorMessage
    );
  }

  if (!arangodb::iresearch::IResearchAnalyzerFeature::canUse(name, arangodb::auth::Level::RO)) {
    TRI_V8_THROW_EXCEPTION_MESSAGE( // exception
      TRI_ERROR_FORBIDDEN, // code
      "insufficient rights to get analyzer" // message
    );
  }

  try {
    auto analyzer = analyzers.get(name, arangodb::QueryAnalyzerRevisions::QUERY_LATEST);

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
  auto context = TRI_IGETC;
  auto& vocbase = GetContextVocBase(isolate);

  if (vocbase.isDropped()) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  TRI_GET_GLOBALS();
  auto& analyzers =
      v8g->_server.getFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
  auto sysVocbase =
      v8g->_server.hasFeature<arangodb::SystemDatabaseFeature>()
          ? v8g->_server.getFeature<arangodb::SystemDatabaseFeature>().use()
          : nullptr;

  // ...........................................................................
  // end of parameter parsing
  // ...........................................................................

  typedef arangodb::iresearch::AnalyzerPool::ptr AnalyzerPoolPtr;
  std::vector<AnalyzerPoolPtr> result;
  auto visitor = [&result](AnalyzerPoolPtr const& analyzer)->bool {
    if (analyzer) {
      result.emplace_back(analyzer);
    }

    return true; // continue with next analyzer
  };

  try {
    analyzers.visit(visitor, nullptr);  // include static analyzers

    if (arangodb::iresearch::IResearchAnalyzerFeature::canUse(vocbase, arangodb::auth::Level::RO)) {
      analyzers.visit(visitor, &vocbase);
    }

    // include analyzers from the system vocbase if possible
    if (sysVocbase // have system vocbase
       && sysVocbase->name() != vocbase.name() // not same vocbase as current
       && arangodb::iresearch::IResearchAnalyzerFeature::canUse(*sysVocbase, arangodb::auth::Level::RO)) {
      analyzers.visit(visitor, sysVocbase.get());
    }

    auto v8Result = v8::Array::New(isolate);

    for (size_t i = 0, count = result.size(); i < count; ++i) {
      auto analyzer = WrapAnalyzer(isolate, result[i]);

      if (analyzer.IsEmpty() || i > std::numeric_limits<uint32_t>::max()) {
        TRI_V8_THROW_EXCEPTION_MEMORY();
      }

      v8Result->Set(context, static_cast<uint32_t>(i), analyzer).FromMaybe(false); // cast safe because of check above
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

  TRI_GET_GLOBALS();
  auto& analyzers =
      v8g->_server.getFeature<arangodb::iresearch::IResearchAnalyzerFeature>();

  auto nameFromArgs = TRI_ObjectToString(isolate, args[0]);
  auto splittedAnalyzerName =
    arangodb::iresearch::IResearchAnalyzerFeature::splitAnalyzerName(nameFromArgs);
  if (!arangodb::iresearch::IResearchAnalyzerFeature::analyzerReachableFromDb(
           splittedAnalyzerName.first, vocbase.name())) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(
      TRI_ERROR_FORBIDDEN,
      "Database in analyzer name does not match current database");
    return;
  }

  if (!TRI_vocbase_t::IsAllowedName(false, arangodb::velocypack::StringRef(splittedAnalyzerName.second.c_str(),
                                                                           splittedAnalyzerName.second.size()))) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(
      TRI_ERROR_BAD_PARAMETER,
      std::string("Invalid characters in analyzer name '").append(splittedAnalyzerName.second)
        .append("'.")
    );
  }

  auto name = arangodb::iresearch::IResearchAnalyzerFeature::normalize(splittedAnalyzerName.second,
                                                                       vocbase.name());
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
    auto res = analyzers.remove(name, force);
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

    v8g.IResearchAnalyzerManagerTempl.Reset(isolate, objTemplate);

    auto instance = objTemplate->NewInstance(TRI_IGETC).FromMaybe(v8::Local<v8::Object>());

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

    v8g.IResearchAnalyzerInstanceTempl.Reset(isolate, objTemplate);
    TRI_AddGlobalFunctionVocbase( // required only for pretty-printing via JavaScript (must to be defined AFTER v8g.IResearchAnalyzerTempl.Reset(...))
      isolate, // isolate
      TRI_V8_ASCII_STRING(isolate, "ArangoAnalyzer"), // name
      fnTemplate->GetFunction(TRI_IGETC).FromMaybe(v8::Local<v8::Function>()) // impl
    );
  }
}

} // iresearch
} // arangodb

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
