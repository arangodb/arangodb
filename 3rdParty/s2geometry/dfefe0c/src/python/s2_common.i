// Copyright 2006 Google Inc. All Rights Reserved.

// These SWIG definitions are shared between the internal Google and external
// open source releases of s2.

%{
#include <sstream>

#include "s2/s2cell_id.h"
#include "s2/s2region.h"
#include "s2/s2cap.h"
#include "s2/s2edge_crossings.h"
#include "s2/s2latlng.h"
#include "s2/s2latlng_rect.h"
#include "s2/s2loop.h"
#include "s2/s2measures.h"
#include "s2/s2pointutil.h"
#include "s2/s2polygon.h"
#include "s2/s2polyline.h"
#include "s2/s2region_coverer.h"
#include "s2/s2cell.h"
#include "s2/s2cell_union.h"
%}

%inline %{
  static PyObject *FromS2CellId(const S2CellId &cell_id) {
    return SWIG_NewPointerObj(new S2CellId(cell_id), SWIGTYPE_p_S2CellId,
                              SWIG_POINTER_OWN);
  }
%}

%apply std::vector<S2CellId> *OUTPUT {std::vector<S2CellId> *covering};
%apply std::vector<S2CellId> *OUTPUT {std::vector<S2CellId> *interior};
%apply std::vector<S2CellId> *OUTPUT {std::vector<S2CellId> *output};

%typemap(in, numinputs=0) S2CellId *OUTPUT_ARRAY_4(S2CellId temp[4]) {
  $1 = temp;
}

%typemap(argout) S2CellId *OUTPUT_ARRAY_4 {
  $result = PyList_New(4);
  if ($result == nullptr) return nullptr;

  for (int i = 0; i < 4; i++) {
    PyObject *const o = FromS2CellId($1[i]);
    if (!o) {
      Py_DECREF($result);
      return nullptr;
    }
    PyList_SET_ITEM($result, i, o);
  }
}

%apply S2CellId *OUTPUT_ARRAY_4 {S2CellId neighbors[4]};

// This overload shadows the one the takes vector<uint64>&, and it
// does not work anyway.
%ignore S2CellUnion::Init(std::vector<S2CellId> const& cell_ids);

// The SWIG code which picks between overloaded methods doesn't work
// when given a list parameter.  SWIG_Python_ConvertPtrAndOwn calls
// SWIG_Python_GetSwigThis, doesn't find the 'this' attribute and gives up.
// To avoid this problem rename the Polyline::Init methods so they aren't
// overloaded.
%rename(InitFromS2LatLngs) S2Polyline::Init(std::vector<S2LatLng> const& vertices);
%rename(InitFromS2Points) S2Polyline::Init(std::vector<S2Point> const& vertices);

%apply int *OUTPUT {int *next_vertex};
%apply int *OUTPUT {int *psi};
%apply int *OUTPUT {int *pti};
%apply int *OUTPUT {int *pi};
%apply int *OUTPUT {int *pj};
%apply int *OUTPUT {int *orientation};
%apply SWIGTYPE *DISOWN {S2Loop *loop_disown};

%typemap(in) std::vector<S2Loop *> * (std::vector<S2Loop *> loops){
  PyObject *element(nullptr);
  PyObject *iterator(PyObject_GetIter($input));
  if (!iterator) {
    SWIG_fail;
  }
  int i(0);
  while ((element = PyIter_Next(iterator))) {
    i++;
    S2Loop *loop(nullptr);
    int res(SWIG_ConvertPtr(element, (void **)&loop, $descriptor(S2Loop *), 0));
    if (SWIG_IsOK(res)) {
      loops.push_back(loop->Clone());
    } else {
      SWIG_Python_TypeError(SWIG_TypePrettyName($descriptor(S2Loop *)), element);
      SWIG_Python_ArgFail(i);
      Py_DECREF(element);
      Py_DECREF(iterator);
      SWIG_fail;
    }
    Py_DECREF(element);
  }
  Py_DECREF(iterator);
  $1 = &loops;
}

%typemap(in, numinputs=0)
std::vector<S2Polyline *> *out(std::vector<S2Polyline *> temp) {
  $1 = &temp;
}

%typemap(argout) std::vector<S2Polyline *> *out {
  $result = PyList_New($1->size());
  if ($result == nullptr) return nullptr;

  for (int i = 0; i < $1->size(); i++) {
    PyObject *const o =
      SWIG_NewPointerObj((*$1)[i], $descriptor(S2Polyline *), SWIG_POINTER_OWN);
    if (!o) {
      Py_DECREF($result);
      return nullptr;
    }
    PyList_SET_ITEM($result, i, o);
  }
}

// We provide our own definition of S2Point, because the real one is too
// difficult to wrap correctly.
class S2Point {
 public:
  double Norm();
  S2Point Normalize();
  ~S2Point();
};

// The extensions below exist because of the difficulty swigging S2Point.

// This alternate method of S2Loop::vertex() returns a S2LatLng instead.
%extend S2Loop {
 public:
  S2LatLng GetS2LatLngVertex(int i) {
    return S2LatLng($self->vertex(i));
  }
};

// This alternate method of S2Cell::GetVertex() returns a S2LatLng instead.
%extend S2Cell {
 public:
  S2LatLng GetS2LatLngVertex(int k) {
    return S2LatLng($self->GetVertex(k));
  }
};

// This alternate method of S2Cell::GetEdge() returns a S2LatLng instead.
%extend S2Cell {
 public:
  S2LatLng GetS2LatLngEdge(int k) {
    return S2LatLng($self->GetEdge(k));
  }
};

// Add raw pointer versions of these functions because SWIG doesn't
// understand unique_ptr and when std::move() must be used.
// TODO(user): Make swig understand unique_ptr and vector<unique_ptr>.
%extend S2Polygon {
 public:
  // Takes ownership of the loop.  The _disown suffix is used to tell SWIG
  // that S2Polygon takes ownership of the loop.
  explicit S2Polygon(S2Loop* loop_disown) {
    // SWIG recognizes this as a constructor, but implements this
    // as a free function, so write it that way.
    return new S2Polygon(std::unique_ptr<S2Loop>(loop_disown));
  }

  void InitNested(std::vector<S2Loop*>* loops) {
    std::vector<std::unique_ptr<S2Loop>> unique_loops(loops->size());
    for (int i = 0; i < loops->size(); ++i) {
      unique_loops[i].reset((*loops)[i]);
    }
    loops->clear();
    $self->InitNested(std::move(unique_loops));
  }

  void IntersectWithPolyline(S2Polyline const* in,
                             std::vector<S2Polyline*>* out) const {
    std::vector<std::unique_ptr<S2Polyline>> polylines =
        $self->IntersectWithPolyline(*in);
    S2_DCHECK(out->empty());
    out->reserve(polylines.size());
    for (auto& polyline : polylines) {
      out->push_back(polyline.release());
    }
  }
}

// Expose Options functions on S2RegionCoverer until we figure out
// nested classes in SWIG.
%extend S2RegionCoverer {
  int max_cells() const { return $self->options().max_cells(); }
  void set_max_cells(int max_cells) {
    $self->mutable_options()->set_max_cells(max_cells);
  }

  int min_level() const { return $self->options().min_level(); }
  void set_min_level(int min_level) {
    $self->mutable_options()->set_min_level(min_level);
  }

  int max_level() const { return $self->options().max_level(); }
  void set_max_level(int max_level) {
    $self->mutable_options()->set_max_level(max_level);
  }

  void set_fixed_level(int fixed_level) {
    $self->mutable_options()->set_fixed_level(fixed_level);
  }

  int level_mod() const { return $self->options().level_mod(); }
  void set_level_mod(int level_mod) {
    $self->mutable_options()->set_level_mod(level_mod);
  }
}

%ignoreall

%unignore R1Interval;
%ignore R1Interval::operator[];
%unignore R1Interval::GetLength;
%unignore S1Angle;
%unignore S1Angle::S1Angle;
%unignore S1Angle::~S1Angle;
%unignore S1Angle::Degrees;
%unignore S1Angle::E5;
%unignore S1Angle::E6;
%unignore S1Angle::E7;
%unignore S1Angle::Normalize;
%unignore S1Angle::Normalized;
%unignore S1Angle::Radians;
%unignore S1Angle::UnsignedE6;
%unignore S1Angle::abs;
%unignore S1Angle::degrees;
%unignore S1Angle::e6;
%unignore S1Angle::e7;
%unignore S1Angle::radians;
%unignore S1ChordAngle;
%unignore S1ChordAngle::ToAngle;
%unignore S1Interval;
%ignore S1Interval::operator[];
%unignore S1Interval::GetLength;
%unignore S2;
%unignore S2::CrossingSign;
%unignore S2::GetIntersection;
%unignore S2::Rotate;
%unignore S2::TurnAngle;
%unignore S2Cap;
%unignore S2Cap::S2Cap;
%unignore S2Cap::~S2Cap;
%unignore S2Cap::AddPoint;
%unignore S2Cap::ApproxEquals;
%unignore S2Cap::Clone;
%unignore S2Cap::Contains;
%unignore S2Cap::Decode;
%unignore S2Cap::Empty;
%unignore S2Cap::Encode;
%unignore S2Cap::Expanded;
%unignore S2Cap::FromPoint;
%unignore S2Cap::Full;
%unignore S2Cap::GetCapBound() const;
%unignore S2Cap::GetCentroid;
%unignore S2Cap::GetRectBound;
%unignore S2Cap::Intersects;
%unignore S2Cap::MayIntersect(const S2Cell&) const;
%unignore S2Cap::Union;
%unignore S2Cap::center;
%unignore S2Cap::height;
%unignore S2Cap::is_empty;
%unignore S2Cap::is_valid;
%unignore S2Cell;
%unignore S2Cell::S2Cell;
%unignore S2Cell::~S2Cell;
%unignore S2Cell::ApproxArea;
%unignore S2Cell::Clone;
%unignore S2Cell::Contains;
%unignore S2Cell::Decode;
%unignore S2Cell::Encode;
%unignore S2Cell::ExactArea;
%unignore S2Cell::GetBoundaryDistance;
%unignore S2Cell::GetCapBound() const;
%unignore S2Cell::GetCenter;
%unignore S2Cell::GetDistance;
%unignore S2Cell::GetRectBound;
%unignore S2Cell::GetS2LatLngEdge;
%unignore S2Cell::GetS2LatLngVertex;
%unignore S2Cell::GetVertex;
%unignore S2Cell::MayIntersect(const S2Cell&) const;
%unignore S2Cell::face;
%unignore S2Cell::id;
%unignore S2Cell::level;
%unignore S2CellId;
%unignore S2CellId::S2CellId;
%unignore S2CellId::~S2CellId;
%unignore S2CellId::Begin;
%unignore S2CellId::End;
%unignore S2CellId::FromFaceIJ(int, int, int);
%unignore S2CellId::FromFacePosLevel(int, uint64, int);
%unignore S2CellId::FromLatLng;
%unignore S2CellId::FromPoint;
%unignore S2CellId::FromToken;
%unignore S2CellId::GetCenterSiTi(int*, int*) const;
%unignore S2CellId::GetEdgeNeighbors;
%unignore S2CellId::ToFaceIJOrientation(int*, int*, int*) const;
%unignore S2CellId::ToLatLng;
%unignore S2CellId::ToPoint;
%unignore S2CellId::ToString;
%unignore S2CellId::ToToken;
%unignore S2CellId::child;
%unignore S2CellId::child_begin;
%unignore S2CellId::child_end;
%unignore S2CellId::contains;
%unignore S2CellId::face;
%unignore S2CellId::id;
%unignore S2CellId::intersects;
%unignore S2CellId::is_face;
%unignore S2CellId::is_valid;
%unignore S2CellId::kMaxLevel;
%unignore S2CellId::level;
%unignore S2CellId::next;
%unignore S2CellId::parent;
%unignore S2CellId::pos;
%unignore S2CellId::prev;
%unignore S2CellId::range_max;
%unignore S2CellId::range_min;
%unignore S2CellUnion;
%ignore S2CellUnion::operator[];  // Silence the SWIG warning.
%unignore S2CellUnion::S2CellUnion;
%unignore S2CellUnion::~S2CellUnion;
%unignore S2CellUnion::ApproxArea;
%unignore S2CellUnion::Clone;
%unignore S2CellUnion::Contains;
%unignore S2CellUnion::Decode;
%unignore S2CellUnion::Denormalize(int, int, std::vector<S2CellId>*) const;
%unignore S2CellUnion::Encode;
%unignore S2CellUnion::ExactArea;
%unignore S2CellUnion::GetCapBound() const;
%unignore S2CellUnion::GetDifference;
%unignore S2CellUnion::GetRectBound;
%unignore S2CellUnion::Init(std::vector<uint64> const &);
%unignore S2CellUnion::Intersects;
%unignore S2CellUnion::MayIntersect(const S2Cell&) const;
%unignore S2CellUnion::Normalize;
%unignore S2CellUnion::cell_id;
%unignore S2CellUnion::cell_ids;
%unignore S2CellUnion::num_cells;
%unignore S2LatLng;
%unignore S2LatLng::S2LatLng;
%unignore S2LatLng::~S2LatLng;
%unignore S2LatLng::ApproxEquals;
%unignore S2LatLng::FromDegrees;
%unignore S2LatLng::FromE6;
%unignore S2LatLng::FromE7;
%unignore S2LatLng::FromRadians;
%unignore S2LatLng::FromUnsignedE6;
%unignore S2LatLng::FromUnsignedE7;
%unignore S2LatLng::GetDistance;
%unignore S2LatLng::Normalized;
%unignore S2LatLng::ToPoint;
%unignore S2LatLng::ToStringInDegrees;
%unignore S2LatLng::coords;
%unignore S2LatLng::is_valid;
%unignore S2LatLng::lat;
%unignore S2LatLng::lng;
%unignore S2LatLngRect;
%unignore S2LatLngRect::S2LatLngRect;
%unignore S2LatLngRect::~S2LatLngRect;
%unignore S2LatLngRect::AddPoint;
%unignore S2LatLngRect::ApproxEquals;
%unignore S2LatLngRect::Area;
%unignore S2LatLngRect::Clone;
%unignore S2LatLngRect::Contains;
%unignore S2LatLngRect::Decode;
%unignore S2LatLngRect::Empty;
%unignore S2LatLngRect::Encode;
%unignore S2LatLngRect::ExpandedByDistance;
%unignore S2LatLngRect::FromCenterSize;
%unignore S2LatLngRect::FromPoint;
%unignore S2LatLngRect::FromPointPair;
%unignore S2LatLngRect::Full;
%unignore S2LatLngRect::GetCapBound() const;
%unignore S2LatLngRect::GetCenter;
%unignore S2LatLngRect::GetCentroid;
%unignore S2LatLngRect::GetDistance;
%unignore S2LatLngRect::GetRectBound;
%unignore S2LatLngRect::GetSize;
%unignore S2LatLngRect::GetVertex;
%unignore S2LatLngRect::Intersection;
%unignore S2LatLngRect::Intersects;
%unignore S2LatLngRect::MayIntersect(const S2Cell&) const;
%unignore S2LatLngRect::Union;
%unignore S2LatLngRect::hi;
%unignore S2LatLngRect::is_empty;
%unignore S2LatLngRect::is_point;
%unignore S2LatLngRect::is_valid;
%unignore S2LatLngRect::lat;
%unignore S2LatLngRect::lat_hi;
%unignore S2LatLngRect::lat_lo;
%unignore S2LatLngRect::lng;
%unignore S2LatLngRect::lng_hi;
%unignore S2LatLngRect::lng_lo;
%unignore S2LatLngRect::lo;
%unignore S2Loop;
%unignore S2Loop::S2Loop;
%unignore S2Loop::~S2Loop;
%unignore S2Loop::Clone;
%unignore S2Loop::Contains;
%unignore S2Loop::Decode;
%unignore S2Loop::Encode;
%unignore S2Loop::Equals;
%unignore S2Loop::GetCapBound() const;
%unignore S2Loop::GetCentroid;
%unignore S2Loop::GetDistance;
%unignore S2Loop::GetRectBound;
%unignore S2Loop::GetS2LatLngVertex;
%unignore S2Loop::Init;
%unignore S2Loop::Intersects;
%unignore S2Loop::IsValid;
%unignore S2Loop::MayIntersect(const S2Cell&) const;
%unignore S2Loop::Normalize;
%unignore S2Loop::Project;
%unignore S2Loop::depth;
%unignore S2Loop::is_empty;
%unignore S2Loop::is_hole;
%unignore S2Loop::num_vertices;
%unignore S2Loop::sign;
%unignore S2Loop::vertex;
%unignore S2Polygon;
%unignore S2Polygon::S2Polygon;
%unignore S2Polygon::~S2Polygon;
%unignore S2Polygon::Clone;
%unignore S2Polygon::Contains;
%unignore S2Polygon::Copy;
%unignore S2Polygon::Decode;
%unignore S2Polygon::Encode;
%unignore S2Polygon::Equals;
%unignore S2Polygon::GetArea;
%unignore S2Polygon::GetCapBound() const;
%unignore S2Polygon::GetCentroid;
%unignore S2Polygon::GetDistance;
%unignore S2Polygon::GetRectBound;
%unignore S2Polygon::Init;
%unignore S2Polygon::InitNested;
%unignore S2Polygon::Intersects;
%unignore S2Polygon::IntersectWithPolyline;
%unignore S2Polygon::IsValid;
%unignore S2Polygon::MayIntersect(const S2Cell&) const;
%unignore S2Polygon::Project;
%unignore S2Polygon::is_empty;
%unignore S2Polygon::loop;
%unignore S2Polygon::num_loops;
%unignore S2Polygon::num_vertices;
%unignore S2Polyline;
%unignore S2Polyline::S2Polyline();
%unignore S2Polyline::S2Polyline(std::vector<S2LatLng> const &);
%ignore S2Polyline::S2Polyline(std::vector<S2Point> const &);
%ignore S2Polyline::S2Polyline(std::vector<S2Point> const &, S2Debug);
%unignore S2Polyline::~S2Polyline;
%unignore S2Polyline::ApproxEquals;
%unignore S2Polyline::Clone;
%unignore S2Polyline::Contains;
%unignore S2Polyline::Decode;
%unignore S2Polyline::Encode;
%unignore S2Polyline::GetCapBound() const;
%unignore S2Polyline::GetCentroid;
%unignore S2Polyline::GetLength;
%unignore S2Polyline::GetRectBound;
%unignore S2Polyline::GetSuffix;
%unignore S2Polyline::InitFromS2LatLngs;
%unignore S2Polyline::InitFromS2Points;
%unignore S2Polyline::Interpolate;
%unignore S2Polyline::Intersects;
%unignore S2Polyline::IsOnRight;
%unignore S2Polyline::IsValid;
%unignore S2Polyline::MayIntersect(const S2Cell&) const;
%unignore S2Polyline::Project;
%unignore S2Polyline::Reverse;
%unignore S2Polyline::UnInterpolate;
%unignore S2Polyline::num_vertices;
%unignore S2Polyline::vertex;
%unignore S2RegionCoverer;
%unignore S2RegionCoverer::S2RegionCoverer;
%unignore S2RegionCoverer::~S2RegionCoverer;
%unignore S2RegionCoverer::GetCovering(const S2Region&, std::vector<S2CellId>*);
%unignore S2RegionCoverer::GetInteriorCovering(const S2Region&,
                                               std::vector<S2CellId>*);

%include "s2/r1interval.h"
%include "s2/s1angle.h"
%include "s2/s1chord_angle.h"
%include "s2/s1interval.h"
%include "s2/s2cell_id.h"
%include "s2/s2edge_crossings.h"
%include "s2/s2region.h"
%include "s2/s2cap.h"
%include "s2/s2latlng.h"
%include "s2/s2latlng_rect.h"
%include "s2/s2loop.h"
%include "s2/s2measures.h"
%include "s2/s2pointutil.h"
%include "s2/s2polygon.h"
%include "s2/s2polyline.h"
%include "s2/s2region_coverer.h"
%include "s2/s2cell.h"
%include "s2/s2cell_union.h"

%unignoreall

%define USE_STREAM_INSERTOR_FOR_STR(type)
  %extend type {
    string __str__() {
      std::ostringstream output;
      output << *self << std::ends;
      return output.str();
    }
  }
%enddef

%define USE_EQUALS_FOR_EQ_AND_NE(type)
  %extend type {
    bool __eq__(const type& other) {
      return *self == other;
    }

    bool __ne__(const type& other) {
      return *self != other;
    }
  }
%enddef

%define USE_COMPARISON_FOR_LT_AND_GT(type)
  %extend type {
    bool __lt__(const type& other) {
      return *self < other;
    }

    bool __gt__(const type& other) {
      return *self > other;
    }
  }
%enddef

%define USE_HASH_FOR_TYPE(type, hash_type)
  %extend type {
    size_t __hash__() {
      return hash_type()(*self);
    }
  }
%enddef

USE_STREAM_INSERTOR_FOR_STR(S1Angle)
USE_STREAM_INSERTOR_FOR_STR(S1Interval)
USE_STREAM_INSERTOR_FOR_STR(S2CellId)
USE_STREAM_INSERTOR_FOR_STR(S2Cap)
USE_STREAM_INSERTOR_FOR_STR(S2LatLng)
USE_STREAM_INSERTOR_FOR_STR(S2LatLngRect)

USE_EQUALS_FOR_EQ_AND_NE(S2CellId)
USE_COMPARISON_FOR_LT_AND_GT(S2CellId)
USE_HASH_FOR_TYPE(S2CellId, S2CellIdHash)

USE_EQUALS_FOR_EQ_AND_NE(S1Angle)
USE_COMPARISON_FOR_LT_AND_GT(S1Angle)
