#!/usr/bin/env python

# Copyright Rene Rivera 2016
#
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)

import os
import inspect
import optparse
import sys
import glob
import fnmatch
import json

class check_library():
    '''
    This is a collection of checks for a library to test if a library
    follows the Boost C++ Libraries requirements and guidelines. It also
    checks for possible and likely errors in the library.
    '''
    
    def __init__(self):
        self.main()
    
    def check_organization(self):
        self.run_batch('check_organization_')
    
    def check_organization_build(self):
        if os.path.isdir(os.path.join(self.library_dir, 'build')):
            self.assert_file_exists(os.path.join(self.library_dir, 'build'), self.jamfile,
                '''
                Did not find a Boost Build file in the [project-root]/build directory.
                The library needs to provide a Boost Build project that the user,
                and the top level Boost project, can use to build the library if it
                has sources to build.
                ''',
                'org-build-ok')
        if os.path.isdir(os.path.join(self.library_dir, 'src')):
            self.assert_dir_exists(os.path.join(self.library_dir,'build'),
                '''
                Missing [project-root]/build directory. The [project-root]/build directory
                is required for libraries that have a [project-root]/src directory.
                ''',
                'org-build-src')
    
    def check_organization_doc(self):
        self.assert_file_exists(self.library_dir, ['index.html'],
            '''
            Did not find [project-root]/index.html file.
            
            The file is required for all libraries. Redirection to HTML documentation.
            ''',
            'org-doc-redir')
        self.assert_dir_exists(os.path.join(self.library_dir,'doc'),
            '''
            Missing [project-root]/doc directory. The [project-root]/doc directory
            is required for all libraries.
            
            Sources to build with and built documentation for the library. If the
            library needs to build documentation from non-HTML files this location
            must be buildable with Boost Build.
            ''',
            'org-doc-dir')
    
    def check_organization_include(self):
        if os.path.isdir(os.path.join(self.library_dir,'include','boost',self.library_name)):
            self.warn_file_exists(os.path.join(self.library_dir,'include','boost'), ['*.h*'],
                '''
                Found extra files in [project-root]/include/boost directory.
                ''',
                'org-inc-extra',
                negate = True,
                globs_to_exclude = ['%s.h*'%(self.library_name)])
        else:
            self.warn_file_exists(os.path.join(self.library_dir,'include','boost'), ['%s.h*'%(self.library_name)],
                '''
                Did not find [project-root]/include/boost/[library].h* file.
                
                A single header for the library is suggested at [project-root]/include/boost/[library].h*
                if the library does not have a header directory at [project-root]/include/boost/[library].
                ''',
                'org-inc-one')
    
    def check_organization_meta(self):
        parent_dir = os.path.dirname(self.library_dir)
        # If this is a sublibrary it's possible that the library information is the
        # parent library's meta/libraries.json. Otherwise it's a regular library
        # and structure.
        if not self.test_dir_exists(os.path.join(self.library_dir,'meta')) \
            and self.test_file_exists(os.path.join(parent_dir,'meta'),['libraries.json']):
            if self.get_library_meta():
                return
            self.assert_file_exists(os.path.join(self.library_dir, 'meta'), ['libraries.json'],
                '''
                Did not find [project-root]/meta/libraries.json file, nor did
                [super-project]/meta/libraries.json contain an entry for the sublibrary.
                
                The file is required for all libraries. And contains information about
                the library used to generate website and documentation for the
                Boost C++ Libraries collection.
                ''',
                'org-meta-libs')
        elif self.assert_dir_exists(os.path.join(self.library_dir,'meta'),
            '''
            Missing [project-root]/meta directory. The [project-root]/meta directory
            is required for all libraries.
            ''',
            'org-meta-dir'):
            self.assert_file_exists(os.path.join(self.library_dir, 'meta'), ['libraries.json'],
                '''
                Did not find [project-root]/meta/libraries.json file.
                
                The file is required for all libraries. And contains information about
                the library used to generate website and documentation for the
                Boost C++ Libraries collection.
                ''',
                'org-meta-libs')
    
    def check_organization_test(self):
        if self.assert_dir_exists(os.path.join(self.library_dir,'test'),
            '''
            Missing [project-root]/test directory. The [project-root]/test directory
            is required for all libraries.
            
            Regression or other test programs or scripts. This is the only location
            considered for automated testing. If you have additional locations that
            need to be part of automated testing it is required that this location
            refer to the additional test locations.
            ''',
            'org-test-dir'):
            self.assert_file_exists(os.path.join(self.library_dir, 'test'), self.jamfile,
                '''
                Did not find a Boost Build file in the [project-root]/test directory.
                ''',
                'org-test-ok')

    def main(self):
        commands = [];
        for method in inspect.getmembers(self, predicate=inspect.ismethod):
            if method[0].startswith('check_'):
                commands.append(method[0][6:].replace('_','-'))
        commands = "commands: %s" % ', '.join(commands)

        opt = optparse.OptionParser(
            usage="%prog [options] [commands]",
            description=commands)
        opt.add_option('--boost-root')
        opt.add_option('--library')
        opt.add_option('--jamfile')
        opt.add_option('--debug', action='store_true')
        self.boost_root = None
        self.library = None
        self.jamfile = None
        self.debug = False
        ( _opt_, self.actions ) = opt.parse_args(None,self)
        
        self.library_dir = os.path.join(self.boost_root, self.library)
        self.error_count = 0;
        self.jamfile = self.jamfile.split(';')
        self.library_name = self.library.split('/',1)[1] #os.path.basename(self.library)
        self.library_key = self.library.split('/',1)[1]

        if self.debug:
            print(">>> cwd: %s"%(os.getcwd()))
            print(">>> actions: %s"%(self.actions))
            print(">>> boost_root: %s"%(self.boost_root))
            print(">>> library: %s"%(self.library))
            print(">>> jamfile: %s"%(self.jamfile))

        for action in self.actions:
            action_m = "check_"+action.replace('-','_')
            if hasattr(self,action_m):
                getattr(self,action_m)()
    
    def run_batch(self, action_base, *args, **kargs):
        for method in inspect.getmembers(self, predicate=inspect.ismethod):
            if method[0].startswith(action_base):
                getattr(self,method[0])(*args, **kargs)

    def get_library_meta(self):
        '''
        Fetches the meta data for the current library. The data could be in
        the superlib meta data file. If we can't find the data None is returned.
        '''
        parent_dir = os.path.dirname(self.library_dir)
        if self.test_file_exists(os.path.join(self.library_dir,'meta'),['libraries.json']):
            with open(os.path.join(self.library_dir,'meta','libraries.json'),'r') as f:
                meta_data = json.load(f)
                if isinstance(meta_data,list):
                    for lib in meta_data:
                        if lib['key'] == self.library_key:
                            return lib
                elif 'key' in meta_data and meta_data['key'] == self.library_key:
                    return meta_data
        if not self.test_dir_exists(os.path.join(self.library_dir,'meta')) \
            and self.test_file_exists(os.path.join(parent_dir,'meta'),['libraries.json']):
            with open(os.path.join(parent_dir,'meta','libraries.json'),'r') as f:
                libraries_json = json.load(f)
                if isinstance(libraries_json,list):
                    for lib in libraries_json:
                        if lib['key'] == self.library_key:
                            return lib
        return None

    def error(self, reason, message, key):
        self.error_count += 1
        print("%s: error: %s; %s <<%s>>"%(
            self.library,
            self.clean_message(reason),
            self.clean_message(message),
            key,
            ))
    
    def warn(self, reason, message, key):
        print("%s: warning: %s; %s <<%s>>"%(
            self.library,
            self.clean_message(reason),
            self.clean_message(message),
            key,
            ))
    
    def info(self, message):
        if self.debug:
            print("%s: info: %s"%(self.library, self.clean_message(message)))
    
    def clean_message(self, message):
        return " ".join(message.strip().split())
    
    def assert_dir_exists(self, dir, message, key, negate = False):
        self.info("check directory '%s', negate = %s"%(dir,negate))
        if os.path.isdir(dir):
            if negate:
                self.error("directory found", message, key)
                return False
        else:
            if not negate:
                self.error("directory not found", message, key)
                return False
        return True
    
    def warn_dir_exists(self, dir, message, key, negate = False):
        self.info("check directory '%s', negate = %s"%(dir,negate))
        if os.path.isdir(dir):
            if negate:
                self.warn("directory found", message, key)
                return False
        else:
            if not negate:
                self.warn("directory not found", message, key)
                return False
        return True
    
    def assert_file_exists(self, dir, globs_to_include, message, key, negate = False, globs_to_exclude = []):
        found = self.test_file_exists(dir, globs_to_include = globs_to_include, globs_to_exclude = globs_to_exclude)
        if negate:
            if found:
                self.error("file found", message, key)
                return False
        else:
            if not found:
                self.error("file not found", message, key)
                return False
        return True
    
    def warn_file_exists(self, dir, globs_to_include, message, key, negate = False, globs_to_exclude = []):
        found = self.test_file_exists(dir, globs_to_include = globs_to_include, globs_to_exclude = globs_to_exclude)
        if negate:
            if found:
                self.warn("file found", message, key)
                return False
        else:
            if not found:
                self.warn("file not found", message, key)
                return False
        return True
    
    def test_dir_exists(self, dir):
        return os.path.isdir(dir)
    
    def test_file_exists(self, dir, globs_to_include, globs_to_exclude = []):
        self.info("test file(s) in dir '%s', include = '%s', exclude = %s"%(dir,globs_to_include,globs_to_exclude))
        found = False
        if os.path.isdir(dir):
            for g in globs_to_include:
                for f in glob.iglob(os.path.join(dir,g)):
                    exclude = False
                    for ge in globs_to_exclude:
                        if fnmatch.fnmatch(os.path.basename(f),ge):
                            exclude = True
                    found = not exclude
                if found:
                    break
        return found

if check_library().error_count > 0:
    sys.exit(1)

