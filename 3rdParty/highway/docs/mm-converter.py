#!/usr/bin/env python3

# set correct links (pandoc did not deal with github links properly)

import os
import re
import subprocess

regex_pdf_links1 = re.compile(r'`(.*)\<g3doc\/(.*)\.pdf\>`__', 
                            re.M | re.X) # Multiline and Verbose
regex_md_links = re.compile(r'`(.*)\<g3doc\/(.*)\.md\>`__', 
                            re.M | re.X) # Multiline and Verbose
regex_md_links2 = re.compile(r'`(.*)\n(.*)\<g3doc\/(.*)\.md\>`__', 
                            re.M | re.X) # Multiline and Verbose
regex_pdf_links2 = re.compile(r'`(.*)\n\s+(.*)\<g3doc\/(.*)\.pdf\>`__', 
                            re.M | re.X) # Multiline and Verbose

def remove_links_to_index2(data):
    # remove liks to the index, they are useless in py4web docs
    data = data
    print(re.search(regex_pdf_links2, data))
    return re.sub(regex_pdf_links2, 
                  r':download:`\1 \2<g3doc/\3.pdf>`',
                  data)

def remove_links_to_index(data):
    # remove liks to the index, they are useless in py4web docs
    data = data
    print(re.search(regex_pdf_links1, data))
    return re.sub(regex_pdf_links1, 
                  r':download:`\1<g3doc/\2.pdf>`',
                  data)

def rewrite_md_links(data):
    # remove liks to the index, they are useless in py4web docs
    data = data
    print(re.search(regex_md_links, data))
    data = re.sub(regex_md_links, 
                  r'`\1<\2.html>`__',
                  data)
    data = re.sub(regex_md_links2, 
                  r'`\1 \2<\3.html>`__',
                  data)
    return data


docs_on_pages = [
    'README.md',
    'quick_reference.md',
    'design_philosophy.md',
    'impl_details.md',
    'faq.md',
    'release_testing_process.md'
]

def convert2md(file):
    print(f"    Working on file {file}")
    file = os.path.join('g3doc', file)
    data = open(file, 'r').read()
    write_files(file, data)
    
def write_files(file, data):
    for extension in ['rst']:
        ext_dir = os.getcwd()
        md_dir = os.path.join(os.getcwd(), 'g3doc')
        if not os.path.isdir(ext_dir):
            os.mkdir(ext_dir)
        ext_file = os.path.join(ext_dir , os.path.splitext(os.path.basename(file))[0] + "." + extension)
        md_file = os.path.join(md_dir , os.path.splitext(os.path.basename(file))[0] + ".md")
        print(f'writing {ext_file}')
        if os.path.exists(ext_file):
            os.unlink(ext_file)
        with open(ext_file, 'w') as handler:
            write_format(extension, ext_file, handler, md_file, data)


def write_format(extension, ext_file, handler, md_file, data):
    if extension =='md':
            handler.write(data)
    elif extension =='rst':
        try:
            subprocess.call(['pandoc', '-s', md_file, '-f', 'markdown', '-t', 'rst', '-o', ext_file])
            data = open(ext_file, 'r').read() 
            data = remove_links_to_index(data)
            data = remove_links_to_index2(data)
            data = rewrite_md_links(data)
            handler.write(data)
            # Open a file for writing
            # with open('tmp.txt', 'w') as f:
                # Call the subprocess and redirect the output to the file
                # subprocess.call(['awk', '{ gsub(/<g3doc\//, "<"); print }', ext_file], stdout=f)
                # os.system('mv tmp.txt ' + ext_file)
 
        except FileNotFoundError:
            print("\n **** ERROR ****: you need the Pandoc module installed!")
            exit(0)
    elif extension =='html':
        try:
            subprocess.call(['pandoc', '-s', md_file, '-f', 'markdown', '-t', 'html', '-o', ext_file,  '--highlight-style=kate'])
        except FileNotFoundError:
            print("\n **** ERROR ****: you need the Pandoc module installed!")
            exit(0)


if __name__ == "__main__":
    for doc in docs_on_pages:
        print(doc)
        convert2md(doc)
