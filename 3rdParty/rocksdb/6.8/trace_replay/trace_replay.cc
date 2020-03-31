//  Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).

#include "trace_replay/trace_replay.h"

#include <chrono>
#include <sstream>
#include <thread>
#include "db/db_impl/db_impl.h"
#include "rocksdb/slice.h"
#include "rocksdb/write_batch.h"
#include "util/coding.h"
#include "util/string_util.h"
#include "util/threadpool_imp.h"

namespace ROCKSDB_NAMESPACE {

const std::string kTraceMagic = "feedcafedeadbeef";

namespace {
void EncodeCFAndKey(std::string* dst, uint32_t cf_id, const Slice& key) {
  PutFixed32(dst, cf_id);
  PutLengthPrefixedSlice(dst, key);
}

void DecodeCFAndKey(std::string& buffer, uint32_t* cf_id, Slice* key) {
  Slice buf(buffer);
  GetFixed32(&buf, cf_id);
  GetLengthPrefixedSlice(&buf, key);
}
}  // namespace

void TracerHelper::EncodeTrace(const Trace& trace, std::string* encoded_trace) {
  assert(encoded_trace);
  PutFixed64(encoded_trace, trace.ts);
  encoded_trace->push_back(trace.type);
  PutFixed32(encoded_trace, static_cast<uint32_t>(trace.payload.size()));
  encoded_trace->append(trace.payload);
}

Status TracerHelper::DecodeTrace(const std::string& encoded_trace,
                                 Trace* trace) {
  assert(trace != nullptr);
  Slice enc_slice = Slice(encoded_trace);
  if (!GetFixed64(&enc_slice, &trace->ts)) {
    return Status::Incomplete("Decode trace string failed");
  }
  if (enc_slice.size() < kTraceTypeSize + kTracePayloadLengthSize) {
    return Status::Incomplete("Decode trace string failed");
  }
  trace->type = static_cast<TraceType>(enc_slice[0]);
  enc_slice.remove_prefix(kTraceTypeSize + kTracePayloadLengthSize);
  trace->payload = enc_slice.ToString();
  return Status::OK();
}

Tracer::Tracer(Env* env, const TraceOptions& trace_options,
               std::unique_ptr<TraceWriter>&& trace_writer)
    : env_(env),
      trace_options_(trace_options),
      trace_writer_(std::move(trace_writer)),
      trace_request_count_ (0) {
  WriteHeader();
}

Tracer::~Tracer() { trace_writer_.reset(); }

Status Tracer::Write(WriteBatch* write_batch) {
  TraceType trace_type = kTraceWrite;
  if (ShouldSkipTrace(trace_type)) {
    return Status::OK();
  }
  Trace trace;
  trace.ts = env_->NowMicros();
  trace.type = trace_type;
  trace.payload = write_batch->Data();
  return WriteTrace(trace);
}

Status Tracer::Get(ColumnFamilyHandle* column_family, const Slice& key) {
  TraceType trace_type = kTraceGet;
  if (ShouldSkipTrace(trace_type)) {
    return Status::OK();
  }
  Trace trace;
  trace.ts = env_->NowMicros();
  trace.type = trace_type;
  EncodeCFAndKey(&trace.payload, column_family->GetID(), key);
  return WriteTrace(trace);
}

Status Tracer::IteratorSeek(const uint32_t& cf_id, const Slice& key) {
  TraceType trace_type = kTraceIteratorSeek;
  if (ShouldSkipTrace(trace_type)) {
    return Status::OK();
  }
  Trace trace;
  trace.ts = env_->NowMicros();
  trace.type = trace_type;
  EncodeCFAndKey(&trace.payload, cf_id, key);
  return WriteTrace(trace);
}

Status Tracer::IteratorSeekForPrev(const uint32_t& cf_id, const Slice& key) {
  TraceType trace_type = kTraceIteratorSeekForPrev;
  if (ShouldSkipTrace(trace_type)) {
    return Status::OK();
  }
  Trace trace;
  trace.ts = env_->NowMicros();
  trace.type = trace_type;
  EncodeCFAndKey(&trace.payload, cf_id, key);
  return WriteTrace(trace);
}

bool Tracer::ShouldSkipTrace(const TraceType& trace_type) {
  if (IsTraceFileOverMax()) {
    return true;
  }
  if ((trace_options_.filter & kTraceFilterGet
    && trace_type == kTraceGet)
   || (trace_options_.filter & kTraceFilterWrite
    && trace_type == kTraceWrite)) {
    return true;
  }
  ++trace_request_count_;
  if (trace_request_count_ < trace_options_.sampling_frequency) {
    return true;
  }
  trace_request_count_ = 0;
  return false;
}

bool Tracer::IsTraceFileOverMax() {
  uint64_t trace_file_size = trace_writer_->GetFileSize();
  return (trace_file_size > trace_options_.max_trace_file_size);
}

Status Tracer::WriteHeader() {
  std::ostringstream s;
  s << kTraceMagic << "\t"
    << "Trace Version: 0.1\t"
    << "RocksDB Version: " << kMajorVersion << "." << kMinorVersion << "\t"
    << "Format: Timestamp OpType Payload\n";
  std::string header(s.str());

  Trace trace;
  trace.ts = env_->NowMicros();
  trace.type = kTraceBegin;
  trace.payload = header;
  return WriteTrace(trace);
}

Status Tracer::WriteFooter() {
  Trace trace;
  trace.ts = env_->NowMicros();
  trace.type = kTraceEnd;
  trace.payload = "";
  return WriteTrace(trace);
}

Status Tracer::WriteTrace(const Trace& trace) {
  std::string encoded_trace;
  TracerHelper::EncodeTrace(trace, &encoded_trace);
  return trace_writer_->Write(Slice(encoded_trace));
}

Status Tracer::Close() { return WriteFooter(); }

Replayer::Replayer(DB* db, const std::vector<ColumnFamilyHandle*>& handles,
                   std::unique_ptr<TraceReader>&& reader)
    : trace_reader_(std::move(reader)) {
  assert(db != nullptr);
  db_ = static_cast<DBImpl*>(db->GetRootDB());
  env_ = Env::Default();
  for (ColumnFamilyHandle* cfh : handles) {
    cf_map_[cfh->GetID()] = cfh;
  }
  fast_forward_ = 1;
}

Replayer::~Replayer() { trace_reader_.reset(); }

Status Replayer::SetFastForward(uint32_t fast_forward) {
  Status s;
  if (fast_forward < 1) {
    s = Status::InvalidArgument("Wrong fast forward speed!");
  } else {
    fast_forward_ = fast_forward;
    s = Status::OK();
  }
  return s;
}

Status Replayer::Replay() {
  Status s;
  Trace header;
  s = ReadHeader(&header);
  if (!s.ok()) {
    return s;
  }

  std::chrono::system_clock::time_point replay_epoch =
      std::chrono::system_clock::now();
  WriteOptions woptions;
  ReadOptions roptions;
  Trace trace;
  uint64_t ops = 0;
  Iterator* single_iter = nullptr;
  while (s.ok()) {
    trace.reset();
    s = ReadTrace(&trace);
    if (!s.ok()) {
      break;
    }

    std::this_thread::sleep_until(
        replay_epoch +
        std::chrono::microseconds((trace.ts - header.ts) / fast_forward_));
    if (trace.type == kTraceWrite) {
      WriteBatch batch(trace.payload);
      db_->Write(woptions, &batch);
      ops++;
    } else if (trace.type == kTraceGet) {
      uint32_t cf_id = 0;
      Slice key;
      DecodeCFAndKey(trace.payload, &cf_id, &key);
      if (cf_id > 0 && cf_map_.find(cf_id) == cf_map_.end()) {
        return Status::Corruption("Invalid Column Family ID.");
      }

      std::string value;
      if (cf_id == 0) {
        db_->Get(roptions, key, &value);
      } else {
        db_->Get(roptions, cf_map_[cf_id], key, &value);
      }
      ops++;
    } else if (trace.type == kTraceIteratorSeek) {
      uint32_t cf_id = 0;
      Slice key;
      DecodeCFAndKey(trace.payload, &cf_id, &key);
      if (cf_id > 0 && cf_map_.find(cf_id) == cf_map_.end()) {
        return Status::Corruption("Invalid Column Family ID.");
      }

      if (cf_id == 0) {
        single_iter = db_->NewIterator(roptions);
      } else {
        single_iter = db_->NewIterator(roptions, cf_map_[cf_id]);
      }
      single_iter->Seek(key);
      ops++;
      delete single_iter;
    } else if (trace.type == kTraceIteratorSeekForPrev) {
      // Currently, only support to call the Seek()
      uint32_t cf_id = 0;
      Slice key;
      DecodeCFAndKey(trace.payload, &cf_id, &key);
      if (cf_id > 0 && cf_map_.find(cf_id) == cf_map_.end()) {
        return Status::Corruption("Invalid Column Family ID.");
      }

      if (cf_id == 0) {
        single_iter = db_->NewIterator(roptions);
      } else {
        single_iter = db_->NewIterator(roptions, cf_map_[cf_id]);
      }
      single_iter->SeekForPrev(key);
      ops++;
      delete single_iter;
    } else if (trace.type == kTraceEnd) {
      // Do nothing for now.
      // TODO: Add some validations later.
      break;
    }
  }

  if (s.IsIncomplete()) {
    // Reaching eof returns Incomplete status at the moment.
    // Could happen when killing a process without calling EndTrace() API.
    // TODO: Add better error handling.
    return Status::OK();
  }
  return s;
}

// The trace can be replayed with multithread by configurnge the number of
// threads in the thread pool. Trace records are read from the trace file
// sequentially and the corresponding queries are scheduled in the task
// queue based on the timestamp. Currently, we support Write_batch (Put,
// Delete, SingleDelete, DeleteRange), Get, Iterator (Seek and SeekForPrev).
Status Replayer::MultiThreadReplay(uint32_t threads_num) {
  Status s;
  Trace header;
  s = ReadHeader(&header);
  if (!s.ok()) {
    return s;
  }

  ThreadPoolImpl thread_pool;
  thread_pool.SetHostEnv(env_);

  if (threads_num > 1) {
    thread_pool.SetBackgroundThreads(static_cast<int>(threads_num));
  } else {
    thread_pool.SetBackgroundThreads(1);
  }

  std::chrono::system_clock::time_point replay_epoch =
      std::chrono::system_clock::now();
  WriteOptions woptions;
  ReadOptions roptions;
  uint64_t ops = 0;
  while (s.ok()) {
    std::unique_ptr<ReplayerWorkerArg> ra(new ReplayerWorkerArg);
    ra->db = db_;
    s = ReadTrace(&(ra->trace_entry));
    if (!s.ok()) {
      break;
    }
    ra->woptions = woptions;
    ra->roptions = roptions;

    std::this_thread::sleep_until(
        replay_epoch + std::chrono::microseconds(
                           (ra->trace_entry.ts - header.ts) / fast_forward_));
    if (ra->trace_entry.type == kTraceWrite) {
      thread_pool.Schedule(&Replayer::BGWorkWriteBatch, ra.release(), nullptr,
                           nullptr);
      ops++;
    } else if (ra->trace_entry.type == kTraceGet) {
      thread_pool.Schedule(&Replayer::BGWorkGet, ra.release(), nullptr,
                           nullptr);
      ops++;
    } else if (ra->trace_entry.type == kTraceIteratorSeek) {
      thread_pool.Schedule(&Replayer::BGWorkIterSeek, ra.release(), nullptr,
                           nullptr);
      ops++;
    } else if (ra->trace_entry.type == kTraceIteratorSeekForPrev) {
      thread_pool.Schedule(&Replayer::BGWorkIterSeekForPrev, ra.release(),
                           nullptr, nullptr);
      ops++;
    } else if (ra->trace_entry.type == kTraceEnd) {
      // Do nothing for now.
      // TODO: Add some validations later.
      break;
    } else {
      // Other trace entry types that are not implemented for replay.
      // To finish the replay, we continue the process.
      continue;
    }
  }

  if (s.IsIncomplete()) {
    // Reaching eof returns Incomplete status at the moment.
    // Could happen when killing a process without calling EndTrace() API.
    // TODO: Add better error handling.
    s = Status::OK();
  }
  thread_pool.JoinAllThreads();
  return s;
}

Status Replayer::ReadHeader(Trace* header) {
  assert(header != nullptr);
  Status s = ReadTrace(header);
  if (!s.ok()) {
    return s;
  }
  if (header->type != kTraceBegin) {
    return Status::Corruption("Corrupted trace file. Incorrect header.");
  }
  if (header->payload.substr(0, kTraceMagic.length()) != kTraceMagic) {
    return Status::Corruption("Corrupted trace file. Incorrect magic.");
  }

  return s;
}

Status Replayer::ReadFooter(Trace* footer) {
  assert(footer != nullptr);
  Status s = ReadTrace(footer);
  if (!s.ok()) {
    return s;
  }
  if (footer->type != kTraceEnd) {
    return Status::Corruption("Corrupted trace file. Incorrect footer.");
  }

  // TODO: Add more validations later
  return s;
}

Status Replayer::ReadTrace(Trace* trace) {
  assert(trace != nullptr);
  std::string encoded_trace;
  Status s = trace_reader_->Read(&encoded_trace);
  if (!s.ok()) {
    return s;
  }
  return TracerHelper::DecodeTrace(encoded_trace, trace);
}

void Replayer::BGWorkGet(void* arg) {
  std::unique_ptr<ReplayerWorkerArg> ra(
      reinterpret_cast<ReplayerWorkerArg*>(arg));
  auto cf_map = static_cast<std::unordered_map<uint32_t, ColumnFamilyHandle*>*>(
      ra->cf_map);
  uint32_t cf_id = 0;
  Slice key;
  DecodeCFAndKey(ra->trace_entry.payload, &cf_id, &key);
  if (cf_id > 0 && cf_map->find(cf_id) == cf_map->end()) {
    return;
  }

  std::string value;
  if (cf_id == 0) {
    ra->db->Get(ra->roptions, key, &value);
  } else {
    ra->db->Get(ra->roptions, (*cf_map)[cf_id], key, &value);
  }

  return;
}

void Replayer::BGWorkWriteBatch(void* arg) {
  std::unique_ptr<ReplayerWorkerArg> ra(
      reinterpret_cast<ReplayerWorkerArg*>(arg));
  WriteBatch batch(ra->trace_entry.payload);
  ra->db->Write(ra->woptions, &batch);
  return;
}

void Replayer::BGWorkIterSeek(void* arg) {
  std::unique_ptr<ReplayerWorkerArg> ra(
      reinterpret_cast<ReplayerWorkerArg*>(arg));
  auto cf_map = static_cast<std::unordered_map<uint32_t, ColumnFamilyHandle*>*>(
      ra->cf_map);
  uint32_t cf_id = 0;
  Slice key;
  DecodeCFAndKey(ra->trace_entry.payload, &cf_id, &key);
  if (cf_id > 0 && cf_map->find(cf_id) == cf_map->end()) {
    return;
  }

  std::string value;
  Iterator* single_iter = nullptr;
  if (cf_id == 0) {
    single_iter = ra->db->NewIterator(ra->roptions);
  } else {
    single_iter = ra->db->NewIterator(ra->roptions, (*cf_map)[cf_id]);
  }
  single_iter->Seek(key);
  delete single_iter;
  return;
}

void Replayer::BGWorkIterSeekForPrev(void* arg) {
  std::unique_ptr<ReplayerWorkerArg> ra(
      reinterpret_cast<ReplayerWorkerArg*>(arg));
  auto cf_map = static_cast<std::unordered_map<uint32_t, ColumnFamilyHandle*>*>(
      ra->cf_map);
  uint32_t cf_id = 0;
  Slice key;
  DecodeCFAndKey(ra->trace_entry.payload, &cf_id, &key);
  if (cf_id > 0 && cf_map->find(cf_id) == cf_map->end()) {
    return;
  }

  std::string value;
  Iterator* single_iter = nullptr;
  if (cf_id == 0) {
    single_iter = ra->db->NewIterator(ra->roptions);
  } else {
    single_iter = ra->db->NewIterator(ra->roptions, (*cf_map)[cf_id]);
  }
  single_iter->SeekForPrev(key);
  delete single_iter;
  return;
}

}  // namespace ROCKSDB_NAMESPACE
