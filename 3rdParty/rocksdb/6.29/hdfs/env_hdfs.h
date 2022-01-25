//  Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).
//

#pragma once
#include <algorithm>
#include <stdio.h>
#include <time.h>
#include <iostream>
#include "port/sys_time.h"
#include "rocksdb/env.h"
#include "rocksdb/status.h"

#ifdef USE_HDFS
#include <hdfs.h>

namespace ROCKSDB_NAMESPACE {

// Thrown during execution when there is an issue with the supplied
// arguments.
class HdfsUsageException : public std::exception { };

// A simple exception that indicates something went wrong that is not
// recoverable.  The intention is for the message to be printed (with
// nothing else) and the process terminate.
class HdfsFatalException : public std::exception {
public:
  explicit HdfsFatalException(const std::string& s) : what_(s) { }
  virtual ~HdfsFatalException() throw() { }
  virtual const char* what() const throw() {
    return what_.c_str();
  }
private:
  const std::string what_;
};

//
// The HDFS environment for rocksdb. This class overrides all the
// file/dir access methods and delegates the thread-mgmt methods to the
// default posix environment.
//
class HdfsEnv : public Env {

 public:
  explicit HdfsEnv(const std::string& fsname) : fsname_(fsname) {
    posixEnv = Env::Default();
    fileSys_ = connectToPath(fsname_);
  }
  static const char* kClassName() { return "HdfsEnv"; }
  const char* Name() const override { return kClassName(); }
  static const char* kNickName() { return "hdfs"; }
  const char* NickName() const override { return kNickName(); }

  virtual ~HdfsEnv() {
    fprintf(stderr, "Destroying HdfsEnv::Default()\n");
    hdfsDisconnect(fileSys_);
  }

  Status NewSequentialFile(const std::string& fname,
                           std::unique_ptr<SequentialFile>* result,
                           const EnvOptions& options) override;

  Status NewRandomAccessFile(const std::string& fname,
                             std::unique_ptr<RandomAccessFile>* result,
                             const EnvOptions& options) override;

  Status NewWritableFile(const std::string& fname,
                         std::unique_ptr<WritableFile>* result,
                         const EnvOptions& options) override;

  Status NewDirectory(const std::string& name,
                      std::unique_ptr<Directory>* result) override;

  Status FileExists(const std::string& fname) override;

  Status GetChildren(const std::string& path,
                     std::vector<std::string>* result) override;

  Status DeleteFile(const std::string& fname) override;

  Status CreateDir(const std::string& name) override;

  Status CreateDirIfMissing(const std::string& name) override;

  Status DeleteDir(const std::string& name) override;

  Status GetFileSize(const std::string& fname, uint64_t* size) override;

  Status GetFileModificationTime(const std::string& fname,
                                 uint64_t* file_mtime) override;

  Status RenameFile(const std::string& src, const std::string& target) override;

  Status LinkFile(const std::string& /*src*/,
                  const std::string& /*target*/) override {
    return Status::NotSupported(); // not supported
  }

  Status LockFile(const std::string& fname, FileLock** lock) override;

  Status UnlockFile(FileLock* lock) override;

  Status NewLogger(const std::string& fname,
                   std::shared_ptr<Logger>* result) override;

  Status IsDirectory(const std::string& path, bool* is_dir) override;

  void Schedule(void (*function)(void* arg), void* arg, Priority pri = LOW,
                void* tag = nullptr,
                void (*unschedFunction)(void* arg) = 0) override {
    posixEnv->Schedule(function, arg, pri, tag, unschedFunction);
  }

  int UnSchedule(void* tag, Priority pri) override {
    return posixEnv->UnSchedule(tag, pri);
  }

  void StartThread(void (*function)(void* arg), void* arg) override {
    posixEnv->StartThread(function, arg);
  }

  void WaitForJoin() override { posixEnv->WaitForJoin(); }

  unsigned int GetThreadPoolQueueLen(Priority pri = LOW) const override {
    return posixEnv->GetThreadPoolQueueLen(pri);
  }

  Status GetTestDirectory(std::string* path) override {
    return posixEnv->GetTestDirectory(path);
  }

  uint64_t NowMicros() override { return posixEnv->NowMicros(); }

  void SleepForMicroseconds(int micros) override {
    posixEnv->SleepForMicroseconds(micros);
  }

  Status GetHostName(char* name, uint64_t len) override {
    return posixEnv->GetHostName(name, len);
  }

  Status GetCurrentTime(int64_t* unix_time) override {
    return posixEnv->GetCurrentTime(unix_time);
  }

  Status GetAbsolutePath(const std::string& db_path,
                         std::string* output_path) override {
    return posixEnv->GetAbsolutePath(db_path, output_path);
  }

  void SetBackgroundThreads(int number, Priority pri = LOW) override {
    posixEnv->SetBackgroundThreads(number, pri);
  }

  int GetBackgroundThreads(Priority pri = LOW) override {
    return posixEnv->GetBackgroundThreads(pri);
  }

  void IncBackgroundThreadsIfNeeded(int number, Priority pri) override {
    posixEnv->IncBackgroundThreadsIfNeeded(number, pri);
  }

  std::string TimeToString(uint64_t number) override {
    return posixEnv->TimeToString(number);
  }

  static uint64_t gettid() { return Env::Default()->GetThreadID(); }

  uint64_t GetThreadID() const override { return HdfsEnv::gettid(); }

 private:
  std::string fsname_;  // string of the form "hdfs://hostname:port/"
  hdfsFS fileSys_;      //  a single FileSystem object for all files
  Env*  posixEnv;       // This object is derived from Env, but not from
                        // posixEnv. We have posixnv as an encapsulated
                        // object here so that we can use posix timers,
                        // posix threads, etc.

  static const std::string kProto;
  static const std::string pathsep;

  /**
   * If the URI is specified of the form hdfs://server:port/path,
   * then connect to the specified cluster
   * else connect to default.
   */
  hdfsFS connectToPath(const std::string& uri) {
    if (uri.empty()) {
      return nullptr;
    }
    if (uri.find(kProto) != 0) {
      // uri doesn't start with hdfs:// -> use default:0, which is special
      // to libhdfs.
      return hdfsConnectNewInstance("default", 0);
    }
    const std::string hostport = uri.substr(kProto.length());

    std::vector <std::string> parts;
    split(hostport, ':', parts);
    if (parts.size() != 2) {
      throw HdfsFatalException("Bad uri for hdfs " + uri);
    }
    // parts[0] = hosts, parts[1] = port/xxx/yyy
    std::string host(parts[0]);
    std::string remaining(parts[1]);

    int rem = static_cast<int>(remaining.find(pathsep));
    std::string portStr = (rem == 0 ? remaining :
                           remaining.substr(0, rem));

    tPort port = static_cast<tPort>(atoi(portStr.c_str()));
    if (port == 0) {
      throw HdfsFatalException("Bad host-port for hdfs " + uri);
    }
    hdfsFS fs = hdfsConnectNewInstance(host.c_str(), port);
    return fs;
  }

  void split(const std::string &s, char delim,
             std::vector<std::string> &elems) {
    elems.clear();
    size_t prev = 0;
    size_t pos = s.find(delim);
    while (pos != std::string::npos) {
      elems.push_back(s.substr(prev, pos));
      prev = pos + 1;
      pos = s.find(delim, prev);
    }
    elems.push_back(s.substr(prev, s.size()));
  }
};

}  // namespace ROCKSDB_NAMESPACE

#else // USE_HDFS

namespace ROCKSDB_NAMESPACE {

class HdfsEnv : public Env {

 public:
  explicit HdfsEnv(const std::string& /*fsname*/) {
    fprintf(stderr, "You have not build rocksdb with HDFS support\n");
    fprintf(stderr, "Please see hdfs/README for details\n");
    abort();
  }
  static const char* kClassName() { return "HdfsEnv"; }
  const char* Name() const override { return kClassName(); }
  static const char* kNickName() { return "hdfs"; }
  const char* NickName() const override { return kNickName(); }

  virtual ~HdfsEnv() {
  }

  virtual Status NewSequentialFile(const std::string& fname,
                                   std::unique_ptr<SequentialFile>* result,
                                   const EnvOptions& options) override;

  virtual Status NewRandomAccessFile(
      const std::string& /*fname*/,
      std::unique_ptr<RandomAccessFile>* /*result*/,
      const EnvOptions& /*options*/) override {
    return Status::NotSupported();
  }

  virtual Status NewWritableFile(const std::string& /*fname*/,
                                 std::unique_ptr<WritableFile>* /*result*/,
                                 const EnvOptions& /*options*/) override {
    return Status::NotSupported();
  }

  virtual Status NewDirectory(const std::string& /*name*/,
                              std::unique_ptr<Directory>* /*result*/) override {
    return Status::NotSupported();
  }

  virtual Status FileExists(const std::string& /*fname*/) override {
    return Status::NotSupported();
  }

  virtual Status GetChildren(const std::string& /*path*/,
                             std::vector<std::string>* /*result*/) override {
    return Status::NotSupported();
  }

  virtual Status DeleteFile(const std::string& /*fname*/) override {
    return Status::NotSupported();
  }

  virtual Status CreateDir(const std::string& /*name*/) override {
    return Status::NotSupported();
  }

  virtual Status CreateDirIfMissing(const std::string& /*name*/) override {
    return Status::NotSupported();
  }

  virtual Status DeleteDir(const std::string& /*name*/) override {
    return Status::NotSupported();
  }

  virtual Status GetFileSize(const std::string& /*fname*/,
                             uint64_t* /*size*/) override {
    return Status::NotSupported();
  }

  virtual Status GetFileModificationTime(const std::string& /*fname*/,
                                         uint64_t* /*time*/) override {
    return Status::NotSupported();
  }

  virtual Status RenameFile(const std::string& /*src*/,
                            const std::string& /*target*/) override {
    return Status::NotSupported();
  }

  virtual Status LinkFile(const std::string& /*src*/,
                          const std::string& /*target*/) override {
    return Status::NotSupported();
  }

  virtual Status LockFile(const std::string& /*fname*/,
                          FileLock** /*lock*/) override {
    return Status::NotSupported();
  }

  virtual Status UnlockFile(FileLock* /*lock*/) override {
    return Status::NotSupported();
  }

  virtual Status NewLogger(const std::string& /*fname*/,
                           std::shared_ptr<Logger>* /*result*/) override {
    return Status::NotSupported();
  }

  Status IsDirectory(const std::string& /*path*/, bool* /*is_dir*/) override {
    return Status::NotSupported();
  }

  virtual void Schedule(void (* /*function*/)(void* arg), void* /*arg*/,
                        Priority /*pri*/ = LOW, void* /*tag*/ = nullptr,
                        void (* /*unschedFunction*/)(void* arg) = 0) override {}

  virtual int UnSchedule(void* /*tag*/, Priority /*pri*/) override { return 0; }

  virtual void StartThread(void (* /*function*/)(void* arg),
                           void* /*arg*/) override {}

  virtual void WaitForJoin() override {}

  virtual unsigned int GetThreadPoolQueueLen(
      Priority /*pri*/ = LOW) const override {
    return 0;
  }

  virtual Status GetTestDirectory(std::string* /*path*/) override {
    return Status::NotSupported();
  }

  virtual uint64_t NowMicros() override { return 0; }

  virtual void SleepForMicroseconds(int /*micros*/) override {}

  virtual Status GetHostName(char* /*name*/, uint64_t /*len*/) override {
    return Status::NotSupported();
  }

  virtual Status GetCurrentTime(int64_t* /*unix_time*/) override {
    return Status::NotSupported();
  }

  virtual Status GetAbsolutePath(const std::string& /*db_path*/,
                                 std::string* /*outputpath*/) override {
    return Status::NotSupported();
  }

  virtual void SetBackgroundThreads(int /*number*/,
                                    Priority /*pri*/ = LOW) override {}
  virtual int GetBackgroundThreads(Priority /*pri*/ = LOW) override {
    return 0;
  }
  virtual void IncBackgroundThreadsIfNeeded(int /*number*/,
                                            Priority /*pri*/) override {}
  virtual std::string TimeToString(uint64_t /*number*/) override { return ""; }

  virtual uint64_t GetThreadID() const override {
    return 0;
  }
};
}  // namespace ROCKSDB_NAMESPACE

#endif // USE_HDFS
