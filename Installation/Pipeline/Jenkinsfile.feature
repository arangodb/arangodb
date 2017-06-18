//  -*- mode: groovy-mode

// start with empty build directory
cleanBuild = false

// build enterprise version
buildEnterprise = false

// build linux
buildLinux = true

// build mac
buildMac = false

// build windows
buildWindows = false

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
// --SECTION--                                                           SCRIPTS
// -----------------------------------------------------------------------------

def checkoutCommunity() {
    retry(3) {
        try {
            checkout scm
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

        buildEnterprise = true
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

            if (msg ==~ /(?i).*\[ci:[^\]]*no-linux[ \]].*/) {
                echo "not building linux because message contained 'no-linux'"
                buildLinux = false
            }

            if (msg ==~ /(?i).*\[ci:[^\]]*mac[ \]].*/) {
                echo "building mac because message contained 'mac'"
                buildMac = true
            }

            if (msg ==~ /(?i).*\[ci:[^\]]*windows[ \]].*/) {
                echo "building windows because message contained 'windows'"
                buildWindows = true
            }

            if (msg ==~ /(?i).*\[ci:[^\]]*enterprise[ \]].*/) {
                echo "building enterprise because message contained 'enterprise'"
                buildEnterprise = true
            }

            def files = new ArrayList(entry.affectedFiles)

            for (int k = 0; k < files.size(); k++) {
                def file = files[k]
                def editType = file.editType.name
                def path = file.path
            }
        }
    }
}

def stashSourceCode() {
    sh 'rm -f source.*'

    if (buildLinux || buildMac) {
        sh 'tar -c -z -f source.tar.gz --exclude "source.*" --exclude "*tmp" --exclude ".git" *'
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
        sh 'tar -x -z -p -f source.tar.gz'
        sh 'mkdir -p artefacts'
    }
    else if (os == 'windows') {
        unstash 'source'
        if (!fileExists('artefacts')) {
           bat 'mkdir artefacts'
        }
    }
}

def buildEdition(edition, os) {
    try {
        if (os == 'linux' || os == 'mac') {
            def tarfile = 'build-' + edition + '-' + os + '.tar.gz'

            cache(maxCacheSize: 50000, caches: [
                [$class: 'ArbitraryFileCache',
                 includes: tarfile,
                 path: 'artefacts']]) {
                    if (!cleanBuild && fileExists('artefacts/' + tarfile)) {
                        sh 'tar -x -z -p -f artefacts/' + tarfile
                    }

                    sh 'rm -f artefacts/' + tarfile
                    sh './Installation/Pipeline/build_' + edition + '_' + os + '.sh 16'
                    sh 'tar -c -z -f artefacts/' + tarfile + ' build-' + edition
            }
        }
        else if (os == 'windows') {
            if (!cleanBuild) {
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

def jslint() {
    try {
        sh './Installation/Pipeline/test_jslint.sh'
    }
    catch (exc) {
        echo exc.toString()
        currentBuild.result = 'UNSTABLE'
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
        stashSourceCode()
    }
}

if (buildLinux) {
    stage('build linux') {
        def os = 'linux'

        parallel(
            'build-community-linux': {
                def edition = 'community'

                node(os) {
                    unstashSourceCode(os)
                    buildEdition(edition, os)
                    stashBinaries(edition, os)
                }
            },

            'build-enterprise-linux': {
                if (buildEnterprise) {
                    def edition = 'enterprise'

                    node(os) {
                        unstashSourceCode(os)
                        buildEdition(edition, os)
                        stashBinaries(edition, os)
                    }
                }
                else {
                    echo "Not building enterprise version"
                }
            }
        )
    }

    stage('test linux') {
        def os = 'linux'

        parallel(
            'test-singleserver-community': {
                def edition = 'community'

                node(os) {
                    echo "Running singleserver community rocksdb linux test"

                    unstashBinaries(edition, os)
                    testEdition(edition, os, 'singleserver', 'rocksdb')
                }
            },

            'test-singleserver-enterprise': {
                def edition = 'enterprise'

                if (buildEnterprise) {
                    node(os) {
                        echo "Running singleserver enterprise mmfiles linux test"

                        unstashBinaries(edition, os)
                        testEdition(edition, os, 'singleserver', 'mmfiles')
                    }
                }
                else {
                    echo "Enterprise version not built, skipping 'test-singleserver-enterprise'"
                }
            },

            'jslint': {
                node(os) {
                    echo "Running jslint test"

                    unstashBinaries('community', os)
                    jslint()
                }
            }
        )
    }
}

stage('build other') {
    parallel(
        'build-community-windows': {
            def os = 'windows'

            if (buildWindows) {
                node('windows') {
                    unstashSourceCode(os)
                    buildEdition('community', os)
                }
            }
            else {
                echo "Not building " + os + " version"
            }
        },

        'build-community-mac': {
            def os = 'mac'

            if (buildMac) {
                node('mac') {
                    unstashSourceCode(os)
                    buildEdition('community', os)
                }
            }
            else {
                echo "Not building " + os + " version"
            }
        }
    )
}
