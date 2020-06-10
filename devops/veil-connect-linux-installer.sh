#!/bin/bash

echo "Welcome to VeiL Connect installer!"
echo "Select your OS:"

echo "
    1. Debian 9
    2. Debian 10
    3. Ubuntu 18
    4. Ubuntu 19/20
    5. Centos 7
    6. Centos 8
    7. Astra Linux
"

echo "My OS is:"
read OS

case $OS in
    1|3|7)
        sudo apt update
        sudo apt install ./veil-connect_*-bionic_amd64.deb -y
        ;;
    2|4)
        sudo apt update
        sudo apt install ./veil-connect_*-buster_amd64.deb -y
        ;;
    5)
        sudo yum install epel-release -y
        sudo yum install veil-connect-*.el7.x86_64.rpm -y
        ;;
    6)
        sudo yum install epel-release -y
        sudo dnf --enablerepo=PowerTools install freerdp-devel -y
        sudo yum install veil-connect-*.el8.x86_64.rpm -y
        ;;
    *)
        echo "Error: Empty OS" ;;
esac