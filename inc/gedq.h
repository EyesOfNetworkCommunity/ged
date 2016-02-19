/****************************************************************************************************************************************
 gedq.h
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

#ifndef __GEDQ_H__
#define __GEDQ_H__

#include "gedcommon.h"

//---------------------------------------------------------------------------------------------------------------------------------------
// gedq context class (for automated gedq run)
//---------------------------------------------------------------------------------------------------------------------------------------
class CGEDQCtx
{
	public :

		CGEDQCtx			(const TGEDQCfg &, const CString & =CString()) THROWABLE;
		virtual ~CGEDQCtx		();

	public :

		TGEDQCfg			m_GEDQCfg;
		CString				m_Md5Sum;

		#ifdef __GED_TUN__
		CString				m_TunSpec;
		#endif

		xmlDocPtr			m_XMLdoc;
		xmlNodePtr			m_XMLroot;

		CSocketClient *			m_GEDQSocketClient;
		#ifdef __GED_TUN__
		CSocketServer *			m_GEDQSocketServer;
		#endif

		#ifdef __GED_TUN__
		int				m_GEDQConnInTunsSem;
		TBuffer <int>			m_GEDQConnInTuns;
		#endif

		TGEDHttpReqCtx			m_GEDQHttpReqCtx;

		TGEDAckCtx			m_GEDAckCtx;
		time_t				m_oltm;
		time_t				m_iltm;

		static bool			m_Light;

		static int			m_Res;

	public :

		void				Run			(const TBuffer <TGEDPktOut> & =TBuffer<TGEDPktOut>()) THROWABLE;

	public :

		#ifdef __GED_TUN__
		int				LockGEDQConnInTuns	();
		int				UnLockGEDQConnInTuns	();
		#endif

	public :

		#ifdef __GED_TUN__
		static void *			m_GEDQTunPulseCB	(void *);
		#endif
		static void *			m_GEDQAckTimerCB	(void *);

		static void			m_SigTerm		(int);

	public :

		static CGEDQCtx *		m_GEDQCtx;
};

//---------------------------------------------------------------------------------------------------------------------------------------
// gedq client socket listener
//---------------------------------------------------------------------------------------------------------------------------------------
class CGEDQClientListener : public CSocketSSLClientListener
{
	public :

		CGEDQClientListener		();
		virtual ~CGEDQClientListener	();

	public :

		virtual void 			OnConnect 		(CObject *, void *);
		virtual void 			OnReceive 		(CObject *, const void *inData, const int inLen);

	public :

		static void 			m_RecvGEDPktCB 		(const CString &, long, bool, TGEDPktIn *, void *);
		TGEDPktInCtx * 			m_GEDPktInCtx;
};

#ifdef __GED_TUN__
//---------------------------------------------------------------------------------------------------------------------------------------
// gedq tun socket listener
//---------------------------------------------------------------------------------------------------------------------------------------
class CGEDQServerListener : public CSocketSSLServerListener
{
	public :

		virtual void 			OnConnect 		(CObject *, const CString &, const UInt16, const int, SSL *, bool &, void *&);
		virtual void			OnReceive		(CObject *, const CString &, const UInt16, const int, SSL *, const void *, const int, void *&);
		virtual void			OnDisconnect 		(CObject *, const CString &, const UInt16, const int, SSL *, void *&);
};
#endif

//---------------------------------------------------------------------------------------------------------------------------------------
// gedq cli (for sequential gedq requests [no thread])
//---------------------------------------------------------------------------------------------------------------------------------------
class CGEDQCli 
{
	public :

		CGEDQCli			(const TGEDQCfg &);
		virtual ~CGEDQCli		();

	public :

		virtual bool			Connect			() THROWABLE;
		
		virtual bool			Push			(const long, const CChunk &, const bool =false);
		virtual bool			Update			(const long, const CChunk &, const bool =false);

		virtual bool			Drop			(const long, const long =GED_PKT_REQ_BKD_ACTIVE, const CChunk &inChk=CChunk(), const bool =false);
		virtual bool			Drop			(const TBuffer <UInt32> &, const long =GED_PKT_REQ_BKD_HISTORY);

		virtual TBuffer <TGEDRcd *> *	Peek			(const long =GED_PKT_TYP_ANY, const long =GED_PKT_REQ_BKD_ACTIVE, const CChunk &inChk=CChunk(),
									 const long =GED_PKT_REQ_PEEK_SORT_ASC, const UInt32 =0L, const UInt32 =0L, const UInt32 =0L, const UInt32 =0L);
		static void			Free			(TBuffer <TGEDRcd *> *&);

		virtual void			Disconnect		();

	public :

		TGEDQCfg			m_GEDQCfg;

		CSocketClient *			m_Socket;
		TGEDHttpReqCtx			m_GEDQHttpReqCtx;
		TGEDPktInCtx * 			m_GEDPktInCtx;

	protected :

		virtual bool			PerformRequestWaitAck	(TBuffer <TGEDPktOut> &, TBuffer <TGEDRcd *> * =NULL);
		virtual bool			Push			(const long, const long, const CChunk &, const bool);

		static void 			m_RecvGEDPktCB 		(const CString &, long, bool, TGEDPktIn *, void *);
		static void			m_SigTerm		(int);
};

#endif

