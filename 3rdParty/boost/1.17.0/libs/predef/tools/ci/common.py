#!/usr/bin/env python

# Copyright Rene Rivera 2016
#
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)

import sys
import inspect
import optparse
import os.path
import string
import time
import subprocess
import codecs
import shutil
import threading

toolset_info = {
    'clang-3.4' : {
        'ppa' : ["ppa:h-rayflood/llvm"],
        'package' : 'clang-3.4',
        'command' : 'clang++-3.4',
        'toolset' : 'clang',
        'version' : ''
        },
    'clang-3.5' : {
        'ppa' : ["ppa:h-rayflood/llvm"],
        'package' : 'clang-3.5',
        'command' : 'clang++-3.5',
        'toolset' : 'clang',
        'version' : ''
        },
    'clang-3.6' : {
        'ppa' : ["ppa:h-rayflood/llvm"],
        'package' : 'clang-3.6',
        'command' : 'clang++-3.6',
        'toolset' : 'clang',
        'version' : ''
        },
    'clang-3.7' : {
        'deb' : ["http://apt.llvm.org/trusty/","llvm-toolchain-trusty-3.7","main"],
        'apt-key' : ['http://apt.llvm.org/llvm-snapshot.gpg.key'],
        'package' : 'clang-3.7',
        'command' : 'clang++-3.7',
        'toolset' : 'clang',
        'version' : ''
        },
    'clang-3.8' : {
        'deb' : ["http://apt.llvm.org/trusty/","llvm-toolchain-trusty-3.8","main"],
        'apt-key' : ['http://apt.llvm.org/llvm-snapshot.gpg.key'],
        'package' : 'clang-3.8',
        'command' : 'clang++-3.8',
        'toolset' : 'clang',
        'version' : ''
        },
    'clang-3.9' : {
        'deb' : ["http://apt.llvm.org/trusty/","llvm-toolchain-trusty-3.9","main"],
        'apt-key' : ['http://apt.llvm.org/llvm-snapshot.gpg.key'],
        'package' : 'clang-3.9',
        'command' : 'clang++-3.9',
        'toolset' : 'clang',
        'version' : ''
        },
    'clang-4.0' : {
        'deb' : ["http://apt.llvm.org/trusty/","llvm-toolchain-trusty-4.0","main"],
        'apt-key' : ['http://apt.llvm.org/llvm-snapshot.gpg.key'],
        'package' : 'clang-4.0',
        'command' : 'clang++-4.0',
        'toolset' : 'clang',
        'version' : ''
        },
    'clang-5.0' : {
        'deb' : ["http://apt.llvm.org/trusty/","llvm-toolchain-trusty-5.0","main"],
        'apt-key' : ['http://apt.llvm.org/llvm-snapshot.gpg.key'],
        'package' : 'clang-5.0',
        'command' : 'clang++-5.0',
        'toolset' : 'clang',
        'version' : ''
        },
    'clang-6.0' : {
        'deb' : ["http://apt.llvm.org/trusty/","llvm-toolchain-trusty-6.0","main"],
        'apt-key' : ['http://apt.llvm.org/llvm-snapshot.gpg.key'],
        'package' : 'clang-6.0',
        'command' : 'clang++-6.0',
        'toolset' : 'clang',
        'version' : ''
        },
    'gcc-4.7' : {
        'ppa' : ["ppa:ubuntu-toolchain-r/test"],
        'package' : 'g++-4.7',
        'command' : 'g++-4.7',
        'toolset' : 'gcc',
        'version' : ''
        },
    'gcc-4.8' : {
        'bin' : 'gcc-4.8',
        'ppa' : ["ppa:ubuntu-toolchain-r/test"],
        'package' : 'g++-4.8',
        'command' : 'g++-4.8',
        'toolset' : 'gcc',
        'version' : ''
        },
    'gcc-4.9' : {
        'ppa' : ["ppa:ubuntu-toolchain-r/test"],
        'package' : 'g++-4.9',
        'command' : 'g++-4.9',
        'toolset' : 'gcc',
        'version' : ''
        },
    'gcc-5.1' : {
        'ppa' : ["ppa:ubuntu-toolchain-r/test"],
        'package' : 'g++-5',
        'command' : 'g++-5',
        'toolset' : 'gcc',
        'version' : ''
        },
    'gcc-5' : {
        'ppa' : ["ppa:ubuntu-toolchain-r/test"],
        'package' : 'g++-5',
        'command' : 'g++-5',
        'toolset' : 'gcc',
        'version' : ''
        },
    'gcc-6' : {
        'ppa' : ["ppa:ubuntu-toolchain-r/test"],
        'package' : 'g++-6',
        'command' : 'g++-6',
        'toolset' : 'gcc',
        'version' : ''
        },
    'gcc-7' : {
        'ppa' : ["ppa:ubuntu-toolchain-r/test"],
        'package' : 'g++-7',
        'command' : 'g++-7',
        'toolset' : 'gcc',
        'version' : ''
        },
    'gcc-8' : {
        'ppa' : ["ppa:ubuntu-toolchain-r/test"],
        'package' : 'g++-8',
        'command' : 'g++-8',
        'toolset' : 'gcc',
        'version' : ''
        },
    'mingw-5' : {
        'toolset' : 'gcc',
        'command' : 'C:\\\\MinGW\\\\bin\\\\g++.exe',
        'version' : ''
        },
    'mingw64-6' : {
        'toolset' : 'gcc',
        'command' : 'C:\\\\mingw-w64\\\\x86_64-6.3.0-posix-seh-rt_v5-rev1\\\\mingw64\\\\bin\\\\g++.exe',
        'version' : ''
        },
    'vs-2008' : {
        'toolset' : 'msvc',
        'command' : '',
        'version' : '9.0'
        },
    'vs-2010' : {
        'toolset' : 'msvc',
        'command' : '',
        'version' : '10.0'
        },
    'vs-2012' : {
        'toolset' : 'msvc',
        'command' : '',
        'version' : '11.0'
        },
    'vs-2013' : {
        'toolset' : 'msvc',
        'command' : '',
        'version' : '12.0'
        },
    'vs-2015' : {
        'toolset' : 'msvc',
        'command' : '',
        'version' : '14.0'
        },
    'vs-2017' : {
        'toolset' : 'msvc',
        'command' : '',
        'version' : '14.1'
        },
    'xcode-6.1' : {
        'command' : 'clang++',
        'toolset' : 'clang',
        'version' : ''
        },
    'xcode-6.2' : {
        'command' : 'clang++',
        'toolset' : 'clang',
        'version' : ''
        },
    'xcode-6.3' : {
        'command' : 'clang++',
        'toolset' : 'clang',
        'version' : ''
        },
    'xcode-6.4' : {
        'command' : 'clang++',
        'toolset' : 'clang',
        'version' : ''
        },
    'xcode-7.0' : {
        'command' : 'clang++',
        'toolset' : 'clang',
        'version' : ''
        },
    'xcode-7.1' : {
        'command' : 'clang++',
        'toolset' : 'clang',
        'version' : ''
        },
    'xcode-7.2' : {
        'command' : 'clang++',
        'toolset' : 'clang',
        'version' : ''
        },
    'xcode-7.3' : {
        'command' : 'clang++',
        'toolset' : 'clang',
        'version' : ''
        },
    'xcode-8.0' : {
        'command' : 'clang++',
        'toolset' : 'clang',
        'version' : ''
        },
    'xcode-8.1' : {
        'command' : 'clang++',
        'toolset' : 'clang',
        'version' : ''
        },
    'xcode-8.2' : {
        'command' : 'clang++',
        'toolset' : 'clang',
        'version' : ''
        },
    'xcode-8.3' : {
        'command' : 'clang++',
        'toolset' : 'clang',
        'version' : ''
        },
    'xcode-9.0' : {
        'command' : 'clang++',
        'toolset' : 'clang',
        'version' : ''
        },
    'xcode-9.1' : {
        'command' : 'clang++',
        'toolset' : 'clang',
        'version' : ''
        },
    'xcode-9.2' : {
        'command' : 'clang++',
        'toolset' : 'clang',
        'version' : ''
        },
    'xcode-9.3' : {
        'command' : 'clang++',
        'toolset' : 'clang',
        'version' : ''
        },
    'xcode-9.4' : {
        'command' : 'clang++',
        'toolset' : 'clang',
        'version' : ''
        },
    'xcode-10.0' : {
        'command' : 'clang++',
        'toolset' : 'clang',
        'version' : ''
        },
    }

class SystemCallError(Exception):
    def __init__(self, command, result):
        self.command = command
        self.result = result
    def __str__(self, *args, **kwargs):
        return "'%s' ==> %s"%("' '".join(self.command), self.result)

class utils:
    
    call_stats = []
    
    @staticmethod
    def call(*command, **kargs):
        utils.log( "%s> '%s'"%(os.getcwd(), "' '".join(command)) )
        t = time.time()
        result = subprocess.call(command, **kargs)
        t = time.time()-t
        if result != 0:
            print "Failed: '%s' ERROR = %s"%("' '".join(command), result)
        utils.call_stats.append((t,os.getcwd(),command,result))
        utils.log( "%s> '%s' execution time %s seconds"%(os.getcwd(), "' '".join(command), t) )
        return result
    
    @staticmethod
    def print_call_stats():
        utils.log("================================================================================")
        for j in sorted(utils.call_stats, reverse=True):
            utils.log("{:>12.4f}\t{}> {} ==> {}".format(*j))
        utils.log("================================================================================")
    
    @staticmethod
    def check_call(*command, **kargs):
        cwd = os.getcwd()
        result = utils.call(*command, **kargs)
        if result != 0:
            raise(SystemCallError([cwd].extend(command), result))
    
    @staticmethod
    def makedirs( path ):
        if not os.path.exists( path ):
            os.makedirs( path )
    
    @staticmethod
    def log_level():
        frames = inspect.stack()
        level = 0
        for i in frames[ 3: ]:
            if i[0].f_locals.has_key( '__log__' ):
                level = level + i[0].f_locals[ '__log__' ]
        return level
    
    @staticmethod
    def log( message ):
        sys.stdout.flush()
        sys.stderr.flush()
        sys.stderr.write( '# ' + '    ' * utils.log_level() +  message + '\n' )
        sys.stderr.flush()

    @staticmethod
    def rmtree(path):
        if os.path.exists( path ):
            #~ shutil.rmtree( unicode( path ) )
            if sys.platform == 'win32':
                os.system( 'del /f /s /q "%s" >nul 2>&1' % path )
                shutil.rmtree( unicode( path ) )
            else:
                os.system( 'rm -f -r "%s"' % path )

    @staticmethod
    def retry( f, max_attempts=5, sleep_secs=10 ):
        for attempts in range( max_attempts, -1, -1 ):
            try:
                return f()
            except Exception, msg:
                utils.log( '%s failed with message "%s"' % ( f.__name__, msg ) )
                if attempts == 0:
                    utils.log( 'Giving up.' )
                    raise

                utils.log( 'Retrying (%d more attempts).' % attempts )
                time.sleep( sleep_secs )

    @staticmethod
    def web_get( source_url, destination_file, proxy = None ):
        import urllib

        proxies = None
        if proxy is not None:
            proxies = {
                'https' : proxy,
                'http' : proxy
                }

        src = urllib.urlopen( source_url, proxies = proxies )

        f = open( destination_file, 'wb' )
        while True:
            data = src.read( 16*1024 )
            if len( data ) == 0: break
            f.write( data )

        f.close()
        src.close()

    @staticmethod
    def unpack_archive( archive_path ):
        utils.log( 'Unpacking archive ("%s")...' % archive_path )

        archive_name = os.path.basename( archive_path )
        extension = archive_name[ archive_name.find( '.' ) : ]

        if extension in ( ".tar.gz", ".tar.bz2" ):
            import tarfile
            import stat

            mode = os.path.splitext( extension )[1][1:]
            tar = tarfile.open( archive_path, 'r:%s' % mode )
            for tarinfo in tar:
                tar.extract( tarinfo )
                if sys.platform == 'win32' and not tarinfo.isdir():
                    # workaround what appears to be a Win32-specific bug in 'tarfile'
                    # (modification times for extracted files are not set properly)
                    f = os.path.join( os.curdir, tarinfo.name )
                    os.chmod( f, stat.S_IWRITE )
                    os.utime( f, ( tarinfo.mtime, tarinfo.mtime ) )
            tar.close()
        elif extension in ( ".zip" ):
            import zipfile

            z = zipfile.ZipFile( archive_path, 'r', zipfile.ZIP_DEFLATED )
            for f in z.infolist():
                destination_file_path = os.path.join( os.curdir, f.filename )
                if destination_file_path[-1] == "/": # directory
                    if not os.path.exists( destination_file_path  ):
                        os.makedirs( destination_file_path  )
                else: # file
                    result = open( destination_file_path, 'wb' )
                    result.write( z.read( f.filename ) )
                    result.close()
            z.close()
        else:
            raise 'Do not know how to unpack archives with extension \"%s\"' % extension
    
    @staticmethod
    def make_file(filename, *text):
        text = string.join( text, '\n' )
        with codecs.open( filename, 'w', 'utf-8' ) as f:
            f.write( text )
    
    @staticmethod
    def append_file(filename, *text):
        with codecs.open( filename, 'a', 'utf-8' ) as f:
            f.write( string.join( text, '\n' ) )
    
    @staticmethod
    def mem_info():
        if sys.platform == "darwin":
            utils.call("top","-l","1","-s","0","-n","0")
        elif sys.platform.startswith("linux"):
            utils.call("free","-m","-l")
    
    @staticmethod
    def query_boost_version(boost_root):
        '''
        Read in the Boost version from a given boost_root.
        '''
        boost_version = None
        if os.path.exists(os.path.join(boost_root,'Jamroot')):
            with codecs.open(os.path.join(boost_root,'Jamroot'), 'r', 'utf-8') as f:
                for line in f.readlines():
                    parts = line.split()
                    if len(parts) >= 5 and parts[1] == 'BOOST_VERSION':
                        boost_version = parts[3]
                        break
        if not boost_version:
            boost_version = 'default'
        return boost_version
    
    @staticmethod
    def git_clone(owner, repo, branch, commit = None, repo_dir = None, submodules = False, url_format = "https://github.com/%(owner)s/%(repo)s.git"):
        '''
        This clone mimicks the way Travis-CI clones a project's repo. So far
        Travis-CI is the most limiting in the sense of only fetching partial
        history of the repo.
        '''
        if not repo_dir:
            repo_dir = os.path.join(os.getcwd(), owner+','+repo)
        utils.makedirs(os.path.dirname(repo_dir))
        if not os.path.exists(os.path.join(repo_dir,'.git')):
            utils.check_call("git","clone",
                "--depth=1",
                "--branch=%s"%(branch),
                url_format%{'owner':owner,'repo':repo},
                repo_dir)
            os.chdir(repo_dir)
        else:
            os.chdir(repo_dir)
            utils.check_call("git","pull",
                # "--depth=1", # Can't do depth as we get merge errors.
                "--quiet","--no-recurse-submodules")
        if commit:
            utils.check_call("git","checkout","-qf",commit)
        if os.path.exists(os.path.join('.git','modules')):
            if sys.platform == 'win32':
                utils.check_call('dir',os.path.join('.git','modules'))
            else:
                utils.check_call('ls','-la',os.path.join('.git','modules'))
        if submodules:
            utils.check_call("git","submodule","--quiet","update",
                "--quiet","--init","--recursive",
                )
            utils.check_call("git","submodule","--quiet","foreach","git","fetch")
        return repo_dir

class parallel_call(threading.Thread):
    '''
    Runs a synchronous command in a thread waiting for it to complete.
    '''
    
    def __init__(self, *command, **kargs):
        super(parallel_call,self).__init__()
        self.command = command
        self.command_kargs = kargs
        self.start()
    
    def run(self):
        self.result = utils.call(*self.command, **self.command_kargs)
    
    def join(self):
        super(parallel_call,self).join()
        if self.result != 0:
            raise(SystemCallError(self.command, self.result))

def set_arg(args, k, v = None):
    if not args.get(k):
        args[k] = v
    return args[k]

class script_common(object):
    '''
    Main script to run continuous integration.
    '''

    def __init__(self, ci_klass, **kargs):
        self.ci = ci_klass(self)

        opt = optparse.OptionParser(
            usage="%prog [options] [commands]")

        #~ Debug Options:
        opt.add_option( '--debug-level',
            help="debugging level; controls the amount of debugging output printed",
            type='int' )
        opt.add_option( '-j',
            help="maximum number of parallel jobs to use for building with b2",
            type='int', dest='jobs')
        opt.add_option('--branch')
        opt.add_option('--commit')
        kargs = self.init(opt,kargs)
        kargs = self.ci.init(opt, kargs)
        set_arg(kargs,'debug_level',0)
        set_arg(kargs,'jobs',2)
        set_arg(kargs,'branch',None)
        set_arg(kargs,'commit',None)
        set_arg(kargs,'repo',None)
        set_arg(kargs,'repo_dir',None)
        set_arg(kargs,'actions',None)
        set_arg(kargs,'pull_request', None)

        #~ Defaults
        for (k,v) in kargs.iteritems():
            setattr(self,k,v)
        ( _opt_, self.actions ) = opt.parse_args(None,self)
        if not self.actions or self.actions == []:
            self.actions = kargs.get('actions',None)
        if not self.actions or self.actions == []:
            self.actions = [ 'info' ]
        if not self.repo_dir:
            self.repo_dir = os.getcwd()
        self.build_dir = os.path.join(os.path.dirname(self.repo_dir), "build")
        
        # API keys.
        self.bintray_key = os.getenv('BINTRAY_KEY')

        try:
            self.start()
            self.command_info()
            self.main()
            utils.print_call_stats()
        except:
            utils.print_call_stats()
            raise
    
    def init(self, opt, kargs):
        return kargs
    
    def start(self):
        pass

    def main(self):
        for action in self.actions:
            action_m = "command_"+action.replace('-','_')
            ci_command = getattr(self.ci, action_m, None)
            ci_script = getattr(self, action_m, None)
            if ci_command or ci_script:
                utils.log( "### %s.."%(action) )
                if os.path.exists(self.repo_dir):
                    os.chdir(self.repo_dir)
                if ci_command:
                    ci_command()
                elif ci_script:
                    ci_script()
        
    def b2( self, *args, **kargs ):
        cmd = ['b2','--debug-configuration', '-j%s'%(self.jobs)]
        cmd.extend(args)

        if 'toolset' in kargs:
            cmd.append('toolset=' + kargs['toolset'])

        if 'parallel' in kargs:
            return parallel_call(*cmd)
        else:
            return utils.check_call(*cmd)

    # Common test commands in the order they should be executed..
    
    def command_info(self):
        pass
    
    def command_install(self):
        utils.makedirs(self.build_dir)
        os.chdir(self.build_dir)
    
    def command_install_toolset(self, toolset):
        if self.ci and hasattr(self.ci,'install_toolset'):
            self.ci.install_toolset(toolset)
    
    def command_before_build(self):
        pass

    def command_build(self):
        pass

    def command_before_cache(self):
        pass

    def command_after_success(self):
        pass

class ci_cli(object):
    '''
    This version of the script provides a way to do manual building. It sets up
    additional environment and adds fetching of the git repos that would
    normally be done by the CI system.
    
    The common way to use this variant is to invoke something like:
    
        mkdir ci
        cd ci
        python path-to/library_test.py --branch=develop [--repo=mylib] ...
    
    Status: In working order.
    '''
    
    def __init__(self,script):
        if sys.platform == 'darwin':
            # Requirements for running on OSX:
            # https://www.stack.nl/~dimitri/doxygen/download.html#srcbin
            # https://tug.org/mactex/morepackages.html
            doxygen_path = "/Applications/Doxygen.app/Contents/Resources"
            if os.path.isdir(doxygen_path):
                os.environ["PATH"] = doxygen_path+':'+os.environ['PATH']
        self.script = script
        self.repo_dir = os.getcwd()
        self.exit_result = 0
    
    def init(self, opt, kargs):
        kargs['actions'] = [
            # 'clone',
            'install',
            'before_build',
            'build',
            'before_cache',
            'finish'
            ]
        return kargs
    
    def finish(self, result):
        self.exit_result = result
    
    def command_finish(self):
        exit(self.exit_result)

class ci_travis(object):
    '''
    This variant build releases in the context of the Travis-CI service.
    '''
    
    def __init__(self,script):
        self.script = script
    
    def init(self, opt, kargs):
        set_arg(kargs,'repo_dir', os.getenv("TRAVIS_BUILD_DIR"))
        set_arg(kargs,'branch', os.getenv("TRAVIS_BRANCH"))
        set_arg(kargs,'commit', os.getenv("TRAVIS_COMMIT"))
        set_arg(kargs,'repo', os.getenv("TRAVIS_REPO_SLUG").split("/")[1])
        set_arg(kargs,'pull_request',
            os.getenv('TRAVIS_PULL_REQUEST') \
                if os.getenv('TRAVIS_PULL_REQUEST') != 'false' else None)
        return kargs
    
    def finish(self, result):
        exit(result)
    
    def install_toolset(self, toolset):
        '''
        Installs specific toolset on CI system.
        '''
        info = toolset_info[toolset]
        if sys.platform.startswith('linux'):
            os.chdir(self.script.build_dir)
            if 'ppa' in info:
                for ppa in info['ppa']:
                    utils.check_call(
                        'sudo','add-apt-repository','--yes',ppa)
            if 'deb' in info:
                utils.make_file('sources.list',
                    "deb %s"%(' '.join(info['deb'])),
                    "deb-src %s"%(' '.join(info['deb'])))
                utils.check_call('sudo','bash','-c','cat sources.list >> /etc/apt/sources.list')
            if 'apt-key' in info:
                for key in info['apt-key']:
                    utils.check_call('wget',key,'-O','apt.key')
                    utils.check_call('sudo','apt-key','add','apt.key')
            utils.check_call(
                'sudo','apt-get','update','-qq')
            utils.check_call(
                'sudo','apt-get','install','-qq',info['package'])
            if 'debugpackage' in info and info['debugpackage']:
                utils.check_call(
                    'sudo','apt-get','install','-qq',info['debugpackage'])

    # Travis-CI commands in the order they are executed. We need
    # these to forward to our common commands, if they are different.
    
    def command_before_install(self):
        pass
    
    def command_install(self):
        self.script.command_install()

    def command_before_script(self):
        self.script.command_before_build()

    def command_script(self):
        self.script.command_build()

    def command_before_cache(self):
        self.script.command_before_cache()

    def command_after_success(self):
        self.script.command_after_success()

    def command_after_failure(self):
        pass

    def command_before_deploy(self):
        pass

    def command_after_deploy(self):
        pass

    def command_after_script(self):
        pass

class ci_circleci(object):
    '''
    This variant build releases in the context of the CircleCI service.
    '''
    
    def __init__(self,script):
        self.script = script
    
    def init(self, opt, kargs):
        set_arg(kargs,'repo_dir', os.path.join(os.getenv("HOME"),os.getenv("CIRCLE_PROJECT_REPONAME")))
        set_arg(kargs,'branch', os.getenv("CIRCLE_BRANCH"))
        set_arg(kargs,'commit', os.getenv("CIRCLE_SHA1"))
        set_arg(kargs,'repo', os.getenv("CIRCLE_PROJECT_REPONAME").split("/")[1])
        set_arg(kargs,'pull_request', os.getenv('CIRCLE_PR_NUMBER'))
        return kargs
    
    def finish(self, result):
        exit(result)
    
    def command_machine_post(self):
        # Apt update for the pckages installs we'll do later.
        utils.check_call('sudo','apt-get','-qq','update')
        # Need PyYAML to read Travis yaml in a later step.
        utils.check_call("pip","install","--user","PyYAML")
    
    def command_checkout_post(self):
        os.chdir(self.script.repo_dir)
        utils.check_call("git","submodule","update","--quiet","--init","--recursive")
    
    def command_dependencies_pre(self):
        # Read in .travis.yml for list of packages to install
        # as CircleCI doesn't have a convenient apt install method.
        import yaml
        utils.check_call('sudo','-E','apt-get','-yqq','update')
        utils.check_call('sudo','apt-get','-yqq','purge','texlive*')
        with open(os.path.join(self.script.repo_dir,'.travis.yml')) as yml:
            travis_yml = yaml.load(yml)
            utils.check_call('sudo','apt-get','-yqq',
                '--no-install-suggests','--no-install-recommends','--force-yes','install',
                *travis_yml['addons']['apt']['packages'])
    
    def command_dependencies_override(self):
        self.script.command_install()
    
    def command_dependencies_post(self):
        pass
    
    def command_database_pre(self):
        pass
    
    def command_database_override(self):
        pass
    
    def command_database_post(self):
        pass
    
    def command_test_pre(self):
        self.script.command_install()
        self.script.command_before_build()
    
    def command_test_override(self):
        # CircleCI runs all the test subsets. So in order to avoid
        # running the after_success we do it here as the build step
        # will halt accordingly.
        self.script.command_build()
        self.script.command_before_cache()
        self.script.command_after_success()
    
    def command_test_post(self):
        pass

class ci_appveyor(object):
    
    def __init__(self,script):
        self.script = script
    
    def init(self, opt, kargs):
        set_arg(kargs,'repo_dir',os.getenv("APPVEYOR_BUILD_FOLDER"))
        set_arg(kargs,'branch',os.getenv("APPVEYOR_REPO_BRANCH"))
        set_arg(kargs,'commit',os.getenv("APPVEYOR_REPO_COMMIT"))
        set_arg(kargs,'repo',os.getenv("APPVEYOR_REPO_NAME").split("/")[1])
        set_arg(kargs,'address_model',os.getenv("PLATFORM",None))
        set_arg(kargs,'variant',os.getenv("CONFIGURATION","debug"))
        set_arg(kargs,'pull_request', os.getenv('APPVEYOR_PULL_REQUEST_NUMBER'))
        return kargs
    
    def finish(self, result):
        exit(result)
    
    # Appveyor commands in the order they are executed. We need
    # these to forward to our common commands, if they are different.
    
    def command_install(self):
        self.script.command_install()
    
    def command_before_build(self):
        os.chdir(self.script.repo_dir)
        utils.check_call("git","submodule","update","--quiet","--init","--recursive")
        self.script.command_before_build()
    
    def command_build_script(self):
        self.script.command_build()
    
    def command_after_build(self):
        self.script.command_before_cache()
    
    def command_before_test(self):
        pass
    
    def command_test_script(self):
        pass
    
    def command_after_test(self):
        pass
    
    def command_on_success(self):
        self.script.command_after_success()
    
    def command_on_failure(self):
        pass
    
    def command_on_finish(self):
        pass

def main(script_klass):
    if os.getenv('TRAVIS', False):
        script_klass(ci_travis)
    elif os.getenv('CIRCLECI', False):
        script_klass(ci_circleci)
    elif os.getenv('APPVEYOR', False):
        script_klass(ci_appveyor)
    else:
        script_klass(ci_cli)
