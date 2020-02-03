// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_D8_D8_H_
#define V8_D8_D8_H_

#include <iterator>
#include <map>
#include <memory>
#include <queue>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "src/base/once.h"
#include "src/base/platform/time.h"
#include "src/d8/async-hooks-wrapper.h"
#include "src/strings/string-hasher.h"
#include "src/utils/allocation.h"
#include "src/utils/utils.h"

namespace v8 {

// A single counter in a counter collection.
class Counter {
 public:
  static const int kMaxNameSize = 64;
  int32_t* Bind(const char* name, bool histogram);
  int32_t* ptr() { return &count_; }
  int32_t count() { return count_; }
  int32_t sample_total() { return sample_total_; }
  bool is_histogram() { return is_histogram_; }
  void AddSample(int32_t sample);

 private:
  int32_t count_;
  int32_t sample_total_;
  bool is_histogram_;
  uint8_t name_[kMaxNameSize];
};

// A set of counters and associated information.  An instance of this
// class is stored directly in the memory-mapped counters file if
// the --map-counters options is used
class CounterCollection {
 public:
  CounterCollection();
  Counter* GetNextCounter();

 private:
  static const unsigned kMaxCounters = 512;
  uint32_t magic_number_;
  uint32_t max_counters_;
  uint32_t max_name_size_;
  uint32_t counters_in_use_;
  Counter counters_[kMaxCounters];
};

using CounterMap = std::unordered_map<std::string, Counter*>;

class SourceGroup {
 public:
  SourceGroup()
      : next_semaphore_(0),
        done_semaphore_(0),
        thread_(nullptr),
        argv_(nullptr),
        begin_offset_(0),
        end_offset_(0) {}

  ~SourceGroup();

  void Begin(char** argv, int offset) {
    argv_ = const_cast<const char**>(argv);
    begin_offset_ = offset;
  }

  void End(int offset) { end_offset_ = offset; }

  // Returns true on success, false if an uncaught exception was thrown.
  bool Execute(Isolate* isolate);

  void StartExecuteInThread();
  void WaitForThread();
  void JoinThread();

 private:
  class IsolateThread : public base::Thread {
   public:
    explicit IsolateThread(SourceGroup* group);

    void Run() override { group_->ExecuteInThread(); }

   private:
    SourceGroup* group_;
  };

  void ExecuteInThread();

  base::Semaphore next_semaphore_;
  base::Semaphore done_semaphore_;
  base::Thread* thread_;

  void ExitShell(int exit_code);
  Local<String> ReadFile(Isolate* isolate, const char* name);

  const char** argv_;
  int begin_offset_;
  int end_offset_;
};

class SerializationData {
 public:
  SerializationData() : size_(0) {}

  uint8_t* data() { return data_.get(); }
  size_t size() { return size_; }
  const std::vector<std::shared_ptr<v8::BackingStore>>& backing_stores() {
    return backing_stores_;
  }
  const std::vector<std::shared_ptr<v8::BackingStore>>& sab_backing_stores() {
    return sab_backing_stores_;
  }
  const std::vector<WasmModuleObject::TransferrableModule>&
  transferrable_modules() {
    return transferrable_modules_;
  }

 private:
  struct DataDeleter {
    void operator()(uint8_t* p) const { free(p); }
  };

  std::unique_ptr<uint8_t, DataDeleter> data_;
  size_t size_;
  std::vector<std::shared_ptr<v8::BackingStore>> backing_stores_;
  std::vector<std::shared_ptr<v8::BackingStore>> sab_backing_stores_;
  std::vector<WasmModuleObject::TransferrableModule> transferrable_modules_;

 private:
  friend class Serializer;

  DISALLOW_COPY_AND_ASSIGN(SerializationData);
};

class SerializationDataQueue {
 public:
  void Enqueue(std::unique_ptr<SerializationData> data);
  bool Dequeue(std::unique_ptr<SerializationData>* data);
  bool IsEmpty();
  void Clear();

 private:
  base::Mutex mutex_;
  std::vector<std::unique_ptr<SerializationData>> data_;
};

class Worker {
 public:
  explicit Worker(const char* script);
  ~Worker();

  // Post a message to the worker's incoming message queue. The worker will
  // take ownership of the SerializationData.
  // This function should only be called by the thread that created the Worker.
  void PostMessage(std::unique_ptr<SerializationData> data);
  // Synchronously retrieve messages from the worker's outgoing message queue.
  // If there is no message in the queue, block until a message is available.
  // If there are no messages in the queue and the worker is no longer running,
  // return nullptr.
  // This function should only be called by the thread that created the Worker.
  std::unique_ptr<SerializationData> GetMessage();
  // Terminate the worker's event loop. Messages from the worker that have been
  // queued can still be read via GetMessage().
  // This function can be called by any thread.
  void Terminate();
  // Terminate and join the thread.
  // This function can be called by any thread.
  void WaitForThread();

  // Start running the given worker in another thread.
  static bool StartWorkerThread(std::shared_ptr<Worker> worker);

 private:
  class WorkerThread : public base::Thread {
   public:
    explicit WorkerThread(std::shared_ptr<Worker> worker)
        : base::Thread(base::Thread::Options("WorkerThread")),
          worker_(std::move(worker)) {}

    void Run() override;

   private:
    std::shared_ptr<Worker> worker_;
  };

  void ExecuteInThread();
  static void PostMessageOut(const v8::FunctionCallbackInfo<v8::Value>& args);

  base::Semaphore in_semaphore_;
  base::Semaphore out_semaphore_;
  SerializationDataQueue in_queue_;
  SerializationDataQueue out_queue_;
  base::Thread* thread_;
  char* script_;
  base::Atomic32 running_;
};

class PerIsolateData {
 public:
  explicit PerIsolateData(Isolate* isolate);

  ~PerIsolateData();

  inline static PerIsolateData* Get(Isolate* isolate) {
    return reinterpret_cast<PerIsolateData*>(isolate->GetData(0));
  }

  class RealmScope {
   public:
    explicit RealmScope(PerIsolateData* data);
    ~RealmScope();

   private:
    PerIsolateData* data_;
  };

  inline void HostCleanupFinalizationGroup(Local<FinalizationGroup> fg);
  inline MaybeLocal<FinalizationGroup> GetCleanupFinalizationGroup();
  inline void SetTimeout(Local<Function> callback, Local<Context> context);
  inline MaybeLocal<Function> GetTimeoutCallback();
  inline MaybeLocal<Context> GetTimeoutContext();

  AsyncHooks* GetAsyncHooks() { return async_hooks_wrapper_; }

 private:
  friend class Shell;
  friend class RealmScope;
  Isolate* isolate_;
  int realm_count_;
  int realm_current_;
  int realm_switch_;
  Global<Context>* realms_;
  Global<Value> realm_shared_;
  std::queue<Global<Function>> set_timeout_callbacks_;
  std::queue<Global<Context>> set_timeout_contexts_;
  std::queue<Global<FinalizationGroup>> cleanup_finalization_groups_;
  AsyncHooks* async_hooks_wrapper_;

  int RealmIndexOrThrow(const v8::FunctionCallbackInfo<v8::Value>& args,
                        int arg_offset);
  int RealmFind(Local<Context> context);
};

class ShellOptions {
 public:
  enum CodeCacheOptions {
    kNoProduceCache,
    kProduceCache,
    kProduceCacheAfterExecute
  };

  ~ShellOptions() { delete[] isolate_sources; }

  bool send_idle_notification = false;
  bool invoke_weak_callbacks = false;
  bool omit_quit = false;
  bool wait_for_wasm = true;
  bool stress_opt = false;
  bool stress_deopt = false;
  int stress_runs = 1;
  bool interactive_shell = false;
  bool test_shell = false;
  bool expected_to_throw = false;
  bool mock_arraybuffer_allocator = false;
  size_t mock_arraybuffer_allocator_limit = 0;
  bool enable_inspector = false;
  int num_isolates = 1;
  v8::ScriptCompiler::CompileOptions compile_options =
      v8::ScriptCompiler::kNoCompileOptions;
  bool stress_background_compile = false;
  CodeCacheOptions code_cache_options = CodeCacheOptions::kNoProduceCache;
  SourceGroup* isolate_sources = nullptr;
  const char* icu_data_file = nullptr;
  const char* icu_locale = nullptr;
  const char* snapshot_blob = nullptr;
  bool trace_enabled = false;
  const char* trace_path = nullptr;
  const char* trace_config = nullptr;
  const char* lcov_file = nullptr;
  bool disable_in_process_stack_traces = false;
  int read_from_tcp_port = -1;
  bool enable_os_system = false;
  bool quiet_load = false;
  int thread_pool_size = 0;
  bool stress_delay_tasks = false;
  std::vector<const char*> arguments;
  bool include_arguments = true;
};

class Shell : public i::AllStatic {
 public:
  enum PrintResult : bool { kPrintResult = true, kNoPrintResult = false };
  enum ReportExceptions : bool {
    kReportExceptions = true,
    kNoReportExceptions = false
  };
  enum ProcessMessageQueue : bool {
    kProcessMessageQueue = true,
    kNoProcessMessageQueue = false
  };

  static bool ExecuteString(Isolate* isolate, Local<String> source,
                            Local<Value> name, PrintResult print_result,
                            ReportExceptions report_exceptions,
                            ProcessMessageQueue process_message_queue);
  static bool ExecuteModule(Isolate* isolate, const char* file_name);
  static void ReportException(Isolate* isolate, TryCatch* try_catch);
  static Local<String> ReadFile(Isolate* isolate, const char* name);
  static Local<Context> CreateEvaluationContext(Isolate* isolate);
  static int RunMain(Isolate* isolate, int argc, char* argv[], bool last_run);
  static int Main(int argc, char* argv[]);
  static void Exit(int exit_code);
  static void OnExit(Isolate* isolate);
  static void CollectGarbage(Isolate* isolate);
  static bool EmptyMessageQueues(Isolate* isolate);
  static bool CompleteMessageLoop(Isolate* isolate);

  static std::unique_ptr<SerializationData> SerializeValue(
      Isolate* isolate, Local<Value> value, Local<Value> transfer);
  static MaybeLocal<Value> DeserializeValue(
      Isolate* isolate, std::unique_ptr<SerializationData> data);
  static int* LookupCounter(const char* name);
  static void* CreateHistogram(const char* name, int min, int max,
                               size_t buckets);
  static void AddHistogramSample(void* histogram, int sample);
  static void MapCounters(v8::Isolate* isolate, const char* name);

  static void PerformanceNow(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void PerformanceMeasureMemory(
      const v8::FunctionCallbackInfo<v8::Value>& args);

  static void RealmCurrent(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void RealmOwner(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void RealmGlobal(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void RealmCreate(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void RealmNavigate(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void RealmCreateAllowCrossRealmAccess(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static void RealmDetachGlobal(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static void RealmDispose(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void RealmSwitch(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void RealmEval(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void RealmSharedGet(Local<String> property,
                             const PropertyCallbackInfo<Value>& info);
  static void RealmSharedSet(Local<String> property, Local<Value> value,
                             const PropertyCallbackInfo<void>& info);

  static void AsyncHooksCreateHook(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static void AsyncHooksExecutionAsyncId(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static void AsyncHooksTriggerAsyncId(
      const v8::FunctionCallbackInfo<v8::Value>& args);

  static void Print(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void PrintErr(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Write(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void WaitUntilDone(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void NotifyDone(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void QuitOnce(v8::FunctionCallbackInfo<v8::Value>* args);
  static void Quit(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Version(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Read(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void ReadBuffer(const v8::FunctionCallbackInfo<v8::Value>& args);
  static Local<String> ReadFromStdin(Isolate* isolate);
  static void ReadLine(const v8::FunctionCallbackInfo<v8::Value>& args) {
    args.GetReturnValue().Set(ReadFromStdin(args.GetIsolate()));
  }
  static void Load(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetTimeout(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void WorkerNew(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void WorkerPostMessage(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static void WorkerGetMessage(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void WorkerTerminate(const v8::FunctionCallbackInfo<v8::Value>& args);
  // The OS object on the global object contains methods for performing
  // operating system calls:
  //
  // os.system("program_name", ["arg1", "arg2", ...], timeout1, timeout2) will
  // run the command, passing the arguments to the program.  The standard output
  // of the program will be picked up and returned as a multiline string.  If
  // timeout1 is present then it should be a number.  -1 indicates no timeout
  // and a positive number is used as a timeout in milliseconds that limits the
  // time spent waiting between receiving output characters from the program.
  // timeout2, if present, should be a number indicating the limit in
  // milliseconds on the total running time of the program.  Exceptions are
  // thrown on timeouts or other errors or if the exit status of the program
  // indicates an error.
  //
  // os.chdir(dir) changes directory to the given directory.  Throws an
  // exception/ on error.
  //
  // os.setenv(variable, value) sets an environment variable.  Repeated calls to
  // this method leak memory due to the API of setenv in the standard C library.
  //
  // os.umask(alue) calls the umask system call and returns the old umask.
  //
  // os.mkdirp(name, mask) creates a directory.  The mask (if present) is anded
  // with the current umask.  Intermediate directories are created if necessary.
  // An exception is not thrown if the directory already exists.  Analogous to
  // the "mkdir -p" command.
  static void System(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void ChangeDirectory(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetEnvironment(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void UnsetEnvironment(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetUMask(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void MakeDirectory(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void RemoveDirectory(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void HostCleanupFinalizationGroup(Local<Context> context,
                                           Local<FinalizationGroup> fg);
  static MaybeLocal<Promise> HostImportModuleDynamically(
      Local<Context> context, Local<ScriptOrModule> referrer,
      Local<String> specifier);
  static void ModuleResolutionSuccessCallback(
      const v8::FunctionCallbackInfo<v8::Value>& info);
  static void ModuleResolutionFailureCallback(
      const v8::FunctionCallbackInfo<v8::Value>& info);
  static void HostInitializeImportMetaObject(Local<Context> context,
                                             Local<Module> module,
                                             Local<Object> meta);

  // Data is of type DynamicImportData*. We use void* here to be able
  // to conform with MicrotaskCallback interface and enqueue this
  // function in the microtask queue.
  static void DoHostImportModuleDynamically(void* data);
  static void AddOSMethods(v8::Isolate* isolate,
                           Local<ObjectTemplate> os_template);

  static const char* kPrompt;
  static ShellOptions options;
  static ArrayBuffer::Allocator* array_buffer_allocator;

  static void SetWaitUntilDone(Isolate* isolate, bool value);

  static char* ReadCharsFromTcpPort(const char* name, int* size_out);

  static void set_script_executed() { script_executed_.store(true); }
  static bool use_interactive_shell() {
    return (options.interactive_shell || !script_executed_.load()) &&
           !options.test_shell;
  }

  static void WaitForRunningWorkers();
  static void AddRunningWorker(std::shared_ptr<Worker> worker);
  static void RemoveRunningWorker(const std::shared_ptr<Worker>& worker);

 private:
  static Global<Context> evaluation_context_;
  static base::OnceType quit_once_;
  static Global<Function> stringify_function_;
  static const char* stringify_source_;
  static CounterMap* counter_map_;
  // We statically allocate a set of local counters to be used if we
  // don't want to store the stats in a memory-mapped file
  static CounterCollection local_counters_;
  static CounterCollection* counters_;
  static base::OS::MemoryMappedFile* counters_file_;
  static base::LazyMutex context_mutex_;
  static const base::TimeTicks kInitialTicks;

  static base::LazyMutex workers_mutex_;  // Guards the following members.
  static bool allow_new_workers_;
  static std::unordered_set<std::shared_ptr<Worker>> running_workers_;

  // Multiple isolates may update this flag concurrently.
  static std::atomic<bool> script_executed_;

  static void WriteIgnitionDispatchCountersFile(v8::Isolate* isolate);
  // Append LCOV coverage data to file.
  static void WriteLcovData(v8::Isolate* isolate, const char* file);
  static Counter* GetCounter(const char* name, bool is_histogram);
  static Local<String> Stringify(Isolate* isolate, Local<Value> value);
  static void Initialize(Isolate* isolate);
  static void RunShell(Isolate* isolate);
  static bool SetOptions(int argc, char* argv[]);
  static Local<ObjectTemplate> CreateGlobalTemplate(Isolate* isolate);
  static MaybeLocal<Context> CreateRealm(
      const v8::FunctionCallbackInfo<v8::Value>& args, int index,
      v8::MaybeLocal<Value> global_object);
  static void DisposeRealm(const v8::FunctionCallbackInfo<v8::Value>& args,
                           int index);
  static MaybeLocal<Module> FetchModuleTree(v8::Local<v8::Context> context,
                                            const std::string& file_name);
  static ScriptCompiler::CachedData* LookupCodeCache(Isolate* isolate,
                                                     Local<Value> name);
  static void StoreInCodeCache(Isolate* isolate, Local<Value> name,
                               const ScriptCompiler::CachedData* data);
  // We may have multiple isolates running concurrently, so the access to
  // the isolate_status_ needs to be concurrency-safe.
  static base::LazyMutex isolate_status_lock_;
  static std::map<Isolate*, bool> isolate_status_;

  static base::LazyMutex cached_code_mutex_;
  static std::map<std::string, std::unique_ptr<ScriptCompiler::CachedData>>
      cached_code_map_;
};

}  // namespace v8

#endif  // V8_D8_D8_H_
