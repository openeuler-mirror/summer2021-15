#
# spec file for package ImageWriter
#
# Copyright (c) 2021 Hollow Man
#
# All modifications and additions to the file contributed by third parties
# remain the property of their copyright owners, unless otherwise agreed
# upon. The license for this file, and modifications and additions to the
# file, is the same license as for the pristine package itself (unless the
# license for the pristine package is not an Open Source License, in which
# case the license is the MIT License). An "Open Source License" is a
# license that conforms to the Open Source Definition (Version 1.9)
# published by the Open Source Initiative.

# Please submit bugfixes or comments via https://gitee.com/openeuler-competition/summer2021-15/issues
#


%global debug_package %{nil}
%global filename 210010031-master
%global forgeurl https://gitlab.summer-ospp.ac.cn/summer2021/210010031
Name:           ImageWriter
Version:        1.0.0
Release:        0
Summary:        Creating bootable USB flash disks
License:        GPL-3.0-or-later
URL:            %{forgeurl}
Source0:        %{forgeurl}/-/archive/master/%{filename}.tar.gz
Requires:       systemd-devel notify-send
BuildRequires:  gcc-c++ make systemd-devel qt5-devel

%description
Creating bootable USB flash disks

%prep
%setup -q -n %{filename}

%build
qmake-qt5 %{name}.pro
%make_build BINDIR=%{_bindir}

%install
mkdir -p %{buildroot}/%{_bindir}
mkdir -p %{buildroot}/%{_datadir}/applications
mkdir -p %{buildroot}/%{_datadir}/qt5/translations
mkdir -p %{buildroot}/%{_datadir}/locale/zh_CN/LC_MESSAGES
mkdir -p %{buildroot}/%{_datadir}/icons/hicolor/scalable/apps
make install PREFIX=%{buildroot}%{_prefix} INSTALL_ROOT=%{buildroot}
%find_lang verifyimagewriter

%files -f verifyimagewriter.lang
%doc README.md
%{_bindir}/*
%{_datadir}/qt5/translations/*
%{_datadir}/icons/hicolor/scalable/apps/*
%{_datadir}/applications/imagewriter.desktop

%changelog
* Tue Sep 14 2021 Hollow Man <hollowman@hollowman.ml> - 1.0.0
- Initial openEuler package.
