////////////////////////////////////////////////////////////////////////////////
/// @brief legends for shaped JSON objects to make them self-contained
///
/// @file
/// Declaration for legends.
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
/// @author Max Neunhoeffer
/// @author Copyright 2014-2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_VOCBASE_LEGENDS_H
#define ARANGODB_VOCBASE_LEGENDS_H 1

#include "Basics/Common.h"
#include "Basics/structures.h"
#include "Basics/StringBuffer.h"
#include "VocBase/shaped-json.h"
#include "VocBase/Shaper.h"

namespace triagens {
  namespace basics {

////////////////////////////////////////////////////////////////////////////////
/// @brief one entry in the table of attribute IDs
////////////////////////////////////////////////////////////////////////////////

    struct AttributeId {
      TRI_shape_aid_t aid;
      TRI_shape_size_t offset;

      AttributeId (TRI_shape_aid_t a, TRI_shape_size_t o)
        : aid(a), 
          offset(o) {

      }

    };


////////////////////////////////////////////////////////////////////////////////
/// @brief one entry in the table of shapes
////////////////////////////////////////////////////////////////////////////////

    struct Shape {
      TRI_shape_sid_t sid;
      TRI_shape_size_t offset;
      TRI_shape_size_t size;

      Shape (TRI_shape_sid_t sid, TRI_shape_size_t o, TRI_shape_size_t siz)
        : sid(sid), 
          offset(o), 
          size(siz) {

      }

    };

////////////////////////////////////////////////////////////////////////////////
/// @brief create a legend for one or more shaped json objects
////////////////////////////////////////////////////////////////////////////////

    class JsonLegend {

////////////////////////////////////////////////////////////////////////////////
/// @brief disable default constructor because we need a shaper
////////////////////////////////////////////////////////////////////////////////

        JsonLegend () = delete;

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor, taking a shaper
////////////////////////////////////////////////////////////////////////////////

        explicit JsonLegend (Shaper* shaper)
          : _shaper(shaper), 
            _att_data(TRI_UNKNOWN_MEM_ZONE),
            _shape_data(TRI_UNKNOWN_MEM_ZONE) {
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

        ~JsonLegend () {
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief clear all data and register a new shaper
////////////////////////////////////////////////////////////////////////////////

        void reset (Shaper* shaper) {
          clear();
          _shaper = shaper;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief clear all data to build a new legend, keep shaper
////////////////////////////////////////////////////////////////////////////////

        void clear ();

////////////////////////////////////////////////////////////////////////////////
/// @brief add an attribute ID to the legend
////////////////////////////////////////////////////////////////////////////////

        int addAttributeId (TRI_shape_aid_t aid);

////////////////////////////////////////////////////////////////////////////////
/// @brief add a shape to the legend
////////////////////////////////////////////////////////////////////////////////

        int addShape (TRI_shaped_json_t const* sh_json) {
          return addShape(sh_json->_sid, sh_json->_data.data, sh_json->_data.length);
        }

        int addShape (TRI_shape_sid_t sid, TRI_blob_t const* blob) {
          return addShape(sid, blob->data, blob->length);
        }

        int addShape (TRI_shape_sid_t sid, char const* data, uint32_t len);

////////////////////////////////////////////////////////////////////////////////
/// @brief get the total size in bytes of the legend
////////////////////////////////////////////////////////////////////////////////

        size_t getSize () const;

////////////////////////////////////////////////////////////////////////////////
/// @brief dump the legend to the buffer pointed to by buf
////////////////////////////////////////////////////////////////////////////////

        void dump (void* buf);

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief the underlying shaper
////////////////////////////////////////////////////////////////////////////////

        Shaper* _shaper;

////////////////////////////////////////////////////////////////////////////////
/// @brief used to sort attribute ID table by attribute ID
////////////////////////////////////////////////////////////////////////////////

        static struct AttributeComparerClass {
          bool operator() (AttributeId const& a, AttributeId const& b) {
            return a.aid < b.aid;
          }
        } 
        AttributeComparerObject;

////////////////////////////////////////////////////////////////////////////////
/// @brief used to sort shapes by shape ID
////////////////////////////////////////////////////////////////////////////////

        static struct ShapeComparerClass {
          bool operator() (Shape const& a, Shape const& b) {
            return a.sid < b.sid;
          }
        } 
        ShapeComparerObject;

////////////////////////////////////////////////////////////////////////////////
/// @brief remember which aids we already have
////////////////////////////////////////////////////////////////////////////////

        std::unordered_set<TRI_shape_aid_t> _have_attribute;

////////////////////////////////////////////////////////////////////////////////
/// @brief table of attribute IDs
////////////////////////////////////////////////////////////////////////////////

        std::vector<AttributeId> _attribs;

////////////////////////////////////////////////////////////////////////////////
/// @brief here we collect the string data for the attributes
////////////////////////////////////////////////////////////////////////////////

        StringBuffer _att_data;

////////////////////////////////////////////////////////////////////////////////
/// @brief remember which sids we already have
////////////////////////////////////////////////////////////////////////////////

        std::unordered_set<TRI_shape_sid_t> _have_shape;

////////////////////////////////////////////////////////////////////////////////
/// @brief table of shapes
////////////////////////////////////////////////////////////////////////////////

        std::vector<Shape> _shapes;

////////////////////////////////////////////////////////////////////////////////
/// @brief here we collect the actual shape data
////////////////////////////////////////////////////////////////////////////////

        StringBuffer _shape_data;

    };

////////////////////////////////////////////////////////////////////////////////
/// @brief a class to read a legend
////////////////////////////////////////////////////////////////////////////////

    class LegendReader : public Shaper {
        // This inherits from Shaper, note however, that at least
        // for the time being it only implements lookupAttributeId and
        // lookupShapeId.

        char const* _legend;
        TRI_shape_size_t _numberAttributes;
        AttributeId const* _aids;
        TRI_shape_size_t _numberShapes;
        Shape const* _shapes;

      public:
        explicit LegendReader (char const* l) 
          : Shaper(),
            _legend(l) {

          auto p = reinterpret_cast<TRI_shape_size_t const*>(l);
          _numberAttributes = *p++;
          _aids = reinterpret_cast<AttributeId const*>(p);
          p = reinterpret_cast<TRI_shape_size_t const*>
                              (_aids + _numberAttributes);
          _numberShapes = *p++;
          _shapes = reinterpret_cast<Shape const*>(p);
        }

        ~LegendReader () {
          // nothing to do
        }

        char const* lookupAttributeId (TRI_shape_aid_t aid) override final {
          // binary search in AttributeIds
          TRI_shape_size_t low = 0;
          TRI_shape_size_t high = _numberAttributes;
          TRI_shape_size_t mid;

          while (low < high) {
            // at the beginning of the loop we always have:
            //   0 <= low < high <= _numberAttributes
            // thus 0 <= mid < _numberAttributes, so access is allowed
            // and for ind (index of element to be found):
            //      low <= ind <= high
            // Once low == high, we have either found it or found nothing
            mid = (low + high) / 2;
            if (_aids[mid].aid < aid) {
              low = mid + 1;
            }
            else {  // Note: aids[mid].aid == aid possible,
                    //       thus ind == high possible as well
              high = mid;
            }
          }
          if (low == high && high < _numberAttributes &&
              _aids[low].aid == aid) {
            return _legend + _aids[low].offset;
          }
          return nullptr;
        }

        TRI_shape_t const* lookupShapeId (TRI_shape_sid_t sid) override final {
          // Is it a builtin basic one?
          if (sid < Shaper::firstCustomShapeId()) {
            return Shaper::lookupSidBasicShape(sid);
          }

          // binary search in Shapes
          TRI_shape_size_t low = 0;
          TRI_shape_size_t high = _numberShapes;
          TRI_shape_size_t mid;

          while (low < high) {
            // at the beginning of the loop we always have:
            //   0 <= low < high <= _numberShapes
            // thus 0 <= mid < _numberShapes, so access is allowed
            // and for ind (index of element to be found):
            //      low <= ind <= high
            // Once low == high, we have either found it or found nothing
            mid = (low + high) / 2;
            if (_shapes[mid].sid < sid) {
              low = mid + 1;
            }
            else {  // Note: _shapes[mid].sid == sid possible,
                    //       thus ind == high possible as well
              high = mid;
            }
          }
          if (low == high && high < _numberShapes &&
              _shapes[low].sid == sid) {
            return reinterpret_cast<TRI_shape_t const*>
                                   (_legend + _shapes[low].offset);
          }
          return nullptr;
        }

    };

  }
}

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
