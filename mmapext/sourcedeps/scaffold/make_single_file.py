# Makes a single file library. Detects circular dependencies as a bonus.

import os
import re

from collections import OrderedDict

top_level_dir = os.path.dirname(os.path.abspath(__file__))

header_include_regex = re.compile(r'''#include ["<]scaffold/([a-zA-Z0-9_/\.]+)[">]|.+''')

def files_in_topdir(dirname):
    return [os.path.join(top_level_dir, dirname, filename)
            for filename in os.listdir(os.path.join(top_level_dir, dirname))]


unique_includes = []
header_includes = {}

header_dir = os.path.join(top_level_dir, 'include', 'scaffold')

header_regex = re.compile(r'''#include ["<]scaffold/([a-zA-Z0-9_/\.]+)[">]''')

for header_file in os.listdir(header_dir):
    if not header_file.endswith('.h'):
        continue

    source = None

    header_paths_included = []

    with open(os.path.join(header_dir, header_file)) as f:
        source = f.read()

    assert source is not None

    # Filter the accessed headers
    for match in re.finditer(header_regex, source):
        if match.start(1) != -1:
            header = match.group(1)
            if header not in unique_includes:
                unique_includes.append(header)
            header_paths_included.append(header)

    header_includes[header_file] = header_paths_included

# print(unique_includes)
# print(header_includes)

def toposort(header_includes, states, current_header, chain):
    states[current_header] = 1 # Scanning

    for included_file in header_includes[current_header]:
        if included_file not in states:
            states[included_file] = 0
            toposort(header_includes, states, included_file, chain)
        elif states[included_file] == 2:
            continue
        else:
            raise RuntimeError('Cyclic dependency for included file {}'.format(included_file))

    chain.append(current_header)
    states[current_header] = 2 # Done


states = {}
chain = []
# Keep adding to chain until all nodes are added
while len(chain) != len(header_includes):
    for file in header_includes:
        if file not in states:
            toposort(header_includes, states, file, chain)


# Add the source in order of the dependencies

header_file_contents = OrderedDict()

for header_file in chain:
    pathname = os.path.join(header_dir, header_file)

    string = None

    with open(pathname) as f:
        string = f.read()

    if string is None:
        raise RuntimeError('Failed to read header {}'.format(header_file))

    source = bytearray()

    for match in re.finditer(header_include_regex, string):
        if not match:
            continue

        if match.start(1) == -1:
            source.extend(match.group(0).encode('utf-8'))
            source.append(ord('\n'))

    header_file_contents[pathname] = source.decode('utf-8')

for filepath, content in header_file_contents.items():
    print('// File - {}\n'.format(filepath))
    print(content)


print("\n\n // Implementations\n\n")


# Get all the headers
all_headers = []

source_file_contents = {}

for pathname in files_in_topdir('src'):
    if not pathname.endswith('.cpp'):
        continue

    string = None
    with open(pathname) as f:
        string = f.read()

    source = bytearray()

    for match in re.finditer(header_include_regex, string):
        if not match:
            continue

        if match.start(1) == -1:
            source.extend(match.group(0).encode('utf-8'))
            source.append(ord('\n'))
        else:
            header_file = match.group(1)
            if header_file not in all_headers:
                all_headers.append(header_file)

    source_file_contents[pathname] = source.decode('utf-8')


print('#if defined(SCAFFOLD_IMPLEMENTATION)\n')

for filepath, content in source_file_contents.items():
    print('// File - {}\n'.format(filepath))
    print(content)

print('\n#endif')
