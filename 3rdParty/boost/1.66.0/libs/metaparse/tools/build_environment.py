#!/usr/bin/python

# Copyright Abel Sinkovics (abel@sinkovics.hu) 2016.
# Distributed under the Boost Software License, Version 1.0.
#    (See accompanying file LICENSE_1_0.txt or copy at
#          http://www.boost.org/LICENSE_1_0.txt)

import os
import subprocess
import json
import argparse


def load_json(filename):
    with open(filename, 'r') as f:
        return json.load(f)


class ChildProcess:
    def __init__(self, cmd, cwd = os.getcwd()):
        self.cmd = cmd
        self.cwd = cwd

    def run(self, cmd):
        cmd_string = ' '.join(cmd)
        print 'Running {0}'.format(cmd_string)
        proc = subprocess.Popen(
            self.cmd + cmd,
            cwd = self.cwd,
            stdout = subprocess.PIPE
        )
        out = proc.communicate()[0]
        if proc.returncode == 0:
            return out
        else:
            raise Exception(
                'Command {0} exited with {1}'.format(
                    cmd_string,
                    proc.returncode
                )
            )

    def in_dir(self, cwd):
        return ChildProcess(self.cmd, cwd)

    def in_subdir(self, subdir):
        return self.in_dir(os.path.join(self.cwd, subdir))


def head_of_master(submodule, git, ref):
    git.run(['fetch'])
    return git.run(['show-ref', ref]).split()[0]


def build_environment(submodules_file, out_dir, git, repo, action, ref):
    submodules = load_json(submodules_file)
    git.run(['clone', repo, out_dir])
    git_in_boost = git.in_dir(out_dir)

    git_in_boost.run(
        ['submodule', 'init', '--'] + [k for k in submodules.keys() if k != '']
    )
    git_in_boost.run(['submodule', 'update'])
    if action == 'update':
        with open(submodules_file, 'w') as f:
            f.write(json.dumps(
                dict([
                    (k, head_of_master(k, git_in_boost.in_subdir(k), ref))
                    for k, v in submodules.iteritems()
                ]),
                sort_keys=True,
                indent=2
            ))
    elif action == 'checkout':
        for name, commit in submodules.iteritems():
            git_in_boost.in_subdir(name).run(['checkout', commit])
    else:
        raise Exception('Invalid action {0}'.format(action))


def main():
    """The main function of the utility"""
    parser = argparse.ArgumentParser(
        description='Manage the build environment of Boost.Metaparse'
    )
    parser.add_argument(
        '--dep_json',
        required=True,
        help='The json file describing the dependencies'
    )
    parser.add_argument(
        '--git',
        required=False,
        default='git',
        help='The git command to use'
    )
    parser.add_argument(
        '--out',
        required=False,
        default='boost',
        help='The directory to clone into'
    )
    parser.add_argument(
        '--action',
        required=True,
        choices=['update', 'checkout'],
        help='The action to do with the dependencies'
    )
    parser.add_argument(
        '--boost_repository',
        required=False,
        default='https://github.com/boostorg/boost.git',
        help='The Boost repository to clone'
    )
    parser.add_argument(
        '--ref',
        required=False,
        default='origin/master',
        help='The reference to set to in update'
    )
    args = parser.parse_args()

    build_environment(
        args.dep_json,
        args.out,
        ChildProcess([args.git]),
        args.boost_repository,
        args.action,
        args.ref
    )


if __name__ == '__main__':
    main()
