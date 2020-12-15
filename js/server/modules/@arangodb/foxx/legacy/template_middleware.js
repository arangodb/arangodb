'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / @brief Foxx Template Middleware
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2013 triagens GmbH, Cologne, Germany
// /
// / Licensed under the Apache License, Version 2.0 (the "License")
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     http://www.apache.org/licenses/LICENSE-2.0
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is triAGENS GmbH, Cologne, Germany
// /
// / @author Lucas Dohmen
// / @author Copyright 2013, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

var db = require('@arangodb').db,
  _ = require('lodash');

// //////////////////////////////////////////////////////////////////////////////
  // / @start Docu Block JSF_foxx_TemplateMiddleware_initializer
  // /
  // / Initialize with the name of a collection or a collection and optionally
  // / a set of helper functions.
  // / Then use *before* to attach the initialized middleware to your Foxx.Controller
  // /
  // / @EXAMPLES
  // /
  // / ```js
  // / templateMiddleware = new TemplateMiddleware("templates", {
  // /   uppercase: function (x) { return x.toUpperCase(); }
  // / })
  // / // or without helpers:
  // / //templateMiddleware = new TemplateMiddleware("templates")
  // /
  // / app.before(templateMiddleware)
  // / ```
  // / @end Docu Block
  // //////////////////////////////////////////////////////////////////////////////

function TemplateMiddleware (templateCollection, helper) {
  var middleware = function (request, response) {
    var responseFunctions,
      _ = require('lodash');

    responseFunctions = {

      // //////////////////////////////////////////////////////////////////////////////
      // / @start Docu Block JSF_foxx_TemplateMiddleware_response_render
      // /
      // / `response.render(templatePath, data)`
      // /
      // / When the TemplateMiddleware is included, you will have access to the
      // / *render* function on the response object.
      // / If you call render, Controller will look into the this collection and
      // / search by the path attribute.  It will then render the template with the
      // / given data.
      // /
      // / @EXAMPLES
      // /
      // / ```js
      // / response.render("high/way", {username: 'Application'})
      // / ```
      // / @end Docu Block
      // //////////////////////////////////////////////////////////////////////////////

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
