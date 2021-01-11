#!/bin/bash

ABSOLUTE_FILENAME=`readlink -e "$0"`
DIRECTORY=`dirname "$ABSOLUTE_FILENAME"`
cd $DIRECTORY

echo $1 | sudo -S apt-get --yes update || true
echo $1 | sudo -S apt-get --yes install $2

