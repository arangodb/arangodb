////////////////////////////////////////////////////////////////////////////////
/// @brief protobuf to json converter helper functions
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2012 triAGENS GmbH, Cologne, Germany
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

#ifndef TRIAGENS_PROTOCOL_BUFFERS_JSON_CONVERTER_PROTOBUF_H
#define TRIAGENS_PROTOCOL_BUFFERS_JSON_CONVERTER_PROTOBUF_H 1

#include "BasicsC/json.h"

#include "Logger/Logger.h"
#include "ProtocolBuffers/arangodb.pb.h"

// -----------------------------------------------------------------------------
// --SECTION--                                       class JsonConverterProtobuf
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Rest
/// @{
////////////////////////////////////////////////////////////////////////////////

namespace triagens {
  namespace rest {

////////////////////////////////////////////////////////////////////////////////
/// @brief json to protobuf and vice versa conversion functions
////////////////////////////////////////////////////////////////////////////////

    class JsonConverterProtobuf {
      private:
        JsonConverterProtobuf (JsonConverterProtobuf const&);
        JsonConverterProtobuf& operator= (JsonConverterProtobuf const&);
        JsonConverterProtobuf ();
        ~JsonConverterProtobuf ();

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                             public static methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Rest
/// @{
////////////////////////////////////////////////////////////////////////////////

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief convert a protobuf json list object into a TRI_json_t*
////////////////////////////////////////////////////////////////////////////////

        static TRI_json_t* ParseList (TRI_memory_zone_t* zone, 
                                      const PB_ArangoJsonValue& object) {
          TRI_json_t* result = TRI_CreateListJson(zone);

          for (int i = 0; i < object.objects_size(); ++i) {
            const PB_ArangoJsonContent& atom = object.objects(i);

            TRI_json_t* json = ParseObject(zone, atom);
            TRI_PushBack3ListJson(zone, result, json);
          }

          return result;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief convert a protobuf json array object into a TRI_json_t*
////////////////////////////////////////////////////////////////////////////////
        
        static TRI_json_t* ParseArray (TRI_memory_zone_t* zone, 
                                       const PB_ArangoJsonValue& object) {
          TRI_json_t* result = TRI_CreateArrayJson(zone);

          for (int i = 0; i < object.objects_size(); ) {
            const PB_ArangoJsonContent& key = object.objects(i);
            const PB_ArangoJsonContent& value = object.objects(i + 1);
            i += 2;

            if (key.type() != PB_REQUEST_TYPE_STRING) {
              LOGGER_WARNING << "invalid json contained in protobuf message";
              return 0;
            }

            TRI_json_t* json = ParseObject(zone, value);
            TRI_InsertArrayJson(zone, result, key.value().stringvalue().c_str(), json);
          }

          return result;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief convert a protobuf json object into a TRI_json_t*
////////////////////////////////////////////////////////////////////////////////

        static TRI_json_t* ParseObject (TRI_memory_zone_t* zone, 
                                        const PB_ArangoJsonContent& object) {
          TRI_json_t* result;

          switch (object.type()) {
            case PB_REQUEST_TYPE_NULL: {
              result = TRI_CreateNullJson(zone);
              break;
            }

            case PB_REQUEST_TYPE_BOOLEAN: {
              result = TRI_CreateBooleanJson(zone, object.value().booleanvalue());
              break;
            }

            case PB_REQUEST_TYPE_NUMBER: {
              result = TRI_CreateNumberJson(zone, object.value().numbervalue());
              break;
            }

            case PB_REQUEST_TYPE_STRING: {
              result = TRI_CreateString2CopyJson(zone, (char*) object.value().stringvalue().c_str(), object.value().stringvalue().size());
              break;
            }
            
            case PB_REQUEST_TYPE_ARRAY: {
              result = ParseArray(zone, object.value());
              break;
            }

            case PB_REQUEST_TYPE_LIST: {
              result = ParseList(zone, object.value());
              break;
            }
          }

          return result;
        }

    };
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}\\)"
// End:
