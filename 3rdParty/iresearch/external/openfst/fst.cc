// fst.cc

// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Copyright 2005-2010 Google, Inc.
// Author: riley@google.com (Michael Riley)
//
// \file
// FST definitions.

#include <fst/fst.h>

// Include these so they are registered
#include <fst/compact-fst.h>
#include <fst/const-fst.h>
#include <fst/edit-fst.h>
#include <fst/matcher-fst.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <fst/vector-fst.h>

// FST flag definitions

DEFINE_bool(fst_verify_properties, false,
            "Verify fst properties queried by TestProperties");

DEFINE_string(fst_weight_separator, ",",
              "Character separator between printed composite weights; "
              "must be a single character");

DEFINE_string(fst_weight_parentheses, "",
              "Characters enclosing the first weight of a printed composite "
              "weight (e.g. pair weight, tuple weight and derived classes) to "
              "ensure proper I/O of nested composite weights; "
              "must have size 0 (none) or 2 (open and close parenthesis)");

DEFINE_bool(fst_default_cache_gc, true, "Enable garbage collection of cache");

DEFINE_int64(fst_default_cache_gc_limit, 1<<20LL,
             "Cache byte size that triggers garbage collection");

DEFINE_bool(fst_align, false, "Write FST data aligned where appropriate");

DEFINE_string(save_relabel_ipairs, "",  "Save input relabel pairs to file");
DEFINE_string(save_relabel_opairs, "",  "Save output relabel pairs to file");

DEFINE_string(fst_read_mode, "read",
              "Default file reading mode for mappable files");

namespace fst {

// Register VectorFst, ConstFst and EditFst for common arcs types
REGISTER_FST(VectorFst, StdArc);
REGISTER_FST(VectorFst, LogArc);
REGISTER_FST(VectorFst, Log64Arc);
REGISTER_FST(ConstFst, StdArc);
REGISTER_FST(ConstFst, LogArc);
REGISTER_FST(ConstFst, Log64Arc);
REGISTER_FST(EditFst, StdArc);
REGISTER_FST(EditFst, LogArc);
REGISTER_FST(EditFst, Log64Arc);

// Register CompactFst for common arcs with the default (uint32) size type
static FstRegisterer<
  CompactFst<StdArc, StringCompactor<StdArc> > >
CompactFst_StdArc_StringCompactor_registerer;
static FstRegisterer<
  CompactFst<LogArc, StringCompactor<LogArc> > >
CompactFst_LogArc_StringCompactor_registerer;
static FstRegisterer<
  CompactFst<StdArc, WeightedStringCompactor<StdArc> > >
CompactFst_StdArc_WeightedStringCompactor_registerer;
static FstRegisterer<
  CompactFst<LogArc, WeightedStringCompactor<LogArc> > >
CompactFst_LogArc_WeightedStringCompactor_registerer;
static FstRegisterer<
  CompactFst<StdArc, AcceptorCompactor<StdArc> > >
CompactFst_StdArc_AcceptorCompactor_registerer;
static FstRegisterer<
  CompactFst<LogArc, AcceptorCompactor<LogArc> > >
CompactFst_LogArc_AcceptorCompactor_registerer;
static FstRegisterer<
  CompactFst<StdArc, UnweightedCompactor<StdArc> > >
CompactFst_StdArc_UnweightedCompactor_registerer;
static FstRegisterer<
  CompactFst<LogArc, UnweightedCompactor<LogArc> > >
CompactFst_LogArc_UnweightedCompactor_registerer;
static FstRegisterer<
  CompactFst<StdArc, UnweightedAcceptorCompactor<StdArc> > >
CompactFst_StdArc_UnweightedAcceptorCompactor_registerer;
static FstRegisterer<
  CompactFst<LogArc, UnweightedAcceptorCompactor<LogArc> > >
CompactFst_LogArc_UnweightedAcceptorCompactor_registerer;

// Fst type definitions for lookahead Fsts.
extern const char arc_lookahead_fst_type[] = "arc_lookahead";
extern const char ilabel_lookahead_fst_type[] = "ilabel_lookahead";
extern const char olabel_lookahead_fst_type[] = "olabel_lookahead";

// Identifies stream data as an FST (and its endianity)
static const int32 kFstMagicNumber = 2125659606;

// Check for Fst magic number in stream, to indicate
// caller function that the stream content is an Fst header;
bool IsFstHeader(istream &strm, const string &source) {
  int64 pos = strm.tellg();
  bool match = true;
  int32 magic_number = 0;
  ReadType(strm, &magic_number);
  if (magic_number != kFstMagicNumber
      ) {
    match = false;
  }
  strm.seekg(pos);
  return match;
}

// Check Fst magic number and read in Fst header.
// If rewind = true, reposition stream to before call (if possible).
bool FstHeader::Read(istream &strm, const string &source, bool rewind) {
  int64 pos = 0;
  if (rewind) pos = strm.tellg();
  int32 magic_number = 0;
  ReadType(strm, &magic_number);
  if (magic_number != kFstMagicNumber
      ) {
    LOG(ERROR) << "FstHeader::Read: Bad FST header: " << source;
    if (rewind) strm.seekg(pos);
    return false;
  }

  ReadType(strm, &fsttype_);
  ReadType(strm, &arctype_);
  ReadType(strm, &version_);
  ReadType(strm, &flags_);
  ReadType(strm, &properties_);
  ReadType(strm, &start_);
  ReadType(strm, &numstates_);
  ReadType(strm, &numarcs_);
  if (!strm) {
    LOG(ERROR) << "FstHeader::Read: read failed: " << source;
    return false;
  }
  if (rewind) strm.seekg(pos);
  return true;
}

// Write Fst magic number and Fst header.
bool FstHeader::Write(ostream &strm, const string &source) const {
  WriteType(strm, kFstMagicNumber);
  WriteType(strm, fsttype_);
  WriteType(strm, arctype_);
  WriteType(strm, version_);
  WriteType(strm, flags_);
  WriteType(strm, properties_);
  WriteType(strm, start_);
  WriteType(strm, numstates_);
  WriteType(strm, numarcs_);
  return true;
}

string FstHeader::DebugString() const {
  ostringstream ostrm;
  ostrm << "fsttype: \"" << fsttype_ << "\" arctype: \"" << arctype_
        << "\" version: \"" << version_ << "\" flags: \"" << flags_
        << "\" properties: \"" << properties_ << "\" start: \"" << start_
        << "\" numstates: \"" << numstates_ << "\" numarcs: \"" << numarcs_
        << "\"";
  return ostrm.str();
}

FstReadOptions::FstReadOptions(const string& src, const FstHeader *hdr,
                               const SymbolTable* isym, const SymbolTable* osym)
  : source(src), header(hdr), isymbols(isym), osymbols(osym),
    read_isymbols(true), read_osymbols(true) {
  mode = ReadMode(FLAGS_fst_read_mode);
}

FstReadOptions::FstReadOptions(const string& src, const SymbolTable* isym,
                               const SymbolTable* osym)
  : source(src), header(0), isymbols(isym), osymbols(osym),
    read_isymbols(true), read_osymbols(true) {
  mode = ReadMode(FLAGS_fst_read_mode);
}

FstReadOptions::FileReadMode FstReadOptions::ReadMode(const string &mode) {
  if (mode == "read") {
    return READ;
  }
  if (mode == "map") {
    return MAP;
  }
  LOG(ERROR) << "Unknown file read mode " << mode;
  return READ;
}

string FstReadOptions::DebugString() const {
  ostringstream ostrm;
  ostrm << "source: \"" << source << "\" mode: \""
        << (mode == READ ? "READ" : "MAP") << "\" read_isymbols: \""
        << (read_isymbols ? "true" : "false") << "\" read_osymbols: \""
        << (read_osymbols ? "true" : "false") << "\" header: \""
        << (header ? "set" : "null") << "\" isymbols: \""
        << (isymbols ? "set" : "null") << "\" osymbols: \""
        << (osymbols ? "set" : "null") << "\"";
  return ostrm.str();
}

}  // namespace fst
