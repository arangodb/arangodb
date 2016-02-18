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

#ifndef ARANGOD_VOC_BASE_DITCH_H
#define ARANGOD_VOC_BASE_DITCH_H 1

#include "Basics/Common.h"
#include "Basics/Mutex.h"

#include <functional>

struct TRI_document_collection_t;
struct TRI_collection_t;
struct TRI_datafile_t;

namespace arangodb {

class Ditches;

class Ditch {
  friend class Ditches;

 protected:
  Ditch(Ditch const&) = delete;
  Ditch& operator=(Ditch const&) = delete;

  Ditch(Ditches*, char const*, int);

 public:
  virtual ~Ditch();

 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief ditch type
  //////////////////////////////////////////////////////////////////////////////

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
  //////////////////////////////////////////////////////////////////////////////
  /// @brief return the ditch type
  //////////////////////////////////////////////////////////////////////////////

  virtual DitchType type() const = 0;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief return the ditch type name
  //////////////////////////////////////////////////////////////////////////////

  virtual char const* typeName() const = 0;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief return the associated source filename
  //////////////////////////////////////////////////////////////////////////////

  char const* filename() const { return _filename; }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief return the associated source line
  //////////////////////////////////////////////////////////////////////////////

  int line() const { return _line; }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief return the next ditch in the linked list
  //////////////////////////////////////////////////////////////////////////////

  inline Ditch* next() const { return _next; }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief return the link to all ditches
  //////////////////////////////////////////////////////////////////////////////

  Ditches* ditches() { return _ditches; }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief return the associated collection
  //////////////////////////////////////////////////////////////////////////////

  struct TRI_document_collection_t* collection() const;

 protected:
  Ditches* _ditches;

 private:
  Ditch* _prev;
  Ditch* _next;
  char const* _filename;
  int _line;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief document ditch
////////////////////////////////////////////////////////////////////////////////

class DocumentDitch : public Ditch {
  friend class Ditches;

 public:
  DocumentDitch(Ditches* ditches, bool usedByTransaction, char const* filename,
                int line);

  ~DocumentDitch();

 public:
  DitchType type() const override final { return TRI_DITCH_DOCUMENT; }

  char const* typeName() const override final { return "document-reference"; }

  void setUsedByTransaction();
  void setUsedByExternal();

 private:
  uint32_t _usedByExternal;
  bool _usedByTransaction;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief replication ditch
////////////////////////////////////////////////////////////////////////////////

class ReplicationDitch : public Ditch {
 public:
  ReplicationDitch(Ditches* ditches, char const* filename, int line);

  ~ReplicationDitch();

 public:
  DitchType type() const override final { return TRI_DITCH_REPLICATION; }

  char const* typeName() const override final { return "replication"; }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief compaction ditch
////////////////////////////////////////////////////////////////////////////////

class CompactionDitch : public Ditch {
 public:
  CompactionDitch(Ditches* ditches, char const* filename, int line);

  ~CompactionDitch();

 public:
  DitchType type() const override final { return TRI_DITCH_COMPACTION; }

  char const* typeName() const override final { return "compaction"; }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief datafile removal ditch
////////////////////////////////////////////////////////////////////////////////

class DropDatafileDitch : public Ditch {
 public:
  DropDatafileDitch(Ditches* ditches, struct TRI_datafile_t* datafile,
                    void* data,
                    std::function<void(struct TRI_datafile_t*, void*)>,
                    char const* filename, int line);

  ~DropDatafileDitch();

 public:
  DitchType type() const override final { return TRI_DITCH_DATAFILE_DROP; }

  char const* typeName() const override final { return "datafile-drop"; }

  void executeCallback() { _callback(_datafile, _data); }

 private:
  struct TRI_datafile_t* _datafile;
  void* _data;
  std::function<void(struct TRI_datafile_t*, void*)> _callback;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief datafile rename ditch
////////////////////////////////////////////////////////////////////////////////

class RenameDatafileDitch : public Ditch {
 public:
  RenameDatafileDitch(Ditches* ditches, struct TRI_datafile_t* datafile,
                      void* data,
                      std::function<void(struct TRI_datafile_t*, void*)>,
                      char const* filename, int line);

  ~RenameDatafileDitch();

 public:
  DitchType type() const override final { return TRI_DITCH_DATAFILE_RENAME; }

  char const* typeName() const override final { return "datafile-rename"; }

  void executeCallback() { _callback(_datafile, _data); }

 private:
  struct TRI_datafile_t* _datafile;
  void* _data;
  std::function<void(struct TRI_datafile_t*, void*)> _callback;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief collection unload ditch
////////////////////////////////////////////////////////////////////////////////

class UnloadCollectionDitch : public Ditch {
 public:
  UnloadCollectionDitch(
      Ditches* ditches, struct TRI_collection_t* collection, void* data,
      std::function<bool(struct TRI_collection_t*, void*)> callback,
      char const* filename, int line);

  ~UnloadCollectionDitch();

  DitchType type() const override final { return TRI_DITCH_COLLECTION_UNLOAD; }

  char const* typeName() const override final { return "collection-unload"; }

  bool executeCallback() { return _callback(_collection, _data); }

 private:
  struct TRI_collection_t* _collection;
  void* _data;
  std::function<bool(struct TRI_collection_t*, void*)> _callback;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief collection drop ditch
////////////////////////////////////////////////////////////////////////////////

class DropCollectionDitch : public Ditch {
 public:
  DropCollectionDitch(
      arangodb::Ditches* ditches, struct TRI_collection_t* collection,
      void* data, std::function<bool(struct TRI_collection_t*, void*)> callback,
      char const* filename, int line);

  ~DropCollectionDitch();

  DitchType type() const override final { return TRI_DITCH_COLLECTION_DROP; }

  char const* typeName() const override final { return "collection-drop"; }

  bool executeCallback() { return _callback(_collection, _data); }

 private:
  struct TRI_collection_t* _collection;
  void* _data;
  std::function<bool(struct TRI_collection_t*, void*)> _callback;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief doubly linked list of ditches
////////////////////////////////////////////////////////////////////////////////

class Ditches {
 public:
  Ditches(Ditches const&) = delete;
  Ditches& operator=(Ditches const&) = delete;
  Ditches() = delete;

  explicit Ditches(struct TRI_document_collection_t*);
  ~Ditches();

 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief destroy the ditches - to be called on shutdown only
  //////////////////////////////////////////////////////////////////////////////

  void destroy();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief return the associated collection
  //////////////////////////////////////////////////////////////////////////////

  TRI_document_collection_t* collection() const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief run a user-defined function under the lock
  //////////////////////////////////////////////////////////////////////////////

  void executeProtected(std::function<void()>);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief process the first element from the list
  /// the list will remain unchanged if the first element is either a
  /// DocumentDitch, a ReplicationDitch or a CompactionDitch, or if the list
  /// contains any DocumentDitches.
  //////////////////////////////////////////////////////////////////////////////

  Ditch* process(bool&, std::function<bool(Ditch const*)>);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief return the type name of the ditch at the head of the active ditches
  //////////////////////////////////////////////////////////////////////////////

  char const* head();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief return the number of document ditches active
  //////////////////////////////////////////////////////////////////////////////

  uint64_t numDocumentDitches();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief check whether the ditches contain a ditch of a certain type
  //////////////////////////////////////////////////////////////////////////////

  bool contains(Ditch::DitchType);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief unlinks and frees a ditch
  //////////////////////////////////////////////////////////////////////////////

  void freeDitch(Ditch*);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief unlinks and frees a document ditch
  /// this is used for ditches used by transactions or by externals to protect
  /// the flags by the lock
  //////////////////////////////////////////////////////////////////////////////

  void freeDocumentDitch(DocumentDitch*, bool fromTransaction);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief creates a new document ditch and links it
  //////////////////////////////////////////////////////////////////////////////

  DocumentDitch* createDocumentDitch(bool usedByTransaction,
                                     char const* filename, int line);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief creates a new replication ditch and links it
  //////////////////////////////////////////////////////////////////////////////

  ReplicationDitch* createReplicationDitch(char const* filename, int line);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief creates a new compaction ditch and links it
  //////////////////////////////////////////////////////////////////////////////

  CompactionDitch* createCompactionDitch(char const* filename, int line);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief creates a new datafile deletion ditch
  //////////////////////////////////////////////////////////////////////////////

  DropDatafileDitch* createDropDatafileDitch(
      struct TRI_datafile_t* datafile, void* data,
      std::function<void(struct TRI_datafile_t*, void*)> callback,
      char const* filename, int line);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief creates a new datafile rename ditch
  //////////////////////////////////////////////////////////////////////////////

  RenameDatafileDitch* createRenameDatafileDitch(
      struct TRI_datafile_t* datafile, void* data,
      std::function<void(struct TRI_datafile_t*, void*)> callback,
      char const* filename, int line);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief creates a new collection unload ditch
  //////////////////////////////////////////////////////////////////////////////

  UnloadCollectionDitch* createUnloadCollectionDitch(
      struct TRI_collection_t* collection, void* data,
      std::function<bool(struct TRI_collection_t*, void*)> callback,
      char const* filename, int line);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief creates a new collection drop ditch
  //////////////////////////////////////////////////////////////////////////////

  DropCollectionDitch* createDropCollectionDitch(
      struct TRI_collection_t* collection, void* data,
      std::function<bool(struct TRI_collection_t*, void*)> callback,
      char const* filename, int line);

 private:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief inserts the ditch into the linked list of ditches
  //////////////////////////////////////////////////////////////////////////////

  void link(Ditch*);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief unlinks the ditch from the linked list of ditches
  //////////////////////////////////////////////////////////////////////////////

  void unlink(Ditch*);

 private:
  struct TRI_document_collection_t* _collection;

  arangodb::Mutex _lock;
  Ditch* _begin;
  Ditch* _end;
  uint64_t _numDocumentDitches;
};
}

#endif
