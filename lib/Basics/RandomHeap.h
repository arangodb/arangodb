////////////////////////////////////////////////////////////////////////////////
/// @brief random heap
///
/// @file
/// This structure allows only conventional access to a heap (which is
/// by means of removing the head element). To use it, you must derive
/// from it and implement function "static K key(const T&)", to implement
/// random access you must derive from it and overload
/// "location"/"setLocation" accessor and implement function
/// "static K& key(T&)".
///
/// DISCLAIMER
///
/// Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
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
/// @author Richard Bruch
/// @author Copyright 2010-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_BASICS_RANDOM_HEAP_H
#define TRIAGENS_BASICS_RANDOM_HEAP_H 1

#include "Basics/Common.h"

#include "Basics/Exceptions.h"

namespace triagens {
  namespace basics {

// -----------------------------------------------------------------------------
// --SECTION--                                             struct HeapTraitsBase
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Utilities
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief random heap traits
////////////////////////////////////////////////////////////////////////////////

    template <typename T, typename K = T, typename Compare = std::less<K> >
    struct HeapTraitsBase {
        typedef K KeyType;

        bool operator() (const K& f, const K& s) const {
          return _compare(f, s);
        }

        static void setLocation (T&, size_t) {/* do nothing */
        }

        // we only access head of queue
        static size_t location (const T& item) {
          return 1;
        }

        Compare _compare;
    };

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 struct RandomHeap
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Utilities
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief random heap
////////////////////////////////////////////////////////////////////////////////

    template <typename T, typename TRAITS = HeapTraitsBase<T> >
    struct RandomHeap {
        typedef TRAITS TraitsType;
        typedef typename TraitsType::KeyType K;

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

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

        RandomHeap ()
          : _bufferLen(0), _traits() {
          _queue = new T[1];
          _queueLen = _bufferLen = 1;
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

////////////////////////////////////////////////////////////////////////////////
/// @brief referesh
///
// TODO find a better name for semantics of 'preserve'
////////////////////////////////////////////////////////////////////////////////

        bool refresh (T& item, const K& key, bool & preserve) {
          bool changed = false, inside = TraitsType::location(item);

          if (_queueLen > 1 && item == _queue[1]) {
            changed = true;
          }

          if (inside) {
            remove(item);
          }

          TraitsType::key(item) = key;

          if (preserve) {
            insert(item);
          }

          if (_queueLen > 1 && item == _queue[1]) {
            changed = true;
          }

          preserve = inside;

          return changed;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief compare
////////////////////////////////////////////////////////////////////////////////

        bool operator() (T& f, T& s) const {
          return _traits(TraitsType::key(f), TraitsType::key(s));
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief head
////////////////////////////////////////////////////////////////////////////////

        T& head () {
          if (_queueLen == 1) {
            THROW_INTERNAL_ERROR("head has size 1");
          }

          return _queue[1];
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief remove
////////////////////////////////////////////////////////////////////////////////

        void remove (T& item) {
          size_t i = TraitsType::location(item);

          assert(i < _queueLen);
          assert(!(*this)(_queue[i], item) && !(*this)(item, _queue[i]));

          TraitsType::setLocation(item, 0);
          T* iptr = _queue + i, *tail = _queue + --_queueLen;

          while ((i <<= 1) < _queueLen) {

            // select the smaller child of iptr and move it upheap
            T* pptr = _queue + i;

            if (i < _queueLen && (*this)(*(pptr + 1), *pptr)) {
              pptr++, i++;
            }

            *iptr = *pptr;
            TraitsType::setLocation(*iptr, iptr - _queue);
            iptr = pptr;
          }

          if (tail != iptr) {
            adjust(*tail, iptr - _queue);
          }

          *tail = T();
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief insert
////////////////////////////////////////////////////////////////////////////////

        void insert (T& item) {
          if (_bufferLen == _queueLen) { // no place for the new element

            // we use the simple doubling which should be OK in this case
            T* queue = new T[_bufferLen <<= 1];for(size_t i = 1; i < _queueLen; i ++) queue[i] = _queue[i];
            delete [] _queue;
            _queue = queue;
          }

          adjust(item, _queueLen ++);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief adjust
////////////////////////////////////////////////////////////////////////////////

        void adjust(T& item, int i) {
          T *iptr = _queue + i;

          while(i > 1) {
            T* pptr = _queue + (i >>= 1);

            if ((*this)(*pptr, item)) {
              break;
            }

            *iptr = *pptr;
            TraitsType::setLocation(*iptr, iptr - _queue);
            iptr = pptr;
          }

          *iptr = item;
          TraitsType::setLocation(item, iptr - _queue);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief validate
////////////////////////////////////////////////////////////////////////////////

        void validate () {
          for(size_t i = 1; i < _queueLen; i ++) {
            T child = _queue[i], *former = this->former(child);

            if (!former) {
              continue;
            }

            if ((*this)(child, former)) {
              assert(false);
              THROW_INTERNAL_ERROR("cannot validate random head");
            }
          }
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief former
////////////////////////////////////////////////////////////////////////////////

        T former (T wt) {
          size_t index = TraitsType::location(wt);

          if (index > 1) {
            return _queue[index >> 1];
          }

          return 0;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief empty
////////////////////////////////////////////////////////////////////////////////

        bool empty () {
          return _queueLen == 1;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief size
////////////////////////////////////////////////////////////////////////////////

        size_t size () {
          return _queueLen - 1;
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
/// @brief queue
////////////////////////////////////////////////////////////////////////////////

        T* _queue;

////////////////////////////////////////////////////////////////////////////////
/// @brief buffer length
////////////////////////////////////////////////////////////////////////////////

        size_t _bufferLen;

////////////////////////////////////////////////////////////////////////////////
/// @brief queue length
////////////////////////////////////////////////////////////////////////////////

        size_t _queueLen;

////////////////////////////////////////////////////////////////////////////////
/// @brief traits
////////////////////////////////////////////////////////////////////////////////

        TraitsType _traits;
    };
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:

