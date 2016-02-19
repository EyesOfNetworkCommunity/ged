/****************************************************************************************************************************************
 ged.h
****************************************************************************************************************************************/

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

#ifndef __GED_H__
#define __GED_H__

#include "gedcommon.h"
#include "cgedbackend.h"

struct TGEDConnInCtx
{
	TGEDConnInCtx				(const CString &, const unsigned long, const int, const SSL *);
	~TGEDConnInCtx				();

	CString					addr;
	unsigned long				port;
	int					desc;
	SSL *					ssl;

	TGEDHttpAswCtx				http;

	time_t					iltm;

	TGEDPktInCtx *				pktctx;
	TBuffer <TGEDPktIn *>			pktin;
	
	bool					rly;
	int					md5;

	unsigned long				kpalv;
	pthread_t				tmrthd;

	TGEDAckCtx				ack;

	int					sndsem;

	#ifdef __GED_TUN__
	TBuffer <CSocketClient *>		tuns;
	#endif
};

struct TGEDConnOutCtx
{
	TGEDConnOutCtx				(const TGEDRelayToCfg &);
	~TGEDConnOutCtx				();

	TGEDRelayToCfg				cfg;

	CString					addr;
	int					desc;
	SSL *					ssl;
	TGEDHttpReqCtx				http;

	TGEDPktInCtx *				pktctx;
	TBuffer <TGEDPktIn *>			pktin;

	time_t					iltm;
	time_t					oltm;

	int					md5;
	unsigned long				totbyt;

	TGEDAckCtx				ack;

	int					sem;
	int					sndsem;
	int					asysem;
	pthread_t				conthd;
	pthread_t				tmrthd;
	pthread_t				plsthd;
	pthread_t				sndthd;
};

//---------------------------------------------------------------------------------------------------------------------------------------
// ged global context class
//---------------------------------------------------------------------------------------------------------------------------------------
class CGEDCtx
{
	public :

		CGEDCtx				(const TGEDCfg &, const bool) THROWABLE;
		virtual ~CGEDCtx		();

	public :

		void				Run				();
		void				Exit				();

		void				SysLog				(int, int, CString);

		int				LockGEDConnInCtx		();
		int				UnLockGEDConnInCtx		();

		int				LockGEDConnInSndCtx		(TGEDConnInCtx *);
		int				UnLockGEDConnInSndCtx		(TGEDConnInCtx *);

		int				LockGEDConnOutCtx		();
		int				UnLockGEDConnOutCtx		();

		int				LockGEDConnOutCtx 		(TGEDConnOutCtx *);
		int				UnLockGEDConnOutCtx 		(TGEDConnOutCtx *);

		int				LockGEDConnOutSendCtx 		(TGEDConnOutCtx *);
		int				UnLockGEDConnOutSendCtx		(TGEDConnOutCtx *);

		int				LockGEDConnOutIncCtx 		(TGEDConnOutCtx *);
		int				UnLockGEDConnOutIncCtx		(TGEDConnOutCtx *);
		
	public :

		TGEDCfg				m_GEDCfg;
		CString				m_Md5Sum;

		CMetaModuleImporter *		m_BackEndImporter;
		CGEDBackEnd *			m_BackEnd;

		TBuffer <CSocketServer *>	m_SocketServer;

		int				m_GEDConnInCtxSem;
		TBuffer <TGEDConnInCtx *>	m_GEDConnInCtx;

		int				m_GEDConnOutCtxSem;
		TBuffer <TGEDConnOutCtx *>	m_GEDConnOutCtx;

		static CGEDCtx *		m_GEDCtx;

	public :

		static void 			m_SSLInfoCallBack		(const SSL *, int, int);
		static int			m_SSLVerifyCallBack		(int, X509_STORE_CTX *);

		static void			m_SigTerm			(int);
};

//---------------------------------------------------------------------------------------------------------------------------------------
// server socket listener
//---------------------------------------------------------------------------------------------------------------------------------------
class CGEDServerListener : public CSocketSSLServerListener
{
	public :

		virtual void			OnConnect			(CObject *, const CString &, const UInt16, const int, 
										 SSL *, bool &, void *&);

		virtual void			OnReceive			(CObject *, const CString &, const UInt16, const int, 
										 SSL *, const void *, const int, void *&);

		virtual void			OnDisconnect			(CObject *, const CString &, const UInt16, const int , 
										 SSL *, void *&);

	public :

		static void 			RecvGEDPktPoolCB 		(const CString &, long, bool, TGEDPktIn *, void *);
		static bool 			RecvGEDPktCB			(const CString &, TGEDPktIn *, TGEDConnInCtx *);

		static void *			m_SyncGEDConnInTimerCB		(void *);
		static void *			m_GEDConnInAckTimerCB		(void *);
		static void *			m_GEDConnInSendCB		(void *);

		static void *			m_GEDConnInFifoCB		(void *);

		static void			SendGEDPktMd5ToSrc		(TGEDConnInCtx *);
		static void			SendGEDPktCloseToSrc		(TGEDConnInCtx *, const int, const SSL *, CString);
		static void			SendGEDPktAckToSrc		(TGEDConnInCtx *, CString);
};

//---------------------------------------------------------------------------------------------------------------------------------------
// client socket listener
//---------------------------------------------------------------------------------------------------------------------------------------
class CGEDClientListener : public CSocketSSLClientListener
{
	public :

		CGEDClientListener		(TGEDConnOutCtx *);

	public :

		virtual void			OnConnect			(CObject *, void *);
		virtual void			OnReceive			(CObject *, const void *, const int);

	public :

		TGEDConnOutCtx *		m_GEDConnOutCtx;

		static void			RecvGEDPktPoolCB		(const CString &, long, bool, TGEDPktIn *, void *);
		static bool			RecvGEDPktCB			(const CString &, TGEDPktIn *, TGEDConnOutCtx *);

		static void *			m_GEDConnOutAckTimerCB		(void *);
	
		static void *			m_ASyncGEDConnOutTimerCB	(void *);
		static void *			m_SyncGEDConnOutPulseCB		(void *);
		static void *			m_GEDConnOutConnectCB		(void *);
		static void *			m_GEDConnOutSendCB		(void *);

		static void			SendGEDPktMd5ToTgt		(TGEDConnOutCtx *);
		static void			SendGEDPktAckToTgt		(TGEDConnOutCtx *, CString);
};

#ifdef __GED_TUN__
//---------------------------------------------------------------------------------------------------------------------------------------
// client tun socket listener
//---------------------------------------------------------------------------------------------------------------------------------------
class CGEDTunClientListener : public CSocketSSLClientListener
{
	public :

		CGEDTunClientListener		(const TGEDConnInCtx *, const int, const CString &);

	public :

		virtual void			OnReceive			(CObject *, const void *, const int);
		virtual void			OnDisconnect			(CObject *inSender);

	public :

		TGEDConnInCtx *			m_GEDConnInCtx;
		int				m_Id;

		const CString			m_TunSpec;
};
#endif

#endif
