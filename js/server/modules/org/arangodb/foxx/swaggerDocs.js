/*global require, exports */

////////////////////////////////////////////////////////////////////////////////
/// @brief Foxx Swagger documentation
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2013 triagens GmbH, Cologne, Germany
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
/// @author Michael Hackstein
/// @author Copyright 2015, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

(function() {
  "use strict";

  var foxxInternal = require("org/arangodb/foxx/internals");
  var _ = require("underscore");
  var internal = require("internal");

  // Wraps the docs object of a route to add swagger compatible documentation
  var SwaggerDocs = function (docs, models) {
    this.docs = docs;
    this.models = models;
  };

  SwaggerDocs.prototype.addNickname = function (httpMethod, match) {
    this.docs.nickname = foxxInternal.constructNickname(httpMethod, match);
  };

  SwaggerDocs.prototype.addPathParam = function (paramName, description, dataType, required) {
    this.docs.parameters.push(foxxInternal.constructPathParamDoc(paramName, description, dataType, required));
  };

  SwaggerDocs.prototype.addQueryParam = function (paramName, description, dataType, required, allowMultiple) {
    this.docs.parameters.push(foxxInternal.constructQueryParamDoc(
      paramName,
      description,
      dataType,
      required,
      allowMultiple
    ));
  };

  SwaggerDocs.prototype.addBodyParam = function (paramName, description, jsonSchema) {

    var token = internal.genRandomAlphaNumbers(32);
    this.models[token] = jsonSchema;

    var param = _.find(this.docs.parameters, function (parameter) {
      return parameter.name === 'undocumented body';
    });

    if (_.isUndefined(param)) {
      this.docs.parameters.push({
        name: paramName,
        paramType: "body",
        description: description,
        dataType: token
      });
    } else {
      param.name = paramName;
      param.description = description;
      param.dataType = token;
    }
  };

  SwaggerDocs.prototype.addSummary = function (summary) {
    this.docs.summary = summary;
  };

  SwaggerDocs.prototype.addNotes = function (notes) {
    this.docs.notes = notes;
  };

exports.Docs = SwaggerDocs;
}());
