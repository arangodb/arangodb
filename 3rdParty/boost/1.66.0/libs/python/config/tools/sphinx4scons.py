"""SCons.Tool.spinx4scons

Tool-specific initialization for the Sphinx document build system.

There normally shouldn't be any need to import this module directly.
It will usually be imported through the generic SCons.Tool.Tool()
selection method.

It should be placed in e.g. ~/site_scons/site_tools/sphinx4scons/
directory. Then it may be loaded by placing

  sphinx = Tool('sphinx4scons')
  sphinx(env)

in your SConstruct file.

For further details, please see the SCons documentation on how to
install and enable custom tools.
"""

#
# This package is provided under the Expat license
#
# Copyright (c) 2012 Orlando Wingbrant
#
# Permission is hereby granted, free of charge, to any person obtaining
# a copy of this software and associated documentation files (the
# "Software"), to deal in the Software without restriction, including
# without limitation the rights to use, copy, modify, merge, publish,
# distribute, sublicense, and/or sell copies of the Software, and to
# permit persons to whom the Software is furnished to do so, subject to
# the following conditions:
#
# The above copyright notice and this permission notice shall be included
# in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
# IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
# CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
# TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
# SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
#

__author__ = "Orlando Wingbrant"
__email__ = "orlando@widesite.org"
__url__ = "https://bitbucket.org/wingbrant/sphinx4scons"
__license__ = "Expat license"

import SCons.Action
import SCons.Builder
import SCons.Defaults
import SCons.Util
import SCons.Node.FS
import os

from sphinx.util.matching import patfilter, compile_matchers
from sphinx.util.osutil import make_filename


class ToolSphinxWarning(SCons.Warnings.Warning):
    pass


class SphinxBuilderNotFound(ToolSphinxWarning):
    pass

SCons.Warnings.enableWarningClass(ToolSphinxWarning)


def exists(env):
    return _detect(env)


def _detect(env):
    """Try to detect the sphinx-build script."""
    try:
        return env['SPHINXBUILD']
    except KeyError:
        pass

    sphinx = env.WhereIs('sphinx-build')
    if sphinx:
        return sphinx

    raise SCons.Errors.StopError(
        SphinxBuilderNotFound,
        "Could not detect sphinx-build script")
    return None


def generate(env):
    """Add Builders and construction variables to the Environment."""

    env['SPHINXBUILD'] = _detect(env)
    sphinx = _create_sphinx_builder(env)

    env.SetDefault(
        # Additional command-line flags
        SPHINXFLAGS = '',

        # Tag definitions, each entry will appear on the command line preceded by -t
        SPHINXTAGS = [],

        # Directory for doctrees
        SPHINXDOCTREE = '',

        # Path to sphinx configuration file
        SPHINXCONFIG = '',

        # Config file override settings, each entry will be preceded by -D
        SPHINXSETTINGS = {},

        # Default sphinx builder,
        SPHINXBUILDER = 'html',

        # Sphinx command
        SPHINXCOM = "$SPHINXBUILD $_SPHINXOPTIONS ${SOURCE.attributes.root} ${TARGET.attributes.root}",

        # Alternate console output when building sphinx documents
        SPHINXCOMSTR = ""
        )

    try:
        env.AddMethod(Sphinx, "Sphinx")
    except AttributeError:
        # Looks like we use a pre-0.98 version of SCons...
        from SCons.Script.SConscript import SConsEnvironment
        SConsEnvironment.Sphinx = Sphinx


def Sphinx(env, target, source, **kw):
    """A pseudo-builder wrapper for the sphinx builder."""
    builder = env['BUILDERS']['Sphinx4Scons']
    env_kw = env.Override(kw)
    options = _get_sphinxoptions(env_kw, target, source)
    output = builder(env, target, source, _SPHINXOPTIONS=options, **kw)
    return output


def _get_sphinxoptions(env, target, source):
    """Concatenates all the options for the sphinx command line."""
    options = []

    builder = _get_sphinxbuilder(env)
    options.append("-b %s" % env.subst(builder, target=target, source=source))

    flags = env.get('options', env.get('SPHINXFLAGS', ''))
    options.append(env.subst(flags, target=target, source=source))

    tags = env.get('tags', env.get('SPHINXTAGS', None))
    if tags is not None:
        if not SCons.SCons.Util.is_List(tags):
            tags = [tags]
        for tag in tags:
            if tag != '':
                tag = env.subst(tag, target=target, source=source)
                options.append("-t %s" % tag)

    settings = env.get('settings', env.get('SPHINXSETTINGS', None))
    if settings is not None:
        if not SCons.SCons.Util.is_Dict(settings):
            raise TypeError('SPHINXSETTINGS and/or settings argument must be a dictionary')
        for key, value in settings.iteritems():
            if value != '':
                value = env.subst(value, target=target, source=source)
                options.append('-D "%s=%s"' % (key, value))

    doctree = env.get('doctree', env.get("SPHINXDOCTREE", None))
    if isinstance(doctree, SCons.Node.FS.Dir):
        options.append("-d %s" % doctree.get_abspath())
    elif doctree is not None and doctree != '':
        doctree = env.subst(doctree, target=target, source=source)
        options.append("-d %s" % env.Dir(doctree).get_abspath())

    config = _get_sphinxconfig_path(env, None)
    if config is not None and config != '':
        config = env.subst(config, target=target, source=source)
        options.append("-c %s" % env.Dir(config).File('conf.py').rfile().dir.get_abspath())
    return " ".join(options)


def _create_sphinx_builder(env):
    try:
        sphinx = env['BUILDERS']['Sphinx4Scons']
    except KeyError:
        fs = SCons.Node.FS.get_default_fs()
        sphinx_com = SCons.Action.Action('$SPHINXCOM', '$SPHINXCOMSTR')
        sphinx = SCons.Builder.Builder(action=sphinx_com,
                                       emitter=sphinx_emitter,
                                       target_factory=fs.Dir,
                                       source_factory=fs.Dir
                                       )
        env['BUILDERS']['Sphinx4Scons'] = sphinx
    return sphinx


def sphinx_emitter(target, source, env):
    target[0].must_be_same(SCons.Node.FS.Dir)
    targetnode = target[0]

    source[0].must_be_same(SCons.Node.FS.Dir)
    srcnode = source[0]

    configdir = _get_sphinxconfig_path(env, None)
    if not configdir:
        confignode = srcnode
    else:
        confignode = env.Dir(configdir)

    srcinfo = SourceInfo(srcnode, confignode, env)
    targets, sources = _get_emissions(env, target, srcinfo)
    env.Clean(targets, target[0])

    return targets, sources


def sphinx_path(os_path):
    """Create sphinx-style path from os-style path."""
    return os_path.replace(os.sep, "/")


def os_path(sphinx_path):
    """Create os-style path from sphinx-style path."""
    return sphinx_path.replace("/", os.sep)


class SourceInfo(object):
    """
    Data container for all different kinds of source files used in
    a sphinx project.
    """
    def __init__(self, srcnode, confignode, env):
        self.confignode = confignode
        self.config = self._get_config(self.confignode, env)
        self.templates = self._get_templates(self.confignode, self.config)
        self.statics = self._get_statics(self.confignode, self.config)
        self.srcnode = srcnode
        self.sources = self._get_sources(self.srcnode, self.config)

        self.srcroot = srcnode
        if not srcnode.duplicate:
            self.srcroot = srcnode.srcnode().rdir()


    def _get_config(self, confignode, env):
        config = {}
        execfile(confignode.File('conf.py').rfile().get_abspath(), config)
        return config


    def _get_templates(self, confignode, config):
        """Returns template files defined in the project."""
        templates = []
        for path in config.get('templates_path', []):
            # Check if path is dir or file.
            # We can't use FS.Entry since that will create nodes, and
            # these nodes don't know about the source tree and will
            # get disambiguated to files even if they are directories in the
            # source tree.
            p = confignode.File('conf.py').rfile().dir.srcnode().get_abspath()
            p = os.path.join(p, os_path(path))
            if os.path.isfile(p):
                templates.append(confignode.File(path))
            elif os.path.isdir(p):
                node = confignode.Dir(path)
                for root, dirs, files in os.walk(p):
                    mydir = node.Dir(os.path.relpath(root, p))
                    templates += [mydir.File(f) for f in files]
        return templates


    def _get_statics(self, confignode, config):
        """Returns static files, filtered through exclude_patterns."""
        statics = []
        matchers = compile_matchers(config.get('exclude_patterns', []))

        for path in config.get('html_static_path', []):
            # Check _get_templates() why we use this construction.
            p = confignode.File('conf.py').rfile().dir.srcnode().get_abspath()
            p = os.path.join(p, os_path(path))
            if os.path.isfile(p):
                statics.append(confignode.File(path))
            elif os.path.isdir(p):
                node = confignode.Dir(path)
                for root, dirs, files in os.walk(p):
                    relpath = os.path.relpath(root, p)
                    for entry in [d for d in dirs if
                                  self._anymatch(matchers,
                                                 sphinx_path(os.path.join(relpath, d)))]:
                        dirs.remove(entry)
                    statics += [node.File(os_path(f)) for f in
                                self._exclude(matchers,
                                              [sphinx_path(os.path.join(relpath, name))
                                               for name in files])]
        return statics


    def _get_sources(self, srcnode, config):
        """Returns all source files in the project filtered through exclude_patterns."""
        suffix = config.get('source_suffix', '.rst')
        matchers = compile_matchers(config.get('exclude_patterns', []))

        srcfiles = []
        scannode = srcnode.srcnode().rdir()

        for root, dirs, files in os.walk(scannode.get_abspath()):
            relpath = os.path.relpath(root, scannode.get_abspath())
            for entry in [d for d in dirs if
                          self._anymatch(matchers,
                                         sphinx_path(os.path.join(relpath, d)))]:
                dirs.remove(entry)
            srcfiles += [srcnode.File(os_path(f)) for f in
                         self._exclude(matchers,
                                       [sphinx_path(os.path.join(relpath, name))
                                        for name in files if name.endswith(suffix)])]
        return srcfiles


    def _exclude(self, matchers, items):
        result = items
        for matcher in matchers:
            result = filter(lambda x: not matcher(x), result)
        return result


    def _anymatch(self, matchers, item):
        for matcher in matchers:
            if matcher(item):
                return True
        return False


def _get_sphinxconfig_path(env, default):
    path = env.get('config', env.get('SPHINXCONFIG', None))
    if path is None or path == '':
        path = default
    return path


def _get_emissions(env, target, srcinfo):
    targets = []
    sources = []
    builder = _get_sphinxbuilder(env)
    if builder == 'changes':
        targets, sources = _get_changes_emissions(env, target, srcinfo)
    if builder == 'devhelp':
        targets, sources = _get_help_emissions(env, target, srcinfo,
                                                ['.devhelp.gz'])
    elif builder == 'dirhtml':
        targets, sources = _get_dirhtml_emissions(env, target, srcinfo)
    elif builder == 'doctest':
        targets, sources = _get_doctest_emissions(env, target, srcinfo)
    elif builder == 'epub':
        targets, sources = _get_epub_emissions(env, target, srcinfo)
    elif builder == 'html':
        targets, sources = _get_serialize_emissions(env, target, srcinfo)
    elif builder == 'htmlhelp':
        targets, sources = _get_help_emissions(env, target, srcinfo,
                                               ['.hhp'], 'htmlhelp_basename')
    elif builder == 'gettext':
        targets, sources = _get_gettext_emissions(env, target, srcinfo)
    elif builder == 'json':
        targets, sources = _get_serialize_emissions(env, target, srcinfo,
                                                     '.fjson',
                                                     ['globalcontext.json',
                                                      'searchindex.json',
                                                      'self.environment.pickle'])
    elif builder == 'latex':
        targets, sources = _get_latex_emissions(env, target, srcinfo)
    elif builder == 'linkcheck':
        targets, sources = _get_linkcheck_emissions(env, target, srcinfo)
    elif builder == 'man':
        targets, sources = _get_man_emissions(env, target, srcinfo)
    elif builder == 'pickle':
        targets, sources = _get_serialize_emissions(env, target, srcinfo,
                                                    '.fpickle',
                                                    ['globalcontext.pickle',
                                                     'searchindex.pickle',
                                                     'environment.pickle'])
    elif builder == 'qthelp':
        targets, sources = _get_help_emissions(env, target, srcinfo,
                                               ['.qhp', '.qhcp'])
    elif builder == 'singlehtml':
        targets, sources = _get_singlehtml_emissions(env, target, srcinfo)
    elif builder == 'texinfo':
        targets, sources = _get_texinfo_emissions(env, target, srcinfo)
    elif builder == 'text':
        targets, sources = _get_text_emissions(env, target, srcinfo)

    sources.append(srcinfo.confignode.File('conf.py'))

    for s in sources:
        s.attributes.root = srcinfo.srcroot

    for t in targets:
        t.attributes.root = target[0]

    return targets, sources


def _get_sphinxbuilder(env):
    builder = env.get('builder', env["SPHINXBUILDER"])
    if builder is None or builder == '':
        raise SCons.Errors.UserError(("Missing construction variable " +
                                      "SPHINXBUILDER or variable is empty."))
    return builder


def _get_changes_emissions(env, target, srcinfo):
    sources = []
    sources.extend(srcinfo.sources)
    targets = [target[0].File("changes.html")]
    return targets, sources


def _get_dirhtml_emissions(env, target, srcinfo):
    suffix = srcinfo.config.get('html_file_suffix', ".html")

    def get_outfilename(pagename):
        pagename = os.path.splitext(pagename)[0]

        #Special treatment of files named "index". Don't create directory.
        if pagename == 'index' or pagename.endswith(os.sep + 'index'):
            outfilename = pagename + suffix
        else:
            outfilename = os.path.join(pagename, 'index' + suffix)
        return outfilename

    sources = []
    sources.extend(srcinfo.sources)
    sources.extend(srcinfo.templates)
    sources.extend(srcinfo.statics)

    targets = []
    for s in srcinfo.sources:
        t = os.path.relpath(str(s), str(srcinfo.srcroot))
        targets.append(target[0].File(get_outfilename(t)))

    for key in srcinfo.config.get('html_additional_pages', {}):
        t = target[0].File(get_outfilename(key))
        targets.append(t)

    return targets, sources


def _get_doctest_emissions(env, target, srcinfo):
    sources = []
    sources.extend(srcinfo.sources)
    targets = [target[0].File("output.txt")]
    return targets, sources


def _get_epub_emissions(env, target, srcinfo):
    epubPreFiles = srcinfo.config.get('epub_pre_files', [])
    epubPostFiles = srcinfo.config.get('epub_post_files', [])
    epubCover = srcinfo.config.get('epub_cover', (None, None))

    sources = []
    sources.extend(srcinfo.sources)
    sources.extend([srcinfo.srcroot.File(os_path(f[0])) for f in epubPreFiles])
    sources.extend([srcinfo.srcroot.File(os_path(f[0])) for f in epubPostFiles])
    if not (epubCover[0] is None or epubCover[0] == ''):
        sources.append(srcinfo.srcroot.File(os_path(epubCover[0])))
    if not (epubCover[1] is None or epubCover[1] == ''):
        sources.append(srcinfo.srcroot.File(os_path(epubCover[1])))

    t = srcinfo.config.get('epub_basename',
                        srcinfo.config.get('project',
                                         'Python'))

    targets = [target[0].File("%s.epub" % make_filename(t))]

    return targets, sources


def _get_gettext_emissions(env, target, srcinfo):
    sources = []
    sources.extend(srcinfo.sources)

    targets = [os.path.relpath(str(s), str(srcinfo.srcroot)) for s in sources]
    targets = [os.path.splitext(t)[0] for t in targets]
    targets = set([t.split(os.sep)[0] for t in targets])
    targets = [target[0].File(t + ".pot") for t in targets]

    return targets, sources


def _get_help_emissions(env, target, srcinfo, suffixes, basenameConfigKey='project'):
    basename = make_filename(
        srcinfo.config.get(basenameConfigKey, srcinfo.config['project']))

    sources = []
    sources.extend(srcinfo.sources)
    sources.extend(srcinfo.templates)
    sources.extend(srcinfo.statics)

    targets = [target[0].File(basename + s) for s in suffixes]

    return targets, sources


def _get_latex_emissions(env, target, srcinfo):
    sources = []
    sources.extend(srcinfo.sources)

    targets = map(lambda x: target[0].File(os_path(x[1])),
                  srcinfo.config.get('latex_documents'))

    return targets, sources


def _get_linkcheck_emissions(env, target, srcinfo):
    sources = []
    sources.extend(srcinfo.sources)
    targets = [target[0].File("output.txt")]
    return targets, sources


def _get_man_emissions(env, target, srcinfo):
    sources = []
    sources.extend(srcinfo.sources)
    targets = map(lambda x: target[0].File(os_path("%s.%s" % (x[1], x[4]))),
                  srcinfo.config.get('man_pages'))
    return targets, sources


def _get_serialize_emissions(env, target, srcinfo, suffix=None, extrafiles=[]):
    if suffix is None:
        suffix = srcinfo.config.get('html_file_suffix', '.html')

    sources = []
    sources.extend(srcinfo.sources)
    sources.extend(srcinfo.templates)
    sources.extend(srcinfo.statics)

    targets = []
    for s in srcinfo.sources:
        t = os.path.splitext(str(s))[0] + suffix
        t = os.path.relpath(t, str(srcinfo.srcroot))
        targets.append(t)

    for key in srcinfo.config.get('html_additional_pages', {}):
        targets.append(os_path("%s%s" % (key, suffix)))

    targets.extend(extrafiles)
    targets = [target[0].File(t) for t in targets]

    return targets, sources


def _get_singlehtml_emissions(env, target, srcinfo):
    suffix = srcinfo.config.get('html_file_suffix', ".html")

    sources = []
    sources.extend(srcinfo.sources)
    sources.extend(srcinfo.templates)
    sources.extend(srcinfo.statics)

    t = os.path.relpath(srcinfo.config['master_doc'] + suffix,
                        str(srcinfo.srcroot))
    targets = [target[0].File(t)]

    return targets, sources


def _get_texinfo_emissions(env, target, srcinfo):
    suffix = srcinfo.config.get('source_suffix', '.rst')

    sources = []
    sources.extend(srcinfo.sources)
    sources.extend(map(lambda x: source[0].File(os_path(x + suffix)),
                       srcinfo.config.get('texinfo_appendices', [])))

    targets = map(lambda x: target[0].File(os_path("%s.texi" % x[1])),
                  srcinfo.config.get('texinfo_documents'))

    return targets, sources


def _get_text_emissions(env, target, srcinfo):
    sources = []
    sources.extend(srcinfo.sources)

    targets = []
    for s in sources:
        t = os.path.relpath(str(s), str(srcinfo.srcroot))
        t = os.path.splitext(t)[0] + ".txt"
        targets.append(target[0].File(t))

    return targets, sources
