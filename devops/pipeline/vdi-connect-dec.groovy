def currentDate = new Date().format('yyyyMMddHHmmss')
def slackNotify = true

notifyBuild(slackNotify, "STARTED", "Start new build. Version: ${currentDate}")

pipeline {
    agent {
        label 'win10_x64_veil_guest_agent'
    }

    post {
        failure {
            println "Something goes wrong"
            println "Current build marked as ${currentBuild.result}"
            notifyBuild(slackNotify,"FAILED", "Something goes wrong. Version: ${currentDate}")
        }

        aborted {
            println "Build was interrupted manually"
            println "Current build marked as ${currentBuild.result}"
            notifyBuild(slackNotify,"FAILED", "Build was interrupted manually. Version: ${currentDate}")
        }

        success {
            notifyBuild(slackNotify, "SUCCESSFUL","Build SUCCESSFUL. Version: ${currentDate}")
        }
    }

    options {
        buildDiscarder(logRotator(daysToKeepStr: '60', numToKeepStr: '30'))
        gitLabConnection('gitlab')
        timestamps()
        ansiColor('xterm')
    }

    parameters {
        string(      name: 'BRANCH',               defaultValue: 'feature_tg_7975',             description: 'branch')
        string(      name: 'VERSION',              defaultValue: '1.2.5',           description: 'version')
    }

    stages {
        stage ('create environment') {
            steps {
                cleanWs()
                script {
                    def scmVars = checkout([ $class: 'GitSCM',
                        branches: [[name: '$BRANCH']],
                        doGenerateSubmoduleConfigurations: false,
                        extensions: [], submoduleCfg: [],
                        userRemoteConfigs: [[credentialsId: '',
                        url: 'http://gitlab+deploy-token-3:LD2jHQCWDYSEt-8AJQzs@gitlab.bazalt.team/vdi/veil_connect.git']]
                    ])
                    env.GIT_COMMIT = scmVars.GIT_COMMIT
                    env.GIT_BRANCH = scmVars.GIT_BRANCH
                }
                        
                println "scmVars - $scmVars"
                println "GIT_COMMIT - $env.GIT_COMMIT"
                println "GIT_BRANCH - $env.GIT_BRANCH"
                println "env.DATE - ${currentDate}"
            }
        }
        
        stage ('build') {
            steps {
                bat script: '''
                    mkdir build
                    cd build
                    cmake -DCMAKE_BUILD_TYPE=Release -G "MinGW Makefiles" ..
                    mingw32-make
                '''
            }
        }
        
        stage ('copy dependencies') {
            steps {
                bat script: '''
                    copy doc\\veil_connect.ico build
                    cd build
                    rmdir /S /Q CMakeFiles
                    del CMakeCache.txt cmake_install.cmake Makefile libthin_client_veil.dll.a start_vdi_client.sh
        
                    mkdir lib\\gdk-pixbuf-2.0\\2.10.0\\loaders log share
                    copy C:\\msys32\\mingw64\\lib\\gdk-pixbuf-2.0\\2.10.0\\loaders\\*.dll lib\\gdk-pixbuf-2.0\\2.10.0\\loaders
                    copy C:\\msys32\\mingw64\\lib\\gdk-pixbuf-2.0\\2.10.0\\loaders.cache lib\\gdk-pixbuf-2.0\\2.10.0
                    xcopy C:\\msys32\\mingw64\\lib\\gio lib\\gio /E /H /I
        
                    for %%I in (libgstapp libgstaudioconvert libgstaudiorate libgstaudioresample libgstautodetect libgstcoreelements ^
                    libgstcoretracers libgstdirectsound) do (echo D|xcopy C:\\msys32\\mingw64\\lib\\gstreamer-1.0\\%%I.dll lib\\gstreamer-1.0)
                            
                    copy rdp_data\\rdp_template_file.txt rdp_data\\rdp_file.rdp
                            
                    xcopy C:\\msys32\\mingw64\\share\\glib-2.0 share\\glib-2.0 /E /H /I
                    xcopy C:\\msys32\\mingw64\\share\\icons share\\icons /E /H /I /Q
                            
                    for %%I in (freerdp2 freerdp-client2 winpr2 winpr-tools2) do (copy C:\\job\\FreeRDP-2.0.0-rc4\\Release\\%%I.dll)
        
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
                '''
            }
        }
        
        stage ('make installer') {
            steps {
                bat script: '''
                    sed -i -e "s:&&VER&&:%VERSION%:g" %WORKSPACE%/devops\\inno_setup_script\\veil_connect_installer.iss
                    iscc "%WORKSPACE%/devops\\inno_setup_script\\veil_connect_installer.iss"
                '''
            }
        }
        
        stage ('deploy to repo') {
            steps {
                bat script: '''
                    ssh uploader@mothership.bazalt.team rm -f /local_storage/veil_connect/veil_connect_installer.exe
                    scp veil_connect_installer.exe uploader@mothership.bazalt.team:/local_storage/veil_connect/
                '''
            }
        }
    }
}

def notifyBuild(slackNotify, buildStatus, msg) {
    buildStatus =  buildStatus ?: 'SUCCESSFUL'

    def colorName = 'RED'
    def colorCode = '#FF0000'
    def subject = "${buildStatus}: Job '${env.JOB_NAME} [${env.BUILD_NUMBER}]'"
    def summary = "${subject} (${env.BUILD_URL})" + "\n"

    if (buildStatus == 'STARTED') {
        color = 'YELLOW'
        colorCode = '#FFFF00'
        summary += "${msg}"
    } else if (buildStatus == 'SUCCESSFUL') {
        color = 'GREEN'
        colorCode = '#00FF00'
        summary += "${msg}"
    } else {
        color = 'RED'
        colorCode = '#FF0000'
        summary += "${msg}"
    }

    if (slackNotify){
        slackSend (color: colorCode, message: summary)
    }
}