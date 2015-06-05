Name:           libmedia-thumbnail
Version:        0.1.80
Release:        0
License:        Apache-2.0
Summary:        Media thumbnail service Library
Group:          Multimedia/Libraries
Source0:        %{name}-%{version}.tar.gz
Source1001:     %{name}.manifest
Source1002:     %{name}-devel.manifest
Source1003:     media-thumbnail-server.manifest

BuildRequires:  cmake
BuildRequires:  pkgconfig(aul)
BuildRequires:  pkgconfig(dlog)
BuildRequires:  pkgconfig(drm-client)
BuildRequires:  pkgconfig(heynoti)
BuildRequires:  pkgconfig(libexif)
BuildRequires:  pkgconfig(libmedia-utils)
BuildRequires:  pkgconfig(mm-fileinfo)
BuildRequires:  pkgconfig(mmutil-imgp)
BuildRequires:  pkgconfig(mmutil-jpeg)
BuildRequires:  pkgconfig(vconf)
BuildRequires:  pkgconfig(libtzplatform-config)
BuildRequires:  pkgconfig(gdk-pixbuf-2.0)
Requires:       media-server

%description
Media thumbnail service library for multimedia applications.

%package devel
Summary:        Media Thumbnail Service Library (development)
Requires:       %{name} = %{version}-%{release}

%description devel
Media thumbnail service library for multimedia applications. (development)

%package -n media-thumbnail-server
Summary:        Thumbnail generator
Requires:       %{name} = %{version}-%{release}

%description -n media-thumbnail-server
Media Thumbnail Server.

%package test
Summary:        Thumbnail generator Tests
Requires:       %{name} = %{version}-%{release}

%description test
Media Thumbnail Tests.

%prep
%setup -q
cp %{SOURCE1001} %{SOURCE1002} %{SOURCE1003} .


%build
%cmake .
make %{?_smp_mflags}

%install
%make_install


%post -p /sbin/ldconfig
%postun -p /sbin/ldconfig

%files
%manifest %{name}.manifest
%license LICENSE
%defattr(-,root,root,-)
%{_libdir}/libmedia-thumbnail.so
%{_libdir}/libmedia-thumbnail.so.*
%{_libdir}/libmedia-hash.so
%{_libdir}/libmedia-hash.so.1
%{_libdir}/libmedia-hash.so.1.0.0

%files devel
%manifest %{name}-devel.manifest
%defattr(-,root,root,-)
%{_libdir}/pkgconfig/media-thumbnail.pc
%{_includedir}/media-thumbnail/*.h

%files -n media-thumbnail-server
%manifest media-thumbnail-server.manifest
%defattr(-,root,root,-)
%{_bindir}/media-thumbnail-server

%files test
%{_bindir}/test-thumb
