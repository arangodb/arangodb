import collections
from typing import List, Dict, Any, Optional, Tuple

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

    def group_key(self) -> Tuple:
        # For 'Running' tasks, do not group (return unique key)
        if self.state == "Running":
            return (id(self),)
        # For non-'Running', group by name, state, thread name (not ID), and source location
        return (
            self.name,
            self.state,
            self.thread["name"],
            self.source_location["file_name"],
            self.source_location["line"],
            self.source_location["function_name"]
        )

    def __str__(self):
        if self.state == "Running":
            return f"{self.name} [{self.state}] (thread: {self.thread['name']}:{self.thread['LWPID']}) @ {self.source_location['function_name']} ({self.source_location['file_name']}:{self.source_location['line']})"
        else:
            return f"{self.name} [{self.state}] (thread: {self.thread['name']}) @ {self.source_location['function_name']} ({self.source_location['file_name']}:{self.source_location['line']})"

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

    def pretty_print(self, show_deleted: bool = False):
        state_order = ["Running", "Finished", "Deleted"]
        grouped = collections.defaultdict(list)
        for node in self.roots:
            grouped[node.state].append(node)
        for state in state_order:
            if state == "Deleted" and not show_deleted:
                continue
            if grouped[state]:
                print(f"=== {state} Tasks ===")
                if state == "Running":
                    for node in reversed(grouped[state]):
                        self._print_grouped_nodes([node], top_level=True, force_no_group=True)
                else:
                    self._print_grouped_nodes(list(reversed(grouped[state])), top_level=True)
                print()

    def _print_grouped_nodes(self, nodes: List[TaskNode], prefix: str = "", is_last: bool = True, top_level: bool = False, force_no_group: bool = False):
        if force_no_group:
            # Post-order: print children first
            for idx, node in enumerate(reversed(nodes)):
                count = 1
                if node.children:
                    self._print_grouped_nodes(list(reversed(node.children)), prefix + ("   " if (is_last and idx == len(nodes) - 1) else "│  "), True, top_level=False, force_no_group=force_no_group)
                if top_level:
                    count_str = f"{count:3d} x"
                    print(f"{count_str} {str(node)}")
                else:
                    connector = "└─ " if (is_last and idx == len(nodes) - 1) else "├─ "
                    print(prefix + connector + str(node))
            return
        # Group nodes by their group_key
        group_map = collections.defaultdict(list)
        for node in nodes:
            group_map[node.group_key()].append(node)
        group_items = list(group_map.items())
        for idx, (key, group) in enumerate(reversed(group_items)):
            node = group[0]
            count = len(group)
            # Post-order: print children first
            all_children = []
            for n in group:
                all_children.extend(n.children)
            if all_children:
                self._print_grouped_nodes(list(reversed(all_children)), prefix + ("   " if (is_last and idx == len(group_items) - 1) else "│  "), True, top_level=False)
            if top_level:
                count_str = f"{count:3d} x" if count < 1000 else f"{count} x"
                print(f"{count_str} {str(node)}")
            else:
                connector = "└─ " if (is_last and idx == len(group_items) - 1) else "├─ "
                count_str = f" [x{count}]" if count > 1 else ""
                print(prefix + connector + str(node) + count_str) 