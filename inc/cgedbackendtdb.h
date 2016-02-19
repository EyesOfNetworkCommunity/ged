/*****************************************************************************************************************************************
 cgedbackendtdb.h - ged berkeley btree backend -
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

#ifndef __CGEDACKENDTDB_H__
#define __CGEDACKENDTDB_H__

#include "cgedbackendbdb.h"

//----------------------------------------------------------------------------------------------------------------------------------------
// berkeley backend btree api
//----------------------------------------------------------------------------------------------------------------------------------------
class CGEDBackEndTDB : public CGEDBackEndBDB
{
	public :

		CGEDBackEndTDB			();
		virtual ~CGEDBackEndTDB		();

	public :

		virtual bool			Initialize		(const TGEDCfg &);
		virtual void			Finalize		();

	public :

		virtual bool			Push			(const CString &, const int, const TGEDPktIn *);
		virtual bool			Drop			(const CString &, const int, const TGEDPktIn *);
		virtual TBuffer <TGEDRcd *>	Peek			(const CString &, const int, const TGEDPktIn *);

		virtual bool			Recover			(const TBuffer <TGEDRcd *> &inGEDRecords, void (*) (const UInt32, const UInt32));

	protected :

		virtual bool			NotifyPktCfgChange	(const long, const TGEDPktCfgChange);

	protected :

		virtual bool			BDBOpen			(const TGEDCfg &);

	protected :

		static int 			m_TDBCmpA		(DB *, const DBT *, const DBT *);
		static int 			m_TDBCmpH		(DB *, const DBT *, const DBT *);
		static int 			m_TDBCmpS		(DB *, const DBT *, const DBT *);

		bool				GEDPktInToTDBKey	(const TGEDPktCfg *, const TGEDPktIn *, CChunk *) const;

		bool				GEDPktInToTDBAData	(const TGEDPktIn  *, const struct in_addr *, CChunk *) const;
		
	protected :

		pthread_t			m_TTLTimerTh;
		static void *			m_TTLTimerCB		(void *);

		static volatile bool		m_fTTL;

		SECTION_DYNAMIC_METACLASS;
};

DECLARE_DYNAMIC_METACLASS ('bktd', CGEDBackEndTDB, CGEDBackEndBDB);

#endif


