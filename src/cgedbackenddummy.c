/*****************************************************************************************************************************************
 cgedbackenddummy.c - ged core debug and test purpose -
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

#include "cgedbackenddummy.h"

//----------------------------------------------------------------------------------------------------------------------------------------
// metaclass code resolution
//----------------------------------------------------------------------------------------------------------------------------------------
RESOLVE_DYNAMIC_METACLASS (CGEDBackEndDummy);
DECLARE_METAMODULE_EXPORT (CGEDBackEndDummy);

//----------------------------------------------------------------------------------------------------------------------------------------
// backend instanciation
//----------------------------------------------------------------------------------------------------------------------------------------
CGEDBackEndDummy::CGEDBackEndDummy ()
{ }

//----------------------------------------------------------------------------------------------------------------------------------------
// backend deletion
//----------------------------------------------------------------------------------------------------------------------------------------
CGEDBackEndDummy::~CGEDBackEndDummy ()
{ }

//----------------------------------------------------------------------------------------------------------------------------------------
// backend initialization
//----------------------------------------------------------------------------------------------------------------------------------------
bool CGEDBackEndDummy::Initialize (const TGEDCfg &inGEDCfg)
{
	CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_BKD_OPERATION, 
				   CString("DUMMY backend loading, NO DATA RETENTION AT ALL"));

	return CGEDBackEnd::Initialize (inGEDCfg);
}

//----------------------------------------------------------------------------------------------------------------------------------------
// finalize
//----------------------------------------------------------------------------------------------------------------------------------------
void CGEDBackEndDummy::Finalize ()
{
	CGEDBackEnd::Finalize();
}

//----------------------------------------------------------------------------------------------------------------------------------------
// backend packet push handler
//----------------------------------------------------------------------------------------------------------------------------------------
bool CGEDBackEndDummy::Push (const CString &inAddr, const int inQueue, const TGEDPktIn *inGEDPktIn)
{
	CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, CString("DUMMY backend push : no data retention"));

	return true;
}

//----------------------------------------------------------------------------------------------------------------------------------------
// backend packet drop handler
//----------------------------------------------------------------------------------------------------------------------------------------
bool CGEDBackEndDummy::Drop (const CString &inAddr, const int inQueue, const TGEDPktIn *inGEDPktIn)
{
	CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, CString("DUMMY backend drop : no data retention"));
	
	return true;
}

//----------------------------------------------------------------------------------------------------------------------------------------
// backend record peek handler
//----------------------------------------------------------------------------------------------------------------------------------------
TBuffer <TGEDRcd *> CGEDBackEndDummy::Peek (const CString &inAddr, const int inQueue, const TGEDPktIn *inGEDPktIn)
{
	TBuffer <TGEDRcd *> outGEDRcds;

	CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, CString("DUMMY backend peek : no data retention"));

	return outGEDRcds;
}

//----------------------------------------------------------------------------------------------------------------------------------------
// backend stat
//----------------------------------------------------------------------------------------------------------------------------------------
TGEDStatRcd * CGEDBackEndDummy::Stat () const
{
	TGEDStatRcd *outGEDStatRcd = new TGEDStatRcd; ::bzero(outGEDStatRcd,sizeof(TGEDStatRcd));

	outGEDStatRcd->queue = GED_PKT_REQ_BKD_STAT;
	outGEDStatRcd->id    = classtag_cast(this);
	outGEDStatRcd->vrs   = GED_VERSION;
	outGEDStatRcd->nta   = 0L;
	outGEDStatRcd->nts   = 0L;
	outGEDStatRcd->nth   = 0L;
	outGEDStatRcd->ntr   = 0L;

	return outGEDStatRcd;
}

//----------------------------------------------------------------------------------------------------------------------------------------
// config cache retreival
//----------------------------------------------------------------------------------------------------------------------------------------
bool CGEDBackEndDummy::ReadCfgCache (TKeyBuffer <UInt32, CString> &, UInt32 &)
{
	CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, CString("DUMMY backend does not handle config cache retreival"));

	return true;
}

//----------------------------------------------------------------------------------------------------------------------------------------
// config cache save
//----------------------------------------------------------------------------------------------------------------------------------------
bool CGEDBackEndDummy::WriteCfgCache ()
{
	CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, CString("DUMMY backend does not handle config cache saving"));

	return true;
}

//----------------------------------------------------------------------------------------------------------------------------------------
// version upgrade notification
//----------------------------------------------------------------------------------------------------------------------------------------
bool CGEDBackEndDummy::NotifyVersionUpgrade (const UInt32 inOldVersion)
{
	CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, CString("DUMMY backend does not handle upgrade requests !"));

	return true;
}

//----------------------------------------------------------------------------------------------------------------------------------------
// configuration change notification
//----------------------------------------------------------------------------------------------------------------------------------------
bool CGEDBackEndDummy::NotifyPktCfgChange (const long inType, const TGEDPktCfgChange inGEDPktCfgChange)
{
	CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, CString("DUMMY backend does not handle packets configurations !"));

	return true;
}

//----------------------------------------------------------------------------------------------------------------------------------------
// dummy recover
//----------------------------------------------------------------------------------------------------------------------------------------
bool CGEDBackEndDummy::Recover (const TBuffer <TGEDRcd *> &, void (*) (const UInt32, const UInt32, const TGEDRcd *))
{
	UInt32 na=0L, ns=0L, nh=0L;

	CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, CString("DUMMY backend does not handle recover requests !"));

	CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_BKD_OPERATION, 
				   "DUMMY backend recovered " + 
				   CString(na) + " active queue records, " + 
				   CString(nh) + " history queue records, " +
				   CString(ns) + " sync queue records");

	return false;
}



