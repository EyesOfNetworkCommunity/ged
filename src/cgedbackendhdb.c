/*****************************************************************************************************************************************
 cgedbackendhdb.c - ged berkeley hash backend -
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

#include "cgedbackendhdb.h"

//----------------------------------------------------------------------------------------------------------------------------------------
// metaclass code resolution and backend export resolution
//----------------------------------------------------------------------------------------------------------------------------------------
RESOLVE_DYNAMIC_METACLASS (CGEDBackEndHDB);
DECLARE_METAMODULE_EXPORT (CGEDBackEndHDB);

//----------------------------------------------------------------------------------------------------------------------------------------
// static resolution
//----------------------------------------------------------------------------------------------------------------------------------------
volatile bool CGEDBackEndHDB::m_fTTL = false;

//----------------------------------------------------------------------------------------------------------------------------------------
// constructor
//----------------------------------------------------------------------------------------------------------------------------------------
CGEDBackEndHDB::CGEDBackEndHDB ()
	       :CGEDBackEndBDB (DB_HASH, DB_CREATE|DB_INIT_MPOOL|DB_INIT_CDB),
		m_HID	       (0L)
{ }

//----------------------------------------------------------------------------------------------------------------------------------------
// destructor
//----------------------------------------------------------------------------------------------------------------------------------------
CGEDBackEndHDB::~CGEDBackEndHDB ()
{ }

//----------------------------------------------------------------------------------------------------------------------------------------
// hash berkeley backend initialization
//----------------------------------------------------------------------------------------------------------------------------------------
bool CGEDBackEndHDB::Initialize (const TGEDCfg &inGEDCfg)
{
	if (!CGEDBackEndBDB::Initialize (inGEDCfg)) return false;

	int res; DBC *inDBCursor; if ((res = m_DBh->cursor (m_DBh, NULL, &inDBCursor, 0)) != 0)
	{
		CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION,
                                "BERKELEY backend could not get history queue cursor");
                return false;
	}

	DBT inDBKey, inDBData; ::bzero(&inDBKey,sizeof(DBT)); ::bzero(&inDBData,sizeof(DBT));

	while ((res = inDBCursor->c_get (inDBCursor, &inDBKey, &inDBData, DB_NEXT)) == 0)
	{
		TGEDHRcd *inGEDHRcd = GEDHDBDataToHRcd (&inDBData);

		if (inGEDHRcd->hid >= m_HID) m_HID = inGEDHRcd->hid+1;

		::DeleteGEDRcd ((TGEDRcd*&)inGEDHRcd);
	}

	inDBCursor->c_close (inDBCursor);

	if (m_TTLTimer > 0)
	{
		if (::pthread_create (&m_TTLTimerTh, NULL, CGEDBackEndHDB::m_TTLTimerCB, this) != 0)
		{
			CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, 
				CString("BERKELEY backend could not launch TTL thread"));

			return false;
		}

		while (!CGEDBackEndHDB::m_fTTL) ::usleep (250);
	}
	else
		CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_BKD_OPERATION, 
			CString("BERKELEY backend TTL handling disabled"));

	if ((res = m_DBa->cursor (m_DBa, NULL, &inDBCursor, 0)) != 0)
	{
		CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION,
                                "BERKELEY backend could not get active queue cursor");
                return false;
	}
	while ((res = inDBCursor->c_get (inDBCursor, &inDBKey, &inDBData, DB_NEXT)) == 0) m_Na++;
	inDBCursor->c_close (inDBCursor);

	if ((res = m_DBh->cursor (m_DBh, NULL, &inDBCursor, 0)) != 0)
	{
		CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION,
                                "BERKELEY backend could not get history queue cursor");
                return false;
	}
	while ((res = inDBCursor->c_get (inDBCursor, &inDBKey, &inDBData, DB_NEXT)) == 0) m_Nh++;
	inDBCursor->c_close (inDBCursor);

	if ((res = m_DBs->cursor (m_DBs, NULL, &inDBCursor, 0)) != 0)
	{
		CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION,
                                "BERKELEY backend could not get sync queue cursor");
                return false;
	}
	while ((res = inDBCursor->c_get (inDBCursor, &inDBKey, &inDBData, DB_NEXT)) == 0) m_Ns++;
	inDBCursor->c_close (inDBCursor);

	for (size_t z=m_aInc; z<=BUFFER_INC_65536 && ((m_Na>>m_aInc)>0L); z++) m_aInc = (TBufferInc)z;
	for (size_t z=m_hInc; z<=BUFFER_INC_65536 && ((m_Nh>>m_hInc)>0L); z++) m_hInc = (TBufferInc)z;
	for (size_t z=m_sInc; z<=BUFFER_INC_65536 && ((m_Ns>>m_sInc)>0L); z++) m_sInc = (TBufferInc)z;

	CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_BKD_OPERATION,
		"BERKELEY backend handles " + CString(m_Na) + " active records, " + CString(m_Nh) + " history records, " + CString(m_Ns) + " sync records");

	return true;
}

//----------------------------------------------------------------------------------------------------------------------------------------
// backend finalization
//----------------------------------------------------------------------------------------------------------------------------------------
void CGEDBackEndHDB::Finalize ()
{
	if (m_TTLTimer > 0)
	{
		::pthread_cancel (m_TTLTimerTh);
		::pthread_detach (m_TTLTimerTh);
	}

	CGEDBackEndBDB::Finalize();
}

//----------------------------------------------------------------------------------------------------------------------------------------
// db backend push handler
//----------------------------------------------------------------------------------------------------------------------------------------
bool CGEDBackEndHDB::Push (const CString &inAddr, const int inQueue, const TGEDPktIn *inGEDPktIn)
{
	m_Nr = 0L;

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
			CChunk inDBKey; if (!GEDPktInToHDBKey (inGEDPktCfg, inGEDPktIn, &inDBKey)) return false;

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
				CChunk outDBData; if (!GEDPktInToHDBAData (inGEDPktIn, &inSrcAddr, &outDBData)) return false;

				ioDBData.data = outDBData.GetChunk();
				ioDBData.size = outDBData.GetSize();

				if ((res = m_DBa->put (m_DBa, NULL, &outDBKey, &ioDBData, /*DB_AUTO_COMMIT*/0)) != 0)
				{
					CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, 
					"BERKELEY backend could not put active queue record; " +
					CString(::db_strerror(res)));

					return false;
				}

				m_Na++;

				m_Nr = 1L;

				m_aInc=BUFFER_INC_8; for (size_t z=BUFFER_INC_8; z<=BUFFER_INC_65536 && ((m_Na>>m_aInc)>0L); z++) m_aInc = (TBufferInc)z;

				CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_BKD_OPERATION, 
				"BERKELEY backend push active queue on packet type \"" + CString(inGEDPktIn->typ) + "\" [" + inAddr + 
				"] : created occurence");
			}
			else
			{
				TGEDARcd *inGEDARec (GEDHDBDataToARcd (&ioDBData));

				bool found=false; for (size_t j=0; j<inGEDARec->nsrc && !found; j++)
				{
					found = inGEDARec->src[j].addr.s_addr == inSrcAddr.s_addr;
					if (found && !inGEDARec->src[j].rly)
						inGEDARec->src[j].rly = (inGEDPktIn->req&GED_PKT_REQ_SRC_RELAY);
				}

				if (!found)
				{
					TGEDRcdSrc *tmpGEDRcdSrc = inGEDARec->src;
					inGEDARec->src = new TGEDRcdSrc [++inGEDARec->nsrc];
					::memcpy (inGEDARec->src, tmpGEDRcdSrc, sizeof(TGEDRcdSrc)*(inGEDARec->nsrc-1));
					inGEDARec->src[inGEDARec->nsrc-1].addr.s_addr = inSrcAddr.s_addr;
					inGEDARec->src[inGEDARec->nsrc-1].rly = (inGEDPktIn->req&GED_PKT_REQ_SRC_RELAY);
					delete [] tmpGEDRcdSrc;
				}

				if ((inGEDPktIn->req & GED_PKT_REQ_PUSH_MASK) == GED_PKT_REQ_PUSH_TMSP)
				{
					if (inGEDARec->ltv.tv_sec > inGEDPktIn->tv.tv_sec)
					{
						::DeleteGEDRcd ((TGEDRcd*&)inGEDARec);
						return true;
					}

					if (inGEDARec->ltv.tv_sec == inGEDPktIn->tv.tv_sec && inGEDARec->ltv.tv_usec > inGEDPktIn->tv.tv_usec)
					{
						::DeleteGEDRcd ((TGEDRcd*&)inGEDARec);
						return true;
					}

					if (inGEDARec->ltv.tv_sec == inGEDPktIn->tv.tv_sec && inGEDARec->ltv.tv_usec == inGEDPktIn->tv.tv_usec)
					{
						CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_BKD_OPERATION, 
									   "BERKELEY backend push on DUP packet type \"" + 
									   CString(inGEDPktIn->typ) + "\"");

						::DeleteGEDRcd ((TGEDRcd*&)inGEDARec);

						return true;
					}

					inGEDARec->occ++;

					::gettimeofday (&inGEDARec->rtv, NULL);
					inGEDARec->ltv = inGEDPktIn->tv;

					CChunk inPkData (inGEDPktIn->data, inGEDPktIn->len, GEDPktCfgToTData(inGEDPktCfg), false);
					CChunk inDbData (inGEDARec ->data, inGEDARec ->len, GEDPktCfgToTData(inGEDPktCfg), false);
					CChunk outDbData;

					for (size_t i=0; i<inGEDPktCfg->fields.GetLength(); i++)
					{
						if (!inGEDPktCfg->fields[i]->meta)
						{
							switch (inPkData.NextDataIs())
							{
								case DATA_SINT32 :
								{
									SInt32 inSint32=0L; 
									inPkData >> inSint32;
									inDbData++;
									outDbData << inSint32;
								}
								break;
								case DATA_STRING :
								{
									SInt8 *inSint8=NULL; 
									inPkData >> inSint8; 
									inDbData++;
									if (inSint8 != NULL)
										outDbData << inSint8;
									else
										outDbData << "";
									if (inSint8 != NULL) delete [] inSint8;
								}
								break;
								case DATA_FLOAT64 :
								{
									Float64 inFloat64=0.;
									inPkData >> inFloat64;
									inDbData++;
									outDbData << inFloat64;
								}
								break;
							}
						}
						else
						{
							switch (inPkData.NextDataIs())
							{
								case DATA_SINT32 :
								{
									SInt32 inSint32=0L; 
									inDbData >> inSint32;
									inPkData++;
									outDbData << inSint32;
								}
								break;
								case DATA_STRING :
								{
									SInt8 *inSint8=NULL; 
									inDbData >> inSint8; 
									inPkData++;
									if (inSint8 != NULL)
										outDbData << inSint8;
									else
										outDbData << "";
									if (inSint8 != NULL) delete [] inSint8;
								}
								break;
								case DATA_FLOAT64 :
								{
									Float64 inFloat64=0.;
									inDbData >> inFloat64;
									inPkData++;
									outDbData << inFloat64;
								}
								break;
							}
						}
					}

					if (inGEDARec->data != NULL) delete [] reinterpret_cast <char *> (inGEDARec->data); 
					inGEDARec->data=NULL; inGEDARec->len=0L;

					if (outDbData.GetChunk() != NULL && outDbData.GetSize() > 0)
					{
						inGEDARec->len  = outDbData.GetSize();
						inGEDARec->data = new char [inGEDARec->len];
						::memcpy (inGEDARec->data, outDbData.GetChunk(), inGEDARec->len);
					}

					if ((res = m_DBa->del (m_DBa, NULL, &outDBKey, /*DB_AUTO_COMMIT*/0)) != 0)
					{
						CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, 
						"BERKELEY backend could not del active queue record; " +
						CString(::db_strerror(res)));

						return false;
					}

					CChunk outDBData; if (!GEDARcdToHDBAData (inGEDARec, &outDBData))
					{
						::DeleteGEDRcd ((TGEDRcd*&)inGEDARec);
						return false;
					}

					::bzero(&ioDBData,sizeof(DBT));

					ioDBData.data = outDBData.GetChunk();
					ioDBData.size = outDBData.GetSize();

					if ((res = m_DBa->put (m_DBa, NULL, &outDBKey, &ioDBData, /*DB_AUTO_COMMIT*/0)) != 0)
					{
						CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, 
						"BERKELEY backend could not put active queue record; " +
						CString(::db_strerror(res)));

						return false;
					}

					::DeleteGEDRcd ((TGEDRcd*&)inGEDARec);

					CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_BKD_OPERATION, 
					"BERKELEY backend push active queue on packet type \"" + CString(inGEDPktIn->typ) + "\" [" + inAddr + 
					"] : updated occurences number");

					m_Nr = 1L;
				}
				else
				{
					if (inGEDARec->mtv.tv_sec > inGEDPktIn->tv.tv_sec)
					{
						::DeleteGEDRcd ((TGEDRcd*&)inGEDARec);
						return true;
					}

					if (inGEDARec->mtv.tv_sec == inGEDPktIn->tv.tv_sec && inGEDARec->mtv.tv_usec > inGEDPktIn->tv.tv_usec)
					{
						::DeleteGEDRcd ((TGEDRcd*&)inGEDARec);
						return true;
					}

					if (inGEDARec->mtv.tv_sec == inGEDPktIn->tv.tv_sec && inGEDARec->mtv.tv_usec == inGEDPktIn->tv.tv_usec)
					{
						CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_BKD_OPERATION, 
									   "BERKELEY backend update on DUP packet type \"" + 
									   CString(inGEDPktIn->typ) + "\"");

						::DeleteGEDRcd ((TGEDRcd*&)inGEDARec);

						return true;
					}

					inGEDARec->mtv = inGEDPktIn->tv;

					CChunk inPkData (inGEDPktIn->data, inGEDPktIn->len, GEDPktCfgToTData(inGEDPktCfg), false);
					CChunk inDbData (inGEDARec ->data, inGEDARec ->len, GEDPktCfgToTData(inGEDPktCfg), false);
					CChunk outDbData;

					for (size_t i=0; i<inGEDPktCfg->fields.GetLength(); i++)
					{
						if (inGEDPktCfg->fields[i]->meta)
						{
							switch (inPkData.NextDataIs())
							{
								case DATA_SINT32 :
								{
									SInt32 inSint32=0L; 
									inPkData >> inSint32;
									inDbData++;
									outDbData << inSint32;
								}
								break;
								case DATA_STRING :
								{
									SInt8 *inSint8=NULL; 
									inPkData >> inSint8; 
									inDbData++;
									if (inSint8 != NULL)
										outDbData << inSint8;
									else
										outDbData << "";
									if (inSint8 != NULL) delete [] inSint8;
								}
								break;
								case DATA_FLOAT64 :
								{
									Float64 inFloat64=0.;
									inPkData >> inFloat64;
									inDbData++;
									outDbData << inFloat64;
								}
								break;
							}
						}
						else
						{
							switch (inPkData.NextDataIs())
							{
								case DATA_SINT32 :
								{
									SInt32 inSint32=0L; 
									inDbData >> inSint32;
									inPkData++;
									outDbData << inSint32;
								}
								break;
								case DATA_STRING :
								{
									SInt8 *inSint8=NULL; 
									inDbData >> inSint8; 
									inPkData++;
									if (inSint8 != NULL)
										outDbData << inSint8;
									else
										outDbData << "";
									if (inSint8 != NULL) delete [] inSint8;
								}
								break;
								case DATA_FLOAT64 :
								{
									Float64 inFloat64=0.;
									inDbData >> inFloat64;
									inPkData++;
									outDbData << inFloat64;
								}
								break;
							}
						}
					}

					if (inGEDARec->data != NULL) delete [] reinterpret_cast <char *> (inGEDARec->data); 
					inGEDARec->data=NULL; inGEDARec->len=0L;

					if (outDbData.GetChunk() != NULL && outDbData.GetSize() > 0)
					{
						inGEDARec->len  = outDbData.GetSize();
						inGEDARec->data = new char [inGEDARec->len];
						::memcpy (inGEDARec->data, outDbData.GetChunk(), inGEDARec->len);
					}

					if ((res = m_DBa->del (m_DBa, NULL, &outDBKey, /*DB_AUTO_COMMIT*/0)) != 0)
					{
						CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, 
						"BERKELEY backend could not del active queue record; " +
						CString(::db_strerror(res)));

						return false;
					}

					CChunk outDBData; if (!GEDARcdToHDBAData (inGEDARec, &outDBData))
					{
						::DeleteGEDRcd ((TGEDRcd*&)inGEDARec);
						return false;
					}

					::bzero(&ioDBData,sizeof(DBT));

					ioDBData.data = outDBData.GetChunk();
					ioDBData.size = outDBData.GetSize();

					if ((res = m_DBa->put (m_DBa, NULL, &outDBKey, &ioDBData, /*DB_AUTO_COMMIT*/0)) != 0)
					{
						CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, 
						"BERKELEY backend could not put active queue record; " +
						CString(::db_strerror(res)));

						return false;
					}

					::DeleteGEDRcd ((TGEDRcd*&)inGEDARec);

					CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_BKD_OPERATION, 
					"BERKELEY backend push active queue on packet type \"" + CString(inGEDPktIn->typ) + "\" [" + inAddr + 
					"] : updated user meta");

					m_Nr = 1L;
				}
			}
		}
		break;

		case GED_PKT_REQ_BKD_SYNC :
		{
			DBT outDBKey, outDBData; ::bzero(&outDBKey,sizeof(DBT)); ::bzero(&outDBData,sizeof(DBT));

			CChunk inDBKey; if (!GEDPktInToHDBKey (inGEDPktCfg, inGEDPktIn, &inDBKey)) return false;

			inDBKey << (long)inSrcAddr.s_addr;
			inDBKey << (unsigned long)inGEDPktIn->tv.tv_sec;
			inDBKey << (unsigned long)inGEDPktIn->tv.tv_usec;

			outDBKey.data = inDBKey.GetChunk();
			outDBKey.size = inDBKey.GetSize();

			CChunk inDBData; if (!GEDPktInToHDBSData (inGEDPktIn, &inSrcAddr, &inDBData)) return false;

			outDBData.data = inDBData.GetChunk();
			outDBData.size = inDBData.GetSize();

			if ((res = m_DBs->put (m_DBs, NULL, &outDBKey, &outDBData, /*DB_AUTO_COMMIT*/0)) != 0)
			{
				CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, 
				"BERKELEY backend could not put sync queue record; " +
				CString(::db_strerror(res)));

				return false;
			}

			m_Ns++;

			m_Nr = 1L;

			m_sInc=BUFFER_INC_8; for (size_t z=BUFFER_INC_8; z<=BUFFER_INC_65536 && ((m_Ns>>m_sInc)>0L); z++) m_sInc = (TBufferInc)z;

			CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_BKD_OPERATION, 
			"BERKELEY backend push sync queue on packet type \"" + CString(inGEDPktIn->typ) + "\" [" + inAddr + 
			"] : created occurence");
		}
		break;

		case GED_PKT_REQ_BKD_HISTORY :
		{
			CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, 
						   CString("BERKELEY backend push in history queue is not authorized"));
			return false;
		}
		break;
	}

	return true;
}

//----------------------------------------------------------------------------------------------------------------------------------------
// db backend drop handler
//----------------------------------------------------------------------------------------------------------------------------------------
bool CGEDBackEndHDB::Drop (const CString &inAddr, const int inQueue, const TGEDPktIn *inGEDPktIn)
{
	m_Nr = 0L;

	if (inGEDPktIn == NULL)
	{
		CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, CString("BERKELEY backend drop on NULL packet"));
		return false;
	}

	TGEDPktCfg *inGEDPktCfg (::GEDPktInToCfg (const_cast <TGEDPktIn *> (inGEDPktIn), CGEDCtx::m_GEDCtx->m_GEDCfg.pkts));

	struct in_addr inSrcAddr; if (!::GetStrAddrToAddrIn (inAddr, &inSrcAddr)) return false; 

	if ((inGEDPktIn->req & GED_PKT_REQ_DROP_MASK) == GED_PKT_REQ_DROP_DATA && inGEDPktCfg == NULL)
	{
		CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, 
					   "BERKELEY backend drop on unknown packet type \"" + CString(inGEDPktIn->typ) + "\"");
		return false;
	}

	int res, n=0; switch (inQueue)
	{
		case GED_PKT_REQ_BKD_ACTIVE :
		{
			DBC *inDBCursor; if ((res = m_DBa->cursor (m_DBa, NULL, &inDBCursor, DB_WRITECURSOR)) != 0)
			{
				CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, 
				"BERKELEY backend could not get active queue cursor");

				return false;
			}

			CChunk inGEDPktInKey; if (inGEDPktIn->data != NULL)
				 if (!GEDPktInToHDBKey (inGEDPktCfg, inGEDPktIn, &inGEDPktInKey)) return false;

			DBT inDBKey, inDBData; ::bzero(&inDBKey,sizeof(DBT)); ::bzero(&inDBData,sizeof(DBT));

			while ((res = inDBCursor->c_get (inDBCursor, &inDBKey, &inDBData, DB_NEXT)) == 0)
			{
				TGEDARcd *inGEDARcd = GEDHDBDataToARcd (&inDBData);

				if (inGEDARcd->typ != inGEDPktIn->typ)
				{
					::DeleteGEDRcd ((TGEDRcd*&)inGEDARcd);
					continue;
				}

				if (inGEDPktIn->data != NULL)
				{
					if (::memcmp (inGEDPktInKey.GetChunk(), inDBKey.data, min(inGEDPktInKey.GetSize(),inDBKey.size)) != 0)
					{
						::DeleteGEDRcd ((TGEDRcd*&)inGEDARcd);
						continue;
					}

					if ((inGEDARcd->ltv.tv_sec  >  inGEDPktIn->tv.tv_sec) ||
					    (inGEDARcd->ltv.tv_sec  == inGEDPktIn->tv.tv_sec  && 
					     inGEDARcd->ltv.tv_usec >= inGEDPktIn->tv.tv_usec))
					{
						::DeleteGEDRcd ((TGEDRcd*&)inGEDARcd);
						continue;
					}
			
					if (inAddr != CString())
					{
						bool found=false; for (size_t j=0; j<inGEDARcd->nsrc && !found; j++)
							found = inGEDARcd->src[j].addr.s_addr == inSrcAddr.s_addr;

						if (!found)
						{
							::DeleteGEDRcd ((TGEDRcd*&)inGEDARcd);
							continue;
						}
					}
				}
				
				Historize (inGEDARcd, false);		

				::DeleteGEDRcd ((TGEDRcd*&)inGEDARcd);

				if ((res = m_DBa->del (m_DBa, NULL, &inDBKey, /*DB_AUTO_COMMIT*/0)) != 0)
				{
					CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, 
					"BERKELEY backend could not del active queue record; " +
					CString(::db_strerror(res)));

					inDBCursor->c_close (inDBCursor);

					return false;
				}

				n++;
			}

			inDBCursor->c_close (inDBCursor);

			m_Na -= n;

			m_Nr  = n;

			m_aInc=BUFFER_INC_8; for (size_t z=BUFFER_INC_8; z<=BUFFER_INC_65536 && ((m_Na>>m_aInc)>0L); z++) m_aInc = (TBufferInc)z;

			CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_BKD_OPERATION, 
			"BERKELEY backend drop active queue on packet type \"" + CString(inGEDPktIn->typ) + 
			"\" [" + inAddr + "] : droped " + CString((long)n) + " record(s)");
		}
		break;

		case GED_PKT_REQ_BKD_SYNC :
		{
			DBC *inDBCursor; if ((res = m_DBs->cursor (m_DBs, NULL, &inDBCursor, DB_WRITECURSOR)) != 0)
			{
				CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, 
				"BERKELEY backend could not get sync queue cursor");

				return false;
			}

			CChunk inGEDPktInKey; if (inGEDPktIn->data != NULL)
				 if (!GEDPktInToHDBKey (inGEDPktCfg, inGEDPktIn, &inGEDPktInKey)) return false;

			DBT inDBKey, inDBData; ::bzero(&inDBKey,sizeof(DBT)); ::bzero(&inDBData,sizeof(DBT));

			while ((res = inDBCursor->c_get (inDBCursor, &inDBKey, &inDBData, DB_NEXT)) == 0)
			{
				TGEDSRcd *inGEDSRcd = GEDHDBDataToSRcd (&inDBData);

				if (inGEDSRcd->pkt->typ != inGEDPktIn->typ)
				{
					::DeleteGEDRcd ((TGEDRcd*&)inGEDSRcd);
					continue;
				}

				if ((inGEDSRcd->pkt->tv.tv_sec  >  inGEDPktIn->tv.tv_sec)  ||
				    (inGEDSRcd->pkt->tv.tv_sec  == inGEDPktIn->tv.tv_sec   && 
				     inGEDSRcd->pkt->tv.tv_usec >  inGEDPktIn->tv.tv_usec))
				{
					::DeleteGEDRcd ((TGEDRcd*&)inGEDSRcd);
					continue;
				}

				if (inAddr != CString())
				{
					if (inSrcAddr.s_addr != inGEDSRcd->tgt.s_addr)
					{
						::DeleteGEDRcd ((TGEDRcd*&)inGEDSRcd);
						continue;
					}
				}

				if (inGEDPktIn->data != NULL)
				{
					if (::memcmp (inGEDPktInKey.GetChunk(), inDBKey.data, min(inGEDPktInKey.GetSize(),inDBKey.size)) != 0)
					{
						::DeleteGEDRcd ((TGEDRcd*&)inGEDSRcd);
						continue;
					}
				}

				::DeleteGEDRcd ((TGEDRcd*&)inGEDSRcd);

				if ((res = m_DBs->del (m_DBs, NULL, &inDBKey, /*DB_AUTO_COMMIT*/0)) != 0)
				{
					CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, 
					"BERKELEY backend could not del sync queue record; " +
					CString(::db_strerror(res)));

					inDBCursor->c_close (inDBCursor);

					return false;
				}
				
				n++;
			}

			inDBCursor->c_close (inDBCursor);

			m_Ns -= n;

			m_Nr  = n;

			m_sInc=BUFFER_INC_8; for (size_t z=BUFFER_INC_8; z<=BUFFER_INC_65536 && ((m_Ns>>m_sInc)>0L); z++) m_sInc = (TBufferInc)z;

			CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_BKD_OPERATION, 
			"BERKELEY backend drop sync queue on packet type \"" + CString(inGEDPktIn->typ) + 
			"\" [" + inAddr + "] : droped " + CString((long)n) + " record(s)");
		}
		break;

		case GED_PKT_REQ_BKD_HISTORY :
		{
			DBC *inDBCursor; if ((res = m_DBh->cursor (m_DBh, NULL, &inDBCursor, DB_WRITECURSOR)) != 0)
			{
				CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, 
				"BERKELEY backend could not get history queue cursor");

				return false;
			}

			if ((inGEDPktIn->req & GED_PKT_REQ_DROP_MASK) == GED_PKT_REQ_DROP_DATA)
			{
				CChunk inGEDPktInKey; if (inGEDPktIn->data != NULL)
					 if (!GEDPktInToHDBKey (inGEDPktCfg, inGEDPktIn, &inGEDPktInKey)) return false;

				DBT inDBKey, inDBData; ::bzero(&inDBKey,sizeof(DBT)); ::bzero(&inDBData,sizeof(DBT));

				while ((res = inDBCursor->c_get (inDBCursor, &inDBKey, &inDBData, DB_NEXT)) == 0)
				{
					TGEDHRcd *inGEDHRcd = GEDHDBDataToHRcd (&inDBData);

					if (inGEDHRcd->typ != inGEDPktIn->typ)
					{
						::DeleteGEDRcd ((TGEDRcd*&)inGEDHRcd);
						continue;
					}

					if (inAddr != CString())
					{
						bool found=false; for (size_t j=0; j<inGEDHRcd->nsrc && !found; j++)
							found = inGEDHRcd->src[j].addr.s_addr == inSrcAddr.s_addr;
						if (!found)
						{
							::DeleteGEDRcd ((TGEDRcd*&)inGEDHRcd);
							continue;
						}
					}

					if (inGEDPktIn->data != NULL)
					{
						if (::memcmp (inGEDPktInKey.GetChunk(), inDBKey.data, min(inGEDPktInKey.GetSize(),inDBKey.size)) != 0)
						{
							::DeleteGEDRcd ((TGEDRcd*&)inGEDHRcd);
							continue;
						}
					}

					if ((res = m_DBh->del (m_DBh, NULL, &inDBKey, /*DB_AUTO_COMMIT*/0)) != 0)
					{
						CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, 
						"BERKELEY backend could not del history queue record; " +
						CString(::db_strerror(res)));

						inDBCursor->c_close (inDBCursor);

						::DeleteGEDRcd ((TGEDRcd*&)inGEDHRcd);

						return false;
					}

					if (m_GEDCfg.loghloc)
						::syslog (m_GEDCfg.loghloc|GED_SYSLOG_INFO, "%s", ::GEDRcdToString(inGEDHRcd,m_GEDCfg.pkts).Get());

					::DeleteGEDRcd ((TGEDRcd*&)inGEDHRcd);

					n++;
				}

				m_Nh -= n;

				m_Nr  = n;

				m_hInc=BUFFER_INC_8; for (size_t z=BUFFER_INC_8; z<=BUFFER_INC_65536 && ((m_Nh>>m_hInc)>0L); z++) m_hInc = (TBufferInc)z;

				CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_BKD_OPERATION, 
				"BERKELEY backend drop history queue on packet type \"" + CString(inGEDPktIn->typ) + 
				"\" [" + inAddr + "] : droped " + CString((long)n) + " record(s)");
			}
			else if ((inGEDPktIn->req & GED_PKT_REQ_DROP_MASK) == GED_PKT_REQ_DROP_ID)
			{
				if (inGEDPktIn->data == NULL)
				{
					CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, 
					CString("BERKELEY backend could not drop history queue record on null id"));

					inDBCursor->c_close (inDBCursor);

					return false;
				}

				DBT inDBKey, inDBData; ::bzero(&inDBKey,sizeof(DBT)); ::bzero(&inDBData,sizeof(DBT));

                                while ((res = inDBCursor->c_get (inDBCursor, &inDBKey, &inDBData, DB_NEXT)) == 0)
                                {
                                        TGEDHRcd *inGEDHRcd = GEDHDBDataToHRcd (&inDBData);

					if (inGEDHRcd->hid != *((unsigned long*)inGEDPktIn->data))
					{
						::DeleteGEDRcd ((TGEDRcd*&)inGEDHRcd);
						continue;
					}

                                        if ((res = m_DBh->del (m_DBh, NULL, &inDBKey, /*DB_AUTO_COMMIT*/0)) != 0)
                                        {
                                                CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION,
                                                "BERKELEY backend could not del history queue record; " +
                                                CString(::db_strerror(res)));

                                                inDBCursor->c_close (inDBCursor);

						::DeleteGEDRcd ((TGEDRcd*&)inGEDHRcd);

                                                return false;
                                        }

					if (m_GEDCfg.loghloc)
						::syslog (m_GEDCfg.loghloc|GED_SYSLOG_INFO, "%s", ::GEDRcdToString(inGEDHRcd,m_GEDCfg.pkts).Get());

					::DeleteGEDRcd ((TGEDRcd*&)inGEDHRcd);

                                        n++;

					break;
				}

				m_Nh -= n;

				m_Nr  = n;

				m_hInc=BUFFER_INC_8; for (size_t z=BUFFER_INC_8; z<=BUFFER_INC_65536 && ((m_Nh>>m_hInc)>0L); z++) m_hInc = (TBufferInc)z;

				CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_BKD_OPERATION, 
				"BERKELEY backend drop history queue on packet id \"" + CString(*((unsigned long*)inGEDPktIn->data)) + 
				"\" [" + inAddr + "] : droped " + CString((long)n) + " record(s)");
			}

			inDBCursor->c_close (inDBCursor);
		}
		break;
	}

	return true;
}

//----------------------------------------------------------------------------------------------------------------------------------------
// db backend peek handler
//----------------------------------------------------------------------------------------------------------------------------------------
TBuffer <TGEDRcd *> CGEDBackEndHDB::Peek (const CString &inAddr, const int inQueue, const TGEDPktIn *inGEDPktIn)
{
	m_Nr = 0L;

	TBuffer <TGEDRcd *> outGEDRcds; 

	struct in_addr inSrcAddr; if (!GetStrAddrToAddrIn (inAddr, &inSrcAddr)) return false;

	UInt32 inTm1 = ((inGEDPktIn!=NULL)&&(inGEDPktIn->req&GED_PKT_REQ_PEEK_PARM_1_TM))?inGEDPktIn->p1:0L;
	UInt32 inTm2 = ((inGEDPktIn!=NULL)&&(inGEDPktIn->req&GED_PKT_REQ_PEEK_PARM_3_TM))?(inGEDPktIn->p3!=0L)?inGEDPktIn->p3:UINT32MAX:UINT32MAX;

	UInt32 inOff = ((inGEDPktIn!=NULL)&&(inGEDPktIn->req&GED_PKT_REQ_PEEK_PARM_2_OFFSET))?inGEDPktIn->p2:0L;
	UInt32 inNum = ((inGEDPktIn!=NULL)&&(inGEDPktIn->req&GED_PKT_REQ_PEEK_PARM_4_NUMBER))?(inGEDPktIn->p4!=0L)?inGEDPktIn->p4:UINT32MAX:UINT32MAX;

	int res; switch (inQueue)
	{
		case GED_PKT_REQ_BKD_ACTIVE :
		{
			TGEDPktCfg *inGEDPktCfg (::GEDPktInToCfg (const_cast<TGEDPktIn*>(inGEDPktIn), CGEDCtx::m_GEDCtx->m_GEDCfg.pkts));

			if (inGEDPktIn != NULL && inGEDPktIn->data != NULL && inGEDPktCfg == NULL)
			{
				CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, 
				"BERKELEY backend peek active queue on unknown packet type \"" + CString(inGEDPktIn->typ) + "\"");

				return outGEDRcds;
			}

			DBC *inDBCursor; if ((res = m_DBa->cursor (m_DBa, NULL, &inDBCursor, 0)) != 0)
			{
				CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, 
				"BERKELEY backend could not get active queue cursor");

				return outGEDRcds;
			}

			CChunk inGEDPktInKey; if (inGEDPktIn != NULL && inGEDPktIn->data != NULL)
				 if (!GEDPktInToHDBKey (inGEDPktCfg, inGEDPktIn, &inGEDPktInKey)) return outGEDRcds;

			DBT inDBKey, inDBData; ::bzero(&inDBKey,sizeof(DBT)); ::bzero(&inDBData,sizeof(DBT));

			if (inGEDPktInKey.GetSize() == 0L) outGEDRcds.SetInc (m_aInc);

			while ((res = inDBCursor->c_get (inDBCursor, &inDBKey, &inDBData, DB_NEXT)) == 0)
			{
				TGEDARcd *inGEDARcd = GEDHDBDataToARcd (&inDBData);

				if (inGEDPktIn != NULL && inGEDARcd->typ != inGEDPktIn->typ && inGEDPktIn->typ != 0)
				{
					::DeleteGEDRcd ((TGEDRcd*&)inGEDARcd);
					continue;
				}

				if (inGEDARcd->otv.tv_sec < inTm1 || inGEDARcd->otv.tv_sec > inTm2)
				{
					::DeleteGEDRcd ((TGEDRcd*&)inGEDARcd);
					continue;
				}

				if (inAddr != CString())
				{
					bool found=false; for (size_t j=0; j<inGEDARcd->nsrc && !found; j++)
						found = inGEDARcd->src[j].addr.s_addr == inSrcAddr.s_addr;
					if (!found)
					{
						::DeleteGEDRcd ((TGEDRcd*&)inGEDARcd);
						continue;
					}
				}

				if (inGEDPktIn != NULL && inGEDPktIn->data != NULL)
				{
					if (::memcmp (inGEDPktInKey.GetChunk(), inDBKey.data, 
						      min (inGEDPktInKey.GetSize(), inDBKey.size)) != 0)
					{
						::DeleteGEDRcd ((TGEDRcd*&)inGEDARcd);
						continue;
					}
				}

				outGEDRcds += inGEDARcd;
			}

			inDBCursor->c_close (inDBCursor);

			m_Nr = outGEDRcds.GetLength();

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

				for (UInt32 i=min(inOff,outGEDRcds.GetLength()); i>0L; i--) ::DeleteGEDRcd (*outGEDRcds[i-1]);
				for (UInt32 i=outGEDRcds.GetLength(); i>inOff+inNum; i--) ::DeleteGEDRcd (*outGEDRcds[i-1]);

				outGEDRcds = TBuffer <TGEDRcd *> (outGEDRcds.Get(inOff), min(inNum,outGEDRcds.GetLength()-inOff), m_aInc);
			}

			CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_BKD_OPERATION, 
			"BERKELEY backend peek active queue on packet type \"" + CString(inGEDPktIn!=NULL?inGEDPktIn->typ:0) + 
			"\" [" + inAddr + "] : " + CString((long)outGEDRcds.GetLength()) + " result(s)");
		}
		break;

		case GED_PKT_REQ_BKD_SYNC :
		{
			TGEDPktCfg *inGEDPktCfg (::GEDPktInToCfg (const_cast<TGEDPktIn*>(inGEDPktIn), CGEDCtx::m_GEDCtx->m_GEDCfg.pkts));

			if (inGEDPktIn != NULL && inGEDPktIn->data != NULL && inGEDPktCfg == NULL)
			{
				CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, 
				"BERKELEY backend peek sync queue on unknown packet type \"" + CString(inGEDPktIn->typ) + "\"");

				return outGEDRcds;
			}

			DBC *inDBCursor; if ((res = m_DBs->cursor (m_DBs, NULL, &inDBCursor, 0)) != 0)
			{
				CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, 
				"BERKELEY backend could not get sync queue cursor");

				return outGEDRcds;
			}

			CChunk inGEDPktInKey; if (inGEDPktIn != NULL && inGEDPktIn->data != NULL)
				 if (!GEDPktInToHDBKey (inGEDPktCfg, inGEDPktIn, &inGEDPktInKey)) return outGEDRcds;

			if (inGEDPktInKey.GetSize() == 0L) outGEDRcds.SetInc (m_sInc);

			DBT inDBKey, inDBData; ::bzero(&inDBKey,sizeof(DBT)); ::bzero(&inDBData,sizeof(DBT));

			while ((res = inDBCursor->c_get (inDBCursor, &inDBKey, &inDBData, DB_NEXT)) == 0)
			{
				TGEDSRcd *inGEDSRcd = GEDHDBDataToSRcd (&inDBData);

				if (inGEDSRcd->pkt->tv.tv_sec < inTm1 || inGEDSRcd->pkt->tv.tv_sec > inTm2)
				{
					::DeleteGEDRcd ((TGEDRcd*&)inGEDSRcd);
					continue;
				}

				if (inAddr != CString() && inGEDSRcd->tgt.s_addr != inSrcAddr.s_addr)
				{
					::DeleteGEDRcd ((TGEDRcd*&)inGEDSRcd);
					continue;
				}

				if (inGEDPktIn != NULL && inGEDPktIn->data != NULL && inGEDSRcd->pkt != NULL && 
				    inGEDSRcd->pkt->data != NULL)
				{
					if (inGEDPktIn->typ != inGEDSRcd->pkt->typ)
					{
						::DeleteGEDRcd ((TGEDRcd*&)inGEDSRcd);
						continue;
					}

					if (::memcmp (inGEDPktInKey.GetChunk(), inDBKey.data, 
						      min (inGEDPktInKey.GetSize(), inDBKey.size)) != 0)
					{
						::DeleteGEDRcd ((TGEDRcd*&)inGEDSRcd);
						continue;
					}
				}

				outGEDRcds += inGEDSRcd;
			}

			inDBCursor->c_close (inDBCursor);

			m_Nr = outGEDRcds.GetLength();

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

				for (UInt32 i=min(inOff,outGEDRcds.GetLength()); i>0L; i--) ::DeleteGEDRcd (*outGEDRcds[i-1]);
				for (UInt32 i=outGEDRcds.GetLength(); i>inOff+inNum; i--) ::DeleteGEDRcd (*outGEDRcds[i-1]);

				outGEDRcds = TBuffer <TGEDRcd *> (outGEDRcds.Get(inOff), min(inNum,outGEDRcds.GetLength()-inOff), m_sInc);
			}

			CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_BKD_OPERATION, 
			"BERKELEY backend peek sync queue on packet type \"" + CString(inGEDPktIn!=NULL?inGEDPktIn->typ:0) + 
			"\" [" + inAddr + "] : " + CString((long)outGEDRcds.GetLength()) + " result(s)");
		}
		break;

		case GED_PKT_REQ_BKD_HISTORY :
		{
			TGEDPktCfg *inGEDPktCfg (::GEDPktInToCfg (const_cast<TGEDPktIn*>(inGEDPktIn), CGEDCtx::m_GEDCtx->m_GEDCfg.pkts));

			if (inGEDPktIn != NULL && inGEDPktIn->data != NULL && inGEDPktCfg == NULL)
			{
				CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, 
					"BERKELEY backend peek history queue on unknown packet type \"" + CString(inGEDPktIn->typ) + "\"");

				return outGEDRcds;
			}

			DBC *inDBCursor; if ((res = m_DBh->cursor (m_DBh, NULL, &inDBCursor, 0)) != 0)
			{
				CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, 
				"BERKELEY backend could not get history queue cursor");

				return outGEDRcds;
			}

			CChunk inGEDPktInKey; if (inGEDPktIn != NULL && inGEDPktIn->data != NULL)
				 if (!GEDPktInToHDBKey (inGEDPktCfg, inGEDPktIn, &inGEDPktInKey)) return outGEDRcds;

			if (inGEDPktInKey.GetSize() == 0L) outGEDRcds.SetInc (m_hInc);

			DBT inDBKey, inDBData; ::bzero(&inDBKey,sizeof(DBT)); ::bzero(&inDBData,sizeof(DBT));

			while ((res = inDBCursor->c_get (inDBCursor, &inDBKey, &inDBData, DB_NEXT)) == 0)
			{
				TGEDHRcd *inGEDHRcd = GEDHDBDataToHRcd (&inDBData);

				if (inGEDPktIn != NULL && inGEDHRcd->typ != inGEDPktIn->typ && inGEDPktIn->typ != 0)
				{
					::DeleteGEDRcd ((TGEDRcd*&)inGEDHRcd);
					continue;
				}

				if (inGEDHRcd->otv.tv_sec < inTm1 || inGEDHRcd->otv.tv_sec > inTm2)
				{
					::DeleteGEDRcd ((TGEDRcd*&)inGEDHRcd);
					continue;
				}

				if (inAddr != CString())
				{
					bool found=false; for (size_t j=0; j<inGEDHRcd->nsrc && !found; j++)
						found = inGEDHRcd->src[j].addr.s_addr == inSrcAddr.s_addr;
					if (!found)
					{
						::DeleteGEDRcd ((TGEDRcd*&)inGEDHRcd);
						continue;
					}
				}

				if (inGEDPktIn != NULL && inGEDPktIn->data != NULL)
				{
					if (::memcmp (inGEDPktInKey.GetChunk(), inDBKey.data, 
						      min (inGEDPktInKey.GetSize(), inDBKey.size)) != 0)
					{
						::DeleteGEDRcd ((TGEDRcd*&)inGEDHRcd);
						continue;
					}
				}

				outGEDRcds += inGEDHRcd;
			}

			inDBCursor->c_close (inDBCursor);

			m_Nr = outGEDRcds.GetLength();

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

				for (UInt32 i=min(inOff,outGEDRcds.GetLength()); i>0L; i--) ::DeleteGEDRcd (*outGEDRcds[i-1]);
				for (UInt32 i=outGEDRcds.GetLength(); i>inOff+inNum; i--) ::DeleteGEDRcd (*outGEDRcds[i-1]);

				outGEDRcds = TBuffer <TGEDRcd *> (outGEDRcds.Get(inOff), min(inNum,outGEDRcds.GetLength()-inOff), m_hInc);
			}

			CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_BKD_OPERATION, 
			"BERKELEY backend peek history queue on packet type \"" + CString(inGEDPktIn!=NULL?inGEDPktIn->typ:0) + 
			"\" [" + inAddr + "] : " + CString((long)outGEDRcds.GetLength()) + " result(s)");
		}
		break;
	}

	return outGEDRcds;
}

//----------------------------------------------------------------------------------------------------------------------------------------
// db recover request
//----------------------------------------------------------------------------------------------------------------------------------------
bool CGEDBackEndHDB::Recover (const TBuffer <TGEDRcd *> &inGEDRecords, void (*inCB) (const UInt32, const UInt32, const TGEDRcd *))
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

				TGEDPktCfg *inGEDPktCfg = NULL; for (size_t i=m_GEDCfg.pkts.GetLength(); i>0 && !inGEDPktCfg; i--)
					if (m_GEDCfg.pkts[i-1]->type == inGEDARcd->typ)
						inGEDPktCfg = m_GEDCfg.pkts[i-1];

				CChunk outDBKeyChunk, outDBDataChunk;

				if (!GEDARcdToHDBKey (inGEDPktCfg, inGEDARcd, &outDBKeyChunk) || !GEDARcdToHDBAData (inGEDARcd, &outDBDataChunk))
				{
					CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, 
					"BERKELEY backend could not recover active queue record number " + CString(naa) + " : key or data error");
					break;
				}

				DBT outDBKey, outDBData;

				bzero (&outDBKey,  sizeof(DBT));
				bzero (&outDBData, sizeof(DBT));

				outDBKey.data = outDBKeyChunk.GetChunk();		
				outDBKey.size = outDBKeyChunk.GetSize();

				outDBData.data = outDBDataChunk.GetChunk();
				outDBData.size = outDBDataChunk.GetSize();

				int res; if ((res = m_DBa->put (m_DBa, NULL, &outDBKey, &outDBData, DB_NOOVERWRITE|/*DB_AUTO_COMMIT*/0)) != 0)
				{
					CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, 
					"BERKELEY backend could not recover active queue record number " + CString(naa) + " : " +
					CString(::db_strerror(res)));
				}
				else
				{
					na++;
				}
			}
			break;

			case GED_PKT_REQ_BKD_HISTORY :
			{
				nhh++;

				TGEDHRcd *inGEDHRcd (static_cast <TGEDHRcd *> (inGEDRcd));

				if (inGEDHRcd->hid <= 0L) inGEDHRcd->hid = m_HID++;

				TGEDPktCfg *inGEDPktCfg = NULL; for (size_t i=m_GEDCfg.pkts.GetLength(); i>0 && !inGEDPktCfg; i--)
					if (m_GEDCfg.pkts[i-1]->type == inGEDHRcd->typ)
						inGEDPktCfg = m_GEDCfg.pkts[i-1];

				CChunk outDBKeyChunk, outDBDataChunk;

				if (!GEDHRcdToHDBKey (inGEDPktCfg, inGEDHRcd, &outDBKeyChunk) || !GEDHRcdToHDBHData (inGEDHRcd, &outDBDataChunk))
				{
					CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, 
					"BERKELEY backend could not recover history queue record number " + CString(nhh) + " : key or data error");
					break;
				}

				DBT outDBKey, outDBData;

				bzero (&outDBKey,  sizeof(DBT));
				bzero (&outDBData, sizeof(DBT));

				outDBKey.data = outDBKeyChunk.GetChunk();		
				outDBKey.size = outDBKeyChunk.GetSize();

				outDBData.data = outDBDataChunk.GetChunk();
				outDBData.size = outDBDataChunk.GetSize();

				int res; if ((res = m_DBh->put (m_DBh, NULL, &outDBKey, &outDBData, DB_NOOVERWRITE|/*DB_AUTO_COMMIT*/0)) != 0)
				{
					CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, 
					"BERKELEY backend could not recover history queue record number " + CString(nhh) + " : " +
					CString(::db_strerror(res)));
				}
				else
				{
					nh++;
				}
			}
			break;

			case GED_PKT_REQ_BKD_SYNC :
			{
				nss++;

				TGEDSRcd *inGEDSRcd (static_cast <TGEDSRcd *> (inGEDRcd));

				if (inGEDSRcd->pkt == NULL)
				{
					CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, 
					"BERKELEY backend could not recover sync queue record number " + CString(nss) + " : null data error");
					break;
				}

				TGEDPktCfg *inGEDPktCfg = NULL; for (size_t i=m_GEDCfg.pkts.GetLength(); i>0 && !inGEDPktCfg; i--)
					if (m_GEDCfg.pkts[i-1]->type == inGEDSRcd->pkt->typ)
						inGEDPktCfg = m_GEDCfg.pkts[i-1];

				CChunk outDBKeyChunk, outDBDataChunk;

				if (!GEDSRcdToHDBKey (inGEDPktCfg, inGEDSRcd, &outDBKeyChunk) || !GEDSRcdToHDBSData (inGEDSRcd, &outDBDataChunk))
				{
					CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, 
					"BERKELEY backend could not recover sync queue record number " + CString(nss) + " : key or data error");
					break;
				}

				DBT outDBKey, outDBData;

				bzero (&outDBKey,  sizeof(DBT));
				bzero (&outDBData, sizeof(DBT));

				outDBKey.data = outDBKeyChunk.GetChunk();		
				outDBKey.size = outDBKeyChunk.GetSize();

				outDBData.data = outDBDataChunk.GetChunk();
				outDBData.size = outDBDataChunk.GetSize();

				int res; if ((res = m_DBs->put (m_DBs, NULL, &outDBKey, &outDBData, DB_NOOVERWRITE|/*DB_AUTO_COMMIT*/0)) != 0)
				{
					CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, 
					"BERKELEY backend could not recover sync queue record number " + CString(nss) + " : " +
					CString(::db_strerror(res)));
				}
				else
				{
					ns++;
				}
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
				   "BERKELEY backend recovered " + CString(na) + "/" + CString(naa) + " active records, " + 
				   CString(nh) + "/" + CString(nhh) + " history records, " +
				   CString(ns) + "/" + CString(nss) + " sync records");

	return true;
}


//----------------------------------------------------------------------------------------------------------------------------------------
// ged version upgrade notification handler
//----------------------------------------------------------------------------------------------------------------------------------------
bool CGEDBackEndHDB::NotifyVersionUpgrade (const UInt32 inOldVersion)
{
	// nothing to do for now...
	return true;
}

//----------------------------------------------------------------------------------------------------------------------------------------
// db packet template modification notification handler
//----------------------------------------------------------------------------------------------------------------------------------------
bool CGEDBackEndHDB::NotifyPktCfgChange (const long inType, const TGEDPktCfgChange inGEDPktCfgChange)
{
	if (inGEDPktCfgChange != GED_PKT_CFG_CHANGE_DELETE && inGEDPktCfgChange != GED_PKT_CFG_CHANGE_MODIFY) return true;

	int res; DBC *inDBCursor; if ((res = m_DBa->cursor (m_DBa, NULL, &inDBCursor, DB_WRITECURSOR)) != 0)
	{
		CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, 
		"BERKELEY backend could not get active queue cursor");

		return false;
	}

	DBT inDBKey, inDBData; ::bzero(&inDBKey,sizeof(DBT)); ::bzero(&inDBData,sizeof(DBT));

	long na=0L; while ((res = inDBCursor->c_get (inDBCursor, &inDBKey, &inDBData, DB_NEXT)) == 0)
	{
		TGEDARcd *inGEDARcd = GEDHDBDataToARcd (&inDBData);

		if (inGEDARcd->typ != inType)
		{
			::DeleteGEDRcd ((TGEDRcd*&)inGEDARcd);
			continue;
		}

		if ((res = m_DBa->del (m_DBa, NULL, &inDBKey, /*DB_AUTO_COMMIT*/0)) != 0)
		{
			CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, 
			"BERKELEY backend could not del active queue record; " +
			CString(::db_strerror(res)));

			::DeleteGEDRcd ((TGEDRcd*&)inGEDARcd);

			inDBCursor->c_close (inDBCursor);

			return false;
		}

		::DeleteGEDRcd ((TGEDRcd*&)inGEDARcd);

		na++;
	}

	inDBCursor->c_close (inDBCursor);

	if ((res = m_DBs->cursor (m_DBs, NULL, &inDBCursor, DB_WRITECURSOR)) != 0)
	{
		CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, 
		"BERKELEY backend could not get sync queue cursor");

		return false;
	}

	::bzero(&inDBKey,sizeof(DBT)); ::bzero(&inDBData,sizeof(DBT));

	long ns=0L; while ((res = inDBCursor->c_get (inDBCursor, &inDBKey, &inDBData, DB_NEXT)) == 0)
	{
		TGEDSRcd *inGEDSRcd = GEDHDBDataToSRcd (&inDBData);

		if (inGEDSRcd->pkt->typ != inType)
		{
			::DeleteGEDRcd ((TGEDRcd*&)inGEDSRcd);
			continue;
		}

		if ((res = m_DBs->del (m_DBs, NULL, &inDBKey, /*DB_AUTO_COMMIT*/0)) != 0)
		{
			CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, 
			"BERKELEY backend could not del sync queue record; " +
			CString(::db_strerror(res)));

			::DeleteGEDRcd ((TGEDRcd*&)inGEDSRcd);

			inDBCursor->c_close (inDBCursor);

			return false;
		}

		::DeleteGEDRcd ((TGEDRcd*&)inGEDSRcd);

		ns++;
	}

	inDBCursor->c_close (inDBCursor);

	if ((res = m_DBh->cursor (m_DBh, NULL, &inDBCursor, DB_WRITECURSOR)) != 0)
	{
		CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, 
		"BERKELEY backend could not get history queue cursor");

		return false;
	}

	::bzero(&inDBKey,sizeof(DBT)); ::bzero(&inDBData,sizeof(DBT));

	long nh=0L; while ((res = inDBCursor->c_get (inDBCursor, &inDBKey, &inDBData, DB_NEXT)) == 0)
	{
		TGEDHRcd *inGEDHRcd = GEDHDBDataToHRcd (&inDBData);

		if (inGEDHRcd->typ != inType)
		{
			::DeleteGEDRcd ((TGEDRcd*&)inGEDHRcd);
			continue;
		}

		if ((res = m_DBh->del (m_DBh, NULL, &inDBKey, /*DB_AUTO_COMMIT*/0)) != 0)
		{
			CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, 
			"BERKELEY backend could not del history queue record; " +
			CString(::db_strerror(res)));

			::DeleteGEDRcd ((TGEDRcd*&)inGEDHRcd);

			inDBCursor->c_close (inDBCursor);

			return false;
		}

		::DeleteGEDRcd ((TGEDRcd*&)inGEDHRcd);

		nh++;
	}

	inDBCursor->c_close (inDBCursor);

	CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_BKD_OPERATION, 
	"BERKELEY backend droped " + CString(na) + " active record(s), " + CString(ns) + " sync record(s), " + 
	CString(nh) + " history record(s), packet type \"" + CString(inType) + "\" due to cfg change");

	return true;
}

//----------------------------------------------------------------------------------------------------------------------------------------
// ged pkt to hash db key
//----------------------------------------------------------------------------------------------------------------------------------------
bool CGEDBackEndHDB::GEDPktInToHDBKey (const TGEDPktCfg *inGEDPktCfg, const TGEDPktIn *inGEDPktIn, CChunk *outDBKey) const
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
				long val; inChunk >> val; *outDBKey << val;
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
				double val; inChunk >> val; *outDBKey << val;
			}
			break;
		}

		j++;
	}

	return true;
}

//----------------------------------------------------------------------------------------------------------------------------------------
// active record to db key
//----------------------------------------------------------------------------------------------------------------------------------------
bool CGEDBackEndHDB::GEDARcdToHDBKey (const TGEDPktCfg *inGEDPktCfg, const TGEDARcd *inGEDARec, CChunk *outDBKey) const
{
	if (inGEDARec == NULL)
	{
		CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, 
					   CString("BERKELEY backend cannot convert key of NULL record"));
		return false;
	}

	if (inGEDPktCfg == NULL)
	{
		CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, 
					   "BERKELEY backend cannot convert key of unknown packet type \"" + 
					   CString(inGEDARec->typ) + "\"");
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

	*outDBKey << (long)inGEDARec->typ;

	TBuffer <TData> inTData (::GEDPktCfgToTData(const_cast<TGEDPktCfg*>(inGEDPktCfg)));

	CChunk inChunk (const_cast<TGEDARcd*>(inGEDARec)->data, const_cast<TGEDARcd*>(inGEDARec)->len, inTData, false);

	for (size_t i=0, j=0; i<gedPktCfg.keyidc.GetLength(); i++)
	{
		for (; j<*gedPktCfg.keyidc[i]; j++) { inChunk++; }

		switch (gedPktCfg.fields[*gedPktCfg.keyidc[i]]->type)
		{
			case DATA_SINT32 :
			{
				long val; inChunk >> val; *outDBKey << val;
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
				double val; inChunk >> val; *outDBKey << val;
			}
			break;
		}

		j++;
	}

	return true;
}

//----------------------------------------------------------------------------------------------------------------------------------------
// history record to db key
//----------------------------------------------------------------------------------------------------------------------------------------
bool CGEDBackEndHDB::GEDHRcdToHDBKey (const TGEDPktCfg *inGEDPktCfg, const TGEDHRcd *inGEDHRec, CChunk *outDBKey) const
{
	if (inGEDHRec == NULL)
	{
		CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, 
					   CString("BERKELEY backend cannot convert key of NULL record"));
		return false;
	}

	if (inGEDPktCfg == NULL)
	{
		CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, 
					   "BERKELEY backend cannot convert key of unknown packet type \"" + 
					   CString(inGEDHRec->typ) + "\"");
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

	*outDBKey << (long)inGEDHRec->typ;

	TBuffer <TData> inTData (::GEDPktCfgToTData(const_cast<TGEDPktCfg*>(inGEDPktCfg)));

	CChunk inChunk (const_cast<TGEDHRcd*>(inGEDHRec)->data, const_cast<TGEDHRcd*>(inGEDHRec)->len, inTData, false);

	for (size_t i=0, j=0; i<gedPktCfg.keyidc.GetLength(); i++)
	{
		for (; j<*gedPktCfg.keyidc[i]; j++) { inChunk++; }

		switch (gedPktCfg.fields[*gedPktCfg.keyidc[i]]->type)
		{
			case DATA_SINT32 :
			{
				long val; inChunk >> val; *outDBKey << val;
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
				double val; inChunk >> val; *outDBKey << val;
			}
			break;
		}

		j++;
	}

	*outDBKey << (unsigned long) inGEDHRec->atv.tv_sec;
	*outDBKey << (unsigned long) inGEDHRec->atv.tv_usec;

	return true;
}

//----------------------------------------------------------------------------------------------------------------------------------------
// history record to db key
//----------------------------------------------------------------------------------------------------------------------------------------
bool CGEDBackEndHDB::GEDSRcdToHDBKey (const TGEDPktCfg *inGEDPktCfg, const TGEDSRcd *inGEDSRec, CChunk *outDBKey) const
{
	if (inGEDSRec == NULL || inGEDSRec->pkt == NULL || inGEDSRec->pkt->data == NULL)
	{
		CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, 
					   CString("BERKELEY backend cannot convert key of NULL record"));
		return false;
	}

	if (inGEDPktCfg == NULL)
	{
		CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, 
					   "BERKELEY backend cannot convert key of unknown packet type \"" + 
					   CString(inGEDSRec->pkt->typ) + "\"");
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

	*outDBKey << (long)inGEDSRec->pkt->typ;

	TBuffer <TData> inTData (::GEDPktCfgToTData(const_cast<TGEDPktCfg*>(inGEDPktCfg)));

	CChunk inChunk (const_cast<TGEDSRcd*>(inGEDSRec)->pkt->data, const_cast<TGEDSRcd*>(inGEDSRec)->pkt->len, inTData, false);

	for (size_t i=0, j=0; i<gedPktCfg.keyidc.GetLength(); i++)
	{
		for (; j<*gedPktCfg.keyidc[i]; j++) { inChunk++; }

		switch (gedPktCfg.fields[*gedPktCfg.keyidc[i]]->type)
		{
			case DATA_SINT32 :
			{
				long val; inChunk >> val; *outDBKey << val;
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
				double val; inChunk >> val; *outDBKey << val;
			}
			break;
		}

		j++;
	}

	*outDBKey << (long)inGEDSRec->tgt.s_addr;
	*outDBKey << (unsigned long)inGEDSRec->pkt->tv.tv_sec;
	*outDBKey << (unsigned long)inGEDSRec->pkt->tv.tv_usec;

	return true;
}

//----------------------------------------------------------------------------------------------------------------------------------------
// ged pkt to db active data
//----------------------------------------------------------------------------------------------------------------------------------------
bool CGEDBackEndHDB::GEDPktInToHDBAData (const TGEDPktIn *inGEDPktIn, const struct in_addr *inSrcAddr, CChunk *outDBData) const
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
	*outDBData << (long)inGEDPktIn->dmy[0];
	*outDBData << (long)inGEDPktIn->dmy[1];

	*outDBData << (long)(inGEDPktIn->req&GED_PKT_REQ_SRC_RELAY);
	*outDBData << (long)inSrcAddr->s_addr;

	outDBData->WritePVoid (inGEDPktIn->data, inGEDPktIn->len);

	return true;
}

//----------------------------------------------------------------------------------------------------------------------------------------
// ged pkt to db sync data
//----------------------------------------------------------------------------------------------------------------------------------------
bool CGEDBackEndHDB::GEDPktInToHDBSData (const TGEDPktIn *inGEDPktIn, const struct in_addr *inTgtAddr, CChunk *outDBData) const
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

	if (inTgtAddr == NULL || outDBData == NULL) return false;

	*outDBData << (long)GED_PKT_REQ_BKD_SYNC;
	*outDBData << (long)inTgtAddr->s_addr;
	*outDBData << (unsigned long)inGEDPktIn->tv.tv_sec;
	*outDBData << (unsigned long)inGEDPktIn->tv.tv_usec;
	*outDBData << (long)inGEDPktIn->addr.s_addr;
	*outDBData << (long)inGEDPktIn->typ;
	*outDBData << (long)(inGEDPktIn->req|GED_PKT_REQ_SRC_RELAY);
	*outDBData << (unsigned long)inGEDPktIn->len;
	*outDBData << (long)inGEDPktIn->dmy[0];
	*outDBData << (long)inGEDPktIn->dmy[1];

	outDBData->WritePVoid (inGEDPktIn->data, inGEDPktIn->len);

	return true;
}

//----------------------------------------------------------------------------------------------------------------------------------------
// db data to ged active record
//----------------------------------------------------------------------------------------------------------------------------------------
TGEDARcd * CGEDBackEndHDB::GEDHDBDataToARcd (DBT *inDBData) const
{
	TGEDARcd *outGEDARec = new TGEDARcd; ::bzero(outGEDARec,sizeof(TGEDARcd));

	if (inDBData == NULL) return outGEDARec;

	CChunk inDBChunk (inDBData->data, inDBData->size, TBuffer <TData> (), false);

	inDBChunk >> (long &)outGEDARec->queue;
	inDBChunk >> (long &)outGEDARec->typ;
	inDBChunk >> (unsigned long &)outGEDARec->occ;
	inDBChunk >> (unsigned long &)outGEDARec->otv.tv_sec;
	inDBChunk >> (unsigned long &)outGEDARec->otv.tv_usec;
	inDBChunk >> (unsigned long &)outGEDARec->ltv.tv_sec;
	inDBChunk >> (unsigned long &)outGEDARec->ltv.tv_usec;
	inDBChunk >> (unsigned long &)outGEDARec->rtv.tv_sec;
	inDBChunk >> (unsigned long &)outGEDARec->rtv.tv_usec;
	inDBChunk >> (unsigned long &)outGEDARec->mtv.tv_sec;
	inDBChunk >> (unsigned long &)outGEDARec->mtv.tv_usec;
	inDBChunk >> (unsigned long &)outGEDARec->nsrc;
	inDBChunk >> (unsigned long &)outGEDARec->len;
	inDBChunk >> (long &)outGEDARec->rsv[0];
	inDBChunk >> (long &)outGEDARec->rsv[1];

	if (outGEDARec->nsrc > 0)
	{
		outGEDARec->src = new TGEDRcdSrc [outGEDARec->nsrc];
		for (size_t i=0; i<outGEDARec->nsrc; i++)
		{
			inDBChunk >> (long&)outGEDARec->src[i].rly;
			inDBChunk >> (long&)outGEDARec->src[i].addr.s_addr;
		}
	}

	if (outGEDARec->len > 0) inDBChunk.ReadPVoid ((void*&)outGEDARec->data);

	return outGEDARec;
}

//----------------------------------------------------------------------------------------------------------------------------------------
// db data to ged sync record
//----------------------------------------------------------------------------------------------------------------------------------------
TGEDSRcd * CGEDBackEndHDB::GEDHDBDataToSRcd (DBT *inDBData) const
{
	TGEDSRcd *outGEDSRec = new TGEDSRcd; ::bzero(outGEDSRec,sizeof(TGEDSRcd));

	if (inDBData == NULL) return outGEDSRec;

	CChunk inDBChunk (inDBData->data, inDBData->size, TBuffer <TData> (), false);

	outGEDSRec->pkt = new TGEDPktIn; ::bzero(outGEDSRec->pkt,sizeof(TGEDPktIn));
	outGEDSRec->pkt->vrs = GED_VERSION;

	inDBChunk >> (long&)outGEDSRec->queue;
	inDBChunk >> (long&)outGEDSRec->tgt.s_addr;
	inDBChunk >> (unsigned long &)outGEDSRec->pkt->tv.tv_sec;
	inDBChunk >> (unsigned long &)outGEDSRec->pkt->tv.tv_usec;
	inDBChunk >> (long&)outGEDSRec->pkt->addr.s_addr;
	inDBChunk >> (long&)outGEDSRec->pkt->typ;
	inDBChunk >> (long&)outGEDSRec->pkt->req;
	inDBChunk >> (unsigned long &)outGEDSRec->pkt->len;
	inDBChunk >> (long&)outGEDSRec->pkt->dmy[0];
	inDBChunk >> (long&)outGEDSRec->pkt->dmy[1];

	if (outGEDSRec->pkt->len > 0) inDBChunk.ReadPVoid ((void*&)outGEDSRec->pkt->data);

	return outGEDSRec;
}

//----------------------------------------------------------------------------------------------------------------------------------------
// db data to ged history record
//----------------------------------------------------------------------------------------------------------------------------------------
TGEDHRcd * CGEDBackEndHDB::GEDHDBDataToHRcd (DBT *inDBData) const
{
	TGEDHRcd *outGEDHRec = new TGEDHRcd; ::bzero(outGEDHRec,sizeof(TGEDHRcd));

	if (inDBData == NULL) return outGEDHRec;

	CChunk inDBChunk (inDBData->data, inDBData->size, TBuffer <TData> (), false);

	inDBChunk >> (long&)outGEDHRec->queue;
	inDBChunk >> (long&)outGEDHRec->typ;
	inDBChunk >> (unsigned long &)outGEDHRec->hid;
	inDBChunk >> (unsigned long &)outGEDHRec->occ;
	inDBChunk >> (unsigned long &)outGEDHRec->otv.tv_sec;
	inDBChunk >> (unsigned long &)outGEDHRec->otv.tv_usec;
	inDBChunk >> (unsigned long &)outGEDHRec->ltv.tv_sec;
	inDBChunk >> (unsigned long &)outGEDHRec->ltv.tv_usec;
	inDBChunk >> (unsigned long &)outGEDHRec->rtv.tv_sec;
	inDBChunk >> (unsigned long &)outGEDHRec->rtv.tv_usec;
	inDBChunk >> (unsigned long &)outGEDHRec->mtv.tv_sec;
	inDBChunk >> (unsigned long &)outGEDHRec->mtv.tv_usec;
	inDBChunk >> (unsigned long &)outGEDHRec->atv.tv_sec;
	inDBChunk >> (unsigned long &)outGEDHRec->atv.tv_usec;
	inDBChunk >> (unsigned long &)outGEDHRec->nsrc;
	inDBChunk >> (unsigned long &)outGEDHRec->len;
	inDBChunk >> (long&)outGEDHRec->rsv[0];
	inDBChunk >> (long&)outGEDHRec->rsv[1];

	if (outGEDHRec->nsrc > 0)
	{
		outGEDHRec->src = new TGEDRcdSrc [outGEDHRec->nsrc];
		for (size_t i=0; i<outGEDHRec->nsrc; i++)
		{
			inDBChunk >> (long&)outGEDHRec->src[i].rly;
			inDBChunk >> (long&)outGEDHRec->src[i].addr.s_addr;
		}
	}

	if (outGEDHRec->len > 0) inDBChunk.ReadPVoid ((void*&)outGEDHRec->data);

	return outGEDHRec;
}

//----------------------------------------------------------------------------------------------------------------------------------------
// ged record to db data
//----------------------------------------------------------------------------------------------------------------------------------------
bool CGEDBackEndHDB::GEDARcdToHDBAData (TGEDARcd *inGEDARec, CChunk *outDBData) const
{
	if (inGEDARec == NULL || outDBData == NULL) return false;

	*outDBData << (long)inGEDARec->queue;
	*outDBData << (long)inGEDARec->typ;
	*outDBData << (unsigned long)inGEDARec->occ;
	*outDBData << (unsigned long)inGEDARec->otv.tv_sec;
	*outDBData << (unsigned long)inGEDARec->otv.tv_usec;
	*outDBData << (unsigned long)inGEDARec->ltv.tv_sec;
	*outDBData << (unsigned long)inGEDARec->ltv.tv_usec;
	*outDBData << (unsigned long)inGEDARec->rtv.tv_sec;
	*outDBData << (unsigned long)inGEDARec->rtv.tv_usec;
	*outDBData << (unsigned long)inGEDARec->mtv.tv_sec;
	*outDBData << (unsigned long)inGEDARec->mtv.tv_usec;
	*outDBData << (unsigned long)inGEDARec->nsrc;
	*outDBData << (unsigned long)inGEDARec->len;
	*outDBData << (long)inGEDARec->rsv[0];
	*outDBData << (long)inGEDARec->rsv[1];

	for (size_t i=0; i<inGEDARec->nsrc; i++)
	{
		*outDBData << (long)inGEDARec->src[i].rly;
		*outDBData << (long)inGEDARec->src[i].addr.s_addr;
	}

	if (inGEDARec->len > 0) outDBData->WritePVoid (inGEDARec->data, inGEDARec->len);

	return true;
}

//----------------------------------------------------------------------------------------------------------------------------------------
// ged record to db data
//----------------------------------------------------------------------------------------------------------------------------------------
bool CGEDBackEndHDB::GEDHRcdToHDBHData (TGEDHRcd *inGEDHRec, CChunk *outDBData) const
{
	if (inGEDHRec == NULL || outDBData == NULL) return false;

	*outDBData << (long)inGEDHRec->queue;
	*outDBData << (long)inGEDHRec->typ;
	*outDBData << (unsigned long)inGEDHRec->hid;
	*outDBData << (unsigned long)inGEDHRec->occ;
	*outDBData << (unsigned long)inGEDHRec->otv.tv_sec;
	*outDBData << (unsigned long)inGEDHRec->otv.tv_usec;
	*outDBData << (unsigned long)inGEDHRec->ltv.tv_sec;
	*outDBData << (unsigned long)inGEDHRec->ltv.tv_usec;
	*outDBData << (unsigned long)inGEDHRec->rtv.tv_sec;
	*outDBData << (unsigned long)inGEDHRec->rtv.tv_usec;
	*outDBData << (unsigned long)inGEDHRec->mtv.tv_sec;
	*outDBData << (unsigned long)inGEDHRec->mtv.tv_usec;
	*outDBData << (unsigned long)inGEDHRec->atv.tv_sec;
	*outDBData << (unsigned long)inGEDHRec->atv.tv_usec;
	*outDBData << (unsigned long)inGEDHRec->nsrc;
	*outDBData << (unsigned long)inGEDHRec->len;
	*outDBData << (long)inGEDHRec->rsv[0];
	*outDBData << (long)inGEDHRec->rsv[1];

	for (size_t i=0; i<inGEDHRec->nsrc; i++)
	{
		*outDBData << (long)inGEDHRec->src[i].rly;
		*outDBData << (long)inGEDHRec->src[i].addr.s_addr;
	}

	if (inGEDHRec->len > 0) outDBData->WritePVoid (inGEDHRec->data, inGEDHRec->len);

	return true;
}

//----------------------------------------------------------------------------------------------------------------------------------------
// ged record to db data
//----------------------------------------------------------------------------------------------------------------------------------------
bool CGEDBackEndHDB::GEDSRcdToHDBSData (TGEDSRcd *inGEDSRec, CChunk *outDBData) const
{
	if (inGEDSRec == NULL || inGEDSRec->pkt == NULL || inGEDSRec->pkt->data == NULL || outDBData == NULL) return false;

	*outDBData << (long)inGEDSRec->queue;
	*outDBData << (long)inGEDSRec->tgt.s_addr;
	*outDBData << (unsigned long)inGEDSRec->pkt->tv.tv_sec;
	*outDBData << (unsigned long)inGEDSRec->pkt->tv.tv_usec;
	*outDBData << (long)inGEDSRec->pkt->addr.s_addr;
	*outDBData << (long)inGEDSRec->pkt->typ;
	*outDBData << (long)(inGEDSRec->pkt->req|GED_PKT_REQ_SRC_RELAY);
	*outDBData << (unsigned long)inGEDSRec->pkt->len;
	*outDBData << (long)inGEDSRec->pkt->dmy[0];
	*outDBData << (long)inGEDSRec->pkt->dmy[1];

	outDBData->WritePVoid (inGEDSRec->pkt->data, inGEDSRec->pkt->len);

	return true;
}

//----------------------------------------------------------------------------------------------------------------------------------------
// active record to history db utility
//----------------------------------------------------------------------------------------------------------------------------------------
bool CGEDBackEndHDB::Historize (TGEDARcd *inGEDARec, const bool inTTL)
{
	if (inGEDARec == NULL)
	{
		CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, 
					   CString("BERKELEY backend cannot historize NULL active data"));
		return false;
	}

	TGEDPktCfg *inGEDPktCfg=NULL; for (size_t i=m_GEDCfg.pkts.GetLength(); i>0 && !inGEDPktCfg; i--)
		if (m_GEDCfg.pkts[i-1]->type == inGEDARec->typ) inGEDPktCfg = m_GEDCfg.pkts[i-1];

	CChunk outDBDChunk, outDBKChunk; DBT outDBKey, outDBData; ::bzero(&outDBKey,sizeof(DBT)); ::bzero(&outDBData,sizeof(DBT));

	if (!GEDARcdToHDBKey (inGEDPktCfg, inGEDARec, &outDBKChunk)) return false;

	struct timeval tv; ::gettimeofday (&tv, NULL);

	outDBDChunk << (long)(GED_PKT_REQ_BKD_HISTORY|(inTTL ? GED_PKT_REQ_BKD_HST_TTL : GED_PKT_REQ_BKD_HST_PKT));
	outDBDChunk << (long)inGEDARec->typ;
	outDBDChunk << (unsigned long)m_HID++;
	outDBDChunk << (unsigned long)inGEDARec->occ;
	outDBDChunk << (unsigned long)inGEDARec->otv.tv_sec;
	outDBDChunk << (unsigned long)inGEDARec->otv.tv_usec;
	outDBDChunk << (unsigned long)inGEDARec->ltv.tv_sec;
	outDBDChunk << (unsigned long)inGEDARec->ltv.tv_usec;
	outDBDChunk << (unsigned long)inGEDARec->rtv.tv_sec;
	outDBDChunk << (unsigned long)inGEDARec->rtv.tv_usec;
	outDBDChunk << (unsigned long)inGEDARec->mtv.tv_sec;
	outDBDChunk << (unsigned long)inGEDARec->mtv.tv_usec;
	outDBDChunk << (unsigned long)tv.tv_sec;
	outDBDChunk << (unsigned long)tv.tv_usec;
	outDBDChunk << (unsigned long)inGEDARec->nsrc;
	outDBDChunk << (unsigned long)inGEDARec->len;
	outDBDChunk << (long)inGEDARec->rsv[0];
	outDBDChunk << (long)inGEDARec->rsv[1];

	for (size_t i=0; i<inGEDARec->nsrc; i++)
	{
		outDBDChunk << (long)inGEDARec->src[i].rly;
		outDBDChunk << (long)inGEDARec->src[i].addr.s_addr;
	}

	if (inGEDARec->len > 0) outDBDChunk.WritePVoid (inGEDARec->data, inGEDARec->len);

	outDBKChunk << (unsigned long)tv.tv_sec;
	outDBKChunk << (unsigned long)tv.tv_usec;

	outDBKey.data = outDBKChunk.GetChunk();
	outDBKey.size = outDBKChunk.GetSize();

	outDBData.data = outDBDChunk.GetChunk();
	outDBData.size = outDBDChunk.GetSize();

	int res; if ((res = m_DBh->put (m_DBh, NULL, &outDBKey, &outDBData, /*DB_AUTO_COMMIT*/0)) != 0)
	{
		CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, 
		"BERKELEY backend could not put history queue record; " +
		CString(::db_strerror(res)));

		return false;
	}

	m_Nh++;

	m_hInc=BUFFER_INC_8; for (size_t z=BUFFER_INC_8; z<=BUFFER_INC_65536 && ((m_Nh>>m_hInc)>0L); z++) m_hInc = (TBufferInc)z;

	return true;
}

//----------------------------------------------------------------------------------------------------------------------------------------
// TTL thread callback
//----------------------------------------------------------------------------------------------------------------------------------------
void * CGEDBackEndHDB::m_TTLTimerCB (void *inParam)
{
	CGEDBackEndHDB *inGEDBackEndHDB = reinterpret_cast <CGEDBackEndHDB *> (inParam);

	while (true)
	{
		if (inGEDBackEndHDB->Lock() != 0) break;

		time_t tm; ::time (&tm);

		unsigned long na=0L, nh=0L, ns=0L;

		int res; DBC *inDBCursor; DBT inDBKey, inDBData;

		if (inGEDBackEndHDB->m_aTTL > 0)
		{
			::bzero(&inDBKey,sizeof(DBT)); ::bzero(&inDBData,sizeof(DBT));

			if ((res = inGEDBackEndHDB->m_DBa->cursor (inGEDBackEndHDB->m_DBa, NULL, &inDBCursor, DB_WRITECURSOR)) != 0)
			{
				CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, 
				"BERKELEY backend could not get active queue cursor");

				inGEDBackEndHDB->UnLock();

				break;
			}

			while ((res = inDBCursor->c_get (inDBCursor, &inDBKey, &inDBData, DB_NEXT)) == 0)
			{
				TGEDARcd *inGEDARcd = inGEDBackEndHDB->GEDHDBDataToARcd (&inDBData);

				if ((tm-inGEDARcd->ltv.tv_sec) >= inGEDBackEndHDB->m_aTTL)
				{
					inGEDBackEndHDB->Historize (inGEDARcd, true);

					if ((res = inGEDBackEndHDB->m_DBa->del (inGEDBackEndHDB->m_DBa, NULL, &inDBKey, /*DB_AUTO_COMMIT*/0)) != 0)
					{
						CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, 
						"BERKELEY backend could not del active queue record; " +
						CString(::db_strerror(res)));
	
						::DeleteGEDRcd ((TGEDRcd*&)inGEDARcd);

						inDBCursor->c_close (inDBCursor);

						inGEDBackEndHDB->UnLock();

						break;
					}

					na++;
				}

				::DeleteGEDRcd ((TGEDRcd*&)inGEDARcd);
			}

			inDBCursor->c_close (inDBCursor);
		}

		if (inGEDBackEndHDB->m_sTTL > 0)
		{
			::bzero(&inDBKey,sizeof(DBT)); ::bzero(&inDBData,sizeof(DBT));

			if ((res = inGEDBackEndHDB->m_DBs->cursor (inGEDBackEndHDB->m_DBs, NULL, &inDBCursor, DB_WRITECURSOR)) != 0)
			{
				CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, 
				"BERKELEY backend could not get sync queue cursor");

				inGEDBackEndHDB->UnLock();

				break;
			}

			while ((res = inDBCursor->c_get (inDBCursor, &inDBKey, &inDBData, DB_NEXT)) == 0)
			{
				TGEDSRcd *inGEDSRcd = inGEDBackEndHDB->GEDHDBDataToSRcd (&inDBData);

				if ((tm-inGEDSRcd->pkt->tv.tv_sec) >= inGEDBackEndHDB->m_sTTL)
				{
					if ((res = inGEDBackEndHDB->m_DBs->del (inGEDBackEndHDB->m_DBs, NULL, &inDBKey, /*DB_AUTO_COMMIT*/0)) != 0)
					{
						CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, 
						"BERKELEY backend could not del sync queue record; " +
						CString(::db_strerror(res)));
	
						::DeleteGEDRcd ((TGEDRcd*&)inGEDSRcd);

						inDBCursor->c_close (inDBCursor);

						inGEDBackEndHDB->UnLock();

						break;
					}

					ns++;
				}

				::DeleteGEDRcd ((TGEDRcd*&)inGEDSRcd);
			}

			inDBCursor->c_close (inDBCursor);
		}

		if (inGEDBackEndHDB->m_hTTL > 0)
		{
			::bzero(&inDBKey,sizeof(DBT)); ::bzero(&inDBData,sizeof(DBT));

			if ((res = inGEDBackEndHDB->m_DBh->cursor (inGEDBackEndHDB->m_DBh, NULL, &inDBCursor, DB_WRITECURSOR)) != 0)
			{
				CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, 
				"BERKELEY backend could not get history queue cursor");

				inGEDBackEndHDB->UnLock();

				break;
			}

			while ((res = inDBCursor->c_get (inDBCursor, &inDBKey, &inDBData, DB_NEXT)) == 0)
			{
				TGEDHRcd *inGEDHRcd = inGEDBackEndHDB->GEDHDBDataToHRcd (&inDBData);

				if ((tm-inGEDHRcd->otv.tv_sec) >= inGEDBackEndHDB->m_hTTL)
				{
					if ((res = inGEDBackEndHDB->m_DBh->del (inGEDBackEndHDB->m_DBh, NULL, &inDBKey, /*DB_AUTO_COMMIT*/0)) != 0)
					{
						CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, 
						"BERKELEY backend could not del history queue record; " +
						CString(::db_strerror(res)));
	
						::DeleteGEDRcd ((TGEDRcd*&)inGEDHRcd);

						inDBCursor->c_close (inDBCursor);

						inGEDBackEndHDB->UnLock();

						break;
					}

					if (inGEDBackEndHDB->m_GEDCfg.loghloc)
						::syslog (inGEDBackEndHDB->m_GEDCfg.loghloc|GED_SYSLOG_INFO, "%s", ::GEDRcdToString(inGEDHRcd,inGEDBackEndHDB->m_GEDCfg.pkts).Get());

					::DeleteGEDRcd ((TGEDRcd*&)inGEDHRcd);

					nh++;
				}

				::DeleteGEDRcd ((TGEDRcd*&)inGEDHRcd);
			}

			inDBCursor->c_close (inDBCursor);
		}

		if (CGEDBackEndHDB::m_fTTL)
		{
			inGEDBackEndHDB->m_Na -= na;
			inGEDBackEndHDB->m_Nh -= nh;
			inGEDBackEndHDB->m_Ns -= ns;

			inGEDBackEndHDB->m_aInc=BUFFER_INC_8; for (size_t z=BUFFER_INC_8; z<=BUFFER_INC_65536 && ((inGEDBackEndHDB->m_Na>>inGEDBackEndHDB->m_aInc)>0L); z++) 
				inGEDBackEndHDB->m_aInc = (TBufferInc)z;
			inGEDBackEndHDB->m_hInc=BUFFER_INC_8; for (size_t z=BUFFER_INC_8; z<=BUFFER_INC_65536 && ((inGEDBackEndHDB->m_Nh>>inGEDBackEndHDB->m_hInc)>0L); z++) 
				inGEDBackEndHDB->m_hInc = (TBufferInc)z;
			inGEDBackEndHDB->m_sInc=BUFFER_INC_8; for (size_t z=BUFFER_INC_8; z<=BUFFER_INC_65536 && ((inGEDBackEndHDB->m_Ns>>inGEDBackEndHDB->m_sInc)>0L); z++) 
				inGEDBackEndHDB->m_sInc = (TBufferInc)z;
		}

		CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_BKD_OPERATION, "BERKELEY backend TTL droped " + CString(na) +
			" active record(s), " + CString(ns) + " sync record(s), " + CString(nh) + " history record(s), next check in " +
			CString(inGEDBackEndHDB->m_TTLTimer) + "s");

		if (inGEDBackEndHDB->UnLock() != 0) break;

		CGEDBackEndHDB::m_fTTL = true;

		::sleep (inGEDBackEndHDB->m_TTLTimer);
	}

	CGEDBackEndHDB::m_fTTL = true;

	return NULL;
}




