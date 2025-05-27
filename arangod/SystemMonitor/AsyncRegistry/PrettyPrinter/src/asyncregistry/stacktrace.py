from typing import Optional, Any

def branching_symbol(hierarchy:int, previous_hierarchy: Optional[int]) -> str:
    if hierarchy == previous_hierarchy:
        return "├"
    else:
        return "┌"
    
def branch_ascii(hierarchy: int, continuations: list) -> str:
    ascii = [" " for x in range(2*hierarchy+2)] + [branching_symbol(hierarchy, continuations[-1] if len(continuations) > 0 else None)] + [" "]
    for continuation in continuations:
        if continuation < hierarchy:
            ascii[2*continuation+2] = "│"
    return ''.join(ascii)
    
class Stacktrace(object):
    def __init__(self):
        self.lines = []
        self.stack = [] # hierarchy stack, includes hierarchies that are not yet finished
        
    def append(self, hierarchy: int, promise: Any) -> Optional[list[str, Any]]:
        # pop finished hierarchy
        if len(self.stack) > 0 and hierarchy < self.stack[-1]:
            self.stack.pop()
            
        ascii = branch_ascii(hierarchy, self.stack)
            
        # push current but not if already on stack
        if len(self.stack) == 0 or hierarchy != self.stack[-1]:
            self.stack.append(hierarchy)

        self.lines.append((ascii, promise))

        if (hierarchy == 0):
            return self.lines

        return None
