/*jslint indent: 2, nomen: true, maxlen: 120, sloppy: true, vars: true */
/*global module, require, exports */

////////////////////////////////////////////////////////////////////////////////
/// @brief Foxx Template Middleware
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Lucas Dohmen
/// @author Copyright 2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var TemplateMiddleware;

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_foxx_TemplateMiddleware_initializer
/// @brief The Template Middleware
///
/// The `TemplateMiddleware` manipulates the request and response
/// objects to give you a nicer API.
////////////////////////////////////////////////////////////////////////////////

TemplateMiddleware = function (templateCollection, helperCollection) {
  'use strict';
  var middleware = function (request, response, options, next) {
    var responseFunctions,
      _ = require("underscore");

    responseFunctions = {

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_foxx_BaseMiddleware_response_render
/// @brief The magical `json` function
///
/// @FUN{response.render(@FA{templatePath}, @FA{data})}
///
/// If you initialize your Application with a `templateCollection`, you're in
/// luck now.
///
/// It expects documents in the following form in this collection:
///
/// @code
/// {
///   path: "high/way",
///   content: "hello <%= username %>",
///   contentType: "text/plain",
///   templateLanguage: "underscore"
/// }
/// @endcode
///
/// The `content` is the string that will be rendered by the template
/// processor. The `contentType` is the type of content that results from this
/// call. And with the `templateLanguage` you can choose your template
/// processor. There is only one choice now: `underscore`.
///
/// If you call render, Application will look into the this collection and
/// search by the path attribute.  It will then render the template with the
/// given data:
///
/// Which would set the body of the response to `hello Application` with the
/// template defined above. It will also set the `contentType` to `text/plain`
/// in this case.
///
/// In addition to the attributes you provided, you also have access to all your
/// view helpers. How to define them? Read above in the ViewHelper section.
///
/// @EXAMPLES
///
/// @code
///     response.render("high/way", {username: 'Application'})
/// @endcode
////////////////////////////////////////////////////////////////////////////////

      render: function (templatePath, data) {
        var template;

        if (_.isUndefined(templateCollection)) {
          throw new Error("No template collection has been provided when creating a new FoxxApplication");
        }

        template = templateCollection.firstExample({path: templatePath });

        if (_.isNull(template)) {
          throw new Error("Template '" + templatePath + "' does not exist");
        }

        if (template.templateLanguage !== "underscore") {
          throw new Error("Unknown template language '" + template.templateLanguage + "'");
        }

        this.body = _.template(template.content, _.extend(data, helperCollection));
        this.contentType = template.contentType;
      }
    };

    _.extend(response, responseFunctions);
  };

  return {
    stringRepresentation: String(middleware),
    functionRepresentation: middleware
  };
};

exports.TemplateMiddleware = TemplateMiddleware;

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

/// Local Variables:
/// mode: outline-minor
/// outline-regexp: "/// @brief\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}\\|/\\*jslint"
/// End:
