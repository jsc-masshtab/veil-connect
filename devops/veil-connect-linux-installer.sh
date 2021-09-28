#!/bin/bash

TITLE="VeiL Connect installer"
HEIGHT=17
WIDTH=82
HELLO="Welcome to VeiL Connect installer"
ERROR="ERROR: VeiL Connect installer must be run as root!"
SUCSESS="Installation of VeiL Connect was successful!"
ERROR_INSTALL="ERROR: An error occurred during the installation of the program!"

REPO_URL="https://veil-update.mashtab.org/veil-connect/linux"

which dialog > /dev/null && WINDOW=dialog; which whiptail > /dev/null && WINDOW=whiptail

if [ -n "$WINDOW" ]; then
    if [ "$EUID" -ne 0 ]; then
        $WINDOW --title  "$TITLE" --msgbox "$ERROR" $HEIGHT $WIDTH  2>/dev/null
        clear 2> /dev/null || :
        exit 1
    fi

    $WINDOW --title  "$TITLE" --msgbox "$HELLO" $HEIGHT $WIDTH  2>/dev/null

    clear 2> /dev/null || :

    # if dialog exists
    OS=$($WINDOW --title  "$TITLE" --menu  "Select your OS:" $HEIGHT $WIDTH 12 \
    "1" "Debian 9" \
    "2" "Debian 10" \
    "3" "Ubuntu 16.04" \
    "4" "Ubuntu 18.04" \
    "5" "Ubuntu 20.04" \
    "6" "Centos 7" \
    "7" "Centos 8" \
    "8" "Astra Linux Orel 2.12" \
    "9" "Astra Linux Smolensk 1.6" \
    "10" "Alt Linux 9" \
    "11" "RedOS 7.2" \
    "12" "Redos 7.3" 3>&1 1>&2 2>&3)
    

    clear 2> /dev/null || :

    if [ "$?" -ne 0 ]; then
        exit 1
    fi
else
    echo "Welcome to VeiL Connect installer! (sudo required)"
    echo "Select your OS:"
        
    echo "
        1.  Debian 9
        2.  Debian 10
        3.  Ubuntu 16.04
        4.  Ubuntu 18.04
        5.  Ubuntu 20.04
        6.  Centos 7
        7.  Centos 8
        8.  Astra Linux Orel 2.12
        9.  Astra Linux Smolensk 1.6
        10. Alt Linux 9
        11. RedOS 7.2 
        12. Redos 7.3
    "
    echo "My OS is:"
    read OS
fi

case $OS in
    1)
        apt-get update && apt-get install apt-transport-https wget lsb-release gnupg -y
        wget -qO - https://veil-update.mashtab.org/veil-repo-key.gpg | apt-key add -
        # add stretch-backports repo (for freerdp 2.0)
        echo "deb http://deb.debian.org/debian stretch-backports main" | tee /etc/apt/sources.list.d/stretch-backports.list
        echo "deb [arch=amd64] $REPO_URL/apt $(lsb_release -cs) main" | tee /etc/apt/sources.list.d/veil-connect.list
        apt-get update && apt-get install veil-connect -y
        result="$?"
        rm -f /etc/apt/sources.list.d/stretch-backports.list && apt-get update
        ;;
    2|3|4|5)
        apt-get update && apt-get install apt-transport-https wget lsb-release gnupg -y
        wget -qO - https://veil-update.mashtab.org/veil-repo-key.gpg | apt-key add -
        echo "deb [arch=amd64] $REPO_URL/apt $(lsb_release -cs) main" | tee /etc/apt/sources.list.d/veil-connect.list
        apt-get update && apt-get install veil-connect -y
        result="$?"
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
        result="$?"
        ;;
    8)
        apt-get update && apt-get install apt-transport-https wget gnupg -y
        wget -qO - https://veil-update.mashtab.org/veil-repo-key.gpg | apt-key add -
        echo "deb [arch=amd64] $REPO_URL/apt bionic main" | tee /etc/apt/sources.list.d/veil-connect.list
        apt-get update && apt-get install veil-connect -y
        result="$?"
        ;;
    9)
        if [ -n "$WINDOW" ]; then
            $WINDOW --title  "$TITLE" --msgbox "Please visit https://veil.mashtab.org/docs/vdi/connect/how_to/install for the more info" $HEIGHT $WIDTH  2>/dev/null
        else
            echo "Please visit https://veil.mashtab.org/docs/vdi/connect/how_to/install for info" 
        fi
        exit 1
        ;;
    10)
        apt-get update && apt-get install wget -y
        wget https://veil-update.mashtab.org/veil-connect/linux/apt-rpm/x86_64/RPMS.alt9/veil-connect-latest.rpm
        apt-get install ./veil-connect-latest.rpm -y
        result="$?"
        rm -f veil-connect-latest.rpm
        ;;
    11) tee /etc/yum.repos.d/veil-connect.repo <<EOF
[veil-connect]
name=VeiL Connect repository
baseurl=$REPO_URL/yum/el7/\$basearch
gpgcheck=1
gpgkey=$REPO_URL/yum/RPM-GPG-KEY-veil-connect
enabled=1
EOF

        yum install veil-connect -y
        result="$?"
        ;;

    12) tee /etc/yum.repos.d/veil-connect.repo <<EOF
[veil-connect]
name=VeiL Connect repository
baseurl=$REPO_URL/yum/redos7.3/\$basearch
gpgcheck=1
gpgkey=$REPO_URL/yum/RPM-GPG-KEY-veil-connect
enabled=1
EOF

        dnf install veil-connect -y
        result="$?"
        ;;
    
    *)
        echo "Error: Empty OS" ;;
esac

if [ "$result" -eq 0 ]; then
        echo -e "\n$TITLE\n\n$SUCSESS"
    else
        echo -e "\n$TITLE\n\n$ERROR_INSTALL"
fi