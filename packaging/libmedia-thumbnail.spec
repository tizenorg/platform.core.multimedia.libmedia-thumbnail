Name:       libmedia-thumbnail
Summary:    Media thumbnail service library for multimedia applications
Version: 0.1.94
Release:    0
Group:      Multimedia/Libraries
License:    Apache-2.0 and public domain
Source0:    %{name}-%{version}.tar.gz
Source1001: %{name}.manifest
Source1002: %{name}-devel.manifest
Source1003: media-thumbnail-server.manifest

Requires: media-server
BuildRequires: cmake
BuildRequires: pkgconfig(dlog)
BuildRequires: pkgconfig(mm-fileinfo)
BuildRequires: pkgconfig(mmutil-imgp)
BuildRequires: pkgconfig(mmutil-jpeg)
BuildRequires: pkgconfig(libexif)
BuildRequires: pkgconfig(evas)
BuildRequires: pkgconfig(ecore)
BuildRequires: pkgconfig(aul)
BuildRequires: pkgconfig(vconf)
BuildRequires: pkgconfig(libmedia-utils)
BuildRequires: pkgconfig(libtzplatform-config)
BuildRequires: pkgconfig(sqlite3)
BuildRequires: pkgconfig(db-util)

%description
Description: Media thumbnail service library for multimedia applications


%package devel
Summary:        Media thumbnail service library for multimedia applications (development)
Requires:       %{name}  = %{version}-%{release}
Group:          Multimedia/Development

%description devel
Description: Media thumbnail service library for multimedia applications (development)

%package -n media-thumbnail-server
Summary:        Thumbnail generator
Requires:       %{name}  = %{version}-%{release}
Group:          Multimedia/Service

%description -n media-thumbnail-server
Description: Media Thumbnail Server


%prep
%setup -q
cp %{SOURCE1001} %{SOURCE1002} %{SOURCE1003} .


%build
%cmake .
make %{?_smp_mflags}

%install
%make_install

#License
mkdir -p %{buildroot}/%{_datadir}/license
cp -rf %{_builddir}/%{name}-%{version}/LICENSE.APLv2.0 %{buildroot}/%{_datadir}/license/%{name}
cp -rf %{_builddir}/%{name}-%{version}/LICENSE.APLv2.0 %{buildroot}/%{_datadir}/license/media-thumbnail-server

%post -p /sbin/ldconfig
%postun -p /sbin/ldconfig

%files
%manifest %{name}.manifest
%defattr(-,root,root,-)
%{_libdir}/libmedia-thumbnail.so
%{_libdir}/libmedia-thumbnail.so.*
%{_libdir}/libmedia-hash.so
%{_libdir}/libmedia-hash.so.1
%{_libdir}/libmedia-hash.so.1.0.0
#License
%{_datadir}/license/%{name}
%{_datadir}/license/media-thumbnail-server

%files devel
%manifest %{name}-devel.manifest
%defattr(-,root,root,-)
%{_libdir}/pkgconfig/media-thumbnail.pc
%{_includedir}/media-thumbnail/*.h

%files -n media-thumbnail-server
%manifest media-thumbnail-server.manifest
%defattr(-,root,root,-)
%{_bindir}/media-thumbnail-server
#%{_bindir}/test-thumb
