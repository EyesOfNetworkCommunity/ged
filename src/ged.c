/****************************************************************************************************************************************
 ged.c
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

#include "ged.h"

//---------------------------------------------------------------------------------------------------------------------------------------
// connection in context instanciation commodity
//---------------------------------------------------------------------------------------------------------------------------------------
TGEDConnInCtx::TGEDConnInCtx (const CString &inAddr, const unsigned long inPort, const int inDesc, const SSL *inSSL)
	      :addr	     (inAddr),
	       port	     (inPort),
	       desc	     (inDesc),
	       ssl	     (const_cast <SSL *> (inSSL)),
	       pktctx	     (::NewGEDPktInCtx()),
	       sndsem	     (-1),
	       rly	     (false),
	       md5	     (0),
	       kpalv	     (0L)
{
	if ((sndsem = ::semget (IPC_PRIVATE, 1, 0666)) < 0)
		throw new CException (CString("could not allocate semaphore"));
	semun p; p.val = 1; ::semctl (sndsem, 0, SETVAL, p);

	http.srv   = CGEDCtx::m_GEDCtx->m_GEDCfg.http.srv;
	http.vrs   = CGEDCtx::m_GEDCtx->m_GEDCfg.http.vrs;
	http.typ   = CGEDCtx::m_GEDCtx->m_GEDCfg.http.typ;
	http.zlv   = CGEDCtx::m_GEDCtx->m_GEDCfg.http.zlv;
	http.kpalv = false;

	::time (&iltm);
}

//---------------------------------------------------------------------------------------------------------------------------------------
// connection in context deletion commodity
//---------------------------------------------------------------------------------------------------------------------------------------
TGEDConnInCtx::~TGEDConnInCtx ()
{
	for (size_t i=pktin.GetLength(); i>0; i--) ::DeleteGEDPktIn (*pktin[i-1]);

	if (md5 && kpalv)
	{
		::pthread_cancel (tmrthd);
		::pthread_detach (tmrthd);
	}

	::semctl (sndsem, 0, IPC_RMID);

	::DeleteGEDPktInCtx (pktctx);

	#ifdef __GED_TUN__
	for (size_t i=0; i<tuns.GetLength(); i++) delete *tuns[i];
	#endif
}

//---------------------------------------------------------------------------------------------------------------------------------------
// connection out context instanciation commodity
//---------------------------------------------------------------------------------------------------------------------------------------
TGEDConnOutCtx::TGEDConnOutCtx (const TGEDRelayToCfg &inGEDRelayToCfg)
	       :cfg	       (inGEDRelayToCfg),
	        sem	       (-1),
	        desc	       (-1),
	        ssl	       (NULL),
	        pktctx	       (::NewGEDPktInCtx()),
	        sndsem	       (-1),
	        totbyt	       (0L),
	        md5	       (0),
		asysem	       (-1)
{
	if (((sem = ::semget (IPC_PRIVATE, 1, 0666)) < 0) || ((sndsem = ::semget (IPC_PRIVATE, 1, 0666)) < 0))
		throw new CException (CString("could not allocate semaphore"));

	semun p;
	p.val = 1; ::semctl (sem,    0, SETVAL, p); 
	p.val = 0; ::semctl (sndsem, 0, SETVAL, p);

	if (cfg.kpalv == 0)
	{
		if ((asysem = ::semget (IPC_PRIVATE, 1, 0666)) < 0)
			throw new CException (CString("could not allocate semaphore"));
		p.val = 1; ::semctl (asysem, 0, SETVAL, p);
	}

	http.cmd   = cfg.http.cmd;
	http.vrs   = cfg.http.vrs;
	http.agt   = cfg.http.agt;
	http.typ   = cfg.http.typ;
	http.zlv   = cfg.http.zlv;
	http.kpalv = cfg.kpalv > 0;
	http.host  = cfg.bind.addr;

	::time (&iltm);
	::time (&oltm);
}

//---------------------------------------------------------------------------------------------------------------------------------------
// connection out context deletion commodity
//---------------------------------------------------------------------------------------------------------------------------------------
TGEDConnOutCtx::~TGEDConnOutCtx ()
{
	if (sndsem != -1) ::semctl (sndsem, 0, IPC_RMID);
	if (sem    != -1) ::semctl (sem,    0, IPC_RMID);
	if (asysem != -1) ::semctl (asysem, 0, IPC_RMID);

	for (size_t i=pktin.GetLength(); i>0; i--) ::DeleteGEDPktIn (*pktin[i-1]);

	::DeleteGEDPktInCtx (pktctx);
}

//---------------------------------------------------------------------------------------------------------------------------------------
// static context member resolution
//---------------------------------------------------------------------------------------------------------------------------------------
CGEDCtx * CGEDCtx::m_GEDCtx = NULL;

//---------------------------------------------------------------------------------------------------------------------------------------
// sigterm handler (thread M)
//---------------------------------------------------------------------------------------------------------------------------------------
void CGEDCtx::m_SigTerm (int inSig)
{
	::signal (inSig, CGEDCtx::m_SigTerm);

	CString strSig ("SIGNAL ");

	switch (inSig)
	{
		case SIGINT  : strSig += "SIGINT ";  break;
		case SIGTERM : strSig += "SIGTERM "; break;
		case SIGQUIT : strSig += "SIGQUIT";  break;
	}

	CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_CONNECTION, strSig + "(" +  CString((SInt32)inSig) + ") received, exiting...");
	
	if (CGEDCtx::m_GEDCtx->m_BackEnd->Lock() == 0) 
		CGEDCtx::m_GEDCtx->m_BackEnd->Finalize();

	CGEDCtx::m_GEDCtx->Exit();
}

//---------------------------------------------------------------------------------------------------------------------------------------
// global context instanciation (thread M)
//---------------------------------------------------------------------------------------------------------------------------------------
CGEDCtx::CGEDCtx 	    (const TGEDCfg &inGEDCfg, const bool forRun) THROWABLE
        :m_GEDCfg 	    (inGEDCfg),
 	 m_Md5Sum	    (::HashGEDPktCfg(inGEDCfg.pkts)),
	 m_BackEndImporter  (NULL),
	 m_BackEnd	    (NULL),
	 m_GEDConnInCtxSem  (-1),
	 m_GEDConnOutCtxSem (-1)
{
	CGEDCtx::m_GEDCtx = this;

        struct sigaction sa; sa.sa_handler = CGEDCtx::m_SigTerm; sa.sa_flags = SA_NOCLDSTOP;
	::sigaction (SIGINT,  &sa, NULL);
	::sigaction (SIGTERM, &sa, NULL);
	::sigaction (SIGQUIT, &sa, NULL);

	CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_DFT, CString("GED ") + GED_VERSION_STR + " loading [gcc " + 
				   __VERSION__
	#if defined(__GED_NTLM__)
		+ ", __GED_NTLM__"
	#endif
	#if defined(__GED_DEBUG_SQL__)
		+ ", __GED_DEBUG_SQL__"
	#endif
	#if defined(__GED_DEBUG_SEM__)
		+ ", __GED_DEBUG_SEM__"
	#endif
	#if defined(__GED_TUN__)
		+ ", __GED_TUN__"
	#endif
	+ "]");

	if (forRun)
	{
		try
		{
			for (size_t i=0; i<m_GEDCfg.lst.GetLength(); i++)
			{
				if (m_GEDCfg.lst[i]->port != 0L)
				{
					if (m_GEDCfg.tls.ca != CString() || m_GEDCfg.tls.crt != CString() || m_GEDCfg.tls.key != CString())
					{
						m_SocketServer += new CSocketSSLServer (m_GEDCfg.lst[i]->addr, 
											m_GEDCfg.lst[i]->port, 
											m_GEDCfg.tls.ca, 
											m_GEDCfg.tls.crt,
											m_GEDCfg.tls.key,
											m_GEDCfg.tls.vfy,
											m_GEDCfg.tls.cph,
											m_GEDCfg.tls.dhp,
											NULL,
											CGEDCtx::m_SSLInfoCallBack,
											CGEDCtx::m_SSLVerifyCallBack);
					}
					else
					{
						m_SocketServer += new CSocketServer    (m_GEDCfg.lst[i]->addr, 
											m_GEDCfg.lst[i]->port,
											NULL);
					}
				}
				else
				{
					m_SocketServer += new CSocketServer (m_GEDCfg.lst[i]->sock,
									     (const CSocketServerListener*)NULL);
				}
			}
		}
		catch (CException *e)
		{
			throw e;
		}

		if (m_GEDCfg.fifo != CString())
		{
			::remove (m_GEDCfg.fifo.Get());
			if (::mkfifo (m_GEDCfg.fifo.Get(), 0666) != 0)
				throw new CException ("could not create fifo file \"" + m_GEDCfg.fifo + "\"");
			::chmod  (m_GEDCfg.fifo.Get(), S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IWGRP|S_IXGRP|S_IROTH|S_IWOTH|S_IXOTH);
		}
	}

	if (::getuid() == 0L && (m_GEDCfg.uid != 0 || m_GEDCfg.gid != 0))
        {
		if (m_GEDCfg.gid != 0 && ::setgid (m_GEDCfg.gid) < 0)
			throw new CException ("could not set gid to \"" + CString(static_cast <unsigned long> (m_GEDCfg.gid)) + "\"");
		if (m_GEDCfg.uid != 0 && ::setuid (m_GEDCfg.uid) < 0)
			throw new CException ("could not set uid to \"" + CString(static_cast <unsigned long> (m_GEDCfg.uid)) + "\"");
	}

	m_BackEndImporter = new CMetaModuleImporter (m_GEDCfg.bkd, __metaclass(CGEDBackEnd));
	m_BackEnd = static_cast <CGEDBackEnd *> (m_BackEndImporter -> InstanciateModule());

	if (!m_BackEnd->Initialize (m_GEDCfg))
		throw new CException ("could not initialize \"" + m_GEDCfg.bkd + "\" backend");

	CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_BKD_OPERATION, 
				   CString("backend initialization done, ready to handle requests [") + 
				   CString((UInt32)m_GEDCfg.uid) + ":" + CString((UInt32)m_GEDCfg.gid) + "]");
}

//---------------------------------------------------------------------------------------------------------------------------------------
// global ged context deletion (thread M)
//---------------------------------------------------------------------------------------------------------------------------------------
CGEDCtx::~CGEDCtx ()
{
	::exit (0);
}

//---------------------------------------------------------------------------------------------------------------------------------------
// global ged context execution (thread M)
//---------------------------------------------------------------------------------------------------------------------------------------
void CGEDCtx::Run () 
{
	if ((m_GEDConnInCtxSem = ::semget (IPC_PRIVATE, 1, 0666)) < 0)
		throw new CException (CString("could not allocate semaphore"));
	if ((m_GEDConnOutCtxSem = ::semget (IPC_PRIVATE, 1, 0666)) < 0)
		throw new CException (CString("could not allocate semaphore"));
	semun p; p.val = 1; ::semctl (m_GEDConnInCtxSem, 0, SETVAL, p); ::semctl (m_GEDConnOutCtxSem, 0, SETVAL, p);

	for (size_t i=0; i<m_GEDCfg.rlyto.GetLength(); i++)
	{
		TGEDConnOutCtx *newGEDConnOutCtx = new TGEDConnOutCtx (*m_GEDCfg.rlyto[i]);

		if (newGEDConnOutCtx->cfg.syncb)
		{
			if (m_BackEnd->Lock() == 0)
			{
				TBuffer <TGEDRcd*> inGEDRcds (m_BackEnd->Peek(newGEDConnOutCtx->cfg.bind.addr,GED_PKT_REQ_BKD_SYNC,NULL));

				m_BackEnd->UnLock();

				if (LockGEDConnOutIncCtx (newGEDConnOutCtx) == 0)
				{
					for (size_t i=0; i<inGEDRcds.GetLength(); i++)
					{
						newGEDConnOutCtx->totbyt += GED_PKT_FIX_SIZE + static_cast<TGEDSRcd*>(*inGEDRcds[i])->pkt->len;
						::DeleteGEDRcd (*inGEDRcds[i]);
					}

					UnLockGEDConnOutIncCtx (newGEDConnOutCtx);
				}

				CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_CONNECTION, newGEDConnOutCtx->cfg.bind.addr + 
					":" + CString(newGEDConnOutCtx->cfg.bind.port) + " : async target has " + 
					CString(newGEDConnOutCtx->totbyt) + " bytes in sync queue");

				if (LockGEDConnOutIncCtx (newGEDConnOutCtx) == 0)
				{
					if (newGEDConnOutCtx->totbyt >= newGEDConnOutCtx->cfg.syncb) 
						UnLockGEDConnOutSendCtx (newGEDConnOutCtx);

					UnLockGEDConnOutIncCtx (newGEDConnOutCtx);
				}
			}
		}

		m_GEDConnOutCtx += newGEDConnOutCtx;

		if (::pthread_create (&newGEDConnOutCtx->conthd, NULL, CGEDClientListener::m_GEDConnOutConnectCB, 
			newGEDConnOutCtx) != 0) throw new CException ("could not create pthread");
	}

	CString inStrBindings;
	for (size_t i=0; i<m_GEDCfg.lst.GetLength(); i++)
	{
		if (m_GEDCfg.lst[i]->port != 0L)
		{
			inStrBindings += m_GEDCfg.lst[i]->addr + ":" + CString(m_GEDCfg.lst[i]->port);
			if (m_GEDCfg.tls.ca != CString() || m_GEDCfg.tls.crt != CString() || m_GEDCfg.tls.key != CString()) inStrBindings += "/SSL";
		}
		else
			inStrBindings += m_GEDCfg.lst[i]->sock;
		if (i<m_GEDCfg.lst.GetLength()-1) inStrBindings += ", ";
	}

	CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_CONNECTION, 
				   CString("binding initialization done, ready to handle connections [" + inStrBindings + "]"));

	CGEDServerListener *theGEDServerListener = new CGEDServerListener();

	for (size_t i=0; i<m_SocketServer.GetLength(); i++) 
		(*m_SocketServer[i]) -> AssignListener (theGEDServerListener);

	if (m_GEDCfg.fifo != CString())
	{
		pthread_t pth;

		if (::pthread_create (&pth, NULL, CGEDServerListener::m_GEDConnInFifoCB, NULL) != 0) 
			throw new CException ("could not create pthread");

		CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_CONNECTION, 
				   CString("fifo initialization done, ready to handle pipe [" + m_GEDCfg.fifo + "]"));
	}

	if (m_SocketServer.GetLength() > 0) (*m_SocketServer[0])->Wait();
}

//---------------------------------------------------------------------------------------------------------------------------------------
// global ged context exit (thread Any !!!)
//---------------------------------------------------------------------------------------------------------------------------------------
void CGEDCtx::Exit ()
{
	if (m_SocketServer.GetLength() > 0) (*m_SocketServer[0])->Close();
}

//---------------------------------------------------------------------------------------------------------------------------------------
// global ged context syslog facility
//---------------------------------------------------------------------------------------------------------------------------------------
void CGEDCtx::SysLog (int inKind, int inLev, CString inLog)
{
	if ((m_GEDCfg.loglev & inLev) == 0L) return;

	if (m_GEDCfg.logloc == GED_SYSLOG_LOC_DFT)
	{
		time_t intime; ::time (&intime); struct tm tm; ::localtime_r (&intime, &tm);
		switch (inKind)
		{
			case GED_SYSLOG_ERROR :
			{
				::fprintf (stderr, "[%.2d/%.2d/%.2d %.2d:%.2d:%.2d] ERROR   : %s\n", tm.tm_mday, tm.tm_mon+1, 
					   tm.tm_year%100, tm.tm_hour, tm.tm_min, tm.tm_sec, inLog.Get());
                        }
			break;
			case GED_SYSLOG_WARNING :
			{
				::fprintf (stderr, "[%.2d/%.2d/%.2d %.2d:%.2d:%.2d] WARNING : %s\n", tm.tm_mday, tm.tm_mon+1,
					   tm.tm_year%100, tm.tm_hour, tm.tm_min, tm.tm_sec, inLog.Get());
			}
			break;
			case GED_SYSLOG_INFO :
			{
				::fprintf (stdout, "[%.2d/%.2d/%.2d %.2d:%.2d:%.2d] INFO    : %s\n", tm.tm_mday, tm.tm_mon+1, 
					   tm.tm_year%100, tm.tm_hour, tm.tm_min, tm.tm_sec, inLog.Get());
			}
			break;
		}
	}
	else
		::syslog (m_GEDCfg.logloc|inKind, "%s", inLog.Get());
}

//----------------------------------------------------------------------------------------------------------------------------------------
// ssl info callback
//----------------------------------------------------------------------------------------------------------------------------------------
void CGEDCtx::m_SSLInfoCallBack (const SSL *inSSL, int inWhere, int inRet)
{
	char *op, *state = (char *) ::SSL_state_string_long ((SSL *)inSSL);

        if ((inWhere & ~SSL_ST_MASK) & SSL_ST_CONNECT) 
		op = (char *)"SSL_connect";
	else if ((inWhere & ~SSL_ST_MASK) & SSL_ST_ACCEPT)
		op = (char *)"SSL_accept";
	else 
		op = (char *)"undefined";

        if (inWhere & SSL_CB_LOOP)
	{
		CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_SSL, CString(op) + ":" + state);
        } 
	else if (inWhere & SSL_CB_ALERT)
	{
		char *atype = (char *) ::SSL_alert_type_string_long (inRet);
		char *adesc = (char *) ::SSL_alert_desc_string_long (inRet);
		op = (inWhere & SSL_CB_READ) ? (char*)"read" : (char*)"write";

		CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_SSL, CString(op) + ":" + atype + ":" + adesc);
	}
	else if (inWhere & SSL_CB_EXIT)
	{
		if (inRet == 0)
		{
			CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_SSL, CString(op) + CString(" failed in ") + state);
		} 
		else if (inRet < 0)
		{
			CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_SSL, CString(op) + CString(" error in ") + state);
		}
	}
}

//----------------------------------------------------------------------------------------------------------------------------------------
// SSL verify callback
//----------------------------------------------------------------------------------------------------------------------------------------
int CGEDCtx::m_SSLVerifyCallBack (int inOk, X509_STORE_CTX *inCtx)
{
	X509 *cert     = ::X509_STORE_CTX_get_current_cert (inCtx);
	long  errnum   = ::X509_STORE_CTX_get_error 	   (inCtx);
	long  errdepth = ::X509_STORE_CTX_get_error_depth  (inCtx);

	X509_NAME *subject = ::X509_get_subject_name (cert);
	X509_NAME *issuer  = ::X509_get_issuer_name  (cert);
        char *sname 	   = ::X509_NAME_oneline     (subject, NULL, 0);
        char *iname 	   = ::X509_NAME_oneline     (issuer,  NULL, 0);

	CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_SSL, "SSL certificate verification: depth: " + CString(errdepth) + 
		", err: " + CString(errnum) + ", subject: " + sname);

//	char data[256]; X509_NAME_get_text_by_NID (subject, NID_commonName, data, 256); data[255]=0;
//	fprintf (stdout, "SSL %s\n", data);

	CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_SSL, "SSL issuer: " + CString(iname));

	if (!inOk)
	{
		char *certerr = (char *) ::X509_verify_cert_error_string (errnum);
		CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_SSL, "SSL certificate verification error " + CString(certerr));
	}

        if (sname) ::CRYPTO_free (sname);
        if (iname) ::CRYPTO_free (iname);

	return inOk;
}

//---------------------------------------------------------------------------------------------------------------------------------------
// connection in context buffer lock
//---------------------------------------------------------------------------------------------------------------------------------------
int CGEDCtx::LockGEDConnInCtx ()
{
#ifdef __GED_DEBUG_SEM__
	SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_DEBUG, "SEM CGEDCtx::LockGEDConnInCtx() " + CString(::pthread_self()));
#endif
	struct sembuf op[1]; op[0].sem_num = 0; op[0].sem_op = -1; op[0].sem_flg = 0;
	int res =  ::semop (m_GEDConnInCtxSem, op, 1);
#ifdef __GED_DEBUG_SEM__
	SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_DEBUG, "SEM CGEDCtx::LockGEDConnInCtx() done [" + CString((long)res) + "] " + 
		CString(::pthread_self()));
#endif
	return res;
}

//---------------------------------------------------------------------------------------------------------------------------------------
// connection in context buffer unlock
//---------------------------------------------------------------------------------------------------------------------------------------
int CGEDCtx::UnLockGEDConnInCtx ()
{
#ifdef __GED_DEBUG_SEM__
	SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_DEBUG, "SEM CGEDCtx::UnLockGEDConnInCtx() " + CString(::pthread_self()));
#endif
	struct sembuf op[1]; op[0].sem_num = 0; op[0].sem_op = 1; op[0].sem_flg = 0;
	int res = ::semop (m_GEDConnInCtxSem, op, 1);
#ifdef __GED_DEBUG_SEM__
	SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_DEBUG, "SEM CGEDCtx::UnLockGEDConnInCtx() done [" + CString((long)res) + "] " + 
		CString(::pthread_self()));
#endif
	return res;
}

//---------------------------------------------------------------------------------------------------------------------------------------
// connection in emission context buffer lock
//---------------------------------------------------------------------------------------------------------------------------------------
int CGEDCtx::LockGEDConnInSndCtx (TGEDConnInCtx *inGEDConnInCtx)
{
#ifdef __GED_DEBUG_SEM__
	SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_DEBUG, "SEM CGEDCtx::LockGEDConnInSndCtx(TGEDConnInCtx*) " + CString(::pthread_self()));
#endif
	struct sembuf op[1]; op[0].sem_num = 0; op[0].sem_op = -1; op[0].sem_flg = 0;
	int res =  ::semop (inGEDConnInCtx->sndsem, op, 1);
#ifdef __GED_DEBUG_SEM__
	SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_DEBUG, "SEM CGEDCtx::LockGEDConnInSndCtx(TGEDConnInCtx*) done [" + CString((long)res) + 
		"] " + CString(::pthread_self()));
#endif
	return res;
}

//---------------------------------------------------------------------------------------------------------------------------------------
// connection in emission context buffer unlock
//---------------------------------------------------------------------------------------------------------------------------------------
int CGEDCtx::UnLockGEDConnInSndCtx (TGEDConnInCtx *inGEDConnInCtx)
{
#ifdef __GED_DEBUG_SEM__
	SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_DEBUG, "SEM CGEDCtx::UnLockGEDConnInSndCtx(TGEDConnInCtx*) " + CString(::pthread_self()));
#endif
	struct sembuf op[1]; op[0].sem_num = 0; op[0].sem_op = 1; op[0].sem_flg = 0;
	int res = ::semop (inGEDConnInCtx->sndsem, op, 1);
#ifdef __GED_DEBUG_SEM__
	SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_DEBUG, "SEM CGEDCtx::UnLockGEDConnInSndCtx(TGEDConnInCtx*) done [" + CString((long)res) + 
		"] " + CString(::pthread_self()));
#endif
	return res;
}

//---------------------------------------------------------------------------------------------------------------------------------------
// connection out context buffer lock
//---------------------------------------------------------------------------------------------------------------------------------------
int CGEDCtx::LockGEDConnOutCtx ()
{
#ifdef __GED_DEBUG_SEM__
	SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_DEBUG, "SEM CGEDCtx::LockGEDConnOutCtx() " + CString(::pthread_self()));
#endif
	struct sembuf op[1]; op[0].sem_num = 0; op[0].sem_op = -1; op[0].sem_flg = 0;
	int res = ::semop (m_GEDConnOutCtxSem, op, 1);
#ifdef __GED_DEBUG_SEM__
	SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_DEBUG, "SEM CGEDCtx::LockGEDConnOutCtx() done [" + CString((long)res) + "] " + 
		CString(::pthread_self()));
#endif
	return res;
}

//---------------------------------------------------------------------------------------------------------------------------------------
// connection out context buffer unlock
//---------------------------------------------------------------------------------------------------------------------------------------
int CGEDCtx::UnLockGEDConnOutCtx ()
{
#ifdef __GED_DEBUG_SEM__
	SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_DEBUG, "SEM CGEDCtx::UnLockGEDConnOutCtx() " + CString(::pthread_self()));
#endif
	struct sembuf op[1]; op[0].sem_num = 0; op[0].sem_op = 1; op[0].sem_flg = 0;
	int res = ::semop (m_GEDConnOutCtxSem, op, 1);
#ifdef __GED_DEBUG_SEM__
	CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_DEBUG, "SEM CGEDCtx::UnLockGEDConnOutCtx() done [" + 
		CString((long)res) + "] " + CString(::pthread_self()));
#endif
	return res;
}

//---------------------------------------------------------------------------------------------------------------------------------------
// connection out context lock 
//---------------------------------------------------------------------------------------------------------------------------------------
int CGEDCtx::LockGEDConnOutCtx (TGEDConnOutCtx *inGEDConnOutCtx)
{
#ifdef __GED_DEBUG_SEM__
	SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_DEBUG, "SEM CGEDCtx::LockGEDConnOutCtx(TGEDConnOutCtx*) " + CString(::pthread_self()));
#endif
	struct sembuf op[1]; op[0].sem_num = 0; op[0].sem_op = -1; op[0].sem_flg = 0;
	int res = ::semop (inGEDConnOutCtx->sem, op, 1);
#ifdef __GED_DEBUG_SEM__
	SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_DEBUG, "SEM CGEDCtx::LockGEDConnOutCtx(TGEDConnOutCtx*) done [" + 
		CString((long)res) + "] " + CString(::pthread_self()));
#endif
	return res;
}

//---------------------------------------------------------------------------------------------------------------------------------------
// connection out context unlock
//---------------------------------------------------------------------------------------------------------------------------------------
int CGEDCtx::UnLockGEDConnOutCtx (TGEDConnOutCtx *inGEDConnOutCtx)
{
#ifdef __GED_DEBUG_SEM__
	static unsigned long n=0;
	SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_DEBUG, "SEM CGEDCtx::UnLockGEDConnOutCtx(TGEDConnOutCtx*) " + CString(::pthread_self()));
#endif
	struct sembuf op[1]; op[0].sem_num = 0; op[0].sem_op = 1; op[0].sem_flg = 0;
	int res = ::semop (inGEDConnOutCtx->sem, op, 1);
#ifdef __GED_DEBUG_SEM__
	SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_DEBUG, "SEM CGEDCtx::UnLockGEDConnOutCtx(TGEDConnOutCtx*) done [" + 
		CString((long)res) + "] " + CString(::pthread_self()));
#endif
	return res;
}

//---------------------------------------------------------------------------------------------------------------------------------------
// connection out context send lock
//---------------------------------------------------------------------------------------------------------------------------------------
int CGEDCtx::LockGEDConnOutSendCtx (TGEDConnOutCtx *inGEDConnOutCtx)
{
#ifdef __GED_DEBUG_SEM__
	SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_DEBUG, "SEM CGEDCtx::LockGEDConnOutSendCtx(TGEDConnOutCtx*) " + CString(::pthread_self()));
#endif
	struct sembuf op[1]; op[0].sem_num = 0; op[0].sem_op = -1; op[0].sem_flg = 0;
	int res = ::semop (inGEDConnOutCtx->sndsem, op, 1);
#ifdef __GED_DEBUG_SEM__
	SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_DEBUG, "SEM CGEDCtx::LockGEDConnOutSendCtx(TGEDConnOutCtx*) done [" + 
		CString((long)res) + "] " + CString(::pthread_self()));
#endif
	return res;
}

//---------------------------------------------------------------------------------------------------------------------------------------
// connection out context send unlock
//---------------------------------------------------------------------------------------------------------------------------------------
int CGEDCtx::UnLockGEDConnOutSendCtx (TGEDConnOutCtx *inGEDConnOutCtx)
{
#ifdef __GED_DEBUG_SEM__
	SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_DEBUG, "SEM CGEDCtx::UnLockGEDConnOutSendCtx(TGEDConnOutCtx*) " + 
		CString(::pthread_self()));
#endif
	struct sembuf op[1]; op[0].sem_num = 0; op[0].sem_op = 1; op[0].sem_flg = 0;
	int res = ::semop (inGEDConnOutCtx->sndsem, op, 1);
#ifdef __GED_DEBUG_SEM__
	SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_DEBUG, "SEM CGEDCtx::UnLockGEDConnOutSendCtx(TGEDConnOutCtx*) done [" + 
		CString((long)res) + "] " + CString(::pthread_self()));
#endif
	return res;
}

//---------------------------------------------------------------------------------------------------------------------------------------
// connection out context inc lock
//---------------------------------------------------------------------------------------------------------------------------------------
int CGEDCtx::LockGEDConnOutIncCtx (TGEDConnOutCtx *inGEDConnOutCtx)
{
#ifdef __GED_DEBUG_SEM__
	SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_DEBUG, "SEM CGEDCtx::LockGEDConnOutIncCtx(TGEDConnOutCtx*) " + CString(::pthread_self()));
#endif
	struct sembuf op[1]; op[0].sem_num = 0; op[0].sem_op = -1; op[0].sem_flg = 0;
	int res = ::semop (inGEDConnOutCtx->asysem, op, 1);
#ifdef __GED_DEBUG_SEM__
	SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_DEBUG, "SEM CGEDCtx::LockGEDConnOutIncCtx(TGEDConnOutCtx*) done [" + 
		CString((long)res) + "] " + CString(::pthread_self()));
#endif
	return res;
}

//---------------------------------------------------------------------------------------------------------------------------------------
// connection out context inc unlock
//---------------------------------------------------------------------------------------------------------------------------------------
int CGEDCtx::UnLockGEDConnOutIncCtx (TGEDConnOutCtx *inGEDConnOutCtx)
{
#ifdef __GED_DEBUG_SEM__
	SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_DEBUG, "SEM CGEDCtx::UnLockGEDConnOutIncCtx(TGEDConnOutCtx*) " + 
		CString(::pthread_self()));
#endif
	struct sembuf op[1]; op[0].sem_num = 0; op[0].sem_op = 1; op[0].sem_flg = 0;
	int res = ::semop (inGEDConnOutCtx->asysem, op, 1);
#ifdef __GED_DEBUG_SEM__
	SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_DEBUG, "SEM CGEDCtx::UnLockGEDConnOutIncCtx(TGEDConnOutCtx*) done [" + 
		CString((long)res) + "] " + CString(::pthread_self()));
#endif
	return res;
}

//----------------------------------------------------------------------------------------------------------------------------------------
// server side new connection handling (thread I1)
//----------------------------------------------------------------------------------------------------------------------------------------
void CGEDServerListener::OnConnect (CObject *, const CString &inAddr, const UInt16 inPort, const int inDesc, SSL *inSSL, bool &ioAccept,
				    void *&outParam)
{
	__PTHREAD_DISABLE_CANCEL__

	if (CGEDCtx::m_GEDCtx->LockGEDConnInCtx() == 0)
	{
		if (CGEDCtx::m_GEDCtx->m_GEDCfg.maxin > 0)
		{
			if (CGEDCtx::m_GEDCtx->m_GEDConnInCtx.GetLength() >= CGEDCtx::m_GEDCtx->m_GEDCfg.maxin)
			{
				CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_WARNING, GED_SYSLOG_LEV_CONNECTION, inAddr + ":" +
					CString((UInt32)inPort) + " : connection ignored, maximum " + 
					CString((SInt32)CGEDCtx::m_GEDCtx->m_GEDCfg.maxin) + " allowed connections already reached");

				ioAccept = false;

				CGEDCtx::m_GEDCtx->UnLockGEDConnInCtx();

				__PTHREAD_ENABLE_CANCEL__

				return;
			}
		}
		
		CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_CONNECTION, inAddr + ":" + CString((UInt32)inPort) + 
					   " : connection accepted");
		
		outParam = new TGEDConnInCtx (inAddr, inPort, inDesc, inSSL);

		CGEDCtx::m_GEDCtx->m_GEDConnInCtx += reinterpret_cast <TGEDConnInCtx *> (outParam);

		CGEDCtx::m_GEDCtx->UnLockGEDConnInCtx();
	}
	else
	{
		ioAccept = false;
	}

	__PTHREAD_ENABLE_CANCEL__
}

//----------------------------------------------------------------------------------------------------------------------------------------
// server side raw data reception handling (thread I2)
//----------------------------------------------------------------------------------------------------------------------------------------
void CGEDServerListener::OnReceive (CObject *, const CString &, const UInt16, const int, SSL *, const void *inData, const int inLen, void *&inParam)
{
	int inRes=0; TGEDConnInCtx *inGEDConnInCtx = reinterpret_cast <TGEDConnInCtx *> (inParam);

	if (inGEDConnInCtx->port == 0L)
		inRes = ::RecvRawGEDPktFromBuf  (const_cast <void *> (inData), inLen, inGEDConnInCtx->pktctx, CGEDServerListener::RecvGEDPktPoolCB, inGEDConnInCtx);
	else
		inRes = ::RecvHttpGEDPktFromBuf (const_cast <void *> (inData), inLen, inGEDConnInCtx->pktctx, CGEDServerListener::RecvGEDPktPoolCB, inGEDConnInCtx);

	if (inRes != inLen)
	{
		CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_CONNECTION, inGEDConnInCtx->addr + ":" + 
			CString(inGEDConnInCtx->port) + " : data reception corruption, closing socket");

		::shutdown (inGEDConnInCtx->desc, SHUT_RDWR);
		::close    (inGEDConnInCtx->desc);
	}
}

//----------------------------------------------------------------------------------------------------------------------------------------
// server side disconnection handling (thread I2)
//----------------------------------------------------------------------------------------------------------------------------------------
void CGEDServerListener::OnDisconnect (CObject *, const CString &inAddr, const UInt16 inPort, const int , SSL *, void *&ioParam)
{
	TGEDConnInCtx *inGEDConnInCtx = reinterpret_cast <TGEDConnInCtx *> (ioParam);

	__PTHREAD_DISABLE_CANCEL__

	if (CGEDCtx::m_GEDCtx->LockGEDConnInCtx() == 0)
	{
		CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_CONNECTION, inAddr + ":" + CString((UInt32)inPort) + 
					   " : disconnection");

		for (size_t i=CGEDCtx::m_GEDCtx->m_GEDConnInCtx.GetLength(); i>0; i--)
			if ((*CGEDCtx::m_GEDCtx->m_GEDConnInCtx[i-1]) == inGEDConnInCtx)
			{
				CGEDCtx::m_GEDCtx->m_GEDConnInCtx.Delete (i-1, 1);
				break;
			}

		if (CGEDCtx::m_GEDCtx->LockGEDConnInSndCtx (inGEDConnInCtx) == 0)
			delete inGEDConnInCtx;

		CGEDCtx::m_GEDCtx->UnLockGEDConnInCtx();
	}

	__PTHREAD_ENABLE_CANCEL__
}

//----------------------------------------------------------------------------------------------------------------------------------------
// server side packets pool reception callback (thread I2)
//----------------------------------------------------------------------------------------------------------------------------------------
void CGEDServerListener::RecvGEDPktPoolCB (const CString &inGEDHttpHeader, long inGEDPktNum, bool inGEDPktLast, TGEDPktIn *inGEDPktIn, void *inParam)
{
	TGEDConnInCtx *inGEDConnInCtx = reinterpret_cast <TGEDConnInCtx *> (inParam);

	::time (&inGEDConnInCtx->iltm);

	if (inGEDPktNum == 0L && (inGEDHttpHeader != CString()))
	{
		CString outStr (inGEDConnInCtx->addr + ":" + CString(inGEDConnInCtx->port) + " : " + inGEDHttpHeader);
		outStr.Substitute (GED_HTTP_REGEX_CRLF, CString(" "));

		CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_HTTP_HEADER, outStr);
	}
  if (GED_MAJ(inGEDPktIn->vrs) < 1 && GED_MIN(inGEDPktIn->vrs) < 12) // Asume compatibility with last 1.2 series update.
  {
  	if (GED_MAJ(inGEDPktIn->vrs) != GED_MAJOR || GED_MIN(inGEDPktIn->vrs) != GED_MINOR)
   	{
    		CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_CONNECTION, inGEDConnInCtx->addr + ":" + 
	    		CString(inGEDConnInCtx->port) + " : listener version is " + GED_VERSION_STR + 
		    	", received a packet version " + CString(GED_MAJ(inGEDPktIn->vrs)) + "." + CString(GED_MIN(inGEDPktIn->vrs)) + 
			   " ." + CString(GED_PAT(inGEDPktIn->vrs)) + ", it's prior to version 1.2-12, ignoring");
      
		  return;
	   }
  }

	if (inGEDConnInCtx->iltm < inGEDPktIn->tv.tv_sec)
	{
		CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_WARNING, GED_SYSLOG_LEV_CONNECTION, inGEDConnInCtx->addr + ":" + 
			CString(inGEDConnInCtx->port) + " : packet timestamp in the future, consider adjusting systems time !");
	}

	inGEDConnInCtx->pktin += ::NewGEDPktIn (inGEDPktIn);

	if (inGEDPktLast)
	{
		bool ok=true; for (size_t i=inGEDConnInCtx->pktin.GetLength(), j=0; i>0 && ok; i--, j++)
			ok = CGEDServerListener::RecvGEDPktCB (inGEDHttpHeader, *inGEDConnInCtx->pktin[j], inGEDConnInCtx);
	
		for (size_t i=inGEDConnInCtx->pktin.GetLength(); i>0; i--) ::DeleteGEDPktIn (*inGEDConnInCtx->pktin[i-1]);
		inGEDConnInCtx->pktin.Reset();

		if (inGEDConnInCtx->port != 0L)
		{
			size_t n; if (ok && CString(inGEDHttpHeader).ToLower().Find(CString("content-md5: "),0,&n))
			{
				char outDigest[128]; n = ::sscanf (inGEDHttpHeader.Get()+n, "%*s %s", outDigest); if (n>=1)
				{
					__PTHREAD_DISABLE_CANCEL__

					if (CGEDCtx::m_GEDCtx->LockGEDConnInSndCtx (inGEDConnInCtx) == 0)
					{
						CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_DETAIL_CONN, 
							inGEDConnInCtx->addr + ":" + CString(inGEDConnInCtx->port) + 
							" : sending requested sequence ack");

						CGEDServerListener::SendGEDPktAckToSrc (inGEDConnInCtx, CString(outDigest));

						CGEDCtx::m_GEDCtx->UnLockGEDConnInSndCtx (inGEDConnInCtx);
					}

					__PTHREAD_ENABLE_CANCEL__
				}
			}
		}
		else
		{
			if (CGEDCtx::m_GEDCtx->LockGEDConnInSndCtx (inGEDConnInCtx) == 0)
			{
				CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_DETAIL_CONN, 
					inGEDConnInCtx->addr + ":" + CString(inGEDConnInCtx->port) + 
					" : sending requested sequence close");

				CGEDServerListener::SendGEDPktCloseToSrc (inGEDConnInCtx, inGEDConnInCtx->desc, NULL, CString(ok?"":"failed to handle the request"));

				CGEDCtx::m_GEDCtx->UnLockGEDConnInSndCtx (inGEDConnInCtx);
			}
		}
	}
}

//----------------------------------------------------------------------------------------------------------------------------------------
// server side packet reception callback (thread I2)
//----------------------------------------------------------------------------------------------------------------------------------------
bool CGEDServerListener::RecvGEDPktCB (const CString &inGEDHttpHeader, TGEDPktIn *inGEDPktIn, TGEDConnInCtx *inGEDConnInCtx)
{
	CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_PKT_HEADER, inGEDConnInCtx->addr + ":" + 
		CString(inGEDConnInCtx->port) + " : " + ::GEDPktInToHeaderString(inGEDPktIn));

	CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_PKT_CONTENT, inGEDConnInCtx->addr + ":" + 
		CString(inGEDConnInCtx->port) + " : " + ::GEDPktInToContentString (inGEDPktIn,
		::GEDPktInToCfg (inGEDPktIn, CGEDCtx::m_GEDCtx->m_GEDCfg.pkts)));

	switch (inGEDPktIn->req & GED_PKT_REQ_MASK)
	{
		case GED_PKT_REQ_NONE :
		{
			switch (inGEDPktIn->typ)
			{
				case GED_PKT_TYP_MD5 :
				{
					CString inMd5 (reinterpret_cast <char *> (inGEDPktIn->data));

					if (inMd5 != CGEDCtx::m_GEDCtx->m_Md5Sum)
					{
						CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_DETAIL_CONN, 
							 inGEDConnInCtx->addr +
							 ":" + CString(inGEDConnInCtx->port) + " : md5sum rejected, closing socket");

						__PTHREAD_DISABLE_CANCEL__

						if (CGEDCtx::m_GEDCtx->LockGEDConnInSndCtx (inGEDConnInCtx) == 0)
						{
							CGEDServerListener::SendGEDPktCloseToSrc (inGEDConnInCtx, inGEDConnInCtx->desc, inGEDConnInCtx->ssl, 
								CString("md5sum rejected, configurations differ"));

							CGEDCtx::m_GEDCtx->UnLockGEDConnInSndCtx (inGEDConnInCtx);
						}

						__PTHREAD_ENABLE_CANCEL__

						return false;
					}

					CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_DETAIL_CONN, inGEDConnInCtx->addr + ":" +
								   CString(inGEDConnInCtx->port) + " : md5sum accepted");

					inGEDConnInCtx->http.kpalv = CString(inGEDHttpHeader).ToLower().Find(CString(": keep-alive"));
					inGEDConnInCtx->rly	   = inGEDPktIn->req & GED_PKT_REQ_SRC_RELAY;
					inGEDConnInCtx->kpalv      = *(unsigned long *)(((char*)inGEDPktIn->data)+inMd5.GetLength()+1);
					inGEDConnInCtx->md5	   = 1;

					if (inGEDConnInCtx->rly)
					{
						if (CGEDCtx::m_GEDCtx->m_GEDCfg.alwsyncfrm.GetLength() > 0)
						{
							bool found=false; size_t n=0; for (size_t i=CGEDCtx::m_GEDCtx->m_GEDCfg.alwsyncfrm.GetLength(); i>0 && !found; i--)
								if (inGEDConnInCtx->addr.Find (*CGEDCtx::m_GEDCtx->m_GEDCfg.alwsyncfrm[i-1], 0, &n) && n==0) found=true;

							if (!found)
							{
								CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_WARNING, GED_SYSLOG_LEV_CONNECTION, inGEDConnInCtx->addr + ":" +
									CString((UInt32)inGEDConnInCtx->port) + " : connection rejected, not an authorized sync source");

								__PTHREAD_DISABLE_CANCEL__

								if (CGEDCtx::m_GEDCtx->LockGEDConnInSndCtx (inGEDConnInCtx) == 0)
								{
									CGEDServerListener::SendGEDPktCloseToSrc (inGEDConnInCtx, inGEDConnInCtx->desc, inGEDConnInCtx->ssl, 
										inGEDConnInCtx->addr + " is not authorized to synchronize");

									CGEDCtx::m_GEDCtx->UnLockGEDConnInSndCtx (inGEDConnInCtx);
								}

								__PTHREAD_ENABLE_CANCEL__

								return false;
							}
						}
					}

					if (inGEDConnInCtx->kpalv)
						if (::pthread_create (&inGEDConnInCtx->tmrthd, NULL, 
							CGEDServerListener::m_SyncGEDConnInTimerCB, inGEDConnInCtx) != 0)
								throw new CException ("could not create pthread");

					if (inGEDConnInCtx->rly)
					{
						pthread_t thd; if (::pthread_create (&thd, NULL, CGEDServerListener::m_GEDConnInSendCB,
							inGEDConnInCtx) != 0) throw new CException ("could not create pthread");
					}
				}
				break;

				case GED_PKT_TYP_ACK :
				{
					__PTHREAD_DISABLE_CANCEL__

					if (CGEDCtx::m_GEDCtx->LockGEDConnInCtx() == 0)
					{
						if (CString((char*)inGEDPktIn->data) == inGEDConnInCtx->ack.hash)
						{
							CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_DETAIL_CONN, 
								inGEDConnInCtx->addr + ":" + CString(inGEDConnInCtx->port) + 
								" : ack reception " + CString((char*)inGEDPktIn->data) + " (ok)");

							inGEDConnInCtx->ack.ack = true;
	
							::pthread_cancel (inGEDConnInCtx->ack.tmrthd);
							::pthread_detach (inGEDConnInCtx->ack.tmrthd);

							::UnLockGEDAckCtx (&inGEDConnInCtx->ack);
						}
						else
							CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_DETAIL_CONN, 
								inGEDConnInCtx->addr + ":" + CString(inGEDConnInCtx->port) + 
								" : ack reception " + CString((char*)inGEDPktIn->data) + " (no match)");

						CGEDCtx::m_GEDCtx->UnLockGEDConnInCtx();
					}

					__PTHREAD_ENABLE_CANCEL__
				}
				break;

				case GED_PKT_TYP_RECORD :
				{
					CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_WARNING, GED_SYSLOG_LEV_DETAIL_CONN, inGEDConnInCtx->addr +
						":" + CString(inGEDConnInCtx->port) + " : record reception, should not happen");
				}
				break;

				case GED_PKT_TYP_CLOSE :
				{
					CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_WARNING, GED_SYSLOG_LEV_DETAIL_CONN, inGEDConnInCtx->addr +
						":" + CString(inGEDConnInCtx->port) + " : close requested, should not happen");
				}
				break;

				case GED_PKT_TYP_PULSE :
				{
					CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_DETAIL_CONN, inGEDConnInCtx->addr +
						":" + CString(inGEDConnInCtx->port) + " : pulse reception");
				}
				break;

				#ifdef __GED_TUN__
				case GED_PKT_TYP_OPEN_TUN :
				{
					if (!CGEDCtx::m_GEDCtx->m_GEDCfg.maytun)
					{
						CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_DETAIL_CONN, inGEDConnInCtx->addr +
								":" + CString(inGEDConnInCtx->port) + " : requesting unauthorized tun initialization, ignoring request");

						return false;
					}
					
					bool found=false; size_t n=0; for (size_t i=CGEDCtx::m_GEDCtx->m_GEDCfg.alwtunfrm.GetLength(); i>0 && !found; i--)
						if (inGEDConnInCtx->addr.Find (*CGEDCtx::m_GEDCtx->m_GEDCfg.alwtunfrm[i-1], 0, &n) && n==0) found=true;

					if (!found)
					{
						CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, inGEDConnInCtx->addr +
							":" + CString(inGEDConnInCtx->port) + " : is not an authorized tun source, ignoring request");

						return false;
					}

					int inId = *reinterpret_cast <int *> (inGEDPktIn->data); CString inTunSpec (reinterpret_cast <char *> (inGEDPktIn->data) + sizeof(int));

					if (CGEDCtx::m_GEDCtx->m_GEDCfg.alwtunto.GetLength() > 0)
					{
						bool found=false; size_t n=0; for (size_t i=CGEDCtx::m_GEDCtx->m_GEDCfg.alwtunto.GetLength(); i>0 && !found; i--)
							if (inTunSpec.Cut(CString(":"))[0]->Find (*CGEDCtx::m_GEDCtx->m_GEDCfg.alwtunto[i-1], 0, &n) && n==0) found=true;

						if (!found)
						{
							CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_WARNING, GED_SYSLOG_LEV_DETAIL_CONN, inGEDConnInCtx->addr + ":" + 
								CString(inGEDConnInCtx->port) + " : " + *inTunSpec.Cut(CString(":"))[0] + 
								" is not an authorized tun target, ignoring request");

							return false;
						}
					}

					__PTHREAD_DISABLE_CANCEL__

					if (CGEDCtx::m_GEDCtx->LockGEDConnInCtx() == 0)
					{
						if (CGEDCtx::m_GEDCtx->m_GEDCfg.maxtun > 0)
						{
							if (inGEDConnInCtx->tuns.GetLength() >= CGEDCtx::m_GEDCtx->m_GEDCfg.maxtun)
							{
								CGEDCtx::m_GEDCtx->UnLockGEDConnInCtx();

								__PTHREAD_ENABLE_CANCEL__

								CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_WARNING, GED_SYSLOG_LEV_DETAIL_CONN, inGEDConnInCtx->addr +
									":" + CString(inGEDConnInCtx->port) + " : maximum " + 
									CString((SInt32)CGEDCtx::m_GEDCtx->m_GEDCfg.maxtun) + " allowed tun sources reached, ignoring request");

								return false;
							}
						}

						try
						{
							inGEDConnInCtx->tuns += new CSocketClient (
								*inTunSpec.Cut(CString(":"))[0], 
 								 inTunSpec.Cut(CString(":"))[1]->ToULong(), 
								 CString(),
								 new CGEDTunClientListener (inGEDConnInCtx, inId, CString(reinterpret_cast <char *> (inGEDPktIn->data) + sizeof(int))));
						}
						catch (CException *e)
						{
							CGEDCtx::m_GEDCtx->UnLockGEDConnInCtx();

							__PTHREAD_ENABLE_CANCEL__

							CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_DETAIL_CONN, inGEDConnInCtx->addr +
								":" + CString(inGEDConnInCtx->port) + " : " + e->GetMessage());

							return false;
						}

						CGEDCtx::m_GEDCtx->UnLockGEDConnInCtx();

						__PTHREAD_ENABLE_CANCEL__

						CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_DETAIL_CONN, inGEDConnInCtx->addr +
							":" + CString(inGEDConnInCtx->port) + " : successfully opened tun to " + *inTunSpec.Cut(CString(":"))[0] + ":" + 
							*inTunSpec.Cut(CString(":"))[1]);
					}
					else
					{
						__PTHREAD_ENABLE_CANCEL__

						return false;
					}
				}
				break;

				case GED_PKT_TYP_DATA_TUN :
				{
					int inId = *reinterpret_cast <int *> (inGEDPktIn->data);

					__PTHREAD_DISABLE_CANCEL__

					if (CGEDCtx::m_GEDCtx->LockGEDConnInCtx() == 0)
					{
						for (size_t i=0; i<inGEDConnInCtx->tuns.GetLength(); i++)
						{
							if (static_cast <CGEDTunClientListener *> ((*inGEDConnInCtx->tuns[i])->GetListener()) -> m_Id == inId)
							{
								(*inGEDConnInCtx->tuns[i])->Send (reinterpret_cast <UInt8 *> (inGEDPktIn->data) + sizeof(int), inGEDPktIn->len - sizeof(int));
								break;
							}
						}

						CGEDCtx::m_GEDCtx->UnLockGEDConnInCtx();
					}

					__PTHREAD_ENABLE_CANCEL__
				}
				break;

				case GED_PKT_TYP_SHUT_TUN :
				{
					int inId = *reinterpret_cast <int *> (inGEDPktIn->data);

					__PTHREAD_DISABLE_CANCEL__

					if (CGEDCtx::m_GEDCtx->LockGEDConnInCtx() == 0)
					{
						for (size_t i=0; i<inGEDConnInCtx->tuns.GetLength(); i++)
						{
							if (static_cast <CGEDTunClientListener *> ((*inGEDConnInCtx->tuns[i])->GetListener()) -> m_Id == inId)
							{
								CSocketClient *inTun  = *inGEDConnInCtx->tuns[i];
								inGEDConnInCtx->tuns -= inTun;

								CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_DETAIL_CONN, inGEDConnInCtx->addr +
									":" + CString(inGEDConnInCtx->port) + " : shutted down tun to " + 
									static_cast <CGEDTunClientListener *> (inTun -> GetListener()) -> m_TunSpec);

								delete inTun;

								break;
							}
						}

						CGEDCtx::m_GEDCtx->UnLockGEDConnInCtx();
					}

					__PTHREAD_ENABLE_CANCEL__
				}
				break;
				#endif
			}
		}
		break;

		case GED_PKT_REQ_PUSH :
		{
			if (!inGEDConnInCtx->rly && inGEDConnInCtx->port != 0L)
			{
				bool found=false; if (CGEDCtx::m_GEDCtx->m_GEDCfg.alwreqfrm.GetLength() > 0)
				{
					size_t n=0; for (size_t i=CGEDCtx::m_GEDCtx->m_GEDCfg.alwreqfrm.GetLength(); i>0 && !found; i--)
						if (inGEDConnInCtx->addr.Find (*CGEDCtx::m_GEDCtx->m_GEDCfg.alwreqfrm[i-1], 0, &n) && n==0) found=true;
				}
				else
				{
					CStrings inIfCfg ((*CGEDCtx::m_GEDCtx->m_SocketServer[0])->SiocGifConf());

					for (size_t i=0; i<inIfCfg.GetLength() && !found; i++)
						if (inIfCfg[i]->Find(CString("|")) && inGEDConnInCtx->addr == *inIfCfg[i]->Cut(CString("|"))[1])
							found=true;
				}

				if (!found)
				{
					CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, inGEDConnInCtx->addr +
						":" + CString(inGEDConnInCtx->port) + " : is not an authorized requester, closing connection");

					__PTHREAD_DISABLE_CANCEL__

					if (CGEDCtx::m_GEDCtx->LockGEDConnInSndCtx (inGEDConnInCtx) == 0)
					{
						CGEDServerListener::SendGEDPktCloseToSrc (inGEDConnInCtx, inGEDConnInCtx->desc, inGEDConnInCtx->ssl, 
							inGEDConnInCtx->addr + " is not authorized to push");

						CGEDCtx::m_GEDCtx->UnLockGEDConnInSndCtx (inGEDConnInCtx);
					}

					__PTHREAD_ENABLE_CANCEL__

					::shutdown (inGEDConnInCtx->desc, SHUT_RDWR);
					::close    (inGEDConnInCtx->desc);

					return false;
				}
			}

			__PTHREAD_DISABLE_CANCEL__

			if (CGEDCtx::m_GEDCtx->m_BackEnd->Lock() == 0)
			{
				TBuffer <TGEDRcd *> inGEDRec;

				if (!(inGEDPktIn->req&GED_PKT_REQ_NO_SYNC))
					if ((inGEDPktIn->req&GED_PKT_REQ_PUSH_MASK) == GED_PKT_REQ_PUSH_NOTMSP)
						inGEDRec = CGEDCtx::m_GEDCtx->m_BackEnd->Peek (CString(), GED_PKT_REQ_BKD_ACTIVE, inGEDPktIn);

				CStrings inSrcs; for (size_t i=0; i<inGEDRec.GetLength(); i++)
				{
					TGEDARcd *inGEDARec (static_cast <TGEDARcd *> (*inGEDRec[i]));

					for (size_t j=0; j<inGEDARec->nsrc; j++)
					{
						if (!inGEDARec->src[j].rly) continue;

						char inAd[INET_ADDRSTRLEN]; 
						CString outAddr (::inet_ntop (AF_INET, &(inGEDARec->src[j].addr), inAd, INET_ADDRSTRLEN));

						if (inGEDConnInCtx->rly && inGEDConnInCtx->addr == outAddr) continue;

						if (!inSrcs.Find(outAddr))
							if (CGEDCtx::m_GEDCtx->m_BackEnd->Push (outAddr, GED_PKT_REQ_BKD_SYNC, inGEDPktIn))
								inSrcs += outAddr;
					}
				}

				for (size_t i=0; i<inGEDRec.GetLength(); i++) ::DeleteGEDRcd (*inGEDRec[i]);

				if (!CGEDCtx::m_GEDCtx->m_BackEnd->Push (inGEDConnInCtx->addr, inGEDPktIn->req&GED_PKT_REQ_BKD_MASK,
				     inGEDPktIn))
				{
					CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, inGEDConnInCtx->addr +
						":" + CString(inGEDConnInCtx->port) + " : could not push packet into backend");

					CGEDCtx::m_GEDCtx->m_BackEnd->UnLock();

					__PTHREAD_ENABLE_CANCEL__

					return false;
				}

				if (!(inGEDPktIn->req&GED_PKT_REQ_NO_SYNC))
				{
					for (size_t i=0; i<CGEDCtx::m_GEDCtx->m_GEDConnOutCtx.GetLength(); i++)
					{
						TGEDConnOutCtx *inGEDConnOutCtx = *CGEDCtx::m_GEDCtx->m_GEDConnOutCtx[i];

						if (CGEDCtx::m_GEDCtx->m_BackEnd->Push (inGEDConnOutCtx->cfg.bind.addr,
						    GED_PKT_REQ_BKD_SYNC, inGEDPktIn) && !inGEDConnOutCtx->cfg.kpalv && 
						    inGEDConnOutCtx->cfg.syncb)
						{
							if (CGEDCtx::m_GEDCtx->LockGEDConnOutIncCtx (inGEDConnOutCtx) != 0) continue;

							inGEDConnOutCtx->totbyt += GED_PKT_FIX_SIZE + inGEDPktIn->len;

							CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_BKD_OPERATION, 
							inGEDConnOutCtx->cfg.bind.addr + ":" + CString(inGEDConnOutCtx->cfg.bind.port) + 
							" : async target has " + CString(inGEDConnOutCtx->totbyt) + " bytes in sync queue");

							CGEDCtx::m_GEDCtx->UnLockGEDConnOutIncCtx (inGEDConnOutCtx);
						}
					}
				}

				CGEDCtx::m_GEDCtx->m_BackEnd->UnLock();

				if (!(inGEDPktIn->req&GED_PKT_REQ_NO_SYNC))
				{
					for (size_t i=0; i<CGEDCtx::m_GEDCtx->m_GEDConnOutCtx.GetLength(); i++)
					{
						TGEDConnOutCtx *inGEDConnOutCtx = *CGEDCtx::m_GEDCtx->m_GEDConnOutCtx[i];

						if (inGEDConnOutCtx->cfg.kpalv)
						{
							if (inGEDConnOutCtx->md5>1)
								CGEDCtx::m_GEDCtx->UnLockGEDConnOutSendCtx (inGEDConnOutCtx);
						}
						else if (inGEDConnOutCtx->cfg.syncb)
						{
							if (CGEDCtx::m_GEDCtx->LockGEDConnOutIncCtx (inGEDConnOutCtx) != 0) continue;

							if (inGEDConnOutCtx->totbyt >= inGEDConnOutCtx->cfg.syncb)
								CGEDCtx::m_GEDCtx->UnLockGEDConnOutSendCtx (inGEDConnOutCtx);

							CGEDCtx::m_GEDCtx->UnLockGEDConnOutIncCtx (inGEDConnOutCtx);
						}
					}
				}

				if (!(inGEDPktIn->req&GED_PKT_REQ_NO_SYNC))
				{
					if (CGEDCtx::m_GEDCtx->LockGEDConnInCtx() == 0)
					{
						for (size_t i=0; i<inSrcs.GetLength(); )
						{
							bool found=false;
							for (size_t j=0; j<CGEDCtx::m_GEDCtx->m_GEDConnInCtx.GetLength() && !found; j++)
							{
								TGEDConnInCtx *inCtx (*CGEDCtx::m_GEDCtx->m_GEDConnInCtx[j]);

								if (!(inCtx->rly && inCtx->kpalv && inCtx->md5>1)) continue;

								if ((*inSrcs[i]) == inCtx->addr)
								{
									pthread_t thd; if (::pthread_create (&thd, NULL,
										CGEDServerListener::m_GEDConnInSendCB, inCtx) != 0)
									{
										CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, 
										GED_SYSLOG_LEV_CONNECTION, inCtx->addr + ":" + inCtx->port + 
										" : could not create pthread");
									}

									inSrcs.Delete (i, 1); found=true; continue;
								}
							}
							if (!found) i++;
						}

						CGEDCtx::m_GEDCtx->UnLockGEDConnInCtx();
					}
				}
			}

			__PTHREAD_ENABLE_CANCEL__
		}
		break;

		case GED_PKT_REQ_DROP :
		{
			if (!inGEDConnInCtx->rly && inGEDConnInCtx->port != 0L)
			{
				bool found=false; if (CGEDCtx::m_GEDCtx->m_GEDCfg.alwreqfrm.GetLength() > 0)
				{
					size_t n=0; for (size_t i=CGEDCtx::m_GEDCtx->m_GEDCfg.alwreqfrm.GetLength(); i>0 && !found; i--)
						if (inGEDConnInCtx->addr.Find (*CGEDCtx::m_GEDCtx->m_GEDCfg.alwreqfrm[i-1], 0, &n) && n==0) found=true;
				}
				else
				{
					CStrings inIfCfg ((*CGEDCtx::m_GEDCtx->m_SocketServer[0])->SiocGifConf());

					for (size_t i=0; i<inIfCfg.GetLength() && !found; i++)
						if (inIfCfg[i]->Find(CString("|")) && inGEDConnInCtx->addr == *inIfCfg[i]->Cut(CString("|"))[1])
							found=true;
				}

				if (!found)
				{
					CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, inGEDConnInCtx->addr +
						":" + CString(inGEDConnInCtx->port) + " : is not an authorized requester, closing connection");

					__PTHREAD_DISABLE_CANCEL__

					if (CGEDCtx::m_GEDCtx->LockGEDConnInSndCtx (inGEDConnInCtx) == 0)
					{
						CGEDServerListener::SendGEDPktCloseToSrc (inGEDConnInCtx, inGEDConnInCtx->desc, inGEDConnInCtx->ssl, 
							inGEDConnInCtx->addr + " is not authorized to drop");

						CGEDCtx::m_GEDCtx->UnLockGEDConnInSndCtx (inGEDConnInCtx);
					}

					__PTHREAD_ENABLE_CANCEL__

					::shutdown (inGEDConnInCtx->desc, SHUT_RDWR);
					::close    (inGEDConnInCtx->desc);

					return false;
				}
			}

			__PTHREAD_DISABLE_CANCEL__

			if (CGEDCtx::m_GEDCtx->m_BackEnd->Lock() == 0)
			{
				TBuffer <TGEDRcd *> inGEDRec; TBuffer <TGEDConnOutCtx *> outForward;

				if (!(inGEDPktIn->req&GED_PKT_REQ_NO_SYNC))
				{
					if ((inGEDPktIn->req&GED_PKT_REQ_BKD_MASK) == GED_PKT_REQ_BKD_ACTIVE)
					{
						inGEDRec = CGEDCtx::m_GEDCtx->m_BackEnd->Peek (CString(), GED_PKT_REQ_BKD_ACTIVE, inGEDPktIn);

						for (size_t i=0; i<CGEDCtx::m_GEDCtx->m_GEDConnOutCtx.GetLength(); i++)
						{
							TGEDConnOutCtx *inGEDConnOutCtx = *CGEDCtx::m_GEDCtx->m_GEDConnOutCtx[i];

							if (inGEDConnInCtx->rly && inGEDConnInCtx->addr == inGEDConnOutCtx->addr) continue;
	
							if (CGEDCtx::m_GEDCtx->m_BackEnd->Push (inGEDConnOutCtx->cfg.bind.addr,
							    GED_PKT_REQ_BKD_SYNC, inGEDPktIn))
							{
								outForward += inGEDConnOutCtx;

								if (!inGEDConnOutCtx->cfg.kpalv && inGEDConnOutCtx->cfg.syncb)
								{
									if (CGEDCtx::m_GEDCtx->LockGEDConnOutIncCtx (inGEDConnOutCtx) != 0) continue;

									inGEDConnOutCtx->totbyt += GED_PKT_FIX_SIZE + inGEDPktIn->len;

									CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_BKD_OPERATION, 
									inGEDConnOutCtx->cfg.bind.addr + ":" + CString(inGEDConnOutCtx->cfg.bind.port) + 
									" : async target has " + CString(inGEDConnOutCtx->totbyt) + " bytes in sync queue");
	
									CGEDCtx::m_GEDCtx->UnLockGEDConnOutIncCtx (inGEDConnOutCtx);
								}
							}
						}
					}
				}

				CStrings inSrcs; for (size_t i=0; i<inGEDRec.GetLength(); i++)
				{
					TGEDARcd *inGEDARec (static_cast <TGEDARcd *> (*inGEDRec[i]));

					for (size_t j=0; j<inGEDARec->nsrc; j++)
					{
						if (!inGEDARec->src[j].rly) continue;

						char inAd[INET_ADDRSTRLEN]; 
						CString outAddr (::inet_ntop (AF_INET, &(inGEDARec->src[j].addr), inAd, INET_ADDRSTRLEN));

						if (inGEDConnInCtx->rly && inGEDConnInCtx->addr == outAddr) continue;

						if (!inSrcs.Find(outAddr))
							if (CGEDCtx::m_GEDCtx->m_BackEnd->Push (outAddr, GED_PKT_REQ_BKD_SYNC, inGEDPktIn))
								inSrcs += outAddr;
					}
				}

				if (!CGEDCtx::m_GEDCtx->m_BackEnd->Drop (CString(), inGEDPktIn->req&GED_PKT_REQ_BKD_MASK, inGEDPktIn))
				{
					CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, inGEDConnInCtx->addr +
						":" + CString(inGEDConnInCtx->port) + " : could not drop packet from backend");

					CGEDCtx::m_GEDCtx->m_BackEnd->UnLock();

					for (size_t i=0; i<inGEDRec.GetLength(); i++) ::DeleteGEDRcd (*inGEDRec[i]);

					__PTHREAD_ENABLE_CANCEL__

					return false;
				}

				for (size_t i=0; i<inGEDRec.GetLength(); i++) ::DeleteGEDRcd (*inGEDRec[i]);

				for (size_t i=0; i<outForward.GetLength(); i++)
				{
					TGEDConnOutCtx *inGEDConnOutCtx = *outForward[i];

					if (inGEDConnOutCtx->cfg.kpalv)
					{
						if (inGEDConnOutCtx->md5>1)
							CGEDCtx::m_GEDCtx->UnLockGEDConnOutSendCtx (inGEDConnOutCtx);
					}
					else if (inGEDConnOutCtx->cfg.syncb)
					{
						if (CGEDCtx::m_GEDCtx->LockGEDConnOutIncCtx (inGEDConnOutCtx) != 0) continue;

						if (inGEDConnOutCtx->totbyt >= inGEDConnOutCtx->cfg.syncb)
							CGEDCtx::m_GEDCtx->UnLockGEDConnOutSendCtx (inGEDConnOutCtx);

						CGEDCtx::m_GEDCtx->UnLockGEDConnOutIncCtx (inGEDConnOutCtx);
					}
				}

				CGEDCtx::m_GEDCtx->m_BackEnd->UnLock();

				if (!(inGEDPktIn->req&GED_PKT_REQ_NO_SYNC))
				{
					if (CGEDCtx::m_GEDCtx->LockGEDConnInCtx() == 0)
					{
						for (size_t i=0; i<inSrcs.GetLength(); )
						{
							bool found=false;
							for (size_t j=0; j<CGEDCtx::m_GEDCtx->m_GEDConnInCtx.GetLength() && !found; j++)
							{
								TGEDConnInCtx *inCtx (*CGEDCtx::m_GEDCtx->m_GEDConnInCtx[j]);

								if (!(inCtx->rly && inCtx->kpalv && inCtx->md5>1)) continue;

								if ((*inSrcs[i]) == inCtx->addr)
								{
									pthread_t thd; if (::pthread_create (&thd, NULL,
										CGEDServerListener::m_GEDConnInSendCB, inCtx) != 0)
									{
										CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, 
										GED_SYSLOG_LEV_CONNECTION, inCtx->addr + ":" + inCtx->port + 
										" : could not create pthread");
									}

									inSrcs.Delete (i, 1); found=true; continue;
								}
							}
							if (!found) i++;
						}

						CGEDCtx::m_GEDCtx->UnLockGEDConnInCtx();
					}
				}
			}

			__PTHREAD_ENABLE_CANCEL__
		}
		break;

		case GED_PKT_REQ_PEEK :
		{
			CString filterAddr (inGEDConnInCtx->rly ? "" : inGEDConnInCtx->addr); 

			if (!inGEDConnInCtx->rly && inGEDConnInCtx->port != 0L)
			{
				if (CGEDCtx::m_GEDCtx->m_GEDCfg.alwreqfrm.GetLength() > 0)
				{
					size_t n=0; for (size_t i=CGEDCtx::m_GEDCtx->m_GEDCfg.alwreqfrm.GetLength(); i>0 && (filterAddr!=CString()); i--)
						if (inGEDConnInCtx->addr.Find (*CGEDCtx::m_GEDCtx->m_GEDCfg.alwreqfrm[i-1], 0, &n) && n==0)
							filterAddr = CString();
				}
				else
				{
					CStrings inIfCfg ((*CGEDCtx::m_GEDCtx->m_SocketServer[0])->SiocGifConf());

					for (size_t i=0; i<inIfCfg.GetLength() && filterAddr != CString(); i++)
						if (inIfCfg[i]->Find(CString("|")) && inGEDConnInCtx->addr == *inIfCfg[i]->Cut(CString("|"))[1])
							filterAddr = CString();
				}
			}
			else
			{
				filterAddr = "";
			}

			__PTHREAD_DISABLE_CANCEL__

			if (CGEDCtx::m_GEDCtx->m_BackEnd->Lock() == 0)
			{
				TBuffer <TGEDRcd *> outGEDRcds = 
					CGEDCtx::m_GEDCtx->m_BackEnd->Peek (filterAddr, inGEDPktIn->req&GED_PKT_REQ_BKD_MASK, inGEDPktIn);

				outGEDRcds += CGEDCtx::m_GEDCtx->m_BackEnd->Stat();

				CGEDCtx::m_GEDCtx->m_BackEnd->UnLock();

				for (size_t i=0; i<outGEDRcds.GetLength(); i+=CGEDCtx::m_GEDCtx->m_GEDCfg.pool)
				{
					TBuffer <TGEDRcd *> outRcds; outRcds.SetInc (BUFFER_INC_64);
					for (size_t j=i; j<CGEDCtx::m_GEDCtx->m_GEDCfg.pool+i && j<outGEDRcds.GetLength(); j++)
						outRcds += *outGEDRcds[j];

					TBuffer <TGEDPktOut> outGEDPktOutBuf; outGEDPktOutBuf.SetInc (BUFFER_INC_64); 
					for (size_t j=0; j<outRcds.GetLength(); j++)
						outGEDPktOutBuf += ::NewGEDPktOut (*outRcds[j]);

					if (CGEDCtx::m_GEDCtx->LockGEDConnInSndCtx (inGEDConnInCtx) == 0)
					{
						if (inGEDConnInCtx->port == 0L)
							::SendRawGEDPkt (inGEDConnInCtx->desc, outGEDPktOutBuf);
						else
							::SendHttpGEDPktToSrc (inGEDConnInCtx->desc, inGEDConnInCtx->ssl, &inGEDConnInCtx->http, outGEDPktOutBuf);
						
						CGEDCtx::m_GEDCtx->UnLockGEDConnInSndCtx (inGEDConnInCtx);
					}

					for (size_t j=0; j<outGEDPktOutBuf.GetLength(); j++) ::DeleteGEDPktOut (*outGEDPktOutBuf[j]);
				}

				for (size_t j=outGEDRcds.GetLength(); j>0; j--) ::DeleteGEDRcd (*outGEDRcds[j-1]);
			}

			__PTHREAD_ENABLE_CANCEL__
		}
		break;
	}

	return true;
}

//----------------------------------------------------------------------------------------------------------------------------------------
// server side sync connection in state timer checker (thread I3)
//----------------------------------------------------------------------------------------------------------------------------------------
void * CGEDServerListener::m_SyncGEDConnInTimerCB (void *inParam)
{
	TGEDConnInCtx *inGEDConnInCtx = reinterpret_cast <TGEDConnInCtx *> (inParam);

	unsigned long patience = inGEDConnInCtx->kpalv;

	while (true)
	{
		::sleep (patience); time_t tm; ::time (&tm);
		
		if ((tm - inGEDConnInCtx->iltm) > inGEDConnInCtx->kpalv)
		{
			CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_DETAIL_CONN, inGEDConnInCtx->addr + ":" + 
				CString(inGEDConnInCtx->port) + " : it\'s been " + CString(tm - inGEDConnInCtx->iltm) + 
				"s since the persistent source did not send anything, closing socket");

			::shutdown (inGEDConnInCtx->desc, SHUT_RDWR);
			::close    (inGEDConnInCtx->desc);

			break;
		}

		patience = inGEDConnInCtx->kpalv - (tm - inGEDConnInCtx->iltm);
	}

	::pthread_detach (::pthread_self());

	return NULL;
}

//----------------------------------------------------------------------------------------------------------------------------------------
// ack context timer callback (thread I5)
//----------------------------------------------------------------------------------------------------------------------------------------
void * CGEDServerListener::m_GEDConnInAckTimerCB (void *inParam)
{
	TGEDConnInCtx *inGEDConnInCtx = reinterpret_cast <TGEDConnInCtx *> (inParam);

	::sleep (CGEDCtx::m_GEDCtx->m_GEDCfg.ackto);

	__PTHREAD_DISABLE_CANCEL__

	if (CGEDCtx::m_GEDCtx->LockGEDConnInCtx() == 0)
	{
		inGEDConnInCtx->ack.hash = CString();

		CGEDCtx::m_GEDCtx->UnLockGEDConnInCtx();

		::UnLockGEDAckCtx (&inGEDConnInCtx->ack);
	}

	__PTHREAD_ENABLE_CANCEL__

	::pthread_detach (::pthread_self());

	return NULL;
}

//----------------------------------------------------------------------------------------------------------------------------------------
// server side reverse sync callback (thread I4)
//----------------------------------------------------------------------------------------------------------------------------------------
void * CGEDServerListener::m_GEDConnInSendCB (void *inParam)
{
	TGEDConnInCtx *inGEDConnInCtx = reinterpret_cast <TGEDConnInCtx *> (inParam);

	__PTHREAD_DISABLE_CANCEL__

	if (CGEDCtx::m_GEDCtx->LockGEDConnInSndCtx (inGEDConnInCtx) != 0)
	{
		__PTHREAD_ENABLE_CANCEL__

		::pthread_detach (::pthread_self()); 

		return NULL;
	}

	CGEDCtx::m_GEDCtx->m_BackEnd->Lock();

	TBuffer <TGEDRcd *> inRcds (CGEDCtx::m_GEDCtx->m_BackEnd->Peek (inGEDConnInCtx->addr, GED_PKT_REQ_BKD_SYNC, NULL));

	CGEDCtx::m_GEDCtx->m_BackEnd->UnLock();

	for (size_t i=0; i<inRcds.GetLength(); i+=CGEDCtx::m_GEDCtx->m_GEDCfg.pool)
	{
		CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_DETAIL_CONN, inGEDConnInCtx->addr + ":" + 
					 CString(inGEDConnInCtx->port) + " : performing sync, waiting for ack (" + 
					 CString(CGEDCtx::m_GEDCtx->m_GEDCfg.ackto) + "s)");

		TBuffer <TGEDRcd *> outRcds; outRcds.SetInc (BUFFER_INC_64); 
		for (size_t j=i; j<CGEDCtx::m_GEDCtx->m_GEDCfg.pool+i && j<inRcds.GetLength(); j++)
			outRcds += *inRcds[j];

		inGEDConnInCtx->ack.ack  = false;
		inGEDConnInCtx->ack.hash = ::GetGEDRandomHash();

		TBuffer <TGEDPktOut> outGEDPktOutBuf; outGEDPktOutBuf.SetInc (BUFFER_INC_64); for (size_t j=0; j<outRcds.GetLength(); j++)
			outGEDPktOutBuf += ::NewGEDPktOut (static_cast <TGEDSRcd *> (*outRcds[j]) -> pkt);

		if (::pthread_create (&inGEDConnInCtx->ack.tmrthd, NULL, CGEDServerListener::m_GEDConnInAckTimerCB, inGEDConnInCtx) != 0) 
			throw new CException ("could not create pthread");

		::SendHttpGEDPktToSrc (inGEDConnInCtx->desc, inGEDConnInCtx->ssl, &inGEDConnInCtx->http, outGEDPktOutBuf, inGEDConnInCtx->ack.hash);

		for (size_t j=0; j<outGEDPktOutBuf.GetLength(); j++) ::DeleteGEDPktOut (*outGEDPktOutBuf[j]);

		::LockGEDAckCtx (&inGEDConnInCtx->ack);

		if (inGEDConnInCtx->ack.ack)
		{
			CGEDCtx::m_GEDCtx->m_BackEnd->Lock();

			CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_DETAIL_CONN, inGEDConnInCtx->addr + ":" + 
				 CString(inGEDConnInCtx->port) + " : synchronized " + CString((long)outRcds.GetLength()) + " packets");

			for (size_t j=0; j<outRcds.GetLength(); j++)
				CGEDCtx::m_GEDCtx->m_BackEnd->Drop (inGEDConnInCtx->addr, GED_PKT_REQ_BKD_SYNC, static_cast <TGEDSRcd *> (*outRcds[j]) -> pkt);

			CGEDCtx::m_GEDCtx->m_BackEnd->UnLock();
		}
		else
		{
			CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_DETAIL_CONN, inGEDConnInCtx->addr + ":" + 
					CString(inGEDConnInCtx->port) + " : sync ack timed out (" + 
					CString(CGEDCtx::m_GEDCtx->m_GEDCfg.ackto) + "s), closing socket");

			for (size_t j=inRcds.GetLength(); j>0; j--) ::DeleteGEDRcd (*inRcds[j-1]);

			::shutdown (inGEDConnInCtx->desc, SHUT_RDWR);
			::close    (inGEDConnInCtx->desc);

			CGEDCtx::m_GEDCtx->UnLockGEDConnInSndCtx (inGEDConnInCtx);

			__PTHREAD_ENABLE_CANCEL__

			::pthread_detach (::pthread_self());

			return NULL;
		}
	}

	for (size_t j=inRcds.GetLength(); j>0; j--) ::DeleteGEDRcd (*inRcds[j-1]);

	if (inGEDConnInCtx->md5 == 1)
	{
		inGEDConnInCtx->md5++;

		CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_DETAIL_CONN, inGEDConnInCtx->addr + ":" + 
			 CString(inGEDConnInCtx->port) + " : sending md5sum announcement answer");

		CGEDServerListener::SendGEDPktMd5ToSrc (inGEDConnInCtx);
	}

	CGEDCtx::m_GEDCtx->UnLockGEDConnInSndCtx (inGEDConnInCtx);

	__PTHREAD_ENABLE_CANCEL__

	::pthread_detach (::pthread_self());

	return NULL;
}

//----------------------------------------------------------------------------------------------------------------------------------------
// Server direct argv fifo input (typeSEPARATORfield0SEPARATORfieldnSEPARATOR...)
//----------------------------------------------------------------------------------------------------------------------------------------
void * CGEDServerListener::m_GEDConnInFifoCB (void *)
{
	TGEDPktIn inGEDPktIn; ::bzero(&inGEDPktIn,sizeof(TGEDPktIn));
	inGEDPktIn.vrs  = GED_VERSION;
	inGEDPktIn.req  = GED_PKT_REQ_BKD_ACTIVE|GED_PKT_REQ_PUSH|GED_PKT_REQ_PUSH_TMSP;

	TGEDConnInCtx inGEDConnInCtx (CString("0.0.0.0"), 0L, -1, NULL);

	char input[(PIPE_BUF+1)*2]; ::bzero(input,(PIPE_BUF+1)*2); char *c = input; FILE *f = NULL;

	while (true)
	{
		if (f == NULL)
			f = ::fopen (CGEDCtx::m_GEDCtx->m_GEDCfg.fifo.Get(), "rb");

		if (f == NULL)
		{
			CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_CONNECTION, "could not open " + 
						   CGEDCtx::m_GEDCtx->m_GEDCfg.fifo + " fifo file");
			return NULL;
		}

		char raw[PIPE_BUF+1]; ::bzero(raw,PIPE_BUF+1); size_t n=0; 
		while ((n = fread (raw, sizeof(char), PIPE_BUF, f)) > 0)
		{
			if ((c+n) < (input+(PIPE_BUF+1)*2))
			{
				::memcpy (c, raw, n);
				c += n;
			}
		}

		if (feof(f))
		{
			::fclose (f);
			f = NULL;
		}

		*c = '\0';

		char *d=NULL; while (d = strstr (input, CGEDCtx::m_GEDCtx->m_GEDCfg.fifors.Get()))
		{
			char dst[PIPE_BUF+1]; ::bzero(dst,PIPE_BUF+1); 
			strncpy (dst, input, d-input);

			memcpy (input, d+1, (PIPE_BUF+1)*2-(d+1-input));
			c -= (d+1-input);

			*c = '\0';

			CStrings inFields (CString(dst).Cut(CGEDCtx::m_GEDCtx->m_GEDCfg.fifofs));

			if (inFields.GetLength() <= 1 || inFields[0]->ToLong() == 0L)
			{
				CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_CONNECTION, 
							   "fifo input from " + CGEDCtx::m_GEDCtx->m_GEDCfg.fifo + 
							   " did not provide enough fields or mismatched packet type specification");
				continue;
			}

			TGEDPktCfg *inGEDPktCfg = NULL; for (size_t i=0; i<CGEDCtx::m_GEDCtx->m_GEDCfg.pkts.GetLength() && !inGEDPktCfg; i++)
			if (CGEDCtx::m_GEDCtx->m_GEDCfg.pkts[i]->type == inFields[0]->ToLong())
				inGEDPktCfg = CGEDCtx::m_GEDCtx->m_GEDCfg.pkts[i];

			if (inGEDPktCfg == NULL || inGEDPktCfg->fields.GetLength() != (inFields.GetLength()-1))
			{
				CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_CONNECTION, 
							   "fifo input from " + CGEDCtx::m_GEDCtx->m_GEDCfg.fifo + 
							   " mismatched packet type specification (" + *inFields[0] + ")");
				continue;
			}

			CChunk chk; for (size_t i=0, j=1; i<inGEDPktCfg->fields.GetLength() && j<inFields.GetLength(); i++, j++)
			{
				switch (inGEDPktCfg->fields[i]->type)
				{
					case DATA_SINT32 :
					{
						chk << inFields[j]->ToLong();
					}
					break;
					case DATA_FLOAT64 :
					{
						chk << inFields[j]->ToDouble();
					}
					break;
					case DATA_STRING :
					{
						chk << inFields[j]->Get();
					}
					break;
				}
			}

			::gettimeofday (&inGEDPktIn.tv, NULL);
			inGEDPktIn.typ  = inFields[0]->ToLong();
			inGEDPktIn.len  = chk.GetSize();
			inGEDPktIn.data = chk.GetChunk();

			if (!CGEDServerListener::RecvGEDPktCB (CString(), &inGEDPktIn, &inGEDConnInCtx))
			{
				CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_CONNECTION, 
							   "fifo input from " + CGEDCtx::m_GEDCtx->m_GEDCfg.fifo + 
							   " : could not perform request");
				continue;
			}
		}
	}

	return NULL;
}

//----------------------------------------------------------------------------------------------------------------------------------------
// server side md5 packet emission
//----------------------------------------------------------------------------------------------------------------------------------------
void CGEDServerListener::SendGEDPktMd5ToSrc (TGEDConnInCtx *inGEDConnInCtx)
{
	TBuffer <TGEDPktOut> outGEDPktOutBuf; 
	CChunk outChunk; 
	outChunk << CGEDCtx::m_GEDCtx->m_Md5Sum.Get(); 
	outChunk << inGEDConnInCtx->kpalv;
	outGEDPktOutBuf += ::NewGEDPktOut (CString(), GED_PKT_TYP_MD5, GED_PKT_REQ_NONE, outChunk.GetChunk(), outChunk.GetSize());
	if (inGEDConnInCtx->port != 0L)
		::SendHttpGEDPktToSrc (inGEDConnInCtx->desc, inGEDConnInCtx->ssl, &inGEDConnInCtx->http, outGEDPktOutBuf);
	else
		::SendRawGEDPkt (inGEDConnInCtx->desc, outGEDPktOutBuf);
	::DeleteGEDPktOut (*outGEDPktOutBuf[0]);
}

//----------------------------------------------------------------------------------------------------------------------------------------
// server side ack packet emission
//----------------------------------------------------------------------------------------------------------------------------------------
void CGEDServerListener::SendGEDPktAckToSrc (TGEDConnInCtx *inGEDConnInCtx, CString inDigest)
{
	TBuffer <TGEDPktOut> outGEDPktOutBuf;
	outGEDPktOutBuf += ::NewGEDPktOut (CString(), GED_PKT_TYP_ACK, GED_PKT_REQ_NONE, inDigest.Get(), inDigest.GetLength()+1);
	if (inGEDConnInCtx->port != 0L)
		::SendHttpGEDPktToSrc (inGEDConnInCtx->desc, inGEDConnInCtx->ssl, &inGEDConnInCtx->http, outGEDPktOutBuf);
	else
		::SendRawGEDPkt (inGEDConnInCtx->desc, outGEDPktOutBuf);
	::DeleteGEDPktOut (*outGEDPktOutBuf[0]);
}

//----------------------------------------------------------------------------------------------------------------------------------------
// server side close packet emission
//----------------------------------------------------------------------------------------------------------------------------------------
void CGEDServerListener::SendGEDPktCloseToSrc (TGEDConnInCtx *inGEDConnInCtx, const int inDesc, const SSL *inSSL, CString inMessage)
{
	TGEDHttpAswCtx outHttpAswCtx; 
	outHttpAswCtx.vrs   = CGEDCtx::m_GEDCtx->m_GEDCfg.http.vrs;
	outHttpAswCtx.srv   = CGEDCtx::m_GEDCtx->m_GEDCfg.http.srv;
	outHttpAswCtx.typ   = CGEDCtx::m_GEDCtx->m_GEDCfg.http.typ;
	outHttpAswCtx.zlv   = CGEDCtx::m_GEDCtx->m_GEDCfg.http.zlv;
	outHttpAswCtx.kpalv = false;

	TBuffer <TGEDPktOut> outGEDPktOutBuf;
	if (inMessage.GetLength() > 0)
		outGEDPktOutBuf += ::NewGEDPktOut (CString(), GED_PKT_TYP_CLOSE, GED_PKT_REQ_NONE, inMessage.Get(), 
						   inMessage.GetLength()+1);
	else
		outGEDPktOutBuf += ::NewGEDPktOut (CString(), GED_PKT_TYP_CLOSE, GED_PKT_REQ_NONE, NULL, 0);
	if (inGEDConnInCtx->port != 0L)
		::SendHttpGEDPktToSrc (inDesc, const_cast <SSL *> (inSSL), &outHttpAswCtx, outGEDPktOutBuf);
	else
		::SendRawGEDPkt (inDesc, outGEDPktOutBuf);
	::DeleteGEDPktOut (*outGEDPktOutBuf[0]);
}

//----------------------------------------------------------------------------------------------------------------------------------------
// client side listener constructor (thread O1)
//----------------------------------------------------------------------------------------------------------------------------------------
CGEDClientListener::CGEDClientListener (TGEDConnOutCtx *inGEDConnOutCtx)
		   :m_GEDConnOutCtx    (inGEDConnOutCtx)
{ }

//----------------------------------------------------------------------------------------------------------------------------------------
// client connection notification handler, perform proxy passthru tunneling whenever using a ssl connection just after socket has 
// initiated the connection but right before the ssl handshake occurs (thread O1)
//----------------------------------------------------------------------------------------------------------------------------------------
void CGEDClientListener::OnConnect (CObject *inSender, void *)
{
	if (m_GEDConnOutCtx->cfg.proxy.addr == CString()) return;

	CString outMethod (inSender->ClassIs(__metaclass(CSocketSSLClient)) ? "CONNECT " : (m_GEDConnOutCtx->cfg.http.cmd + " "));

	CString outCmd (outMethod + m_GEDConnOutCtx->cfg.bind.addr + ":" + CString(m_GEDConnOutCtx->cfg.bind.port) + 
			" HTTP/" + m_GEDConnOutCtx->cfg.http.vrs + GED_HTTP_REGEX_CRLF);

	if (m_GEDConnOutCtx->cfg.http.agt != CString())
		outCmd += "User-Agent: " + m_GEDConnOutCtx->cfg.http.agt + GED_HTTP_REGEX_CRLF;

	int n=1, s=200;

	switch (m_GEDConnOutCtx->cfg.proxy.auth)
	{
		case GED_HTTP_PROXY_AUTH_NONE :
		{
			if (inSender->ClassIs(__metaclass(CSocketSSLClient)))
			{
				outCmd += GED_HTTP_REGEX_CRLF;

				static_cast <CSocket *> (inSender) -> Send (outCmd.Get(), outCmd.GetLength());

				char ans[1024]; CString inAnswer; while (!inAnswer.Find (GED_HTTP_REGEX_CRLF + GED_HTTP_REGEX_CRLF))
				{
					if (static_cast <CSocket *> (inSender) -> Receive (ans, 1024) <= 0)
						throw new CException (CString("could not read answer from proxy " + 
							m_GEDConnOutCtx->cfg.proxy.addr + ":" + CString(m_GEDConnOutCtx->cfg.proxy.port)));

					ans[1023] = '\0'; inAnswer += ans;
				}

				n = ::sscanf (inAnswer.Get(), "%*s %d", &s);

				if (n >= 1 && s == 407)
					throw new CException ("proxy code " + CString((SInt32)s) + " returned from " + 
						m_GEDConnOutCtx->cfg.proxy.addr + ":" + CString(m_GEDConnOutCtx->cfg.proxy.port) + 
						", authenticiation required but none specified");
			}
		}
		break;

		case GED_HTTP_PROXY_AUTH_BASIC :
		{
			outCmd += "Proxy-Authorization: Basic " + 
				::Base64Encode (m_GEDConnOutCtx->cfg.proxy.user + ":" + 
						m_GEDConnOutCtx->cfg.proxy.pass) + GED_HTTP_REGEX_CRLF;

			outCmd += GED_HTTP_REGEX_CRLF;

			static_cast <CSocket *> (inSender) -> Send (outCmd.Get(), outCmd.GetLength());

			char ans[1024]; CString inAnswer; while (!inAnswer.Find (GED_HTTP_REGEX_CRLF + GED_HTTP_REGEX_CRLF))
			{
				if (static_cast <CSocket *> (inSender) -> Receive (ans, 1024) <= 0)
					throw new CException (CString("could not read answer from proxy " + 
						m_GEDConnOutCtx->cfg.proxy.addr + ":" + CString(m_GEDConnOutCtx->cfg.proxy.port)));

				ans[1023] = '\0'; inAnswer += ans;
			}

			n = ::sscanf (inAnswer.Get(), "%*s %d", &s);
		}
		break;

		#ifdef __GED_NTLM__
		case GED_HTTP_PROXY_AUTH_NTLM :
		{
			outCmd += "Proxy-Authorization: NTLM " + ::GetHttpProxyAuthNTLM1() + GED_HTTP_REGEX_CRLF;

			outCmd += GED_HTTP_REGEX_CRLF;

			static_cast <CSocket *> (inSender) -> Send (outCmd.Get(), outCmd.GetLength());

			char ans[1024]; CString inAnswer; while (!inAnswer.Find (GED_HTTP_REGEX_CRLF + GED_HTTP_REGEX_CRLF))
			{
				if (static_cast <CSocket *> (inSender) -> Receive (ans, 1024) <= 0)
					throw new CException (CString("could not read answer from proxy " + 
						m_GEDConnOutCtx->cfg.proxy.addr + ":" + CString(m_GEDConnOutCtx->cfg.proxy.port)));

				ans[1023] = '\0'; inAnswer += ans;
			}

			n = ::sscanf (inAnswer.Get(), "%*s %d", &s);

			if (n >= 1 && s == 407)
			{
				if (!inAnswer.Find (CString("NTLM"), 0, (size_t*)&n))
					throw new CException (CString("could not retreive NTLM challenge from proxy " + 
						m_GEDConnOutCtx->cfg.proxy.addr + ":" + CString(m_GEDConnOutCtx->cfg.proxy.port)));

				n = ::sscanf (inAnswer.Get()+n, "NTLM %s", ans); ans[127]=0;

				if (n != 1)
					throw new CException (CString("could not retreive NTLM challenge from proxy " + 
						m_GEDConnOutCtx->cfg.proxy.addr + ":" + CString(m_GEDConnOutCtx->cfg.proxy.port)));

				outCmd = outMethod + m_GEDConnOutCtx->cfg.bind.addr + ":" + CString(m_GEDConnOutCtx->cfg.bind.port) + 
					 " HTTP/" + m_GEDConnOutCtx->cfg.http.vrs + GED_HTTP_REGEX_CRLF;

				if (m_GEDConnOutCtx->cfg.http.agt != CString())
					outCmd += "User-Agent: " + m_GEDConnOutCtx->cfg.http.agt + GED_HTTP_REGEX_CRLF;

				outCmd += "Host: " + m_GEDConnOutCtx->cfg.bind.addr + GED_HTTP_REGEX_CRLF;

				outCmd += "Proxy-Authorization: NTLM " + ::GetHttpProxyAuthNTLM3 (m_GEDConnOutCtx->cfg.proxy.user,
					m_GEDConnOutCtx->cfg.proxy.pass, ans) + GED_HTTP_REGEX_CRLF;

				outCmd += GED_HTTP_REGEX_CRLF;

				static_cast <CSocket *> (inSender) -> Send (outCmd.Get(), outCmd.GetLength());

				inAnswer = ""; while (!inAnswer.Find (GED_HTTP_REGEX_CRLF + GED_HTTP_REGEX_CRLF))
				{
					if (static_cast <CSocket *> (inSender) -> Receive (ans, 1024) <= 0)
						throw new CException (CString("could not read answer from proxy " + 
							m_GEDConnOutCtx->cfg.proxy.addr + ":" + CString(m_GEDConnOutCtx->cfg.proxy.port)));

					ans[1023] = '\0'; inAnswer += ans;
				}

				n = ::sscanf (inAnswer.Get(), "%*s %d", &s);
			}
		}
		break;
		#endif
	}

	if (n < 1 || s != 200)
		throw new CException (CString("proxy " + m_GEDConnOutCtx->cfg.proxy.addr + ":" + 
				   CString(m_GEDConnOutCtx->cfg.proxy.port) + " returned bad status code " + CString((SInt32)s)));

	CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_CONNECTION, m_GEDConnOutCtx->cfg.proxy.addr + ":" + 
				   CString(m_GEDConnOutCtx->cfg.proxy.port) + " : http proxy connection established");
}

//----------------------------------------------------------------------------------------------------------------------------------------
// client side raw data reception handling (thread O2)
//----------------------------------------------------------------------------------------------------------------------------------------
void CGEDClientListener::OnReceive (CObject *, const void *inData, const int inLen)
{
	::RecvHttpGEDPktFromBuf (const_cast <void *> (inData), inLen, m_GEDConnOutCtx->pktctx, CGEDClientListener::RecvGEDPktPoolCB, m_GEDConnOutCtx);
}

//----------------------------------------------------------------------------------------------------------------------------------------
// client side packets pool reception callback (thread O2)
//----------------------------------------------------------------------------------------------------------------------------------------
void CGEDClientListener::RecvGEDPktPoolCB (const CString &inGEDHttpHeader, long inGEDPktNum, bool inGEDPktLast, TGEDPktIn *inGEDPktIn, 
					   void *inParam)
{
	TGEDConnOutCtx *inGEDConnOutCtx = reinterpret_cast <TGEDConnOutCtx *> (inParam);

	::time (&inGEDConnOutCtx->iltm);

	if (inGEDPktNum == 0L)
	{
		CString outStr (inGEDConnOutCtx->cfg.bind.addr + ":" + CString(inGEDConnOutCtx->cfg.bind.port) + " : " + inGEDHttpHeader);
		outStr.Substitute (GED_HTTP_REGEX_CRLF, CString(" "));

		CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_HTTP_HEADER, outStr);
	}

	if (GED_MAJ(inGEDPktIn->vrs) != GED_MAJOR || GED_MIN(inGEDPktIn->vrs) != GED_MINOR)
	{
		CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_CONNECTION, inGEDConnOutCtx->cfg.bind.addr + ":" + 
			CString(inGEDConnOutCtx->cfg.bind.port) + " : listener version is " + GED_VERSION_STR + 
			", received a packet version " + CString(GED_MAJ(inGEDPktIn->vrs)) + "." + CString(GED_MIN(inGEDPktIn->vrs)) + 
			"." + CString(GED_PAT(inGEDPktIn->vrs)) + ", ignoring");

		return;
	}

	if (inGEDConnOutCtx->iltm < inGEDPktIn->tv.tv_sec)
	{
		CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_WARNING, GED_SYSLOG_LEV_CONNECTION, inGEDConnOutCtx->cfg.bind.addr + ":" + 
			CString(inGEDConnOutCtx->cfg.bind.port) + " : packet timestamp in the future, consider adjusting systems time !");
	}

	inGEDConnOutCtx->pktin += ::NewGEDPktIn (inGEDPktIn);

	if (inGEDPktLast)
	{
		bool ok=true; for (size_t i=inGEDConnOutCtx->pktin.GetLength(), j=0; i>0 && ok; i--, j++)
			ok = CGEDClientListener::RecvGEDPktCB (inGEDHttpHeader, *inGEDConnOutCtx->pktin[j], inGEDConnOutCtx);

		for (size_t i=inGEDConnOutCtx->pktin.GetLength(); i>0; i--) ::DeleteGEDPktIn (*inGEDConnOutCtx->pktin[i-1]);
		inGEDConnOutCtx->pktin.Reset();

		size_t n; if (ok && CString(inGEDHttpHeader).ToLower().Find(CString("content-md5: "),0,&n))
		{
			char outDigest[128]; n = ::sscanf (inGEDHttpHeader.Get()+n, "%*s %s", outDigest); if (n>=1)
			{
				__PTHREAD_DISABLE_CANCEL__

				if (CGEDCtx::m_GEDCtx->LockGEDConnOutCtx (inGEDConnOutCtx) == 0)
				{
					CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_DETAIL_CONN, 
						inGEDConnOutCtx->cfg.bind.addr + ":" + CString(inGEDConnOutCtx->cfg.bind.port) + 
						" : sending requested sequence ack");

					CGEDClientListener::SendGEDPktAckToTgt (inGEDConnOutCtx, CString(outDigest));

					CGEDCtx::m_GEDCtx->UnLockGEDConnOutCtx (inGEDConnOutCtx);
				}

				__PTHREAD_ENABLE_CANCEL__
			}
		}
	}
}

//----------------------------------------------------------------------------------------------------------------------------------------
// client side packet reception analyse callback (thread O2)
//----------------------------------------------------------------------------------------------------------------------------------------
bool CGEDClientListener::RecvGEDPktCB (const CString &, TGEDPktIn *inGEDPktIn, TGEDConnOutCtx *inGEDConnOutCtx)
{
	CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_PKT_HEADER, inGEDConnOutCtx->cfg.bind.addr + ":" + 
		CString(inGEDConnOutCtx->cfg.bind.port) + " : " + ::GEDPktInToHeaderString(inGEDPktIn));

	CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_PKT_CONTENT, inGEDConnOutCtx->cfg.bind.addr + ":" + 
		CString(inGEDConnOutCtx->cfg.bind.port) + " : " + ::GEDPktInToContentString (inGEDPktIn,
		::GEDPktInToCfg (inGEDPktIn, CGEDCtx::m_GEDCtx->m_GEDCfg.pkts)));

	switch (inGEDPktIn->req & GED_PKT_REQ_MASK)
	{
		case GED_PKT_REQ_NONE :
		{
			switch (inGEDPktIn->typ)
			{
				case GED_PKT_TYP_MD5 :
				{
					CString inMd5 (reinterpret_cast <char *> (inGEDPktIn->data));

					if (inMd5 != CGEDCtx::m_GEDCtx->m_Md5Sum)
					{
						CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_DETAIL_CONN, 
							inGEDConnOutCtx->cfg.bind.addr + ":" + CString(inGEDConnOutCtx->cfg.bind.port) + 
							" : md5sum rejected, closing socket");

						::shutdown (inGEDConnOutCtx->desc, SHUT_RDWR);
						::close    (inGEDConnOutCtx->desc);

						return false;
					}

					inGEDConnOutCtx->md5 = 1;

					CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_DETAIL_CONN, 
						inGEDConnOutCtx->cfg.bind.addr + ":" + CString(inGEDConnOutCtx->cfg.bind.port) + 
						" : md5sum accepted");

					CGEDCtx::m_GEDCtx->UnLockGEDConnOutSendCtx (inGEDConnOutCtx);
				}
				break;

				case GED_PKT_TYP_ACK :
				{
					__PTHREAD_DISABLE_CANCEL__

					if (CGEDCtx::m_GEDCtx->LockGEDConnOutCtx() == 0)
					{
						if (CString((char*)inGEDPktIn->data) == inGEDConnOutCtx->ack.hash)
						{
							CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_DETAIL_CONN, 
								inGEDConnOutCtx->cfg.bind.addr + ":" + 
								CString(inGEDConnOutCtx->cfg.bind.port) + " : ack reception " + 
								CString((char*)inGEDPktIn->data) + " (ok)");

							inGEDConnOutCtx->ack.ack = true;
	
							::pthread_cancel (inGEDConnOutCtx->ack.tmrthd);
							::pthread_detach (inGEDConnOutCtx->ack.tmrthd);

							::UnLockGEDAckCtx (&inGEDConnOutCtx->ack);
						}
						else
							CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_DETAIL_CONN, 
								inGEDConnOutCtx->cfg.bind.addr + ":" + 
								CString(inGEDConnOutCtx->cfg.bind.port) + " : ack reception " + 
								CString((char*)inGEDPktIn->data) + " (no match)");

						CGEDCtx::m_GEDCtx->UnLockGEDConnOutCtx();
					}

					__PTHREAD_ENABLE_CANCEL__
				}
				break;

				case GED_PKT_TYP_RECORD :
				{
					CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_WARNING, GED_SYSLOG_LEV_DETAIL_CONN, 
						inGEDConnOutCtx->cfg.bind.addr + ":" + CString(inGEDConnOutCtx->cfg.bind.port) + 
						" : record reception, should not happen");
				}
				break;

				case GED_PKT_TYP_CLOSE :
				{
					CString outMessage (inGEDConnOutCtx->cfg.bind.addr + ":" + CString(inGEDConnOutCtx->cfg.bind.port));
					if (inGEDPktIn->data) outMessage += " : " + CString(reinterpret_cast <char *> (inGEDPktIn->data));
					outMessage += " : won\'t try to relay anymore, closing socket";
					CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_DETAIL_CONN, outMessage);

					::pthread_detach (inGEDConnOutCtx->conthd);
					::pthread_cancel (inGEDConnOutCtx->conthd);

					::pthread_cancel (inGEDConnOutCtx->sndthd);
					::pthread_detach (inGEDConnOutCtx->sndthd);

					if (inGEDConnOutCtx->cfg.kpalv)
					{
						::pthread_detach (inGEDConnOutCtx->plsthd);
						::pthread_cancel (inGEDConnOutCtx->plsthd);
					}
					else if (inGEDConnOutCtx->cfg.syncs)
					{
						::pthread_detach (inGEDConnOutCtx->tmrthd);
						::pthread_cancel (inGEDConnOutCtx->tmrthd);
					}

					::shutdown (inGEDConnOutCtx->desc, SHUT_RDWR);
					::close    (inGEDConnOutCtx->desc);

					inGEDConnOutCtx->md5  = 0;
					inGEDConnOutCtx->addr = CString();
					inGEDConnOutCtx->desc = -1;
					inGEDConnOutCtx->ssl  = NULL;
				}
				break;

				case GED_PKT_TYP_PULSE :
				{
					CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_WARNING, GED_SYSLOG_LEV_DETAIL_CONN, 
						inGEDConnOutCtx->cfg.bind.addr + ":" + CString(inGEDConnOutCtx->cfg.bind.port) + 
						" : pulse reception, should not happen on client side");
				}
				break;
			}
		}
		break;

		case GED_PKT_REQ_PUSH :
		{
			if ((inGEDPktIn->req&GED_PKT_REQ_PUSH_MASK) == GED_PKT_REQ_PUSH_NOTMSP)
			{
				if ((inGEDPktIn->req&GED_PKT_REQ_BKD_MASK) != GED_PKT_REQ_BKD_ACTIVE)
				{
					CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_WARNING, GED_SYSLOG_LEV_DETAIL_CONN, 
						inGEDConnOutCtx->cfg.bind.addr + ":" + CString(inGEDConnOutCtx->cfg.bind.port) + 
						" : update reception, should not happen on client side on non active queue");

					break;
				}

				__PTHREAD_DISABLE_CANCEL__

				if (CGEDCtx::m_GEDCtx->m_BackEnd->Lock() != 0)
				{
					__PTHREAD_ENABLE_CANCEL__

					break;
				}

				TBuffer <TGEDRcd *> inGEDRec;

				if ((inGEDPktIn->req&GED_PKT_REQ_BKD_MASK) == GED_PKT_REQ_BKD_ACTIVE)
					inGEDRec = CGEDCtx::m_GEDCtx->m_BackEnd->Peek (CString(), GED_PKT_REQ_BKD_ACTIVE, inGEDPktIn);

				for (size_t i=0; i<CGEDCtx::m_GEDCtx->m_GEDConnOutCtx.GetLength(); i++)
				{
					TGEDConnOutCtx *outCtx = *CGEDCtx::m_GEDCtx->m_GEDConnOutCtx[i];

					if (outCtx == inGEDConnOutCtx) continue;

					if (CGEDCtx::m_GEDCtx->m_BackEnd->Push (outCtx->cfg.bind.addr, GED_PKT_REQ_BKD_SYNC, inGEDPktIn))
					{
						if (!outCtx->cfg.kpalv && outCtx->cfg.syncb)
						{
							if (CGEDCtx::m_GEDCtx->LockGEDConnOutIncCtx (outCtx) != 0) continue;

							outCtx->totbyt += GED_PKT_FIX_SIZE + inGEDPktIn->len;

							CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_BKD_OPERATION, 
							outCtx->cfg.bind.addr + ":" + CString(outCtx->cfg.bind.port) + 
							" : async target has " + CString(outCtx->totbyt) + " bytes in sync queue");

							CGEDCtx::m_GEDCtx->UnLockGEDConnOutIncCtx (outCtx);
						}
					}
				}

				CStrings inSrcs; for (size_t i=0; i<inGEDRec.GetLength(); i++)
				{
					TGEDARcd *inGEDARec (static_cast <TGEDARcd *> (*inGEDRec[i]));

					for (size_t j=0; j<inGEDARec->nsrc; j++)
					{
						if (!inGEDARec->src[j].rly) continue;

						char inAd[INET_ADDRSTRLEN]; 
						CString outAddr (::inet_ntop (AF_INET, &(inGEDARec->src[j].addr), inAd, INET_ADDRSTRLEN));

						if (CGEDCtx::m_GEDCtx->LockGEDConnInCtx() != 0) continue;

						for (size_t k=0; k<CGEDCtx::m_GEDCtx->m_GEDConnInCtx.GetLength(); k++)
						{
							TGEDConnInCtx *inGEDConnInCtx = *CGEDCtx::m_GEDCtx->m_GEDConnInCtx[k];

							if (!inGEDConnInCtx->rly || inGEDConnInCtx->addr != outAddr) continue;

							if (CGEDCtx::m_GEDCtx->m_BackEnd->Push (outAddr, GED_PKT_REQ_BKD_SYNC, inGEDPktIn))
								if (!inSrcs.Find(outAddr)) inSrcs += outAddr;
						}

						CGEDCtx::m_GEDCtx->UnLockGEDConnInCtx();
					}
				}

				if (!CGEDCtx::m_GEDCtx->m_BackEnd->Push (inGEDConnOutCtx->addr, inGEDPktIn->req&GED_PKT_REQ_BKD_MASK,
				     inGEDPktIn))
				{
					CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, inGEDConnOutCtx->cfg.bind.addr +
						":" + CString(inGEDConnOutCtx->cfg.bind.port) + " : could not push packet into backend");

					CGEDCtx::m_GEDCtx->m_BackEnd->UnLock();

					__PTHREAD_ENABLE_CANCEL__

					return false;
				}

				CGEDCtx::m_GEDCtx->m_BackEnd->UnLock();

				for (size_t i=0; i<inGEDRec.GetLength(); i++) ::DeleteGEDRcd (*inGEDRec[i]);

				for (size_t i=0; i<CGEDCtx::m_GEDCtx->m_GEDConnOutCtx.GetLength(); i++)
				{
					TGEDConnOutCtx *outCtx = *CGEDCtx::m_GEDCtx->m_GEDConnOutCtx[i];

					if (outCtx == inGEDConnOutCtx) continue;

					if (outCtx->cfg.kpalv)
					{
						if (outCtx->md5>1)
							CGEDCtx::m_GEDCtx->UnLockGEDConnOutSendCtx (outCtx);
					}
					else if (outCtx->cfg.syncb)
					{
						if (CGEDCtx::m_GEDCtx->LockGEDConnOutIncCtx (outCtx) != 0) continue;

						if (outCtx->totbyt >= outCtx->cfg.syncb)
							CGEDCtx::m_GEDCtx->UnLockGEDConnOutSendCtx (outCtx);

						CGEDCtx::m_GEDCtx->UnLockGEDConnOutIncCtx (outCtx);
					}
				}

				if (CGEDCtx::m_GEDCtx->LockGEDConnInCtx() == 0)
				{
					for (size_t i=0; i<inSrcs.GetLength(); )
					{
						bool found=false;
						for (size_t j=0; j<CGEDCtx::m_GEDCtx->m_GEDConnInCtx.GetLength() && !found; j++)
						{
							TGEDConnInCtx *inCtx (*CGEDCtx::m_GEDCtx->m_GEDConnInCtx[j]);

							if (!(inCtx->rly && inCtx->kpalv && inCtx->md5>1)) continue;

							if ((*inSrcs[i]) == inCtx->addr)
							{
								pthread_t thd; if (::pthread_create (&thd, NULL,
									CGEDServerListener::m_GEDConnInSendCB, inCtx) != 0)
								{
									CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, 
									GED_SYSLOG_LEV_CONNECTION, inCtx->addr + ":" + inCtx->port + 
									" : could not create pthread");
								}

								inSrcs.Delete (i, 1); found=true; continue;
							}
						}
						if (!found) i++;
					}

					CGEDCtx::m_GEDCtx->UnLockGEDConnInCtx();
				}

				__PTHREAD_ENABLE_CANCEL__

			}
			else
			{
				CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_WARNING, GED_SYSLOG_LEV_DETAIL_CONN, 
					inGEDConnOutCtx->cfg.bind.addr + ":" + CString(inGEDConnOutCtx->cfg.bind.port) + 
					" : push reception, should not happen on client side");
			}
		}
		break;

		case GED_PKT_REQ_DROP :
		{
			__PTHREAD_DISABLE_CANCEL__

			if (CGEDCtx::m_GEDCtx->m_BackEnd->Lock() != 0)
			{
				__PTHREAD_ENABLE_CANCEL__

				break;
			}

			TBuffer <TGEDRcd *> inGEDRec;

			if ((inGEDPktIn->req&GED_PKT_REQ_BKD_MASK) == GED_PKT_REQ_BKD_ACTIVE)
				inGEDRec = CGEDCtx::m_GEDCtx->m_BackEnd->Peek (CString(), GED_PKT_REQ_BKD_ACTIVE, inGEDPktIn);

			if ((inGEDPktIn->req&GED_PKT_REQ_BKD_MASK) != GED_PKT_REQ_BKD_SYNC &&
			    !CGEDCtx::m_GEDCtx->m_BackEnd->Drop (inGEDConnOutCtx->cfg.bind.addr, GED_PKT_REQ_BKD_SYNC, inGEDPktIn))
			{
				CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, inGEDConnOutCtx->cfg.bind.addr +
					":" + CString(inGEDConnOutCtx->cfg.bind.port) + " : could not drop packet from backend");

				CGEDCtx::m_GEDCtx->m_BackEnd->UnLock();

				for (size_t i=0; i<inGEDRec.GetLength(); i++) ::DeleteGEDRcd (*inGEDRec[i]);

				__PTHREAD_ENABLE_CANCEL__

				return false;
			}

			for (size_t i=0; i<CGEDCtx::m_GEDCtx->m_GEDConnOutCtx.GetLength(); i++)
			{
				TGEDConnOutCtx *outCtx = *CGEDCtx::m_GEDCtx->m_GEDConnOutCtx[i];

				if (outCtx == inGEDConnOutCtx) continue;

				if (CGEDCtx::m_GEDCtx->m_BackEnd->Push (outCtx->cfg.bind.addr, GED_PKT_REQ_BKD_SYNC, inGEDPktIn))
				{
					if (!outCtx->cfg.kpalv && outCtx->cfg.syncb)
					{
						if (CGEDCtx::m_GEDCtx->LockGEDConnOutIncCtx (outCtx) != 0) continue;

						outCtx->totbyt += GED_PKT_FIX_SIZE + inGEDPktIn->len;

						CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_BKD_OPERATION, 
						outCtx->cfg.bind.addr + ":" + CString(outCtx->cfg.bind.port) + 
						" : async target has " + CString(outCtx->totbyt) + " bytes in sync queue");

						CGEDCtx::m_GEDCtx->UnLockGEDConnOutIncCtx (outCtx);
					}
				}
			}

			CStrings inSrcs; for (size_t i=0; i<inGEDRec.GetLength(); i++)
			{
				TGEDARcd *inGEDARec (static_cast <TGEDARcd *> (*inGEDRec[i]));

				for (size_t j=0; j<inGEDARec->nsrc; j++)
				{
					if (!inGEDARec->src[j].rly) continue;

					char inAd[INET_ADDRSTRLEN]; 
					CString outAddr (::inet_ntop (AF_INET, &(inGEDARec->src[j].addr), inAd, INET_ADDRSTRLEN));

					if (CGEDCtx::m_GEDCtx->LockGEDConnInCtx() != 0) continue;

					for (size_t k=0; k<CGEDCtx::m_GEDCtx->m_GEDConnInCtx.GetLength(); k++)
					{
						TGEDConnInCtx *inGEDConnInCtx = *CGEDCtx::m_GEDCtx->m_GEDConnInCtx[k];

						if (!inGEDConnInCtx->rly || inGEDConnInCtx->addr != outAddr) continue;

						if (CGEDCtx::m_GEDCtx->m_BackEnd->Push (outAddr, GED_PKT_REQ_BKD_SYNC, inGEDPktIn))
							if (!inSrcs.Find(outAddr)) inSrcs += outAddr;
					}

					CGEDCtx::m_GEDCtx->UnLockGEDConnInCtx();
				}
			}

			if (!CGEDCtx::m_GEDCtx->m_BackEnd->Drop (CString(), inGEDPktIn->req&GED_PKT_REQ_BKD_MASK, inGEDPktIn))
			{
				CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, inGEDConnOutCtx->cfg.bind.addr +
					":" + CString(inGEDConnOutCtx->cfg.bind.port) + " : could not drop packet from backend");

				CGEDCtx::m_GEDCtx->m_BackEnd->UnLock();

				for (size_t i=0; i<inGEDRec.GetLength(); i++) ::DeleteGEDRcd (*inGEDRec[i]);

				__PTHREAD_ENABLE_CANCEL__

				return false;
			}

			CGEDCtx::m_GEDCtx->m_BackEnd->UnLock();

			for (size_t i=0; i<inGEDRec.GetLength(); i++) ::DeleteGEDRcd (*inGEDRec[i]);

			for (size_t i=0; i<CGEDCtx::m_GEDCtx->m_GEDConnOutCtx.GetLength(); i++)
			{
				TGEDConnOutCtx *outCtx = *CGEDCtx::m_GEDCtx->m_GEDConnOutCtx[i];

				if (outCtx == inGEDConnOutCtx) continue;

				if (outCtx->cfg.kpalv)
				{
					if (outCtx->md5>1)
						CGEDCtx::m_GEDCtx->UnLockGEDConnOutSendCtx (outCtx);
				}
				else if (outCtx->cfg.syncb)
				{
					if (CGEDCtx::m_GEDCtx->LockGEDConnOutIncCtx (outCtx) != 0) continue;

					if (outCtx->totbyt >= outCtx->cfg.syncb)
						CGEDCtx::m_GEDCtx->UnLockGEDConnOutSendCtx (outCtx);

					CGEDCtx::m_GEDCtx->UnLockGEDConnOutIncCtx (outCtx);
				}
			}

			if (CGEDCtx::m_GEDCtx->LockGEDConnInCtx() == 0)
			{
				for (size_t i=0; i<inSrcs.GetLength(); )
				{
					bool found=false;
					for (size_t j=0; j<CGEDCtx::m_GEDCtx->m_GEDConnInCtx.GetLength() && !found; j++)
					{
						TGEDConnInCtx *inCtx (*CGEDCtx::m_GEDCtx->m_GEDConnInCtx[j]);

						if (!(inCtx->rly && inCtx->kpalv && inCtx->md5>1)) continue;

						if ((*inSrcs[i]) == inCtx->addr)
						{
							pthread_t thd; if (::pthread_create (&thd, NULL,
								CGEDServerListener::m_GEDConnInSendCB, inCtx) != 0)
							{
								CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, 
								GED_SYSLOG_LEV_CONNECTION, inCtx->addr + ":" + inCtx->port + 
								" : could not create pthread");
							}

							inSrcs.Delete (i, 1); found=true; continue;
						}
					}
					if (!found) i++;
				}

				CGEDCtx::m_GEDCtx->UnLockGEDConnInCtx();
			}

			__PTHREAD_ENABLE_CANCEL__
		}
		break;

		case GED_PKT_REQ_PEEK :
		{
			CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_WARNING, GED_SYSLOG_LEV_DETAIL_CONN, 
				inGEDConnOutCtx->cfg.bind.addr + ":" + CString(inGEDConnOutCtx->cfg.bind.port) + 
				" : peek reception, should not happen on client side");
		}
		break;
	}

	return true;
}

//----------------------------------------------------------------------------------------------------------------------------------------
// connection out context ack timer callback (thread O6)
//----------------------------------------------------------------------------------------------------------------------------------------
void * CGEDClientListener::m_GEDConnOutAckTimerCB (void *inParam)
{
	TGEDConnOutCtx *inGEDConnOutCtx = reinterpret_cast <TGEDConnOutCtx *> (inParam);

	::sleep (inGEDConnOutCtx->cfg.ackto);

	__PTHREAD_DISABLE_CANCEL__

	if (CGEDCtx::m_GEDCtx->LockGEDConnOutCtx() == 0)
	{
		inGEDConnOutCtx->ack.hash = CString();

		CGEDCtx::m_GEDCtx->UnLockGEDConnOutCtx();

		::UnLockGEDAckCtx (&inGEDConnOutCtx->ack);
	}

	__PTHREAD_ENABLE_CANCEL__

	::pthread_detach (::pthread_self());

	return NULL;
}

//----------------------------------------------------------------------------------------------------------------------------------------
// client side async connection timer callback (thread O5)
//----------------------------------------------------------------------------------------------------------------------------------------
void * CGEDClientListener::m_ASyncGEDConnOutTimerCB (void *inParam)
{
	TGEDConnOutCtx *inGEDConnOutCtx = reinterpret_cast <TGEDConnOutCtx *> (inParam);

	unsigned long patience = inGEDConnOutCtx->cfg.syncs;

	while (true)
	{
		::sleep (patience); time_t tm; ::time (&tm);

		if ((tm - inGEDConnOutCtx->oltm) >= inGEDConnOutCtx->cfg.syncs)
		{
			if (CGEDCtx::m_GEDCtx->UnLockGEDConnOutSendCtx (inGEDConnOutCtx) != 0) break;

			patience = inGEDConnOutCtx->cfg.syncs;

			continue;
		}

		patience = inGEDConnOutCtx->cfg.syncs - (tm - inGEDConnOutCtx->oltm);
	}

	::pthread_detach (::pthread_self());

	return NULL;
}

//----------------------------------------------------------------------------------------------------------------------------------------
// client side sync connection pulse callback (thread O4)
//----------------------------------------------------------------------------------------------------------------------------------------
void * CGEDClientListener::m_SyncGEDConnOutPulseCB (void *inParam)
{
	TGEDConnOutCtx *inGEDConnOutCtx = reinterpret_cast <TGEDConnOutCtx *> (inParam);

	SInt32 patience = inGEDConnOutCtx->cfg.kpalv;

	while (true)
	{
		::sleep (patience); time_t tm; ::time (&tm);

		__PTHREAD_DISABLE_CANCEL__

		if (CGEDCtx::m_GEDCtx->LockGEDConnOutCtx (inGEDConnOutCtx) != 0) 
		{
			__PTHREAD_ENABLE_CANCEL__

			break;
		}

		if ((tm - inGEDConnOutCtx->oltm) >= inGEDConnOutCtx->cfg.kpalv)
		{
			CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_DETAIL_CONN, inGEDConnOutCtx->cfg.bind.addr + ":" + 
					CString(inGEDConnOutCtx->cfg.bind.port) + " : pulse packet emission, waiting for ack (" + 
					CString(inGEDConnOutCtx->cfg.ackto) + "s)");

			inGEDConnOutCtx->ack.ack  = false;
			inGEDConnOutCtx->ack.hash = ::GetGEDRandomHash();

			TBuffer <TGEDPktOut> outGEDPktOutBuf; 
			outGEDPktOutBuf += ::NewGEDPktOut (inGEDConnOutCtx->addr, GED_PKT_TYP_PULSE, GED_PKT_REQ_NONE, NULL, 0);

			if (::pthread_create (&inGEDConnOutCtx->ack.tmrthd, NULL, CGEDClientListener::m_GEDConnOutAckTimerCB, 
				inGEDConnOutCtx) != 0) throw new CException ("could not create pthread");

			::time (&inGEDConnOutCtx->oltm);

			::SendHttpGEDPktToTgt (inGEDConnOutCtx->desc, inGEDConnOutCtx->ssl, &inGEDConnOutCtx->http, outGEDPktOutBuf, inGEDConnOutCtx->ack.hash);

			::DeleteGEDPktOut (*outGEDPktOutBuf[0]);

			::LockGEDAckCtx (&inGEDConnOutCtx->ack);

			if (!inGEDConnOutCtx->ack.ack)
			{
				CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_DETAIL_CONN, inGEDConnOutCtx->cfg.bind.addr +
					":" + CString(inGEDConnOutCtx->cfg.bind.port) + " : pulse ack timed out (" + 
					CString(inGEDConnOutCtx->cfg.ackto) + "s), closing socket");
	
				::shutdown (inGEDConnOutCtx->desc, SHUT_RDWR);
				::close    (inGEDConnOutCtx->desc);

				CGEDCtx::m_GEDCtx->UnLockGEDConnOutCtx (inGEDConnOutCtx);

				__PTHREAD_ENABLE_CANCEL__

				::pthread_detach (::pthread_self());

				return NULL;
			}

			::time (&tm); patience = inGEDConnOutCtx->cfg.kpalv - (tm - inGEDConnOutCtx->oltm); 

			if (CGEDCtx::m_GEDCtx->UnLockGEDConnOutCtx (inGEDConnOutCtx) != 0)
			{
				__PTHREAD_ENABLE_CANCEL__

				break;
			}

			__PTHREAD_ENABLE_CANCEL__

			continue;
		}

		patience = inGEDConnOutCtx->cfg.kpalv - (tm - inGEDConnOutCtx->oltm);
		if (patience < 0) patience = 0;

		if (CGEDCtx::m_GEDCtx->UnLockGEDConnOutCtx (inGEDConnOutCtx) != 0) 
		{
			__PTHREAD_ENABLE_CANCEL__

			break;
		}

		__PTHREAD_ENABLE_CANCEL__
	}

	::pthread_detach (::pthread_self());

	return NULL;
}

//----------------------------------------------------------------------------------------------------------------------------------------
// client side sync/async connection callback (thread O1)
//----------------------------------------------------------------------------------------------------------------------------------------
void * CGEDClientListener::m_GEDConnOutConnectCB (void *inParam)
{
	TGEDConnOutCtx *inGEDConnOutCtx = reinterpret_cast <TGEDConnOutCtx *> (inParam);

	if (inGEDConnOutCtx->cfg.syncs)
		if (::pthread_create (&inGEDConnOutCtx->tmrthd, NULL, CGEDClientListener::m_ASyncGEDConnOutTimerCB, inGEDConnOutCtx) != 0) 
			throw new CException ("could not create pthread");

	while (true)
	{
		if (!inGEDConnOutCtx->cfg.kpalv)
			if (CGEDCtx::m_GEDCtx->LockGEDConnOutSendCtx (inGEDConnOutCtx) != 0) break;

		try
		{
			CString outBindAddr (inGEDConnOutCtx->cfg.bind.addr);
			UInt32  outBindPort (inGEDConnOutCtx->cfg.bind.port);

			if (inGEDConnOutCtx->cfg.proxy.addr != CString()) outBindAddr = inGEDConnOutCtx->cfg.proxy.addr;
			if (inGEDConnOutCtx->cfg.proxy.port != 0L	) outBindPort = inGEDConnOutCtx->cfg.proxy.port;

			CSocketClient *SocketClient = NULL;

			if (inGEDConnOutCtx->cfg.tls.ca  != CString() || 
			    inGEDConnOutCtx->cfg.tls.crt != CString() ||
			    inGEDConnOutCtx->cfg.tls.key != CString())

				SocketClient = new CSocketSSLClient (
						outBindAddr,
						(UInt16)outBindPort,
						inGEDConnOutCtx->cfg.tls.ca,
						inGEDConnOutCtx->cfg.tls.crt,
						inGEDConnOutCtx->cfg.tls.key,
						inGEDConnOutCtx->cfg.tls.vfy,
						inGEDConnOutCtx->cfg.tls.cph,
						inGEDConnOutCtx->cfg.bind.bind,
						new CGEDClientListener(inGEDConnOutCtx),
						CGEDCtx::m_SSLInfoCallBack,
						CGEDCtx::m_SSLVerifyCallBack);
			else
				SocketClient = new CSocketClient (
						outBindAddr,
						(UInt16)outBindPort,
						inGEDConnOutCtx->cfg.bind.bind,
						new CGEDClientListener(inGEDConnOutCtx));

			CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_CONNECTION, inGEDConnOutCtx->cfg.bind.addr + ":" + 
				CString(inGEDConnOutCtx->cfg.bind.port) + " : connection established");

			inGEDConnOutCtx->addr = SocketClient->GetAddr();
			inGEDConnOutCtx->desc = SocketClient->GetDesc();
			inGEDConnOutCtx->ssl  = SocketClient->ClassIs(__metaclass(CSocketSSLClient)) ? 
					        static_cast <CSocketSSLClient *> (SocketClient) -> GetSSL() : NULL;

			CGEDClientListener::SendGEDPktMd5ToTgt (inGEDConnOutCtx);

			if (::pthread_create (&inGEDConnOutCtx->sndthd, NULL, CGEDClientListener::m_GEDConnOutSendCB,
				 inGEDConnOutCtx) != 0)  throw new CException ("could not create pthread");

			if (inGEDConnOutCtx->cfg.kpalv)
				if (::pthread_create (&inGEDConnOutCtx->plsthd, NULL, CGEDClientListener::m_SyncGEDConnOutPulseCB,
					 inGEDConnOutCtx) != 0)  throw new CException ("could not create pthread");

			SocketClient->Wait();

			inGEDConnOutCtx->md5  = 0;
			inGEDConnOutCtx->addr = CString();
			inGEDConnOutCtx->desc = -1;
			inGEDConnOutCtx->ssl  = NULL;

			CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_CONNECTION, inGEDConnOutCtx->cfg.bind.addr + ":" + 
					CString(inGEDConnOutCtx->cfg.bind.port) + " : disconnection");

			if (inGEDConnOutCtx->cfg.kpalv)
			{
				::pthread_cancel (inGEDConnOutCtx->plsthd);
				::pthread_detach (inGEDConnOutCtx->plsthd);
			}

			if (inGEDConnOutCtx->md5>1 && inGEDConnOutCtx->md5!=3)
			{
				::pthread_cancel (inGEDConnOutCtx->sndthd);
				::pthread_detach (inGEDConnOutCtx->sndthd);
			}
	
			::semctl (inGEDConnOutCtx->sndsem, 0, IPC_RMID);
			::semctl (inGEDConnOutCtx->sem,    0, IPC_RMID);

			inGEDConnOutCtx->sem    = ::semget (IPC_PRIVATE, 1, 0666);
			inGEDConnOutCtx->sndsem = ::semget (IPC_PRIVATE, 1, 0666);

			semun p; 
			p.val = 1; ::semctl (inGEDConnOutCtx->sem,    0, SETVAL, p); 
			p.val = 0; ::semctl (inGEDConnOutCtx->sndsem, 0, SETVAL, p);

			delete SocketClient;
		}
		catch (CException *inException)
		{
			CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_CONNECTION, inGEDConnOutCtx->cfg.bind.addr + ":" + 
				CString(inGEDConnOutCtx->cfg.bind.port) + " : " + inException->GetMessage().Get());

			delete inException;
		}

		if (inGEDConnOutCtx->cfg.kpalv)
		{
			CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_DETAIL_CONN, inGEDConnOutCtx->cfg.bind.addr + ":" + 
				CString(inGEDConnOutCtx->cfg.bind.port) + " : next attempt in " + CString(inGEDConnOutCtx->cfg.kpalv) +
				"s");

			::sleep (inGEDConnOutCtx->cfg.kpalv);
		}
		else
		{
			CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_DETAIL_CONN, inGEDConnOutCtx->cfg.bind.addr + ":" + 
				CString(inGEDConnOutCtx->cfg.bind.port) + " : next attempt in " + CString(inGEDConnOutCtx->cfg.syncs) + 
				"s/" + CString(inGEDConnOutCtx->cfg.syncb) + "bytes");
		}
	}

	::pthread_detach (::pthread_self());

	return NULL;
}

//----------------------------------------------------------------------------------------------------------------------------------------
// connection out context data sending (thread O3)
//----------------------------------------------------------------------------------------------------------------------------------------
void * CGEDClientListener::m_GEDConnOutSendCB (void *inParam)
{
	TGEDConnOutCtx *inGEDConnOutCtx = reinterpret_cast <TGEDConnOutCtx *> (inParam);

	while (true)
	{
		if (CGEDCtx::m_GEDCtx->LockGEDConnOutSendCtx (inGEDConnOutCtx) != 0) 
		{
			inGEDConnOutCtx->md5=3;
			break;
		}

		__PTHREAD_DISABLE_CANCEL__

		if (CGEDCtx::m_GEDCtx->LockGEDConnOutCtx (inGEDConnOutCtx) != 0) 
		{
			inGEDConnOutCtx->md5=3;
			break;
		}

		CGEDCtx::m_GEDCtx->m_BackEnd->Lock();

		TBuffer <TGEDRcd *> inRcds 
			(CGEDCtx::m_GEDCtx->m_BackEnd->Peek (inGEDConnOutCtx->cfg.bind.addr, GED_PKT_REQ_BKD_SYNC, NULL));

		CGEDCtx::m_GEDCtx->m_BackEnd->UnLock();

		for (size_t i=0; i<inRcds.GetLength(); i+=CGEDCtx::m_GEDCtx->m_GEDCfg.pool)
		{
			CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_DETAIL_CONN, inGEDConnOutCtx->cfg.bind.addr + ":" + 
						 CString(inGEDConnOutCtx->cfg.bind.port) + " : performing sync, waiting for ack (" + 
						 CString(inGEDConnOutCtx->cfg.ackto) + "s)");

			TBuffer <TGEDRcd *> outRcds; for (size_t j=i; j<inGEDConnOutCtx->cfg.pool+i && j<inRcds.GetLength(); j++)
				outRcds += *inRcds[j];

			inGEDConnOutCtx->ack.ack  = false;
			inGEDConnOutCtx->ack.hash = ::GetGEDRandomHash();

			TBuffer <TGEDPktOut> outGEDPktOutBuf; for (size_t j=0; j<outRcds.GetLength(); j++)
				outGEDPktOutBuf += ::NewGEDPktOut (static_cast <TGEDSRcd *> (*outRcds[j]) -> pkt);

			if (::pthread_create (&inGEDConnOutCtx->ack.tmrthd, NULL, CGEDClientListener::m_GEDConnOutAckTimerCB, 
				inGEDConnOutCtx) != 0) throw new CException ("could not create pthread");

			::time (&inGEDConnOutCtx->oltm);

			::SendHttpGEDPktToTgt (inGEDConnOutCtx->desc, inGEDConnOutCtx->ssl, &inGEDConnOutCtx->http, outGEDPktOutBuf, inGEDConnOutCtx->ack.hash);

			for (size_t j=0; j<outGEDPktOutBuf.GetLength(); j++) ::DeleteGEDPktOut (*outGEDPktOutBuf[j]);

			::LockGEDAckCtx (&inGEDConnOutCtx->ack);

			if (inGEDConnOutCtx->ack.ack)
			{
				CGEDCtx::m_GEDCtx->m_BackEnd->Lock();

				CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_INFO, GED_SYSLOG_LEV_DETAIL_CONN, inGEDConnOutCtx->cfg.bind.addr + 
					":" + CString(inGEDConnOutCtx->cfg.bind.port) + " : synchronized " + 
					CString((long)outRcds.GetLength()) + " packets");

				if (!inGEDConnOutCtx->cfg.kpalv) CGEDCtx::m_GEDCtx->LockGEDConnOutIncCtx (inGEDConnOutCtx);

				for (size_t j=0; j<outRcds.GetLength(); j++)
				{
					if (!inGEDConnOutCtx->cfg.kpalv)
						inGEDConnOutCtx->totbyt -= 
							(GED_PKT_FIX_SIZE + static_cast <TGEDSRcd *> (*outRcds[j])->pkt->len);

					CGEDCtx::m_GEDCtx->m_BackEnd->Drop (inGEDConnOutCtx->cfg.bind.addr, GED_PKT_REQ_BKD_SYNC, 
						static_cast <TGEDSRcd *> (*outRcds[j]) -> pkt);
				}

				if (!inGEDConnOutCtx->cfg.kpalv) CGEDCtx::m_GEDCtx->UnLockGEDConnOutIncCtx (inGEDConnOutCtx);

				CGEDCtx::m_GEDCtx->m_BackEnd->UnLock();
			}
			else
			{
				CGEDCtx::m_GEDCtx->SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_DETAIL_CONN, inGEDConnOutCtx->cfg.bind.addr +
					":" + CString(inGEDConnOutCtx->cfg.bind.port) + " : sync ack timed out (" + 
					CString(inGEDConnOutCtx->cfg.ackto) + "s), closing socket");

				for (size_t j=inRcds.GetLength(); j>0; j--) ::DeleteGEDRcd (*inRcds[j-1]);

				inGEDConnOutCtx->md5=3;

				::shutdown (inGEDConnOutCtx->desc, SHUT_RDWR);
				::close    (inGEDConnOutCtx->desc);

				CGEDCtx::m_GEDCtx->UnLockGEDConnOutCtx (inGEDConnOutCtx);

				__PTHREAD_ENABLE_CANCEL__

				::pthread_detach (::pthread_self());

				return NULL;
			}
		}

		for (size_t j=inRcds.GetLength(); j>0; j--) ::DeleteGEDRcd (*inRcds[j-1]);

		if (inGEDConnOutCtx->cfg.kpalv == 0L)
		{
			inGEDConnOutCtx->md5=3;

			::shutdown (inGEDConnOutCtx->desc, SHUT_RDWR);
			::close    (inGEDConnOutCtx->desc);

			CGEDCtx::m_GEDCtx->UnLockGEDConnOutCtx (inGEDConnOutCtx);

			__PTHREAD_ENABLE_CANCEL__

			break;
		}

		if (inGEDConnOutCtx->md5 == 1) inGEDConnOutCtx->md5++;

		if (CGEDCtx::m_GEDCtx->UnLockGEDConnOutCtx (inGEDConnOutCtx) != 0)
		{
			__PTHREAD_ENABLE_CANCEL__

			inGEDConnOutCtx->md5=3;

			break;
		}

		__PTHREAD_ENABLE_CANCEL__
	}

	::pthread_detach (::pthread_self());

	return NULL;
}

//----------------------------------------------------------------------------------------------------------------------------------------
// client side md5 packet emission
//----------------------------------------------------------------------------------------------------------------------------------------
void CGEDClientListener::SendGEDPktMd5ToTgt (TGEDConnOutCtx *inGEDConnOutCtx)
{
	TBuffer <TGEDPktOut> outGEDPktOutBuf; 
	CChunk outChunk; outChunk << CGEDCtx::m_GEDCtx->m_Md5Sum.Get(); 
	if (inGEDConnOutCtx->cfg.kpalv) 
		outChunk << (inGEDConnOutCtx->cfg.kpalv+inGEDConnOutCtx->cfg.ackto);
	else
		outChunk << 0L;
	outGEDPktOutBuf += ::NewGEDPktOut (inGEDConnOutCtx->addr, GED_PKT_TYP_MD5, GED_PKT_REQ_NONE, outChunk.GetChunk(), 
					   outChunk.GetSize());
	::SendHttpGEDPktToTgt (inGEDConnOutCtx->desc, inGEDConnOutCtx->ssl, &inGEDConnOutCtx->http, outGEDPktOutBuf);
	::DeleteGEDPktOut (*outGEDPktOutBuf[0]);
}

//----------------------------------------------------------------------------------------------------------------------------------------
// client side ack packet emission
//----------------------------------------------------------------------------------------------------------------------------------------
void CGEDClientListener::SendGEDPktAckToTgt (TGEDConnOutCtx *inGEDConnOutCtx, CString inDigest)
{
	TBuffer <TGEDPktOut> outGEDPktOutBuf;
	outGEDPktOutBuf += ::NewGEDPktOut (CString(), GED_PKT_TYP_ACK, GED_PKT_REQ_NONE, inDigest.Get(), inDigest.GetLength()+1);
	::SendHttpGEDPktToTgt (inGEDConnOutCtx->desc, inGEDConnOutCtx->ssl, &inGEDConnOutCtx->http, outGEDPktOutBuf);
	::DeleteGEDPktOut (*outGEDPktOutBuf[0]);
}

#ifdef __GED_TUN__
//----------------------------------------------------------------------------------------------------------------------------------------
// client tun listener
//----------------------------------------------------------------------------------------------------------------------------------------
CGEDTunClientListener::CGEDTunClientListener (const TGEDConnInCtx *inGEDConnInCtx, const int inId, const CString &inTunSpec)
		      :m_GEDConnInCtx	     (const_cast <TGEDConnInCtx *> (inGEDConnInCtx)),
		       m_Id		     (inId),
		       m_TunSpec	     (inTunSpec)
{ }

//----------------------------------------------------------------------------------------------------------------------------------------
// client tun data reception
//----------------------------------------------------------------------------------------------------------------------------------------
void CGEDTunClientListener::OnReceive (CObject *inSender, const void *inData, const int inLen)
{
	__PTHREAD_DISABLE_CANCEL__

	if (CGEDCtx::m_GEDCtx->LockGEDConnInCtx() == 0)
	{
		TBuffer <TGEDPktOut> outGEDPktOutBuf;

		char *outData = new char [sizeof(SInt32)+inLen];
		::memcpy (outData, &m_Id, sizeof(SInt32));
		::memcpy (outData + sizeof(SInt32), inData, inLen);

		outGEDPktOutBuf += ::NewGEDPktOut (CString(), GED_PKT_TYP_DATA_TUN, GED_PKT_REQ_NONE, outData, sizeof(SInt32)+inLen);

		if (CGEDCtx::m_GEDCtx->LockGEDConnInSndCtx (m_GEDConnInCtx) == 0)
		{
			if (m_GEDConnInCtx->port != 0L)
				::SendHttpGEDPktToSrc (m_GEDConnInCtx->desc, m_GEDConnInCtx->ssl, &m_GEDConnInCtx->http, outGEDPktOutBuf);
			else
				::SendRawGEDPkt (m_GEDConnInCtx->desc, outGEDPktOutBuf);

			CGEDCtx::m_GEDCtx->UnLockGEDConnInSndCtx (m_GEDConnInCtx);
		}

		::DeleteGEDPktOut (*outGEDPktOutBuf[0]); delete [] outData;
		
		CGEDCtx::m_GEDCtx->UnLockGEDConnInCtx();
	}

	__PTHREAD_ENABLE_CANCEL__
}

//----------------------------------------------------------------------------------------------------------------------------------------
// client tun disconnection
//----------------------------------------------------------------------------------------------------------------------------------------
void CGEDTunClientListener::OnDisconnect (CObject *inSender)
{
	__PTHREAD_DISABLE_CANCEL__

	if (CGEDCtx::m_GEDCtx->LockGEDConnInCtx() == 0)
	{
		TBuffer <TGEDPktOut> outGEDPktOutBuf; CChunk outChunk; outChunk << (SInt32)m_Id;

		outGEDPktOutBuf += ::NewGEDPktOut (CString(), GED_PKT_TYP_SHUT_TUN, GED_PKT_REQ_NONE, outChunk.GetChunk(), outChunk.GetSize());

		if (CGEDCtx::m_GEDCtx->LockGEDConnInSndCtx (m_GEDConnInCtx) == 0)
		{
			if (m_GEDConnInCtx->port != 0L)
				::SendHttpGEDPktToSrc (m_GEDConnInCtx->desc, m_GEDConnInCtx->ssl, &m_GEDConnInCtx->http, outGEDPktOutBuf);
			else
				::SendRawGEDPkt (m_GEDConnInCtx->desc, outGEDPktOutBuf);

			CGEDCtx::m_GEDCtx->UnLockGEDConnInSndCtx (m_GEDConnInCtx);
		}

		::DeleteGEDPktOut (*outGEDPktOutBuf[0]);

		m_GEDConnInCtx->tuns -= static_cast <CSocketClient *> (inSender);

		CGEDCtx::m_GEDCtx->UnLockGEDConnInCtx();

		__PTHREAD_ENABLE_CANCEL__

		delete inSender;
	}
}
#endif


