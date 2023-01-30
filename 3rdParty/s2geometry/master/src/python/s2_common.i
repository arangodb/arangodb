// Copyright 2006 Google Inc. All Rights Reserved.

// These SWIG definitions are shared between the internal Google and external
// open source releases of s2.

%{
#include <sstream>
#include <string>

#include "s2/s2boolean_operation.h"
#include "s2/s2buffer_operation.h"
#include "s2/s2builder.h"
#include "s2/s2builder_layer.h"
#include "s2/s2builderutil_s2polygon_layer.h"
#include "s2/s2cell_id.h"
#include "s2/s2region.h"
#include "s2/s2cap.h"
#include "s2/s2edge_crossings.h"
#include "s2/s2edge_distances.h"
#include "s2/s2earth.h"
#include "s2/s2latlng.h"
#include "s2/s2latlng_rect.h"
#include "s2/s2loop.h"
#include "s2/s2measures.h"
#include "s2/s2pointutil.h"
#include "s2/s2polygon.h"
#include "s2/s2polyline.h"
#include "s2/s2predicates.h"
#include "s2/s2region_coverer.h"
#include "s2/s2region_term_indexer.h"
#include "s2/s2cell.h"
#include "s2/s2cell_union.h"
#include "s2/s2shape_index.h"
#include "s2/mutable_s2shape_index.h"

// Wrapper for S2BufferOperation::Options to work around the inability
// to handle nested classes in SWIG.
class S2BufferOperationOptions {
public:
  S2BufferOperation::Options opts;

  void set_buffer_radius(S1Angle buffer_radius) {
    opts.set_buffer_radius(buffer_radius);
  }

  void set_error_fraction(double error_fraction) {
    opts.set_error_fraction(error_fraction);
  }
};

// Wrapper for S2PolygonLayer::Options to work around the inability to
// handle nested classes in SWIG.
class S2PolygonLayerOptions {
public:
  s2builderutil::S2PolygonLayer::Options opts;

  void set_edge_type(S2Builder::EdgeType edge_type) {
    opts.set_edge_type(edge_type);
  }
};

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

%typemap(in) absl::string_view {
  if (PyUnicode_Check($input)) {
    $1 = absl::string_view(PyUnicode_AsUTF8($input));
  } else {
    SWIG_exception(SWIG_TypeError, "string expected");
  }
}
%typemap(typecheck) absl::string_view = char *;


%typemap(in, numinputs=0) S2CellId *OUTPUT_ARRAY_4(S2CellId temp[4]) {
  $1 = temp;
}

// For S2Polygon::GetOverlapFractions
%typemap(out) std::pair<double, double> {
  $result = Py_BuildValue("dd", $1.first, $1.second);
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

// Convert an S2Error into a Python exception. This requires two
// typemaps: the first declares a local variable of type S2Error and
// passes a pointer to it into the called function, and the second
// inspects the S2Error variable after the call and throws an
// exception if its ok() method returns false.
%typemap(in, numinputs=0) S2Error * (S2Error err) {
  $1 = &err;
}

%typemap(argout) S2Error * {
  if(!$1->ok())
    SWIG_exception(SWIG_ValueError, $1->text().c_str());
}

// This overload shadows the one the takes vector<uint64>&, and it
// does not work anyway.
%ignore S2CellUnion::Init(std::vector<S2CellId> const& cell_ids);

// The SWIG code which picks between overloaded methods doesn't work
// when given a list parameter.  SWIG_Python_ConvertPtrAndOwn calls
// SWIG_Python_GetSwigThis, doesn't find the 'this' attribute and gives up.
// To avoid this problem rename the Polyline::Init methods so they aren't
// overloaded.  We also need to reimplement them since SWIG doesn't
// seem to understand absl::Span.
%extend S2Polyline {
 public:
  void InitFromS2LatLngs(const std::vector<S2LatLng>& vertices) {
    $self->Init(absl::MakeConstSpan(vertices));
  }

  void InitFromS2Points(const std::vector<S2Point>& vertices) {
    $self->Init(absl::MakeConstSpan(vertices));
  }
 };

// And similarly for the overloaded S2CellUnion::Normalize method.
%rename(NormalizeS2CellUnion) S2CellUnion::Normalize();

%apply int *OUTPUT {int *next_vertex};
%apply int *OUTPUT {int *psi};
%apply int *OUTPUT {int *pti};
%apply int *OUTPUT {int *pi};
%apply int *OUTPUT {int *pj};
%apply int *OUTPUT {int *orientation};
%apply SWIGTYPE *DISOWN {S2Loop *loop_disown};
%apply SWIGTYPE *DISOWN {S2Builder::Layer *layer_disown};
%apply SWIGTYPE *DISOWN {S2Polygon *polygon_disown};

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

%inline %{
  // This isn't a constructor because it clashes with the SWIG-redefinition
  // below and the actual S2Point (a Vector_3d).
  static PyObject *S2Point_FromRaw(double x, double y, double z) {
    // Creates an S2Point directly, mostly useful for testing.
    return SWIG_NewPointerObj(new S2Point(x, y, z), SWIGTYPE_p_S2Point,
                              SWIG_POINTER_OWN);
  }

  static PyObject *S2Point_ToRaw(const S2Point& p) {
    return Py_BuildValue("ddd", p[0], p[1], p[2]);
  }
%}

// We provide our own definition of S2Point, because the real one is too
// difficult to wrap correctly.
class S2Point {
 public:
  double x();
  double y();
  double z();
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

// The extensions below exist to work around the use of absl::Span.
%extend S2Loop {
 public:
  explicit S2Loop(const std::vector<S2Point>& vertices) {
    return new S2Loop(absl::MakeConstSpan(vertices));
  }

  void Init(const std::vector<S2Point>& vertices) {
    $self->Init(absl::MakeConstSpan(vertices));
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

%extend S2Builder {
 public:
  void StartLayer(S2Builder::Layer* layer_disown) {
    $self->StartLayer(std::unique_ptr<S2Builder::Layer>(layer_disown));
  }
}

class S2PolygonLayerOptions {
public:
  s2builderutil::S2PolygonLayer::Options opts;
  void set_edge_type(S2Builder::EdgeType);
};

// S2PolygonLayer's constructor takes a pointer to an S2Polygon. We
// need to ensure that the S2Polygon is not destroyed while the
// S2PolygonLayer still references it. To do this, save a reference to
// the polygon in the Python wrapper object.
%pythonprepend s2builderutil::S2PolygonLayer::S2PolygonLayer %{
  self._incref = args[0]
%}

%extend s2builderutil::S2PolygonLayer {
 public:
  S2PolygonLayer(S2Polygon* layer,
                 const S2PolygonLayerOptions& options) {
   return new s2builderutil::S2PolygonLayer(layer, options.opts);
  }
}

%extend MutableS2ShapeIndex {
 public:
  void Add(S2Polygon* polygon_disown) {
    auto polygon = std::unique_ptr<S2Polygon>(polygon_disown);
    $self->Add(std::unique_ptr<S2Shape>(new S2Polygon::OwningShape(std::move(polygon))));
  }
}

%extend S2BooleanOperation {
 public:
 S2BooleanOperation(OpType op_type,
                              S2Builder::Layer* layer_disown) {
   S2BooleanOperation::Options options;
   auto layer = std::unique_ptr<S2Builder::Layer>(layer_disown);
   return new S2BooleanOperation(op_type, std::move(layer), options);
  }
}

class S2BufferOperationOptions {
public:
  S2BufferOperation::Options opts;
  void set_buffer_radius(S1Angle);
  void set_error_fraction(double);
};

%extend S2BufferOperation {
 public:
  S2BufferOperation(S2Builder::Layer* layer_disown) {
   auto layer = std::unique_ptr<S2Builder::Layer>(layer_disown);
   return new S2BufferOperation(std::move(layer));
  }

  S2BufferOperation(S2Builder::Layer* layer_disown,
                    const S2BufferOperationOptions& options) {
   auto layer = std::unique_ptr<S2Builder::Layer>(layer_disown);
   return new S2BufferOperation(std::move(layer), options.opts);
  }

 void AddPolygon(S2Polygon* polygon_disown) {
   auto polygon = std::unique_ptr<S2Polygon>(polygon_disown);
   $self->AddShape(S2Polygon::OwningShape(std::move(polygon)));
 }

 void AddPoint(S2Point& point) {
   $self->AddPoint(point);
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

  int true_max_level() const { return $self->options().true_max_level(); }
}

// Expose Options functions on S2RegionTermIndexer until we figure out
// nested classes in SWIG.
%extend S2RegionTermIndexer {
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

  int true_max_level() const { return $self->options().true_max_level(); }

  bool index_contains_points_only() const {
    return $self->options().index_contains_points_only();
  }
  void set_index_contains_points_only(bool value) {
    $self->mutable_options()->set_index_contains_points_only(value);
  }

  bool optimize_for_space() const {
    return $self->options().optimize_for_space();
  }
  void set_optimize_for_space(bool value) {
    $self->mutable_options()->set_optimize_for_space(value);
  }

  char marker_character() const { return $self->options().marker_character(); }
  void set_marker_character(char ch) {
    $self->mutable_options()->set_marker_character(ch);
  }
}

%copyctor S1ChordAngle;

// Raise ValueError for any functions that would trigger a S2_CHECK/S2_DCHECK.
%pythonprepend S2CellId::child %{
  if not self.is_valid():
    raise ValueError("S2CellId must be valid.")
  if self.is_leaf():
    raise ValueError("S2CellId must be non-leaf.")

  if not 0 <= position < 4:
    raise ValueError("Position must be 0-3.")
%}

// TODO(jrosenstock): child_begin()
// TODO(jrosenstock): child_end()

%pythonprepend S2CellId::child_position(int) const %{
  if not self.is_valid():
    raise ValueError("S2CellId must be valid.")
  if level < 1 or level > self.level():
    raise ValueError("level must must be in range [1, S2 cell level]")
%}

%pythonprepend S2CellId::contains %{
  if not self.is_valid() or not other.is_valid():
    raise ValueError("Both S2CellIds must be valid.")
%}

%pythonprepend S2CellId::intersects %{
  if not self.is_valid() or not other.is_valid():
    raise ValueError("Both S2CellIds must be valid.")
%}

%pythonprepend S2CellId::level %{
  # As in the C++ version:
  # We can't just check is_valid() because we want level() to be
  # defined for end-iterators, i.e. S2CellId.End(level).  However there is
  # no good way to define S2CellId::None().level(), so we do prohibit that.
  if self.id() == 0:
    raise ValueError("None has no level.")
%}

%pythonprepend S2CellId::parent %{
  if not self.is_valid():
    raise ValueError("S2CellId must be valid.")
  if len(args) == 1:
    level, = args
    if level < 0:
      raise ValueError("Level must be non-negative.")
    if level > self.level():
      raise ValueError("Level must be less than or equal to cell's level.")
%}

%ignoreall

%unignore MutableS2ShapeIndex;
%unignore MutableS2ShapeIndex::~MutableS2ShapeIndex;
%unignore MutableS2ShapeIndex::Add(S2Shape*);
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
%unignore S1ChordAngle::degrees;
%unignore S1ChordAngle::Infinity;
%unignore S1Interval;
%ignore S1Interval::operator[];
%unignore S1Interval::GetLength;
%unignore S2;
%unignore S2::CrossingSign;
%unignore S2::GetIntersection;
%unignore S2::Interpolate;
%unignore S2::Rotate;
%unignore S2::TurnAngle;
%unignore S2::UpdateMinDistance;
%unignore S2BooleanOperation;
%unignore S2BooleanOperation::Build;
%unignore S2BooleanOperation::OpType;
%unignore S2BooleanOperation::OpType::UNION;
%unignore S2BooleanOperation::OpType::INTERSECTION;
%unignore S2BooleanOperation::OpType::DIFFERENCE;
%unignore S2BooleanOperation::OpType::SYMMETRIC_DIFFERENCE;
%unignore S2BooleanOperation::S2BooleanOperation(OpType, S2Builder::Layer*, const Options&);
%ignore S2BooleanOperation::S2BooleanOperation(OpType, std::unique_ptr<S2Builder::Layer>, const Options&);
%ignore S2BooleanOperation::S2BooleanOperation(OpType, std::unique_ptr<S2Builder::Layer>);
%unignore S2BufferOperation;
%unignore S2BufferOperation::Build;
%unignore S2BufferOperation::Options;
%unignore S2BufferOperation::S2BufferOperation(S2Builder::Layer*);
%ignore S2BufferOperation::S2BufferOperation(std::unique_ptr<S2Builder::Layer>);
%ignore S2BufferOperation::S2BufferOperation(std::unique_ptr<S2Builder::Layer>, const Options&);
%unignore S2BufferOperationOptions;
%unignore S2BufferOperationOptions::set_buffer_radius;
%unignore S2BufferOperationOptions::set_error_fraction;
%unignore S2Builder;
%unignore S2Builder::Layer;
%unignore S2Builder::S2Builder;
%unignore S2Builder::StartLayer(S2Builder::Layer*);
%unignore S2Builder::AddEdge;
%unignore S2Builder::Build;
%unignore S2Builder::EdgeType;
%unignore S2Builder::EdgeType::DIRECTED;
%unignore S2Builder::EdgeType::UNDIRECTED;
%unignore s2builderutil;
%unignore s2builderutil::S2PolygonLayer;
%unignore s2builderutil::S2PolygonLayer::S2PolygonLayer(S2Polygon*);
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
%unignore S2Cap::FromCenterArea(const S2Point&, double);
%unignore S2Cap::FromCenterHeight(const S2Point&, double);
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
%unignore S2Cell::AverageArea;
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
%unignore S2CellId::AppendAllNeighbors(int, std::vector<S2CellId>*) const;
%rename(GetAllNeighbors) S2CellId::AppendAllNeighbors(int, std::vector<S2CellId>*) const;
%unignore S2CellId::AppendVertexNeighbors(int, std::vector<S2CellId>*) const;
%rename(GetVertexNeighbors) S2CellId::AppendVertexNeighbors(int, std::vector<S2CellId>*) const;
%unignore S2CellId::Begin;
%unignore S2CellId::End;
%unignore S2CellId::FromDebugString(absl::string_view);
%unignore S2CellId::FromFaceIJ(int, int, int);
%unignore S2CellId::FromFacePosLevel(int, uint64, int);
%unignore S2CellId::FromLatLng;
%unignore S2CellId::FromPoint;
%unignore S2CellId::FromToken(absl::string_view);
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
%unignore S2CellId::child_position(int) const;
%unignore S2CellId::contains;
%unignore S2CellId::face;
%unignore S2CellId::id;
%unignore S2CellId::intersects;
%unignore S2CellId::is_leaf;
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
%unignore S2CellUnion::FromNormalized(std::vector<S2CellId>);
%unignore S2CellUnion::GetCapBound() const;
%unignore S2CellUnion::GetDifference;
%unignore S2CellUnion::GetRectBound;
%unignore S2CellUnion::Init(std::vector<uint64> const &);
%unignore S2CellUnion::Intersection;
%unignore S2CellUnion::Intersects;
%unignore S2CellUnion::IsNormalized() const;
%unignore S2CellUnion::MayIntersect(const S2Cell&) const;
// SWIG doesn't handle disambiguation of the overloaded Normalize methods, so
// the Normalize() instance method is renamed to NormalizeS2CellUnion.
%unignore S2CellUnion::Normalize(std::vector<S2CellId>*);
%unignore S2CellUnion::cell_id;
%unignore S2CellUnion::cell_ids;
%unignore S2CellUnion::empty;
%unignore S2CellUnion::num_cells;
%unignore S2Earth;
%unignore S2Earth::GetDistance(const S2LatLng&, const S2LatLng&);
%unignore S2Earth::GetDistance(const S2Point&, const S2Point&);
%unignore S2Earth::GetDistanceKm(const S2LatLng&, const S2LatLng&);
%unignore S2Earth::GetDistanceKm(const S2Point&, const S2Point&);
%unignore S2Earth::GetDistanceMeters(const S2LatLng&, const S2LatLng&);
%unignore S2Earth::GetDistanceMeters(const S2Point&, const S2Point&);
%unignore S2Earth::GetInitialBearing(const S2LatLng&, const S2LatLng&);
%unignore S2Earth::HighestAltitude();
%unignore S2Earth::HighestAltitudeKm();
%unignore S2Earth::HighestAltitudeMeters();
%unignore S2Earth::KmToRadians(double);
%unignore S2Earth::LowestAltitude();
%unignore S2Earth::LowestAltitudeKm();
%unignore S2Earth::LowestAltitudeMeters();
%unignore S2Earth::MetersToRadians(double);
%unignore S2Earth::RadiansToKm(double);
%unignore S2Earth::RadiansToMeters(double);
%unignore S2Earth::Radius();
%unignore S2Earth::RadiusKm();
%unignore S2Earth::RadiusMeters();
%unignore S2Earth::SquareKmToSteradians(double);
%unignore S2Earth::SquareMetersToSteradians(double);
%unignore S2Earth::SteradiansToSquareKm(double);
%unignore S2Earth::SteradiansToSquareMeters(double);
%unignore S2Earth::ToAngle(const util::units::Meters&);
%unignore S2Earth::ToChordAngle(const util::units::Meters&);
%unignore S2Earth::ToDistance(const S1Angle&);
%unignore S2Earth::ToDistance(const S1ChordAngle&);
%unignore S2Earth::ToKm(const S1Angle&);
%unignore S2Earth::ToKm(const S1ChordAngle&);
%unignore S2Earth::ToLongitudeRadians(const util::units::Meters&, double);
%unignore S2Earth::ToMeters(const S1Angle&);
%unignore S2Earth::ToMeters(const S1ChordAngle&);
%unignore S2Earth::ToRadians(const util::units::Meters&);
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
%unignore S2Loop::~S2Loop;
%unignore S2Loop::Clone;
%unignore S2Loop::Contains;
%unignore S2Loop::Decode;
%unignore S2Loop::Encode;
%unignore S2Loop::Equals;
%unignore S2Loop::GetArea;
%unignore S2Loop::GetCapBound() const;
%unignore S2Loop::GetCentroid;
%unignore S2Loop::GetDistance;
%unignore S2Loop::GetRectBound;
%unignore S2Loop::GetS2LatLngVertex;
%unignore S2Loop::Intersects;
%unignore S2Loop::IsNormalized() const;
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
%unignore S2Polygon::BoundaryNear;
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
%unignore S2Polygon::GetLastDescendant(int) const;
%unignore S2Polygon::GetOverlapFractions(const S2Polygon&, const S2Polygon&);
%unignore S2Polygon::GetRectBound;
%unignore S2Polygon::Init;
%unignore S2Polygon::InitNested;
%unignore S2Polygon::InitToUnion;
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
%unignore s2pred;
%unignore s2pred::OrderedCCW;
%unignore S2RegionCoverer;
%unignore S2RegionCoverer::S2RegionCoverer;
%unignore S2RegionCoverer::~S2RegionCoverer;
%unignore S2RegionCoverer::GetCovering(const S2Region&, std::vector<S2CellId>*);
%unignore S2RegionCoverer::GetInteriorCovering(const S2Region&,
                                               std::vector<S2CellId>*);
%unignore S2RegionTermIndexer;
%unignore S2RegionTermIndexer::S2RegionTermIndexer;
%unignore S2RegionTermIndexer::~S2RegionTermIndexer;
%unignore S2RegionTermIndexer::GetIndexTerms(const S2Point&, absl::string_view);
%unignore S2RegionTermIndexer::GetIndexTerms(const S2Region&,
                                             absl::string_view);
%unignore S2RegionTermIndexer::GetIndexTermsForCanonicalCovering(
    const S2CellUnion&, absl::string_view);
%unignore S2RegionTermIndexer::GetQueryTerms(const S2Point&, absl::string_view);
%unignore S2RegionTermIndexer::GetQueryTerms(const S2Region&,
                                             absl::string_view);
%unignore S2RegionTermIndexer::GetQueryTermsForCanonicalCovering(
    const S2CellUnion&, absl::string_view);
%unignore S2ShapeIndex;

%include "s2/r1interval.h"
%include "s2/s1angle.h"
%include "s2/s1chord_angle.h"
%include "s2/s1interval.h"
%include "s2/s2boolean_operation.h"
%include "s2/s2buffer_operation.h"
%include "s2/s2builder.h"
%include "s2/s2builder_layer.h"
%include "s2/s2builderutil_s2polygon_layer.h"
%include "s2/s2cell_id.h"
%include "s2/s2edge_crossings.h"
%include "s2/s2edge_distances.h"
%include "s2/s2earth.h"
%include "s2/s2region.h"
%include "s2/s2cap.h"
%include "s2/s2latlng.h"
%include "s2/s2latlng_rect.h"
%include "s2/s2loop.h"
%include "s2/s2measures.h"
%include "s2/s2pointutil.h"
%include "s2/s2polygon.h"
%include "s2/s2polyline.h"
%include "s2/s2predicates.h"
%include "s2/s2region_coverer.h"
%include "s2/s2region_term_indexer.h"
%include "s2/s2cell.h"
%include "s2/s2cell_union.h"
%include "s2/s2shape_index.h"
%include "s2/mutable_s2shape_index.h"

%unignoreall

%define USE_STREAM_INSERTOR_FOR_STR(type)
  %extend type {
    std::string __str__() {
      std::ostringstream output;
      output << *$self;
      return output.str();
    }
  }
%enddef

%define USE_EQUALS_FN_FOR_EQ_AND_NE(type)
  %extend type {
    bool __eq__(const type& other) {
      return $self->Equals(other);
    }

    bool __ne__(const type& other) {
      return !$self->Equals(other);
    }
  }
%enddef

%define USE_EQUALS_FOR_EQ_AND_NE(type)
  %extend type {
    bool __eq__(const type& other) {
      return *$self == other;
    }

    bool __ne__(const type& other) {
      return *$self != other;
    }
  }
%enddef

%define USE_COMPARISON_FOR_LT_AND_GT(type)
  %extend type {
    bool __lt__(const type& other) {
      return *$self < other;
    }

    bool __gt__(const type& other) {
      return *$self > other;
    }
  }
%enddef

%define USE_ARITHMETIC_FOR_ADD_AND_SUB(type)
  %extend type {
    type __add__(const type& other) {
      return *$self + other;
    }

    type __sub__(const type& other) {
      return *$self - other;
    }
  }
%enddef

%define USE_HASH_FOR_TYPE(type, hash_type)
  %extend type {
    size_t __hash__() {
      return hash_type()(*$self);
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

USE_EQUALS_FOR_EQ_AND_NE(S1ChordAngle)
USE_COMPARISON_FOR_LT_AND_GT(S1ChordAngle)
USE_ARITHMETIC_FOR_ADD_AND_SUB(S1ChordAngle)

USE_EQUALS_FN_FOR_EQ_AND_NE(S2Loop)
USE_EQUALS_FN_FOR_EQ_AND_NE(S2Polygon)
USE_EQUALS_FN_FOR_EQ_AND_NE(S2Polyline)

// Simple implementation of key S2Testing methods
%pythoncode %{
import random

class S2Testing(object):
  """ Simple implementation of key S2Testing methods. """
  _rnd = random.Random(1)

  @classmethod
  def RandomPoint(cls):
    """ Return a random unit-length vector. """
    x = cls._rnd.uniform(-1, 1)
    y = cls._rnd.uniform(-1, 1)
    z = cls._rnd.uniform(-1, 1)
    return S2Point_FromRaw(x, y, z).Normalize()

  @classmethod
  def GetRandomCap(cls, min_area, max_area):
    """
    Return a cap with a random axis such that the log of its area is
    uniformly distributed between the logs of the two given values.
    (The log of the cap angle is also approximately uniformly distributed.)
    """
    cap_area = max_area * pow(min_area / max_area, cls._rnd.random())
    assert cap_area >= min_area
    assert cap_area <= max_area

    # The surface area of a cap is 2*Pi times its height.
    return S2Cap.FromCenterArea(cls.RandomPoint(), cap_area)
%}
