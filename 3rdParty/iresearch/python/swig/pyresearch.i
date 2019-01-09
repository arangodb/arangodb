%module pyresearch

%{
#define SWIG_FILE_WITH_INIT
#include "pyresearch.hpp"
%}

%include "stdint.i"
%include "std_pair.i"
%include "std_string.i"
%include "std_vector.i"
%include "exception.i"

%template(StringVector) std::vector<std::string>;
%template(ColumnValue) std::pair<bool, irs::bytes_ref>;

%typemap(in) (irs::string_ref) {
  if (PyBytes_Check($input)) {
    $1 = irs::string_ref(
      PyBytes_AsString($input),
      PyBytes_Size($input)
    );
  } else if (PyUnicode_Check($input)) {
    Py_ssize_t size{};
    const char* str = PyUnicode_AsUTF8AndSize($input, &size);
    $1 = irs::string_ref(str, size);
  } else if (PyString_Check($input)) {
    $1 = irs::string_ref(
      PyString_AsString($input),
      PyString_Size($input)
    );
  } else {
    PyErr_SetString(PyExc_ValueError, "Expected a string or binary");
    SWIG_fail;
  }
}

%typemap(typecheck, precedence=0) irs::string_ref {
  $1 = PyBytes_Check($input) || PyUnicode_Check($input) || PyString_Check($input) ? 1 : 0;
}

%typemap(out) irs::bytes_ref {
  $result = PyBytes_FromStringAndSize(
    reinterpret_cast<const char*>($1.c_str()),
    $1.size()
  );
}

%typemap(out) irs::string_ref {
  $result = PyBytes_FromStringAndSize($1.c_str(), $1.size());
}

%typemap(out) std::pair<bool, irs::bytes_ref> {
  // create tuple of size 2
  $result = PyTuple_New(2);

  // set first value
  PyObject* first_value = $1.first
    ? Py_True
    : Py_False;
  Py_INCREF(first_value);
  PyTuple_SetItem($result, 0, first_value);

  // set second value
  const auto& second = $1.second;

  PyObject* second_value = second.null()
    ? Py_None
    : PyBytes_FromStringAndSize(
        reinterpret_cast<const char*>($1.second.c_str()),
        $1.second.size()
      );
  Py_INCREF(second_value);
  PyTuple_SetItem($result, 1, second_value);
}

%exception index_reader::segment %{
  try {
    $action
  } catch (const std::out_of_range& e) {
    SWIG_exception(SWIG_IndexError, const_cast<char*>(e.what()));
  } catch (const std::exception& e) {
    SWIG_exception(SWIG_SystemError, const_cast<char*>(e.what()));
  }
%}

%exception segment_reader::field %{
  $action
  if (!static_cast<field_reader&>(result)) {
    Py_RETURN_NONE;
  }
%}

%exception segment_reader::column %{
  $action
  if (!static_cast<column_reader&>(result)) {
    Py_RETURN_NONE;
  }
%}

%exception index_reader::open %{
  try {
    $action
  } catch (const std::exception& e) {
    SWIG_exception(SWIG_SystemError, const_cast<char*>(e.what()));
  }
%}

%include "../pyresearch.hpp"

%extend field_iterator {
%insert("python") %{
  def __iter__(self) :
    return self;

  def __next__(self) :
    if not self.next() :
      raise StopIteration();

    return self.value();
%}
}

%extend field_reader {
%insert("python") %{
  def __iter__(self) :
    return self.iterator();
%}
}

%extend term_iterator {
%insert("python") %{
  def __iter__(self) :
    return self;

  def __next__(self) :
    if not self.next() :
      raise StopIteration();

    return self.value();
%}
}

%extend column_iterator {
%insert("python") %{
  def __iter__(self) :
    return self;

  def __next__(self) :
    if not self.next() :
      raise StopIteration();

    return self.value();
%}
}

%extend column_reader {
%insert("python") %{
  def __iter__(self) :
    return self.iterator();
%}
}

%extend doc_iterator {
%insert("python") %{
  def __iter__(self) :
    return self;

  def __next__(self) :
    if not self.next() :
      raise StopIteration();

    return self.value();
%}
}

%extend segment_iterator {
%insert("python") %{
  def __iter__(self) :
    return self;

  def __next__(self) :
    value = self.next();

    if not value.valid():
      raise StopIteration();

    return value;
%}
}

%extend index_reader {
%insert("python") %{
  def __getitem__(self, key) :
    return self.segment(key);

  def __iter__(self) :
    return self.iterator();
%}
}
