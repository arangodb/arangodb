#!/usr/bin/env python3
import csv, json, sys, os.path, re, io

os.system("cd js/node && npx license-checker --relativeLicensePath --json > OUT")

nv_re = re.compile(r'(@?[^@]*)@([^@]*)')
git = "https://raw.githubusercontent.com/arangodb/arangodb/3.7"

with io.open('js/node/OUT') as f:
    licenses = json.load(f)

for name_version, value in licenses.items():
    nv = nv_re.match(name_version)

    if nv:
        name = nv.group(1)
        version = nv.group(2)
        licenseId = value['licenses']
        license = "######################################## ERROR: %s" % (licenseId)
        licenseFile = "%s/js/node/%s" % (git, value['licenseFile'])

        if 'url' in value:
            home = value['url']
        elif 'repository' in value:
            home = value['repository']
        else:
            home = "(none)"

        if name == "xmldom":
            license = "MIT (dual license)"
            licenseId = "MIT"
        elif licenseId == "MIT" or licenseId == "MIT*" or licenseId == "(WTFPL OR MIT)":
            license = "MIT License"
            licenseId = "MIT"
        elif licenseId == "BSD" or licenseId == "BSD*":
            license = 'BSD License'
            licenseId = "BSD"
        elif licenseId == "BSD-2-Clause":
            license = 'BSD 2-clause "Simplified" License'
        elif licenseId == "BSD-3-Clause":
            license = 'BSD 3-clause "New" or "Revised" License'
        elif licenseId == "Apache*":
            license = "Apache License 2.0"
            licenseId = "Apache-2.0"
        elif licenseId == "Apache-2.0":
            license = "Apache License 2.0"
        elif licenseId == "ISC":
            license = "ISC License"

        print("#### %s" % (name))
        print()
        print("Name: %s" % (name))
        print("Version: %s" % (version))
        print("Project Home: %s" % (home))
        print("License: %s" % (licenseFile))
        print("License Name: %s" % (license))
        print("License Id: %s" % (licenseId))
        print()
    else:
        print("#################### ERROR: %s" % name_version)
