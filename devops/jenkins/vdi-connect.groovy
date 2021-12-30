def currentDate = new Date().format('yyyyMMddHHmmss')
def rocketNotify = true

pipeline {
    agent {
        label "bld-agent"
    }

    environment {
        APT_SRV = "192.168.11.118"
        PRJNAME = "veil-connect"
        DATE = "$currentDate"
        AGENT = "bld-agent"
        NFS_DIR = "/nfs/veil-connect"
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
        string(      name: 'BRANCH',               defaultValue: 'master',          description: 'branch')
        string(      name: 'VERSION',              defaultValue: '1.10.0',           description: 'version')
        booleanParam(name: 'STRETCH',              defaultValue: true,              description: 'create DEB?')
        booleanParam(name: 'BUSTER',               defaultValue: true,              description: 'create DEB?')
        booleanParam(name: 'BIONIC',               defaultValue: true,              description: 'create DEB?')
        booleanParam(name: 'FOCAL',                defaultValue: true,              description: 'create DEB?')
        booleanParam(name: 'EL7',                  defaultValue: true,              description: 'create RPM?')
        booleanParam(name: 'EL8',                  defaultValue: true,              description: 'create RPM?')
        booleanParam(name: 'RED73',                defaultValue: true,              description: 'create RPM?')
        booleanParam(name: 'ALT9',                 defaultValue: true,              description: 'create RPM?')
        booleanParam(name: 'WIN32',                defaultValue: true,              description: 'create EXE?')
        booleanParam(name: 'WIN64',                defaultValue: true,              description: 'create EXE?')
        booleanParam(name: 'EMBEDDED',             defaultValue: true,              description: 'create DEB?')
    }

    stages {
        stage ('checkout') {
            steps {
                notifyBuild(rocketNotify, ":bell: STARTED")
                cleanWs()
                git branch: '$BRANCH', url: 'git@gitlab.bazalt.team:vdi/veil-connect.git'
                stash name: 'src', includes: '**', excludes: '**/.git,**/.git/**'
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
                        sh "docker build -f devops/docker/Dockerfile.stretch . -t veil-connect-builder-stretch:${VERSION}"
                    }
                }

                stage ('buster. docker build') {
                    when {
                        beforeAgent true
                        expression { params.BUSTER == true }
                    }
                    steps {
                        sh "docker build -f devops/docker/Dockerfile.buster . -t veil-connect-builder-buster:${VERSION}"
                    }
                }

                stage ('bionic. docker build') {
                    when {
                        beforeAgent true
                        expression { params.BIONIC == true }
                    }
                    steps {
                        sh "docker build -f devops/docker/Dockerfile.bionic . -t veil-connect-builder-bionic:${VERSION}"
                    }
                }

                stage ('focal. docker build') {
                    when {
                        beforeAgent true
                        expression { params.FOCAL == true }
                    }
                    steps {
                        sh "docker build -f devops/docker/Dockerfile.focal . -t veil-connect-builder-focal:${VERSION}"
                    }
                }

                stage ('el7. docker build') {
                    when {
                        beforeAgent true
                        expression { params.EL7 == true }
                    }
                    steps {
                        sh "docker build -f devops/docker/Dockerfile.el7 . -t veil-connect-builder-el7:${VERSION}"
                    }
                }

                stage ('el8. docker build') {
                    when {
                        beforeAgent true
                        expression { params.EL8 == true }
                    }
                    steps {
                        sh "docker build -f devops/docker/Dockerfile.el8 . -t veil-connect-builder-el8:${VERSION}"
                    }
                }

                stage ('alt9. docker build') {
                    when {
                        beforeAgent true
                        expression { params.ALT9 == true }
                    }
                    steps {
                        sh "docker build -f devops/docker/Dockerfile.alt9 . -t veil-connect-builder-alt9:${VERSION}"
                    }
                }

                stage ('red7-3. docker build') {
                    when {
                        beforeAgent true
                        expression { params.RED73 == true }
                    }
                    steps {
                        sh "docker build -f devops/docker/Dockerfile.RED7-3 . -t veil-connect-builder-redos7.3:${VERSION}"
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
                            image "veil-connect-builder-stretch:${VERSION}"
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
                            image "veil-connect-builder-buster:${VERSION}"
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
                            image "veil-connect-builder-bionic:${VERSION}"
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
                            image "veil-connect-builder-focal:${VERSION}"
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
                            image "veil-connect-builder-el7:${VERSION}"
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
                            image "veil-connect-builder-el8:${VERSION}"
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
                            image "veil-connect-builder-alt9:${VERSION}"
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
                            rpmbuild --define "_topdir `pwd`" -v -bb SPECS/veil-connect-alt.spec
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
                            image "veil-connect-builder-redos7.3:${VERSION}"
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

                stage ('windows-x32. build') {
                    when {
                        beforeAgent true
                        expression { params.WIN32 == true }
                    }
                    agent {
                        label 'win7_x32'
                    }
                    steps {
                        unstash 'src'
                        bat script: '''
                            mkdir build
                            cd build
                            cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_x32=ON -G "MinGW Makefiles" ..
                            mingw32-make

                            cd ..
                            copy doc\\veil-connect.ico build
                            cd build
                            rmdir /S /Q CMakeFiles
                            del CMakeCache.txt cmake_install.cmake Makefile libveil_connect.dll.a start_vdi_client.sh

                            REM ### Copy files ###
                            mkdir log
                            copy C:\\msys32\\mingw32\\lib\\gdk-pixbuf-2.0\\2.10.0\\loaders.cache lib\\gdk-pixbuf-2.0\\2.10.0
                            copy rdp_data\\rdp_template_file.txt rdp_data\\rdp_file.rdp
                            xcopy C:\\msys32\\mingw32\\share\\glib-2.0 share\\glib-2.0 /E /H /I
                            xcopy C:\\msys32\\mingw32\\share\\icons share\\icons /E /H /I /Q
                            copy C:\\msys32\\mingw32\\bin\\gspawn-win32-helper.exe
                            copy C:\\msys32\\mingw32\\bin\\gspawn-win32-helper-console.exe

                            REM ### Add vcredist to installer ###
                            curl http://nexus.bazalt.team/repository/files/vcredist/VC_redist.x86.exe --output vc_redist.exe

                            REM ### Add UsbDk to installer ###
                            curl http://mothership.bazalt.team/files/usbdk/UsbDk_1.0.22_x86.msi --output usbdk.msi

                            REM ### Make installer ###
                            sed -i -e "s:&&VER&&:%VERSION%:g" -e "s:&&BUILD_VER&&:%BUILD_NUMBER%:g" %WORKSPACE%/devops\\inno-setup\\veil-connect-installer-32.iss
                            iscc "%WORKSPACE%/devops\\inno-setup\\veil-connect-installer-32.iss"
                        '''
                    }
                }

                stage ('windows-x64. build') {
                    when {
                        beforeAgent true
                        expression { params.WIN64 == true }
                    }
                    agent {
                        label 'win10_x64_veil_guest_agent'
                    }
                    steps {
                        unstash 'src'
                        bat script: '''
                            mkdir build
                            cd build
                            cmake -DCMAKE_BUILD_TYPE=Release -G "MinGW Makefiles" ..
                            mingw32-make

                            cd ..
                            copy doc\\veil-connect.ico build
                            cd build
                            rmdir /S /Q CMakeFiles
                            del CMakeCache.txt cmake_install.cmake Makefile libveil_connect.dll.a start_vdi_client.sh

                            REM ### Copy files ###
                            mkdir log
                            copy C:\\msys32\\mingw64\\lib\\gdk-pixbuf-2.0\\2.10.0\\loaders.cache lib\\gdk-pixbuf-2.0\\2.10.0
                            copy rdp_data\\rdp_template_file.txt rdp_data\\rdp_file.rdp
                            xcopy C:\\msys32\\mingw64\\share\\glib-2.0 share\\glib-2.0 /E /H /I
                            xcopy C:\\msys32\\mingw64\\share\\icons share\\icons /E /H /I /Q
                            copy C:\\msys32\\mingw64\\bin\\gspawn-win64-helper.exe
                            copy C:\\msys32\\mingw64\\bin\\gspawn-win64-helper-console.exe

                            perl -pi -e 's/crosshair/default\\0\\0/g' libspice-client-gtk-3.0-5.dll

                            REM ### Add vcredist to installer ###
                            curl http://nexus.bazalt.team/repository/files/vcredist/VC_redist.x64.exe --output vc_redist.exe

                            REM ### Add UsbDk to installer ###
                            curl http://mothership.bazalt.team/files/usbdk/UsbDk_1.0.22_x64.msi --output usbdk.msi

                            REM ### Add FreeRDP alternative libs to installer
                            mkdir freerdp_2.2.0
                            for %%I in (freerdp2 freerdp-client2 winpr2 winpr-tools2) do (curl http://nexus.bazalt.team/repository/files/freerdp/x64/2.2.0/%%I.dll --output freerdp_2.2.0\\%%I.dll)
                            mkdir freerdp_2.3.2
                            for %%I in (freerdp2 freerdp-client2 winpr2 winpr-tools2) do (move %%I.dll freerdp_2.3.2)

                            REM ### Make installer ###
                            sed -i -e "s:&&VER&&:%VERSION%:g" -e "s:&&BUILD_VER&&:%BUILD_NUMBER%:g" %WORKSPACE%/devops\\inno-setup\\veil-connect-installer-64.iss
                            iscc "%WORKSPACE%/devops\\inno-setup\\veil-connect-installer-64.iss"
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
                    image "veil-connect-builder-bionic:${VERSION}"
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
                        sh script: '''
                            ssh uploader@192.168.10.144 "mkdir -p /local_storage/veil-connect/linux/yum/${DISTR}/x86_64/Packages"
                            scp ${WORKSPACE}/rpmbuild-${DISTR}/RPMS/x86_64/*.rpm uploader@192.168.10.144:/local_storage/veil-connect/linux/yum/${DISTR}/x86_64/Packages/

                            ssh uploader@192.168.10.144 "
                              rpm --resign /local_storage/veil-connect/linux/yum/${DISTR}/x86_64/Packages/*.rpm
                              createrepo --update /local_storage/veil-connect/linux/yum/${DISTR}/x86_64
                            "
                        '''
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
                        sh script: '''
                            ssh uploader@192.168.10.144 "mkdir -p /local_storage/veil-connect/linux/yum/${DISTR}/x86_64/Packages"
                            scp ${WORKSPACE}/rpmbuild-${DISTR}/RPMS/x86_64/*.rpm uploader@192.168.10.144:/local_storage/veil-connect/linux/yum/${DISTR}/x86_64/Packages/

                            ssh uploader@192.168.10.144 "
                              rpm --resign /local_storage/veil-connect/linux/yum/${DISTR}/x86_64/Packages/*.rpm
                              createrepo --update /local_storage/veil-connect/linux/yum/${DISTR}/x86_64
                            "
                        '''
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
                        sh script: '''
                            ssh uploader@192.168.10.144 "mkdir -p /local_storage/veil-connect/linux/apt-rpm/${ARCH}/RPMS.${DISTR}"
                            scp ${WORKSPACE}/rpmbuild-${DISTR}/RPMS/${ARCH}/${PRJNAME}-${VERSION}*.rpm uploader@192.168.10.144:/local_storage/veil-connect/linux/apt-rpm/${ARCH}/RPMS.${DISTR}/
                            ssh uploader@192.168.10.144 "cd /local_storage/veil-connect/linux/apt-rpm/${ARCH}/RPMS.${DISTR}; ln -sf ${PRJNAME}-${VERSION}*.rpm ${PRJNAME}-latest.rpm"
                        '''
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
                        sh script: '''
                            ssh uploader@192.168.10.144 "mkdir -p /local_storage/veil-connect/linux/yum/${DISTR}/x86_64/Packages"
                            scp ${WORKSPACE}/rpmbuild-${DISTR}/RPMS/x86_64/*.rpm uploader@192.168.10.144:/local_storage/veil-connect/linux/yum/${DISTR}/x86_64/Packages/

                            ssh uploader@192.168.10.144 "
                              rpm --resign /local_storage/veil-connect/linux/yum/${DISTR}/x86_64/Packages/*.rpm
                              createrepo --update /local_storage/veil-connect/linux/yum/${DISTR}/x86_64
                            "
                        '''
                    }
                }

                stage ('windows-x32. deploy to repo') {
                    when {
                        beforeAgent true
                        expression { params.WIN32 == true }
                    }
                    agent {
                        label 'win7_x32'
                    }
                    steps {
                        bat script: '''
                            ssh uploader@mothership.bazalt.team mkdir -p /local_storage/veil-connect/windows/%VERSION%
                            scp veil-connect-installer.exe uploader@mothership.bazalt.team:/local_storage/veil-connect/windows/%VERSION%/veil-connect_%VERSION%-x32-installer.exe
                            ssh uploader@192.168.10.144 "cd /local_storage/veil-connect/windows/; ln -sfT %VERSION% latest"
                        '''
                    }
                }

                stage ('windows-x64. deploy to repo') {
                    when {
                        beforeAgent true
                        expression { params.WIN64 == true }
                    }
                    agent {
                        label 'win10_x64_veil_guest_agent'
                    }
                    steps {
                        bat script: '''
                            ssh uploader@mothership.bazalt.team mkdir -p /local_storage/veil-connect/windows/%VERSION%
                            scp veil-connect-installer.exe uploader@mothership.bazalt.team:/local_storage/veil-connect/windows/%VERSION%/veil-connect_%VERSION%-x64-installer.exe
                            ssh uploader@192.168.10.144 "cd /local_storage/veil-connect/windows/; ln -sfT %VERSION% latest"
                        '''
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
                sh script: '''
                    ssh uploader@192.168.10.144 mkdir -p /local_storage/veil-connect-embedded/${VERSION}
                    scp ${WORKSPACE}/devops/deb_embedded/*.deb uploader@192.168.10.144:/local_storage/veil-connect-embedded/${VERSION}/
                    ssh uploader@192.168.10.144 "cd /local_storage/veil-connect-embedded/; ln -sfT ${VERSION} latest"
                '''
            }
        }

        stage ('deploy universal linux installer') {
            steps {
                sh script: '''
                    scp ${WORKSPACE}/devops/veil-connect-linux-installer.sh uploader@192.168.10.144:/local_storage/veil-connect/linux/
                '''
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
