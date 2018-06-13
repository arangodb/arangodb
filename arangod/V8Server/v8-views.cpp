////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "v8-views.h"
#include "Basics/conversions.h"
#include "Basics/StaticStrings.h"
#include "Logger/Logger.h"
#include "RestServer/DatabaseFeature.h"
#include "Transaction/V8Context.h"
#include "Utils/CollectionNameResolver.h"
#include "V8/v8-conv.h"
#include "V8/v8-globals.h"
#include "V8/v8-utils.h"
#include "V8/v8-vpack.h"
#include "V8Server/v8-externals.h"
#include "V8Server/v8-vocbaseprivate.h"
#include "VocBase/LogicalView.h"
#include "VocBase/vocbase.h"

using namespace arangodb;
using namespace arangodb::basics;

static std::shared_ptr<arangodb::LogicalView> GetViewFromArgument(
    TRI_vocbase_t* vocbase, v8::Handle<v8::Value> const val) {
  // number
  if (val->IsNumber() || val->IsNumberObject()) {
    uint64_t id = TRI_ObjectToUInt64(val, true);
    return vocbase->lookupView(id);
  }

  return vocbase->lookupView(TRI_ObjectToString(val));
}

/// @brief weak reference callback for views
static void WeakViewCallback(
    const v8::WeakCallbackInfo<v8::Persistent<v8::External>>& data) {
  auto isolate = data.GetIsolate();
  auto persistent = data.GetParameter();
  auto myView = v8::Local<v8::External>::New(isolate, *persistent);
  auto v = static_cast<std::shared_ptr<LogicalView>*>(myView->Value());

  TRI_ASSERT(v != nullptr);
  LogicalView* view = v->get();
  TRI_ASSERT(view != nullptr);

  TRI_GET_GLOBALS();

  v8g->decreaseActiveExternals();

  // decrease the reference-counter for the database
  TRI_ASSERT(!view->vocbase().isDangling());

// find the persistent handle
#if ARANGODB_ENABLE_MAINTAINER_MODE
  auto const& it = v8g->JSViews.find(view);
  TRI_ASSERT(it != v8g->JSViews.end());
#endif

  // dispose and clear the persistent handle
  v8g->JSViews[view].Reset();
  v8g->JSViews.erase(view);

  view->vocbase().release();
  delete v;  // delete the shared_ptr on the heap
}

/// @brief wraps a LogicalView
v8::Handle<v8::Object> WrapView(v8::Isolate* isolate,
                                std::shared_ptr<arangodb::LogicalView> view) {
  v8::EscapableHandleScope scope(isolate);

  TRI_GET_GLOBALS();
  TRI_GET_GLOBAL(VocbaseViewTempl, v8::ObjectTemplate);
  v8::Handle<v8::Object> result = VocbaseViewTempl->NewInstance();

  if (!result.IsEmpty()) {
    // create a new shared_ptr on the heap
    result->SetInternalField(SLOT_CLASS_TYPE,
                             v8::Integer::New(isolate, WRP_VOCBASE_VIEW_TYPE));

    auto const& it = v8g->JSViews.find(view.get());

    if (it == v8g->JSViews.end()) {
      // increase the reference-counter for the database
      TRI_ASSERT(!view->vocbase().isDangling());
      view->vocbase().forceUse();

      try {
        auto v = new std::shared_ptr<arangodb::LogicalView>(view);
        auto externalView = v8::External::New(isolate, v);

        result->SetInternalField(SLOT_CLASS, v8::External::New(isolate, v));

        result->SetInternalField(SLOT_EXTERNAL, externalView);

        v8g->JSViews[view.get()].Reset(isolate, externalView);
        v8g->JSViews[view.get()].SetWeak(&v8g->JSViews[view.get()],
                                         WeakViewCallback,
                                         v8::WeakCallbackType::kFinalizer);
        v8g->increaseActiveExternals();
      } catch (...) {
        view->vocbase().release();
        throw;
      }
    } else {
      auto myView = v8::Local<v8::External>::New(isolate, it->second);
      result->SetInternalField(SLOT_CLASS,
                               v8::External::New(isolate, myView->Value()));
      result->SetInternalField(SLOT_EXTERNAL, myView);
    }
    TRI_GET_GLOBAL_STRING(_IdKey);
    TRI_GET_GLOBAL_STRING(_DbNameKey);
    result->ForceSet(_IdKey,
                     TRI_V8UInt64String<TRI_voc_cid_t>(isolate, view->id()),
                     v8::ReadOnly);
    result->Set(_DbNameKey, TRI_V8_STD_STRING(isolate, view->vocbase().name()));
  }

  return scope.Escape<v8::Object>(result);
}

static void JS_CreateViewVocbase(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);
  auto& vocbase = GetContextVocBase(isolate);

  // we require exactly 3 arguments
  if (args.Length() != 3) {
    TRI_V8_THROW_EXCEPTION_USAGE("_createView(<name>, <type>, <properties>)");
  }

  PREVENT_EMBEDDED_TRANSACTION();

  // extract the name
  std::string const name = TRI_ObjectToString(args[0]);

  // extract the type
  std::string const type = TRI_ObjectToString(args[1]);

  if (!args[2]->IsObject()) {
    TRI_V8_THROW_TYPE_ERROR("<properties> must be an object");
  }
  v8::Handle<v8::Object> obj = args[2]->ToObject();

  // fiddle "name" attribute into the object
  obj->Set(TRI_V8_ASCII_STRING(isolate, "name"), TRI_V8_STD_STRING(isolate, name));

  VPackBuilder full;
  full.openObject();
  full.add(arangodb::StaticStrings::DataSourceName, VPackValue(name));
  full.add(arangodb::StaticStrings::DataSourceType, VPackValue(type));
  VPackSlice infoSlice;

  VPackBuilder properties;
  int res = TRI_V8ToVPack(isolate, properties, obj, false);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  full.add("properties", properties.slice());
  full.close();
  infoSlice = full.slice();

  try {
    auto view = vocbase.createView(infoSlice);

    TRI_ASSERT(view != nullptr);

    v8::Handle<v8::Value> result = WrapView(isolate, view);
    if (result.IsEmpty()) {
      TRI_V8_THROW_EXCEPTION_MEMORY();
    }

    TRI_V8_RETURN(result);
  } catch (basics::Exception const& ex) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(ex.code(), ex.what());
  } catch (std::exception const& ex) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, ex.what());
  } catch (...) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "cannot create view");
  }
  TRI_V8_TRY_CATCH_END
}

static void JS_DropViewVocbase(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);
  auto& vocbase = GetContextVocBase(isolate);

  // we require exactly 1 string argument and an optional boolean argument
  if (args.Length() < 1 || args.Length() > 2) {
    TRI_V8_THROW_EXCEPTION_USAGE("_dropView(<name> [, allowDropSystem])");
  }

  PREVENT_EMBEDDED_TRANSACTION();

  bool allowDropSystem = false;

  if (args.Length() > 1) {
    // options
    if (args[1]->IsObject()) {
      TRI_GET_GLOBALS();
      v8::Handle<v8::Object> optionsObject = args[1].As<v8::Object>();
      TRI_GET_GLOBAL_STRING(IsSystemKey);

      if (optionsObject->Has(IsSystemKey)) {
        allowDropSystem = TRI_ObjectToBoolean(optionsObject->Get(IsSystemKey));
      }
    } else {
      allowDropSystem = TRI_ObjectToBoolean(args[1]);
    }
  }

  // extract the name
  std::string const name = TRI_ObjectToString(args[0]);
  auto view = vocbase.lookupView(name);

  if (view) {
    auto res = vocbase.dropView(view->id(), allowDropSystem).errorNumber();

    if (res != TRI_ERROR_NO_ERROR) {
      TRI_V8_THROW_EXCEPTION(res);
    }
  }

  TRI_V8_RETURN_UNDEFINED();
  TRI_V8_TRY_CATCH_END
}

/// @brief drops a view
static void JS_DropViewVocbaseObj(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  std::shared_ptr<arangodb::LogicalView>* v =
      TRI_UnwrapClass<std::shared_ptr<arangodb::LogicalView>>(
          args.Holder(), WRP_VOCBASE_VIEW_TYPE);

  if (v == nullptr || v->get() == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract view");
  }

  LogicalView* view = v->get();

  PREVENT_EMBEDDED_TRANSACTION();

  bool allowDropSystem = false;

  if (args.Length() > 0) {
    // options
    if (args[0]->IsObject()) {
      TRI_GET_GLOBALS();
      v8::Handle<v8::Object> optionsObject = args[0].As<v8::Object>();
      TRI_GET_GLOBAL_STRING(IsSystemKey);

      if (optionsObject->Has(IsSystemKey)) {
        allowDropSystem = TRI_ObjectToBoolean(optionsObject->Get(IsSystemKey));
      }
    } else {
      allowDropSystem = TRI_ObjectToBoolean(args[0]);
    }
  }

  auto res =
    view->vocbase().dropView(view->id(), allowDropSystem).errorNumber();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(res, "cannot drop view");
  }

  TRI_V8_RETURN_UNDEFINED();
  TRI_V8_TRY_CATCH_END
}

static void JS_ViewVocbase(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);
  auto& vocbase = GetContextVocBase(isolate);

  if (vocbase.isDropped()) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  // expecting one argument
  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("_view(<name>|<identifier>)");
  }

  v8::Handle<v8::Value> val = args[0];
  auto view = GetViewFromArgument(&vocbase, val);

  if (view == nullptr) {
    TRI_V8_RETURN_NULL();
  }

  v8::Handle<v8::Value> result = WrapView(isolate, view);

  if (result.IsEmpty()) {
    TRI_V8_THROW_EXCEPTION_MEMORY();
  }

  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

/// @brief return a list of all views
static void JS_ViewsVocbase(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);
  auto& vocbase = GetContextVocBase(isolate);
  auto views = vocbase.views();

  std::sort(views.begin(), views.end(),
            [](std::shared_ptr<LogicalView> const& lhs,
               std::shared_ptr<LogicalView> const& rhs) -> bool {
              return StringUtils::tolower(lhs->name()) <
                     StringUtils::tolower(rhs->name());
            });

  bool error = false;
  // already create an array of the correct size
  v8::Handle<v8::Array> result = v8::Array::New(isolate);

  size_t const n = views.size();

  for (size_t i = 0; i < n; ++i) {
    auto view = views[i];

    v8::Handle<v8::Value> c = WrapView(isolate, view);

    if (c.IsEmpty()) {
      error = true;
      break;
    }

    result->Set(static_cast<uint32_t>(i), c);
  }

  if (error) {
    TRI_V8_THROW_EXCEPTION_MEMORY();
  }

  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

/// @brief returns the name of a view
static void JS_NameViewVocbase(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  std::shared_ptr<arangodb::LogicalView>* v =
      TRI_UnwrapClass<std::shared_ptr<arangodb::LogicalView>>(
          args.Holder(), WRP_VOCBASE_VIEW_TYPE);

  if (v == nullptr || v->get() == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract view");
  }

  LogicalView* view = v->get();

  std::string const name(view->name());

  if (name.empty()) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
  }

  v8::Handle<v8::Value> result = TRI_V8_STD_STRING(isolate, name);
  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

/// @brief returns the properties of a view
static void JS_PropertiesViewVocbase(
    v8::FunctionCallbackInfo<v8::Value> const& args
) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);
  auto* viewPtr = TRI_UnwrapClass<std::shared_ptr<arangodb::LogicalView>>(
    args.Holder(), WRP_VOCBASE_VIEW_TYPE
  );

  if (!viewPtr || !*viewPtr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract view");
  }

  auto view = *viewPtr;

  // In the cluster the view object might contain outdated properties,
  // which will break tests. We need an extra lookup for each operation.
  arangodb::CollectionNameResolver resolver(view->vocbase());

  // check if we want to change some parameters
  if (args.Length() > 0 && args[0]->IsObject()) {
    arangodb::velocypack::Builder builder;

    {
      auto res = TRI_V8ToVPack(isolate, builder, args[0], false);

      if (TRI_ERROR_NO_ERROR != res) {
        TRI_V8_THROW_EXCEPTION(res);
      }
    }

    bool partialUpdate = true; // partial update by default

    if (args.Length() > 1) {
      if (!args[1]->IsBoolean()) {
        TRI_V8_THROW_EXCEPTION_PARAMETER("<partialUpdate> must be a boolean");
      }

      partialUpdate = args[1]->ToBoolean()->Value();
    }

    auto doSync = arangodb::application_features::ApplicationServer::getFeature<
      DatabaseFeature
    >("Database")->forceSyncProperties();

    view = resolver.getView(view->id()); // ensure have the latest definition

    if (!view) {
      TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
    }

    auto res = view->updateProperties(builder.slice(), partialUpdate, doSync);

    if (!res.ok()) {
      TRI_V8_THROW_EXCEPTION_MESSAGE(res.errorNumber(), res.errorMessage());
    }
  }

  view = resolver.getView(view->id());

  if (!view) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
  }

  arangodb::velocypack::Builder builder;

  builder.openObject();
  view->toVelocyPack(builder, true, false);
  builder.close();

  // return the current parameter set
  // FIXME TODO this should be the full view representation similar to JS_PropertiesVocbaseCol(...), not just "properties"
  auto result =
    TRI_VPackToV8(isolate, builder.slice().get("properties")) ->ToObject();

  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief rename a view
////////////////////////////////////////////////////////////////////////////////

static void JS_RenameViewVocbase(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() < 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("rename(<name>)");
  }

  std::string const name = TRI_ObjectToString(args[0]);
  
  if (name.empty()) {
    TRI_V8_THROW_EXCEPTION_PARAMETER("<name> must be non-empty");
  }
  
  std::shared_ptr<arangodb::LogicalView>* v =
      TRI_UnwrapClass<std::shared_ptr<arangodb::LogicalView>>(
          args.Holder(), WRP_VOCBASE_VIEW_TYPE);

  if (v == nullptr || v->get() == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract view");
  }

  std::shared_ptr<LogicalView> view = *v;

  PREVENT_EMBEDDED_TRANSACTION();

  int res = view->vocbase().renameView(view, name);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(res, "cannot rename view");
  }

  TRI_V8_RETURN_UNDEFINED();
  TRI_V8_TRY_CATCH_END
}

/// @brief return the type of a view
static void JS_TypeViewVocbase(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  std::shared_ptr<arangodb::LogicalView>* v =
      TRI_UnwrapClass<std::shared_ptr<arangodb::LogicalView>>(
          args.Holder(), WRP_VOCBASE_VIEW_TYPE);

  if (v == nullptr || v->get() == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract view");
  }

  LogicalView* view = v->get();

  auto& type = view->type().name();
  TRI_V8_RETURN(TRI_V8_STD_STRING(isolate, type));
  TRI_V8_TRY_CATCH_END
}

void TRI_InitV8Views(v8::Handle<v8::Context> context, TRI_vocbase_t* vocbase,
                     TRI_v8_global_t* v8g, v8::Isolate* isolate,
                     v8::Handle<v8::ObjectTemplate> ArangoDBNS) {
  TRI_AddMethodVocbase(isolate, ArangoDBNS, TRI_V8_ASCII_STRING(isolate, "_createView"),
                       JS_CreateViewVocbase);
  TRI_AddMethodVocbase(isolate, ArangoDBNS, TRI_V8_ASCII_STRING(isolate, "_dropView"),
                       JS_DropViewVocbase);
  TRI_AddMethodVocbase(isolate, ArangoDBNS, TRI_V8_ASCII_STRING(isolate, "_view"),
                       JS_ViewVocbase);
  TRI_AddMethodVocbase(isolate, ArangoDBNS, TRI_V8_ASCII_STRING(isolate, "_views"),
                       JS_ViewsVocbase);

  v8::Handle<v8::ObjectTemplate> rt;
  v8::Handle<v8::FunctionTemplate> ft;

  ft = v8::FunctionTemplate::New(isolate);
  ft->SetClassName(TRI_V8_ASCII_STRING(isolate, "ArangoView"));

  rt = ft->InstanceTemplate();
  rt->SetInternalFieldCount(3);

  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "drop"),
                       JS_DropViewVocbaseObj);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "name"),
                       JS_NameViewVocbase);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "properties"),
                       JS_PropertiesViewVocbase);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "rename"),
                       JS_RenameViewVocbase);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "type"),
                       JS_TypeViewVocbase);

  v8g->VocbaseViewTempl.Reset(isolate, rt);
  TRI_AddGlobalFunctionVocbase(isolate, TRI_V8_ASCII_STRING(isolate, "ArangoView"),
                               ft->GetFunction());
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------