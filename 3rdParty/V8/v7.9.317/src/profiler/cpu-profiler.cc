// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/profiler/cpu-profiler.h"

#include <unordered_map>
#include <utility>

#include "src/base/lazy-instance.h"
#include "src/base/template-utils.h"
#include "src/debug/debug.h"
#include "src/execution/frames-inl.h"
#include "src/execution/vm-state-inl.h"
#include "src/logging/counters.h"
#include "src/logging/log.h"
#include "src/profiler/cpu-profiler-inl.h"
#include "src/utils/locked-queue-inl.h"
#include "src/wasm/wasm-engine.h"

namespace v8 {
namespace internal {

static const int kProfilerStackSize = 64 * KB;

class CpuSampler : public sampler::Sampler {
 public:
  CpuSampler(Isolate* isolate, SamplingEventsProcessor* processor)
      : sampler::Sampler(reinterpret_cast<v8::Isolate*>(isolate)),
        processor_(processor) {}

  void SampleStack(const v8::RegisterState& regs) override {
    TickSample* sample = processor_->StartTickSample();
    if (sample == nullptr) return;
    Isolate* isolate = reinterpret_cast<Isolate*>(this->isolate());
    sample->Init(isolate, regs, TickSample::kIncludeCEntryFrame,
                 /* update_stats */ true,
                 /* use_simulator_reg_state */ true, processor_->period());
    if (is_counting_samples_ && !sample->timestamp.IsNull()) {
      if (sample->state == JS) ++js_sample_count_;
      if (sample->state == EXTERNAL) ++external_sample_count_;
    }
    processor_->FinishTickSample();
  }

 private:
  SamplingEventsProcessor* processor_;
};

ProfilingScope::ProfilingScope(Isolate* isolate, ProfilerListener* listener)
    : isolate_(isolate), listener_(listener) {
  size_t profiler_count = isolate_->num_cpu_profilers();
  profiler_count++;
  isolate_->set_num_cpu_profilers(profiler_count);
  isolate_->set_is_profiling(true);
  isolate_->wasm_engine()->EnableCodeLogging(isolate_);

  Logger* logger = isolate_->logger();
  logger->AddCodeEventListener(listener_);
  // Populate the ProfilerCodeObserver with the initial functions and
  // callbacks on the heap.
  DCHECK(isolate_->heap()->HasBeenSetUp());

  if (!FLAG_prof_browser_mode) {
    logger->LogCodeObjects();
  }
  logger->LogCompiledFunctions();
  logger->LogAccessorCallbacks();
}

ProfilingScope::~ProfilingScope() {
  isolate_->logger()->RemoveCodeEventListener(listener_);

  size_t profiler_count = isolate_->num_cpu_profilers();
  DCHECK_GT(profiler_count, 0);
  profiler_count--;
  isolate_->set_num_cpu_profilers(profiler_count);
  if (profiler_count == 0) isolate_->set_is_profiling(false);
}

ProfilerEventsProcessor::ProfilerEventsProcessor(
    Isolate* isolate, ProfileGenerator* generator,
    ProfilerCodeObserver* code_observer)
    : Thread(Thread::Options("v8:ProfEvntProc", kProfilerStackSize)),
      generator_(generator),
      code_observer_(code_observer),
      running_(1),
      last_code_event_id_(0),
      last_processed_code_event_id_(0),
      isolate_(isolate) {
  DCHECK(!code_observer_->processor());
  code_observer_->set_processor(this);
}

SamplingEventsProcessor::SamplingEventsProcessor(
    Isolate* isolate, ProfileGenerator* generator,
    ProfilerCodeObserver* code_observer, base::TimeDelta period,
    bool use_precise_sampling)
    : ProfilerEventsProcessor(isolate, generator, code_observer),
      sampler_(new CpuSampler(isolate, this)),
      period_(period),
      use_precise_sampling_(use_precise_sampling) {
  sampler_->Start();
}

SamplingEventsProcessor::~SamplingEventsProcessor() { sampler_->Stop(); }

ProfilerEventsProcessor::~ProfilerEventsProcessor() {
  DCHECK_EQ(code_observer_->processor(), this);
  code_observer_->clear_processor();
}

void ProfilerEventsProcessor::Enqueue(const CodeEventsContainer& event) {
  event.generic.order = ++last_code_event_id_;
  events_buffer_.Enqueue(event);
}

void ProfilerEventsProcessor::AddDeoptStack(Address from, int fp_to_sp_delta) {
  TickSampleEventRecord record(last_code_event_id_);
  RegisterState regs;
  Address fp = isolate_->c_entry_fp(isolate_->thread_local_top());
  regs.sp = reinterpret_cast<void*>(fp - fp_to_sp_delta);
  regs.fp = reinterpret_cast<void*>(fp);
  regs.pc = reinterpret_cast<void*>(from);
  record.sample.Init(isolate_, regs, TickSample::kSkipCEntryFrame, false,
                     false);
  ticks_from_vm_buffer_.Enqueue(record);
}

void ProfilerEventsProcessor::AddCurrentStack(bool update_stats) {
  TickSampleEventRecord record(last_code_event_id_);
  RegisterState regs;
  StackFrameIterator it(isolate_);
  if (!it.done()) {
    StackFrame* frame = it.frame();
    regs.sp = reinterpret_cast<void*>(frame->sp());
    regs.fp = reinterpret_cast<void*>(frame->fp());
    regs.pc = reinterpret_cast<void*>(frame->pc());
  }
  record.sample.Init(isolate_, regs, TickSample::kSkipCEntryFrame, update_stats,
                     false);
  ticks_from_vm_buffer_.Enqueue(record);
}

void ProfilerEventsProcessor::AddSample(TickSample sample) {
  TickSampleEventRecord record(last_code_event_id_);
  record.sample = sample;
  ticks_from_vm_buffer_.Enqueue(record);
}

void ProfilerEventsProcessor::StopSynchronously() {
  if (!base::Relaxed_AtomicExchange(&running_, 0)) return;
  {
    base::MutexGuard guard(&running_mutex_);
    running_cond_.NotifyOne();
  }
  Join();
}


bool ProfilerEventsProcessor::ProcessCodeEvent() {
  CodeEventsContainer record;
  if (events_buffer_.Dequeue(&record)) {
    if (record.generic.type == CodeEventRecord::NATIVE_CONTEXT_MOVE) {
      NativeContextMoveEventRecord& nc_record =
          record.NativeContextMoveEventRecord_;
      generator_->UpdateNativeContextAddress(nc_record.from_address,
                                             nc_record.to_address);
    } else {
      code_observer_->CodeEventHandlerInternal(record);
    }
    last_processed_code_event_id_ = record.generic.order;
    return true;
  }
  return false;
}

void ProfilerEventsProcessor::CodeEventHandler(
    const CodeEventsContainer& evt_rec) {
  switch (evt_rec.generic.type) {
    case CodeEventRecord::CODE_CREATION:
    case CodeEventRecord::CODE_MOVE:
    case CodeEventRecord::CODE_DISABLE_OPT:
    case CodeEventRecord::NATIVE_CONTEXT_MOVE:
      Enqueue(evt_rec);
      break;
    case CodeEventRecord::CODE_DEOPT: {
      const CodeDeoptEventRecord* rec = &evt_rec.CodeDeoptEventRecord_;
      Address pc = rec->pc;
      int fp_to_sp_delta = rec->fp_to_sp_delta;
      Enqueue(evt_rec);
      AddDeoptStack(pc, fp_to_sp_delta);
      break;
    }
    case CodeEventRecord::NONE:
    case CodeEventRecord::REPORT_BUILTIN:
      UNREACHABLE();
  }
}

ProfilerEventsProcessor::SampleProcessingResult
SamplingEventsProcessor::ProcessOneSample() {
  TickSampleEventRecord record1;
  if (ticks_from_vm_buffer_.Peek(&record1) &&
      (record1.order == last_processed_code_event_id_)) {
    TickSampleEventRecord record;
    ticks_from_vm_buffer_.Dequeue(&record);
    generator_->RecordTickSample(record.sample);
    return OneSampleProcessed;
  }

  const TickSampleEventRecord* record = ticks_buffer_.Peek();
  if (record == nullptr) {
    if (ticks_from_vm_buffer_.IsEmpty()) return NoSamplesInQueue;
    return FoundSampleForNextCodeEvent;
  }
  if (record->order != last_processed_code_event_id_) {
    return FoundSampleForNextCodeEvent;
  }
  generator_->RecordTickSample(record->sample);
  ticks_buffer_.Remove();
  return OneSampleProcessed;
}

void SamplingEventsProcessor::Run() {
  base::MutexGuard guard(&running_mutex_);
  while (!!base::Relaxed_Load(&running_)) {
    base::TimeTicks nextSampleTime =
        base::TimeTicks::HighResolutionNow() + period_;
    base::TimeTicks now;
    SampleProcessingResult result;
    // Keep processing existing events until we need to do next sample
    // or the ticks buffer is empty.
    do {
      result = ProcessOneSample();
      if (result == FoundSampleForNextCodeEvent) {
        // All ticks of the current last_processed_code_event_id_ are
        // processed, proceed to the next code event.
        ProcessCodeEvent();
      }
      now = base::TimeTicks::HighResolutionNow();
    } while (result != NoSamplesInQueue && now < nextSampleTime);

    if (nextSampleTime > now) {
#if V8_OS_WIN
      if (use_precise_sampling_ &&
          nextSampleTime - now < base::TimeDelta::FromMilliseconds(100)) {
        // Do not use Sleep on Windows as it is very imprecise, with up to 16ms
        // jitter, which is unacceptable for short profile intervals.
        while (base::TimeTicks::HighResolutionNow() < nextSampleTime) {
        }
      } else  // NOLINT
#else
      USE(use_precise_sampling_);
#endif  // V8_OS_WIN
      {
        // Allow another thread to interrupt the delay between samples in the
        // event of profiler shutdown.
        while (now < nextSampleTime &&
               running_cond_.WaitFor(&running_mutex_, nextSampleTime - now)) {
          // If true was returned, we got interrupted before the timeout
          // elapsed. If this was not due to a change in running state, a
          // spurious wakeup occurred (thus we should continue to wait).
          if (!base::Relaxed_Load(&running_)) {
            break;
          }
          now = base::TimeTicks::HighResolutionNow();
        }
      }
    }

    // Schedule next sample.
    sampler_->DoSample();
  }

  // Process remaining tick events.
  do {
    SampleProcessingResult result;
    do {
      result = ProcessOneSample();
    } while (result == OneSampleProcessed);
  } while (ProcessCodeEvent());
}

void SamplingEventsProcessor::SetSamplingInterval(base::TimeDelta period) {
  if (period_ == period) return;
  StopSynchronously();

  period_ = period;
  base::Relaxed_Store(&running_, 1);

  StartSynchronously();
}

void* SamplingEventsProcessor::operator new(size_t size) {
  return AlignedAlloc(size, alignof(SamplingEventsProcessor));
}

void SamplingEventsProcessor::operator delete(void* ptr) { AlignedFree(ptr); }

ProfilerCodeObserver::ProfilerCodeObserver(Isolate* isolate)
    : isolate_(isolate), processor_(nullptr) {
  CreateEntriesForRuntimeCallStats();
  LogBuiltins();
}

void ProfilerCodeObserver::CodeEventHandler(
    const CodeEventsContainer& evt_rec) {
  if (processor_) {
    processor_->CodeEventHandler(evt_rec);
    return;
  }
  CodeEventHandlerInternal(evt_rec);
}

void ProfilerCodeObserver::CodeEventHandlerInternal(
    const CodeEventsContainer& evt_rec) {
  CodeEventsContainer record = evt_rec;
  switch (evt_rec.generic.type) {
#define PROFILER_TYPE_CASE(type, clss)        \
  case CodeEventRecord::type:                 \
    record.clss##_.UpdateCodeMap(&code_map_); \
    break;

    CODE_EVENTS_TYPE_LIST(PROFILER_TYPE_CASE)

#undef PROFILER_TYPE_CASE
    default:
      break;
  }
}

void ProfilerCodeObserver::CreateEntriesForRuntimeCallStats() {
  RuntimeCallStats* rcs = isolate_->counters()->runtime_call_stats();
  for (int i = 0; i < RuntimeCallStats::kNumberOfCounters; ++i) {
    RuntimeCallCounter* counter = rcs->GetCounter(i);
    DCHECK(counter->name());
    auto entry = new CodeEntry(CodeEventListener::FUNCTION_TAG, counter->name(),
                               "native V8Runtime");
    code_map_.AddCode(reinterpret_cast<Address>(counter), entry, 1);
  }
}

void ProfilerCodeObserver::LogBuiltins() {
  Builtins* builtins = isolate_->builtins();
  DCHECK(builtins->is_initialized());
  for (int i = 0; i < Builtins::builtin_count; i++) {
    CodeEventsContainer evt_rec(CodeEventRecord::REPORT_BUILTIN);
    ReportBuiltinEventRecord* rec = &evt_rec.ReportBuiltinEventRecord_;
    Builtins::Name id = static_cast<Builtins::Name>(i);
    rec->instruction_start = builtins->builtin(id).InstructionStart();
    rec->builtin_id = id;
    CodeEventHandlerInternal(evt_rec);
  }
}

int CpuProfiler::GetProfilesCount() {
  // The count of profiles doesn't depend on a security token.
  return static_cast<int>(profiles_->profiles()->size());
}


CpuProfile* CpuProfiler::GetProfile(int index) {
  return profiles_->profiles()->at(index).get();
}


void CpuProfiler::DeleteAllProfiles() {
  if (is_profiling_) StopProcessor();
  ResetProfiles();
}


void CpuProfiler::DeleteProfile(CpuProfile* profile) {
  profiles_->RemoveProfile(profile);
  if (profiles_->profiles()->empty() && !is_profiling_) {
    // If this was the last profile, clean up all accessory data as well.
    ResetProfiles();
  }
}

namespace {

class CpuProfilersManager {
 public:
  void AddProfiler(Isolate* isolate, CpuProfiler* profiler) {
    base::MutexGuard lock(&mutex_);
    profilers_.emplace(isolate, profiler);
  }

  void RemoveProfiler(Isolate* isolate, CpuProfiler* profiler) {
    base::MutexGuard lock(&mutex_);
    auto range = profilers_.equal_range(isolate);
    for (auto it = range.first; it != range.second; ++it) {
      if (it->second != profiler) continue;
      profilers_.erase(it);
      return;
    }
    UNREACHABLE();
  }

  void CallCollectSample(Isolate* isolate) {
    base::MutexGuard lock(&mutex_);
    auto range = profilers_.equal_range(isolate);
    for (auto it = range.first; it != range.second; ++it) {
      it->second->CollectSample();
    }
  }

 private:
  std::unordered_multimap<Isolate*, CpuProfiler*> profilers_;
  base::Mutex mutex_;
};

DEFINE_LAZY_LEAKY_OBJECT_GETTER(CpuProfilersManager, GetProfilersManager)

}  // namespace

CpuProfiler::CpuProfiler(Isolate* isolate, CpuProfilingNamingMode naming_mode,
                         CpuProfilingLoggingMode logging_mode)
    : CpuProfiler(isolate, naming_mode, logging_mode,
                  new CpuProfilesCollection(isolate), nullptr, nullptr) {}

CpuProfiler::CpuProfiler(Isolate* isolate, CpuProfilingNamingMode naming_mode,
                         CpuProfilingLoggingMode logging_mode,
                         CpuProfilesCollection* test_profiles,
                         ProfileGenerator* test_generator,
                         ProfilerEventsProcessor* test_processor)
    : isolate_(isolate),
      naming_mode_(naming_mode),
      logging_mode_(logging_mode),
      base_sampling_interval_(base::TimeDelta::FromMicroseconds(
          FLAG_cpu_profiler_sampling_interval)),
      profiles_(test_profiles),
      generator_(test_generator),
      processor_(test_processor),
      code_observer_(isolate),
      is_profiling_(false) {
  profiles_->set_cpu_profiler(this);
  GetProfilersManager()->AddProfiler(isolate, this);

  if (logging_mode == kEagerLogging) EnableLogging();
}

CpuProfiler::~CpuProfiler() {
  DCHECK(!is_profiling_);
  GetProfilersManager()->RemoveProfiler(isolate_, this);

  DisableLogging();
}

void CpuProfiler::set_sampling_interval(base::TimeDelta value) {
  DCHECK(!is_profiling_);
  base_sampling_interval_ = value;
}

void CpuProfiler::set_use_precise_sampling(bool value) {
  DCHECK(!is_profiling_);
  use_precise_sampling_ = value;
}

void CpuProfiler::ResetProfiles() {
  profiles_.reset(new CpuProfilesCollection(isolate_));
  profiles_->set_cpu_profiler(this);
  generator_.reset();
  if (!profiling_scope_) profiler_listener_.reset();
}

void CpuProfiler::EnableLogging() {
  if (profiling_scope_) return;

  if (!profiler_listener_) {
    profiler_listener_.reset(
        new ProfilerListener(isolate_, &code_observer_, naming_mode_));
  }
  profiling_scope_.reset(
      new ProfilingScope(isolate_, profiler_listener_.get()));
}

void CpuProfiler::DisableLogging() {
  if (!profiling_scope_) return;

  DCHECK(profiler_listener_);
  profiling_scope_.reset();
}

base::TimeDelta CpuProfiler::ComputeSamplingInterval() const {
  return profiles_->GetCommonSamplingInterval();
}

void CpuProfiler::AdjustSamplingInterval() {
  if (!processor_) return;

  base::TimeDelta base_interval = ComputeSamplingInterval();
  processor_->SetSamplingInterval(base_interval);
}

// static
void CpuProfiler::CollectSample(Isolate* isolate) {
  GetProfilersManager()->CallCollectSample(isolate);
}

void CpuProfiler::CollectSample() {
  if (processor_) {
    processor_->AddCurrentStack();
  }
}

void CpuProfiler::StartProfiling(const char* title,
                                 CpuProfilingOptions options) {
  if (profiles_->StartProfiling(title, options)) {
    TRACE_EVENT0("v8", "CpuProfiler::StartProfiling");
    AdjustSamplingInterval();
    StartProcessorIfNotStarted();
  }
}

void CpuProfiler::StartProfiling(String title, CpuProfilingOptions options) {
  StartProfiling(profiles_->GetName(title), options);
}

void CpuProfiler::StartProcessorIfNotStarted() {
  if (processor_) {
    processor_->AddCurrentStack();
    return;
  }

  if (!profiling_scope_) {
    DCHECK_EQ(logging_mode_, kLazyLogging);
    EnableLogging();
  }

  if (!generator_) {
    generator_.reset(
        new ProfileGenerator(profiles_.get(), code_observer_.code_map()));
  }

  base::TimeDelta sampling_interval = ComputeSamplingInterval();
  processor_.reset(
      new SamplingEventsProcessor(isolate_, generator_.get(), &code_observer_,
                                  sampling_interval, use_precise_sampling_));
  is_profiling_ = true;

  // Enable stack sampling.
  processor_->AddCurrentStack();
  processor_->StartSynchronously();
}

CpuProfile* CpuProfiler::StopProfiling(const char* title) {
  if (!is_profiling_) return nullptr;
  StopProcessorIfLastProfile(title);
  CpuProfile* result = profiles_->StopProfiling(title);
  AdjustSamplingInterval();
  return result;
}

CpuProfile* CpuProfiler::StopProfiling(String title) {
  return StopProfiling(profiles_->GetName(title));
}

void CpuProfiler::StopProcessorIfLastProfile(const char* title) {
  if (!profiles_->IsLastProfile(title)) return;
  StopProcessor();
}

void CpuProfiler::StopProcessor() {
  is_profiling_ = false;
  processor_->StopSynchronously();
  processor_.reset();

  DCHECK(profiling_scope_);
  if (logging_mode_ == kLazyLogging) {
    DisableLogging();
  }
}
}  // namespace internal
}  // namespace v8
