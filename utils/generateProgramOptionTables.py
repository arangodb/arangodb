import sys
import os.path
import json
from itertools import groupby, chain
from cgi import escape

dump = os.path.normpath('./Documentation/Examples/arangobench.json')

# Load program options dump and convert to Python object
with open(dump, 'r') as fp:
    try:
        optionsRaw = json.load(fp)
    except ValueError as err:
        # invalid JSON
        raise err

# Allows to test if a group will be empty with hidden options ignored
def peekIterator(iterable, condition):
    try:
        while True:
            first = next(iterable)
            if condition(first):
                break
    except StopIteration:
        return None
    return first, chain([first], iterable)

# Give options a the section name 'global' if they don't have one
def groupBySection(elem):
    return elem[1]["section"] or 'global'

# Empty section string means global option, which should appear first
def sortBySection(elem):
    section = elem[1]["section"]
    if section:
        return (1, section)
    return (0, u'global')

# Format possible values as unordered list
def formatList(arr, text=''):
    formatItem = lambda elem: '<li>{}</li>'.format(elem)
    return '{}<ul>{}</ul>\n'.format(text, '\n'.join(map(formatItem, arr)))

# Group and sort by section name, global section first
for groupName, group in groupby(
        sorted(optionsRaw.items(), key=sortBySection),
        key=groupBySection):

    # Use some trickery to skip hidden options without consuming items from iterator
    groupPeek = peekIterator(group, lambda elem: elem[1]["hidden"] is False)
    if groupPeek is None:
        # Skip empty section to avoid useless headline (all options are hidden)
        continue

    # Output table header with column labels (one table per section)
    print '\n<h2>{} Options</h2>'.format(groupName.title())
    print '<table><thead><tr>'
    print '<td>{}</td><td>{}</td><td>{}</td>'.format('Name', 'Type', 'Description')
    print '</tr></thead><tbody>'

    # Sort options by name and output table rows
    for optionName, option in sorted(groupPeek[1], key=lambda elem: elem[0]):

        # Skip options marked as hidden
        if option["hidden"]:
            continue

        # Recover JSON syntax, because the Python representation uses [u'this format']
        default = json.dumps(option["default"])

        # Parse and re-format the optional field for possible values
        # (not fully safe, but ', 'is unlikely to occur in strings)
        try:
            optionList = option["values"].partition('Possible values: ')[2].split(', ')
            values = formatList(optionList, '<br/>Possible values:\n')
        except KeyError:
            values = ''

        # Expected data type for argument
        valueType = option["type"]

        # Upper-case first letter, period at the end, HTML entities
        description = option["description"].strip()
        description = description[0].upper() + description[1:]
        if description[-1] != '.':
            description += '.'
        description = escape(description)

        # Description, default value and possible values separated by line breaks
        descriptionCombined = '\n'.join([description, '<br/>Default: ' + default, values])

        print '<tr><td>{}</td><td>{}</td><td>{}</td></tr>'.format(optionName, valueType, descriptionCombined)

    print '</tbody></table>'
