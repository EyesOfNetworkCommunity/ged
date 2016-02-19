/*****************************************************************************************************************************************
 cgedbackendmysql.h - ged mysql backend -
*****************************************************************************************************************************************/

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

#ifndef __CGEDACKENDMYSQL_H__
#define __CGEDACKENDMYSQL_H__

#include "ged.h"

#include <mysql/mysql.h>
#include <mysql/errmsg.h>

#define TIMEVAL_MSEC_SUBTRACT(a,b) ((((a).tv_sec - (b).tv_sec) * 1000) + ((a).tv_usec - (b).tv_usec) / 1000)

#define VARCHAR_LEN_STR_DEFAULT "2048"

//----------------------------------------------------------------------------------------------------------------------------------------
// backend configuration known parameters
//----------------------------------------------------------------------------------------------------------------------------------------
const static CString GEDCfgMySQLHost					("mysql_host");
const static CString GEDCfgMySQLDatabase				("mysql_database");
const static CString GEDCfgMySQLLogin					("mysql_login");
const static CString GEDCfgMySQLPassword				("mysql_password");
const static CString GEDCfgMySQLOptConnectTimeout			("mysql_opt_connect_timeout");
const static CString GEDCfgMySQLOptCompress				("mysql_opt_compress");
const static CString GEDCfgMySQLOptReconnect				("mysql_opt_reconnect");
const static CString GEDCfgMySQLModeNoBackslashEscapes			("mysql_mode_no_backslash_escapes");
const static CString GEDCfgMySQLModeLtGtFilter				("mysql_mode_lt_gt_filter");
const static CString GEDCfgMySQLModeQuotFilter				("mysql_mode_quote_filter");
const static CString GEDCfgMySQLVarcharLength				("mysql_varchar_length");

//----------------------------------------------------------------------------------------------------------------------------------------
// backend default values
//----------------------------------------------------------------------------------------------------------------------------------------
const static CString GED_MYSQL_HOST_DFT					("localhost");
const static UInt32  GED_MYSQL_PORT_DFT					(MYSQL_PORT);

const static CString GED_MYSQL_TB_TYPE					("INNODB");

const static CString GED_MYSQL_PACKET_TYPE_TB				("pkt_type");
const static CString GED_MYSQL_PACKET_TYPE_TB_COL_ID			("pkt_type_id");
const static CString GED_MYSQL_PACKET_TYPE_TB_COL_NAME			("pkt_type_name");
const static CString GED_MYSQL_PACKET_TYPE_TB_COL_HASH			("pkt_type_hash");
const static CString GED_MYSQL_PACKET_TYPE_TB_COL_VERS			("pkt_type_vers");

const static size_t GED_MYSQL_PACKET_TYPE_TB_COL_ID_IDX			(0);
const static size_t GED_MYSQL_PACKET_TYPE_TB_COL_NAME_IDX		(1);
const static size_t GED_MYSQL_PACKET_TYPE_TB_COL_HASH_IDX		(2);
const static size_t GED_MYSQL_PACKET_TYPE_TB_COL_VERS_IDX		(3);

const static CString GED_MYSQL_DATA_TB_COL_ID				("id");

const static CString GED_MYSQL_DATA_TB_COL_QUEUE			("queue");
const static CString GED_MYSQL_DATA_TBL_QUEUE_ACTIVE			("_queue_active");
const static CString GED_MYSQL_DATA_TBL_QUEUE_HISTORY		("_queue_history");
const static CString GED_MYSQL_DATA_TBL_QUEUE_SYNC		("_queue_sync");

const static CString GED_MYSQL_DATA_TB_COL_OCC				("occ");
const static CString GED_MYSQL_DATA_TB_COL_OTV_SEC			("o_sec");
const static CString GED_MYSQL_DATA_TB_COL_OTV_USEC			("o_usec");
const static CString GED_MYSQL_DATA_TB_COL_LTV_SEC			("l_sec");
const static CString GED_MYSQL_DATA_TB_COL_LTV_USEC			("l_usec");
const static CString GED_MYSQL_DATA_TB_COL_RTV_SEC			("r_sec");
const static CString GED_MYSQL_DATA_TB_COL_RTV_USEC			("r_usec");
const static CString GED_MYSQL_DATA_TB_COL_MTV_SEC			("m_sec");
const static CString GED_MYSQL_DATA_TB_COL_MTV_USEC			("m_usec");
const static CString GED_MYSQL_DATA_TB_COL_FTV_SEC			("f_sec");
const static CString GED_MYSQL_DATA_TB_COL_FTV_USEC			("f_usec");
const static CString GED_MYSQL_DATA_TB_COL_ATV_SEC			("a_sec");
const static CString GED_MYSQL_DATA_TB_COL_ATV_USEC			("a_usec");
const static CString GED_MYSQL_DATA_TB_COL_REASON          		("reason");
const static CString GED_MYSQL_DATA_TB_COL_SRC          		("src");
const static CString GED_MYSQL_DATA_TB_COL_TGT          		("tgt");
const static CString GED_MYSQL_DATA_TB_COL_REQ          		("req");

const static size_t GED_MYSQL_DATA_TB_COL_ID_IDX			(0);
const static size_t GED_MYSQL_DATA_TB_COL_QUEUE_IDX			(1);
const static size_t GED_MYSQL_DATA_TB_COL_OCC_IDX			(2);
const static size_t GED_MYSQL_DATA_TB_COL_OTV_SEC_IDX			(3);
const static size_t GED_MYSQL_DATA_TB_COL_OTV_USEC_IDX			(4);
const static size_t GED_MYSQL_DATA_TB_COL_LTV_SEC_IDX			(5);
const static size_t GED_MYSQL_DATA_TB_COL_LTV_USEC_IDX			(6);
const static size_t GED_MYSQL_DATA_TB_COL_RTV_SEC_IDX			(7);
const static size_t GED_MYSQL_DATA_TB_COL_RTV_USEC_IDX			(8);
const static size_t GED_MYSQL_DATA_TB_COL_MTV_SEC_IDX			(9);
const static size_t GED_MYSQL_DATA_TB_COL_MTV_USEC_IDX			(10);
const static size_t GED_MYSQL_DATA_TB_COL_FTV_SEC_IDX			(11);
const static size_t GED_MYSQL_DATA_TB_COL_FTV_USEC_IDX			(12);
const static size_t GED_MYSQL_DATA_TB_COL_ATV_SEC_IDX			(13);
const static size_t GED_MYSQL_DATA_TB_COL_ATV_USEC_IDX			(14);
const static size_t GED_MYSQL_DATA_TB_COL_REASON_IDX          		(15);
const static size_t GED_MYSQL_DATA_TB_COL_SRC_IDX          		(16);
const static size_t GED_MYSQL_DATA_TB_COL_TGT_IDX          		(17);
const static size_t GED_MYSQL_DATA_TB_COL_REQ_IDX          		(18);
const static size_t GED_MYSQL_DATA_TB_COL_FIRST_DATA_IDX          	(19);

//----------------------------------------------------------------------------------------------------------------------------------------
// mysql backend specific api
//----------------------------------------------------------------------------------------------------------------------------------------
class CGEDBackEndMySQL : public CGEDBackEnd
{
	public :

		CGEDBackEndMySQL		();
		virtual ~CGEDBackEndMySQL	();

	public :

		virtual bool			Initialize		(const TGEDCfg &);
		virtual void			Finalize		();

		virtual bool			Push			(const CString &, const int, const TGEDPktIn *);
		virtual bool			Drop			(const CString &, const int, const TGEDPktIn *);
		virtual TBuffer <TGEDRcd *>	Peek			(const CString &, const int, const TGEDPktIn *);

		virtual bool			Recover			(const TBuffer <TGEDRcd *> &inGEDRecords, 
									 void (*) (const UInt32, const UInt32, const TGEDRcd *));

		virtual TGEDStatRcd *           Stat                    () const;

	protected :

		virtual bool			ReadCfgCache		(TKeyBuffer <UInt32, CString> &, UInt32 &);
		virtual bool			WriteCfgCache		();

		virtual bool                    NotifyVersionUpgrade    (const UInt32 inOldVersion);
		virtual bool			NotifyPktCfgChange	(const long, const TGEDPktCfgChange);

	protected :

		CString				GetSelectQuery		(const int, const TGEDPktIn *, const TGEDPktCfg *, const CString &, 
									 const UInt32 &, const UInt32 &, const UInt32 &, const UInt32 &);

		bool				ExecuteSQLQuery		(const CString &);

		TGEDARcd * 			MySQLRowToARcd		(MYSQL_ROW &, TGEDPktCfg *);
		TGEDHRcd *			MySQLRowToHRcd		(MYSQL_ROW &, TGEDPktCfg *);
		TGEDSRcd *			MySQLRowToSRcd		(MYSQL_ROW &, TGEDPktCfg *);
		
	protected :

		MYSQL				m_MYSQL;

		CString				m_MySQLHost;
		UInt32				m_MySQLPort;
		CString				m_MySQLDatabase;
		CString				m_MySQLLogin;
		CString				m_MySQLPassword;
		CString				m_MySQLOptReconnect;
		CString				m_MySQLNoBackslashEscapes;
		CString				m_MySQLVarcharLength;
		bool				m_MySQLQuotFilter;
		bool				m_MySQLLtGtFilter;

	protected :

		unsigned long			m_Id;
    unsigned long			m_Ida;
    unsigned long			m_Idh;
    unsigned long			m_Ids;

		unsigned long			m_Na;
		unsigned long			m_Nh;
		unsigned long			m_Ns;
		unsigned long			m_Nr;

	protected :

		pthread_t			m_TTLTimerTh;
		static void *			m_TTLTimerCB		(void *);

		static volatile bool		m_fTTL;

	protected :

		SECTION_DYNAMIC_METACLASS;
};

DECLARE_DYNAMIC_METACLASS ('bkmy', CGEDBackEndMySQL, CGEDBackEnd);

#endif
