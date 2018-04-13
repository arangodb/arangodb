//
// Exposes Encoder and Decoder a subset of functionality plus the
// ability to set and get internal buffer. This allows passing them
// into other SWIG'd methods, such as those in the S2 SWIG library.
// For example:
//
// class S2Polygon {
//   ...
//   void Encode(Encoder* const encoder) const override;
//
//   bool Decode(Decoder* const decoder) override;
// };
//
// Usage:
//   # data is a bytes object.
//   dec = pywrapcoder.Decoder(data)
//   polygon = s2.S2Polygon()
//   polygon.Decode(dec)
//
//   enc = pywrapcoder.Encoder()
//   polygon.Encode(enc)
//   data = enc.buffer()
//

%{
#include "s2/util/coding/coder.h"
%}

// For Decoder::reset to accept a bytes or bytearray object.
%typemap(in) (const void* buf, size_t maxn) {
  if (PyBytes_Check($input)) {
    $1 = (void *) PyBytes_AsString($input);
    $2 = PyBytes_Size($input);
  } else if (PyByteArray_Check($input)) {
    $1 = (void *) PyByteArray_AsString($input);
    $2 = PyByteArray_Size($input);
  } else {
    PyErr_Format(PyExc_TypeError,
                 "bytes or bytearray needed, %s found",
                 $input->ob_type->tp_name);
    return nullptr;
  }
};
// For Encoder::reset to accept a bytearray object.
%typemap(in) (void* buf, size_t maxn) {
  if (PyByteArray_Check($input)) {
    $1 = (void *) PyByteArray_AsString($input);
    $2 = PyByteArray_Size($input);
  } else {
    PyErr_Format(PyExc_TypeError,
                 "bytearray needed, %s found", $input->ob_type->tp_name);
    return nullptr;
  }
};

// Keep a reference to the object so that outside users don't
// have to keep one, or else it could be released.
// The auto-generated code passes *args to each of the methods, but the use
// cases we support is only when a single arg is passed in.
%pythonprepend Decoder::Decoder %{
  if len(args) == 1:
    self._data_keepalive = args[0]
%}

%pythonprepend Decoder::reset %{
  self._data_keepalive = args[0]
%}

%extend Decoder {
  // Allows direct construction with a bytes or bytearray objects.
  Decoder(PyObject *obj) {
    if (PyBytes_Check(obj)) {
      return new Decoder(PyBytes_AsString(obj), PyBytes_Size(obj));
    }
    if (PyByteArray_Check(obj)) {
      return new Decoder(PyByteArray_AsString(obj), PyByteArray_Size(obj));
    }
    PyErr_Format(PyExc_TypeError,
                 "bytes or bytearray needed, %s found", obj->ob_type->tp_name);
    return nullptr;
  }
}

%extend Encoder {
  // Returns internal buffer as a bytearray.
  PyObject* buffer() {
    return PyByteArray_FromStringAndSize($self->base(), $self->length());
  }
}

%ignoreall

%unignore Decoder;
%unignore Decoder::Decoder();
%unignore Decoder::Decoder(const void*, size_t);
%unignore Decoder::~Decoder;
%unignore Decoder::avail() const;
%unignore Decoder::get8();
%unignore Decoder::get16();
%unignore Decoder::get32();
%unignore Decoder::get64();
%unignore Decoder::getfloat();
%unignore Decoder::getdouble();
%unignore Decoder::pos() const;
%unignore Decoder::reset(const void*, size_t);
%unignore Encoder;
%unignore Encoder::Encoder();
%unignore Encoder::Encoder(void*, size_t);
%unignore Encoder::~Encoder;
%unignore Encoder::Ensure(size_t);
%unignore Encoder::avail() const;
%unignore Encoder::buffer();
%unignore Encoder::clear();
%unignore Encoder::length() const;
%unignore Encoder::put8(unsigned char);
%unignore Encoder::put16(uint16);
%unignore Encoder::put32(uint32);
%unignore Encoder::put64(uint64);
%unignore Encoder::putdouble(double);
%unignore Encoder::putfloat(float);
%unignore Encoder::reset(void *, size_t);

%include "s2/util/coding/coder.h"

%unignoreall
