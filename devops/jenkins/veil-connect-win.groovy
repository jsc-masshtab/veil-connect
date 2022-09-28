library "veil-connect-libraries@$BRANCH"

def branch = buildParameters.branch()
def version = buildParameters.version()
def agents32 = buildParameters.win32Agents()
def agents64 = buildParameters.win64Agents()

notifyBuild("STARTED")

pipeline {
    agent {
        label "master"
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
        string(      name: 'BRANCH',         defaultValue: branch,     description: 'branch')
        string(      name: 'VERSION',        defaultValue: version,    description: 'version')
        choice(      name: 'WIN32_AGENT',    choices: agents32,        description: 'jenkins build agent')
        choice(      name: 'WIN64_AGENT',    choices: agents64,        description: 'jenkins build agent')
        booleanParam(name: 'WIN32',          defaultValue: true,       description: 'create EXE?')
        booleanParam(name: 'WIN64',          defaultValue: true,       description: 'create EXE?')
    }

    stages {
        stage ('checkout') {
            steps {
                script {
                    buildSteps.gitCheckout("$BRANCH")
                }
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

                            mkdir lib\\gstreamer-1.0
                            copy C:\\msys32\\mingw32\\lib\\gstreamer-1.0\\*.dll lib\\gstreamer-1.0

                            copy rdp_data\\rdp_template_file.txt rdp_data\\rdp_file.rdp

                            xcopy C:\\msys32\\mingw32\\share\\glib-2.0 share\\glib-2.0 /E /H /I
                            xcopy C:\\msys32\\mingw32\\share\\icons share\\icons /E /H /I /Q

                            for %%I in (freerdp2 freerdp-client2 winpr2 winpr-tools2) do (copy C:\\job\\FreeRDP-2.0.0-rc4\\Release\\%%I.dll)

                            copy C:\\msys32\\mingw32\\bin\\*.dll

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

                            mkdir lib\\gstreamer-1.0
                            copy C:\\msys32\\mingw64\\lib\\gstreamer-1.0\\*.dll lib\\gstreamer-1.0

                            copy rdp_data\\rdp_template_file.txt rdp_data\\rdp_file.rdp

                            xcopy C:\\msys32\\mingw64\\share\\glib-2.0 share\\glib-2.0 /E /H /I
                            xcopy C:\\msys32\\mingw64\\share\\icons share\\icons /E /H /I /Q

                            copy C:\\msys32\\mingw64\\bin\\*.dll

                            for %%I in (gspawn-win64-helper.exe gspawn-win64-helper-console.exe ^
                            gdbus.exe) do (copy C:\\msys32\\mingw64\\bin\\%%I)

                            copy C:\\job\\openh264-6\\openh264-6.dll

                            for %%I in (libfreerdp2 libfreerdp-client2 libwinpr2 libwinpr-tools2 avcodec-58 avutil-56 swresample-3 ^
                            swscale-5) do (copy C:\\job\\FreeRDP-2.8.0\\Release\\%%I.dll)

                            perl -pi -e 's/crosshair/default\\0\\0/g' libspice-client-gtk-3.0-5.dll

                            REM ### Add vcredist to installer ###
                            curl http://nexus.bazalt.team/repository/files/vcredist/VC_redist.x64.exe --output vc_redist.exe

                            REM ### Add UsbDk to installer ###
                            curl http://mothership.bazalt.team/files/usbdk/UsbDk_1.0.22_x64.msi --output usbdk.msi

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