//  -*- mode: groovy-mode

properties([
    parameters([
        booleanParam(
            defaultValue: false,
            description: 'build and run tests on Linux',
            name: 'Linux'
        ),
        booleanParam(
            defaultValue: false,
            description: 'build and run tests on Mac',
            name: 'Mac'
        ),
        booleanParam(
            defaultValue: false,
            description: 'build and run tests in Windows',
            name: 'Windows'
        ),
        booleanParam(
            defaultValue: false,
            description: 'clean build directories',
            name: 'cleanBuild'
        ),
        booleanParam(
            defaultValue: false,
            description: 'build and run tests for community',
            name: 'buildCommunity'
        ),
        booleanParam(
            defaultValue: false,
            description: 'build and run tests for enterprise',
            name: 'buildEnterprise'
        ),
        booleanParam(
            defaultValue: false,
            description: 'run jslint',
            name: 'runJslint'
        ),
        booleanParam(
            defaultValue: false,
            description: 'run resilience tests',
            name: 'runResilience'
        ),
        booleanParam(
            defaultValue: false,
            description: 'run tests',
            name: 'runTests'
        )
    ])
])

// start with empty build directory
cleanBuild = params.cleanBuild

// build all combinations
buildFull = false

// build community
buildCommunity = params.buildCommunity

// build enterprise
buildEnterprise = params.buildEnterprise

// build linux
buildLinux = params.Linux

// build mac
buildMac = params.Mac

// build windows
buildWindows = params.Windows

// run jslint
runJslint = params.runJslint

// run resilience tests
runResilience = params.runResilience

// run tests
runTests = params.runTests

// -----------------------------------------------------------------------------
// --SECTION--                                             CONSTANTS AND HELPERS
// -----------------------------------------------------------------------------

// github repositiory for resilience tests
resilienceRepo = 'https://github.com/arangodb/resilience-tests'

// github repositiory for enterprise version
enterpriseRepo = 'https://github.com/arangodb/enterprise'

// Jenkins credentials for enterprise repositiory
credentials = '8d893d23-6714-4f35-a239-c847c798e080'

// jenkins cache
cacheDir = '/vol/cache/' + env.JOB_NAME.replaceAll('%', '_')

// execute a powershell
def PowerShell(psCmd) {
    bat "powershell.exe -NonInteractive -ExecutionPolicy Bypass -Command \"\$ErrorActionPreference='Stop';[Console]::OutputEncoding=[System.Text.Encoding]::UTF8;$psCmd;EXIT \$global:LastExitCode\""
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       SCRIPTS SCM
// -----------------------------------------------------------------------------

def checkoutCommunity() {
    if (cleanBuild) {
       sh 'rm -rf *'
    }

    retry(3) {
        try {
            checkout scm
            sh 'git clean -f -d -x'
        }
        catch (exc) {
            echo "GITHUB checkout failed, retrying in 5min"
            echo exc.toString()
            sleep 300
        }
    }
}

def checkoutEnterprise() {
    try {
        echo "Trying enterprise branch ${env.BRANCH_NAME}"

        checkout(
            changelog: false,
            poll: false,
            scm: [
                $class: 'GitSCM',
                branches: [[name: "*/${env.BRANCH_NAME}"]],
                doGenerateSubmoduleConfigurations: false,
                extensions: [[$class: 'RelativeTargetDirectory', relativeTargetDir: 'enterprise']],
                submoduleCfg: [],
                userRemoteConfigs: [[credentialsId: credentials, url: enterpriseRepo]]])
    }
    catch (exc) {
        echo "Failed ${env.BRANCH_NAME}, trying enterprise branch devel"

        checkout(
            changelog: false,
            poll: false,
            scm: [
                $class: 'GitSCM',
                branches: [[name: "*/devel"]],
                doGenerateSubmoduleConfigurations: false,
                extensions: [[$class: 'RelativeTargetDirectory', relativeTargetDir: 'enterprise']],
                submoduleCfg: [],
                userRemoteConfigs: [[credentialsId: credentials, url: enterpriseRepo]]])
    }

    sh 'cd enterprise && git clean -f -d -x'
}

def checkoutResilience() {
    checkout(
        changelog: false,
        poll: false,
        scm: [
            $class: 'GitSCM',
            branches: [[name: "*/master"]],
            doGenerateSubmoduleConfigurations: false,
            extensions: [[$class: 'RelativeTargetDirectory', relativeTargetDir: 'resilience']],
            submoduleCfg: [],
            userRemoteConfigs: [[credentialsId: credentials, url: resilienceRepo]]])

    sh 'cd resilience && git clean -f -d -x'
}

def checkCommitMessages() {
    def changeLogSets = currentBuild.changeSets

    for (int i = 0; i < changeLogSets.size(); i++) {
        def entries = changeLogSets[i].items

        for (int j = 0; j < entries.length; j++) {
            def entry = entries[j]

            def author = entry.author
            def commitId = entry.commitId
            def msg = entry.msg
            def timestamp = new Date(entry.timestamp)

            echo msg

            if (msg ==~ /(?i).*\[ci:[^\]]*clean[ \]].*/) {
                echo "using clean build because message contained 'clean'"
                cleanBuild = true
            }

            if (msg ==~ /(?i).*\[ci:[^\]]*no-clean[ \]].*/) {
                echo "using clean build because message contained 'no-clean'"
                cleanBuild = false
            }

            def files = new ArrayList(entry.affectedFiles)

            for (int k = 0; k < files.size(); k++) {
                def file = files[k]
                def editType = file.editType.name
                def path = file.path

                echo "File " + file + ", path " + path
            }
        }
    }

    echo 'Linux: ' + (buildLinux ? 'true' : 'false')
    echo 'Mac: ' + (buildMac ? 'true' : 'false')
    echo 'Windows: ' + (buildWindows ? 'true' : 'false')
    echo 'Clean Build: ' + (cleanBuild ? 'true' : 'false')
    echo 'Build Community: ' + (buildCommunity ? 'true' : 'false')
    echo 'Build Enterprise: ' + (buildEnterprise ? 'true' : 'false')
    echo 'Run Jslint: ' + (runJslint ? 'true' : 'false')
    echo 'Run Resilience: ' + (runResilience ? 'true' : 'false')
    echo 'Run Tests: ' + (runTests ? 'true' : 'false')
}

// -----------------------------------------------------------------------------
// --SECTION--                                                     SCRIPTS STASH
// -----------------------------------------------------------------------------

def stashSourceCode() {
    sh 'rm -f source.*'
    sh 'find -L . -type l -delete'
    sh 'zip -r -1 -x "*tmp" -x ".git" -y -q source.zip *'

    lock('cache') {
        sh 'mkdir -p ' + cacheDir
        sh 'mv -f source.zip ' + cacheDir + '/source.zip'
    }
}

def unstashSourceCode(os) {
    if (os == 'linux' || os == 'mac') {
        sh 'rm -rf *'
    }
    else if (os == 'windows') {
        bat 'del /F /Q *'
    }

    def name = env.JOB_NAME

    if (os == 'linux' || os == 'mac') {
        lock('cache') {
            sh 'scp "jenkins@c1:' + cacheDir + '/source.zip" source.zip'
        }

        sh 'unzip -o -q source.zip'
    }
    else if (os == 'windows') {
        lock('cache') {
            bat 'scp "jenkins@c1:' + cacheDir + '/source.zip" source.zip'
        }

        PowerShell('Expand-Archive -Path source.zip -DestinationPath .')
    }
}

def stashBuild(edition, os) {
    def name = 'build-' + edition + '-' + os + '.zip'

    if (os == 'linux' || os == 'mac') {
        sh 'rm -f ' + name
        sh 'zip -r -1 -y -q ' + name + ' build-' + edition

        lock('cache') {
            sh 'scp ' + name + ' "jenkins@c1:' + cacheDir + '"'
        }
    }
    else if (os == 'windows') {
        bat 'del /F /q ' + name
        PowerShell('Compress -Archive -Path build-' + edition + ' -DestinationPath ' + name)

        lock('cache') {
            bat 'scp ' + name + ' "jenkins@c1:' + cacheDir + '"'
        }
    }
}

def unstashBuild(edition, os) {
    def name = 'build-' + edition + '-' + os + '.zip'

    if (os == 'linux' || os == 'mac') {
        lock('cache') {
            sh 'scp "jenkins@c1:' + cacheDir + '/' + name + '" ' + name
        }

        sh 'unzip -o -q ' + name
    }
    else if (os == 'windows') {
        lock('cache') {
            bat 'scp "jenkins@c1:' + cacheDir + '/' + name + '" ' + name
        }

        PowerShell('Expand-Archive -Path ' + name + ' -DestinationPath .')
    }
}

def stashBinaries(edition, os) {
    def name = 'binaries-' + edition + '-' + os + '.zip'

    if (os == 'linux' || os == 'mac') {
        def dirs = 'build etc Installation/Pipeline js scripts UnitTests utils'

        if (edition == 'community') {
            sh 'zip -r -1 -y -q ' + name + ' ' + dirs
        }
        else if (edition == 'enterprise') {
            sh 'zip -r -1 -y -q ' + name + ' ' + dirs + ' enterprise/js'
        }

        lock('cache') {
            sh 'scp ' + name + ' "jenkins@c1:' + cacheDir + '"'
        }
    }
}

def unstashBinaries(edition, os) {
    def name = 'binaries-' + edition + '-' + os + '.zip'

    sh 'rm -rf *'

    if (os == 'linux' || os == 'mac') {
        lock('cache') {
            sh 'scp "jenkins@c1:' + cacheDir + '/' + name + '" ' + name
        }

        sh 'unzip -o -q ' + name
    }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                     SCRIPTS BUILD
// -----------------------------------------------------------------------------

def buildEdition(edition, os) {
    try {
        if (os == 'linux' || os == 'mac') {
            def tarfile = 'build-' + edition + '-' + os + '.tar.gz'

            if (! cleanBuild) {
                try {
                    unstashBuild(edition, os)
                }
                catch (exc) {
                    echo exc.toString()
                }
            }

            sh './Installation/Pipeline/build_' + edition + '_' + os + '.sh 64'

            stashBuild(edition, os)
        }
        else if (os == 'windows') {
            def builddir = 'build-' + edition + '-' + os

            if (cleanBuild) {
                bat 'del /F /Q build'
            }
            else {
                try {
                    step($class: 'hudson.plugins.copyartifact.CopyArtifact',
                         projectName: "/" + "${env.JOB_NAME}",
                         filter: builddir + '/**')

                    bat 'move ' + builddir + ' build'
                }
                catch (exc) {
                    echo exc.toString()
                }
            }

            PowerShell('. .\\Installation\\Pipeline\\build_' + edition + '_windows.ps1')

            bat 'move build ' + builddir
            archiveArtifacts allowEmptyArchive: true, artifacts: builddir + '/**', defaultExcludes: false
        }
    }
    catch (exc) {
        echo exc.toString()
        throw exc
    }
    finally {
        archiveArtifacts allowEmptyArchive: true, artifacts: 'log-output/**', defaultExcludes: false
    }
}

def buildStepCheck(edition, os, full) {
    if (full && ! buildFull) {
        echo "Not building combination " + os + " " + edition + " "
        return false
    }

    if (os == 'linux' && ! buildLinux) {
        echo "Not building " + os + " version"
        return false
    }

    if (os == 'mac' && ! buildMac) {
        echo "Not building " + os + " version"
        return false
    }

    if (os == 'windows' && ! buildWindows) {
        echo "Not building " + os + " version"
        return false
    }

    if (edition == 'enterprise' && ! buildEnterprise) {
        echo "Not building " + edition + " version"
        return false
    }

    if (edition == 'community' && ! buildCommunity) {
        echo "Not building " + edition + " version"
        return false
    }

    return true
}

def buildStep(edition, os, full) {
    if (! buildStepCheck(edition, os, full)) {
        return {}
    }

    return {
        node(os) {
            unstashSourceCode(os)
            buildEdition(edition, os)

            if (runTests || runResilience) {
                stashBinaries(edition, os)
            }
        }
    }
}

def buildStepParallel() {
    def branches = [:]
    def full = false

    for (edition in ['community', 'enterprise']) {
        for (os in ['linux', 'mac', 'windows']) {
            if (buildStepCheck(edition, os, full)) {
                def name = 'build-' + edition + '-' + os

                branches[name] = buildStep(edition, os, full)
            }
        }
    }

    parallel branches
}

// -----------------------------------------------------------------------------
// --SECTION--                                                     SCRIPTS TESTS
// -----------------------------------------------------------------------------

def jslint() {
    try {
        sh './Installation/Pipeline/test_jslint.sh'
    }
    catch (exc) {
        echo exc.toString()
        currentBuild.result = 'UNSTABLE'
    }
}

def jslintStep() {
    def edition = 'community'
    def os = 'linux'

    if (runJslint) {
        return {
            node(os) {
                echo "Running jslint test"

                try {
                    unstashBinaries(edition, os)
                }
                catch (exc) {
                    echo exc.toString()
                    currentBuild.result = 'UNSTABLE'
                    return
                }
                
                jslint()
            }
        }
    }
}

def testEdition(edition, os, mode, engine) {
    try {
        sh './Installation/Pipeline/test_' + mode + '_' + edition + '_' + engine + '_' + os + '.sh 10'
    }
    catch (exc) {
        echo exc.toString()
        throw exc
    }
    finally {
        archiveArtifacts allowEmptyArchive: true, artifacts: 'log-output/**', defaultExcludes: false
    }
}

def testStepCheck(edition, os, mode, engine, full) {
    if (! runTests) {
        echo "Not running tests"
        return
    }

    if (full && ! buildFull) {
        echo "Not building combination " + os + " " + edition + " "
        return
    }

    if (os == 'linux' && ! buildLinux) {
        echo "Not building " + os + " version"
        return
    }

    if (os == 'mac' && ! buildMac) {
        echo "Not building " + os + " version"
        return
    }

    if (os == 'windows' && ! buildWindows) {
        echo "Not building " + os + " version"
        return
    }

    if (edition == 'enterprise' && ! buildEnterprise) {
        echo "Not building " + edition + " version"
        return
    }

    if (edition == 'community' && ! buildCommunity) {
        echo "Not building " + edition + " version"
        return
    }

    return true
}

def testStepName(edition, os, mode, engine, full) {
    def name = "test-" + mode + '-' + edition + '-' + engine + '-' + os;

    if (! testStepCheck(edition, os, mode, engine, full)) {
        name = "DISABLED-" + name
    }

    return name 
}

def testStep(edition, os, mode, engine, full) {
    return {
        node(os) {
            echo "Running " + mode + " " + edition + " " + engine + " " + os + " test"

            unstashBinaries(edition, os)
            testEdition(edition, os, mode, engine)
        }
    }
}

def testStepParallel() {
    def branches = [:]
    def full = false

    for (edition in ['community', 'enterprise']) {
        for (os in ['linux', 'mac', 'windows']) {
            for (mode in ['cluster', 'singleserver']) {
                for (engine in ['mmfiles', 'rocksdb']) {
                    if (testStepCheck(edition, os, mode, engine, full)) {
                        def name = testStepName(edition, os, mode, engine, full)

                        branches[name] = testStep(edition, os, mode, engine, full)
                    }
                }
            }
        }
    }

    if (runJslint) {
        branches['jslint'] = jslintStep()
    }

    parallel branches
}

def testEditionResilience(edition, os, engine) {
    echo "missing"
    sh "ls -l"
}

def testResilienceStep(os, edition, engine, full) {
    if (! runResilience) {
        echo "Not running resilience tests"
        return
    }

    if (os == 'linux' && ! buildLinux) {
        echo "Not building " + os + " version"
        return
    }

    if (os == 'mac' && ! buildMac) {
        echo "Not building " + os + " version"
        return
    }

    if (os == 'windows' && ! buildWindows) {
        echo "Not building " + os + " version"
        return
    }

    if (edition == 'enterprise' && ! buildEnterprise) {
        echo "Not building " + edition + " version"
        return
    }

    if (edition == 'community' && ! buildCommunity) {
        echo "Not building " + edition + " version"
        return
    }

    node(os) {
        unstashBinaries(edition, os)
        testEditionResilience(edition, os, engine)
    }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                          PIPELINE
// -----------------------------------------------------------------------------

stage('checkout') {
    node('master') {
        checkoutCommunity()
        checkCommitMessages()
        checkoutEnterprise()
        checkoutResilience()
        stashSourceCode()
    }
}

// cmake is very picky about the absolute path. Therefore never put a stage
// into an `if`

stage('build') {
    buildStepParallel()
}

stage('tests') {
    testStepParallel()
}

stage('resilience') {
    parallel(
        'test-resilience-community-mmfiles-linux':    { testResilienceStep('linux',   'community',  'mmfiles', false) },
        'test-resilience-community-mmfiles-mac':      { testResilienceStep('mac',     'community',  'mmfiles', false) },
        'test-resilience-community-mmfiles-windows':  { testResilienceStep('windows', 'community',  'mmfiles', false) },
        'test-resilience-community-rocksdb-linux':    { testResilienceStep('linux',   'community',  'rocksdb', false) },
        'test-resilience-community-rocksdb-mac':      { testResilienceStep('mac',     'community',  'rocksdb', false) },
        'test-resilience-community-rocksdb-windows':  { testResilienceStep('windows', 'community',  'rocksdb', false) },
        'test-resilience-enterprise-mmfiles-linux':   { testResilienceStep('linux',   'enterprise', 'mmfiles', false) },
        'test-resilience-enterprise-mmfiles-mac':     { testResilienceStep('mac',     'enterprise', 'mmfiles', false) },
        'test-resilience-enterprise-mmfiles-windows': { testResilienceStep('windows', 'enterprise', 'mmfiles', false) },
        'test-resilience-enterprise-rocksdb-linux':   { testResilienceStep('linux',   'enterprise', 'rocksdb', false) },
        'test-resilience-enterprise-rocksdb-mac':     { testResilienceStep('mac',     'enterprise', 'rocksdb', false) },
        'test-resilience-enterprise-rocksdb-windows': { testResilienceStep('windows', 'enterprise', 'rocksdb', false) }
    )
}
