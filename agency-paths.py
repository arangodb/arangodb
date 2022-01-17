#!/usr/bin/python3

import xml.etree.ElementTree as ET
import sys


class PathNode:
    def __init__(self, xml, parent=None):
        self.tag = xml.tag
        self.parent = parent
        self.children = [PathNode(c, self) for c in xml]
        self.param = xml.attrib.get('param', None)

    def is_dynamic_component(self):
        return self.param is not None

    def is_static_component(self):
        return not self.is_dynamic_component()

    def __iter__(self):
        return self.children.__iter__()

    def get_class_name(self):
        return self.tag[0].upper() + self.tag[1:]

    def get_function_name(self):
        return self.tag[0].lower() + self.tag[1:]

    def get_component_name(self):
        return self.tag

    def get_recursive_class_name(self):
        return self.parent.get_recursive_class_name() + "::" + self.get_class_name()


class RootNode:
    def get_recursive_class_name(self):
        return self.get_class_name()

    def get_class_name(self):
        return "Root"


def generate_node_inline(out, node, prefix):
    cls = node.get_class_name()
    if node.is_static_component():
        out(f"class {cls} : public StaticComponent<{cls}, {node.parent.get_class_name()}> {{")
    else:
        out(f"class {cls} : public DynamicComponent<{cls}, {node.parent.get_class_name()}, std::string> {{")
    out("public:")

    if node.is_static_component():
        out(f"constexpr char const* component() const noexcept {{ return \"{node.get_component_name()}\"; }}")
        out("using BaseType::StaticComponent;")
    else:
        out(f"char const* component() const noexcept {{ return value().c_str(); }}")
        out("using BaseType::DynamicComponent;")

    for child in node:
        generate_node_inline(out, child)
    out("};")

    if node.is_static_component():
        out(f"std::shared_ptr<{cls} const> {node.get_function_name()}() const {{")
        out(f"return {cls}::make_shared(shared_from_this());")
    else:
        out(f"std::shared_ptr<{cls} const> {node.get_function_name()}({node.param} const& p) const {{")
        out(f"return {cls}::make_shared(shared_from_this(), PathParamConverter<{node.param}>{{}}(p));")
    out("}")

def generate_node_header(out, node, prefix, level = 0):
    cls = node.get_class_name()
    if node.is_static_component():
        out(f"class {cls} : public StaticComponent<{cls}, {node.parent.get_class_name()}> {{")
    else:
        out(f"class {cls} : public DynamicComponent<{cls}, {node.parent.get_class_name()}, std::string> {{")
    out("public:")

    if node.is_static_component():
        out(f"char const* component() const noexcept;")
        out("using BaseType::StaticComponent;")
    else:
        out(f"char const* component() const noexcept;")
        out("using BaseType::DynamicComponent;")

    for child in node:
        generate_node_header(out, child, prefix, level + 1)
    out("};")

    if node.is_static_component():
        out(f"std::shared_ptr<{cls} const> {node.get_function_name()}() const;")
    else:
        out(f"std::shared_ptr<{cls} const> {node.get_function_name()}({node.param} const& p) const;")
        if node.param != "std::string":
            out(f"std::shared_ptr<{cls} const> {node.get_function_name()}(std::string_view p) const;")

def generate_node_body(out, node, prefix, level = 0):
    cls = prefix + node.get_recursive_class_name()
    par = prefix + node.parent.get_recursive_class_name()

    if node.is_static_component():
        out(f"char const* {cls}::component() const noexcept {{ return \"{node.get_component_name()}\"; }}")
        out(f"std::shared_ptr<{cls} const> {par}::{node.get_function_name()}() const {{")
        out(f"return {cls}::make_shared(shared_from_this());")
    else:
        if node.param != "std::string":
            out(f"std::shared_ptr<{cls} const> {par}::{node.get_function_name()}(std::string_view p) const {{")
            out(f"return {cls}::make_shared(shared_from_this(), std::string{{p}});")
            out("}")
        out(f"char const* {cls}::component() const noexcept {{ return value().c_str(); }}")
        out(f"std::shared_ptr<{cls} const> {par}::{node.get_function_name()}({node.param} const& p) const {{")
        out(f"return {cls}::make_shared(shared_from_this(), PathParamConverter<{node.param}>{{}}(p));")
    out("}")



    for c in node:
        generate_node_body(out, c, prefix, level)

def run(input_file, mode, prefix = ""):
    tree = ET.parse(input_file)
    root = PathNode(tree.getroot(), RootNode())
    def print_to_file(f):
        return lambda str: print(str, file=f)
    if mode == "header":
        generate_node_header(print_to_file(sys.stdout), root, prefix)
    elif mode == "body":
        generate_node_body(print_to_file(sys.stdout), root, prefix)



if __name__ == "__main__":
    run(*sys.argv[1:])
