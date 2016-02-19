Summary: Generic Event Dispatcher
Name:ged
Version:1.5
Release:6.eon
Source:%{name}-%{version}.tar.gz
BuildRoot:/tmp/%{name}-%{version}
Group:Applications/Base
Packager: http://generic-ed.sourceforge.net
License:GPL

BuildRequires: gcc >= 4.0.0
BuildRequires: pkgconfig
BuildRequires: libgenerics-devel >= 1.2-1
BuildRequires: libgcrypt-devel
BuildRequires: mysql-devel >= 5.0.3
BuildRequires: glib2-devel
BuildRequires: zlib-devel
BuildRequires: db4-devel

Requires: libgenerics >= 1.2-1
Requires: openssl
Requires: libgcrypt
Requires: glib2
Requires: zlib

%description
GED is a wire designed to handle templated data transmission over HTTP in distributed networks. 

%package mysql
Summary: Generic Event Dispatcher MySQL backend
License: GPL
Group: Applications/Base

Requires: %{name} = %{version}
Requires: mysql

%description mysql
GED is a wire designed to handle templated data transmission over HTTP in distributed networks. 
This is the mysql GED backend.

%package bdb
Summary: Generic Event Dispatcher Berkeley backend
License: GPL
Group: Applications/Base

Requires: %{name} = %{version}
Requires: db4

%description bdb
GED is a wire designed to handle templated data transmission over HTTP in distributed networks.
This is the berkeley GED backend.

%package devel

Summary: Generic Event Dispatcher
License: GPL
Group: Applications/Base

Requires: %{name} = %{version}
Requires: libgenerics-devel >= 1.2
Requires: libgcrypt-devel
Requires: mysql-devel >= 5.0.0
Requires: glib2-devel
Requires: zlib-devel
Requires: db4-devel
Requires: openssl-devel

%description devel
GED is a wire designed to handle templated data transmission over HTTP in distributed networks. 
This is the devel part as you may want to write your own backend.

%prep
%setup -q

%build
	make 

%install
	rm -rf ${RPM_BUILD_ROOT}
	mkdir -p ${RPM_BUILD_ROOT}/srv/eyesofnetwork/%{name}-%{version}/bin
        mkdir -p ${RPM_BUILD_ROOT}/srv/eyesofnetwork/%{name}-%{version}/etc/ssl/easy-rsa
        mkdir -p ${RPM_BUILD_ROOT}/srv/eyesofnetwork/%{name}-%{version}/etc/bkd
        mkdir -p ${RPM_BUILD_ROOT}/etc/init.d
        mkdir -p ${RPM_BUILD_ROOT}/etc/cron.d
        mkdir -p ${RPM_BUILD_ROOT}/etc/httpd/conf.d
        mkdir -p ${RPM_BUILD_ROOT}/usr/lib64/pkgconfig
        mkdir -p ${RPM_BUILD_ROOT}/srv/eyesofnetwork/%{name}-%{version}/lib64
        mkdir -p ${RPM_BUILD_ROOT}/srv/eyesofnetwork/%{name}-%{version}/man/man8
        mkdir -p ${RPM_BUILD_ROOT}/srv/eyesofnetwork/%{name}-%{version}/scripts
        mkdir -p ${RPM_BUILD_ROOT}/srv/eyesofnetwork/%{name}-%{version}/var/www/

        install -m 744 etc.in/gedd ${RPM_BUILD_ROOT}/etc/init.d
        install -m 640 etc.in/ged2rss ${RPM_BUILD_ROOT}/etc/cron.d/
        install -m 640 etc.in/ged_rss.conf ${RPM_BUILD_ROOT}/etc/httpd/conf.d/
        install -m 666 etc.in/*.cfg ${RPM_BUILD_ROOT}/srv/eyesofnetwork/%{name}-%{version}/etc
        install -m 666 etc.in/bkd/*.cfg ${RPM_BUILD_ROOT}/srv/eyesofnetwork/%{name}-%{version}/etc/bkd
        install -m 666 etc.in/bkd/geddummy.cfg etc.in/bkd/gedhdb.cfg ${RPM_BUILD_ROOT}/srv/eyesofnetwork/%{name}-%{version}/etc/bkd
        install -m 755 ged ${RPM_BUILD_ROOT}/srv/eyesofnetwork/%{name}-%{version}/bin
        install -m 755 gedq ${RPM_BUILD_ROOT}/srv/eyesofnetwork/%{name}-%{version}/bin
        install -m 755 gedbackup ${RPM_BUILD_ROOT}/srv/eyesofnetwork/%{name}-%{version}/bin
        install -m 755 gedrestore ${RPM_BUILD_ROOT}/srv/eyesofnetwork/%{name}-%{version}/bin
        install -m 655 %{name}dummy-%{version}.so %{name}hdb-%{version}.so %{name}mysql-%{version}.so ${RPM_BUILD_ROOT}/srv/eyesofnetwork/%{name}-%{version}/lib64
        install -m 655 lib%{name}-%{version}.* lib%{name}q-%{version}.* ${RPM_BUILD_ROOT}/usr/lib64
        install -m 644 etc.in/bkd/*.sql ${RPM_BUILD_ROOT}/srv/eyesofnetwork/%{name}-%{version}/etc/bkd
        install -m 644 gedq.8.gz ${RPM_BUILD_ROOT}/srv/eyesofnetwork/%{name}-%{version}/man/man8
        install -m 744 ssl/mk* ssl/check* ${RPM_BUILD_ROOT}/srv/eyesofnetwork/%{name}-%{version}/etc/ssl
        install -m 744 ssl/easy-rsa/* ${RPM_BUILD_ROOT}/srv/eyesofnetwork/%{name}-%{version}/etc/ssl/easy-rsa
        install -m 755 scripts/* ${RPM_BUILD_ROOT}/srv/eyesofnetwork/%{name}-%{version}/scripts

        mkdir -p ${RPM_BUILD_ROOT}/usr/include/ged
        install -m 644 inc/* ${RPM_BUILD_ROOT}/usr/include/ged
        install -m 644 %{name}-%{version}.pc ${RPM_BUILD_ROOT}/usr/lib64/pkgconfig
        install -m 644 var/www/index.html ${RPM_BUILD_ROOT}/srv/eyesofnetwork/%{name}-%{version}/var/www/

%post
	mkdir -p ${RPM_BUILD_ROOT}/srv/eyesofnetwork/%{name}-%{version}/var/cache
        chmod 777 ${RPM_BUILD_ROOT}/srv/eyesofnetwork/%{name}-%{version}/var/cache

        mkdir -p ${RPM_BUILD_ROOT}/srv/eyesofnetwork/%{name}-%{version}/var/lib
        chmod 777 ${RPM_BUILD_ROOT}/srv/eyesofnetwork/%{name}-%{version}/var/lib

        ln -s /srv/eyesofnetwork/%{name}-%{version} /srv/eyesofnetwork/%{name}

        if [ ! -f /srv/eyesofnetwork/%{name}-%{version}/etc/ssl/ca.crt ]; then
                cd /srv/eyesofnetwork/%{name}-%{version}/etc/ssl
                ./mkgedsrvcerts.sh geds
                ./mkgedclicerts.sh gedc
                cd -
        fi

%preun
	chkconfig gedd off
	/etc/init.d/gedd stop > /dev/null 2>&1

%postun
	rm -rf /srv/eyesofnetwork/%{name}

%clean
	rm -rf ${RPM_BUILD_ROOT}
	rm -rf /tmp/%{name}-%{version}

%files
%config(noreplace) /srv/eyesofnetwork/%{name}-%{version}/etc/ged.cfg
%config(noreplace) /srv/eyesofnetwork/%{name}-%{version}/etc/gedq.cfg
%config(noreplace) /srv/eyesofnetwork/%{name}-%{version}/etc/gedp.cfg
%config(noreplace) /srv/eyesofnetwork/%{name}-%{version}/etc/gedt.cfg
%config(noreplace) /srv/eyesofnetwork/%{name}-%{version}/etc/bkd/geddummy.cfg
/srv/eyesofnetwork/%{name}-%{version}/etc/ssl
/etc/init.d/gedd
/etc/cron.d/ged2rss
/etc/httpd/conf.d/ged_rss.conf
/srv/eyesofnetwork/%{name}-%{version}/lib64/geddummy-%{version}.so
/srv/eyesofnetwork/%{name}-%{version}/bin/ged
/srv/eyesofnetwork/%{name}-%{version}/bin/gedq
/srv/eyesofnetwork/%{name}-%{version}/bin/gedbackup
/srv/eyesofnetwork/%{name}-%{version}/bin/gedrestore
/srv/eyesofnetwork/%{name}-%{version}/man/man8/gedq.8.gz
/srv/eyesofnetwork/%{name}-%{version}/scripts/ged-nagios-host
/srv/eyesofnetwork/%{name}-%{version}/scripts/ged-nagios-service
/srv/eyesofnetwork/%{name}-%{version}/scripts/ged-snmptt
/srv/eyesofnetwork/%{name}-%{version}/scripts/ged2rss.pl
/srv/eyesofnetwork/%{name}-%{version}/scripts/
/srv/eyesofnetwork/%{name}-%{version}/var/www/index.html
/usr/lib64/libged-%{version}.so
/usr/lib64/libged-%{version}.a
/usr/lib64/libgedq-%{version}.so
/usr/lib64/libgedq-%{version}.a

%files bdb
%config(noreplace) /srv/eyesofnetwork/%{name}-%{version}/etc/bkd/gedhdb.cfg
/srv/eyesofnetwork/%{name}-%{version}/lib64/gedhdb-%{version}.so

%files mysql
%config(noreplace) /srv/eyesofnetwork/%{name}-%{version}/etc/bkd/gedmysql.cfg
/srv/eyesofnetwork/%{name}-%{version}/lib64/gedmysql-%{version}.so
/srv/eyesofnetwork/%{name}-%{version}/etc/bkd/ged-init.sql

%files devel
/usr/include/ged
/usr/lib64/pkgconfig/%{name}-%{version}.pc

%changelog
* Mon Jan 13 2014 Jeremie Bernard <gremi@users.sourceforge.net> - 1.5.6
- Fix casting variables for 64 Bits arch.

* Sat Jun 29 2013 Michael Aubertin <michael.aubertin@gmail.com> - 1.5.5
- Remove glib forgetted dependencies. 
- Fix ged2rss group bug.

* Wed Jun 26 2013 Michael Aubertin <michael.aubertin@gmail.com> - 1.5.4
- Add GED Rss feature per user and per filter but the service group.

* Sat Jun 08 2013 Michael Aubertin <michael.aubertin@gmail.com> - 1.5.3
- Add Ged 2 RSS XML feature :)

* Fri Jun 07 2013 Michael Aubertin <michael.aubertin@gmail.com> - 1.5.2
- Compile against 64Bit with fPIC translation.
- Port to glib2 compliance GString

* Sat Jul 07 2012 Michael Aubertin <michael.aubertin@gmail.com> - 1.5.1
- Backport 1.4.11 patch
- 1.4.11 Correcting minor state bug when unspecified massive drop.

* Mon Jun 25 2012 Michael Aubertin <michael.aubertin@gmail.com> - 1.5.0
- Adding Time To Own Flags handling feature

* Wed May 30 2012 Jeremie Bernard <gremi@users.sourceforge.net> - 1.4.10
- Handling my database issue regarding mysql backend.

* Wed May 11 2012 Michael Aubertin <michael.aubertin@gmail.com> - 1.4.9
- Hacking script for migration facilities 
- Fixing 0 addition in case of uncomplete gedq -push packet.
- Change ack default time in conf file
- Remove sync as default in gedt.cfg

* Wed Apr 23 2012 Michael Aubertin <michael.aubertin@gmail.com> - 1.4.8
- Fixing end of heap segfault in CChunk object in case of null argument packet dropping.

* Wed Apr 18 2012 Michael Aubertin <michael.aubertin@gmail.com> - 1.4.7
- Allow compatibility of sync queue between 1.2.12 and 1.4 series.

* Wed Apr 16 2012 Michael Aubertin <michael.aubertin@gmail.com> - 1.4.6
- Fixing gedq version number

* Wed Apr 04 2012 Michael Aubertin <michael.aubertin@gmail.com> - 1.4.5
- Fixing duplicate id in case of multiple cross migration queue packet

* Mon Apr 02 2012 Michael Aubertin <michael.aubertin@gmail.com> - 1.4.4
- Fixing duplicate id using multiple SYNC queue. 
- Set AUTO_INCREMENT to all data packet tables.

* Fri Mar 30 2012 Michael Aubertin <michael.aubertin@gmail.com> - 1.4.3
- Fixing last id record major bug... Yeeeha !!!!

* Mon Mar 19 2012 Michael Aubertin <michael.aubertin@gmail.com> - 1.4.2
- Fixing SLA usec bug.

* Fri Mar 9 2012 Michael Aubertin <michael.aubertin@gmail.com> - 1.4.1
- Multiple optimisation. MySQL 5.5 compliant. Multi-table handling

* Thu Oct 4 2011 Jeremie Bernard <gremi@users.sourceforge.net> - 1.2.12
- Daemonization patch (tty detach : pgrp)

* Sun Oct 2 2011 Jeremie Bernard <gremi@users.sourceforge.net> - 1.2.11
- Backend config cache is no more written when finalizing (but when initializing)
- CREATE TABLE has been added the IF NOT EXISTS option which is permissive 
- DROP TABLE has been added the IF EXIXTS option which is permissive

* Wed Sep 14 2011 Jeremie Bernard <gremi@users.sourceforge.net> - 1.2.10
- MySQL backend query execution error handling modification

* Wed May 19 2010 Jeremie Bernard <gremi@users.sourceforge.net> - 1.2.9
- TTL backend nrecords queue inc patch

* Wed May 12 2010 Jeremie Bernard <gremi@users.sourceforge.net> - 1.2.8
- potential TTL backend dead threads while forking patch 

* Fri Apr 9 2010 Jeremie Bernard <gremi@users.sourceforge.net> - 1.2.7
- mysql filters (< > ' ") options addon

* Thu Feb 9 2010 Jeremie Bernard <gremi@users.sourceforge.net> - 1.2.6
- connect option replaces bind to specify gedq and gedt targets
- bind option remains but now forces source interface to be used

* Thu Feb 4 2010 Jeremie Bernard <gremi@users.sourceforge.net> - 1.2.5
- gcrypt init statement and pthread protection (to avoid recurent warning logs)

* Mon Jan 18 2010 Jeremie Bernard <gremi@users.sourceforge.net> - 1.2.4
- static mysql varchar 255 => varchar N option (mysql > 5.0.3 required)
- mysql ' " < > filtering

* Wed Nov 25 2009 Jeremie Bernard <gremi@users.sourceforge.net> - 1.2.3
- mysql mode no backslash escapes option addon

* Sat May 30 2009 Jeremie Bernard <gremi@users.sourceforge.net> - 1.2.2 
- nosync flag implementation
- performance inc, light xml format option
- local file socket bindings
- ged/gedq communication improvement
- history queue gedq drop id list arg
- mysql backend proposal

* Fri Feb 20 2009 Jeremie Bernard <gremi@users.sourceforge.net> - 1.2.1 
- non root low port binding enabling
- ged/gedq security enhencement (peer data and request access notion)
- gedq sem patch (exception catch in ctx constructor)
- xml output update (root version attr, content key spec, content meta spec)
- xml recover patch (empty fields specifics)
- backend config cache flush once loaded
- async mode patch
- user meta data implementation (push/update requests specifics)
- update request reverse sync
- gedq utf8 filtering
- history queue record id addon and drop specific request modification
- ged daemon mode
- mysql backend temporary removed

* Thu Dec 30 2008 Jeremie Bernard <gremi@users.sourceforge.net> - 1.2.0 
- lzo removal, zlib use instead
- http/s tunnel implementation
- proxy basic and ntlm auth handling

* Sun Sep 14 2008 Jeremie Bernard <gremi@users.sourceforge.net> - 1.1
- xml output tree update
- packages split (backends)

* Thu Nov 29 2007 Jeremie Bernard <gremi@users.sourceforge.net> - 1.1
- simple http proxy passthru

* Mon Sep 03 2007 Jeremie Bernard <gremi@users.sourceforge.net> - 1.0b
- first beta package including mysql and hash berkeley backends
