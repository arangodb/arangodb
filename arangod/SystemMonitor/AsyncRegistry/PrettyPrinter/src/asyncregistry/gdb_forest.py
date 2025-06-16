from typing import Iterable, Any
from asyncregistry.gdb_data import Promise, PromiseId, ThreadInfo

Id = str
class Forest:
    def __init__(self, parents: [Id | None], nodes: [Any], positions: {Id: int}, roots: [Id]):
        self._parents = parents # all parents promise ids
        self._nodes = nodes # all nodes
        self._positions = positions # lookup table for parent and node per promise id
        self._roots = roots
        self._index = None

    def data(self, id: Id) -> Any:
        return self._nodes[self._positions[id]]

    def index(self):
        children = [[] for _ in range(len(self._parents))]
        for id, position in self._positions.items():
            parent = self._parents[position]
            if parent != None and parent in self._positions.keys():
                parent_position = self._positions[parent]
                children[parent_position].append(id)
        self._index = children

    def children(self, id: Id) -> Iterable[Id]:
        if id not in self._positions:
            return None
        position = self._positions[id]
        if position != None and self._index != None:
            for child in self._index[position]:
                yield child

    @classmethod
    def from_promises(cls, promises: Iterable[tuple[PromiseId, ThreadInfo | PromiseId, Any]]):
        parents: [Id] = [] # all parents promise ids
        nodes: [Any] = [] # all nodes
        positions: {Id: int} = {} # lookup table for parent and node per promise id
        roots: [Id] = []

        count = 0
        for (promise, parent, data) in promises:
            promise_id = str(promise.id)
            positions[promise_id] = count
            nodes.append(data)
            if type(parent) == ThreadInfo:
                parents.append(None)
                roots.append(promise_id)
            else:
                parents.append(str(parent.id))
            count += 1

        forest = cls(parents, nodes, positions, roots)
        forest.index()

        return forest


    def dfs(self, start: Id) -> Iterable[tuple[int, Id, Promise]]:
        stack: tuple[Id, int, bool] = [(start, 0, False)]
        while len(stack) != 0:
            (id, hierarchy, hasChildrenProcessed) = stack.pop()
            if hasChildrenProcessed:
                yield (hierarchy, id, self.data(id))
            else:
                stack.append((id, hierarchy, True))
                stack.extend([(child, hierarchy+1, False) for child in self.children(id)])

    def __iter__(self) -> Iterable[tuple[int, Id, Promise]]:
        if (self._index == None):
            return None
        for root in self._roots:
            yield from self.dfs(root)


