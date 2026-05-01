#!/usr/bin/env python3

# import clang.cindex

# # Configure the path to your libclang if it's not in your PATH
# clang.cindex.Config.set_library_path('/usr/lib/llvm-18/lib')

# def find_variables_of_type(file_path, target_type_name):
#     index = clang.cindex.Index.create()
    
#     # Parse the source file
#     # "-std=c++20" is used here to support modern features
#     tu = index.parse(file_path, args=['-std=c++20'])

#     found_vars = []

#     def walk_ast(node):
#         # We are looking for Variable Declarations
#         if node.kind == clang.cindex.CursorKind.VAR_DECL:
#             # Check if the type name matches our target
#             if target_type_name in node.type.spelling:
#                 found_vars.append({
#                     "name": node.spelling,
#                     "line": node.location.line,
#                     "file": node.location.file.name
#                 })
        
#         # Recurse through children
#         for child in node.get_children():
#             walk_ast(child)

#     walk_ast(tu.cursor)
#     return found_vars

# # --- Example Usage ---
# target = "ApiVersion"
# results = find_variables_of_type("/home/jvolmer/code/arangodb/arangod/RocksDBEngine/RocksDBDumpContext.cpp", target)

# print(f"Variables of type '{target}':")
# for var in results:
#     print(f"  - {var['name']} (Line {var['line']} in {var['file']})")

#=============================

# import clang.cindex
# import os

# # For LLVM 18, the library is typically here:
# lib_file = '/usr/lib/llvm-18/lib/libclang-18.so.1'

# if os.path.exists(lib_file):
#     clang.cindex.Config.set_library_file(lib_file)
# else:
#     # Fallback search if the path differs slightly
#     print("Searching for libclang.so.1...")
#     # This is a common path on some distros
#     clang.cindex.Config.set_library_file('/usr/lib/x86_64-linux-gnu/libclang-18.so.1')

# def find_variables(file_path, target_type):
#     index = clang.cindex.Index.create()
    
#     # We use PARSE_SKIP_FUNCTION_BODIES to reduce the chance of 
#     # hitting complex templates inside standard library functions.
#     tu = index.parse(file_path, args=['-std=c++20'], 
#                      options=clang.cindex.TranslationUnit.PARSE_SKIP_FUNCTION_BODIES)

#     def walk_ast(node):
#         try:
#             # We check for VAR_DECL (Variable Declarations)
#             if node.kind == clang.cindex.CursorKind.VAR_DECL:
#                 # We check the type name
#                 if target_type in node.type.spelling:
#                     print(f"Found {target_type} variable: '{node.spelling}' at line {node.location.line}")
            
#             for child in node.get_children():
#                 walk_ast(child)
#         except ValueError as e:
#             # This catches the 'Unknown kind' error specifically for a node
#             # and allows the script to keep running instead of crashing.
#             pass

#     walk_ast(tu.cursor)

# target = "std::string"
# results = find_variables("main.cpp", target)

# # print(f"Variables of type '{target}':")
# # for var in results:
# #     print(f"  - {var['name']} (Line {var['line']} in {var['file']})")

import argparse
import json
import sys
from clang.cindex import CompilationDatabase, CursorKind, Index, TranslationUnit

VAR_KINDS = {
    CursorKind.VAR_DECL,
    CursorKind.FIELD_DECL,
    CursorKind.PARM_DECL,
}

def type_matches(cursor_type, target):
    """Match on the underlying record name, ignoring const/&/* and typedefs."""
    t = cursor_type.get_canonical()
    decl = t.get_declaration()
    if decl and decl.spelling == target:
        return True
    # Fallback: textual match, catches builtins like 'int'
    return t.spelling == target

def walk(cursor, target, hits):
    if cursor.kind in VAR_KINDS and type_matches(cursor.type, target):
        loc = cursor.location
        if loc.file and "/usr/" not in loc.file.name:   # skip system headers
            hits.append({
                "file": loc.file.name,
                "line": loc.line,
                "col":  loc.column,
                "name": cursor.spelling,
                "type": cursor.type.spelling,
                "kind": cursor.kind.name,
            })
    for child in cursor.get_children():
        walk(child, target, hits)

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("-p", "--build-dir", required=True,
                    help="directory containing compile_commands.json")
    ap.add_argument("--type", required=True, help="type name to search for")
    ap.add_argument("files", nargs="*", help="files to scan (default: all)")
    args = ap.parse_args()

    db = CompilationDatabase.fromDirectory(args.build_dir)
    index = Index.create()
    hits = []

    commands = db.getAllCompileCommands() if not args.files else [
        cmd for f in args.files for cmd in db.getCompileCommands(f) or []
    ]

    for cmd in commands:
        # Drop the compiler name and the source file from the args list.
        cargs = [a for a in list(cmd.arguments)[1:-1]]
        tu = index.parse(cmd.filename, args=cargs,
                         options=TranslationUnit.PARSE_SKIP_FUNCTION_BODIES
                                 if False else 0)
        if not tu:
            print(f"parse failed: {cmd.filename}", file=sys.stderr)
            continue
        walk(tu.cursor, args.type, hits)

    json.dump(hits, sys.stdout, indent=2)
    print()

if __name__ == "__main__":
    main()
