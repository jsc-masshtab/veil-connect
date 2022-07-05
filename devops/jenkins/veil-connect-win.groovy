def rocketNotify = true

pipeline {
    agent {
        label "master"
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
        string(      name: 'BRANCH',      defaultValue: 'master',                                        description: 'branch')
        string(      name: 'VERSION',     defaultValue: '1.14.0',                                        description: 'version')
        choice(      name: 'WIN32_AGENT', choices: ['win7_x32'],                                         description: 'jenkins build agent')
        choice(      name: 'WIN64_AGENT', choices: ['cloud-windows-2012', 'win10_x64_veil_guest_agent'], description: 'jenkins build agent')
        booleanParam(name: 'WIN32',       defaultValue: true,                                            description: 'create EXE?')
        booleanParam(name: 'WIN64',       defaultValue: true,                                            description: 'create EXE?')
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
                stash name: 'src', includes: '**', excludes: '**/.git,**/.git/**'
            }
        }

        stage ('build') {
            parallel {
                stage ('windows-x32. build') {
                    when {
                        beforeAgent true
                        expression { params.WIN32 == true }
                    }
                    agent {
                        label "${WIN32_AGENT}"
                    }
                    steps {
                        cleanWs()
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

                            REM ### Copy files ###
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
                        label "${WIN64_AGENT}"
                    }
                    steps {
                        cleanWs()
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

                            REM ### Add vcredist to installer ###
                            curl http://nexus.bazalt.team/repository/files/vcredist/VC_redist.x64.exe --output vc_redist.exe

                            REM ### Add UsbDk to installer ###
                            curl http://mothership.bazalt.team/files/usbdk/UsbDk_1.0.22_x64.msi --output usbdk.msi

                            REM ### Add FreeRDP alternative libs to installer
                            mkdir freerdp_2.2.0
                            for %%I in (freerdp2 freerdp-client2 winpr2 winpr-tools2) do (curl http://nexus.bazalt.team/repository/files/freerdp/x64/2.2.0/%%I.dll --output freerdp_2.2.0\\%%I.dll)
                            mkdir freerdp_2.3.2
                            for %%I in (freerdp2 freerdp-client2 winpr2 winpr-tools2) do (copy C:\\job\\FreeRDP-2.3.2\\Release\\%%I.dll freerdp_2.3.2)

                            REM ### Make installer ###
                            sed -i -e "s:&&VER&&:%VERSION%:g" -e "s:&&BUILD_VER&&:%BUILD_NUMBER%:g" %WORKSPACE%/devops\\inno-setup\\veil-connect-installer-64.iss
                            iscc "%WORKSPACE%/devops\\inno-setup\\veil-connect-installer-64.iss"
                        '''
                    }
                }
            }
        }

        stage ('deploy to repo') {
            parallel {
                stage ('windows-x32. deploy to repo') {
                    when {
                        beforeAgent true
                        expression { params.WIN32 == true }
                    }
                    agent {
                        label "${WIN32_AGENT}"
                    }
                    steps {
                        withCredentials([sshUserPrivateKey(credentialsId: 'uploader_ssh_key.id_rsa', keyFileVariable: 'SSH_KEY')]) {
                            bat script: '''
                                ssh -o StrictHostKeyChecking=no -i %SSH_KEY% uploader@mothership.bazalt.team mkdir -p /local_storage/veil-connect/windows/%VERSION%
                                scp -o StrictHostKeyChecking=no -i %SSH_KEY% veil-connect-installer.exe uploader@mothership.bazalt.team:/local_storage/veil-connect/windows/%VERSION%/veil-connect_%VERSION%-x32-installer.exe
                                ssh -o StrictHostKeyChecking=no -i %SSH_KEY% uploader@192.168.10.144 "cd /local_storage/veil-connect/windows/; ln -sfT %VERSION% latest"
                            '''
                        }
                    }
                }

                stage ('windows-x64. deploy to repo') {
                    when {
                        beforeAgent true
                        expression { params.WIN64 == true }
                    }
                    agent {
                        label "${WIN64_AGENT}"
                    }
                    steps {
                        withCredentials([sshUserPrivateKey(credentialsId: 'uploader_ssh_key.id_rsa', keyFileVariable: 'SSH_KEY')]) {
                            bat script: '''
                                icacls %SSH_KEY% /inheritance:r
                                icacls %SSH_KEY% /grant:r "%username%":"(R)"

                                ssh -o StrictHostKeyChecking=no -i %SSH_KEY% uploader@mothership.bazalt.team "mkdir -p /local_storage/veil-connect/windows/%VERSION%"
                                scp -o StrictHostKeyChecking=no -i %SSH_KEY% veil-connect-installer.exe uploader@mothership.bazalt.team:/local_storage/veil-connect/windows/%VERSION%/veil-connect_%VERSION%-x64-installer.exe
                                ssh -o StrictHostKeyChecking=no -i %SSH_KEY% uploader@mothership.bazalt.team "cd /local_storage/veil-connect/windows/; ln -sfT %VERSION% latest"
                            '''
                        }
                    }
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
