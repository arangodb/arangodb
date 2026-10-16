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
// Registration of common FST and arc types.

#include <fst/arc.h>
#include <fst/compact-fst.h>
#include <fst/const-fst.h>
#include <fst/edit-fst.h>
#include <fst/register.h>
#include <fst/vector-fst.h>

namespace fst {

REGISTER_FST(VectorFst, StdArc);
REGISTER_FST(VectorFst, LogArc);
REGISTER_FST(VectorFst, Log64Arc);

REGISTER_FST(ConstFst, StdArc);
REGISTER_FST(ConstFst, LogArc);
REGISTER_FST(ConstFst, Log64Arc);

REGISTER_FST(EditFst, StdArc);
REGISTER_FST(EditFst, LogArc);
REGISTER_FST(EditFst, Log64Arc);

REGISTER_FST(CompactStringFst, StdArc);
REGISTER_FST(CompactStringFst, LogArc);
REGISTER_FST(CompactStringFst, Log64Arc);

REGISTER_FST(CompactWeightedStringFst, StdArc);
REGISTER_FST(CompactWeightedStringFst, LogArc);
REGISTER_FST(CompactWeightedStringFst, Log64Arc);

REGISTER_FST(CompactAcceptorFst, StdArc);
REGISTER_FST(CompactAcceptorFst, LogArc);
REGISTER_FST(CompactAcceptorFst, Log64Arc);

REGISTER_FST(CompactUnweightedFst, StdArc);
REGISTER_FST(CompactUnweightedFst, LogArc);
REGISTER_FST(CompactUnweightedFst, Log64Arc);

REGISTER_FST(CompactUnweightedAcceptorFst, StdArc);
REGISTER_FST(CompactUnweightedAcceptorFst, LogArc);
REGISTER_FST(CompactUnweightedAcceptorFst, Log64Arc);

}  // namespace fst
