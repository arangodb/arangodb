////////////////////////////////////////////////////////////////////////////////
/// @brief a string buffer wrapper to be used by VelocyPackDumper
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2015 ArangoDB GmbH, Cologne, Germany
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

#include "string-buffer.h"
#include <velocypack/Sink.h>
#include <velocypack/velocypack-aliases.h>

namespace triagens {
  namespace basics {

    class VPackStringBufferAdapter final : public VPackSink {

      public: 
        VPackStringBufferAdapter (TRI_string_buffer_t* buffer) 
          : _buffer(buffer) {
        }

        void push_back (char c) override final;
        void append (std::string const& p) override final;
        void append (char const* p) override final;
        void append (char const* p, uint64_t len) override final;
        void reserve (uint64_t len) override final;

      private:
        TRI_string_buffer_t* _buffer;

    };

  }
}
