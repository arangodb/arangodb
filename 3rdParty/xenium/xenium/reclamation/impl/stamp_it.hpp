//
// Copyright (c) 2018 Manuel Pöter.
// Licensed under the MIT License. See LICENSE file in the project root for full license information.
//

#ifndef STAMP_IT_IMPL
#error "This is an impl file and must not be included directly!"
#endif

#include <xenium/aligned_object.hpp>
#include <xenium/reclamation/detail/perf_counter.hpp>
#include <xenium/detail/port.hpp>
#include <xenium/reclamation/detail/thread_block_list.hpp>

#include <algorithm>

namespace xenium { namespace reclamation {

  struct stamp_it::thread_control_block :
    aligned_object<thread_control_block, 1 << MarkBits>,
    detail::thread_block_list<thread_control_block>::entry
  {
    using concurrent_ptr = std::atomic<detail::marked_ptr<thread_control_block, MarkBits>>;

    concurrent_ptr prev;
    concurrent_ptr next;

    std::atomic<stamp_t> stamp;

#ifdef WITH_PERF_COUNTER
    performance_counters counters;
    friend class thread_order_queue;
#endif
  };

  class stamp_it::thread_order_queue
  {
  public:
    using marked_ptr = detail::marked_ptr<thread_control_block, MarkBits>;
    using concurrent_ptr = std::atomic<marked_ptr>;

    thread_order_queue()
    {
      head = new thread_control_block();
      tail = new thread_control_block();
      tail->next.store(head, std::memory_order_relaxed);
      tail->stamp.store(StampInc, std::memory_order_relaxed);
      head->prev.store(tail, std::memory_order_relaxed);
      head->stamp.store(StampInc, std::memory_order_relaxed);
    }

    void push(thread_control_block* block)
    {
      INC_PERF_CNT(block->counters.push_calls);
      PERF_COUNTER(iterations, block->counters.push_iterations)

      // (1) - this release-store synchronizes-with the acquire-loads (7, 8, 20, 24, 25, 29, 31, 32)
      block->next.store(make_clean_marked(head, block->next), std::memory_order_release);

      marked_ptr head_prev = head->prev.load(std::memory_order_relaxed);
      marked_ptr my_prev;
      size_t stamp;
      for (;;)
      {
        iterations.inc();

        assert((head_prev.mark() & DeleteMark) == 0 && "head must never be marked");
        marked_ptr head_prev2 = head->prev.load(std::memory_order_relaxed);
        if (head_prev != head_prev2)
        {
          head_prev = head_prev2;
          continue;
        }

        // fetch a new stamp and set the PendingPush flag
        // (2) - this seq_cst-fetch-add enforces a total order with (12)
        //       and synchronizes-with the acquire-loads (19, 23)
        stamp = head->stamp.fetch_add(StampInc, std::memory_order_seq_cst);
        auto pending_stamp = stamp - (StampInc - PendingPush);
        assert((pending_stamp & PendingPush) && !(pending_stamp & NotInList));

        // We need release-semantic here to establish synchronize-with relation with
        // the load in save_next_as_last_and_move_next_to_next_prev.
        // (3) - this release-store synchronizes-with the acquire-loads (19, 23, 30)
        block->stamp.store(pending_stamp, std::memory_order_release);

        if (head->prev.load(std::memory_order_relaxed) != head_prev)
          continue;

        my_prev = make_clean_marked(head_prev.get(), block->prev);

        // (4) - this release-store synchronizes-with the acquire-loads (15, 17, 18, 22, 26)
        block->prev.store(my_prev, std::memory_order_release);

        // (5) - in this acq_rel-CAS the:
        //       - acquire-load synchronizes-with the release-stores (5, 21, 28)
        //       - release-store synchronizes-with the acquire-loads (5, 15, 18, 22)
        if (head->prev.compare_exchange_weak(head_prev, make_marked(block, head_prev),
                                             std::memory_order_acq_rel,
                                             std::memory_order_relaxed))
          break;
        // Back-Off
      }

      // reset the PendingPush flag
      // (6) - this release-store synchronizes-with the acquire-load (19, 23, 30)
      block->stamp.store(stamp, std::memory_order_release);

      // try to update our successor's next pointer
      // (7) - this acquire-load synchronizes-with the release-stores (1, 8, 27)
      auto link = my_prev->next.load(std::memory_order_acquire);
      for (;;)
      {
        if (link.get() == block ||
            link.mark() & DeleteMark ||
            block->prev.load(std::memory_order_relaxed) != my_prev)
          break; // our successor is in the process of getting removed or has been removed already -> never mind

        assert(link.get() != tail);

        // (8) - in this CAS the:
        //       - release-store synchronizes-with the acquire-loads (7, 8, 14, 20, 24, 25, 29, 31, 32)
        //       - acquire-reload synchronizes-with the release-stores (1, 8, 27)
        if (my_prev->next.compare_exchange_weak(link, make_marked(block, link),
                                             std::memory_order_release,
                                             std::memory_order_acquire))
          break;
        // Back-Off
      }
    }

    bool remove(marked_ptr block)
    {
      INC_PERF_CNT(block->counters.remove_calls);

      // We need acq-rel semantic here to ensure the happens-before relation
      // between the remove operation and the reclamation of any node.
      //  - acquire to establish sychnronize-with relation with previous blocks
      //    that removed themselves by updating our prev.
      //  - release to establish synchronize-with relation with other threads
      //    that potentially remove our own block before we can do so.

      // (9) - in this acq_rel CAS the:
      //       - acquire-load synchronizes-with the release-stores (21, 28)
      //       - release-store synchronizes-with the acquire-loads (15, 17, 18, 22, 26)
      marked_ptr prev = set_mark_flag(block->prev, std::memory_order_acq_rel);
      marked_ptr next = set_mark_flag(block->next, std::memory_order_relaxed);

      bool fully_removed = remove_from_prev_list(prev, block, next);
      if (!fully_removed)
        remove_from_next_list(prev, block, next);

      auto stamp = block->stamp.load(std::memory_order_relaxed);
      assert((stamp & (PendingPush | NotInList)) == 0);
      // set the NotInList flag to signal that this block is no longer part of the queue
      block->stamp.store(stamp + NotInList, std::memory_order_relaxed);

      bool wasTail = block->prev.load(std::memory_order_relaxed).get() == tail;
      if (wasTail)
      {
        // since the stamps of the blocks between tail and head are strong monotonically increasing we can
        // call update_tail_stamp with the next higher stamp (i.e., stamp + StampInc) as the 'next best guess'
        update_tail_stamp(stamp + StampInc);
      }

      return wasTail;
    }

    void add_to_global_retired_nodes(deletable_object_with_stamp* chunk)
    {
      add_to_global_retired_nodes(chunk, chunk);
    }

    void add_to_global_retired_nodes(deletable_object_with_stamp* first_chunk, deletable_object_with_stamp* last_chunk)
    {
      assert(first_chunk != nullptr && last_chunk != nullptr);
      auto n = global_retired_nodes.load(std::memory_order_relaxed);
      do
      {
        last_chunk->next_chunk = n;
        // (10) - this release-CAS synchronizes-with the acquire_xchg (11)
      } while (!global_retired_nodes.compare_exchange_weak(n, first_chunk,
                                                           std::memory_order_release,
                                                           std::memory_order_relaxed));
    }

    deletable_object_with_stamp* steal_global_retired_nodes()
    {
      if (global_retired_nodes.load(std::memory_order_relaxed) != nullptr)
        // (11) - this acquire-xchg synchronizes-with the release-CAS (10)
        return global_retired_nodes.exchange(nullptr, std::memory_order_acquire);
      return nullptr;
    }

    stamp_t head_stamp() {
      // (12) - this seq-cst-load enforces a total order with (2)
      return head->stamp.load(std::memory_order_seq_cst);
    }
    stamp_t tail_stamp() {
      // (13) - this acquire-load synchronizes-with the release-CAS (16)
      return tail->stamp.load(std::memory_order_acquire);
    }

    thread_control_block* acquire_control_block()
    {
      return global_thread_block_list.acquire_entry();
    }

  private:
    static const unsigned DeleteMark = 1;
    static const unsigned TagInc = 2;
    static const unsigned MarkMask = (1 << MarkBits) - 1;

    marked_ptr make_marked(thread_control_block* p, const marked_ptr& mark)
    {
      return marked_ptr(p, (mark.mark() + TagInc) & MarkMask);
    }

    marked_ptr make_clean_marked(thread_control_block* p, const concurrent_ptr& mark)
    {
      auto m = mark.load(std::memory_order_relaxed);
      return marked_ptr(p, (m.mark() + TagInc) & MarkMask & ~DeleteMark);
    }

    void update_tail_stamp(size_t stamp)
    {
      // In the best case the stamp of tail equals the stamp of tail's predecessor (in prev
      // direction), but we don't want to waste too much time finding the "actual" predecessor.
      // Therefore we simply check whether the block referenced by tail->next is the actual
      // predecessor and if so take its stamp. Otherwise we simply use the stamp that was
      // passed (which is kind of a "best guess").
      // (14) - this acquire-load synchronizes-with the release-stores (8, 27)
      auto last = tail->next.load(std::memory_order_acquire);
      // (15) - this acquire-load synchronizes-with the release-stores (4, 5, 9, 21, 28)
      auto last_prev = last->prev.load(std::memory_order_acquire);
      auto last_stamp = last->stamp.load(std::memory_order_relaxed);
      if (last_stamp > stamp &&
          last_prev.get() == tail &&
          tail->next.load(std::memory_order_relaxed) == last)
      {
        assert((last_stamp & PendingPush) == 0);
        assert((last_stamp & NotInList) == 0);
        assert(last_stamp >= stamp);
        if (last.get() != head)
          stamp = last_stamp;
        else
        {
          // Special case when we take the stamp from head - the stamp in head gets incremented
          // before a new block is actually inserted, but we must not use such a value if the block
          // is not yet inserted. By updating prev with an incremented version a pending insertion
          // would fail and cause a retry, therefore enforcing the strict odering.
          // However, since we are potentially disturbing push operations, we only want to do this if
          // it is "worth it", i.e., if the stamp we read from head is at least one increment ahead
          // of our "next best guess".
          if (stamp < last_stamp - StampInc &&
              head->prev.compare_exchange_strong(last_prev, make_marked(last_prev.get(), last_prev),
                                                 std::memory_order_relaxed))
            stamp = last_stamp;
        }
      }

      // Try to update tail->stamp, but only as long as our new value is actually greater.
      auto tail_stamp = tail->stamp.load(std::memory_order_relaxed);
      while (tail_stamp < stamp)
      {
        // (16) - this release-CAS synchronizes-with the acquire-load (13)
        if (tail->stamp.compare_exchange_weak(tail_stamp, stamp, std::memory_order_release))
          break;
      }
    }

    bool remove_from_prev_list(marked_ptr& prev, marked_ptr b, marked_ptr& next)
    {
      PERF_COUNTER(iterations, b->counters.remove_prev_iterations);

      const auto my_stamp = b->stamp.load(std::memory_order_relaxed);
      marked_ptr last = nullptr;
      for (;;)
      {
        iterations.inc();

        // check if the block is already deleted
        if (next.get() == prev.get())
        {
          next = b->next.load(std::memory_order_relaxed);
          return false;
        }

        auto prev_prev = prev->prev.load(std::memory_order_relaxed);
        auto prev_stamp = prev->stamp.load(std::memory_order_relaxed);

        // check if prev has been removed
        if (prev_stamp > my_stamp || // prev has been reinserted already
            prev_stamp & NotInList)  // prev has been removed
        {
          return true;
        }

        if (prev_prev.mark() & DeleteMark)
        {
          if (!mark_next(prev, prev_stamp))
          {
            // prev is marked for deletion, but mark_next failed because the stamp
            // of prev has been updated - i.e., prev has been deleted already (and
            // maybe even reinserted)
            // -> this implies that b must have been removed as well.
            return true;
          }
          // This acquire-reload is needed to establish a happens-before relation
          // between the remove operations and the reclamation of a node.
          // (17) - this acquire-load synchronizes-with the release-stores (4, 9, 21, 28)
          prev = prev->prev.load(std::memory_order_acquire);
          continue;
        }

        // We need need to obtain a consistent set of "prev" and "stamp" values
        // from next, otherwise we could wrongfully update next_prev's stamp in
        // save_next_as_last_and_move_next_to_next_prev, since we cannot be sure
        // if the "prev" value we see in the reload belongs to a block that is
        // part of the list.

        // (18) - this acquire-load synchronizes-with the release-stores (4, 5, 9, 21, 28)
        auto next_prev = next->prev.load(std::memory_order_acquire);
        // (19) - this acquire-load synchronizes-with the release-stores (2, 3, 6)
        auto next_stamp = next->stamp.load(std::memory_order_acquire);

        if (next_prev != next->prev.load(std::memory_order_relaxed))
          continue;

        if (next_stamp < my_stamp)
        {
          next = b->next.load(std::memory_order_relaxed);
          return false;
        }

        // Check if next has been removed from list or whether it is currently getting inserted.
        // It could be that the block is already inserted, but the PendingPush flag has not yet
        // been cleared. Unfortunately, there is no way to identify this case here, so we have
        // to go back yet another block.
        // We can help resetting this flag once we are sure that the block is already part of the
        // list, which is exactly what happens in save_next_as_last_and_move_next_to_next_prev.
        if (next_stamp & (NotInList | PendingPush))
        {
          if (last.get() != nullptr)
          {
            next = last;
            last.reset();
          }
          else
            // (20) - this acquire-load synchronizes-with the release-stores (1, 8, 27)
            next = next->next.load(std::memory_order_acquire);
          continue;
        }

        if (remove_or_skip_marked_block(next, last, next_prev, next_stamp))
          continue;

        // check if next is the predecessor of b
        if (next_prev.get() != b.get())
        {
          save_next_as_last_and_move_next_to_next_prev(next_prev, next, last);
          continue;
        }

        // unlink "b" from prev list
        // (21) - this release-CAS synchronizes-with the acquire-loads (5, 9, 15, 17, 18, 22, 26)
        if (next->prev.compare_exchange_strong(next_prev, make_marked(prev.get(), next_prev),
                                               std::memory_order_release,
                                               std::memory_order_relaxed))
          return false;

        // Back-Off
      }
    }

    void remove_from_next_list(marked_ptr prev, marked_ptr removed, marked_ptr next)
    {
      PERF_COUNTER(iterations, removed->counters.remove_next_iterations);

      const auto my_stamp = removed->stamp.load(std::memory_order_relaxed);

      marked_ptr last = nullptr;
      for (;;)
      {
        iterations.inc();

        // FIXME - why do we have to load next_prev before prev_next??
        // We have to ensure that next is part of the list before obtaining the
        // pointer we have to update, in order to ensure that we don't set the
        // pointer to a block that has been removed in the meantime.

        // (22) - this acquire-load synchronizes-with the release-stores (4, 5, 9, 21, 28)
        auto next_prev = next->prev.load(std::memory_order_acquire);
        // (23) - this acquire-load synchronizes-with the release-stores (2, 3, 6)
        auto next_stamp = next->stamp.load(std::memory_order_acquire);

        if (next_prev != next->prev.load(std::memory_order_relaxed))
          continue;

        // check if next has been removed from list
        if (next_stamp & (NotInList | PendingPush))
        {
          if (last.get() != nullptr)
          {
            next = last;
            last.reset();
          }
          else
          {
            // (24) - this acquire-load synchronizes-with the release-stores (1, 8, 27)
            next = next->next.load(std::memory_order_acquire);
          }
          continue;
        }

        // (25) - this acquire-load synchronizes-with the release-stores (1, 8, 27)
        auto prev_next = prev->next.load(std::memory_order_acquire);
        auto prev_stamp = prev->stamp.load(std::memory_order_relaxed);

        assert(prev.get() != removed.get() || prev_stamp > my_stamp);

        // check if prev has a higher stamp than the block we want to remove.
        if (prev_stamp > my_stamp || prev_stamp & NotInList)
        {
          // due to strict order of stamps the prev block must have been removed already - and with it b.
          return;
        }

        // check if prev block is marked for deletion
        if (prev_next.mark() & DeleteMark)
        {
          // This acquire-load is needed to establish a happens-before relation
          // between the different remove operations and the reclamation of a node.
          // (26) - this acquire-load synchronizes-with the release-stores (4, 9, 21, 28)
          prev = prev->prev.load(std::memory_order_acquire);
          continue;
        }

        if (next.get() == prev.get())
          return;

        if (remove_or_skip_marked_block(next, last, next_prev, next_stamp))
          continue;

        // check if next is the predecessor of prev
        if (next_prev.get() != prev.get())
        {
          save_next_as_last_and_move_next_to_next_prev(next_prev, next, last);
          continue;
        }

        if (next_stamp <= my_stamp || prev_next.get() == next.get())
          return;

        auto new_next = make_marked(next.get(), prev_next);
        if (next->prev.load(std::memory_order_relaxed) == next_prev &&
            // (27) - this release-CAS synchronizes-with the acquire-loads (7, 8, 14, 20, 24, 25, 29, 31, 32)
            prev->next.compare_exchange_weak(prev_next, new_next,
                                             std::memory_order_release,
                                             std::memory_order_relaxed))
        {
          if ((next->next.load(std::memory_order_relaxed).mark() & DeleteMark) == 0)
            return;
        }
        // Back-Off
      }
    }

    bool remove_or_skip_marked_block(marked_ptr &next, marked_ptr &last,
                                     marked_ptr next_prev, stamp_t next_stamp)
    {
      // check if next is marked
      if (next_prev.mark() & DeleteMark)
      {
        if (last.get() != nullptr)
        {
          // check if next has "overtaken" last
          assert((next.mark() & DeleteMark) == 0);
          if (mark_next(next, next_stamp) &&
              last->prev.load(std::memory_order_relaxed) == next)
          {
            // unlink next from prev-list
            // (28) - this release-CAS synchronizes-with the acquire-loads (5, 9, 15, 17, 18, 22, 26)
            last->prev.compare_exchange_strong(next, make_marked(next_prev.get(), next),
                                               std::memory_order_release,
                                               std::memory_order_relaxed);
          }
          next = last;
          last.reset();
        }
        else
          // (29) - this acquire-load synchronizes-with the release-stores (1, 8, 27)
          next = next->next.load(std::memory_order_acquire);

        return true;
      }
      return false;
    }

    void save_next_as_last_and_move_next_to_next_prev(
      marked_ptr next_prev, marked_ptr& next, marked_ptr& last)
    {
      // (30) - this acquire-load synchronizes-with the release-stores (3, 6)
      size_t next_prev_stamp = next_prev->stamp.load(std::memory_order_acquire);

      if (next_prev_stamp & PendingPush && next_prev == next->prev.load(std::memory_order_relaxed))
      {
        assert((next_prev_stamp & NotInList) == 0);
        // since we got here via an (unmarked) prev pointer next_prev has been added to
        // the "prev-list", but the PendingPush flag has not been cleared yet.
        // i.e., the push operation for next_prev is still pending -> help clear the PendingPush flag
        auto expected = next_prev_stamp;
        const auto new_stamp = next_prev_stamp + (StampInc - PendingPush);
        if (!next_prev->stamp.compare_exchange_strong(expected, new_stamp, std::memory_order_relaxed))
        {
          // CAS operation failed, i.e., the stamp of next_prev has been changed
          // since we read it. Check if some other thread cleared the flag already
          // or whether next_prev has been removed (and potentially readded).
          if (expected != new_stamp)
          {
            // the stamp has been updated to an unexpected value, so next_prev has been removed
            // already -> we cannot move to next_prev, but we can keep the current next and last.
            return;
          }
        }
      }
      last = next;
      next = next_prev;
    }

    marked_ptr set_mark_flag(concurrent_ptr& ptr, std::memory_order order)
    {
      auto link = ptr.load(std::memory_order_relaxed);
      for (;;)
      {
        if (link.mark() & DeleteMark ||
            ptr.compare_exchange_weak(link, marked_ptr(link.get(), link.mark() | DeleteMark),
                                      order, std::memory_order_relaxed))
          return link;
      }
    }

    // tries to mark the next ptr of 'block', as long as block's stamp matches the given stamp
    bool mark_next(marked_ptr block, size_t stamp)
    {
      assert((stamp & (NotInList | PendingPush)) == 0);
      // (31) - this acquire-load synchronizes-with the release-stores (1, 8, 27)
      auto link = block->next.load(std::memory_order_acquire);
      // We need acquire to synchronize-with the release store in push. This way it is
      // guaranteed that the following stamp.load sees the NotInList flag or some newer
      // stamp, thus causing termination of the loop.
      while (block->stamp.load(std::memory_order_relaxed) == stamp)
      {
        auto mark = link.mark();
        if (mark & DeleteMark ||
            // (32) - this acquire-reload synchronizes-with the release-stores (1, 8, 27)
            block->next.compare_exchange_weak(link,
                                              marked_ptr(link.get(), mark | DeleteMark),
                                              std::memory_order_relaxed,
                                              std::memory_order_acquire))
          // Note: the C++11 standard states that "the failure argument shall be no stronger than
          // the success argument". However, this has been relaxed in C++17.
          // (see http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2016/p0418r1.html)
          // Some implementations (e.g., the Microsoft STL) perform runtime checks to enforce these
          // requirements. So in case the above CAS operation causes runtime errors, one has to use
          // release instead of relaxed order.
          return true;
      }
      return false;
    }

    thread_control_block* head;
    thread_control_block* tail;

    alignas(64) std::atomic<deletable_object_with_stamp*> global_retired_nodes;
    alignas(64) detail::thread_block_list<thread_control_block> global_thread_block_list;
    friend class stamp_it;
  };

  struct stamp_it::thread_data
  {
    ~thread_data()
    {
      assert(region_entries == 0);
      if (control_block)
      {
        control_block->abandon();
        control_block = nullptr;
      }

      // reclaim as much nodes as possible
      process_local_nodes();
      if (first_retired_node)
      {
        // we still have retired nodes that cannot yet be reclaimed
        // -> add them to the global list.
        queue.add_to_global_retired_nodes(first_retired_node);
      }
    }

    void enter_region()
    {
      if (++region_entries == 1)
      {
        ensure_has_control_block();
        queue.push(control_block);
      }
    }

    void leave_region()
    {
      if (--region_entries == 0)
      {
        auto wasLast = queue.remove(control_block);

        if (wasLast)
          process_global_nodes();
        else
        {
          process_local_nodes();
          if (number_of_retired_nodes > max_remaining_retired_nodes)
          {
            queue.add_to_global_retired_nodes(first_retired_node);
            first_retired_node = nullptr;
            prev_retired_node = &first_retired_node;
            number_of_retired_nodes = 0;
          }
        }
      }
    }

    void add_retired_node(deletable_object_with_stamp* p)
    {
      p->stamp = queue.head_stamp();
      *prev_retired_node = p;
      prev_retired_node = &p->next;
      
      ++number_of_retired_nodes;
      if (number_of_retired_nodes > try_reclaim_threshold)
        process_local_nodes();
    }

  private:
    void ensure_has_control_block()
    {
      if (control_block == nullptr)
      {
        control_block = queue.acquire_control_block();
#ifdef WITH_PERF_COUNTER
        control_block->counters = performance_counters{}; // reset counters
#endif
      }
    }

    void process_local_nodes()
    {
      auto tail_stamp = queue.tail_stamp();
      std::size_t cnt = 0;
      auto cur = first_retired_node;
      for (deletable_object_with_stamp* next = nullptr; cur != nullptr; cur = next)
      {
        next = cur->next;
        if (cur->stamp <= tail_stamp)
        {
          cur->delete_self();
          ++cnt;
        }
        else
          break;
      }

      first_retired_node = cur;
      if (cur == nullptr)
        prev_retired_node = &first_retired_node;
      number_of_retired_nodes -= cnt;
    }

    void process_global_nodes()
    {
      auto tail_stamp = queue.tail_stamp();
      auto cur_chunk = queue.steal_global_retired_nodes();

      if (first_retired_node != nullptr)
      {
        first_retired_node->next_chunk = cur_chunk;
        cur_chunk = first_retired_node;
        first_retired_node = nullptr;
        prev_retired_node = &first_retired_node;
        number_of_retired_nodes = 0;
      }
      if (cur_chunk == nullptr)
        return;

      stamp_t lowest_stamp;
      auto process_chunk_nodes = [tail_stamp, &lowest_stamp](deletable_object_with_stamp* chunk)
      {
        auto cur = chunk;
        while (cur)
        {
          if (cur->stamp <= tail_stamp)
          {
            lowest_stamp = std::min(lowest_stamp, cur->stamp);
            auto next = cur->next;
            cur->delete_self();
            cur = next;
          }
          else
            break; // we cannot process any more nodes from this chunk
        }
        return cur;
      };

    restart:
      lowest_stamp = std::numeric_limits<stamp_t>::max();
      deletable_object_with_stamp* first_remaining_chunk = nullptr;
      deletable_object_with_stamp* last_remaining_chunk = nullptr;
      deletable_object_with_stamp** prev_remaining_chunk = &first_remaining_chunk;
      while (cur_chunk)
      {
        auto next_chunk = cur_chunk->next_chunk;
        auto remaining_chunk = process_chunk_nodes(cur_chunk);
        if (remaining_chunk)
        {
          *prev_remaining_chunk = remaining_chunk;
          last_remaining_chunk = remaining_chunk;
          prev_remaining_chunk = &remaining_chunk->next_chunk;
        }
        cur_chunk = next_chunk;
      }

      *prev_remaining_chunk = nullptr;
      if (first_remaining_chunk)
      {
        auto new_tail_stamp = queue.tail_stamp();
        if (lowest_stamp < new_tail_stamp)
        {
          cur_chunk = first_remaining_chunk;
          tail_stamp = new_tail_stamp;
          goto restart;
        }
        else
        {
          assert(last_remaining_chunk != nullptr);
          queue.add_to_global_retired_nodes(first_remaining_chunk, last_remaining_chunk);
        }
      }
    }

    // This threshold defines the max. number of nodes a thread may collect
    // in the local retire-list before trying to reclaim them. It is checked
    // every time a new node is added to the local retire-list.
    static const std::size_t try_reclaim_threshold = 40;
    // The max. number of nodes that may remain a threads local retire-list
    // when it leaves it's critical region. If there are more nodes in the
    // list, then the whole list will be added to the global retire-list.
    static const std::size_t max_remaining_retired_nodes = 20;

    thread_control_block* control_block = nullptr;
    unsigned region_entries = 0;
    std::size_t number_of_retired_nodes = 0;

    deletable_object_with_stamp* first_retired_node = nullptr;
    deletable_object_with_stamp** prev_retired_node = &first_retired_node;

    friend class stamp_it;
    ALLOCATION_COUNTER(stamp_it);
  };

  inline stamp_it::region_guard::region_guard() noexcept
  {
      local_thread_data().enter_region();
  }

  inline stamp_it::region_guard::~region_guard() //noexcept ??
  {
      local_thread_data().leave_region();
  }

  template <class T, class MarkedPtr>
  stamp_it::guard_ptr<T, MarkedPtr>::guard_ptr(const MarkedPtr& p) noexcept :
    base(p)
  {
    if (this->ptr)
      local_thread_data().enter_region();
  }

  template <class T, class MarkedPtr>
  stamp_it::guard_ptr<T, MarkedPtr>::guard_ptr(const guard_ptr& p) noexcept :
    guard_ptr(MarkedPtr(p))
  {}

  template <class T, class MarkedPtr>
  stamp_it::guard_ptr<T, MarkedPtr>::guard_ptr(guard_ptr&& p) noexcept :
    base(p.ptr)
  {
    p.ptr.reset();
  }

  template <class T, class MarkedPtr>
  typename stamp_it::guard_ptr<T, MarkedPtr>&
    stamp_it::guard_ptr<T, MarkedPtr>::operator=(const guard_ptr& p) noexcept
  {
    if (&p == this)
      return *this;

    reset();
    this->ptr = p.ptr;
    if (this->ptr)
      local_thread_data().enter_region();

    return *this;
  }

  template <class T, class MarkedPtr>
  typename stamp_it::guard_ptr<T, MarkedPtr>&
    stamp_it::guard_ptr<T, MarkedPtr>::operator=(guard_ptr&& p) noexcept
  {
    if (&p == this)
      return *this;

    reset();
    this->ptr = std::move(p.ptr);
    p.ptr.reset();

    return *this;
  }

  template <class T, class MarkedPtr>
  void stamp_it::guard_ptr<T, MarkedPtr>::acquire(const concurrent_ptr<T>& p,
    std::memory_order order) noexcept
  {
    if (p.load(std::memory_order_relaxed) == nullptr)
    {
      reset();
      return;
    }

    if (!this->ptr)
      local_thread_data().enter_region();
    this->ptr = p.load(order);
    if (!this->ptr)
      local_thread_data().leave_region();
  }

  template <class T, class MarkedPtr>
  bool stamp_it::guard_ptr<T, MarkedPtr>::acquire_if_equal(
    const concurrent_ptr<T>& p, const MarkedPtr& expected, std::memory_order order) noexcept
  {
    auto actual = p.load(std::memory_order_relaxed);
    if (actual == nullptr || actual != expected)
    {
      reset();
      return actual == expected;
    }

    if (!this->ptr)
      local_thread_data().enter_region();
    this->ptr = p.load(order);
    if (!this->ptr || this->ptr != expected)
    {
      local_thread_data().leave_region();
      this->ptr.reset();
    }

    return this->ptr == expected;
  }

  template <class T, class MarkedPtr>
  void stamp_it::guard_ptr<T, MarkedPtr>::reset() noexcept
  {
    if (this->ptr)
      local_thread_data().leave_region();
    this->ptr.reset();
  }

  template <class T, class MarkedPtr>
  void stamp_it::guard_ptr<T, MarkedPtr>::reclaim(Deleter d) noexcept
  {
    this->ptr->set_deleter(std::move(d));
    local_thread_data().add_retired_node(this->ptr.get());
    reset();
  }

  inline stamp_it::thread_data& stamp_it::local_thread_data()
  {
    static thread_local thread_data local_thread_data;
    return local_thread_data;
  }

  SELECT_ANY stamp_it::thread_order_queue stamp_it::queue;

#ifdef WITH_PERF_COUNTER
  inline stamp_it::performance_counters stamp_it::get_performance_counters()
  {
    performance_counters result{};
    std::for_each(queue.global_thread_block_list.begin(),
                  queue.global_thread_block_list.end(),
                  [&result](const auto& block)
                  {
                    result.push_calls += block.counters.push_calls;
                    result.push_iterations += block.counters.push_iterations;
                    result.remove_calls += block.counters.remove_calls;
                    result.remove_prev_iterations += block.counters.remove_prev_iterations;
                    result.remove_next_iterations += block.counters.remove_next_iterations;
                  });

    return result;
  }
#endif

#ifdef TRACK_ALLOCATIONS
  SELECT_ANY detail::allocation_tracker stamp_it::allocation_tracker;

  inline void stamp_it::count_allocation()
  { local_thread_data().allocation_counter.count_allocation(); }

  inline void stamp_it::count_reclamation()
  { local_thread_data().allocation_counter.count_reclamation(); }
#endif
}}
