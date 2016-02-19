/*****************************************************************************************************************************************
 csocketserver.h - server socket definition -
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

#ifndef __CSOCKETSERVER_H__
#define __CSOCKETSERVER_H__

#include "csocket.h"

//----------------------------------------------------------------------------------------------------------------------------------------
// server socket listener
//----------------------------------------------------------------------------------------------------------------------------------------
class CSocketServerListener : public CSocketListener
{
	// instanciation section
	public :

		CSocketServerListener		();
		virtual ~CSocketServerListener	() =0;

	// server socket events notification
	public :

		// called whenever the server is waiting for a new connection
		virtual void			OnAccept		(CObject *inSender) { }

		// called whenever a client has requested a new connection (runs in an independant thread "B")
		virtual void			OnConnect		(CObject *inSender, const CString &inAddr, const UInt16 inPort,
									 const int inDesc, SSL *inSSL, bool &ioAccept, void *&outParam) { }

		// called whenever the socket received some data (this section runs in an independant thread "C")
		virtual void			OnReceive		(CObject *inSender, const CString &inAddr, const UInt16 inPort,
									 const int inDesc, SSL *inSSL, const void *inData,  
									 const int inLen, void *&inParam) { }

		// called whenever a client has been disconnected (runs in an independant thread "C")
		virtual void			OnDisconnect		(CObject *inSender, const CString &inAddr, const UInt16 inPort,
									 const int inDesc, SSL *inSSL, void *&ioParam) { }

		// metaclass association
		SECTION_GENERIC_METACLASS;
};

// metaclass declaration
DECLARE_GENERIC_METACLASS ('_ssk', CSocketServerListener, CSocketListener);

//----------------------------------------------------------------------------------------------------------------------------------------
// server socket definition
//----------------------------------------------------------------------------------------------------------------------------------------
class CSocketServer : public CSocket
{
	// instanciation section
	public :

		CSocketServer			(const CString &inAddress, const UInt16 inPort, const CSocketServerListener *inListener=NULL) THROWABLE;
		CSocketServer			(const CString &inFile, const CSocketServerListener *inListener=NULL) THROWABLE;
		virtual ~CSocketServer		();

	// overloading
	public :

		// expected listener type : CSocketServerListener
		virtual const CMetaClass *	ListenerMustBe		() const;
		virtual bool			AssignListener		(const CObjectListener *inListener);
                virtual CObjectListener *	RemoveListener		();

		// call an endless wait of client connections / data reception on the instance
		virtual void 			Wait			();

		// close the socket
		virtual void			Close			();

	// protected section
	protected :

		pthread_t			m_ListenTh;
		static void *			m_ListenCallBack	(void *);
		
		struct TReceiveCBData
		{
			CSocketServer *		inst;
			int			desc;
			CString			addr;
			UInt16			port;
			void *			param;
		};
		static void *			m_ReceiveCallBack	(void *);

		CString				m_File;

		SECTION_CAPSULE_METACLASS;
};

// metaclass declaration
DECLARE_GENERIC_METACLASS ('sskt', CSocketServer, CSocket);

#endif

