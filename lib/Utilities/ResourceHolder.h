////////////////////////////////////////////////////////////////////////////////
/// @brief resource holder with auto-free functionality
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2012 triagens GmbH, Cologne, Germany
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
/// @author Jan Steemann
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_UTILITIES_RESOURCE_HOLDER_H
#define TRIAGENS_UTILITIES_RESOURCE_HOLDER_H 1

#include "BasicsC/json.h"
#include "BasicsC/strings.h"
#include "ShapedJson/json-shaper.h"
#include "VocBase/barrier.h"

// -----------------------------------------------------------------------------
// --SECTION--                                              class ResourceHolder
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief resource holder
////////////////////////////////////////////////////////////////////////////////

class ResourceHolder {
  ResourceHolder (ResourceHolder const&);
  ResourceHolder& operator= (ResourceHolder const&);

// -----------------------------------------------------------------------------
// --SECTION--                                                     private types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Utilities
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief resource types
////////////////////////////////////////////////////////////////////////////////

  typedef enum {
    TYPE_STRING,
    TYPE_JSON,
    TYPE_SHAPED_JSON,
    TYPE_BARRIER
  } 
  resource_type_e;

////////////////////////////////////////////////////////////////////////////////
/// @brief resource value structure
////////////////////////////////////////////////////////////////////////////////

  struct Resource {
    Resource (const resource_type_e type,
              void* const context,
              void* const value) : 
      _type(type),
      _context(context),
      _value(value) {
    }

    ~Resource () {
      switch (_type) {
        case TYPE_STRING: 
          TRI_FreeString((TRI_memory_zone_t*) _context, (char*) _value);
          break;
        case TYPE_JSON:
          TRI_FreeJson((TRI_memory_zone_t*) _context, (TRI_json_t*) _value);
          break;
        case TYPE_SHAPED_JSON:
          TRI_FreeShapedJson((TRI_shaper_t*) _context, (TRI_shaped_json_t*) _value);
          break;
        case TYPE_BARRIER:
          TRI_FreeBarrier((TRI_barrier_t*) _value);
          break;
        default: {
        }
      }
    }

    resource_type_e  _type;
    void*            _context;
    void*            _value;
  };

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Utilities
/// @{
////////////////////////////////////////////////////////////////////////////////

  public:

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a resource holder
////////////////////////////////////////////////////////////////////////////////

    ResourceHolder () : _resources() {
    }

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a resource holder
/// this will free all registered elements
////////////////////////////////////////////////////////////////////////////////

    ~ResourceHolder () {
      for (size_t i = 0; i < _resources.size(); ++i) {
        Resource* resource = _resources[i];
        if (resource != 0) {
          delete resource;
        }
      }

      _resources.clear();
    }

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Utilities
/// @{
////////////////////////////////////////////////////////////////////////////////

  public:

////////////////////////////////////////////////////////////////////////////////
/// @brief register a string object
////////////////////////////////////////////////////////////////////////////////

    bool registerString (TRI_memory_zone_t* context,
                         char* const value) {
      return registerResource(TYPE_STRING, (void*) context, (void*) value);
    }

////////////////////////////////////////////////////////////////////////////////
/// @brief register a JSON object
////////////////////////////////////////////////////////////////////////////////

    bool registerJson (TRI_memory_zone_t* context,
                       TRI_json_t* const value) {
      return registerResource(TYPE_JSON, (void*) context, (void*) value);
    }

////////////////////////////////////////////////////////////////////////////////
/// @brief register a shaped json object
////////////////////////////////////////////////////////////////////////////////

    bool registerShapedJson (TRI_shaper_t* context,
                             TRI_shaped_json_t* const value) {
      return registerResource(TYPE_SHAPED_JSON, (void*) context, (void*) value);
    }

////////////////////////////////////////////////////////////////////////////////
/// @brief register a barrier object
////////////////////////////////////////////////////////////////////////////////

    bool registerBarrier (TRI_barrier_t* const value) {
      return registerResource(TYPE_BARRIER, 0, (void*) value);
    }

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Utilities
/// @{
////////////////////////////////////////////////////////////////////////////////

  private:

////////////////////////////////////////////////////////////////////////////////
/// @brief register an object
////////////////////////////////////////////////////////////////////////////////

    bool registerResource (const resource_type_e type,
                           void* const context,
                           void* const value) {
      if (value == 0) {
        return false;
      }

      Resource* resource = new Resource(type, context, value);
      _resources.push_back(resource);

      return true;
    }

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Utilities
/// @{
////////////////////////////////////////////////////////////////////////////////

  private:

////////////////////////////////////////////////////////////////////////////////
/// @brief resources managed
////////////////////////////////////////////////////////////////////////////////

    std::vector<Resource*> _resources;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

};

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
