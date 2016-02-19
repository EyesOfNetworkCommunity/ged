/*****************************************************************************************************************************************
 cgedbackend.c
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

#include "cgedbackend.h"
#include "ged.h"

//----------------------------------------------------------------------------------------------------------------------------------------
// metaclass code resolution
//----------------------------------------------------------------------------------------------------------------------------------------
RESOLVE_GENERIC_METACLASS (CGEDBackEnd);

//----------------------------------------------------------------------------------------------------------------------------------------
// constructor
//----------------------------------------------------------------------------------------------------------------------------------------
CGEDBackEnd::CGEDBackEnd    ()
	    :m_CfgCacheFile (GED_CFG_CACHE_FILE_DFT),
	     m_TTLTimer	    (GED_TTL_TIMER_DFT),
	     m_aTTL	    (0L),
	     m_hTTL	    (0L),
	     m_sTTL	    (0L),
	     m_aInc	    (BUFFER_INC_8),
	     m_hInc	    (BUFFER_INC_8),
	     m_sInc	    (BUFFER_INC_8)
{
	::pthread_mutexattr_settype (&m_BackEndMutexAttr, PTHREAD_MUTEX_RECURSIVE);
	::pthread_mutex_init (&m_BackEndMutex, /*&m_BackEndMutexAttr*/NULL); 
}

//----------------------------------------------------------------------------------------------------------------------------------------
// destructor
//----------------------------------------------------------------------------------------------------------------------------------------
CGEDBackEnd::~CGEDBackEnd ()
{ }

//----------------------------------------------------------------------------------------------------------------------------------------
// back end initialization
//----------------------------------------------------------------------------------------------------------------------------------------
bool CGEDBackEnd::Initialize (const TGEDCfg &inGEDCfg)
{
	m_GEDCfg = inGEDCfg;

	if (m_GEDCfg.bkdcfg.Contains (GEDCfgCacheFile) && const_cast<TGEDCfg&>(inGEDCfg).bkdcfg[GEDCfgCacheFile].GetLength()>0) 
		m_CfgCacheFile = *m_GEDCfg.bkdcfg[GEDCfgCacheFile][0];

	if (inGEDCfg.bkdcfg.Contains (GEDCfgTTLTimer) && const_cast<TGEDCfg&>(inGEDCfg).bkdcfg[GEDCfgTTLTimer].GetLength()>0) 
		m_TTLTimer = const_cast <TGEDCfg &> (inGEDCfg).bkdcfg[GEDCfgTTLTimer][0]->ToULong();

	if (inGEDCfg.bkdcfg.Contains (GEDCfgTTLActive) && const_cast<TGEDCfg&>(inGEDCfg).bkdcfg[GEDCfgTTLActive].GetLength()>0)
		m_aTTL = const_cast <TGEDCfg &> (inGEDCfg).bkdcfg[GEDCfgTTLActive][0]->ToULong();

	if (inGEDCfg.bkdcfg.Contains (GEDCfgTTLHistory) && const_cast<TGEDCfg&>(inGEDCfg).bkdcfg[GEDCfgTTLHistory].GetLength()>0)
		m_hTTL = const_cast <TGEDCfg &> (inGEDCfg).bkdcfg[GEDCfgTTLHistory][0]->ToULong();

	if (inGEDCfg.bkdcfg.Contains (GEDCfgTTLSync) && const_cast<TGEDCfg&>(inGEDCfg).bkdcfg[GEDCfgTTLSync].GetLength()>0)
		m_sTTL = const_cast <TGEDCfg &> (inGEDCfg).bkdcfg[GEDCfgTTLSync][0]->ToULong();

	TKeyBuffer <UInt32, CString> inLastPktHash; UInt32 inVersion=0L; if (!ReadCfgCache (inLastPktHash, inVersion))
	{
		CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, 
			CString("could not retreive config cache"));
		return false;
	}

	if (inVersion > 0L && inVersion < GED_VERSION)
	{
		CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_WARNING, GED_SYSLOG_LEV_BKD_OPERATION, 
			CString("performing backend version upgrade from " + CString(inVersion) + " to " + CString((SInt32)GED_VERSION)));

		if (!NotifyVersionUpgrade (inVersion))
		{
			CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, 
				CString("could not perform backend version upgrade from " + CString(inVersion) + " to " + CString((SInt32)GED_VERSION)));
			return false;
		}

		CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_BKD_OPERATION, 
			CString("backend version successfully upgraded from " + CString(inVersion) + " to " + CString((SInt32)GED_VERSION)));
	}

	if (inLastPktHash.GetKeys().GetLength() > 0)
	{
		if (inLastPktHash[0] != ::HashGEDPktCfg(m_GEDCfg.pkts))
		{
			CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_WARNING, GED_SYSLOG_LEV_BKD_OPERATION, 
				CString("packets configuration has changed since last execution"));

			for (size_t i=0; i<m_GEDCfg.pkts.GetLength(); i++)
			{
				if (inLastPktHash.Contains(m_GEDCfg.pkts[i]->type) &&
				    inLastPktHash[m_GEDCfg.pkts[i]->type] != ::HashGEDPktCfg(m_GEDCfg.pkts[i]))
					if (!NotifyPktCfgChange (m_GEDCfg.pkts[i]->type, GED_PKT_CFG_CHANGE_MODIFY)) return false;
			}

			for (size_t i=1; i<inLastPktHash.GetKeys().GetLength(); i++)
			{
				bool found=false; for (size_t j=0; j<m_GEDCfg.pkts.GetLength() && !found; j++)
					found = m_GEDCfg.pkts[j]->type == *inLastPktHash.GetKeys()[i];
				if (!found) if (!NotifyPktCfgChange (*inLastPktHash.GetKeys()[i], GED_PKT_CFG_CHANGE_DELETE)) return false;
			}
		}
	}

	for (size_t i=0; i<m_GEDCfg.pkts.GetLength(); i++)
	{
		bool found=false; for (size_t j=1; j<inLastPktHash.GetKeys().GetLength() && !found; j++)
			found = m_GEDCfg.pkts[i]->type == *inLastPktHash.GetKeys()[j];
		if (!found) if (!NotifyPktCfgChange (m_GEDCfg.pkts[i]->type, GED_PKT_CFG_CHANGE_CREATE)) return false;
	}

	if (!WriteCfgCache())
	{
		CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, CString("could not save config cache"));
		return false;
	}

	return true;
} 

//----------------------------------------------------------------------------------------------------------------------------------------
// backend finalization
//----------------------------------------------------------------------------------------------------------------------------------------
void CGEDBackEnd::Finalize ()
{
	/*
 	 the config cache has been written with the backend initialization, do not write it again...

	if (!WriteCfgCache())
		CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, CString("could not save config cache"));
	*/

	::pthread_mutexattr_destroy (&m_BackEndMutexAttr);
	::pthread_mutex_destroy (&m_BackEndMutex);
}

//----------------------------------------------------------------------------------------------------------------------------------------
// backend lock
//----------------------------------------------------------------------------------------------------------------------------------------
int CGEDBackEnd::Lock ()
{
#ifdef __GED_DEBUG_SEM__
	CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_DEBUG, "SEM CGEDBackEnd::Lock() " + CString(::pthread_self()));
#endif
	int res = ::pthread_mutex_lock (&m_BackEndMutex);
#ifdef __GED_DEBUG_SEM__
	CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_DEBUG, "SEM CGEDBackEnd::Lock() done [" + CString((long)res) + "] " + 
				   CString(::pthread_self()));
#endif
	return res;
}

//----------------------------------------------------------------------------------------------------------------------------------------
// backend unlock
//----------------------------------------------------------------------------------------------------------------------------------------
int CGEDBackEnd::UnLock ()
{
#ifdef __GED_DEBUG_SEM__
	CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_DEBUG, "SEM CGEDBackEnd::UnLock() " + CString(::pthread_self()));
#endif
	int res = ::pthread_mutex_unlock (&m_BackEndMutex);
#ifdef __GED_DEBUG_SEM__
	CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_DEBUG, "SEM CGEDBackEnd::UnLock() done [" + CString((long)res) + "] " + 
				   CString(::pthread_self()));
#endif
	return res;
}

//----------------------------------------------------------------------------------------------------------------------------------------
// retreive backend config cache
//----------------------------------------------------------------------------------------------------------------------------------------
bool CGEDBackEnd::ReadCfgCache (TKeyBuffer <UInt32, CString> &outLastPktHash, UInt32 &outVersion)
{
	if (CFile::Exists (m_CfgCacheFile))
	{
		CFile inFile (m_CfgCacheFile, CString("rb"));
		if (inFile.GetSize() >= 4)
		{
			char *inData = new char [inFile.GetSize()];
			inFile.Read (inData, inFile.GetSize(), 1);
			CChunk inChunk (inData, inFile.GetSize());
			UInt32 n; inChunk >> outVersion;
			if (outVersion < (GED_MAJOR*10000))
			{
				n = outVersion;
				outVersion = 0L;
			}
			else 
				inChunk >> n; 
			char *inHash=NULL; inChunk >> inHash;
			outLastPktHash[0] = CString(inHash);
			delete [] inHash; inHash = NULL;
			for (size_t i=0; i<n; i++)
			{
				UInt32 t; inChunk >> t;
				char *inHash=NULL; inChunk >> inHash;
				outLastPktHash[t] = CString(inHash);
				delete [] inHash; inHash = NULL;
			}
		}
	}

	return true;
}

//----------------------------------------------------------------------------------------------------------------------------------------
// save backend config cache
//----------------------------------------------------------------------------------------------------------------------------------------
bool CGEDBackEnd::WriteCfgCache ()
{
	CChunk outChunk;

	outChunk << (UInt32)GED_VERSION;
	outChunk << (UInt32)m_GEDCfg.pkts.GetLength();
	outChunk << ::HashGEDPktCfg(m_GEDCfg.pkts).Get();
	for (size_t i=0; i<m_GEDCfg.pkts.GetLength(); i++)
	{
		outChunk << (UInt32)m_GEDCfg.pkts[i]->type;
		outChunk << ::HashGEDPktCfg(m_GEDCfg.pkts[i]).Get();
	}

	CFile outFile (m_CfgCacheFile, CString("w+b"));
	if (outFile.GetFile() == NULL)
		CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, "could not write backend config cache to " +
					   m_CfgCacheFile + ", unable to stat");
	else
		outFile.Write (outChunk.GetChunk(), outChunk.GetSize(), 1);

	return true;
}

//----------------------------------------------------------------------------------------------------------------------------------------
// backend global dump
//----------------------------------------------------------------------------------------------------------------------------------------
TBuffer <TGEDRcd *> CGEDBackEnd::Dump (const int inQueue)
{
	TBuffer <TGEDRcd *> outGEDRcds; 

	outGEDRcds.SetInc (max(max(m_aInc,m_hInc),m_sInc));

	if (inQueue & GED_PKT_REQ_BKD_HISTORY) outGEDRcds += Peek (CString(), GED_PKT_REQ_BKD_HISTORY, NULL);
	if (inQueue & GED_PKT_REQ_BKD_ACTIVE)  outGEDRcds += Peek (CString(), GED_PKT_REQ_BKD_ACTIVE,  NULL);
	if (inQueue & GED_PKT_REQ_BKD_SYNC)    outGEDRcds += Peek (CString(), GED_PKT_REQ_BKD_SYNC,    NULL);

	return outGEDRcds;
}


