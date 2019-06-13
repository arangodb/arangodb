#!/usr/bin/env python

# Copyright Rene Rivera 2016
#
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)

import os.path
import shutil
import sys
from common import toolset_info, main, utils, script_common, ci_cli, set_arg

__dirname__ = os.path.dirname(os.path.realpath(__file__))

class script(script_common):
    '''
    Main script to test a Boost C++ Library.
    '''

    def __init__(self, ci_klass, **kargs):
        script_common.__init__(self, ci_klass, **kargs)
    
    def init(self, opt, kargs):
        opt.add_option( '--toolset',
            help="single toolset to test with" )
        opt.add_option( '--target',
            help="test target to build for testing, defaults to TARGET or 'minimal'")
        opt.add_option( '--address-model',
            help="address model to test, ie 64 or 32" )
        opt.add_option( '--variant',
            help="variant to test, ie debug, release" )
        set_arg(kargs, 'toolset', os.getenv("TOOLSET"))
        set_arg(kargs, 'target', os.getenv('TARGET', 'minimal'))
        set_arg(kargs, 'address_model', os.getenv("ADDRESS_MODEL",None))
        set_arg(kargs, 'variant', os.getenv("VARIANT","debug"))
        set_arg(kargs, 'cxxflags', os.getenv("CXXFLAGS",None))
        return kargs

    def start(self):
        script_common.start(self)
        # Some setup we need to redo for each invocation.
        self.b2_dir = os.path.join(self.build_dir, 'b2')

    def command_install(self):
        script_common.command_install(self)
        # Fetch & install toolset..
        utils.log( "Install toolset: %s"%(self.toolset) )
        if self.toolset:
            self.command_install_toolset(self.toolset)
    
    def command_before_build(self):
        script_common.command_before_build(self)
        
        # Fetch dependencies.
        utils.git_clone('boostorg','build','develop',repo_dir=self.b2_dir)
        
        # Create config file for b2 toolset.
        if not isinstance(self.ci, ci_cli):
            cxxflags = None
            if self.cxxflags:
                cxxflags = self.cxxflags.split()
                cxxflags = " <cxxflags>".join(cxxflags)
            utils.make_file(os.path.join(self.repo_dir, 'project-config.jam'),
                """
using %(toolset)s : %(version)s : %(command)s : %(cxxflags)s ;
using python : %(pyversion)s : "%(python)s" ;
"""%{
                'toolset':toolset_info[self.toolset]['toolset'],
                'version':toolset_info[self.toolset]['version'],
                'command':toolset_info[self.toolset]['command'],
                'cxxflags':"<cxxflags>"+cxxflags if cxxflags else "",
                'pyversion':"%s.%s"%(sys.version_info[0],sys.version_info[1]),
                'python':sys.executable.replace("\\","\\\\")
                })
        
        # "Convert" boostorg-predef into standalone b2 project.
        if os.path.exists(os.path.join(self.repo_dir,'build.jam')) and not os.path.exists(os.path.join(self.repo_dir,'project-root.jam')):
            os.rename(os.path.join(self.repo_dir,'build.jam'), os.path.join(self.repo_dir,'project-root.jam'))

    def command_build(self):
        script_common.command_build(self)
        
        # Set up tools.
        if not isinstance(self.ci, ci_cli) and toolset_info[self.toolset]['command']:
            os.environ['PATH'] = os.pathsep.join([
                os.path.dirname(toolset_info[self.toolset]['command']),
                os.environ['PATH']])
        
        # Bootstrap Boost Build engine.
        os.chdir(self.b2_dir)
        if sys.platform == 'win32':
            utils.check_call(".\\bootstrap.bat")
        else:
            utils.check_call("./bootstrap.sh")
        os.environ['PATH'] = os.pathsep.join([self.b2_dir, os.environ['PATH']])
        os.environ['BOOST_BUILD_PATH'] = self.b2_dir
        
        # Run the limited tests.
        print("--- Testing %s ---"%(self.repo_dir))
        os.chdir(os.path.join(self.repo_dir,'test'))
        toolset_to_test = ""
        if self.toolset:
            if not isinstance(self.ci, ci_cli):
                toolset_to_test = toolset_info[self.toolset]['toolset']
            else:
                toolset_to_test = self.toolset
        self.b2(
            '-d1',
            '-p0',
            'preserve-test-targets=off',
            '--dump-tests',
            '--verbose-test',
            '--build-dir=%s'%(self.build_dir),
            '--out-xml=%s'%(os.path.join(self.build_dir,'regression.xml')),
            '' if not toolset_to_test else 'toolset=%s'%(toolset_to_test),
            '' if not self.address_model else 'address-model=%s'%(self.address_model),
            'variant=%s'%(self.variant),
            self.target
            )
        
        # Generate a readable test report.
        import build_log
        log_main = build_log.Main([
            '--output=console',
            os.path.join(self.build_dir,'regression.xml')])
        # And exit with an error if the report contains failures.
        # This lets the CI notice the error and report a failed build.
        # And hence trigger the failure machinery, like sending emails.
        if log_main.failed:
            self.ci.finish(-1)

    def command_before_cache(self):
        script_common.command_before_cache(self)
        os.chdir(self.b2_dir)
        utils.check_call("git","clean","-dfqx")
        utils.check_call("git","status","-bs")
        # utils.check_call("git","submodule","--quiet","foreach","git","clean","-dfqx")
        # utils.check_call("git","submodule","foreach","git","status","-bs")

main(script)
