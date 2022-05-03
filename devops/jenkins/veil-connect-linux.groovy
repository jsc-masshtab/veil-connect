def currentDate = new Date().format('yyyyMMddHHmmss')
def rocketNotify = true

pipeline {
    agent {
        label "${AGENT}"
    }

    environment {
        APT_SRV = "192.168.11.118"
        PRJNAME = "veil-connect"
        DATE = "$currentDate"
        NFS_DIR = "/nfs/veil-connect"
        NEXUS_DOCKER_REGISTRY = "nexus.bazalt.team"
        NEXUS_CREDS = credentials('nexus-jenkins-creds')
        DOCKER_IMAGE_NAME = "${NEXUS_DOCKER_REGISTRY}/veil-connect-builder"
    }

    post {
        failure {
            println "Something goes wrong"
            println "Current build marked as ${currentBuild.result}"
            notifyBuild(rocketNotify,":x: FAILED")
        }

        aborted {
            println "Build was interrupted manually"
            println "Current build marked as ${currentBuild.result}"
            notifyBuild(rocketNotify,":x: FAILED")
        }

        success {
            notifyBuild(rocketNotify, ":white_check_mark: SUCCESSFUL")
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
        string(      name: 'BRANCH',    defaultValue: 'master',                    description: 'branch')
        string(      name: 'VERSION',   defaultValue: '1.11.3',                    description: 'version')
        choice(      name: 'AGENT',     choices: ['cloud-ubuntu-20', 'bld-agent'], description: 'jenkins build agent')
        booleanParam(name: 'STRETCH',   defaultValue: true,                        description: 'create DEB?')
        booleanParam(name: 'BUSTER',    defaultValue: true,                        description: 'create DEB?')
        booleanParam(name: 'BIONIC',    defaultValue: true,                        description: 'create DEB?')
        booleanParam(name: 'FOCAL',     defaultValue: true,                        description: 'create DEB?')
        booleanParam(name: 'EL7',       defaultValue: true,                        description: 'create RPM?')
        booleanParam(name: 'EL8',       defaultValue: true,                        description: 'create RPM?')
        booleanParam(name: 'RED73',     defaultValue: true,                        description: 'create RPM?')
        booleanParam(name: 'ALT9',      defaultValue: true,                        description: 'create RPM?')
        booleanParam(name: 'EMBEDDED',  defaultValue: true,                        description: 'create DEB?')
    }

    stages {
        stage ('checkout') {
            steps {
                notifyBuild(rocketNotify, ":bell: STARTED")
                cleanWs()
                checkout([ $class: 'GitSCM',
                    branches: [[name: '$BRANCH']],
                    doGenerateSubmoduleConfigurations: false,
                    extensions: [], submoduleCfg: [],
                    userRemoteConfigs: [[credentialsId: 'jenkins-veil-connect-token',
                    url: 'http://gitlab.bazalt.team/vdi/veil-connect.git']]
                ])
            }
        }

        stage('prepare build images') {
            parallel {
                stage ('stretch. docker build') {
                    when {
                        beforeAgent true
                        expression { params.STRETCH == true }
                    }
                    steps {
                        sh script: '''
                            DISTR="stretch"
                            echo -n $NEXUS_CREDS_PSW | docker login -u $NEXUS_CREDS_USR --password-stdin $NEXUS_DOCKER_REGISTRY
                            docker pull $DOCKER_IMAGE_NAME-$DISTR:latest || true
                            docker build -f devops/docker/Dockerfile.$DISTR . --pull --cache-from $DOCKER_IMAGE_NAME-$DISTR:latest --tag $DOCKER_IMAGE_NAME-$DISTR:$VERSION
                            docker push $DOCKER_IMAGE_NAME-$DISTR:$VERSION
                            docker tag $DOCKER_IMAGE_NAME-$DISTR:$VERSION $DOCKER_IMAGE_NAME-$DISTR:latest
                            docker push $DOCKER_IMAGE_NAME-$DISTR:latest
                        '''
                    }
                }

                stage ('buster. docker build') {
                    when {
                        beforeAgent true
                        expression { params.BUSTER == true }
                    }
                    steps {
                        sh script: '''
                            DISTR="buster"
                            echo -n $NEXUS_CREDS_PSW | docker login -u $NEXUS_CREDS_USR --password-stdin $NEXUS_DOCKER_REGISTRY
                            docker pull $DOCKER_IMAGE_NAME-$DISTR:latest || true
                            docker build -f devops/docker/Dockerfile.$DISTR . --pull --cache-from $DOCKER_IMAGE_NAME-$DISTR:latest --tag $DOCKER_IMAGE_NAME-$DISTR:$VERSION
                            docker push $DOCKER_IMAGE_NAME-$DISTR:$VERSION
                            docker tag $DOCKER_IMAGE_NAME-$DISTR:$VERSION $DOCKER_IMAGE_NAME-$DISTR:latest
                            docker push $DOCKER_IMAGE_NAME-$DISTR:latest
                        '''
                    }
                }

                stage ('bionic. docker build') {
                    when {
                        beforeAgent true
                        expression { params.BIONIC == true }
                    }
                    steps {
                        sh script: '''
                            DISTR="bionic"
                            echo -n $NEXUS_CREDS_PSW | docker login -u $NEXUS_CREDS_USR --password-stdin $NEXUS_DOCKER_REGISTRY
                            docker pull $DOCKER_IMAGE_NAME-$DISTR:latest || true
                            docker build -f devops/docker/Dockerfile.$DISTR . --pull --cache-from $DOCKER_IMAGE_NAME-$DISTR:latest --tag $DOCKER_IMAGE_NAME-$DISTR:$VERSION
                            docker push $DOCKER_IMAGE_NAME-$DISTR:$VERSION
                            docker tag $DOCKER_IMAGE_NAME-$DISTR:$VERSION $DOCKER_IMAGE_NAME-$DISTR:latest
                            docker push $DOCKER_IMAGE_NAME-$DISTR:latest
                        '''
                    }
                }

                stage ('focal. docker build') {
                    when {
                        beforeAgent true
                        expression { params.FOCAL == true }
                    }
                    steps {
                        sh script: '''
                            DISTR="focal"
                            echo -n $NEXUS_CREDS_PSW | docker login -u $NEXUS_CREDS_USR --password-stdin $NEXUS_DOCKER_REGISTRY
                            docker pull $DOCKER_IMAGE_NAME-$DISTR:latest || true
                            docker build -f devops/docker/Dockerfile.$DISTR . --pull --cache-from $DOCKER_IMAGE_NAME-$DISTR:latest --tag $DOCKER_IMAGE_NAME-$DISTR:$VERSION
                            docker push $DOCKER_IMAGE_NAME-$DISTR:$VERSION
                            docker tag $DOCKER_IMAGE_NAME-$DISTR:$VERSION $DOCKER_IMAGE_NAME-$DISTR:latest
                            docker push $DOCKER_IMAGE_NAME-$DISTR:latest
                        '''
                    }
                }

                stage ('el7. docker build') {
                    when {
                        beforeAgent true
                        expression { params.EL7 == true }
                    }
                    steps {
                        sh script: '''
                            DISTR="el7"
                            echo -n $NEXUS_CREDS_PSW | docker login -u $NEXUS_CREDS_USR --password-stdin $NEXUS_DOCKER_REGISTRY
                            docker pull $DOCKER_IMAGE_NAME-$DISTR:latest || true
                            docker build -f devops/docker/Dockerfile.$DISTR . --pull --cache-from $DOCKER_IMAGE_NAME-$DISTR:latest --tag $DOCKER_IMAGE_NAME-$DISTR:$VERSION
                            docker push $DOCKER_IMAGE_NAME-$DISTR:$VERSION
                            docker tag $DOCKER_IMAGE_NAME-$DISTR:$VERSION $DOCKER_IMAGE_NAME-$DISTR:latest
                            docker push $DOCKER_IMAGE_NAME-$DISTR:latest
                        '''
                    }
                }

                stage ('el8. docker build') {
                    when {
                        beforeAgent true
                        expression { params.EL8 == true }
                    }
                    steps {
                        sh script: '''
                            DISTR="el8"
                            echo -n $NEXUS_CREDS_PSW | docker login -u $NEXUS_CREDS_USR --password-stdin $NEXUS_DOCKER_REGISTRY
                            docker pull $DOCKER_IMAGE_NAME-$DISTR:latest || true
                            docker build -f devops/docker/Dockerfile.$DISTR . --pull --cache-from $DOCKER_IMAGE_NAME-$DISTR:latest --tag $DOCKER_IMAGE_NAME-$DISTR:$VERSION
                            docker push $DOCKER_IMAGE_NAME-$DISTR:$VERSION
                            docker tag $DOCKER_IMAGE_NAME-$DISTR:$VERSION $DOCKER_IMAGE_NAME-$DISTR:latest
                            docker push $DOCKER_IMAGE_NAME-$DISTR:latest
                        '''
                    }
                }

                stage ('alt9. docker build') {
                    when {
                        beforeAgent true
                        expression { params.ALT9 == true }
                    }
                    steps {
                        sh script: '''
                            DISTR="alt9"
                            echo -n $NEXUS_CREDS_PSW | docker login -u $NEXUS_CREDS_USR --password-stdin $NEXUS_DOCKER_REGISTRY
                            docker pull $DOCKER_IMAGE_NAME-$DISTR:latest || true
                            docker build -f devops/docker/Dockerfile.$DISTR . --pull --cache-from $DOCKER_IMAGE_NAME-$DISTR:latest --tag $DOCKER_IMAGE_NAME-$DISTR:$VERSION
                            docker push $DOCKER_IMAGE_NAME-$DISTR:$VERSION
                            docker tag $DOCKER_IMAGE_NAME-$DISTR:$VERSION $DOCKER_IMAGE_NAME-$DISTR:latest
                            docker push $DOCKER_IMAGE_NAME-$DISTR:latest
                        '''
                    }
                }

                stage ('red7-3. docker build') {
                    when {
                        beforeAgent true
                        expression { params.RED73 == true }
                    }
                    steps {
                        sh script: '''
                            DISTR="redos7.3"
                            echo -n $NEXUS_CREDS_PSW | docker login -u $NEXUS_CREDS_USR --password-stdin $NEXUS_DOCKER_REGISTRY
                            docker pull $DOCKER_IMAGE_NAME-$DISTR:latest || true
                            docker build -f devops/docker/Dockerfile.$DISTR . --pull --cache-from $DOCKER_IMAGE_NAME-$DISTR:latest --tag $DOCKER_IMAGE_NAME-$DISTR:$VERSION
                            docker push $DOCKER_IMAGE_NAME-$DISTR:$VERSION
                            docker tag $DOCKER_IMAGE_NAME-$DISTR:$VERSION $DOCKER_IMAGE_NAME-$DISTR:latest
                            docker push $DOCKER_IMAGE_NAME-$DISTR:latest
                        '''
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
                        sh script: '''
                            mkdir build-${DISTR}
                            cd build-${DISTR}
                            cmake -DCMAKE_BUILD_TYPE=Release ../
                            make
                            rm -rf CMakeCache.txt  CMakeFiles  Makefile  cmake_install.cmake

                            # make installer
                            cp -r ${WORKSPACE}/devops/deb ${WORKSPACE}/devops/deb-${DISTR}
                            mkdir -p ${WORKSPACE}/devops/deb-${DISTR}/root/opt/veil-connect
                            mkdir -p ${WORKSPACE}/devops/deb-${DISTR}/root/usr/share/applications
                            cp -r ${WORKSPACE}/build-${DISTR}/* ${WORKSPACE}/doc/veil-connect.ico ${WORKSPACE}/devops/deb-${DISTR}/root/opt/veil-connect
                            cp ${WORKSPACE}/doc/veil-connect.desktop ${WORKSPACE}/devops/deb-${DISTR}/root/usr/share/applications
                            sed -i -e "s:%%VER%%:${VERSION}~${DISTR}:g" ${WORKSPACE}/devops/deb-${DISTR}/root/DEBIAN/control
                            chmod -R 777 ${WORKSPACE}/devops/deb-${DISTR}/root
                            chmod -R 755 ${WORKSPACE}/devops/deb-${DISTR}/root/DEBIAN
                            chown -R root:root ${WORKSPACE}/devops/deb-${DISTR}/root
                            cd ${WORKSPACE}/devops/deb-${DISTR}
                            dpkg-deb -b root .
                        '''
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
                        sh script: '''
                            mkdir build-${DISTR}
                            cd build-${DISTR}
                            cmake -DCMAKE_BUILD_TYPE=Release ../
                            make
                            rm -rf CMakeCache.txt  CMakeFiles  Makefile  cmake_install.cmake

                            # make installer
                            cp -r ${WORKSPACE}/devops/deb ${WORKSPACE}/devops/deb-${DISTR}
                            mkdir -p ${WORKSPACE}/devops/deb-${DISTR}/root/opt/veil-connect
                            mkdir -p ${WORKSPACE}/devops/deb-${DISTR}/root/usr/share/applications
                            cp -r ${WORKSPACE}/build-${DISTR}/* ${WORKSPACE}/doc/veil-connect.ico ${WORKSPACE}/devops/deb-${DISTR}/root/opt/veil-connect
                            cp ${WORKSPACE}/doc/veil-connect.desktop ${WORKSPACE}/devops/deb-${DISTR}/root/usr/share/applications
                            sed -i -e "s:%%VER%%:${VERSION}~${DISTR}:g" ${WORKSPACE}/devops/deb-${DISTR}/root/DEBIAN/control
                            chmod -R 777 ${WORKSPACE}/devops/deb-${DISTR}/root
                            chmod -R 755 ${WORKSPACE}/devops/deb-${DISTR}/root/DEBIAN
                            chown -R root:root ${WORKSPACE}/devops/deb-${DISTR}/root
                            cd ${WORKSPACE}/devops/deb-${DISTR}
                            dpkg-deb -b root .
                        '''
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
                        sh script: '''
                            mkdir build-${DISTR}
                            cd build-${DISTR}
                            cmake -DCMAKE_BUILD_TYPE=Release ../
                            make
                            rm -rf CMakeCache.txt  CMakeFiles  Makefile  cmake_install.cmake

                            # make installer
                            cp -r ${WORKSPACE}/devops/deb ${WORKSPACE}/devops/deb-${DISTR}
                            mkdir -p ${WORKSPACE}/devops/deb-${DISTR}/root/opt/veil-connect
                            mkdir -p ${WORKSPACE}/devops/deb-${DISTR}/root/usr/share/applications
                            cp -r ${WORKSPACE}/build-${DISTR}/* ${WORKSPACE}/doc/veil-connect.ico ${WORKSPACE}/devops/deb-${DISTR}/root/opt/veil-connect
                            cp ${WORKSPACE}/doc/veil-connect.desktop ${WORKSPACE}/devops/deb-${DISTR}/root/usr/share/applications
                            sed -i -e "s:%%VER%%:${VERSION}~${DISTR}:g" ${WORKSPACE}/devops/deb-${DISTR}/root/DEBIAN/control
                            chmod -R 777 ${WORKSPACE}/devops/deb-${DISTR}/root
                            chmod -R 755 ${WORKSPACE}/devops/deb-${DISTR}/root/DEBIAN
                            chown -R root:root ${WORKSPACE}/devops/deb-${DISTR}/root
                            cd ${WORKSPACE}/devops/deb-${DISTR}
                            dpkg-deb -b root .
                        '''
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
                        sh script: '''
                            mkdir build-${DISTR}
                            cd build-${DISTR}
                            cmake -DCMAKE_BUILD_TYPE=Release ../
                            make
                            rm -rf CMakeCache.txt  CMakeFiles  Makefile  cmake_install.cmake

                            # make installer
                            cp -r ${WORKSPACE}/devops/deb ${WORKSPACE}/devops/deb-${DISTR}
                            mkdir -p ${WORKSPACE}/devops/deb-${DISTR}/root/opt/veil-connect
                            mkdir -p ${WORKSPACE}/devops/deb-${DISTR}/root/usr/share/applications
                            cp -r ${WORKSPACE}/build-${DISTR}/* ${WORKSPACE}/doc/veil-connect.ico ${WORKSPACE}/devops/deb-${DISTR}/root/opt/veil-connect
                            cp ${WORKSPACE}/doc/veil-connect.desktop ${WORKSPACE}/devops/deb-${DISTR}/root/usr/share/applications
                            sed -i -e "s:%%VER%%:${VERSION}~${DISTR}:g" ${WORKSPACE}/devops/deb-${DISTR}/root/DEBIAN/control
                            chmod -R 777 ${WORKSPACE}/devops/deb-${DISTR}/root
                            chmod -R 755 ${WORKSPACE}/devops/deb-${DISTR}/root/DEBIAN
                            chown -R root:root ${WORKSPACE}/devops/deb-${DISTR}/root
                            cd ${WORKSPACE}/devops/deb-${DISTR}
                            dpkg-deb -b root .
                        '''
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
                        sh script: '''
                            mkdir build-${DISTR}
                            cd build-${DISTR}
                            cmake3 -DCMAKE_BUILD_TYPE=Release ../
                            make
                            rm -rf CMakeCache.txt  CMakeFiles  Makefile  cmake_install.cmake
                            cd ${WORKSPACE}

                            # make installer
                            mkdir -p rpmbuild-${DISTR}/{BUILD,BUILDROOT,RPMS,SOURCES,SPECS,SRPMS}
                            cp devops/rpm/veil-connect.spec rpmbuild-${DISTR}/SPECS
                            sed -i -e "s:%%VER%%:${VERSION}:g" rpmbuild-${DISTR}/SPECS/veil-connect.spec
                            mkdir -p rpmbuild-${DISTR}/BUILD/opt/veil-connect
                            mkdir -p rpmbuild-${DISTR}/BUILD/usr/share/applications
                            cp -r build-${DISTR}/* doc/veil-connect.ico rpmbuild-${DISTR}/BUILD/opt/veil-connect
                            cp doc/veil-connect.desktop rpmbuild-${DISTR}/BUILD/usr/share/applications
                            cd rpmbuild-${DISTR}
                            rpmbuild --define "_topdir `pwd`" -v -bb SPECS/veil-connect.spec
                        '''
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
                        sh script: '''
                            mkdir build-${DISTR}
                            cd build-${DISTR}
                            cmake3 -DCMAKE_BUILD_TYPE=Release ../
                            make
                            rm -rf CMakeCache.txt  CMakeFiles  Makefile  cmake_install.cmake
                            cd ${WORKSPACE}

                            # make installer
                            mkdir -p rpmbuild-${DISTR}/{BUILD,BUILDROOT,RPMS,SOURCES,SPECS,SRPMS}
                            cp devops/rpm/veil-connect.spec rpmbuild-${DISTR}/SPECS
                            sed -i -e "s:%%VER%%:${VERSION}:g" rpmbuild-${DISTR}/SPECS/veil-connect.spec
                            mkdir -p rpmbuild-${DISTR}/BUILD/opt/veil-connect
                            mkdir -p rpmbuild-${DISTR}/BUILD/usr/share/applications
                            cp -r build-${DISTR}/* doc/veil-connect.ico rpmbuild-${DISTR}/BUILD/opt/veil-connect
                            cp doc/veil-connect.desktop rpmbuild-${DISTR}/BUILD/usr/share/applications
                            cd rpmbuild-${DISTR}
                            rpmbuild --define "_topdir `pwd`" -v -bb SPECS/veil-connect.spec
                        '''
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
                        sh script: '''
                            mkdir build-${DISTR}
                            cd build-${DISTR}
                            cmake -DCMAKE_BUILD_TYPE=Release ../
                            make
                            rm -rf CMakeCache.txt  CMakeFiles  Makefile  cmake_install.cmake
                            cd ${WORKSPACE}

                            # make installer
                            mkdir -p rpmbuild-${DISTR}/{BUILD,BUILDROOT,RPMS,SOURCES,SPECS,SRPMS}
                            cp devops/rpm/veil-connect-alt.spec rpmbuild-${DISTR}/SPECS
                            sed -i -e "s:%%VER%%:${VERSION}:g" rpmbuild-${DISTR}/SPECS/veil-connect-alt.spec
                            mkdir -p rpmbuild-${DISTR}/BUILD/opt/veil-connect
                            mkdir -p rpmbuild-${DISTR}/BUILD/usr/share/applications
                            cp -r build-${DISTR}/* doc/veil-connect.ico rpmbuild-${DISTR}/BUILD/opt/veil-connect
                            cp doc/veil-connect.desktop rpmbuild-${DISTR}/BUILD/usr/share/applications
                            cd rpmbuild-${DISTR}
                            rpmbuild --define "_topdir `pwd`" --define "_tmppath %{_topdir}/tmp" -v -bb SPECS/veil-connect-alt.spec
                        '''
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
                        sh script: '''
                            mkdir build-${DISTR}
                            cd build-${DISTR}
                            cmake3 -DCMAKE_BUILD_TYPE=Release ../
                            make
                            rm -rf CMakeCache.txt  CMakeFiles  Makefile  cmake_install.cmake
                            cd ${WORKSPACE}

                            # make installer
                            mkdir -p rpmbuild-${DISTR}/{BUILD,BUILDROOT,RPMS,SOURCES,SPECS,SRPMS}
                            cp devops/rpm/veil-connect.spec rpmbuild-${DISTR}/SPECS
                            sed -i -e "s:%%VER%%:${VERSION}:g" rpmbuild-${DISTR}/SPECS/veil-connect.spec
                            mkdir -p rpmbuild-${DISTR}/BUILD/opt/veil-connect
                            mkdir -p rpmbuild-${DISTR}/BUILD/usr/share/applications
                            cp -r build-${DISTR}/* doc/veil-connect.ico rpmbuild-${DISTR}/BUILD/opt/veil-connect
                            cp doc/veil-connect.desktop rpmbuild-${DISTR}/BUILD/usr/share/applications
                            cd rpmbuild-${DISTR}
                            rpmbuild --define "_topdir `pwd`" -v -bb SPECS/veil-connect.spec
                        '''
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
                        sh script: '''
                            REPO=${PRJNAME}-${DISTR}
                            DEB=$(ls -1 ${WORKSPACE}/devops/deb-${DISTR}/*.deb)

                            curl -sS -X POST -F file=@$DEB http://$APT_SRV:8008/api/files/${REPO}; echo ""
                            curl -sS -X POST http://$APT_SRV:8008/api/repos/${REPO}/file/${REPO}?forceReplace=1
                            JSON1="{\\"Name\\":\\"${REPO}-${DATE}\\"}"
                            JSON2="{\\"Snapshots\\":[{\\"Component\\":\\"main\\",\\"Name\\":\\"${REPO}-\${DATE}\\"}],\\"ForceOverwrite\\":true}"
                            curl -sS -X POST -H 'Content-Type: application/json' -d ${JSON1} http://$APT_SRV:8008/api/repos/${REPO}/snapshots
                            curl -sS -X PUT -H 'Content-Type: application/json' -d ${JSON2} http://$APT_SRV:8008/api/publish/${PRJNAME}/${DISTR}
                        '''
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
                        sh script: '''
                            REPO=${PRJNAME}-${DISTR}
                            DEB=$(ls -1 ${WORKSPACE}/devops/deb-${DISTR}/*.deb)

                            curl -sS -X POST -F file=@$DEB http://$APT_SRV:8008/api/files/${REPO}; echo ""
                            curl -sS -X POST http://$APT_SRV:8008/api/repos/${REPO}/file/${REPO}?forceReplace=1
                            JSON1="{\\"Name\\":\\"${REPO}-${DATE}\\"}"
                            JSON2="{\\"Snapshots\\":[{\\"Component\\":\\"main\\",\\"Name\\":\\"${REPO}-\${DATE}\\"}],\\"ForceOverwrite\\":true}"
                            curl -sS -X POST -H 'Content-Type: application/json' -d ${JSON1} http://$APT_SRV:8008/api/repos/${REPO}/snapshots
                            curl -sS -X PUT -H 'Content-Type: application/json' -d ${JSON2} http://$APT_SRV:8008/api/publish/${PRJNAME}/${DISTR}
                        '''
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
                        sh script: '''
                            REPO=${PRJNAME}-${DISTR}
                            DEB=$(ls -1 ${WORKSPACE}/devops/deb-${DISTR}/*.deb)

                            curl -sS -X POST -F file=@$DEB http://$APT_SRV:8008/api/files/${REPO}; echo ""
                            curl -sS -X POST http://$APT_SRV:8008/api/repos/${REPO}/file/${REPO}?forceReplace=1
                            JSON1="{\\"Name\\":\\"${REPO}-${DATE}\\"}"
                            JSON2="{\\"Snapshots\\":[{\\"Component\\":\\"main\\",\\"Name\\":\\"${REPO}-\${DATE}\\"}],\\"ForceOverwrite\\":true}"
                            curl -sS -X POST -H 'Content-Type: application/json' -d ${JSON1} http://$APT_SRV:8008/api/repos/${REPO}/snapshots
                            curl -sS -X PUT -H 'Content-Type: application/json' -d ${JSON2} http://$APT_SRV:8008/api/publish/${PRJNAME}/${DISTR}
                        '''
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
                        sh script: '''
                            REPO=${PRJNAME}-${DISTR}
                            DEB=$(ls -1 ${WORKSPACE}/devops/deb-${DISTR}/*.deb)

                            curl -sS -X POST -F file=@$DEB http://$APT_SRV:8008/api/files/${REPO}; echo ""
                            curl -sS -X POST http://$APT_SRV:8008/api/repos/${REPO}/file/${REPO}?forceReplace=1
                            JSON1="{\\"Name\\":\\"${REPO}-${DATE}\\"}"
                            JSON2="{\\"Snapshots\\":[{\\"Component\\":\\"main\\",\\"Name\\":\\"${REPO}-\${DATE}\\"}],\\"ForceOverwrite\\":true}"
                            curl -sS -X POST -H 'Content-Type: application/json' -d ${JSON1} http://$APT_SRV:8008/api/repos/${REPO}/snapshots
                            curl -sS -X PUT -H 'Content-Type: application/json' -d ${JSON2} http://$APT_SRV:8008/api/publish/${PRJNAME}/${DISTR}
                        '''
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
                        withCredentials([sshUserPrivateKey(credentialsId: 'uploader_ssh_key.id_rsa', keyFileVariable: 'SSH_KEY')]) {
                            sh script: '''
                                ssh -o StrictHostKeyChecking=no -i $SSH_KEY uploader@192.168.10.144 "mkdir -p /local_storage/veil-connect/linux/yum/${DISTR}/x86_64/Packages"
                                scp -o StrictHostKeyChecking=no -i $SSH_KEY ${WORKSPACE}/rpmbuild-${DISTR}/RPMS/x86_64/*.rpm uploader@192.168.10.144:/local_storage/veil-connect/linux/yum/${DISTR}/x86_64/Packages/
                                ssh -o StrictHostKeyChecking=no -i $SSH_KEY uploader@192.168.10.144 "
                                  rpm --resign /local_storage/veil-connect/linux/yum/${DISTR}/x86_64/Packages/*.rpm
                                  createrepo_c --update /local_storage/veil-connect/linux/yum/${DISTR}/x86_64
                                "
                            '''
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
                        withCredentials([sshUserPrivateKey(credentialsId: 'uploader_ssh_key.id_rsa', keyFileVariable: 'SSH_KEY')]) {
                            sh script: '''
                                ssh -o StrictHostKeyChecking=no -i $SSH_KEY uploader@192.168.10.144 "mkdir -p /local_storage/veil-connect/linux/yum/${DISTR}/x86_64/Packages"
                                scp -o StrictHostKeyChecking=no -i $SSH_KEY ${WORKSPACE}/rpmbuild-${DISTR}/RPMS/x86_64/*.rpm uploader@192.168.10.144:/local_storage/veil-connect/linux/yum/${DISTR}/x86_64/Packages/
                                ssh -o StrictHostKeyChecking=no -i $SSH_KEY uploader@192.168.10.144 "
                                  rpm --resign /local_storage/veil-connect/linux/yum/${DISTR}/x86_64/Packages/*.rpm
                                  createrepo_c --update /local_storage/veil-connect/linux/yum/${DISTR}/x86_64
                                "
                            '''
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
                        withCredentials([sshUserPrivateKey(credentialsId: 'uploader_ssh_key.id_rsa', keyFileVariable: 'SSH_KEY')]) {
                            sh script: '''
                                ssh -o StrictHostKeyChecking=no -i $SSH_KEY uploader@192.168.10.144 "mkdir -p /local_storage/veil-connect/linux/apt-rpm/${ARCH}/RPMS.${DISTR}"
                                scp -o StrictHostKeyChecking=no -i $SSH_KEY ${WORKSPACE}/rpmbuild-${DISTR}/RPMS/${ARCH}/${PRJNAME}-${VERSION}*.rpm uploader@192.168.10.144:/local_storage/veil-connect/linux/apt-rpm/${ARCH}/RPMS.${DISTR}/
                                ssh -o StrictHostKeyChecking=no -i $SSH_KEY uploader@192.168.10.144 "cd /local_storage/veil-connect/linux/apt-rpm/${ARCH}/RPMS.${DISTR}; ln -sf ${PRJNAME}-${VERSION}*.rpm ${PRJNAME}-latest.rpm"
                            '''
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
                        withCredentials([sshUserPrivateKey(credentialsId: 'uploader_ssh_key.id_rsa', keyFileVariable: 'SSH_KEY')]) {
                            sh script: '''
                                ssh -o StrictHostKeyChecking=no -i $SSH_KEY uploader@192.168.10.144 "mkdir -p /local_storage/veil-connect/linux/yum/${DISTR}/x86_64/Packages"
                                scp -o StrictHostKeyChecking=no -i $SSH_KEY ${WORKSPACE}/rpmbuild-${DISTR}/RPMS/x86_64/*.rpm uploader@192.168.10.144:/local_storage/veil-connect/linux/yum/${DISTR}/x86_64/Packages/
                                ssh -o StrictHostKeyChecking=no -i $SSH_KEY uploader@192.168.10.144 "
                                  rpm --resign /local_storage/veil-connect/linux/yum/${DISTR}/x86_64/Packages/*.rpm
                                  createrepo_c --update /local_storage/veil-connect/linux/yum/${DISTR}/x86_64
                                "
                            '''
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

def notifyBuild(rocketNotify, buildStatus) {
    buildStatus =  buildStatus ?: 'SUCCESSFUL'

    def summary = "${buildStatus}: Job '${env.JOB_NAME} [${env.BUILD_NUMBER}]' (${env.BUILD_URL})" + "\n"

    summary += "BRANCH: ${BRANCH}. VERSION: ${VERSION}"

    if (rocketNotify){
        try {
            rocketSend (channel: 'jenkins-notify', message: summary, serverUrl: '192.168.14.210', trustSSL: true, rawMessage: true)
        } catch (Exception e) {
            println "Notify is failed"
        }
    }
}
