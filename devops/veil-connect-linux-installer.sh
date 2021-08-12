#!/bin/bash

REPO_URL="https://veil-update.mashtab.org/veil-connect/linux"

echo "Welcome to VeiL Connect installer! (sudo required)"
echo "Select your OS:"

echo "
    1. Debian 9
    2. Debian 10
    3. Ubuntu 16.04
    4. Ubuntu 18.04
    5. Ubuntu 20.04
    6. Centos 7
    7. Centos 8
    8. Astra Linux Orel 2.12
    9. Astra Linux Smolensk 1.6
    10. Alt Linux 9
"

echo "My OS is:"
read OS

case $OS in
    1)
        apt-get update && apt-get install apt-transport-https wget lsb-release -y
        wget -qO - https://veil-update.mashtab.org/veil-repo-key.gpg | apt-key add -
        # add stretch-backports repo (for freerdp 2.0)
        echo "deb http://deb.debian.org/debian stretch-backports main" | tee /etc/apt/sources.list.d/stretch-backports.list
        echo "deb [arch=amd64] $REPO_URL/apt $(lsb_release -cs) main" | tee /etc/apt/sources.list.d/veil-connect.list
        apt-get update && apt-get install veil-connect -y
        rm -f /etc/apt/sources.list.d/stretch-backports.list && apt-get update
        ;;
    2|3|4|5)
        apt-get update && apt-get install apt-transport-https wget lsb-release -y
        wget -qO - https://veil-update.mashtab.org/veil-repo-key.gpg | apt-key add -
        echo "deb [arch=amd64] $REPO_URL/apt $(lsb_release -cs) main" | tee /etc/apt/sources.list.d/veil-connect.list
        apt-get update && apt-get install veil-connect -y
        ;;
    6|7)
        tee /etc/yum.repos.d/veil-connect.repo <<EOF
[veil-connect]
name=VeiL Connect repository
baseurl=$REPO_URL/yum/el\$releasever/\$basearch
gpgcheck=1
gpgkey=$REPO_URL/yum/RPM-GPG-KEY-veil-connect
enabled=1
EOF

        yum install epel-release -y
        yum install veil-connect -y
        ;;
    8)
        apt-get update && apt-get install apt-transport-https wget -y
        wget -qO - https://veil-update.mashtab.org/veil-repo-key.gpg | apt-key add -
        echo "deb [arch=amd64] $REPO_URL/apt bionic main" | tee /etc/apt/sources.list.d/veil-connect.list
        apt-get update && apt-get install veil-connect -y
        ;;
    9)
        echo "Please visit https://veil.mashtab.org/docs/vdi/connect/how_to/install for info" ;;
    10)
        apt-get update && apt-get install wget -y
        wget https://veil-update.mashtab.org/veil-connect/linux/apt-rpm/x86_64/RPMS.alt9/veil-connect-latest.rpm
        apt-get install ./veil-connect-latest.rpm -y
        rm -f veil-connect-latest.rpm
        ;;
    
    *)
        echo "Error: Empty OS" ;;
esac
