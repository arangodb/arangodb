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

if __name__ == '__main__':
    unittest.main() 