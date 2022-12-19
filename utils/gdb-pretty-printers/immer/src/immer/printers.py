from .ImmerFlexVectorPrinter import ImmerFlexVectorPrinter
from .ImmerRrbTreePrinter import ImmerRrbTreePrinter
import gdb.printing


def build_pretty_printer():
    pp = gdb.printing.RegexpCollectionPrettyPrinter("immer")
    pp.add_printer('flex_vector<T>', "^immer::flex_vector<.*>$", ImmerFlexVectorPrinter)
    pp.add_printer('rrbtree<T>', "^immer::detail::rbts::rrbtree<.*>$", ImmerRrbTreePrinter)
    return pp


def register_immer_printers():
    gdb.printing.register_pretty_printer(
        gdb.current_objfile(),
        build_pretty_printer())
