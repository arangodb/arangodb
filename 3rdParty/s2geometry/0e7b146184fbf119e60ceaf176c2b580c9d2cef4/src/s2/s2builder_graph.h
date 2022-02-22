// Copyright 2016 Google Inc. All Rights Reserved.
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

// Author: ericv@google.com (Eric Veach)

#ifndef S2_S2BUILDER_GRAPH_H_
#define S2_S2BUILDER_GRAPH_H_

#include <array>
#include <cstddef>
#include <iterator>
#include <utility>
#include <vector>
#include "s2/base/integral_types.h"
#include "s2/id_set_lexicon.h"
#include "s2/s2builder.h"
#include "s2/s2error.h"

// An S2Builder::Graph represents a collection of snapped edges that is passed
// to a Layer for assembly.  (Example layers include polygons, polylines, and
// polygon meshes.)  The Graph object does not own any of its underlying data;
// it is simply a view of data that is stored elsewhere.  You will only
// need this interface if you want to implement a new Layer subtype.
//
// The graph consists of vertices and directed edges.  Vertices are numbered
// sequentially starting from zero.  An edge is represented as a pair of
// vertex ids.  The edges are sorted in lexicographic order, therefore all of
// the outgoing edges from a particular vertex form a contiguous range.
//
// S2Builder::Graph is movable and copyable.  Note that although this class
// does not own the underlying vertex and edge data, S2Builder guarantees that
// all Graph objects passed to S2Builder::Layer::Build() methods will remain
// valid until all layers have been built.
//
// TODO(ericv): Consider pulling out the methods that are helper functions for
// Layer implementations (such as GetDirectedLoops) into s2builderutil_graph.h.
class S2Builder::Graph {
 public:
  // Identifies a vertex in the graph.  Vertices are numbered sequentially
  // starting from zero.
  using VertexId = int32;

  // Defines an edge as an (origin, destination) vertex pair.
  using Edge = std::pair<VertexId, VertexId>;

  // Identifies an edge in the graph.  Edges are numbered sequentially
  // starting from zero.
  using EdgeId = int32;

  // Identifies an S2Builder *input* edge (before snapping).
  using InputEdgeId = S2Builder::InputEdgeId;

  // Identifies a set of S2Builder input edges.
  using InputEdgeIdSetId = S2Builder::InputEdgeIdSetId;

  // Identifies a set of edge labels.
  using LabelSetId = S2Builder::LabelSetId;

  // Determines whether a degenerate polygon is empty or full.
  using IsFullPolygonPredicate = S2Builder::IsFullPolygonPredicate;

  // The default constructor exists only for the benefit of STL containers.
  // The graph must be initialized (using the assignment operator) before it
  // is used.
  Graph();

  // Note that most of the parameters are passed by const reference and must
  // exist for the duration of the Graph object.  Notes on parameters:
  // "options":
  //    - the GraphOptions used to build the Graph.  In some cases these
  //      can be different than the options provided by the Layer.
  // "vertices":
  //   - a vector of S2Points indexed by VertexId.
  // "edges":
  //   - a vector of VertexId pairs (sorted in lexicographic order)
  //     indexed by EdgeId.
  // "input_edge_id_set_ids":
  //   - a vector indexed by EdgeId that allows access to the set of
  //     InputEdgeIds that were mapped to the given edge, by looking up the
  //     returned value (an InputEdgeIdSetId) in "input_edge_id_set_lexicon".
  // "input_edge_id_set_lexicon":
  //   - a class that maps an InputEdgeIdSetId to a set of InputEdgeIds.
  // "label_set_ids":
  //   - a vector indexed by InputEdgeId that allows access to the set of
  //     labels that were attached to the given input edge, by looking up the
  //     returned value (a LabelSetId) in the "label_set_lexicon".
  // "label_set_lexicon":
  //   - a class that maps a LabelSetId to a set of S2Builder::Labels.
  // "is_full_polygon_predicate":
  //   - a predicate called to determine whether a graph consisting only of
  //     polygon degeneracies represents the empty polygon or the full polygon
  //     (see s2builder.h for details).
  Graph(const GraphOptions& options,
        const std::vector<S2Point>* vertices,
        const std::vector<Edge>* edges,
        const std::vector<InputEdgeIdSetId>* input_edge_id_set_ids,
        const IdSetLexicon* input_edge_id_set_lexicon,
        const std::vector<LabelSetId>* label_set_ids,
        const IdSetLexicon* label_set_lexicon,
        IsFullPolygonPredicate is_full_polygon_predicate);

  const GraphOptions& options() const;

  // Returns the number of vertices in the graph.
  VertexId num_vertices() const;

  // Returns the vertex at the given index.
  const S2Point& vertex(VertexId v) const;

  // Returns the entire set of vertices.
  const std::vector<S2Point>& vertices() const;

  // Returns the total number of edges in the graph.
  EdgeId num_edges() const;

  // Returns the endpoints of the given edge (as vertex indices).
  const Edge& edge(EdgeId e) const;

  // Returns the entire set of edges.
  const std::vector<Edge>& edges() const;

  // Given an edge (src, dst), returns the reverse edge (dst, src).
  static Edge reverse(const Edge& e);

  // Returns a vector of edge ids sorted in lexicographic order by
  // (destination, origin).  All of the incoming edges to each vertex form a
  // contiguous subrange of this ordering.
  std::vector<EdgeId> GetInEdgeIds() const;

  // Given a graph such that every directed edge has a sibling, returns a map
  // from EdgeId to the sibling EdgeId.  This method is identical to
  // GetInEdgeIds() except that (1) it requires edges to have siblings, and
  // (2) undirected degenerate edges are grouped together in pairs such that
  // one edge is the sibling of the other.  Handles duplicate edges correctly
  // and is also consistent with GetLeftTurnMap().
  //
  // REQUIRES: An option is chosen that guarantees sibling pairs:
  //     (options.sibling_pairs() == { REQUIRE, CREATE } ||
  //      options.edge_type() == UNDIRECTED)
  std::vector<EdgeId> GetSiblingMap() const;

  // Like GetSiblingMap(), but constructs the map starting from the vector of
  // incoming edge ids returned by GetInEdgeIds().  (This operation is a no-op
  // except unless undirected degenerate edges are present, in which case such
  // edges are grouped together in pairs to satisfy the requirement that every
  // edge must have a sibling edge.)
  void MakeSiblingMap(std::vector<EdgeId>* in_edge_ids) const;

  class VertexOutMap;  // Forward declaration
  class VertexInMap;   // Forward declaration

  // A helper class for VertexOutMap that represents the outgoing edges
  // from a given vertex.
  class VertexOutEdges {
   public:
    const Edge* begin() const { return begin_; }
    const Edge* end() const { return end_; }
    size_t size() const { return end_ - begin_; }

   private:
    friend class VertexOutMap;
    VertexOutEdges(const Edge* begin, const Edge* end);
    const Edge* begin_;
    const Edge* end_;
  };

  // A helper class for VertexOutMap that represents the outgoing edge *ids*
  // from a given vertex.
  class VertexOutEdgeIds
      : public std::iterator<std::forward_iterator_tag, EdgeId> {
   public:
    // An iterator over a range of edge ids (like boost::counting_iterator).
    class Iterator {
     public:
      explicit Iterator(EdgeId id) : id_(id) {}
      const EdgeId& operator*() const { return id_; }
      Iterator& operator++() { ++id_; return *this; }
      Iterator operator++(int) { return Iterator(id_++); }
      size_t operator-(const Iterator& x) const { return id_ - x.id_; }
      bool operator==(const Iterator& x) const { return id_ == x.id_; }
      bool operator!=(const Iterator& x) const { return id_ != x.id_; }

     private:
      EdgeId id_;
    };
    Iterator begin() const { return Iterator(begin_); }
    Iterator end() const { return Iterator(end_); }
    size_t size() const { return end_ - begin_; }

   private:
    friend class VertexOutMap;
    VertexOutEdgeIds(EdgeId begin, EdgeId end);
    EdgeId begin_, end_;
  };

  // A class that maps vertices to their outgoing edge ids.  Example usage:
  //   VertexOutMap out(g);
  //   for (Graph::EdgeId e : out.edge_ids(v)) { ... }
  //   for (const Graph::Edge& edge : out.edges(v)) { ... }
  class VertexOutMap {
   public:
    explicit VertexOutMap(const Graph& g);

    int degree(VertexId v) const;
    VertexOutEdges edges(VertexId v) const;
    VertexOutEdgeIds edge_ids(VertexId v) const;

    // Return the edges (or edge ids) between a specific pair of vertices.
    VertexOutEdges edges(VertexId v0, VertexId v1) const;
    VertexOutEdgeIds edge_ids(VertexId v0, VertexId v1) const;

   private:
    const std::vector<Edge>& edges_;
    std::vector<EdgeId> edge_begins_;
    VertexOutMap(const VertexOutMap&) = delete;
    void operator=(const VertexOutMap&) = delete;
  };

  // A helper class for VertexInMap that represents the incoming edge *ids*
  // to a given vertex.
  class VertexInEdgeIds {
   public:
    const EdgeId* begin() const { return begin_; }
    const EdgeId* end() const { return end_; }
    size_t size() const { return end_ - begin_; }

   private:
    friend class VertexInMap;
    VertexInEdgeIds(const EdgeId* begin, const EdgeId* end);
    const EdgeId* begin_;
    const EdgeId* end_;
  };

  // A class that maps vertices to their incoming edge ids.  Example usage:
  //   VertexInMap in(g);
  //   for (Graph::EdgeId e : in.edge_ids(v)) { ... }
  class VertexInMap {
   public:
    explicit VertexInMap(const Graph& g);

    int degree(VertexId v) const;
    VertexInEdgeIds edge_ids(VertexId v) const;

    // Returns a sorted vector of all incoming edges (see GetInEdgeIds).
    const std::vector<EdgeId>& in_edge_ids() const { return in_edge_ids_; }

   private:
    std::vector<EdgeId> in_edge_ids_;
    std::vector<EdgeId> in_edge_begins_;
    VertexInMap(const VertexInMap&) = delete;
    void operator=(const VertexInMap&) = delete;
  };

  // Defines a value larger than any valid InputEdgeId.
  static const InputEdgeId kMaxInputEdgeId =
      std::numeric_limits<InputEdgeId>::max();

  // The following value of InputEdgeId means that an edge does not
  // corresponds to any input edge.
  static const InputEdgeId kNoInputEdgeId = kMaxInputEdgeId - 1;

  // Returns the set of input edge ids that were snapped to the given
  // edge.  ("Input edge ids" are assigned to input edges sequentially in
  // the order they are added to the builder.)  For example, if input
  // edges 2 and 17 were snapped to edge 12, then input_edge_ids(12)
  // returns a set containing the numbers 2 and 17.  Example usage:
  //
  //   for (InputEdgeId input_edge_id : g.input_edge_ids(e)) { ... }
  //
  // Please note the following:
  //
  //  - When edge chains are simplified, the simplified edge is assigned all
  //    the input edge ids associated with edges of the chain.
  //
  //  - Edges can also have multiple input edge ids due to edge merging
  //    (if DuplicateEdges::MERGE is specified).
  //
  //  - Siblings edges automatically created by EdgeType::UNDIRECTED or
  //    SiblingPairs::CREATE have an empty set of input edge ids.  (However
  //    you can use a LabelFetcher to retrieve the set of labels associated
  //    with both edges of a given sibling pair.)
  IdSetLexicon::IdSet input_edge_ids(EdgeId e) const;

  // Low-level method that returns an integer representing the entire set of
  // input edge ids that were snapped to the given edge.  The elements of the
  // IdSet can be accessed using input_edge_id_set_lexicon().
  InputEdgeIdSetId input_edge_id_set_id(EdgeId e) const;

  // Low-level method that returns a vector where each element represents the
  // set of input edge ids that were snapped to a particular output edge.
  const std::vector<InputEdgeIdSetId>& input_edge_id_set_ids() const;

  // Returns a mapping from an InputEdgeIdSetId to a set of input edge ids.
  const IdSetLexicon& input_edge_id_set_lexicon() const;

  // Returns the minimum input edge id that was snapped to this edge, or -1 if
  // no input edges were snapped (see SiblingPairs::CREATE).  This is
  // useful for layers that wish to preserve the input edge ordering as much
  // as possible (e.g., to ensure idempotency).
  InputEdgeId min_input_edge_id(EdgeId e) const;

  // Returns a vector containing the minimum input edge id for every edge.
  // If an edge has no input ids, kNoInputEdgeId is used.
  std::vector<InputEdgeId> GetMinInputEdgeIds() const;

  // Returns a vector of EdgeIds sorted by minimum input edge id.  This is an
  // approximation of the input edge ordering.
  std::vector<EdgeId> GetInputEdgeOrder(
      const std::vector<InputEdgeId>& min_input_edge_ids) const;

  // Convenience class to return the set of labels associated with a given
  // graph edge.  Note that due to snapping, one graph edge may correspond to
  // several different input edges and will have all of their labels.
  // This class is the preferred way to retrieve edge labels.
  //
  // The reason this is a class rather than a graph method is because for
  // undirected edges, we need to fetch the labels associated with both
  // siblings.  This is because only the original edge of the sibling pair has
  // labels; the automatically generated sibling edge does not.
  class LabelFetcher {
   public:
    // Prepares to fetch labels associated with the given edge type.  For
    // EdgeType::UNDIRECTED, labels associated with both edges of the sibling
    // pair will be returned.  "edge_type" is a parameter (rather than using
    // g.options().edge_type()) so that clients can explicitly control whether
    // labels from one or both siblings are returned.
    LabelFetcher(const Graph& g, EdgeType edge_type);

    // Returns the set of labels associated with edge "e" (and also the labels
    // associated with the sibling of "e" if edge_type() is UNDIRECTED).
    // Labels are sorted and duplicate labels are automatically removed.
    //
    // This method uses an output parameter rather than returning by value in
    // order to avoid allocating a new vector on every call to this method.
    void Fetch(EdgeId e, std::vector<S2Builder::Label>* labels);

   private:
    const Graph& g_;
    EdgeType edge_type_;
    std::vector<EdgeId> sibling_map_;
  };

  // Returns the set of labels associated with a given input edge.  Example:
  //   for (Label label : g.labels(input_edge_id)) { ... }
  IdSetLexicon::IdSet labels(InputEdgeId e) const;

  // Low-level method that returns an integer representing the set of
  // labels associated with a given input edge.  The elements of
  // the IdSet can be accessed using label_set_lexicon().
  LabelSetId label_set_id(InputEdgeId e) const;

  // Low-level method that returns a vector where each element represents the
  // set of labels associated with a particular output edge.
  const std::vector<LabelSetId>& label_set_ids() const;

  // Returns a mapping from a LabelSetId to a set of labels.
  const IdSetLexicon& label_set_lexicon() const;

  // Convenience method that calls is_full_polygon_predicate() to determine
  // whether a graph that consists only of polygon degeneracies represents the
  // empty polygon or the full polygon (see s2builder.h for details).
  bool IsFullPolygon(S2Error* error) const;

  // Returns a method that determines whether a graph that consists only of
  // polygon degeneracies represents the empty polygon or the full polygon
  // (see s2builder.h for details).
  const IsFullPolygonPredicate& is_full_polygon_predicate() const;

  // Returns a map "m" that maps each edge e=(v0,v1) to the following outgoing
  // edge around "v1" in clockwise order.  (This corresponds to making a "left
  // turn" at the vertex.)  By starting at a given edge and making only left
  // turns, you can construct a loop whose interior does not contain any edges
  // in the same connected component.
  //
  // If the incoming and outgoing edges around a vertex do not alternate
  // perfectly (e.g., there are two incoming edges in a row), then adjacent
  // (incoming, outgoing) pairs are repeatedly matched and removed.  This is
  // similar to finding matching parentheses in a string such as "(()())()".
  //
  // For sibling edge pairs, the incoming edge is assumed to immediately
  // follow the outgoing edge in clockwise order.  Thus a left turn is made
  // from an edge to its sibling only if there are no other outgoing edges.
  // With respect to the parentheses analogy, a sibling pair is ")(".
  // Similarly, if there are multiple copies of a sibling edge pair then the
  // duplicate incoming and outgoing edges are sorted in alternating order
  // (e.g., ")()(").
  //
  // Degenerate edges (edges from a vertex to itself) are treated as loops
  // consisting of a single edge.  This avoids the problem of deciding the
  // connectivity and ordering of such edges when they share a vertex with
  // other edges (possibly including other degenerate edges).
  //
  // If it is not possible to make a left turn from every input edge, this
  // method returns false and sets "error" appropriately.  In this situation
  // the left turn map is still valid except that any incoming edge where it
  // is not possible to make a left turn will have its entry set to -1.
  //
  // "in_edge_ids" should be equal to GetInEdgeIds() or GetSiblingMap().
  bool GetLeftTurnMap(const std::vector<EdgeId>& in_edge_ids,
                      std::vector<EdgeId>* left_turn_map,
                      S2Error* error) const;

  // Rotates the edges of "loop" if necessary so that the edge(s) with the
  // largest input edge ids are last.  This ensures that when an output loop
  // is equivalent to an input loop, their cyclic edge orders are the same.
  // "min_input_ids" is the output of GetMinInputEdgeIds().
  static void CanonicalizeLoopOrder(
      const std::vector<InputEdgeId>& min_input_ids,
      std::vector<EdgeId>* loop);

  // Sorts the given edge chains (i.e., loops or polylines) by the minimum
  // input edge id of each chains's first edge.  This ensures that when the
  // output consists of multiple loops or polylines, they are sorted in the
  // same order as they were provided in the input.
  static void CanonicalizeVectorOrder(
      const std::vector<InputEdgeId>& min_input_ids,
      std::vector<std::vector<EdgeId>>* chains);

  // A loop consisting of a sequence of edges.
  using EdgeLoop = std::vector<EdgeId>;

  // Indicates whether loops should be simple cycles (no repeated vertices) or
  // circuits (which allow repeated vertices but not repeated edges).  In
  // terms of how the loops are built, this corresponds to closing off a loop
  // at the first repeated vertex vs. the first repeated edge.
  enum class LoopType { SIMPLE, CIRCUIT };

  // Builds loops from a set of directed edges, turning left at each vertex
  // until either a repeated vertex (for LoopType::SIMPLE) or a repeated edge
  // (for LoopType::CIRCUIT) is found.  (Use LoopType::SIMPLE if you intend to
  // construct an S2Loop.)
  //
  // Each loop is represented as a sequence of edges.  The edge ordering and
  // loop ordering are automatically canonicalized in order to preserve the
  // input ordering as much as possible.  Loops are non-crossing provided that
  // the graph contains no crossing edges.  If some edges cannot be turned
  // into loops, returns false and sets "error" appropriately.
  //
  // If any degenerate edges are present, then each such edge is treated as a
  // separate loop.  This is mainly useful in conjunction with
  // options.degenerate_edges() == DISCARD_EXCESS, in order to build polygons
  // that preserve degenerate geometry.
  //
  // REQUIRES: options.degenerate_edges() == {DISCARD, DISCARD_EXCESS}
  // REQUIRES: options.edge_type() == DIRECTED
  bool GetDirectedLoops(LoopType loop_type, std::vector<EdgeLoop>* loops,
                        S2Error* error) const;

  // Builds loops from a set of directed edges, turning left at each vertex
  // until a repeated edge is found (i.e., LoopType::CIRCUIT).  The loops are
  // further grouped into connected components, where each component consists
  // of one or more loops connected by shared vertices.
  //
  // This method is used to build polygon meshes from directed or undirected
  // input edges.  To convert the output of this method into a mesh, the
  // client must determine how the loops in different components are related
  // to each other: for example, several loops from different components may
  // bound the same region on the sphere, in which case all of those loops are
  // combined into a single polygon.  (See s2shapeutil::BuildPolygonBoundaries
  // and s2builderutil::LaxPolygonVectorLayer for details.)
  //
  // Note that loops may include both edges of a sibling pair.  When several
  // such edges are connected in a chain or a spanning tree, they form a
  // zero-area "filament".  The entire loop may be a filament (i.e., a
  // degenerate loop with an empty interior), or the loop may have have
  // non-empty interior with several filaments that extend inside it, or the
  // loop may consist of several "holes" connected by filaments.  These
  // filaments do not change the interior of any loop, so if you are only
  // interested in point containment then they can safely be removed by
  // setting the "degenerate_boundaries" parameter to DISCARD.  (They can't be
  // removed by setting (options.sibling_pairs() == DISCARD) because the two
  // siblings might belong to different polygons of the mesh.)  Note that you
  // can prevent multiple copies of sibling pairs by specifying
  // options.duplicate_edges() == MERGE.
  //
  // Each loop is represented as a sequence of edges.  The edge ordering and
  // loop ordering are automatically canonicalized in order to preserve the
  // input ordering as much as possible.  Loops are non-crossing provided that
  // the graph contains no crossing edges.  If some edges cannot be turned
  // into loops, returns false and sets "error" appropriately.
  //
  // REQUIRES: options.degenerate_edges() == { DISCARD, DISCARD_EXCESS }
  //           (but requires DISCARD if degenerate_boundaries == DISCARD)
  // REQUIRES: options.sibling_pairs() == { REQUIRE, CREATE }
  //           [i.e., every edge must have a sibling edge]
  enum class DegenerateBoundaries { DISCARD, KEEP };
  using DirectedComponent = std::vector<EdgeLoop>;
  bool GetDirectedComponents(
      DegenerateBoundaries degenerate_boundaries,
      std::vector<DirectedComponent>* components, S2Error* error) const;

  // Builds loops from a set of undirected edges, turning left at each vertex
  // until either a repeated vertex (for LoopType::SIMPLE) or a repeated edge
  // (for LoopType::CIRCUIT) is found.  The loops are further grouped into
  // "components" such that all the loops in a component are connected by
  // shared vertices.  Finally, the loops in each component are divided into
  // two "complements" such that every edge in one complement is the sibling
  // of an edge in the other complement.  This corresponds to the fact that
  // given any set of non-crossing undirected loops, there are exactly two
  // possible interpretations of the region that those loops represent (where
  // one possibility is the complement of the other).  This method does not
  // attempt to resolve this ambiguity, but instead returns both possibilities
  // for each connected component and lets the client choose among them.
  //
  // This method is used to build single polygons.  (Use GetDirectedComponents
  // to build polygon meshes, even when the input edges are undirected.)  To
  // convert the output of this method into a polygon, the client must choose
  // one complement from each component such that the entire set of loops is
  // oriented consistently (i.e., they define a region such that the interior
  // of the region is always on the left).  The non-chosen complements form
  // another set of loops that are also oriented consistently but represent
  // the complementary region on the sphere.  Finally, the client needs to
  // choose one of these two sets of loops based on heuristics (e.g., the area
  // of each region), since both sets of loops are equally valid
  // interpretations of the input.
  //
  // Each loop is represented as a sequence of edges.  The edge ordering and
  // loop ordering are automatically canonicalized in order to preserve the
  // input ordering as much as possible.  Loops are non-crossing provided that
  // the graph contains no crossing edges.  If some edges cannot be turned
  // into loops, returns false and sets "error" appropriately.
  //
  // REQUIRES: options.degenerate_edges() == { DISCARD, DISCARD_EXCESS }
  // REQUIRES: options.edge_type() == UNDIRECTED
  // REQUIRES: options.siblings_pairs() == { DISCARD, DISCARD_EXCESS, KEEP }
  //           [since REQUIRE, CREATE convert the edge_type() to DIRECTED]
  using UndirectedComponent = std::array<std::vector<EdgeLoop>, 2>;
  bool GetUndirectedComponents(LoopType loop_type,
                               std::vector<UndirectedComponent>* components,
                               S2Error* error) const;

  // Indicates whether polylines should be "paths" (which don't allow
  // duplicate vertices, except possibly the first and last vertex) or
  // "walks" (which allow duplicate vertices and edges).
  enum class PolylineType { PATH, WALK };

  // Builds polylines from a set of edges.  If "polyline_type" is PATH, then
  // only vertices of indegree and outdegree 1 (or degree 2 in the case of
  // undirected edges) will appear in the interior of polylines.  This
  // essentially generates one polyline for each edge chain in the graph.  If
  // "polyline_type" is WALK, then polylines may pass through the same vertex
  // or even the same edge multiple times (if duplicate edges are present),
  // and each polyline will be as long as possible.  This option is useful for
  // reconstructing a polyline that has been snapped to a lower resolution,
  // since snapping can cause edges to become identical.
  //
  // This method attempts to preserve the input edge ordering in order to
  // implement idempotency, even when there are repeated edges or loops.  This
  // is true whether directed or undirected edges are used.  Degenerate edges
  // are also handled appropriately.
  //
  // REQUIRES: options.sibling_pairs() == { DISCARD, DISCARD_EXCESS, KEEP }
  using EdgePolyline = std::vector<EdgeId>;
  std::vector<EdgePolyline> GetPolylines(PolylineType polyline_type) const;

  ////////////////////////////////////////////////////////////////////////
  //////////////// Helper Functions for Creating Graphs //////////////////

  // Given an unsorted collection of edges, transform them according to the
  // given set of GraphOptions.  This includes actions such as discarding
  // degenerate edges; merging duplicate edges; and canonicalizing sibling
  // edge pairs in several possible ways (e.g. discarding or creating them).
  // The output is suitable for passing to the Graph constructor.
  //
  // If options.edge_type() == EdgeType::UNDIRECTED, then all input edges
  // should already have been transformed into a pair of directed edges.
  //
  // "input_ids" is a vector of the same length as "edges" that indicates
  // which input edges were snapped to each edge.  This vector is also updated
  // appropriately as edges are discarded, merged, etc.
  //
  // Note that "options" may be modified by this method: in particular, the
  // edge_type() can be changed if sibling_pairs() is CREATE or REQUIRE (see
  // the description of S2Builder::GraphOptions).
  static void ProcessEdges(
      GraphOptions* options, std::vector<Edge>* edges,
      std::vector<InputEdgeIdSetId>* input_ids, IdSetLexicon* id_set_lexicon,
      S2Error* error);

  // Given a set of vertices and edges, removes all vertices that do not have
  // any edges and returned the new, minimal set of vertices.  Also updates
  // each edge in "edges" to correspond to the new vertex numbering.  (Note
  // that this method does *not* merge duplicate vertices, it simply removes
  // vertices of degree zero.)
  //
  // The new vertex ordering is a subsequence of the original ordering,
  // therefore if the edges were lexicographically sorted before calling this
  // method then they will still be sorted after calling this method.
  //
  // The extra argument "tmp" points to temporary storage used by this method.
  // All calls to this method from a single thread can reuse the same
  // temporary storage.  It should initially point to an empty vector.  This
  // can make a big difference to efficiency when this method is called many
  // times (e.g. to extract the vertices for different layers), since the
  // incremental running time for each layer becomes O(edges.size()) rather
  // than O(vertices.size() + edges.size()).
  static std::vector<S2Point> FilterVertices(
      const std::vector<S2Point>& vertices, std::vector<Edge>* edges,
      std::vector<VertexId>* tmp);

  // A comparison function that allows stable sorting with std::sort (which is
  // fast but not stable).  It breaks ties between equal edges by comparing
  // their edge ids.
  static bool StableLessThan(const Edge& a, const Edge& b,
                             EdgeId ai, EdgeId bi);

 private:
  class EdgeProcessor;
  class PolylineBuilder;

  GraphOptions options_;
  VertexId num_vertices_;  // Cached to avoid division by 24.

  const std::vector<S2Point>* vertices_;
  const std::vector<Edge>* edges_;
  const std::vector<InputEdgeIdSetId>* input_edge_id_set_ids_;
  const IdSetLexicon* input_edge_id_set_lexicon_;
  const std::vector<LabelSetId>* label_set_ids_;
  const IdSetLexicon* label_set_lexicon_;
  IsFullPolygonPredicate is_full_polygon_predicate_;
};


//////////////////   Implementation details follow   ////////////////////


inline S2Builder::Graph::Graph()
    : options_(), num_vertices_(-1), vertices_(nullptr), edges_(nullptr),
      input_edge_id_set_ids_(nullptr), input_edge_id_set_lexicon_(nullptr),
      label_set_ids_(nullptr), label_set_lexicon_(nullptr) {
}

inline const S2Builder::GraphOptions& S2Builder::Graph::options() const {
  return options_;
}

inline S2Builder::Graph::VertexId S2Builder::Graph::num_vertices() const {
  return num_vertices_;  // vertices_.size() requires division by 24.
}

inline const S2Point& S2Builder::Graph::vertex(VertexId v) const {
  return vertices()[v];
}

inline const std::vector<S2Point>& S2Builder::Graph::vertices() const {
  return *vertices_;
}

inline S2Builder::Graph::EdgeId S2Builder::Graph::num_edges() const {
  return static_cast<S2Builder::Graph::EdgeId>(edges().size());
}

inline const S2Builder::Graph::Edge& S2Builder::Graph::edge(EdgeId e) const {
  return edges()[e];
}

inline const std::vector<S2Builder::Graph::Edge>&
S2Builder::Graph::edges() const {
  return *edges_;
}

inline S2Builder::Graph::Edge S2Builder::Graph::reverse(const Edge& e) {
  return Edge(e.second, e.first);
}

inline S2Builder::Graph::VertexOutEdges::VertexOutEdges(const Edge* begin,
                                                        const Edge* end)
    : begin_(begin), end_(end) {
}

inline S2Builder::Graph::VertexOutEdges
S2Builder::Graph::VertexOutMap::edges(VertexId v) const {
  return VertexOutEdges(edges_.data() + edge_begins_[v],
                        edges_.data() + edge_begins_[v + 1]);
}

inline S2Builder::Graph::VertexOutEdges
S2Builder::Graph::VertexOutMap::edges(VertexId v0, VertexId v1) const {
  auto range = std::equal_range(edges_.data() + edge_begins_[v0],
                                edges_.data() + edge_begins_[v0 + 1],
                                Edge(v0, v1));
  return VertexOutEdges(range.first, range.second);
}

inline S2Builder::Graph::VertexOutEdgeIds::VertexOutEdgeIds(EdgeId begin,
                                                            EdgeId end)
    : begin_(begin), end_(end) {
}

inline S2Builder::Graph::VertexOutEdgeIds
S2Builder::Graph::VertexOutMap::edge_ids(VertexId v) const {
  return VertexOutEdgeIds(edge_begins_[v], edge_begins_[v + 1]);
}

inline S2Builder::Graph::VertexOutEdgeIds
S2Builder::Graph::VertexOutMap::edge_ids(VertexId v0, VertexId v1) const {
  auto range = std::equal_range(edges_.data() + edge_begins_[v0],
                                edges_.data() + edge_begins_[v0 + 1],
                                Edge(v0, v1));
  return VertexOutEdgeIds(
      static_cast<S2Builder::Graph::EdgeId>(range.first - edges_.data()),
      static_cast<S2Builder::Graph::EdgeId>(range.second - edges_.data()));
}

inline int S2Builder::Graph::VertexOutMap::degree(VertexId v) const {
  return static_cast<int>(edge_ids(v).size());
}

inline S2Builder::Graph::VertexInEdgeIds::VertexInEdgeIds(const EdgeId* begin,
                                                          const EdgeId* end)
    : begin_(begin), end_(end) {
}

inline S2Builder::Graph::VertexInEdgeIds
S2Builder::Graph::VertexInMap::edge_ids(VertexId v) const {
  return VertexInEdgeIds(in_edge_ids_.data() + in_edge_begins_[v],
                         in_edge_ids_.data() + in_edge_begins_[v + 1]);
}

inline int S2Builder::Graph::VertexInMap::degree(VertexId v) const {
  return static_cast<int>(edge_ids(v).size());
}

inline IdSetLexicon::IdSet S2Builder::Graph::input_edge_ids(EdgeId e) const {
  return input_edge_id_set_lexicon().id_set(input_edge_id_set_ids()[e]);
}

inline const std::vector<S2Builder::InputEdgeIdSetId>&
S2Builder::Graph::input_edge_id_set_ids() const {
  return *input_edge_id_set_ids_;
}

inline S2Builder::InputEdgeIdSetId
S2Builder::Graph::input_edge_id_set_id(EdgeId e) const {
  return input_edge_id_set_ids()[e];
}

inline const IdSetLexicon& S2Builder::Graph::input_edge_id_set_lexicon() const {
  return *input_edge_id_set_lexicon_;
}

inline IdSetLexicon::IdSet S2Builder::Graph::labels(LabelSetId id) const {
  return label_set_lexicon().id_set(label_set_ids()[id]);
}

inline S2Builder::LabelSetId S2Builder::Graph::label_set_id(EdgeId e) const {
  return label_set_ids()[e];
}

inline const std::vector<S2Builder::LabelSetId>&
S2Builder::Graph::label_set_ids() const {
  return *label_set_ids_;
}

inline const IdSetLexicon& S2Builder::Graph::label_set_lexicon() const {
  return *label_set_lexicon_;
}

inline bool S2Builder::Graph::IsFullPolygon(S2Error* error) const {
  return is_full_polygon_predicate_(*this, error);
}

inline const S2Builder::IsFullPolygonPredicate&
S2Builder::Graph::is_full_polygon_predicate() const {
  return is_full_polygon_predicate_;
}

inline bool S2Builder::Graph::StableLessThan(
    const Edge& a, const Edge& b, EdgeId ai, EdgeId bi) {
  // The following is simpler but the compiler (2016) doesn't optimize it as
  // well as it should:
  //   return make_pair(a, ai) < make_pair(b, bi);
  if (a.first < b.first) return true;
  if (b.first < a.first) return false;
  if (a.second < b.second) return true;
  if (b.second < a.second) return false;
  return ai < bi;  // Stable sort.
}

#endif  // S2_S2BUILDER_GRAPH_H_
