#!/usr/bin/python
import csv, sys, os.path, re, urllib2

# TODO: Fix and update this script so that it can process the input file again
filepath = 'LICENSES-OTHER-COMPONENTS.md'

name = ''
version = ''
license = ''
licenseName = ''
licenseId = ''

csvfile = open('license.csv', 'w')
writer = csv.writer(csvfile, delimiter=',', quotechar='"', quoting=csv.QUOTE_ALL)

ccomment = re.compile(r"([^#]+)\n(#|\()", re.MULTILINE)

def generateLine():
    global name, version, license, licenseName, licenseId
    print name

    if licenseName != "" and licenseId == "":
        print "license id missing"
        sys.exit(1)
        
    html = ""
    version = version.replace(" ", "_")

    if True or licenseId != "Apache-2.0":
        if licenseId == "":
            licenseId = "NON-STANDARD"

        try:
            if license.startswith("see "):
                html = license[4:]
            else:
                response = urllib2.urlopen(license)
                html = unicode(response.read(), "utf-8", "ignore").replace("\\", "").encode('latin-1','replace')

                if html.startswith("/*"):
                    m = ccomment.match(html)

                    if m:
                        html = m.groups()[0]

                elif html.startswith("// "):
                    m = ccomment.match(html)

                    if m:
                        html = m.groups()[0]

        except:
            print "cannot load %s" % license

    writer.writerow([name, version, licenseId, html])

with open(filepath) as fp:  
   line = fp.readline()
   cnt = 1

   while line:
       if line.startswith("* Name: "):
         if name != "":
             generateLine()

         name = line[8:].strip()
         version = ''
         license = ''
         licenseName = ''
         licenseId = ''
       elif line.startswith("* Version: "):
         version = line[11:].strip()
       elif line.startswith("* License: "):
         license = line[11:].strip()
       elif line.startswith("* License Name: "):
         licenseName = line[16:].strip()
       elif line.startswith("* License Id: "):
         licenseId = line[14:].strip()

       line = fp.readline()
       cnt += 1

if name != "":
    generateLine()

csvfile.close()
