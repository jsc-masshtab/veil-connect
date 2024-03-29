#!/bin/bash

TITLE="VeiL Connect installer"
HEIGHT=18
WIDTH=82
HELLO="Welcome to VeiL Connect installer"
ERROR="ERROR: VeiL Connect installer must be run as root!"
SUCCESS="Installation of VeiL Connect was successful!"
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
    OS=$($WINDOW --title  "$TITLE" --menu  "Select your OS:" $HEIGHT $WIDTH 10 \
    "1" "Debian 9" \
    "2" "Debian 10" \
    "3" "Debian 11" \
    "4" "Ubuntu 16.04" \
    "5" "Ubuntu 18.04" \
    "6" "Ubuntu 20.04" \
    "7" "Ubuntu 22.04" \
    "8" "Centos 7" \
    "9" "Centos 8" \
    "10" "Astra Linux Orel 2.12" \
    "11" "Astra Linux Smolensk (SE)" \
    "12" "ALT Linux 9" \
    "13" "RedOS 7.2" \
    "14" "RedOS 7.3" \
    "15" "AlterOS 7" 3>&1 1>&2 2>&3)


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
        3.  Debian 11
        4.  Ubuntu 16.04
        5.  Ubuntu 18.04
        6.  Ubuntu 20.04
        7.  Ubuntu 22.04
        8.  Centos 7
        9.  Centos 8
        10. Astra Linux Orel 2.12
        11. Astra Linux Smolensk (SE)
        12. ALT Linux 9
        13. RedOS 7.2
        14. RedOS 7.3
        15. AlterOS 7
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
    2|3|4|5|6|7)
        apt-key del 4BD0CAA7
        apt-get update && apt-get install apt-transport-https wget lsb-release gnupg -y
        wget -O /usr/share/keyrings/veil-repo-key.gpg https://veil-update.mashtab.org/veil-repo-key.gpg
        echo "deb [arch=amd64 signed-by=/usr/share/keyrings/veil-repo-key.gpg] $REPO_URL/apt $(lsb_release -cs) main" | tee /etc/apt/sources.list.d/veil-connect.list
        apt-get update && apt-get install veil-connect -y
        result="$?"
        ;;
    8|9)
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
    10)
        apt-get update && apt-get install apt-transport-https wget gnupg -y
        wget -qO - https://veil-update.mashtab.org/veil-repo-key.gpg | apt-key add -
        echo "deb [arch=amd64] $REPO_URL/apt bionic main" | tee /etc/apt/sources.list.d/veil-connect.list
        apt-get update && apt-get install veil-connect -y
        result="$?"
        ;;
    11)
        if [ -n "$WINDOW" ]; then
            $WINDOW --title  "$TITLE" --msgbox "Please visit https://veil.mashtab.org/vdi-docs/connect/operator_guide/install/net/linux/smolensk/ for info" $HEIGHT $WIDTH  2>/dev/null
        else
            echo "Please visit https://veil.mashtab.org/vdi-docs/connect/operator_guide/install/net/linux/smolensk/ for info" 
        fi
        exit 1
        ;;
    12)
        apt-get update && apt-get install wget -y
        wget https://veil-update.mashtab.org/veil-connect/linux/apt-rpm/x86_64/RPMS.alt9/veil-connect-latest.rpm
        apt-get install ./veil-connect-latest.rpm -y
        result="$?"
        rm -f veil-connect-latest.rpm
        ;;
    13) tee /etc/yum.repos.d/veil-connect.repo <<EOF
[veil-connect]
name=VeiL Connect repository
baseurl=$REPO_URL/yum/el7/\$basearch
gpgcheck=1
gpgkey=$REPO_URL/yum/RPM-GPG-KEY-veil-connect
enabled=1
EOF

        yum install veil-connect freerdp-libs -y
        result="$?"
        ;;

    14) tee /etc/yum.repos.d/veil-connect.repo <<EOF
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
    
    15) tee /etc/yum.repos.d/veil-connect.repo <<EOF
[veil-connect]
name=VeiL Connect repository
baseurl=$REPO_URL/yum/alteros7/\$basearch
gpgcheck=1
gpgkey=$REPO_URL/yum/RPM-GPG-KEY-veil-connect
enabled=1
EOF

        # install centos 7 packages
        if [[ $(rpm -qa | grep freerdp-libs) != "freerdp-libs-2.1.1-2.el7.x86_64" ]]; then
            yum erase -y freerdp-libs
            yum install -y http://mirror.centos.org/centos/7/os/x86_64/Packages/libwinpr-2.1.1-2.el7.x86_64.rpm \
                           http://mirror.centos.org/centos/7/os/x86_64/Packages/freerdp-libs-2.1.1-2.el7.x86_64.rpm
        fi

        if [[ $(rpm -qa | grep hiredis) != "hiredis-0.12.1-2.el7.x86_64" ]]; then
            yum install -y https://download-ib01.fedoraproject.org/pub/epel/7/x86_64/Packages/h/hiredis-0.12.1-2.el7.x86_64.rpm
        fi

        # install veil-connect
        yum install -y veil-connect
        result="$?"
        ;;

    *)
        echo "Error: Empty OS" ;;
esac

if [ "$result" -eq 0 ]; then
        echo -e "\n$TITLE\n\n$SUCCESS"
    else
        echo -e "\n$TITLE\n\n$ERROR_INSTALL"
fi