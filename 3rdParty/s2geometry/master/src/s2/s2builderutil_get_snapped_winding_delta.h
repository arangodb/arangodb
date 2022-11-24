// Copyright 2020 Google Inc. All Rights Reserved.
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

#ifndef S2_S2BUILDERUTIL_GET_SNAPPED_WINDING_DELTA_H_
#define S2_S2BUILDERUTIL_GET_SNAPPED_WINDING_DELTA_H_

#include <functional>

#include "absl/types/span.h"
#include "s2/s2builder.h"
#include "s2/s2builder_graph.h"

namespace s2builderutil {

// A function that returns true if the given S2Builder input edge should be
// ignored in the winding number calculation.  This means either that the edge
// is not a loop edge (e.g., a non-closed polyline) or that this loop should
// not affect the winding number.  This is useful for two purposes:
//
//  - To process graphs that contain polylines and points in addition to loops.
//
//  - To process graphs where the winding number is computed with respect to
//    only a subset of the input loops.
//
// It can be default-constructed to indicate that no edges should be ignored.
using InputEdgeFilter = std::function<bool (S2Builder::Graph::InputEdgeId)>;

// Given an S2Builder::Graph of output edges after snap rounding and a
// reference vertex R, computes the change in winding number of R due to
// snapping.  (See S2WindingOperation for an introduction to winding numbers on
// the sphere.)  The return value can be added to the original winding number
// of R to obtain the winding number of the corresponding snapped vertex R'.
//
// The algorithm requires that the S2Builder input edges consist entirely of
// (possibly self-intersecting) closed loops.  If you need to process inputs
// that include other types of geometry (e.g., non-closed polylines), you will
// need to either (1) put them into a different S2Builder layer, (2) close the
// polylines into loops (e.g. using GraphOptions::SiblingEdges::CREATE), or (3)
// provide a suitable InputEdgeFilter (see above) so that the non-loop edges
// can be ignored.
//
// The algorithm is designed to be robust for any input edge configuration and
// snapping result.  However note that it cannot be used in conjunction with
// edge chain simplification (S2Builder::Options::simplify_edge_chains).  It
// also requires that S2Builder::GraphOptions be configured to keep all snapped
// edges, even degenerate ones (see requirements below).
//
// "ref_in" is the reference vertex location before snapping.  It *must* be an
// input vertex to S2Builder, however this is not checked.
//
// "ref_v" is the Graph::VertexId of the reference vertex after snapping.
// (This can be found using the FindFirstVertexId() function below if desired.)
//
// "input_edge_filter" can optionally be used to ignore input edges that
// should not affect the winding number calculation (such as polyline edges).
// The value can be default-constructed (InputEdgeFilter{}) to use all edges.
//
// "builder" is the S2Builder that produced the given edge graph.  It is used
// to map InputEdgeIds back to the original edge definitions, and also to
// verify that no incompatible S2Builder::Options were used (see below).
//
// "g" is the S2Builder output graph of snapped edges.
//
// The only possible errors are usage errors, in which case "error" is set to
// an appropriate error message and a very large value is returned.
//
// Example usage:
//
// This function is generally called from an S2Builder::Layer implementation.
// We assume here that the reference vertex is the first vertex of the input
// edge identified by "ref_input_edge_id_", and that its desired winding number
// with respect to the input loops is "ref_winding_".
//
// using Graph = S2Builder::Graph;
// class SomeLayer : public S2Builder::Layer {
//  private:
//   int ref_input_edge_id_;
//   int ref_winding_;
//   const S2Builder& builder_;
//
//  public:
//   ...
//   void Build(const Graph& g, S2Error* error) {
//     // Find the positions of the reference vertex before and after snapping.
//     S2Point ref_in = builder_.input_edge(ref_input_edge_id_).v0;
//     Graph::VertexId ref_v =
//         s2builderutil::FindFirstVertexId(ref_input_edge_id_, g);
//     S2Point ref_out = g.vertex(ref_v);
//
//     // Compute the change in winding number due to snapping.
//     S2Error error;
//     ref_winding_ += s2builderutil::GetSnappedWindingDelta(
//         ref_in, ref_v, InputEdgeFilter{}, builder_, g, error);
//     S2_CHECK(error->ok());  // All errors are usage errors.
//
//     // Winding numbers of others points can now be found by counting signed
//     // edge crossings (S2EdgeCrosser::SignedEdgeOrVertexCrossing) between
//     // "ref_out" and the desired point.  Note that if DuplicateEdges::MERGE
//     // or SiblingPairs::CREATE was used, each crossing has a multiplicity
//     // equal to the number of non-filtered input edges that snapped to that
//     // output edge.
//   }
// }
//
// REQUIRES: The input edges after filtering consist entirely of closed loops.
//           (If DuplicateEdges::MERGE or SiblingPairs::CREATE was used,
//           each graph edge has a multiplicity equal to the number of
//           non-filtered input edges that snapped to it.)
//
// REQUIRES: g.options().edge_type() == DIRECTED
// REQUIRES: g.options().degenerate_edges() == KEEP
// REQUIRES: g.options().sibling_pairs() == {KEEP, REQUIRE, CREATE}
// REQUIRES: builder.options().simplify_edge_chains() == false
//
// CAVEAT: The running time is proportional to the S2Builder::Graph size.  If
//         you need to call this function many times on the same graph then
//         use the alternate version below.  (Most clients only need to call
//         GetSnappedWindingDelta() once per graph because the winding numbers
//         of other points can be computed by counting signed edge crossings.)
int GetSnappedWindingDelta(
    const S2Point& ref_in, S2Builder::Graph::VertexId ref_v,
    const InputEdgeFilter &input_edge_filter, const S2Builder& builder,
    const S2Builder::Graph& g, S2Error* error);

// This version can be used when GetSnappedWindingDelta() needs to be called
// many times on the same graph.  It is faster than the function above, but
// less convenient to use because it requires the client to provide the set of
// graph edges incident to the snapped reference vertex.  It runs in time
// proportional to the size of this set.
//
// "incident_edges" is the set of incoming and outgoing graph edges incident
// to ref_v.  (These edges can be found efficiently using Graph::VertexOutMap
// and Graph::VertexInMap.)
//
// See the function above for the remaining parameters and requirements.
int GetSnappedWindingDelta(
    const S2Point& ref_in, S2Builder::Graph::VertexId ref_v,
    absl::Span<const S2Builder::Graph::EdgeId> incident_edges,
    const InputEdgeFilter &input_edge_filter, const S2Builder& builder,
    const S2Builder::Graph& g, S2Error* error);

// Returns the first vertex of the snapped edge chain for the given input
// edge, or -1 if this input edge does not exist in the graph "g".
S2Builder::Graph::VertexId FindFirstVertexId(
    S2Builder::Graph::InputEdgeId input_edge_id, const S2Builder::Graph& g);

}  // namespace s2builderutil

#endif  // S2_S2BUILDERUTIL_GET_SNAPPED_WINDING_DELTA_H_
