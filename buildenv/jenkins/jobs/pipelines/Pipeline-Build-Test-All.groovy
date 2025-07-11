/*******************************************************************************
 * Copyright IBM Corp. and others 2018
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following
 * Secondary Licenses when the conditions for such availability set
 * forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
 * General Public License, version 2 with the GNU Classpath
 * Exception [1] and GNU General Public License, version 2 with the
 * OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

/**
 * It builds and tests the Eclipse OpenJ9 extensions for OpenJDK for one or more
 * versions and platforms by launching Pipeline-Build-Test-JDK${SDK_VERSION}-${SPEC}
 * builds. Multiple pipelines are executed in parallel.
 * When TESTS_TARGETS=none, it only builds the Eclipse OpenJ9 extensions for OpenJDK.
 * VARIABLE_FILE allows to run it in a custom configuration on a different server.
 *
 * Parameters:
 *   PLATFORMS: String - Comma separated platforms to build, or `all`. For the list of platforms, see `id=` in the `.spec` files found in the buildspecs directory (the id should be the same as the spec file name without the `.spec`).
 *   OPENJ9_REPO: String - the OpenJ9 git repository URL: e.g. https://github.com/eclipse-openj9/openj9.git (default)
 *   OPENJ9_BRANCH: String - the OpenJ9 branch to clone from: e.g. master (default)
 *   OPENJ9_SHA: String - the last commit SHA of the OpenJ9 repository
 *   OMR_REPO: String - the OMR git repository URL: e.g. https://github.com/eclipse-openj9/openj9-omr.git (default)
 *   OMR_BRANCH: String - the OMR branch to clone from: e.g. openj9 (default)
 *   OMR_SHA: String - the last commit SHA of the OMR repository
 *   ADOPTOPENJDK_REPO: String - the Adoptium AQA testing repository URL: e.g. https://github.com/adoptium/aqa-tests.git
 *   ADOPTOPENJDK_BRANCH: String - the Adoptium AQA testing branch: e.g. master
 *   VENDOR_CODE_REPO: String - the vendor code git repository URL
 *   VENDOR_CODE_BRANCH: String - the vendor code branch to clone from: e.g. master (no default)
 *   VENDOR_CODE_SHA: String - the last commit SHA of the vendor code repository
 *   TESTS_TARGETS: String - The test targets to run. Expected values: _sanity, _extended, none
 *   VARIABLE_FILE: String - the custom variables file. Uses defaults.yml when no value is provided.
 *   VENDOR_REPO: String - the repository URL of a Git repository that stores a custom variables file
 *   VENDOR_BRANCH: String - the vendor branch to clone from
 *   VENDOR_CREDENTIALS_ID: String - the Jenkins credentials to connect to the vendor Git repository if VENDOR_REPO is a private repository
 *   SETUP_LABEL: String - the node label(s) to run this job on; could be any node that has Git installed on it
 *   BUILD_NODE_LABELS: String - the labels of a node to compile and build the Eclipse OpenJ9 extensions for OpenJDK
 *   TEST_NODE_LABELS: String - the labels of a node to run tests on
 *
 *   Node labels could be a single label or node name or a boolean expression(e.g. hw.arch.x86 && sw.os.windows)
 *   Expected value for multiple platforms builds: platform.1=labels.1,platform.2=labels.2,...,etc
 *   e.g. aix_ppc-64_cmprssptrs=csp70027,linux_x86-64=(ci.project.openj9 && hw.arch.x86 && sw.os.ubuntu.14)
 *   Expected value for single platforms builds: label (no platform name required), e.g. csp70027
 *
 *   PERSONAL_BUILD: Choice: true, false - Indicates if is a personal build or not
 *
 *   Note: replicate the following parameters for each supported version, where supported version are: 8, 9, 10, 11, next
 *   Java<version>: Boolean (at least one of the following is required: Java8, Java9, Java10, etc)
 *   OPENJDK<version>_REPO: String - the OpenJDK<version> repository URL: e.g. https://github.com/ibmruntimes/openj9-openjdk-jdk<version>.git (default)
 *   OPENJDK<version>_BRANCH: String - the OpenJDK<version> branch to clone from: e.g. openj9 (default)
 *   OPENJDK<version>_SHA: String - the OpenJDK<version> last commit SHA
 *
 *   OPENJDK<version>_REPO_<platform>: String - the OpenJDK<version> repository URL for <platform>
 *   OPENJDK<version>_BRANCH_<platform>: String - the branch to clone from
 *   OPENJDK<version>_SHA_<platform>: String - the last commit SHA
 *   SUMMARY_AUTO_REFRESH_TIME: String - the downstream summary badge auto-refresh time [in minutes], default: 5
 *   ENABLE_SUMMARY_AUTO_REFRESH: Boolean - flag to enable the downstream summary auto-refresh, default: false
 */

CURRENT_RELEASES = ['8', '11', '17', '21', '23', '24', '25', 'next']

SPECS = ['ppc64_aix' : CURRENT_RELEASES,
         'ppc64le_linux'  : CURRENT_RELEASES,
         'ppc64le_linux_jit' : CURRENT_RELEASES,
         's390x_linux'    : CURRENT_RELEASES,
         's390x_linux_jit' : CURRENT_RELEASES,
         's390x_zos'      : CURRENT_RELEASES,
         'x86-64_linux'   : CURRENT_RELEASES,
         'x86-64_linux_jit' : CURRENT_RELEASES,
         'x86-64_linux_valhalla'   : ['next'],
         'x86-64_linux_vt_standard' : ['next'],
         'x86-64_mac'     : CURRENT_RELEASES,
         'x86-32_windows' : ['8'],
         'x86-64_windows' : CURRENT_RELEASES,
         'aarch64_linux' : CURRENT_RELEASES,
         'aarch64_linux_gcc11' : CURRENT_RELEASES,
         'aarch64_linux_jit' : CURRENT_RELEASES,
         'aarch64_mac' : CURRENT_RELEASES - '8',
         'ppc64_aix_ojdk292' : ['8', '11'],
         'ppc64le_linux_ojdk292' : ['8', '11'],
         's390x_linux_ojdk292' : ['8', '11'],
         's390x_zos_ojdk292' : ['11'],
         'x86-64_linux_ojdk292' : ['8', '11'],
         'x86-64_mac_ojdk292' : ['8', '11'],
         'x86-32_windows_ojdk292' : ['8'],
         'x86-64_windows_ojdk292' : ['8', '11'],
         'aarch64_linux_ojdk292' : ['8', '11'],
         'aarch64_mac_ojdk292' : ['11'],
         'aarch64_linux_aot'  : CURRENT_RELEASES,
         'aarch64_mac_aot'    : CURRENT_RELEASES - '8',
         'ppc64_aix_aot'      : CURRENT_RELEASES,
         'ppc64le_linux_aot'  : CURRENT_RELEASES,
         's390x_linux_aot'    : CURRENT_RELEASES,
         'x86-64_linux_aot'   : CURRENT_RELEASES,
         'x86-64_mac_aot'     : CURRENT_RELEASES,
         'x86-64_windows_aot' : CURRENT_RELEASES,
         'ppc64le_linux_valhalla'   : ['next'],
         'ppc64le_linux_vt_standard' : ['next'],
         'aarch64_linux_valhalla'   : ['next'],
         'aarch64_linux_vt_standard' : ['next'],
         'x86-64_mac_valhalla'   : ['next'],
         'x86-64_mac_vt_standard' : ['next'],
         's390x_linux_valhalla'   : ['next'],
         's390x_linux_vt_standard' : ['next'],
         'x86-64_windows_valhalla'   : ['next'],
         'x86-64_windows_vt_standard' : ['next'],
         'ppc64_aix_valhalla'   : ['next'],
         'ppc64_aix_vt_standard' : ['next']]

// SHORT_NAMES is used for PullRequest triggers
// TODO Combine SHORT_NAMES and SPECS
SHORT_NAMES = ['all' : ['ppc64le_linux', 's390x_linux', 'x86-64_linux', 'ppc64_aix', 'x86-64_windows', 'x86-32_windows', 'x86-64_mac', 'aarch64_linux', 'aarch64_mac'],
            'aix' : ['ppc64_aix'],
            'zlinux' : ['s390x_linux'],
            'zlinuxjit' : ['s390x_linux_jit'],
            'plinux' : ['ppc64le_linux'],
            'plinuxjit' : ['ppc64le_linux_jit'],
            'xlinux' : ['x86-64_linux'],
            'xlinuxjit' : ['x86-64_linux_jit'],
            'xlinuxval' : ['x86-64_linux_valhalla'],
            'xlinuxvalst' : ['x86-64_linux_vt_standard'],
            'win32' : ['x86-32_windows'],
            'win' : ['x86-64_windows'],
            'osx' : ['x86-64_mac'],
            'xmac' : ['x86-64_mac'],
            'alinux' : ['aarch64_linux'],
            'alinux64' : ['aarch64_linux'],
            'alinux64gcc11' : ['aarch64_linux_gcc11'],
            'alinux64jit' : ['aarch64_linux_jit'],
            'amac' : ['aarch64_mac'],
            'zos' : ['s390x_zos'],
            'aixojdk292' : ['ppc64_aix_ojdk292'],
            'plinuxojdk292' : ['ppc64le_linux_ojdk292'],
            'zlinuxojdk292' : ['s390x_linux_ojdk292'],
            'xlinuxojdk292' : ['x86-64_linux_ojdk292'],
            'win32ojdk292' : ['x86-32_windows_ojdk292'],
            'winojdk292' : ['x86-64_windows_ojdk292'],
            'osxojdk292' : ['x86-64_mac_ojdk292'],
            'xmacojdk292' : ['x86-64_mac_ojdk292'],
            'alinux64ojdk292' : ['aarch64_linux_ojdk292'],
            'amacojdk292' : ['aarch64_mac_ojdk292'],
            'zosojdk292' : ['s390x_zos_ojdk292'],
            'zosxlojdk292' : ['s390x_zos_xl_ojdk292'],
            'zoslargeheapojdk292' : ['s390x_zos_xl_ojdk292'],
            'aixaot' : ['ppc64_aix_aot'],
            'alinuxaot' : ['aarch64_linux_aot'],
            'amacaot' : ['aarch64_mac_aot'],
            'plinuxaot' : ['ppc64le_linux_aot'],
            'zlinuxaot' : ['s390x_linux_aot'],
            'xlinuxaot' : ['x86-64_linux_aot'],
            'osxaot' : ['x86-64_mac_aot'],
            'xmacaot' : ['x86-64_mac_aot'],
            'winaot' : ['x86-64_windows_aot'],
            'alinuxval' : ['aarch64_linux_valhalla'],
            'alinuxvalst' : ['aarch64_linux_vt_standard'],
            'zlinuxval' : ['s390x_linux_valhalla'],
            'zlinuxvalst' : ['s390x_linux_vt_standard'],
            'winval' : ['x86-64_windows_valhalla'],
            'winvalst' : ['x86-64_windows_vt_standard'],
            'osxval' : ['x86-64_mac_valhalla'],
            'xmacval' : ['x86-64_mac_valhalla'],
            'osxvalst' : ['x86-64_mac_vt_standard'],
            'xmacvalst' : ['x86-64_mac_vt_standard'],
            'plinuxval' : ['ppc64le_linux_valhalla'],
            'plinuxvalst' : ['ppc64le_linux_vt_standard'],
            'aixval' : ['ppc64_aix_valhalla'],
            'aixvalst' : ['ppc64_aix_vt_standard']]

// Initialize all PARAMETERS (params) to Groovy Variables even if they are not passed
echo "Initialize all PARAMETERS..."
SETUP_LABEL = (params.SETUP_LABEL) ? params.SETUP_LABEL : "worker"
echo "Setup SETUP_LABEL:'${SETUP_LABEL}'"
TESTS_TARGETS = (params.TESTS_TARGETS) ? params.TESTS_TARGETS : ""
echo "TESTS_TARGETS:'${TESTS_TARGETS}'"
PERSONAL_BUILD = (params.PERSONAL_BUILD) ? params.PERSONAL_BUILD : ""
echo "PERSONAL_BUILD:'${PERSONAL_BUILD}'"
BUILD_NODE_LABELS = (params.BUILD_NODE_LABELS) ? params.BUILD_NODE_LABELS : ""
echo "BUILD_NODE_LABELS:'${BUILD_NODE_LABELS}'"
TEST_NODE_LABELS = (params.TEST_NODE_LABELS) ? params.TEST_NODE_LABELS : ""
echo "TEST_NODE_LABELS:'${TEST_NODE_LABELS}'"
PROMOTE = (params.PROMOTE) ? params.PROMOTE : ""
echo "PROMOTE:'${PROMOTE}'"
SLACK_CHANNEL = (params.SLACK_HANDLE) ? params.SLACK_HANDLE : ""
echo "SLACK_CHANNEL:'${SLACK_CHANNEL}'"
BUILD_IDENTIFIER = (params.BUILD_IDENTIFIER) ? params.BUILD_IDENTIFIER : ""
echo "BUILD_IDENTIFIER:'${BUILD_IDENTIFIER}'"
AUTOMATIC_GENERATION = (params.AUTOMATIC_GENERATION) ? params.AUTOMATIC_GENERATION : 'true'
echo "AUTOMATIC_GENERATION:'${AUTOMATIC_GENERATION}'"
TIMEOUT_TIME = (params.TIMEOUT_TIME) ? params.TIMEOUT_TIME : '12'
echo "TIMEOUT_TIME: ${TIMEOUT_TIME}"
TIMEOUT_UNIT = (params.TIMEOUT_UNITS) ? params.TIMEOUT_UNITS : 'HOURS'
echo "TIMEOUT_UNIT: ${TIMEOUT_UNIT}"
CUSTOM_DESCRIPTION = (params.CUSTOM_DESCRIPTION) ? params.CUSTOM_DESCRIPTION : ""
echo "CUSTOM_DESCRIPTION:'${CUSTOM_DESCRIPTION}'"

// param PLATFORMS is a string, we convert it to an array later on
PLATFORMS = []
echo "PLATFORMS:'${params.PLATFORMS}'"

// Pull Request Builds
ghprbPullId = (params.ghprbPullId) ? params.ghprbPullId : ""
echo "ghprbPullId:'${ghprbPullId}'"
ghprbGhRepository = (params.ghprbGhRepository) ? params.ghprbGhRepository : ""
echo "ghprbGhRepository:'${ghprbGhRepository}'"
ghprbCommentBody = (params.ghprbCommentBody) ? params.ghprbCommentBody : ""
echo "ghprbCommentBody:'${ghprbCommentBody}'"
ghprbTargetBranch = (params.ghprbTargetBranch) ? params.ghprbTargetBranch : ""
echo "ghprbTargetBranch:'${ghprbTargetBranch}'"
ghprbActualCommit = (params.ghprbActualCommit) ? params.ghprbActualCommit : ""
echo "ghprbActualCommit:'${ghprbActualCommit}'"
ghprbPullLink = (params.ghprbPullLink) ? params.ghprbPullLink : ""
echo "ghprbPullLink:'${ghprbPullLink}'"

// If custom repo/branch/refspec is passed, use it,
// elif build is OpenJ9 PR, use pr merge-ref/refspec,
// else use eclipse/master/blank for defaults respectively.
SCM_REPO = 'https://github.com/eclipse-openj9/openj9.git'
if (params.SCM_REPO) {
    SCM_REPO = params.SCM_REPO
}
echo "SCM_REPO:'${SCM_REPO}'"
SCM_BRANCH = 'refs/heads/master'
if (params.SCM_BRANCH) {
    SCM_BRANCH = params.SCM_BRANCH
} else if (ghprbPullId && ghprbGhRepository ==~ /.*\/openj9/) {
    SCM_BRANCH = sha1
}
echo "SCM_BRANCH:'${SCM_BRANCH}'"
SCM_REFSPEC = ''
if (params.SCM_REFSPEC) {
    SCM_REFSPEC = params.SCM_REFSPEC
} else if (ghprbPullId && ghprbGhRepository ==~ /.*\/openj9/) {
    SCM_REFSPEC = "+refs/pull/${ghprbPullId}/merge:refs/remotes/origin/pr/${ghprbPullId}/merge"
}
echo "SCM_REFSPEC:'${SCM_REFSPEC}'"

SUMMARY_AUTO_REFRESH_TIME = (params.SUMMARY_AUTO_REFRESH_TIME) ? params.SUMMARY_AUTO_REFRESH_TIME : '5'
echo "SUMMARY_AUTO_REFRESH_TIME:'${SUMMARY_AUTO_REFRESH_TIME}' [minutes]"
ENABLE_SUMMARY_AUTO_REFRESH = (params.keySet().contains('ENABLE_SUMMARY_AUTO_REFRESH')) ? params.ENABLE_SUMMARY_AUTO_REFRESH : false
echo "ENABLE_SUMMARY_AUTO_REFRESH:'${ENABLE_SUMMARY_AUTO_REFRESH}'"

RELEASES = []

OPENJDK_REPO = [:]
OPENJDK_BRANCH = [:]
OPENJDK_SHA = [:]

BUILD_SPECS = [:]
builds = [:]
pipelineNames = []
pipelinesStatus = [:]
buildFile = ''

try {
    timeout(time: TIMEOUT_TIME.toInteger(), unit: TIMEOUT_UNIT) {
        timestamps {
            node(SETUP_LABEL) {
                try {
                    def gitConfig = scm.getUserRemoteConfigs().get(0)
                    def remoteConfigParameters = [url: "${gitConfig.getUrl()}"]
                    remoteConfigParameters.put("refspec", SCM_REFSPEC)

                    if (gitConfig.getCredentialsId()) {
                        remoteConfigParameters.put("credentialsId", "${gitConfig.getCredentialsId()}")
                    }

                    checkout changelog: false,
                            poll: false,
                            scm: [$class: 'GitSCM',
                            branches: [[name: SCM_BRANCH]],
                            doGenerateSubmoduleConfigurations: false,
                            extensions: [[$class: 'CloneOption',
                                          reference: "${HOME}/openjdk_cache"],
                                        [$class: 'SparseCheckoutPaths', sparseCheckoutPaths: [[path: 'buildenv/jenkins']]]],
                            userRemoteConfigs: [remoteConfigParameters]]

                    variableFile = load 'buildenv/jenkins/common/variables-functions.groovy'
                    buildFile = load 'buildenv/jenkins/common/pipeline-functions.groovy'

                    // Determine if build is a PullRequest
                    if (ghprbPullId) {
                        buildFile.setup_pull_request()
                    }

                    // Add link of PullRequest in description, if it is set
                    if (ghprbPullLink) {
                        def ghprbPullLink_hyperlink_tag = "<a href='${ghprbPullLink}'>PR #${ghprbPullId}</a>: ${ghprbPullTitle}"
                        CUSTOM_DESCRIPTION = ((CUSTOM_DESCRIPTION) ? CUSTOM_DESCRIPTION + "<br>" : "") + ghprbPullLink_hyperlink_tag
                    }

                    BUILD_SPECS.putAll(variableFile.get_specs(SPECS))

                    // parse variables file and initialize variables
                    variableFile.set_job_variables('wrapper')

                    SHAS = buildFile.get_shas(OPENJDK_REPO, OPENJDK_BRANCH, OPENJ9_REPO, OPENJ9_BRANCH, OMR_REPO, OMR_BRANCH, VENDOR_CODE_REPO, VENDOR_CODE_BRANCH)

                    if (PERSONAL_BUILD.equalsIgnoreCase('true')) {
                        // update build description
                        currentBuild.description += "<br/>${PLATFORMS}"
                    }

                    def BUILD_NODES = get_node_labels(BUILD_NODE_LABELS, BUILD_SPECS.keySet())
                    def TEST_NODES = get_node_labels(TEST_NODE_LABELS, BUILD_SPECS.keySet())

                    // Stash DSL file so we can quickly load it on Jenkins Manager node
                    if (AUTOMATIC_GENERATION != 'false') {
                        stash includes: 'buildenv/jenkins/jobs/pipelines/Pipeline_Template.groovy', name: 'DSL'
                    }

                    BUILD_SPECS.each { SPEC, SDK_VERSIONS ->
                        if (VARIABLES."${SPEC}") {
                            SDK_VERSIONS.each { SDK_VERSION ->
                                def job_name = get_pipeline_name(SPEC, SDK_VERSION)
                                pipelineNames.add(job_name)

                                // set OpenJDK repos and branch for the downstream build
                                def REPO = OPENJDK_REPO.get(SDK_VERSION).get(SPEC)
                                def BRANCH = OPENJDK_BRANCH.get(SDK_VERSION).get(SPEC)

                                // set nodes for the downstream builds
                                def BUILD_NODE = ''
                                if (BUILD_NODES[SPEC]) {
                                    BUILD_NODE = BUILD_NODES[SPEC]
                                }
                                def TEST_NODE = ''
                                if (TEST_NODES[SPEC]) {
                                    TEST_NODE = TEST_NODES[SPEC]
                                }

                                def EXTRA_GETSOURCE_OPTIONS = get_value_by_spec(EXTRA_GETSOURCE_OPTIONS_MAP, SDK_VERSION, SPEC)
                                def EXTRA_CONFIGURE_OPTIONS = get_value_by_spec(EXTRA_CONFIGURE_OPTIONS_MAP, SDK_VERSION, SPEC)
                                def EXTRA_MAKE_OPTIONS = get_value_by_spec(EXTRA_MAKE_OPTIONS_MAP, SDK_VERSION, SPEC)
                                def OPENJDK_CLONE_DIR = get_value_by_spec(OPENJDK_CLONE_DIR_MAP, SDK_VERSION, SPEC)

                                builds["${job_name}"] = {
                                    if (AUTOMATIC_GENERATION != 'false') {
                                        node(SETUP_LABEL) {
                                            unstash 'DSL'
                                            variableFile.create_job(job_name, SDK_VERSION, SPEC, 'pipeline', 'Pipeline')
                                        }
                                    }
                                    pipelinesStatus[job_name] = 'RUNNING'
                                    build(job_name, REPO, BRANCH, SHAS, OPENJ9_REPO, OPENJ9_BRANCH, OMR_REPO, OMR_BRANCH, VENDOR_CODE_REPO, VENDOR_CODE_BRANCH, SPEC, SDK_VERSION, BUILD_NODE, TEST_NODE, EXTRA_GETSOURCE_OPTIONS, EXTRA_CONFIGURE_OPTIONS, EXTRA_MAKE_OPTIONS, OPENJDK_CLONE_DIR, ADOPTOPENJDK_REPO, ADOPTOPENJDK_BRANCH, AUTOMATIC_GENERATION, CUSTOM_DESCRIPTION, ARCHIVE_JAVADOC, CODE_COVERAGE, USE_TESTENV_PROPERTIES)
                                }
                            }
                        }
                    }
                } finally {
                    // disableDeferredWipeout also requires deleteDirs. See https://issues.jenkins-ci.org/browse/JENKINS-54225
                    cleanWs notFailBuild: true, disableDeferredWipeout: true, deleteDirs: true
                }
            }

            if (ENABLE_SUMMARY_AUTO_REFRESH) {
                builds["downstream_jobs_summary"] = { refresh_summary_table() }
            }

            // launch all pipeline builds
            parallel builds

            if (PROMOTE) {
                stage('Promote') {
                    ghprbGhRepository = OPENJ9_REPO.substring(OPENJ9_REPO.indexOf('.com') +5, OPENJ9_REPO.indexOf('.git'))
                    ghprbActualCommit = SHAS['OPENJ9']
                    GITHUB_SERVER = OPENJ9_REPO.substring(OPENJ9_REPO.indexOf('github.'), OPENJ9_REPO.indexOf('.com') +4)

                    // Determine which Repo to promote. OpenJDK Acceptance should only be testing 1 SDK_VERSION

                    switch (PROMOTE) {
                        case 'OMR':
                            PROMOTE_JOB = buildFile.build_with_slack('Promote_OMR', ghprbGhRepository, ghprbActualCommit, GITHUB_SERVER,
                                                                    [string(name: 'REPO', value: OMR_REPO),
                                                                    string(name: 'TARGET_BRANCH', value: 'openj9'),
                                                                    string(name: 'COMMIT', value: "${SHAS['OMR']}")])
                            break
                        case 'OpenJDK':
                            def SDK_VERSION = ''
                            item = params.find { key, value -> ((key.indexOf('Java') != -1) && (value == true)) }
                            SDK_VERSION = item ? item.key.substring(4) : ''
                            echo "SDK_VERSION:${SDK_VERSION}"
                            // Assume Repo assigned to the first SPEC is the one we're promoting
                            def REPO = OPENJDK_REPO.get(SDK_VERSION).get(BUILD_SPECS.keySet()[0])
                            echo "REPO:${REPO}"
                            def COMMIT = SHAS.get('OPENJDK').get(SDK_VERSION).get(REPO)
                            echo "COMMIT:${COMMIT}"
                            PROMOTE_JOB = buildFile.build_with_slack('Promote_OpenJDK', ghprbGhRepository, ghprbActualCommit, GITHUB_SERVER,
                                                                    [string(name: 'REPO', value: REPO),
                                                                    string(name: 'TARGET_BRANCH', value: 'openj9'),
                                                                    string(name: 'COMMIT', value: COMMIT),
                                                                    booleanParam(name: 'TAG', value: 'false')])
                            break
                        default:
                            error("Unknown PROMOTE option:${PROMOTE}")
                            break
                    }
                }
            }
            if (params.SLACK_ON_SUCCESS && SLACK_CHANNEL) {
                slackSend channel: SLACK_CHANNEL, color: 'good', message: "Build Passed: ${JOB_NAME} #${BUILD_NUMBER} (<${BUILD_URL}|Open>)"
            }
        }
    }
} finally {
    draw_summary_table()
}

def build(JOB_NAME, OPENJDK_REPO, OPENJDK_BRANCH, SHAS, OPENJ9_REPO, OPENJ9_BRANCH, OMR_REPO, OMR_BRANCH, VENDOR_CODE_REPO, VENDOR_CODE_BRANCH, SPEC, SDK_VERSION, BUILD_NODE, TEST_NODE, EXTRA_GETSOURCE_OPTIONS, EXTRA_CONFIGURE_OPTIONS, EXTRA_MAKE_OPTIONS, OPENJDK_CLONE_DIR, ADOPTOPENJDK_REPO, ADOPTOPENJDK_BRANCH, AUTOMATIC_GENERATION, CUSTOM_DESCRIPTION, ARCHIVE_JAVADOC, CODE_COVERAGE, USE_TESTENV_PROPERTIES) {
    stage ("${JOB_NAME}") {
        JOB = build job: JOB_NAME,
                parameters: [
                    string(name: 'OPENJDK_REPO', value: OPENJDK_REPO),
                    string(name: 'OPENJDK_BRANCH', value: OPENJDK_BRANCH),
                    string(name: 'OPENJDK_SHA', value: SHAS.get('OPENJDK').get(SDK_VERSION).get(OPENJDK_REPO)),
                    string(name: 'OPENJ9_REPO', value: OPENJ9_REPO),
                    string(name: 'OPENJ9_BRANCH', value: OPENJ9_BRANCH),
                    string(name: 'OPENJ9_SHA', value: SHAS['OPENJ9']),
                    string(name: 'OMR_REPO', value: OMR_REPO),
                    string(name: 'OMR_BRANCH', value: OMR_BRANCH),
                    string(name: 'OMR_SHA', value: SHAS['OMR']),
                    string(name: 'VENDOR_CODE_REPO', value: VENDOR_CODE_REPO),
                    string(name: 'VENDOR_CODE_BRANCH', value: VENDOR_CODE_BRANCH),
                    string(name: 'VENDOR_CODE_SHA', value: SHAS['VENDOR_CODE']),
                    string(name: 'ADOPTOPENJDK_REPO', value: ADOPTOPENJDK_REPO),
                    string(name: 'ADOPTOPENJDK_BRANCH', value: ADOPTOPENJDK_BRANCH),
                    string(name: 'TESTS_TARGETS', value: TESTS_TARGETS),
                    string(name: 'VARIABLE_FILE', value: VARIABLE_FILE),
                    string(name: 'VENDOR_REPO', value: VENDOR_REPO),
                    string(name: 'VENDOR_BRANCH', value: VENDOR_BRANCH),
                    string(name: 'VENDOR_CREDENTIALS_ID', value: VENDOR_CREDENTIALS_ID),
                    string(name: 'BUILD_NODE', value: BUILD_NODE),
                    string(name: 'TEST_NODE', value: TEST_NODE),
                    string(name: 'PERSONAL_BUILD', value: PERSONAL_BUILD),
                    string(name: 'SLACK_CHANNEL', value: SLACK_CHANNEL),
                    string(name: 'RESTART_TIMEOUT', value: RESTART_TIMEOUT),
                    string(name: 'RESTART_TIMEOUT_UNITS', value: RESTART_TIMEOUT_UNITS),
                    string(name: 'SETUP_LABEL', value: SETUP_LABEL),
                    string(name: 'BUILD_IDENTIFIER', value: BUILD_IDENTIFIER),
                    string(name: 'EXTRA_GETSOURCE_OPTIONS', value: EXTRA_GETSOURCE_OPTIONS),
                    string(name: 'EXTRA_CONFIGURE_OPTIONS', value: EXTRA_CONFIGURE_OPTIONS),
                    string(name: 'EXTRA_MAKE_OPTIONS', value: EXTRA_MAKE_OPTIONS),
                    string(name: 'OPENJDK_CLONE_DIR', value: OPENJDK_CLONE_DIR),
                    string(name: 'AUTOMATIC_GENERATION', value: AUTOMATIC_GENERATION),
                    string(name: 'CUSTOM_DESCRIPTION', value: CUSTOM_DESCRIPTION),
                    string(name: 'ghprbPullId', value: ghprbPullId),
                    string(name: 'ghprbGhRepository', value: ghprbGhRepository),
                    string(name: 'ghprbCommentBody', value: ghprbCommentBody),
                    string(name: 'ghprbTargetBranch', value: ghprbTargetBranch),
                    string(name: 'ghprbActualCommit', value: ghprbActualCommit),
                    string(name: 'SCM_BRANCH', value: SCM_BRANCH),
                    string(name: 'SCM_REFSPEC', value: SCM_REFSPEC),
                    string(name: 'SCM_REPO', value: SCM_REPO),
                    booleanParam(name: 'ARCHIVE_JAVADOC', value: ARCHIVE_JAVADOC),
                    booleanParam(name: 'CODE_COVERAGE', value: CODE_COVERAGE),
                    booleanParam(name: 'USE_TESTENV_PROPERTIES', value: USE_TESTENV_PROPERTIES)]
        return JOB
    }
}

/**
 * Returns a map containing node labels per platform.
 *
 * Labels can be single machine labels - e.g. ub14hcxrt2 or boolean expressions
 * of machine's labels - e.g. sw.os.linux && hw.arch.x66 && !sw.ubuntu14.
 * Multiple platform node labels can be specified as comma separated labels per
 * platform:
 * e.g. linux_x86-64=sw.os.linux && hw.arhc.x66 && !sw.ubuntu14,aix_ppc-64_cmprssptrs=csp700003.
 * For a single platform build, the platform is optional:
 * e.g. sw.os.linux && hw.arch.x66 && !sw.ubuntu14
 */
def get_node_labels(NODE_LABELS, SPECS) {
    def LABELS = [:]

    if (!NODE_LABELS) {
        return LABELS
    }

    if ((SPECS.size() == 1) && (NODE_LABELS.indexOf("=") == -1)) {
        // single platform labels, e.g. NODE_LABELS = label1 && label2
        LABELS.put(SPECS[0], NODE_LABELS.trim())
    } else {
        // multiple platform expected labels:
        // e.g. linux_xx86-64=label1 && label2, linux_ppc-64_cmprssptrs_le=label3, win_x86=label4
        NODE_LABELS.trim().split(",").each { ITEM ->
            def ENTRY = ITEM.trim().split("=")
            if (ENTRY.size() != 2) {
                error("Invalid format for node labels: ${ITEM}! Expected value: spec1=labels1,spec2=labels2,...,specN=labelsN e.g. aix_ppc-64_cmprssptrs=csp70027,linux_x86-64=(ci.project.openj9 && hw.arch.x86 && sw.os.ubuntu.14)")
            }

            if (!SPECS.contains(ENTRY[0].trim())) {
                error("Wrong node labels platform: ${ENTRY[0]} in ${ITEM}! It does not match any of your selected platforms: ${PLATFORMS}")
            }

            LABELS.put(ENTRY[0].trim(), ENTRY[1].trim())
        }
    }

    return LABELS
}

def get_value_by_spec(map, release, spec) {
    if (map && map.containsKey(release) && map.get(release).containsKey(spec)) {
        return map.get(release).get(spec)
    }
    return ''
}

def get_pipeline_name(spec, version) {
    return "Pipeline_Build_Test_JDK${version}_${spec}"
}

/*
 * Returns an HTML summary table for the downstream builds.
 */
def get_summary_table(identifier) {
    // fetch the downstream builds of the current build
    if (!buildFile) {
        echo 'buildFile not loaded. Cannot create summary table.'
        return ''
    }
    def pipelineBuilds = buildFile.get_downstream_builds(currentBuild, currentBuild.projectName, pipelineNames)
    if (pipelineBuilds.isEmpty()) {
        return ''
    }

    def buildReleases = get_sorted_releases()

    def headerCols = ['&nbsp;']
    headerCols.addAll(buildReleases)

    // summary table
    def summaryText = "Downstream Jobs Status:<br/>"
    summaryText += "<table>"

    // table header
    summaryText += "<tr>"
    headerCols.each { it ->
        summaryText += "<th>${it}</th>"
    }
    summaryText += "</tr>"

    // table body
    summaryText += "<tbody>"

    for (spec in BUILD_SPECS.keySet().sort()) {
        if (!VARIABLES."${spec}") {
            // unsupported spec (is not defined in variables file), skip it
            continue
        }

        // table row
        summaryText += "<tr>"
        summaryText += "<td style=\"font-weight: bold;\">${spec}</td>"
        def showLabel = true

        buildReleases.each { release ->
            // table cell
            def innerTable = "<table><tbody>"
            def downstreamJobNames = buildFile.get_downstream_job_names(spec, release, identifier)
            def downstreamBuilds = [:]
            def pipelineLink = '&nbsp;'
            def pipelineDuration = '&nbsp;'

            // check if this release is supported for this spec
            if (BUILD_SPECS.get(spec).contains(release)) {
                def pipelineName = get_pipeline_name(spec, release)
                def build = null

                if (pipelineBuilds.keySet().contains(pipelineName)) {
                    build = pipelineBuilds.get(pipelineName).get(0)
                }

                if (build) {
                    pipelineLink = buildFile.get_build_embedded_status_link(build)
                    downstreamBuilds.putAll(buildFile.get_downstream_builds(build, pipelineName, downstreamJobNames.values()))
                    pipelineDuration = build.getDurationString()
                    pipelineDuration = pipelineDuration.replaceAll(" and counting", "+")

                    if (build.getResult()) {
                        // pipeline finished, cache its status
                        pipelinesStatus[pipelineName] = build.getResult()
                    }
                }
            }

            innerTable += "<tr>"
            innerTable += "<td>&nbsp;</td>"
            innerTable += "<td style=\"text-align: right;\">${pipelineLink}</td>"
            innerTable += "<td>&nbsp;</td>"
            innerTable += "<td style=\"white-space: nowrap;\">${pipelineDuration}</td>"
            innerTable += "</tr>"

            // add pipeline's downstream builds
            downstreamJobNames.each { label, jobName ->
                def downstreamBuild = null
                // downstreamJobBuilds is a list of builds in descending order
                def downstreamJobBuilds = downstreamBuilds.get(jobName)
                if (downstreamJobBuilds) {
                    // fetch the latest build
                    downstreamBuild = downstreamJobBuilds.get(0)
                    downstreamJobBuilds.remove(0)
                }

                def link = '&nbsp;'
                def duration = '&nbsp;'
                def aLabel = '&nbsp;'

                if (downstreamBuild) {
                    link = buildFile.get_build_embedded_status_link(downstreamBuild)
                    duration = downstreamBuild.getDurationString()
                    duration = duration.replaceAll(" and counting", "+")
                }

                if (showLabel) {
                    //show downstream jobs short names only once per platform
                    aLabel = label
                }

                // show restart info if there are previous builds for this job
                def restartImage = '&nbsp;'
                if (downstreamJobBuilds && downstreamJobBuilds.size() > 0) {
                    restartImage += "<img title=\"This build has been restarted ${downstreamJobBuilds.size()} time[s]!\" src=\"/static/images/24x24/refresh.png\" alt=\"Restarts\" style=\"display: inline-block;\" />"
                }

                innerTable += "<tr>"
                innerTable += "<td>${aLabel}</td>"
                innerTable += "<td style=\"text-align: right;\">${link}</td>"
                innerTable += "<td>${restartImage}</td>"
                innerTable += "<td style=\"text-align: right; white-space: nowrap;\">${duration}</td>"
                innerTable += "</tr>"
            }

            innerTable += "</tbody></table>"
            summaryText += "<td>${innerTable}</td>"

            // do not show the jobs short names for the other releases
            showLabel = false
        }
        summaryText += "</tr>"
    }

    summaryText += "</tbody>"
    summaryText += "</table>"

    return summaryText
}

/*
 * Returns the sorted build releases.
 */
def get_sorted_releases() {
    // not using comparator due to https://issues.jenkins-ci.org/browse/JENKINS-44924

    def unsortedReleases = variableFile.get_build_releases(BUILD_SPECS)
    def sortedReleases = []

    for (release in CURRENT_RELEASES) {
        if (unsortedReleases.contains(release)) {
            sortedReleases.add(release)
        }
    }

    return sortedReleases
}

def draw_summary_table() {
    def table = get_summary_table(BUILD_IDENTIFIER)

    if (table) {
        if (ENABLE_SUMMARY_AUTO_REFRESH) {
            def actions = manager.build.actions
            for (int i = 0; i < actions.size(); i++) {
                def action = actions.get(i)
                if (action.metaClass && action.metaClass.hasProperty(action, "text") && action.text.contains("Downstream Jobs Status")) {
                    actions.remove(action)
                }
            }
        }

        manager.createSummary('plugin.png').appendText(table)
    }
}

def refresh_summary_table() {
    while (pipelinesStatus.values().isEmpty() || pipelinesStatus.values().contains('RUNNING')) {
        sleep(time: SUMMARY_AUTO_REFRESH_TIME.toInteger(), unit: 'MINUTES')
        draw_summary_table()
    }
}
