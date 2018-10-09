////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 by EMC Corporation, All Rights Reserved
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
/// Copyright holder is EMC Corporation
///
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include <memory>
#include <mutex>

#if defined(_MSC_VER)
  #include <Windows.h> // must be included before DbgHelp.h
  #include <Psapi.h>
  #include <DbgHelp.h>

  #include <stdio.h> // for *printf(...)
#else
  #include <cstdio> // for *printf(...)
  #include <thread>

  #include <cxxabi.h> // for abi::__cxa_demangle(...)
  #include <dlfcn.h> // for dladdr(...)
  #include <execinfo.h>
  #include <unistd.h> // for STDIN_FILENO/STDOUT_FILENO/STDERR_FILENO
  #include <sys/wait.h> // for waitpid(...)
#endif

#if defined(__APPLE__)
  #include <libproc.h> // for proc_pidpath(...)
#endif

#if defined(USE_LIBBFD)
  #include <bfd.h>
#endif

#if defined(USE_LIBUNWIND)
  #include <libunwind.h>
#endif

#include "file_utils.hpp"
#include "shared.hpp"
#include "singleton.hpp"
#include "thread_utils.hpp"

#include "misc.hpp"
#include "log.hpp"

NS_LOCAL

MSVC_ONLY(__pragma(warning(push)))
MSVC_ONLY(__pragma(warning(disable: 4996))) // the compiler encountered a deprecated declaration
static FILE* dev_null() {
  static auto dev_null = file_open(static_cast<file_path_t>(nullptr), "wb");
  return dev_null.get();
}
MSVC_ONLY(__pragma(warning(pop)))

class file_streambuf: public std::streambuf {
 public:
  typedef std::streambuf::char_type char_type;
  typedef std::streambuf::int_type int_type;

  file_streambuf(FILE* out): out_(out ? out : dev_null()) {
  }

  file_streambuf& operator=(FILE* out) {
    out_ = out ? out : dev_null();
    return *this;
  }

  virtual std::streamsize xsputn(const char_type* data, std::streamsize size) override {
    return std::fwrite(data, sizeof(char_type), size, out_);
  }

  virtual int_type overflow(int_type ch) override {
    return std::fputc(ch, out_);
  }

 private:
  FILE* out_;
};

class logger_ctx: public iresearch::singleton<logger_ctx> {
 public:
  logger_ctx()
    : singleton(),
      stack_trace_level_(irs::logger::level_t::IRL_DEBUG) {
    // set everything up to and including INFO to stderr
    for (size_t i = 0, last = iresearch::logger::IRL_INFO; i <= last; ++i) {
      out_[i].file_ = stderr;
      out_[i].streambuf_ = stderr;
    }
  }

  bool enabled(iresearch::logger::level_t level) const { return dev_null() != out_[level].file_; }
  FILE* file(iresearch::logger::level_t level) const { return out_[level].file_; }
  logger_ctx& output(iresearch::logger::level_t level, FILE* out) {
    out_[level].file_ = out ? out : dev_null();
    out_[level].streambuf_ = out;
    return *this;
  }
  logger_ctx& output_le(iresearch::logger::level_t level, FILE* out) {
    for (size_t i = 0, count = IRESEARCH_COUNTOF(out_); i < count; ++i) {
      output(static_cast<iresearch::logger::level_t>(i), i > level ? nullptr : out);
    }
    return *this;
  }
  irs::logger::level_t stack_trace_level() { return stack_trace_level_; }
  logger_ctx& stack_trace_level(irs::logger::level_t level) {
    stack_trace_level_ = level;
    return *this;
  }
  std::ostream& stream(iresearch::logger::level_t level) { return out_[level].stream_; }

 private:
  struct level_ctx_t {
    FILE* file_;
    std::ostream stream_;
    file_streambuf streambuf_;
    level_ctx_t(): file_(dev_null()), stream_(&streambuf_), streambuf_(nullptr) {}
  };

  level_ctx_t out_[iresearch::logger::IRL_TRACE + 1]; // IRL_TRACE is the last value, +1 for 0'th id
  irs::logger::level_t stack_trace_level_;
};

typedef std::function<void(const char* file, size_t line, const char* fn)> bfd_callback_type_t;
bool file_line_bfd(const bfd_callback_type_t& callback, const char* obj, void* addr); // predeclaration
bool stack_trace_libunwind(iresearch::logger::level_t level); // predeclaration

#if defined(_MSC_VER)
  DWORD stack_trace_win32(iresearch::logger::level_t level, struct _EXCEPTION_POINTERS* ex) {
    auto* out = output(level);
    static std::mutex mutex;
    SCOPED_LOCK(mutex); // win32 stack trace API is not thread safe

    if (!ex || !ex->ContextRecord) {
      std::fprintf(out, "No stack_trace available\n");
      std::fflush(out);
      return EXCEPTION_EXECUTE_HANDLER;
    }

    CONTEXT ctx = *(ex->ContextRecord); // create a modifiable copy
    STACKFRAME frame = { 0 };

    frame.AddrPC.Offset = ctx.Rip;
    frame.AddrPC.Mode = AddrModeFlat;
    frame.AddrFrame.Offset = ctx.Rbp;
    frame.AddrFrame.Mode = AddrModeFlat;

    auto process = GetCurrentProcess();

    // SYMOPT_DEFERRED_LOADS required to avoid win32 GUI exception
    //SymSetOptions(SymGetOptions() | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES | SYMOPT_UNDNAME);
    // always load symbols
    SymInitialize(process, NULL, TRUE);

    HMODULE module_handle;
    DWORD module_handle_size;

    if (!EnumProcessModules(process, &module_handle, sizeof(HMODULE), &module_handle_size)) {
      std::fprintf(out, "Failed to enumerate modules for current process\n");
      std::fflush(out);
      return EXCEPTION_EXECUTE_HANDLER;
    }

    MODULEINFO module_info;

    GetModuleInformation(process, module_handle, &module_info, sizeof(MODULEINFO));

    auto image_type = ImageNtHeader(module_info.lpBaseOfDll)->FileHeader.Machine;
    auto thread = GetCurrentThread();
    char symbol_buf[sizeof(IMAGEHLP_SYMBOL) + 256];
    auto& symbol = reinterpret_cast<IMAGEHLP_SYMBOL&>(symbol_buf);

    symbol.SizeOfStruct = sizeof(IMAGEHLP_SYMBOL);
    symbol.MaxNameLength = 256;

    IMAGEHLP_LINE line = { 0 };

    line.SizeOfStruct = sizeof(IMAGEHLP_LINE);

    IMAGEHLP_MODULE module = { 0 };

    module.SizeOfStruct = sizeof(IMAGEHLP_MODULE);

    DWORD offset_from_line = 0;
    DWORD64 offset_from_symbol = 0;
    bool skip_frame = true;
    size_t frame_count = size_t(-1);

    while (StackWalk64(image_type, process, thread, &frame, &ctx, NULL, NULL, NULL, NULL)) {
      static const std::string stack_trace_fn_symbol("iresearch::logger::stack_trace");
      auto has_module = SymGetModuleInfo(process, frame.AddrPC.Offset, &module);
      auto has_symbol = SymGetSymFromAddr(process, frame.AddrPC.Offset, &offset_from_symbol, &symbol);
      auto has_line = SymGetLineFromAddr(process, frame.AddrPC.Offset, &offset_from_line, &line);

      // skip frames until stack_trace entry point is encountered
      if (skip_frame) {
        if (has_symbol && stack_trace_fn_symbol == symbol.Name) {
          skip_frame = false; // next frame is the start of the exception stack trace
        }

        continue;
      }

      std::fprintf(out, "#%Iu ", ++frame_count);

      if (has_module) {
        std::fprintf(out, "%s", module.ModuleName);
      }

      if (has_symbol) {
        std::fprintf(out, "(%s+0x%Ix)", symbol.Name, offset_from_symbol);
      }

      if (has_line) {
        std::fprintf(out, ": %s:%u+0x%x", line.FileName, line.LineNumber, offset_from_line);
      }

      std::fprintf(out, "\n");
    }

    if (skip_frame) {
      std::fprintf(out, "No stack_trace available outside of logger\n");
    }

    std::fflush(out);

    return EXCEPTION_EXECUTE_HANDLER;
  }
#else
  #if defined(__APPLE__)
    bool file_line_addr2line(iresearch::logger::level_t level, const char* obj, const char* addr) {
      auto fd = fileno(output(level));
      auto pid = fork();

      if (!pid) {
        char name_buf[PROC_PIDPATHINFO_MAXSIZE]; // buffer size requirement for proc_pidpath(...)
        auto ppid = getppid();

        if (0 < proc_pidpath(ppid, name_buf, sizeof(name_buf))) {
          // The exec() family of functions replaces the current process image with a new process image.
          // The exec() functions only return if an error has occurred.
          dup2(fd, 1); // redirect stdout to fd
          dup2(fd, 2); // redirect stderr to fd
          execlp("addr2line", "addr2line", "-e", obj, addr, NULL);
        }

        exit(1);
      }

      int status;

      return 0 < waitpid(pid, &status, 0) && !WEXITSTATUS(status);
    }
  #else
    bool file_line_addr2line(iresearch::logger::level_t level, const char* obj, const char* addr) {
      auto fd = fileno(output(level));
      auto pid = fork();

      if (!pid) {
        CONSTEXPR const size_t pid_size = sizeof(pid_t)*3 + 1; // aproximately 3 chars per byte +1 for \0
        CONSTEXPR const char PROC_EXE[] = { "/proc//exe" };
        CONSTEXPR const size_t name_size = IRESEARCH_COUNTOF(PROC_EXE) + pid_size + 1; // +1 for \0
        char pid_buf[pid_size];
        char name_buf[name_size];
        auto ppid = getppid();

        snprintf(pid_buf, pid_size, "%d", ppid);
        snprintf(name_buf, name_size, "/proc/%d/exe", ppid);

        // The exec() family of functions replaces the current process image with a new process image.
        // The exec() functions only return if an error has occurred.
        dup2(fd, 1); // redirect stdout to fd
        dup2(fd, 2); // redirect stderr to fd
        execlp("addr2line", "addr2line", "-e", obj, addr, nullptr);
        exit(1);
      }

      int status;

      return 0 < waitpid(pid, &status, 0) && !WEXITSTATUS(status);
    }
  #endif

  bool file_line_bfd(const bfd_callback_type_t& callback, const char* obj, const char* addr) {
    char* suffix;
    auto address = std::strtoll(addr, &suffix, 0);

    // negative addresses or parse errors are deemed invalid
    return address < 0 || suffix ? false : file_line_bfd(callback, obj, (void*)address);
  }

  std::shared_ptr<char> proc_name_demangle(const char* symbol) {
    int status;

    // abi::__cxa_demangle(...) expects 'output_buffer' to be malloc()ed and does realloc()/free() internally
    std::shared_ptr<char> buf(abi::__cxa_demangle(symbol, nullptr, nullptr, &status), std::free);

    return buf && !status ? std::move(buf) : nullptr;
  }

  #if defined(__APPLE__)
    bool stack_trace_gdb(iresearch::logger::level_t level) {
      auto fd = fileno(output(level));
      auto pid = fork();

      if (!pid) {
        CONSTEXPR const size_t pid_size = sizeof(pid_t)*3 + 1; // approximately 3 chars per byte +1 for \0
        char pid_buf[pid_size];
        char name_buf[PROC_PIDPATHINFO_MAXSIZE]; // buffer size requirement for proc_pidpath(...)
        auto ppid = getppid();

        snprintf(pid_buf, pid_size, "%d", ppid);

        if (0 < proc_pidpath(ppid, name_buf, sizeof(name_buf))) {
          // The exec() family of functions replaces the current process image with a new process image.
          // The exec() functions only return if an error has occurred.
          dup2(fd, 1); // redirect stdout to fd
          dup2(fd, 2); // redirect stderr to fd
          execlp("gdb", "gdb", "-n", "-nx", "-return-child-result", "-batch", "-ex", "thread", "-ex", "bt", name_buf, pid_buf, NULL);
        }

        exit(1);
      }

      int status;

      return 0 < waitpid(pid, &status, 0) && !WEXITSTATUS(status);
    }
  #else
    bool stack_trace_gdb(iresearch::logger::level_t level) {
      auto fd = fileno(output(level));
      auto pid = fork();

      if (!pid) {
        CONSTEXPR const size_t pid_size = sizeof(pid_t)*3 + 1; // approximately 3 chars per byte +1 for \0
        CONSTEXPR const char PROC_EXE[] = { "/proc//exe" };
        CONSTEXPR const size_t name_size = IRESEARCH_COUNTOF(PROC_EXE) + pid_size + 1; // +1 for \0
        char pid_buf[pid_size];
        char name_buf[name_size];
        auto ppid = getppid();

        snprintf(pid_buf, pid_size, "%d", ppid);
        snprintf(name_buf, name_size, "/proc/%d/exe", ppid);

        // The exec() family of functions replaces the current process image with a new process image.
        // The exec() functions only return if an error has occurred.
        dup2(fd, 1); // redirect stdout to fd
        dup2(fd, 2); // redirect stderr to fd
        execlp("gdb", "gdb", "-n", "-nx", "-return-child-result", "-batch", "-ex", "thread", "-ex", "bt", name_buf, pid_buf, nullptr);
        exit(1);
      }

      int status;

      return 0 < waitpid(pid, &status, 0) && !WEXITSTATUS(status);
    }
  #endif

  void stack_trace_posix(iresearch::logger::level_t level) {
    auto* out = output(level);
    CONSTEXPR const size_t frames_max = 128; // arbitrary size
    void* frames_buf[frames_max];
    auto frames_count = backtrace(frames_buf, frames_max);

    if (frames_count < 2) {
      return; // nothing to log
    }

    frames_count -= 2; // -2 to skip stack_trace(...) + stack_trace_posix(...)

    auto frames_buf_ptr = frames_buf + 2; // +2 to skip backtrace(...) + stack_trace_posix(...)
    int pipefd[2];

    if (pipe(pipefd)) {
      std::fprintf(out, "Failed to output detailed stack trace to stream, outputting plain stack trace\n");
      std::fflush(out);
      backtrace_symbols_fd(frames_buf_ptr, frames_count, fileno(out)); // fallback plain output
      return;
    }

    size_t buf_len = 0;
    CONSTEXPR const size_t buf_size = 1024; // arbitrary size
    char buf[buf_size];
    std::thread thread([&pipefd, level, out, &buf, &buf_len, buf_size]()->void {
      for (char ch; read(pipefd[0], &ch, 1) > 0;) {
        if (ch != '\n') {
          if (buf_len < buf_size - 1) {
            buf[buf_len++] = ch;
            continue;
          }

          if (buf_len < buf_size) {
            buf[buf_len++] = '\0';
            std::fprintf(out, "%s", buf);
          }

          std::fputc(ch, out); // line longer than buf, output line directly
          continue;
        }

        if (buf_len >= buf_size) {
          buf_len = 0;
          std::fprintf(out, "\n");
          std::fflush(out);
          continue;
        }

        char* addr_start = nullptr;
        char* addr_end = nullptr;
        char* fn_start = nullptr;
        char* offset_start = nullptr;
        char* offset_end = nullptr;
        char* path_start = buf;

        for (size_t i = 0; i < buf_len; ++i) {
          switch(buf[i]) {
           case '(':
            fn_start = &buf[i + 1];
            continue;
           case '+':
            offset_start = &buf[i + 1];
            continue;
           case ')':
            offset_end = &buf[i];
            continue;
           case '[':
            addr_start = &buf[i + 1];
            continue;
           case ']':
            addr_end = &buf[i];
            continue;
          }
        }

        buf[buf_len] = '\0';
        buf_len = 0;

        auto fn_end = offset_start ? offset_start - 1 : nullptr;
        auto path_end = fn_start ? fn_start - 1 : (addr_start ? addr_start - 1 : nullptr);
        bfd_callback_type_t callback = [out](
          const char* file, size_t line, const char* fn
        )->void {
          UNUSED(fn);

          if (file) {
            std::fprintf(out, "%s:%lu\n", file, line);
          } else {
            std::fprintf(out, "??:?");
          }

          std::fflush(out);
        };

        if (path_start < path_end) {
          if (offset_start < offset_end) {
            std::fwrite(path_start, sizeof(char), path_end - path_start, out);

            if (fn_start < fn_end) {
              std::fprintf(out, "(");
              *fn_end = '\0';

              auto fn_name = proc_name_demangle(fn_start);

              if(fn_name) {
                std::fprintf(out, "%s", fn_name.get());
              } else {
                std::fwrite(fn_start, sizeof(char), fn_end - fn_start, out);
              }

              std::fprintf(out, "+%s\n", offset_start);
              std::fflush(out);
            } else {
              std::fprintf(out, "%s ", path_end);
              *offset_end = '\0';
              *path_end = '\0';

              if (!file_line_bfd(callback, path_start, offset_start) && !file_line_addr2line(level, path_start, offset_start)) {
                std::fprintf(out, "\n");
                std::fflush(out);
              }
            }

            continue;
          }

          if (addr_start < addr_end) {
            std::fprintf(out, "%s ", path_start);
            *addr_end = '\0';
            *path_end = '\0';

            if (!file_line_bfd(callback, path_start, addr_start) && !file_line_addr2line(level, path_start, addr_start)) {
              std::fprintf(out, "\n");
              std::fflush(out);
            }

            continue;
          }
        }

        std::fprintf(out, "%s\n", buf);
        std::fflush(out);
      }
    });

    backtrace_symbols_fd(frames_buf_ptr, frames_count, pipefd[1]);
    close(pipefd[1]);
    thread.join();
    close(pipefd[0]);
    std::fprintf(out, "\n");
    std::fflush(out);
  }
#endif

#if defined(USE_LIBBFD)
  bool file_line_bfd(const bfd_callback_type_t& callback, const char* obj, void* addr) {
    struct bfd_init_t { bfd_init_t() { bfd_init(); } };
    static bfd_init_t static_bfd_init; // one-time init of BFD
    UNUSED(static_bfd_init);

    auto* file_bfd = bfd_openr(obj, nullptr);

    if (!file_bfd || !bfd_check_format(file_bfd, bfd_object)) {
      return false;
    }

    CONSTEXPR const size_t symbols_size = 1048576; // arbitrary size (4K proved too small)
    auto symbol_bytes = bfd_get_symtab_upper_bound(file_bfd);

    if (symbol_bytes <= 0 || symbols_size < size_t(symbol_bytes)) {
      return false; // prealocated buffer is not large enough
    }

    char symbols[symbols_size];
    asymbol** symbols_ptr = (asymbol**)&symbols;
    auto symbols_len = bfd_canonicalize_symtab(file_bfd, symbols_ptr);
    UNUSED(symbols_len); // actual number of symbol pointers, not including the NULL
    auto* section = bfd_get_section_by_name(file_bfd, ".text"); // '.text' is a hardcoded section name
    auto bfd_addr = bfd_vma(addr);

    if (!section || bfd_addr < section->vma) {
      return false; // no section or address not within section
    }

    auto offset = bfd_addr - section->vma;
    const char* file;
    const char* func;
    unsigned int line;

    if (!bfd_find_nearest_line(file_bfd, section, symbols_ptr, offset, &file, &func, &line)) {
      return false; // unable to obtain file/line
    }

    callback(file, line, func);

    return true;
  }

#else
  bool file_line_bfd(const bfd_callback_type_t&, const char*, void*) {
    return false;
  }
#endif

#if defined(USE_LIBUNWIND)
  bool file_line_bfd(const bfd_callback_type_t& callback, const char* obj, unw_word_t addr) {
    return file_line_bfd(callback, obj, (void*)addr);
  }

  bool file_line_addr2line(iresearch::logger::level_t level, const char* obj, unw_word_t addr) {
    size_t addr_size = sizeof(unw_word_t)*3 + 2 + 1; // aproximately 3 chars per byte +2 for 0x, +1 for \0
    char addr_buf[addr_size];

    snprintf(addr_buf, addr_size, "0x%lx", addr);

    return file_line_addr2line(level, obj, addr_buf);
  }

  bool stack_trace_libunwind(iresearch::logger::level_t level) {
    unw_context_t ctx;
    unw_cursor_t cursor;

    if (0 != unw_getcontext(&ctx) || 0 != unw_init_local(&cursor, &ctx)) {
      return false;
    }

    // skip backtrace(...) + stack_trace_libunwind(...)
    if (unw_step(&cursor) <= 0) {
      return true; // nothing to log
    }

    auto* out = output(level);
    unw_word_t instruction_pointer;

    while (unw_step(&cursor) > 0) {
      if (0 != unw_get_reg(&cursor, UNW_REG_IP, &instruction_pointer)) {
        std::fprintf(out, "<unknown>\n");
        std::fflush(out);
        continue; // no instruction pointer available
      }

      Dl_info dl_info;

      // resolve function/flie/line via dladdr() + addr2line
      if (0 != dladdr((void*)instruction_pointer, &dl_info) || !dl_info.dli_fname) {
        // there appears to be a magic number base address which should not be subtracted from the instruction_pointer
        static const void* static_fbase = (void*)0x400000;
        auto addr = instruction_pointer - (static_fbase == dl_info.dli_fbase ? unw_word_t(dl_info.dli_saddr) : unw_word_t(dl_info.dli_fbase));
        bool use_addr2line = false;
        bfd_callback_type_t callback = [level, out, instruction_pointer, &addr, &dl_info, &use_addr2line](const char* file, size_t line, const char* fn)->void {
          std::fprintf(out, "%s(", dl_info.dli_fname ? dl_info.dli_fname : "\?\?");

          auto proc_name = proc_name_demangle(fn);

          if (!proc_name) {
            proc_name = proc_name_demangle(dl_info.dli_sname);
          }

          if (proc_name) {
            std::fprintf(out, "%s", proc_name.get());
          } else if (fn) {
            std::fprintf(out, "%s", fn);
          } else if (dl_info.dli_sname) {
            std::fprintf(out, "%s", dl_info.dli_sname);
          }

          // match offsets in Posix backtrace output
          std::fprintf(
            out, "+0x%lx)[0x%lx] ",
            dl_info.dli_saddr ? (instruction_pointer - unw_word_t(dl_info.dli_saddr)) : addr, instruction_pointer
          );

          if (use_addr2line) {
            if (!file_line_addr2line(level, dl_info.dli_fname, addr)) {
              std::fprintf(out, "\n");
              std::fflush(out);
            }

            return;
          }

          std::fprintf(out, "%s:", file ? file : "??");

          if (line) {
            std::fprintf(out, "%lu", line);
          } else {
            std::fprintf(out, "?");
          }

          std::fprintf(out, "\n");
          std::fflush(out);
        };

        if (!file_line_bfd(callback, dl_info.dli_fname, addr)) {
          use_addr2line = true;
          callback(nullptr, 0, nullptr);
        }

        continue;
      }

      CONSTEXPR const size_t proc_size = 1024; // arbitrary size
      char proc_buf[proc_size];
      unw_word_t offset;

      if (0 != unw_get_proc_name(&cursor, proc_buf, proc_size, &offset)) {
        std::fprintf(out, "\?\?[0x%lx]\n", instruction_pointer);
        std::fflush(out);
        continue; // no function info available
      }

      auto proc_name = proc_name_demangle(proc_buf);

      std::fprintf(out, "\?\?(%s+0x%lx)[0x%lx]\n", proc_name ? proc_name.get() : proc_buf, offset, instruction_pointer);
      std::fflush(out);
    }

    return true;
  }
#else
  bool stack_trace_libunwind(iresearch::logger::level_t) {
    return false;
  }
#endif

NS_END

NS_ROOT
NS_BEGIN(logger)

bool enabled(level_t level) {
  return logger_ctx::instance().enabled(level);
}

FILE* output(level_t level) {
  return logger_ctx::instance().file(level);
}

void output(level_t level, FILE* out) {
  logger_ctx::instance().output(level, out);
}

void output_le(level_t level, FILE* out) {
  logger_ctx::instance().output_le(level, out);
}

void stack_trace(level_t level) {
  if (!enabled(level)) {
    return; // skip generating trace if logging is disabled for this level altogether
  }

  #if defined(_MSC_VER)
    __try {
      RaiseException(1, 0, 0, NULL);
    } __except(stack_trace_win32(level, GetExceptionInformation())) {
      return;
    }

    stack_trace_win32(level, nullptr);
  #else
    try {
      if (!stack_trace_libunwind(level) && !stack_trace_gdb(level)) {
        stack_trace_posix(level);
      }
    } catch(std::bad_alloc&) {
      stack_trace_nomalloc(level, 2); // +2 to skip stack_trace()
      throw;
    }
  #endif
}

void stack_trace(level_t level, const std::exception_ptr& eptr) {
  UNUSED(eptr); // no known way to get original instruction pointer from exception_ptr

  if (!enabled(level)) {
    return; // skip generating trace if logging is disabled for this level altogether
  }

  // copy of stack_trace() for proper ignored-frames calculation
  #if defined(_MSC_VER)
    __try {
      RaiseException(1, 0, 0, NULL);
    } __except(stack_trace_win32(level, GetExceptionInformation())) {
      return;
    }

    stack_trace_win32(level, nullptr);
  #else
    try {
      if (!stack_trace_libunwind(level) && !stack_trace_gdb(level)) {
        stack_trace_posix(level);
      }
    } catch(std::bad_alloc&) {
      stack_trace_nomalloc(level, 2); // +2 to skip stack_trace()
      throw;
    }
  #endif
}

#ifndef _MSC_VER
  void stack_trace_nomalloc(level_t level, size_t skip) {
    if (!enabled(level)) {
      return; // skip generating trace if logging is disabled for this level altogether
    }

    CONSTEXPR const size_t frames_max = 128; // arbitrary size
    void* frames_buf[frames_max];
    auto frames_count = backtrace(frames_buf, frames_max);

    if (frames_count > 0 && size_t(frames_count) > skip) {
      static std::mutex mtx;
      SCOPED_LOCK(mtx);
      backtrace_symbols_fd(frames_buf + skip, frames_count - skip, fileno(output(level)));
    }
  }
#endif

irs::logger::level_t stack_trace_level() {
  return logger_ctx::instance().stack_trace_level();
}

void stack_trace_level(level_t level) {
  logger_ctx::instance().stack_trace_level(level);
}

std::ostream& stream(level_t level) {
  return logger_ctx::instance().stream(level);
}

NS_END // logger
NS_END

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------