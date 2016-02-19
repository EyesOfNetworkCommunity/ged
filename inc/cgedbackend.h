/*****************************************************************************************************************************************
 cgedbackend.h - ged backend abstract API -

 The back end should handle multiple queues as defined by the GED_PKT_REQ_BKD_xxx flags:
	 - active
	 - history
	 - sync

 Whenever a drop is performed on the active queue of the backend, it is its responsability to fill in its associated history queue. No
 history push should be authorized from the core to the backend (whereas the drop should).

 Each of the queues should be able to handle multiple user defined packet types as given within the configuration file and presented 
 while the initialization occurs.

 The GED core specifies packets as entries (except for the recover query) and is waiting for records when it performs peek requests. So 
 the specific backend has to be aware of the expected record struct contents and fill in the struct parameters whatever its data retention 
 is.

 This abstract backend definition checks for packet types configuration changes between launches and notifies the specific backend
 whenever a packet type has been added, removed or modified. The specific backend should handle those notifications and do the 
 appropriate stuff (deleting, updating or adding packet templates). 

 As far as the backend is concerned, the specified input/output parameters are non contigous packets/records i.e. should contain a 
 specific pointer to a data section (if any).

 It is the backend responsability to ckeck packet timestamps and do the appropriate operations to store them, count them or discard them
 whenever a push or drop is requested.
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

#ifndef __CGEDBACKEND_H__
#define __CGEDBACKEND_H__

#include "gedcommon.h"
#include "cmetamodule.h"

typedef enum
{
	GED_PKT_CFG_CHANGE_CREATE=0, 
	GED_PKT_CFG_CHANGE_MODIFY, 
	GED_PKT_CFG_CHANGE_DELETE
} 
TGEDPktCfgChange;

//----------------------------------------------------------------------------------------------------------------------------------------
// backend configuration parameters and associated default values
//----------------------------------------------------------------------------------------------------------------------------------------

const static CString GEDCfgCacheFile		("cfg_cache");
const static CString GED_CFG_CACHE_FILE_DFT 	("/srv/eyesofnetwork/ged/var/cache/ged.dat");

//----------------------------------------------------------------------------------------------------------------------------------------
// abstract backend api
//----------------------------------------------------------------------------------------------------------------------------------------
class CGEDBackEnd : public CMetaModule
{
	// instanciation section
	public :

		CGEDBackEnd			();
		virtual ~CGEDBackEnd		();

	// backend commodity
	public :
	
		// lock, unlock the backend : 0 on success
		virtual int			Lock			();
		virtual int			UnLock			();

	// specific backend api definition
	public :

		// initialize the backend from the given loaded informations, this function checks the backend cache file and detects
		// whenever a packet type configuration modification has occured, if so it calls the virtual specific handler (see below);
		// this function keeps a local copy of desired ttl values too but without handling any
		virtual bool			Initialize		(const TGEDCfg &);

		// finalize i.e. ensure the backend data integrity until beeing closed
		virtual void			Finalize		();

	public :

		// push a packet into the specified queue, associate it with the given ip address (push in history should not be authorized
		// and should be a backend internal process whenever a drop occurs)
		virtual bool			Push			(const CString &inAddr, const int inQueue,
									 const TGEDPktIn *inGEDPktIn)				=0;

		// drop a packet from the specified queue, it is the backend responsability to update its history records whenever
		// its active records are impacted by the drop request, the record(s) matching the given ip addr if specified should be
		// considered only like the packet template filter whenever specified; the given ip address should be considered only
		// when performing a drop request on the handled SYNC queue
		virtual bool			Drop			(const CString &inAddr, const int inQueue, 
									 const TGEDPktIn *inGEDPktIn)				=0;

		// peek records from the specified queue given the packet search filter if any and the potential ip address
		virtual TBuffer <TGEDRcd *> 	Peek			(const CString &inAddr, const int inQueue, 
									 const TGEDPktIn *inGEDPktIn=NULL)			=0;

		// get statistics about the backend
		virtual TGEDStatRcd *		Stat			() const						=0;

	public :

		// dump the backend into one single generic records buffer (the output sequence is the active records first, then 
		// history records and then sync records whenever all requested)
		virtual TBuffer <TGEDRcd *>	Dump			(const int inQueue=GED_PKT_REQ_BKD_MASK);

		// restore the backend from the given records buffer regarding the previous dump sequence note
		virtual bool			Recover			(const TBuffer <TGEDRcd *> &inGEDRecords, 
									 void (*) (const UInt32, const UInt32, const TGEDRcd *))=0;

	protected :

		// whatever the backend retention is, retreive or save its configuration cache (default is to handle GEDCfgCacheFile)
		virtual bool			ReadCfgCache		(TKeyBuffer <UInt32, CString> &, UInt32 &);
		virtual bool			WriteCfgCache		();

	protected :

		// called by the Initialize abstract function whenever a version upgrade has to be handled by the backend
		virtual bool			NotifyVersionUpgrade	(const UInt32 inOldVersion)				=0;

		// called by the Initialize abstract function whenever a packet configuration has been modified since the previous
		// backend instanciation, the specific backend should perform any action to update, remove or delete the old definition
		virtual bool			NotifyPktCfgChange	(const long inType, const TGEDPktCfgChange)		=0;

	protected :

		// ged loaded global configuration local copy
		TGEDCfg				m_GEDCfg;

		// TTL loaded values
		unsigned long			m_aTTL;
		unsigned long			m_hTTL;
		unsigned long			m_sTTL;
		unsigned long			m_TTLTimer;

		// record buffers tunnable inc 
		TBufferInc			m_aInc;
		TBufferInc			m_hInc;
		TBufferInc			m_sInc;

	// private section
	private :

		// backend mutex services
		pthread_mutexattr_t		m_BackEndMutexAttr;
		pthread_mutex_t			m_BackEndMutex;

		// config cache file name whenever used
		CString				m_CfgCacheFile;

		// backend API generic metaclass association
		SECTION_GENERIC_METACLASS;
};

// backend API generic metaclass declaration
DECLARE_GENERIC_METACLASS ('gdbk', CGEDBackEnd, CMetaModule);

#endif
