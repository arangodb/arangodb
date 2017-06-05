// util.cc

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
// FST utility definitions.

#include <cctype>
#include <string>
#include <fst/util.h>
#include <fst/mapped-file.h>

// Utility flag definitions

DEFINE_bool(fst_error_fatal, true,
            "FST errors are fatal; o.w. return objects flagged as bad: "
            " e.g., FSTs - kError prop. true, FST weights - not  a Member()");

namespace fst {

void SplitToVector(char* full, const char* delim, vector<char*>* vec,
                   bool omit_empty_strings) {
  char *p = full;
  while (p) {
    if ((p = strpbrk(full, delim)))
      p[0] = '\0';
    if (!omit_empty_strings || full[0] != '\0')
      vec->push_back(full);
    if (p)
      full = p + 1;
  }
}

int64 StrToInt64(const string &s, const string &src, size_t nline,
                 bool allow_negative, bool *error) {
  int64 n;
  const char *cs = s.c_str();
  char *p;
  if (error) *error = false;
  n = strtoll(cs, &p, 10);
  if (p < cs + s.size() || (!allow_negative && n < 0)) {
    FSTERROR() << "StrToInt64: Bad integer = " << s
               << "\", source = " << src << ", line = " << nline;
    if (error) *error = true;
    return 0;
  }
  return n;
}

void Int64ToStr(int64 n, string *s) {
  ostringstream nstr;
  nstr << n;
  s->append(nstr.str().data(), nstr.str().size());
}

void ConvertToLegalCSymbol(string *s) {
  for (string::iterator it = s->begin(); it != s->end(); ++it)
    if (!isalnum(*it)) *it = '_';
}

// Skips over input characters to align to 'align' bytes. Returns
// false if can't align.
bool AlignInput(istream &strm) {
  char c;
  for (int i = 0; i < MappedFile::kArchAlignment; ++i) {
    int64 pos = strm.tellg();
    if (pos < 0) {
      LOG(ERROR) << "AlignInput: can't determine stream position";
      return false;
    }
    if (pos % MappedFile::kArchAlignment == 0) break;
    strm.read(&c, 1);
  }
  return true;
}

// Write null output characters to align to 'align' bytes. Returns
// false if can't align.
bool AlignOutput(ostream &strm) {
  for (int i = 0; i < MappedFile::kArchAlignment; ++i) {
    int64 pos = strm.tellp();
    if (pos < 0) {
      LOG(ERROR) << "AlignOutput: can't determine stream position";
      return false;
    }
    if (pos % MappedFile::kArchAlignment == 0) break;
    strm.write("", 1);
  }
  return true;
}


}  // namespace fst
