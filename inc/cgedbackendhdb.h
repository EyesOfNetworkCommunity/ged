/*****************************************************************************************************************************************
 cgedbackendhdb.h - ged berkeley hash backend -
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

#ifndef __CGEDACKENDHDB_H__
#define __CGEDACKENDHDB_H__

#include "cgedbackendbdb.h"

//----------------------------------------------------------------------------------------------------------------------------------------
// berkeley backend hash api
//----------------------------------------------------------------------------------------------------------------------------------------
class CGEDBackEndHDB : public CGEDBackEndBDB
{
	public :

		CGEDBackEndHDB			();
		virtual ~CGEDBackEndHDB		();

	public :

		virtual bool			Initialize		(const TGEDCfg &);
		virtual void			Finalize		();

		virtual bool			Push			(const CString &, const int, const TGEDPktIn *);
		virtual bool			Drop			(const CString &, const int, const TGEDPktIn *);
		virtual TBuffer <TGEDRcd *>	Peek			(const CString &, const int, const TGEDPktIn *);

		virtual bool			Recover			(const TBuffer <TGEDRcd *> &inGEDRecords, void (*) (const UInt32, const UInt32, const TGEDRcd *));

	protected :

		virtual bool                    NotifyVersionUpgrade    (const UInt32 inOldVersion);
		virtual bool			NotifyPktCfgChange	(const long, const TGEDPktCfgChange);

	protected :

		bool				GEDPktInToHDBKey	(const TGEDPktCfg *, const TGEDPktIn *, CChunk *) const;

		bool				GEDARcdToHDBKey		(const TGEDPktCfg *, const TGEDARcd  *, CChunk *) const;
		bool				GEDHRcdToHDBKey		(const TGEDPktCfg *, const TGEDHRcd  *, CChunk *) const;
		bool				GEDSRcdToHDBKey		(const TGEDPktCfg *, const TGEDSRcd  *, CChunk *) const;

		bool				GEDPktInToHDBAData	(const TGEDPktIn  *, const struct in_addr *, CChunk *) const;
		bool				GEDPktInToHDBSData	(const TGEDPktIn  *, const struct in_addr *, CChunk *) const;

		TGEDARcd *			GEDHDBDataToARcd	(DBT *) const;
		TGEDSRcd *			GEDHDBDataToSRcd	(DBT *) const;
		TGEDHRcd *			GEDHDBDataToHRcd	(DBT *) const;

		bool				GEDARcdToHDBAData	(TGEDARcd *, CChunk *) const;
		bool				GEDHRcdToHDBHData	(TGEDHRcd *, CChunk *) const;
		bool				GEDSRcdToHDBSData	(TGEDSRcd *, CChunk *) const;

		bool				Historize		(TGEDARcd *, const bool =false);

	protected :

		unsigned long			m_HID;

	protected :

		pthread_t			m_TTLTimerTh;
		static void *			m_TTLTimerCB		(void *);

		static volatile bool		m_fTTL;

		SECTION_DYNAMIC_METACLASS;
};

DECLARE_DYNAMIC_METACLASS ('bkhd', CGEDBackEndHDB, CGEDBackEndBDB);

#endif


