import sys

from paver.easy import *
import paver.virtual

options(
    virtualenv=Bunch(
        packages_to_install=['pip'],
        paver_command_line="initial"
    )
)

@task
def initial():
    """Initial setup help."""
    venv_command = "Scripts/activate.bat" if sys.platform == 'win32' \
        else "source bin/activate"
        
    print """This is the source for the CommonJS website.

You can build the website, by running these two commands (the
result will be in _site):

%s
paver build
""" % (venv_command)

@task
def build(options):
    """Builds the documentation."""
    if not path("src/growl").exists():
        sh("pip install -r requirements.txt")
    sh("growl.py . ../_site", cwd="docs")
    