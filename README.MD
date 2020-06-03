# Veil VDI Thin Cleint
## Таблица соответствия сборок с ОС

| ОС                 | Пакет  |
|--------------------|--------|
| Astra 1.6 Smolensk | bionic |
| Astra 2.12 Orel    | bionic |
| Debian 9           | bionic |
| Debian 10          | buster |
| Ubuntu 18.04       | bionic |
| Ubuntu 20.04       | buster |
| Centos 7           |    ?   |
| Centos 8           |    ?   |

## Building on Linux
1)Install followwing packages:

spice-client-gtk

libspice-client-gtk-3.0-dev

libjson-glib-dev

libxml2-dev

libsoup2.4-dev
freerdp2-dev (Разработка велась под версию 2.0.0~git202002281153-0)

libfreerdp-client2-2

libwinpr2-2

libwinpr2-dev
2)Install gcc, cmake, pkg-config
3)Open terminal in desktop-client-c directory and execute commands:

mkdir build

cd build

cmake -DCMAKE_BUILD_TYPE=Release ../

make

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

On Windows unpack archive vdi_client_release_win7.zip and just launch executable file

On Linux unpack archive vdi_client_release_Ubuntu_18_04_2_LTS.tar.xz and execute script start_vdi_client.sh
