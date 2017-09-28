//  -*- mode: groovy-mode

properties([buildDiscarder(logRotator(artifactDaysToKeepStr: '3', artifactNumToKeepStr: '5', daysToKeepStr: '3', numToKeepStr: '5'))])


// -----------------------------------------------------------------------------
// --SECTION--                                             SELECTABLE PARAMETERS
// -----------------------------------------------------------------------------

def defaultLinux = true
def defaultMac = false
def defaultWindows = false
def defaultBuild = true
def defaultDocker = false
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
            defaultValue: defaultDocker,
            description: 'build docker images',
            name: 'Docker'
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

// build docker image
useDocker = params.Docker

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

// results
resultsKeys = []
resultsStart = [:]
resultsStop = [:]
resultsStatus = [:]
resultsLink = [:]

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
if (env.JOB_BASE_NAME == "arangodb-ci-devel") {
    env.BRANCH_NAME = "devel"
}

sourceBranchLabel = env.BRANCH_NAME

if (env.BRANCH_NAME =~ /^PR-/) {
  def prUrl = new URL("https://api.github.com/repos/arangodb/arangodb/pulls/${env.CHANGE_ID}")
  sourceBranchLabel = new groovy.json.JsonSlurper().parseText(prUrl.text).head.label

  def reg = ~/^arangodb:/
  sourceBranchLabel = sourceBranchLabel - reg
}

branchLabel = sourceBranchLabel.replaceAll(/[^0-9a-z]/, '-')

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

def checkCores(os, runDir) {
    if (os == 'windows') {
        def files = findFiles(glob: "${runDir}/*.dmp")
        
        if (files.length > 0) {
            error("found windows core file")
        }
    }
    else {
        def files = findFiles(glob: "${runDir}/core*")

        if (files.length > 0) {
            error("found linux core file")
        }
    }
}

def saveCores(os, runDir, name, archRun) {
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

            return true
        }
    }
    else {
        sh "for i in logs out tmp result; do test -e \"${runDir}/\$i\" && mv \"${runDir}/\$i\" \"${archRun}/${name}.\$i\" || true; done"

        def files = findFiles(glob: "${runDir}/core*")

        if (files.length > 0) {
            for (file in files) {
                sh "mv ${file} ${archRun}"
            }

            sh "cp -a build/bin/* ${archRun}"

            return true
        }
    }

    return false
}

def getStartPort(os) {
    if (os == "windows") {
        return powershell (returnStdout: true, script: "Installation/Pipeline/port.ps1")
    }
    else {
        return sh (returnStdout: true, script: "Installation/Pipeline/port.sh")
    }
}

def releaseStartPort(os, port) {
    if (port != 0) {
        if (os == 'linux' || os == 'mac') {
            sh "Installation/Pipeline/port.sh --clean ${port}"
        }
        else if (os == 'windows') {
            powershell "remove-item -Force -ErrorAction Ignore C:\\ports\\${port}"
        }
    }
}

def rspecify(os, test) {
    if (os == "windows") {
        return [test, test, "--rspec C:\\tools\\ruby23\\bin\\rspec.bat"]
    } else {
        return [test, test, ""]
    }
}

def deleteDirDocker(os) {
    if (os == "linux") {
        sh "docker run --rm -v \$(pwd):/workspace alpine rm -rf /workspace/build-deb"
    }

    deleteDir()
}

def shellAndPipe(command, logfile) {
    def cmd = command.replaceAll(/"/, "\\\"")

    echo "executing ${cmd}"
    sh "(echo 1 > \"${logfile}.result\" ; ${cmd} ; echo \$? > \"${logfile}.result\") 2>&1 | ts '[%Y-%m-%d %H:%M:%S]' | tee -a \"${logfile}\" ; exit `cat \"${logfile}.result\"`"
}

def logStartStage(os, logFile, link) {
    resultsKeys << logFile

    resultsStart[logFile] = new Date()
    resultsLink[logFile] = link
    resultsStatus[logFile] = "started"

    echo "started ${logFile}: ${resultsStart[logFile]}"

    if (os == "linux") {
        shellAndPipe("echo 'started ${logFile}: ${resultsStart[logFile]}'", logFile)
    }

    generateResult()
}

def logStopStage(os, logFile) {
    resultsStop[logFile] = new Date()
    resultsStatus[logFile] = "finished"

    echo "finished ${logFile}: ${resultsStop[logFile]}"

    if (os == "linux") {
        shellAndPipe("echo 'finished ${logFile}: ${resultsStop[logFile]}'", logFile)
    }

    generateResult()
}

def logExceptionStage(os, logFile, link, exc) {
    def msg = exc.toString()

    resultsStop[logFile] = new Date()
    resultsStatus[logFile] = "failed ${msg}"

    if (link != null) {
        resultsLink[logFile] = link
    }

    echo "failed ${logFile}: ${resultsStop[logFile]} ${msg}"

    if (os == "linux") {
        shellAndPipe("echo 'failed ${logFile}: ${resultsStart[logFile]} ${msg}'", logFile)
    }

    generateResult()
}

def generateResult() {
    def results = ""
    def html = "<html><body><table>\n"
    html += "<tr><th>Name</th><th>Start</th><th>Stop</th><th>Duration</th><th>Message</th></tr>\n"

    for (key in resultsKeys) {
        def start = resultsStart[key] ?: ""
        def stop = resultsStop[key] ?: ""
        def msg = resultsStatus[key] ?: ""
        def link = resultsLink[key] ?: ""

        if (start != "" && stop == "") {
            stop = new Date()
        }

        def diff = (start != "" && stop != "") ? groovy.time.TimeCategory.minus(stop, start) : "-"
        def startf = start == "" ? "-" : start.format('yyyy/MM/dd HH:mm:ss')
        def stopf = stop == "" ? "-" : stop.format('yyyy/MM/dd HH:mm:ss')
        def color = 'bgcolor="#FFA0A0"'

        def la = ""
        def lb = ""

        if (link != "") {
            la = "<a href=\"$link\">"
            lb = "</a>"
        }

        if (msg == "finished") {
            color = 'bgcolor="#A0FFA0"'
        }
        else if (msg == "started") {
            color = 'bgcolor="#A0A0FF"'
            la = ""
            lb = ""
        }

        results += "${key}: ${startf} - ${stopf} (${diff}) ${msg}\n"
        html += "<tr ${color}><td>${la}${key}${lb}</td><td>${startf}</td><td>${stopf}</td><td align=\"right\">${diff}</td><td align=\"right\">${msg}</td></tr>\n"
    }

    html += "</table></body></html>\n"

    node("master") {
        fileOperations([fileCreateOperation(fileContent: results, fileName: "results.txt")])
        fileOperations([fileCreateOperation(fileContent: html, fileName: "results.html")])

        archiveArtifacts(allowEmptyArchive: true, artifacts: "results.*")
    }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       SCRIPTS SCM
// -----------------------------------------------------------------------------

def checkoutCommunity(os) {
    if (cleanBuild) {
        deleteDirDocker(os)
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
            deleteDir()
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

            restrictions = [
                // OS EDITION MAINTAINER
                "build-linux-community-maintainer" : true,
                "build-linux-enterprise-maintainer" : true,
                "build-linux-community-user" : true,
                "build-linux-enterprise-user" : true,
                "build-mac-community-user" : true,
                "build-mac-enterprise-user" : true,
                "build-windows-community-user" : true,
                "build-windows-enterprise-user" : true,

                // OS EDITION MAINTAINER MODE ENGINE
                "test-linux-community-maintainer-singleserver-mmfiles" : true,
                "test-linux-community-maintainer-singleserver-rocksdb" : true,
                "test-linux-enterprise-user-cluster-mmfiles" : true,
                "test-linux-enterprise-user-cluster-rocksdb" : true,
                "test-mac-community-user-singleserver-rocksdb" : true,
                "test-mac-enterprise-user-cluster-rocksdb" : true,
                "test-windows-community-user-singleserver-rocksdb" : true,
                "test-windows-mac-enterprise-user-cluster-rocksdb" : true,
            ]
        }
        else if (env.BRANCH_NAME =~ /^PR-/) {
            echo "build of PR"

            restrictions = [
                // OS EDITION MAINTAINER
                "build-linux-community-maintainer" : true,
                "build-linux-enterprise-maintainer" : true,
                "build-mac-enterprise-user" : true,
                "build-windows-enterprise-maintainer" : true,

                // OS EDITION MAINTAINER MODE ENGINE
                "test-linux-enterprise-maintainer-cluster-rocksdb" : true,
                "test-linux-community-maintainer-singleserver-mmfiles" : true
            ]
        }
        else {
            echo "build of branch"

            useDocker = false

            restrictions = [
                // OS EDITION MAINTAINER
                "build-linux-enterprise-maintainer" : true,

                // OS EDITION MAINTAINER MODE ENGINE
                "test-linux-enterprise-maintainer-singleserver-rocksdb" : true,
                "test-linux-enterprise-maintainer-cluster-mmfiles" : true
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
Building Docker: ${useDocker}
Running Tests: ${runTests}
"""
    }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                     SCRIPTS STASH
// -----------------------------------------------------------------------------

def stashBuild(os, edition, maintainer) {
    lock("stashing-${branchLabel}-${os}-${edition}-${maintainer}") {
        if (os == 'linux' || os == 'mac') {
            def name = "build.tar.gz"

            sh "rm -f ${name}"
            sh "GZIP=-1 tar cpzf ${name} build"
            sh "scp ${name} c1:/vol/cache/build-${branchLabel}-${os}-${edition}-${maintainer}.tar.gz"
        }
        else if (os == 'windows') {
            def name = "build.zip"

            bat "del /F /Q ${name}"
            powershell "7z a ${name} -r -bd -mx=1 build"
            powershell "echo 'y' | pscp -i C:\\Users\\Jenkins\\.ssh\\putty-jenkins.ppk ${name} jenkins@c1:/vol/cache/build-${branchLabel}-${os}-${edition}-${maintainer}.zip"
        }
    }
}

def unstashBuild(os, edition, maintainer) {
    lock("stashing-${branchLabel}-${os}-${edition}-${maintainer}") {
        try {
            if (os == "windows") {
                powershell "echo 'y' | pscp -i C:\\Users\\Jenkins\\.ssh\\putty-jenkins.ppk jenkins@c1:/vol/cache/build-${branchLabel}-${os}-${edition}-${maintainer}.zip build.zip"
                powershell "Expand-Archive -Path build.zip -Force -DestinationPath ."
            }
            else {
                sh "scp c1:/vol/cache/build-${branchLabel}-${os}-${edition}-${maintainer}.tar.gz build.tar.gz"
                sh "tar xpzf build.tar.gz"
            }
        }
        catch (exc) {
        }
    }
}

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
        powershell "copy build\\tests\\RelWithDebInfo\\* build\\bin"
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
        fileDeleteOperation(excludes: '', includes: "${archDir}-*"),
        folderDeleteOperation(arch),
        folderDeleteOperation(archFail),
        folderCreateOperation(arch)
    ])

    def logFile = "${arch}/jslint.log"

    try {
        logStartStage(os, logFile, logFile)

        shellAndPipe("./Installation/Pipeline/test_jslint.sh", logFile)
        sh "if grep ERROR ${logFile}; then exit 1; fi"

        logStopStage(os, logFile)
    }
    catch (exc) {
        logExceptionStage(os, logFile, "${archFail}/jslint.log", exc)

        renameFolder(arch, archFail)
        fileOperations([fileCreateOperation(fileContent: 'JSLINT FAILED', fileName: "${archDir}-FAIL.txt")])
        throw exc
    }
    finally {
        archiveArtifacts allowEmptyArchive: true,
            artifacts: "${archDir}-FAIL.txt, ${arch}/**, ${archFail}/**",
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
            ["catch", "catch", "--skipCache false"],
            ["cluster_sync", "cluster_sync", ""],
            ["dfdb", "dfdb", ""],
            ["replication_ongoing", "replication_ongoing", ""],
            ["replication_static", "replication_static", ""],
            ["replication_sync", "replication_sync", ""],
            ["shell_replication", "shell_replication", ""],
            rspecify(os, "http_replication")
        ]

        if (maintainer == "maintainer" && os == "linux") {
            tests += [
                ["recovery", "recovery", ""]
            ]
        }
    }

   if (mode == "cluster") {
        tests += [
            ["resilience", "resilience", ""]
        ]
    }
 
    return tests
}

def setupTestEnvironment(os, edition, maintainer, logFile, runDir) {
    fileOperations([
        folderCreateOperation("${runDir}/tmp"),
    ])

    def subdirs = ['build', 'etc', 'js', 'scripts', 'UnitTests']

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

def singleTest(os, edition, maintainer, mode, engine, test, testArgs, testIndex, stageName, name, port) {
  return {
          def portInterval = 40

          stage("${stageName}-${name}") {
              def archDir  = "${os}-${edition}-${maintainer}"
              def arch     = "${archDir}/03-test-${mode}-${engine}"
              def archFail = "${arch}-FAIL"
              def archRun  = "${arch}-RUN"

              def logFile          = pwd() + "/" + "${arch}/${name}.log"
              def logFileRel       = "${arch}/${name}.log"
              def logFileFailed    = pwd() + "/" + "${arch}-FAIL/${name}.log"
              def logFileFailedRel = "${arch}-FAIL/${name}.log"

              def runDir = "run.${testIndex}"

              logStartStage(os, logFileRel, logFileRel)

              try {

                  // setup links
                  setupTestEnvironment(os, edition, maintainer, logFile, runDir)

                  // assemble command
                  def command = "./build/bin/arangosh " +
                                "-c etc/jenkins/arangosh.conf " +
                                "--log.level warning " +
                                "--javascript.execute UnitTests/unittest.js " +
                                "${test} -- " +
                                "${testArgs} " + 
                                "--minPort " + (port + testIndex * portInterval) + " " +
                                "--maxPort " + (port + (testIndex + 1) * portInterval - 1)

                  // 30 minutes is the super absolute max max max.
                  // even in the worst situations ArangoDB MUST be able to
                  // finish within 60 minutes. Even if the features are green
                  // this is completely broken performance wise...
                  // DO NOT INCREASE!!

                  timeout(os == 'linux' ? 30 : 60) {
                      def tmpDir = pwd() + "/" + runDir + "/tmp"

                      withEnv(["TMPDIR=${tmpDir}", "TEMPDIR=${tmpDir}", "TMP=${tmpDir}"]) {
                          if (os == "windows") {
                              def hostname = powershell(returnStdout: true, script: "hostname")

                              echo "executing ${command} on ${hostname}"
                              powershell "cd ${runDir} ; ${command} | Add-Content -PassThru ${logFile}"
                          }
                          else {
                              shellAndPipe("echo \"Host: `hostname`\"", logFile)
                              shellAndPipe("echo \"PWD:  `pwd`\"", logFile)
                              shellAndPipe("echo \"Date: `date`\"", logFile)

                              shellAndPipe("cd ${runDir} ; ./build/bin/arangosh --version", logFile)

                              command = "(cd ${runDir} ; ${command})"
                              echo "executing ${command}"
                              shellAndPipe(command, logFile)
                          }
                      }
                  }

                  checkCores(os, runDir)
                  logStopStage(os, logFileRel)
              }
              catch (exc) {
                  logExceptionStage(os, logFileRel, logFileFailedRel, exc)

                  def msg = exc.toString()

                  echo "caught error, copying log to ${logFileFailed}: ${msg}"

                  fileOperations([
                      fileCreateOperation(fileContent: "TEST FAILED: ${msg}", fileName: "${archDir}-FAIL.txt")
                  ])

                  if (os == 'linux' || os == 'mac') {
                      sh "echo \"${msg}\" >> ${logFile}"
                  }
                  else {
                      powershell "echo \"${msg}\" | Out-File -filepath ${logFile} -append"
                  }

                  copyFile(os, logFile, logFileFailed)
                  throw exc
              }
              finally {
                  saveCores(os, runDir, name, archRun)

                  archiveArtifacts allowEmptyArchive: true,
                      artifacts: "${archDir}-FAIL.txt, ${archRun}/**, ${logFileRel}, ${logFileFailedRel}",
                      defaultExcludes: false
              }
          }
    }
}

def executeTests(os, edition, maintainer, mode, engine, stageName) {
    def archDir  = "${os}-${edition}-${maintainer}"
    def arch     = "${archDir}/03-test-${mode}-${engine}"
    def archFail = "${arch}-FAIL"
    def archRun  = "${arch}-RUN"

    def testIndex = 0
    def tests = getTests(os, edition, maintainer, mode, engine)

    node(testJenkins[os]) {

        // clean the current workspace completely
        deleteDirDocker(os)

        // create directories for the artifacts
        fileOperations([
            fileDeleteOperation(excludes: '', includes: "${archDir}-*"),
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
            // this is an `Array.reduce()` in groovy :S
            def testSteps = tests.inject([:]) { testMap, testStruct ->
                def name = testStruct[0]
                def test = testStruct[1]
                def testArgs = "--prefix ${os}-${edition}-${mode}-${engine} " +
                               "--configDir etc/jenkins " +
                               "--skipLogAnalysis true " +
                               "--skipTimeCritical true " +
                               "--skipNondeterministic true " +
                               "--storageEngine ${engine} " +
                               testStruct[2]

                if (mode == "cluster") {
                    testArgs += " --cluster true"
                }

                testIndex++

                testMap["${stageName}-${name}"] = singleTest(os, edition, maintainer, mode, engine,
                                                             test, testArgs, testIndex,
                                                             stageName, name, port)

                return testMap
            }

            // fire all tests
            parallel testSteps
        }
        finally {
            releaseStartPort(os, port)
        }
    }
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
            executeTests(os, edition, maintainer, mode, engine, stageName)
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

    def name = "test-${os}-${edition}-${maintainer}"

    if (branches) {
        try {
            logStartStage(null, name, null)
            parallel branches
            logStopStage(null, name)
        }
        catch (exc) {
            logExceptionStage(null, name, null, exc)
            throw exc
        }
    }
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
        fileDeleteOperation(excludes: '', includes: "${archDir}-*"),
        folderDeleteOperation(arch),
        folderDeleteOperation(archFail),
        folderCreateOperation(arch)
    ])

    def logFile = "${arch}/build.log"

    try {
        logStartStage(os, logFile, logFile)

        if (os == 'linux' || os == 'mac') {
            if (! fileExists('build/Makefile') && ! cleanBuild) {
                unstashBuild(os, edition, maintainer)
            }

            shellAndPipe("echo \"Host: `hostname`\"", logFile)
            shellAndPipe("echo \"PWD:  `pwd`\"", logFile)
            shellAndPipe("echo \"Date: `date`\"", logFile)

            if (os == 'linux') {
                shellAndPipe("./Installation/Pipeline/build_OS_EDITION_MAINTAINER.sh 64 ${os} ${edition} ${maintainer}", logFile)
            }
            else if (os == 'mac') {
                shellAndPipe("./Installation/Pipeline/build_OS_EDITION_MAINTAINER.sh 16 ${os} ${edition} ${maintainer}", logFile)
            }
        }
        else if (os == 'windows') {
            powershell ". .\\Installation\\Pipeline\\windows\\build_${os}_${edition}_${maintainer}.ps1"
        }

        logStopStage(os, logFile)
    }
    catch (exc) {
        logExceptionStage(os, logFile, "${archFail}/build.log", exc)

        def msg = exc.toString()
        
        fileOperations([
            fileCreateOperation(fileContent: "BUILD FAILED: ${msg}", fileName: "${archDir}-FAIL.txt")
        ])

        if (os == 'linux' || os == 'mac') {
            sh "echo \"${msg}\" >> ${logFile}"
        }
        else {
            powershell "echo \"${msg}\" | Out-File -filepath ${logFile} -append"
        }

        renameFolder(arch, archFail)
        throw exc
    }
    finally {
        if (os == "linux") {
            stashBuild(os, edition, maintainer)
        }

        archiveArtifacts allowEmptyArchive: true,
            artifacts: "${archDir}-FAIL.txt, ${arch}/**, ${archFail}/**",
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

def checkoutSource(os, edition) {
    timeout(30) {
        checkoutCommunity(os)

        if (edition == "enterprise") {
            checkoutEnterprise()
        }

        // checkoutResilience()
    }
}

def createDockerImage(edition, maintainer, stageName) {
    def os = "linux"

    return {
        if (buildStepCheck(os, edition, maintainer)) {
            node(buildJenkins[os]) {
                stage(stageName) {
                    checkoutSource(os, edition)

                    def archDir  = "${os}-${edition}-${maintainer}"
                    def arch     = "${archDir}/04-docker"
                    def archFail = "${archDir}/04-docker-FAIL"

                    fileOperations([
                        fileDeleteOperation(excludes: '', includes: "${archDir}-*"),
                        folderDeleteOperation(arch),
                        folderDeleteOperation(archFail),
                        folderCreateOperation(arch)
                    ])

                    def logFile = "${arch}/build.log"

                    def packageName = "${os}-${edition}-${maintainer}"

                    withEnv(["DOCKERTAG=${packageName}-${branchLabel}"]) {
                        try {
                            logStartStage(os, logFile, logFile)

                            shellAndPipe("./scripts/build-docker.sh", logFile)
                            shellAndPipe("docker tag arangodb:${packageName}-${branchLabel} c1.triagens-gmbh.zz:5000/arangodb/${packageName}:${branchLabel}", logFile)
                            shellAndPipe("docker push c1.triagens-gmbh.zz:5000/arangodb/${packageName}:${branchLabel}", logFile)

                            logStopStage(os, logFile)
                        }
                        catch (exc) {
                            logExceptionStage(os, logFile, "${archFail}/build.log", exc)

                            renameFolder(arch, archFail)
                            fileOperations([fileCreateOperation(fileContent: 'DOCKER FAILED', fileName: "${archDir}-FAIL.txt")])
                            throw exc
                        }
                        finally {
                            archiveArtifacts allowEmptyArchive: true,
                                artifacts: "${archDir}-FAIL.txt, ${arch}/**, ${archFail}/**",
                                defaultExcludes: false
                        }
                    }
                }
            }
        }
    }
}

def runEdition(os, edition, maintainer, stageName) {
    return {
        if (buildStepCheck(os, edition, maintainer)) {
            node(buildJenkins[os]) {
                stage(stageName) {
                    checkoutSource(os, edition)

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
                def name = "${os}-${edition}-${maintainer}"
                def stageName = "build-${name}"
                branches[stageName] = runEdition(os, edition, maintainer, stageName)

                if (os == 'linux' && useDocker) {
                    branches["docker-${name}"] = createDockerImage(edition, maintainer, "docker-${name}")
                }
            }
        }
    }

    parallel branches
}

timestamps {
    try {
        node("master") {
            echo sh(returnStdout: true, script: 'env')
        }

        checkCommitMessages()

        node("master") {
            fileOperations([fileCreateOperation(fileContent: overview, fileName: "overview.txt")])
            archiveArtifacts(allowEmptyArchive: true, artifacts: "overview.txt")
        }

        runOperatingSystems(['linux', 'mac', 'windows'])
    }
    finally {
        generateResult()
    }
}
