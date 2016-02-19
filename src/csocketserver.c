/*****************************************************************************************************************************************
 csocketserver.c
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

#include "csocketserver.h"

//----------------------------------------------------------------------------------------------------------------------------------------
// metaclass resolution
//----------------------------------------------------------------------------------------------------------------------------------------
RESOLVE_GENERIC_METACLASS (CSocketServerListener);

//----------------------------------------------------------------------------------------------------------------------------------------
// constructor
//----------------------------------------------------------------------------------------------------------------------------------------
CSocketServerListener::CSocketServerListener ()
{ }

//----------------------------------------------------------------------------------------------------------------------------------------
// destructor
//----------------------------------------------------------------------------------------------------------------------------------------
CSocketServerListener::~CSocketServerListener ()
{ }

//----------------------------------------------------------------------------------------------------------------------------------------
// metaclass resolution
//----------------------------------------------------------------------------------------------------------------------------------------
RESOLVE_CAPSULE_METACLASS (CSocketServer);

// dummy signal handling, see bellow
static void DummySigPipe (int) { }

//----------------------------------------------------------------------------------------------------------------------------------------
// constructor
//----------------------------------------------------------------------------------------------------------------------------------------
CSocketServer::CSocketServer (const CString &inAddress, const UInt16 inPort, const CSocketServerListener *inListener)
	      :CSocket	     (inAddress, inPort, inListener),
	       m_ListenTh    (::pthread_self())
{
	// when we try to write on a closed socket, the SIGPIPE signal is emitted and the process exists returing EPIPE... It is not
	// sended if the flag MSG_NOSIGNAL was specified when sending the data over the socket. As this definition stands for SSL too
	// and as I did not find any bypassing in the ssl api, just overwritte the default system behaviour and keep on listening
	// if such an event occurs...
	struct sigaction sa; sa.sa_handler = ::DummySigPipe; sa.sa_flags = SA_NOCLDSTOP; ::sigaction (SIGPIPE, &sa, NULL);

	// what address should we bind to ?
	if (inAddress == CString())
		((struct sockaddr_in*)m_SockAddr)->sin_addr.s_addr = INADDR_ANY;
	else if (!::inet_aton (inAddress.Get(), &((struct sockaddr_in*)m_SockAddr)->sin_addr))
		throw new CException ("invalid address \"" + inAddress + "\"");

	// set the reuse address flag so we don't get errors if restarting and keep socket alive
	int flag=1; ::setsockopt (m_Desc, SOL_SOCKET, SO_REUSEADDR, (char*)&flag, sizeof(int));

        // bind the address to the Internet socket
        if (::bind (m_Desc, m_SockAddr, sizeof(struct sockaddr_in)) < 0)
                throw new CException ("could\'t bind to \"" + inAddress + "\"");

        // prepare socket to listen for connections
        if (::listen (m_Desc, SOMAXCONN) < 0)
                throw new CException ("couldn\'t listen on \"" + inAddress + "\"");

	// try to create our listener thread
	int res; if ((inListener != NULL) && ((res = ::pthread_create (&m_ListenTh, NULL, CSocketServer::m_ListenCallBack, this)) != 0))
                throw new CException ("couldn\'t create pthread");
}

//----------------------------------------------------------------------------------------------------------------------------------------
// constructor
//----------------------------------------------------------------------------------------------------------------------------------------
CSocketServer::CSocketServer (const CString &inFile, const CSocketServerListener *inListener)
	      :CSocket	     (inFile, inListener),
	       m_ListenTh    (::pthread_self()),
	       m_File	     (inFile)
{
	// when we try to write on a closed socket, the SIGPIPE signal is emitted and the process exists returing EPIPE... It is not
	// sended if the flag MSG_NOSIGNAL was specified when sending the data over the socket. As this definition stands for SSL too
	// and as I did not find any bypassing in the ssl api, just overwritte the default system behaviour and keep on listening
	// if such an event occurs...
	struct sigaction sa; sa.sa_handler = ::DummySigPipe; sa.sa_flags = SA_NOCLDSTOP; ::sigaction (SIGPIPE, &sa, NULL);

        // bind the address to the local socket, remove it before if any and be sure to chmod it...
	if (m_File != CString()) ::remove (m_File.Get());
        if (::bind (m_Desc, m_SockAddr, sizeof(struct sockaddr_un)) < 0)
                throw new CException ("could\'t bind to \"" + inFile + "\"");
	::chmod (inFile.Get(), S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IWGRP|S_IXGRP|S_IROTH|S_IWOTH|S_IXOTH);

        // prepare socket to listen for connections
        if (::listen (m_Desc, SOMAXCONN) < 0)
                throw new CException ("couldn\'t listen on \"" + inFile + "\"");

	// try to create our listener thread
	int res; if ((inListener != NULL) && ((res = ::pthread_create (&m_ListenTh, NULL, CSocketServer::m_ListenCallBack, this)) != 0))
                throw new CException ("couldn\'t create pthread");
}

//----------------------------------------------------------------------------------------------------------------------------------------
// destructor
//----------------------------------------------------------------------------------------------------------------------------------------
CSocketServer::~CSocketServer ()
{ 
	Close();
	if (m_File != CString()) ::remove (m_File.Get());
}

//----------------------------------------------------------------------------------------------------------------------------------------
// expected listener type
//----------------------------------------------------------------------------------------------------------------------------------------
const CMetaClass * CSocketServer::ListenerMustBe () const
{
	return __metaclass(CSocketServerListener);
}

//----------------------------------------------------------------------------------------------------------------------------------------
// listener affectation
//----------------------------------------------------------------------------------------------------------------------------------------
bool CSocketServer::AssignListener (const CObjectListener *inListener)
{
	CObjectListener *oldListener = RemoveListener();
	if (oldListener != NULL) delete oldListener;
	if (inListener != NULL)
	{
		if (inListener -> ClassIs (ListenerMustBe()))
		{
			if (!CSocket::AssignListener (inListener)) return false;
			int res; if ((res = ::pthread_create (&m_ListenTh, NULL, CSocketServer::m_ListenCallBack, this)) != 0)
				return false;
		}
		else return false;
	}
	return true;
}

//----------------------------------------------------------------------------------------------------------------------------------------
// listener removal
//----------------------------------------------------------------------------------------------------------------------------------------
CObjectListener * CSocketServer::RemoveListener ()
{
	if (m_ListenTh != m_PThreadMain)
	{
		::pthread_cancel (m_ListenTh); 
		::pthread_detach (m_ListenTh);
	}
	m_ListenTh = m_PThreadMain;
	CObjectListener *outListener = m_Listener;
	m_Listener = NULL;
	return outListener;
}

//----------------------------------------------------------------------------------------------------------------------------------------
// close the socket
//----------------------------------------------------------------------------------------------------------------------------------------
void CSocketServer::Close ()
{
	// cancel the listener thread
	if (m_ListenTh != m_PThreadMain) { ::pthread_cancel (m_ListenTh); ::pthread_detach (m_ListenTh); } m_ListenTh = m_PThreadMain;

	// generic call
	CSocket::Close();
}

//----------------------------------------------------------------------------------------------------------------------------------------
// wait...
//----------------------------------------------------------------------------------------------------------------------------------------
void CSocketServer::Wait ()
{
	::pthread_join (m_ListenTh, NULL);
}

//----------------------------------------------------------------------------------------------------------------------------------------
// connection callback
//----------------------------------------------------------------------------------------------------------------------------------------
void * CSocketServer::m_ListenCallBack (void *inData)
{
	// retreive the server socket instance
	CSocketServer *inSocketServer = reinterpret_cast <CSocketServer *> (inData);

	// incoming attributes
	struct sockaddr_in inSockAddrIn; ::bzero(&inSockAddrIn,sizeof(inSockAddrIn)); int inDesc; socklen_t outSockLen = sizeof(inSockAddrIn);

	// loop...
	while (true)
	{
		// listener notification
		if (inSocketServer->GetListener())
			static_cast <CSocketServerListener *> (inSocketServer->GetListener()) -> OnAccept (inSocketServer);

		// wait for a new connection on the server' socket
		inDesc = ::accept (inSocketServer->m_Desc, reinterpret_cast <struct sockaddr *> (&inSockAddrIn), &outSockLen);
		if (inDesc < 0) continue;

		// retrieve the str addr
		char inAddr[INET_ADDRSTRLEN]; CString outAddr (::inet_ntop (AF_INET, &(inSockAddrIn.sin_addr), inAddr, INET_ADDRSTRLEN));

		// notification if any to be done
		bool ioAccept=true; void *inParam=NULL; if (inSocketServer->GetListener())
			static_cast <CSocketServerListener *> (inSocketServer->GetListener()) ->
				OnConnect (inSocketServer, outAddr, ntohs(inSockAddrIn.sin_port), inDesc, NULL, ioAccept, inParam);

		// if not accepted...
		if (!ioAccept) { ::close(inDesc); continue; }

		// launch a new reception thread
		TReceiveCBData *outReceiveCBData = new TReceiveCBData;
		outReceiveCBData->inst  = inSocketServer;
		outReceiveCBData->desc  = inDesc;
		outReceiveCBData->addr  = outAddr;
		outReceiveCBData->port  = ntohs(inSockAddrIn.sin_port);
		outReceiveCBData->param = inParam;
		pthread_t inReceiveTh; if (::pthread_create (&inReceiveTh, NULL, CSocketServer::m_ReceiveCallBack, outReceiveCBData) != 0)
			{ fprintf (stderr, "CSocketServer could not create pthread\n"); ::close(inDesc); }
	}

	// ok
	return NULL;
}

//----------------------------------------------------------------------------------------------------------------------------------------
// reception callback
//----------------------------------------------------------------------------------------------------------------------------------------
void * CSocketServer::m_ReceiveCallBack (void *inData)
{
	// retrieve the cb data
	TReceiveCBData *inReceiveCBData = reinterpret_cast <TReceiveCBData *> (inData);

	// loop...
	while (true)
	{
		// wait for incoming data (8192 bytes max)
		char inBuffer[8192]; int result = ::recv (inReceiveCBData->desc, inBuffer, 8192, 0);

		// check result
		if (result <= 0)
		{
			if (inReceiveCBData->inst->GetListener() != NULL)
				static_cast <CSocketServerListener *> (inReceiveCBData->inst->GetListener()) -> 
					OnDisconnect (inReceiveCBData->inst, inReceiveCBData->addr, inReceiveCBData->port, 
						      inReceiveCBData->desc, NULL, inReceiveCBData->param);

			::close (inReceiveCBData->desc); 
			delete inReceiveCBData;
			::pthread_detach (::pthread_self());
			break;
		}

		// send information to listener if any
		if (inReceiveCBData->inst->GetListener() != NULL)
			static_cast <CSocketServerListener *> (inReceiveCBData->inst->GetListener()) ->
				OnReceive (inReceiveCBData->inst, inReceiveCBData->addr, inReceiveCBData->port, inReceiveCBData->desc,
					   NULL, inBuffer, result, inReceiveCBData->param);
	}

	// ok
	return NULL;
}

