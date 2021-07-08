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
        string(      name: 'VERSION',              defaultValue: '1.6.6',           description: 'version')
        booleanParam(name: 'STRETCH',              defaultValue: true,              description: 'create DEB?')
        booleanParam(name: 'BUSTER',               defaultValue: true,              description: 'create DEB?')
        booleanParam(name: 'XENIAL',               defaultValue: true,              description: 'create DEB?')
        booleanParam(name: 'BIONIC',               defaultValue: true,              description: 'create DEB?')
        booleanParam(name: 'FOCAL',                defaultValue: true,              description: 'create DEB?')
        booleanParam(name: 'EL7',                  defaultValue: true,              description: 'create RPM?')
        booleanParam(name: 'EL8',                  defaultValue: true,              description: 'create RPM?')
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

                stage ('xenial. docker build') {
                    when {
                        beforeAgent true
                        expression { params.XENIAL == true }
                    }
                    steps {
                        sh "docker build -f devops/docker/Dockerfile.xenial . -t veil-connect-builder-xenial:${VERSION}"
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
                
                            mkdir lib\\gdk-pixbuf-2.0\\2.10.0\\loaders log
                            copy C:\\msys32\\mingw32\\lib\\gdk-pixbuf-2.0\\2.10.0\\loaders\\*.dll lib\\gdk-pixbuf-2.0\\2.10.0\\loaders
                            copy C:\\msys32\\mingw32\\lib\\gdk-pixbuf-2.0\\2.10.0\\loaders.cache lib\\gdk-pixbuf-2.0\\2.10.0
                            xcopy C:\\msys32\\mingw32\\lib\\gio lib\\gio /E /H /I
                
                            for %%I in (libgstapp libgstaudioconvert libgstaudiorate libgstaudioresample libgstautodetect libgstcoreelements ^
                            libgstcoretracers libgstdirectsound) do (echo D|xcopy C:\\msys32\\mingw32\\lib\\gstreamer-1.0\\%%I.dll lib\\gstreamer-1.0)
                                    
                            copy rdp_data\\rdp_template_file.txt rdp_data\\rdp_file.rdp
                                    
                            xcopy C:\\msys32\\mingw32\\share\\glib-2.0 share\\glib-2.0 /E /H /I
                            xcopy C:\\msys32\\mingw32\\share\\icons share\\icons /E /H /I /Q
                                    
                            for %%I in (freerdp2 freerdp-client2 winpr2 winpr-tools2) do (copy C:\\job\\FreeRDP-2.0.0-rc4\\Release\\%%I.dll)
                
                            for %%I in (libatk-1.0-0 libbz2-1 libcairo-2 libcairo-gobject-2 libcares-4 libcrypto-1_1 ^
                            libdatrie-1 libdbus-1-3 libepoxy-0 libexpat-1 libffi-7 libfontconfig-1 libfreeglut libfreetype-6 libfribidi-0 ^
                            libgailutil-18 libgailutil-3-0 libgcc_s_dw2-1 libgd libgdbm-6 libgdbm_compat-4 libgdk-3-0 ^
                            libgdk-win32-2.0-0 libgdkglext-win32-1.0-0 libgdk_pixbuf-2.0-0 libgettextlib-0-19-8-1 libgettextpo-0 ^
                            libgettextsrc-0-19-8-1 libgif-7 libgio-2.0-0 libgirepository-1.0-1 libglade-2.0-0 libgladeui-2-6 ^
                            libglib-2.0-0 libgmodule-2.0-0 libgmp-10 libgmpxx-4 libgnarl-10 ^
                            libgnutls-30 libgnutlsxx-28 libgobject-2.0-0 libgomp-1 libgraphene-1.0-0 libgraphite2 libgslcblas-0 ^
                            libgstallocators-1.0-0 libgstapp-1.0-0 libgstaudio-1.0-0 libgstbase-1.0-0 libgstcheck-1.0-0 libgstcontroller-1.0-0 ^
                            libgstfft-1.0-0 libgstgl-1.0-0 libgstnet-1.0-0 libgstpbutils-1.0-0 libgstreamer-1.0-0 libgstriff-1.0-0 libgstrtp-1.0-0 ^
                            libgstrtsp-1.0-0 libgstsdp-1.0-0 libgsttag-1.0-0 libgstvideo-1.0-0 libgthread-2.0-0 libgtk-3-0 ^
                            libgtk-win32-2.0-0 libgtkglext-win32-1.0-0 libgts-0-7-5 libgvc-6 libgvplugin_core-6 ^
                            libgvplugin_devil-6 libgvplugin_dot_layout-6 libgvplugin_gd-6 libgvplugin_gdk-6 libgvplugin_gtk-6 libgvplugin_neato_layout-6 ^
                            libgvplugin_pango-6 libgvplugin_poppler-6 libgvplugin_rsvg-6 libgvplugin_webp-6 libgvpr-2 libHalf-2_5 libharfbuzz-0 ^
                            libharfbuzz-gobject-0 libharfbuzz-icu-0 libharfbuzz-subset-0 libhistory8 libhogweed-6 libiconv-2 libicuio67 libicutest67 ^
                            libicutu67 libicuuc67 libidn2-0 libIex-2_5 libIexMath-2_5 libIL libIlmImfUtil-2_5 libIlmThread-2_5 libILU libILUT ^
                            libImath-2_5 libintl-8 libjansson-4 libjasper-4 libjemalloc libjpeg-8 libjson-glib-1.0-0 liblcms2-2 libltdl-7 ^
                            liblz4 liblzma-5 liblzo2-2 libmetalink-3 libmng-2 libmpdec-2 libnettle-8 libnghttp2-14 libnspr4 libobjc-4 libogg-0 ^
                            libopenjp2-7 libopenjp3d-7 libopenjpip-7 libopenjpwl-7 libopenmj2-7 libopus-0 liborc-0.4-0 liborc-test-0.4-0 ^
                            libp11-kit-0 libpango-1.0-0 libpangocairo-1.0-0 libpangoft2-1.0-0 libpangowin32-1.0-0 libpathplan-4 ^
                            libpcre-1 libpcre16-0 libpcre32-0 libpcrecpp-0 libpcreposix-0 libphodav-2.0-0 libpixman-1-0 libplc4 libplds4 ^
                            libpng16-16 libpoppler-cpp-0 libpoppler-glib-8 libpoppler-qt5-1 libproxy-1 libpsl-5 libquadmath-0 ^
                            libreadline8 librsvg-2-2 libsasl2-3 libsoup-2.4-1 libsoup-gnome-2.4-1 ^
                            libspice-client-glib-2.0-8 libspice-client-gtk-3.0-5 libsqlite3-0 libsquish libssh2-1 libssl-1_1 ^
                            libssp-0 "libstdc++-6" libsystre-0 libtasn1-6 libtermcap-0 libthai-0 libtheora-0 libtheoradec-1 libtheoraenc-1 ^
                            libtiff-5 libtiffxx-5 libtre-5 libturbojpeg libunistring-2 libusb-1.0 libusbredirhost-1 libusbredirparser-1 ^
                            libvalaccodegen libvaladoc-0.48-0 libbrotlidec libbrotlicommon ^
                            libvorbis-0 libvorbisenc-2 libvorbisfile-3 libvorbisidec-1 libwebp-7 libwebpdecoder-3 libwebpdemux-2 libwebpmux-3 ^
                            libwinpthread-1 libxdot-4 libxml2-2 libXpm-noX4 libzstd nss3 nssckbi nssutil3 smime3 softokn3 ssl3 ^
                            tcl86 zlib1) do (copy C:\\msys32\\mingw32\\bin\\%%I.dll)

                            copy C:\\msys32\\mingw32\\bin\\gspawn-win32-helper.exe
                            copy C:\\msys32\\mingw32\\bin\\gspawn-win32-helper-console.exe

                            # make installer
                            sed -i -e "s:&&VER&&:%VERSION%:g" -e "s:&&BUILD_VER&&:%BUILD_NUMBER%:g" %WORKSPACE%/devops\\inno-setup\\veil-connect-installer.iss
                            iscc "%WORKSPACE%/devops\\inno-setup\\veil-connect-installer.iss"
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

                            mkdir lib\\gdk-pixbuf-2.0\\2.10.0\\loaders log
                            copy C:\\msys32\\mingw64\\lib\\gdk-pixbuf-2.0\\2.10.0\\loaders\\*.dll lib\\gdk-pixbuf-2.0\\2.10.0\\loaders
                            copy C:\\msys32\\mingw64\\lib\\gdk-pixbuf-2.0\\2.10.0\\loaders.cache lib\\gdk-pixbuf-2.0\\2.10.0
                            xcopy C:\\msys32\\mingw64\\lib\\gio lib\\gio /E /H /I

                            for %%I in (libgstapp libgstaudioconvert libgstaudiorate libgstaudioresample libgstautodetect ^
                            libgstcoreelements libgstcoretracers libgstdirectsound libgstdirectsoundsink ^
                            libgstdirectsoundsrc) do (echo D|xcopy C:\\msys32\\mingw64\\lib\\gstreamer-1.0\\%%I.dll lib\\gstreamer-1.0)

                            copy rdp_data\\rdp_template_file.txt rdp_data\\rdp_file.rdp

                            xcopy C:\\msys32\\mingw64\\share\\glib-2.0 share\\glib-2.0 /E /H /I
                            xcopy C:\\msys32\\mingw64\\share\\icons share\\icons /E /H /I /Q
     
                            for %%I in (freerdp2 freerdp-client2 winpr2 winpr-tools2) do (copy C:\\job\\FreeRDP-2.3.2\\Release\\%%I.dll)

                            for %%I in (libatk-1.0-0 libbz2-1 libcairo-2 libcairo-gobject-2 libcares-3 libcroco-0.6-3 libcrypto-1_1-x64 ^
                            libdatrie-1 libdbus-1-3 libepoxy-0 libexpat-1 libffi-6 libfontconfig-1 libfreeglut libfreetype-6 libfribidi-0 ^
                            libgailutil-18 libgailutil-3-0 libgcc_s_seh-1 libgcrypt-20 libgd libgdbm-6 libgdbm_compat-4 libgdk-3-0 ^
                            libgdk-win32-2.0-0 libgdkglext-win32-1.0-0 libgdkmm-2.4-1 libgdk_pixbuf-2.0-0 libgettextlib-0-19-8-1 libgettextpo-0 ^
                            libgettextsrc-0-19-8-1 libgif-7 libgio-2.0-0 libgiomm-2.4-1 libgirepository-1.0-1 libglade-2.0-0 libgladeui-2-6 ^
                            libglib-2.0-0 libglibmm-2.4-1 libglibmm_generate_extra_defs-2.4-1 libgmodule-2.0-0 libgmp-10 libgmpxx-4 libgnarl-8 ^
                            libgnutls-30 libgnutlsxx-28 libgobject-2.0-0 libgomp-1 libgpg-error-0 libgraphene-1.0-0 libgraphite2 libgslcblas-0 ^
                            libgstallocators-1.0-0 libgstapp-1.0-0 libgstaudio-1.0-0 libgstbase-1.0-0 libgstcheck-1.0-0 libgstcontroller-1.0-0 ^
                            libgstfft-1.0-0 libgstgl-1.0-0 libgstnet-1.0-0 libgstpbutils-1.0-0 libgstreamer-1.0-0 libgstriff-1.0-0 libgstrtp-1.0-0 ^
                            libgstrtsp-1.0-0 libgstsdp-1.0-0 libgsttag-1.0-0 libgstvideo-1.0-0 libgthread-2.0-0 libgtk-3-0 libgtk-vnc-2.0-0 ^
                            libgtk-win32-2.0-0 libgtkglext-win32-1.0-0 libgtkreftestprivate-0 libgts-0-7-5 libgvc-6 libgvnc-1.0-0 libgvplugin_core-6 ^
                            libgvplugin_devil-6 libgvplugin_dot_layout-6 libgvplugin_gd-6 libgvplugin_gdk-6 libgvplugin_gtk-6 libgvplugin_neato_layout-6 ^
                            libgvplugin_pango-6 libgvplugin_poppler-6 libgvplugin_rsvg-6 libgvplugin_webp-6 libgvpr-2 libHalf-2_3 libharfbuzz-0 ^
                            libharfbuzz-gobject-0 libharfbuzz-icu-0 libharfbuzz-subset-0 libhistory8 libhogweed-4 libiconv-2 libicuio64 libicutest64 ^
                            libicutu64 libicuuc64 libidn2-0 libIex-2_3 libIexMath-2_3 libIL libIlmImfUtil-2_3 libIlmThread-2_3 libILU libILUT ^
                            libImath-2_3 libintl-8 libjansson-4 libjasper-4 libjemalloc libjpeg-8 libjson-glib-1.0-0 liblcms2-2 libltdl-7 ^
                            liblz4 liblzma-5 liblzo2-2 libmetalink-3 libmng-2 libmpdec-2 libnettle-6 libnghttp2-14 libnspr4 libobjc-4 libogg-0 ^
                            libopenjp2-7 libopenjp3d-7 libopenjpip-7 libopenjpwl-7 libopenmj2-7 libopus-0 liborc-0.4-0 liborc-test-0.4-0 ^
                            libp11-kit-0 libpango-1.0-0 libpangocairo-1.0-0 libpangoft2-1.0-0 libpangomm-1.4-1 libpangowin32-1.0-0 libpathplan-4 ^
                            libpcre-1 libpcre16-0 libpcre32-0 libpcrecpp-0 libpcreposix-0 libphodav-2.0-0 libpixman-1-0 libplc4 libplds4 ^
                            libpng16-16 libpoppler-cpp-0 libpoppler-glib-8 libpoppler-qt5-1 libportablexdr-0 libproxy-1 libpsl-5 libquadmath-0 ^
                            libreadline8 librest-0.7-0 librest-extras-0.7-0 librsvg-2-2 libsasl2-3 libsigc-2.0-0 libsoup-2.4-1 libsoup-gnome-2.4-1 ^
                            libsource-highlight-4 libspice-client-glib-2.0-8 libspice-client-gtk-3.0-5 libsqlite3-0 libsquish libssh2-1 libssl-1_1-x64 ^
                            libssp-0 "libstdc++-6" libsystre-0 libtasn1-6 libtermcap-0 libthai-0 libtheora-0 libtheoradec-1 libtheoraenc-1 ^
                            libtiff-5 libtiffxx-5 libtre-5 libturbojpeg libunistring-2 libusb-1.0 libusbredirhost-1 libusbredirparser-1 ^
                            libvalaccodegen libvaladoc-0.44-0 libview-2 libvirt-admin-0 libvirt-glib-1.0-0 libvirt-lxc-0 libvirt-qemu-0 ^
                            libvorbis-0 libvorbisenc-2 libvorbisfile-3 libvorbisidec-1 libwebp-7 libwebpdecoder-3 libwebpdemux-2 libwebpmux-3 ^
                            libwinpthread-1 libxdot-4 libxml2-2 libXpm-noX4 libzstd nss3 nssckbi nssutil3 smime3 softokn3 ssl3 ^
                            tcl86 zlib1) do (copy C:\\msys32\\mingw64\\bin\\%%I.dll)

                            copy C:\\msys32\\mingw64\\bin\\gspawn-win64-helper.exe
                            copy C:\\msys32\\mingw64\\bin\\gspawn-win64-helper-console.exe

                            copy C:\\job\\openh264-6\\openh264-6.dll

                            perl -pi -e 's/crosshair/default\\0\\0/g' libspice-client-gtk-3.0-5.dll

                            rem make installer
                            sed -i -e "s:&&VER&&:%VERSION%:g" -e "s:&&BUILD_VER&&:%BUILD_NUMBER%:g" %WORKSPACE%/devops\\inno-setup\\veil-connect-installer.iss
                            iscc "%WORKSPACE%/devops\\inno-setup\\veil-connect-installer.iss"
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
