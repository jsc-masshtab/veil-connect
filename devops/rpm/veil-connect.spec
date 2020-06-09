Name: veil-connect
Version: %%VER%%
Release: 1%{?dist}
Summary: Veil Connect Thin Client

Group: Network
Packager: JSC Research institute Masshtab
License: GPL
URL: https://mashtab.org/

Requires: epel-release
Requires: spice-gtk3-devel
Requires: freerdp-libs
Requires: freerdp-devel
Requires: json-glib-devel
Requires: libsoup-devel
Requires: libwinpr
Requires: libxml2-devel
Requires: gtk3-devel
Requires: hiredis-devel

%description
Veil Connect Thin Client

%prep

%build

%install
mkdir -p %{buildroot}/
cp -r ./* %{buildroot}/

%postun
rm -rf /opt/veil-connect

%files
%dir %attr (777,root,root) /opt/veil-connect
%dir %attr (777,root,root) /opt/veil-connect/locale
%attr (777,root,root) /opt/veil-connect/css_style.css
%attr (777,root,root) /opt/veil-connect/locale/ru/LC_MESSAGES/thin_client_veil.gmo
%attr (777,root,root) /opt/veil-connect/locale/ru/LC_MESSAGES/thin_client_veil.mo
%attr (777,root,root) /opt/veil-connect/start_vdi_client.sh
%attr (777,root,root) /opt/veil-connect/veil_connect
%attr (777,root,root) /opt/veil-connect/veil-connect.ico
%attr (644,root,root) /usr/share/applications/veil-connect.desktop

%changelog