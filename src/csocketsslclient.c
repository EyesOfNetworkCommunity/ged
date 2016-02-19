/*****************************************************************************************************************************************
 csocketsslclient.c
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

#include "csocketsslclient.h"

//----------------------------------------------------------------------------------------------------------------------------------------
// metaclass resolution
//----------------------------------------------------------------------------------------------------------------------------------------
RESOLVE_GENERIC_METACLASS (CSocketSSLClientListener);

//----------------------------------------------------------------------------------------------------------------------------------------
// constructor
//----------------------------------------------------------------------------------------------------------------------------------------
CSocketSSLClientListener::CSocketSSLClientListener ()
{ }

//----------------------------------------------------------------------------------------------------------------------------------------
// destructor
//----------------------------------------------------------------------------------------------------------------------------------------
CSocketSSLClientListener::~CSocketSSLClientListener ()
{ }

//----------------------------------------------------------------------------------------------------------------------------------------
// metaclass resolution
//----------------------------------------------------------------------------------------------------------------------------------------
RESOLVE_CAPSULE_METACLASS (CSocketSSLClient);

//----------------------------------------------------------------------------------------------------------------------------------------
// constructor
//----------------------------------------------------------------------------------------------------------------------------------------
CSocketSSLClient::CSocketSSLClient (const CString &inAddress, const UInt16 inPort, const CString &inSSLCAFile, 
				    const CString &inSSLCertFile, const CString &inSSLKeyFile, const bool inSSLReqPeer,
				    const CString &inSSLCiphers, const CString &inBind, const CSocketSSLClientListener *inListener,
				    SSLInfoCallBack inSSLInfoCallBack, SSLVerifyCallBack inSSLVerifyCallBack, void *inParam)
		 :CSocketClient	   (inAddress, inPort, inBind, NULL, inParam),
		  m_SSLCAFile	   (inSSLCAFile),
		  m_SSLCertFile	   (inSSLCertFile),
		  m_SSLKeyFile	   (inSSLKeyFile),
		  m_SSLReqPeer	   (inSSLReqPeer),
		  m_SSLCiphers	   (inSSLCiphers),
		  m_SSLCTX	   (NULL),
		  m_SSL		   (NULL)
{
	// listener affectation
	m_Listener = const_cast <CSocketSSLClientListener *> (inListener);

	// at this point, the generic client socket must have connected its target but the listener should not have been notified because 
	// there was no listener specified while constructing the generic part, so call the notification now
	if (inListener != NULL) const_cast <CSocketSSLClientListener *> (inListener) -> OnConnect (this, inParam);

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

	// ssl info callback
	if (inSSLInfoCallBack) SSL_CTX_set_info_callback (m_SSLCTX, inSSLInfoCallBack);

	// instanciate the SSL handle from the defined context
	m_SSL = ::SSL_new (m_SSLCTX);

	// associate the scoket desc to the SSL struct
	::SSL_set_fd (m_SSL, m_Desc);

	// try to connect ssl, perform handshake...
	int inRes; if ((inRes = ::SSL_connect (m_SSL)) <= 0)
		throw new CException ("SSL ERROR number " + CString((long)::SSL_get_error (m_SSL, inRes)));

	// listener notification and thread allocation
	if (inListener != NULL)
	{
		// notify the listener
		const_cast <CSocketSSLClientListener *> (inListener) -> OnSSLConnect (this, inParam);

		// try to create our reception thread
		int res; if ((res = ::pthread_create (&m_ReceiveTh, NULL, CSocketSSLClient::m_ReceiveSSLCallBack, this)) != 0)
			throw new CException ("couldn\'t create pthread");
	}
}

//----------------------------------------------------------------------------------------------------------------------------------------
// destructor
//----------------------------------------------------------------------------------------------------------------------------------------
CSocketSSLClient::~CSocketSSLClient ()
{
	Close();
}

//----------------------------------------------------------------------------------------------------------------------------------------
// receive some data
//----------------------------------------------------------------------------------------------------------------------------------------
int CSocketSSLClient::Receive (void *ioData, const int inLen)
{
	// check if no SSL...
	if (m_SSL == NULL) return CSocketClient::Receive (ioData, inLen);

	// ok ?
	return ::SSL_read (m_SSL, ioData, inLen);
}

//----------------------------------------------------------------------------------------------------------------------------------------
// send some data
//----------------------------------------------------------------------------------------------------------------------------------------
int CSocketSSLClient::Send (const void *inData, const int inLen)
{
	// check if no SSL...
	if (m_SSL == NULL) return CSocketClient::Send (inData, inLen);

	// ok ?
	return ::SSL_write (m_SSL, inData, inLen);
}

//----------------------------------------------------------------------------------------------------------------------------------------
// SSL struct
//----------------------------------------------------------------------------------------------------------------------------------------
SSL * CSocketSSLClient::GetSSL () const
{
	return const_cast <SSL *> (m_SSL);
}

//----------------------------------------------------------------------------------------------------------------------------------------
// close the socket
//----------------------------------------------------------------------------------------------------------------------------------------
void CSocketSSLClient::Close ()
{
	// cancel thread
	if (m_ReceiveTh != m_PThreadMain) { ::pthread_cancel (m_ReceiveTh); ::pthread_detach (m_ReceiveTh); } m_ReceiveTh = m_PThreadMain;

	// shutdown the ssl connection (call twice when return code equals zero)
	if (m_SSL != NULL)
	{
		if (::SSL_shutdown (m_SSL) == 0) ::SSL_shutdown (m_SSL);
		::SSL_free (m_SSL); m_SSL = NULL;
	}

	// free the context
	if (m_SSLCTX != NULL) ::SSL_CTX_free (m_SSLCTX); m_SSLCTX = NULL;

	// generic call
	CSocket::Close();
}

//----------------------------------------------------------------------------------------------------------------------------------------
// expected listener type
//----------------------------------------------------------------------------------------------------------------------------------------
const CMetaClass * CSocketSSLClient::ListenerMustBe () const
{
	return __metaclass(CSocketSSLClientListener);
}

//----------------------------------------------------------------------------------------------------------------------------------------
// listener affectation
//----------------------------------------------------------------------------------------------------------------------------------------
bool CSocketSSLClient::AssignListener (const CObjectListener *inListener)
{
	CObjectListener *oldListener = RemoveListener();
	if (oldListener != NULL) delete oldListener;
	if (inListener != NULL)
	{
		if (inListener -> ClassIs (ListenerMustBe()))
		{
			if (!CSocket::AssignListener (inListener)) return false;
			int res; if ((res = ::pthread_create (&m_ReceiveTh, NULL, CSocketSSLClient::m_ReceiveSSLCallBack, this)) != 0)
				return false;
		}
		else return false;
	}
	return true;
}

//----------------------------------------------------------------------------------------------------------------------------------------
// SSL data reception callback
//----------------------------------------------------------------------------------------------------------------------------------------
void * CSocketSSLClient::m_ReceiveSSLCallBack (void *inData)
{
	// retreive the socket instance
	CSocketSSLClient *inSocketSSLClient = reinterpret_cast <CSocketSSLClient *> (inData);

	// loop...
	while (true)
	{
		// wait for incoming data (maximum 8192 bytes)
		char inBuffer[8192]; int result = ::SSL_read (inSocketSSLClient->m_SSL, inBuffer, 8192);
		if (result <= 0) 
		{
			if (inSocketSSLClient->GetListener() != NULL)
	                        static_cast <CSocketSSLClientListener *> (inSocketSSLClient->GetListener()) ->
        	                        OnDisconnect (inSocketSSLClient);

			::pthread_detach (::pthread_self());

			break;
		}

		// send information to listener if any
		if (inSocketSSLClient->GetListener() != NULL)
			static_cast <CSocketSSLClientListener *> (inSocketSSLClient->GetListener()) ->
				OnReceive (inSocketSSLClient, inBuffer, result);
	}

	// ok
	return NULL;
}


