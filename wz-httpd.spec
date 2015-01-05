Summary:	WZ http server
Name:		wz-httpd	
Version:	1.0
Release:	0%{?dist}

Group:		Networking/Daemons
License:	GPL
Source:		wz-httpd.tar.gz
BuildRoot:	%{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

%description
WZ http server

%prep
%setup -q -n %{name}


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
%{_includedir}/wz_handler.h



%changelog

