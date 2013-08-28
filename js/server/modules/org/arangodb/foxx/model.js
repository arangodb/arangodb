/*jslint indent: 2, nomen: true, maxlen: 120 */
/*global module, require, exports */

////////////////////////////////////////////////////////////////////////////////
/// @brief Foxx Model
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

var Model,
  _ = require("underscore"),
  backbone_helpers = require("backbone");

// -----------------------------------------------------------------------------
// --SECTION--                                                             Model
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_foxx_model_initializer
/// @brief Create a new instance of Model
///
/// @FUN{new FoxxModel(@FA{data})}
///
/// If you initialize a model, you can give it initial @FA{data} as an object.
///
/// @EXAMPLES
///
/// @code
///     instance = new Model({
///       a: 1
///     });
/// @endcode
////////////////////////////////////////////////////////////////////////////////

Model = function (attributes) {
  'use strict';

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_foxx_model_attributes
/// @brief The attributes property is the internal hash containing the model's state.
////////////////////////////////////////////////////////////////////////////////

  this.attributes = attributes || {};
};

_.extend(Model.prototype, {

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_foxx_model_get
/// @brief Get the value of an attribute
///
/// @FUN{FoxxModel::get(@FA{name})}
///
/// Get the value of an attribute
/// 
/// @EXAMPLES
///
/// @code
///     instance = new Model({
///       a: 1
///     });
///
///     instance.get("a");
/// @endcode
////////////////////////////////////////////////////////////////////////////////

  get: function (attributeName) {
    'use strict';
    return this.attributes[attributeName];
  },

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_foxx_model_set
/// @brief Set the value of an attribute
///
/// @FUN{FoxxModel::set(@FA{name}, @FA{value})}
///
/// Set the value of an attribute
/// 
/// @EXAMPLES
///
/// @code
///     instance = new Model({
///       a: 1
///     });
///
///     instance.set("a", 2);
/// @endcode
////////////////////////////////////////////////////////////////////////////////

  set: function (attributeName, value) {
    'use strict';
    this.attributes[attributeName] = value;
  },

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_foxx_model_has
/// @brief Returns true if attribute is set to a non-null or non-undefined value
///
/// @FUN{FoxxModel::has(@FA{name})}
///
/// Returns true if the attribute is set to a non-null or non-undefined value.
///
/// @EXAMPLES
///
/// @code
///     instance = new Model({
///       a: 1
///     });
///
///     instance.has("a"); //=> true
///     instance.has("b"); //=> false
/// @endcode
////////////////////////////////////////////////////////////////////////////////

  has: function (attributeName) {
    'use strict';
    return !(_.isUndefined(this.attributes[attributeName]) ||
             _.isNull(this.attributes[attributeName]));
  },

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_foxx_model_forDB
/// @brief Return a copy of the model which can be saved into ArangoDB
///
/// @FUN{FoxxModel::forDB()}
///
/// Return a copy of the model which can be saved into ArangoDB
////////////////////////////////////////////////////////////////////////////////

  forDB: function () {
    'use strict';
    return this.attributes;
  },

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_foxx_model_forClient
/// @brief Return a copy of the model which can be returned to the client
///
/// @FUN{FoxxModel::forClient()}
///
/// Return a copy of the model which you can send to the client.
////////////////////////////////////////////////////////////////////////////////

  forClient: function () {
    'use strict';
    return this.attributes;
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_foxx_model_extend
/// @brief Extend the Model prototype to add or overwrite methods.
///
/// @FUN{FoxxModel::extent(@FA{prototype})}
///
/// Extend the Model prototype to add or overwrite methods.
////////////////////////////////////////////////////////////////////////////////

Model.extend = backbone_helpers.extend;

exports.Model = Model;

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

/// Local Variables:
/// mode: outline-minor
/// outline-regexp: "/// @brief\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}\\|/\\*jslint"
/// End:
