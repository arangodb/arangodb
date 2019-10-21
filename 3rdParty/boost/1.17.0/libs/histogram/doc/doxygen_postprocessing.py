from __future__ import print_function
import sys
import xml.etree.ElementTree as ET
import re


def log(*args):
    print("post-processing:", *args)


def select(condition, *tags):
    result = []
    for tag in tags:
        for item in root.iter(tag):
            if condition(item):
                result.append(item)
    return result


def is_detail(x):
    if x.text is not None:
        if "detail" in x.text:
            return True
        m = re.match("(?:typename)? *([A-Za-z0-9_\:]+)", x.text)
        if m is not None:
            s = m.group(1)
            if s.startswith("detail") or s.endswith("_impl"):
                x.text = s
                return True

    p = x.find("purpose")
    if p is not None:
        return p.text.lower().lstrip().startswith("implementation detail")
    return False


def is_deprecated(x):
    p = x.find("purpose")
    if p is not None:
        return p.text.lower().lstrip().startswith("deprecated")
    return False


tree = ET.parse(sys.argv[1])
root = tree.getroot()

parent_map = {c:p for p in tree.iter() for c in p}

unspecified = ET.Element("emphasis")
unspecified.text = "unspecified"

# hide all unnamed template parameters, these are used for SFINAE
for item in select(lambda x: x.get("name") == "", "template-type-parameter"):
    parent = parent_map[item]
    assert parent.tag == "template"
    parent.remove(item)
    parent = parent_map[parent]
    name = parent.get("name")
    if name is None:
        log("removing unnamed template parameter from", parent.tag)
    else:
        log("removing unnamed template parameter from", parent.tag, name)

# replace any type with "detail" in its name with "unspecified"
for item in select(is_detail, "type"):
    log("replacing", '"%s"' % item.text, 'with "unspecified"')
    item.clear()
    item.append(unspecified)

# hide everything that's deprecated
for item in select(is_deprecated, "typedef"):
    parent = parent_map[item]
    log("removing deprecated", item.tag, item.get("name"), "from", parent.tag, parent.get("name"))
    parent.remove(item)

# hide private member functions
for item in select(lambda x: x.get("name") == "private member functions", "method-group"):
    parent = parent_map[item]
    log("removing private member functions from", parent.tag, parent.get("name"))
    parent.remove(item)

# hide undocumented classes, structs, functions and replace those declared
# "implementation detail" with typedef to implementation_defined
for item in select(lambda x:True, "class", "struct", "function"):
    purpose = item.find("purpose")
    if purpose is None:
        parent = parent_map[item]
        log("removing undocumented", item.tag, item.get("name"), "from",
            parent.tag, parent.get("name"))
        if item in parent_map[item]:
            parent_map[item].remove(item)
    elif purpose.text.strip().lower() == "implementation detail":
        log("replacing", item.tag, item.get("name"), "with unspecified typedef")
        name = item.get("name")
        item.clear()
        item.tag = "typedef"
        item.set("name", name)
        type = ET.Element("type")
        type.append(unspecified)
        item.append(type)

parent_map = {c:p for p in tree.iter() for c in p}

# hide methods and constructors explicitly declared as "implementation detail"
for item in select(is_detail, "constructor", "method"):
    name = item.get("name")
    log("removing", (item.tag + " " + name) if name is not None else item.tag,
        "declared as implementation detail")
    parent_map[item].remove(item)

tree.write(sys.argv[2])
