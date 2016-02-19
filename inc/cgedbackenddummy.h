/*****************************************************************************************************************************************
 cgedbackenddummy.h - ged core debug and test purpose -
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

#ifndef __CGEDACKENDDUMMY_H__
#define __CGEDACKENDDUMMY_H__

#include "ged.h"

//----------------------------------------------------------------------------------------------------------------------------------------
// dummy backend specific api
//----------------------------------------------------------------------------------------------------------------------------------------
class CGEDBackEndDummy : public CGEDBackEnd
{
	public :

		CGEDBackEndDummy		();
		virtual ~CGEDBackEndDummy	();

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

		SECTION_DYNAMIC_METACLASS;
};

DECLARE_DYNAMIC_METACLASS ('bkdy', CGEDBackEndDummy, CGEDBackEnd);

#endif
