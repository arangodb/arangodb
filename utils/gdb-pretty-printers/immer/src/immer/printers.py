# TODO use separate modules

class ImmerRrbTreeNode:
    def __init__(self, node):
        self.node = node
        self.impl = node['impl']
        self.value_type = node.type.template_argument(0)
        self.value_pointer_type = self.value_type.pointer()

    def relaxed(self):
        return self.impl['d']['data']['inner']['relaxed']

    def leaf(self):
        buffer = self.impl['d']['data']['leaf']['buffer']
        return buffer.address.reinterpret_cast(self.value_pointer_type)

    def inner(self):
        buffer = self.impl['d']['data']['inner']['buffer']
        return buffer.address.reinterpret_cast(self.node.type.pointer().pointer())


class ImmerRrbTreePrinter:
    """Print an immer::detail::rbts::rrbtree<T>."""

    def __init__(self, rrbtree):
        self.rrbtree = rrbtree
        # TODO Writing out all template arguments doesn't seem helpful, but it would be better to rely on
        #      rrbtree.type as the basis of this, instead of a hard-coded string.
        self.typename = 'immer::detail::rbts::rrbtree'
        self.size = rrbtree['size']
        assert self.size >= 0
        self.shift = rrbtree['shift']
        self.root = ImmerRrbTreeNode(rrbtree['root'].dereference())
        self.tail = ImmerRrbTreeNode(rrbtree['tail'].dereference())
        self.bits = rrbtree.type.template_argument(2)
        self.bits_leaf = rrbtree.type.template_argument(3)

    def mask(self, bits):
        return (1 << bits) - 1

    def tail_offset(self):
        r = self.root.relaxed()
        assert r == 0x0 or r.dereference()['d']['count']
        if r != 0x0:
            #  r->d.sizes[r->d.count - 1]
            d = r.dereference()['d']
            sizes = d['sizes']
            count = d['count']
            return sizes[count - 1]
        else:
            if self.size > 0:
                # (size - 1) & ~mask<BL>
                return (self.size - 1) & ~self.mask(self.bits_leaf)
            else:
                # TODO This branch is not covered by any test yet
                return 0

    def _get_leaf(self, node, idx):
        # pos.node()->leaf()[pos.index(idx)];
        return (node.leaf() + (idx & self.mask(self.bits_leaf))).dereference()

    def _get_inner(self, node, idx):
        # visit_maybe_relaxed_descent(root, shift, v, idx)
        assert node != 0x0
        assert self.shift >= self.bits_leaf
        r = node.relaxed()
        if r != 0x0:
            # return relaxed_descent_pos<NodeT, Shift>{node, r}.visit(v, idx)
            return self._get_inner_relaxed(node, r, self.shift, idx)
        else:
            # return visit_regular_descent(node, shift, v, idx);
            assert self.shift >= self.bits_leaf
            return self._get_inner_regular(node, self.shift, idx)

    def _get_inner_relaxed(self, node, relaxed, shift, idx):
        # relaxed_descent_pos<NodeT, Shift>{node, r}.visit(v, idx)
        if shift != self.bits_leaf:
            # descent to inner node
            assert self.shift > 0
            sizes = relaxed.dereference()['d']['sizes']
            offset = idx >> shift
            while (sizes[offset] <= idx):
                offset = offset + 1
            child = ImmerRrbTreeNode((node.inner() + offset).dereference().dereference())
            left_size = sizes[offset - 1] if offset > 0 else 0
            next_idx = idx - left_size
            r = child.relaxed()
            if r != 0x0:
                # relaxed_descent_pos<NodeT, Shift - B>{child, r}.visit(v, next_idx)
                return self._get_inner_relaxed(child, r, shift - self.bits, next_idx)
            else:
                # regular_descent_pos<NodeT, Shift - B>{child}.visit(v, next_idx)
                return self._get_inner_regular(child, shift - self.bits, next_idx)
        else:
            # descent to leaf
            # (idx >> BL) & mask<B>
            offset = (idx >> self.bits_leaf) & self.mask(self.bits)
            sizes = relaxed.dereference()['d']['sizes']
            while sizes[offset] <= idx:
                offset = offset + 1
            child = ImmerRrbTreeNode((node.inner() + offset).dereference().dereference())
            left_size = sizes[offset - 1] if offset > 0 else 0
            next_idx = idx - left_size
            # leaf_descent_pos<NodeT>{child}.visit(v, next_idx)
            return self._get_leaf(child, next_idx)

    def _get_inner_regular(self, node, shift, idx):
        assert node != 0x0
        # regular_descent_pos<NodeT, Shift>{node}.visit(v, idx)
        if shift != self.bits_leaf:
            # descent to inner node
            assert shift > 0
            # (idx >> Shift) & mask<B>
            offset = (idx >> shift) & self.mask(self.bits)
            # child  = node_->inner()[offset]
            child = ImmerRrbTreeNode((node.inner() + offset).dereference().dereference())
            # regular_descent_pos<NodeT, Shift - B>{child}.visit(v, idx)
            return self._get_inner_regular(child, shift - self.bits, idx)
        else:
            # descent to leaf
            # (idx >> BL) & mask<B>
            offset = (idx >> self.bits_leaf) & self.mask(self.bits)
            # child  = node_->inner()[offset]
            child = ImmerRrbTreeNode((node.inner() + offset).dereference().dereference())
            # make_leaf_descent_pos(child).visit(v, idx)
            return self._get_leaf(child, idx)

    def get(self, idx):
        tail_off = self.tail_offset()
        if idx >= tail_off:
            # make_leaf_descent_pos(tail).visit(v, idx - tail_off)
            return self._get_leaf(self.tail, idx - tail_off)
        else:
            # visit_maybe_relaxed_descent(root, shift, v, idx)
            return self._get_inner(self.root, idx)

    def to_string(self):
        return f'{self.typename} of length {self.size}'

    def children(self):
        for i in range(0, self.size):
            yield (f'[{i}]', self.get(i))

    def display_hint(self):
        return 'array'


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
