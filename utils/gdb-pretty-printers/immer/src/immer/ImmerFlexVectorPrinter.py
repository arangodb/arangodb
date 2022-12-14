from . import ImmerRrbTreePrinter


class ImmerFlexVectorPrinter:
    """Print an immer::flex_vector<T>."""

    def __init__(self, val):
        self.val = val
        # TODO Writing out all template arguments doesn't seem helpful, but it would be better to rely on
        #      rrbtree.type as the basis of this, instead of a hard-coded string.
        self.typename = 'immer::flex_vector'
        self.rrbtree = ImmerRrbTreePrinter(val['impl_'])
        self.bits = val.type.template_argument(2)
        self.bits_leaf = val.type.template_argument(3)

    def to_string(self):
        return f'{self.typename} of length {self.rrbtree.size}'

    def children(self):
        return self.rrbtree.children()

    def display_hint(self):
        return 'array'
