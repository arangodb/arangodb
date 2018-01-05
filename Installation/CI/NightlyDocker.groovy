properties([
    parameters([
        booleanParam(
            defaultValue: false,
            description: 'clean build directories',
            name: 'CleanBuild'
        )
    ])
])

cleanBuild = params.CleanBuild

// github proxy repository
arangodbRepo = 'http://github-proxy.arangodb.biz:8088/github.com/arangodb/arangodb'

// source branch for pull requests
mainBranch = "unknown"

if ("devel" == "devel") {
    mainBranch = "devel"
}
else { 
    mainBranch = "3.3"
}

mainBranch = "feature/night-docker"

if (! env.BRANCH_NAME) {
    env.BRANCH_NAME = mainBranch
}

sourceBranchLabel = env.BRANCH_NAME

if (env.BRANCH_NAME =~ /^PR-/) {
  def prUrl = new URL("https://api.github.com/repos/arangodb/arangodb/pulls/${env.CHANGE_ID}")
  sourceBranchLabel = new groovy.json.JsonSlurper().parseText(prUrl.text).head.label

  def reg = ~/^arangodb:/
  sourceBranchLabel = sourceBranchLabel - reg
}

def shellAndPipe(command, logfile) {
    def cmd = command.replaceAll(/"/, "\\\"")

    sh "echo 'executing ${cmd}' > ${logfile}"
    sh "(echo 1 > \"${logfile}.result\" ; ${cmd} ; echo \$? > \"${logfile}.result\") 2>&1 | ts '[%Y-%m-%d %H:%M:%S]' | tee -a \"${logfile}\" ; exit `cat \"${logfile}.result\"`"
}

def deleteDirDocker(os) {
    if (os == "linux") {
        sh "docker run --rm -v \$(pwd):/workspace alpine rm -rf /workspace/build-deb"
    }

    deleteDir()
}

def checkoutCommunity(os) {
    if (cleanBuild || os == "windows") {
        deleteDirDocker(os)
    }

    retry(3) {
        try {
            def commitHash = checkout(
                changelog: false,
                poll: false,
                scm: [
                    $class: 'GitSCM',
                    branches: [[name: "*/${sourceBranchLabel}"]],
                    doGenerateSubmoduleConfigurations: false,
                    extensions: [],
                    submoduleCfg: [],
                    userRemoteConfigs: [[url: arangodbRepo]]]).GIT_COMMIT

            echo "GIT COMMIT: ${commitHash}"
        }
        catch (exc) {
            echo "GITHUB checkout failed, retrying in 1min"
            deleteDir()
            sleep 60
            throw exc
        }
    }
}

timestamps {
    node("linux") {
        echo sh(returnStdout: true, script: 'env | sort')
    }

    timeout(30) {
        def os = "linux"

        node("${os}&&dockerhub") {
            stage("checkout") {
                checkoutCommunity(os)
            }

            stage("build") {
                shellAndPipe("./Installation/CI/linux/nightly-docker.sh devel", "nightly-docker.log")
            }
        }
    }
}
