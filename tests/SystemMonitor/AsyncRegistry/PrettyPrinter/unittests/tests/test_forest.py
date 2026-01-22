import unittest

from asyncregistry.gdb_forest import Forest, Id
from asyncregistry.gdb_data import PromiseId, Thread
from typing import Iterable

class ForestIsIndexedByChildren(unittest.TestCase):
    def test_empty_forest(self):
        forest = Forest.from_promises([])
        self.assertEqual([a for a in forest.children("0")], [])

    def test_one_root(self):
        forest = Forest.from_promises([
            (PromiseId("0"), Thread(4, 1), "id_0"),
        ])
        self.assertEqual([a for a in forest.children("0")], [])
        self.assertEqual([a for a in forest.children("1")], [])

    def test_one_tree(self):
        forest = Forest.from_promises([
            (PromiseId("1"), PromiseId("0"), "id_1"),
            (PromiseId("0"), Thread(4, 1), "id_0"),
            (PromiseId("2"), PromiseId("0"), "id_2")
        ])
        self.assertEqual([a for a in forest.children("0")], ["1", "2"])
        self.assertEqual([a for a in forest.children("1")], [])
        self.assertEqual([a for a in forest.children("2")], [])

    def test_ignores_non_existing_parents(self):
        forest = Forest.from_promises([
            (PromiseId("1"), PromiseId("0"), "id_1"),
            (PromiseId("2"), PromiseId("0"), "id_2"),
            (PromiseId("3"), PromiseId("1"), "id_3")
        ])
        self.assertEqual([a for a in forest.children("0")], [])
        self.assertEqual([a for a in forest.children("1")], ["3"])
        self.assertEqual([a for a in forest.children("2")], [])
        self.assertEqual([a for a in forest.children("3")], [])
    
class ForestIteratesDepthFirst(unittest.TestCase):
    def test_iterates_over_empty_promises(self):
        forest = Forest.from_promises([])
        self.assertEqual([promise for promise in forest], [])

    def test_gives_nothing_when_forest_has_no_roots(self):
        forest = Forest.from_promises([
            (PromiseId("1"), PromiseId("2"), "id_1"),
            (PromiseId("2"), PromiseId("0"), "id_2"),
            (PromiseId("3"), PromiseId("1"), "id_3")
        ])
        self.assertEqual([promise for promise in forest], [])

    def test_iterates_over_one_promises(self):
        forest = Forest.from_promises([
            (PromiseId("1"), Thread(4, 1), "some_data")
        ])
        self.assertEqual([promise for promise in forest], [
            (0, "1", "some_data")
        ])

    def test_iterates_over_two_roots(self):
        forest = Forest.from_promises([
            (PromiseId("1"), Thread(4, 1), "id_1"),
            (PromiseId("2"), Thread(4, 1), "id_2")
        ])
        self.assertEqual([promise for promise in forest], [
            (0, "1", "id_1"),
            (0, "2", "id_2")
        ])

    def test_iterates_over_one_tree(self):
        forest = Forest.from_promises([
            (PromiseId("1"), PromiseId("0"), "id_1"),
            (PromiseId("0"), Thread(4, 1), "id_0"),
            (PromiseId("2"), PromiseId("0"), "id_2")
        ])
        stacktrace = [promise for promise in forest]
        self.assertEqual(len(stacktrace), 3)
        self.assertTrue(forest_iteration_includes_sequence(forest, [(1, "2", "id_2")]))
        self.assertTrue(forest_iteration_includes_sequence(forest, [(1 ,"1", "id_1")]))
        self.assertEqual(stacktrace[2], (0, "0", "id_0"))

    def test_gives_direct_children_just_before_node(self):
        forest = Forest.from_promises([
            (PromiseId("1"), PromiseId("0"), "id_1"),
            (PromiseId("0"), Thread(4, 1), "id_0"),
            (PromiseId("2"), PromiseId("0"), "id_2"),
            (PromiseId("3"), PromiseId("1"), "id_3")
        ])
        stacktrace = [promise for promise in forest]
        self.assertEqual(len(stacktrace), 4)
        self.assertTrue(forest_iteration_includes_sequence(forest, [(2, "3", "id_3"), (1, "1", "id_1")]))
        self.assertTrue(forest_iteration_includes_sequence(forest, [(1, "2", "id_2")]))
        self.assertEqual(stacktrace[3], (0, "0", "id_0"))

    def test_iterates_over_several_trees(self):
        forest = Forest.from_promises([
            (PromiseId("0"), Thread(4, 1), "id_0"),
            (PromiseId("1"), PromiseId("0"), "id_1"),
            (PromiseId("2"), PromiseId("0"), "id_2"),
            
            (PromiseId("3"), Thread(3, 3), "id_3"),
            (PromiseId("4"), PromiseId("3"), "id_4"),
            (PromiseId("5"), PromiseId("4"), "id_5"),

            (PromiseId("6"), Thread(3, 3), "id_6"),
        ])
        stacktrace = [promise for promise in forest]
        self.assertEqual(len(stacktrace), 7)
        self.assertTrue(
            forest_iteration_includes_sequence(forest, [(1, "2", "id_2"), (1, "1", "id_1"), (0, "0", "id_0")]) or
            forest_iteration_includes_sequence(forest, [(1 ,"1", "id_1"), (1, "2", "id_2"), (0, "0", "id_0")]))
        self.assertTrue(forest_iteration_includes_sequence(forest, [(2, "5", "id_5"), (1, "4", "id_4"), (0, "3", "id_3")]))
        self.assertTrue(forest_iteration_includes_sequence(forest, [(0, "6", "id_6")]))

def forest_iteration_includes_sequence(forest: Forest, sequence: Iterable[tuple[int, Id, str]]): 
    sequence_idx = 0
    previous_matching_promise_idx = None
    promise_idx = 0
    if len(sequence) == 0:
        return True
    for promise in forest:
        if promise == sequence[sequence_idx]:
            sequence_idx += 1
            if previous_matching_promise_idx != None and (promise_idx != previous_matching_promise_idx + 1):
                return False
            previous_matching_promise_idx = promise_idx
            if sequence_idx == len(sequence):
                return True
        promise_idx += 1
    return False


if __name__ == '__main__':
    unittest.main()
