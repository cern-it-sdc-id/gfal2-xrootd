# unversionned doc dir F20 change https://fedoraproject.org/wiki/Changes/UnversionedDocdirs
%{!?_pkgdocdir: %global _pkgdocdir %{_docdir}/%{name}-%{version}}

Name:           gfal2-plugin-xrootd
Version:        0.4.0
Release:        1%{?dist}
Summary:        Provide xrootd support for GFAL2

Group:          Applications/Internet
License:        ASL 2.0
URL:            http://dmc.web.cern.ch/projects/gfal-2/home
# git clone https://gitlab.cern.ch/dmc/gfal2-plugin-xrootd.git gfal2-plugin-xrootd-0.4.0
# pushd gfal2-plugin-xrootd-0.4.0
# git checkout v0.4.0
# git submodule init && git submodule update
# popd
# tar czf gfal2-plugin-xrootd-0.4.0.tar.gz gfal2-plugin-xrootd-0.4.0
Source0:        %{name}-%{version}.tar.gz
BuildRoot:      %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

%if %{?fedora}%{!?fedora:0} >= 10 || %{?rhel}%{!?rhel:0} >= 6
BuildRequires:  boost-devel >= 1.41.0
%else
BuildRequires:  boost141-devel
%endif
BuildRequires:  cmake
BuildRequires:  gfal2-devel >= 2.9.1
BuildRequires:  xrootd-client-devel >= 1:4.1.1

Requires:       gfal2 >= 2.9.1

%description
The Grid File Access Library, GFAL2, provides a simple POSIX-like API for file
operations in grid and cloud environments. Plug-ins are available to allow
access via a variety of protocols. This package contains a plugin for the
xrootd protocol (root://).

%global pkgdir gfal2-plugins

%prep
%setup -q

%build
%cmake \
-DCMAKE_INSTALL_PREFIX=/ \
-DDOC_INSTALL_DIR=%{_pkgdocdir} \
 . 

make %{?_smp_mflags}

%install
rm -rf %{buildroot}
make install DESTDIR=%{buildroot}

%clean
rm -rf %{buildroot}

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%files
%defattr(-,root,root,-)
%config(noreplace) %{_sysconfdir}/gfal2.d/xrootd_plugin.conf
%{_libdir}/%{pkgdir}/libgfal_plugin_xrootd.so
%dir %{_pkgdocdir}
%{_pkgdocdir}/*

%changelog
* Fri Apr 17 2015 Alejandro Alvarez Ayllon <aalvarez at cern.ch> - 0.4.0-1
- Update for new upstream release 0.4.0

* Tue Jan 27 2015 Petr Machata <pmachata@redhat.com> - 0.3.4-2
- Rebuild for boost 1.57.0

* Mon Jan 19 2015 Alejandro Alvarez Ayllon <aalvarez at cern.ch> - 0.3.4-1
- Update for new upstream release

* Sat Aug 16 2014 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 0.3.pre1-2
- Rebuilt for https://fedoraproject.org/wiki/Fedora_21_22_Mass_Rebuild

* Mon Jul 07 2014 Alejandro Alvarez Ayllon <aalvarez at cern.ch> - 0.3.pre1-1
- Update for upstream preview release, with fixes for xrootd 4 

* Sat Jun 07 2014 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 0.2.2-5
- Rebuilt for https://fedoraproject.org/wiki/Fedora_21_Mass_Rebuild

* Wed Aug 14 2013 Adrien Devresse <adevress at cern.ch>  - 0.2.3-4
 - unversionned documentation directory 

* Sat Aug 03 2013 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 0.2.2-3
- Rebuilt for https://fedoraproject.org/wiki/Fedora_20_Mass_Rebuild


* Wed May 08 2013 Adrien Devresse <adevress at cern.ch> - 0.2.2-2
 - First EPEL compatible version from review comments


