#!/bin/bash


ABSOLUTE_FILENAME=`readlink -e "$0"`
DIRECTORY=`dirname "$ABSOLUTE_FILENAME"`
cd $DIRECTORY
echo
echo "$1"


if [ $1 == "apt_update" ]
then
  echo $2 | sudo -S apt-get --yes update || true
elif [ $1 == "apt_install" ]
then
  echo $2 | sudo -S apt-get --yes install $3
elif [ $1 == "yum_update" ]
then
  echo $2 | sudo -S yum -y makecache
elif [ $1 == "yum_install" ]
then
  echo $2 | sudo -S yum -y update $3
elif [ $1 == "check_sudo_pass" ]
then
  echo $2 | sudo -S echo "success_pass"
else
  echo "Wrong option"
fi
