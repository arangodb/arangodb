//  -*- mode: groovy-mode

def enterpriseRepo = 'https://github.com/arangodb/enterprise'
def credentialsId = '8d893d23-6714-4f35-a239-c847c798e080'

def binariesCommunity = 'build/**,etc/**,Installation/Pipeline/**,js/**,scripts/**,tests/arangodbtests,UnitTests/**,utils/**'
def binariesEnterprise = 'build/**,enterprise/js/**,etc/**,Installation/Pipeline/**,js/**,scripts/**,tests/arangodbtests,UnitTests/**,utils/**'

def PowerShell(psCmd) {
    bat "powershell.exe -NonInteractive -ExecutionPolicy Bypass -Command \"\$ErrorActionPreference='Stop';[Console]::OutputEncoding=[System.Text.Encoding]::UTF8;$psCmd;EXIT \$global:LastExitCode\""
}

def cleanBuild = false

def buildEnterprise = false

def buildLinux = true
def buildMac = false
def buildWindows = false

def checkoutCommunity() {
    retry(3) {
        try {
            checkout scm
    }
    catch (err) {
        echo "GITHUB checkout failed, retrying in 5min"
        echo err.toString()
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
                userRemoteConfigs: [[credentialsId: credentialsId, url: enterpriseRepo]]])

        buildEnterprise = true
    }
    catch (err) {
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
                userRemoteConfigs: [[credentialsId: credentialsId, url: enterpriseRepo]]])
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

            if (msg ==~ /(?i).*ci: *clean[ \\]].*/) {
                echo "using clean build because message contained 'ci: clean'"
                cleanBuild = true
            }

            if (msg ==~ /(?i).*ci: *no-linux[ \\]].*/) {
                echo "not building linux because message contained 'ci: no-linux'"
                buildLinux = false
            }

            if (msg ==~ /(?i).*ci: *mac[ \\]].*/) {
                echo "building mac because message contained 'ci: mac'"
                buildMac = true
            }

            if (msg ==~ /(?i).*ci: *windows[ \\]].*/) {
                echo "building windows because message contained 'ci: windows'"
                buildWindows = true
            }

            if (msg ==~ /(?i).*ci: *enterprise[ \\]].*/) {
                echo "building enterprise because message contained 'ci: enterprise'"
                buildMac = true
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

stage('checkout') {
    node('master') {
        checkoutCommunity()
        checkCommitMessages()
        checkoutEnterprise()

        sh 'rm -f source.*'
        sh 'tar -c -z -f source.tar.gz --exclude "source.*" --exclude "*tmp" *'
        stash includes: 'source.tar.gz', name: 'source'
    }
}

if (buildLinux) {
    stage('build linux') {
        parallel(
            'community': {
                node('linux') {
                    sh 'rm -rf *'

                    unstash 'source'
                    sh 'tar -x -z -p -f source.tar.gz'
                    sh 'mkdir -p artefacts'

                    script {
                        try {
                            cache(maxCacheSize: 50000, caches: [
                                [$class: 'ArbitraryFileCache',
                                 includes: 'build-community-linux.tar.gz',
                                 path: "artefacts"]]) {
                                    if (!cleanBuild) {
                                        sh 'if test -f artefacts/build-community-linux.tar.gz; then tar -x -z -p -f artefacts/build-community-linux.tar.gz; fi'
                                    }

                                    sh 'rm -f artefacts/build-community-linux.tar.gz'
                                    sh './Installation/Pipeline/build_community_linux.sh 16'
                                    sh 'tar -c -z -f artefacts/build-community-linux.tar.gz build-community'
                            }
                        }
                        catch (exc) {
                            throw exc
                        }
                        finally {
                            archiveArtifacts allowEmptyArchive: true, artifacts: 'log-output/**', defaultExcludes: false
                        }
                    }

                    stash includes: binariesCommunity, name: 'build-community-linux'
                }
            },

            'enterprise': {
                if (buildEnterprise) {
                    node('linux') {
                        sh 'rm -rf *'

                        unstash 'source'
                        sh 'tar -x -z -p -f source.tar.gz'
                        sh 'mkdir -p artefacts'

                        script {
                            try {
                                cache(maxCacheSize: 50000, caches: [
                                    [$class: 'ArbitraryFileCache',
                                     includes: 'build-enterprise-linux.tar.gz',
                                     path: "artefacts"]]) {
                                        if (!cleanBuild) {
                                            sh 'if test -f artefacts/build-enterprise-linux.tar.gz; then tar -x -z -p -f artefacts/build-enterprise-linux.tar.gz; fi'
                                        }

                                        sh 'rm -f artefacts/build-enterprise-linux.tar.gz'
                                        sh './Installation/Pipeline/build_enterprise_linux.sh 16'
                                        sh 'tar -c -z -f artefacts/build-enterprise-linux.tar.gz build-enterprise'
                                }
                            }
                            catch (exc) {
                                throw exc
                            }
                            finally {
                                archiveArtifacts allowEmptyArchive: true, artifacts: 'log-output/**', defaultExcludes: false
                            }
                        }

                        stash includes: binariesEnterprise, name: 'build-enterprise-linux'
                    }
                }
                else {
                    echo "Not build enterprise version"
                }
            }
        )
    }
}

stage('build & test') {
    parallel(
        'test-singleserver-community': {
            if (buildLinux) {
                node('linux') {
                    sh 'rm -rf *'
                    unstash 'build-community-linux'
                    echo "Running singleserver community rocksdb linux test"
                    script {
                        try {
                            sh './Installation/Pipeline/test_singleserver_community_rocksdb_linux.sh 8'
                        }
                        catch (exc) {
                            throw exc
                        }
                        finally {
                            archiveArtifacts allowEmptyArchive: true, artifacts: 'log-output/**', defaultExcludes: false
                        }
                    }
                }
            }
        },

        'test-singleserver-enterprise': {
            if (buildLinux && buildEnterprise) {
                node('linux') {
                    sh 'rm -rf *'
                    unstash 'build-enterprise-linux'
                    echo "Running singleserver enterprise mmfiles linux test"
                    script {
                        try {
                            sh './Installation/Pipeline/test_singleserver_enterprise_mmfiles_linux.sh 8'
                        }
                        catch (exc) {
                            throw exc
                        }
                        finally {
                            archiveArtifacts allowEmptyArchive: true, artifacts: 'log-output/**', defaultExcludes: false
                        }
                    }
                }
            }
            else {
                echo "Not build enterprise version"
            }
        },

        'jslint': {
            if (buildLinux) {
                node('linux') {
                    sh 'rm -rf *'
                    unstash 'build-community-linux'
                    echo "Running jslint test"

                    script {
                        try {
                            sh './Installation/Pipeline/test_jslint.sh'
                        }
                        catch (exc) {
                            currentBuild.result = 'UNSTABLE'
                        }
                    }
                }
            }
        }
    )
}




/*

        'build-mac': {
            if (buildMac) {
                node('mac') {
                    if (cleanAll) {
                        sh 'rm -rf *'
                    }
                    else if (cleanBuild) {
                        sh 'rm -rf build-jenkins'
                    }

                    unstash 'source'

                    sh './Installation/Pipeline/build_community_mac.sh 16'
                }
            }
        },

        'build-windows': {
            if (buildWindows) {

                node('windows') {
                    if (cleanAll) {
                        bat 'del /F /Q *'
                    }
                    else if (cleanBuild) {
                        bat 'del /F /Q build'
                    }

                    unstash 'source'

                    PowerShell(". .\\Installation\\Pipeline\\build_enterprise_windows.ps1")
                }
            }
        }
*/