Summary: Programs used to test Priority Inheritance Mutexes
Name: pi_tests
Version: 1.17
Release: 1%{?dist}
License: GPL
Group: Development/Tools
URL: http://people.redhat.com/~williams/tests
Source0: %{name}-%{version}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root

%define mandir usr/share/man

%description
The pi_tests package contains programs used to test the functionality of 
the priority inheritance attribute of POSIX mutexes on the Linux kernel

%prep
%setup -q

%build
make all

%install
rm -rf $RPM_BUILD_ROOT
mkdir $RPM_BUILD_ROOT
install -D -m 755 classic_pi $RPM_BUILD_ROOT/usr/sbin/classic_pi
install -D -m 755 pi_stress $RPM_BUILD_ROOT/usr/sbin/pi_stress
install -D -m 644 pi_stress.8 $RPM_BUILD_ROOT/%{mandir}/man8/pi_stress.8
gzip $RPM_BUILD_ROOT/%{mandir}/man8/pi_stress.8
install -D -m 644 COPYING $RPM_BUILD_ROOT/usr/share/doc/%{name}-%{version}/COPYING
install -D -m 644 README $RPM_BUILD_ROOT/usr/share/doc/%{name}-%{version}/README

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,-)
/usr/sbin/classic_pi
/usr/sbin/pi_stress
%doc
/usr/share/man/man8/pi_stress.8.gz
/usr/share/doc/%{name}-%{version}/COPYING
/usr/share/doc/%{name}-%{version}/README

%changelog
* Wed Jan  2 2008 Clark Williams <williams@redhat.com> - 1.17-1
- fixed logic error with watchdog_check()

* Wed Jul 11 2007 Clark Williams <williams@redhat.com> - 1.16-1
- bumped version for packaging in RHEL-RT
- added usage() to classic_pi
- removed --signal option in pi_stress

* Wed Jul 11 2007 Clark Williams <williams@redhat.com> - 1.15-1
- added rpmlint target to Makefile
- fixed rpmlint complaints in specfile

* Wed Dec 12 2006 Clark Williams <williams@redhat.com> - 1.14-1
- changed how pi_stress shutdown is handled
- fixed dealing with --iterations shutdown

* Thu Dec  7 2006 Clark Williams <williams@redhat.com> - 1.13-1
- Added README and COPYING files

* Wed Dec  6 2006 Clark Williams <williams@redhat.com> - 1.12-1
- changed shutdown variable to volatile
- bumped version

* Mon Dec  4 2006 Clark Williams <williams@redhat.com> - 1.11-1
- Modified priorities to be just above minimum (as opposed to just below max)

* Thu Nov 30 2006 Clark Williams <williams@redhat.com> - 1.10-1
- added timing logic to pi_stress.c
- refactored function order in pi_stress.c

* Wed Nov 29 2006 Clark Williams <williams@redhat.com> - 1.9-1
- added --duration logic to pi_stress.c

* Mon Nov 27 2006 Clark Williams <williams@redhat.com> - 1.8-1
- Initial packaging.

