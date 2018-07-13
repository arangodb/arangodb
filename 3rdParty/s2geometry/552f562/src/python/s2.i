%include "std_vector.i"
%include "std_string.i"

%template() std::vector<unsigned long long>;
%template() std::vector<S2CellId>;
%template() std::vector<S2Point>;
%template() std::vector<S2LatLng>;

%apply int {int32};
%apply unsigned long long {uint64};
%apply std::string {string};
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
