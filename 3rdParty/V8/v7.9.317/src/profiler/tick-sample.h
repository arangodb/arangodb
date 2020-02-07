// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_PROFILER_TICK_SAMPLE_H_
#define V8_PROFILER_TICK_SAMPLE_H_

#include "include/v8.h"
#include "src/base/platform/time.h"
#include "src/common/globals.h"

namespace v8 {
namespace internal {

class Isolate;

// TickSample captures the information collected for each sample.
struct V8_EXPORT TickSample {
  // Internal profiling (with --prof + tools/$OS-tick-processor) wants to
  // include the runtime function we're calling. Externally exposed tick
  // samples don't care.
  enum RecordCEntryFrame { kIncludeCEntryFrame, kSkipCEntryFrame };

  TickSample()
      : state(OTHER),
        pc(nullptr),
        external_callback_entry(nullptr),
        frames_count(0),
        has_external_callback(false),
        update_stats(true) {}

  /**
   * Initialize a tick sample from the isolate.
   * \param isolate The isolate.
   * \param state Execution state.
   * \param record_c_entry_frame Include or skip the runtime function.
   * \param update_stats Whether update the sample to the aggregated stats.
   * \param use_simulator_reg_state When set to true and V8 is running under a
   *                                simulator, the method will use the simulator
   *                                register state rather than the one provided
   *                                with |state| argument. Otherwise the method
   *                                will use provided register |state| as is.
   */
  void Init(Isolate* isolate, const v8::RegisterState& state,
            RecordCEntryFrame record_c_entry_frame, bool update_stats,
            bool use_simulator_reg_state = true,
            base::TimeDelta sampling_interval = base::TimeDelta());
  /**
   * Get a call stack sample from the isolate.
   * \param isolate The isolate.
   * \param state Register state.
   * \param record_c_entry_frame Include or skip the runtime function.
   * \param frames Caller allocated buffer to store stack frames.
   * \param frames_limit Maximum number of frames to capture. The buffer must
   *                     be large enough to hold the number of frames.
   * \param sample_info The sample info is filled up by the function
   *                    provides number of actual captured stack frames and
   *                    the current VM state.
   * \param use_simulator_reg_state When set to true and V8 is running under a
   *                                simulator, the method will use the simulator
   *                                register state rather than the one provided
   *                                with |state| argument. Otherwise the method
   *                                will use provided register |state| as is.
   * \note GetStackSample is thread and signal safe and should only be called
   *                      when the JS thread is paused or interrupted.
   *                      Otherwise the behavior is undefined.
   */
  static bool GetStackSample(Isolate* isolate, v8::RegisterState* state,
                             RecordCEntryFrame record_c_entry_frame,
                             void** frames, size_t frames_limit,
                             v8::SampleInfo* sample_info,
                             bool use_simulator_reg_state = true,
                             void** contexts = nullptr);

  void print() const;

  StateTag state;  // The state of the VM.
  void* pc;        // Instruction pointer.
  union {
    void* tos;  // Top stack value (*sp).
    void* external_callback_entry;
  };
  static const unsigned kMaxFramesCountLog2 = 8;
  static const unsigned kMaxFramesCount = (1 << kMaxFramesCountLog2) - 1;
  void* stack[kMaxFramesCount];     // Call stack.
  void* contexts[kMaxFramesCount];  // Stack of associated native contexts.
  void* top_context = nullptr;      // Address of the incumbent native context.
  unsigned frames_count : kMaxFramesCountLog2;  // Number of captured frames.
  bool has_external_callback : 1;
  bool update_stats : 1;  // Whether the sample should update aggregated stats.

  base::TimeTicks timestamp;
  base::TimeDelta sampling_interval;  // Sampling interval used to capture.
};

}  // namespace internal
}  // namespace v8

#endif  // V8_PROFILER_TICK_SAMPLE_H_
