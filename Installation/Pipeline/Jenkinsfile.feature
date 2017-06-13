//  -*- mode: groovy-mode

def binaries = 'build/**,etc/**,Installation/Pipeline/**,js/**,scripts/**,tests/**,UnitTests/**,utils/**'
def enterpriseRepo = 'https://github.com/arangodb/enterprise'
def credentialsId = '8d893d23-6714-4f35-a239-c847c798e080'

def PowerShell(psCmd) {
    bat "powershell.exe -NonInteractive -ExecutionPolicy Bypass -Command \"\$ErrorActionPreference='Stop';[Console]::OutputEncoding=[System.Text.Encoding]::UTF8;$psCmd;EXIT \$global:LastExitCode\""
}

def cleanBuild = false
def cleanAll = false

stage('checkout') {
    node('master') {
        milestone(1)

        retry(3) {
            try {
                checkout scm

                script {
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

                            if (msg ==~ /(?i).*ci: *clean-all[ \\]].*/) {
                                echo "using clean all because message contained 'ci: clean-all'"
                                cleanAll = true
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

            }
            catch (err) {
                echo "GITHUB checkout failed, retrying in 5min"
                echo err.toString()
                sleep 300
            }
        }

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

        stash includes: '**', name: 'source'

        milestone(2)
    }
}

stage('build linux') {
    node('linux') {
        if (cleanAll) {
            sh 'rm -rf *'
        } else if (cleanBuild) {
            sh 'rm -rf build-jenkins'
        }

        unstash 'source'

        sh './Installation/Pipeline/build_enterprise_linux.sh 16'
        stash includes: binaries, name: 'build-enterprise-linux'
    }
}

stage('build & test') {
    parallel(
        'test-singleserver-community': {
            node('linux') {
                sh 'rm -rf *'
                unstash 'build-enterprise-linux'
                echo "Running singleserver comunity mmfiles linux test"
                sh './Installation/Pipeline/test_singleserver_community_mmfiles_linux.sh 8'
                archiveArtifacts allowEmptyArchive: true, artifacts: 'log-output/**', defaultExcludes: false
            }
        },

        'test-cluster-enterprise': {
            node('linux') {
                sh 'rm -rf *'
                unstash 'build-enterprise-linux'
                echo "Running cluster enterprise rocksdb linux test"
                sh './Installation/Pipeline/test_cluster_enterprise_rocksdb_linux.sh 8'
            }
        },

        'jslint': {
            node('linux') {
                sh 'rm -rf *'
                unstash 'build-enterprise-linux'
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
        },

        'build-mac': {
            node('mac') {
                if (cleanAll) {
                    sh 'rm -rf *'
                } else if (cleanBuild) {
                    sh 'rm -rf build-jenkins'
                }

                unstash 'source'

                sh './Installation/Pipeline/build_community_mac.sh 16'
            }
        },

        'build-windows': {
            node('windows') {
                if (cleanAll) {
                    bat 'del /F /Q *'
                } else if (cleanBuild) {
                    bat 'del /F /Q build'
                }

                unstash 'source'

                PowerShell(". .\\Installation\\Pipeline\\build_enterprise_windows.ps1")
            }
        }
    )
}
