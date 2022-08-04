def gitCheckout(String branchName) {
    cleanWs()
    checkout([ $class: 'GitSCM',
        branches: [[name: branchName]],
        doGenerateSubmoduleConfigurations: false,
        extensions: [], submoduleCfg: [],
        userRemoteConfigs: [[credentialsId: 'jenkins-veil-connect-token',
        url: 'http://gitlab.bazalt.team/vdi/veil-connect.git']]
    ])
}

def prepareBuildImage() {
    sh script: '''
        echo -n $NEXUS_CREDS_PSW | docker login -u $NEXUS_CREDS_USR --password-stdin $NEXUS_DOCKER_REGISTRY
        docker pull $DOCKER_IMAGE_NAME-$DISTR:latest || true
        docker build -f devops/docker/Dockerfile.$DISTR . --pull --cache-from $DOCKER_IMAGE_NAME-$DISTR:latest --tag $DOCKER_IMAGE_NAME-$DISTR:$VERSION
        docker push $DOCKER_IMAGE_NAME-$DISTR:$VERSION
        docker tag $DOCKER_IMAGE_NAME-$DISTR:$VERSION $DOCKER_IMAGE_NAME-$DISTR:latest
        docker push $DOCKER_IMAGE_NAME-$DISTR:latest
    '''
}

def buildDebPackage() {
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
        dpkg-deb -Zxz -b root .
    '''
}

def buildRpmPackage() {
    sh script: '''
        mkdir build-${DISTR}
        cd build-${DISTR}
        cmake -DCMAKE_BUILD_TYPE=Release ../
        make
        rm -rf CMakeCache.txt  CMakeFiles  Makefile  cmake_install.cmake
        cd ${WORKSPACE}

        # make installer
        mkdir -p rpmbuild-${DISTR}/{BUILD,BUILDROOT,RPMS,SOURCES,SPECS,SRPMS}
        cp devops/rpm/veil-connect-${SPEC}.spec rpmbuild-${DISTR}/SPECS
        sed -i -e "s:%%VER%%:${VERSION}:g" rpmbuild-${DISTR}/SPECS/veil-connect-${SPEC}.spec
        mkdir -p rpmbuild-${DISTR}/BUILD/opt/veil-connect
        mkdir -p rpmbuild-${DISTR}/BUILD/usr/share/applications
        cp -r build-${DISTR}/* doc/veil-connect.ico rpmbuild-${DISTR}/BUILD/opt/veil-connect
        cp doc/veil-connect.desktop rpmbuild-${DISTR}/BUILD/usr/share/applications
        cd rpmbuild-${DISTR}
        rpmbuild --define "_topdir `pwd`" --define "_tmppath %{_topdir}/tmp" -v -bb SPECS/veil-connect-${SPEC}.spec
    '''
}

def deployToAptly() {
    sh script: '''
        APT_SRV="192.168.11.118"
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

def deployToRpmRepo() {
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

def deployToAptRpmRepo() {
    withCredentials([sshUserPrivateKey(credentialsId: 'uploader_ssh_key.id_rsa', keyFileVariable: 'SSH_KEY')]) {
        sh script: '''
            ssh -o StrictHostKeyChecking=no -i $SSH_KEY uploader@192.168.10.144 "mkdir -p /local_storage/veil-connect/linux/apt-rpm/${ARCH}/RPMS.${DISTR}"
            scp -o StrictHostKeyChecking=no -i $SSH_KEY ${WORKSPACE}/rpmbuild-${DISTR}/RPMS/${ARCH}/${PRJNAME}-${VERSION}*.rpm uploader@192.168.10.144:/local_storage/veil-connect/linux/apt-rpm/${ARCH}/RPMS.${DISTR}/
            ssh -o StrictHostKeyChecking=no -i $SSH_KEY uploader@192.168.10.144 "cd /local_storage/veil-connect/linux/apt-rpm/${ARCH}/RPMS.${DISTR}; ln -sf ${PRJNAME}-${VERSION}*.rpm ${PRJNAME}-latest.rpm"
        '''
    }
}