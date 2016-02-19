/****************************************************************************************************************************************
 gedcommon.h - ged common types and utility functions -
****************************************************************************************************************************************/

/*
 Copyright (C) 2007 Jérémie Bernard, Michaël Aubertin

 This package is free software; you can redistribute it and/or modify it under the terms of the 
 GNU General Public License as published by the Free Software Foundation; either version 2 of 
 the License, or (at your option) any later version.

 This software is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; 
 without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 See the GNU General Public License for more details.

 You should have received a copy of the GNU General Public License along with this software; 
 if not, write to :
 Free Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
*/

#ifndef __GEDCOMMON_H__
#define __GEDCOMMON_H__

#define GCC_VERSION (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)

#if GCC_VERSION < 40000
#error gcc >= 4.0.0 required
#endif

#define GED_MAJOR		1						// major version
#define GED_MINOR		5						// minor version (may impact backend data structure)
#define GED_PATCH		4						// patch level (not impacting backend data structure)
#define GED_VERSION		(GED_MAJOR*10000+GED_MINOR*100+GED_PATCH)

#define GED_MAJ(VERSION)	(VERSION/10000)
#define GED_MIN(VERSION)	((VERSION%10000)/100)
#define GED_PAT(VERSION)	((VERSION%10000)%100)

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/sem.h>

#if defined(__GNU_LIBRARY__) && !defined(_SEM_SEMUN_UNDEFINED)
// defined in sys/sem.h
#else 
// given X/OPEN we should define it ourself (POSIX.1-2001)
union semun
{
        int				val;
        struct semid_ds *		buf;
        unsigned short  *		array;
        struct seminfo  *		__buf;
};
#endif

#ifdef __GED__

#include <pwd.h>
#include <grp.h>

#include <syslog.h>

#include "cmetamoduleimporter.h"

#endif

#define __PTHREAD_DISABLE_CANCEL__ 	int inPrevThState;  	::pthread_setcancelstate (PTHREAD_CANCEL_DISABLE, &inPrevThState);
#define __PTHREAD_ENABLE_CANCEL__  	int inDummyThState; 	::pthread_setcancelstate (inPrevThState, &inDummyThState);

#include "csocketsslclient.h"
#include "csocketsslserver.h"
#include "cfile.h"

#include <zlib.h>

#include <glib.h>
#include <gcrypt.h>

#include <openssl/rand.h>
#ifdef __GED_NTLM__
#include <openssl/des.h>
#include <openssl/md4.h>
#endif

#include <locale.h>

using namespace NServices;

#define	GED_PKT_REQ_BKD_MASK		0x00000007			// backend queue mask
#define	GED_PKT_REQ_BKD_STAT		0x00000000			// stat queue
#define	GED_PKT_REQ_BKD_ACTIVE		0x00000001			// active queue backend
#define	GED_PKT_REQ_BKD_HISTORY		0x00000002			// history queue backend
#define	GED_PKT_REQ_BKD_SYNC		0x00000004			// sync queue backend

#define GED_PKT_REQ_BKD_HST_MASK	0x00000008			// history reason flag mask
#define GED_PKT_REQ_BKD_HST_PKT		0x00000000			// history drop reason
#define GED_PKT_REQ_BKD_HST_TTL		0x00000008			// history ttl reason

#define	GED_PKT_REQ_MASK		0x000000F0			// request mask
#define	GED_PKT_REQ_NONE		0x00000000			// no request
#define	GED_PKT_REQ_PUSH		0x00000010			// push request
#define GED_PKT_REQ_PUSH_MASK		0x00000100			// push request inc mask
#define GED_PKT_REQ_PUSH_TMSP		0x00000000			// push request with timestamp and inc update
#define GED_PKT_REQ_PUSH_NOTMSP		0x00000100			// push request without timestamp nor inc update
#define	GED_PKT_REQ_DROP		0x00000020			// drop request
#define GED_PKT_REQ_DROP_MASK		0x00000400			// drop request mask
#define GED_PKT_REQ_DROP_DATA		0x00000000			// drop request by data
#define GED_PKT_REQ_DROP_ID		0x00000400			// drop request by id (history only)
#define	GED_PKT_REQ_PEEK		0x00000040			// peek request
#define GED_PKT_REQ_PEEK_SORT_MASK	0x00000200			// peek sort mask
#define GED_PKT_REQ_PEEK_SORT_ASC	0x00000000			// peek ascendant sort
#define GED_PKT_REQ_PEEK_SORT_DSC	0x00000200			// peek descendant sort
#define GED_PKT_REQ_PEEK_PARM_MASK	0x000F0000			// peek parameters mask
#define GED_PKT_REQ_PEEK_PARM_1_TM	0x00010000			// first param is timestamp
#define GED_PKT_REQ_PEEK_PARM_2_OFFSET	0x00020000			// second param is record offset
#define GED_PKT_REQ_PEEK_PARM_3_TM	0x00040000			// third param is timestamp
#define GED_PKT_REQ_PEEK_PARM_4_NUMBER	0x00080000			// fourth param is number of records
#define GED_PKT_REQ_NO_SYNC		0x00100000			// do not relay even if the instance should

#define GED_PKT_REQ_SRC_MASK		0x0000F000			// request source mask
#define GED_PKT_REQ_SRC_RELAY		0x00001000			// ged daemon source flag

#define	GED_PKT_TYP_ANY			0x00000000			// packet type any
#define	GED_PKT_TYP_MD5			0x80000001			// md5sum packet
#define	GED_PKT_TYP_RECORD		0x80000002			// record packet (peek result)
#define	GED_PKT_TYP_CLOSE		0x80000004			// close packet (error notification)
#define	GED_PKT_TYP_ACK			0x80000008			// ack packet (request success notification)
#define	GED_PKT_TYP_PULSE		0x80000010			// heartbeat packet (sync and tun connections only)
#ifdef __GED_TUN__
#define	GED_PKT_TYP_OPEN_TUN		0x80000020			// open tun packet
#define	GED_PKT_TYP_DATA_TUN		0x80000040			// data tun packet
#define	GED_PKT_TYP_SHUT_TUN		0x80000080			// shutdown tun packet
#endif

// ged version commodity string
const static CString GED_VERSION_STR 	(CString((SInt32)GED_MAJOR)+"."+CString((SInt32)GED_MINOR)+"."+CString((SInt32)GED_PATCH));

// non contigous GED packet (reception handler output, callback input)
struct TGEDPktIn
{
	long				vrs;				// packet version
	struct timeval			tv;				// packet timestamp
	struct in_addr			addr;				// packet abs source
	long				typ;				// packet type
	long				req;				// packet request
	long				p1;				// request param 1
	long				p2;				// request param 2
	long				p3;				// request param 3
	long				p4;				// request param 4
	long				dmy[2];				// reserved for future use
	long				len;				// packet data length
	void *				data;				// packet data
};
const static size_t			GED_PKT_FIX_SIZE		= sizeof(TGEDPktIn) - sizeof(void*);

// contigous GED packet (emission handler raw input, callback output)
typedef void *				TGEDPktOut;

// GED record source
struct TGEDRcdSrc
{
	long				rly;				// ged daemon source
	struct in_addr			addr;				// record source
};

// generic GED record
struct TGEDRcd
{
	long				queue;				// record queue
	long				rsv[2];				// reserved for future use
};

// statistic GED record
struct TGEDStatRcd : public TGEDRcd
{
	long				vrs;				// bkd version 
	long				id;				// bkd identification
	long				nta;				// active queue total length
	long				nts;				// sync queue total length
	long				nth;				// history queue total length
	long				ntr;				// last request impacted queue length (before offset/number subset)
	long				dmy[8];				// reserved for future use
};

const static size_t			GED_STATRCD_FIX_SIZE		= sizeof(TGEDStatRcd);

// GED sync record
struct TGEDSRcd : public TGEDRcd
{
	struct in_addr			tgt;				// target to relay to
	TGEDPktIn *			pkt;				// packet to relay
};

const static size_t			GED_SRCD_FIX_SIZE		= sizeof(TGEDSRcd) - sizeof(TGEDPktIn*);

// GED active record
struct TGEDARcd : public TGEDRcd
{
	long				typ;				// record type <=> packet type
	long				occ;				// record occurrences
	struct timeval			otv;				// original timestamp
	struct timeval			ltv;				// last occurrence timestamp
	struct timeval			rtv;				// last reception timestamp
	struct timeval			mtv;				// last user meta timestamp
 	struct timeval			ftv;				// First user action
	char				nsrc;				// record sources number
	long				len;				// record data length
	TGEDRcdSrc *			src;				// record sources
	void *				data;				// record data
};

const static size_t			GED_ARCD_FIX_SIZE		= sizeof(TGEDARcd) - sizeof(TGEDRcdSrc*) - sizeof(void*);

// GED history record
struct TGEDHRcd : public TGEDRcd
{
	long				typ;				// record type <=> packet type
	unsigned long			hid;				// record history uniq local id
	long				occ;				// record occurrences
	struct timeval			otv;				// original timestamp
	struct timeval			ltv;				// last occurrence timestamp
	struct timeval			rtv;				// last reception timestamp
	struct timeval			mtv;				// last user meta timestamp
  struct timeval			ftv;				// First user action
	struct timeval			atv;				// acknowledge timestamp
	char				nsrc;				// record sources number
	long				len;				// record data length
	TGEDRcdSrc *			src;				// record sources
	void *				data;
};

const static size_t			GED_HRCD_FIX_SIZE		= sizeof(TGEDHRcd) - sizeof(TGEDRcdSrc*) - sizeof(void*);

// GED / GEDQ known core parameters
#ifdef __GED__
const static CString			GEDCfgFileDefault		("/srv/eyesofnetwork/ged/etc/ged.cfg");
const static CString			GEDCfgSysLogLocal		("syslog_local");
const static CString			GEDCfgSysLogHistoryLocal	("syslog_history_local");
const static CString			GEDCfgSysLogLevel		("syslog_level");
const static CString			GEDCfgUser			("uid");
const static CString			GEDCfgGroup			("gid");
const static CString			GEDCfgListen			("listen");
const static CString			GEDCfgFifo			("fifo");
const static CString			GEDCfgTlsDHParam		("tls_dhparam");
const static CString			GEDCfgHttpServer		("http_server");
const static CString			GEDCfgRelayToBeg		("<relay_to>");
const static CString			GEDCfgDataSync			("data_sync");
const static CString			GEDCfgRelayToEnd		("</relay_to>");
const static CString			GEDCfgAllowSyncFrom		("allow_sync_from");
const static CString			GEDCfgAllowRequestFrom		("allow_request_from");
const static CString			GEDCfgBackendBeg		("<backend");
const static CString			GEDCfgBackendMod		("mod");
const static CString			GEDCfgBackendEnd		("</backend");
const static CString			GEDCfgTTLTimer			("ttl_timer");
const static CString			GEDCfgTTLActive			("ttl_active_queue");
const static CString			GEDCfgTTLSync			("ttl_sync_queue");
const static CString			GEDCfgTTLHistory		("ttl_history_queue");
const static CString			GEDCfgPacketsPool		("packets_pool");
const static CString			GEDCfgMaxConnIn			("max_conn_in");
#ifdef __GED_TUN__
const static CString			GEDCfgMaxTunConnOut		("max_tun_conn_out");
const static CString			GEDCfgAllowTunFrom		("allow_tun_from");
const static CString			GEDCfgAllowTunTo		("allow_tun_to");
const static CString			GEDCfgEnableTun			("enable_tun");
#endif
#endif
const static CString			GEDCfgKeepAlive			("keep_alive");
const static CString			GEDCfgInclude			("include");
const static CString			GEDCfgTlsCaCert			("tls_ca_certificate");
const static CString			GEDCfgTlsCert			("tls_certificate");
const static CString			GEDCfgTlsCertKey		("tls_certificate_key");
const static CString			GEDCfgTlsVerifyPeer		("tls_verify_peer");
const static CString			GEDCfgTlsCipherSuite		("tls_cipher_suite");
const static CString			GEDCfgHttpProxy			("http_proxy");
const static CString			GEDCfgHttpProxyAuth		("http_proxy_auth");
const static CString			GEDCfgHttpProxyUser		("http_proxy_user");
const static CString			GEDCfgHttpProxyPass		("http_proxy_pass");
const static CString			GEDCfgHttpVersion		("http_version");
const static CString			GEDCfgHttpContentType		("http_content_type");
const static CString			GEDCfgHttpCommand		("http_command");
const static CString			GEDCfgHttpUserAgent		("http_user_agent");
const static CString			GEDCfgAckTimeOut		("ack_timeout");
const static CString			GEDCfgConnect			("connect");
const static CString			GEDCfgBind			("bind");
const static CString			GEDCfgPacketBeg			("<packet");
const static CString			GEDCfgPacketType		("type");
const static CString			GEDCfgPacketName		("name");
const static CString			GEDCfgPacketFieldString		("STRING");
const static CString			GEDCfgPacketFieldSInt32		("SINT32");
const static CString			GEDCfgPacketFieldFloat64	("FLOAT64");
const static CString			GEDCfgPacketFieldMeta		("META");
const static CString			GEDCfgPacketEnd			("</packet");
#ifdef __GEDQ__
const static CString			GEDQCfgFileDefault		("/srv/eyesofnetwork/ged/etc/gedq.cfg");
const static CString			GEDQCfgPeekMaxRecords		("peek_max_records");
#endif

// ged/gedq xml records
const static xmlChar *			XML_ELEMENT_NODE_GED_ROOT_VERSION_ATTR            	=(const xmlChar*)"version";
const static xmlChar *			XML_ELEMENT_NODE_GED_RECORD                       	=(const xmlChar*)"record";
const static xmlChar *			XML_ELEMENT_NODE_GED_RECORD_QUEUE_ATTR            	=(const xmlChar*)"queue";
const static xmlChar *			XML_ELEMENT_NODE_GED_RECORD_QUEUE_ATTR_ACTIVE     	=(const xmlChar*)"active";
const static xmlChar *			XML_ELEMENT_NODE_GED_RECORD_QUEUE_ATTR_SYNC       	=(const xmlChar*)"sync";
const static xmlChar *			XML_ELEMENT_NODE_GED_RECORD_QUEUE_ATTR_HISTORY    	=(const xmlChar*)"history";
const static xmlChar *			XML_ELEMENT_NODE_GED_RECORD_TYPE_ATTR             	=(const xmlChar*)"type";
const static xmlChar *			XML_ELEMENT_NODE_GED_RECORD_HASH_ATTR             	=(const xmlChar*)"hash";
const static xmlChar *			XML_ELEMENT_NODE_GED_RECORD_ID_ATTR               	=(const xmlChar*)"id";
const static xmlChar *			XML_ELEMENT_NODE_GED_RECORD_NAME_ATTR		  	=(const xmlChar*)"name";
const static xmlChar *			XML_ELEMENT_NODE_GED_RECORD_REQ_ATTR              	=(const xmlChar*)"request";
const static xmlChar *			XML_ELEMENT_NODE_GED_RECORD_REQ_ATTR_PUSH         	=(const xmlChar*)"push";
const static xmlChar *			XML_ELEMENT_NODE_GED_RECORD_REQ_ATTR_UPDATE       	=(const xmlChar*)"update";
const static xmlChar *			XML_ELEMENT_NODE_GED_RECORD_REQ_ATTR_DROP         	=(const xmlChar*)"drop";
const static xmlChar *			XML_ELEMENT_NODE_GED_RECORD_BKD_ATTR              	=(const xmlChar*)"backend";
const static xmlChar *			XML_ELEMENT_NODE_GED_RECORD_BKD_HST_ATTR_RSN		=(const xmlChar*)"reason";
const static xmlChar *			XML_ELEMENT_NODE_GED_RECORD_BKD_HST_ATTR_RSN_TTL  	=(const xmlChar*)"ttl";
const static xmlChar *			XML_ELEMENT_NODE_GED_RECORD_BKD_HST_ATTR_RSN_PKT  	=(const xmlChar*)"pkt";
const static xmlChar *			XML_ELEMENT_NODE_GED_RECORD_SOURCES               	=(const xmlChar*)"sources";
const static xmlChar *			XML_ELEMENT_NODE_GED_RECORD_SOURCE                	=(const xmlChar*)"source";
const static xmlChar *			XML_ELEMENT_NODE_GED_RECORD_SOURCE_ATTR_RLY       	=(const xmlChar*)"relay";
const static xmlChar *			XML_ELEMENT_NODE_GED_RECORD_TARGET                	=(const xmlChar*)"target";
const static xmlChar *			XML_ELEMENT_NODE_GED_RECORD_OCC_ATTR              	=(const xmlChar*)"occurrences";
const static xmlChar *			XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP		  	=(const xmlChar*)"timestamp";
const static xmlChar *			XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_ORIGINAL    	=(const xmlChar*)"original-time";
const static xmlChar *			XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_LAST        	=(const xmlChar*)"last-time";
const static xmlChar *			XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_RECEPTION   	=(const xmlChar*)"reception-time";
const static xmlChar *			XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_META   	  	=(const xmlChar*)"meta-time";
const static xmlChar *			XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_FIRST   	  	=(const xmlChar*)"account-time";
const static xmlChar *			XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_ACKNOWLEGDE 	=(const xmlChar*)"acknowledge-time";
const static xmlChar *			XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_SEC_ATTR    	=(const xmlChar*)"sec";
const static xmlChar *			XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_USEC_ATTR   	=(const xmlChar*)"usec";
const static xmlChar *			XML_ELEMENT_NODE_GED_RECORD_DURATION_ATTR   	  	=(const xmlChar*)"duration";
const static xmlChar *			XML_ELEMENT_NODE_GED_RECORD_CONTENT               	=(const xmlChar*)"content";
const static xmlChar *			XML_ELEMENT_NODE_GED_RECORD_CONTENT_KEY_ATTR      	=(const xmlChar*)"key";
const static xmlChar *			XML_ELEMENT_NODE_GED_RECORD_CONTENT_META_ATTR     	=(const xmlChar*)"meta";
const static xmlChar *			XML_ELEMENT_NODE_GED_RECORD_STAT_NTA_ATTR		=(const xmlChar*)"atot-len";
const static xmlChar *			XML_ELEMENT_NODE_GED_RECORD_STAT_NTS_ATTR		=(const xmlChar*)"stot-len";
const static xmlChar *			XML_ELEMENT_NODE_GED_RECORD_STAT_NTH_ATTR		=(const xmlChar*)"htot-len";
const static xmlChar *			XML_ELEMENT_NODE_GED_RECORD_STAT_NTR_ATTR		=(const xmlChar*)"rtot-len";

const static xmlChar *			XML_ELEMENT_NODE_GED_RECORD_LIGHT                    	=(const xmlChar*)"r";
const static xmlChar *			XML_ELEMENT_NODE_GED_RECORD_QUEUE_ATTR_LIGHT         	=(const xmlChar*)"q";
const static xmlChar *			XML_ELEMENT_NODE_GED_RECORD_QUEUE_ATTR_ACTIVE_LIGHT  	=(const xmlChar*)"a";
const static xmlChar *			XML_ELEMENT_NODE_GED_RECORD_QUEUE_ATTR_SYNC_LIGHT    	=(const xmlChar*)"s";
const static xmlChar *			XML_ELEMENT_NODE_GED_RECORD_QUEUE_ATTR_HISTORY_LIGHT 	=(const xmlChar*)"h";
const static xmlChar *			XML_ELEMENT_NODE_GED_RECORD_TYPE_ATTR_LIGHT          	=(const xmlChar*)"t";
const static xmlChar *			XML_ELEMENT_NODE_GED_RECORD_HASH_ATTR_LIGHT          	=(const xmlChar*)"h";
const static xmlChar *			XML_ELEMENT_NODE_GED_RECORD_ID_ATTR_LIGHT            	=(const xmlChar*)"i";
const static xmlChar *			XML_ELEMENT_NODE_GED_RECORD_REQ_ATTR_LIGHT           	=(const xmlChar*)"r";
const static xmlChar *			XML_ELEMENT_NODE_GED_RECORD_REQ_ATTR_PUSH_LIGHT      	=(const xmlChar*)"p";
const static xmlChar *			XML_ELEMENT_NODE_GED_RECORD_REQ_ATTR_UPDATE_LIGHT    	=(const xmlChar*)"u";
const static xmlChar *			XML_ELEMENT_NODE_GED_RECORD_REQ_ATTR_DROP_LIGHT      	=(const xmlChar*)"d";
const static xmlChar *			XML_ELEMENT_NODE_GED_RECORD_BKD_ATTR_LIGHT           	=(const xmlChar*)"b";
const static xmlChar *			XML_ELEMENT_NODE_GED_RECORD_BKD_HST_ATTR_RSN_LIGHT   	=(const xmlChar*)"r";
const static xmlChar *			XML_ELEMENT_NODE_GED_RECORD_BKD_HST_ATTR_RSN_TTL_LIGHT	=(const xmlChar*)"t";
const static xmlChar *			XML_ELEMENT_NODE_GED_RECORD_BKD_HST_ATTR_RSN_PKT_LIGHT	=(const xmlChar*)"p";
const static xmlChar *			XML_ELEMENT_NODE_GED_RECORD_SOURCES_LIGHT	        =(const xmlChar*)"u";
const static xmlChar *			XML_ELEMENT_NODE_GED_RECORD_SOURCE_LIGHT          	=(const xmlChar*)"s";
const static xmlChar *			XML_ELEMENT_NODE_GED_RECORD_SOURCE_ATTR_RLY_LIGHT	=(const xmlChar*)"r";
const static xmlChar *			XML_ELEMENT_NODE_GED_RECORD_TARGET_LIGHT           	=(const xmlChar*)"g";
const static xmlChar *			XML_ELEMENT_NODE_GED_RECORD_OCC_ATTR_LIGHT           	=(const xmlChar*)"o";
const static xmlChar *			XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_LIGHT	  	=(const xmlChar*)"t";
const static xmlChar *			XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_ORIGINAL_LIGHT	=(const xmlChar*)"o";
const static xmlChar *			XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_LAST_LIGHT        =(const xmlChar*)"l";
const static xmlChar *			XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_RECEPTION_LIGHT   =(const xmlChar*)"r";
const static xmlChar *			XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_META_LIGHT   	=(const xmlChar*)"m";
const static xmlChar *			XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_FIRST_LIGHT   	=(const xmlChar*)"f";
const static xmlChar *			XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_ACKNOWLEGDE_LIGHT =(const xmlChar*)"a";
const static xmlChar *			XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_SEC_ATTR_LIGHT    =(const xmlChar*)"s";
const static xmlChar *			XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_USEC_ATTR_LIGHT   =(const xmlChar*)"u";
const static xmlChar *			XML_ELEMENT_NODE_GED_RECORD_DURATION_ATTR_LIGHT	  	=(const xmlChar*)"d";
const static xmlChar *			XML_ELEMENT_NODE_GED_RECORD_CONTENT_LIGHT               =(const xmlChar*)"c";
const static xmlChar *			XML_ELEMENT_NODE_GED_RECORD_STAT_NTA_ATTR_LIGHT		=(const xmlChar*)"a";
const static xmlChar *			XML_ELEMENT_NODE_GED_RECORD_STAT_NTS_ATTR_LIGHT		=(const xmlChar*)"s";
const static xmlChar *			XML_ELEMENT_NODE_GED_RECORD_STAT_NTH_ATTR_LIGHT		=(const xmlChar*)"h";
const static xmlChar *			XML_ELEMENT_NODE_GED_RECORD_STAT_NTR_ATTR_LIGHT		=(const xmlChar*)"r";

// GED / GEDQ parameters default values
#ifdef __GED__
#define	GED_SYSLOG_LOC_DFT		0x80000000			// stdout / stderr

#define	GED_SYSLOG_LEV_NONE		0x00000000			// no log
#define	GED_SYSLOG_LEV_CONNECTION	0x00000001			// connections log
#define	GED_SYSLOG_LEV_DETAIL_CONN	0x00000002			// detailed connections log
#define	GED_SYSLOG_LEV_PKT_HEADER	0x00000004			// pkt header log
#define	GED_SYSLOG_LEV_PKT_CONTENT	0x00000008			// pkt content log
#define	GED_SYSLOG_LEV_BKD_OPERATION	0x00000010			// backend operation log
#define	GED_SYSLOG_LEV_HTTP_HEADER	0x00000020			// http header log
#define	GED_SYSLOG_LEV_SSL		0x00000040			// ssl trace log
#if defined(__GED_DEBUG_SQL__) || defined(__GED_DEBUG_SEM__) 
#define	GED_SYSLOG_LEV_DEBUG		0x00000080			// debug log
#endif
#define	GED_SYSLOG_LEV_DFT		GED_SYSLOG_LEV_CONNECTION	// default log verbosity

#define	GED_SYSLOG_INFO			LOG_INFO			// syslog binding
#define	GED_SYSLOG_WARNING		LOG_WARNING			// syslog binding
#define	GED_SYSLOG_ERROR		LOG_ERR				// syslog binding

#define GED_KEEP_ALIVE_DFT		0				// no target sync
#define GED_DATA_SYNCS_DFT		120				// async target timer
#define GED_DATA_SYNCB_DFT		512				// async target bytes threshold
#define GED_PKT_POOL_DFT		30				// pool emission
#define GED_TTL_TIMER_DFT		300				// ttl thread timer
#define GED_MAX_CONN_IN_DFT		0				// maximum connections in
#ifdef __GED_TUN__
#define GED_TUN_ENABLE_DFT		0				// ged tun enable default
#define GED_MAX_TUN_OUT_DFT		0				// ged tun maximum connections out
#endif
#define GED_TLS_VERIFY_PEER_DFT		true				// ssl peer verification
#endif
#define GED_ACK_TIMEOUT_DFT		5				// ack timer
#ifdef __GEDQ__
#define GED_TLS_VERIFY_PEER_DFT		false				// ssl peer verification
#define GED_KEEP_ALIVE_DFT		30				// gedq tunnel default heartbeating
#endif

#define GED_HTTP_PROXY_AUTH_NONE	0				// no proxy auth method specified
#define GED_HTTP_PROXY_AUTH_BASIC	1				// basic proxy auth method specified
#ifdef __GED_NTLM__
#define GED_HTTP_PROXY_AUTH_NTLM	2				// ntlm proxy auth method specified
#endif

const static CString			GED_HTTP_REGEX_CRLF		("\r\n");

// http tunable request
struct TGEDHttpReqCfg
{
	CString				cmd;				// http command
	CString				vrs;				// http version
	CString				agt;				// http user agent
	CString				typ;				// http content type
	int				zlv;				// zlib comp level whenever used
};

// http tunable answer
#ifdef __GED__
struct TGEDHttpAswCfg
{
	CString				vrs;				// http version
	CString				srv;				// http server chain
	CString				typ;				// http content type
	int				zlv;				// zlib comp level whenever used
};
#endif

// binding information
struct TGEDBindCfg
{
	CString				addr;				// address to bind to
	unsigned long			port;				// port to bind to
	CString				sock;				// socket file to bind to
	CString				bind;				// optionnal stack to bind to
};

// proxy binding information
struct TGEDHttpProxyCfg : public TGEDBindCfg
{
	int				auth;				// proxy auth method
	CString				user;				// proxy user supplied
	CString				pass;				// proxy password supplied
};

// tls information
struct TGEDTlsCfg
{
	CString				ca;				// certificate authorities file
	CString				crt;				// certificate file
	CString				key;				// key file
	bool				vfy;				// peer verification
#ifdef __GED__
	CString				dhp;				// Diffie Hellman key file
#endif
	CString				cph;				// ssl cipher string
};

// GED target struct
#ifdef __GED__
struct TGEDRelayToCfg
{
	TGEDBindCfg			bind;				// target binding
	unsigned long			ackto;				// target specific ack timeout
	unsigned long			pool;				// target pool
	unsigned long			kpalv;				// sync connection heartbeating
	unsigned long			syncs;				// async connection timer
	unsigned long			syncb;				// async connection bytes threshold
	TGEDTlsCfg			tls;				// target ssl
	TGEDHttpProxyCfg		proxy;				// potential http proxy to be used
	TGEDHttpReqCfg			http;				// specific tuned http request
};
#endif

// packet field definition
struct TGEDPktFieldCfg
{
	TData				type;				// field type
	CString				name;				// field name
	int				meta;				// meta field (0/1)
};

// data packet definition
struct TGEDPktCfg
{
	int				type;				// packet type (id)
	CString				name;				// packet name
	TBuffer <TGEDPktFieldCfg>	fields;				// packet data fields
	TBuffer <size_t>		keyidc;				// packet key

	CString				hash;				// packet md5 hash

	bool operator <			(const TGEDPktCfg &) const;	// used to sort and generate config md5sum
};

// GED loaded configuration
#ifdef __GED__
struct TGEDCfg
{
	int				logloc;				// syslog local id 
	int				loghloc;			// syslog history local id 
	int				loglev;				// log verbosity

	uid_t				uid;				// uid to switch to
	gid_t				gid;				// gid to switch to

	TBuffer <TGEDBindCfg>		lst;				// interfaces to listen on
	TGEDTlsCfg			tls;				// common interfaces ssl specification
	unsigned long			ackto;				// backwarding ack timer
	unsigned long			pool;				// backwarding pool
	TGEDHttpAswCfg			http;				// common tuned http answer

	CString				fifo;				// fifo file to read at if any
	CString				fifofs;				// fifo file fields separator if any
	CString				fifors;				// fifo file records separator if any

	TBuffer <TGEDRelayToCfg>	rlyto;				// targets to relay to

	int				maxin;				// max connections in ctx
	CStrings			alwsyncfrm;			// ged sync sources restriction
	CStrings			alwreqfrm;			// gedq sources full req identification

	#ifdef __GED_TUN__
	int				maytun;				// whereas ged might tun
	int				maxtun;				// max tun conn out per ctx
	CStrings			alwtunfrm;			// tun allowed sources
	CStrings			alwtunto;			// tun allowed targets
	#endif

	TBuffer <TGEDPktCfg>		pkts;				// known packet types

	CString				bkd;				// backend module to be loaded
	TKeyBuffer <CString, CStrings>	bkdcfg;				// backend specific parameters
};
#endif

// GEDQ loaded configuration
#ifdef __GEDQ__
struct TGEDQCfg
{
	TGEDBindCfg			bind;				// target to bind to
	TGEDTlsCfg			tls;				// ssl to bind to 
	TGEDHttpProxyCfg		proxy;				// potential http proxy to be used
	TGEDHttpReqCfg			http;				// http tuned request
	unsigned long			ackto;				// target ack timer
	unsigned long			kpalv;				// tun connection heartbeating
	unsigned long			pmaxr;				// peek max records if any

	TBuffer <TGEDPktCfg>		pkts;				// known packet types
};
#endif

// http tuned request context
struct TGEDHttpReqCtx : public TGEDHttpReqCfg
{
	CString				host;				// host chain
	bool				kpalv;				// connection chain
};

// http custom answer
#ifdef __GED__
struct TGEDHttpAswCtx : public TGEDHttpAswCfg
{
	bool				kpalv;				// connection chain
};
#endif

// ack ctx commodity
struct TGEDAckCtx
{
	TGEDAckCtx			() THROWABLE;
	~TGEDAckCtx			();

	int				sem;

	pthread_t			tmrthd;

	CString				hash;
	bool				ack;
};

#ifdef __GED__
bool					GEDInit			(TGEDCfg &);
bool					LoadGEDCfg		(const CString &, TGEDCfg &, int =0);
#endif

#ifdef __GEDQ__
bool					GEDQInit		(TGEDQCfg &);
bool					LoadGEDQCfg		(const CString &, TGEDQCfg &, int =0);
#endif

CString					HashGEDPktCfg		(const TBuffer <TGEDPktCfg> &);
CString					HashGEDPktCfg		(const TGEDPktCfg *);
CString					GetGEDRandomHash	();

bool					GetStrAddrToAddrIn	(const CString &, struct in_addr *);

TGEDPktOut  				NewGEDPktOut		(TGEDPktIn *);
TGEDPktOut 				NewGEDPktOut		(CString, long, long, void *, long);
TGEDPktOut 				NewGEDPktOut		(struct in_addr, long, long, void *, long);
#ifdef __GED__
TGEDPktOut 				NewGEDPktOut		(TGEDRcd *);
#endif
long					SizeOfGEDPktOut		(TGEDPktOut);
void					DeleteGEDPktOut		(TGEDPktOut &);
TGEDPktIn  *				NewGEDPktIn		(TGEDPktIn *);
void					DeleteGEDPktIn		(TGEDPktIn *&);
#ifdef __GED__
TGEDRcd *				NewGEDRcd		(TGEDRcd *);
#endif
#ifdef __GEDQ__
TGEDRcd *				NewGEDRcd		(TGEDPktIn *);
#endif
void					DeleteGEDRcd		(TGEDRcd *&);

TGEDPktCfg *				GEDPktInToCfg		(TGEDPktIn *, const TBuffer <TGEDPktCfg> &);
CString					GEDPktInToHeaderString	(TGEDPktIn *);
CString					GEDPktInToContentString	(TGEDPktIn *, TGEDPktCfg *);

bool					GEDRcdToXmlNode		(TGEDRcd *, const TBuffer <TGEDPktCfg> &, xmlNodePtr &);
bool					GEDRcdToXmlNodeLight	(TGEDRcd *, const TBuffer <TGEDPktCfg> &, xmlNodePtr &);
#ifdef __GED__
TGEDRcd *				GEDXmlNodeToRcd		(const TBuffer <TGEDPktCfg> &, xmlNodePtr, const bool =true);
CString					GEDRcdToString		(TGEDRcd *, const TBuffer <TGEDPktCfg> &);
#endif

TBuffer <TData>				GEDPktCfgToTData	(TGEDPktCfg *);
#ifdef __GED__
void					QuickSortGEDARcdsAsc	(TBuffer <TGEDRcd *> &, const int, const int);
void					QuickSortGEDARcdsDsc	(TBuffer <TGEDRcd *> &, const int, const int);
void					QuickSortGEDSRcdsAsc	(TBuffer <TGEDRcd *> &, const int, const int);
void					QuickSortGEDSRcdsDsc	(TBuffer <TGEDRcd *> &, const int, const int);
void					QuickSortGEDHRcdsAsc	(TBuffer <TGEDRcd *> &, const int, const int);
void					QuickSortGEDHRcdsDsc	(TBuffer <TGEDRcd *> &, const int, const int);
#endif
struct TGEDPktInCtx;
TGEDPktInCtx *				NewGEDPktInCtx		();
void					DeleteGEDPktInCtx	(TGEDPktInCtx *&);

typedef	void				(*RecvGEDPktFromCb)	(const CString &, long, bool, TGEDPktIn *, void *);

int					SendRawGEDPkt		(int, TBuffer <TGEDPktOut> &);
int					SendHttpGEDPktToTgt	(int, SSL *, TGEDHttpReqCtx *, TBuffer <TGEDPktOut> &, CString =CString());
#ifdef __GED__
int 					SendHttpGEDPktToSrc	(int, SSL *, TGEDHttpAswCtx *, TBuffer <TGEDPktOut> &, CString =CString());
#endif
int 					RecvRawGEDPktFromSkt	(int, TGEDPktInCtx *, long, RecvGEDPktFromCb, void *);
int 					RecvHttpGEDPktFromSkt	(int, SSL *, TGEDPktInCtx *, long, RecvGEDPktFromCb, void *);
int					RecvRawGEDPktFromBuf 	(void *, long, TGEDPktInCtx *, RecvGEDPktFromCb, void *);
int					RecvHttpGEDPktFromBuf	(void *, long, TGEDPktInCtx *, RecvGEDPktFromCb, void *);

int					LockGEDAckCtx		(TGEDAckCtx *);
int					UnLockGEDAckCtx		(TGEDAckCtx *);

CString					Base64Encode		(const CString &);

#ifdef __GEDQ__
int 					IsUtf8 			(const CString &);
#endif

#ifdef __GED_NTLM__
CString					GetHttpProxyAuthNTLM1	();
CString					GetHttpProxyAuthNTLM3	(const CString &, const CString &, const CString &);
#endif

#endif
