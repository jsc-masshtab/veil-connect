# VeiL Connect
## Таблица соответствия сборок с ОС

| ОС                 | Пакет   |
|--------------------|---------|
| Astra 1.6 Smolensk | bionic  |
| Astra 2.12 Orel    | bionic  |
| Debian 9           | stretch |
| Debian 10          | buster  |
| Ubuntu 18.04       | bionic  |
| Ubuntu 20.04       | focal   |
| Centos 7           |  el7    |
| Centos 8           |  el8    |


## Building on Debian 9
```
echo "deb http://deb.debian.org/debian stretch-backports main" | tee /etc/apt/sources.list.d/stretch-backports.list
apt-get update
apt-get install -y -q libhiredis-dev \
libspice-client-gtk-3.0-dev libjson-glib-dev libxml2-dev libsoup2.4-dev \
freerdp2-dev gcc cmake pkg-config libusb-1.0-0-dev libusbredirparser-dev

mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ../
make
```
## Building on Debian 10 and Ubuntu 18.04 / 20.04
```
apt-get install libhiredis-dev libspice-client-gtk-3.0-dev libjson-glib-dev \
libxml2-dev libsoup2.4-dev freerdp2-dev gcc cmake pkg-config libusb-1.0-0-dev \
libusbredirparser-dev

mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ../
make
```

## Building on Centos 7
```
yum install -y epel-release
yum install -y rpmdevtools spice-gtk3-devel freerdp-devel json-glib-devel libsoup-devel \
libxml2-devel gcc cmake3 make gtk3-devel hiredis-devel usbredir-devel libusb-devel

mkdir build
cd build
cmake3 -DCMAKE_BUILD_TYPE=Release ../
make
rm -rf CMakeCache.txt  CMakeFiles  Makefile  cmake_install.cmake
```

## Building on Centos 8
```
yum install -y epel-release dnf-plugins-core
yum config-manager --set-enabled powertools
yum install -y rpmdevtools spice-gtk3-devel freerdp-devel json-glib-devel libsoup-devel \
libxml2-devel gcc cmake3 make gtk3-devel hiredis-devel usbredir-devel libusb-devel

mkdir build
cd build
cmake3 -DCMAKE_BUILD_TYPE=Release ../
make
rm -rf CMakeCache.txt  CMakeFiles  Makefile  cmake_install.cmake
```

## Building on Windows

MSYS2 installation


The MSYS2 project provides a UNIX-like development environment for Windows. It provides packages for many software applications and libraries, including the GTK stack. If you prefer developing using Visual Studio, you may be better off installing GTK from vcpkg instead.


In MSYS2 packages are installed using the pacman package manager.


Note: in the following steps, we will assume you're using a 64-bit Windows. Therefore, the package names include the x86_64 architecture identifier. If you're using a 32-bit Windows, please adapt the instructions below using the i686 architecture identifier.

Step 1: Install MSYS2


Download the MSYS2 installer that matches your platform and follow the installation instructions.

Step 2: Install GTK3 and its dependencies


Open a MSYS2 shell, and run:

pacman -S mingw-w64-x86_64-gtk3


Install build tools

pacman -S mingw-w64-x86_64-toolchain base-devel


Install dependencies:
pacman -S mingw64/mingw-w64-x86_64-libsoup

pacman -S mingw64/mingw-w64-x86_64-spice-gtk

pacman -S mingw64/mingw-w64-x86_64-libusb

In file CmaleLists.txt in variable LIBS_INCLUDE_PATH set the path to MSYS_INCLUDE


Сборка freerdp

-Скачать freerdp https://github.com/FreeRDP/FreeRDP/releases/tag/2.0.0-rc4 распаковать.

freerdp на винде возможно собрать только с помощью нативного компилятора (visual C) 

-Ставим visual studio https://visualstudio.microsoft.com/ru/downloads/
Файл FreeRDP.sln открываем в студии. Выбираем режим Release и собираем. В папке freerdp

будет создана папка Release с собранными либами
В файле CmaleLists.txt в переменной FREERDP_PATH указываем путь к папке freerdp
 Install Clion


In Clion open CmakeListx.txt and build the project



## INSTALL AND LAUNCH

Windows
Запустить инсталятор, следовать инструкциям. 

Linux
Установить deb пакет.
