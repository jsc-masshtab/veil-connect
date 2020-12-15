# Инструкция по сборке veil-connect-embedded
## Подготовка
0. Сборка производится в ОС Ubuntu 18.04
1. Дополнительно к ОС должны быть установлены следующие пакеты:
```
spice-client-gtk
libspice-client-gtk-3.0-dev
libjson-glib-dev
libxml2-dev
libsoup2.4-dev
freerdp2-dev
libfreerdp-client2-2
libwinpr2-2
libwinpr2-dev
gcc
cmake
pkg-config
libusb-1.0-0-dev
libusbredirparser-dev
```
Выполнить установку этих пакетов:
```
apt update
apt install -y spice-client-gtk libspice-client-gtk-3.0-dev libjson-glib-dev libxml2-dev libsoup2.4-dev freerdp2-dev libfreerdp-client2-2 libwinpr2-2 libwinpr2-dev gcc cmake pkg-config libusb-1.0-0-dev libusbredirparser-dev
```
2. Создать рабочий каталог, в который будут скопированы исходные тексты, с помощью команды:
```
mkdir /tmp/veil-connect
```
Вставить в устройство чтения компакт-дисков РМС компакт-диск с исходными текстами, смонтировать и скопировать содержимое диска в рабочий каталог с помощью команд:
```
mount /media/cdrom
cp -a /media/cdrom/* /tmp/veil-connect
```
Размонтировать компакт-диск с исходными текстами командой:
```
umount /media/cdrom
```
## Сборка
3. Выполнить команды для сборки бинарных файлов:
```
cd /tmp/veil-connect
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ../
make
rm -rf CMakeCache.txt  CMakeFiles  Makefile  cmake_install.cmake
```
Выполнить команды для сборки deb-пакета:
```
cd /tmp/veil-connect
mkdir -p devops/deb_embedded/root/opt/veil-connect
mkdir -p devops/deb_embedded/root/usr/share/applications
cp -r build/* doc/veil-connect.ico devops/deb_embedded/root/opt/veil-connect
cp doc/veil-connect.desktop devops/deb_embedded/root/usr/share/applications
sed -i -e "s:%%VER%%:1.3.6:g" devops/deb_embedded/root/DEBIAN/control
chmod -R 777 devops/deb_embedded/root
chmod -R 755 devops/deb_embedded/root/DEBIAN
sudo chown -R root:root devops/deb_embedded/root
cd devops/deb_embedded
dpkg-deb -b root .
```
4. Готовый deb-пакет будет находиться в каталоге: `/tmp/veil-connect/devops/deb_embedded`