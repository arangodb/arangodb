#!/usr/bin/python

# Copyright Abel Sinkovics (abel@sinkovics.hu)  2015.
# Distributed under the Boost Software License, Version 1.0.
#    (See accompanying file LICENSE_1_0.txt or copy at
#          http://www.boost.org/LICENSE_1_0.txt)

import sys
import argparse
import re
import os

def remove_last_dot(s):
  if s.endswith('.'):
    return s[:-1]
  else:
    return s

def remove_newline(s):
  return re.sub('[\r\n]', '', s)

def is_definition(s):
  cmd = s.strip()
  
  def_prefixes = ['#include ', 'using ', 'struct ', 'template ']
  return any([cmd.startswith(s) for s in def_prefixes]) or cmd.endswith(';')

def prefix_lines(prefix, s):
  return '\n'.join(['%s%s' % (prefix, l) for l in s.split('\n')])

def protect_metashell(s):
  if s.startswith('#include <metashell'):
    return '#ifdef __METASHELL\n%s\n#endif' % (s)
  else:
    return s

def parse_md(qbk):
  sections = []
  defs = []
  current_section = ''
  in_cpp_snippet = False
  numbered_section_header = re.compile('^\[section *([0-9.]+)')
  metashell_command = re.compile('^> [^ ]')
  metashell_prompt = re.compile('^(\.\.\.|)>')
  msh_cmd = ''
  for l in qbk:
    if l.startswith('  '):
      ll = l[2:]
      if not in_cpp_snippet:
        in_msh_cpp_snippet = True
      if in_msh_cpp_snippet:
        if metashell_command.match(ll) or msh_cmd != '':
          cmd = metashell_prompt.sub('', remove_newline(ll))
          if msh_cmd != '':
            msh_cmd = msh_cmd + '\n'
          msh_cmd = msh_cmd + cmd
          if msh_cmd.endswith('\\'):
            msh_cmd = msh_cmd[:-1].strip() + ' '
          else:
            if not is_definition(msh_cmd):
              msh_cmd = '// query:\n%s' % (prefix_lines('//   ', msh_cmd))
            defs.append((current_section, protect_metashell(msh_cmd.strip())))
            msh_cmd = ''
        elif not in_cpp_snippet:
          in_msh_cpp_snippet = False
      in_cpp_snippet = True
    else:
      in_cpp_snippet = False
      m = numbered_section_header.match(l)
      if m:
        current_section = remove_last_dot(m.group(1)).replace('.', '_')
        sections.append(current_section)

  sections.sort(key = lambda s: [int(n) for n in s.split('_')])
  return (sections, defs)

def delete_old_headers(path):
  for f in os.listdir(path):
    if f.endswith('.hpp'):
      os.remove(os.path.join(path, f))

def gen_headers(sections, defs, path):
  files = {}

  prev_section = ''
  for s in sections:
    prev_name = prev_section.replace('_', '.')
    include_guard = 'BOOST_METAPARSE_GETTING_STARTED_%s_HPP' % (s)
    if prev_section == '':
      prev_include = ''
    else:
      prev_include = \
        '// Definitions before section {0}\n'.format(prev_name) + \
        '#include "{0}.hpp"\n'.format(prev_section) + \
        '\n'

    files[os.path.join(path, s + '.hpp')] = \
      '#ifndef {0}\n'.format(include_guard) + \
      '#define {0}\n'.format(include_guard) + \
      '\n' + \
      '// Automatically generated header file\n' + \
      '\n' + \
      prev_include + \
      '// Definitions of section {0}\n'.format(prev_name) + \
      '\n'.join( \
        ['%s\n' % (d) for (sec, d) in defs if sec == prev_section] \
      ) + \
      '\n' + \
      '#endif\n' + \
      '\n'
    prev_section = s
  return files

def remove_metashell_protection(s):
  prefix = '#ifdef __METASHELL\n'
  suffix = '#endif'
  return \
    s[len(prefix):-len(suffix)] \
    if s.startswith(prefix) and s.endswith(suffix) \
    else s

def make_code_snippet(s):
  return '\n'.join(['  {0}'.format(l) for l in s.split('\n')])

def what_we_have_so_far_docs(doc_dir, qbk, defs, sections):
  files = {}
  so_far = ''
  sections_with_definition = []
  for s in sections:
    if so_far != '':
      files[os.path.join(doc_dir, 'before_{0}.qbk'.format(s))] = \
        '[#before_{0}]\n[\'Definitions before section {1}]\n\n{2}\n'.format(
          s,
          s.replace('_', '.') + '.',
          so_far
        )
      sections_with_definition.append(s)

    so_far = so_far + '\n'.join([
      '{0}\n'.format(make_code_snippet(remove_metashell_protection(d)))
      for (sec, d) in defs
      if sec == s and not d.startswith('//')
    ])

  is_section = re.compile('^\[section (([0-9]\.)+)')
  note_prefix = \
    '[note Note that you can find everything that has been included and' \
    ' defined so far [link before_'

  in_definitions_before_each_section = False

  result = []
  for l in qbk:
    if in_definitions_before_each_section:
      if l.strip() == '[endsect]':
        in_definitions_before_each_section = False
        result.append(l)
    elif l.strip() == '[section Definitions before each section]':
      in_definitions_before_each_section = True
      result.append(l)
      result.append('\n')
      for s in sections_with_definition:
        result.append('[include before_{0}.qbk]\n'.format(s))
      result.append('\n')
    elif not l.startswith(note_prefix):
      result.append(l)
      m = is_section.match(l)
      if m:
        section_number = m.group(1).replace('.', '_')[:-1]
        if section_number in sections_with_definition:
          result.append('{0}{1} here].]\n'.format(note_prefix, section_number))

  return (files, result)

def strip_not_finished_line(s):
  s = s.strip()
  return s[:-1] if s.endswith('\\') else s

def make_copy_paste_friendly(lines):
  result = []
  for l in lines:
    if l.startswith('> '):
      result.append(l[2:])
    elif l.startswith('...> '):
      result[-1] = strip_not_finished_line(result[-1]) + l[5:].lstrip()
  return result

def extract_code_snippets(qbk, fn_base):
  code_prefix = '  '

  files = {}

  result = []
  in_cpp_code = False
  counter = 0
  in_copy_paste_friendly_examples = False
  skip_empty_lines = False
  for l in qbk:
    if l.strip() != '' or not skip_empty_lines:
      skip_empty_lines = False
      if in_copy_paste_friendly_examples:
        if 'endsect' in l:
          in_copy_paste_friendly_examples = False
          result.append('\n')
          result.extend([
            '[include {0}_{1}.qbk]\n'.format(re.sub('^.*/', '', fn_base), i) \
            for i in range(0, counter)
          ])
          result.append('\n')
          result.append(l)
          in_copy_paste_friendly_examples = False
      elif '[section Copy-paste friendly code examples]' in l:
        in_copy_paste_friendly_examples = True
        result.append(l)
      elif 'copy-paste friendly version' in l:
        skip_empty_lines = True
      else:
        result.append(l)
  
        if in_cpp_code:
          if not l.startswith(code_prefix):
            in_cpp_code = False
            if len(code) > 1:
              f = '{0}_{1}'.format(fn_base, counter)
              basename_f = re.sub('^.*/', '', f)
              files['{0}.qbk'.format(f)] = \
                '[#{0}]\n\n{1}\n'.format(
                  basename_f,
                  ''.join(
                    [code_prefix + s for s in make_copy_paste_friendly(code)]
                  )
                )
              result.append(
                '[link {0} copy-paste friendly version]\n'.format(basename_f)
              )
              result.append('\n')
              counter = counter + 1
          elif \
              l.startswith(code_prefix + '> ') \
              or l.startswith(code_prefix + '...> '):
            code.append(l[len(code_prefix):])
        elif l.startswith(code_prefix):
          in_cpp_code = True
          code = [l[len(code_prefix):]]

  return (files, result)

def write_file(fn, content):
  with open(fn, 'w') as f:
    f.write(content)

def write_files(files):
  for fn in files:
    write_file(fn, files[fn])

def main():
  desc = 'Generate headers with the definitions of a Getting Started guide'
  parser = argparse.ArgumentParser(description=desc)
  parser.add_argument(
    '--src',
    dest='src',
    default='doc/getting_started.qbk',
    help='The .qbk source of the Getting Started guide'
  )
  parser.add_argument(
    '--dst',
    dest='dst',
    default='example/getting_started',
    help='The target directory to generate into (all headers in that directory will be deleted!)'
  )

  args = parser.parse_args()

  qbk = open(args.src, 'r').readlines()

  delete_old_headers(args.dst)
  doc_dir = os.path.dirname(args.src)

  (sections, defs) = parse_md(qbk)
  files1 = gen_headers(sections, defs, args.dst)
  (files2, qbk) = what_we_have_so_far_docs(doc_dir, qbk, defs, sections)
  (files3, qbk) = \
    extract_code_snippets(
      qbk,
      args.src[:-4] if args.src.endswith('.qbk') else args.src
    )

  write_files(files1)
  write_files(files2)
  write_files(files3)
  write_file(args.src, ''.join(qbk))

if __name__ == "__main__":
  main()

