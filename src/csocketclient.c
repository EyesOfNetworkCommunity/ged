/*****************************************************************************************************************************************
 csocketclient.c
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

#include "csocketclient.h"

//----------------------------------------------------------------------------------------------------------------------------------------
// metaclass resolution
//----------------------------------------------------------------------------------------------------------------------------------------
RESOLVE_GENERIC_METACLASS (CSocketClientListener);

//----------------------------------------------------------------------------------------------------------------------------------------
// constructor
//----------------------------------------------------------------------------------------------------------------------------------------
CSocketClientListener::CSocketClientListener ()
{ }

//----------------------------------------------------------------------------------------------------------------------------------------
// destructor
//----------------------------------------------------------------------------------------------------------------------------------------
CSocketClientListener::~CSocketClientListener ()
{ }

// dummy signal handling, see bellow
static void DummySigPipe (int) { }

//----------------------------------------------------------------------------------------------------------------------------------------
// metaclass resolution
//----------------------------------------------------------------------------------------------------------------------------------------
RESOLVE_CAPSULE_METACLASS (CSocketClient);

//----------------------------------------------------------------------------------------------------------------------------------------
// constructor
//----------------------------------------------------------------------------------------------------------------------------------------
CSocketClient::CSocketClient (const CString &inAddress, const UInt16 inPort, const CString &inBind, const CSocketClientListener *inListener, void *inParam)
	      :CSocket	     (inAddress, inPort, inListener),
	       m_ReceiveTh   (::pthread_self()),
	       m_BindAddr    (NULL)
{
	// when we try to write on a closed socket, the SIGPIPE signal is emitted and the process exists returning EPIPE... It is not
	// sended if the flag MSG_NOSIGNAL was specified when sending the data over the socket. As this definition stands for SSL too
	// and as I did not find any bypassing in the ssl api, just overwritte the default system behaviour and keep on listening
	// if such an event occurs...
	struct sigaction sa; sa.sa_handler = ::DummySigPipe; sa.sa_flags = SA_NOCLDSTOP; ::sigaction (SIGPIPE, &sa, NULL);

	// try to bypass the DNS lookup if this is just an IP address
	if (!::inet_aton (inAddress.Get(), &((struct sockaddr_in*)m_SockAddr)->sin_addr))
	{
		struct hostent *hp = ::gethostbyname (inAddress.Get());
		if (hp == NULL) throw new CException ("gethostbyname : invalid address \"" + inAddress + "\"");
		::memcpy (&((struct sockaddr_in*)m_SockAddr)->sin_addr, hp->h_addr, hp->h_length);
	}

	// optional client binding
	m_BindAddr = (struct sockaddr *) new struct sockaddr_in;
        ::bzero (m_BindAddr, sizeof(struct sockaddr_in));
        ::bzero (&((struct sockaddr_in *)m_BindAddr)->sin_zero, 8);
        ((struct sockaddr_in *)m_BindAddr)->sin_family = AF_INET;
        ((struct sockaddr_in *)m_BindAddr)->sin_port = 0L;

        if (inBind != CString() && !::inet_aton (inBind.Get(), &((struct sockaddr_in*)m_BindAddr)->sin_addr))
                throw new CException ("invalid address \"" + inBind + "\"");

        if (inBind != CString() && ::bind (m_Desc, m_BindAddr, sizeof(struct sockaddr_in)) < 0)
                throw new CException ("could\'t bind to \"" + inBind + "\"");

	// try to open a connection
	if (::connect (m_Desc, m_SockAddr, sizeof(struct sockaddr_in)) < 0)
	{
		switch (errno)
		{
			case ECONNREFUSED : throw new CException ("connection refused by host \"" + inAddress + ":" + CString((UInt32)inPort) + "\"", errno);
			case ETIMEDOUT    : throw new CException ("timeout while attempting connection on \"" + inAddress + ":" + CString((UInt32)inPort) + "\"", errno);
			case ENETUNREACH  : throw new CException (CString("network is unreachable"), errno);
			default           : throw new CException (CString("connection refused or timed out"), errno);
		}
	}

	// listener notification if any and thread instanciation
	if (inListener != NULL)
	{
		// notify the listener about the connect that has been performed
		const_cast <CSocketClientListener *> (inListener) -> OnConnect (this, inParam);

		// try to create our reception thread
		int res;  if ((res = ::pthread_create (&m_ReceiveTh, NULL, CSocketClient::m_ReceiveCallBack, this)) != 0)
			throw new CException ("couldn\'t create pthread");
	}
}

//----------------------------------------------------------------------------------------------------------------------------------------
// constructor
//----------------------------------------------------------------------------------------------------------------------------------------
CSocketClient::CSocketClient (const CString &inFile, const CSocketClientListener *inListener, void *inParam)
	      :CSocket	     (inFile, inListener),
	       m_ReceiveTh   (::pthread_self()),
	       m_BindAddr    (NULL)
{
	// when we try to write on a closed socket, the SIGPIPE signal is emitted and the process exists returning EPIPE... It is not
	// sended if the flag MSG_NOSIGNAL was specified when sending the data over the socket. As this definition stands for SSL too
	// and as I did not find any bypassing in the ssl api, just overwritte the default system behaviour and keep on listening
	// if such an event occurs...
	struct sigaction sa; sa.sa_handler = ::DummySigPipe; sa.sa_flags = SA_NOCLDSTOP; ::sigaction (SIGPIPE, &sa, NULL);

	// try to open a connection
	if (::connect (m_Desc, m_SockAddr, sizeof(struct sockaddr_un)) < 0)
	{
		switch (errno)
		{
			case ECONNREFUSED : throw new CException ("connection refused by host \"" + inFile + "\"", errno);
			case ETIMEDOUT    : throw new CException ("timeout while attempting connection on \"" + inFile + "\"", errno);
			case ENETUNREACH  : throw new CException (CString("network is unreachable"), errno);
			default           : throw new CException (CString("connection refused or timed out"), errno);
		}
	}

	// listener notification if any and thread instanciation
	if (inListener != NULL)
	{
		// notify the listener about the connect that has been performed
		const_cast <CSocketClientListener *> (inListener) -> OnConnect (this, inParam);

		// try to create our reception thread
		int res;  if ((res = ::pthread_create (&m_ReceiveTh, NULL, CSocketClient::m_ReceiveCallBack, this)) != 0)
			throw new CException ("couldn\'t create pthread");
	}
}

//----------------------------------------------------------------------------------------------------------------------------------------
// destructor
//----------------------------------------------------------------------------------------------------------------------------------------
CSocketClient::~CSocketClient ()
{
	Close(); 
}

//----------------------------------------------------------------------------------------------------------------------------------------
// expected listener
//----------------------------------------------------------------------------------------------------------------------------------------
const CMetaClass * CSocketClient::ListenerMustBe () const
{
	return __metaclass(CSocketClientListener);
}

//----------------------------------------------------------------------------------------------------------------------------------------
// listener affectation
//----------------------------------------------------------------------------------------------------------------------------------------
bool CSocketClient::AssignListener (const CObjectListener *inListener)
{
	CObjectListener *oldListener = RemoveListener();
	if (oldListener != NULL) delete oldListener;
	if (inListener != NULL)
	{
		if (inListener -> ClassIs (ListenerMustBe()))
		{
			if (!CSocket::AssignListener (inListener)) return false;
			int res; if ((res = ::pthread_create (&m_ReceiveTh, NULL, CSocketClient::m_ReceiveCallBack, this)) != 0)
				return false;
		}
		else return false;
	}
	return true;
}

//----------------------------------------------------------------------------------------------------------------------------------------
// listener removal
//----------------------------------------------------------------------------------------------------------------------------------------
CObjectListener * CSocketClient::RemoveListener ()
{
	if (m_ReceiveTh != m_PThreadMain)
	{
		::pthread_cancel (m_ReceiveTh); 
		::pthread_detach (m_ReceiveTh);
	}
	m_ReceiveTh = m_PThreadMain;
	CObjectListener *outListener = m_Listener;
	m_Listener = NULL;
	return outListener;
}

//----------------------------------------------------------------------------------------------------------------------------------------
// close the socket
//----------------------------------------------------------------------------------------------------------------------------------------
void CSocketClient::Close ()
{
	// cancel the listener thread
	if (m_ReceiveTh != m_PThreadMain) { ::pthread_cancel (m_ReceiveTh); ::pthread_detach (m_ReceiveTh); } m_ReceiveTh = m_PThreadMain;

	if (m_BindAddr != NULL) delete m_BindAddr; m_BindAddr = NULL;

	// generic call 
	CSocket::Close();
}

//----------------------------------------------------------------------------------------------------------------------------------------
// wait for data
//----------------------------------------------------------------------------------------------------------------------------------------
void CSocketClient::Wait ()
{
	if (m_ReceiveTh != m_PThreadMain) ::pthread_join (m_ReceiveTh, NULL);
}

//----------------------------------------------------------------------------------------------------------------------------------------
// reception callback
//----------------------------------------------------------------------------------------------------------------------------------------
void * CSocketClient::m_ReceiveCallBack (void *inData)
{
	// retrieve the socket instance
	CSocketClient *inSocketClient = reinterpret_cast <CSocketClient *> (inData);

	// loop..
	while (true)
	{
		// wait for incoming data (maximum 8192 bytes)
		char inBuffer[8192]; int result = ::recv (inSocketClient->m_Desc, inBuffer, 8192, 0);
		if (result <= 0)
		{
			if (inSocketClient->GetListener() != NULL)
	                        static_cast <CSocketClientListener *> (inSocketClient->GetListener()) ->
					OnDisconnect (inSocketClient);

			::pthread_detach (::pthread_self());

			break;
		}
                                                                                                                          
		// send information to listener if any
		if (inSocketClient->GetListener() != NULL)
			static_cast <CSocketClientListener *> (inSocketClient->GetListener()) ->
				OnReceive (inSocketClient, inBuffer, result);
	}

	// ok
	return NULL;
}

