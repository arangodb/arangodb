'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
// / Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
// /
// / Licensed under the Business Source License 1.1 (the "License");
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     https://github.com/arangodb/arangodb/blob/devel/LICENSE
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
// / @author Lucas Dohmen
// / @author Copyright 2013, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

var db = require('@arangodb').db,
  _ = require('lodash');

function TemplateMiddleware (templateCollection, helper) {
  var middleware = function (request, response) {
    var responseFunctions,
      _ = require('lodash');

    responseFunctions = {

      render: function (templatePath, data) {
        var template;

        template = templateCollection.firstExample({path: templatePath });

        if (_.isNull(template)) {
          throw new Error("Template '" + templatePath + "' does not exist");
        }

        if (template.templateLanguage !== 'underscore') {
          throw new Error("Unknown template language '" + template.templateLanguage + "'");
        }

        this.body = _.template(template.content)(Object.assign(data, helper));
        this.contentType = template.contentType;
      }
    };

    Object.assign(response, responseFunctions);
  };

  if (_.isString(templateCollection)) {
    templateCollection = db._collection(templateCollection) ||
    db._create(templateCollection);
  }

  return middleware;
}

exports.TemplateMiddleware = TemplateMiddleware;
