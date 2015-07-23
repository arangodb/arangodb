////////////////////////////////////////////////////////////////////////////////
/// @brief Helper class to isolate data protection with hazard pointers.
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

#ifndef ARANGODB_BASICS_DATA_GUARDIAN_H
#define ARANGODB_BASICS_DATA_GUARDIAN_H 1

namespace triagens {
  namespace basics {

////////////////////////////////////////////////////////////////////////////////
/// @brief DataGuardian, a template class to manage a single pointer to some
/// class, optimized for many fast readers and slow writers using lockfree
/// hazard pointer technology.
////////////////////////////////////////////////////////////////////////////////

    template<typename T>
    class DataGuardian {

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief HazardPtr, class for keeping a hazard pointer with appropriate
/// padding.
////////////////////////////////////////////////////////////////////////////////

        class HazardPtr {
          public:
            std::atomic<T const*> ptr;
          private:
            char padding[64-sizeof(std::atomic<T const*>)];
        };

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

        DataGuardian () {
          _P[0].ptr = nullptr;
          _P[1].ptr = nullptr;
          _V = 0;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

        ~DataGuardian () {
          std::lock_guard<std::mutex> lock(_mutex);
          while (isHazard(_P[_V].ptr)) {
            usleep(250);
          }
          T const* temp = _P[_V].ptr.load();
          delete temp;  // OK, if nullptr
          _P[_V].ptr = nullptr;
        }

        void registerHazard (HazardPtr& h) {
          std::lock_guard<std::mutex> lock(_mutex);
          _H.push_back(&h);
          return;
        }

        void unregisterHazard (HazardPtr& h) {
          std::lock_guard<std::mutex> lock(_mutex);
          for (auto it = _H.begin(); it != _H.end(); it++) {
            if (*it == &h) {
              _H.erase(it);
              return;
            }
          }
        }

        T const* lease (HazardPtr& h) {
          int v;
          T const* p;

          while (true) {
            v = _V.load(std::memory_order_consume);           // (XXX)
            // This memory_order_consume corresponds to the change to _V
            // in exchange() below which uses memory_order_seq_cst, which
            // implies release semantics. This is important to ensure that
            // we see the changes to _P just before the version _V
            // is flipped.
            p = _P[v].ptr.load(std::memory_order_relaxed);
            h.ptr = p;                 // implicit memory_order_seq_cst
            if (_V.load(std::memory_order_relaxed) != v) {    // (YYY)
              h.ptr = nullptr;         // implicit memory_order_seq_cst
              continue;
            }
            break;
          };
          return p;
        }

        void unlease (HazardPtr& h) {
          h.ptr = nullptr;             // implicit memory_order_seq_cst
        }

        T const* exchange (T const* replacement) {
          std::lock_guard<std::mutex> lock(_mutex);

          int v = _V.load(std::memory_order_relaxed);
          _P[1-v].ptr.store(replacement, std::memory_order_relaxed);
          _V = 1-v;      // implicit memory_order_seq_cst, whoever sees this
                         // also sees the two above modifications!
          // Our job is essentially done, we only need to destroy
          // the old value. However, this might be unsafe, because there might
          // be a reader. All readers have indicated their reading activity
          // with a store(std::memory_order_seq_cst) to _H[<theirId>]. After that
          // indication, they have rechecked the value of _V and have thus
          // confirmed that it was not yet changed. Therefore, we can simply
          // observe _H[*] and wait until none is equal to _P[v]:
          T const* p = _P[v].ptr.load(std::memory_order_relaxed);
          while (isHazard(p)) {
            usleep(250);
          }
          // Now it is safe to destroy _P[v]
          _P[v].ptr = nullptr;
          return p;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief internal check whether a given pointer is a hazard
////////////////////////////////////////////////////////////////////////////////

      private:

        bool isHazard (T const* p) {
          for (size_t i = 0; i < _H.size(); i++) {
            T const* g = _H[i]->ptr.load(std::memory_order_relaxed);
            if (g != nullptr && g == p) {
              return true;
            }
          }
          return false;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief the two versions of the pointer we take care of
////////////////////////////////////////////////////////////////////////////////

        HazardPtr _P[2];

////////////////////////////////////////////////////////////////////////////////
/// @brief current version of _P, can be 0 or 1
////////////////////////////////////////////////////////////////////////////////

        std::atomic<int> _V;

////////////////////////////////////////////////////////////////////////////////
/// @brief pointers to all hazard pointer structures that are registered
////////////////////////////////////////////////////////////////////////////////

        std::vector<HazardPtr*> _H;

////////////////////////////////////////////////////////////////////////////////
/// @brief a mutex, only used for slow operations
////////////////////////////////////////////////////////////////////////////////

        std::mutex _mutex;

      // Here is a proof that this is all OK: The mutex only ensures
      // that there is always only at most one mutating thread.
      // All is standard, except that we must ensure that whenever
      // _V is changed the mutating thread knows about all readers
      // that are still using the old version, which is done through
      // _H[myId]->ptr where id is the id of a thread. The critical
      // argument needed is the following: Both the change to
      // _H[myId]->ptr in lease() and the change to _V in exchange() use
      // memory_order_seq_cst, therefore they happen in some sequential
      // order and all threads observe the same order. If the reader
      // in line (YYY) above sees the same value as before in line
      // (XXX), then any write to _V must be later in the total order
      // of modifications than the change to _H[myId]->ptr. Therefore
      // the mutating thread must see the change to _H[myId]->ptr, after
      // all, it sees its own change to _V. Therefore it is ensured
      // that _P[v].ptr is returned only when all reading threads have
      // terminated their lease through unlease(), and thus it is safe
      // to be deleted.
    };

  }   // namespace triagens::basics
}   // namespace triagens

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
