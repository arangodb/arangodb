//  -*- mode: groovy-mode

properties([
    parameters([
        booleanParam(
            defaultValue: false,
            description: 'clean build directories',
            name: 'cleanBuild'
        ),
        booleanParam(
            defaultValue: true,
            description: 'build and run tests for community',
            name: 'buildCommunity'
        ),
        booleanParam(
            defaultValue: false,
            description: 'build and run tests for enterprise',
            name: 'buildEnterprise'
        ),
        booleanParam(
            defaultValue: true,
            description: 'build and run tests on Linux',
            name: 'buildLinux'
        ),
        booleanParam(
            defaultValue: false,
            description: 'build and run tests on Mac',
            name: 'buildMac'
        ),
        booleanParam(
            defaultValue: false,
            description: 'build and run tests in Windows',
            name: 'buildWindows'
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
buildLinux = params.buildLinux

// build mac
buildMac = params.buildMac

// build windows
buildWindows = params.buildWindows

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

// binaries to copy for testing
binariesCommunity = 'build/**,etc/**,Installation/Pipeline/**,js/**,scripts/**,UnitTests/**,utils/**'
binariesEnterprise = binariesCommunity + ',enterprise/js/**'

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

    echo 'Clean Build: ' + (cleanBuild ? 'true' : 'false')
    echo 'Build Community: ' + (buildCommunity ? 'true' : 'false')
    echo 'Build Enterprise: ' + (buildEnterprise ? 'true' : 'false')
    echo 'Build Linux: ' + (buildLinux ? 'true' : 'false')
    echo 'Build Mac: ' + (buildMac ? 'true' : 'false')
    echo 'Build Windows: ' + (buildWindows ? 'true' : 'false')
    echo 'Run Jslint: ' + (runJslint ? 'true' : 'false')
    echo 'Run Resilience: ' + (runResilience ? 'true' : 'false')
    echo 'Run Tests: ' + (runTests ? 'true' : 'false')
}

// -----------------------------------------------------------------------------
// --SECTION--                                                     SCRIPTS STASH
// -----------------------------------------------------------------------------

def stashSourceCode() {
    sh 'rm -f source.*'

    if (buildLinux || buildMac) {
        sh 'tar -c -f source.tar --exclude "source.*" --exclude "*tmp" --exclude ".git" *'
        stash includes: 'source.*', name: 'sourceTar'
    }

    if (buildWindows) {
        stash includes: '**', excludes: '*tmp,.git,source.*', name: 'source'
    }
}

def unstashSourceCode(os) {
    if (os == 'linux' || os == 'mac') {
        sh 'rm -rf *'
    }
    else if (os == 'windows') {
        bat 'del /F /Q *'
    }

    if (os == 'linux' || os == 'mac') {
        unstash 'sourceTar'
        sh 'tar -x -p -f source.tar'
        sh 'mkdir -p artefacts'
    }
    else if (os == 'windows') {
        unstash 'source'

        if (!fileExists('artefacts')) {
           bat 'mkdir artefacts'
        }
    }
}

def stashBinaries(edition, os) {
    if (edition == 'community') {
        stash includes: binariesCommunity, name: 'build-' + edition + '-' + os
    }
    else if (edition == 'enterprise') {
        stash includes: binariesEnterprise, name: 'build-' + edition + '-' + os
    }
}

def unstashBinaries(edition, os) {
    sh 'rm -rf *'
    unstash 'build-' + edition + '-' + os
}

// -----------------------------------------------------------------------------
// --SECTION--                                                     SCRIPTS BUILD
// -----------------------------------------------------------------------------

def buildEdition(edition, os) {
    try {
        if (os == 'linux' || os == 'mac') {
/*
            def tarfile = 'build-' + edition + '-' + os + '.tar.gz'

            cache(maxCacheSize: 50000, caches: [
                [$class: 'ArbitraryFileCache',
                 includes: tarfile,
                 path: 'artefacts']]) {
                    if (!cleanBuild && fileExists('artefacts/' + tarfile)) {
                        sh 'tar -x -z -p -f artefacts/' + tarfile
                    }

                    sh 'rm -f artefacts/' + tarfile
                    sh './Installation/Pipeline/build_' + edition + '_' + os + '.sh 64'
                    sh 'GZIP=--fast tar -c -z -f artefacts/' + tarfile + ' build-' + edition
            }
*/
            sh './Installation/Pipeline/build_' + edition + '_' + os + '.sh 64'
        }
        else if (os == 'windows') {
            if (cleanBuild) {
                bat 'del /F /Q build'
            }

            PowerShell('. .\\Installation\\Pipeline\\build_' + edition + '_windows.ps1')
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

def buildStep(os, edition, full) {
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

    node(os) {
        unstashSourceCode(os)
        buildEdition(edition, os)

        if (runTests || runResilience) {
            stashBinaries(edition, os)
        }
    }
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
    if (runJslint) {
        if (buildLinux) {
            node(os) {
                echo "Running jslint test"

                unstashBinaries('community', os)
                jslint()
            }
        }
        else {
            echo "Not building " + os + " version"
        }
    }
    else {
        echo "Not running tests"
    }
}

def testEdition(edition, os, type, engine) {
    try {
        sh './Installation/Pipeline/test_' + type + '_' + edition + '_' + engine + '_' + os + '.sh 8'
    }
    catch (exc) {
        echo exc.toString()
        throw exc
    }
    finally {
        archiveArtifacts allowEmptyArchive: true, artifacts: 'log-output/**', defaultExcludes: false
    }
}

def testStep(edition, os, mode, engine, full) {
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

    node(os) {
        echo "Running " + mode + " " + edition + " " + engine + " " + os + " test"

        unstashBinaries(edition, os)
        testEdition(edition, os, mode, engine)
    }
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
    parallel(
        'build-community-linux': {
            buildStep('linux', 'community', false)
        },

        'build-community-mac':      { buildStep('mac',     'community',  false) },
        'build-community-windows':  { buildStep('windows', 'community',  false) },

        'build-enterprise-linux':   { buildStep('linux',   'enterprise', false) },
        'build-enterprise-mac':     { buildStep('mac',     'enterprise', true) },
        'build-enterprise-windows': { buildStep('windows', 'enterprise', true) },
    )
}

stage('test') {
    parallel(
        'test-cluster-community-mmfiles-linux':        { testStep('community',  'linux', 'cluster',      'mmfiles', false) },
        'test-cluster-community-rocksdb-linux':        { testStep('community',  'linux', 'cluster',      'rocksdb', true)  },
        'test-cluster-enterprise-mmfiles-linux':       { testStep('enterprise', 'linux', 'cluster',      'mmfiles', true)  },
        'test-cluster-enterprise-rocksdb-linux':       { testStep('enterprise', 'linux', 'cluster',      'rocksdb', false) },
        'test-singleserver-community-mmfiles-linux':   { testStep('community',  'linux', 'singleserver', 'mmfiles', true)  },
        'test-singleserver-community-rocksdb-linux':   { testStep('community',  'linux', 'singleserver', 'rocksdb', false) },
        'test-singleserver-enterprise-mmfiles-linux':  { testStep('enterprise', 'linux', 'singleserver', 'mmfiles', false) },
        'test-singleserver-enterprise-rocksdb-linux':  { testStep('enterprise', 'linux', 'singleserver', 'rocksdb', true)  },

        'test-resilience-community-rocksdb': { testResilienceStep('linux', 'community', 'rocksdb', false) },
        'test-resilience-community-mmfiles': { testResilienceStep('linux', 'community', 'mmfiles', false) },

        'jslint': { jslintStep() }
    )
}
