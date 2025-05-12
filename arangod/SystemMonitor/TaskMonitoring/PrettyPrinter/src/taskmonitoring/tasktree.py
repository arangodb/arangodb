import collections
from typing import List, Dict, Any, Optional

class TaskNode:
    def __init__(self, data: dict, hierarchy: int):
        self.id = data["id"]
        self.name = data["name"]
        self.state = data["state"]
        self.parent_id = data["parent"].get("id") if data["parent"] else None
        self.thread = data["thread"]
        self.source_location = data["source_location"]
        self.hierarchy = hierarchy
        self.children: List['TaskNode'] = []

    def add_child(self, child: 'TaskNode'):
        self.children.append(child)

    def __str__(self):
        return f"{self.name} [{self.state}] (thread: {self.thread['name']}:{self.thread['LWPID']}) @ {self.source_location['function_name']} ({self.source_location['file_name']}:{self.source_location['line']})"

class TaskTree:
    def __init__(self, roots: List[TaskNode]):
        self.roots = roots

    @staticmethod
    def from_json(task_stacktraces: List[List[Dict[str, Any]]]) -> 'TaskTree':
        # Flatten all tasks and build id->node mapping
        nodes = {}
        all_nodes = []
        for stack in task_stacktraces:
            for entry in stack:
                node = TaskNode(entry["data"], entry["hierarchy"])
                nodes[node.id] = node
                all_nodes.append(node)
        # Build hierarchy
        roots = []
        for node in all_nodes:
            if node.parent_id and node.parent_id in nodes:
                nodes[node.parent_id].add_child(node)
            else:
                roots.append(node)
        return TaskTree(roots)

    def pretty_print(self):
        # Group by state: Running, Finished, Deleted (in this order)
        state_order = ["Running", "Finished", "Deleted"]
        grouped = collections.defaultdict(list)
        for node in self.roots:
            grouped[node.state].append(node)
        for state in state_order:
            if grouped[state]:
                print(f"=== {state} Tasks ===")
                for node in grouped[state]:
                    self._print_node(node)
                print()

    def _print_node(self, node: TaskNode, prefix: str = "", is_last: bool = True):
        connector = "└─ " if is_last else "├─ "
        print(prefix + connector + str(node))
        if node.children:
            for i, child in enumerate(node.children):
                last = (i == len(node.children) - 1)
                self._print_node(child, prefix + ("   " if is_last else "│  "), last) 