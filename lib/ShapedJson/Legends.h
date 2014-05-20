////////////////////////////////////////////////////////////////////////////////
/// @brief legends for shaped JSON objects to make them self-contained
///
/// @file
/// Declaration for legends.
///
/// DISCLAIMER
///
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Max Neunhoeffer
/// @author Copyright 2014-2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_SHAPED_JSON_LEGENDS_H
#define TRIAGENS_SHAPED_JSON_LEGENDS_H 1

#include "BasicsC/common.h"
#include "Basics/Common.h"

#include "BasicsC/structures.h"
#include "Basics/StringBuffer.h"
#include "ShapedJson/shaped-json.h"
#include "ShapedJson/json-shaper.h"


namespace triagens {
  namespace basics {

    class JsonLegend {

      public:

        JsonLegend (TRI_shaper_t* shaper) 
          : _shaper(shaper), _att_data(TRI_UNKNOWN_MEM_ZONE),
            _shape_data(TRI_UNKNOWN_MEM_ZONE) {
        }

        ~JsonLegend () {
        };

        void reset (TRI_shaper_t* shaper) {
          clear();
          _shaper = shaper;
        }

        void clear ();

        int addAttributeId (TRI_shape_aid_t aid);

        int addShape (TRI_shaped_json_t const* sh_json) {
          return addShape(sh_json->_sid, sh_json->_data.data,
                          sh_json->_data.length);
        }

        int addShape (TRI_shape_sid_t sid, TRI_blob_t const* blob) {
          return addShape(sid, blob->data, blob->length);
        }

        int addShape (TRI_shape_sid_t sid, char const* data, uint32_t len);

        size_t getSize () const;

        void dump (void* buf);

      private:

        TRI_shaper_t* _shaper;

        struct AttributeId {
          TRI_shape_aid_t aid;
          TRI_shape_size_t offset;

          AttributeId (TRI_shape_aid_t a, TRI_shape_size_t o)
            : aid(a), offset(o) {}

        };

        static struct AttributeComparerClass {
          bool operator() (AttributeId const& a, AttributeId const& b) {
            return a.aid < b.aid;
          }
        } AttributeComparerObject;

        struct Shape {
          TRI_shape_sid_t sid;
          TRI_shape_size_t offset;
          TRI_shape_size_t size;

          Shape (TRI_shape_sid_t sid, TRI_shape_size_t o, TRI_shape_size_t siz)
            : sid(sid), offset(o), size(siz) {}

        };

        static struct ShapeComparerClass {
          bool operator() (Shape const& a, Shape const& b) {
            return a.sid < b.sid;
          }
        } ShapeComparerObject;

        unordered_set<TRI_shape_aid_t> _have_attribute;
        vector<AttributeId> _attribs;
        StringBuffer _att_data;
        unordered_set<TRI_shape_sid_t> _have_shape;
        vector<Shape> _shapes;
        StringBuffer _shape_data;

    };

  }
}

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
