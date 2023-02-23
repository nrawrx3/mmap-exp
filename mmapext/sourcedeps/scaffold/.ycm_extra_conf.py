#!/usr/bin/env python2

# ~/.vim and/or ~/.config/nvim
# ----------------------------

import os
#import ycm_core

# For checking if I'm on NixOS
import subprocess as sp
import re

IS_NIXOS = False
GCC_VERSION = '8.1.0'

# Usual include paths
SYSTEM_INCLUDES = ['/usr/include', '/usr/include/c++/{}'.format(GCC_VERSION)]

uname = sp.Popen(['uname', '-a'], stdout=sp.PIPE)
uname_out, uname_err = uname.communicate()
is_nixos = re.match(r'.*(NixOS).*', uname_out)

# NixOS
if is_nixos:
    print('We are on nix -', uname_out)
    SYSTEM_INCLUDES = [
            '/run/current-system/sw/include',
            '/run/current-system/sw/include/c++/{}'.format(GCC_VERSION)
    ]

# Add more flags to this list as required
flags = [
    '-Wall',
    '-Wextra',
    '-Werror',
    '-Wno-variadic-macros',
    '-fexceptions',
    '-std=c++14',
    '-x', 'c++',
    '-I.',
    '-Iinclude'
]

# Append the includes
for i in SYSTEM_INCLUDES:
    flags.append('-isystem')
    flags.append(i)


# Set this to the absolute path to the folder (NOT the file!) containing the
# compile_commands.json file to use that instead of 'flags'. See here for
# more details: http://clang.llvm.org/docs/JSONCompilationDatabase.html
#
# Most projects will NOT need to set this to anything; you can just change the
# 'flags' list of compilation flags. Notice that YCM itself uses that approach.
compilation_database_folder = ''

if compilation_database_folder:
  database = ycm_core.CompilationDatabase( compilation_database_folder )
else:
  database = None


def DirectoryOfThisScript():
  return os.path.dirname( os.path.abspath( __file__ ) )


def MakeRelativePathsInFlagsAbsolute( flags, working_directory ):
  if not working_directory:
    return list( flags )
  new_flags = []
  make_next_absolute = False
  path_flags = [ '-isystem', '-I', '-iquote', '--sysroot=' ]
  for flag in flags:
    new_flag = flag

    if make_next_absolute:
      make_next_absolute = False
      if not flag.startswith( '/' ):
        new_flag = os.path.join( working_directory, flag )

    for path_flag in path_flags:
      if flag == path_flag:
        make_next_absolute = True
        break

      if flag.startswith( path_flag ):
        path = flag[ len( path_flag ): ]
        new_flag = path_flag + os.path.join( working_directory, path )
        break

    if new_flag:
      new_flags.append( new_flag )
  return new_flags


def FlagsForFile( filename ):
  if database:
    # Bear in mind that compilation_info.compiler_flags_ does NOT return a
    # python list, but a "list-like" StringVec object
    compilation_info = database.GetCompilationInfoForFile( filename )
    final_flags = MakeRelativePathsInFlagsAbsolute(
      compilation_info.compiler_flags_,
      compilation_info.compiler_working_dir_ )
  else:
    relative_to = DirectoryOfThisScript()
    final_flags = MakeRelativePathsInFlagsAbsolute( flags, relative_to )

  return {
    'flags': final_flags,
    'do_cache': True
  }
