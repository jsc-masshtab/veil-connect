#!/bin/bash

REPO_URL="https://veil-update.mashtab.org/veil-connect/linux"

echo "Welcome to VeiL Connect installer! (sudo required)"
echo "Select your OS:"

echo "
    1. Debian 9
    2. Debian 10
    3. Ubuntu 18.04
    4. Ubuntu 20.04
    5. Centos 7
    6. Centos 8
    7. Astra Linux Orel 2.12
    8. Astra Linux Smolensk 1.6
"

echo "My OS is:"
read OS

case $OS in
    1)
        apt-get update && apt-get install apt-transport-https wget lsb-release -y
        wget -qO - https://veil-update.mashtab.org/veil-repo-key.gpg | apt-key add -
        # add stretch-backports repo (for freerdp 2.0)
        echo "deb http://deb.debian.org/debian stretch-backports main" | tee /etc/apt/sources.list.d/stretch-backports.list
        echo "deb $REPO_URL/apt $(lsb_release -cs) main" | tee /etc/apt/sources.list.d/veil-connect.list
        apt-get update && apt-get install veil-connect -y
        rm -f /etc/apt/sources.list.d/stretch-backports.list && apt-get update
        ;;
    2|3|4)
        apt-get update && apt-get install apt-transport-https wget lsb-release -y
        wget -qO - https://veil-update.mashtab.org/veil-repo-key.gpg | apt-key add -
        echo "deb $REPO_URL/apt $(lsb_release -cs) main" | tee /etc/apt/sources.list.d/veil-connect.list
        apt-get update && apt-get install veil-connect -y
        ;;
    5|6)
        tee /etc/yum.repos.d/veil-connect.repo <<EOF
[veil-connect]
name=VeiL Connect repository
baseurl=$REPO_URL/yum/el\$releasever/\$basearch
gpgcheck=1
gpgkey=$REPO_URL/yum/RPM-GPG-KEY-veil-connect
enabled=1
EOF

        yum install veil-connect -y
        ;;
    7)
        apt-get update && apt-get install apt-transport-https wget -y
        wget -qO - https://veil-update.mashtab.org/veil-repo-key.gpg | apt-key add -
        echo "deb $REPO_URL/apt bionic main" | tee /etc/apt/sources.list.d/veil-connect.list
        apt-get update && apt-get install veil-connect -y
        ;;
    8)
        echo "Please visit https://veil.mashtab.org/docs/vdi/connect/how_to/install for info" ;;
    *)
        echo "Error: Empty OS" ;;
esac