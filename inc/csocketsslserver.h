/*****************************************************************************************************************************************
 csocketsslserver.h - ssl server socket definition -
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

#ifndef __CSOCKETSSLSERVER_H__
#define __CSOCKETSSLSERVER_H__

#include "csocketserver.h"

//----------------------------------------------------------------------------------------------------------------------------------------
// ssl server socket listener
//----------------------------------------------------------------------------------------------------------------------------------------
class CSocketSSLServerListener : public CSocketServerListener
{
	// instanciation section
	public :

		CSocketSSLServerListener	 ();
		virtual ~CSocketSSLServerListener() =0;

	// server socket events notification
	public :

		// metaclass association
		SECTION_GENERIC_METACLASS;
};

// metaclass declaration
DECLARE_GENERIC_METACLASS ('_sls', CSocketSSLServerListener, CSocketServerListener);

//----------------------------------------------------------------------------------------------------------------------------------------
// ssl socket server 
//----------------------------------------------------------------------------------------------------------------------------------------
class CSocketSSLServer : public CSocketServer
{
	// instanciation section
	public :

		CSocketSSLServer		(const CString &inAddress, const UInt16 inPort, const CString &inSSLCAFile, 
						 const CString &inSSLCertFile, const CString &inSSLKeyFile, const bool inSSLReqPeer,
						 const CString &inSSLCiphers, const CString &inSSLDHParam,
						 const CSocketSSLServerListener *inListener=NULL, SSLInfoCallBack =NULL,
						 SSLVerifyCallBack =NULL) THROWABLE;
		virtual ~CSocketSSLServer	();

	// overloading
	public :

		// listener affectation
		virtual const CMetaClass *	ListenerMustBe		() const;
		virtual bool 			AssignListener 		(const CObjectListener *);

		// close the socket
		virtual void			Close			();

	// protected section
	protected :

		CString				m_SSLCAFile;
		CString				m_SSLCertFile;
		CString				m_SSLKeyFile;
		bool				m_SSLReqPeer;
		CString				m_SSLCiphers;
		CString				m_SSLDHParam;

		SSL_CTX *			m_SSLCTX;

		static void *			m_ListenSSLCallBack	(void *);

		struct TReceiveSSLCBData
		{
			CSocketServer *		inst;
			int			desc;
			SSL *			ssl;
			CString			addr;
			UInt16			port;
			void *			param;
		};
		static void *			m_ReceiveSSLCallBack	(void *);

		SECTION_CAPSULE_METACLASS;
};

// metaclass declaration
DECLARE_GENERIC_METACLASS ('sslk', CSocketSSLServer, CServerSocket);

#endif
