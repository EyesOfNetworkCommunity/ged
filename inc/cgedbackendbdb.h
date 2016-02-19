/*****************************************************************************************************************************************
 cgedbackendbdb.h - ged berkeley hash/btree backend abstract -
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

#ifndef __CGEDACKENDBDB_H__
#define __CGEDACKENDBDB_H__

#include "ged.h"

#include <db.h>

//----------------------------------------------------------------------------------------------------------------------------------------
// backend specific handled parameters
//----------------------------------------------------------------------------------------------------------------------------------------
const static CString GEDCfgBdbHome		("db_home");
const static CString GEDCfgBdbCacheSize		("db_cachesize");
const static CString GEDCfgBdbDataDir		("db_data_dir");
const static CString GEDCfgBdbLgDir		("db_lg_dir");
const static CString GEDCfgBdbLgMax		("db_lg_max");
const static CString GEDCfgBdbLgRegionMax	("db_lg_regionmax");
const static CString GEDCfgBdbLgBSize		("db_lg_bsize");
const static CString GEDCfgBdbTmpDir		("db_tmp_dir");

//----------------------------------------------------------------------------------------------------------------------------------------
// backend constants
//----------------------------------------------------------------------------------------------------------------------------------------
const static CString GED_CFG_DFT		("c.db");
const static CString GED_ADB_DFT 		("a.db");
const static CString GED_HDB_DFT 		("h.db");
const static CString GED_SDB_DFT 		("s.db");

//----------------------------------------------------------------------------------------------------------------------------------------
// berkeley backend abstract api
//----------------------------------------------------------------------------------------------------------------------------------------
class CGEDBackEndBDB : public CGEDBackEnd
{
	public :

		CGEDBackEndBDB			(const DBTYPE inDBType, const UInt32 inDBEnvFlags);
		virtual ~CGEDBackEndBDB		() =0;

	public :

		virtual bool			Initialize		(const TGEDCfg &);
		virtual void			Finalize		();

		virtual TGEDStatRcd *           Stat                    () const;

	protected :

		virtual bool			ReadCfgCache		(TKeyBuffer <UInt32, CString> &, UInt32 &);
		virtual bool			WriteCfgCache		();

	protected :

		virtual bool			BDBCreate		(const TGEDCfg &);
		virtual bool			BDBOpen			(const TGEDCfg &);
		virtual bool			BDBClose		();

	protected :

		DB_ENV *			m_DBENV;

		DB *				m_DBc;
		DB *				m_DBa;
		DB *				m_DBh;
		DB *				m_DBs;

	protected :

		unsigned long			m_Na;
		unsigned long			m_Nh;
		unsigned long			m_Ns;
		unsigned long			m_Nr;

	private :

		UInt32				m_DBEnvFlags;
		DBTYPE				m_DBType;

		SECTION_GENERIC_METACLASS;
};

DECLARE_GENERIC_METACLASS ('bkdb', CGEDBackEndBDB, CGEDBackEnd);

#endif
