//  -*- mode: groovy-mode

properties(
    [[
      $class: 'BuildDiscarderProperty',
      strategy: [$class: 'LogRotator',
                 artifactDaysToKeepStr: '3',
                 artifactNumToKeepStr: '5',
                 daysToKeepStr: '3',
                 numToKeepStr: '5']
    ]]
)

// -----------------------------------------------------------------------------
// --SECTION--                                             SELECTABLE PARAMETERS
// -----------------------------------------------------------------------------

def defaultLinux = true
def defaultMac = false
def defaultWindows = false
def defaultBuild = true
def defaultCleanBuild = false
def defaultCommunity = true
def defaultEnterprise = true
def defaultMaintainer = true
def defaultUser = false
// def defaultRunResilience = false
def defaultRunTests = true

properties([
    parameters([
        booleanParam(
            defaultValue: defaultLinux,
            description: 'build and run tests on Linux',
            name: 'Linux'
        ),
        booleanParam(
            defaultValue: defaultMac,
            description: 'build and run tests on Mac',
            name: 'Mac'
        ),
        booleanParam(
            defaultValue: defaultWindows,
            description: 'build and run tests in Windows',
            name: 'Windows'
        ),
        booleanParam(
            defaultValue: defaultCleanBuild,
            description: 'clean build directories',
            name: 'cleanBuild'
        ),
        booleanParam(
            defaultValue: defaultCommunity,
            description: 'build and run tests for community',
            name: 'Community'
        ),
        booleanParam(
            defaultValue: defaultEnterprise,
            description: 'build and run tests for enterprise',
            name: 'Enterprise'
        ),
        booleanParam(
            defaultValue: defaultMaintainer,
            description: 'build in maintainer mode',
            name: 'Maintainer'
        ),
        booleanParam(
            defaultValue: defaultUser,
            description: 'build in user (aka non-maintainer) mode',
            name: 'User'
        ),
        // booleanParam(
        //     defaultValue: defaultRunResilience,
        //     description: 'run resilience tests',
        //     name: 'runResilience'
        // ),
        booleanParam(
            defaultValue: defaultRunTests,
            description: 'run tests',
            name: 'runTests'
        )
    ])
])

// start with empty build directory
cleanBuild = params.cleanBuild

// build linux
useLinux = params.Linux

// build mac
useMac = params.Mac

// build windows
useWindows = params.Windows

// build and test community
useCommunity = params.Community

// build and test enterprise
useEnterprise = params.Enterprise

// build maintainer mode
useMaintainer = params.Maintainer

// build user mode
useUser = params.User

// run resilience tests
//runResilience = params.runResilience

// run tests
runTests = params.runTests

// restrict builds
restrictions = [:]

// overview of configured builds and tests
overview = ""

// -----------------------------------------------------------------------------
// --SECTION--                                             CONSTANTS AND HELPERS
// -----------------------------------------------------------------------------

// github proxy repositiory
proxyRepo = 'http://c1:8088/github.com/arangodb/arangodb'

// github repositiory for resilience tests
// resilienceRepo = 'http://c1:8088/github.com/arangodb/resilience-tests'

// github repositiory for enterprise version
enterpriseRepo = 'http://c1:8088/github.com/arangodb/enterprise'

// Jenkins credentials for enterprise repositiory
credentials = '8d893d23-6714-4f35-a239-c847c798e080'

// source branch for pull requests
sourceBranchLabel = env.BRANCH_NAME

if (env.BRANCH_NAME =~ /^PR-/) {
  def prUrl = new URL("https://api.github.com/repos/arangodb/arangodb/pulls/${env.CHANGE_ID}")
  sourceBranchLabel = new groovy.json.JsonSlurper().parseText(prUrl.text).head.label

  def reg = ~/^arangodb:/
  sourceBranchLabel = sourceBranchLabel - reg
}

if (sourceBranchLabel == ~/devel$/) {
    useWindows = true
    useMac = true
}

buildJenkins = [
    "linux": "linux && build",
    "mac" : "mac",
    "windows": "windows"
]

testJenkins = [
    "linux": "linux && tests",
    "mac" : "mac",
    "windows": "windows"
]

def copyFile(os, src, dst) {
    if (os == "windows") {
        powershell "copy-item -Force -ErrorAction Ignore ${src} ${dst}"
    }
    else {
        sh "cp ${src} ${dst}"
    }
}

def renameFolder(src, dst) {
    fileOperations([
        folderRenameOperation(destination: dst, source: src)
    ])
}

def checkEnabledOS(os, text) {
    if (os == 'linux' && ! useLinux) {
        echo "Not ${text} ${os} because ${os} is not enabled"
        return false
    }

    if (os == 'mac' && ! useMac) {
        echo "Not ${text} ${os} because ${os} is not enabled"
        return false
    }

    if (os == 'windows' && ! useWindows) {
        echo "Not ${text} ${os} because ${os} is not enabled"
        return false
    }

    return true
}

def checkEnabledEdition(edition, text) {
    if (edition == 'enterprise' && ! useEnterprise) {
        echo "Not ${text} ${edition} because ${edition} is not enabled"
        return false
    }

    if (edition == 'community' && ! useCommunity) {
        echo "Not ${text} ${edition} because ${edition} is not enabled"
        return false
    }

    return true
}

def checkEnabledMaintainer(maintainer, os, text) {
    if (os == 'windows' && maintainer != 'maintainer') {
        echo "Not ${text} ${maintainer} because ${maintainer} is not enabled under Windows"
        return false
    }

    if (maintainer == 'maintainer' && ! useMaintainer) {
        echo "Not ${text} ${maintainer} because ${maintainer} is not enabled"
        return false
    }

    if (maintainer == 'user' && ! useUser) {
        echo "Not ${text} ${maintainer} because ${maintainer} is not enabled"
        return false
    }

    return true
}

def checkCoresAndSave(os, runDir, name, archRun) {
    if (os == 'windows') {
        powershell "move-item -Force -ErrorAction Ignore ${runDir}/logs ${archRun}/${name}.logs"
        powershell "move-item -Force -ErrorAction Ignore ${runDir}/out ${archRun}/${name}.logs"
        powershell "move-item -Force -ErrorAction Ignore ${runDir}/tmp ${archRun}/${name}.tmp"

        def files = findFiles(glob: "${runDir}/*.dmp")
        
        if (files.length > 0) {
            for (file in files) {
                powershell "move-item -Force -ErrorAction Ignore ${file} ${archRun}"
            }

            powershell "copy-item .\\build\\bin\\* -Include *.exe,*.pdb,*.ilk ${archRun}"

            error("found dmp file")
        }
    }
    else {
        sh "for i in logs out tmp result; do test -e \"${runDir}/\$i\" && mv \"${runDir}/\$i\" \"${archRun}/${name}.\$i\" || true; done"

        def files = findFiles(glob: '${runDir}/core*')

        if (files.length > 0) {
            for (file in files) {
                sh "mv ${file} ${archRun}"
            }

            sh "cp -a build/bin/* ${archRun}"

            error("found core file")
        }
    }
}

def getStartPort(os) {
    if (os == "windows") {
        return powershell (returnStdout: true, script: "Installation/Pipeline/port.ps1")
    }
    else {
        return sh (returnStdout: true, script: "Installation/Pipeline/port.sh")
    }
}

def rspecify(os, test) {
    if (os == "windows") {
        return [test, test, "--rspec C:\\tools\\ruby23\\bin\\rspec.bat"]
    } else {
        return [test, test, ""]
    }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       SCRIPTS SCM
// -----------------------------------------------------------------------------

def checkoutCommunity() {
    if (cleanBuild) {
        deleteDir()
    }

    retry(3) {
        try {
            checkout(
                changelog: false,
                poll: false,
                scm: [
                    $class: 'GitSCM',
                    branches: [[name: "*/${sourceBranchLabel}"]],
                    doGenerateSubmoduleConfigurations: false,
                    extensions: [],
                    submoduleCfg: [],
                    userRemoteConfigs: [[url: proxyRepo]]])
        }
        catch (exc) {
            echo "GITHUB checkout failed, retrying in 1min"
            sleep 60
            throw exc
        }
    }
}

def checkoutEnterprise() {
    try {
        echo "Trying enterprise branch ${sourceBranchLabel}"

        checkout(
            changelog: false,
            poll: false,
            scm: [
                $class: 'GitSCM',
                branches: [[name: "*/${sourceBranchLabel}"]],
                doGenerateSubmoduleConfigurations: false,
                extensions: [[$class: 'RelativeTargetDirectory', relativeTargetDir: 'enterprise']],
                submoduleCfg: [],
                userRemoteConfigs: [[credentialsId: credentials, url: enterpriseRepo]]])
    }
    catch (exc) {
        echo "Failed ${sourceBranchLabel}, trying enterprise branch devel"

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

// def checkoutResilience() {
//     checkout(
//         changelog: false,
//         poll: false,
//         scm: [
//             $class: 'GitSCM',
//             branches: [[name: "*/master"]],
//             doGenerateSubmoduleConfigurations: false,
//             extensions: [[$class: 'RelativeTargetDirectory', relativeTargetDir: 'resilience']],
//             submoduleCfg: [],
//             userRemoteConfigs: [[credentialsId: credentials, url: resilienceRepo]]])

// }

def checkCommitMessages() {
    def causes = currentBuild.rawBuild.getCauses()
    def causeDescription = causes[0].getShortDescription();
    def changeLogSets = currentBuild.changeSets
    def seenCommit = false
    def skip = false

    for (int i = 0; i < changeLogSets.size(); i++) {
        def entries = changeLogSets[i].items

        for (int j = 0; j < entries.length; j++) {
            seenCommit = true

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

            if (msg ==~ /(?i).*\[ci:[^\]]*skip[ \]].*/) {
                echo "skipping everything because message contained 'skip'"
                skip = true
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

    if (causeDescription =~ /Started by user/) {
        echo "build started by user"
    }
    else if (skip) {
        useLinux = false
        useMac = false

        useWindows = false
        useCommunity = false
        useEnterprise = false

        useMaintainer = false
        useUser = false

        // runResilience = false
        runTests = false
    }
    else {
        if (env.BRANCH_NAME == "devel" || env.BRANCH_NAME == "3.2") {
            echo "build of main branch"
        }
        else if (env.BRANCH_NAME =~ /^PR-/) {
            echo "build of PR"

            restrictions = [
                // OS EDITION MAINTAINER
                "build-linux-community-maintainer" : true,
                "build-linux-enterprise-maintainer" : true,
                "build-mac-community-maintainer" : true,
                "build-mac-enterprise-user" : true,
                "build-windows-community-maintainer" : true,
                "build-windows-enterprise-maintainer" : true,

                // OS EDITION MAINTAINER MODE ENGINE
                "test-linux-enterprise-maintainer-cluster-rocksdb" : true,
                "test-linux-community-maintainer-singleserver-mmfiles" : true
            ]
        }
        else {
            restrictions = [
                // OS EDITION MAINTAINER
                "build-linux-community-user" : true,
                "build-linux-enterprise-maintainer" : true,

                // OS EDITION MAINTAINER MODE ENGINE
                "test-linux-enterprise-maintainer-cluster-rocksdb" : true,
                "test-linux-community-user-singleserver-mmfiles" : true
            ]
        }
    }

    overview = """BRANCH_NAME: ${env.BRANCH_NAME}
SOURCE: ${sourceBranchLabel}
CHANGE_ID: ${env.CHANGE_ID}
CHANGE_TARGET: ${env.CHANGE_TARGET}
JOB_NAME: ${env.JOB_NAME}
CAUSE: ${causeDescription}
"""

    if (restrictions) {
        useLinux = true
        useMac = true
        useWindows = true

        useCommunity = true
        useEnterprise = true

        useMaintainer = true
        useUser = true

        // runResilience = true
        runTests = true

        overview += "Restrictions:\n"

        for (r in restrictions.keySet()) {
            overview += "    " + r + "\n"
        }
    }
    else {
        overview += """Linux: ${useLinux}
Mac: ${useMac}
Windows: ${useWindows}
Clean Build: ${cleanBuild}
Building Community: ${useCommunity}
Building Enterprise: ${useEnterprise}
Building Maintainer: ${useMaintainer}
Building Non-Maintainer: ${useUser}
Running Tests: ${runTests}
"""
    }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                     SCRIPTS STASH
// -----------------------------------------------------------------------------

def stashBinaries(os, edition, maintainer) {
    def paths = ["build/etc", "etc", "Installation/Pipeline", "js", "scripts", "UnitTests"]

    if (edition == "enterprise") {
       paths << "enterprise/js"
    }

    if (os == "windows") {
        paths << "build/bin/RelWithDebInfo"
        paths << "build/tests/RelWithDebInfo"

        // so frustrating...compress-archive is built in but it simply won't include the relative path to
        // the archive :(
        // powershell "Compress-Archive -Force -Path (Get-ChildItem -Recurse -Path " + paths.join(',') + ") -DestinationPath stash.zip -Confirm -CompressionLevel Fastest"
        // install 7z portable (https://chocolatey.org/packages/7zip.portable)

        powershell "7z a stash.zip -r -bd -mx=1 " + paths.join(" ")

        // this is a super mega mess...scp will run as the system user and not as jenkins when run as a server
        // I couldn't figure out how to properly get it running for hours...so last resort was to install putty

        powershell "echo 'y' | pscp -i C:\\Users\\Jenkins\\.ssh\\putty-jenkins.ppk stash.zip jenkins@c1:/vol/cache/binaries-${env.BUILD_TAG}-${os}-${edition}-${maintainer}.zip"
    }
    else {
        paths << "build/bin/"
        paths << "build/tests/"

        sh "GZIP=-1 tar cpzf stash.tar.gz " + paths.join(" ")
        sh "scp stash.tar.gz c1:/vol/cache/binaries-${env.BUILD_TAG}-${os}-${edition}-${maintainer}.tar.gz"
    }
}

def unstashBinaries(os, edition, maintainer) {
    if (os == "windows") {
        powershell "echo 'y' | pscp -i C:\\Users\\Jenkins\\.ssh\\putty-jenkins.ppk jenkins@c1:/vol/cache/binaries-${env.BUILD_TAG}-${os}-${edition}-${maintainer}.zip stash.zip"
        powershell "Expand-Archive -Path stash.zip -Force -DestinationPath ."
        powershell "copy build\\bin\\RelWithDebInfo\\* build\\bin"
    }
    else {
        sh "scp c1:/vol/cache/binaries-${env.BUILD_TAG}-${os}-${edition}-${maintainer}.tar.gz stash.tar.gz"
        sh "tar xpzf stash.tar.gz"
    }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    SCRIPTS JSLINT
// -----------------------------------------------------------------------------

def jslint(os, edition, maintainer) {
    def archDir  = "${os}-${edition}-${maintainer}"
    def arch     = "${archDir}/02-jslint"
    def archFail = "${archDir}/02-jslint-FAIL"

    fileOperations([
        folderDeleteOperation(arch),
        folderDeleteOperation(archFail),
        folderCreateOperation(arch)
    ])

    def logFile = "${arch}/jslint.log"

    try {
        sh "./Installation/Pipeline/test_jslint.sh | tee ${logFile}"
        sh "if grep ERROR ${logFile}; then exit 1; fi"
    }
    catch (exc) {
        renameFolder(arch, archFail)
        fileOperations([fileCreateOperation(fileContent: 'BUILD FAILED', fileName: "${arch}-FAIL.txt")])
        throw exc
    }
    finally {
        archiveArtifacts allowEmptyArchive: true,
            artifacts: "${arch}-*, ${arch}/**, ${archFail}/**",
            defaultExcludes: false
    }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                     SCRIPTS TESTS
// -----------------------------------------------------------------------------

def getTests(os, edition, maintainer, mode, engine) {
    def tests = [
        ["arangobench", "arangobench" , ""],
        ["arangosh", "arangosh", "--skipShebang true"],
        ["authentication", "authentication", ""],
        ["authentication_parameters", "authentication_parameters", ""],
        ["config", "config" , ""],
        ["dump", "dump" , ""],
        ["dump_authentication", "dump_authentication" , ""],
        ["endpoints", "endpoints", ""],
        ["server_http", "server_http", ""],
        ["shell_client", "shell_client", ""],
        ["shell_server", "shell_server", ""],
        ["shell_server_aql_1", "shell_server_aql", "--testBuckets 4/0", ,""],
        ["shell_server_aql_2", "shell_server_aql", "--testBuckets 4/1", ,""],
        ["shell_server_aql_3", "shell_server_aql", "--testBuckets 4/2", ,""],
        ["shell_server_aql_4", "shell_server_aql", "--testBuckets 4/3", ,""],
        ["upgrade", "upgrade" , ""],
        rspecify(os, "http_server"),
        rspecify(os, "ssl_server")
    ]

    if (edition == "enterprise") {
        tests += [
            ["authentication_server", "authentication_server", ""]
        ]
    }

    if (mode == "singleserver") {
        tests += [
            ["agency", "agency", ""],
            ["boost", "boost", "--skipCache false"],
            ["cluster_sync", "cluster_sync", ""],
            ["dfdb", "dfdb", ""],
            ["replication_ongoing", "replication_ongoing", ""],
            ["replication_static", "replication_static", ""],
            ["replication_sync", "replication_sync", ""],
            ["shell_replication", "shell_replication", ""],
            rspecify(os, "http_replication")
        ]
    }

    return tests
}

def setupTestEnvironment(os, edition, maintainer, logFile, runDir) {
    fileOperations([
        folderCreateOperation("${runDir}/tmp"),
    ])

    def subdirs = ['build', 'etc', 'js', 'UnitTests']

    if (edition == "enterprise") {
       subdirs << "enterprise"
    }

    if (os == "windows") {
        for (file in subdirs) {
            powershell "cd ${runDir} ; New-Item -Path ${file} -ItemType SymbolicLink -Value ..\\${file} | Out-Null"
        }
    }
    else {
        for (file in subdirs) {
            sh "ln -s ../${file} ${runDir}/${file}"
        }

        sh "echo `date` > ${logFile}"
    }
}

def executeTests(os, edition, maintainer, mode, engine, portInit, archDir, arch, stageName) {
    def parallelity = 4
    def testIndex = 0
    def tests = getTests(os, edition, maintainer, mode, engine)

    def portInterval = (mode == "cluster") ? 40 : 10

    // this is an `Array.reduce()` in groovy :S
    def testSteps = tests.inject([:]) { testMap, testStruct ->
        def lockIndex = testIndex % parallelity
        def currentIndex = testIndex

        testIndex++

        def name = testStruct[0]
        def test = testStruct[1]
        def testArgs = "--prefix ${os}-${edition}-${mode}-${engine} " +
                       "--configDir etc/jenkins " +
                       "--skipLogAnalysis true " +
                       "--skipTimeCritical true " +
                       "--skipNonDeterministic true " +
                       "--storageEngine ${engine} " +
                       testStruct[2]

        if (mode == "cluster") {
            testArgs += " --cluster true"
        }

        testMap["${stageName}-${name}"] = {
            def logFile       = pwd() + "/" + "${arch}/${name}.log"
            def logFileFailed = pwd() + "/" + "${arch}-FAIL/${name}.log"
            def archRun       = pwd() + "/" + "${arch}-RUN"

            def runDir = "run.${currentIndex}"
            def port   = portInit + currentIndex * portInterval

            testArgs += " --minPort " + port
            testArgs += " --maxPort " + (port + portInterval - 1)

            def command = "./build/bin/arangosh " +
                          "--log.level warning " +
                          "--javascript.execute UnitTests/unittest.js " +
                          " ${test} -- " +
                          testArgs

            try {
                lock("test-${env.NODE_NAME}-${env.JOB_NAME}-${env.BUILD_ID}-${edition}-${maintainer}-${engine}-${lockIndex}") {
                    setupTestEnvironment(os, edition, maintainer, logFile, runDir)

                    try {
                        // seriously...30 minutes is the super absolute max max max.
                        // even in the worst situations ArangoDB MUST be able to finish within 60 minutes
                        // even if the features are green this is completely broken performance wise..
                        // DO NOT INCREASE!!

                        timeout(30) {
                            def tmpDir = pwd() + "/" + runDir + "/tmp"

                            withEnv(["TMPDIR=${tmpDir}", "TEMPDIR=${tmpDir}", "TMP=${tmpDir}"]) {
                                if (os == "windows") {
                                    echo "executing ${command}"
                                    powershell "cd ${runDir} ; ${command} | Add-Content -PassThru ${logFile}"
                                }
                                else {
                                    sh "echo \"Host: `hostname`\" | tee ${logFile}"
                                    sh "echo \"PWD:  `pwd`\" | tee -a ${logFile}"
                                    sh "echo \"Date: `date`\" | tee -a ${logFile}"

                                    command = "(cd ${runDir} ; echo 1 > result ; ${command} ; echo \$? > result) 2>&1 | " +
                                              "tee -a ${logFile} ; exit `cat ${runDir}/result`"
                                    echo "executing ${command}"
                                    sh command
                                }
                            }
                        }
                    }
                    catch (exc) {
                        echo "caught error, copying log to ${logFileFailed}"
                        echo exc.toString()

                        fileOperations([
                            fileCreateOperation(fileContent: 'TEST FAILED', fileName: "${archDir}-FAIL.txt")
                        ])

                        copyFile(os, logFile, logFileFailed)
                        throw exc
                    }
                    finally {
                        def logFileRel       = "${arch}/${name}.log"
                        def logFileFailedRel = "${arch}-FAIL/${name}.log"

                        checkCoresAndSave(os, runDir, name, archRun)

                        archiveArtifacts allowEmptyArchive: true,
                            artifacts: "${logFileRel}, ${logFileFailedRel}",
                            defaultExcludes: false
                    }
                }
            }
            catch (exc) {
                error "test ${name} failed"
            }
        }

        testMap
    }

    parallel testSteps
}

def testCheck(os, edition, maintainer, mode, engine) {
    if (! runTests) {
        echo "Not testing ${os} ${mode} because testing is not enabled"
        return false
    }

    if (! checkEnabledOS(os, 'testing')) {
       return false
    }

    if (! checkEnabledEdition(edition, 'testing')) {
       return false
    }

    if (! checkEnabledMaintainer(maintainer, os, 'building')) {
       return false
    }

    if (restrictions && !restrictions["test-${os}-${edition}-${maintainer}-${mode}-${engine}"]) {
        return false
    }

    return true
}

def testStep(os, edition, maintainer, mode, engine, stageName) {
    return {
        if (testCheck(os, edition, maintainer, mode, engine)) {
            node(testJenkins[os]) {
                stage(stageName) {
                    def archDir    = "${os}-${edition}-${maintainer}"
                    def arch       = "${archDir}/03-test-${mode}-${engine}"
                    def archFail   = "${arch}-FAIL"
                    def archRun    = "${arch}-RUN"

                    // clean the current workspace completely
                    deleteDir()

                    // create directories for the artifacts
                    fileOperations([
                        folderCreateOperation(arch),
                        folderCreateOperation(archFail),
                        folderCreateOperation(archRun)
                    ])

                    // unstash binaries
                    unstashBinaries(os, edition, maintainer)

                    // find a suitable port
                    def port = (getStartPort(os) as Integer)
                    echo "Using start port: ${port}"

                    try {
                        executeTests(os, edition, maintainer, mode, engine, port, archDir, arch, stageName)
                    }
                    finally {
                        // release the port reservation
                        if (os == 'linux' || os == 'mac') {
                            sh "Installation/Pipeline/port.sh --clean ${port}"
                        }
                        else if (os == 'windows') {
                            powershell "remove-item -Force -ErrorAction Ignore C:\\ports\\${port}"
                        }

                        // archive all artifacts
                        archiveArtifacts allowEmptyArchive: true,
                            artifacts: "${arch}-*, ${archRun}/**",
                            defaultExcludes: false
                    }
                }
            }
        }
    }
}

def testStepParallel(os, edition, maintainer, modeList) {
    def branches = [:]

    for (mode in modeList) {
        for (engine in ['mmfiles', 'rocksdb']) {
            def stageName = "test-${os}-${edition}-${maintainer}-${mode}-${engine}";
            branches[stageName] = testStep(os, edition, maintainer, mode, engine, stageName)
        }
    }

    parallel branches
}

// -----------------------------------------------------------------------------
// --SECTION--                                                SCRIPTS RESILIENCE
// -----------------------------------------------------------------------------

// def testResilience(os, engine, foxx) {
//     withEnv(['LOG_COMMUNICATION=debug', 'LOG_REQUESTS=trace', 'LOG_AGENCY=trace']) {
//         if (os == 'linux') {
//             sh "./Installation/Pipeline/linux/test_resilience_${foxx}_${engine}_${os}.sh"
//         }
//         else if (os == 'mac') {
//             sh "./Installation/Pipeline/mac/test_resilience_${foxx}_${engine}_${os}.sh"
//         }
//         else if (os == 'windows') {
//             powershell ".\\Installation\\Pipeline\\test_resilience_${foxx}_${engine}_${os}.ps1"
//         }
//     }
// }

// def testResilienceCheck(os, engine, foxx) {
//     if (! runResilience) {
//         return false
//     }

//     if (os == 'linux' && ! useLinux) {
//         return false
//     }

//     if (os == 'mac' && ! useMac) {
//         return false
//     }

//     if (os == 'windows' && ! useWindows) {
//         return false
//     }

//     if (! useCommunity) {
//         return false
//     }

//     if (restrictions && !restrictions["test-resilience-${foxx}-${engine}-${os}"]) {
//         return false
//     }

//     return true
// }

// def testResilienceStep(os, engine, foxx) {
//     return {
//         node(testJenkins[os]) {
//             def edition = "community"
//             def buildName = "${edition}-${os}"

//             def name = "${os}-${engine}-${foxx}"
//             def arch = "LOG_resilience_${foxx}_${engine}_${os}"

//             stage("resilience-${name}") {
//                 if (os == 'linux' || os == 'mac') {
//                     sh "rm -rf ${arch}"
//                     sh "mkdir -p ${arch}"
//                 }
//                 else if (os == 'windows') {
//                     bat "del /F /Q ${arch}"
//                     powershell "New-Item -ItemType Directory -Force -Path ${arch}"
//                 }

//                 try {
//                     try {
//                         timeout(120) {
//                             unstashBinaries(edition, os)
//                             testResilience(os, engine, foxx)
//                         }

//                         if (findFiles(glob: 'resilience/core*').length > 0) {
//                             error("found core file")
//                         }
//                     }
//                     catch (exc) {
//                         if (os == 'linux' || os == 'mac') {
//                             sh "for i in build resilience/core* tmp; do test -e \"\$i\" && mv \"\$i\" ${arch} || true; done"
//                         }
//                         throw exc
//                     }
//                     finally {
//                         if (os == 'linux' || os == 'mac') {
//                             sh "for i in log-output; do test -e \"\$i\" && mv \"\$i\" ${arch}; done"
//                         }
//                         else if (os == 'windows') {
//                             bat "move log-output ${arch}"
//                         }
//                     }
//                 }
//                 finally {
//                     archiveArtifacts allowEmptyArchive: true,
//                                         artifacts: "${arch}/**",
//                                         defaultExcludes: false
//                 }
//             }
//         }
//     }
// }

// def testResilienceParallel(osList) {
//     def branches = [:]

//     for (foxx in ['foxx', 'nofoxx']) {
//         for (os in osList) {
//             for (engine in ['mmfiles', 'rocksdb']) {
//                 if (testResilienceCheck(os, engine, foxx)) {
//                     def name = "test-resilience-${foxx}-${engine}-${os}"

//                     branches[name] = testResilienceStep(os, engine, foxx)
//                 }
//             }
//         }
//     }

//     if (branches.size() > 1) {
//         parallel branches
//     }
//     else if (branches.size() == 1) {
//         branches.values()[0]()
//     }
// }

// -----------------------------------------------------------------------------
// --SECTION--                                                     SCRIPTS BUILD
// -----------------------------------------------------------------------------

def buildEdition(os, edition, maintainer) {
    def archDir  = "${os}-${edition}-${maintainer}"
    def arch     = "${archDir}/01-build"
    def archFail = "${archDir}/01-build-FAIL"

    fileOperations([
        folderDeleteOperation(arch),
        fileDeleteOperation(excludes: '', includes: "${archDir}-*"),
        folderDeleteOperation(archFail),
        folderCreateOperation(arch)
    ])

    def logFile = "${arch}/build.log"

    try {
        if (os == 'linux' || os == 'mac') {
            sh "echo \"Host: `hostname`\" | tee ${logFile}"
            sh "echo \"PWD:  `pwd`\" | tee -a ${logFile}"
            sh "echo \"Date: `date`\" | tee -a ${logFile}"

            if (os == 'linux') {
                sh "./Installation/Pipeline/build_OS_EDITION_MAINTAINER.sh 64 ${os} ${edition} ${maintainer} ${arch}"
            }
            else if (os == 'mac') {
                sh "./Installation/Pipeline/build_OS_EDITION_MAINTAINER.sh 16 ${os} ${edition} ${maintainer} ${arch}"
            }
        }
        else if (os == 'windows') {
            def tmpDir = "${arch}/tmp"

            fileOperations([
                folderCreateOperation(tmpDir)
            ])

            // withEnv(["TMPDIR=${tmpDir}", "TEMPDIR=${tmpDir}", "TMP=${tmpDir}",
            //          "_MSPDBSRV_ENDPOINT_=${edition}-${env.BUILD_TAG}", "GYP_USE_SEPARATE_MSPDBSRV=1"]) {
                powershell ". .\\Installation\\Pipeline\\windows\\build_${os}_${edition}.ps1"
            // }

            fileOperations([
                folderDeleteOperation(tmpDir)
            ])
        }
    }
    catch (exc) {
        fileOperations([
            fileCreateOperation(fileContent: 'BUILD FAILED', fileName: "${archDir}-FAIL.txt")
        ])

        renameFolder(arch, archFail)
        throw exc
    }
    finally {
        archiveArtifacts allowEmptyArchive: true,
            artifacts: "${archDir}-*, ${arch}/**, ${archFail}/**",
            defaultExcludes: false
    }
}

def buildStepCheck(os, edition, maintainer) {
    if (! checkEnabledOS(os, 'building')) {
       return false
    }

    if (! checkEnabledEdition(edition, 'building')) {
       return false
    }

    if (! checkEnabledMaintainer(maintainer, os, 'building')) {
       return false
    }

    if (restrictions && !restrictions["build-${os}-${edition}-${maintainer}"]) {
        return false
    }

    return true
}

def runEdition(os, edition, maintainer, stageName) {
    return {
        if (buildStepCheck(os, edition, maintainer)) {
            node(buildJenkins[os]) {
                stage(stageName) {
                    timeout(30) {
                        checkoutCommunity()

                        if (edition == "enterprise") {
                            checkoutEnterprise()
                        }

                        // checkoutResilience()
                    }

                    // I concede...we need a lock for windows...I could not get it to run concurrently...
                    // v8 would not build multiple times at the same time on the same machine:
                    // PDB API call failed, error code '24': ' etc etc
                    // in theory it should be possible to parallelize it by setting an environment variable
                    // (see the build script) but for v8 it won't work :(
                    // feel free to recheck if there is time somewhen...this thing here really should not be possible but
                    // ensure that there are 2 concurrent builds on the SAME node building v8 at the same time to properly
                    // test it. I just don't want any more "yeah that might randomly fail. just restart" sentences any more.

                    if (os == "windows") {
                        def hostname = powershell(returnStdout: true, script: "hostname").trim()

                        lock("build-windows-${hostname}") {
                            timeout(90) {
                                buildEdition(os, edition, maintainer)
                                stashBinaries(os, edition, maintainer)
                            }
                        }
                    }
                    else {
                        timeout(90) {
                            buildEdition(os, edition, maintainer)
                            stashBinaries(os, edition, maintainer)
                        }
                    }
                }

                // we only need one jslint test per edition
                if (os == "linux") {
                    stage("jslint-${edition}") {
                        echo "Running jslint for ${edition}"
                        jslint(os, edition, maintainer)
                    }
                }
            }

            testStepParallel(os, edition, maintainer, ['cluster', 'singleserver'])
        }
    }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                              MAIN
// -----------------------------------------------------------------------------

def runOperatingSystems(osList) {
    def branches = [:]

    for (os in osList) {
        for (edition in ['community', 'enterprise']) {
            for (maintainer in ['maintainer', 'user']) {
                def stageName = "build-${os}-${edition}-${maintainer}"
                branches[stageName] = runEdition(os, edition, maintainer, stageName)
            }
        }
    }

    parallel branches
}

timestamps {
    checkCommitMessages()

    node("master") {
        fileOperations([fileCreateOperation(fileContent: overview, fileName: "overview.txt")])
        archiveArtifacts(allowEmptyArchive: true, artifacts: "overview.txt")
    }

    runOperatingSystems(['linux', 'mac', 'windows'])
}
