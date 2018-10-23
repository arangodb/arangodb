////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_MMFILES_MMFILES_DITCH_H
#define ARANGOD_MMFILES_MMFILES_DITCH_H 1

#include "Basics/Common.h"
#include "Basics/Mutex.h"

struct MMFilesDatafile;

namespace arangodb {
class LogicalCollection;

class MMFilesDitches;

class MMFilesDitch {
  friend class MMFilesDitches;

 protected:
  MMFilesDitch(MMFilesDitch const&) = delete;
  MMFilesDitch& operator=(MMFilesDitch const&) = delete;

  MMFilesDitch(MMFilesDitches*, char const*, int);

 public:
  virtual ~MMFilesDitch();

 public:
  /// @brief ditch type
  enum DitchType {
    TRI_DITCH_DOCUMENT = 1,
    TRI_DITCH_REPLICATION,
    TRI_DITCH_COMPACTION,
    TRI_DITCH_DATAFILE_DROP,
    TRI_DITCH_DATAFILE_RENAME,
    TRI_DITCH_COLLECTION_UNLOAD,
    TRI_DITCH_COLLECTION_DROP
  };

 public:
  /// @brief return the ditch type
  virtual DitchType type() const = 0;

  /// @brief return the ditch type name
  virtual char const* typeName() const = 0;

  /// @brief return the associated source filename
  char const* filename() const { return _filename; }

  /// @brief return the associated source line
  int line() const { return _line; }

  /// @brief return the next ditch in the linked list
  inline MMFilesDitch* next() const { return _next; }

  /// @brief return the link to all ditches
  MMFilesDitches* ditches() { return _ditches; }

  /// @brief return the associated collection
  LogicalCollection* collection() const;

 protected:
  MMFilesDitches* _ditches;

 private:
  MMFilesDitch* _prev;
  MMFilesDitch* _next;
  char const* _filename;
  int _line;
};

/// @brief document ditch
class MMFilesDocumentDitch final : public MMFilesDitch {
  friend class MMFilesDitches;

 public:
  MMFilesDocumentDitch(MMFilesDitches* ditches, bool usedByTransaction, char const* filename,
                int line);

  ~MMFilesDocumentDitch();

 public:
  DitchType type() const override final { return TRI_DITCH_DOCUMENT; }

  char const* typeName() const override final { return "document-reference"; }

  bool usedByTransaction() const { return _usedByTransaction; }

 private:
  bool const _usedByTransaction;
};

/// @brief replication ditch
class MMFilesReplicationDitch final : public MMFilesDitch {
 public:
  MMFilesReplicationDitch(MMFilesDitches* ditches, char const* filename, int line);

  ~MMFilesReplicationDitch();

 public:
  DitchType type() const override final { return TRI_DITCH_REPLICATION; }

  char const* typeName() const override final { return "replication"; }
};

/// @brief compaction ditch
class MMFilesCompactionDitch final : public MMFilesDitch {
 public:
  MMFilesCompactionDitch(MMFilesDitches* ditches, char const* filename, int line);

  ~MMFilesCompactionDitch();

 public:
  DitchType type() const override final { return TRI_DITCH_COMPACTION; }

  char const* typeName() const override final { return "compaction"; }
};

/// @brief datafile removal ditch
class MMFilesDropDatafileDitch final : public MMFilesDitch {
 public:
  MMFilesDropDatafileDitch(MMFilesDitches* ditches, MMFilesDatafile* datafile,
                    LogicalCollection* collection,
                    std::function<void(MMFilesDatafile*, LogicalCollection*)> const& callback,
                    char const* filename, int line);

  ~MMFilesDropDatafileDitch();

 public:
  DitchType type() const override final { return TRI_DITCH_DATAFILE_DROP; }

  char const* typeName() const override final { return "datafile-drop"; }

  void executeCallback() { _callback(_datafile, _collection); _datafile = nullptr; }

 private:
  MMFilesDatafile* _datafile;
  LogicalCollection* _collection;
  std::function<void(MMFilesDatafile*, LogicalCollection*)> _callback;
};

/// @brief datafile rename ditch
class MMFilesRenameDatafileDitch final : public MMFilesDitch {
 public:
  MMFilesRenameDatafileDitch(MMFilesDitches* ditches, MMFilesDatafile* datafile,
                      MMFilesDatafile* compactor, LogicalCollection* collection,
                      std::function<void(MMFilesDatafile*, MMFilesDatafile*, LogicalCollection*)> const& callback,
                      char const* filename, int line);

  ~MMFilesRenameDatafileDitch();

 public:
  DitchType type() const override final { return TRI_DITCH_DATAFILE_RENAME; }

  char const* typeName() const override final { return "datafile-rename"; }

  void executeCallback() { _callback(_datafile, _compactor, _collection); }

 private:
  MMFilesDatafile* _datafile;
  MMFilesDatafile* _compactor;
  LogicalCollection* _collection;
  std::function<void(MMFilesDatafile*, MMFilesDatafile*, LogicalCollection*)> _callback;
};

/// @brief collection unload ditch
class MMFilesUnloadCollectionDitch final : public MMFilesDitch {
 public:
  MMFilesUnloadCollectionDitch(
      MMFilesDitches* ditches, LogicalCollection* collection,
      std::function<bool(LogicalCollection*)> const& callback,
      char const* filename, int line);

  ~MMFilesUnloadCollectionDitch();

  DitchType type() const override final { return TRI_DITCH_COLLECTION_UNLOAD; }

  char const* typeName() const override final { return "collection-unload"; }

  bool executeCallback() { return _callback(_collection); }

 private:
  LogicalCollection* _collection;
  std::function<bool(LogicalCollection*)> _callback;
};

/// @brief collection drop ditch
class MMFilesDropCollectionDitch final : public MMFilesDitch {
 public:
  MMFilesDropCollectionDitch(
    arangodb::MMFilesDitches* ditches,
    arangodb::LogicalCollection& collection,
    std::function<bool(arangodb::LogicalCollection&)> const& callback,
    char const* filename,
    int line
  );

  ~MMFilesDropCollectionDitch();

  DitchType type() const override final { return TRI_DITCH_COLLECTION_DROP; }

  char const* typeName() const override final { return "collection-drop"; }

  bool executeCallback() { return _callback(_collection); }

 private:
  arangodb::LogicalCollection& _collection;
  std::function<bool(arangodb::LogicalCollection&)> _callback;
};

/// @brief doubly linked list of ditches
class MMFilesDitches {
 public:
  MMFilesDitches(MMFilesDitches const&) = delete;
  MMFilesDitches& operator=(MMFilesDitches const&) = delete;
  MMFilesDitches() = delete;

  explicit MMFilesDitches(LogicalCollection*);
  ~MMFilesDitches();

 public:
  /// @brief destroy the ditches - to be called on shutdown only
  void destroy();

  /// @brief return the associated collection
  LogicalCollection* collection() const;

  /// @brief run a user-defined function under the lock
  void executeProtected(std::function<void()>);

  /// @brief process the first element from the list
  /// the list will remain unchanged if the first element is either a
  /// MMFilesDocumentDitch, a MMFilesReplicationDitch or a MMFilesCompactionDitch, or if the list
  /// contains any MMFilesDocumentMMFilesDitches.
  MMFilesDitch* process(bool&, std::function<bool(MMFilesDitch const*)>);

  /// @brief return the type name of the ditch at the head of the active ditches
  char const* head();

  /// @brief return the number of document ditches active
  uint64_t numMMFilesDocumentMMFilesDitches();

  /// @brief check whether the ditches contain a ditch of a certain type
  bool contains(MMFilesDitch::DitchType);

  /// @brief unlinks and frees a ditch
  void freeDitch(MMFilesDitch*);

  /// @brief unlinks and frees a document ditch
  /// this is used for ditches used by transactions or by externals to protect
  /// the flags by the lock
  void freeMMFilesDocumentDitch(MMFilesDocumentDitch*, bool fromTransaction);

  /// @brief creates a new document ditch and links it
  MMFilesDocumentDitch* createMMFilesDocumentDitch(bool usedByTransaction,
                                     char const* filename, int line);

  /// @brief creates a new replication ditch and links it
  MMFilesReplicationDitch* createMMFilesReplicationDitch(char const* filename, int line);

  /// @brief creates a new compaction ditch and links it
  MMFilesCompactionDitch* createMMFilesCompactionDitch(char const* filename, int line);

  /// @brief creates a new datafile deletion ditch
  MMFilesDropDatafileDitch* createMMFilesDropDatafileDitch(
      MMFilesDatafile* datafile, LogicalCollection* collection, 
      std::function<void(MMFilesDatafile*, LogicalCollection*)> const& callback,
      char const* filename, int line);

  /// @brief creates a new datafile rename ditch
  MMFilesRenameDatafileDitch* createMMFilesRenameDatafileDitch(
      MMFilesDatafile* datafile, MMFilesDatafile* compactor, LogicalCollection* collection,
      std::function<void(MMFilesDatafile*, MMFilesDatafile*, LogicalCollection*)> const& callback,
      char const* filename, int line);

  /// @brief creates a new collection unload ditch
  MMFilesUnloadCollectionDitch* createMMFilesUnloadCollectionDitch(
      LogicalCollection* collection,
      std::function<bool(LogicalCollection*)> const& callback,
      char const* filename, int line);

  /// @brief creates a new collection drop ditch
  MMFilesDropCollectionDitch* createMMFilesDropCollectionDitch(
    arangodb::LogicalCollection& collection,
    std::function<bool(arangodb::LogicalCollection&)> const& callback,
    char const* filename,
    int line
  );

 private:
  /// @brief inserts the ditch into the linked list of ditches
  void link(MMFilesDitch*);

  /// @brief unlinks the ditch from the linked list of ditches
  void unlink(MMFilesDitch*);

 private:
  LogicalCollection* _collection;

  arangodb::Mutex _lock;
  MMFilesDitch* _begin;
  MMFilesDitch* _end;
  uint64_t _numMMFilesDocumentMMFilesDitches;
};
}

#endif