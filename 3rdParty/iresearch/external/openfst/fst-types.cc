// Registration of common Fst and arc types.

#include <fst/arc.h>
#include <fst/compact-fst.h>
#include <fst/const-fst.h>
#include <fst/edit-fst.h>
#include <fst/register.h>
#include <fst/vector-fst.h>

namespace fst {

// Registers VectorFst, ConstFst and EditFst for common arcs types.
REGISTER_FST(VectorFst, StdArc);
REGISTER_FST(VectorFst, LogArc);
REGISTER_FST(VectorFst, Log64Arc);
REGISTER_FST(ConstFst, StdArc);
REGISTER_FST(ConstFst, LogArc);
REGISTER_FST(ConstFst, Log64Arc);
REGISTER_FST(EditFst, StdArc);
REGISTER_FST(EditFst, LogArc);
REGISTER_FST(EditFst, Log64Arc);

// Register CompactFst for common arcs with the default (uint32) size type
static FstRegisterer<
  CompactFst<StdArc, StringCompactor<StdArc> > >
CompactFst_StdArc_StringCompactor_registerer;
static FstRegisterer<
  CompactFst<LogArc, StringCompactor<LogArc> > >
CompactFst_LogArc_StringCompactor_registerer;
static FstRegisterer<
  CompactFst<StdArc, WeightedStringCompactor<StdArc> > >
CompactFst_StdArc_WeightedStringCompactor_registerer;
static FstRegisterer<
  CompactFst<LogArc, WeightedStringCompactor<LogArc> > >
CompactFst_LogArc_WeightedStringCompactor_registerer;
static FstRegisterer<
  CompactFst<StdArc, AcceptorCompactor<StdArc> > >
CompactFst_StdArc_AcceptorCompactor_registerer;
static FstRegisterer<
  CompactFst<LogArc, AcceptorCompactor<LogArc> > >
CompactFst_LogArc_AcceptorCompactor_registerer;
static FstRegisterer<
  CompactFst<StdArc, UnweightedCompactor<StdArc> > >
CompactFst_StdArc_UnweightedCompactor_registerer;
static FstRegisterer<
  CompactFst<LogArc, UnweightedCompactor<LogArc> > >
CompactFst_LogArc_UnweightedCompactor_registerer;
static FstRegisterer<
  CompactFst<StdArc, UnweightedAcceptorCompactor<StdArc> > >
CompactFst_StdArc_UnweightedAcceptorCompactor_registerer;
static FstRegisterer<
  CompactFst<LogArc, UnweightedAcceptorCompactor<LogArc> > >
CompactFst_LogArc_UnweightedAcceptorCompactor_registerer;

}  // namespace fst
