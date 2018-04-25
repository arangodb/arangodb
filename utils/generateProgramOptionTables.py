import sys
import os.path
import json
from itertools import groupby, chain
from cgi import escape

dump = os.path.normpath('../Documentation/Examples/arangobench.json')

options = []

try:
    fp = open(dump, 'r')
    optionsRaw = json.load(fp)
except ValueError as err:
    # invalid JSON
    raise err
finally:
    fp.close()

# Allows to test if a group will be empty with hidden options ignored
def peekIterator(iterable):
    try:
        while True:
            first = next(iterable)
            if first[1]["hidden"] is False:
                break
    except StopIteration:
        return None
    return first, chain([first], iterable)

# Empty section string means global option, which should appear first
def sortBySection(elem):
    section = elem[1]["section"]
    if section:
        return (1, section)
    return (0, u'global')

def groupBySection(elem):
    return elem[1]["section"] or 'global'

def formatList(arr, text=''):
    formatItem = lambda elem: '<li>{}</li>'.format(elem)
    return '{}<ul>{}</ul>\n'.format(text, '\n'.join(map(formatItem, arr)))

for groupName, group in groupby(
        sorted(optionsRaw.items(), key=sortBySection),
        key=groupBySection):

    groupPeek = peekIterator(group)
    if groupPeek is None:
        # Skip empty sections (hidden options only)
        continue

    print '\n<h2>{} Options</h2>'.format(groupName.title())
    print '<table><thead><tr>'
    print '<td>{}</td><td>{}</td><td>{}</td>'.format('Name', 'Type', 'Description')
    print '</tr></thead><tbody>'

    for optionName, option in sorted(groupPeek[1], key=lambda elem: elem[0]):
        if option["hidden"]:
            continue
        default = json.dumps(option["default"])
        try:
            #values = '\nValues:\n- ' + '\n- '.join(option["values"].partition('Possible values: ')[2].split(', '))
            optionList = option["values"].partition('Possible values: ')[2].split(', ')
            values = formatList(optionList, '<br/>Possible values:\n')
        except KeyError:
            values = ''
        valueType = option["type"]#.join('<>')

        # Upper-case first letter, period at the end, HTML entities
        description = option["description"].strip()
        description = description[0].upper() + description[1:]
        if description[-1] != '.':
            description += '.'
        description = escape(description)

        # Description, default value and possible values together
        descriptionCombined = '\n'.join([description, '<br/>Default: ' + default, values])

        #print '{:30} {:10} {:15} {} {}'.format(optionName, valueType, default, description, values)
        print '<tr><td>{}</td><td>{}</td><td>{}</td></tr>'.format(optionName, valueType, descriptionCombined)

    print '</tbody></table>'
