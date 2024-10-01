// Copyright 2022 Google Inc. All Rights Reserved.
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


#ifndef S2_S2SHAPE_NESTING_QUERY_H_
#define S2_S2SHAPE_NESTING_QUERY_H_

#include <climits>
#include <vector>

#include "absl/container/inlined_vector.h"
#include "s2/s2shape_index.h"

// On a sphere, polygon hierarchy is ambiguous.  If you imagine two chains
// around a sphere at +/- 10 degrees latitude, then we're completely justified
// to consider either one a shell, with the other being a hole in that shell.
//
// Given this ambiguity, we need to specify a strategy to choose a chain to be a
// shell by definition (which we call the datum shell). Other chains are then
// classified relative to that chain.  Often, the first chain in a shape will be
// a shell by construction, so use that as our default strategy.
//
// To specify the datum chain, we just need a function that takes an S2Shape and
// produces a chain id to use.  We use a function pointer to avoid the copying
// issues associated with std::function.  Non-capturing lambdas may still be
// used with a function pointer, but this may change to an `AnyInvocable` in the
// future.
using S2DatumStrategy = int (*)(const S2Shape*);

// `S2ShapeNestingQuery` defines a query to determine the relationships between
// chains in a shape.  Chains are either shells or holes.  Shells have no parent
// and may have zero or more holes associated with them.  Holes belong to a
// single parent shell and have zero holes of their own.  Polygon interiors are
// always on the left of their boundaries, so shells and holes face each other
// in the sense of containing the interior between them.
//
// It's always possible to reverse how we interpret shells and holes due to the
// geometry being on a sphere; so to construct a consistent relationship we need
// to have a strategy to identify a chain to use as a 'datum' shell to which all
// the other chains are classified relatively.  The strategy used for this is
// configurable through the `S2ShapeNestingQueryOptions` and defaults to
// FIRST_CHAIN, selecting the first chain in the shape as the datum.
//
// The `ShapeNesting`() function determines the relationship between shells and
// returns a vector of `ChainRelation` instances.  The `ChainRelation`s are in
// 1:1 correspondence with the chain_id in the shape, e.g. chain 1's relations
// are always given by `ShapeNesting()[1]`.
//
// In the common case of a shell having one or no holes, we store the hole
// relationships efficiently so that no memory allocation is required when
// creating a `ChainRelation`.  Complex geometry may exceed those limits and
// allocate however.
//
// Restrictions:
//   `S2ShapeNestingQuery` is only meaningful for S2ShapeIndex instances
//   containing 2D geometry.
//
//   The query currently doesn't handle any sort of degeneracy in the underlying
//   geometry.
//
class S2ShapeNestingQuery {
 public:
  // Class to model options for the query, passed to `Init()`.
  class Options {
   public:
    // Returns the first chain id in a shape (always zero), used as the
    // default datum strategy.
    static int FirstChain(const S2Shape*) { return 0; }

    Options() : datum_strategy_(FirstChain) {}

    S2DatumStrategy datum_strategy() const { return datum_strategy_; }
    Options& set_datum_strategy(S2DatumStrategy strategy) {
      datum_strategy_ = strategy;
      return *this;
    }

   private:
    S2DatumStrategy datum_strategy_;
  };

  // `ChainRelation` models the parent/child relationship between chains in a
  // shape and chain classification as a shell or a hole.
  class ChainRelation {
   public:
    // Builds a `ChainRelation` that's a shell with given holes.
    static ChainRelation MakeShell(absl::Span<const int32> holes = {}) {
      ChainRelation relation;
      for (int32 chain : holes) {
        relation.AddHole(chain);
      }
      return relation;
    }

    // Builds a `ChainRelation` that's a hole with given parent.
    static ChainRelation MakeHole(int32 parent) {
      return ChainRelation(parent);
    }

    explicit ChainRelation(int32 parent = -1) : parent_(parent) {}

    // Returns the id of the parent chain of the associated chain.  Chains that
    // are shells don't have a parent and have a parent id of -1.
    int32 parent_id() const { return parent_; }

    // Returns true if the associated chain is a shell.  Otherwise the chain
    // is a hole in its parent chain.
    bool is_shell() const { return parent_id() < 0; }

    // Returns true if the associated chain is a hole.  This is true iff the
    // chain is not a shell.
    bool is_hole() const { return !is_shell(); }

    // Return number of holes in the associated chain.
    int num_holes() const { return holes_.size(); }

    // Returns a read only view over the hole ids for the associated chain.
    absl::Span<const int32> holes() const {
      return absl::Span<const int32>(holes_);
    }

   private:
    // So that the query can add Holes to the chain before returning it.
    friend class S2ShapeNestingQuery;

    // `absl::InlinedVector` doesn't round up its inline capacity even if it
    // wouldn't make the object any bigger.  It stores a pointer and a capacity
    // with the inlined data in a union:
    //
    //   [ pointer | capacity ]
    //   [        T[N]        ]
    //
    // We pay for the size of the pointer and capacity regardless, so it makes
    // sense for us to scale the number of reserved elements to take advantage
    // of this.  On 64-bit systems we can store 4 int32s and on 32-bit we can
    // store 2.
    absl::InlinedVector<int32, 2 * sizeof(void*) / sizeof(int32)> holes_;
    int32 parent_;

    // Adds the given id as a hole of this chain.  The `ParentId` of the hole
    // should therefore be the id of this chain as an invariant.
    void AddHole(int32 id) { holes_.push_back(id); }

    // Sets the parent chain id.
    void SetParent(int32 id) { parent_ = id; }

    // Clears the parent of the associated chain (thus marking it as a shell).
    void ClearParent() { parent_ = -1; }
  };  // class ChainRelation

  // Default constructor, requires Init() to be called.
  S2ShapeNestingQuery() = default;

  explicit S2ShapeNestingQuery(const S2ShapeIndex* index,
                               const Options& options = Options());

  const S2ShapeIndex& index() const { return *index_; }
  const Options& options() const { return options_; }
  Options& mutable_options() { return options_; }

  // Equivalent to the two-argument constructor above.
  void Init(const S2ShapeIndex* index, const Options& options = Options());

  // Evaluates the relationships between chains in a shape.  This determines
  // which chains are shells, and which are holes associated with a parent
  // shell.
  //
  // The returned `ChainRelation` instances are in 1:1 correspondence with the
  // chains in the shape, i.e. chain id 3 responds to `result[3]`.
  std::vector<ChainRelation> ComputeShapeNesting(int shape_id);

 private:
  const S2ShapeIndex* index_;
  Options options_;
};

#endif  // S2_S2SHAPE_NESTING_QUERY_H_
