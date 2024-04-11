# Copyright (C) 2018 and later: Unicode, Inc. and others.
# License & terms of use: http://www.unicode.org/copyright.html

# Python 2/3 Compatibility (ICU-20299)
# TODO(ICU-20301): Remove this.
from __future__ import print_function

from buildtool import *
from buildtool import locale_dependencies
from buildtool import utils
from buildtool.request_types import *

import os
import sys
import xml.etree.ElementTree as ET


def generate(config, glob, common_vars):
    requests = []

    if len(glob("misc/*")) == 0:
        print("Error: Cannot find data directory; please specify --src_dir", file=sys.stderr)
        exit(1)

    requests += generate_cnvalias(config, glob, common_vars)
    requests += generate_confusables(config, glob, common_vars)
    requests += generate_conversion_mappings(config, glob, common_vars)
    requests += generate_brkitr_brk(config, glob, common_vars)
    requests += generate_stringprep(config, glob, common_vars)
    requests += generate_brkitr_dictionaries(config, glob, common_vars)
    requests += generate_normalization(config, glob, common_vars)
    requests += generate_coll_ucadata(config, glob, common_vars)
    requests += generate_full_unicore_data(config, glob, common_vars)
    requests += generate_unames(config, glob, common_vars)
    requests += generate_ulayout(config, glob, common_vars)
    requests += generate_misc(config, glob, common_vars)
    requests += generate_curr_supplemental(config, glob, common_vars)
    requests += generate_translit(config, glob, common_vars)

    # Res Tree Files
    # (input dirname, output dirname, resfiles.mk path, mk version var, mk source var, use pool file, dep files)
    requests += generate_tree(config, glob, common_vars,
        "locales",
        None,
        "icu-locale-deprecates.xml",
        True,
        [])

    requests += generate_tree(config, glob, common_vars,
        "curr",
        "curr",
        "icu-locale-deprecates.xml",
        True,
        [])

    requests += generate_tree(config, glob, common_vars,
        "lang",
        "lang",
        "icu-locale-deprecates.xml",
        True,
        [])

    requests += generate_tree(config, glob, common_vars,
        "region",
        "region",
        "icu-locale-deprecates.xml",
        True,
        [])

    requests += generate_tree(config, glob, common_vars,
        "zone",
        "zone",
        "icu-locale-deprecates.xml",
        True,
        [])

    requests += generate_tree(config, glob, common_vars,
        "unit",
        "unit",
        "icu-locale-deprecates.xml",
        True,
        [])

    requests += generate_tree(config, glob, common_vars,
        "coll",
        "coll",
        "icu-coll-deprecates.xml",
        False,
        # Depends on timezoneTypes.res and keyTypeData.res.
        # TODO: We should not need this dependency to build collation.
        # TODO: Bake keyTypeData.res into the common library?
        [DepTarget("coll_ucadata"), DepTarget("misc_res"), InFile("unidata/UCARules.txt")])

    requests += generate_tree(config, glob, common_vars,
        "brkitr",
        "brkitr",
        "icu-locale-deprecates.xml",
        False,
        [DepTarget("brkitr_brk"), DepTarget("dictionaries")])

    requests += generate_tree(config, glob, common_vars,
        "rbnf",
        "rbnf",
        "icu-rbnf-deprecates.xml",
        False,
        [])

    requests += [
        ListRequest(
            name = "icudata_list",
            variable_name = "icudata_all_output_files",
            output_file = TmpFile("icudata.lst"),
            include_tmp = False
        )
    ]

    return requests


def generate_cnvalias(config, glob, common_vars):
    # UConv Name Aliases
    input_file = InFile("mappings/convrtrs.txt")
    output_file = OutFile("cnvalias.icu")
    return [
        SingleExecutionRequest(
            name = "cnvalias",
            category = "cnvalias",
            dep_targets = [],
            input_files = [input_file],
            output_files = [output_file],
            tool = IcuTool("gencnval"),
            args = "-s {IN_DIR} -d {OUT_DIR} "
                "{INPUT_FILES[0]}",
            format_with = {}
        )
    ]


def generate_confusables(config, glob, common_vars):
    # CONFUSABLES
    txt1 = InFile("unidata/confusables.txt")
    txt2 = InFile("unidata/confusablesWholeScript.txt")
    cfu = OutFile("confusables.cfu")
    return [
        SingleExecutionRequest(
            name = "confusables",
            category = "confusables",
            dep_targets = [DepTarget("cnvalias")],
            input_files = [txt1, txt2],
            output_files = [cfu],
            tool = IcuTool("gencfu"),
            args = "-d {OUT_DIR} -i {OUT_DIR} "
                "-c -r {IN_DIR}/{INPUT_FILES[0]} -w {IN_DIR}/{INPUT_FILES[1]} "
                "-o {OUTPUT_FILES[0]}",
            format_with = {}
        )
    ]


def generate_conversion_mappings(config, glob, common_vars):
    # UConv Conversion Table Files
    input_files = [InFile(filename) for filename in glob("mappings/*.ucm")]
    output_files = [OutFile("%s.cnv" % v.filename[9:-4]) for v in input_files]
    # TODO: handle BUILD_SPECIAL_CNV_FILES? Means to add --ignore-siso-check flag to makeconv
    return [
        RepeatedOrSingleExecutionRequest(
            name = "conversion_mappings",
            category = "conversion_mappings",
            dep_targets = [],
            input_files = input_files,
            output_files = output_files,
            tool = IcuTool("makeconv"),
            args = "-s {IN_DIR} -d {OUT_DIR} -c {INPUT_FILE_PLACEHOLDER}",
            format_with = {},
            repeat_with = {
                "INPUT_FILE_PLACEHOLDER": utils.SpaceSeparatedList(file.filename for file in input_files)
            }
        )
    ]


def generate_brkitr_brk(config, glob, common_vars):
    # BRK Files
    input_files = [InFile(filename) for filename in glob("brkitr/rules/*.txt")]
    output_files = [OutFile("brkitr/%s.brk" % v.filename[13:-4]) for v in input_files]
    return [
        RepeatedExecutionRequest(
            name = "brkitr_brk",
            category = "brkitr_rules",
            dep_targets = [DepTarget("cnvalias")],
            input_files = input_files,
            output_files = output_files,
            tool = IcuTool("genbrk"),
            args = "-d {OUT_DIR} -i {OUT_DIR} "
                "-c -r {IN_DIR}/{INPUT_FILE} "
                "-o {OUTPUT_FILE}",
            format_with = {},
            repeat_with = {}
        )
    ]


def generate_stringprep(config, glob, common_vars):
    # SPP FILES
    input_files = [InFile(filename) for filename in glob("sprep/*.txt")]
    output_files = [OutFile("%s.spp" % v.filename[6:-4]) for v in input_files]
    bundle_names = [v.filename[6:-4] for v in input_files]
    return [
        RepeatedExecutionRequest(
            name = "stringprep",
            category = "stringprep",
            dep_targets = [InFile("unidata/NormalizationCorrections.txt")],
            input_files = input_files,
            output_files = output_files,
            tool = IcuTool("gensprep"),
            args = "-s {IN_DIR}/sprep -d {OUT_DIR} -i {OUT_DIR} "
                "-b {BUNDLE_NAME} -m {IN_DIR}/unidata -u 3.2.0 {BUNDLE_NAME}.txt",
            format_with = {},
            repeat_with = {
                "BUNDLE_NAME": bundle_names
            }
        )
    ]


def generate_brkitr_dictionaries(config, glob, common_vars):
    # Dict Files
    input_files = [InFile(filename) for filename in glob("brkitr/dictionaries/*.txt")]
    output_files = [OutFile("brkitr/%s.dict" % v.filename[20:-4]) for v in input_files]
    extra_options_map = {
        "brkitr/dictionaries/burmesedict.txt": "--bytes --transform offset-0x1000",
        "brkitr/dictionaries/cjdict.txt": "--uchars",
        "brkitr/dictionaries/khmerdict.txt": "--bytes --transform offset-0x1780",
        "brkitr/dictionaries/laodict.txt": "--bytes --transform offset-0x0e80",
        "brkitr/dictionaries/thaidict.txt": "--bytes --transform offset-0x0e00"
    }
    extra_optionses = [extra_options_map[v.filename] for v in input_files]
    return [
        RepeatedExecutionRequest(
            name = "dictionaries",
            category = "brkitr_dictionaries",
            dep_targets = [],
            input_files = input_files,
            output_files = output_files,
            tool = IcuTool("gendict"),
            args = "-i {OUT_DIR} "
                "-c {EXTRA_OPTIONS} "
                "{IN_DIR}/{INPUT_FILE} {OUT_DIR}/{OUTPUT_FILE}",
            format_with = {},
            repeat_with = {
                "EXTRA_OPTIONS": extra_optionses
            }
        )
    ]


def generate_normalization(config, glob, common_vars):
    # NRM Files
    input_files = [InFile(filename) for filename in glob("in/*.nrm")]
    # nfc.nrm is pre-compiled into C++; see generate_full_unicore_data
    input_files.remove(InFile("in/nfc.nrm"))
    output_files = [OutFile(v.filename[3:]) for v in input_files]
    return [
        RepeatedExecutionRequest(
            name = "normalization",
            category = "normalization",
            dep_targets = [],
            input_files = input_files,
            output_files = output_files,
            tool = IcuTool("icupkg"),
            args = "-t{ICUDATA_CHAR} {IN_DIR}/{INPUT_FILE} {OUT_DIR}/{OUTPUT_FILE}",
            format_with = {},
            repeat_with = {}
        )
    ]


def generate_coll_ucadata(config, glob, common_vars):
    # Collation Dependency File (ucadata.icu)
    input_file = InFile("in/coll/ucadata-%s.icu" % config.coll_han_type)
    output_file = OutFile("coll/ucadata.icu")
    return [
        SingleExecutionRequest(
            name = "coll_ucadata",
            category = "coll_ucadata",
            dep_targets = [],
            input_files = [input_file],
            output_files = [output_file],
            tool = IcuTool("icupkg"),
            args = "-t{ICUDATA_CHAR} {IN_DIR}/{INPUT_FILES[0]} {OUT_DIR}/{OUTPUT_FILES[0]}",
            format_with = {}
        )
    ]


def generate_full_unicore_data(config, glob, common_vars):
    # The core Unicode properties files (pnames.icu, uprops.icu, ucase.icu, ubidi.icu)
    # are hardcoded in the common DLL and therefore not included in the data package any more.
    # They are not built by default but need to be built for ICU4J data,
    # both in the .jar and in the .dat file (if ICU4J uses the .dat file).
    # See ICU-4497.
    if not config.include_uni_core_data:
        return []

    basenames = [
        "pnames.icu",
        "uprops.icu",
        "ucase.icu",
        "ubidi.icu",
        "nfc.nrm"
    ]
    input_files = [InFile("in/%s" % bn) for bn in basenames]
    output_files = [OutFile(bn) for bn in basenames]
    return [
        RepeatedExecutionRequest(
            name = "unicore",
            category = "unicore",
            input_files = input_files,
            output_files = output_files,
            tool = IcuTool("icupkg"),
            args = "-t{ICUDATA_CHAR} {IN_DIR}/{INPUT_FILE} {OUT_DIR}/{OUTPUT_FILE}"
        )
    ]


def generate_unames(config, glob, common_vars):
    # Unicode Character Names
    input_file = InFile("in/unames.icu")
    output_file = OutFile("unames.icu")
    return [
        SingleExecutionRequest(
            name = "unames",
            category = "unames",
            dep_targets = [],
            input_files = [input_file],
            output_files = [output_file],
            tool = IcuTool("icupkg"),
            args = "-t{ICUDATA_CHAR} {IN_DIR}/{INPUT_FILES[0]} {OUT_DIR}/{OUTPUT_FILES[0]}",
            format_with = {}
        )
    ]


def generate_ulayout(config, glob, common_vars):
    # Unicode text layout properties
    basename = "ulayout"
    input_file = InFile("in/%s.icu" % basename)
    output_file = OutFile("%s.icu" % basename)
    return [
        SingleExecutionRequest(
            name = basename,
            category = basename,
            dep_targets = [],
            input_files = [input_file],
            output_files = [output_file],
            tool = IcuTool("icupkg"),
            args = "-t{ICUDATA_CHAR} {IN_DIR}/{INPUT_FILES[0]} {OUT_DIR}/{OUTPUT_FILES[0]}",
            format_with = {}
        )
    ]


def generate_misc(config, glob, common_vars):
    # Misc Data Res Files
    input_files = [InFile(filename) for filename in glob("misc/*.txt")]
    input_basenames = [v.filename[5:] for v in input_files]
    output_files = [OutFile("%s.res" % v[:-4]) for v in input_basenames]
    return [
        RepeatedExecutionRequest(
            name = "misc_res",
            category = "misc",
            dep_targets = [],
            input_files = input_files,
            output_files = output_files,
            tool = IcuTool("genrb"),
            args = "-s {IN_DIR}/misc -d {OUT_DIR} -i {OUT_DIR} "
                "-k -q "
                "{INPUT_BASENAME}",
            format_with = {},
            repeat_with = {
                "INPUT_BASENAME": input_basenames
            }
        )
    ]


def generate_curr_supplemental(config, glob, common_vars):
    # Currency Supplemental Res File
    input_file = InFile("curr/supplementalData.txt")
    input_basename = "supplementalData.txt"
    output_file = OutFile("curr/supplementalData.res")
    return [
        SingleExecutionRequest(
            name = "curr_supplemental_res",
            category = "curr_supplemental",
            dep_targets = [],
            input_files = [input_file],
            output_files = [output_file],
            tool = IcuTool("genrb"),
            args = "-s {IN_DIR}/curr -d {OUT_DIR}/curr -i {OUT_DIR} "
                "-k "
                "{INPUT_BASENAME}",
            format_with = {
                "INPUT_BASENAME": input_basename
            }
        )
    ]


def generate_translit(config, glob, common_vars):
    input_files = [
        InFile("translit/root.txt"),
        InFile("translit/en.txt"),
        InFile("translit/el.txt")
    ]
    dep_files = set(InFile(filename) for filename in glob("translit/*.txt"))
    dep_files -= set(input_files)
    dep_files = list(sorted(dep_files))
    input_basenames = [v.filename[9:] for v in input_files]
    output_files = [
        OutFile("translit/%s.res" % v[:-4])
        for v in input_basenames
    ]
    return [
        RepeatedOrSingleExecutionRequest(
            name = "translit_res",
            category = "translit",
            dep_targets = dep_files,
            input_files = input_files,
            output_files = output_files,
            tool = IcuTool("genrb"),
            args = "-s {IN_DIR}/translit -d {OUT_DIR}/translit -i {OUT_DIR} "
                "-k "
                "{INPUT_BASENAME}",
            format_with = {
            },
            repeat_with = {
                "INPUT_BASENAME": utils.SpaceSeparatedList(input_basenames)
            }
        )
    ]


def generate_tree(
        config,
        glob,
        common_vars,
        sub_dir,
        out_sub_dir,
        xml_filename,
        use_pool_bundle,
        dep_targets):
    requests = []
    category = "%s_tree" % sub_dir
    out_prefix = "%s/" % out_sub_dir if out_sub_dir else ""
    # TODO: Clean this up for curr
    input_files = [InFile(filename) for filename in glob("%s/*.txt" % sub_dir)]
    if sub_dir == "curr":
        input_files.remove(InFile("curr/supplementalData.txt"))
    input_basenames = [v.filename[len(sub_dir)+1:] for v in input_files]
    output_files = [
        OutFile("%s%s.res" % (out_prefix, v[:-4]))
        for v in input_basenames
    ]

    # Generate Pool Bundle
    if use_pool_bundle:
        input_pool_files = [OutFile("%spool.res" % out_prefix)]
        pool_target_name = "%s_pool_write" % sub_dir
        use_pool_bundle_option = "--usePoolBundle {OUT_DIR}/{OUT_PREFIX}".format(
            OUT_PREFIX = out_prefix,
            **common_vars
        )
        requests += [
            SingleExecutionRequest(
                name = pool_target_name,
                category = category,
                dep_targets = dep_targets,
                input_files = input_files,
                output_files = input_pool_files,
                tool = IcuTool("genrb"),
                args = "-s {IN_DIR}/{IN_SUB_DIR} -d {OUT_DIR}/{OUT_PREFIX} -i {OUT_DIR} "
                    "--writePoolBundle -k "
                    "{INPUT_BASENAMES_SPACED}",
                format_with = {
                    "IN_SUB_DIR": sub_dir,
                    "OUT_PREFIX": out_prefix,
                    "INPUT_BASENAMES_SPACED": utils.SpaceSeparatedList(input_basenames)
                }
            ),
        ]
        dep_targets = dep_targets + [DepTarget(pool_target_name)]
    else:
        use_pool_bundle_option = ""

    # Generate Res File Tree
    requests += [
        RepeatedOrSingleExecutionRequest(
            name = "%s_res" % sub_dir,
            category = category,
            dep_targets = dep_targets,
            input_files = input_files,
            output_files = output_files,
            tool = IcuTool("genrb"),
            args = "-s {IN_DIR}/{IN_SUB_DIR} -d {OUT_DIR}/{OUT_PREFIX} -i {OUT_DIR} "
                "{EXTRA_OPTION} -k "
                "{INPUT_BASENAME}",
            format_with = {
                "IN_SUB_DIR": sub_dir,
                "OUT_PREFIX": out_prefix,
                "EXTRA_OPTION": use_pool_bundle_option
            },
            repeat_with = {
                "INPUT_BASENAME": utils.SpaceSeparatedList(input_basenames)
            }
        )
    ]

    # Generate index txt file
    synthetic_locales = set()
    deprecates_xml_path = os.path.join(os.path.dirname(__file__), xml_filename)
    deprecates_xml = ET.parse(deprecates_xml_path)
    for child in deprecates_xml.getroot():
        if child.tag == "alias":
            synthetic_locales.add(child.attrib["from"])
        elif child.tag == "emptyLocale":
            synthetic_locales.add(child.attrib["locale"])
        else:
            raise ValueError("Unknown tag in deprecates XML: %s" % child.tag)
    index_input_files = []
    for f in input_files:
        file_stem = f.filename[f.filename.rfind("/")+1:-4]
        if file_stem == "root":
            continue
        if file_stem in synthetic_locales:
            continue
        index_input_files.append(f)
    cldr_version = locale_dependencies.data["cldrVersion"] if sub_dir == "locales" else None
    index_file_txt = TmpFile("{IN_SUB_DIR}/{INDEX_NAME}.txt".format(
        IN_SUB_DIR = sub_dir,
        **common_vars
    ))
    index_file_target_name = "%s_index_txt" % sub_dir
    requests += [
        IndexTxtRequest(
            name = index_file_target_name,
            category = category,
            input_files = index_input_files,
            output_file = index_file_txt,
            cldr_version = cldr_version
        )
    ]

    # Generate index res file
    index_res_file = OutFile("{OUT_PREFIX}{INDEX_NAME}.res".format(
        OUT_PREFIX = out_prefix,
        **common_vars
    ))
    requests += [
        SingleExecutionRequest(
            name = "%s_index_res" % sub_dir,
            category = category,
            dep_targets = [DepTarget(index_file_target_name)],
            input_files = [],
            output_files = [index_res_file],
            tool = IcuTool("genrb"),
            args = "-s {TMP_DIR}/{IN_SUB_DIR} -d {OUT_DIR}/{OUT_PREFIX} -i {OUT_DIR} "
                "-k "
                "{INDEX_NAME}.txt",
            format_with = {
                "IN_SUB_DIR": sub_dir,
                "OUT_PREFIX": out_prefix
            }
        )
    ]

    return requests
