/*****************************************************************************************************************************************
 cgedbackendbdb.c - ged berkeley environment backend abstract -
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

#include "cgedbackendbdb.h"

//----------------------------------------------------------------------------------------------------------------------------------------
// metaclass code resolution
//----------------------------------------------------------------------------------------------------------------------------------------
RESOLVE_GENERIC_METACLASS (CGEDBackEndBDB);

//----------------------------------------------------------------------------------------------------------------------------------------
// constructor
//----------------------------------------------------------------------------------------------------------------------------------------
CGEDBackEndBDB::CGEDBackEndBDB (const DBTYPE inDBType, const UInt32 inDBEnvFlags)
	       :m_DBType       (inDBType),
	        m_DBEnvFlags   (inDBEnvFlags),
		m_DBENV	       (NULL),
		m_DBc	       (NULL),
		m_DBa	       (NULL),
		m_DBh	       (NULL),
		m_DBs	       (NULL),
		m_Na	       (0L),
		m_Nh	       (0L),
		m_Ns	       (0L),
		m_Nr	       (0L)
{
	if (::db_env_create (&m_DBENV, 0) != 0)
		throw new CException (CString("could not create berkeley environment"));
}

//----------------------------------------------------------------------------------------------------------------------------------------
// destructor
//----------------------------------------------------------------------------------------------------------------------------------------
CGEDBackEndBDB::~CGEDBackEndBDB ()
{ }

//----------------------------------------------------------------------------------------------------------------------------------------
// abstract berkeley backend initialization
//----------------------------------------------------------------------------------------------------------------------------------------
bool CGEDBackEndBDB::Initialize (const TGEDCfg &inGEDCfg)
{
	int res;

	CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_BKD_OPERATION, "BERKELEY backend loading " + 
				   CString((m_DBType == DB_HASH) ? "HASH" : "BTREE")  + " [" + 
				   CString((long)DB_VERSION_MAJOR) + "." + CString((long)DB_VERSION_MINOR) + "." + 
				   CString((long)DB_VERSION_PATCH) + "]");

	if (inGEDCfg.bkdcfg.Contains(GEDCfgBdbCacheSize) && const_cast<TGEDCfg&>(inGEDCfg).bkdcfg[GEDCfgBdbCacheSize].GetLength()>2) 
	{
		if ((res=m_DBENV->set_cachesize (m_DBENV, const_cast<TGEDCfg&>(inGEDCfg).bkdcfg[GEDCfgBdbCacheSize][0]->ToULong(), 
				   		     	  const_cast<TGEDCfg&>(inGEDCfg).bkdcfg[GEDCfgBdbCacheSize][1]->ToULong(), 
						  	  const_cast<TGEDCfg&>(inGEDCfg).bkdcfg[GEDCfgBdbCacheSize][2]->ToLong())) != 0)
		{
			CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, 
						   "BERKELEY backend could not set " + GEDCfgBdbCacheSize + "; " +
						   ::db_strerror(res));
			return false;
		}
	}

	if (inGEDCfg.bkdcfg.Contains(GEDCfgBdbDataDir) && const_cast<TGEDCfg&>(inGEDCfg).bkdcfg[GEDCfgBdbDataDir].GetLength()>0) 
	{
		if ((res=m_DBENV->set_data_dir (m_DBENV, const_cast<TGEDCfg&>(inGEDCfg).bkdcfg[GEDCfgBdbDataDir][0]->Get())) != 0)
		{
			CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, 
						   "BERKELEY backend could not set " + GEDCfgBdbDataDir + " to \"" +
						   *const_cast<TGEDCfg&>(inGEDCfg).bkdcfg[GEDCfgBdbDataDir][0] + "\"; " +
						   ::db_strerror(res));
			return false;
		}
	}

	if (inGEDCfg.bkdcfg.Contains(GEDCfgBdbLgDir) && const_cast<TGEDCfg&>(inGEDCfg).bkdcfg[GEDCfgBdbLgDir].GetLength()>0) 
	{
		if ((res=m_DBENV->set_lg_dir (m_DBENV, const_cast<TGEDCfg&>(inGEDCfg).bkdcfg[GEDCfgBdbLgDir][0]->Get())) != 0)
		{
			CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, 
						   "BERKELEY backend could not set " + GEDCfgBdbLgDir + " to \"" +
						   *const_cast<TGEDCfg&>(inGEDCfg).bkdcfg[GEDCfgBdbLgDir][0] + "\"; "  +
						   ::db_strerror(res));
			return false;
		}
	}

	if (inGEDCfg.bkdcfg.Contains(GEDCfgBdbLgMax) && const_cast<TGEDCfg&>(inGEDCfg).bkdcfg[GEDCfgBdbLgMax].GetLength()>0) 
	{
		if ((res=m_DBENV->set_lg_max (m_DBENV, const_cast<TGEDCfg&>(inGEDCfg).bkdcfg[GEDCfgBdbLgMax][0]->ToULong())) != 0)
		{
			CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, 
						   "BERKELEY backend could not set " + GEDCfgBdbLgMax + " to \"" +
						   *const_cast<TGEDCfg&>(inGEDCfg).bkdcfg[GEDCfgBdbLgMax][0] + "\"; " +
						   ::db_strerror(res));
			return false;
		}
	}

	if (inGEDCfg.bkdcfg.Contains(GEDCfgBdbLgRegionMax) && const_cast<TGEDCfg&>(inGEDCfg).bkdcfg[GEDCfgBdbLgRegionMax].GetLength()>0) 
	{
		if ((res=m_DBENV->set_lg_regionmax (m_DBENV, const_cast<TGEDCfg&>(inGEDCfg).bkdcfg[GEDCfgBdbLgRegionMax][0]->ToULong())) != 0)
		{
			CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, 
						   "BERKELEY backend could not set " + GEDCfgBdbLgRegionMax + " to \"" +
						   *const_cast<TGEDCfg&>(inGEDCfg).bkdcfg[GEDCfgBdbLgRegionMax][0] + "\"; " +
						   ::db_strerror(res));
			return false;
		}
	}

	if (inGEDCfg.bkdcfg.Contains(GEDCfgBdbLgBSize) && const_cast<TGEDCfg&>(inGEDCfg).bkdcfg[GEDCfgBdbLgBSize].GetLength()>0) 
	{
		if ((res=m_DBENV->set_lg_bsize (m_DBENV, const_cast<TGEDCfg&>(inGEDCfg).bkdcfg[GEDCfgBdbLgBSize][0]->ToULong())) != 0)
		{
			CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, 
						   "BERKELEY backend could not set " + GEDCfgBdbLgBSize + " to \"" +
						   *const_cast<TGEDCfg&>(inGEDCfg).bkdcfg[GEDCfgBdbLgBSize][0] + "\"; " +
						   ::db_strerror(res));
			return false;
		}
	}

	if (inGEDCfg.bkdcfg.Contains(GEDCfgBdbTmpDir) && const_cast<TGEDCfg&>(inGEDCfg).bkdcfg[GEDCfgBdbTmpDir].GetLength()>0) 
	{
		if ((res=m_DBENV->set_tmp_dir (m_DBENV, const_cast<TGEDCfg&>(inGEDCfg).bkdcfg[GEDCfgBdbTmpDir][0]->Get())) != 0)
		{
			CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, 
						   "BERKELEY backend could not set " + GEDCfgBdbTmpDir + " to \"" +
						   *const_cast<TGEDCfg&>(inGEDCfg).bkdcfg[GEDCfgBdbTmpDir][0] + "\"; " +
						   ::db_strerror(res));
			return false;
		}
	}

	if (inGEDCfg.bkdcfg.Contains(GEDCfgBdbHome) && const_cast<TGEDCfg&>(inGEDCfg).bkdcfg[GEDCfgBdbHome].GetLength()>0) 
	{
		if ((res=m_DBENV->open (m_DBENV, const_cast<TGEDCfg&>(inGEDCfg).bkdcfg[GEDCfgBdbHome][0]->Get(), m_DBEnvFlags, 0644)) != 0)
		{
			CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, 
						   "BERKELEY backend could not open " + GEDCfgBdbHome + " \"" + 
						   *const_cast<TGEDCfg&>(inGEDCfg).bkdcfg[GEDCfgBdbHome][0] + "\"; " +
						   ::db_strerror(res));
			return false;
		}
	}
	else
	{
		CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, "BERKELEY backend missing \"" + 
					   GEDCfgBdbHome + "\" parameter specification");
		return false;
	}

	if (!BDBCreate (inGEDCfg) || !BDBOpen (inGEDCfg)) return false;
	
	if (!CGEDBackEnd::Initialize (inGEDCfg)) return false;	

	return true;
}

//----------------------------------------------------------------------------------------------------------------------------------------
// backend finalization
//----------------------------------------------------------------------------------------------------------------------------------------
void CGEDBackEndBDB::Finalize ()
{
	CGEDBackEnd::Finalize();

	if (BDBClose())
		CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO,  GED_SYSLOG_LEV_BKD_OPERATION, "BERKELEY backend databases closed");
	else
		CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, "BERKELEY backend could not close databases");

	if (m_DBENV != NULL) m_DBENV->close (m_DBENV, 0);
}

//----------------------------------------------------------------------------------------------------------------------------------------
// backend stat
//----------------------------------------------------------------------------------------------------------------------------------------
TGEDStatRcd * CGEDBackEndBDB::Stat () const
{
	TGEDStatRcd *outGEDStatRcd = new TGEDStatRcd; ::bzero(outGEDStatRcd,sizeof(TGEDStatRcd));

	outGEDStatRcd->queue = GED_PKT_REQ_BKD_STAT;
	outGEDStatRcd->id    = classtag_cast(this);
	outGEDStatRcd->vrs   = DB_VERSION_MAJOR*10000 + DB_VERSION_MINOR*100 + DB_VERSION_PATCH;
	outGEDStatRcd->nta   = m_Na;
	outGEDStatRcd->nts   = m_Ns;
	outGEDStatRcd->nth   = m_Nh;
	outGEDStatRcd->ntr   = m_Nr;

	return outGEDStatRcd;
}

//----------------------------------------------------------------------------------------------------------------------------------------
// read cache config
//----------------------------------------------------------------------------------------------------------------------------------------
bool CGEDBackEndBDB::ReadCfgCache (TKeyBuffer <UInt32, CString> &outPktHash, UInt32 &outVersion)
{
	if (m_DBc == NULL) return false;

	DBT inDBKey, inDBData; ::bzero(&inDBKey,sizeof(DBT)); ::bzero(&inDBData,sizeof(DBT));

	int res; DBC *inDBCursor; if ((res = m_DBc->cursor (m_DBc, NULL, &inDBCursor, 0)) != 0)
	{
		CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION,
                                "BERKELEY backend could not get config cache cursor");
                return false;
	}

	while ((res = inDBCursor->c_get (inDBCursor, &inDBKey, &inDBData, DB_NEXT)) == 0)
	{
		if (*(UInt32*)inDBKey.data == UINT32MAX) 
			outVersion = *((UInt32*)inDBData.data);
		else
			outPktHash[*(UInt32*)inDBKey.data] = CString((char*)inDBData.data);
	}

	inDBCursor->c_close (inDBCursor);

	return true;
}

//----------------------------------------------------------------------------------------------------------------------------------------
// write cache config
//----------------------------------------------------------------------------------------------------------------------------------------
bool CGEDBackEndBDB::WriteCfgCache ()
{
	if (m_DBc == NULL) return false;

	DBT outDBKey, outDBData; 

	UInt32 vrsn = UINT32MAX, vv=GED_VERSION;

	::bzero(&outDBKey,sizeof(DBT)); ::bzero(&outDBData,sizeof(DBT));
	outDBKey.data  = &vrsn;
	outDBKey.size  = sizeof(UInt32);
	outDBData.data = &vv;
	outDBData.size = sizeof(UInt32);

	int res; if ((res = m_DBc->put (m_DBc, NULL, &outDBKey, &outDBData, 0)) != 0)
	{
		CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, 
		"BERKELEY backend could not put config cache record; " + CString(::db_strerror(res)));

		return false;
	}

	UInt32 zero=0L; CString h (::HashGEDPktCfg(m_GEDCfg.pkts));

	::bzero(&outDBKey,sizeof(DBT)); ::bzero(&outDBData,sizeof(DBT));
	outDBKey.data  = &zero;
	outDBKey.size  = sizeof(UInt32);
	outDBData.data = h.Get();
	outDBData.size = h.GetLength()+1;

	if ((res = m_DBc->put (m_DBc, NULL, &outDBKey, &outDBData, 0)) != 0)
	{
		CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, 
		"BERKELEY backend could not put config cache record; " + CString(::db_strerror(res)));

		return false;
	}

	for (size_t i=0; i<m_GEDCfg.pkts.GetLength(); i++)
	{
		h = ::HashGEDPktCfg(m_GEDCfg.pkts[i]);

		::bzero(&outDBKey,sizeof(DBT)); ::bzero(&outDBData,sizeof(DBT));
		outDBKey.data  = &(m_GEDCfg.pkts[i]->type);
		outDBKey.size  = sizeof(UInt32);
		outDBData.data = h.Get();
		outDBData.size = h.GetLength()+1;

		if ((res = m_DBc->put (m_DBc, NULL, &outDBKey, &outDBData, 0)) != 0)
		{
			CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, 
			"BERKELEY backend could not put config cache record; " + CString(::db_strerror(res)));

			return false;
		}
	}

	return true;
}

//----------------------------------------------------------------------------------------------------------------------------------------
// db handles creation
//----------------------------------------------------------------------------------------------------------------------------------------
bool CGEDBackEndBDB::BDBCreate (const TGEDCfg &)
{
	int res; if ((res = ::db_create (&m_DBc, m_DBENV, 0)) != 0)
	{
		CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, 
					   "BERKELEY backend could not create config cache database; " +
					   CString(::db_strerror(res)));
		return false;
	}

	if ((res = ::db_create (&m_DBa, m_DBENV, 0)) != 0)
	{
		CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, 
					   "BERKELEY backend could not create active queue database; " +
					   CString(::db_strerror(res)));
		return false;
	}

	if ((res = ::db_create (&m_DBh, m_DBENV, 0)) != 0)
	{
		CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, 
					   "BERKELEY backend could not create history queue database; " +
					   CString(::db_strerror(res)));
		return false;
	}

	if ((res = ::db_create (&m_DBs, m_DBENV, 0)) != 0)
	{
		CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, 
					   "BERKELEY backend could not create sync queue database; " +
					   CString(::db_strerror(res)));
		return false;
	}

	return true;
}

//----------------------------------------------------------------------------------------------------------------------------------------
// db handles open
//----------------------------------------------------------------------------------------------------------------------------------------
bool CGEDBackEndBDB::BDBOpen (const TGEDCfg &)
{
	int res; 

	if (m_DBa == NULL || m_DBh == NULL || m_DBs == NULL || m_DBc == NULL) return false;

	if ((res = m_DBc->open (m_DBc, NULL, GED_CFG_DFT.Get(), NULL, m_DBType, DB_CREATE/*|DB_AUTO_COMMIT*/, 0644)) != 0)
	{
		CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, 
					   "BERKELEY backend could not open config cache database; " +
					   CString(::db_strerror(res)));
		return false;
	}

	if ((res = m_DBa->open (m_DBa, NULL, GED_ADB_DFT.Get(), NULL, m_DBType, DB_CREATE/*|DB_AUTO_COMMIT*/, 0644)) != 0)
	{
		CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, 
					   "BERKELEY backend could not open active queue database; " +
					   CString(::db_strerror(res)));
		return false;
	}

	if ((res = m_DBh->open (m_DBh, NULL, GED_HDB_DFT.Get(), NULL, m_DBType, DB_CREATE/*|DB_AUTO_COMMIT*/, 0644)) != 0)
	{
		CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, 
					   "BERKELEY backend could not open history queue database; " +
					   CString(::db_strerror(res)));
		return false;
	}

	if ((res = m_DBs->open (m_DBs, NULL, GED_SDB_DFT.Get(), NULL, m_DBType, DB_CREATE/*|DB_AUTO_COMMIT*/, 0644)) != 0)
	{
		CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, 
					   "BERKELEY backend could not open sync queue database; " +
					   CString(::db_strerror(res)));
		return false;
	}

	return true;
}

//----------------------------------------------------------------------------------------------------------------------------------------
// db handles close
//----------------------------------------------------------------------------------------------------------------------------------------
bool CGEDBackEndBDB::BDBClose ()
{
	if (m_DBc != NULL) m_DBc->close (m_DBc, 0); m_DBc = NULL;
	if (m_DBa != NULL) m_DBa->close (m_DBa, 0); m_DBa = NULL;
	if (m_DBh != NULL) m_DBh->close (m_DBh, 0); m_DBh = NULL;
	if (m_DBs != NULL) m_DBs->close (m_DBs, 0); m_DBs = NULL;

	return true;
}


