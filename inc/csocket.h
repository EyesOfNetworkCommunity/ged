/*****************************************************************************************************************************************
 csocket.h - abstract socket class -
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

#ifndef __CSOCKET_H__
#define __CSOCKET_H__

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <signal.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <pthread.h>

#include <openssl/ssl.h>
#include <openssl/err.h>

#include "cobject.h"

typedef void (*SSLInfoCallBack) 		(const SSL *, int, int);
typedef int  (*SSLVerifyCallBack)		(int, X509_STORE_CTX *);

//----------------------------------------------------------------------------------------------------------------------------------------
// socket listener
//----------------------------------------------------------------------------------------------------------------------------------------
class CSocketListener : public CObjectListener
{
	// instanciation section
	public :
		
		CSocketListener			();
		virtual ~CSocketListener	() =0;

		// metaclass association
		SECTION_GENERIC_METACLASS;
};

// metaclass declaration
DECLARE_GENERIC_METACLASS ('_skt', CSocketListener, CObjectListener);

//----------------------------------------------------------------------------------------------------------------------------------------
// socket abstract class
//----------------------------------------------------------------------------------------------------------------------------------------
class CSocket : public CObject
{
	// instanciation section
	public :

		CSocket				(const CString &inAddress, const UInt16 inPort, const CSocketListener *inListener=NULL) THROWABLE;
		CSocket				(const CString &inFile, const CSocketListener *inListener=NULL) THROWABLE;
		virtual ~CSocket		() =0;

	// overloading
	public :

		// expected listener type : CSocketListener
		virtual const CMetaClass *	ListenerMustBe		() const;

	// general abstract socket services
	public :

		// socket descriptor
		int				GetDesc			() const;

		// socket local addr / port
		CString				GetAddr			() const;
		UInt16				GetPort			() const;

		// get sockaddr
		struct sockaddr *		GetSockAddr		() const;

		// close the socket
		virtual void			Close			();

		// call an endless wait of connections/data on the instance
		virtual void 			Wait			() =0;

		// bytes sended on success, -1 on error
		virtual int			Send			(const void *, const int);
		static int			Send			(const int, const void *, const int);
		static int			SendSSL			(SSL *, const void *, const int);

		// bytes received on success, -1 on error, 0 on normal end; this function is an explicit data reception and does not
		// notify the potential associated listener
		virtual int			Receive			(void *, const int);
		static int			Receive			(const int, void *, const int);
		static int			ReceiveSSL		(SSL *, void *, const int);

		// list of local interfaces (each string is of the form device|address)
		CStrings			SiocGifConf		();

	// mutex section
	public :

		// 0 on success
		int				Lock			();
		int				UnLock			();

	// protected attributes
	protected :

		int				m_Desc;
		struct sockaddr *		m_SockAddr;

		pthread_t			m_PThreadMain;
		pthread_mutexattr_t		m_PThreadMutexAttr;
		pthread_mutex_t			m_PThreadMutex;

		SECTION_GENERIC_METACLASS;
};

// metaclass declaration
DECLARE_GENERIC_METACLASS ('sckt', CSocket, CObject);

#endif
