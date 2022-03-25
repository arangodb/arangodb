%include "std_vector.i"
%include "std_string.i"
%include "stdint.i"

%{
#include "absl/strings/string_view.h"
%}

%typemap(typecheck) absl::string_view = char *;

%typemap(in) absl::string_view {
  if ($input != Py_None) {
    Py_ssize_t len;
    if (PyUnicode_Check($input)) {
      const char *buf;
      buf = PyUnicode_AsUTF8AndSize($input, &len);
      if (buf == nullptr) {
        SWIG_fail;
      }
      $1 = absl::string_view(buf, len);
    } else {
      char *buf;
      if (PyBytes_AsStringAndSize($input, &buf, &len) == -1) {
        // Python has raised an error (likely TypeError or UnicodeEncodeError).
        SWIG_fail;
      }
      $1 = absl::string_view(buf, len);
    }
  }
}

%template() std::vector<unsigned long long>;
%template() std::vector<std::string>;
%template() std::vector<S2CellId>;
%template() std::vector<S2Point>;
%template() std::vector<S2LatLng>;

%apply int {int32};
%apply unsigned long long {uint64};
%apply std::vector<unsigned long long> const & {std::vector<uint64> const &};

// Standard Google convention is to ignore all functions and methods, and
// selectively add back those for which wrapping is both required and
// functional.
%define %ignoreall %ignore ""; %enddef
%define %unignore %rename("%s") %enddef
%define %unignoreall %rename("%s") ""; %enddef

%define ABSL_ATTRIBUTE_ALWAYS_INLINE %enddef
%define ABSL_DEPRECATED(msg)
%enddef

// SWIG <3.0 does not understand these C++11 keywords (unsure of exact version).
#if SWIG_VERSION < 0x030000
%define constexpr const %enddef
%define override %enddef
#endif

// Still not supported by SWIG 3.0.12.
%define final %enddef

%include "coder.i"
%include "s2_common.i"
