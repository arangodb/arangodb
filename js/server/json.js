////////////////////////////////////////////////////////////////////////////////
/// @brief JavaScript JSON utility functions
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2011 triagens GmbH, Cologne, Germany
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
/// @author Dr. Frank Celler
/// @author Copyright 2011, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 AvocadoCollection
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8Json
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief string representation of a collection
////////////////////////////////////////////////////////////////////////////////

AvocadoCollection.prototype.toString = function() {
  var status;

  if (this instanceof AvocadoCollection) {
    status = this.status();

    if (status == 1) {
      return "[new born collection " + JSON.stringify(this._name) + "]";
    }
    else if (status == 2) {
      return "[unloaded collection " + JSON.stringify(this._name) + "]";
    }
    else if (status == 3) {
      return "[collection " + JSON.stringify(this._name) + "]";
    }
    else {
      return "[corrupted collection " + JSON.stringify(this._name) + "]";
    }
  }
  else {
    return "[object]";
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief JSON representation of a collection
////////////////////////////////////////////////////////////////////////////////

AvocadoCollection.prototype.toJSON = function() {
  if (this instanceof AvocadoCollection) {
    status = this.status();

    if (status == 1) {
      return "[new born collection " + JSON.stringify(this._name) + "]";
    }
    else if (status == 2) {
      return "[unloaded collection " + JSON.stringify(this._name) + "]";
    }
    else if (status == 3) {
      return "[collection " + JSON.stringify(this._name) + "]";
    }
    else {
      return "[corrupted collection " + JSON.stringify(this._name) + "]";
    }
  }
  else {
    return "[object]";
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                   AvocadoDatabase
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8Json
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief string representation of a vocbase
////////////////////////////////////////////////////////////////////////////////

AvocadoDatabase.prototype.toString = function() {
  if (this instanceof AvocadoDatabase) {
    return "[vocbase at " + JSON.stringify(this._path) + "]";
  }
  else {
    return "[object]";
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief JSON representation of a vocbase
////////////////////////////////////////////////////////////////////////////////

AvocadoDatabase.prototype.toJSON = function() {
  if (this instanceof AvocadoDatabase) {
    return "[vocbase at " + JSON.stringify(this._path) + "]";
  }
  else {
    return "[object]";
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                      AvocadoEdges
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8Json
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief string representation of a vocbase
////////////////////////////////////////////////////////////////////////////////

AvocadoEdges.prototype.toString = function() {
  if (this instanceof AvocadoEdges) {
    return "[edges at " + JSON.stringify(this._path) + "]";
  }
  else {
    return "[object]";
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief JSON representation of a vocbase
////////////////////////////////////////////////////////////////////////////////

AvocadoEdges.prototype.toJSON = function() {
  if (this instanceof AvocadoEdges) {
    return "[edges at " + JSON.stringify(this._path) + "]";
  }
  else {
    return "[object]";
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                            AvocadoEdgesCollection
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8Json
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief string representation of an edges collection
////////////////////////////////////////////////////////////////////////////////

AvocadoEdgesCollection.prototype.toString = function() {
  var status;

  if (this instanceof AvocadoEdgesCollection) {
    status = this.status();

    if (status == 1) {
      return "[new born collection " + JSON.stringify(this._name) + "]";
    }
    else if (status == 2) {
      return "[unloaded collection " + JSON.stringify(this._name) + "]";
    }
    else if (status == 3) {
      return "[collection " + JSON.stringify(this._name) + "]";
    }
    else {
      return "[corrupted collection " + JSON.stringify(this._name) + "]";
    }
  }
  else {
    return "[object]";
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief JSON representation of an edges collection
////////////////////////////////////////////////////////////////////////////////

AvocadoEdgesCollection.prototype.toJSON = function() {
  var status;

  if (this instanceof AvocadoEdgesCollection) {
    status = this.status();

    if (status == 1) {
      return "[new born collection " + JSON.stringify(this._name) + "]";
    }
    else if (status == 2) {
      return "[unloaded collection " + JSON.stringify(this._name) + "]";
    }
    else if (status == 3) {
      return "[collection " + JSON.stringify(this._name) + "]";
    }
    else {
      return "[corrupted collection " + JSON.stringify(this._name) + "]";
    }
  }
  else {
    return "[object]";
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
