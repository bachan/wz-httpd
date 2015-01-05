Summary:	WZ http server
Name:		wz-httpd	
Version:	1.0.0
Release:	0%{?dist}
Group:		Networking/Daemons
License:	GPL
Source:		wz-httpd-%{version}.tar.gz
BuildRoot:	%{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

%package devel
Summary:	Header files and development documentation for %{name}
Group:		Development/Libraries
Requires:	%{name} = %{version}-%{release}

%description
WZ http server

%description devel
WZ header files.

This package contains the header files, static libraries and development
documentation for %{name}. If you like to develop modules for %{name},
you will need to install %{name}-devel.

%prep
%setup -q -n %{name}-%{version}

%build
%cmake .
make %{?_smp_mflags}

%install
rm -rf %{buildroot}
mkdir %{buildroot}
make install DESTDIR=%{buildroot}

%clean
rm -rf %{buildroot}

%files
%defattr(-,root,root,-)
%{_bindir}/wz-httpd
%{_prefix}/etc/wzconfig.xml.example

%files devel
%{_includedir}/wz_handler.h

%changelog

