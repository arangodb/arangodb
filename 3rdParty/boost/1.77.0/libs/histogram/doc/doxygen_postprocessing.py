#            Copyright Hans Dembinski 2018 - 2019.
#   Distributed under the Boost Software License, Version 1.0.
#      (See accompanying file LICENSE_1_0.txt or copy at
#            https://www.boost.org/LICENSE_1_0.txt)

import sys
import xml.etree.ElementTree as ET
import re


def log(*args):
    sys.stdout.write("post-processing:" + " ".join(args) + "\n")


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
        m = re.match(r"(?:typename)? *([A-Za-z0-9_\:]+)", x.text)
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


def sort_headers_by(x):
    name = x.get("name")
    return name.count("/"), name


def tail_stripper(elem):
    if elem.tail:
        elem.tail = elem.tail.rstrip()
    for item in elem:
        tail_stripper(item)


def item_sorter(elem):
    if elem.tag == "namespace":
        if len(elem) > 1:
            items = list(elem)
            items.sort(key=lambda x: x.get("name"))
            elem[:] = items
        for sub in elem:
            item_sorter(sub)


if "ipykernel" in sys.argv[0]:  # used when run interactively in Atom/Hydrogen
    from pathlib import Path

    for input_file in Path().rglob("reference.xml"):
        input_file = str(input_file)
        break
else:
    input_file = sys.argv[1]

output_file = input_file.replace(".xml", "_pp.xml")

tree = ET.parse(input_file)
root = tree.getroot()

parent_map = {c: p for p in tree.iter() for c in p}

unspecified = ET.Element("emphasis")
unspecified.text = "unspecified"

# - hide all unnamed template parameters, these are used for SFINAE
# - hide all template parameters that start with Detail
for item in select(
    lambda x: x.get("name") == "" or x.get("name").startswith("Detail"),
    "template-type-parameter",
    "template-nontype-parameter",
):
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
    log(
        "removing deprecated",
        item.tag,
        item.get("name"),
        "from",
        parent.tag,
        parent.get("name"),
    )
    parent.remove(item)

# replace any typedef with "unspecified"
for item in select(is_detail, "typedef"):
    log("replacing typedef", item.get("name"), 'with "unspecified"')
    name = item.get("name")
    item.clear()
    item.tag = "typedef"
    item.set("name", name)
    type = ET.Element("type")
    type.append(unspecified)
    item.append(type)

# hide private member functions
for item in select(
    lambda x: x.get("name") == "private member functions", "method-group"
):
    parent = parent_map[item]
    log("removing private member functions from", parent.tag, parent.get("name"))
    parent.remove(item)

# hide undocumented classes, structs, functions and replace those declared
# "implementation detail" with typedef to unspecified
for item in select(lambda x: True, "class", "struct", "function"):
    purpose = item.find("purpose")
    if purpose is None:
        parent = parent_map[item]
        log(
            "removing undocumented",
            item.tag,
            item.get("name"),
            "from",
            parent.tag,
            parent.get("name"),
        )
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

parent_map = {c: p for p in tree.iter() for c in p}

# hide methods and constructors explicitly declared as "implementation detail"
for item in select(is_detail, "constructor", "method"):
    name = item.get("name")
    log(
        "removing",
        (item.tag + " " + name) if name is not None else item.tag,
        "declared as implementation detail",
    )
    parent_map[item].remove(item)


log("sorting headers")
reference = tree.getroot()
assert reference.tag == "library-reference"
reference[:] = sorted(reference, key=sort_headers_by)


log("sorting items in each namespace")
for header in reference:
    namespace = header.find("namespace")
    if namespace is None:
        continue
    item_sorter(namespace)


log("stripping trailing whitespace")
tail_stripper(reference)

tree.write(output_file)
