/*****************************************************************************************************************************************
 csocket.c
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

#include "csocket.h"

//----------------------------------------------------------------------------------------------------------------------------------------
// metaclass resolution
//----------------------------------------------------------------------------------------------------------------------------------------
RESOLVE_GENERIC_METACLASS (CSocketListener);

//----------------------------------------------------------------------------------------------------------------------------------------
// constructor
//----------------------------------------------------------------------------------------------------------------------------------------
CSocketListener::CSocketListener ()
{ }

//----------------------------------------------------------------------------------------------------------------------------------------
// destructor
//----------------------------------------------------------------------------------------------------------------------------------------
CSocketListener::~CSocketListener ()
{ }

//----------------------------------------------------------------------------------------------------------------------------------------
// metaclass resolution
//----------------------------------------------------------------------------------------------------------------------------------------
RESOLVE_GENERIC_METACLASS (CSocket);

//----------------------------------------------------------------------------------------------------------------------------------------
// constructor
//----------------------------------------------------------------------------------------------------------------------------------------
CSocket::CSocket 	(const CString &inAddress, const UInt16 inPort, const CSocketListener *inListener)
	:CObject 	(inListener),
	 m_Desc	 	(-1),
	 m_PThreadMain	(::pthread_self()),
	 m_SockAddr	(NULL)
{
	// initialize struct
	m_SockAddr = (struct sockaddr *) new struct sockaddr_in;
	::bzero (m_SockAddr, sizeof(struct sockaddr_in));

	// create a standard IPV4 socket
	if ((m_Desc = ::socket (AF_INET, SOCK_STREAM, 0)) < 0) throw new CException (CString("Could not create socket"));

	// set the default component struct attributes
	::bzero (&((struct sockaddr_in *)m_SockAddr)->sin_zero, 8);
	((struct sockaddr_in *)m_SockAddr)->sin_family = AF_INET;
	((struct sockaddr_in *)m_SockAddr)->sin_port = htons(inPort);

	// mutex initialization
	::pthread_mutexattr_settype (&m_PThreadMutexAttr, PTHREAD_MUTEX_RECURSIVE);
	::pthread_mutex_init (&m_PThreadMutex, /*&m_PThreadMutexAttr*/NULL);
}

//----------------------------------------------------------------------------------------------------------------------------------------
// constructor
//----------------------------------------------------------------------------------------------------------------------------------------
CSocket::CSocket	(const CString &inFile, const CSocketListener *inListener) THROWABLE
	:CObject	(inListener),
	 m_Desc		(-1),
	 m_PThreadMain	(::pthread_self()),
	 m_SockAddr	(NULL)
{
	// initialisze struct
	m_SockAddr = (struct sockaddr *) new struct sockaddr_un;
	::bzero (m_SockAddr, sizeof(struct sockaddr_un));

	// create a standard file socket
	if ((m_Desc = ::socket (PF_UNIX, SOCK_STREAM, 0)) < 0) throw new CException (CString("Could not create socket"));

	// set the default component struct attributes
	((struct sockaddr_un*)m_SockAddr)->sun_family = AF_UNIX;
	::strncpy (((struct sockaddr_un*)m_SockAddr)->sun_path, inFile.Get(), sizeof(((struct sockaddr_un*)m_SockAddr)->sun_path)-1);

	// mutex initialization
	::pthread_mutexattr_settype (&m_PThreadMutexAttr, PTHREAD_MUTEX_RECURSIVE);
	::pthread_mutex_init (&m_PThreadMutex, /*&m_PThreadMutexAttr*/NULL);
}

//----------------------------------------------------------------------------------------------------------------------------------------
// destructor
//----------------------------------------------------------------------------------------------------------------------------------------
CSocket::~CSocket ()
{
	Close();
	::pthread_mutexattr_destroy (&m_PThreadMutexAttr);
	::pthread_mutex_destroy (&m_PThreadMutex);
	if (m_SockAddr != NULL) delete m_SockAddr;
}

//----------------------------------------------------------------------------------------------------------------------------------------
// expected socket listener class
//----------------------------------------------------------------------------------------------------------------------------------------
const CMetaClass * CSocket::ListenerMustBe () const
{
	return __metaclass(CSocketListener);
}

//----------------------------------------------------------------------------------------------------------------------------------------
// socket descriptor
//----------------------------------------------------------------------------------------------------------------------------------------
int CSocket::GetDesc () const
{
	return m_Desc;
}

//----------------------------------------------------------------------------------------------------------------------------------------
// socket addr
//----------------------------------------------------------------------------------------------------------------------------------------
struct sockaddr * CSocket::GetSockAddr () const
{
	return m_SockAddr;
}

//----------------------------------------------------------------------------------------------------------------------------------------
// get addr
//----------------------------------------------------------------------------------------------------------------------------------------
CString CSocket::GetAddr () const
{
	CString outAddr ("0.0.0.0"); char inAddr[INET_ADDRSTRLEN]; struct sockaddr_in ioaddr; socklen_t iolen = sizeof(struct sockaddr);
	::bzero (&ioaddr, sizeof(struct sockaddr_in));
	if (::getsockname (m_Desc, (struct sockaddr*)&ioaddr, &iolen) == -1) return outAddr;
	outAddr = ::inet_ntop (AF_INET, &(ioaddr.sin_addr), inAddr, INET_ADDRSTRLEN);
	return outAddr;
}

//----------------------------------------------------------------------------------------------------------------------------------------
// get port
//----------------------------------------------------------------------------------------------------------------------------------------
UInt16 CSocket::GetPort () const
{
	struct sockaddr_in ioaddr; socklen_t iolen = sizeof(struct sockaddr);
	if (::getsockname (m_Desc, (struct sockaddr*)&ioaddr, &iolen) == -1) return 0L;
	return ntohs(ioaddr.sin_port);
}

//----------------------------------------------------------------------------------------------------------------------------------------
// send some data
//----------------------------------------------------------------------------------------------------------------------------------------
int CSocket::Send (const void *inData, const int inLength)
{
	if (m_Desc < 0) return -1;

	int total=0, bytesleft=inLength, n=0;

        while (total < inLength)
        {
                n = ::send (m_Desc, reinterpret_cast <const char *> (inData) + total, bytesleft, 0);//MSG_NOSIGNAL); server sigpipe bypass
                if (n == -1) break;
                total     += n;
                bytesleft -= n;
        }

        return n == -1 ? -1 : total;
}

//----------------------------------------------------------------------------------------------------------------------------------------
// send some data
//----------------------------------------------------------------------------------------------------------------------------------------
int CSocket::Send (const int inDesc, const void *inData, const int inLength)
{
	if (inDesc < 0) return -1;

	int total=0, bytesleft=inLength, n=0;

        while (total < inLength)
        {
                n = ::send (inDesc, reinterpret_cast <const char *> (inData) + total, bytesleft, 0);//MSG_NOSIGNAL); server sigpipe bypass
                if (n == -1) break;
                total     += n;
                bytesleft -= n;
        }

        return n == -1 ? -1 : total;
}

//----------------------------------------------------------------------------------------------------------------------------------------
// send some data
//----------------------------------------------------------------------------------------------------------------------------------------
int CSocket::SendSSL (SSL *inSSL, const void *inData, const int inLength)
{
	if (inSSL == NULL) return -1;

	int total=0, bytesleft=inLength, n=0;

        while (total < inLength)
        {
                n = ::SSL_write (inSSL, reinterpret_cast <const char *> (inData) + total, bytesleft);
                if (n == -1) break;
                total     += n;
                bytesleft -= n;
        }

        return n == -1 ? -1 : total;
}

//----------------------------------------------------------------------------------------------------------------------------------------
// receive some data
//----------------------------------------------------------------------------------------------------------------------------------------
int CSocket::Receive (const int inDesc, void *ioData, const int inLength)
{
	if (inDesc < 0) return -1;

       ::bzero (ioData, inLength); 
	int n = ::recv (inDesc, ioData, inLength, 0);
	return n;
}

//----------------------------------------------------------------------------------------------------------------------------------------
// receive some data
//----------------------------------------------------------------------------------------------------------------------------------------
int CSocket::Receive (void *ioData, const int inLength)
{
	if (m_Desc < 0) return -1;

        ::bzero (ioData, inLength); 
	return ::recv (m_Desc, ioData, inLength, 0);
}

//----------------------------------------------------------------------------------------------------------------------------------------
// receive some data
//----------------------------------------------------------------------------------------------------------------------------------------
int CSocket::ReceiveSSL (SSL *inSSL, void *ioData, const int inLength)
{
	if (inSSL == NULL) return -1;

	::bzero (ioData, inLength); 
	return ::SSL_read (inSSL, ioData, inLength);
}

//----------------------------------------------------------------------------------------------------------------------------------------
// close the socket
//----------------------------------------------------------------------------------------------------------------------------------------
void CSocket::Close ()
{
	if (m_Desc >= 0)
	{
		::shutdown (m_Desc, SHUT_RDWR);
		::close (m_Desc); 
	}
	m_Desc = -1;
}

//----------------------------------------------------------------------------------------------------------------------------------------
// mutex lock
//----------------------------------------------------------------------------------------------------------------------------------------
int CSocket::Lock ()
{
	return ::pthread_mutex_lock (&m_PThreadMutex);
}

//----------------------------------------------------------------------------------------------------------------------------------------
// mutex unlock
//----------------------------------------------------------------------------------------------------------------------------------------
int CSocket::UnLock ()
{
	return ::pthread_mutex_unlock (&m_PThreadMutex);
}

//----------------------------------------------------------------------------------------------------------------------------------------
// local interfaces names
//----------------------------------------------------------------------------------------------------------------------------------------
CStrings CSocket::SiocGifConf ()
{
	CStrings out;

	ifconf ifc; ifc.ifc_len = sizeof(ifreq)*16; ifc.ifc_req = new ifreq [16];

	if (::ioctl (m_Desc, SIOCGIFCONF, &ifc) == 0)
	{
		for (size_t i=0, j=0; i<ifc.ifc_len; i++, j++)
		{
			CString ifcfg (ifc.ifc_req[j].ifr_name);
			struct sockaddr_in *sa = (struct sockaddr_in *)&ifc.ifc_req[j].ifr_addr;
        		ifcfg += CString("|") + ::inet_ntoa(sa->sin_addr);
			i += sizeof(ifreq);
			out += ifcfg;
		}
	}

	delete [] ifc.ifc_req;

	return out;
}

