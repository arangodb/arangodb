import unittest
import io
import sys
import json
from taskmonitoring.tasktree import TaskTree

SAMPLE_JSON = {
    "task_stacktraces": [
        [
            {
                "hierarchy": 0,
                "data": {
                    "id": "root1",
                    "name": "Top Task 1",
                    "state": "Running",
                    "parent": {},
                    "thread": {"LWPID": 1, "name": "main"},
                    "source_location": {"file_name": "file.cpp", "line": 10, "function_name": "funcA"}
                }
            },
            {
                "hierarchy": 1,
                "data": {
                    "id": "child1",
                    "name": "Child Task 1",
                    "state": "Running",
                    "parent": {"id": "root1"},
                    "thread": {"LWPID": 2, "name": "worker"},
                    "source_location": {"file_name": "file.cpp", "line": 20, "function_name": "funcB"}
                }
            }
        ],
        [
            {
                "hierarchy": 0,
                "data": {
                    "id": "root2",
                    "name": "Top Task 2",
                    "state": "Deleted",
                    "parent": {},
                    "thread": {"LWPID": 3, "name": "main"},
                    "source_location": {"file_name": "file2.cpp", "line": 30, "function_name": "funcC"}
                }
            }
        ]
    ]
}

SAMPLE_GROUP_JSON = {
    "task_stacktraces": [
        [
            {
                "hierarchy": 0,
                "data": {
                    "id": "root1",
                    "name": "Top Task",
                    "state": "Running",
                    "parent": {},
                    "thread": {"LWPID": 1, "name": "main"},
                    "source_location": {"file_name": "file.cpp", "line": 10, "function_name": "funcA"}
                }
            },
            {
                "hierarchy": 0,
                "data": {
                    "id": "root2",
                    "name": "Top Task",
                    "state": "Running",
                    "parent": {},
                    "thread": {"LWPID": 1, "name": "main"},
                    "source_location": {"file_name": "file.cpp", "line": 10, "function_name": "funcA"}
                }
            },
            {
                "hierarchy": 1,
                "data": {
                    "id": "child1",
                    "name": "Child Task",
                    "state": "Running",
                    "parent": {"id": "root1"},
                    "thread": {"LWPID": 2, "name": "worker"},
                    "source_location": {"file_name": "file.cpp", "line": 20, "function_name": "funcB"}
                }
            },
            {
                "hierarchy": 1,
                "data": {
                    "id": "child2",
                    "name": "Child Task",
                    "state": "Running",
                    "parent": {"id": "root2"},
                    "thread": {"LWPID": 2, "name": "worker"},
                    "source_location": {"file_name": "file.cpp", "line": 20, "function_name": "funcB"}
                }
            }
        ]
    ]
}

SAMPLE_GROUP_DIFF_STATE_JSON = {
    "task_stacktraces": [
        [
            {
                "hierarchy": 0,
                "data": {
                    "id": "root1",
                    "name": "Top Task",
                    "state": "Running",
                    "parent": {},
                    "thread": {"LWPID": 1, "name": "main"},
                    "source_location": {"file_name": "file.cpp", "line": 10, "function_name": "funcA"}
                }
            },
            {
                "hierarchy": 0,
                "data": {
                    "id": "root2",
                    "name": "Top Task",
                    "state": "Deleted",
                    "parent": {},
                    "thread": {"LWPID": 1, "name": "main"},
                    "source_location": {"file_name": "file.cpp", "line": 10, "function_name": "funcA"}
                }
            }
        ]
    ]
}

SAMPLE_GROUP_NESTED_JSON = {
    "task_stacktraces": [
        [
            {
                "hierarchy": 0,
                "data": {
                    "id": "root1",
                    "name": "Top Task",
                    "state": "Running",
                    "parent": {},
                    "thread": {"LWPID": 1, "name": "main"},
                    "source_location": {"file_name": "file.cpp", "line": 10, "function_name": "funcA"}
                }
            },
            {
                "hierarchy": 1,
                "data": {
                    "id": "child1",
                    "name": "Child Task",
                    "state": "Running",
                    "parent": {"id": "root1"},
                    "thread": {"LWPID": 2, "name": "worker"},
                    "source_location": {"file_name": "file.cpp", "line": 20, "function_name": "funcB"}
                }
            },
            {
                "hierarchy": 1,
                "data": {
                    "id": "child2",
                    "name": "Child Task",
                    "state": "Running",
                    "parent": {"id": "root1"},
                    "thread": {"LWPID": 2, "name": "worker"},
                    "source_location": {"file_name": "file.cpp", "line": 20, "function_name": "funcB"}
                }
            }
        ]
    ]
}

SAMPLE_REUSED_ID_SIBLINGS = {
    "task_stacktraces": [
        [
            {   # Parent
                "hierarchy": 0,
                "data": {
                    "id": "parent",
                    "name": "Parent",
                    "state": "Running",
                    "parent": {},
                    "thread": {"LWPID": 1, "name": "main"},
                    "source_location": {"file_name": "file.cpp", "line": 1, "function_name": "parentFunc"}
                }
            },
            {   # Child1 (id reused)
                "hierarchy": 1,
                "data": {
                    "id": "child",
                    "name": "Child",
                    "state": "Running",
                    "parent": {"id": "parent"},
                    "thread": {"LWPID": 2, "name": "worker"},
                    "source_location": {"file_name": "file.cpp", "line": 2, "function_name": "childFunc"}
                }
            },
            {   # Child2 (id reused)
                "hierarchy": 1,
                "data": {
                    "id": "child",
                    "name": "Child",
                    "state": "Running",
                    "parent": {"id": "parent"},
                    "thread": {"LWPID": 3, "name": "worker"},
                    "source_location": {"file_name": "file.cpp", "line": 3, "function_name": "childFunc2"}
                }
            }
        ]
    ]
}

SAMPLE_REUSED_ID_COUSINS = {
    "task_stacktraces": [
        [
            {   # Grandparent
                "hierarchy": 0,
                "data": {
                    "id": "grandparent",
                    "name": "Grandparent",
                    "state": "Running",
                    "parent": {},
                    "thread": {"LWPID": 1, "name": "main"},
                    "source_location": {"file_name": "file.cpp", "line": 1, "function_name": "grandparentFunc"}
                }
            },
            {   # Parent1
                "hierarchy": 1,
                "data": {
                    "id": "parent1",
                    "name": "Parent1",
                    "state": "Running",
                    "parent": {"id": "grandparent"},
                    "thread": {"LWPID": 2, "name": "worker"},
                    "source_location": {"file_name": "file.cpp", "line": 2, "function_name": "parentFunc1"}
                }
            },
            {   # Parent2
                "hierarchy": 1,
                "data": {
                    "id": "parent2",
                    "name": "Parent2",
                    "state": "Running",
                    "parent": {"id": "grandparent"},
                    "thread": {"LWPID": 3, "name": "worker"},
                    "source_location": {"file_name": "file.cpp", "line": 3, "function_name": "parentFunc2"}
                }
            },
            {   # Cousin1 (id reused)
                "hierarchy": 2,
                "data": {
                    "id": "cousin",
                    "name": "Cousin",
                    "state": "Running",
                    "parent": {"id": "parent1"},
                    "thread": {"LWPID": 4, "name": "worker"},
                    "source_location": {"file_name": "file.cpp", "line": 4, "function_name": "cousinFunc1"}
                }
            },
            {   # Cousin2 (id reused)
                "hierarchy": 2,
                "data": {
                    "id": "cousin",
                    "name": "Cousin",
                    "state": "Running",
                    "parent": {"id": "parent2"},
                    "thread": {"LWPID": 5, "name": "worker"},
                    "source_location": {"file_name": "file.cpp", "line": 5, "function_name": "cousinFunc2"}
                }
            }
        ]
    ]
}

SAMPLE_REUSED_ID_SEPARATE_TREES = {
    "task_stacktraces": [
        [
            {   # Root1
                "hierarchy": 0,
                "data": {
                    "id": "root",
                    "name": "Root1",
                    "state": "Running",
                    "parent": {},
                    "thread": {"LWPID": 1, "name": "main"},
                    "source_location": {"file_name": "file.cpp", "line": 1, "function_name": "rootFunc1"}
                }
            },
            {   # Root2 (id reused)
                "hierarchy": 0,
                "data": {
                    "id": "root",
                    "name": "Root2",
                    "state": "Running",
                    "parent": {},
                    "thread": {"LWPID": 2, "name": "main"},
                    "source_location": {"file_name": "file.cpp", "line": 2, "function_name": "rootFunc2"}
                }
            }
        ]
    ]
}

SAMPLE_RUNNING_NOT_GROUPED = {
    "task_stacktraces": [
        [
            {
                "hierarchy": 0,
                "data": {
                    "id": "root1",
                    "name": "Top Task",
                    "state": "Running",
                    "parent": {},
                    "thread": {"LWPID": 1, "name": "main"},
                    "source_location": {"file_name": "file.cpp", "line": 10, "function_name": "funcA"}
                }
            },
            {
                "hierarchy": 0,
                "data": {
                    "id": "root2",
                    "name": "Top Task",
                    "state": "Running",
                    "parent": {},
                    "thread": {"LWPID": 2, "name": "main"},
                    "source_location": {"file_name": "file.cpp", "line": 10, "function_name": "funcA"}
                }
            }
        ]
    ]
}

SAMPLE_NON_RUNNING_GROUP_THREAD_NAME = {
    "task_stacktraces": [
        [
            {
                "hierarchy": 0,
                "data": {
                    "id": "root1",
                    "name": "Top Task",
                    "state": "Deleted",
                    "parent": {},
                    "thread": {"LWPID": 1, "name": "main"},
                    "source_location": {"file_name": "file.cpp", "line": 10, "function_name": "funcA"}
                }
            },
            {
                "hierarchy": 0,
                "data": {
                    "id": "root2",
                    "name": "Top Task",
                    "state": "Deleted",
                    "parent": {},
                    "thread": {"LWPID": 2, "name": "main"},
                    "source_location": {"file_name": "file.cpp", "line": 10, "function_name": "funcA"}
                }
            }
        ]
    ]
}

SAMPLE_NON_RUNNING_NOT_GROUP_THREAD_NAME = {
    "task_stacktraces": [
        [
            {
                "hierarchy": 0,
                "data": {
                    "id": "root1",
                    "name": "Top Task",
                    "state": "Deleted",
                    "parent": {},
                    "thread": {"LWPID": 1, "name": "main1"},
                    "source_location": {"file_name": "file.cpp", "line": 10, "function_name": "funcA"}
                }
            },
            {
                "hierarchy": 0,
                "data": {
                    "id": "root2",
                    "name": "Top Task",
                    "state": "Deleted",
                    "parent": {},
                    "thread": {"LWPID": 2, "name": "main2"},
                    "source_location": {"file_name": "file.cpp", "line": 10, "function_name": "funcA"}
                }
            }
        ]
    ]
}

class TestTaskTree(unittest.TestCase):
    def test_hierarchy_and_grouping(self):
        tree = TaskTree.from_json(SAMPLE_JSON["task_stacktraces"])
        self.assertEqual(len(tree.roots), 2)
        running = [n for n in tree.roots if n.state == "Running"]
        deleted = [n for n in tree.roots if n.state == "Deleted"]
        self.assertEqual(len(running), 1)
        self.assertEqual(len(deleted), 1)
        self.assertEqual(running[0].name, "Top Task 1")
        self.assertEqual(deleted[0].name, "Top Task 2")
        self.assertEqual(len(running[0].children), 1)
        self.assertEqual(running[0].children[0].name, "Child Task 1")

    def test_pretty_print_output(self):
        tree = TaskTree.from_json(SAMPLE_JSON["task_stacktraces"])
        captured = io.StringIO()
        sys_stdout = sys.stdout
        sys.stdout = captured
        try:
            tree.pretty_print(show_deleted=True)
        finally:
            sys.stdout = sys_stdout
        output = captured.getvalue()
        self.assertIn("=== Running Tasks ===", output)
        self.assertIn("Top Task 1 [Running]", output)
        self.assertIn("Child Task 1 [Running]", output)
        self.assertIn("=== Deleted Tasks ===", output)
        self.assertIn("Top Task 2 [Deleted]", output)

    def test_grouping_identical_tasks(self):
        tree = TaskTree.from_json(SAMPLE_GROUP_JSON["task_stacktraces"])
        captured = io.StringIO()
        sys_stdout = sys.stdout
        sys.stdout = captured
        try:
            tree.pretty_print()
        finally:
            sys.stdout = sys_stdout
        output = captured.getvalue()
        # Running tasks are not grouped, so expect two separate lines
        self.assertEqual(output.count("Top Task [Running] (thread: main:1) @ funcA (file.cpp:10)"), 2)
        self.assertNotIn("[x2]", output)
        self.assertNotIn("  2 x", output)

    def test_grouping_different_states(self):
        tree = TaskTree.from_json(SAMPLE_GROUP_DIFF_STATE_JSON["task_stacktraces"])
        captured = io.StringIO()
        sys_stdout = sys.stdout
        sys.stdout = captured
        try:
            tree.pretty_print(show_deleted=True)
        finally:
            sys.stdout = sys_stdout
        output = captured.getvalue()
        # Running task: thread ID is printed, not grouped
        self.assertIn("Top Task [Running] (thread: main:1) @ funcA (file.cpp:10)", output)
        # Deleted task: thread ID is NOT printed, not grouped
        self.assertIn("Top Task [Deleted] (thread: main) @ funcA (file.cpp:10)", output)
        self.assertNotIn("[x2]", output)
        self.assertNotIn("  2 x", output)

    def test_grouping_nested(self):
        tree = TaskTree.from_json(SAMPLE_GROUP_NESTED_JSON["task_stacktraces"])
        captured = io.StringIO()
        sys_stdout = sys.stdout
        sys.stdout = captured
        try:
            tree.pretty_print()
        finally:
            sys.stdout = sys_stdout
        output = captured.getvalue()
        # Running children are not grouped, so expect two lines
        self.assertEqual(output.count("Child Task [Running] (thread: worker:2) @ funcB (file.cpp:20)"), 2)
        self.assertNotIn("[x2]", output)
        self.assertNotIn("  2 x", output)
        # Check reverse order: deepest child first, then parent, then root
        idx_child2 = output.find("Child Task [Running] (thread: worker:2) @ funcB (file.cpp:20)")
        idx_root = output.find("Top Task [Running] (thread: main:1) @ funcA (file.cpp:10)")
        if not (idx_child2 < idx_root):
            print("\nDEBUG OUTPUT (test_grouping_nested):\n" + output)
        self.assertTrue(idx_child2 < idx_root, "Deepest child should appear before root in output")

    def test_reverse_ordering_deep_stack(self):
        # Simulate a deep stack
        deep_stack = {
            "task_stacktraces": [[
                {"hierarchy": 0, "data": {"id": "root", "name": "Root", "state": "Running", "parent": {}, "thread": {"LWPID": 1, "name": "main"}, "source_location": {"file_name": "file.cpp", "line": 1, "function_name": "rootFunc"}}},
                {"hierarchy": 1, "data": {"id": "mid", "name": "Mid", "state": "Running", "parent": {"id": "root"}, "thread": {"LWPID": 1, "name": "main"}, "source_location": {"file_name": "file.cpp", "line": 2, "function_name": "midFunc"}}},
                {"hierarchy": 2, "data": {"id": "leaf", "name": "Leaf", "state": "Running", "parent": {"id": "mid"}, "thread": {"LWPID": 1, "name": "main"}, "source_location": {"file_name": "file.cpp", "line": 3, "function_name": "leafFunc"}}}
            ]]
        }
        tree = TaskTree.from_json(deep_stack["task_stacktraces"])
        captured = io.StringIO()
        sys_stdout = sys.stdout
        sys.stdout = captured
        try:
            tree.pretty_print()
        finally:
            sys.stdout = sys_stdout
        output = captured.getvalue()
        idx_leaf = output.find("Leaf [Running] (thread: main:1) @ leafFunc (file.cpp:3)")
        idx_mid = output.find("Mid [Running] (thread: main:1) @ midFunc (file.cpp:2)")
        idx_root = output.find("Root [Running] (thread: main:1) @ rootFunc (file.cpp:1)")
        if not (idx_leaf < idx_mid < idx_root):
            print("\nDEBUG OUTPUT (test_reverse_ordering_deep_stack):\n" + output)
        self.assertTrue(idx_leaf < idx_mid < idx_root, "Order should be leaf, then mid, then root")

    def test_reused_id_siblings(self):
        tree = TaskTree.from_json(SAMPLE_REUSED_ID_SIBLINGS["task_stacktraces"])
        captured = io.StringIO()
        sys_stdout = sys.stdout
        sys.stdout = captured
        try:
            tree.pretty_print()
        finally:
            sys.stdout = sys_stdout
        output = captured.getvalue()
        # Both children should appear under the parent, even though they have the same id
        self.assertIn("Parent [Running]", output)
        self.assertIn("Child [Running] (thread: worker:2)", output)
        self.assertIn("Child [Running] (thread: worker:3)", output)

    def test_reused_id_cousins(self):
        tree = TaskTree.from_json(SAMPLE_REUSED_ID_COUSINS["task_stacktraces"])
        captured = io.StringIO()
        sys_stdout = sys.stdout
        sys.stdout = captured
        try:
            tree.pretty_print()
        finally:
            sys.stdout = sys_stdout
        output = captured.getvalue()
        # Both cousins should appear under their respective parents, even though they have the same id
        self.assertIn("Parent1 [Running]", output)
        self.assertIn("Parent2 [Running]", output)
        self.assertIn("Cousin [Running] (thread: worker:4)", output)
        self.assertIn("Cousin [Running] (thread: worker:5)", output)

    def test_reused_id_separate_trees(self):
        tree = TaskTree.from_json(SAMPLE_REUSED_ID_SEPARATE_TREES["task_stacktraces"])
        captured = io.StringIO()
        sys_stdout = sys.stdout
        sys.stdout = captured
        try:
            tree.pretty_print()
        finally:
            sys.stdout = sys_stdout
        output = captured.getvalue()
        # Both roots should appear, even though they have the same id
        self.assertIn("Root1 [Running] (thread: main:1)", output)
        self.assertIn("Root2 [Running] (thread: main:2)", output)

    def test_running_not_grouped(self):
        tree = TaskTree.from_json(SAMPLE_RUNNING_NOT_GROUPED["task_stacktraces"])
        captured = io.StringIO()
        sys_stdout = sys.stdout
        sys.stdout = captured
        try:
            tree.pretty_print()
        finally:
            sys.stdout = sys_stdout
        output = captured.getvalue()
        # Both should appear, not grouped, and thread ID is printed
        self.assertIn("Top Task [Running] (thread: main:1)", output)
        self.assertIn("Top Task [Running] (thread: main:2)", output)
        self.assertNotIn("[x2]", output)
        self.assertNotIn("  2 x", output)

    def test_deleted_tasks_hidden_by_default(self):
        tree = TaskTree.from_json(SAMPLE_NON_RUNNING_GROUP_THREAD_NAME["task_stacktraces"])
        captured = io.StringIO()
        sys_stdout = sys.stdout
        sys.stdout = captured
        try:
            tree.pretty_print(show_deleted=False)
        finally:
            sys.stdout = sys_stdout
        output = captured.getvalue()
        # Deleted tasks should not be printed
        self.assertNotIn("Deleted Tasks", output)
        self.assertNotIn("Top Task [Deleted]", output)

    def test_deleted_tasks_shown_with_flag(self):
        tree = TaskTree.from_json(SAMPLE_NON_RUNNING_GROUP_THREAD_NAME["task_stacktraces"])
        captured = io.StringIO()
        sys_stdout = sys.stdout
        sys.stdout = captured
        try:
            tree.pretty_print(show_deleted=True)
        finally:
            sys.stdout = sys_stdout
        output = captured.getvalue()
        # Deleted tasks should be printed
        self.assertIn("=== Deleted Tasks ===", output)
        self.assertIn("Top Task [Deleted]", output)

    def test_non_running_group_by_thread_name(self):
        tree = TaskTree.from_json(SAMPLE_NON_RUNNING_GROUP_THREAD_NAME["task_stacktraces"])
        captured = io.StringIO()
        sys_stdout = sys.stdout
        sys.stdout = captured
        try:
            tree.pretty_print(show_deleted=True)
        finally:
            sys.stdout = sys_stdout
        output = captured.getvalue()
        # Should be grouped, thread ID not printed
        self.assertIn("  2 x Top Task [Deleted] (thread: main) @ funcA (file.cpp:10)", output)
        self.assertNotIn(":1)", output)
        self.assertNotIn(":2)", output)

    def test_non_running_not_group_if_thread_name_differs(self):
        tree = TaskTree.from_json(SAMPLE_NON_RUNNING_NOT_GROUP_THREAD_NAME["task_stacktraces"])
        captured = io.StringIO()
        sys_stdout = sys.stdout
        sys.stdout = captured
        try:
            tree.pretty_print(show_deleted=True)
        finally:
            sys.stdout = sys_stdout
        output = captured.getvalue()
        # Should not be grouped, thread ID not printed
        self.assertIn("Top Task [Deleted] (thread: main1) @ funcA (file.cpp:10)", output)
        self.assertIn("Top Task [Deleted] (thread: main2) @ funcA (file.cpp:10)", output)
        self.assertNotIn("[x2]", output)
        self.assertNotIn("  2 x", output)
        self.assertNotIn(":1)", output)
        self.assertNotIn(":2)", output)

if __name__ == '__main__':
    unittest.main() 