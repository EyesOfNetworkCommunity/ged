/*****************************************************************************************************************************************
 csocketsslserver.c
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

#include "csocketsslserver.h"

//----------------------------------------------------------------------------------------------------------------------------------------
// metaclass resolution
//----------------------------------------------------------------------------------------------------------------------------------------
RESOLVE_GENERIC_METACLASS (CSocketSSLServerListener);

//----------------------------------------------------------------------------------------------------------------------------------------
// constructor
//----------------------------------------------------------------------------------------------------------------------------------------
CSocketSSLServerListener::CSocketSSLServerListener ()
{ }

//----------------------------------------------------------------------------------------------------------------------------------------
// destructor
//----------------------------------------------------------------------------------------------------------------------------------------
CSocketSSLServerListener::~CSocketSSLServerListener ()
{ }

//----------------------------------------------------------------------------------------------------------------------------------------
// metaclass resolution
//----------------------------------------------------------------------------------------------------------------------------------------
RESOLVE_CAPSULE_METACLASS (CSocketSSLServer);

//----------------------------------------------------------------------------------------------------------------------------------------
// constructor
//----------------------------------------------------------------------------------------------------------------------------------------
CSocketSSLServer::CSocketSSLServer (const CString &inAddress, const UInt16 inPort, const CString &inSSLCAFile, 
		 	 	    const CString &inSSLCertFile, const CString &inSSLKeyFile, const bool inSSLReqPeer,
				    const CString &inSSLCiphers, const CString &inSSLDHParam, const CSocketSSLServerListener *inListener,
				    SSLInfoCallBack inSSLInfoCallBack, SSLVerifyCallBack inSSLVerifyCallBack)
		 :CSocketServer	   (inAddress, inPort, NULL),
		  m_SSLCAFile	   (inSSLCAFile),
		  m_SSLCertFile	   (inSSLCertFile),
		  m_SSLKeyFile	   (inSSLKeyFile),
		  m_SSLReqPeer	   (inSSLReqPeer),
		  m_SSLCiphers	   (inSSLCiphers),
		  m_SSLDHParam	   (inSSLDHParam),
		  m_SSLCTX	   (NULL)
{
	// listener pointer copy
	m_Listener = const_cast <CSocketSSLServerListener *> (inListener);

	// allocate the context
	m_SSLCTX = ::SSL_CTX_new(::SSLv23_method());

	// set the ssl verification flag
	::SSL_CTX_set_verify (m_SSLCTX, m_SSLReqPeer?SSL_VERIFY_PEER|SSL_VERIFY_FAIL_IF_NO_PEER_CERT:SSL_VERIFY_NONE, inSSLVerifyCallBack);
	::SSL_CTX_set_verify_depth (m_SSLCTX, 1);

	// check a cipher suite is provided
	if (m_SSLCiphers != CString() && !::SSL_CTX_set_cipher_list (m_SSLCTX, m_SSLCiphers.Get()))
		throw new CException ("SSL : could not load cipher list " + m_SSLCiphers);

	// check a CA is provided
	if (m_SSLCAFile != CString())
	{
		CString inFile (*m_SSLCAFile.Cut(CString("/"))[m_SSLCAFile.Cut(CString("/")).GetLength()-1]);
		CString inPath (m_SSLCAFile - ("/" + inFile));

		if (!::SSL_CTX_load_verify_locations (m_SSLCTX, m_SSLCAFile.Get(), inPath.Get()) ||  
		    !::SSL_CTX_set_default_verify_paths (m_SSLCTX))
			throw new CException ("SSL : could not load verify locations " + inPath + "/" + inFile);

		STACK_OF(X509_NAME) *inCAList = ::SSL_load_client_CA_file (m_SSLCAFile.Get());

		if (!::SSL_add_dir_cert_subjects_to_stack (inCAList, inPath.Get()))
                {
			::sk_X509_NAME_free (inCAList);
			inCAList = NULL;
		}

		if (inCAList == NULL) throw new CException ("SSL : could not load client CA list " + inPath + "/" + inFile);

		::SSL_CTX_set_client_CA_list (m_SSLCTX, inCAList);
	}

	// check a certificate is provided
	if (m_SSLCertFile != CString() && !::SSL_CTX_use_certificate_file (m_SSLCTX, m_SSLCertFile.Get(), SSL_FILETYPE_PEM))
		throw new CException ("SSL : could not use certificate file " + m_SSLCertFile);

	// check a key is provided
	if (m_SSLKeyFile != CString() && !::SSL_CTX_use_PrivateKey_file (m_SSLCTX, m_SSLKeyFile.Get(), SSL_FILETYPE_PEM))
		throw new CException ("SSL : could not use key file " + m_SSLKeyFile);

	// chek private key
	if ((m_SSLCertFile != CString() || m_SSLKeyFile != CString()) && !::SSL_CTX_check_private_key (m_SSLCTX))
		throw new CException (CString("SSL : private key mismatch"));

	// check a dhparam is provided
	if (m_SSLDHParam != CString())
	{
		FILE *f = ::fopen (m_SSLDHParam.Get(), "r");
		if (f == NULL) throw new CException ("SSL : could not open the DH params file " + m_SSLDHParam);
		DH *inDH = ::PEM_read_DHparams (f, NULL, NULL, NULL);
		if (!::SSL_CTX_set_tmp_dh (m_SSLCTX, inDH))
			throw new CException ("SSL : could not set the DH params from " + m_SSLDHParam);
		::fclose (f);
	}

	// ssl info callback
	if (inSSLInfoCallBack) SSL_CTX_set_info_callback (m_SSLCTX, inSSLInfoCallBack);

	// launch the listener thread
	int res; if ((inListener != NULL) && 
		((res = ::pthread_create (&m_ListenTh, NULL, CSocketSSLServer::m_ListenSSLCallBack, this)) != 0))
	               throw new CException ("couldn\'t create pthread");
}

//----------------------------------------------------------------------------------------------------------------------------------------
// destructor
//----------------------------------------------------------------------------------------------------------------------------------------
CSocketSSLServer::~CSocketSSLServer ()
{
	Close();
}

//----------------------------------------------------------------------------------------------------------------------------------------
// close the socket
//----------------------------------------------------------------------------------------------------------------------------------------
void CSocketSSLServer::Close ()
{
	// cancel the listener thread
	if (m_ListenTh != m_PThreadMain) { ::pthread_cancel (m_ListenTh); ::pthread_detach (m_ListenTh); } m_ListenTh = m_PThreadMain;

	// free the context
	if (m_SSLCTX != NULL) ::SSL_CTX_free (m_SSLCTX);

	// generic call
	CSocketServer::Close();
}

//----------------------------------------------------------------------------------------------------------------------------------------
// expected listener type
//----------------------------------------------------------------------------------------------------------------------------------------
const CMetaClass * CSocketSSLServer::ListenerMustBe () const
{
	return __metaclass(CSocketSSLServerListener);
}

//----------------------------------------------------------------------------------------------------------------------------------------
// listener affectation
//----------------------------------------------------------------------------------------------------------------------------------------
bool CSocketSSLServer::AssignListener (const CObjectListener *inListener)
{
	CObjectListener *oldListener = RemoveListener();
	if (oldListener != NULL) delete oldListener;
	if (inListener != NULL)
	{
		if (inListener -> ClassIs (ListenerMustBe()))
		{
			if (!CSocket::AssignListener (inListener)) return false;
			int res; if ((res = ::pthread_create (&m_ListenTh, NULL, CSocketSSLServer::m_ListenSSLCallBack, this)) != 0)
				return false;
		}
		else return false;
	}
	return true;
}

//----------------------------------------------------------------------------------------------------------------------------------------
// ssl connection call back
//----------------------------------------------------------------------------------------------------------------------------------------
void * CSocketSSLServer::m_ListenSSLCallBack (void *inData)
{
	// retreive the server socket instance
	CSocketSSLServer *inSocketSSLServer = reinterpret_cast <CSocketSSLServer *> (inData);

	// incoming attributes
	struct sockaddr_in inSockAddrIn; int inDesc; socklen_t outSockLen = sizeof(inSockAddrIn);

	// loop...
	while (true)
	{
		// listener notification
		if (inSocketSSLServer->GetListener())
			static_cast <CSocketSSLServerListener *> (inSocketSSLServer->GetListener()) -> OnAccept (inSocketSSLServer);

		// wait for a new connection on the server socket
		inDesc = ::accept (inSocketSSLServer->m_Desc, reinterpret_cast <struct sockaddr *> (&inSockAddrIn), &outSockLen);
		if (inDesc < 0) continue;

		// create a new ssl handler from the common loaded context
		SSL *inSSL = ::SSL_new (inSocketSSLServer->m_SSLCTX);

		// associate the socket and the ssl struct
		::SSL_set_fd (inSSL, inDesc);

		// ssl accept...
		int inRes; if ((inRes = ::SSL_accept (inSSL)) <= 0)
		{
			switch (::SSL_get_error (inSSL, inRes))
			{
				case SSL_ERROR_NONE :
					break;
				case SSL_ERROR_ZERO_RETURN :
				case SSL_ERROR_WANT_READ :
				case SSL_ERROR_WANT_WRITE :
				case SSL_ERROR_WANT_CONNECT :
				case SSL_ERROR_WANT_ACCEPT :
				case SSL_ERROR_WANT_X509_LOOKUP :
				case SSL_ERROR_SYSCALL :
				case SSL_ERROR_SSL :
					//::fprintf (stderr, "SSL ERROR : %d\n", ::SSL_get_error (inSSL, inRes));
					break;
			}

			::SSL_free (inSSL);
			::close (inDesc);
			continue;
		}

		// retrieve the str addr
		char inAddr[INET_ADDRSTRLEN]; CString outAddr (::inet_ntop (AF_INET, &(inSockAddrIn.sin_addr), inAddr, INET_ADDRSTRLEN));
	
		// notification if any to be done
		bool ioAccept=true; void *inParam=NULL; if (inSocketSSLServer->GetListener())
			static_cast <CSocketSSLServerListener *> (inSocketSSLServer->GetListener()) ->
				OnConnect (inSocketSSLServer, outAddr, ntohs(inSockAddrIn.sin_port), inDesc, inSSL, ioAccept, inParam);

		// if not accepted...
		if (!ioAccept) 
		{
			if (::SSL_shutdown (inSSL) == 0) ::SSL_shutdown (inSSL);
			::SSL_free (inSSL);
			::close (inDesc); 
			continue; 
		}

		// launch a new reception thread
		TReceiveSSLCBData *outReceiveSSLCBData = new TReceiveSSLCBData;
		outReceiveSSLCBData->inst  = inSocketSSLServer;
		outReceiveSSLCBData->desc  = inDesc;
		outReceiveSSLCBData->ssl   = inSSL;
		outReceiveSSLCBData->addr  = outAddr;
		outReceiveSSLCBData->port  = ntohs(inSockAddrIn.sin_port);
		outReceiveSSLCBData->param = inParam;
		pthread_t inReceiveTh; 
		if (::pthread_create (&inReceiveTh, NULL, CSocketSSLServer::m_ReceiveSSLCallBack, outReceiveSSLCBData) != 0)
			::fprintf (stderr, "CSocketSSLServer could not create pthread");
	}

	// ok
	return NULL;
}

//----------------------------------------------------------------------------------------------------------------------------------------
// ssl data reception callback
//----------------------------------------------------------------------------------------------------------------------------------------
void * CSocketSSLServer::m_ReceiveSSLCallBack (void *inData)
{
	// retrieve the callback data
	TReceiveSSLCBData *inReceiveSSLCBData = reinterpret_cast <TReceiveSSLCBData *> (inData);

	// loop...
	while (true)
	{
		// wait for some data
		char inBuffer[8192]; int result = ::SSL_read (inReceiveSSLCBData->ssl, inBuffer, 8192);

		// check result
		if (result <= 0)
		{
			if (inReceiveSSLCBData->inst->GetListener() != NULL)
				static_cast <CSocketSSLServerListener *> (inReceiveSSLCBData->inst->GetListener()) -> 
					OnDisconnect (inReceiveSSLCBData->inst, inReceiveSSLCBData->addr, inReceiveSSLCBData->port, 
						      inReceiveSSLCBData->desc, inReceiveSSLCBData->ssl, inReceiveSSLCBData->param);

			::SSL_free (inReceiveSSLCBData->ssl); ::close (inReceiveSSLCBData->desc); delete inReceiveSSLCBData;
			::pthread_detach (::pthread_self());
			break;
		}

		// notify the listener
		if (inReceiveSSLCBData->inst->GetListener() != NULL)
			static_cast <CSocketSSLServerListener *> (inReceiveSSLCBData->inst->GetListener()) ->
				OnReceive (inReceiveSSLCBData->inst, inReceiveSSLCBData->addr, inReceiveSSLCBData->port, 
					   inReceiveSSLCBData->desc, inReceiveSSLCBData->ssl, inBuffer, result, inReceiveSSLCBData->param);
	}

	// ok
	return NULL;
}

