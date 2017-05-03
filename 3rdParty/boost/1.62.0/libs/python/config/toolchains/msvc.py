#
# Copyright (c) 2016 Stefan Seefeld
# All rights reserved.
#
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)

from . import append_feature_flag

class features:

    @classmethod
    def init_once(cls, env):
        env.AppendUnique(CCFLAGS = ['-TP', '/Z7', '/W3' ,'/GR', '/MDd', '/Zc:forScope', '/Zc:wchar_t', '/wd4675', '/EHs'])
        env.AppendUnique(LINKFLAGS = ['/subsystem:console'])

    @staticmethod
    def architecture(env, arch):
        if arch:
            flag = {'x86' :    '/MACHINE:X86',
                    'x86_64' : '/MACHINE:X64',}.get(arch)
            if flag:
                append_feature_flag(env, LINKFLAGS = flag)

    @staticmethod
    def optimize(env, optimize):
        #if not optimize or optimize == "no":
        #    append_feature_flag(env, CCFLAGS = "-O0 -fno-inline")
        #elif optimize == "speed":
        #    append_feature_flag(env, CCFLAGS = "-O3 -finline-functions -Wno-inline")
        #elif optimize == "space":
        #    append_feature_flag(env, CCFLAGS = "-Os")
        #else:
            append_feature_flag(env, CCFLAGS = "")

    @staticmethod
    def profile(env, profile):
        #if profile:
        #    append_feature_flag(env, CCFLAGS = "-pg", LINKFLAGS = "-pg")
        #else:
            append_feature_flag(env, CCFLAGS = "", LINKFLAGS = "")

    @staticmethod
    def threading(env, threading):
        #if threading == "multi":
        #    append_feature_flag(env, CCFLAGS = "/MT")
        #else:
        #    append_feature_flag(env, CCFLAGS = "", LINKFLAGS = "")
        pass

    @staticmethod
    def debug(env, debug):
        #if debug:
        #    append_feature_flag(env, CCFLAGS = "-g", CPPDEFINES = [])
        #else:
            append_feature_flag(env, CCFLAGS = "", CPPDEFINES = "NDEBUG")
