/****************************************************************************************************************************************
 gedq.c
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

#include "gedq.h"

CGEDQCtx * CGEDQCtx::m_GEDQCtx = NULL;
int	   CGEDQCtx::m_Res     = 0;
bool	   CGEDQCtx::m_Light   = false;

//---------------------------------------------------------------------------------------------------------------------------------------
// gedq context sigterm handler
//---------------------------------------------------------------------------------------------------------------------------------------
void CGEDQCtx::m_SigTerm (int inSig)
{
	::signal (inSig, CGEDQCtx::m_SigTerm);

	if (CGEDQCtx::m_GEDQCtx->m_GEDQSocketClient != NULL)
	{
		::shutdown (CGEDQCtx::m_GEDQCtx->m_GEDQSocketClient->GetDesc(), SHUT_RDWR);
		::close    (CGEDQCtx::m_GEDQCtx->m_GEDQSocketClient->GetDesc());
	}
}

//---------------------------------------------------------------------------------------------------------------------------------------
// gedq context constructor
//---------------------------------------------------------------------------------------------------------------------------------------
CGEDQCtx::CGEDQCtx  		(const TGEDQCfg &inGEDQCfg, const CString &inTunSpec) THROWABLE
	 :m_GEDQCfg 		(const_cast <TGEDQCfg &> (inGEDQCfg)),
	  m_Md5Sum  		(::HashGEDPktCfg(inGEDQCfg.pkts)),
	  #ifdef __GED_TUN__
	  m_TunSpec		(inTunSpec),
	  m_GEDQSocketServer	(NULL),
	  m_GEDQConnInTunsSem	(-1),
	  #endif
	  m_GEDQSocketClient	(NULL),
	  m_XMLdoc		(NULL),
	  m_XMLroot		(NULL),
	  m_oltm		(::time(NULL)),
	  m_iltm		(::time(NULL))
{
	CGEDQCtx::m_GEDQCtx = this;

        struct sigaction sa; sa.sa_handler = CGEDQCtx::m_SigTerm; sa.sa_flags = SA_NOCLDSTOP;
	::sigaction (SIGINT,  &sa, NULL);
	::sigaction (SIGTERM, &sa, NULL);
	::sigaction (SIGQUIT, &sa, NULL);

	m_GEDQHttpReqCtx.cmd   = m_GEDQCfg.http.cmd;
	m_GEDQHttpReqCtx.vrs   = m_GEDQCfg.http.vrs;
	m_GEDQHttpReqCtx.agt   = m_GEDQCfg.http.agt;
	m_GEDQHttpReqCtx.typ   = m_GEDQCfg.http.typ;
	m_GEDQHttpReqCtx.zlv   = m_GEDQCfg.http.zlv;
	m_GEDQHttpReqCtx.host  = m_GEDQCfg.bind.addr;
	#ifdef __GED_TUN__
	m_GEDQHttpReqCtx.kpalv = (m_TunSpec.GetLength() == 0) ? false : true;
	#else
	m_GEDQHttpReqCtx.kpalv = false;
	#endif

	CString outBindAddr (m_GEDQCfg.bind.addr); if (m_GEDQCfg.proxy.addr != CString()) outBindAddr = m_GEDQCfg.proxy.addr;
	UInt32  outBindPort (m_GEDQCfg.bind.port); if (m_GEDQCfg.proxy.port != 0L	) outBindPort = m_GEDQCfg.proxy.port;
	
	try
	{
		if (m_GEDQCfg.bind.addr != CString() && m_GEDQCfg.bind.port != 0L)
		{
			if (m_GEDQCfg.tls.ca  != CString() || m_GEDQCfg.tls.crt != CString() || m_GEDQCfg.tls.key != CString())
			{
				m_GEDQSocketClient = new CSocketSSLClient (
						outBindAddr,
						outBindPort,
						m_GEDQCfg.tls.ca,
						m_GEDQCfg.tls.crt,
						m_GEDQCfg.tls.key,
						m_GEDQCfg.tls.vfy,
					 	m_GEDQCfg.tls.cph,
						m_GEDQCfg.bind.bind,
						NULL);
			}
			else
			{
				m_GEDQSocketClient = new CSocketClient (
						outBindAddr,
						outBindPort,
						m_GEDQCfg.bind.bind,
						NULL);
			}
		}
		else
		{
			#ifdef __GED_TUN__
			if (m_TunSpec != CString() && m_GEDQCfg.bind.port == 0L)
				throw new CException (CString("can not perform tun requests on a local sock file"));
			#endif

			m_GEDQSocketClient = new CSocketClient (
						m_GEDQCfg.bind.sock,
						(const CSocketClientListener*)NULL);
		}
		
		#ifdef __GED_TUN__
		if (m_TunSpec != CString() && m_TunSpec.Find(CString(":")))
			m_GEDQSocketServer = new CSocketServer (CString(), m_TunSpec.Cut(CString(":"))[0]->ToULong(), NULL);
		#endif
	}
	catch (CException *e)
	{
		#ifdef __GED_TUN__
		if (m_GEDQConnInTunsSem != -1) ::semctl (m_GEDQConnInTunsSem, 0, IPC_RMID);
		#endif

		throw e;
	}
}

//---------------------------------------------------------------------------------------------------------------------------------------
// gedq context destructor
//---------------------------------------------------------------------------------------------------------------------------------------
CGEDQCtx::~CGEDQCtx ()
{
	#ifdef __GED_TUN__
	if (LockGEDQConnInTuns() == 0 && m_GEDQSocketClient != NULL)
	{
		for (size_t i=0; i<m_GEDQConnInTuns.GetLength(); i++)
		{
			TBuffer <TGEDPktOut> outGEDPktOutBuf;

			outGEDPktOutBuf += ::NewGEDPktOut (m_GEDQSocketClient->GetAddr(), 
							   GED_PKT_TYP_SHUT_TUN, 
							   GED_PKT_REQ_NONE, 
							   m_GEDQConnInTuns[i], 
							   sizeof(int));

			if (m_GEDQSocketClient->Lock() == 0)
			{
				::time (&m_oltm);

				if (m_GEDQSocketClient->GetSockAddr()->sa_family == AF_INET)
					::SendHttpGEDPktToTgt  (m_GEDQSocketClient->GetDesc(), 
								m_GEDQSocketClient->ClassIs(__metaclass(CSocketSSLClient)) ? 
							   	static_cast <CSocketSSLClient *> (m_GEDQSocketClient) -> GetSSL() : NULL,
							   	&m_GEDQHttpReqCtx, outGEDPktOutBuf, CString());
				else
					::SendRawGEDPkt (m_GEDQSocketClient->GetDesc(), outGEDPktOutBuf);

				m_GEDQSocketClient->UnLock();
			}

			for (size_t j=outGEDPktOutBuf.GetLength(); j>0; j--) ::DeleteGEDPktOut (*outGEDPktOutBuf[j-1]);
		}
	}

	if (m_GEDQConnInTunsSem != -1) ::semctl (m_GEDQConnInTunsSem, 0, IPC_RMID);
	#endif

	if (m_GEDQSocketClient != NULL && m_GEDQSocketClient->Lock() == 0)
	{
		::shutdown (m_GEDQSocketClient->GetDesc(), SHUT_RDWR);
		::close    (m_GEDQSocketClient->GetDesc());

		delete m_GEDQSocketClient;
	}

	#ifdef __GED_TUN__
	if (m_GEDQSocketServer != NULL) delete m_GEDQSocketServer;
	#endif

	if (m_XMLdoc != NULL)
	{
		if (CGEDQCtx::m_Res == 0)
		{
			if (m_Light)
			{
				::xmlSaveFormatFileEnc ("-", m_XMLdoc, "UTF-8", 0);
			}
			else
			{
				::xmlSaveFormatFileEnc ("-", m_XMLdoc, "UTF-8", 1);
			}
		}

		::xmlFreeDoc (m_XMLdoc);
	}
}

//---------------------------------------------------------------------------------------------------------------------------------------
// context run
//---------------------------------------------------------------------------------------------------------------------------------------
void CGEDQCtx::Run (const TBuffer <TGEDPktOut> &inGEDPktOut) THROWABLE
{
	#ifdef __GED_TUN__
	if ((m_GEDQConnInTunsSem = ::semget (IPC_PRIVATE, 1, 0666)) < 0)
		throw new CException (CString("CGEDQCtx::CGEDQCtx could not allocate semaphore"));

	semun p; p.val = 1; ::semctl (m_GEDQConnInTunsSem, 0, SETVAL, p);
	#endif

	m_GEDQSocketClient -> AssignListener (new CGEDQClientListener());

	#ifdef __GED_TUN__
	if (m_TunSpec != CString() && m_TunSpec.Find(CString(":")))
		m_GEDQSocketServer -> AssignListener (new CGEDQServerListener());
	#endif

	TBuffer <TGEDPktOut> outGEDPktOutBuf;

	CChunk outChunk; outChunk << m_Md5Sum.Get(); 
	if (m_GEDQCfg.ackto > 0L)
		outChunk << (m_GEDQCfg.kpalv + m_GEDQCfg.ackto);
	else
		outChunk << 0L;
	outGEDPktOutBuf += ::NewGEDPktOut (m_GEDQSocketClient->GetAddr(), GED_PKT_TYP_MD5, GED_PKT_REQ_NONE, outChunk.GetChunk(), outChunk.GetSize());

	if (inGEDPktOut.GetLength() > 0)
	{
		for (size_t i=0; i<inGEDPktOut.GetLength(); i++)
		{
			if ((((TGEDPktIn*)(*inGEDPktOut[i]))->req & GED_PKT_REQ_MASK) == GED_PKT_REQ_PEEK)
			{
				m_XMLdoc  = ::xmlNewDoc  ((xmlChar*)"1.0");
				m_XMLroot = ::xmlNewNode (NULL, (xmlChar*)"ged");
				::xmlNewProp (m_XMLroot, XML_ELEMENT_NODE_GED_ROOT_VERSION_ATTR, (xmlChar*)GED_VERSION_STR.Get());
				::xmlDocSetRootElement (m_XMLdoc, m_XMLroot);
			}
		}

		outGEDPktOutBuf += inGEDPktOut;
	}

	if (m_GEDQSocketClient->Lock() == 0)
	{
		m_GEDAckCtx.ack  = false;
		m_GEDAckCtx.hash = ::GetGEDRandomHash();

		#ifdef __GED_TUN__
		if (m_TunSpec == CString())
		#endif
		{
			if (m_GEDQCfg.ackto > 0)
				if (::pthread_create (&CGEDQCtx::m_GEDQCtx->m_GEDAckCtx.tmrthd, NULL, CGEDQCtx::m_GEDQAckTimerCB, NULL) != 0)
					throw new CException (CString("CGEDQCtx::Run could not create ack pthread"));
		}
		#ifdef __GED_TUN__
		else
		{
			pthread_t pth; if (::pthread_create (&pth, NULL, CGEDQCtx::m_GEDQTunPulseCB, NULL) != 0)
				throw new CException (CString("CGEDQCtx::Run could not create pulse pthread"));
		}
		#endif

		::time (&m_oltm);

		#ifdef __GED_TUN__
		if (m_GEDQSocketClient->GetSockAddr()->sa_family == AF_INET)
			::SendHttpGEDPktToTgt (m_GEDQSocketClient->GetDesc(), 
					       m_GEDQSocketClient->ClassIs(__metaclass(CSocketSSLClient)) ? 
					       static_cast <CSocketSSLClient *> (m_GEDQSocketClient) -> GetSSL() : NULL,
					       &m_GEDQHttpReqCtx, outGEDPktOutBuf, (m_TunSpec == CString()) ? m_GEDAckCtx.hash : CString());
		else
			::SendRawGEDPkt (m_GEDQSocketClient->GetDesc(), outGEDPktOutBuf);
		#else
		if (m_GEDQSocketClient->GetSockAddr()->sa_family == AF_INET)
			::SendHttpGEDPktToTgt (m_GEDQSocketClient->GetDesc(), 
					       m_GEDQSocketClient->ClassIs(__metaclass(CSocketSSLClient)) ? 
					       static_cast <CSocketSSLClient *> (m_GEDQSocketClient) -> GetSSL() : NULL,
					       &m_GEDQHttpReqCtx, outGEDPktOutBuf, m_GEDAckCtx.hash);
		else
			::SendRawGEDPkt (m_GEDQSocketClient->GetDesc(), outGEDPktOutBuf);
		#endif

		m_GEDQSocketClient->UnLock();
	}

	for (size_t i=outGEDPktOutBuf.GetLength(); i>0; i--) ::DeleteGEDPktOut (*outGEDPktOutBuf[i-1]);

	m_GEDQSocketClient->Wait();
}

#ifdef __GED_TUN__
//---------------------------------------------------------------------------------------------------------------------------------------
// context tun descriptors buffer lock
//---------------------------------------------------------------------------------------------------------------------------------------
int CGEDQCtx::LockGEDQConnInTuns ()
{
	struct sembuf op[1]; op[0].sem_num = 0; op[0].sem_op = -1; op[0].sem_flg = 0;
	return ::semop (m_GEDQConnInTunsSem, op, 1);
}

//---------------------------------------------------------------------------------------------------------------------------------------
// context tun descriptors buffer unlock
//---------------------------------------------------------------------------------------------------------------------------------------
int CGEDQCtx::UnLockGEDQConnInTuns ()
{
	struct sembuf op[1]; op[0].sem_num = 0; op[0].sem_op = 1; op[0].sem_flg = 0;
	return ::semop (m_GEDQConnInTunsSem, op, 1);
}

//---------------------------------------------------------------------------------------------------------------------------------------
// whenever a tunnel is used, this callback sends a heartbeat to target trying to keep the connection alive
//---------------------------------------------------------------------------------------------------------------------------------------
void * CGEDQCtx::m_GEDQTunPulseCB (void *)
{
	SInt32 patience = CGEDQCtx::m_GEDQCtx->m_GEDQCfg.kpalv;

	while (true)
	{
		::sleep (patience); time_t tm; ::time (&tm);

		__PTHREAD_DISABLE_CANCEL__

		if (CGEDQCtx::m_GEDQCtx->m_GEDQSocketClient->Lock() != 0) 
		{
			__PTHREAD_ENABLE_CANCEL__

			break;
		}

		if ((tm - CGEDQCtx::m_GEDQCtx->m_oltm) >= CGEDQCtx::m_GEDQCtx->m_GEDQCfg.kpalv)
		{
			CGEDQCtx::m_GEDQCtx->m_GEDAckCtx.ack  = false;
			CGEDQCtx::m_GEDQCtx->m_GEDAckCtx.hash = ::GetGEDRandomHash();

			TBuffer <TGEDPktOut> outGEDPktOutBuf; 
			outGEDPktOutBuf += ::NewGEDPktOut (CGEDQCtx::m_GEDQCtx->m_GEDQSocketClient->GetAddr(), GED_PKT_TYP_PULSE, GED_PKT_REQ_NONE, NULL, 0);

			if (::pthread_create (&CGEDQCtx::m_GEDQCtx->m_GEDAckCtx.tmrthd, NULL, CGEDQCtx::m_GEDQAckTimerCB, NULL) != 0)
			{
				CGEDQCtx::m_GEDQCtx->m_GEDQSocketClient->UnLock();

				__PTHREAD_ENABLE_CANCEL__ 

				throw new CException ("CGEDQCtx::m_GEDQTunPulseCB could not create ack pthread");
			}

			::time (&CGEDQCtx::m_GEDQCtx->m_oltm);

			if (CGEDQCtx::m_GEDQCtx->m_GEDQSocketClient->GetSockAddr()->sa_family == AF_INET)
				::SendHttpGEDPktToTgt (CGEDQCtx::m_GEDQCtx->m_GEDQSocketClient->GetDesc(), CGEDQCtx::m_GEDQCtx->m_GEDQSocketClient->ClassIs(__metaclass(CSocketSSLClient)) ? 
					static_cast <CSocketSSLClient *> (CGEDQCtx::m_GEDQCtx->m_GEDQSocketClient) -> GetSSL() : NULL, &CGEDQCtx::m_GEDQCtx->m_GEDQHttpReqCtx, 
					outGEDPktOutBuf, CGEDQCtx::m_GEDQCtx->m_GEDAckCtx.hash);
			else
				::SendRawGEDPkt (CGEDQCtx::m_GEDQCtx->m_GEDQSocketClient->GetDesc(), outGEDPktOutBuf);

			::DeleteGEDPktOut (*outGEDPktOutBuf[0]);

			::LockGEDAckCtx (&CGEDQCtx::m_GEDQCtx->m_GEDAckCtx);

			if (!CGEDQCtx::m_GEDQCtx->m_GEDAckCtx.ack)
			{
				::shutdown (CGEDQCtx::m_GEDQCtx->m_GEDQSocketClient->GetDesc(), SHUT_RDWR);
				::close    (CGEDQCtx::m_GEDQCtx->m_GEDQSocketClient->GetDesc());

				CGEDQCtx::m_GEDQCtx->m_GEDQSocketClient->UnLock();

				__PTHREAD_ENABLE_CANCEL__

				::pthread_detach (::pthread_self());

				return NULL;
			}
			
			::time (&tm); patience = CGEDQCtx::m_GEDQCtx->m_GEDQCfg.kpalv - (tm - CGEDQCtx::m_GEDQCtx->m_oltm); 

			if (CGEDQCtx::m_GEDQCtx->m_GEDQSocketClient->UnLock() != 0)
			{
				__PTHREAD_ENABLE_CANCEL__

				break;
			}

			__PTHREAD_ENABLE_CANCEL__

			continue;
		}

		patience = CGEDQCtx::m_GEDQCtx->m_GEDQCfg.kpalv - (tm - CGEDQCtx::m_GEDQCtx->m_oltm);
		if (patience < 0) patience = 0;

		if (CGEDQCtx::m_GEDQCtx->m_GEDQSocketClient->UnLock() != 0) 
		{
			__PTHREAD_ENABLE_CANCEL__

			break;
		}

		__PTHREAD_ENABLE_CANCEL__
	}

	::pthread_detach (::pthread_self());

	return NULL;
}
#endif

//---------------------------------------------------------------------------------------------------------------------------------------
// ack timer callback
//---------------------------------------------------------------------------------------------------------------------------------------
void * CGEDQCtx::m_GEDQAckTimerCB (void *)
{
	unsigned long patience = CGEDQCtx::m_GEDQCtx->m_GEDQCfg.ackto;
	
	while (true)
	{
		::sleep (patience); time_t tm; ::time (&tm); 
		
		if ((tm - CGEDQCtx::m_GEDQCtx->m_iltm) >= CGEDQCtx::m_GEDQCtx->m_GEDQCfg.ackto)
		{
			CGEDQCtx::m_GEDQCtx->m_GEDAckCtx.hash = CString();

			::UnLockGEDAckCtx (&CGEDQCtx::m_GEDQCtx->m_GEDAckCtx);

			::fprintf (stderr, ("ERROR : no ack reception within the expected time (" + 
				   CString(CGEDQCtx::m_GEDQCtx->m_GEDQCfg.ackto) + 
				   "s) the target " + CGEDQCtx::m_GEDQCtx->m_GEDQCfg.bind.addr + ":" + 
				   CString(CGEDQCtx::m_GEDQCtx->m_GEDQCfg.bind.port) + " may have failed to handle the request\n").Get());

			#ifdef __GED_TUN__
			if (CGEDQCtx::m_GEDQCtx->m_TunSpec == CString())
			#endif
			{
				CGEDQCtx::m_Res = 1;

				::shutdown (CGEDQCtx::m_GEDQCtx->m_GEDQSocketClient->GetDesc(), SHUT_RDWR);
				::close    (CGEDQCtx::m_GEDQCtx->m_GEDQSocketClient->GetDesc());
			}

			break;
		}

		patience = CGEDQCtx::m_GEDQCtx->m_GEDQCfg.ackto - (tm - CGEDQCtx::m_GEDQCtx->m_iltm);
	}

	::pthread_detach (::pthread_self());

	return NULL;
}

//---------------------------------------------------------------------------------------------------------------------------------------
// gedq client listener constructor
//---------------------------------------------------------------------------------------------------------------------------------------
CGEDQClientListener::CGEDQClientListener ()
		    :m_GEDPktInCtx	 (::NewGEDPktInCtx())
{ }

//---------------------------------------------------------------------------------------------------------------------------------------
// gedq client listener destructor
//---------------------------------------------------------------------------------------------------------------------------------------
CGEDQClientListener::~CGEDQClientListener ()
{
	::DeleteGEDPktInCtx (m_GEDPktInCtx);
}

//---------------------------------------------------------------------------------------------------------------------------------------
// gedq client listener connection handler (potential proxy passthru handling)
//---------------------------------------------------------------------------------------------------------------------------------------
void CGEDQClientListener::OnConnect (CObject *inSender, void *)
{
	if (CGEDQCtx::m_GEDQCtx->m_GEDQCfg.proxy.addr == CString()) 
		return;

	CString outMethod (inSender->ClassIs(__metaclass(CSocketSSLClient)) ? "CONNECT " : (CGEDQCtx::m_GEDQCtx->m_GEDQCfg.http.cmd + " "));

	CString outCmd (outMethod + CGEDQCtx::m_GEDQCtx->m_GEDQCfg.bind.addr + ":" + CString(CGEDQCtx::m_GEDQCtx->m_GEDQCfg.bind.port) + 
			" HTTP/" + CGEDQCtx::m_GEDQCtx->m_GEDQCfg.http.vrs + GED_HTTP_REGEX_CRLF);

	if (CGEDQCtx::m_GEDQCtx->m_GEDQCfg.http.agt != CString())
		outCmd += "User-Agent: " + CGEDQCtx::m_GEDQCtx->m_GEDQCfg.http.agt + GED_HTTP_REGEX_CRLF;

	int n=1, s=200;

	switch (CGEDQCtx::m_GEDQCtx->m_GEDQCfg.proxy.auth)
	{
		case GED_HTTP_PROXY_AUTH_NONE :
		{
			if (inSender->ClassIs(__metaclass(CSocketSSLClient)))
			{
				outCmd += GED_HTTP_REGEX_CRLF;

				::time (&CGEDQCtx::m_GEDQCtx->m_oltm);

				static_cast <CSocket *> (inSender) -> Send (outCmd.Get(), outCmd.GetLength());

				char ans[1024]; CString inAnswer; while (!inAnswer.Find (GED_HTTP_REGEX_CRLF + GED_HTTP_REGEX_CRLF))
				{
					if (static_cast <CSocket *> (inSender) -> Receive (ans, 1024) <= 0)
						throw new CException (CString("could not read answer from proxy " + 
							CGEDQCtx::m_GEDQCtx->m_GEDQCfg.proxy.addr + ":" + CString(CGEDQCtx::m_GEDQCtx->m_GEDQCfg.proxy.port)));

					ans[1023] = '\0'; inAnswer += ans;
				}

				n = ::sscanf (inAnswer.Get(), "%*s %d", &s);

				if (n >= 1 && s == 407)
					throw new CException ("proxy code " + CString((SInt32)s) + " returned from " + 
						CGEDQCtx::m_GEDQCtx->m_GEDQCfg.proxy.addr + ":" + CString(CGEDQCtx::m_GEDQCtx->m_GEDQCfg.proxy.port) + 
						", authenticiation required but none specified");
			}
		}
		break;

		case GED_HTTP_PROXY_AUTH_BASIC :
		{
			outCmd += "Proxy-Authorization: Basic " + 
				::Base64Encode (CGEDQCtx::m_GEDQCtx->m_GEDQCfg.proxy.user + ":" + 
						CGEDQCtx::m_GEDQCtx->m_GEDQCfg.proxy.pass) + GED_HTTP_REGEX_CRLF;

			outCmd += GED_HTTP_REGEX_CRLF;

			::time (&CGEDQCtx::m_GEDQCtx->m_oltm);

			static_cast <CSocket *> (inSender) -> Send (outCmd.Get(), outCmd.GetLength());

			char ans[1024]; CString inAnswer; while (!inAnswer.Find (GED_HTTP_REGEX_CRLF + GED_HTTP_REGEX_CRLF))
			{
				if (static_cast <CSocket *> (inSender) -> Receive (ans, 1024) <= 0)
					throw new CException (CString("could not read answer from proxy " + 
						CGEDQCtx::m_GEDQCtx->m_GEDQCfg.proxy.addr + ":" + CString(CGEDQCtx::m_GEDQCtx->m_GEDQCfg.proxy.port)));

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

			::time (&CGEDQCtx::m_GEDQCtx->m_oltm);

			static_cast <CSocket *> (inSender) -> Send (outCmd.Get(), outCmd.GetLength());

			char ans[1024]; CString inAnswer; while (!inAnswer.Find (GED_HTTP_REGEX_CRLF + GED_HTTP_REGEX_CRLF))
			{
				if (static_cast <CSocket *> (inSender) -> Receive (ans, 1024) <= 0)
					throw new CException (CString("could not read answer from proxy " + 
						CGEDQCtx::m_GEDQCtx->m_GEDQCfg.proxy.addr + ":" + CString(CGEDQCtx::m_GEDQCtx->m_GEDQCfg.proxy.port)));

				ans[1023] = '\0'; inAnswer += ans;
			}

			n = ::sscanf (inAnswer.Get(), "%*s %d", &s);

			if (n >= 1 && s == 407)
			{
				if (!inAnswer.Find (CString("NTLM"), 0, (size_t*)&n))
					throw new CException (CString("could not retreive NTLM challenge from proxy " + 
						CGEDQCtx::m_GEDQCtx->m_GEDQCfg.proxy.addr + ":" + CString(CGEDQCtx::m_GEDQCtx->m_GEDQCfg.proxy.port)));

				n = ::sscanf (inAnswer.Get()+n, "NTLM %s", ans); ans[127]=0;

				if (n != 1)
					throw new CException (CString("could not retreive NTLM challenge from proxy " + 
						CGEDQCtx::m_GEDQCtx->m_GEDQCfg.proxy.addr + ":" + CString(CGEDQCtx::m_GEDQCtx->m_GEDQCfg.proxy.port)));

				outCmd = outMethod + CGEDQCtx::m_GEDQCtx->m_GEDQCfg.bind.addr + ":" + CString(CGEDQCtx::m_GEDQCtx->m_GEDQCfg.bind.port) + 
					 " HTTP/" + CGEDQCtx::m_GEDQCtx->m_GEDQCfg.http.vrs + GED_HTTP_REGEX_CRLF;

				if (CGEDQCtx::m_GEDQCtx->m_GEDQCfg.http.agt != CString())
					outCmd += "User-Agent: " + CGEDQCtx::m_GEDQCtx->m_GEDQCfg.http.agt + GED_HTTP_REGEX_CRLF;

				outCmd += "Host: " + CGEDQCtx::m_GEDQCtx->m_GEDQCfg.bind.addr + GED_HTTP_REGEX_CRLF;

				outCmd += "Proxy-Authorization: NTLM " + ::GetHttpProxyAuthNTLM3 (CGEDQCtx::m_GEDQCtx->m_GEDQCfg.proxy.user,
					CGEDQCtx::m_GEDQCtx->m_GEDQCfg.proxy.pass, ans) + GED_HTTP_REGEX_CRLF;

				outCmd += GED_HTTP_REGEX_CRLF;

				static_cast <CSocket *> (inSender) -> Send (outCmd.Get(), outCmd.GetLength());

				inAnswer = ""; while (!inAnswer.Find (GED_HTTP_REGEX_CRLF + GED_HTTP_REGEX_CRLF))
				{
					if (static_cast <CSocket *> (inSender) -> Receive (ans, 1024) <= 0)
						throw new CException (CString("could not read answer from proxy " + 
							CGEDQCtx::m_GEDQCtx->m_GEDQCfg.proxy.addr + ":" + CString(CGEDQCtx::m_GEDQCtx->m_GEDQCfg.proxy.port)));

					ans[1023] = '\0'; inAnswer += ans;
				}

				n = ::sscanf (inAnswer.Get(), "%*s %d", &s);
			}
		}
		break;
		#endif
	}

	if (n < 1 || s != 200)
		throw new CException (CString("proxy " + CGEDQCtx::m_GEDQCtx->m_GEDQCfg.proxy.addr + ":" + 
			      CString(CGEDQCtx::m_GEDQCtx->m_GEDQCfg.proxy.port) + " returned bad status code " + CString((SInt32)s)));
}

//---------------------------------------------------------------------------------------------------------------------------------------
// gedq client listener raw data reception handler
//---------------------------------------------------------------------------------------------------------------------------------------
void CGEDQClientListener::OnReceive (CObject *inSender, const void *inData, const int inLen)
{
	int inRes=0;

	if (static_cast <CSocket *> (inSender) -> GetSockAddr() -> sa_family == AF_INET)
		inRes = ::RecvHttpGEDPktFromBuf (const_cast <void *> (inData), inLen, m_GEDPktInCtx, CGEDQClientListener::m_RecvGEDPktCB, NULL);
	else
		inRes = ::RecvRawGEDPktFromBuf  (const_cast <void *> (inData), inLen, m_GEDPktInCtx, CGEDQClientListener::m_RecvGEDPktCB, NULL);

	if (inRes != inLen)
	{
		::fprintf (stderr, "CGEDQClientListener::OnReceive : data reception corruption, closing socket\n");

		::shutdown (CGEDQCtx::m_GEDQCtx->m_GEDQSocketClient->GetDesc(), SHUT_RDWR);
		::close    (CGEDQCtx::m_GEDQCtx->m_GEDQSocketClient->GetDesc());
	}
}

//---------------------------------------------------------------------------------------------------------------------------------------
// gedq client listener packet reception callback
//---------------------------------------------------------------------------------------------------------------------------------------
void CGEDQClientListener::m_RecvGEDPktCB (const CString &inHttp, long inIndex, bool, TGEDPktIn *inGEDPktIn, void *)
{
	::time (&CGEDQCtx::m_GEDQCtx->m_iltm);

	switch (inGEDPktIn->typ)
	{
		case GED_PKT_TYP_CLOSE :
		{
			if (inGEDPktIn->data != NULL)
			{
				CGEDQCtx::m_Res = 1;

				::fprintf (stderr, "ERROR : %s\n", (char*)inGEDPktIn->data);
			}

			::shutdown (CGEDQCtx::m_GEDQCtx->m_GEDQSocketClient->GetDesc(), SHUT_RDWR);
			::close    (CGEDQCtx::m_GEDQCtx->m_GEDQSocketClient->GetDesc());
		}
		break;

		case GED_PKT_TYP_ACK :
		{
			if (CString((char*)inGEDPktIn->data) == CGEDQCtx::m_GEDQCtx->m_GEDAckCtx.hash)
			{
				CGEDQCtx::m_GEDQCtx->m_GEDAckCtx.ack = true;

				if (CGEDQCtx::m_GEDQCtx->m_GEDQCfg.ackto > 0)
				{
					::pthread_cancel (CGEDQCtx::m_GEDQCtx->m_GEDAckCtx.tmrthd);
					::pthread_detach (CGEDQCtx::m_GEDQCtx->m_GEDAckCtx.tmrthd);
				}

				::UnLockGEDAckCtx (&CGEDQCtx::m_GEDQCtx->m_GEDAckCtx);
			}

			#ifdef __GED_TUN__
			if (CGEDQCtx::m_GEDQCtx->m_TunSpec == CString())
			#endif
			{
				::shutdown (CGEDQCtx::m_GEDQCtx->m_GEDQSocketClient->GetDesc(), SHUT_RDWR);
				::close    (CGEDQCtx::m_GEDQCtx->m_GEDQSocketClient->GetDesc());
			}
		}
		break;

		#ifdef __GED_TUN__
		case GED_PKT_TYP_DATA_TUN :
		{
			if (inGEDPktIn->len > sizeof(int))
				CSocket::Send (*reinterpret_cast <int *> (inGEDPktIn->data), reinterpret_cast <UInt8 *> (inGEDPktIn->data) + sizeof(int), inGEDPktIn->len - sizeof(int));
		}
		break;
	
		case GED_PKT_TYP_SHUT_TUN :
		{
			::shutdown (*reinterpret_cast <int *> (inGEDPktIn->data), SHUT_RDWR);
			::close    (*reinterpret_cast <int *> (inGEDPktIn->data));
		}
		break;
		#endif

		case GED_PKT_TYP_RECORD	:
		{
			TGEDRcd *inGEDRcd = reinterpret_cast <TGEDRcd *> (inGEDPktIn->data);
			
			#ifdef __GED_TUN__
			if (CGEDQCtx::m_GEDQCtx->m_TunSpec == CString())
			#endif
			{
				if (CGEDQCtx::m_GEDQCtx->m_Light)
				{
					::GEDRcdToXmlNodeLight (inGEDRcd, CGEDQCtx::m_GEDQCtx->m_GEDQCfg.pkts, CGEDQCtx::m_GEDQCtx->m_XMLroot);
				}
				else
				{
					::GEDRcdToXmlNode      (inGEDRcd, CGEDQCtx::m_GEDQCtx->m_GEDQCfg.pkts, CGEDQCtx::m_GEDQCtx->m_XMLroot);
				}
			}
		}
		break;
	}
}

#ifdef __GED_TUN__
//---------------------------------------------------------------------------------------------------------------------------------------
// gedq tun new connection handler
//---------------------------------------------------------------------------------------------------------------------------------------
void CGEDQServerListener::OnConnect (CObject *, const CString &inAddr, const UInt16, const int inDesc, SSL *, bool &ioAccept, void *&)
{
	__PTHREAD_DISABLE_CANCEL__

	if (CGEDQCtx::m_GEDQCtx->LockGEDQConnInTuns() != 0)
	{
		__PTHREAD_ENABLE_CANCEL__

		return;
	}

	CChunk outChunk; 
	outChunk << (SInt32)inDesc; 
	outChunk << ((*CGEDQCtx::m_GEDQCtx->m_TunSpec.Cut(CString(":"))[1]) + ":" + (*CGEDQCtx::m_GEDQCtx->m_TunSpec.Cut(CString(":"))[2])).Get();
	
	TBuffer <TGEDPktOut> outGEDPktOutBuf;

	outGEDPktOutBuf += ::NewGEDPktOut (inAddr, 
					   GED_PKT_TYP_OPEN_TUN, 
					   GED_PKT_REQ_NONE, 
					   outChunk.GetChunk(), 
					   outChunk.GetSize());

	if (CGEDQCtx::m_GEDQCtx->m_GEDQSocketClient->Lock() == 0)
	{
		CGEDQCtx::m_GEDQCtx->m_GEDAckCtx.ack  = false;
		CGEDQCtx::m_GEDQCtx->m_GEDAckCtx.hash = ::GetGEDRandomHash();

		if (::pthread_create (&CGEDQCtx::m_GEDQCtx->m_GEDAckCtx.tmrthd, NULL, CGEDQCtx::m_GEDQAckTimerCB, NULL) != 0)
		{
			CGEDQCtx::m_GEDQCtx->m_GEDQSocketClient->UnLock();
			CGEDQCtx::m_GEDQCtx->UnLockGEDQConnInTuns();

			__PTHREAD_ENABLE_CANCEL__ 

			throw new CException ("CGEDQServerListener::OnConnect could not create ack pthread");
		}

		::time (&CGEDQCtx::m_GEDQCtx->m_oltm);

		::SendHttpGEDPktToTgt (CGEDQCtx::m_GEDQCtx->m_GEDQSocketClient->GetDesc(), 
			CGEDQCtx::m_GEDQCtx->m_GEDQSocketClient->ClassIs(__metaclass(CSocketSSLClient)) ? 
			static_cast <CSocketSSLClient *> (CGEDQCtx::m_GEDQCtx->m_GEDQSocketClient) -> GetSSL() : NULL,
			&CGEDQCtx::m_GEDQCtx->m_GEDQHttpReqCtx, outGEDPktOutBuf, CGEDQCtx::m_GEDQCtx->m_GEDAckCtx.hash);

		CGEDQCtx::m_GEDQCtx->m_GEDQSocketClient->UnLock();
	}

	for (size_t i=outGEDPktOutBuf.GetLength(); i>0; i--) ::DeleteGEDPktOut (*outGEDPktOutBuf[i-1]);

	::LockGEDAckCtx (&CGEDQCtx::m_GEDQCtx->m_GEDAckCtx);

	if (!CGEDQCtx::m_GEDQCtx->m_GEDAckCtx.ack)
	{
		ioAccept = false;
	}
	else
	{
		CGEDQCtx::m_GEDQCtx->m_GEDQConnInTuns += inDesc;
	}

	CGEDQCtx::m_GEDQCtx->UnLockGEDQConnInTuns();

	__PTHREAD_ENABLE_CANCEL__
}

//---------------------------------------------------------------------------------------------------------------------------------------
// gedq tun interface data reception
//---------------------------------------------------------------------------------------------------------------------------------------
void CGEDQServerListener::OnReceive (CObject *, const CString &inAddr, const UInt16, const int inDesc, SSL *, const void *inData, const int inLen, void *&)
{
	UInt8 *outData = new UInt8 [sizeof(int)+inLen];
	::memcpy (outData, &inDesc, sizeof(int));
	::memcpy (outData+sizeof(int), inData, inLen);

	TBuffer <TGEDPktOut> outGEDPktOutBuf;

	outGEDPktOutBuf += ::NewGEDPktOut (inAddr, 
					   GED_PKT_TYP_DATA_TUN, 
					   GED_PKT_REQ_NONE, 
					   outData, 
					   sizeof(int)+inLen);

	if (CGEDQCtx::m_GEDQCtx->m_GEDQSocketClient->Lock() == 0)
	{
		::time (&CGEDQCtx::m_GEDQCtx->m_oltm);

		::SendHttpGEDPktToTgt (CGEDQCtx::m_GEDQCtx->m_GEDQSocketClient->GetDesc(), 
			CGEDQCtx::m_GEDQCtx->m_GEDQSocketClient->ClassIs(__metaclass(CSocketSSLClient)) ? 
			static_cast <CSocketSSLClient *> (CGEDQCtx::m_GEDQCtx->m_GEDQSocketClient) -> GetSSL() : NULL,
			&CGEDQCtx::m_GEDQCtx->m_GEDQHttpReqCtx, outGEDPktOutBuf, CString());
	
		CGEDQCtx::m_GEDQCtx->m_GEDQSocketClient->UnLock();
	}

	delete [] outData; for (size_t i=outGEDPktOutBuf.GetLength(); i>0; i--) ::DeleteGEDPktOut (*outGEDPktOutBuf[i-1]);
}

//---------------------------------------------------------------------------------------------------------------------------------------
// gedq tun interface disconnection
//---------------------------------------------------------------------------------------------------------------------------------------
void CGEDQServerListener::OnDisconnect (CObject *, const CString &inAddr, const UInt16, const int inDesc, SSL *, void *&)
{
	__PTHREAD_DISABLE_CANCEL__

	if (CGEDQCtx::m_GEDQCtx->LockGEDQConnInTuns() != 0)
	{
		__PTHREAD_ENABLE_CANCEL__

		return;
	}

	TBuffer <TGEDPktOut> outGEDPktOutBuf;

	outGEDPktOutBuf += ::NewGEDPktOut (inAddr, 
					   GED_PKT_TYP_SHUT_TUN, 
					   GED_PKT_REQ_NONE, 
					   const_cast <int *> (&inDesc), 
					   sizeof(int));

	if (CGEDQCtx::m_GEDQCtx->m_GEDQSocketClient->Lock() == 0)
	{
		::time (&CGEDQCtx::m_GEDQCtx->m_oltm);

		::SendHttpGEDPktToTgt (CGEDQCtx::m_GEDQCtx->m_GEDQSocketClient->GetDesc(), 
			CGEDQCtx::m_GEDQCtx->m_GEDQSocketClient->ClassIs(__metaclass(CSocketSSLClient)) ? 
			static_cast <CSocketSSLClient *> (CGEDQCtx::m_GEDQCtx->m_GEDQSocketClient) -> GetSSL() : NULL,
			&CGEDQCtx::m_GEDQCtx->m_GEDQHttpReqCtx, outGEDPktOutBuf, CString());

		CGEDQCtx::m_GEDQCtx->m_GEDQSocketClient->UnLock();
	}

	for (size_t i=outGEDPktOutBuf.GetLength(); i>0; i--) ::DeleteGEDPktOut (*outGEDPktOutBuf[i-1]);

	CGEDQCtx::m_GEDQCtx->m_GEDQConnInTuns -= inDesc;
	
	CGEDQCtx::m_GEDQCtx->UnLockGEDQConnInTuns();

	__PTHREAD_ENABLE_CANCEL__
}
#endif

//---------------------------------------------------------------------------------------------------------------------------------------
// gedq cli sigterm handler
//---------------------------------------------------------------------------------------------------------------------------------------
void CGEDQCli::m_SigTerm (int inSig)
{
	::signal (inSig, CGEDQCli::m_SigTerm);
}

//---------------------------------------------------------------------------------------------------------------------------------------
// gedq cli constructor
//---------------------------------------------------------------------------------------------------------------------------------------
CGEDQCli::CGEDQCli  	(const TGEDQCfg &inGEDQCfg)
	 :m_GEDQCfg 	(const_cast <TGEDQCfg &> (inGEDQCfg)),
	  m_Socket  	(NULL),
	  m_GEDPktInCtx (NULL)
{
	struct sigaction sa; sa.sa_handler = CGEDQCli::m_SigTerm; sa.sa_flags = SA_NOCLDSTOP;
	::sigaction (SIGINT,  &sa, NULL);
	::sigaction (SIGTERM, &sa, NULL);
	::sigaction (SIGQUIT, &sa, NULL);
}

//---------------------------------------------------------------------------------------------------------------------------------------
// gedq cli destructor
//---------------------------------------------------------------------------------------------------------------------------------------
CGEDQCli::~CGEDQCli ()
{
	Disconnect();
}

//---------------------------------------------------------------------------------------------------------------------------------------
// gedq cli connect
//---------------------------------------------------------------------------------------------------------------------------------------
bool CGEDQCli::Connect () THROWABLE
{
	if (m_Socket != NULL) return true;

	m_GEDQHttpReqCtx.cmd   = m_GEDQCfg.http.cmd;
	m_GEDQHttpReqCtx.vrs   = m_GEDQCfg.http.vrs;
	m_GEDQHttpReqCtx.agt   = m_GEDQCfg.http.agt;
	m_GEDQHttpReqCtx.typ   = m_GEDQCfg.http.typ;
	m_GEDQHttpReqCtx.zlv   = m_GEDQCfg.http.zlv;
	m_GEDQHttpReqCtx.host  = m_GEDQCfg.bind.addr;
	m_GEDQHttpReqCtx.kpalv = false;

	m_GEDPktInCtx = ::NewGEDPktInCtx();

	CString outBindAddr (m_GEDQCfg.bind.addr); if (m_GEDQCfg.proxy.addr != CString()) outBindAddr = m_GEDQCfg.proxy.addr;
	UInt32  outBindPort (m_GEDQCfg.bind.port); if (m_GEDQCfg.proxy.port != 0L	) outBindPort = m_GEDQCfg.proxy.port;
	
	try
	{
		if (m_GEDQCfg.bind.addr != CString() && m_GEDQCfg.bind.port != 0L)
		{
			if (m_GEDQCfg.tls.ca  != CString() || m_GEDQCfg.tls.crt != CString() || m_GEDQCfg.tls.key != CString())
				m_Socket = new CSocketSSLClient (
						outBindAddr,
						outBindPort,
						m_GEDQCfg.tls.ca,
						m_GEDQCfg.tls.crt,
						m_GEDQCfg.tls.key,
						m_GEDQCfg.tls.vfy,
					 	m_GEDQCfg.tls.cph,
						m_GEDQCfg.bind.bind,
						NULL);
			else
				m_Socket = new CSocketClient (outBindAddr, outBindPort, m_GEDQCfg.bind.bind, NULL);
		}
		else
			m_Socket = new CSocketClient (m_GEDQCfg.bind.sock, (const CSocketClientListener*)NULL);
	}
	catch (CException *e)
	{
		throw e;
	}

	TBuffer <TGEDPktOut> outGEDPktOutBuf;

	CChunk outChunk; outChunk << ::HashGEDPktCfg(m_GEDQCfg.pkts).Get(); 
	if (m_GEDQCfg.ackto > 0L)
		outChunk << (m_GEDQCfg.kpalv + m_GEDQCfg.ackto);
	else
		outChunk << 0L;

	outGEDPktOutBuf += ::NewGEDPktOut (m_Socket->GetAddr(), GED_PKT_TYP_MD5, GED_PKT_REQ_NONE, outChunk.GetChunk(), outChunk.GetSize());

	if (m_Socket->Lock() == 0)
	{
		CString ioAck (::GetGEDRandomHash());

		int res; if (m_Socket->GetSockAddr()->sa_family == AF_INET)
			res = ::SendHttpGEDPktToTgt (m_Socket->GetDesc(), 
					       m_Socket->ClassIs(__metaclass(CSocketSSLClient)) ? 
					       static_cast <CSocketSSLClient *> (m_Socket) -> GetSSL() : NULL,
					       &m_GEDQHttpReqCtx, outGEDPktOutBuf, ioAck);
		else
			res = ::SendRawGEDPkt (m_Socket->GetDesc(), outGEDPktOutBuf);

		if (res <= 0)
		{
			m_Socket->UnLock();
			return false;
		}

		TBuffer <TGEDPktIn *> inGEDPktIn; inGEDPktIn.SetInc (BUFFER_INC_2);

		bool ok=false; while (!ok)
		{
			if (m_Socket->GetSockAddr()->sa_family == AF_INET)
				res = ::RecvHttpGEDPktFromSkt (m_Socket->GetDesc(), 
						m_Socket->ClassIs(__metaclass(CSocketSSLClient)) ? 
						static_cast <CSocketSSLClient *> (m_Socket) -> GetSSL() : NULL, m_GEDPktInCtx, 
						m_GEDQCfg.ackto, CGEDQCli::m_RecvGEDPktCB, (void*)&inGEDPktIn);
			else
				res = ::RecvRawGEDPktFromSkt (m_Socket->GetDesc(), m_GEDPktInCtx, m_GEDQCfg.ackto, CGEDQCli::m_RecvGEDPktCB, (void*)&inGEDPktIn);

			if (res <= 0)
			{
				for (size_t i=0; i<inGEDPktIn.GetLength(); i++) ::DeleteGEDPktIn (*inGEDPktIn[i]);

				m_Socket->UnLock();

				return false;
			}

			for (size_t i=0; i<inGEDPktIn.GetLength(); i++)
			{
				switch ((*inGEDPktIn[i])->typ)
				{
					case GED_PKT_TYP_CLOSE :
					{
						if ((*inGEDPktIn[i])->data != NULL)
						{
							CString outEx ((char*)(*inGEDPktIn[i])->data);

							for (size_t j=0; j<inGEDPktIn.GetLength(); j++) ::DeleteGEDPktIn (*inGEDPktIn[j]);

							m_Socket->UnLock();

							throw new CException (outEx);
						}

						ok = true;
					}
					break;

					case GED_PKT_TYP_ACK :
					{
						if (CString((char*)(*inGEDPktIn[i])->data) != ioAck)
						{
							for (size_t j=0; j<inGEDPktIn.GetLength(); j++) ::DeleteGEDPktIn (*inGEDPktIn[j]);

							m_Socket->UnLock();

							return false;
						}

						ok = true;
					}
					break;
				}
			}

			for (size_t i=0; i<inGEDPktIn.GetLength(); i++) ::DeleteGEDPktIn (*inGEDPktIn[i]);
			inGEDPktIn.Reset();
		}

		m_Socket->UnLock();
	}

	return true;
}

//---------------------------------------------------------------------------------------------------------------------------------------
// push / update / drop request and ack wait
//---------------------------------------------------------------------------------------------------------------------------------------
bool CGEDQCli::PerformRequestWaitAck (TBuffer <TGEDPktOut> &outGEDPktOutBuf, TBuffer <TGEDRcd *> *outGEDRcds)
{
	if (m_Socket == NULL) return false;

	if (m_Socket->Lock() == 0)
	{
		CString ioAck (::GetGEDRandomHash());

		int res; if (m_Socket->GetSockAddr()->sa_family == AF_INET)
			res = ::SendHttpGEDPktToTgt (m_Socket->GetDesc(), 
					       m_Socket->ClassIs(__metaclass(CSocketSSLClient)) ? 
					       static_cast <CSocketSSLClient *> (m_Socket) -> GetSSL() : NULL,
					       &m_GEDQHttpReqCtx, outGEDPktOutBuf, ioAck);
		else
			res = ::SendRawGEDPkt (m_Socket->GetDesc(), outGEDPktOutBuf);

		if (res <= 0)
		{
			m_Socket->UnLock();
			return false;
		}

		TBuffer <TGEDPktIn *> inGEDPktIn; inGEDPktIn.SetInc (BUFFER_INC_2);

		bool ok=false; while (!ok)
		{
			if (m_Socket->GetSockAddr()->sa_family == AF_INET)
				res = ::RecvHttpGEDPktFromSkt (m_Socket->GetDesc(), 
						m_Socket->ClassIs(__metaclass(CSocketSSLClient)) ? 
						static_cast <CSocketSSLClient *> (m_Socket) -> GetSSL() : NULL, m_GEDPktInCtx, 
						m_GEDQCfg.ackto, CGEDQCli::m_RecvGEDPktCB, (void*)&inGEDPktIn);
			else
				res = ::RecvRawGEDPktFromSkt (m_Socket->GetDesc(), m_GEDPktInCtx, m_GEDQCfg.ackto, CGEDQCli::m_RecvGEDPktCB, (void*)&inGEDPktIn);

			if (res <= 0)
			{
				for (size_t i=0; i<inGEDPktIn.GetLength(); i++) ::DeleteGEDPktIn (*inGEDPktIn[i]);

				m_Socket->UnLock();

				return false;
			}

			for (size_t i=0; i<inGEDPktIn.GetLength(); i++)
			{
				switch ((*inGEDPktIn[i])->typ)
				{
					case GED_PKT_TYP_CLOSE :
					{
						if ((*inGEDPktIn[i])->data != NULL)
						{
							for (size_t j=0; j<inGEDPktIn.GetLength(); j++) ::DeleteGEDPktIn (*inGEDPktIn[j]);

							m_Socket->UnLock();

							return false;
						}

						ok = true;
					}
					break;

					case GED_PKT_TYP_ACK :
					{
						if (CString((char*)(*inGEDPktIn[i])->data) != ioAck)
						{
							for (size_t j=0; j<inGEDPktIn.GetLength(); j++) ::DeleteGEDPktIn (*inGEDPktIn[j]);

							m_Socket->UnLock();

							return false;
						}

						ok = true;
					}
					break;

					case GED_PKT_TYP_RECORD :
					{
						if (outGEDRcds) 
							*outGEDRcds += ::NewGEDRcd (*inGEDPktIn[i]);
					}
					break;
				}
			}

			for (size_t i=0; i<inGEDPktIn.GetLength(); i++) ::DeleteGEDPktIn (*inGEDPktIn[i]);
			inGEDPktIn.Reset();
		}

		m_Socket->UnLock();
	}

	return true;
}

//---------------------------------------------------------------------------------------------------------------------------------------
// gedq protected cli push
//---------------------------------------------------------------------------------------------------------------------------------------
bool CGEDQCli::Push (const long inReq, const long inType, const CChunk &inChunk, const bool inNoSync)
{
	if (m_Socket == NULL || inType <= 0L) return false;

	TBuffer <TGEDPktOut> outGEDPktOutBuf;

	outGEDPktOutBuf += ::NewGEDPktOut (m_Socket->GetAddr(), inType, inReq|(inNoSync?GED_PKT_REQ_NO_SYNC:0L), 
					   inChunk.GetChunk(), inChunk.GetSize());

	bool res = PerformRequestWaitAck (outGEDPktOutBuf);

	for (size_t i=outGEDPktOutBuf.GetLength(); i>0; i--) ::DeleteGEDPktOut (*outGEDPktOutBuf[i-1]);

	return res;
}

//---------------------------------------------------------------------------------------------------------------------------------------
// gedq cli push
//---------------------------------------------------------------------------------------------------------------------------------------
bool CGEDQCli::Push (const long inType, const CChunk &inChunk, const bool inNoSync)
{
	return Push (GED_PKT_REQ_PUSH|GED_PKT_REQ_PUSH_TMSP|GED_PKT_REQ_BKD_ACTIVE, inType, inChunk, inNoSync);
}

//---------------------------------------------------------------------------------------------------------------------------------------
// gedq cli update
//---------------------------------------------------------------------------------------------------------------------------------------
bool CGEDQCli::Update (const long inType, const CChunk &inChunk, const bool inNoSync)
{
	return Push (GED_PKT_REQ_PUSH|GED_PKT_REQ_PUSH_NOTMSP|GED_PKT_REQ_BKD_ACTIVE, inType, inChunk, inNoSync);
}

//---------------------------------------------------------------------------------------------------------------------------------------
// gedq cli drop by data
//---------------------------------------------------------------------------------------------------------------------------------------
bool CGEDQCli::Drop (const long inType, const long inQueue, const CChunk &inChunk, const bool inNoSync)
{
	if (m_Socket == NULL || inType <= 0L) return false;

	TBuffer <TGEDPktOut> outGEDPktOutBuf;

	outGEDPktOutBuf += ::NewGEDPktOut (m_Socket->GetAddr(), inType, 
					   GED_PKT_REQ_DROP|inQueue|GED_PKT_REQ_DROP_DATA|(inNoSync?GED_PKT_REQ_NO_SYNC:0L), 
					   inChunk.GetChunk(), inChunk.GetSize());

	bool res = PerformRequestWaitAck (outGEDPktOutBuf);

	for (size_t i=outGEDPktOutBuf.GetLength(); i>0; i--) ::DeleteGEDPktOut (*outGEDPktOutBuf[i-1]);

	return res;
}

//---------------------------------------------------------------------------------------------------------------------------------------
// gedq cli drop by id (history)
//---------------------------------------------------------------------------------------------------------------------------------------
bool CGEDQCli::Drop (const TBuffer <UInt32> &inIds, const long inQueue)
{
	if (m_Socket == NULL || inIds.GetLength() == 0) return false;

	TBuffer <TGEDPktOut> outGEDPktOutBuf;

	for (size_t i=inIds.GetLength(), j=0; i>0; i--, j++)
		outGEDPktOutBuf += ::NewGEDPktOut (m_Socket->GetAddr(), GED_PKT_TYP_ANY, 
						   GED_PKT_REQ_DROP|inQueue|GED_PKT_REQ_DROP_ID, 
						   inIds[j], sizeof(UInt32));

	bool res = PerformRequestWaitAck (outGEDPktOutBuf);

	for (size_t i=outGEDPktOutBuf.GetLength(); i>0; i--) ::DeleteGEDPktOut (*outGEDPktOutBuf[i-1]);

	return res;
}

//---------------------------------------------------------------------------------------------------------------------------------------
// gedq cli peek
//---------------------------------------------------------------------------------------------------------------------------------------
TBuffer <TGEDRcd *> * CGEDQCli::Peek (const long inType, const long inQueue, const CChunk &inChk, const long inSort, 
				      const UInt32 inTm1, const UInt32 inTm2, const UInt32 inOffset, const UInt32 inNumber)
{
	TBuffer <TGEDRcd *> *outGEDRcds = new TBuffer <TGEDRcd *> (); 
	outGEDRcds->SetInc (BUFFER_INC_512);

	if (m_Socket == NULL) return outGEDRcds;

	TBuffer <TGEDPktOut> outGEDPktOutBuf;

	TGEDPktOut inGEDPktOut = ::NewGEDPktOut (m_Socket->GetAddr(), inType, GED_PKT_REQ_PEEK|inSort|inQueue, inChk.GetChunk(), inChk.GetSize());
	TGEDPktIn *gedPktIn = (TGEDPktIn *)inGEDPktOut;

	if (inTm1 > 0L)
	{
		gedPktIn->req |= GED_PKT_REQ_PEEK_PARM_1_TM;
		gedPktIn->p1 = inTm1;
	}

	if (inOffset > 0L)
	{
		gedPktIn->req |= GED_PKT_REQ_PEEK_PARM_2_OFFSET;
		gedPktIn->p2 = inOffset;
	}

	if (inTm2 > 0L)
	{
		gedPktIn->req |= GED_PKT_REQ_PEEK_PARM_3_TM;
		gedPktIn->p3 = inTm2;
	}

	if (m_GEDQCfg.pmaxr > 0L)
	{
		gedPktIn->req |= GED_PKT_REQ_PEEK_PARM_4_NUMBER;
		gedPktIn->p4 = m_GEDQCfg.pmaxr;
	}

	if (inNumber > 0L)
	{
		gedPktIn->req |= GED_PKT_REQ_PEEK_PARM_4_NUMBER;
		gedPktIn->p4 = inNumber;
	}

	outGEDPktOutBuf += inGEDPktOut;

	PerformRequestWaitAck (outGEDPktOutBuf, outGEDRcds);

	for (size_t i=outGEDPktOutBuf.GetLength(); i>0; i--) ::DeleteGEDPktOut (*outGEDPktOutBuf[i-1]);

	return outGEDRcds;
}

//---------------------------------------------------------------------------------------------------------------------------------------
// free utility
//---------------------------------------------------------------------------------------------------------------------------------------
void CGEDQCli::Free (TBuffer <TGEDRcd *> *&ioGEDRcds)
{
	if (ioGEDRcds == NULL) return;
	
	for (size_t i=ioGEDRcds->GetLength(); i>0; i--) ::DeleteGEDRcd (*(*ioGEDRcds)[i-1]);

	delete ioGEDRcds;

	ioGEDRcds = NULL;
}

//---------------------------------------------------------------------------------------------------------------------------------------
// gedq cli disconnect
//---------------------------------------------------------------------------------------------------------------------------------------
void CGEDQCli::Disconnect ()
{
	if (m_Socket != NULL) delete m_Socket; m_Socket = NULL;
	if (m_GEDPktInCtx != NULL) ::DeleteGEDPktInCtx (m_GEDPktInCtx); m_GEDPktInCtx = NULL;
}

//---------------------------------------------------------------------------------------------------------------------------------------
// gedq cli reception callback
//---------------------------------------------------------------------------------------------------------------------------------------
void CGEDQCli::m_RecvGEDPktCB (const CString &, long, bool, TGEDPktIn *inGEDPktIn, void *inParam)
{
	*reinterpret_cast <TBuffer <TGEDPktIn *> *> (inParam) += ::NewGEDPktIn (inGEDPktIn);
}











