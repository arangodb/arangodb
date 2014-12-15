////////////////////////////////////////////////////////////////////////////////
/// @brief V8 wrapper for external structures
///
/// @file
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
/// @author Dr. Frank Celler
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2013, triAGENS GmbH, Cologne, Germany
///
/// Parts of the code are based on:
///
/// Copyright Joyent, Inc. and other Node contributors.
///
/// Permission is hereby granted, free of charge, to any person obtaining a
/// copy of this software and associated documentation files (the
/// "Software"), to deal in the Software without restriction, including
/// without limitation the rights to use, copy, modify, merge, publish,
/// distribute, sublicense, and/or sell copies of the Software, and to permit
/// persons to whom the Software is furnished to do so, subject to the
/// following conditions:
///
/// The above copyright notice and this permission notice shall be included
/// in all copies or substantial portions of the Software.
///
/// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
/// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
/// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
/// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
/// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
/// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
/// USE OR OTHER DEALINGS IN THE SOFTWARE.
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_V8_V8__WRAPPER_H
#define ARANGODB_V8_V8__WRAPPER_H 1

#include "Basics/Common.h"

#include <v8.h>

// -----------------------------------------------------------------------------
// --SECTION--                                                   class V8Wrapper
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief V8 wrapper helper
////////////////////////////////////////////////////////////////////////////////

template<typename STRUCT, uint16_t CID>
class V8Wrapper {

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

  public:

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

    V8Wrapper (v8::Isolate* isolate,
               STRUCT* object,
               void (*free)(STRUCT* object),
               v8::Handle<v8::Object> result)
      : _refs(0),
        _object(object),
        _free(free),
        _isolate(isolate) {

      // sanity checks
      TRI_ASSERT(_handle.IsEmpty());
      TRI_ASSERT(result->InternalFieldCount() > 0);

      // create a new persistent handle
      result->SetAlignedPointerInInternalField(0, this);
      _handle.Reset(_isolate, result);

      _handle.SetWrapperClassId(CID);

      // and make it weak, so that we can garbage collect
      makeWeak();
    }

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

    virtual ~V8Wrapper () {
      if (! _handle.IsEmpty()) {
        TRI_ASSERT(_handle.IsNearDeath());

        _handle.ClearWeak();
        v8::Local<v8::Object> data = v8::Local<v8::Object>::New(_isolate, _handle);
        data->SetInternalField(0, v8::Undefined(_isolate));
        _handle.Reset();

        _handle.Reset();

        if (_free != 0) {
          _free(_object);
        }
      }
    }

// -----------------------------------------------------------------------------
// --SECTION--                                             static public methods
// -----------------------------------------------------------------------------

  public:

////////////////////////////////////////////////////////////////////////////////
/// @brief unwraps a structure
////////////////////////////////////////////////////////////////////////////////

    static STRUCT* unwrap (v8::Handle<v8::Object> handle) {
      return static_cast<V8Wrapper*>(handle->GetAlignedPointerFromInternalField(0))->_object;
    }

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes a structure
////////////////////////////////////////////////////////////////////////////////

    static void deleteObject (STRUCT* object) {
      delete object;
    }

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

  public:

////////////////////////////////////////////////////////////////////////////////
/// @brief increases reference count
///
/// Marks the object as being attached to an external entity.  Refed objects
/// will not be garbage collected, even if all references are lost.
////////////////////////////////////////////////////////////////////////////////

    virtual void ref () {
      TRI_ASSERT(! _handle.IsEmpty());
      ++_refs;
      _handle.ClearWeak();
    }

////////////////////////////////////////////////////////////////////////////////
/// @brief decreases reference count
///
/// Marks an object as detached from the event loop.  This is its default state.
/// When an object with a "weak" reference changes from attached to detached
/// state it will be freed. Be careful not to access the object after making
/// this call as it might be gone!  (A "weak reference" means an object that
/// only has a persistant handle.)
///
/// DO NOT CALL THIS FROM DESTRUCTOR.
////////////////////////////////////////////////////////////////////////////////

    virtual void unref () {
      TRI_ASSERT(! _handle.IsEmpty());
      TRI_ASSERT(! _handle.IsWeak());
      TRI_ASSERT(_refs > 0);

      if (--_refs == 0) {
        makeWeak();
      }
    }

// -----------------------------------------------------------------------------
// --SECTION--                                                  public variables
// -----------------------------------------------------------------------------

  public:

////////////////////////////////////////////////////////////////////////////////
/// @brief persistent handle for V8 object
////////////////////////////////////////////////////////////////////////////////

    v8::Persistent<v8::Object> _handle;

// -----------------------------------------------------------------------------
// --SECTION--                                                 protected methods
// -----------------------------------------------------------------------------

  private:

////////////////////////////////////////////////////////////////////////////////
/// @brief weak callback
////////////////////////////////////////////////////////////////////////////////

    static void weakCallback (const v8::WeakCallbackData<v8::Object, v8::Persistent<v8::Object>>& data) {
      auto isolate      = data.GetIsolate();
      auto persistent   = data.GetParameter();
      auto myPointer    = v8::Local<v8::Object>::New(isolate, *persistent);
      auto obj          = static_cast<V8Wrapper*>(myPointer->GetAlignedPointerFromInternalField(0))->_object;
        
      TRI_ASSERT(persistent == &obj->_handle);
      TRI_ASSERT(! obj->_refs);
      TRI_ASSERT(persistent->IsNearDeath());
      delete obj;
    }

  protected:

////////////////////////////////////////////////////////////////////////////////
/// @brief weakens the reference
////////////////////////////////////////////////////////////////////////////////

    void makeWeak () {
      _handle.SetWeak(&_handle, weakCallback);
    }

// -----------------------------------------------------------------------------
// --SECTION--                                               protected variables
// -----------------------------------------------------------------------------

  protected:

////////////////////////////////////////////////////////////////////////////////
/// @brief reference count
////////////////////////////////////////////////////////////////////////////////

    ssize_t _refs;

////////////////////////////////////////////////////////////////////////////////
/// @brief stored structure
////////////////////////////////////////////////////////////////////////////////

    STRUCT* _object;

////////////////////////////////////////////////////////////////////////////////
/// @brief free function for stored object
////////////////////////////////////////////////////////////////////////////////

    void (*_free) (STRUCT* object);

 public:
////////////////////////////////////////////////////////////////////////////////
/// @brief isolate
////////////////////////////////////////////////////////////////////////////////

    v8::Isolate* _isolate;

// -----------------------------------------------------------------------------
// --SECTION--                                            static private methods
// -----------------------------------------------------------------------------

};

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
