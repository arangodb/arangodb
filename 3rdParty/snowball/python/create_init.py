#! /bin/sh/env python

import sys
import re
import os

python_out_folder = sys.argv[1]

filematch = re.compile(r"(\w+)_stemmer\.py$")

imports = []
languages = []

for pyscript in os.listdir(python_out_folder):
    match = filematch.match(pyscript)
    if (match):
        langname = match.group(1)
        titlecase = langname.title()
        languages.append("    '%(lang)s': %(title)sStemmer," % {'lang': langname, 'title': titlecase})
        imports.append('from .%(lang)s_stemmer import %(title)sStemmer' % {'lang': langname, 'title': titlecase})
imports.sort()
languages.sort()

src = '''__all__ = ('language', 'stemmer')

%(imports)s

_languages = {
%(languages)s
}

try:
    import Stemmer
    cext_available = True
except ImportError:
    cext_available = False

def algorithms():
    if cext_available:
        return Stemmer.language()
    else:
        return list(_languages.keys())

def stemmer(lang):
    if cext_available:
        return Stemmer.Stemmer(lang)
    if lang.lower() in _languages:
        return _languages[lang.lower()]()
    else:
        raise KeyError("Stemming algorithm '%%s' not found" %% lang)
''' % {'imports': '\n'.join(imports), 'languages': '\n'.join(languages)}

with open(os.path.join(python_out_folder, '__init__.py'), 'w') as out:
    out.write(src)
