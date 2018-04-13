// Copyright 2018 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS-IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//



#ifndef S2_STRINGS_OSTRINGSTREAM_H_
#define S2_STRINGS_OSTRINGSTREAM_H_

#include <ostream>
#include <streambuf>
#include <string>

#include "s2/third_party/absl/base/port.h"

namespace strings {

// The same as std::ostringstream but appends to a user-specified string,
// and is faster. It is ~70% faster to create, ~50% faster to write to, and
// completely free to extract the result string.
//
//   string s;
//   OStringStream strm(&s);
//   strm << 42 << ' ' << 3.14;  // appends to `s`
//
// The stream object doesn't have to be named. Starting from C++11 operator<<
// works with rvalues of std::ostream.
//
//   string s;
//   OStringStream(&s) << 42 << ' ' << 3.14;  // appends to `s`
//
// OStringStream is faster to create than std::ostringstream but it's still
// relatively slow. Avoid creating multiple streams where a single stream will
// do.
//
// Creates unnecessary instances of OStringStream: slow.
//
//   string s;
//   OStringStream(&s) << 42;
//   OStringStream(&s) << ' ';
//   OStringStream(&s) << 3.14;
//
// Creates a single instance of OStringStream and reuses it: fast.
//
//   string s;
//   OStringStream strm(&s);
//   strm << 42;
//   strm << ' ';
//   strm << 3.14;
//
// Note: flush() has no effect. No reason to call it.
class OStringStream : private std::basic_streambuf<char>, public std::ostream {
 public:
  // Export the same types as ostringstream does; for use info, see
  // http://en.cppreference.com/w/cpp/io/basic_ostringstream
  typedef string::allocator_type allocator_type;

  // These types are defined in both basic_streambuf and ostream, and although
  // they are identical, they cause compiler ambiguities, so we define them to
  // avoid that.
  using std::ostream::char_type;
  using std::ostream::int_type;
  using std::ostream::off_type;
  using std::ostream::pos_type;
  using std::ostream::traits_type;

  // The argument can be null, in which case you'll need to call str(p) with a
  // non-null argument before you can write to the stream.
  //
  // The destructor of OStringStream doesn't use the string. It's OK to destroy
  // the string before the stream.
  explicit OStringStream(string* s) : std::ostream(this), s_(s) {}

  string* str() { return s_; }
  const string* str() const { return s_; }
  void str(string* s) { s_ = s; }

  // These functions are defined in both basic_streambuf and ostream, but it's
  // ostream definition that affects the strings produced.
  using std::ostream::getloc;
  using std::ostream::imbue;

 private:
  using Buf = std::basic_streambuf<char>;

  Buf::int_type overflow(int c) override;
  std::streamsize xsputn(const char* s, std::streamsize n) override;

  string* s_;
};

}  // namespace strings

#endif  // S2_STRINGS_OSTRINGSTREAM_H_
