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

#include "RestExportHandler.h"
#include "Basics/Exceptions.h"
#include "Basics/json.h"
#include "Basics/MutexLocker.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/VPackStringBufferAdapter.h"
#include "Utils/CollectionExport.h"
#include "Utils/Cursor.h"
#include "Utils/CursorRepository.h"
#include "Wal/LogfileManager.h"

#include <velocypack/Builder.h>
#include <velocypack/Dumper.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace triagens::arango;
using namespace triagens::rest;


////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

RestExportHandler::RestExportHandler(HttpRequest* request)
    : RestVocbaseBaseHandler(request), _restrictions() {}


////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

HttpHandler::status_t RestExportHandler::execute() {
  if (ServerState::instance()->isCoordinator()) {
    generateError(HttpResponse::NOT_IMPLEMENTED, TRI_ERROR_CLUSTER_UNSUPPORTED,
                  "'/_api/export' is not yet supported in a cluster");
    return status_t(HANDLER_DONE);
  }

  // extract the sub-request type
  HttpRequest::HttpRequestType type = _request->requestType();

  if (type == HttpRequest::HTTP_REQUEST_POST) {
    createCursor();
    return status_t(HANDLER_DONE);
  }

  if (type == HttpRequest::HTTP_REQUEST_PUT) {
    modifyCursor();
    return status_t(HANDLER_DONE);
  }

  if (type == HttpRequest::HTTP_REQUEST_DELETE) {
    deleteCursor();
    return status_t(HANDLER_DONE);
  }

  generateError(HttpResponse::METHOD_NOT_ALLOWED,
                TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
  return status_t(HANDLER_DONE);
}


////////////////////////////////////////////////////////////////////////////////
/// @brief build options for the query as JSON
////////////////////////////////////////////////////////////////////////////////

VPackBuilder RestExportHandler::buildOptions(VPackSlice const& slice) {
  VPackBuilder options;
  options.openObject();

  VPackSlice count = slice.get("count");
  if (count.isBool()) {
    options.add("count", count);
  } else {
    options.add("count", VPackValue(false));
  }

  VPackSlice batchSize = slice.get("batchSize");
  if (batchSize.isNumber()) {
    if ((batchSize.isInteger() && batchSize.getUInt() == 0) ||
        (batchSize.isDouble() && batchSize.getDouble() == 0.0)) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_TYPE_ERROR, "expecting non-zero value for 'batchSize'");
    }
    options.add("batchSize", batchSize);
  } else {
    options.add("batchSize", VPackValue(1000));
  }

  VPackSlice limit = slice.get("limit");
  if (limit.isNumber()) {
    options.add("limit", limit);
  }

  VPackSlice flush = slice.get("flush");
  if (flush.isBool()) {
    options.add("flush", flush);
  } else {
    options.add("flush", VPackValue(false));
  }

  VPackSlice ttl = slice.get("ttl");
  if (ttl.isNumber()) {
    options.add("ttl", ttl);
  } else {
    options.add("ttl", VPackValue(30));
  }

  VPackSlice flushWait = slice.get("flushWait");
  if (flushWait.isNumber()) {
    options.add("flushWait", flushWait);
  } else {
    options.add("flushWait", VPackValue(10));
  }
  options.close();

  // handle "restrict" parameter
  VPackSlice restrct = slice.get("restrict");
  if (!restrct.isObject()) {
    if (!restrct.isNone()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_TYPE_ERROR,
                                     "expecting object for 'restrict'");
    }
  } else {
    // "restrict"."type"
    VPackSlice type = restrct.get("type");
    if (!type.isString()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                     "expecting string for 'restrict.type'");
    }
    std::string typeString = type.copyString();

    if (typeString == "include") {
      _restrictions.type = CollectionExport::Restrictions::RESTRICTION_INCLUDE;
    } else if (typeString == "exclude") {
      _restrictions.type = CollectionExport::Restrictions::RESTRICTION_EXCLUDE;
    } else {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_BAD_PARAMETER,
          "expecting either 'include' or 'exclude' for 'restrict.type'");
    }

    // "restrict"."fields"
    VPackSlice fields = restrct.get("fields");
    if (!fields.isArray()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                     "expecting array for 'restrict.fields'");
    }
    for (auto const& name : VPackArrayIterator(fields)) {
      if (name.isString()) {
        _restrictions.fields.emplace(name.copyString());
      }
    }
  }

  return options;
}

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_post_api_export
/// @brief export all documents from a collection, using a cursor
///
/// @RESTHEADER{POST /_api/export, Create export cursor}
///
/// @RESTBODYPARAM{flush,boolean,required,}
/// if set to *true*, a WAL flush operation will be executed prior to the
/// export. The flush operation will start copying documents from the WAL to the
/// collection's datafiles. There will be an additional wait time of up
/// to *flushWait* seconds after the flush to allow the WAL collector to change
/// the adjusted document meta-data to point into the datafiles, too.
/// The default value is *false* (i.e. no flush) so most recently inserted or
/// updated
/// documents from the collection might be missing in the export.
///
/// @RESTBODYPARAM{flushWait,integer,required,int64}
/// maximum wait time in seconds after a flush operation. The default
/// value is 10. This option only has an effect when *flush* is set to *true*.
///
/// @RESTBODYPARAM{count,boolean,required,}
/// boolean flag that indicates whether the number of documents
/// in the result set should be returned in the "count" attribute of the result
/// (optional).
/// Calculating the "count" attribute might in the future have a performance
/// impact so this option is turned off by default, and "count" is only returned
/// when requested.
///
/// @RESTBODYPARAM{batchSize,integer,required,int64}
/// maximum number of result documents to be transferred from
/// the server to the client in one roundtrip (optional). If this attribute is
/// not set, a server-controlled default value will be used.
///
/// @RESTBODYPARAM{limit,integer,required,int64}
/// an optional limit value, determining the maximum number of documents to
/// be included in the cursor. Omitting the *limit* attribute or setting it to 0
/// will
/// lead to no limit being used. If a limit is used, it is undefined which
/// documents
/// from the collection will be included in the export and which will be
/// excluded.
/// This is because there is no natural order of documents in a collection.
///
/// @RESTBODYPARAM{ttl,integer,required,int64}
/// an optional time-to-live for the cursor (in seconds). The cursor will be
/// removed on the server automatically after the specified amount of time. This
/// is useful to ensure garbage collection of cursors that are not fully fetched
/// by clients. If not set, a server-defined value will be used.
///
/// @RESTBODYPARAM{restrict,object,optional,JSF_post_api_export_restrictions}
/// an object containing an array of attribute names that will be
/// included or excluded when returning result documents.
///
/// Not specifying *restrict* will by default return all attributes of each
/// document.
///
/// @RESTSTRUCT{type,JSF_post_api_export_restrictions,string,required,string}
/// has to be be set to either *include* or *exclude* depending on which you
/// want to use
///
/// @RESTSTRUCT{fields,JSF_post_api_export_restrictions,array,required,string}
/// Contains an array of attribute names to *include* or *exclude*. Matching of
/// attribute names
/// for *inclusion* or *exclusion* will be done on the top level only.
/// Specifying names of nested attributes is not supported at the moment.
///
///
/// @RESTQUERYPARAMETERS
///
/// @RESTQUERYPARAM{collection,string,required}
/// The name of the collection to export.
///
/// @RESTDESCRIPTION
/// A call to this method creates a cursor containing all documents in the
/// specified collection. In contrast to other data-producing APIs, the internal
/// data structures produced by the export API are more lightweight, so it is
/// the preferred way to retrieve all documents from a collection.
///
/// Documents are returned in a similar manner as in the `/_api/cursor` REST
/// API.
/// If all documents of the collection fit into the first batch, then no cursor
/// will be created, and the result object's *hasMore* attribute will be set to
/// *false*. If not all documents fit into the first batch, then the result
/// object's *hasMore* attribute will be set to *true*, and the *id* attribute
/// of the result will contain a cursor id.
///
/// The order in which the documents are returned is not specified.
///
/// By default, only those documents from the collection will be returned that
/// are
/// stored in the collection's datafiles. Documents that are present in the
/// write-ahead
/// log (WAL) at the time the export is run will not be exported.
///
/// To export these documents as well, the caller can issue a WAL flush request
/// before calling the export API or set the *flush* attribute. Setting the
/// *flush*
/// option will trigger a WAL flush before the export so documents get copied
/// from
/// the WAL to the collection datafiles.
///
/// If the result set can be created by the server, the server will respond with
/// *HTTP 201*. The body of the response will contain a JSON object with the
/// result set.
///
/// The returned JSON object has the following properties:
///
/// - *error*: boolean flag to indicate that an error occurred (*false*
///   in this case)
///
/// - *code*: the HTTP status code
///
/// - *result*: an array of result documents (might be empty if the collection
/// was empty)
///
/// - *hasMore*: a boolean indicator whether there are more results
///   available for the cursor on the server
///
/// - *count*: the total number of result documents available (only
///   available if the query was executed with the *count* attribute set)
///
/// - *id*: id of temporary cursor created on the server (optional, see above)
///
/// If the JSON representation is malformed or the query specification is
/// missing from the request, the server will respond with *HTTP 400*.
///
/// The body of the response will contain a JSON object with additional error
/// details. The object has the following attributes:
///
/// - *error*: boolean flag to indicate that an error occurred (*true* in this
/// case)
///
/// - *code*: the HTTP status code
///
/// - *errorNum*: the server error number
///
/// - *errorMessage*: a descriptive error message
///
/// Clients should always delete an export cursor result as early as possible
/// because a
/// lingering export cursor will prevent the underlying collection from being
/// compacted or unloaded. By default, unused cursors will be deleted
/// automatically
/// after a server-defined idle time, and clients can adjust this idle time by
/// setting
/// the *ttl* value.
///
/// Note: this API is currently not supported on cluster coordinators.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{201}
/// is returned if the result set can be created by the server.
///
/// @RESTRETURNCODE{400}
/// is returned if the JSON representation is malformed or the query
/// specification is
/// missing from the request.
///
/// @RESTRETURNCODE{404}
/// The server will respond with *HTTP 404* in case a non-existing collection is
/// accessed in the query.
///
/// @RESTRETURNCODE{405}
/// The server will respond with *HTTP 405* if an unsupported HTTP method is
/// used.
///
/// @RESTRETURNCODE{501}
/// The server will respond with *HTTP 501* if this API is called on a cluster
/// coordinator.
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

void RestExportHandler::createCursor() {
  std::vector<std::string> const& suffix = _request->suffix();

  if (suffix.size() != 0) {
    generateError(HttpResponse::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expecting POST /_api/export");
    return;
  }

  // extract the cid
  bool found;
  char const* name = _request->value("collection", found);

  if (!found || *name == '\0') {
    generateError(HttpResponse::BAD,
                  TRI_ERROR_ARANGO_COLLECTION_PARAMETER_MISSING,
                  "'collection' is missing, expecting " + EXPORT_PATH +
                      "?collection=<identifier>");
    return;
  }

  try {
    bool parseSuccess = true;
    VPackOptions vpoptions;
    std::shared_ptr<VPackBuilder> parsedBody =
        parseVelocyPackBody(&vpoptions, parseSuccess);

    if (!parseSuccess) {
      return;
    }
    VPackSlice body = parsedBody.get()->slice();

    VPackBuilder optionsBuilder;

    if (!body.isNone()) {
      if (!body.isObject()) {
        generateError(HttpResponse::BAD, TRI_ERROR_QUERY_EMPTY);
        return;
      }
      optionsBuilder = buildOptions(body);
    } else {
      // create an empty options object
      optionsBuilder.openObject();
    }

    VPackSlice options = optionsBuilder.slice();

    uint64_t waitTime = 0;
    bool flush = triagens::basics::VelocyPackHelper::getBooleanValue(
        options, "flush", false);

    if (flush) {
      // flush the logfiles so the export can fetch all documents
      int res =
          triagens::wal::LogfileManager::instance()->flush(true, true, false);

      if (res != TRI_ERROR_NO_ERROR) {
        THROW_ARANGO_EXCEPTION(res);
      }

      double flushWait =
          triagens::basics::VelocyPackHelper::getNumericValue<double>(
              options, "flushWait", 10.0);

      waitTime = static_cast<uint64_t>(
          flushWait * 1000 *
          1000);  // flushWait is specified in s, but we need ns
    }

    size_t limit = triagens::basics::VelocyPackHelper::getNumericValue<size_t>(
        options, "limit", 0);

    // this may throw!
    auto collectionExport =
        std::make_unique<CollectionExport>(_vocbase, name, _restrictions);
    collectionExport->run(waitTime, limit);

    {
      size_t batchSize =
          triagens::basics::VelocyPackHelper::getNumericValue<size_t>(
              options, "batchSize", 1000);
      double ttl = triagens::basics::VelocyPackHelper::getNumericValue<double>(
          options, "ttl", 30);
      bool count = triagens::basics::VelocyPackHelper::getBooleanValue(
          options, "count", false);

      createResponse(HttpResponse::CREATED);
      _response->setContentType("application/json; charset=utf-8");

      auto cursors = static_cast<triagens::arango::CursorRepository*>(
          _vocbase->_cursorRepository);
      TRI_ASSERT(cursors != nullptr);

      // create a cursor from the result
      triagens::arango::ExportCursor* cursor = cursors->createFromExport(
          collectionExport.get(), batchSize, ttl, count);
      collectionExport.release();

      try {
        _response->body().appendChar('{');
        cursor->dump(_response->body());
        _response->body().appendText(",\"error\":false,\"code\":");
        _response->body().appendInteger(
            static_cast<uint32_t>(_response->responseCode()));
        _response->body().appendChar('}');

        cursors->release(cursor);
      } catch (...) {
        cursors->release(cursor);
        throw;
      }
    }
  } catch (triagens::basics::Exception const& ex) {
    generateError(HttpResponse::responseCode(ex.code()), ex.code(), ex.what());
  } catch (...) {
    generateError(HttpResponse::SERVER_ERROR, TRI_ERROR_INTERNAL);
  }
}

void RestExportHandler::modifyCursor() {
  std::vector<std::string> const& suffix = _request->suffix();

  if (suffix.size() != 1) {
    generateError(HttpResponse::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expecting PUT /_api/export/<cursor-id>");
    return;
  }

  std::string const& id = suffix[0];

  auto cursors = static_cast<triagens::arango::CursorRepository*>(
      _vocbase->_cursorRepository);
  TRI_ASSERT(cursors != nullptr);

  auto cursorId = static_cast<triagens::arango::CursorId>(
      triagens::basics::StringUtils::uint64(id));
  bool busy;
  auto cursor = cursors->find(cursorId, busy);

  if (cursor == nullptr) {
    if (busy) {
      generateError(HttpResponse::responseCode(TRI_ERROR_CURSOR_BUSY),
                    TRI_ERROR_CURSOR_BUSY);
    } else {
      generateError(HttpResponse::responseCode(TRI_ERROR_CURSOR_NOT_FOUND),
                    TRI_ERROR_CURSOR_NOT_FOUND);
    }
    return;
  }

  try {
    createResponse(HttpResponse::OK);
    _response->setContentType("application/json; charset=utf-8");

    _response->body().appendChar('{');
    cursor->dump(_response->body());
    _response->body().appendText(",\"error\":false,\"code\":");
    _response->body().appendInteger(
        static_cast<uint32_t>(_response->responseCode()));
    _response->body().appendChar('}');

    cursors->release(cursor);
  } catch (triagens::basics::Exception const& ex) {
    cursors->release(cursor);

    generateError(HttpResponse::responseCode(ex.code()), ex.code(), ex.what());
  } catch (...) {
    cursors->release(cursor);

    generateError(HttpResponse::SERVER_ERROR, TRI_ERROR_INTERNAL);
  }
}

void RestExportHandler::deleteCursor() {
  std::vector<std::string> const& suffix = _request->suffix();

  if (suffix.size() != 1) {
    generateError(HttpResponse::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expecting DELETE /_api/export/<cursor-id>");
    return;
  }

  std::string const& id = suffix[0];

  auto cursors = static_cast<triagens::arango::CursorRepository*>(
      _vocbase->_cursorRepository);
  TRI_ASSERT(cursors != nullptr);

  auto cursorId = static_cast<triagens::arango::CursorId>(
      triagens::basics::StringUtils::uint64(id));
  bool found = cursors->remove(cursorId);

  if (!found) {
    generateError(HttpResponse::NOT_FOUND, TRI_ERROR_CURSOR_NOT_FOUND);
    return;
  }

  createResponse(HttpResponse::ACCEPTED);
  _response->setContentType("application/json; charset=utf-8");
  VPackBuilder result;
  result.openObject();
  result.add("id", VPackValue(id));
  result.add("error", VPackValue(false));
  result.add("code", VPackValue(_response->responseCode()));
  result.close();
  VPackSlice s = result.slice();
  triagens::basics::VPackStringBufferAdapter buffer(
      _response->body().stringBuffer());
  VPackDumper dumper(&buffer);
  dumper.dump(s);
}


