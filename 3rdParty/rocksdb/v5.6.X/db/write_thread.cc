//  Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under the BSD-style license found in the
//  LICENSE file in the root directory of this source tree. An additional grant
//  of patent rights can be found in the PATENTS file in the same directory.
//  This source code is also licensed under the GPLv2 license found in the
//  COPYING file in the root directory of this source tree.

#include "db/write_thread.h"
#include <chrono>
#include <thread>
#include "db/column_family.h"
#include "port/port.h"
#include "util/random.h"
#include "util/sync_point.h"

namespace rocksdb {

WriteThread::WriteThread(const ImmutableDBOptions& db_options)
    : max_yield_usec_(db_options.enable_write_thread_adaptive_yield
                          ? db_options.write_thread_max_yield_usec
                          : 0),
      slow_yield_usec_(db_options.write_thread_slow_yield_usec),
      allow_concurrent_memtable_write_(
          db_options.allow_concurrent_memtable_write),
      enable_pipelined_write_(db_options.enable_pipelined_write),
      newest_writer_(nullptr),
      newest_memtable_writer_(nullptr),
      last_sequence_(0) {}

uint8_t WriteThread::BlockingAwaitState(Writer* w, uint8_t goal_mask) {
  // We're going to block.  Lazily create the mutex.  We guarantee
  // propagation of this construction to the waker via the
  // STATE_LOCKED_WAITING state.  The waker won't try to touch the mutex
  // or the condvar unless they CAS away the STATE_LOCKED_WAITING that
  // we install below.
  w->CreateMutex();

  auto state = w->state.load(std::memory_order_acquire);
  assert(state != STATE_LOCKED_WAITING);
  if ((state & goal_mask) == 0 &&
      w->state.compare_exchange_strong(state, STATE_LOCKED_WAITING)) {
    // we have permission (and an obligation) to use StateMutex
    std::unique_lock<std::mutex> guard(w->StateMutex());
    w->StateCV().wait(guard, [w] {
      return w->state.load(std::memory_order_relaxed) != STATE_LOCKED_WAITING;
    });
    state = w->state.load(std::memory_order_relaxed);
  }
  // else tricky.  Goal is met or CAS failed.  In the latter case the waker
  // must have changed the state, and compare_exchange_strong has updated
  // our local variable with the new one.  At the moment WriteThread never
  // waits for a transition across intermediate states, so we know that
  // since a state change has occurred the goal must have been met.
  assert((state & goal_mask) != 0);
  return state;
}

uint8_t WriteThread::AwaitState(Writer* w, uint8_t goal_mask,
                                AdaptationContext* ctx) {
  uint8_t state;

  // On a modern Xeon each loop takes about 7 nanoseconds (most of which
  // is the effect of the pause instruction), so 200 iterations is a bit
  // more than a microsecond.  This is long enough that waits longer than
  // this can amortize the cost of accessing the clock and yielding.
  for (uint32_t tries = 0; tries < 200; ++tries) {
    state = w->state.load(std::memory_order_acquire);
    if ((state & goal_mask) != 0) {
      return state;
    }
    port::AsmVolatilePause();
  }

  // If we're only going to end up waiting a short period of time,
  // it can be a lot more efficient to call std::this_thread::yield()
  // in a loop than to block in StateMutex().  For reference, on my 4.0
  // SELinux test server with support for syscall auditing enabled, the
  // minimum latency between FUTEX_WAKE to returning from FUTEX_WAIT is
  // 2.7 usec, and the average is more like 10 usec.  That can be a big
  // drag on RockDB's single-writer design.  Of course, spinning is a
  // bad idea if other threads are waiting to run or if we're going to
  // wait for a long time.  How do we decide?
  //
  // We break waiting into 3 categories: short-uncontended,
  // short-contended, and long.  If we had an oracle, then we would always
  // spin for short-uncontended, always block for long, and our choice for
  // short-contended might depend on whether we were trying to optimize
  // RocksDB throughput or avoid being greedy with system resources.
  //
  // Bucketing into short or long is easy by measuring elapsed time.
  // Differentiating short-uncontended from short-contended is a bit
  // trickier, but not too bad.  We could look for involuntary context
  // switches using getrusage(RUSAGE_THREAD, ..), but it's less work
  // (portability code and CPU) to just look for yield calls that take
  // longer than we expect.  sched_yield() doesn't actually result in any
  // context switch overhead if there are no other runnable processes
  // on the current core, in which case it usually takes less than
  // a microsecond.
  //
  // There are two primary tunables here: the threshold between "short"
  // and "long" waits, and the threshold at which we suspect that a yield
  // is slow enough to indicate we should probably block.  If these
  // thresholds are chosen well then CPU-bound workloads that don't
  // have more threads than cores will experience few context switches
  // (voluntary or involuntary), and the total number of context switches
  // (voluntary and involuntary) will not be dramatically larger (maybe
  // 2x) than the number of voluntary context switches that occur when
  // --max_yield_wait_micros=0.
  //
  // There's another constant, which is the number of slow yields we will
  // tolerate before reversing our previous decision.  Solitary slow
  // yields are pretty common (low-priority small jobs ready to run),
  // so this should be at least 2.  We set this conservatively to 3 so
  // that we can also immediately schedule a ctx adaptation, rather than
  // waiting for the next update_ctx.

  const size_t kMaxSlowYieldsWhileSpinning = 3;

  bool update_ctx = false;
  bool would_spin_again = false;

  if (max_yield_usec_ > 0) {
    update_ctx = Random::GetTLSInstance()->OneIn(256);

    if (update_ctx || ctx->value.load(std::memory_order_relaxed) >= 0) {
      // we're updating the adaptation statistics, or spinning has >
      // 50% chance of being shorter than max_yield_usec_ and causing no
      // involuntary context switches
      auto spin_begin = std::chrono::steady_clock::now();

      // this variable doesn't include the final yield (if any) that
      // causes the goal to be met
      size_t slow_yield_count = 0;

      auto iter_begin = spin_begin;
      while ((iter_begin - spin_begin) <=
             std::chrono::microseconds(max_yield_usec_)) {
        std::this_thread::yield();

        state = w->state.load(std::memory_order_acquire);
        if ((state & goal_mask) != 0) {
          // success
          would_spin_again = true;
          break;
        }

        auto now = std::chrono::steady_clock::now();
        if (now == iter_begin ||
            now - iter_begin >= std::chrono::microseconds(slow_yield_usec_)) {
          // conservatively count it as a slow yield if our clock isn't
          // accurate enough to measure the yield duration
          ++slow_yield_count;
          if (slow_yield_count >= kMaxSlowYieldsWhileSpinning) {
            // Not just one ivcsw, but several.  Immediately update ctx
            // and fall back to blocking
            update_ctx = true;
            break;
          }
        }
        iter_begin = now;
      }
    }
  }

  if ((state & goal_mask) == 0) {
    state = BlockingAwaitState(w, goal_mask);
  }

  if (update_ctx) {
    auto v = ctx->value.load(std::memory_order_relaxed);
    // fixed point exponential decay with decay constant 1/1024, with +1
    // and -1 scaled to avoid overflow for int32_t
    v = v + (v / 1024) + (would_spin_again ? 1 : -1) * 16384;
    ctx->value.store(v, std::memory_order_relaxed);
  }

  assert((state & goal_mask) != 0);
  return state;
}

void WriteThread::SetState(Writer* w, uint8_t new_state) {
  auto state = w->state.load(std::memory_order_acquire);
  if (state == STATE_LOCKED_WAITING ||
      !w->state.compare_exchange_strong(state, new_state)) {
    assert(state == STATE_LOCKED_WAITING);

    std::lock_guard<std::mutex> guard(w->StateMutex());
    assert(w->state.load(std::memory_order_relaxed) != new_state);
    w->state.store(new_state, std::memory_order_relaxed);
    w->StateCV().notify_one();
  }
}

bool WriteThread::LinkOne(Writer* w, std::atomic<Writer*>* newest_writer) {
  assert(newest_writer != nullptr);
  assert(w->state == STATE_INIT);
  Writer* writers = newest_writer->load(std::memory_order_relaxed);
  while (true) {
    w->link_older = writers;
    if (newest_writer->compare_exchange_weak(writers, w)) {
      return (writers == nullptr);
    }
  }
}

bool WriteThread::LinkGroup(WriteGroup& write_group,
                            std::atomic<Writer*>* newest_writer) {
  assert(newest_writer != nullptr);
  Writer* leader = write_group.leader;
  Writer* last_writer = write_group.last_writer;
  Writer* w = last_writer;
  while (true) {
    // Unset link_newer pointers to make sure when we call
    // CreateMissingNewerLinks later it create all missing links.
    w->link_newer = nullptr;
    w->write_group = nullptr;
    if (w == leader) {
      break;
    }
    w = w->link_older;
  }
  Writer* newest = newest_writer->load(std::memory_order_relaxed);
  while (true) {
    leader->link_older = newest;
    if (newest_writer->compare_exchange_weak(newest, last_writer)) {
      return (newest == nullptr);
    }
  }
}

void WriteThread::CreateMissingNewerLinks(Writer* head) {
  while (true) {
    Writer* next = head->link_older;
    if (next == nullptr || next->link_newer != nullptr) {
      assert(next == nullptr || next->link_newer == head);
      break;
    }
    next->link_newer = head;
    head = next;
  }
}

void WriteThread::CompleteLeader(WriteGroup& write_group) {
  assert(write_group.size > 0);
  Writer* leader = write_group.leader;
  if (write_group.size == 1) {
    write_group.leader = nullptr;
    write_group.last_writer = nullptr;
  } else {
    assert(leader->link_newer != nullptr);
    leader->link_newer->link_older = nullptr;
    write_group.leader = leader->link_newer;
  }
  write_group.size -= 1;
  SetState(leader, STATE_COMPLETED);
}

void WriteThread::CompleteFollower(Writer* w, WriteGroup& write_group) {
  assert(write_group.size > 1);
  assert(w != write_group.leader);
  if (w == write_group.last_writer) {
    w->link_older->link_newer = nullptr;
    write_group.last_writer = w->link_older;
  } else {
    w->link_older->link_newer = w->link_newer;
    w->link_newer->link_older = w->link_older;
  }
  write_group.size -= 1;
  SetState(w, STATE_COMPLETED);
}

void WriteThread::JoinBatchGroup(Writer* w) {
  static AdaptationContext ctx("JoinBatchGroup");

  assert(w->batch != nullptr);
  bool linked_as_leader = LinkOne(w, &newest_writer_);
  if (linked_as_leader) {
    SetState(w, STATE_GROUP_LEADER);
  }

  TEST_SYNC_POINT_CALLBACK("WriteThread::JoinBatchGroup:Wait", w);

  if (!linked_as_leader) {
    /**
     * Wait util:
     * 1) An existing leader pick us as the new leader when it finishes
     * 2) An existing leader pick us as its follewer and
     * 2.1) finishes the memtable writes on our behalf
     * 2.2) Or tell us to finish the memtable writes in pralallel
     * 3) (pipelined write) An existing leader pick us as its follower and
     *    finish book-keeping and WAL write for us, enqueue us as pending
     *    memtable writer, and
     * 3.1) we become memtable writer group leader, or
     * 3.2) an existing memtable writer group leader tell us to finish memtable
     *      writes in parallel.
     */
    AwaitState(w, STATE_GROUP_LEADER | STATE_MEMTABLE_WRITER_LEADER |
                      STATE_PARALLEL_MEMTABLE_WRITER | STATE_COMPLETED,
               &ctx);
    TEST_SYNC_POINT_CALLBACK("WriteThread::JoinBatchGroup:DoneWaiting", w);
  }
}

size_t WriteThread::EnterAsBatchGroupLeader(Writer* leader,
                                            WriteGroup* write_group) {
  assert(leader->link_older == nullptr);
  assert(leader->batch != nullptr);
  assert(write_group != nullptr);

  size_t size = WriteBatchInternal::ByteSize(leader->batch);

  // Allow the group to grow up to a maximum size, but if the
  // original write is small, limit the growth so we do not slow
  // down the small write too much.
  size_t max_size = 1 << 20;
  if (size <= (128 << 10)) {
    max_size = size + (128 << 10);
  }

  leader->write_group = write_group;
  write_group->leader = leader;
  write_group->last_writer = leader;
  write_group->size = 1;
  Writer* newest_writer = newest_writer_.load(std::memory_order_acquire);

  // This is safe regardless of any db mutex status of the caller. Previous
  // calls to ExitAsGroupLeader either didn't call CreateMissingNewerLinks
  // (they emptied the list and then we added ourself as leader) or had to
  // explicitly wake us up (the list was non-empty when we added ourself,
  // so we have already received our MarkJoined).
  CreateMissingNewerLinks(newest_writer);

  // Tricky. Iteration start (leader) is exclusive and finish
  // (newest_writer) is inclusive. Iteration goes from old to new.
  Writer* w = leader;
  while (w != newest_writer) {
    w = w->link_newer;

    if (w->sync && !leader->sync) {
      // Do not include a sync write into a batch handled by a non-sync write.
      break;
    }

    if (w->no_slowdown != leader->no_slowdown) {
      // Do not mix writes that are ok with delays with the ones that
      // request fail on delays.
      break;
    }

    if (!w->disable_wal && leader->disable_wal) {
      // Do not include a write that needs WAL into a batch that has
      // WAL disabled.
      break;
    }

    if (w->batch == nullptr) {
      // Do not include those writes with nullptr batch. Those are not writes,
      // those are something else. They want to be alone
      break;
    }

    if (w->callback != nullptr && !w->callback->AllowWriteBatching()) {
      // dont batch writes that don't want to be batched
      break;
    }

    auto batch_size = WriteBatchInternal::ByteSize(w->batch);
    if (size + batch_size > max_size) {
      // Do not make batch too big
      break;
    }

    w->write_group = write_group;
    size += batch_size;
    write_group->last_writer = w;
    write_group->size++;
  }
  return size;
}

void WriteThread::EnterAsMemTableWriter(Writer* leader,
                                        WriteGroup* write_group) {
  assert(leader != nullptr);
  assert(leader->link_older == nullptr);
  assert(leader->batch != nullptr);
  assert(write_group != nullptr);

  size_t size = WriteBatchInternal::ByteSize(leader->batch);

  // Allow the group to grow up to a maximum size, but if the
  // original write is small, limit the growth so we do not slow
  // down the small write too much.
  size_t max_size = 1 << 20;
  if (size <= (128 << 10)) {
    max_size = size + (128 << 10);
  }

  leader->write_group = write_group;
  write_group->leader = leader;
  write_group->size = 1;
  Writer* last_writer = leader;

  if (!allow_concurrent_memtable_write_ || !leader->batch->HasMerge()) {
    Writer* newest_writer = newest_memtable_writer_.load();
    CreateMissingNewerLinks(newest_writer);

    Writer* w = leader;
    while (w != newest_writer) {
      w = w->link_newer;

      if (w->batch == nullptr) {
        break;
      }

      if (w->batch->HasMerge()) {
        break;
      }

      if (!allow_concurrent_memtable_write_) {
        auto batch_size = WriteBatchInternal::ByteSize(w->batch);
        if (size + batch_size > max_size) {
          // Do not make batch too big
          break;
        }
        size += batch_size;
      }

      w->write_group = write_group;
      last_writer = w;
      write_group->size++;
    }
  }

  write_group->last_writer = last_writer;
  write_group->last_sequence =
      last_writer->sequence + WriteBatchInternal::Count(last_writer->batch) - 1;
}

void WriteThread::ExitAsMemTableWriter(Writer* self, WriteGroup& write_group) {
  Writer* leader = write_group.leader;
  Writer* last_writer = write_group.last_writer;

  Writer* newest_writer = last_writer;
  if (!newest_memtable_writer_.compare_exchange_strong(newest_writer,
                                                       nullptr)) {
    CreateMissingNewerLinks(newest_writer);
    Writer* next_leader = last_writer->link_newer;
    assert(next_leader != nullptr);
    next_leader->link_older = nullptr;
    SetState(next_leader, STATE_MEMTABLE_WRITER_LEADER);
  }
  Writer* w = leader;
  while (true) {
    if (!write_group.status.ok()) {
      w->status = write_group.status;
    }
    Writer* next = w->link_newer;
    if (w != leader) {
      SetState(w, STATE_COMPLETED);
    }
    if (w == last_writer) {
      break;
    }
    w = next;
  }
  // Note that leader has to exit last, since it owns the write group.
  SetState(leader, STATE_COMPLETED);
}

void WriteThread::LaunchParallelMemTableWriters(WriteGroup* write_group) {
  assert(write_group != nullptr);
  write_group->running.store(write_group->size);
  for (auto w : *write_group) {
    SetState(w, STATE_PARALLEL_MEMTABLE_WRITER);
  }
}

// This method is called by both the leader and parallel followers
bool WriteThread::CompleteParallelMemTableWriter(Writer* w) {
  static AdaptationContext ctx("CompleteParallelMemTableWriter");

  auto* write_group = w->write_group;
  if (!w->status.ok()) {
    std::lock_guard<std::mutex> guard(write_group->leader->StateMutex());
    write_group->status = w->status;
  }

  if (write_group->running-- > 1) {
    // we're not the last one
    AwaitState(w, STATE_COMPLETED, &ctx);
    return false;
  }
  // else we're the last parallel worker and should perform exit duties.
  w->status = write_group->status;
  return true;
}

void WriteThread::ExitAsBatchGroupFollower(Writer* w) {
  auto* write_group = w->write_group;

  assert(w->state == STATE_PARALLEL_MEMTABLE_WRITER);
  assert(write_group->status.ok());
  ExitAsBatchGroupLeader(*write_group, write_group->status);
  assert(w->status.ok());
  assert(w->state == STATE_COMPLETED);
  SetState(write_group->leader, STATE_COMPLETED);
}

void WriteThread::ExitAsBatchGroupLeader(WriteGroup& write_group,
                                         Status status) {
  static AdaptationContext ctx("ExitAsBatchGroupLeader");
  Writer* leader = write_group.leader;
  Writer* last_writer = write_group.last_writer;
  assert(leader->link_older == nullptr);

  if (enable_pipelined_write_) {
    // Notify writers don't write to memtable to exit.
    for (Writer* w = last_writer; w != leader;) {
      Writer* next = w->link_older;
      w->status = status;
      if (!w->ShouldWriteToMemtable()) {
        CompleteFollower(w, write_group);
      }
      w = next;
    }
    if (!leader->ShouldWriteToMemtable()) {
      CompleteLeader(write_group);
    }
    // Link the ramaining of the group to memtable writer list.
    if (write_group.size > 0) {
      if (LinkGroup(write_group, &newest_memtable_writer_)) {
        // The leader can now be different from current writer.
        SetState(write_group.leader, STATE_MEMTABLE_WRITER_LEADER);
      }
    }
    // Reset newest_writer_ and wake up the next leader.
    Writer* newest_writer = last_writer;
    if (!newest_writer_.compare_exchange_strong(newest_writer, nullptr)) {
      Writer* next_leader = newest_writer;
      while (next_leader->link_older != last_writer) {
        next_leader = next_leader->link_older;
        assert(next_leader != nullptr);
      }
      next_leader->link_older = nullptr;
      SetState(next_leader, STATE_GROUP_LEADER);
    }
    AwaitState(leader, STATE_MEMTABLE_WRITER_LEADER |
                           STATE_PARALLEL_MEMTABLE_WRITER | STATE_COMPLETED,
               &ctx);
  } else {
    Writer* head = newest_writer_.load(std::memory_order_acquire);
    if (head != last_writer ||
        !newest_writer_.compare_exchange_strong(head, nullptr)) {
      // Either w wasn't the head during the load(), or it was the head
      // during the load() but somebody else pushed onto the list before
      // we did the compare_exchange_strong (causing it to fail).  In the
      // latter case compare_exchange_strong has the effect of re-reading
      // its first param (head).  No need to retry a failing CAS, because
      // only a departing leader (which we are at the moment) can remove
      // nodes from the list.
      assert(head != last_writer);

      // After walking link_older starting from head (if not already done)
      // we will be able to traverse w->link_newer below. This function
      // can only be called from an active leader, only a leader can
      // clear newest_writer_, we didn't, and only a clear newest_writer_
      // could cause the next leader to start their work without a call
      // to MarkJoined, so we can definitely conclude that no other leader
      // work is going on here (with or without db mutex).
      CreateMissingNewerLinks(head);
      assert(last_writer->link_newer->link_older == last_writer);
      last_writer->link_newer->link_older = nullptr;

      // Next leader didn't self-identify, because newest_writer_ wasn't
      // nullptr when they enqueued (we were definitely enqueued before them
      // and are still in the list).  That means leader handoff occurs when
      // we call MarkJoined
      SetState(last_writer->link_newer, STATE_GROUP_LEADER);
    }
    // else nobody else was waiting, although there might already be a new
    // leader now

    while (last_writer != leader) {
      last_writer->status = status;
      // we need to read link_older before calling SetState, because as soon
      // as it is marked committed the other thread's Await may return and
      // deallocate the Writer.
      auto next = last_writer->link_older;
      SetState(last_writer, STATE_COMPLETED);

      last_writer = next;
    }
  }
}

void WriteThread::EnterUnbatched(Writer* w, InstrumentedMutex* mu) {
  static AdaptationContext ctx("EnterUnbatched");
  assert(w != nullptr && w->batch == nullptr);
  mu->Unlock();
  bool linked_as_leader = LinkOne(w, &newest_writer_);
  if (!linked_as_leader) {
    TEST_SYNC_POINT("WriteThread::EnterUnbatched:Wait");
    // Last leader will not pick us as a follower since our batch is nullptr
    AwaitState(w, STATE_GROUP_LEADER, &ctx);
  }
  if (enable_pipelined_write_) {
    WaitForMemTableWriters();
  }
  mu->Lock();
}

void WriteThread::ExitUnbatched(Writer* w) {
  assert(w != nullptr);
  Writer* newest_writer = w;
  if (!newest_writer_.compare_exchange_strong(newest_writer, nullptr)) {
    CreateMissingNewerLinks(newest_writer);
    Writer* next_leader = w->link_newer;
    assert(next_leader != nullptr);
    next_leader->link_older = nullptr;
    SetState(next_leader, STATE_GROUP_LEADER);
  }
}

void WriteThread::WaitForMemTableWriters() {
  static AdaptationContext ctx("WaitForMemTableWriters");
  assert(enable_pipelined_write_);
  if (newest_memtable_writer_.load() == nullptr) {
    return;
  }
  Writer w;
  if (!LinkOne(&w, &newest_memtable_writer_)) {
    AwaitState(&w, STATE_MEMTABLE_WRITER_LEADER, &ctx);
  }
  newest_memtable_writer_.store(nullptr);
}

}  // namespace rocksdb
