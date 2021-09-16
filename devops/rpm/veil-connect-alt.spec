Name: veil-connect
Version: %%VER%%
Release: 0%{?dist}
Summary: VeiL Connect Thin Client

Group: Network
Packager: Research institute Mashtab
License: GPL
URL: https://mashtab.org/

Requires: libspice-gtk3
Requires: libfreerdp
Requires: libjson-glib
Requires: libsoup
Requires: libxml2
Requires: libhiredis-devel
Requires: libusb
Requires: libusbredir

%description
VeiL Connect Thin Client

%prep

%build

%install
mkdir -p %{buildroot}/
cp -r ./* %{buildroot}/

%post
perl -pi -e 's/crosshair/default\0\0/g' /usr/lib64/libspice-client-gtk-3.0.so.5 || true

%files
%dir %attr (777,root,root) /opt/veil-connect
%dir %attr (777,root,root) /opt/veil-connect/locale
%dir %attr (777,root,root) /opt/veil-connect/x2go_data/
%attr (777,root,root) /opt/veil-connect/css_style.css
%attr (777,root,root) /opt/veil-connect/locale/ru/LC_MESSAGES/thin_client_veil.gmo
%attr (777,root,root) /opt/veil-connect/locale/ru/LC_MESSAGES/thin_client_veil.mo
%attr (777,root,root) /opt/veil-connect/start_vdi_client.sh
%attr (777,root,root) /opt/veil-connect/veil_connect
%attr (777,root,root) /opt/veil-connect/veil-connect.ico
%attr (644,root,root) /usr/share/applications/veil-connect.desktop
%attr (777,root,root) /opt/veil-connect/update_script.sh
%attr (777,root,root) /opt/veil-connect/x2go_data/x2go_sessions

%changelog

%define  debug_package %{nil}