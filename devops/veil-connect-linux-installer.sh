#!/bin/bash

echo "Welcome to VeiL Connect installer! (sudo required)"
echo "Select your OS:"

echo "
    1. Debian 9
    2. Debian 10
    3. Ubuntu 18.04
    4. Ubuntu 20.04
    5. Centos 7
    6. Centos 8
    7. Astra Linux Orel
    8. Astra Linux Smolensk
"

echo "My OS is:"
read OS

case $OS in
    1)
        # add stretch-backports repo (for freerdp 2.0)
        echo "deb http://deb.debian.org/debian stretch-backports main" | tee /etc/apt/sources.list.d/stretch-backports.list
        apt-get update
        apt-get -t stretch-backports install freerdp2-dev -y
        apt-get install ./veil-connect_*-stretch_amd64.deb -y
        rm -f /etc/apt/sources.list.d/stretch-backports.list
        apt-get update
        ;;
    2)
        # add buster-backports repo (for freerdp 2.2)
        echo "deb http://deb.debian.org/debian buster-backports main" | tee /etc/apt/sources.list.d/buster-backports.list
        apt-get update
        apt-get -t buster-backports install freerdp2-dev -y
        apt-get install ./veil-connect_*-buster_amd64.deb -y
        rm -f /etc/apt/sources.list.d/buster-backports.list
        apt-get update
        ;;
    3)
        apt-get update
        apt-get install software-properties-common -y
        # add remmina repo (for freerdp 2.2)
        add-apt-repository ppa:remmina-ppa-team/freerdp-daily -y
        apt-get install freerdp2-dev -y
        apt-get install ./veil-connect_*-bionic_amd64.deb -y
        ;;
    4)
        apt-get update
        apt-get install software-properties-common -y
        # add remmina repo (for freerdp 2.2)
        add-apt-repository ppa:remmina-ppa-team/freerdp-daily -y
        apt-get install freerdp2-dev -y
        apt-get install ./veil-connect_*-focal_amd64.deb -y
        ;;
    5)
        yum install epel-release -y
        yum install veil-connect-*.el7.x86_64.rpm -y
        ;;
    6)
        yum install epel-release -y
        yum install veil-connect-*.el8.x86_64.rpm -y
        ;;
    7)
        apt-get update
        dpkg -i freerdp2-astra-orel/*.deb
        apt-get install ./veil-connect_*-bionic_amd64.deb -y
        ;;
    8)
        dpkg -i debs-astra-smolensk/*.deb
        apt-get install ./veil-connect_*-bionic_amd64.deb -y
        ;;
    *)
        echo "Error: Empty OS" ;;
esac