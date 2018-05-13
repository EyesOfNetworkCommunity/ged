/*****************************************************************************************************************************************
 cgedbackendmysql.c
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

#include "cgedbackendmysql.h"

#define GED_MYSQL_FILTER_STRING(str,ltgt,quot) 			\
	if (quot) {						\
		str.Substitute(CString("\'"),CString(" ")); 	\
		str.Substitute(CString("\""),CString(" ")); }	\
	if (ltgt) { 						\
		str.Substitute(CString("<" ),CString(" ")); 	\
		str.Substitute(CString(">" ),CString(" ")); }

//----------------------------------------------------------------------------------------------------------------------------------------
// metaclass code and module entry point resolution
//----------------------------------------------------------------------------------------------------------------------------------------
RESOLVE_DYNAMIC_METACLASS (CGEDBackEndMySQL);
DECLARE_METAMODULE_EXPORT (CGEDBackEndMySQL);

//----------------------------------------------------------------------------------------------------------------------------------------
// static resolution
//----------------------------------------------------------------------------------------------------------------------------------------
volatile bool CGEDBackEndMySQL::m_fTTL = false;

//----------------------------------------------------------------------------------------------------------------------------------------
// constructor
//----------------------------------------------------------------------------------------------------------------------------------------
CGEDBackEndMySQL::CGEDBackEndMySQL   		()
                 :m_MySQLHost       	 	(GED_MYSQL_HOST_DFT),
                  m_MySQLPort        		(GED_MYSQL_PORT_DFT),
                  m_MySQLDatabase    		(),
                  m_MySQLLogin      	 	(),
                  m_MySQLPassword    		(),
		  m_MySQLOptReconnect		(),
		  m_MySQLLtGtFilter		(true),
		  m_MySQLQuotFilter		(true),
		  m_MySQLNoBackslashEscapes	("true"),
		  m_MySQLVarcharLength		(VARCHAR_LEN_STR_DEFAULT),
		  m_Na		     		(0L),
		  m_Nh		     		(0L),
		  m_Ns		     		(0L),
		  m_Nr		     		(0L),
      m_Id            (1L),
		  m_Ida		     		(1L),
      m_Idh           (1L),
      m_Ids           (1L)
        
{
	::mysql_init (&m_MYSQL);
}

//----------------------------------------------------------------------------------------------------------------------------------------
// destructor
//----------------------------------------------------------------------------------------------------------------------------------------
CGEDBackEndMySQL::~CGEDBackEndMySQL ()
{ }

//----------------------------------------------------------------------------------------------------------------------------------------
// backend initialization
//----------------------------------------------------------------------------------------------------------------------------------------
bool CGEDBackEndMySQL::Initialize (const TGEDCfg &inGEDCfg)
{
	CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_BKD_OPERATION, "MYSQL backend loading [" + 
				   CString(MYSQL_SERVER_VERSION) + "]");

	if (inGEDCfg.bkdcfg.Contains(GEDCfgMySQLHost) && const_cast<TGEDCfg&>(inGEDCfg).bkdcfg[GEDCfgMySQLHost].GetLength()>0)
	{
		m_MySQLHost = *const_cast <TGEDCfg &> (inGEDCfg).bkdcfg[GEDCfgMySQLHost][0];

		if (m_MySQLHost.Find(CString(":")))
		{
			m_MySQLPort =  m_MySQLHost.Cut(CString(":"))[1]->ToULong();
			m_MySQLHost = *m_MySQLHost.Cut(CString(":"))[0];
		}
	}

	if (inGEDCfg.bkdcfg.Contains(GEDCfgMySQLDatabase) && const_cast<TGEDCfg&>(inGEDCfg).bkdcfg[GEDCfgMySQLDatabase].GetLength()>0)
		m_MySQLDatabase = *const_cast <TGEDCfg &> (inGEDCfg).bkdcfg[GEDCfgMySQLDatabase][0];
	else
	{
		CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, "MYSQL backend missing \"" + 
				GEDCfgMySQLDatabase + "\" parameter specification");
		return false;
	}

	if (inGEDCfg.bkdcfg.Contains(GEDCfgMySQLLogin) && const_cast<TGEDCfg&>(inGEDCfg).bkdcfg[GEDCfgMySQLLogin].GetLength()>0)
		m_MySQLLogin = *const_cast <TGEDCfg &> (inGEDCfg).bkdcfg[GEDCfgMySQLLogin][0];

	if (inGEDCfg.bkdcfg.Contains(GEDCfgMySQLPassword) && const_cast<TGEDCfg&>(inGEDCfg).bkdcfg[GEDCfgMySQLPassword].GetLength()>0)
		m_MySQLPassword = *const_cast <TGEDCfg &> (inGEDCfg).bkdcfg[GEDCfgMySQLPassword][0];

	if (inGEDCfg.bkdcfg.Contains(GEDCfgMySQLOptConnectTimeout) && const_cast<TGEDCfg&>(inGEDCfg).bkdcfg[GEDCfgMySQLOptConnectTimeout].GetLength()>0)
	{
		UInt32 inConnectTimeout (const_cast<TGEDCfg&>(inGEDCfg).bkdcfg[GEDCfgMySQLOptConnectTimeout][0]->ToULong());
		::mysql_options (&m_MYSQL, MYSQL_OPT_CONNECT_TIMEOUT, (const char*)&inConnectTimeout);
	}

	if (inGEDCfg.bkdcfg.Contains(GEDCfgMySQLOptCompress) && const_cast<TGEDCfg&>(inGEDCfg).bkdcfg[GEDCfgMySQLOptCompress].GetLength()>0)
		if (const_cast<TGEDCfg&>(inGEDCfg).bkdcfg[GEDCfgMySQLOptCompress][0]->ToBool())
			::mysql_options (&m_MYSQL, MYSQL_OPT_COMPRESS, 0);
	
	if (inGEDCfg.bkdcfg.Contains(GEDCfgMySQLOptReconnect) && const_cast<TGEDCfg&>(inGEDCfg).bkdcfg[GEDCfgMySQLOptReconnect].GetLength()>0)
	{
		m_MySQLOptReconnect = *const_cast<TGEDCfg&>(inGEDCfg).bkdcfg[GEDCfgMySQLOptReconnect][0];
		my_bool inReconnect = m_MySQLOptReconnect.ToBool();
		::mysql_options (&m_MYSQL, MYSQL_OPT_RECONNECT, (const char*)&inReconnect);
	}

	if (inGEDCfg.bkdcfg.Contains(GEDCfgMySQLModeNoBackslashEscapes) && const_cast<TGEDCfg&>(inGEDCfg).bkdcfg[GEDCfgMySQLModeNoBackslashEscapes].GetLength()>0)
		m_MySQLNoBackslashEscapes = *const_cast <TGEDCfg &> (inGEDCfg).bkdcfg[GEDCfgMySQLModeNoBackslashEscapes][0];

	if (inGEDCfg.bkdcfg.Contains(GEDCfgMySQLModeLtGtFilter) && const_cast<TGEDCfg&>(inGEDCfg).bkdcfg[GEDCfgMySQLModeLtGtFilter].GetLength()>0)
		m_MySQLLtGtFilter = const_cast <TGEDCfg &> (inGEDCfg).bkdcfg[GEDCfgMySQLModeLtGtFilter][0]->ToBool();

	if (inGEDCfg.bkdcfg.Contains(GEDCfgMySQLModeQuotFilter) && const_cast<TGEDCfg&>(inGEDCfg).bkdcfg[GEDCfgMySQLModeQuotFilter].GetLength()>0)
		m_MySQLQuotFilter = const_cast <TGEDCfg &> (inGEDCfg).bkdcfg[GEDCfgMySQLModeQuotFilter][0]->ToBool();

	if (inGEDCfg.bkdcfg.Contains(GEDCfgMySQLVarcharLength) && const_cast<TGEDCfg&>(inGEDCfg).bkdcfg[GEDCfgMySQLVarcharLength].GetLength()>0)
		m_MySQLVarcharLength = *const_cast <TGEDCfg &> (inGEDCfg).bkdcfg[GEDCfgMySQLVarcharLength][0];

	if (!::mysql_real_connect (&m_MYSQL, m_MySQLHost.Get(), m_MySQLLogin.Get(), m_MySQLPassword.Get(), m_MySQLDatabase.Get(),
				    m_MySQLPort, NULL, 0))
	{
		CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_CONNECTION, "MYSQL backend "+CString(::mysql_error(&m_MYSQL)));
		return false;
	}

	if (m_MySQLNoBackslashEscapes.ToBool())
		if (!ExecuteSQLQuery (CString("SET sql_mode=\'NO_BACKSLASH_ESCAPES\'"))) return false;

	CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_CONNECTION, "MYSQL backend successfully connected to database \"" + 
				   m_MySQLDatabase + "\" on " + m_MySQLHost + ":" + CString(m_MySQLPort) + " [" + 
				   CString(::mysql_get_server_info(&m_MYSQL)) + "]");

	CString outSQL ("CREATE TABLE IF NOT EXISTS " + GED_MYSQL_PACKET_TYPE_TB + " (" + 
			GED_MYSQL_PACKET_TYPE_TB_COL_ID 	+ " INTEGER NOT NULL PRIMARY KEY DEFAULT 0, " + 
			GED_MYSQL_PACKET_TYPE_TB_COL_NAME 	+ " VARCHAR(128) NOT NULL UNIQUE DEFAULT \'\', " +
			GED_MYSQL_PACKET_TYPE_TB_COL_HASH	+ " VARCHAR(128) NOT NULL DEFAULT \'\', " +
			GED_MYSQL_PACKET_TYPE_TB_COL_VERS	+ " INTEGER NOT NULL DEFAULT " + CString((SInt32)GED_VERSION) + 
			") ENGINE=" + GED_MYSQL_TB_TYPE);

	if (!ExecuteSQLQuery (outSQL)) return false;

	if (!CGEDBackEnd::Initialize (inGEDCfg)) return false;

	outSQL = "SELECT " + GED_MYSQL_PACKET_TYPE_TB_COL_ID + " FROM " + GED_MYSQL_PACKET_TYPE_TB;

	if (!ExecuteSQLQuery (outSQL)) return false;

	MYSQL_RES *inSQLRes = ::mysql_store_result (&m_MYSQL);
	#ifdef __GED_DEBUG_SQL__
	UInt32 nrows = ::mysql_num_rows (inSQLRes);
	CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_DEBUG, "MYSQL backend nrows = " + CString(nrows));
	#endif
	MYSQL_ROW  inSQLRow; TBuffer <long> inPktTypes; while (inSQLRow = ::mysql_fetch_row (inSQLRes))
		inPktTypes += CString(inSQLRow[0]).ToLong();
	::mysql_free_result (inSQLRes);

	for (size_t i=0; i<m_GEDCfg.pkts.GetLength(); i++)
	{
		if (!inPktTypes.Find(m_GEDCfg.pkts[i]->type))
		{
			CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, 
				"MYSQL backend missing table reference in table \"" + GED_MYSQL_PACKET_TYPE_TB + 
				"\" for declared packet type \"" + CString((long)m_GEDCfg.pkts[i]->type) + "\"; it seems that the \"" + 
				m_MySQLDatabase + "\" database has been altered since last execution or the backend has changed");
			return false;
		}
	}

	if ((inSQLRes = ::mysql_list_tables (&m_MYSQL, NULL)) == NULL)
	{
		CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, "MYSQL backend " + 
			CString(::mysql_error(&m_MYSQL)));
		return false;
	}

	CStrings inTables; while (inSQLRow = ::mysql_fetch_row (inSQLRes)) inTables += CString(inSQLRow[0]);

	::mysql_free_result (inSQLRes);

	inTables -= GED_MYSQL_PACKET_TYPE_TB;
 
  
	for (size_t i=0; i<m_GEDCfg.pkts.GetLength(); i++)
	{
		bool found=false; for (size_t j=0; j<inTables.GetLength() && !found; j++)
			found = m_GEDCfg.pkts[i]->name == *inTables[j];

		if (!found)
		{
			CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, "MYSQL backend missing table \"" + 
				m_GEDCfg.pkts[i]->name + "\" definition for declared packet type \"" + 
				CString((long)m_GEDCfg.pkts[i]->type) + "\"; it seems that the \"" + m_MySQLDatabase + 
				"\" database has been altered since last execution or the backend has changed");
			return false;
		}
	}
	
	if (m_TTLTimer > 0)
	{
		if (::pthread_create (&m_TTLTimerTh, NULL, CGEDBackEndMySQL::m_TTLTimerCB, this) != 0)
		{
			CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, 
				CString("MYSQL backend could not launch TTL thread"));

			return false;
		}

		while (!CGEDBackEndMySQL::m_fTTL) ::usleep (250);
	}
	else
		CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_BKD_OPERATION, 
			CString("MYSQL backend TTL handling disabled"));
	
 // HERE I REMOVE new tables to keep loop on pkt_type only
 for (size_t j=0; j<inTables.GetLength(); j++)
	{
  inTables -= *inTables[j] + GED_MYSQL_DATA_TBL_QUEUE_ACTIVE;
  inTables -= *inTables[j] + GED_MYSQL_DATA_TBL_QUEUE_HISTORY;
  inTables -= *inTables[j] + GED_MYSQL_DATA_TBL_QUEUE_SYNC;
 }
 // Let's continuing as previously :P
 
	for (size_t j=0; j<inTables.GetLength(); j++)
	{
		outSQL = "SELECT COUNT(*) FROM " + *inTables[j] + GED_MYSQL_DATA_TBL_QUEUE_ACTIVE;
		if (!ExecuteSQLQuery (outSQL)) return false;
		inSQLRes = ::mysql_store_result (&m_MYSQL); inSQLRow = ::mysql_fetch_row (inSQLRes);
		m_Na += CString(inSQLRow[0]).ToULong();
		::mysql_free_result (inSQLRes);

		outSQL = "SELECT COUNT(*) FROM " + *inTables[j] + GED_MYSQL_DATA_TBL_QUEUE_HISTORY;
		if (!ExecuteSQLQuery (outSQL)) return false;
		inSQLRes = ::mysql_store_result (&m_MYSQL); inSQLRow = ::mysql_fetch_row (inSQLRes);
		m_Nh += CString(inSQLRow[0]).ToULong();
		::mysql_free_result (inSQLRes);

		outSQL = "SELECT COUNT(*) FROM " + *inTables[j] + GED_MYSQL_DATA_TBL_QUEUE_SYNC;
		if (!ExecuteSQLQuery (outSQL)) return false;
		inSQLRes = ::mysql_store_result (&m_MYSQL); inSQLRow = ::mysql_fetch_row (inSQLRes);
		m_Ns += CString(inSQLRow[0]).ToULong();
		::mysql_free_result (inSQLRes);
   
   	outSQL = "SELECT MAX(" + GED_MYSQL_DATA_TB_COL_ID + ") FROM " + *inTables[j];
		if (!ExecuteSQLQuery (outSQL)) return false;
		inSQLRes = ::mysql_store_result (&m_MYSQL); inSQLRow = ::mysql_fetch_row (inSQLRes);
		m_Id = max(m_Id,(CString(inSQLRow[0]).ToULong()+1));
		::mysql_free_result (inSQLRes);

		outSQL = "SELECT MAX(" + GED_MYSQL_DATA_TB_COL_ID + ") FROM " + *inTables[j] + GED_MYSQL_DATA_TBL_QUEUE_ACTIVE;
		if (!ExecuteSQLQuery (outSQL)) return false;
		inSQLRes = ::mysql_store_result (&m_MYSQL); inSQLRow = ::mysql_fetch_row (inSQLRes);
		m_Ida = max(m_Ida,(CString(inSQLRow[0]).ToULong()+1));
		::mysql_free_result (inSQLRes);
   
    outSQL = "SELECT MAX(" + GED_MYSQL_DATA_TB_COL_ID + ") FROM " + *inTables[j] + GED_MYSQL_DATA_TBL_QUEUE_HISTORY;
		if (!ExecuteSQLQuery (outSQL)) return false;
		inSQLRes = ::mysql_store_result (&m_MYSQL); inSQLRow = ::mysql_fetch_row (inSQLRes);
		m_Idh = max(m_Idh,(CString(inSQLRow[0]).ToULong()+1));
		::mysql_free_result (inSQLRes);
   
    outSQL = "SELECT MAX(" + GED_MYSQL_DATA_TB_COL_ID + ") FROM " + *inTables[j] + GED_MYSQL_DATA_TBL_QUEUE_SYNC;
		if (!ExecuteSQLQuery (outSQL)) return false;
		inSQLRes = ::mysql_store_result (&m_MYSQL); inSQLRow = ::mysql_fetch_row (inSQLRes);
		m_Ids = max(m_Ids,(CString(inSQLRow[0]).ToULong()+1));
		::mysql_free_result (inSQLRes);
	}

	for (size_t z=m_aInc; z<=BUFFER_INC_65536 && ((m_Na>>m_aInc)>0L); z++) m_aInc = (TBufferInc)z;
	for (size_t z=m_hInc; z<=BUFFER_INC_65536 && ((m_Nh>>m_hInc)>0L); z++) m_hInc = (TBufferInc)z;
	for (size_t z=m_sInc; z<=BUFFER_INC_65536 && ((m_Ns>>m_sInc)>0L); z++) m_sInc = (TBufferInc)z;

	CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_BKD_OPERATION,
		"MYSQL backend handles " + CString(m_Na) + " active records, " + CString(m_Nh) + " history records, " + CString(m_Ns) + " sync records");

	return true;
}

//----------------------------------------------------------------------------------------------------------------------------------------
// finalize
//----------------------------------------------------------------------------------------------------------------------------------------
void CGEDBackEndMySQL::Finalize ()
{
	if (m_TTLTimer > 0)
	{
		::pthread_cancel (m_TTLTimerTh);
		::pthread_detach (m_TTLTimerTh);
	}

	CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_BKD_OPERATION, CString("MYSQL backend connection closed"));

	CGEDBackEnd::Finalize();

	::mysql_close (&m_MYSQL);
}

//----------------------------------------------------------------------------------------------------------------------------------------
// backend stat
//----------------------------------------------------------------------------------------------------------------------------------------
TGEDStatRcd * CGEDBackEndMySQL::Stat () const
{
	TGEDStatRcd *outGEDStatRcd = new TGEDStatRcd; ::bzero(outGEDStatRcd,sizeof(TGEDStatRcd));

	outGEDStatRcd->queue = GED_PKT_REQ_BKD_STAT;
	outGEDStatRcd->id    = classtag_cast(this);
	outGEDStatRcd->vrs   = MYSQL_VERSION_ID;
	outGEDStatRcd->nta   = m_Na;
	outGEDStatRcd->nts   = m_Ns;
	outGEDStatRcd->nth   = m_Nh;
	outGEDStatRcd->ntr   = m_Nr;

	return outGEDStatRcd;
}


//----------------------------------------------------------------------------------------------------------------------------------------
// push handling
//----------------------------------------------------------------------------------------------------------------------------------------
bool CGEDBackEndMySQL::Push (const CString &inAddr, const int inQueue, const TGEDPktIn *inGEDPktIn)
{
	m_Nr = 0L;

	if (inGEDPktIn == NULL)
	{
		CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, CString("MYSQL backend push on NULL packet"));
		return false;
	}

	if (inGEDPktIn->data == NULL)
	{
		CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, CString("MYSQL backend push on NULL content"));
		return false;
	}

	TGEDPktCfg *inGEDPktCfg (::GEDPktInToCfg (const_cast <TGEDPktIn *> (inGEDPktIn), m_GEDCfg.pkts));

	if (inGEDPktCfg == NULL)
	{
		CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, "MYSQL backend push on unknown packet type \"" + 
					   CString(inGEDPktIn->typ) + "\"");
		return false;
	}

	switch (inQueue)
	{
		case GED_PKT_REQ_BKD_ACTIVE :
		{
			TBuffer <TData> inTData (::GEDPktCfgToTData (inGEDPktCfg)); CChunk inChunk (inGEDPktIn->data, inGEDPktIn->len, inTData, false);
   
			CString outSQL = "SELECT * FROM " + 
						inGEDPktCfg->name + GED_MYSQL_DATA_TBL_QUEUE_ACTIVE +
					 " WHERE " + 
						GED_MYSQL_DATA_TB_COL_QUEUE + "=\'a\'";

			for (size_t i=0, j=0; i<inGEDPktCfg->keyidc.GetLength(); i++)
			{
				for (; j<*inGEDPktCfg->keyidc[i]; j++) inChunk++;

				switch (inGEDPktCfg->fields[*inGEDPktCfg->keyidc[i]]->type)
				{
					case DATA_STRING :
					{
						char *inStr=NULL; inChunk >> inStr; CString inString(inStr); if (inStr) delete [] inStr; 
						GED_MYSQL_FILTER_STRING(inString,m_MySQLLtGtFilter,m_MySQLQuotFilter);
						outSQL += " AND " + inGEDPktCfg->fields[*inGEDPktCfg->keyidc[i]]->name + " LIKE \'" + inString + "\'";
					}
					break;

					case DATA_SINT32 :
					{
						long inLong; inChunk >> inLong;
						outSQL += " AND " + inGEDPktCfg->fields[*inGEDPktCfg->keyidc[i]]->name + "=" + CString(inLong);
					}
					break;

					case DATA_FLOAT64 :
					{
						double inDouble; inChunk >> inDouble;
						outSQL += " AND " + inGEDPktCfg->fields[*inGEDPktCfg->keyidc[i]]->name + "=" + CString(inDouble);
					}
					break;
				}

				j++;
			}

			if (!ExecuteSQLQuery (outSQL)) return false;

			MYSQL_RES *inSQLRes = ::mysql_store_result (&m_MYSQL);
			#ifdef __GED_DEBUG_SQL__
			UInt32 nrows = ::mysql_num_rows (inSQLRes);
			CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_DEBUG, "MYSQL backend nrows = " + CString(nrows));
			#endif
			MYSQL_ROW  inSQLRow = ::mysql_fetch_row    (inSQLRes);
			
			if (inGEDPktCfg->keyidc.GetLength() > 0 && inSQLRow != NULL)
			{
				long 		id  = CString(inSQLRow[GED_MYSQL_DATA_TB_COL_ID_IDX]).ToLong();
				long 		occ = CString(inSQLRow[GED_MYSQL_DATA_TB_COL_OCC_IDX]).ToLong();
				time_t  	os  = CString(inSQLRow[GED_MYSQL_DATA_TB_COL_OTV_SEC_IDX]).ToULong();
				suseconds_t	ous = CString(inSQLRow[GED_MYSQL_DATA_TB_COL_OTV_USEC_IDX]).ToULong();
				time_t  	ls  = CString(inSQLRow[GED_MYSQL_DATA_TB_COL_LTV_SEC_IDX]).ToULong();
				suseconds_t	lus = CString(inSQLRow[GED_MYSQL_DATA_TB_COL_LTV_USEC_IDX]).ToULong();
				time_t  	ms  = CString(inSQLRow[GED_MYSQL_DATA_TB_COL_MTV_SEC_IDX]).ToULong();
				suseconds_t	mus = CString(inSQLRow[GED_MYSQL_DATA_TB_COL_MTV_USEC_IDX]).ToULong();
        time_t  	fs  = CString(inSQLRow[GED_MYSQL_DATA_TB_COL_FTV_SEC_IDX]).ToULong();
				suseconds_t	fus = CString(inSQLRow[GED_MYSQL_DATA_TB_COL_FTV_USEC_IDX]).ToULong();
				CString		src = CString(inSQLRow[GED_MYSQL_DATA_TB_COL_SRC_IDX]);

				::mysql_free_result (inSQLRes);

				if ((inGEDPktIn->req & GED_PKT_REQ_PUSH_MASK) == GED_PKT_REQ_PUSH_TMSP)
				{
					if (ls > inGEDPktIn->tv.tv_sec)
						return true;

					if (ls == inGEDPktIn->tv.tv_sec && lus > inGEDPktIn->tv.tv_usec)
						return true;
				}
				else
				{
					if (ms > inGEDPktIn->tv.tv_sec)
						return true;

					if (ms == inGEDPktIn->tv.tv_sec && mus > inGEDPktIn->tv.tv_usec)
						return true;
				}

				bool b=false; if (!src.Find(inAddr))
				{
					src += ";" + inAddr + "/" + CString((inGEDPktIn->req&GED_PKT_REQ_SRC_RELAY)?"1":"0");
					b = true;
				}
				else
				{
					if (inGEDPktIn->req&GED_PKT_REQ_SRC_RELAY) 
					{
						src.Substitute (inAddr+"/0", inAddr+"/1");
						b = true;
					}
				}

				if ((inGEDPktIn->req & GED_PKT_REQ_PUSH_MASK) == GED_PKT_REQ_PUSH_TMSP)
				{
					if (ls == inGEDPktIn->tv.tv_sec && lus == inGEDPktIn->tv.tv_usec)
					{
						CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_BKD_OPERATION, 
									   "MYSQL backend push on DUP packet type \"" + 
									   CString(inGEDPktIn->typ) + "\"");
						return true;
					}

					struct timeval tv; ::gettimeofday (&tv, NULL);

					outSQL = "UPDATE " + 
							inGEDPktCfg->name + GED_MYSQL_DATA_TBL_QUEUE_ACTIVE +
						 " SET " +
							GED_MYSQL_DATA_TB_COL_OCC + "=" + CString(occ+1) + ", " +
							GED_MYSQL_DATA_TB_COL_LTV_SEC + "=" + CString((UInt32)inGEDPktIn->tv.tv_sec) + ", " +
							GED_MYSQL_DATA_TB_COL_LTV_USEC + "=" + CString((UInt32)inGEDPktIn->tv.tv_usec) + ", " +
							GED_MYSQL_DATA_TB_COL_RTV_SEC + "=" + CString((UInt32)tv.tv_sec) + ", " +
							GED_MYSQL_DATA_TB_COL_RTV_USEC + "=" + CString((UInt32)tv.tv_usec);

					if (b) outSQL += ", " + GED_MYSQL_DATA_TB_COL_SRC + "=\'" + src + "\' ";

					CChunk inPktData (inGEDPktIn->data, inGEDPktIn->len, GEDPktCfgToTData(inGEDPktCfg), false);

					for (size_t i=0; i<inGEDPktCfg->fields.GetLength(); i++)
					{
						if (inGEDPktCfg->fields[i]->meta)
						{
							inPktData++;
						}
						else
						{
							bool isKey=false; for (size_t j=0; j<inGEDPktCfg->keyidc.GetLength() && !isKey; j++)
								if (*inGEDPktCfg->keyidc[j] == i)
									isKey = true;

							if (isKey)
							{
								inPktData++;
								continue;
							}

							switch (inPktData.NextDataIs())
							{
								case DATA_SINT32 :
								{
									SInt32 inSint32=0L; 
									inPktData >> inSint32;
									outSQL += ", " + inGEDPktCfg->fields[i]->name + "=" + CString(inSint32);
								}
								break;
								case DATA_STRING :
								{
									SInt8 *inSint8=NULL; 
									inPktData >> inSint8; 
									CString str(inSint8);
									if (inSint8 != NULL) delete [] inSint8;
									GED_MYSQL_FILTER_STRING(str,m_MySQLLtGtFilter,m_MySQLQuotFilter);
									outSQL += ", " + inGEDPktCfg->fields[i]->name + "=\'" + str + "\'";
								}
								break;
								case DATA_FLOAT64 :
								{
									Float64 inFloat64=0.;
									inPktData >> inFloat64;
									outSQL += ", " + inGEDPktCfg->fields[i]->name + "=" + CString(inFloat64);
								}
								break;
							}
						}
					}

					outSQL += " WHERE " + GED_MYSQL_DATA_TB_COL_ID + "=" + CString(id);

					if (!ExecuteSQLQuery (outSQL)) return false;

					CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_BKD_OPERATION, 
						"MYSQL backend push active queue on packet type \"" + CString(inGEDPktIn->typ) + "\" [" + inAddr + 
						"] : updated occurences number");

					m_Nr = 1L;
				}
				else
				{
					if (ms == inGEDPktIn->tv.tv_sec && mus == inGEDPktIn->tv.tv_usec)
					{
						CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_BKD_OPERATION, 
									   "MYSQL backend update on DUP packet type \"" + 
									   CString(inGEDPktIn->typ) + "\"");
						return true;
					}

					struct timeval tv; ::gettimeofday (&tv, NULL);

					outSQL = "UPDATE " + 
							inGEDPktCfg->name + GED_MYSQL_DATA_TBL_QUEUE_ACTIVE +
						 " SET " +
							GED_MYSQL_DATA_TB_COL_MTV_SEC + "=" + CString((UInt32)inGEDPktIn->tv.tv_sec) + ", " +
							GED_MYSQL_DATA_TB_COL_MTV_USEC + "=" + CString((UInt32)inGEDPktIn->tv.tv_usec);
                                               
         if ( ms == os && mus == ous ) 
         {
              outSQL += ", " + GED_MYSQL_DATA_TB_COL_FTV_SEC + "=" + CString((UInt32)inGEDPktIn->tv.tv_sec) + ", " +
							GED_MYSQL_DATA_TB_COL_FTV_USEC + "=" + CString((UInt32)inGEDPktIn->tv.tv_usec);
         } 

					if (b) outSQL += ", " + GED_MYSQL_DATA_TB_COL_SRC + "=\'" + src + "\' ";

					CChunk inPktData (inGEDPktIn->data, inGEDPktIn->len, GEDPktCfgToTData(inGEDPktCfg), false);

					for (size_t i=0; i<inGEDPktCfg->fields.GetLength(); i++)
					{
						if (!inGEDPktCfg->fields[i]->meta)
						{
							inPktData++;
						}
						else
						{
							bool isKey=false; for (size_t j=0; j<inGEDPktCfg->keyidc.GetLength() && !isKey; j++)
								if (*inGEDPktCfg->keyidc[j] == i)
									isKey = true;

							if (isKey)
							{
								inPktData++;
								continue;
							}

							switch (inPktData.NextDataIs())
							{
								case DATA_SINT32 :
								{
									SInt32 inSint32=0L; 
									inPktData >> inSint32;
									outSQL += ", " + inGEDPktCfg->fields[i]->name + "=" + CString(inSint32);
								}
								break;
								case DATA_STRING :
								{
									SInt8 *inSint8=NULL; 
									inPktData >> inSint8; 
									CString str(inSint8);
									if (inSint8 != NULL) delete [] inSint8;
									GED_MYSQL_FILTER_STRING(str,m_MySQLLtGtFilter,m_MySQLQuotFilter);
									outSQL += ", " + inGEDPktCfg->fields[i]->name + "=\'" + str + "\'";
								}
								break;
								case DATA_FLOAT64 :
								{
									Float64 inFloat64=0.;
									inPktData >> inFloat64;
									outSQL += ", " + inGEDPktCfg->fields[i]->name + "=" + CString(inFloat64);
								}
								break;
							}
						}
					}

					outSQL += " WHERE " + GED_MYSQL_DATA_TB_COL_ID + "=" + CString(id);

					if (!ExecuteSQLQuery (outSQL)) return false;

					CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_BKD_OPERATION, 
						"MYSQL backend push active queue on packet type \"" + CString(inGEDPktIn->typ) + "\" [" + inAddr + 
						"] : updated user meta ");

					m_Nr = 1L;
				}
			}
			else
			{
				::mysql_free_result (inSQLRes);

				struct timeval tv; ::gettimeofday (&tv, NULL);

				outSQL = "INSERT INTO " + 
						inGEDPktCfg->name + GED_MYSQL_DATA_TBL_QUEUE_ACTIVE +
					 " (" +
						GED_MYSQL_DATA_TB_COL_ID + ", " +
						GED_MYSQL_DATA_TB_COL_QUEUE + ", " +
						GED_MYSQL_DATA_TB_COL_OTV_SEC + ", " +
						GED_MYSQL_DATA_TB_COL_OTV_USEC + ", " +
						GED_MYSQL_DATA_TB_COL_LTV_SEC + ", " +
						GED_MYSQL_DATA_TB_COL_LTV_USEC + ", " +
						GED_MYSQL_DATA_TB_COL_RTV_SEC + ", " +
						GED_MYSQL_DATA_TB_COL_RTV_USEC + ", " +
						GED_MYSQL_DATA_TB_COL_MTV_SEC + ", " +
            GED_MYSQL_DATA_TB_COL_MTV_USEC + ", " +
            GED_MYSQL_DATA_TB_COL_FTV_SEC + ", " +
            GED_MYSQL_DATA_TB_COL_FTV_USEC + ", " +
						GED_MYSQL_DATA_TB_COL_SRC + ", ";

				for (size_t i=0; i<inGEDPktCfg->fields.GetLength(); i++)
				{
					outSQL += inGEDPktCfg->fields[i]->name;
					if (i<inGEDPktCfg->fields.GetLength()-1) outSQL += ", ";
				}

          m_Id++;
          outSQL += ") VALUES ( '', \'a\', " +
					CString((UInt32)inGEDPktIn->tv.tv_sec) + ", " + 
					CString((UInt32)inGEDPktIn->tv.tv_usec) + ", " + 
					CString((UInt32)inGEDPktIn->tv.tv_sec) + ", " + 
					CString((UInt32)inGEDPktIn->tv.tv_usec) + ", " + 
					CString((UInt32)tv.tv_sec) + ", " + 
					CString((UInt32)tv.tv_usec) + ", " + 
					CString((UInt32)inGEDPktIn->tv.tv_sec) + ", " + 
          CString((UInt32)inGEDPktIn->tv.tv_usec) + ", " +
          CString((UInt32)inGEDPktIn->tv.tv_sec) + ", " +
					CString((UInt32)inGEDPktIn->tv.tv_usec) + ", \'" + 
					inAddr + CString((inGEDPktIn->req&GED_PKT_REQ_SRC_RELAY)?"/1":"/0") + "\', ";

				CChunk inChunk (inGEDPktIn->data, inGEDPktIn->len, inTData, false);

				for (size_t i=0; i<inGEDPktCfg->fields.GetLength(); i++)
				{
					switch (inChunk.NextDataIs())
					{
						case DATA_STRING :
						{
                 char *inStr=NULL; inChunk>>inStr; CString inStg(inStr); if (inStr) delete [] inStr;
						     GED_MYSQL_FILTER_STRING(inStg,m_MySQLLtGtFilter,m_MySQLQuotFilter);
                 if ( CString(inStg.Get()) != CString("0")) 
            {
                 outSQL += "\'" + inStg + "\'"; 
            }
            else
            {
                outSQL += "\'\'"; 
            }
      
						}
						break;

						case DATA_SINT32 :
						{
		           long n=0L; inChunk>>n; CString out; out += CString(n);
               if ( n == 0 ) outSQL += "\'\'"; 
               if ( n != 0 ) outSQL += " " + CString(n);
                             
						}
						break;

						case DATA_FLOAT64 :
						{
						   double n=0.; inChunk>>n; CString out; out += CString(n);
               if ( n == 0 ) outSQL += "\'\'";
               if ( n != 0 ) outSQL += " " + CString(n);  
						}
						break;
					}

					if (i<inGEDPktCfg->fields.GetLength()-1) outSQL += ", ";
				}					

				outSQL += ")";

				if (!ExecuteSQLQuery (outSQL)) return false;

				m_Na++;
				m_Nr = 1L;

				m_aInc=BUFFER_INC_8; for (size_t z=m_aInc; z<=BUFFER_INC_65536 && ((m_Na>>m_aInc)>0L); z++) m_aInc = (TBufferInc)z;

				CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_BKD_OPERATION, 
					"MYSQL backend push active queue on packet type \"" + CString(inGEDPktIn->typ) + "\" [" + inAddr + 
					"] : created occurence");					
			}
		}
		break;

		case GED_PKT_REQ_BKD_SYNC :
		{
			CString inPktAddr (::inet_ntoa(inGEDPktIn->addr));

			CString inReqStr; switch (inGEDPktIn->req&GED_PKT_REQ_MASK)
                        {
                                case GED_PKT_REQ_PUSH : 
				{
					switch (inGEDPktIn->req&GED_PKT_REQ_PUSH_MASK)
					{
						case GED_PKT_REQ_PUSH_TMSP :
						{
							inReqStr = "push"; break;
						}
						break;
						case GED_PKT_REQ_PUSH_NOTMSP :
						{
							inReqStr = "update"; break;
						}
						break;
					}
				}
				break;
                                case GED_PKT_REQ_DROP :
				{
					switch (inGEDPktIn->req&GED_PKT_REQ_DROP_MASK)
					{
						case GED_PKT_REQ_DROP_DATA :
						{
							inReqStr = "drop_by_data";
						}
						break;
						case GED_PKT_REQ_DROP_ID :
						{
							inReqStr = "drop_by_id";
						}
						break;
					}
				}
				break;
                        }

			CString outSQL = "INSERT INTO " + 
				inGEDPktCfg->name + GED_MYSQL_DATA_TBL_QUEUE_SYNC + " (" +
				GED_MYSQL_DATA_TB_COL_ID + ", " +
				GED_MYSQL_DATA_TB_COL_QUEUE + ", " +
				GED_MYSQL_DATA_TB_COL_OTV_SEC + ", " +
				GED_MYSQL_DATA_TB_COL_OTV_USEC + ", " +
				GED_MYSQL_DATA_TB_COL_SRC + ", " +
				GED_MYSQL_DATA_TB_COL_TGT + ", " +
				GED_MYSQL_DATA_TB_COL_REQ + ", ";

			for (size_t i=0; i<inGEDPktCfg->fields.GetLength(); i++)
			{
				outSQL += inGEDPktCfg->fields[i]->name;
				if (i<inGEDPktCfg->fields.GetLength()-1) outSQL += ", ";
			}
        m_Id++;
        outSQL += ") VALUES ( '',  \'s\', " +
				CString((UInt32)inGEDPktIn->tv.tv_sec) + ", " + 
				CString((UInt32)inGEDPktIn->tv.tv_usec) + ", \'" + 
				inPktAddr + CString((inGEDPktIn->req&GED_PKT_REQ_SRC_RELAY)?"/1":"/0") + "\', \'" +
				inAddr + "\', \'" +
				inReqStr + "\'";

			CChunk inChunk (inGEDPktIn->data, inGEDPktIn->len, ::GEDPktCfgToTData(inGEDPktCfg), false);

			for (size_t i=0; i<inGEDPktCfg->fields.GetLength(); i++)
			{
				switch (inChunk.NextDataIs())
				{
					case DATA_STRING :
					{
						char *inStr=NULL; inChunk>>inStr; CString inStg(inStr); if (inStr) delete [] inStr;
						GED_MYSQL_FILTER_STRING(inStg,m_MySQLLtGtFilter,m_MySQLQuotFilter);
						outSQL += ", \'" + inStg + "\'";
					}
					break;

					case DATA_SINT32 :
					{
						long n=0L; inChunk>>n; outSQL += ", " + CString(n);
					}
					break;

					case DATA_FLOAT64 :
					{
						double n=0.; inChunk>>n; outSQL += ", " + CString(n);
					}
					break;
				}
			}					

			outSQL += ")";

			if (!ExecuteSQLQuery (outSQL)) return false;

			m_Ns++;
			m_Nr = 1L;

			m_sInc=BUFFER_INC_8; for (size_t z=m_sInc; z<=BUFFER_INC_65536 && ((m_Ns>>m_sInc)>0L); z++) m_sInc = (TBufferInc)z;

			CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_BKD_OPERATION, 
				"MYSQL backend push sync queue on packet type \"" + CString(inGEDPktIn->typ) + "\" [" + inAddr + 
				"] : created occurence");
		}
		break;

		case GED_PKT_REQ_BKD_HISTORY :
		{
			CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, 
						   CString("MYSQL backend push in history queue is not authorized"));
			return false;
		}
		break;
	}

	return true;
}

//----------------------------------------------------------------------------------------------------------------------------------------
// drop handling
//----------------------------------------------------------------------------------------------------------------------------------------
bool CGEDBackEndMySQL::Drop (const CString &inAddr, const int inQueue, const TGEDPktIn *inGEDPktIn)
{
	m_Nr = 0L;

	if (inGEDPktIn == NULL)
	{
		CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, CString("MYSQL backend drop on NULL packet"));
		return false;
	}

	TGEDPktCfg *inGEDPktCfg (::GEDPktInToCfg (const_cast <TGEDPktIn *> (inGEDPktIn), m_GEDCfg.pkts));

	if ((inGEDPktIn->req & GED_PKT_REQ_DROP_MASK) == GED_PKT_REQ_DROP_DATA && inGEDPktCfg == NULL)
	{
		CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, 
					   "MYSQL backend drop on unknown packet type \"" + CString(inGEDPktIn->typ) + "\"");
		return false;
	}

	switch (inQueue)
	{
		case GED_PKT_REQ_BKD_ACTIVE :
		{
			if (inGEDPktCfg == NULL) return false;

			struct timeval tv; ::gettimeofday (&tv, NULL);

		// Began new approach from selet, insert, delete instead of TData using. INSERT INTO `new_table` VALUES (SELECT * FROM `old_table`)
   
			TBuffer <TData> inTData (::GEDPktCfgToTData (inGEDPktCfg)); CChunk inChunk (inGEDPktIn->data, inGEDPktIn->len, inTData, false);

      CString delSQL = "DELETE FROM " + m_MySQLDatabase + "." + inGEDPktCfg->name + GED_MYSQL_DATA_TBL_QUEUE_ACTIVE + " WHERE " + GED_MYSQL_DATA_TB_COL_QUEUE + "=\'a\' AND (";
			CString outSQL = "INSERT INTO " + m_MySQLDatabase + "." +  
              inGEDPktCfg->name + GED_MYSQL_DATA_TBL_QUEUE_HISTORY;
              outSQL += " SELECT '',\'h\',occ,o_sec,o_usec,l_sec,l_usec,r_sec,r_usec,m_sec,m_usec,f_sec,f_usec,"
                        + CString((UInt32)tv.tv_sec) + ", " 
                        + CString((UInt32)tv.tv_usec) + ",\'pkt\',src,tgt,req,";
        
            for (size_t k=0; k<inGEDPktCfg->fields.GetLength(); k++)
				    {
				    	outSQL += inGEDPktCfg->fields[k]->name;
				     	if (k<inGEDPktCfg->fields.GetLength()-1) outSQL += ", ";
			      }
           outSQL +=" FROM " + m_MySQLDatabase + "." +
              inGEDPktCfg->name + GED_MYSQL_DATA_TBL_QUEUE_ACTIVE +
		          " WHERE " + 
              GED_MYSQL_DATA_TB_COL_QUEUE + "=\'a\' AND (" +
              GED_MYSQL_DATA_TB_COL_OTV_SEC + "<" + CString((UInt32)inGEDPktIn->tv.tv_sec) + " OR (" +
              GED_MYSQL_DATA_TB_COL_OTV_SEC + "=" + CString((UInt32)inGEDPktIn->tv.tv_sec) + " AND " + 
	            GED_MYSQL_DATA_TB_COL_OTV_USEC + "<=" +CString((UInt32)inGEDPktIn->tv.tv_usec) + "))";
                         
		      delSQL += GED_MYSQL_DATA_TB_COL_OTV_SEC + "<" + CString((UInt32)inGEDPktIn->tv.tv_sec) + " OR (" +
              GED_MYSQL_DATA_TB_COL_OTV_SEC + "=" + CString((UInt32)inGEDPktIn->tv.tv_sec) + " AND " + 
	            GED_MYSQL_DATA_TB_COL_OTV_USEC + "<=" +CString((UInt32)inGEDPktIn->tv.tv_usec) + "))";
                         
			for (size_t i=0; i<inGEDPktCfg->fields.GetLength(); i++)
			{
				switch (inGEDPktCfg->fields[i]->type)
				{
					case DATA_STRING :
					{
           #ifdef __GED_DEBUG_SQL__
         			CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_DEBUG, "Field " + CString(inGEDPktCfg->fields[i]->name) + " is a DATA_STRING");
           #endif
           if (inGEDPktIn->data != NULL)
           {
              char *inStr=NULL;
              inChunk >> inStr;
              CString inString(inStr); 
              if (inStr) delete [] inStr; 
            	GED_MYSQL_FILTER_STRING(inString,m_MySQLLtGtFilter,m_MySQLQuotFilter);
              if ( inString.GetLength() != 0)	outSQL += " AND " + inGEDPktCfg->fields[i]->name + " LIKE \'" + inString + "\'";
              if ( inString.GetLength() != 0)	delSQL += " AND " + inGEDPktCfg->fields[i]->name + " LIKE \'" + inString + "\'";
           }
           else
           {
           outSQL += " AND " + inGEDPktCfg->fields[i]->name + " LIKE \'%\'";
           delSQL += " AND " + inGEDPktCfg->fields[i]->name + " LIKE \'%\'";
           }
					}
					break;

					case DATA_SINT32 :
					{
            #ifdef __GED_DEBUG_SQL__
         			CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_DEBUG, "Field " + CString(inGEDPktCfg->fields[i]->name) + " is a DATA_SINT32");
           #endif
           if (inGEDPktIn->data != NULL)
           {
						long inLong; inChunk >> inLong;
						outSQL += " AND " + inGEDPktCfg->fields[i]->name + "=" + CString(inLong);
            delSQL += " AND " + inGEDPktCfg->fields[i]->name + "=" + CString(inLong);
           }
           else
           {
           if ( inGEDPktCfg->fields[i]->name == "state" )
            {
                        outSQL += " AND " + inGEDPktCfg->fields[i]->name + " LIKE \'%\'";
                        delSQL += " AND " + inGEDPktCfg->fields[i]->name + " LIKE \'%\'";
            }
            else
            {
                        outSQL += " AND " + inGEDPktCfg->fields[i]->name + "=\'%\'";
                        delSQL += " AND " + inGEDPktCfg->fields[i]->name + "=\'%\'";
            }
           }
					}
					break;

					case DATA_FLOAT64 :
					{
            #ifdef __GED_DEBUG_SQL__
         			CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_DEBUG, "Field " + CString(inGEDPktCfg->fields[i]->name) + " is a DATA_FLOAT64");
           #endif
           if (inGEDPktIn->data != NULL)
           {
						double inDouble; inChunk >> inDouble;
						outSQL += " AND " + inGEDPktCfg->fields[i]->name + "=" + CString(inDouble);
            delSQL += " AND " + inGEDPktCfg->fields[i]->name + "=" + CString(inDouble);
             }
           else
           {
            outSQL += " AND " + inGEDPktCfg->fields[i]->name + "=\'%\'";
            delSQL += " AND " + inGEDPktCfg->fields[i]->name + "=\'%\'";
           }
					}
					break;
				}
			}

			if (!ExecuteSQLQuery (outSQL)) return false;
			if (!ExecuteSQLQuery (delSQL)) return false;
   
		break;
   }

		case GED_PKT_REQ_BKD_SYNC :
		{
			CString outSQL; UInt32 n=0L;

			if (inGEDPktCfg != NULL)
			{
				outSQL = "DELETE FROM " + 
						inGEDPktCfg->name + GED_MYSQL_DATA_TBL_QUEUE_SYNC + 
					" WHERE " +
						GED_MYSQL_DATA_TB_COL_QUEUE + "=\'s\' AND (" +
						GED_MYSQL_DATA_TB_COL_OTV_SEC + "<" + CString((UInt32)inGEDPktIn->tv.tv_sec) + " OR (" +
						GED_MYSQL_DATA_TB_COL_OTV_SEC + "=" + CString((UInt32)inGEDPktIn->tv.tv_sec) + " AND " + 
						GED_MYSQL_DATA_TB_COL_OTV_USEC + "<=" +CString((UInt32)inGEDPktIn->tv.tv_usec) + "))";
				
				if (inGEDPktIn != NULL && inGEDPktIn->data != NULL)
				{
					TBuffer <TData> inTData (::GEDPktCfgToTData (inGEDPktCfg)); CChunk inChunk (inGEDPktIn->data, inGEDPktIn->len, inTData, false);

					for (size_t i=0, j=0; i<inGEDPktCfg->keyidc.GetLength(); i++)
					{
						for (; j<*inGEDPktCfg->keyidc[i]; j++) inChunk++;

						switch (inGEDPktCfg->fields[*inGEDPktCfg->keyidc[i]]->type)
						{
							case DATA_STRING :
							{
								char *inStr=NULL; inChunk >> inStr; CString inString(inStr); if (inStr) delete [] inStr;
								GED_MYSQL_FILTER_STRING(inString,m_MySQLLtGtFilter,m_MySQLQuotFilter);
								outSQL += " AND " + inGEDPktCfg->name + GED_MYSQL_DATA_TBL_QUEUE_SYNC + "." + inGEDPktCfg->fields[*inGEDPktCfg->keyidc[i]]->name + " LIKE \'" + inString + "\'";
							}
							break;

							case DATA_SINT32 :
							{
								long inLong=0L; inChunk >> inLong;
								outSQL += " AND " + inGEDPktCfg->name + GED_MYSQL_DATA_TBL_QUEUE_SYNC + "." + inGEDPktCfg->fields[*inGEDPktCfg->keyidc[i]]->name + "=" + CString(inLong);
							}
							break;

							case DATA_FLOAT64 :
							{
								double inDouble=0.; inChunk >> inDouble;
								outSQL += " AND " + inGEDPktCfg->name + GED_MYSQL_DATA_TBL_QUEUE_SYNC + "." + inGEDPktCfg->fields[*inGEDPktCfg->keyidc[i]]->name + "=" + CString(inDouble);
							}
							break;
						}

						j++;
					}
				}

				if (inAddr != CString()) 
					outSQL += " AND " + GED_MYSQL_DATA_TB_COL_TGT + " LIKE \'" + inAddr + "\'";

				if (!ExecuteSQLQuery (outSQL)) return false;

				n = ::mysql_affected_rows (&m_MYSQL);
			}
			else
			{
				for (size_t i=0; i<m_GEDCfg.pkts.GetLength(); i++)
				{
					inGEDPktCfg = m_GEDCfg.pkts[i];

					outSQL = "DELETE FROM " + 
						inGEDPktCfg->name + GED_MYSQL_DATA_TBL_QUEUE_SYNC +
					" WHERE " +
						GED_MYSQL_DATA_TB_COL_QUEUE + "=\'s\' AND (" +
						GED_MYSQL_DATA_TB_COL_OTV_SEC + "<" + CString((UInt32)inGEDPktIn->tv.tv_sec) + " OR (" +
						GED_MYSQL_DATA_TB_COL_OTV_SEC + "=" + CString((UInt32)inGEDPktIn->tv.tv_sec) + " AND " + 
						GED_MYSQL_DATA_TB_COL_OTV_USEC + "<=" +CString((UInt32)inGEDPktIn->tv.tv_usec) + "))";

					if (inAddr != CString()) 
						outSQL += " AND " + GED_MYSQL_DATA_TB_COL_TGT + " LIKE \'" + inAddr + "\'";

					if (!ExecuteSQLQuery (outSQL)) return false;

					n += ::mysql_affected_rows (&m_MYSQL);
				}
			}

			m_Ns -= n;

			m_Nr  = n;

			m_sInc=BUFFER_INC_8; for (size_t z=m_sInc; z<=BUFFER_INC_65536 && ((m_Ns>>m_sInc)>0L); z++) m_sInc = (TBufferInc)z;

			CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_BKD_OPERATION, 
				"MYSQL backend drop sync queue on packet type \"" + CString(inGEDPktIn->typ) + 
				"\" [" + inAddr + "] : droped " + CString((long)n) + " record(s)");
		}
		break;

		case GED_PKT_REQ_BKD_HISTORY :
		{
			if ((inGEDPktIn->req & GED_PKT_REQ_DROP_MASK) == GED_PKT_REQ_DROP_DATA)
			{
				UInt32 n=0;

				if (inGEDPktCfg != NULL)
				{
					CString outSQL = "DELETE FROM " + inGEDPktCfg->name + GED_MYSQL_DATA_TBL_QUEUE_HISTORY + " WHERE " + GED_MYSQL_DATA_TB_COL_QUEUE + "=\'h\'";

					if (inAddr != CString())
						outSQL += " AND " + GED_MYSQL_DATA_TB_COL_SRC + " LIKE \'%" + inAddr + "%\'";

					if (inGEDPktIn != NULL && inGEDPktIn->data != NULL)
					{
						TBuffer <TData> inTData (::GEDPktCfgToTData (inGEDPktCfg)); CChunk inChunk (inGEDPktIn->data, inGEDPktIn->len, inTData, false);

						for (size_t i=0, j=0; i<inGEDPktCfg->keyidc.GetLength(); i++)
						{
							for (; j<*inGEDPktCfg->keyidc[i]; j++) inChunk++;

							switch (inGEDPktCfg->fields[*inGEDPktCfg->keyidc[i]]->type)
							{
								case DATA_STRING :
								{
									char *inStr=NULL; inChunk >> inStr; CString inString(inStr); if (inStr) delete [] inStr;
									GED_MYSQL_FILTER_STRING(inString,m_MySQLLtGtFilter,m_MySQLQuotFilter);
									outSQL += " AND " + inGEDPktCfg->fields[*inGEDPktCfg->keyidc[i]]->name + " LIKE \'" + inString + "\'";
								}
								break;

								case DATA_SINT32 :
								{
									long inLong=0L; inChunk >> inLong;
									outSQL += " AND " + inGEDPktCfg->fields[*inGEDPktCfg->keyidc[i]]->name + "=" + CString(inLong);
								}
								break;

								case DATA_FLOAT64 :
								{
									double inDouble=0.; inChunk >> inDouble;
									outSQL += " AND " + inGEDPktCfg->fields[*inGEDPktCfg->keyidc[i]]->name + "=" + CString(inDouble);
								}
								break;
							}

							j++;
						}
					}

					TBuffer <TGEDRcd *> inGEDRcds; inGEDRcds.SetInc (m_hInc); if (m_GEDCfg.loghloc)
					{
						CString sql (outSQL);

						sql.Substitute (CString("DELETE"),CString("SELECT *"));

						if (!ExecuteSQLQuery (sql)) return false;

						MYSQL_RES *inSQLRes = ::mysql_store_result (&m_MYSQL);
						MYSQL_ROW  inSQLRow = NULL;

						#ifdef __GED_DEBUG_SQL__
						UInt32 nrows = ::mysql_num_rows (inSQLRes);
						CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_DEBUG, "MYSQL backend nrows = " + CString(nrows));
						#endif

						while (inSQLRow = ::mysql_fetch_row (inSQLRes))
							inGEDRcds += MySQLRowToHRcd (inSQLRow, inGEDPktCfg);

						::mysql_free_result (inSQLRes);
					}

					if (!ExecuteSQLQuery (outSQL))
					{
						for (size_t k=inGEDRcds.GetLength(); k>0; k--) ::DeleteGEDRcd (*inGEDRcds[k-1]);

						return false;
					}

					for (size_t k=inGEDRcds.GetLength(), l=0; k>0; k--, l++)
					{
						::syslog (m_GEDCfg.loghloc|GED_SYSLOG_INFO, "%s", ::GEDRcdToString(*inGEDRcds[l],m_GEDCfg.pkts).Get());

						::DeleteGEDRcd (*inGEDRcds[l]);
					}

					n = ::mysql_affected_rows (&m_MYSQL);
				}
				else
				{	
					for (size_t i=0; i<m_GEDCfg.pkts.GetLength(); i++)
					{
						inGEDPktCfg = m_GEDCfg.pkts[i];

						CString outSQL = "DELETE FROM " + inGEDPktCfg->name + GED_MYSQL_DATA_TBL_QUEUE_HISTORY + " WHERE " + GED_MYSQL_DATA_TB_COL_QUEUE + "=\'h\'";

						if (inAddr != CString())
							outSQL += " AND " + GED_MYSQL_DATA_TB_COL_SRC + " LIKE \'%" + inAddr + "%\'";

						TBuffer <TGEDRcd *> inGEDRcds; inGEDRcds.SetInc (m_hInc); if (m_GEDCfg.loghloc)
						{
							CString sql (outSQL);

							sql.Substitute (CString("DELETE"),CString("SELECT *"));

							if (!ExecuteSQLQuery (sql)) return false;

							MYSQL_RES *inSQLRes = ::mysql_store_result (&m_MYSQL);
							MYSQL_ROW  inSQLRow = NULL;

							#ifdef __GED_DEBUG_SQL__
							UInt32 nrows = ::mysql_num_rows (inSQLRes);
							CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_DEBUG, "MYSQL backend nrows = " + CString(nrows));
							#endif

							while (inSQLRow = ::mysql_fetch_row (inSQLRes))
								inGEDRcds += MySQLRowToHRcd (inSQLRow, inGEDPktCfg);

							::mysql_free_result (inSQLRes);
						}

						if (!ExecuteSQLQuery (outSQL))
						{
							for (size_t k=inGEDRcds.GetLength(); k>0; k--) ::DeleteGEDRcd (*inGEDRcds[k-1]);
			
							return false;
						}

						for (size_t k=inGEDRcds.GetLength(), l=0; k>0; k--, l++)
						{
							::syslog (m_GEDCfg.loghloc|GED_SYSLOG_INFO, "%s", ::GEDRcdToString(*inGEDRcds[l],m_GEDCfg.pkts).Get());

							::DeleteGEDRcd (*inGEDRcds[l]);
						}

						n += ::mysql_affected_rows (&m_MYSQL);
					}
				}

				m_Nh -= n;

				m_Nr  = n;

				m_hInc=BUFFER_INC_8; for (size_t z=m_hInc; z<=BUFFER_INC_65536 && ((m_Nh>>m_hInc)>0L); z++) m_hInc = (TBufferInc)z;

				CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_BKD_OPERATION, 
				"MYSQL backend drop history queue on packet type \"" + CString(inGEDPktIn->typ) + 
				"\" [" + inAddr + "] : droped " + CString((long)n) + " record(s)");
			}
			else if ((inGEDPktIn->req & GED_PKT_REQ_DROP_MASK) == GED_PKT_REQ_DROP_ID)
			{
				if (inGEDPktIn == NULL || inGEDPktIn->data == NULL)
				{
					CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, 
					CString("MYSQL backend could not drop history queue record on null id"));

					return false;
				}

				UInt32 n=0L; for (size_t i=0; i<m_GEDCfg.pkts.GetLength(); i++)
				{
					CString outSQL = "DELETE FROM " + 
								m_GEDCfg.pkts[i]->name + GED_MYSQL_DATA_TBL_QUEUE_HISTORY + 
							 " WHERE " + 
								GED_MYSQL_DATA_TB_COL_QUEUE + "=\'h\' AND " + 
								GED_MYSQL_DATA_TB_COL_ID + "=" + CString(*((unsigned long*)inGEDPktIn->data));

					if (inAddr != CString())
						outSQL += " AND " + GED_MYSQL_DATA_TB_COL_SRC + " LIKE \'%" + inAddr + "%\'";

					TBuffer <TGEDRcd *> inGEDRcds; inGEDRcds.SetInc (m_hInc); if (m_GEDCfg.loghloc)
					{
						CString sql (outSQL);

						sql.Substitute (CString("DELETE"),CString("SELECT *"));

						if (!ExecuteSQLQuery (sql)) return false;

						MYSQL_RES *inSQLRes = ::mysql_store_result (&m_MYSQL);
						MYSQL_ROW  inSQLRow = NULL;

						#ifdef __GED_DEBUG_SQL__
						UInt32 nrows = ::mysql_num_rows (inSQLRes);
						CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_DEBUG, "MYSQL backend nrows = " + CString(nrows));
						#endif

						while (inSQLRow = ::mysql_fetch_row (inSQLRes))
							inGEDRcds += MySQLRowToHRcd (inSQLRow, m_GEDCfg.pkts[i]);

						::mysql_free_result (inSQLRes);
					}

					if (!ExecuteSQLQuery (outSQL))
					{
						for (size_t k=inGEDRcds.GetLength(); k>0; k--) ::DeleteGEDRcd (*inGEDRcds[k-1]);
		
						return false;
					}

					for (size_t k=inGEDRcds.GetLength(), l=0; k>0; k--, l++)
					{
						::syslog (m_GEDCfg.loghloc|GED_SYSLOG_INFO, "%s", ::GEDRcdToString(*inGEDRcds[l],m_GEDCfg.pkts).Get());

						::DeleteGEDRcd (*inGEDRcds[l]);
					}

					n = ::mysql_affected_rows (&m_MYSQL);

					if (n > 0L)
					{
						m_Nh -= n;

						m_Nr  = n;

						m_hInc=BUFFER_INC_8; for (size_t z=m_hInc; z<=BUFFER_INC_65536 && ((m_Nh>>m_hInc)>0L); z++) m_hInc = (TBufferInc)z;

						break;
					}
				}

				CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_BKD_OPERATION, 
				"MYSQL backend drop history queue on packet id \"" + CString(*((UInt32*)inGEDPktIn->data)) + 
				"\" [" + inAddr + "] : droped " + CString((long)n) + " record(s)");
			}
		}
		break;
	}

	return true;
}

//----------------------------------------------------------------------------------------------------------------------------------------
// peek request handling
//----------------------------------------------------------------------------------------------------------------------------------------
TBuffer <TGEDRcd *> CGEDBackEndMySQL::Peek (const CString &inAddr, const int inQueue, const TGEDPktIn *inGEDPktIn)
{
	m_Nr = 0L;

	TBuffer <TGEDRcd *> outGEDRcds; TGEDPktCfg *inGEDPktCfg (::GEDPktInToCfg (const_cast <TGEDPktIn*> (inGEDPktIn), m_GEDCfg.pkts));

	UInt32 inTm1 = ((inGEDPktIn!=NULL)&&(inGEDPktIn->req&GED_PKT_REQ_PEEK_PARM_1_TM))?inGEDPktIn->p1:0L;
	UInt32 inTm2 = ((inGEDPktIn!=NULL)&&(inGEDPktIn->req&GED_PKT_REQ_PEEK_PARM_3_TM))?(inGEDPktIn->p3!=0L)?inGEDPktIn->p3:UINT32MAX:UINT32MAX;

	UInt32 inOff = ((inGEDPktIn!=NULL)&&(inGEDPktIn->req&GED_PKT_REQ_PEEK_PARM_2_OFFSET))?inGEDPktIn->p2:0L;
	UInt32 inNum = ((inGEDPktIn!=NULL)&&(inGEDPktIn->req&GED_PKT_REQ_PEEK_PARM_4_NUMBER))?(inGEDPktIn->p4!=0L)?inGEDPktIn->p4:UINT32MAX:UINT32MAX;

	switch (inQueue)
	{
		case GED_PKT_REQ_BKD_ACTIVE :
		{
			if (inGEDPktIn != NULL && inGEDPktIn->data != NULL && inGEDPktCfg == NULL)
			{
				CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, 
					"MYSQL backend peek active queue on unknown packet type \"" + CString(inGEDPktIn->typ) + "\"");

				return outGEDRcds;
			}

			if (inGEDPktCfg != NULL)
			{
				if (inGEDPktIn != NULL && inGEDPktIn->data == NULL) outGEDRcds.SetInc (m_aInc);

				CString inSQL (GetSelectQuery (inQueue, inGEDPktIn, inGEDPktCfg, inAddr, inTm1, inTm2, inOff, inNum));

				if (!ExecuteSQLQuery (inSQL)) return outGEDRcds;

				MYSQL_RES *inSQLRes = ::mysql_store_result (&m_MYSQL);
				MYSQL_ROW  inSQLRow = NULL;

				#ifdef __GED_DEBUG_SQL__
				UInt32 nrows = ::mysql_num_rows (inSQLRes);
				CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_DEBUG, "MYSQL backend nrows = " + CString(nrows));
				#endif

				while (inSQLRow = ::mysql_fetch_row (inSQLRes))
					outGEDRcds += MySQLRowToARcd (inSQLRow, inGEDPktCfg);

				::mysql_free_result (inSQLRes);

				size_t n=0; if (inSQL.Find(CString("ORDER BY"),0,&n)) inSQL.Delete (n,inSQL.GetLength()-n);
			
				if (ExecuteSQLQuery (inSQL))
				{
					inSQLRes = ::mysql_store_result (&m_MYSQL);
					UInt32 nrows = ::mysql_num_rows (inSQLRes);
					::mysql_free_result (inSQLRes);

					m_Nr += nrows;

					#ifdef __GED_DEBUG_SQL__
					CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_DEBUG, "MYSQL backend nrows = " + CString(nrows));
					#endif
				}
			}
			else
			{
				outGEDRcds.SetInc (m_aInc);

				for (size_t i=0; i<m_GEDCfg.pkts.GetLength(); i++)
				{
					CString inSQL (GetSelectQuery (inQueue, inGEDPktIn, m_GEDCfg.pkts[i], inAddr, inTm1, inTm2, inOff, inNum));

					if (!ExecuteSQLQuery (inSQL)) return outGEDRcds;

					MYSQL_RES *inSQLRes = ::mysql_store_result (&m_MYSQL);
					MYSQL_ROW  inSQLRow = NULL;

					#ifdef __GED_DEBUG_SQL__
					UInt32 nrows = ::mysql_num_rows (inSQLRes);
					CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_DEBUG, "MYSQL backend nrows = " + CString(nrows));
					#endif

					while (inSQLRow = ::mysql_fetch_row (inSQLRes))
						outGEDRcds += MySQLRowToARcd (inSQLRow, m_GEDCfg.pkts[i]);

					::mysql_free_result (inSQLRes);

					size_t n=0; if (inSQL.Find(CString("ORDER BY"),0,&n)) inSQL.Delete (n,inSQL.GetLength()-n);
				
					if (ExecuteSQLQuery (inSQL))
					{
						inSQLRes = ::mysql_store_result (&m_MYSQL);
						UInt32 nrows = ::mysql_num_rows (inSQLRes);
						::mysql_free_result (inSQLRes);

						m_Nr += nrows;

						#ifdef __GED_DEBUG_SQL__
						CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_DEBUG, "MYSQL backend nrows = " + CString(nrows));
						#endif
					}
				}

				if (outGEDRcds.GetLength() > 0)
				{
					if ((inGEDPktIn != NULL) && ((inGEDPktIn->req & GED_PKT_REQ_PEEK_SORT_MASK) == GED_PKT_REQ_PEEK_SORT_DSC))
					{
						::QuickSortGEDARcdsDsc (outGEDRcds, 0, outGEDRcds.GetLength()-1);
					}
					else
					{
						::QuickSortGEDARcdsAsc (outGEDRcds, 0, outGEDRcds.GetLength()-1);
					}

					if (inNum < outGEDRcds.GetLength())
					{
						for (UInt32 i=outGEDRcds.GetLength(); i>inNum; i--) ::DeleteGEDRcd (*outGEDRcds[i-1]);
						outGEDRcds.Delete (inNum, outGEDRcds.GetLength()-inNum);
					}
				}
			}

			CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_BKD_OPERATION, 
			"MYSQL backend peek active queue on packet type \"" + CString(inGEDPktIn!=NULL?inGEDPktIn->typ:0) + 
			"\" [" + inAddr + "] : " + CString((long)outGEDRcds.GetLength()) + " result(s)");
		}
		break;

		case GED_PKT_REQ_BKD_SYNC :
		{
			if (inGEDPktIn != NULL && inGEDPktIn->data != NULL && inGEDPktCfg == NULL)
			{
				CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, 
					"MYSQL backend peek sync queue on unknown packet type \"" + CString(inGEDPktIn->typ) + "\"");

				return outGEDRcds;
			}

			if (inGEDPktCfg != NULL)
			{
				if (inGEDPktIn != NULL && inGEDPktIn->data == NULL) outGEDRcds.SetInc (m_aInc);

				CString inSQL (GetSelectQuery (inQueue, inGEDPktIn, inGEDPktCfg, inAddr, inTm1, inTm2, inOff, inNum));

				if (!ExecuteSQLQuery (inSQL)) return outGEDRcds;

				MYSQL_RES *inSQLRes = ::mysql_store_result (&m_MYSQL);
				MYSQL_ROW  inSQLRow = NULL;

				#ifdef __GED_DEBUG_SQL__
				UInt32 nrows = ::mysql_num_rows (inSQLRes);
				CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_DEBUG, "MYSQL backend nrows = " + CString(nrows));
				#endif

				while (inSQLRow = ::mysql_fetch_row (inSQLRes))
					outGEDRcds += MySQLRowToSRcd (inSQLRow, inGEDPktCfg);

				::mysql_free_result (inSQLRes);

				size_t n=0; if (inSQL.Find(CString("ORDER BY"),0,&n)) inSQL.Delete (n,inSQL.GetLength()-n);
				
				if (ExecuteSQLQuery (inSQL))
				{
					inSQLRes = ::mysql_store_result (&m_MYSQL);
					UInt32 nrows = ::mysql_num_rows (inSQLRes);
					::mysql_free_result (inSQLRes);

					m_Nr += nrows;

					#ifdef __GED_DEBUG_SQL__
					CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_DEBUG, "MYSQL backend nrows = " + CString(nrows));
					#endif
				}
			}
			else
			{
				outGEDRcds.SetInc (m_sInc);

				for (size_t i=0; i<m_GEDCfg.pkts.GetLength(); i++)
				{
					CString inSQL (GetSelectQuery (inQueue, inGEDPktIn, m_GEDCfg.pkts[i], inAddr, inTm1, inTm2, inOff, inNum));

					if (!ExecuteSQLQuery (inSQL)) return outGEDRcds;

					MYSQL_RES *inSQLRes = ::mysql_store_result (&m_MYSQL);
					MYSQL_ROW  inSQLRow = NULL;

					#ifdef __GED_DEBUG_SQL__
					UInt32 nrows = ::mysql_num_rows (inSQLRes);
					CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_DEBUG, "MYSQL backend nrows = " + CString(nrows));
					#endif

					while (inSQLRow = ::mysql_fetch_row (inSQLRes))
						outGEDRcds += MySQLRowToSRcd (inSQLRow, m_GEDCfg.pkts[i]);

					::mysql_free_result (inSQLRes);

					size_t n=0; if (inSQL.Find(CString("ORDER BY"),0,&n)) inSQL.Delete (n,inSQL.GetLength()-n);
				
					if (ExecuteSQLQuery (inSQL))
					{
						inSQLRes = ::mysql_store_result (&m_MYSQL);
						UInt32 nrows = ::mysql_num_rows (inSQLRes);
						::mysql_free_result (inSQLRes);

						m_Nr += nrows;

						#ifdef __GED_DEBUG_SQL__
						CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_DEBUG, "MYSQL backend nrows = " + CString(nrows));
						#endif
					}
				}

				if (outGEDRcds.GetLength() > 0)
				{
					if ((inGEDPktIn != NULL) && ((inGEDPktIn->req & GED_PKT_REQ_PEEK_SORT_MASK) == GED_PKT_REQ_PEEK_SORT_DSC))
					{
						::QuickSortGEDSRcdsDsc (outGEDRcds, 0, outGEDRcds.GetLength()-1);
					}
					else
					{
						::QuickSortGEDSRcdsAsc (outGEDRcds, 0, outGEDRcds.GetLength()-1);
					}

					if (inNum < outGEDRcds.GetLength())
					{
						for (UInt32 i=outGEDRcds.GetLength(); i>inNum; i--) ::DeleteGEDRcd (*outGEDRcds[i-1]);
						outGEDRcds.Delete (inNum, outGEDRcds.GetLength()-inNum);
					}
				}
			}
		
			CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_BKD_OPERATION, 
			"MYSQL backend peek sync queue on packet type \"" + CString(inGEDPktIn!=NULL?inGEDPktIn->typ:0) + 
			"\" [" + inAddr + "] : " + CString((long)outGEDRcds.GetLength()) + " result(s)");
		}
		break;

		case GED_PKT_REQ_BKD_HISTORY :
		{
			if (inGEDPktIn != NULL && inGEDPktIn->data != NULL && inGEDPktCfg == NULL)
			{
				CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, 
					"MYSQL backend peek history queue on unknown packet type \"" + CString(inGEDPktIn->typ) + "\"");

				return outGEDRcds;
			}

			if (inGEDPktCfg != NULL)
			{
				if (inGEDPktIn != NULL && inGEDPktIn->data == NULL) outGEDRcds.SetInc (m_hInc);

				CString inSQL (GetSelectQuery (inQueue, inGEDPktIn, inGEDPktCfg, inAddr, inTm1, inTm2, inOff, inNum));

				if (!ExecuteSQLQuery (inSQL)) return outGEDRcds;

				MYSQL_RES *inSQLRes = ::mysql_store_result (&m_MYSQL);
				MYSQL_ROW  inSQLRow = NULL;

				#ifdef __GED_DEBUG_SQL__
				UInt32 nrows = ::mysql_num_rows (inSQLRes);
				CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_DEBUG, "MYSQL backend nrows = " + CString(nrows));
				#endif

				while (inSQLRow = ::mysql_fetch_row (inSQLRes))
					outGEDRcds += MySQLRowToHRcd (inSQLRow, inGEDPktCfg);

				::mysql_free_result (inSQLRes);

				size_t n=0; if (inSQL.Find(CString("ORDER BY"),0,&n)) inSQL.Delete (n,inSQL.GetLength()-n);
				
				if (ExecuteSQLQuery (inSQL))
				{
					inSQLRes = ::mysql_store_result (&m_MYSQL);
					UInt32 nrows = ::mysql_num_rows (inSQLRes);
					::mysql_free_result (inSQLRes);

					m_Nr += nrows;

					#ifdef __GED_DEBUG_SQL__
					CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_DEBUG, "MYSQL backend nrows = " + CString(nrows));
					#endif
				}
			}
			else
			{
				outGEDRcds.SetInc (m_hInc);

				for (size_t i=0; i<m_GEDCfg.pkts.GetLength(); i++)
				{
					CString inSQL (GetSelectQuery (inQueue, inGEDPktIn, m_GEDCfg.pkts[i], inAddr, inTm1, inTm2, inOff, inNum));

					if (!ExecuteSQLQuery (inSQL)) return outGEDRcds;

					MYSQL_RES *inSQLRes = ::mysql_store_result (&m_MYSQL);
					MYSQL_ROW  inSQLRow = NULL;

					#ifdef __GED_DEBUG_SQL__
					UInt32 nrows = ::mysql_num_rows (inSQLRes);
					CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_DEBUG, "MYSQL backend nrows = " + CString(nrows));
					#endif

					while (inSQLRow = ::mysql_fetch_row (inSQLRes))
						outGEDRcds += MySQLRowToHRcd (inSQLRow, m_GEDCfg.pkts[i]);

					::mysql_free_result (inSQLRes);

					size_t n=0; if (inSQL.Find(CString("ORDER BY"),0,&n)) inSQL.Delete (n,inSQL.GetLength()-n);
				
					if (ExecuteSQLQuery (inSQL))
					{
						inSQLRes = ::mysql_store_result (&m_MYSQL);
						UInt32 nrows = ::mysql_num_rows (inSQLRes);
						::mysql_free_result (inSQLRes);

						m_Nr += nrows;

						#ifdef __GED_DEBUG_SQL__
						CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_DEBUG, "MYSQL backend nrows = " + CString(nrows));
						#endif
					}
				}

				if (outGEDRcds.GetLength() > 0)
				{
					if ((inGEDPktIn != NULL) && ((inGEDPktIn->req & GED_PKT_REQ_PEEK_SORT_MASK) == GED_PKT_REQ_PEEK_SORT_DSC))
					{
						::QuickSortGEDHRcdsDsc (outGEDRcds, 0, outGEDRcds.GetLength()-1);
					}
					else
					{
						::QuickSortGEDHRcdsAsc (outGEDRcds, 0, outGEDRcds.GetLength()-1);
					}

					if (inNum < outGEDRcds.GetLength())
					{
						for (UInt32 i=outGEDRcds.GetLength(); i>inNum; i--) ::DeleteGEDRcd (*outGEDRcds[i-1]);
						outGEDRcds.Delete (inNum, outGEDRcds.GetLength()-inNum);
					}
				}
			}

			CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_BKD_OPERATION, 
			"MYSQL backend peek history queue on packet type \"" + CString(inGEDPktIn!=NULL?inGEDPktIn->typ:0) + 
			"\" [" + inAddr + "] : " + CString((long)outGEDRcds.GetLength()) + " result(s)");
		}
		break;
	}

	return outGEDRcds;
}

//----------------------------------------------------------------------------------------------------------------------------------------
// mysql backend recover request
//----------------------------------------------------------------------------------------------------------------------------------------
bool CGEDBackEndMySQL::Recover (const TBuffer <TGEDRcd *> &inGEDRecords, void (*inCB) (const UInt32, const UInt32, const TGEDRcd *))
{
	UInt32 na=0L, naa=0L, ns=0L, nss=0L, nh=0L, nhh=0L; for (size_t i=inGEDRecords.GetLength(), j=0, t=inGEDRecords.GetLength(); i>0; i--, j++)
	{
		TGEDRcd *inGEDRcd = *inGEDRecords[j];

		if (inCB != NULL) inCB (j+1, t, inGEDRcd);

		switch (inGEDRcd->queue&GED_PKT_REQ_BKD_MASK)
		{
			case GED_PKT_REQ_BKD_ACTIVE :
			{
				naa++;

				TGEDARcd *inGEDARcd (static_cast <TGEDARcd *> (inGEDRcd));

				TGEDPktCfg *inGEDPktCfg=NULL; for (size_t i=m_GEDCfg.pkts.GetLength(); i>0 && !inGEDPktCfg; i--)
					if (m_GEDCfg.pkts[i-1]->type == inGEDARcd->typ)
						inGEDPktCfg = m_GEDCfg.pkts[i-1];

				if (inGEDPktCfg == NULL)
				{
					CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, "MYSQL backend recover on unknown record type \"" + 
								   CString(inGEDARcd->typ) + "\"");
					return false;
				}

				TBuffer <TData> inTData (::GEDPktCfgToTData (inGEDPktCfg)); CChunk inChunk (inGEDARcd->data, inGEDARcd->len, inTData, false);

				CString outSQL = "INSERT INTO " + inGEDPktCfg->name + GED_MYSQL_DATA_TBL_QUEUE_ACTIVE + " (" +
					GED_MYSQL_DATA_TB_COL_ID + ", " +
					GED_MYSQL_DATA_TB_COL_QUEUE + ", " +
					GED_MYSQL_DATA_TB_COL_OCC + ", " + 
					GED_MYSQL_DATA_TB_COL_OTV_SEC + ", " +
					GED_MYSQL_DATA_TB_COL_OTV_USEC + ", " +
					GED_MYSQL_DATA_TB_COL_LTV_SEC + ", " +
					GED_MYSQL_DATA_TB_COL_LTV_USEC + ", " +
					GED_MYSQL_DATA_TB_COL_RTV_SEC + ", " +
					GED_MYSQL_DATA_TB_COL_RTV_USEC + ", " +
					GED_MYSQL_DATA_TB_COL_MTV_SEC + ", " +
					GED_MYSQL_DATA_TB_COL_MTV_USEC + ", " +
					GED_MYSQL_DATA_TB_COL_FTV_SEC + ", " +
					GED_MYSQL_DATA_TB_COL_FTV_USEC + ", " +
					GED_MYSQL_DATA_TB_COL_SRC + ", ";

				for (size_t k=0; k<inGEDPktCfg->fields.GetLength(); k++)
				{
					outSQL += inGEDPktCfg->fields[k]->name;
					if (k<inGEDPktCfg->fields.GetLength()-1) outSQL += ", ";
				}

				//outSQL += ") VALUES (" + CString(m_Id++) + ", " +
          m_Id++;
          outSQL += ") VALUES ( '',  \'a\', " +
					CString((UInt32)inGEDARcd->occ) + ", " +
					CString((UInt32)inGEDARcd->otv.tv_sec) + ", " + 
					CString((UInt32)inGEDARcd->otv.tv_usec) + ", " + 
					CString((UInt32)inGEDARcd->ltv.tv_sec) + ", " + 
					CString((UInt32)inGEDARcd->ltv.tv_usec) + ", " + 
					CString((UInt32)inGEDARcd->rtv.tv_sec) + ", " + 
					CString((UInt32)inGEDARcd->rtv.tv_usec) + ", " +
					CString((UInt32)inGEDARcd->mtv.tv_sec) + ", " + 
					CString((UInt32)inGEDARcd->mtv.tv_usec) + ", " +
					CString((UInt32)inGEDARcd->ftv.tv_sec) + ", " + 
					CString((UInt32)inGEDARcd->ftv.tv_usec) + ", \'";

				for (size_t k=0; k<inGEDARcd->nsrc; k++)
				{
					outSQL += CString(::inet_ntoa(inGEDARcd->src[k].addr)) + (inGEDARcd->src[k].rly?CString("/1"):CString("/0"));
					if (k < inGEDARcd->nsrc-1) outSQL += ";";
				}

				outSQL += "\'";

				for (size_t k=0; k<inGEDPktCfg->fields.GetLength(); k++)
				{
					switch (inChunk.NextDataIs())
					{
						case DATA_STRING :
						{
							char *inStr=NULL; inChunk>>inStr; CString inStg(inStr); if (inStr) delete [] inStr;
							GED_MYSQL_FILTER_STRING(inStg,m_MySQLLtGtFilter,m_MySQLQuotFilter);
							outSQL += ", \'" + inStg + "\'";
						}
						break;

						case DATA_SINT32 :
						{
							long n=0L; inChunk>>n; outSQL += ", " + CString(n);
						}
						break;

						case DATA_FLOAT64 :
						{
							double n=0.; inChunk>>n; outSQL += ", " + CString(n);
						}
						break;
					}
				}

				outSQL += ")";

				if (!ExecuteSQLQuery (outSQL)) return false;

				na++;
			}
			break;

			case GED_PKT_REQ_BKD_SYNC :
			{
				nss++;

				TGEDSRcd *inGEDSRcd (static_cast <TGEDSRcd *> (inGEDRcd));

				TGEDPktCfg *inGEDPktCfg=NULL; for (size_t i=m_GEDCfg.pkts.GetLength(); i>0 && !inGEDPktCfg; i--)
					if (m_GEDCfg.pkts[i-1]->type == inGEDSRcd->pkt->typ)
						inGEDPktCfg = m_GEDCfg.pkts[i-1];

				if (inGEDPktCfg == NULL)
				{
					CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, "MYSQL backend recover on unknown record type \"" + 
								   CString(inGEDSRcd->pkt->typ) + "\"");
					return false;
				}

				TBuffer <TData> inTData (::GEDPktCfgToTData (inGEDPktCfg)); CChunk inChunk (inGEDSRcd->pkt->data, inGEDSRcd->pkt->len, inTData, false);

				CString inReqStr; switch (inGEDSRcd->pkt->req&GED_PKT_REQ_MASK)
		                {
		                        case GED_PKT_REQ_PUSH : 
					{
						switch (inGEDSRcd->pkt->req&GED_PKT_REQ_PUSH_MASK)
						{
							case GED_PKT_REQ_PUSH_TMSP :
							{
								inReqStr = "push"; break;
							}
							break;
							case GED_PKT_REQ_PUSH_NOTMSP :
							{
								inReqStr = "update"; break;
							}
							break;
						}
					}
					break;
		                        case GED_PKT_REQ_DROP :
					{
						switch (inGEDSRcd->pkt->req&GED_PKT_REQ_DROP_MASK)
						{
							case GED_PKT_REQ_DROP_DATA :
							{
								inReqStr = "drop_by_data";
							}
							break;
							case GED_PKT_REQ_DROP_ID :
							{
								inReqStr = "drop_by_id";
							}
							break;
						}
					}
					break;
		                }

				CString outSQL = "INSERT INTO " + 
					inGEDPktCfg->name + GED_MYSQL_DATA_TBL_QUEUE_SYNC + " (" +
					GED_MYSQL_DATA_TB_COL_ID + ", " +
					GED_MYSQL_DATA_TB_COL_QUEUE + ", " +
					GED_MYSQL_DATA_TB_COL_OTV_SEC + ", " +
					GED_MYSQL_DATA_TB_COL_OTV_USEC + ", " +
					GED_MYSQL_DATA_TB_COL_SRC + ", " +
					GED_MYSQL_DATA_TB_COL_TGT + ", " +
					GED_MYSQL_DATA_TB_COL_REQ + ", ";

				for (size_t i=0; i<inGEDPktCfg->fields.GetLength(); i++)
				{
					outSQL += inGEDPktCfg->fields[i]->name;
					if (i<inGEDPktCfg->fields.GetLength()-1) outSQL += ", ";
				}

				//outSQL += ") VALUES (" + CString(m_Id++) + ", " +
          m_Id++;
          outSQL += ") VALUES ( '', \'s\', " +
					CString((UInt32)inGEDSRcd->pkt->tv.tv_sec) + ", " + 
					CString((UInt32)inGEDSRcd->pkt->tv.tv_usec) + ", \'" + 
					CString(::inet_ntoa(inGEDSRcd->pkt->addr)) + CString((inGEDSRcd->pkt->req&GED_PKT_REQ_SRC_RELAY)?"/1":"/0") + "\', \'" +
					CString(::inet_ntoa(inGEDSRcd->tgt)) + "\', \'" +
					inReqStr + "\'";

				for (size_t i=0; i<inGEDPktCfg->fields.GetLength(); i++)
				{
					switch (inChunk.NextDataIs())
					{
						case DATA_STRING :
						{
							char *inStr=NULL; inChunk>>inStr; CString inStg(inStr); if (inStr) delete [] inStr;
							GED_MYSQL_FILTER_STRING(inStg,m_MySQLLtGtFilter,m_MySQLQuotFilter);
							outSQL += ", \'" + inStg + "\'";
						}
						break;

						case DATA_SINT32 :
						{
							long n=0L; inChunk>>n; outSQL += ", " + CString(n);
						}
						break;

						case DATA_FLOAT64 :
						{
							double n=0.; inChunk>>n; outSQL += ", " + CString(n);
						}
						break;
					}
				}					

				outSQL += ")";

				if (!ExecuteSQLQuery (outSQL)) return false;

				ns++;
			}
			break;

			case GED_PKT_REQ_BKD_HISTORY :
			{
				nhh++;

				TGEDHRcd *inGEDHRcd (static_cast <TGEDHRcd *> (inGEDRcd));

				TGEDPktCfg *inGEDPktCfg=NULL; for (size_t i=m_GEDCfg.pkts.GetLength(); i>0 && !inGEDPktCfg; i--)
					if (m_GEDCfg.pkts[i-1]->type == inGEDHRcd->typ)
						inGEDPktCfg = m_GEDCfg.pkts[i-1];

				if (inGEDPktCfg == NULL)
				{
					CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, "MYSQL backend recover on unknown record type \"" + 
								   CString(inGEDHRcd->typ) + "\"");
					return false;
				}

				TBuffer <TData> inTData (::GEDPktCfgToTData (inGEDPktCfg)); CChunk inChunk (inGEDHRcd->data, inGEDHRcd->len, inTData, false);

				CString outSQL = "INSERT INTO " + inGEDPktCfg->name + GED_MYSQL_DATA_TBL_QUEUE_HISTORY + " (" +
					GED_MYSQL_DATA_TB_COL_ID + ", " +
					GED_MYSQL_DATA_TB_COL_QUEUE + ", " +
					GED_MYSQL_DATA_TB_COL_OCC + ", " + 
					GED_MYSQL_DATA_TB_COL_OTV_SEC + ", " +
					GED_MYSQL_DATA_TB_COL_OTV_USEC + ", " +
					GED_MYSQL_DATA_TB_COL_LTV_SEC + ", " +
					GED_MYSQL_DATA_TB_COL_LTV_USEC + ", " +
					GED_MYSQL_DATA_TB_COL_RTV_SEC + ", " +
					GED_MYSQL_DATA_TB_COL_RTV_USEC + ", " +
					GED_MYSQL_DATA_TB_COL_MTV_SEC + ", " +
					GED_MYSQL_DATA_TB_COL_MTV_USEC + ", " +
					GED_MYSQL_DATA_TB_COL_FTV_SEC + ", " +
					GED_MYSQL_DATA_TB_COL_FTV_USEC + ", " +
					GED_MYSQL_DATA_TB_COL_ATV_SEC + ", " +
					GED_MYSQL_DATA_TB_COL_ATV_USEC + ", " +
					GED_MYSQL_DATA_TB_COL_REASON + ", " +
					GED_MYSQL_DATA_TB_COL_SRC + ", ";

				for (size_t k=0; k<inGEDPktCfg->fields.GetLength(); k++)
				{
					outSQL += inGEDPktCfg->fields[k]->name;
					if (k<inGEDPktCfg->fields.GetLength()-1) outSQL += ", ";
				}

				outSQL += ") VALUES (" +
					CString(inGEDHRcd->hid) + ", " +
					"\'h\', " +
					CString((UInt32)inGEDHRcd->occ) + ", " +
					CString((UInt32)inGEDHRcd->otv.tv_sec) + ", " + 
					CString((UInt32)inGEDHRcd->otv.tv_usec) + ", " + 
					CString((UInt32)inGEDHRcd->ltv.tv_sec) + ", " + 
					CString((UInt32)inGEDHRcd->ltv.tv_usec) + ", " + 
					CString((UInt32)inGEDHRcd->rtv.tv_sec) + ", " + 
					CString((UInt32)inGEDHRcd->rtv.tv_usec) + ", " +
					CString((UInt32)inGEDHRcd->mtv.tv_sec) + ", " + 
					CString((UInt32)inGEDHRcd->mtv.tv_usec) + ", " +
					CString((UInt32)inGEDHRcd->ftv.tv_sec) + ", " + 
					CString((UInt32)inGEDHRcd->ftv.tv_usec) + ", " +
					CString((UInt32)inGEDHRcd->atv.tv_sec) + ", " + 
					CString((UInt32)inGEDHRcd->atv.tv_usec) + ", " +
					CString(((inGEDHRcd->queue&GED_PKT_REQ_BKD_HST_MASK)==GED_PKT_REQ_BKD_HST_PKT)?"\'pkt\'":"\'ttl\'") + ", \'";

				for (size_t k=0; k<inGEDHRcd->nsrc; k++)
				{
					outSQL += CString(::inet_ntoa(inGEDHRcd->src[k].addr)) + (inGEDHRcd->src[k].rly?CString("/1"):CString("/0"));
					if (k < inGEDHRcd->nsrc-1) outSQL += ";";
				}

				outSQL += "\'";

				for (size_t k=0; k<inGEDPktCfg->fields.GetLength(); k++)
				{
					switch (inChunk.NextDataIs())
					{
						case DATA_STRING :
						{
							char *inStr=NULL; inChunk>>inStr; CString inStg(inStr); if (inStr) delete [] inStr;
							GED_MYSQL_FILTER_STRING(inStg,m_MySQLLtGtFilter,m_MySQLQuotFilter);
							outSQL += ", \'" + inStg + "\'";
						}
						break;

						case DATA_SINT32 :
						{
							long n=0L; inChunk>>n; outSQL += ", " + CString(n);
						}
						break;

						case DATA_FLOAT64 :
						{
							double n=0.; inChunk>>n; outSQL += ", " + CString(n);
						}
						break;
					}
				}

				outSQL += ")";

				if (!ExecuteSQLQuery (outSQL)) return false;

				m_Id = max(m_Id,inGEDHRcd->hid+1);

				nh++;
			}
			break;
		}
	}

	m_Na += na;
	m_Nh += nh;
	m_Ns += ns;

	m_aInc=BUFFER_INC_8; for (size_t z=BUFFER_INC_8; z<=BUFFER_INC_65536 && ((m_Na>>m_aInc)>0L); z++) m_aInc = (TBufferInc)z;
	m_hInc=BUFFER_INC_8; for (size_t z=BUFFER_INC_8; z<=BUFFER_INC_65536 && ((m_Nh>>m_hInc)>0L); z++) m_hInc = (TBufferInc)z;
	m_sInc=BUFFER_INC_8; for (size_t z=BUFFER_INC_8; z<=BUFFER_INC_65536 && ((m_Ns>>m_sInc)>0L); z++) m_sInc = (TBufferInc)z;

	CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_BKD_OPERATION, 
				   "MYSQL backend recovered " + CString(na) + "/" + CString(naa) + " active queue records, " + 
				   CString(nh) + "/" + CString(nhh) + " history queue records, " +
				   CString(ns) + "/" + CString(nss) + " sync queue records");

	return true;
}

//----------------------------------------------------------------------------------------------------------------------------------------
// retreive backend config cache
//----------------------------------------------------------------------------------------------------------------------------------------
bool CGEDBackEndMySQL::ReadCfgCache (TKeyBuffer <UInt32, CString> &outPktHash, UInt32 &outVersion)
{
	CString outSQL ("SELECT * FROM " + GED_MYSQL_PACKET_TYPE_TB);

	if (!ExecuteSQLQuery (outSQL)) return false;

	MYSQL_RES *inSQLRes = ::mysql_store_result (&m_MYSQL);
	MYSQL_ROW  inSQLRow = NULL;

	#ifdef __GED_DEBUG_SQL__
	UInt32 nrows = ::mysql_num_rows (inSQLRes);
	CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_DEBUG, "MYSQL backend nrows = " + CString(nrows));
	#endif

	while (inSQLRow = ::mysql_fetch_row (inSQLRes))
	{
		outPktHash[CString(inSQLRow[GED_MYSQL_PACKET_TYPE_TB_COL_ID_IDX]).ToULong()] = inSQLRow[GED_MYSQL_PACKET_TYPE_TB_COL_HASH_IDX];
		if (CString(inSQLRow[GED_MYSQL_PACKET_TYPE_TB_COL_ID_IDX]).ToULong() == 0L)
			outVersion = CString(inSQLRow[GED_MYSQL_PACKET_TYPE_TB_COL_VERS_IDX]).ToULong();
	}

	::mysql_free_result (inSQLRes);

	return true;
}

//----------------------------------------------------------------------------------------------------------------------------------------
// save backend config cache
//----------------------------------------------------------------------------------------------------------------------------------------
bool CGEDBackEndMySQL::WriteCfgCache ()
{
	CString outSQL = "DELETE FROM "+ GED_MYSQL_PACKET_TYPE_TB + " WHERE " + GED_MYSQL_PACKET_TYPE_TB_COL_ID + "=0";

	if (!ExecuteSQLQuery (outSQL)) return false;

	outSQL = "INSERT INTO " + GED_MYSQL_PACKET_TYPE_TB + " (" + GED_MYSQL_PACKET_TYPE_TB_COL_ID + ", " + GED_MYSQL_PACKET_TYPE_TB_COL_NAME + ", " +
		 GED_MYSQL_PACKET_TYPE_TB_COL_HASH + ", " + GED_MYSQL_PACKET_TYPE_TB_COL_VERS + ") VALUES (" + CString(0L) + ", \'md5sum\', \'" + 
		 ::HashGEDPktCfg(m_GEDCfg.pkts) + "\', " + CString((SInt32)GED_VERSION) + ")";

	if (!ExecuteSQLQuery (outSQL)) return false;

	for (size_t i=0; i<m_GEDCfg.pkts.GetLength(); i++)
	{
		outSQL = "DELETE FROM " + GED_MYSQL_PACKET_TYPE_TB + " WHERE " + GED_MYSQL_PACKET_TYPE_TB_COL_ID + "=" + CString((UInt32)m_GEDCfg.pkts[i]->type);

		if (!ExecuteSQLQuery (outSQL)) return false;

		outSQL = "INSERT INTO " + GED_MYSQL_PACKET_TYPE_TB + " (" + GED_MYSQL_PACKET_TYPE_TB_COL_ID + ", " + GED_MYSQL_PACKET_TYPE_TB_COL_NAME + ", " +
				GED_MYSQL_PACKET_TYPE_TB_COL_HASH + ", " + GED_MYSQL_PACKET_TYPE_TB_COL_VERS + ") VALUES (" + CString((UInt32)m_GEDCfg.pkts[i]->type) + ", \'" + 
				m_GEDCfg.pkts[i]->name + "\', \'" + ::HashGEDPktCfg(m_GEDCfg.pkts[i]) + "\', 0)";

		if (!ExecuteSQLQuery (outSQL)) return false;
	}

	return true;
}

//----------------------------------------------------------------------------------------------------------------------------------------
// ged version upgrade notification handler
//----------------------------------------------------------------------------------------------------------------------------------------
bool CGEDBackEndMySQL::NotifyVersionUpgrade (const UInt32 inOldVersion)
{
	// current version
	switch (GED_VERSION)
	{
		case 10204 :
		{
			switch (inOldVersion)
			{
				// 1.2.2 => 1.2.4 : alter varchar len 255 => 32768
				// 1.2.3 => 1.2.4 : alter varchar len 255 => 32768
				case 10202 :
				case 10203 :
				{
					for (size_t i=0; i<m_GEDCfg.pkts.GetLength(); i++)
					{
						CString outSQL ("ALTER TABLE "); outSQL += m_GEDCfg.pkts[i]->name + " MODIFY ";

						for (size_t j=0; j<m_GEDCfg.pkts[i]->fields.GetLength(); j++)
						{
							if (m_GEDCfg.pkts[i]->fields[j]->type == DATA_STRING)
							{
								CString outReq (outSQL + m_GEDCfg.pkts[i]->fields[j]->name + 
									" VARCHAR(" + m_MySQLVarcharLength + ") NOT NULL DEFAULT \'\'");
								if (!ExecuteSQLQuery (outReq)) return false;
							}
						}
					}
				}
				break;
			}
		}
		break;
	}

	return true;
}

//----------------------------------------------------------------------------------------------------------------------------------------
// pkt cfg change notification handler (packet configuration add/mod/del)
//----------------------------------------------------------------------------------------------------------------------------------------
bool CGEDBackEndMySQL::NotifyPktCfgChange (const long inType, const TGEDPktCfgChange inGEDPktCfgChange)
{
	switch (inGEDPktCfgChange)
	{
		case GED_PKT_CFG_CHANGE_CREATE :
		{
			TGEDPktCfg *inGEDPktCfg=NULL; for (size_t i=0; i<m_GEDCfg.pkts.GetLength() && inGEDPktCfg==NULL; i++)
				if (m_GEDCfg.pkts[i]->type == inType) inGEDPktCfg = m_GEDCfg.pkts[i];
			if (inGEDPktCfg == NULL) return false;

			CString outSQL ("CREATE TABLE IF NOT EXISTS " + inGEDPktCfg->name + " (" + 
					GED_MYSQL_DATA_TB_COL_ID		+ " INTEGER UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY, " +
					GED_MYSQL_DATA_TB_COL_QUEUE		+ " ENUM (\'a\', \'h\', \'s\') NOT NULL DEFAULT \'a\', " +
					GED_MYSQL_DATA_TB_COL_OCC		+ " INTEGER UNSIGNED NOT NULL DEFAULT 1, " +
					GED_MYSQL_DATA_TB_COL_OTV_SEC		+ " INTEGER UNSIGNED NOT NULL DEFAULT 0, " +
					GED_MYSQL_DATA_TB_COL_OTV_USEC		+ " INTEGER UNSIGNED NOT NULL DEFAULT 0, " +
					GED_MYSQL_DATA_TB_COL_LTV_SEC		+ " INTEGER UNSIGNED NOT NULL DEFAULT 0, " +
					GED_MYSQL_DATA_TB_COL_LTV_USEC		+ " INTEGER UNSIGNED NOT NULL DEFAULT 0, " +
					GED_MYSQL_DATA_TB_COL_RTV_SEC		+ " INTEGER UNSIGNED NOT NULL DEFAULT 0, " +
					GED_MYSQL_DATA_TB_COL_RTV_USEC		+ " INTEGER UNSIGNED NOT NULL DEFAULT 0, " +
					GED_MYSQL_DATA_TB_COL_MTV_SEC		+ " INTEGER UNSIGNED NOT NULL DEFAULT 0, " +
					GED_MYSQL_DATA_TB_COL_MTV_USEC		+ " INTEGER UNSIGNED NOT NULL DEFAULT 0, " +
          GED_MYSQL_DATA_TB_COL_FTV_SEC		+ " INTEGER UNSIGNED NOT NULL DEFAULT 0, " +
					GED_MYSQL_DATA_TB_COL_FTV_USEC		+ " INTEGER UNSIGNED NOT NULL DEFAULT 0, " +
					GED_MYSQL_DATA_TB_COL_ATV_SEC		+ " INTEGER UNSIGNED NOT NULL DEFAULT 0, " +
					GED_MYSQL_DATA_TB_COL_ATV_USEC		+ " INTEGER UNSIGNED NOT NULL DEFAULT 0, " +
					GED_MYSQL_DATA_TB_COL_REASON		+ " ENUM (\'na\', \'pkt\', \'ttl\') NOT NULL DEFAULT \'na\', " +
					GED_MYSQL_DATA_TB_COL_SRC		+ " VARCHAR(255) NOT NULL DEFAULT \'127.0.0.1\', " +
					GED_MYSQL_DATA_TB_COL_TGT		+ " VARCHAR(255) NOT NULL DEFAULT \'na\', " +
					GED_MYSQL_DATA_TB_COL_REQ		+ " ENUM (\'na\', \'push\', \'update\', \'drop_by_data\', \'drop_by_id\') NOT NULL DEFAULT \'na\'");

			for (size_t i=0; i<inGEDPktCfg->fields.GetLength(); i++)
			{
				switch (inGEDPktCfg->fields[i]->type)
				{
					case DATA_SINT32  :
					{
						outSQL += ", " + inGEDPktCfg->fields[i]->name + " INTEGER NOT NULL DEFAULT 0";
					}
					break;
					case DATA_STRING :
					{
						outSQL += ", " + inGEDPktCfg->fields[i]->name + " VARCHAR(" + m_MySQLVarcharLength + ") NOT NULL DEFAULT \'\'";
					}
					break;
					case DATA_FLOAT64 :
					{
						outSQL += ", " + inGEDPktCfg->fields[i]->name + " DOUBLE NOT NULL DEFAULT 0";
					}
					break;
				}
			}

			for (size_t i=0; i<inGEDPktCfg->keyidc.GetLength(); i++)
				outSQL += ", INDEX USING BTREE (" + inGEDPktCfg->fields[*inGEDPktCfg->keyidc[i]]->name + ")";

			outSQL += ", INDEX USING BTREE (" + GED_MYSQL_DATA_TB_COL_OTV_SEC + "), INDEX USING BTREE (" + GED_MYSQL_DATA_TB_COL_OTV_USEC + 
				  ")) ENGINE=" + GED_MYSQL_TB_TYPE;

			if (!ExecuteSQLQuery (outSQL)) return false;
/*
			outSQL = "INSERT INTO " + GED_MYSQL_PACKET_TYPE_TB + " (" + 
				 GED_MYSQL_PACKET_TYPE_TB_COL_ID + ", " + 
				 GED_MYSQL_PACKET_TYPE_TB_COL_NAME + ", " +
				 GED_MYSQL_PACKET_TYPE_TB_COL_HASH + ", " +
				 GED_MYSQL_PACKET_TYPE_TB_COL_VERS
				 ") VALUES (" + 
				 CString((long)inGEDPktCfg->type) + ", \'" + 
				 inGEDPktCfg->name + "\', \'" +
				 inGEDPktCfg->hash + "\', 0)";

			if (!ExecuteSQLQuery (outSQL)) return false;
*/
			CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_BKD_OPERATION, 
				"MYSQL backend created table \"" + inGEDPktCfg->name + "\" (type=" + CString((long)inGEDPktCfg->type) + ") due to cfg change");	
        
              
        // Active Tables....
     			outSQL = "CREATE TABLE IF NOT EXISTS " + inGEDPktCfg->name + GED_MYSQL_DATA_TBL_QUEUE_ACTIVE + " (" + 
					GED_MYSQL_DATA_TB_COL_ID		+ " INTEGER UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY, " +
					GED_MYSQL_DATA_TB_COL_QUEUE		+ " ENUM (\'a\', \'h\', \'s\') NOT NULL DEFAULT \'a\', " +
					GED_MYSQL_DATA_TB_COL_OCC		+ " INTEGER UNSIGNED NOT NULL DEFAULT 1, " +
					GED_MYSQL_DATA_TB_COL_OTV_SEC		+ " INTEGER UNSIGNED NOT NULL DEFAULT 0, " +
					GED_MYSQL_DATA_TB_COL_OTV_USEC		+ " INTEGER UNSIGNED NOT NULL DEFAULT 0, " +
					GED_MYSQL_DATA_TB_COL_LTV_SEC		+ " INTEGER UNSIGNED NOT NULL DEFAULT 0, " +
					GED_MYSQL_DATA_TB_COL_LTV_USEC		+ " INTEGER UNSIGNED NOT NULL DEFAULT 0, " +
					GED_MYSQL_DATA_TB_COL_RTV_SEC		+ " INTEGER UNSIGNED NOT NULL DEFAULT 0, " +
					GED_MYSQL_DATA_TB_COL_RTV_USEC		+ " INTEGER UNSIGNED NOT NULL DEFAULT 0, " +
					GED_MYSQL_DATA_TB_COL_MTV_SEC		+ " INTEGER UNSIGNED NOT NULL DEFAULT 0, " +
					GED_MYSQL_DATA_TB_COL_MTV_USEC		+ " INTEGER UNSIGNED NOT NULL DEFAULT 0, " +
					GED_MYSQL_DATA_TB_COL_FTV_SEC		+ " INTEGER UNSIGNED NOT NULL DEFAULT 0, " +
					GED_MYSQL_DATA_TB_COL_FTV_USEC		+ " INTEGER UNSIGNED NOT NULL DEFAULT 0, " +
					GED_MYSQL_DATA_TB_COL_ATV_SEC		+ " INTEGER UNSIGNED NOT NULL DEFAULT 0, " +
					GED_MYSQL_DATA_TB_COL_ATV_USEC		+ " INTEGER UNSIGNED NOT NULL DEFAULT 0, " +
					GED_MYSQL_DATA_TB_COL_REASON		+ " ENUM (\'na\', \'pkt\', \'ttl\') NOT NULL DEFAULT \'na\', " +
					GED_MYSQL_DATA_TB_COL_SRC		+ " VARCHAR(255) NOT NULL DEFAULT \'127.0.0.1\', " +
					GED_MYSQL_DATA_TB_COL_TGT		+ " VARCHAR(255) NOT NULL DEFAULT \'na\', " +
					GED_MYSQL_DATA_TB_COL_REQ		+ " ENUM (\'na\', \'push\', \'update\', \'drop_by_data\', \'drop_by_id\') NOT NULL DEFAULT \'na\'";

			for (size_t i=0; i<inGEDPktCfg->fields.GetLength(); i++)
			{
				switch (inGEDPktCfg->fields[i]->type)
				{
					case DATA_SINT32  :
					{
						outSQL += ", " + inGEDPktCfg->fields[i]->name + " INTEGER NOT NULL DEFAULT 0";
					}
					break;
					case DATA_STRING :
					{
						outSQL += ", " + inGEDPktCfg->fields[i]->name + " VARCHAR(" + m_MySQLVarcharLength + ") NOT NULL DEFAULT \'\'";
					}
					break;
					case DATA_FLOAT64 :
					{
						outSQL += ", " + inGEDPktCfg->fields[i]->name + " DOUBLE NOT NULL DEFAULT 0";
					}
					break;
				}
			}

			for (size_t i=0; i<inGEDPktCfg->keyidc.GetLength(); i++)
				outSQL += ", INDEX USING BTREE (" + inGEDPktCfg->fields[*inGEDPktCfg->keyidc[i]]->name + ")";

			outSQL += ", INDEX USING BTREE (" + GED_MYSQL_DATA_TB_COL_OTV_SEC + "), INDEX USING BTREE (" + GED_MYSQL_DATA_TB_COL_OTV_USEC + 
				  ")) ENGINE=" + GED_MYSQL_TB_TYPE;

			if (!ExecuteSQLQuery (outSQL)) return false;

			CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_BKD_OPERATION, 
				"MYSQL backend created table \"" + inGEDPktCfg->name + GED_MYSQL_DATA_TBL_QUEUE_ACTIVE + "\" (type=" + CString((long)inGEDPktCfg->type) + ") due to cfg change");	
        
        
        // History Tables....
     			outSQL = "CREATE TABLE IF NOT EXISTS " + inGEDPktCfg->name + GED_MYSQL_DATA_TBL_QUEUE_HISTORY + " (" + 
					GED_MYSQL_DATA_TB_COL_ID		+ " INTEGER UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY, " +
					GED_MYSQL_DATA_TB_COL_QUEUE		+ " ENUM (\'a\', \'h\', \'s\') NOT NULL DEFAULT \'a\', " +
					GED_MYSQL_DATA_TB_COL_OCC		+ " INTEGER UNSIGNED NOT NULL DEFAULT 1, " +
					GED_MYSQL_DATA_TB_COL_OTV_SEC		+ " INTEGER UNSIGNED NOT NULL DEFAULT 0, " +
					GED_MYSQL_DATA_TB_COL_OTV_USEC		+ " INTEGER UNSIGNED NOT NULL DEFAULT 0, " +
					GED_MYSQL_DATA_TB_COL_LTV_SEC		+ " INTEGER UNSIGNED NOT NULL DEFAULT 0, " +
					GED_MYSQL_DATA_TB_COL_LTV_USEC		+ " INTEGER UNSIGNED NOT NULL DEFAULT 0, " +
					GED_MYSQL_DATA_TB_COL_RTV_SEC		+ " INTEGER UNSIGNED NOT NULL DEFAULT 0, " +
					GED_MYSQL_DATA_TB_COL_RTV_USEC		+ " INTEGER UNSIGNED NOT NULL DEFAULT 0, " +
					GED_MYSQL_DATA_TB_COL_MTV_SEC		+ " INTEGER UNSIGNED NOT NULL DEFAULT 0, " +
					GED_MYSQL_DATA_TB_COL_MTV_USEC		+ " INTEGER UNSIGNED NOT NULL DEFAULT 0, " +
					GED_MYSQL_DATA_TB_COL_FTV_SEC		+ " INTEGER UNSIGNED NOT NULL DEFAULT 0, " +
					GED_MYSQL_DATA_TB_COL_FTV_USEC		+ " INTEGER UNSIGNED NOT NULL DEFAULT 0, " +
					GED_MYSQL_DATA_TB_COL_ATV_SEC		+ " INTEGER UNSIGNED NOT NULL DEFAULT 0, " +
					GED_MYSQL_DATA_TB_COL_ATV_USEC		+ " INTEGER UNSIGNED NOT NULL DEFAULT 0, " +
					GED_MYSQL_DATA_TB_COL_REASON		+ " ENUM (\'na\', \'pkt\', \'ttl\') NOT NULL DEFAULT \'na\', " +
					GED_MYSQL_DATA_TB_COL_SRC		+ " VARCHAR(255) NOT NULL DEFAULT \'127.0.0.1\', " +
					GED_MYSQL_DATA_TB_COL_TGT		+ " VARCHAR(255) NOT NULL DEFAULT \'na\', " +
					GED_MYSQL_DATA_TB_COL_REQ		+ " ENUM (\'na\', \'push\', \'update\', \'drop_by_data\', \'drop_by_id\') NOT NULL DEFAULT \'na\'";

			for (size_t i=0; i<inGEDPktCfg->fields.GetLength(); i++)
			{
				switch (inGEDPktCfg->fields[i]->type)
				{
					case DATA_SINT32  :
					{
						outSQL += ", " + inGEDPktCfg->fields[i]->name + " INTEGER NOT NULL DEFAULT 0";
					}
					break;
					case DATA_STRING :
					{
						outSQL += ", " + inGEDPktCfg->fields[i]->name + " VARCHAR(" + m_MySQLVarcharLength + ") NOT NULL DEFAULT \'\'";
					}
					break;
					case DATA_FLOAT64 :
					{
						outSQL += ", " + inGEDPktCfg->fields[i]->name + " DOUBLE NOT NULL DEFAULT 0";
					}
					break;
				}
			}

			for (size_t i=0; i<inGEDPktCfg->keyidc.GetLength(); i++)
				outSQL += ", INDEX USING BTREE (" + inGEDPktCfg->fields[*inGEDPktCfg->keyidc[i]]->name + ")";

			outSQL += ", INDEX USING BTREE (" + GED_MYSQL_DATA_TB_COL_OTV_SEC + "), INDEX USING BTREE (" + GED_MYSQL_DATA_TB_COL_OTV_USEC + 
				  ")) ENGINE=" + GED_MYSQL_TB_TYPE;

			if (!ExecuteSQLQuery (outSQL)) return false;

			CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_BKD_OPERATION, 
				"MYSQL backend created table \"" + inGEDPktCfg->name + GED_MYSQL_DATA_TBL_QUEUE_HISTORY + "\" (type=" + CString((long)inGEDPktCfg->type) + ") due to cfg change");	
        
        
        
            // Sync Tables....
     			 outSQL = "CREATE TABLE IF NOT EXISTS " + inGEDPktCfg->name + GED_MYSQL_DATA_TBL_QUEUE_SYNC + " (" + 
					GED_MYSQL_DATA_TB_COL_ID		+ " INTEGER UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY, " +
					GED_MYSQL_DATA_TB_COL_QUEUE		+ " ENUM (\'a\', \'h\', \'s\') NOT NULL DEFAULT \'a\', " +
					GED_MYSQL_DATA_TB_COL_OCC		+ " INTEGER UNSIGNED NOT NULL DEFAULT 1, " +
					GED_MYSQL_DATA_TB_COL_OTV_SEC		+ " INTEGER UNSIGNED NOT NULL DEFAULT 0, " +
					GED_MYSQL_DATA_TB_COL_OTV_USEC		+ " INTEGER UNSIGNED NOT NULL DEFAULT 0, " +
					GED_MYSQL_DATA_TB_COL_LTV_SEC		+ " INTEGER UNSIGNED NOT NULL DEFAULT 0, " +
					GED_MYSQL_DATA_TB_COL_LTV_USEC		+ " INTEGER UNSIGNED NOT NULL DEFAULT 0, " +
					GED_MYSQL_DATA_TB_COL_RTV_SEC		+ " INTEGER UNSIGNED NOT NULL DEFAULT 0, " +
					GED_MYSQL_DATA_TB_COL_RTV_USEC		+ " INTEGER UNSIGNED NOT NULL DEFAULT 0, " +
					GED_MYSQL_DATA_TB_COL_MTV_SEC		+ " INTEGER UNSIGNED NOT NULL DEFAULT 0, " +
					GED_MYSQL_DATA_TB_COL_MTV_USEC		+ " INTEGER UNSIGNED NOT NULL DEFAULT 0, " +
					GED_MYSQL_DATA_TB_COL_FTV_SEC		+ " INTEGER UNSIGNED NOT NULL DEFAULT 0, " +
					GED_MYSQL_DATA_TB_COL_FTV_USEC		+ " INTEGER UNSIGNED NOT NULL DEFAULT 0, " +
					GED_MYSQL_DATA_TB_COL_ATV_SEC		+ " INTEGER UNSIGNED NOT NULL DEFAULT 0, " +
					GED_MYSQL_DATA_TB_COL_ATV_USEC		+ " INTEGER UNSIGNED NOT NULL DEFAULT 0, " +
					GED_MYSQL_DATA_TB_COL_REASON		+ " ENUM (\'na\', \'pkt\', \'ttl\') NOT NULL DEFAULT \'na\', " +
					GED_MYSQL_DATA_TB_COL_SRC		+ " VARCHAR(255) NOT NULL DEFAULT \'127.0.0.1\', " +
					GED_MYSQL_DATA_TB_COL_TGT		+ " VARCHAR(255) NOT NULL DEFAULT \'na\', " +
					GED_MYSQL_DATA_TB_COL_REQ		+ " ENUM (\'na\', \'push\', \'update\', \'drop_by_data\', \'drop_by_id\') NOT NULL DEFAULT \'na\'";

			for (size_t i=0; i<inGEDPktCfg->fields.GetLength(); i++)
			{
				switch (inGEDPktCfg->fields[i]->type)
				{
					case DATA_SINT32  :
					{
						outSQL += ", " + inGEDPktCfg->fields[i]->name + " INTEGER NOT NULL DEFAULT 0";
					}
					break;
					case DATA_STRING :
					{
						outSQL += ", " + inGEDPktCfg->fields[i]->name + " VARCHAR(" + m_MySQLVarcharLength + ") NOT NULL DEFAULT \'\'";
					}
					break;
					case DATA_FLOAT64 :
					{
						outSQL += ", " + inGEDPktCfg->fields[i]->name + " DOUBLE NOT NULL DEFAULT 0";
					}
					break;
				}
			}

			for (size_t i=0; i<inGEDPktCfg->keyidc.GetLength(); i++)
				outSQL += ", INDEX USING BTREE (" + inGEDPktCfg->fields[*inGEDPktCfg->keyidc[i]]->name + ")";

			outSQL += ", INDEX USING BTREE (" + GED_MYSQL_DATA_TB_COL_OTV_SEC + "), INDEX USING BTREE (" + GED_MYSQL_DATA_TB_COL_OTV_USEC + 
				  ")) ENGINE=" + GED_MYSQL_TB_TYPE;

			if (!ExecuteSQLQuery (outSQL)) return false;

			CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_BKD_OPERATION, 
				"MYSQL backend created table \"" + inGEDPktCfg->name + GED_MYSQL_DATA_TBL_QUEUE_SYNC + "\" (type=" + CString((long)inGEDPktCfg->type) + ") due to cfg change");	
        
		}
		break;

		case GED_PKT_CFG_CHANGE_MODIFY :
		{
			if (!NotifyPktCfgChange (inType, GED_PKT_CFG_CHANGE_DELETE)) return false;
			if (!NotifyPktCfgChange (inType, GED_PKT_CFG_CHANGE_CREATE)) return false;
		}
		break;

		case GED_PKT_CFG_CHANGE_DELETE : ///////// ATTENTION MAY NOT WORK ANYMORE due to GED_MYSQL_PACKET_TYPE_TB wich is not enough !!!!!!!
		{
			CString outSQL ("SELECT " + GED_MYSQL_PACKET_TYPE_TB_COL_NAME + " FROM " + GED_MYSQL_PACKET_TYPE_TB + " WHERE " +
					GED_MYSQL_PACKET_TYPE_TB_COL_ID + "=" + CString(inType));

			if (!ExecuteSQLQuery (outSQL)) return false;

			MYSQL_RES *inSQLRes = ::mysql_store_result (&m_MYSQL);
			#ifdef __GED_DEBUG_SQL__
			UInt32 nrows = ::mysql_num_rows (inSQLRes);
			CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_DEBUG, "MYSQL backend nrows = " + CString(nrows));
			#endif
			MYSQL_ROW  inSQLRow = ::mysql_fetch_row	   (inSQLRes);

			if (inSQLRow == NULL) { ::mysql_free_result (inSQLRes); return true; }

			CString inTableName (inSQLRow[0]);

			::mysql_free_result (inSQLRes);
			
			outSQL = "DROP TABLE IF EXISTS " + inTableName; 

			if (!ExecuteSQLQuery (outSQL)) return false;

			outSQL = "DELETE FROM " + GED_MYSQL_PACKET_TYPE_TB + " WHERE " + GED_MYSQL_PACKET_TYPE_TB_COL_ID + "=" + CString(inType);

			if (!ExecuteSQLQuery (outSQL)) return false;

			CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_BKD_OPERATION, 
				"MYSQL backend deleted table \"" + inTableName + "\" (type=" + CString(inType) + ") due to cfg change");
		}
		break;
	}

	return true;
}

//----------------------------------------------------------------------------------------------------------------------------------------
// TTL thread callback
//----------------------------------------------------------------------------------------------------------------------------------------
void * CGEDBackEndMySQL::m_TTLTimerCB (void *inParam)
{
	CGEDBackEndMySQL *inGEDBackEndMySQL = reinterpret_cast <CGEDBackEndMySQL *> (inParam);

	while (true)
	{
		if (inGEDBackEndMySQL->Lock() != 0) break;

		struct timeval tv; ::gettimeofday (&tv, NULL); UInt32 na=0L, ns=0L, nh=0L;

		for (size_t j=0; j<inGEDBackEndMySQL->m_GEDCfg.pkts.GetLength(); j++)
		{
			if (inGEDBackEndMySQL->m_aTTL > 0)
			{
				CString outSQL = "UPDATE " + 
							inGEDBackEndMySQL->m_GEDCfg.pkts[j]->name + GED_MYSQL_DATA_TBL_QUEUE_HISTORY +
						 " SET " + 
							GED_MYSQL_DATA_TB_COL_QUEUE + "=\'h\', " +
							GED_MYSQL_DATA_TB_COL_ATV_SEC + "=" + CString((UInt32)tv.tv_sec) + ", " + 
							GED_MYSQL_DATA_TB_COL_ATV_USEC + "=" + CString((UInt32)tv.tv_usec) + ", " +
							GED_MYSQL_DATA_TB_COL_REASON + "=\'ttl\'" +
						 " WHERE " +
							GED_MYSQL_DATA_TB_COL_OTV_SEC + "<=" + CString((UInt32)tv.tv_sec-inGEDBackEndMySQL->m_aTTL);

				if (!inGEDBackEndMySQL->ExecuteSQLQuery (outSQL)) return false;

				na += ::mysql_affected_rows (&inGEDBackEndMySQL->m_MYSQL);
			}

			if (inGEDBackEndMySQL->m_hTTL > 0)
			{
				CString outSQL = "DELETE FROM " + 
							inGEDBackEndMySQL->m_GEDCfg.pkts[j]->name + GED_MYSQL_DATA_TBL_QUEUE_HISTORY +
						 " WHERE " + 
							GED_MYSQL_DATA_TB_COL_QUEUE + "=\'h\' AND " +
							GED_MYSQL_DATA_TB_COL_OTV_SEC + "<=" + CString((UInt32)tv.tv_sec-inGEDBackEndMySQL->m_hTTL);

				TBuffer <TGEDRcd *> inGEDRcds; inGEDRcds.SetInc (inGEDBackEndMySQL->m_hInc); if (inGEDBackEndMySQL->m_GEDCfg.loghloc)
				{
					CString sql (outSQL);

					sql.Substitute (CString("DELETE"),CString("SELECT *"));

					if (!inGEDBackEndMySQL->ExecuteSQLQuery (sql)) return false;

					MYSQL_RES *inSQLRes = ::mysql_store_result (&inGEDBackEndMySQL->m_MYSQL);
					MYSQL_ROW  inSQLRow = NULL;

					#ifdef __GED_DEBUG_SQL__
					UInt32 nrows = ::mysql_num_rows (inSQLRes);
					CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_DEBUG, "MYSQL backend nrows = " + CString(nrows));
					#endif

					while (inSQLRow = ::mysql_fetch_row (inSQLRes))
						inGEDRcds += inGEDBackEndMySQL->MySQLRowToHRcd (inSQLRow, inGEDBackEndMySQL->m_GEDCfg.pkts[j]);

					::mysql_free_result (inSQLRes);
				}

				if (!inGEDBackEndMySQL->ExecuteSQLQuery (outSQL))
				{
					for (size_t k=inGEDRcds.GetLength(); k>0; k--) ::DeleteGEDRcd (*inGEDRcds[k-1]);

					return false;
				}

				for (size_t k=inGEDRcds.GetLength(), l=0; k>0; k--, l++)
				{
					::syslog (inGEDBackEndMySQL->m_GEDCfg.loghloc|GED_SYSLOG_INFO, "%s", 
						  ::GEDRcdToString(*inGEDRcds[l],inGEDBackEndMySQL->m_GEDCfg.pkts).Get());

					::DeleteGEDRcd (*inGEDRcds[l]);
				}

				nh += ::mysql_affected_rows (&inGEDBackEndMySQL->m_MYSQL);	
			}

			if (inGEDBackEndMySQL->m_sTTL > 0)
			{
				CString outSQL = "DELETE FROM " + 
							inGEDBackEndMySQL->m_GEDCfg.pkts[j]->name + GED_MYSQL_DATA_TBL_QUEUE_SYNC +
						 " WHERE " + 
							GED_MYSQL_DATA_TB_COL_QUEUE + "=\'s\' AND " +
							GED_MYSQL_DATA_TB_COL_OTV_SEC + "<=" + CString((UInt32)tv.tv_sec-inGEDBackEndMySQL->m_sTTL);

				if (!inGEDBackEndMySQL->ExecuteSQLQuery (outSQL)) return false;

				ns += ::mysql_affected_rows (&inGEDBackEndMySQL->m_MYSQL);
			}
		}

		if (CGEDBackEndMySQL::m_fTTL)
		{
			inGEDBackEndMySQL->m_Id = 0L;
			inGEDBackEndMySQL->m_Na = 0L;
			inGEDBackEndMySQL->m_Nh = 0L;
			inGEDBackEndMySQL->m_Ns = 0L;

			for (size_t j=0; j<inGEDBackEndMySQL->m_GEDCfg.pkts.GetLength(); j++)
			{
				CString outSQL = "SELECT COUNT(*) FROM " + inGEDBackEndMySQL->m_GEDCfg.pkts[j]->name + " WHERE " + GED_MYSQL_DATA_TB_COL_QUEUE + "=\'a\'";
				if (!inGEDBackEndMySQL->ExecuteSQLQuery (outSQL)) return false;
				MYSQL_RES *inSQLRes = ::mysql_store_result (&inGEDBackEndMySQL->m_MYSQL); MYSQL_ROW inSQLRow = ::mysql_fetch_row (inSQLRes);
				inGEDBackEndMySQL->m_Na += CString(inSQLRow[0]).ToULong();
				::mysql_free_result (inSQLRes);

				outSQL = "SELECT COUNT(*) FROM " + inGEDBackEndMySQL->m_GEDCfg.pkts[j]->name + " WHERE " + GED_MYSQL_DATA_TB_COL_QUEUE + "=\'h\'";
				if (!inGEDBackEndMySQL->ExecuteSQLQuery (outSQL)) return false;
				inSQLRes = ::mysql_store_result (&inGEDBackEndMySQL->m_MYSQL); inSQLRow = ::mysql_fetch_row (inSQLRes);
				inGEDBackEndMySQL->m_Nh += CString(inSQLRow[0]).ToULong();
				::mysql_free_result (inSQLRes);

				outSQL = "SELECT COUNT(*) FROM " + inGEDBackEndMySQL->m_GEDCfg.pkts[j]->name + " WHERE " + GED_MYSQL_DATA_TB_COL_QUEUE + "=\'s\'";
				if (!inGEDBackEndMySQL->ExecuteSQLQuery (outSQL)) return false;
				inSQLRes = ::mysql_store_result (&inGEDBackEndMySQL->m_MYSQL); inSQLRow = ::mysql_fetch_row (inSQLRes);
				inGEDBackEndMySQL->m_Ns += CString(inSQLRow[0]).ToULong();
				::mysql_free_result (inSQLRes);

				outSQL = "SELECT MAX(" + GED_MYSQL_DATA_TB_COL_ID + ") FROM " + inGEDBackEndMySQL->m_GEDCfg.pkts[j]->name;
				if (!inGEDBackEndMySQL->ExecuteSQLQuery (outSQL)) return false;
				inSQLRes = ::mysql_store_result (&inGEDBackEndMySQL->m_MYSQL); inSQLRow = ::mysql_fetch_row (inSQLRes);
				inGEDBackEndMySQL->m_Id = max(inGEDBackEndMySQL->m_Id,(CString(inSQLRow[0]).ToULong()+1));
				::mysql_free_result (inSQLRes);
			}

			for (size_t z=inGEDBackEndMySQL->m_aInc; z<=BUFFER_INC_65536 && ((inGEDBackEndMySQL->m_Na>>inGEDBackEndMySQL->m_aInc)>0L); z++) inGEDBackEndMySQL->m_aInc = (TBufferInc)z;
			for (size_t z=inGEDBackEndMySQL->m_hInc; z<=BUFFER_INC_65536 && ((inGEDBackEndMySQL->m_Nh>>inGEDBackEndMySQL->m_hInc)>0L); z++) inGEDBackEndMySQL->m_hInc = (TBufferInc)z;
			for (size_t z=inGEDBackEndMySQL->m_sInc; z<=BUFFER_INC_65536 && ((inGEDBackEndMySQL->m_Ns>>inGEDBackEndMySQL->m_sInc)>0L); z++) inGEDBackEndMySQL->m_sInc = (TBufferInc)z;
		}
	
		CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_BKD_OPERATION, "MYSQL backend TTL droped " + CString(na) +
			" active record(s), " + CString(ns) + " sync record(s), " + CString(nh) + " history record(s), next check in " +
			CString(inGEDBackEndMySQL->m_TTLTimer) + "s");

		if (inGEDBackEndMySQL->UnLock() != 0) break;

		CGEDBackEndMySQL::m_fTTL = true;

		::sleep (inGEDBackEndMySQL->m_TTLTimer);
	}

	return NULL;
}

//----------------------------------------------------------------------------------------------------------------------------------------
// sql query execution, server reconnection try when lost
//----------------------------------------------------------------------------------------------------------------------------------------
bool CGEDBackEndMySQL::ExecuteSQLQuery (const CString &inSQL)
{
	#ifdef __GED_DEBUG_SQL__
	CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_DEBUG, "MYSQL backend " + inSQL);
	struct timeval tm1; ::gettimeofday (&tm1, NULL);
	#endif

	if (::mysql_real_query (&m_MYSQL, inSQL.Get(), inSQL.GetLength()) != 0)
	{
		/*
		switch (::mysql_errno(&m_MYSQL))
		{
			case CR_SERVER_GONE_ERROR :
			case CR_SERVER_LOST :
			{
				CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_DETAIL_CONN, "MYSQL backend " + 
					   CString(::mysql_error(&m_MYSQL)) + ", trying to resume...");

				if (m_MySQLOptReconnect.GetLength() > 0)
				{
					my_bool inReconnect = m_MySQLOptReconnect.ToBool();
					::mysql_options (&m_MYSQL, MYSQL_OPT_RECONNECT, (const char*)&inReconnect);
				}

				if (::mysql_real_connect (&m_MYSQL, m_MySQLHost.Get(), m_MySQLLogin.Get(), m_MySQLPassword.Get(), 
					m_MySQLDatabase.Get(), m_MySQLPort, NULL, 0) == NULL)
				{
					CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_DETAIL_CONN, "MYSQL backend " + 
						CString(::mysql_error(&m_MYSQL)));
					return false;
				}

				if (m_MySQLNoBackslashEscapes.ToBool())
					if (!ExecuteSQLQuery (CString("SET sql_mode=\'NO_BACKSLASH_ESCAPES\'"))) return false;

				CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_CONNECTION, 
					"MYSQL backend successfully connected to " + m_MySQLHost + ":" + CString(m_MySQLPort) + " [" +
					CString(::mysql_get_server_info(&m_MYSQL)) + "], resuming request");

				return ExecuteSQLQuery (inSQL);
			}
			break;

			default :
			{
				CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_CONNECTION, "MYSQL backend " + 
					   CString(::mysql_error(&m_MYSQL)));
			}
			break;
		}
		
		return false;
		*/

		CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_DETAIL_CONN, "MYSQL backend " + CString(::mysql_error(&m_MYSQL)) + ", trying to resume...");

		::mysql_close (&m_MYSQL);
		::mysql_init  (&m_MYSQL);

		if (CGEDCtx::m_GEDCtx->m_GEDCfg.bkdcfg.Contains(GEDCfgMySQLOptConnectTimeout) && CGEDCtx::m_GEDCtx->m_GEDCfg.bkdcfg[GEDCfgMySQLOptConnectTimeout].GetLength()>0)
		{
			UInt32 inConnectTimeout (CGEDCtx::m_GEDCtx->m_GEDCfg.bkdcfg[GEDCfgMySQLOptConnectTimeout][0]->ToULong());
			::mysql_options (&m_MYSQL, MYSQL_OPT_CONNECT_TIMEOUT, (const char*)&inConnectTimeout);
		}

		if (CGEDCtx::m_GEDCtx->m_GEDCfg.bkdcfg.Contains(GEDCfgMySQLOptCompress) && CGEDCtx::m_GEDCtx->m_GEDCfg.bkdcfg[GEDCfgMySQLOptCompress].GetLength()>0)
			if (CGEDCtx::m_GEDCtx->m_GEDCfg.bkdcfg[GEDCfgMySQLOptCompress][0]->ToBool())
				::mysql_options (&m_MYSQL, MYSQL_OPT_COMPRESS, 0);
	
		if (m_MySQLOptReconnect.GetLength() > 0)
		{
			my_bool inReconnect = m_MySQLOptReconnect.ToBool();
			::mysql_options (&m_MYSQL, MYSQL_OPT_RECONNECT, (const char*)&inReconnect);
		}

		if (!::mysql_real_connect (&m_MYSQL, m_MySQLHost.Get(), m_MySQLLogin.Get(), m_MySQLPassword.Get(), m_MySQLDatabase.Get(), m_MySQLPort, NULL, 0))
		{
			CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_CONNECTION, "MYSQL backend "+CString(::mysql_error(&m_MYSQL)));
			return false;
		}

		if (m_MySQLNoBackslashEscapes.ToBool())
		{
			if (::mysql_real_query (&m_MYSQL, "SET sql_mode=\'NO_BACKSLASH_ESCAPES\'", 35) != 0)
			{
				CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_CONNECTION, "MYSQL backend "+CString(::mysql_error(&m_MYSQL)));
				return false;
			}
		}

		CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_CONNECTION, 
					"MYSQL backend successfully connected to " + m_MySQLHost + ":" + CString(m_MySQLPort) + " [" + 
					CString(::mysql_get_server_info(&m_MYSQL)) + "], resuming request");

		if (::mysql_real_query (&m_MYSQL, inSQL.Get(), inSQL.GetLength()) != 0)
		{
			CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_DETAIL_CONN, "MYSQL backend " + CString(::mysql_error(&m_MYSQL)) + ", aborting.");
			return false;
		}
	}

	#ifdef __GED_DEBUG_SQL__
	struct timeval tm2; ::gettimeofday (&tm2, NULL); size_t n=0;
	if ((inSQL.Find(CString("INSERT"),0,&n) || inSQL.Find(CString("UPDATE"),0,&n)) && n==0)
		CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_DEBUG, "MYSQL backend " + CString(::mysql_info(&m_MYSQL)) + 
			", query time = " + CString(TIMEVAL_MSEC_SUBTRACT(tm2,tm1)) + "ms");
	else
		CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_DEBUG, "MYSQL backend query time = " + 
			CString(TIMEVAL_MSEC_SUBTRACT(tm2,tm1)) + "ms");
	#endif

	return true;
}

//----------------------------------------------------------------------------------------------------------------------------------------
// get a sql select query
//----------------------------------------------------------------------------------------------------------------------------------------
CString CGEDBackEndMySQL::GetSelectQuery (const int inQueue, const TGEDPktIn *inGEDPktIn, const TGEDPktCfg *inGEDPktCfg, const CString &inAddr, 
					  const UInt32 &inTm1, const UInt32 &inTm2, const UInt32 &inOff, const UInt32 &inNum)
{
	CString outSQL; CString inQuStr, inAddrCol; switch (inQueue)
	{
		case GED_PKT_REQ_BKD_ACTIVE :
		{
			inQuStr = "\'a\'";
      outSQL = "SELECT * FROM " + inGEDPktCfg->name + GED_MYSQL_DATA_TBL_QUEUE_ACTIVE + " WHERE " + GED_MYSQL_DATA_TB_COL_QUEUE + "=" + inQuStr;
			inAddrCol = GED_MYSQL_DATA_TB_COL_SRC;
		}
		break;

		case GED_PKT_REQ_BKD_SYNC :
		{
			inQuStr = "\'s\'";
      outSQL = "SELECT * FROM " + inGEDPktCfg->name + GED_MYSQL_DATA_TBL_QUEUE_SYNC + " WHERE " + GED_MYSQL_DATA_TB_COL_QUEUE + "=" + inQuStr;
			inAddrCol = GED_MYSQL_DATA_TB_COL_TGT;
		}
		break;

		case GED_PKT_REQ_BKD_HISTORY :
		{
			inQuStr = "\'h\'";
      outSQL = "SELECT * FROM " + inGEDPktCfg->name + GED_MYSQL_DATA_TBL_QUEUE_HISTORY + " WHERE " + GED_MYSQL_DATA_TB_COL_QUEUE + "=" + inQuStr;
			inAddrCol = GED_MYSQL_DATA_TB_COL_SRC;
		}
		break;

		default : return outSQL;
	}
  
//	outSQL = "SELECT * FROM " + inGEDPktCfg->name + " WHERE " + GED_MYSQL_DATA_TB_COL_QUEUE + "=" + inQuStr;

	if (inAddr != CString())
		outSQL += " AND " + inAddrCol + " LIKE \'%" + inAddr + "%\'";

	if (inTm1 > 0)
		outSQL += " AND " + GED_MYSQL_DATA_TB_COL_OTV_SEC + ">=" + CString(inTm1);

	if (inTm2 < UINT32MAX)
		outSQL += " AND " + GED_MYSQL_DATA_TB_COL_OTV_SEC + "<=" + CString(inTm2);

	if (inGEDPktIn != NULL && inGEDPktIn->data != NULL)
	{
		TBuffer <TData> inTData (::GEDPktCfgToTData (const_cast <TGEDPktCfg *> (inGEDPktCfg))); 
		CChunk inChunk (inGEDPktIn->data, inGEDPktIn->len, inTData, false);

		for (size_t i=0, j=0; i<inGEDPktCfg->keyidc.GetLength(); i++)
		{
			for (; j<*inGEDPktCfg->keyidc[i]; j++) inChunk++;

			switch (inGEDPktCfg->fields[*inGEDPktCfg->keyidc[i]]->type)
			{
				case DATA_STRING :
				{
					char *inStr=NULL; inChunk >> inStr; CString inString(inStr); if (inStr) delete [] inStr; 
					GED_MYSQL_FILTER_STRING(inString,m_MySQLLtGtFilter,m_MySQLQuotFilter);
					outSQL += " AND " + inGEDPktCfg->fields[*inGEDPktCfg->keyidc[i]]->name + " LIKE \'" + inString + "\'";
				}
				break;

				case DATA_SINT32 :
				{
					long inLong=0L; inChunk >> inLong;
					outSQL += " AND " + inGEDPktCfg->fields[*inGEDPktCfg->keyidc[i]]->name + "=" + CString(inLong);
				}
				break;

				case DATA_FLOAT64 :
				{
					double inDouble=0.; inChunk >> inDouble;
					outSQL += " AND " + inGEDPktCfg->fields[*inGEDPktCfg->keyidc[i]]->name + "=" + CString(inDouble);
				}
				break;
			}

			j++;
		}
	}

	if ((inGEDPktIn != NULL) && ((inGEDPktIn->req & GED_PKT_REQ_PEEK_SORT_MASK) == GED_PKT_REQ_PEEK_SORT_DSC))
		outSQL += " ORDER BY " + GED_MYSQL_DATA_TB_COL_OTV_SEC  + " DESC, " +
					 GED_MYSQL_DATA_TB_COL_OTV_USEC + " DESC";
	else
		outSQL += " ORDER BY " + GED_MYSQL_DATA_TB_COL_OTV_SEC  + " ASC, " +
					 GED_MYSQL_DATA_TB_COL_OTV_USEC + " ASC";

	if (inOff > 0 || inNum < UINT32MAX)
		outSQL += " LIMIT " + CString(inOff) + "," + CString(inNum);

	return outSQL;
}

//----------------------------------------------------------------------------------------------------------------------------------------
// sqlrow to active record
//----------------------------------------------------------------------------------------------------------------------------------------
TGEDARcd * CGEDBackEndMySQL::MySQLRowToARcd (MYSQL_ROW &inSQLRow, TGEDPktCfg *inGEDPktCfg)
{
	TGEDARcd *outGEDARcd = new TGEDARcd;
	::bzero (outGEDARcd, sizeof(TGEDARcd));

	outGEDARcd->queue 	= GED_PKT_REQ_BKD_ACTIVE;
	outGEDARcd->typ   	= inGEDPktCfg->type;
	outGEDARcd->occ   	= CString(inSQLRow[GED_MYSQL_DATA_TB_COL_OCC_IDX]).ToULong();
	outGEDARcd->otv.tv_sec  = CString(inSQLRow[GED_MYSQL_DATA_TB_COL_OTV_SEC_IDX]).ToULong();
	outGEDARcd->otv.tv_usec = CString(inSQLRow[GED_MYSQL_DATA_TB_COL_OTV_USEC_IDX]).ToULong();
	outGEDARcd->mtv.tv_sec  = CString(inSQLRow[GED_MYSQL_DATA_TB_COL_MTV_SEC_IDX]).ToULong();
	outGEDARcd->mtv.tv_usec = CString(inSQLRow[GED_MYSQL_DATA_TB_COL_MTV_USEC_IDX]).ToULong();
	outGEDARcd->ltv.tv_sec  = CString(inSQLRow[GED_MYSQL_DATA_TB_COL_LTV_SEC_IDX]).ToULong();
	outGEDARcd->ltv.tv_usec = CString(inSQLRow[GED_MYSQL_DATA_TB_COL_LTV_USEC_IDX]).ToULong();
 	outGEDARcd->ftv.tv_sec  = CString(inSQLRow[GED_MYSQL_DATA_TB_COL_FTV_SEC_IDX]).ToULong();
	outGEDARcd->ftv.tv_usec = CString(inSQLRow[GED_MYSQL_DATA_TB_COL_FTV_USEC_IDX]).ToULong();
	outGEDARcd->rtv.tv_sec  = CString(inSQLRow[GED_MYSQL_DATA_TB_COL_RTV_SEC_IDX]).ToULong();
	outGEDARcd->rtv.tv_usec = CString(inSQLRow[GED_MYSQL_DATA_TB_COL_RTV_USEC_IDX]).ToULong();

	CStrings inAddrs (CString(inSQLRow[GED_MYSQL_DATA_TB_COL_SRC_IDX]).Cut(CString(";")));

	outGEDARcd->nsrc = inAddrs.GetLength();
	outGEDARcd->src  = new TGEDRcdSrc [outGEDARcd->nsrc];

	for (size_t i=0; i<outGEDARcd->nsrc; i++)
	{
		if (inAddrs[i]->Find(CString("/")))
		{
			outGEDARcd->src[i].rly = inAddrs[i]->Find(CString("/1"));
			::GetStrAddrToAddrIn (*(inAddrs[i]->Cut(CString("/"))[0]), &outGEDARcd->src[i].addr);
		}
	}

	CChunk outChunk; for (size_t i=0; i<inGEDPktCfg->fields.GetLength(); i++)
	{
		switch (inGEDPktCfg->fields[i]->type)
		{
			case DATA_STRING  : outChunk << inSQLRow[i+GED_MYSQL_DATA_TB_COL_FIRST_DATA_IDX];		      break;
			case DATA_SINT32  : outChunk << CString(inSQLRow[i+GED_MYSQL_DATA_TB_COL_FIRST_DATA_IDX]).ToLong();   break;
			case DATA_FLOAT64 : outChunk << CString(inSQLRow[i+GED_MYSQL_DATA_TB_COL_FIRST_DATA_IDX]).ToDouble(); break;
		}
	}

	outGEDARcd->len = outChunk.GetSize();

	if (outGEDARcd->len)
	{
		outGEDARcd->data = new char [outGEDARcd->len];
		::memcpy (outGEDARcd->data, outChunk.GetChunk(), outGEDARcd->len);
	}

	return outGEDARcd;
}

//----------------------------------------------------------------------------------------------------------------------------------------
// sqlrow to history record
//----------------------------------------------------------------------------------------------------------------------------------------
TGEDHRcd * CGEDBackEndMySQL::MySQLRowToHRcd (MYSQL_ROW &inSQLRow, TGEDPktCfg *inGEDPktCfg)
{
	TGEDHRcd *outGEDHRcd = new TGEDHRcd;
	::bzero (outGEDHRcd, sizeof(TGEDHRcd));

	outGEDHRcd->queue 	= GED_PKT_REQ_BKD_HISTORY;
	outGEDHRcd->hid   	= CString(inSQLRow[GED_MYSQL_DATA_TB_COL_ID_IDX]).ToLong();
	outGEDHRcd->typ   	= inGEDPktCfg->type;
	outGEDHRcd->occ   	= CString(inSQLRow[GED_MYSQL_DATA_TB_COL_OCC_IDX]).ToULong();
	outGEDHRcd->otv.tv_sec  = CString(inSQLRow[GED_MYSQL_DATA_TB_COL_OTV_SEC_IDX]).ToULong();
	outGEDHRcd->otv.tv_usec = CString(inSQLRow[GED_MYSQL_DATA_TB_COL_OTV_USEC_IDX]).ToULong();
	outGEDHRcd->mtv.tv_sec  = CString(inSQLRow[GED_MYSQL_DATA_TB_COL_MTV_SEC_IDX]).ToULong();
	outGEDHRcd->mtv.tv_usec = CString(inSQLRow[GED_MYSQL_DATA_TB_COL_MTV_USEC_IDX]).ToULong();
	outGEDHRcd->ltv.tv_sec  = CString(inSQLRow[GED_MYSQL_DATA_TB_COL_LTV_SEC_IDX]).ToULong();
	outGEDHRcd->ltv.tv_usec = CString(inSQLRow[GED_MYSQL_DATA_TB_COL_LTV_USEC_IDX]).ToULong();
	outGEDHRcd->rtv.tv_sec  = CString(inSQLRow[GED_MYSQL_DATA_TB_COL_RTV_SEC_IDX]).ToULong();
	outGEDHRcd->rtv.tv_usec = CString(inSQLRow[GED_MYSQL_DATA_TB_COL_RTV_USEC_IDX]).ToULong();
 	outGEDHRcd->ftv.tv_sec  = CString(inSQLRow[GED_MYSQL_DATA_TB_COL_FTV_SEC_IDX]).ToULong();
	outGEDHRcd->ftv.tv_usec = CString(inSQLRow[GED_MYSQL_DATA_TB_COL_FTV_USEC_IDX]).ToULong();
	outGEDHRcd->atv.tv_sec  = CString(inSQLRow[GED_MYSQL_DATA_TB_COL_ATV_SEC_IDX]).ToULong();
	outGEDHRcd->atv.tv_usec = CString(inSQLRow[GED_MYSQL_DATA_TB_COL_ATV_USEC_IDX]).ToULong();

	CStrings inAddrs (CString(inSQLRow[GED_MYSQL_DATA_TB_COL_SRC_IDX]).Cut(CString(";")));

	outGEDHRcd->nsrc = inAddrs.GetLength();
	outGEDHRcd->src  = new TGEDRcdSrc [outGEDHRcd->nsrc];

	for (size_t i=0; i<outGEDHRcd->nsrc; i++)
	{
		if (inAddrs[i]->Find(CString("/")))
		{
			outGEDHRcd->src[i].rly = inAddrs[i]->Find(CString("/1"));
			::GetStrAddrToAddrIn (*(inAddrs[i]->Cut(CString("/"))[0]), &outGEDHRcd->src[i].addr);
		}
	}

	if (CString(inSQLRow[GED_MYSQL_DATA_TB_COL_REASON_IDX]) == CString("ttl"))
		outGEDHRcd->queue |= GED_PKT_REQ_BKD_HST_TTL;
	else if (CString(inSQLRow[GED_MYSQL_DATA_TB_COL_REASON_IDX]) == CString("pkt"))
		outGEDHRcd->queue |= GED_PKT_REQ_BKD_HST_PKT;

	CChunk outChunk; for (size_t i=0; i<inGEDPktCfg->fields.GetLength(); i++)
	{
		switch (inGEDPktCfg->fields[i]->type)
		{
			case DATA_STRING  : outChunk << inSQLRow[i+GED_MYSQL_DATA_TB_COL_FIRST_DATA_IDX];		      break;
			case DATA_SINT32  : outChunk << CString(inSQLRow[i+GED_MYSQL_DATA_TB_COL_FIRST_DATA_IDX]).ToLong();   break;
			case DATA_FLOAT64 : outChunk << CString(inSQLRow[i+GED_MYSQL_DATA_TB_COL_FIRST_DATA_IDX]).ToDouble(); break;
		}
	}

	outGEDHRcd->len = outChunk.GetSize();

	if (outGEDHRcd->len)
	{
		outGEDHRcd->data = new char [outGEDHRcd->len];
		::memcpy (outGEDHRcd->data, outChunk.GetChunk(), outGEDHRcd->len);
	}

	return outGEDHRcd;
}

//----------------------------------------------------------------------------------------------------------------------------------------
// sqlrow to sync record
//----------------------------------------------------------------------------------------------------------------------------------------
TGEDSRcd * CGEDBackEndMySQL::MySQLRowToSRcd (MYSQL_ROW &inSQLRow, TGEDPktCfg *inGEDPktCfg)
{
	TGEDSRcd *outGEDSRcd = new TGEDSRcd;
	::bzero (outGEDSRcd, sizeof(TGEDSRcd));

	outGEDSRcd->queue = GED_PKT_REQ_BKD_SYNC;

	::GetStrAddrToAddrIn (inSQLRow[GED_MYSQL_DATA_TB_COL_TGT_IDX], &outGEDSRcd->tgt);

	outGEDSRcd->pkt = new TGEDPktIn;
	::bzero (outGEDSRcd->pkt, sizeof(TGEDPktIn));

	outGEDSRcd->pkt->vrs 	    = GED_VERSION;
	outGEDSRcd->pkt->tv.tv_sec  = CString(inSQLRow[GED_MYSQL_DATA_TB_COL_OTV_SEC_IDX]).ToULong();
	outGEDSRcd->pkt->tv.tv_usec = CString(inSQLRow[GED_MYSQL_DATA_TB_COL_OTV_USEC_IDX]).ToULong();
	outGEDSRcd->pkt->typ 	    = inGEDPktCfg->type;
	outGEDSRcd->pkt->req	    = GED_PKT_REQ_BKD_ACTIVE|GED_PKT_REQ_SRC_RELAY;

	if (CString(inSQLRow[GED_MYSQL_DATA_TB_COL_SRC_IDX]).Find(CString("/")))
		::GetStrAddrToAddrIn (*CString(inSQLRow[GED_MYSQL_DATA_TB_COL_SRC_IDX]).Cut(CString("/"))[0], &outGEDSRcd->pkt->addr);

	if (CString(inSQLRow[GED_MYSQL_DATA_TB_COL_REQ_IDX]) == CString("push"))
		outGEDSRcd->pkt->req |= GED_PKT_REQ_PUSH|GED_PKT_REQ_PUSH_TMSP;
	else if (CString(inSQLRow[GED_MYSQL_DATA_TB_COL_REQ_IDX]) == CString("update")) 
		outGEDSRcd->pkt->req |= GED_PKT_REQ_PUSH|GED_PKT_REQ_PUSH_NOTMSP;
	else if (CString(inSQLRow[GED_MYSQL_DATA_TB_COL_REQ_IDX]) == CString("drop_by_data")) 
		outGEDSRcd->pkt->req |= GED_PKT_REQ_DROP|GED_PKT_REQ_DROP_DATA;
	else if (CString(inSQLRow[GED_MYSQL_DATA_TB_COL_REQ_IDX]) == CString("drop_by_id")) 
		outGEDSRcd->pkt->req |= GED_PKT_REQ_DROP|GED_PKT_REQ_DROP_ID;

	CChunk outChunk; for (size_t i=0; i<inGEDPktCfg->fields.GetLength(); i++)
	{
		switch (inGEDPktCfg->fields[i]->type)
		{
			case DATA_STRING  : outChunk << inSQLRow[i+GED_MYSQL_DATA_TB_COL_FIRST_DATA_IDX];		      break;
			case DATA_SINT32  : outChunk << CString(inSQLRow[i+GED_MYSQL_DATA_TB_COL_FIRST_DATA_IDX]).ToLong();   break;
			case DATA_FLOAT64 : outChunk << CString(inSQLRow[i+GED_MYSQL_DATA_TB_COL_FIRST_DATA_IDX]).ToDouble(); break;
		}
	}	

	outGEDSRcd->pkt->len = outChunk.GetSize();
	if (outGEDSRcd->pkt->len)
	{
		outGEDSRcd->pkt->data = new char [outGEDSRcd->pkt->len];
		::memcpy (outGEDSRcd->pkt->data, outChunk.GetChunk(), outGEDSRcd->pkt->len);
	}

	return outGEDSRcd;
}




