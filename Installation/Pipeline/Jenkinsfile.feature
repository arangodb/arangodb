//  -*- mode: groovy-mode

properties(
    [[
      $class: 'BuildDiscarderProperty',
      strategy: [$class: 'LogRotator', artifactDaysToKeepStr: '', artifactNumToKeepStr: '', daysToKeepStr: '3', numToKeepStr: '5']
    ]]
)

echo "BRANCH_NAME: ${env.BRANCH_NAME}"
echo "CHANGE_ID: ${env.CHANGE_ID}"
echo "CHANGE_TARGET: ${env.CHANGE_TARGET}"
echo "JOB_NAME: ${env.JOB_NAME}"

properties([
    parameters([
        booleanParam(
            defaultValue: true,
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
            defaultValue: true,
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

// users
jenkinsMaster = 'jenkins-master@c1'
jenkinsSlave = 'jenkins'

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

// copy data to master cache
def scpToMaster(os, from, to) {
    if (os == 'linux' || os == 'mac') {
        sh "scp '${from}' '${jenkinsMaster}:${cacheDir}/${to}'"
    }
    else if (os == 'windows') {
        bat "scp -F c:/Users/jenkins/ssh_config '${from}' '${jenkinsMaster}:${cacheDir}/${to}'"
    }
}

// copy data from master cache
def scpFromMaster(os, from, to) {
    if (os == 'linux' || os == 'mac') {
        sh "scp '${jenkinsMaster}:${cacheDir}/${from}' '${to}'"
    }
    else if (os == 'windows') {
        bat "scp -F c:/Users/jenkins/ssh_config '${jenkinsMaster}:${cacheDir}/${from}' '${to}'"
    }
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
        sh "mv -f source.zip ${cacheDir}/source.zip"
    }
}

def unstashSourceCode(os) {
    if (os == 'linux' || os == 'mac') {
        sh 'rm -rf *'
    }
    else if (os == 'windows') {
        bat 'del /F /Q *'
    }

    lock('cache') {
        scpFromMaster(os, 'source.zip', 'source.zip')
    }

    if (os == 'linux' || os == 'mac') {
        sh 'unzip -o -q source.zip'
    }
    else if (os == 'windows') {
        bat 'c:\\cmake\\bin\\cmake -E tar xf source.zip'
    }
}

def stashBuild(edition, os) {
    def name = "build-${edition}-${os}.zip"

    if (os == 'linux' || os == 'mac') {
        sh "rm -f ${name}"
        sh "zip -r -1 -y -q ${name} build-${edition}"
    }
    else if (os == 'windows') {
        bat "del /F /q ${name}"
        bat "c:\\cmake\\bin\\cmake -E tar cf ${name} build"
    }

    lock('cache') {
        scpToMaster(os, name, name)
    }
}

def unstashBuild(edition, os) {
    def name = "build-${edition}-${os}.zip"

    lock('cache') {
        scpFromMaster(os, name, name)
    }

    if (os == 'linux' || os == 'mac') {
        sh "unzip -o -q ${name}"
    }
    else if (os == 'windows') {
        bat "c:\\cmake\\bin\\cmake -E tar xf ${name}"
    }
}

def stashBinaries(edition, os) {
    def name = "binaries-${edition}-${os}.zip"
    def dirs = 'build etc Installation/Pipeline js scripts UnitTests utils resilience'

    if (edition == 'enterprise') {
        dirs = "${dirs} enterprise/js"
    }

    if (os == 'linux' || os == 'mac') {
        sh "zip -r -1 -y -q ${name} ${dirs}"
    }
    else if (os == 'windows') {
        bat "c:\\cmake\\bin\\cmake -E tar cf ${name} ${dirs}"
    }

    lock('cache') {
        scpToMaster(os, name, name)
    }
}

def unstashBinaries(edition, os) {
    def name = 'binaries-' + edition + '-' + os + '.zip'

    if (os == 'linux' || os == 'mac') {
        sh 'rm -rf *'

        lock('cache') {
            scpFromMaster(os, name, name)
        }

        sh 'unzip -o -q ' + name
    }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                     SCRIPTS BUILD
// -----------------------------------------------------------------------------

buildsSuccess = [:]
allBuildsSuccessful = true

def buildEdition(edition, os) {
    if (! cleanBuild) {
        try {
            unstashBuild(edition, os)
        }
        catch (exc) {
            echo exc.toString()
        }
    }

    try {
        if (os == 'linux') {
            sh "./Installation/Pipeline/build_${edition}_${os}.sh 64"
        }
        else if (os == 'mac') {
            sh "./Installation/Pipeline/build_${edition}_${os}.sh 20"
        }
        else if (os == 'windows') {
            PowerShell('. .\\Installation\\Pipeline\\build_${edition}_windows.ps1')
        }
    }
    catch (exc) {
        echo exc.toString()
        throw exc
    }
    finally {
        stashBuild(edition, os)
        archiveArtifacts allowEmptyArchive: true, artifacts: 'log-output/**', defaultExcludes: false
    }
}

def buildStepCheck(edition, os, full) {
    if (full && ! buildFull) {
        echo "Not building combination ${os} ${edition}"
        return false
    }

    if (os == 'linux' && ! buildLinux) {
        echo "Not building ${os} version"
        return false
    }

    if (os == 'mac' && ! buildMac) {
        echo "Not building ${os} version"
        return false
    }

    if (os == 'windows' && ! buildWindows) {
        echo "Not building ${os} version"
        return false
    }

    if (edition == 'enterprise' && ! buildEnterprise) {
        echo "Not building ${edition} version"
        return false
    }

    if (edition == 'community' && ! buildCommunity) {
        echo "Not building ${edition} version"
        return false
    }

    return true
}

def buildStep(edition, os) {
    return {
        node(os) {
            def name = "${edition}-${os}"

            try {
                unstashSourceCode(os)
                buildEdition(edition, os)
                stashBinaries(edition, os)
                buildsSuccess[name] = true
            }
            catch (exc) {
                echo exc.toString()
                buildsSuccess[name] = false
                allBuildsSuccessful = false
                currentBuild.result = 'UNSTABLE'
            }
        }
    }
}

def buildStepParallel(osList) {
    def branches = [:]
    def full = false

    for (edition in ['community', 'enterprise']) {
        for (os in osList) {
            if (buildStepCheck(edition, os, full)) {
                branches["build-${edition}-${os}"] = buildStep(edition, os)
            }
        }
    }

    parallel branches
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    SCRIPTS JSLINT
// -----------------------------------------------------------------------------

jslintSuccessful = true

def jslint() {
    try {
        sh './Installation/Pipeline/test_jslint.sh'
    }
    catch (exc) {
        echo exc.toString()
        jslintSuccessful = false
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

// -----------------------------------------------------------------------------
// --SECTION--                                                     SCRIPTS TESTS
// -----------------------------------------------------------------------------

testsSuccess = [:]
allTestsSuccessful = true

def testEdition(edition, os, mode, engine) {
    try {
        sh "./Installation/Pipeline/test_${mode}_${edition}_${engine}_${os}.sh 10"
    }
    catch (exc) {
        echo exc.toString()
        throw exc
    }
    finally {
        archiveArtifacts allowEmptyArchive: true, artifacts: 'log-output/*/FAILED_*', defaultExcludes: false
    }
}

def testCheck(edition, os, mode, engine, full) {
    if (! runTests) {
        echo "Not running tests"
        return false
    }

    def name = "${edition}-${os}"

    if (buildsSuccess.containsKey(name) && ! buildsSuccess[name]) {
       echo "Not testing failed build ${name}"
    }

    if (full && ! buildFull) {
        echo "Not building combination ${os} ${edition}"
        return false
    }

    if (os == 'linux' && ! buildLinux) {
        echo "Not building ${os} version"
        return false
    }

    if (os == 'mac' && ! buildMac) {
        echo "Not building ${os} version"
        return false
    }

    if (os == 'windows' && ! buildWindows) {
        echo "Not building ${os} version"
        return false
    }

    if (edition == 'enterprise' && ! buildEnterprise) {
        echo "Not building ${edition} version"
        return false
    }

    if (edition == 'community' && ! buildCommunity) {
        echo "Not building ${edition} version"
        return false
    }

    return true
}

def testName(edition, os, mode, engine, full) {
    def name = "test-${mode}-${edition}-${engine}-${os}";

    if (! testCheck(edition, os, mode, engine, full)) {
        name = "DISABLED-${name}"
    }

    return name 
}

def testStep(edition, os, mode, engine) {
    return {
        node(os) {
            echo "Running ${mode} ${edition} ${engine} ${os} test"

            def name = "${edition}-${os}-${mode}-${engine}"

            try {
                unstashBinaries(edition, os)
                testEdition(edition, os, mode, engine)
                testsSuccess[name] = true
            }
            catch (exc) {
                echo exc.toString()
                testsSuccess[name] = false
                allTestsSuccessful = false
                currentBuild.result = 'UNSTABLE'
            }
        }
    }
}

def testStepParallel(osList, modeList) {
    def branches = [:]
    def full = false

    for (edition in ['community', 'enterprise']) {
        for (os in osList) {
            for (mode in modeList) {
                for (engine in ['mmfiles', 'rocksdb']) {
                    if (testCheck(edition, os, mode, engine, full)) {
                        def name = testName(edition, os, mode, engine, full)

                        branches[name] = testStep(edition, os, mode, engine)
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

// -----------------------------------------------------------------------------
// --SECTION--                                                SCRIPTS RESILIENCE
// -----------------------------------------------------------------------------

resiliencesSuccess = [:]
allResiliencesSuccessful = true

def testResilience(os, engine, foxx) {
    sh "./Installation/Pipeline/test_resilience_${foxx}_${engine}_${os}.sh"
}

def testResilienceCheck(os, engine, foxx, full) {
    if (! runResilience) {
        echo "Not running resilience tests"
        return false
    }

    if (os == 'linux' && ! buildLinux) {
        echo "Not building ${os} version"
        return false
    }

    if (os == 'mac' && ! buildMac) {
        echo "Not building ${os} version"
        return false
    }

    if (os == 'windows' && ! buildWindows) {
        echo "Not building ${os} version"
        return false
    }

    if (! buildCommunity) {
        echo "Not building community version"
        return false
    }

    return true
}

def testResilienceName(os, engine, foxx, full) {
    def name = "test-resilience-${foxx}-${engine}-${os}";

    if (! testResilienceCheck(os, engine, foxx, full)) {
        name = "DISABLED-${name}"
    }

    return name 
}

def testResilienceStep(os, engine, foxx) {
    return {
        node(os) {
            echo "Running ${foxx} ${engine} ${os} test"

            def name = "${os}-${engine}-${foxx}"

            try {
                unstashBinaries('community', os)
                testResilience(os, engine, foxx)
            }
            catch (exc) {
                echo exc.toString()
                resiliencesSuccess[name] = false
                allResiliencesSuccessful = false
                currentBuild.result = 'UNSTABLE'
            }
        }
    }
}

def testResilienceParallel() {
    def branches = [:]
    def full = false

    for (foxx in ['foxx', 'nofoxx']) {
        for (os in ['linux', 'mac', 'windows']) {
            for (engine in ['mmfiles', 'rocksdb']) {
                if (testResilienceCheck(os, engine, foxx, full)) {
                    def name = testResilienceName(os, engine, foxx, full)

                    branches[name] = testResilienceStep(os, engine, foxx)
                }
            }
        }
    }

    parallel branches
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

stage('build linux') {
    buildStepParallel(['linux'])
}

stage('tests linux') {
    testStepParallel(['linux'], ['cluster', 'singleserver'])
}

stage('build mac & windows') {
    buildStepParallel(['mac', 'windows'])
}

stage('tests mac & windows') {
    testStepParallel(['mac', 'windows'], ['cluster', 'singleserver'])
}

stage('resilience') {
    testResilienceParallel();
}

stage('result') {
    node('master') {
        for (kv in buildsSuccess) {
            echo "BUILD ${kv.key}: ${kv.value}"
        }

        for (kv in testsSuccess) {
            echo "TEST ${kv.key}: ${kv.value}"
        }

        for (kv in resiliencesSuccess) {
            echo "RESILIENCE ${kv.key}: ${kv.value}"
        }

        if (! (allBuildsSuccessful
            && allTestsSuccessful
            && resiliencesSuccess
            && jslintSuccessful)) {
            currentBuild.result = 'FAILURE'
        }
    }
}