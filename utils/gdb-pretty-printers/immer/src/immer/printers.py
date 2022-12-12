import re
import traceback


# {
#   static bits = 5,
#   static bits_leaf = 3,
#   static embed_relaxed = false,
#   impl = {
#       n = {
#           <immer::detail::csl::inherit<immer::no_transience_policy::apply<immer::free_list_heap_policy<immer::cpp_heap, 1024ul> >::type::ownee, void>::type> = {<immer::no_transience_policy::apply<immer::free_list_heap_policy<immer::cpp_heap, 1024ul> >::type::ownee> = {<No data fields>}, <No data fields>},
#           d = {refcount = std::atomic<int> = { 2 }}
#       },
#       d = {data = {inner = {relaxed = 0x0, buffer = {__data = "\000\000\000\000\000\000\000", __align = {<No data
#       fields>}}},
#       leaf = {buffer = {__data = '\000' <repeats 31 times>, __align = {<No data fields>}}}}}},
#       static keep_headroom = <optimized out>,
#       static max_sizeof_leaf = <optimized out>,
#       static max_sizeof_inner = 272,
#       static max_sizeof_relaxed = <optimized out>,
#       static max_sizeof_inner_r = <optimized out>}
class Node:
    def __init__(self, node):
        self.node = node
        self.impl = node['impl']
        self.value_type = node.type.template_argument(0)
        self.value_pointer_type = self.value_type.pointer()
        # help(node)

    def relaxed(self):
        return self.impl['d']['data']['inner']['relaxed']

    """
    T* leaf()
    {
        IMMER_ASSERT_TAGGED(kind() == kind_t::leaf);
        return reinterpret_cast<T*>(&impl.d.data.leaf.buffer);
    }
    """

    def leaf(self):
        buffer = self.impl['d']['data']['leaf']['buffer']
        return buffer.address.reinterpret_cast(self.value_pointer_type)

    def inner(self):
        buffer = self.impl['d']['data']['inner']['buffer']
        return buffer.address.reinterpret_cast(self.node.type.pointer().pointer())

    # def to_string(self):
    #     print(f'Node<{self.value_type}>@{self.node.address}')


class ImmerRrbTreePrinter:
    """Print an immer::detail::rbts::rrbtree<T>."""

    def __init__(self, rrbtree):
        self.rrbtree = rrbtree
        self.size = rrbtree['size']
        assert self.size >= 0
        self.shift = rrbtree['shift']
        self.root = Node(rrbtree['root'].dereference())
        self.tail = Node(rrbtree['tail'].dereference())
        self.bits = rrbtree.type.template_argument(2)
        self.bits_leaf = rrbtree.type.template_argument(3)

    def mask(self, bits):
        return (1 << bits) - 1

    """
        auto tail_offset() const
        {
            auto r = root->relaxed();
            assert(r == nullptr || r->d.count);
            return r      ? r->d.sizes[r->d.count - 1]
                   : size ? (size - 1) & ~mask<BL>
                          /* otherwise */
                          : 0;
        }
    """

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
            child = Node((node.inner() + offset).dereference().dereference())
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
            child = Node((node.inner() + offset).dereference().dereference())
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
            child = Node((node.inner() + offset).dereference().dereference())
            # regular_descent_pos<NodeT, Shift - B>{child}.visit(v, idx)
            return self._get_inner_regular(child, shift - self.bits, idx)
        else:
            # descent to leaf
            # (idx >> BL) & mask<B>
            offset = (idx >> self.bits_leaf) & self.mask(self.bits)
            # child  = node_->inner()[offset]
            child = Node((node.inner() + offset).dereference().dereference())
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
        s = f'rrbtree({self.size})['
        for i in range(0, self.size):
            s += str(self.get(i))
            s += ", "
        s += "]"
        return s


# immer::flex_vector<std::string>({"hello, world"s})
#  ===
# $1 = {static bits = 5, static bits_leaf = 3,; impl_ = {size = 1, shift = 3, root = 0x5555561291d8, tail = 0x555556129418, static supports_transient_concat = <optimized out>}}
class ImmerFlexVectorPrinter:
    """Print an immer::flex_vector<T>."""

    def __init__(self, val):
        self.val = val
        self.typename = val.type.name
        self.rrbtree = ImmerRrbTreePrinter(val['impl_'])
        self.bits = val.type.template_argument(2)
        self.bits_leaf = val.type.template_argument(3)
        # static bits = 5, static bits_leaf = 3,

    def to_string(self):
        try:
            return ("flex_vector{" + self.rrbtree.to_string() + "}")
        except Exception as err:
            traceback.print_exc()
            raise


import gdb.printing


def build_pretty_printer():
    pp = gdb.printing.RegexpCollectionPrettyPrinter("immer")
    # pp.add_printer('foo', '^foo$', fooPrinter)
    # pp.add_printer('bar', '^bar$', barPrinter)
    pp.add_printer('flex_vector<T>', "^immer::flex_vector<.*>$", ImmerFlexVectorPrinter)
    pp.add_printer('rrbtree<T>', "^immer::detail::rbts::rrbtree<.*>$", ImmerRrbTreePrinter)
    return pp


def register_immer_printers():
    gdb.printing.register_pretty_printer(
        gdb.current_objfile(),
        build_pretty_printer())
