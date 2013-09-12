%if %dicom == 1
Summary: Beryllium DICOM edition.
%else
Summary: Beryllium free edition.
%endif
Name: beryllium
Provides: beryllium
Version: 0.3.8
Release: 1
License: LGPL
Source: %{name}.tar.gz
#Source: %{name}-%{version}.tar.bz2
URL: http://dc.baikal.ru/products/beryllium
Vendor: Beryllium team <beryllium@dc.baikal.ru>
Packager: Beryllium team <beryllium@dc.baikal.ru>

Requires: gstreamer-0_10-plugins-base >= 0.10.31, gstreamer-0_10-plugins-good >= 0.10.31, gstreamer-0_10-plugins-bad >= 0.10.23, gstreamer-0_10-plugins-ugly >= 0.10.19
Requires: libgstreamer-0_10-0 >= 0.10.31, libmediainfo0 >= 0.7.52, libqt4 >= 4.7.0, libQtGLib-2_0-0, libQtGStreamer-0_10-0, libstdc++6 >= 4.4.0

%description
%if %dicom == 1
Beryllium DICOM edition.
%else
Beryllium free edition.
%endif


Video and image capturing for medicine.

%prep
%setup -qn %{name}
 
%build
%if %dicom == 1
qmake CONFIG+=dicom PREFIX=%{_prefix} QMAKE_CFLAGS+="%optflags" QMAKE_CXXFLAGS+="%optflags";
%else
qmake PREFIX=%{_prefix} QMAKE_CFLAGS+="%optflags" QMAKE_CXXFLAGS+="%optflags";
%endif
make %{?_smp_mflags};

%install
make install INSTALL_ROOT="%buildroot";

%files
%{_bindir}/beryllium
%dir %{_datadir}/%{name}
%{_datadir}/applications/%{name}.desktop
%dir %{_datadir}/%{name}/translations
%{_datadir}/%{name}/translations/%{name}_ru.qm
%{_iconsdir}/%{name}.png
%doc docs/*
%{_mandir}/man1/%{name}.1.gz

%changelog
* Wed Sep 11 2013 Pavel Bludov <pbludov@gmail.com>
+ Version 0.3.8
- First rpm spec for automated builds.
