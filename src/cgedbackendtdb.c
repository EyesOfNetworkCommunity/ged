/*****************************************************************************************************************************************
 cgedbackendtdb.c - ged berkeley btree backend -
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

#include "cgedbackendtdb.h"

//----------------------------------------------------------------------------------------------------------------------------------------
// metaclass code resolution and backend export resolution
//----------------------------------------------------------------------------------------------------------------------------------------
RESOLVE_DYNAMIC_METACLASS (CGEDBackEndTDB);
DECLARE_METAMODULE_EXPORT (CGEDBackEndTDB);

//----------------------------------------------------------------------------------------------------------------------------------------
// static resolution
//----------------------------------------------------------------------------------------------------------------------------------------
volatile bool CGEDBackEndTDB::m_fTTL = false;

//----------------------------------------------------------------------------------------------------------------------------------------
// constructor
//----------------------------------------------------------------------------------------------------------------------------------------
CGEDBackEndTDB::CGEDBackEndTDB ()
	       :CGEDBackEndBDB (DB_BTREE, DB_CREATE|DB_INIT_MPOOL|DB_INIT_CDB)
{
}

//----------------------------------------------------------------------------------------------------------------------------------------
// destructor
//----------------------------------------------------------------------------------------------------------------------------------------
CGEDBackEndTDB::~CGEDBackEndTDB ()
{ }


//----------------------------------------------------------------------------------------------------------------------------------------
// tdb initialization
//----------------------------------------------------------------------------------------------------------------------------------------
bool CGEDBackEndTDB::Initialize (const TGEDCfg &inGEDCfg)
{
	if (!CGEDBackEndBDB::Initialize (inGEDCfg)) return false;

	return true;
}

//----------------------------------------------------------------------------------------------------------------------------------------
// backend finalization
//----------------------------------------------------------------------------------------------------------------------------------------
void CGEDBackEndTDB::Finalize ()
{
/*
	if (m_TTLTimer > 0)
	{
		::pthread_cancel (m_TTLTimerTh);
		::pthread_detach (m_TTLTimerTh);
	}
*/
	CGEDBackEndBDB::Finalize();
}

//----------------------------------------------------------------------------------------------------------------------------------------
// btree db open
//----------------------------------------------------------------------------------------------------------------------------------------
bool CGEDBackEndTDB::BDBOpen (const TGEDCfg &inGEDCfg)
{
	int res;

	if (m_DBa == NULL || m_DBh == NULL || m_DBs == NULL) return false;

	if ((res = m_DBa->set_bt_compare (m_DBa, CGEDBackEndTDB::m_TDBCmpA)) != 0)
	{
		CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, 
					   "BERKELEY backend could not set comparaison on active queue database; " +
					   CString(::db_strerror(res)));

		return false;
	}

	if ((res = m_DBh->set_bt_compare (m_DBh, CGEDBackEndTDB::m_TDBCmpH)) != 0)
	{
		CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, 
					   "BERKELEY backend could not set comparaison on history queue database; " +
					   CString(::db_strerror(res)));

		return false;
	}

	if ((res = m_DBs->set_bt_compare (m_DBs, CGEDBackEndTDB::m_TDBCmpS)) != 0)
	{
		CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, 
					   "BERKELEY backend could not set comparaison on sync queue database; " +
					   CString(::db_strerror(res)));

		return false;
	}

	return CGEDBackEndBDB::BDBOpen (inGEDCfg);
}

//----------------------------------------------------------------------------------------------------------------------------------------
// db backend push handler
//----------------------------------------------------------------------------------------------------------------------------------------
bool CGEDBackEndTDB::Push (const CString &inAddr, const int inQueue, const TGEDPktIn *inGEDPktIn)
{
	if (inGEDPktIn == NULL)
	{
		CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, CString("BERKELEY backend push on NULL packet"));
		return false;
	}

	if (inGEDPktIn->data == NULL)
	{
		CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, CString("BERKELEY backend push on NULL content"));
		return false;
	}

	TGEDPktCfg *inGEDPktCfg (::GEDPktInToCfg (const_cast <TGEDPktIn *> (inGEDPktIn), m_GEDCfg.pkts));

	struct in_addr inSrcAddr; if (!::GetStrAddrToAddrIn (inAddr, &inSrcAddr)) return false;

	if (inGEDPktCfg == NULL)
	{
		CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, "BERKELEY backend push on unknown packet type \"" + 
					   CString(inGEDPktIn->typ) + "\"");
		return false;
	}

	int res; switch (inQueue)
	{
		case GED_PKT_REQ_BKD_ACTIVE :
		{
			CChunk inDBKey; if (!GEDPktInToTDBKey (inGEDPktCfg, inGEDPktIn, &inDBKey)) return false;

			DBT outDBKey, ioDBData; ::bzero(&outDBKey,sizeof(DBT)); ::bzero(&ioDBData,sizeof(DBT));

			outDBKey.data = inDBKey.GetChunk();
			outDBKey.size = inDBKey.GetSize();

			if ((res = m_DBa->get (m_DBa, NULL, &outDBKey, &ioDBData, 0)) != 0 && res != DB_NOTFOUND)
			{
				CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, 
				"BERKELEY backend could not get active queue record; " +
				CString(::db_strerror(res)));

				return false;
			}

			if (res == DB_NOTFOUND)
			{
				inDBKey << (UInt32)inGEDPktIn->tv.tv_sec;
				inDBKey << (UInt32)inGEDPktIn->tv.tv_usec;

				outDBKey.data = inDBKey.GetChunk();
				outDBKey.size = inDBKey.GetSize();

				CChunk outDBData; if (!GEDPktInToTDBAData (inGEDPktIn, &inSrcAddr, &outDBData)) return false;

				ioDBData.data = outDBData.GetChunk();
				ioDBData.size = outDBData.GetSize();

				if ((res = m_DBa->put (m_DBa, NULL, &outDBKey, &ioDBData, 0)) != 0)
				{
					CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, 
					"BERKELEY backend could not put active queue record; " +
					CString(::db_strerror(res)));

					return false;
				}

				m_Na++;

				m_aInc=BUFFER_INC_8; for (size_t z=BUFFER_INC_8; z<=BUFFER_INC_65536 && ((m_Na>>m_aInc)>0L); z++) m_aInc = (TBufferInc)z;

				CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_BKD_OPERATION, 
				"BERKELEY backend push active queue on packet type \"" + CString(inGEDPktIn->typ) + "\" [" + inAddr + 
				"] : created occurence");
			}
			else
			{








				





			}
		}
		break;

		case GED_PKT_REQ_BKD_SYNC :
		{
		}
		break;

		case GED_PKT_REQ_BKD_HISTORY :
		{
		}
		break;
	}

	return true;
}

//----------------------------------------------------------------------------------------------------------------------------------------
// db backend drop handler
//----------------------------------------------------------------------------------------------------------------------------------------
bool CGEDBackEndTDB::Drop (const CString &inAddr, const int inQueue, const TGEDPktIn *inGEDPktIn)
{
	return true;
}

//----------------------------------------------------------------------------------------------------------------------------------------
// db backend peek handler
//----------------------------------------------------------------------------------------------------------------------------------------
TBuffer <TGEDRcd *> CGEDBackEndTDB::Peek (const CString &inAddr, const int inQueue, const TGEDPktIn *inGEDPktIn)
{
	TBuffer <TGEDRcd *> outGEDRcds; 

	return outGEDRcds;
}

//----------------------------------------------------------------------------------------------------------------------------------------
// db recover request
//----------------------------------------------------------------------------------------------------------------------------------------
bool CGEDBackEndTDB::Recover (const TBuffer <TGEDRcd *> &inGEDRecords, void (*) (const UInt32, const UInt32))
{
	return true;
}

//----------------------------------------------------------------------------------------------------------------------------------------
// db packet template modification notification handler
//----------------------------------------------------------------------------------------------------------------------------------------
bool CGEDBackEndTDB::NotifyPktCfgChange (const long inType, const TGEDPktCfgChange inGEDPktCfgChange)
{
	return true;
}

//----------------------------------------------------------------------------------------------------------------------------------------
// btree active queue comp func
//----------------------------------------------------------------------------------------------------------------------------------------
int CGEDBackEndTDB::m_TDBCmpA (DB *, const DBT *inDB1, const DBT *inDB2)
{
	int res=0;

	char *dbk1 = new char [inDB1->size]; 
	char *dbk2 = new char [inDB2->size]; 
	
	memcpy (dbk1, inDB1->data, inDB1->size);
	memcpy (dbk2, inDB2->data, inDB2->size);

	if (memcmp (dbk1, dbk2, min(inDB1->size,inDB2->size)) == 0)
	{
		delete [] dbk1;
		delete [] dbk2;

		return 0;
	}

	UInt32  sec1 = *reinterpret_cast <UInt32 *> ((dbk1+inDB1->size) - 2*sizeof(long));
	UInt32 usec1 = *reinterpret_cast <UInt32 *> ((dbk1+inDB1->size) - 1*sizeof(long));

	UInt32  sec2 = *reinterpret_cast <UInt32 *> ((dbk2+inDB2->size) - 2*sizeof(long));
	UInt32 usec2 = *reinterpret_cast <UInt32 *> ((dbk2+inDB2->size) - 1*sizeof(long));

	delete [] dbk1;
	delete [] dbk2;

	if (sec1 == sec2)
	{
		if (usec1 == usec2) 
		{
			res =  0;
		}
		else if (usec1 <  usec2) 
		{
			res = -1;
		}
		else 
		{
			res = 1;
		}
	}
	else if (sec1 < sec2)
	{
		res = -1;
	}
	else
	{
		res =  1;
	}

//	CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_BKD_OPERATION, "cmp res = \"" + CString((SInt32)res) + "\"");

	return res;
}

//----------------------------------------------------------------------------------------------------------------------------------------
// btree history queue comp func
//----------------------------------------------------------------------------------------------------------------------------------------
int CGEDBackEndTDB::m_TDBCmpH (DB *, const DBT *inDB1, const DBT *inDB2)
{
	return 0;
}

//----------------------------------------------------------------------------------------------------------------------------------------
// btree sync queue comp func
//----------------------------------------------------------------------------------------------------------------------------------------
int CGEDBackEndTDB::m_TDBCmpS (DB *, const DBT *inDB1, const DBT *inDB2)
{
	return 0;
}

//----------------------------------------------------------------------------------------------------------------------------------------
// TTL thread callback
//----------------------------------------------------------------------------------------------------------------------------------------
void * CGEDBackEndTDB::m_TTLTimerCB (void *inParam)
{
	CGEDBackEndTDB *inGEDBackEndTDB = reinterpret_cast <CGEDBackEndTDB *> (inParam);

	return NULL;
}

//----------------------------------------------------------------------------------------------------------------------------------------
// ged pkt to hash db key
//----------------------------------------------------------------------------------------------------------------------------------------
bool CGEDBackEndTDB::GEDPktInToTDBKey (const TGEDPktCfg *inGEDPktCfg, const TGEDPktIn *inGEDPktIn, CChunk *outDBKey) const
{
	if (inGEDPktIn == NULL)
	{
		CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, 
					   CString("BERKELEY backend cannot convert key of NULL packet"));
		return false;
	}

	if (inGEDPktCfg == NULL)
	{
		CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, 
					   "BERKELEY backend cannot convert key of unknown packet type \"" + 
					   CString(inGEDPktIn->typ) + "\"");
		return false;
	}

	TGEDPktCfg gedPktCfg (*inGEDPktCfg);

	if (gedPktCfg.keyidc.GetLength() > 0)
	{
		gedPktCfg.keyidc.Sort(); 
	}
	else
	{
		CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_BKD_OPERATION, 
		"BERKELY backend has no key indices specification on packet type \"" + CString((long)inGEDPktCfg->type) + "\"");

		return false;
	}

	*outDBKey << (long)inGEDPktIn->typ;

	TBuffer <TData> inTData (::GEDPktCfgToTData(const_cast<TGEDPktCfg*>(inGEDPktCfg)));

	CChunk inChunk (const_cast<TGEDPktIn*>(inGEDPktIn)->data, const_cast<TGEDPktIn*>(inGEDPktIn)->len, inTData, false);

	for (size_t i=0, j=0; i<gedPktCfg.keyidc.GetLength(); i++)
	{
		for (; j<*gedPktCfg.keyidc[i]; j++) { inChunk++; }

		switch (gedPktCfg.fields[*gedPktCfg.keyidc[i]]->type)
		{
			case DATA_SINT32 :
			{
				long val=0L; inChunk >> val; *outDBKey << val;
			}
			break;

			case DATA_STRING :
			{
				char *val = NULL; inChunk >> val; *outDBKey << val;
				if (val != NULL) delete [] val;
			}
			break;

			case DATA_FLOAT64 :
			{
				double val=0.; inChunk >> val; *outDBKey << val;
			}
			break;
		}

		j++;
	}

	return true;
}

//----------------------------------------------------------------------------------------------------------------------------------------
// ged pkt to btree db data
//----------------------------------------------------------------------------------------------------------------------------------------
bool CGEDBackEndTDB::GEDPktInToTDBAData (const TGEDPktIn *inGEDPktIn, const struct in_addr *inSrcAddr, CChunk *outDBData) const
{
	if (inGEDPktIn == NULL)
	{
		CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, 
					   CString("BERKELEY backend cannot convery data of NULL packet"));
		return false;
	}

	if (inGEDPktIn->data == NULL)
	{
		CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION,
					   CString("BERKELEY backend cannot convert data of NULL content"));
		return false;
	}

	if (inSrcAddr == NULL || outDBData == NULL) return false;

	struct timeval tv; ::gettimeofday (&tv, NULL);

	*outDBData << (long)GED_PKT_REQ_BKD_ACTIVE;
	*outDBData << (long)inGEDPktIn->typ;
	*outDBData << (unsigned long)1L;
	*outDBData << (unsigned long)inGEDPktIn->tv.tv_sec;
	*outDBData << (unsigned long)inGEDPktIn->tv.tv_usec;
	*outDBData << (unsigned long)inGEDPktIn->tv.tv_sec;
	*outDBData << (unsigned long)inGEDPktIn->tv.tv_usec;
	*outDBData << (unsigned long)tv.tv_sec;
	*outDBData << (unsigned long)tv.tv_usec;
	*outDBData << (unsigned long)inGEDPktIn->tv.tv_sec;
	*outDBData << (unsigned long)inGEDPktIn->tv.tv_usec;
	*outDBData << (unsigned long)1L;
	*outDBData << (unsigned long)inGEDPktIn->len;
	*outDBData << (long)inGEDPktIn->rsv[0];
	*outDBData << (long)inGEDPktIn->rsv[1];

	*outDBData << (long)(inGEDPktIn->req&GED_PKT_REQ_SRC_RELAY);
	*outDBData << (long)inSrcAddr->s_addr;

	outDBData->WritePVoid (inGEDPktIn->data, inGEDPktIn->len);

	return true;
}





