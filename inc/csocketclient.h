/*****************************************************************************************************************************************
 csocketclient.h - client socket definition -
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

#ifndef __CSOCKETCLIENT_H__
#define __CSOCKETCLIENT_H__

#include "csocket.h"

//----------------------------------------------------------------------------------------------------------------------------------------
// socket listener
//----------------------------------------------------------------------------------------------------------------------------------------
class CSocketClientListener : public CSocketListener
{
	// instanciation section
	public :
		
		CSocketClientListener		();
		virtual ~CSocketClientListener	() =0;

	// abstract socket events notification
	public :

		// called once just after the socket has reached its target by opening a connection while performing a ::connect request;
		// from this point, the reception callback is not linked yet so the handler might perform send and recv requests by sequence
		virtual void			OnConnect		(CObject *inSender, void *inParam)				{ }

		// called whenever the socket received some data (this section runs in an independant thread "B")
		virtual void			OnReceive		(CObject *inSender, const void *inData, const int inLen)	{ }

		// called whenever the socket is disconnected (this section runs in an independant thread "B")
		virtual void			OnDisconnect		(CObject *inSender)						{ }

		// metaclass association
		SECTION_GENERIC_METACLASS;
};

// metaclass declaration
DECLARE_GENERIC_METACLASS ('_sck', CSocketClientListener, CSocketListener);

//----------------------------------------------------------------------------------------------------------------------------------------
// client socket definition
//----------------------------------------------------------------------------------------------------------------------------------------
class CSocketClient : public CSocket
{
	// instanciation section
	public :

		CSocketClient			(const CString &inAddress, const UInt16 inPort, const CString &inBind=CString(),
						 const CSocketClientListener *inListener=NULL, void *inParam=NULL) THROWABLE;
		CSocketClient			(const CString &inFile, const CSocketClientListener *inListener=NULL, void *inParam=NULL) THROWABLE;
		virtual ~CSocketClient		();

	// overloading
	public :

		// expected listener type : CSockeClienttListener
		virtual const CMetaClass *	ListenerMustBe		() const;
		virtual bool			AssignListener		(const CObjectListener *inListener);
                virtual CObjectListener *	RemoveListener		();

		// call an endless wait for data reception on the instance
		virtual void 			Wait			();

		// close the socket
		virtual void			Close			();

	// protected section
	protected :

		struct sockaddr *               m_BindAddr;

		pthread_t			m_ReceiveTh;
		static void *			m_ReceiveCallBack	(void *);

		SECTION_CAPSULE_METACLASS;
};

// metaclass declaration
DECLARE_GENERIC_METACLASS ('cskt', CSocketClient, CSocket);

#endif

