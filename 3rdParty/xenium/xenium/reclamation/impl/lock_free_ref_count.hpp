//
// Copyright (c) 2018 Manuel Pöter.
// Licensed under the MIT License. See LICENSE file in the project root for full license information.
//

#ifndef LOCK_FREE_REF_COUNT_IMPL
#error "This is an impl file and must not be included directly!"
#endif

namespace xenium { namespace reclamation {

    template<bool InsertPadding, size_t ThreadLocalFreeListSize>
    template<class T, std::size_t N, class Deleter>
    class lock_free_ref_count<InsertPadding, ThreadLocalFreeListSize>::enable_concurrent_ptr<T, N, Deleter>::free_list {
    public:
      T *pop() {
        if (max_local_elements > 0)
          if (auto result = local_free_list().pop())
            return result;

        guard_ptr guard;

        while (true) {
          // (1) - this acquire-load synchronizes-with the release-CAS (3)
          guard = acquire_guard(head, std::memory_order_acquire);
          if (guard.get() == nullptr)
            return nullptr;

          // Note: ref_count can be anything here since multiple threads
          // could have gotten a reference to the node on the freelist.
          marked_ptr expected(guard);
          auto next = guard->next_free().load(std::memory_order_relaxed);
          // since head is only changed via CAS operations it is sufficient to use relaxed order
          // for this operation as it is always part of a release-sequence headed by (3)
          if (head.compare_exchange_weak(expected, next, std::memory_order_relaxed))
          {
            assert((guard->ref_count().load(std::memory_order_relaxed) & RefCountClaimBit) != 0 &&
                   "ClaimBit must be set for a node on the free list");

            auto ptr = guard.get();
            ptr->ref_count().fetch_sub(RefCountClaimBit, std::memory_order_relaxed); // clear claim bit
            ptr->next_free().store(nullptr, std::memory_order_relaxed);
            guard.ptr.reset(); // reset guard_ptr to prevent decrement of ref_count
            return ptr;
          }
        }
      }

      void push(T *node) {
        assert(node->ref_count().load(std::memory_order_relaxed) & RefCountClaimBit &&
               "ClaimBit must be set for a node to be put on the free list");
        if (max_local_elements > 0 && local_free_list().push(node))
          return;

        add_nodes(node, node);
      }

    private:
      void add_nodes(T *first, T *last) {
        // (2) - this acquire-load synchronizes-with the release-CAS (3)
        auto old = head.load(std::memory_order_acquire);
        do {
          last->next_free().store(old, std::memory_order_relaxed);
          // (3) - if this release-CAS succeeds, it synchronizes-with the acquire-loads (1, 2)
          //       if it failes, the reload synchronizes-with itself (3)
        } while (!head.compare_exchange_weak(old, first, std::memory_order_release, std::memory_order_acquire));
      }

      // the free list is implemented as a FILO single linked list
      // the LSB of a node's ref_count acts as claim bit, so for all nodes on the free list the bit has to be set
      concurrent_ptr <T, N> head;

      class thread_local_free_list {
      public:
        ~thread_local_free_list() noexcept {
          if (head == nullptr)
            return;
          auto first = head;
          auto last = head;
          auto next = last->next_free().load(std::memory_order_relaxed);
          while (next) {
            last = next.get();
            next = next->next_free().load(std::memory_order_relaxed);
          }
          global_free_list.add_nodes(first, last);
        }

        bool push(T *node) {
          if (number_of_elements >= max_local_elements)
            return false;
          node->next_free().store(head, std::memory_order_relaxed);
          head = node;
          ++number_of_elements;
          return true;
        }

        T *pop() {
          auto result = head;
          if (result) {
            assert(number_of_elements > 0);
            head = result->next_free().load(std::memory_order_relaxed).get();
            --number_of_elements;
            // clear claim bit and increment ref_count
            result->ref_count().fetch_add(RefCountInc - RefCountClaimBit, std::memory_order_relaxed);
            result->next_free().store(nullptr, std::memory_order_relaxed);
          }
          return result;
        }

      private:
        size_t number_of_elements = 0;
        T *head = nullptr;
      };

      static constexpr size_t max_local_elements = ThreadLocalFreeListSize;

      static thread_local_free_list &local_free_list() {
        // workaround for gcc issue causing redefinition of __tls_guard when
        // defining this as static thread_local member of free_list.
        alignas(64) static thread_local thread_local_free_list local_free_list;
        return local_free_list;
      }
    };

    template<bool InsertPadding, size_t ThreadLocalFreeListSize>
    template<class T, std::size_t N, class Deleter>
    void* lock_free_ref_count<InsertPadding, ThreadLocalFreeListSize>::
    enable_concurrent_ptr<T, N, Deleter>::operator new(size_t sz) {
      assert(sz == sizeof(T) && "Cannot handle allocations of anything other than T instances");
      T *result = global_free_list.pop();
      if (result == nullptr) {
        auto h = static_cast<header *>(::operator new(sz + sizeof(header)));
        h->ref_count.store(RefCountInc, std::memory_order_release);
        result = static_cast<T *>(static_cast<void *>(h + 1));
      }

      return result;
    }

    template<bool InsertPadding, size_t ThreadLocalFreeListSize>
    template<class T, std::size_t N, class Deleter>
    void lock_free_ref_count<InsertPadding, ThreadLocalFreeListSize>::
    enable_concurrent_ptr<T, N, Deleter>::operator delete(void *p) {
      auto node = static_cast<T *>(p);
      assert((node->ref_count().load(std::memory_order_relaxed) & RefCountClaimBit) == 0);

      if (node->decrement_refcnt())
        node->push_to_free_list();
    }

    template<bool InsertPadding, size_t ThreadLocalFreeListSize>
    template<class T, std::size_t N, class Deleter>
    bool lock_free_ref_count<InsertPadding, ThreadLocalFreeListSize>::
    enable_concurrent_ptr<T, N, Deleter>::decrement_refcnt() {
      unsigned old_refcnt, new_refcnt;
      do {
        old_refcnt = ref_count().load(std::memory_order_relaxed);
        new_refcnt = old_refcnt - RefCountInc;
        if (new_refcnt == 0)
          new_refcnt = RefCountClaimBit;
        // (4) - this release/acquire CAS synchronizes with itself
      } while (!ref_count().compare_exchange_weak(old_refcnt, new_refcnt,
                                                  new_refcnt == RefCountClaimBit
                                                  ? std::memory_order_acquire
                                                  : std::memory_order_release,
                                                  std::memory_order_relaxed));

      // free node iff ref_count is zero AND we're the first thread to "claim" this node for reclamation.
      return (old_refcnt - new_refcnt) & RefCountClaimBit;
    }

    template<bool InsertPadding, size_t ThreadLocalFreeListSize>
    template<class T, class MarkedPtr>
    lock_free_ref_count<InsertPadding, ThreadLocalFreeListSize>::
    guard_ptr<T, MarkedPtr>::guard_ptr(const MarkedPtr &p) noexcept :
      base(p) {
      if (this->ptr.get() != nullptr)
        this->ptr->ref_count().fetch_add(RefCountInc, std::memory_order_relaxed);
    }

    template<bool InsertPadding, size_t ThreadLocalFreeListSize>
    template<class T, class MarkedPtr>
    lock_free_ref_count<InsertPadding, ThreadLocalFreeListSize>::
    guard_ptr<T, MarkedPtr>::guard_ptr(const guard_ptr &p) noexcept :
      guard_ptr(p.ptr) {}

    template<bool InsertPadding, size_t ThreadLocalFreeListSize>
    template<class T, class MarkedPtr>
    lock_free_ref_count<InsertPadding, ThreadLocalFreeListSize>::
    guard_ptr<T, MarkedPtr>::guard_ptr(guard_ptr &&p) noexcept :
      base(p.ptr) {
      p.ptr.reset();
    }

    template<bool InsertPadding, size_t ThreadLocalFreeListSize>
    template<class T, class MarkedPtr>
    auto lock_free_ref_count<InsertPadding, ThreadLocalFreeListSize>::
    guard_ptr<T, MarkedPtr>::operator=(const guard_ptr &p)
    -> guard_ptr & {
      if (&p == this)
        return *this;

      reset();
      this->ptr = p.ptr;
      if (this->ptr.get() != nullptr)
        this->ptr->ref_count().fetch_add(RefCountInc, std::memory_order_relaxed);
      return *this;
    }

    template<bool InsertPadding, size_t ThreadLocalFreeListSize>
    template<class T, class MarkedPtr>
    auto lock_free_ref_count<InsertPadding, ThreadLocalFreeListSize>::
    guard_ptr<T, MarkedPtr>::operator=(guard_ptr &&p)
    -> guard_ptr & {
      if (&p == this)
        return *this;

      reset();
      this->ptr = std::move(p.ptr);
      p.ptr.reset();
      return *this;
    }

    template<bool InsertPadding, size_t ThreadLocalFreeListSize>
    template<class T, class MarkedPtr>
    void lock_free_ref_count<InsertPadding, ThreadLocalFreeListSize>::
    guard_ptr<T, MarkedPtr>::acquire(const concurrent_ptr <T> &p, std::memory_order order) noexcept {
      for (;;) {
        reset();
        // FIXME: If this load is relaxed, TSan reports a data race between the following
        // fetch-add and the initialization of the ref_count field. I tend to disagree, as
        // the fetch_add should be ordered after the initial store (operator new) in the
        // modification order of ref_count. Therefore the acquire-fetch-add should
        // synchronize-with the release store.
        // I created a GitHub issue:  
        // But for now, let's make this an acquire-load to make TSan happy.
        auto q = p.load(std::memory_order_acquire);
        this->ptr = q;
        if (q.get() == nullptr)
          return;

        // (5) - this acquire-fetch_add synchronizes-with the release-fetch_sub (7)
        // this ensures that a change to p becomes visible
        q->ref_count().fetch_add(RefCountInc, std::memory_order_acquire);

        if (q == p.load(order))
          return;
      }
    }

    template<bool InsertPadding, size_t ThreadLocalFreeListSize>
    template<class T, class MarkedPtr>
    bool lock_free_ref_count<InsertPadding, ThreadLocalFreeListSize>::
    guard_ptr<T, MarkedPtr>::acquire_if_equal(
      const concurrent_ptr <T> &p, const MarkedPtr &expected, std::memory_order order) noexcept {
      reset();
      // FIXME: same issue with TSan as in acquire (see above).
      auto q = p.load(std::memory_order_acquire);
      if (q != expected)
        return false;

      this->ptr = q;
      if (q.get() == nullptr)
        return true;

      // (6) - this acquire-fetch_add synchronizes-with the release-fetch_sub (7)
      // this ensures that a change to p becomes visible
      q->ref_count().fetch_add(RefCountInc, std::memory_order_acquire);

      if (q == p.load(order))
        return true;

      reset();
      return false;
    }

    template<bool InsertPadding, size_t ThreadLocalFreeListSize>
    template<class T, class MarkedPtr>
    void lock_free_ref_count<InsertPadding, ThreadLocalFreeListSize>::
    guard_ptr<T, MarkedPtr>::reset() noexcept {
      auto p = this->ptr.get();
      this->ptr.reset();
      if (p == nullptr)
        return;

      if (p->decrement_refcnt()) {
        if (!p->is_destroyed())
          p->~T();

        p->push_to_free_list();
      }
    }

    template<bool InsertPadding, size_t ThreadLocalFreeListSize>
    template<class T, class MarkedPtr>
    void lock_free_ref_count<InsertPadding, ThreadLocalFreeListSize>::
    guard_ptr<T, MarkedPtr>::reclaim(Deleter d) noexcept {
      if (this->ptr.get() != nullptr) {
        assert(this->ptr->refs() > 1);
        // ref_count was initialized with "1", so we need an additional
        // decrement to ensure that the node gets reclaimed.
        // ref_count cannot drop to zero here -> no check required.
        // (7) - this release-fetch-sub synchronizes-with the acquire-fetch-add (5, 6)
        this->ptr->ref_count().fetch_sub(RefCountInc, std::memory_order_release);
      }
      reset();
    }

    template<bool InsertPadding, size_t ThreadLocalFreeListSize>
    template<class T, std::size_t N, class Deleter>
    typename lock_free_ref_count<InsertPadding, ThreadLocalFreeListSize>::
    template enable_concurrent_ptr<T, N, Deleter>::free_list
      lock_free_ref_count<InsertPadding, ThreadLocalFreeListSize>::
      enable_concurrent_ptr<T, N, Deleter>::global_free_list;

#ifdef TRACK_ALLOCATIONS
    template <bool InsertPadding, size_t ThreadLocalFreeListSize>
    detail::allocation_tracker lock_free_ref_count<InsertPadding, ThreadLocalFreeListSize>::allocation_tracker;

    template <bool InsertPadding, size_t ThreadLocalFreeListSize>
    inline detail::allocation_counter&
      lock_free_ref_count<InsertPadding, ThreadLocalFreeListSize>::allocation_counter()
    {
      static thread_local detail::registered_allocation_counter<lock_free_ref_count> counter;
      return counter;
    };

    template <bool InsertPadding, size_t ThreadLocalFreeListSize>
    inline void lock_free_ref_count<InsertPadding, ThreadLocalFreeListSize>::count_allocation()
    { allocation_counter().count_allocation(); }

    template <bool InsertPadding, size_t ThreadLocalFreeListSize>
    inline void lock_free_ref_count<InsertPadding, ThreadLocalFreeListSize>::count_reclamation()
    { allocation_counter().count_reclamation(); }
#endif
}}
