Name:       libmedia-thumbnail
Summary:    Media thumbnail service library for multimedia applications.
Version:	0.2.0
Release:    1
Group:      System/Libraries
License:    Apache-2.0
Source0:    %{name}-%{version}.tar.gz
Source1:    thumbnail-server.service
Source1001: packaging/libmedia-thumbnail.manifest 
BuildRequires: cmake
BuildRequires: pkgconfig(dlog)
BuildRequires: pkgconfig(mm-fileinfo)
BuildRequires: pkgconfig(mmutil-imgp)
BuildRequires: pkgconfig(mmutil-jpeg)
BuildRequires: pkgconfig(drm-service)
BuildRequires: pkgconfig(libexif)
BuildRequires: pkgconfig(heynoti)
BuildRequires: pkgconfig(evas)
BuildRequires: pkgconfig(ecore)
BuildRequires: pkgconfig(aul)


%description
Description: Media thumbnail service library for multimedia applications.


%package devel
License:        Apache-2.0
Summary:        Media thumbnail service library for multimedia applications. (development)
Requires:       %{name}  = %{version}-%{release}
Group:          Development/Libraries

%description devel
Description: Media thumbnail service library for multimedia applications. (development)

%package -n media-thumbnail-server
License:        Apache-2.0
Summary:        Thumbnail generator.
Requires:       %{name}  = %{version}-%{release}
Group:          Development/Libraries

%description -n media-thumbnail-server
Description: Media Thumbnail Server.


%prep
%setup -q


%build
cp %{SOURCE1001} .
cmake . -DCMAKE_INSTALL_PREFIX=%{_prefix}
make %{?jobs:-j%jobs}

%install
rm -rf %{buildroot}
%make_install

mkdir -p %{buildroot}%{_libdir}/systemd/user/tizen-middleware.target.wants
install -m 0644 %SOURCE1 %{buildroot}%{_libdir}/systemd/user/
ln -s ../thumbnail-server.service %{buildroot}%{_libdir}/systemd/user/tizen-middleware.target.wants/thumbnail-server.service

mkdir -p %{buildroot}%{_sysconfdir}/rc.d/rc5.d/
ln -s %{_sysconfdir}/init.d/thumbsvr %{buildroot}%{_sysconfdir}/rc.d/rc5.d/S47thumbsvr
mkdir -p %{buildroot}%{_sysconfdir}/rc.d/rc3.d/
ln -s %{_sysconfdir}/init.d/thumbsvr %{buildroot}%{_sysconfdir}/rc.d/rc3.d/S47thumbsvr



%files
%manifest libmedia-thumbnail.manifest
%{_libdir}/libmedia-thumbnail.so
%{_libdir}/libmedia-thumbnail.so.*
%{_libdir}/libmedia-hash.so
%{_libdir}/libmedia-hash.so.*


%files devel
%manifest libmedia-thumbnail.manifest
%{_libdir}/pkgconfig/media-thumbnail.pc
%{_includedir}/media-thumbnail/media-thumb-types.h
%{_includedir}/media-thumbnail/media-thumb-error.h
%{_includedir}/media-thumbnail/media-thumbnail.h
%{_includedir}/media-thumbnail/media-thumbnail-private.h


%files -n media-thumbnail-server
%manifest libmedia-thumbnail.manifest
%{_bindir}/media-thumbnail-server
%exclude %{_bindir}/test-thumb
%attr(755,root,root) %{_sysconfdir}/init.d/thumbsvr
%attr(755,root,root) %{_sysconfdir}/rc.d/rc3.d/S47thumbsvr
%attr(755,root,root) %{_sysconfdir}/rc.d/rc5.d/S47thumbsvr
%{_libdir}/systemd/user/thumbnail-server.service
%{_libdir}/systemd/user/tizen-middleware.target.wants/thumbnail-server.service

