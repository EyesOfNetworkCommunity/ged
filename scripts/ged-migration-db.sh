#!/bin/bash

VERSION="1.5"

usage() {
echo "Usage :ged-migration-db.sh
        -u username
        -p password
        "
exit 2
}

if [ "${4}" = "" ]; then usage; fi

ARGS="`echo $@ |sed -e 's:-[a-Z] :\n&:g' | sed -e 's: ::g'`"
for i in $ARGS; do
        if [ -n "`echo ${i} | grep "^\-u"`" ]; then USERNAME="`echo ${i} | cut -c 3-`"; if [ ! -n ${USERNAME} ]; then usage;fi;fi
        if [ -n "`echo ${i} | grep "^\-p"`" ]; then PASSWORD="`echo ${i} | cut -c 3-`"; if [ ! -n ${PASSWORD} ]; then usage;fi;fi
done

if [ ! "`id | cut -d'(' -f1 | cut -d'=' -f2`" = "0" ];then
	echo "ERROR: You must be root to run this script."
	echo ""
	echo ""
	echo ""
	usage
fi

if [ ! -d /var/archives/ged-migration ]; then mkdir -p /var/archives/ged-migration; fi
TMPDIR="`mktemp -d /var/archives/ged-migration/ged.XXXXXX`"

OUTPUT="`echo "show databases" | mysql -u ${USERNAME} --password=${PASSWORD} ged 2> /dev/null | grep ged`"

if [ ! -n "$OUTPUT" ]; then
	echo "ERROR: Base Ged introuvable ou connection impossible."
	echo ""
	echo ""
	echo ""
	usage
fi

if [ ! "`rpm -qa ged-mysql | cut -d'-' -f1,2,3`" = "ged-mysql-$VERSION" ]; then
	echo "ERROR: ged-mysql-$VERSION and ged-$VERSION must be installed first."
	echo ""
	echo ""
	usage
fi

/etc/init.d/gedd restart
OUTPUT="`echo "desc nagios_queue_active" | mysql -u ${USERNAME} --password=${PASSWORD} ged 2> /dev/null | head -1 | grep Field`"

if [ -n "$OUTPUT" ]; then
	echo "Database version 1.4 detected."
	/etc/init.d/gedd stop
	echo "Dumping current database in $TMPDIR."
	mysqldump -u ${USERNAME} --password=${PASSWORD} ged -t > $TMPDIR/ged_data.sql
	echo "Droping current database."
	echo "drop database ged;" | mysql -u ${USERNAME} --password=${PASSWORD}
	echo "Creating current database."
	echo "create database ged;" | mysql -u ${USERNAME} --password=${PASSWORD}
	echo "First starting of ged."
	/etc/init.d/gedd start
	echo "Stopping ged."
	/etc/init.d/gedd stop
	echo "Dumping empty tables in $TMPDIR."	
	mysqldump -u ${USERNAME} --password=${PASSWORD} ged -d > $TMPDIR/ged_tables.sql
	echo "Droping old database ged."
	echo "drop database ged;" | mysql -u ${USERNAME} --password=${PASSWORD} 
	echo "Creating new database ged."
	echo "create database ged;" | mysql -u ${USERNAME} --password=${PASSWORD} 
	echo "Creating new tables."
	mysql -u ${USERNAME} --password=${PASSWORD} ged <  $TMPDIR/ged_tables.sql
	echo "Injecting ged old datas."
	mysql -u ${USERNAME} --password=${PASSWORD} ged <  $TMPDIR/ged_data.sql
	echo "Starting definitive ged."
	/etc/init.d/gedd start

else
	echo "Database version 1.2"
	/etc/init.d/gedd stop
	mysqldump -u ${USERNAME} --password=${PASSWORD}  ged > $TMPDIR/ged.sql
	echo "create database ged_old;" | mysql -u ${USERNAME} --password=${PASSWORD}
	mysql -u ${USERNAME} --password=${PASSWORD} ged_old < $TMPDIR/ged.sql
	echo "drop database ged;" | mysql -u ${USERNAME} --password=${PASSWORD}
	echo "create database ged;" | mysql -u ${USERNAME} --password=${PASSWORD}
	/etc/init.d/gedd start
	/etc/init.d/gedd stop
	TYPE="`echo "select pkt_type_name from pkt_type where pkt_type_name NOT LIKE 'md5sum';" | mysql -u ${USERNAME} --password=${PASSWORD} ged | grep -v pkt_type_name`"
	for i in $TYPE; do
		echo "INSERT INTO ged.${i}_queue_active (id,queue,occ,o_sec,o_usec,l_sec,l_usec,r_sec,r_usec,m_sec,m_usec,a_sec,a_usec,reason,src,tgt,req,equipment,service,state,owner,description,ip_address,host_alias,hostgroups,servicegroups,comments) SELECT * from ged_old.${i} where queue='a';" | mysql -u ${USERNAME} --password=${PASSWORD}
		echo "INSERT INTO ged.${i}_queue_history (id,queue,occ,o_sec,o_usec,l_sec,l_usec,r_sec,r_usec,m_sec,m_usec,a_sec,a_usec,reason,src,tgt,req,equipment,service,state,owner,description,ip_address,host_alias,hostgroups,servicegroups,comments) SELECT * from ged_old.${i} where queue='h';" | mysql -u ${USERNAME} --password=${PASSWORD}
		echo "INSERT INTO ged.${i}_queue_sync (id,queue,occ,o_sec,o_usec,l_sec,l_usec,r_sec,r_usec,m_sec,m_usec,a_sec,a_usec,reason,src,tgt,req,equipment,service,state,owner,description,ip_address,host_alias,hostgroups,servicegroups,comments) SELECT * from ged_old.${i} where queue='s';" | mysql -u ${USERNAME} --password=${PASSWORD}
	done
	echo "drop database ged_old;" | mysql -u ${USERNAME} --password=${PASSWORD}
	/etc/init.d/gedd start

fi

