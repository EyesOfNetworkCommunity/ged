/*****************************************************************************************************************************************
 csocketsslclient.h - client ssl socket definition -
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

#ifndef __CSOCKETSSLCLIENT_H__
#define __CSOCKETSSLCLIENT_H__

#include "csocketclient.h"

//----------------------------------------------------------------------------------------------------------------------------------------
// client ssl socket listener
//----------------------------------------------------------------------------------------------------------------------------------------
class CSocketSSLClientListener : public CSocketClientListener
{
	// instanciation section
	public :
		
		CSocketSSLClientListener	 ();
		virtual ~CSocketSSLClientListener() =0;

	// abstract socket events notification
	public :

		// called once just after the socket has reached its target by opening a ssl connection while performing a ::SSL_connect 
		// request; from this point, the reception callback is not linked yet so the handler might perform send and recv requests 
		// by sequence
		virtual void			OnSSLConnect		(CObject *inSender, void *inParam)				{ }

		// metaclass association
		SECTION_GENERIC_METACLASS;
};

// metaclass declaration
DECLARE_GENERIC_METACLASS ('_slc', CSocketSSLClientListener, CSocketClientListener);


//----------------------------------------------------------------------------------------------------------------------------------------
// client ssl socket definition
//----------------------------------------------------------------------------------------------------------------------------------------
class CSocketSSLClient : public CSocketClient
{
	// instanciation section
	public :

		CSocketSSLClient		(const CString &inAddress, const UInt16 inPort, const CString &inSSLCAFile, 
						 const CString &inSSLCertFile, const CString &inSSLKeyFile, const bool inSSLReqPeer,
						 const CString &inSSLCiphers, const CString &inBind=CString(), const CSocketSSLClientListener *inListener=NULL,
						 SSLInfoCallBack =NULL, SSLVerifyCallBack =NULL, void *inParam=NULL) THROWABLE;
		virtual ~CSocketSSLClient	();

	// overloading
	public :

		// listener affectation
		virtual const CMetaClass *	ListenerMustBe		() const;
		virtual bool			AssignListener		(const CObjectListener *inListener);

		// get the ssl struct
		SSL *				GetSSL			() const;

		// close the socket
		virtual void 			Close			();

		// data reception/sending over ssl 
		virtual int			Receive			(void *, const int);
		virtual int			Send			(const void *, const int);

	// protected section
	protected :

		CString				m_SSLCAFile;
		CString				m_SSLCertFile;
		CString				m_SSLKeyFile;
		bool				m_SSLReqPeer;
		CString				m_SSLCiphers;

		SSL_CTX *			m_SSLCTX;
		SSL *				m_SSL;

		static void *			m_ReceiveSSLCallBack	(void *);
	
		SECTION_CAPSULE_METACLASS;
};

// metaclass declaration
DECLARE_GENERIC_METACLASS ('cslk', CSocketSSLClient, CSocketClient);

#endif

