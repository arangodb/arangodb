// Copyright 2005-2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the 'License');
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an 'AS IS' BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// See www.openfst.org for extensive documentation on this weighted
// finite-state transducer library.
//
// Conversions to/from PowerWeight and SparsePowerWeight.

#ifndef FST_POWER_WEIGHT_MAPPERS_H_
#define FST_POWER_WEIGHT_MAPPERS_H_

namespace fst {

// Converts FromWeight to ToPowerWeight (with one component).
// ToPowerWeight may be PowerWeight or SparsePowerWeight.
template <class FromWeight_, class ToPowerWeight>
class ToPowerWeightMapper {
 public:
  using FromWeight = FromWeight_;
  using ToWeight = ToPowerWeight;
  using Index = typename ToPowerWeight::Index;

  explicit ToPowerWeightMapper(Index index = 0) : index_(index) {}

  ToPowerWeight operator()(const FromWeight &w) const {
    return ToPowerWeight(index_, w.Value());
  }

 private:
  const Index index_;
};

// Converts FromPowerWeight to ToWeight. Uses only one component.
// FromPowerWeight may be PowerWeight or SparsePowerWeight.
template <class FromPowerWeight, class ToWeight_>
class FromPowerWeightMapper {
 public:
  using FromWeight = FromPowerWeight;
  using ToWeight = ToWeight_;
  using Index = typename FromPowerWeight::Index;

  explicit FromPowerWeightMapper(Index index = 0) : index_(index) {}

  ToWeight operator()(const FromPowerWeight &w) const {
    return ToWeight(w.Value(index_));
  }

 private:
  const Index index_;
};

// Projects to one dimension of the weight vector, filling the indices
// with `default_weight`.
// PowerWeightT may be PowerWeight or SparsePowerWeight.
template <class PowerWeightT>
class ProjectPowerWeightMapper {
 public:
  using FromWeight = PowerWeightT;
  using ToWeight = PowerWeightT;
  using Index = typename PowerWeightT::Index;
  using ComponentWeight = typename PowerWeightT::Weight;

  explicit ProjectPowerWeightMapper(
      Index from_index = 0, Index to_index = 0,
      const ComponentWeight &default_weight = ComponentWeight::Zero())
      : from_index_(from_index),
        to_index_(to_index),
        default_weight_(default_weight) {}

  PowerWeightT operator()(const PowerWeightT &w) const {
    return PowerWeightT(to_index_, w.Value(from_index_), default_weight_);
  }

 private:
  const Index from_index_;
  const Index to_index_;
  const ComponentWeight default_weight_;
};

// Applies a transformation function to each weight vector.
// PowerWeightT may be PowerWeight or SparsePowerWeight.
template <class PowerWeightT, class TransformFn_>
class TransformPowerWeightMapper {
 public:
  using TransformFn = TransformFn_;
  using FromWeight = PowerWeightT;
  using ToWeight = PowerWeightT;

  explicit TransformPowerWeightMapper(
      const TransformFn &transform = TransformFn())
      : transform_(transform) {}

  PowerWeightT operator()(const PowerWeightT &w) const { return transform_(w); }

 private:
  TransformFn transform_;
};

}  // namespace fst

#endif  // FST_POWER_WEIGHT_MAPPERS_H_
