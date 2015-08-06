////////////////////////////////////////////////////////////////////////////////
/// @brief Helper class to isolate data protection
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2015 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2015 triAGENS GmbH, Cologne, Germany
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
/// @author Copyright 2015, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2009-2015, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_BASICS_THREAD_PROTECTOR_H
#define ARANGODB_BASICS_THREAD_PROTECTOR_H 1

#include "Basics/Common.h"

namespace triagens {
  namespace basics {

////////////////////////////////////////////////////////////////////////////////
/// @brief ThreadProtector, a template class to manage a single atomic
/// value (can be a pointer to some class), optimized for many fast
/// readers and slow writers using lockfree technology.
/// The template parameter Nr should be on the order of magnitude of the
/// maximal number of concurrently running threads.
/// Usage:
///   Put an instance of the ThreadProtector next to the atomic value you
///   want to protect, as in:
///     atomic<SomeClass*> p;
///     ThreadProtector<64> prot;
///   If you want to read p and *p, do
///     auto unuser(prot.use());
///     auto pSeen = p;
///   in the scope where you want to read p and *p and then only use pSeen
///   and *pSeen.
///   It is automatically released when the unuser instances goes out of scope.
///   This is guaranteed to be very fast, even when multiple threads do it
///   concurrently.
///   If you want to change p (and call delete on the old value, say), then
///     auto oldp = p;       // save the old value of p
///     p = <new Value>;     // just assign p whenever you see fit
///     prot.scan();         // This will block until no thread is reading
///                          // the old value any more.
///     delete oldp;         // guaranteed to be safe
///   This can be a slow operation and only one thread should perform it 
///   at a time. Use a mutex to ensure this.
///   Please note:
///     - The value of p *can* change under the feet of the reading threads,
///       which is why you need to use the pSeen variable. However, you know
///       that as long as unused is in scope, pSeen remains valid.
///     - The ThreadProtector instances needs 64*Nr bytes of memory.
///     - ThreadProtector.cpp needs to contain an explicit template 
///       instanciation for all values of Nr used in the executable.
////////////////////////////////////////////////////////////////////////////////

    template<int Nr>
    class ThreadProtector {
        struct alignas(64) Entry {  // 64 is the size of a cache line,
             // it is important that different list entries lie in different
             // cache lines.
          std::atomic<int> _count;
        };

        Entry* _list;

        std::atomic<int> _last;
        static thread_local int _mySlot;

      public:

        // A class to automatically unuse the threadprotector:
        class UnUser {
            ThreadProtector* _prot;
            int _id;

          public:
            UnUser (ThreadProtector* p, int i) : _prot(p), _id(i) {
            }

            ~UnUser () {
              if (_prot != nullptr) {
                _prot->unUse(_id);
              }
            }

            // A move constructor
            UnUser (UnUser&& that) : _prot(that._prot), _id(that._id) {
              // Note that return value optimization will usually avoid
              // this move constructor completely. However, it has to be
              // present for the program to compile.
              that._prot = nullptr;
            }

            // Explicitly delete the others:
            UnUser (UnUser const& that) = delete;
            UnUser& operator= (UnUser const& that) = delete;
            UnUser& operator= (UnUser&& that) = delete;
            UnUser () = delete;
        };

        ThreadProtector () : _last(0) {
          _list = new Entry[Nr];
          // Just to be sure:
          for (size_t i = 0; i < Nr; i++) {
            _list[i]._count = 0;
          }
        }

        ~ThreadProtector () {
          delete[] _list;
        }

        UnUser use () {
          int id = _mySlot;
          if (id < 0) {
            id = _last++;
            if (_last > Nr) {
              _last = 0;
            }
            _mySlot = id;
          }
          _list[id]._count++;   // this is implicitly using memory_order_seq_cst
          return UnUser(this, id);  // return value optimization!
        }

        void scan () {
          for (size_t i = 0; i < Nr; i++) {
            while (_list[i]._count > 0) {
              usleep(250);
            }
          }
        }

      private:

        void unUse (int id) {
          _list[id]._count--;   // this is implicitly using memory_order_seq_cst
        }
    };

  }  // namespace triagens::basics
}   // namespace triagens

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
