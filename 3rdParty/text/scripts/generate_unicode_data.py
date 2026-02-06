#!/usr/bin/env python
import argparse
import urllib
import zipfile
import shutil
import os


# Copyright (C) 2020 T. Zachary Laine
#
# Distributed under the Boost Software License, Version 1.0. (See
# accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)


################################################################################
# NOTE: There is some manual intervention required in
# generate_collation_data.py (marked with TODO).  Handle that before using
# this script.
################################################################################


parser = argparse.ArgumentParser(description='Downloads data files necessary for building Boost.Text\'s Unicode data, and generates those data.  NOTE: There is some manual intervention required in generate_collation_data.py (marked with TODO).  Handle that before using this script.')
parser.add_argument('unicode_version', type=str, help='The X.Y.Z Unicode version from which the data should be generated.  Available versions can be viewed at https://www.unicode.org/Public .')
parser.add_argument('cldr_version', type=str, help='The X[.Y[.Z]] CLDR version from which the data should be generated.  Available versions can be viewed at https://www.unicode.org/Public/cldr .')
parser.add_argument('--icu-dir', type=str, default='', help='The path to icu4c/source/data/coll containing ICU\'s tailoring data.  Without this, the include/boost/text/data headers will not be generated.')
parser.add_argument('--tests', action='store_true', help='Generate the test files associated with the generated Unicode data.')
parser.add_argument('--perf', action='store_true', help='Generate perf-test files instead of regular tests.  Ignored without --tests.')
parser.add_argument('--skip-downloads', action='store_true', help='Don\'t download the data files; just use the ones in this directory.')
args = parser.parse_args()


def version_element(s):
    elements = s.split('.')
    if len(elements) < 2:
        elements.append('0')
    if len(elements) < 3:
        elements.append('0')
    return elements

unicode_major, unicode_minor, unicode_patch = version_element(args.unicode_version)
cldr_major, cldr_minor, cldr_patch = version_element(args.cldr_version)


# Download data

if not args.skip_downloads:

    unicode_ucd_files = [
        'BidiBrackets.txt',
        'BidiCharacterTest.txt',
        'BidiMirroring.txt',
        'BidiTest.txt',
        'DerivedCoreProperties.txt',
        'DerivedNormalizationProps.txt',
        'LineBreak.txt',
        'NormalizationTest.txt',
        'PropList.txt',
        'SpecialCasing.txt',
        'UnicodeData.txt'
    ]
    unicode_auxiliary_files = [
        'GraphemeBreakProperty.txt',
        'GraphemeBreakTest.html',
        'GraphemeBreakTest.txt',
        'LineBreakTest.html',
        'LineBreakTest.txt',
        'SentenceBreakProperty.txt',
        'SentenceBreakTest.html',
        'SentenceBreakTest.txt',
        'WordBreakProperty.txt',
        'WordBreakTest.html',
        'WordBreakTest.txt'
    ]
    unicode_extracted_files = [
        'DerivedBidiClass.txt',
        'DerivedCombiningClass.txt'
    ]

    unicode_files = unicode_ucd_files + unicode_auxiliary_files + unicode_extracted_files

    for f in unicode_ucd_files:
        print 'Downloading {}.'.format(f)
        urllib.urlretrieve(
            'https://www.unicode.org/Public/{}/ucd/{}'.format(args.unicode_version, f), f)

    for f in unicode_auxiliary_files:
        print 'Downloading {}.'.format(f)
        urllib.urlretrieve(
            'https://www.unicode.org/Public/{}/ucd/auxiliary/{}'.format(args.unicode_version, f), f)

    for f in unicode_extracted_files:
        print 'Downloading {}.'.format(f)
        urllib.urlretrieve(
            'https://www.unicode.org/Public/{}/ucd/extracted/{}'.format(args.unicode_version, f), f)

    print 'Downloading {}.'.format('emoji-data.txt')
    urllib.urlretrieve('https://unicode.org/Public/{}/ucd/emoji/emoji-data.txt'.format(args.unicode_version), 'emoji-data.txt')

    path_root = 'https://www.unicode.org/Public/cldr/{}'.format(args.cldr_version)
    for i in [0, 1, 2]:
        zip_file = 'cldr-common-{}.zip'.format(args.cldr_version + '.0' * i)
        full_path = path_root + '/' + zip_file
        print 'Downloading {}.'.format(full_path)
        urllib.urlretrieve(full_path, zip_file)
        print 'Unzipping {}.'.format(zip_file)
        try:
            zip_ref = zipfile.ZipFile(zip_file, 'r')
            zip_ref.extractall('.')
            zip_ref.close()
            break
        except:
            print full_path + ' not found; retrying....'
            pass

    cldr_files = [
        'CollationTest_CLDR_NON_IGNORABLE.txt',
        'CollationTest_CLDR_SHIFTED.txt',
        'FractionalUCA.txt'
    ]

    for f in cldr_files:
        print 'Copying common/uca/{}.'.format(f)
        shutil.copyfile('common/uca/' + f, f)

    shutil.rmtree('common')



# Generate tests

if args.tests:
    if args.perf:
        print 'Generating text segmentation and bidirectional perf tests.'
        os.system('./generate_unicode_break_tests.py --perf')
        print 'Generating normalization perf tests.'
        os.system('./generate_unicode_normalization_tests.py --perf')
        print 'Generating collation perf tests.'
        os.system('./generate_unicode_collation_tests.py --perf')

        print '''You may now want to do:
    mv *cpp ../perf'''
        exit(0)

    print 'Generating text segmentation and bidirectional tests.'
    os.system('./generate_unicode_break_tests.py')
    print 'Generating normalization tests.'
    os.system('./generate_unicode_normalization_tests.py')
    print 'Generating collation tests.'
    os.system('./generate_unicode_collation_tests.py')
    print 'Generating case mapping tests.'
    os.system('./generate_unicode_case_tests.py')
    if args.icu_dir != '':
        print 'Generating tailoring tests.'
        os.system('./generate_unicode_tailoring_data.py {} --tests'.format(args.icu_dir))

    print "Tests were just generated.  Don't forget to re-add the hand adjustments to those tests."
    print '''You may now want to do:
    mv relative_collation_test_* ../test && mv *cpp ../test/generated'''
    exit(0)



# Generate data headers

# If this step fails, look at the notes in generate_unicode_tailoring_data.py
# about possible required fixups to the ICU data to make it work.
if args.icu_dir != '':
    print 'Generating tailoring data headers.'
    os.system('./generate_unicode_tailoring_data.py {}'.format(args.icu_dir))
    print "data/ tailoring headers were just generated.  Don't forget to run generate_tailoring_rule_tests.  Also, don't forget to re-add the hand adjustments to those tests."



# Generate data

print 'Generating text segmentation and bidirectional data.'
os.system('./generate_unicode_breaks.py')
print 'Generating text normalization data.'
os.system('./generate_unicode_normalization_data.py')
print 'Generating text collation data.'
os.system('./generate_unicode_collation_data.py')
print 'Generating text case mapping data.'
os.system('./generate_unicode_case_data.py')


print 'Generating version file.'

version_cpp_form = '''\
// Copyright (C) 2020 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// Warning! This file is autogenerated.
#include <boost/text/data_versions.hpp>


namespace boost {{ namespace text {{

    library_version unicode_version() {{ return {{ {}, {}, {} }}; }}

    library_version cldr_version() {{ return {{ {}, {}, {} }}; }}

}}}}
'''

open('data_versions.cpp', 'w').write(version_cpp_form.format(
    unicode_major, unicode_minor, unicode_patch,
    cldr_major, cldr_minor, cldr_patch
))

print '''You may now want to do:
    mv *cpp ../src && mv *hpp ../include/boost/text/detail'''
