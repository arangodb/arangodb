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
            tree.pretty_print()
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
        # Top Task should be grouped (x2), Child Task should be grouped (x2)
        self.assertIn("Top Task [Running] (thread: main:1) @ funcA (file.cpp:10) [x2]", output)
        self.assertIn("Child Task [Running] (thread: worker:2) @ funcB (file.cpp:20) [x2]", output)

    def test_grouping_different_states(self):
        tree = TaskTree.from_json(SAMPLE_GROUP_DIFF_STATE_JSON["task_stacktraces"])
        captured = io.StringIO()
        sys_stdout = sys.stdout
        sys.stdout = captured
        try:
            tree.pretty_print()
        finally:
            sys.stdout = sys_stdout
        output = captured.getvalue()
        # Should not group across states
        self.assertIn("Top Task [Running] (thread: main:1) @ funcA (file.cpp:10)", output)
        self.assertIn("Top Task [Deleted] (thread: main:1) @ funcA (file.cpp:10)", output)
        self.assertNotIn("[x2]", output)

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
        # Child Task should be grouped (x2) under Top Task
        self.assertIn("Child Task [Running] (thread: worker:2) @ funcB (file.cpp:20) [x2]", output)

if __name__ == '__main__':
    unittest.main() 