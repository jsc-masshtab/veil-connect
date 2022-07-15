library "veil-connect-libraries@$BRANCH"

def currentDate = new Date().format('yyyyMMddHHmmss')
def branch = buildParameters.branch()
def version = buildParameters.version()
def agents = buildParameters.linuxAgents()

notifyBuild("STARTED")

pipeline {
    agent {
        label "${AGENT}"
    }

    environment {
        PRJNAME = "veil-connect"
        DATE = "$currentDate"
        NFS_DIR = "/nfs/veil-connect"
        NEXUS_DOCKER_REGISTRY = "nexus.bazalt.team"
        NEXUS_CREDS = credentials('nexus-jenkins-creds')
        DOCKER_IMAGE_NAME = "${NEXUS_DOCKER_REGISTRY}/veil-connect-builder"
    }

    post {
        always {
            script {
                notifyBuild(currentBuild.result)
            }
        }
    }

    options {
        skipDefaultCheckout(true)
        buildDiscarder(logRotator(daysToKeepStr: '60', numToKeepStr: '100'))
        gitLabConnection('gitlab')
        timestamps()
        ansiColor('xterm')
        parallelsAlwaysFailFast()
    }

    parameters {
        string(      name: 'BRANCH',      defaultValue: branch,     description: 'branch')
        string(      name: 'VERSION',     defaultValue: version,    description: 'version')
        choice(      name: 'AGENT',       choices: agents,          description: 'jenkins build agent')
        booleanParam(name: 'STRETCH',     defaultValue: true,       description: 'create DEB?')
        booleanParam(name: 'BUSTER',      defaultValue: true,       description: 'create DEB?')
        booleanParam(name: 'BULLSEYE',    defaultValue: true,       description: 'create DEB?')
        booleanParam(name: 'BIONIC',      defaultValue: true,       description: 'create DEB?')
        booleanParam(name: 'FOCAL',       defaultValue: true,       description: 'create DEB?')
        booleanParam(name: 'JAMMY',       defaultValue: true,       description: 'create DEB?')
        booleanParam(name: 'EL7',         defaultValue: true,       description: 'create RPM?')
        booleanParam(name: 'EL8',         defaultValue: true,       description: 'create RPM?')
        booleanParam(name: 'RED73',       defaultValue: true,       description: 'create RPM?')
        booleanParam(name: 'ALT9',        defaultValue: true,       description: 'create RPM?')
        booleanParam(name: 'EMBEDDED',    defaultValue: true,       description: 'create DEB?')
    }

    stages {
        stage ('checkout') {
            steps {
                script {
                    buildSteps.gitCheckout("$BRANCH")
                }
            }
        }

        stage('prepare build images') {
            parallel {
                stage ('stretch. docker build') {
                    when {
                        beforeAgent true
                        expression { params.STRETCH == true }
                    }
                    environment {
                      DISTR = "stretch"
                    }
                    steps {
                        script {
                            buildSteps.prepareBuildImage()
                        }
                    }
                }

                stage ('buster. docker build') {
                    when {
                        beforeAgent true
                        expression { params.BUSTER == true }
                    }
                    environment {
                      DISTR = "buster"
                    }
                    steps {
                        script {
                            buildSteps.prepareBuildImage()
                        }
                    }
                }

                stage ('bullseye. docker build') {
                    when {
                        beforeAgent true
                        expression { params.BULLSEYE == true }
                    }
                    environment {
                      DISTR = "bullseye"
                    }
                    steps {
                        script {
                            buildSteps.prepareBuildImage()
                        }
                    }
                }

                stage ('bionic. docker build') {
                    when {
                        beforeAgent true
                        expression { params.BIONIC == true }
                    }
                    environment {
                      DISTR = "bionic"
                    }
                    steps {
                        script {
                            buildSteps.prepareBuildImage()
                        }
                    }
                }

                stage ('focal. docker build') {
                    when {
                        beforeAgent true
                        expression { params.FOCAL == true }
                    }
                    environment {
                      DISTR = "focal"
                    }
                    steps {
                        script {
                            buildSteps.prepareBuildImage()
                        }
                    }
                }

                stage ('jammy. docker build') {
                    when {
                        beforeAgent true
                        expression { params.JAMMY == true }
                    }
                    environment {
                      DISTR = "jammy"
                    }
                    steps {
                        script {
                            buildSteps.prepareBuildImage()
                        }
                    }
                }

                stage ('el7. docker build') {
                    when {
                        beforeAgent true
                        expression { params.EL7 == true }
                    }
                    environment {
                      DISTR = "el7"
                    }
                    steps {
                        script {
                            buildSteps.prepareBuildImage()
                        }
                    }
                }

                stage ('el8. docker build') {
                    when {
                        beforeAgent true
                        expression { params.EL8 == true }
                    }
                    environment {
                      DISTR = "el8"
                    }
                    steps {
                        script {
                            buildSteps.prepareBuildImage()
                        }
                    }
                }

                stage ('alt9. docker build') {
                    when {
                        beforeAgent true
                        expression { params.ALT9 == true }
                    }
                    environment {
                      DISTR = "alt9"
                    }
                    steps {
                        script {
                            buildSteps.prepareBuildImage()
                        }
                    }
                }

                stage ('red7-3. docker build') {
                    when {
                        beforeAgent true
                        expression { params.RED73 == true }
                    }
                    environment {
                      DISTR = "redos7.3"
                    }
                    steps {
                        script {
                            buildSteps.prepareBuildImage()
                        }
                    }
                }
            }
        }

        stage ('build') {
            parallel {
                stage ('stretch. build') {
                    when {
                        beforeAgent true
                        expression { params.STRETCH == true }
                    }
                    environment {
                        DISTR = "stretch"
                    }
                    agent {
                        docker {
                            image "${DOCKER_IMAGE_NAME}-stretch:${VERSION}"
                            args '-u root:root'
                            reuseNode true
                            label "${AGENT}"
                        }
                    }
                    steps {
                        script {
                            buildSteps.buildDebPackage()
                        }
                    }
                }

                stage ('buster. build') {
                    when {
                        beforeAgent true
                        expression { params.BUSTER == true }
                    }
                    environment {
                        DISTR = "buster"
                    }
                    agent {
                        docker {
                            image "${DOCKER_IMAGE_NAME}-buster:${VERSION}"
                            args '-u root:root'
                            reuseNode true
                            label "${AGENT}"
                        }
                    }
                    steps {
                        script {
                            buildSteps.buildDebPackage()
                        }
                    }
                }

                stage ('bullseye. build') {
                    when {
                        beforeAgent true
                        expression { params.BULLSEYE == true }
                    }
                    environment {
                        DISTR = "bullseye"
                    }
                    agent {
                        docker {
                            image "${DOCKER_IMAGE_NAME}-bullseye:${VERSION}"
                            args '-u root:root'
                            reuseNode true
                            label "${AGENT}"
                        }
                    }
                    steps {
                        script {
                            buildSteps.buildDebPackage()
                        }
                    }
                }

                stage ('bionic. build') {
                    when {
                        beforeAgent true
                        expression { params.BIONIC == true || params.EMBEDDED == true }
                    }
                    environment {
                        DISTR = "bionic"
                    }
                    agent {
                        docker {
                            image "${DOCKER_IMAGE_NAME}-bionic:${VERSION}"
                            args '-u root:root'
                            reuseNode true
                            label "${AGENT}"
                        }
                    }
                    steps {
                        script {
                            buildSteps.buildDebPackage()
                        }
                    }
                }

                stage ('focal. build') {
                    when {
                        beforeAgent true
                        expression { params.FOCAL == true }
                    }
                    environment {
                        DISTR = "focal"
                    }
                    agent {
                        docker {
                            image "${DOCKER_IMAGE_NAME}-focal:${VERSION}"
                            args '-u root:root'
                            reuseNode true
                            label "${AGENT}"
                        }
                    }
                    steps {
                        script {
                            buildSteps.buildDebPackage()
                        }
                    }
                }

                stage ('jammy. build') {
                    when {
                        beforeAgent true
                        expression { params.JAMMY == true }
                    }
                    environment {
                        DISTR = "jammy"
                    }
                    agent {
                        docker {
                            image "${DOCKER_IMAGE_NAME}-jammy:${VERSION}"
                            args '-u root:root'
                            reuseNode true
                            label "${AGENT}"
                        }
                    }
                    steps {
                        script {
                            buildSteps.buildDebPackage()
                        }
                    }
                }

                stage ('el7. build') {
                    when {
                        beforeAgent true
                        expression { params.EL7 == true }
                    }
                    environment {
                        DISTR = "el7"
                    }
                    agent {
                        docker {
                            image "${DOCKER_IMAGE_NAME}-el7:${VERSION}"
                            args '-u root:root'
                            reuseNode true
                            label "${AGENT}"
                        }
                    }
                    steps {
                        script {
                            buildSteps.buildRpmPackage()
                        }
                    }
                }

                stage ('el8. build') {
                    when {
                        beforeAgent true
                        expression { params.EL8 == true }
                    }
                    environment {
                        DISTR = "el8"
                    }
                    agent {
                        docker {
                            image "${DOCKER_IMAGE_NAME}-el8:${VERSION}"
                            args '-u root:root'
                            reuseNode true
                            label "${AGENT}"
                        }
                    }
                    steps {
                        script {
                            buildSteps.buildRpmPackage()
                        }
                    }
                }

                stage ('alt9. build') {
                    when {
                        beforeAgent true
                        expression { params.ALT9 == true }
                    }
                    environment {
                        DISTR = "alt9"
                    }
                    agent {
                        docker {
                            image "${DOCKER_IMAGE_NAME}-alt9:${VERSION}"
                            reuseNode true
                            label "${AGENT}"
                        }
                    }
                    steps {
                        script {
                            buildSteps.buildAltRpmPackage()
                        }
                    }
                }

                stage ('red7-3. build') {
                    when {
                        beforeAgent true
                        expression { params.RED73 == true }
                    }
                    environment {
                        DISTR = "redos7.3"
                    }
                    agent {
                        docker {
                            image "${DOCKER_IMAGE_NAME}-redos7.3:${VERSION}"
                            args '-u root:root'
                            reuseNode true
                            label "${AGENT}"
                        }
                    }
                    steps {
                        script {
                            buildSteps.buildRpmPackage()
                        }
                    }
                }
            }
        }

        stage ('embedded. make installer') {
            when {
                beforeAgent true
                expression { params.EMBEDDED == true }
            }
            environment {
                DISTR = "bionic"
            }
            agent {
                docker {
                    image "${DOCKER_IMAGE_NAME}-bionic:${VERSION}"
                    args '-u root:root'
                    reuseNode true
                    label "${AGENT}"
                }
            }
            steps {
                sh script: '''
                    # make installer
                    mkdir -p ${WORKSPACE}/devops/deb_embedded/root/opt/veil-connect
                    mkdir -p ${WORKSPACE}/devops/deb_embedded/root/usr/share/applications
                    cp -r ${WORKSPACE}/build-${DISTR}/* ${WORKSPACE}/doc/veil-connect.ico ${WORKSPACE}/devops/deb_embedded/root/opt/veil-connect
                    cp ${WORKSPACE}/doc/veil-connect.desktop ${WORKSPACE}/devops/deb_embedded/root/usr/share/applications
                    sed -i -e "s:%%VER%%:${VERSION}~orel:g" ${WORKSPACE}/devops/deb_embedded/root/DEBIAN/control
                    chmod -R 777 ${WORKSPACE}/devops/deb_embedded/root
                    chmod -R 755 ${WORKSPACE}/devops/deb_embedded/root/DEBIAN
                    chown -R root:root ${WORKSPACE}/devops/deb_embedded/root
                    cd ${WORKSPACE}/devops/deb_embedded
                    dpkg-deb -b root .
                '''
            }
        }

        stage ('deploy to repo') {
            parallel {
                stage ('stretch. deploy to repo') {
                    when {
                        beforeAgent true
                        expression { params.STRETCH == true }
                    }
                    environment {
                        DISTR = "stretch"
                    }
                    steps {
                        script {
                            buildSteps.deployToAptly()
                        }
                    }
                }

                stage ('buster. deploy to repo') {
                    when {
                        beforeAgent true
                        expression { params.BUSTER == true }
                    }
                    environment {
                        DISTR = "buster"
                    }
                    steps {
                        script {
                            buildSteps.deployToAptly()
                        }
                    }
                }

                stage ('bullseye. deploy to repo') {
                    when {
                        beforeAgent true
                        expression { params.BULLSEYE == true }
                    }
                    environment {
                        DISTR = "bullseye"
                    }
                    steps {
                        script {
                            buildSteps.deployToAptly()
                        }
                    }
                }

                stage ('bionic. deploy to repo') {
                    when {
                        beforeAgent true
                        expression { params.BIONIC == true }
                    }
                    environment {
                        DISTR = "bionic"
                    }
                    steps {
                        script {
                            buildSteps.deployToAptly()
                        }
                    }
                }

                stage ('focal. deploy to repo') {
                    when {
                        beforeAgent true
                        expression { params.FOCAL == true }
                    }
                    environment {
                        DISTR = "focal"
                    }
                    steps {
                        script {
                            buildSteps.deployToAptly()
                        }
                    }
                }

                stage ('jammy. deploy to repo') {
                    when {
                        beforeAgent true
                        expression { params.JAMMY == true }
                    }
                    environment {
                        DISTR = "jammy"
                    }
                    steps {
                        script {
                            buildSteps.deployToAptly()
                        }
                    }
                }

                stage ('el7. deploy to repo') {
                    when {
                        beforeAgent true
                        expression { params.EL7 == true }
                    }
                    environment {
                        DISTR = "el7"
                    }
                    steps {
                        script {
                            buildSteps.deployToRpmRepo()
                        }
                    }
                }

                stage ('el8. deploy to repo') {
                    when {
                        beforeAgent true
                        expression { params.EL8 == true }
                    }
                    environment {
                        DISTR = "el8"
                    }
                    steps {
                        script {
                            buildSteps.deployToRpmRepo()
                        }
                    }
                }

                stage ('alt9. deploy to repo') {
                    when {
                        beforeAgent true
                        expression { params.ALT9 == true }
                    }
                    environment {
                        DISTR = "alt9"
                        ARCH = "x86_64"
                    }
                    steps {
                        script {
                            buildSteps.deployToAptRpmRepo()
                        }
                    }
                }

                stage ('red7-3. deploy to repo') {
                    when {
                        beforeAgent true
                        expression { params.RED73 == true }
                    }
                    environment {
                        DISTR = "redos7.3"
                    }
                    steps {
                        script {
                            buildSteps.deployToRpmRepo()
                        }
                    }
                }
            }
        }

        stage ('embedded. deploy to repo') {
            when {
                beforeAgent true
                expression { params.EMBEDDED == true }
            }
            steps {
                withCredentials([sshUserPrivateKey(credentialsId: 'uploader_ssh_key.id_rsa', keyFileVariable: 'SSH_KEY')]) {
                    sh script: '''
                        ssh -o StrictHostKeyChecking=no -i $SSH_KEY uploader@192.168.10.144 mkdir -p /local_storage/veil-connect-embedded/${VERSION}
                        scp -o StrictHostKeyChecking=no -i $SSH_KEY ${WORKSPACE}/devops/deb_embedded/*.deb uploader@192.168.10.144:/local_storage/veil-connect-embedded/${VERSION}/
                        ssh -o StrictHostKeyChecking=no -i $SSH_KEY uploader@192.168.10.144 "cd /local_storage/veil-connect-embedded/; ln -sfT ${VERSION} latest"
                    '''
                }
            }
        }

        stage ('deploy universal linux installer') {
            steps {
                withCredentials([sshUserPrivateKey(credentialsId: 'uploader_ssh_key.id_rsa', keyFileVariable: 'SSH_KEY')]) {
                    sh script: '''
                        scp -o StrictHostKeyChecking=no -i $SSH_KEY ${WORKSPACE}/devops/veil-connect-linux-installer.sh uploader@192.168.10.144:/local_storage/veil-connect/linux/
                    '''
                }
            }
        }
    }
}