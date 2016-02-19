/****************************************************************************************************************************************
 gedcommon.c - ged common types and utility functions -
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

#include "gedcommon.h"

//---------------------------------------------------------------------------------------------------------------------------------------
// ack context instanciation commodity
//---------------------------------------------------------------------------------------------------------------------------------------
TGEDAckCtx::TGEDAckCtx () THROWABLE
	   :sem	       (-1),
	    tmrthd     (-1)
{
	if ((sem = ::semget (IPC_PRIVATE, 1, 0666)) < 0)
		throw new CException (CString("TGEDAckCtx::TGEDAckCtx could not allocate semaphore"));
	semun p; p.val = 0; ::semctl (sem, 0, SETVAL, p);
}

//---------------------------------------------------------------------------------------------------------------------------------------
// ack context deletion commodity
//---------------------------------------------------------------------------------------------------------------------------------------
TGEDAckCtx::~TGEDAckCtx ()
{
	if (sem != -1) ::semctl (sem, 0, IPC_RMID);
}

//---------------------------------------------------------------------------------------------------------------------------------------
// connection ack context lock
//---------------------------------------------------------------------------------------------------------------------------------------
int LockGEDAckCtx (TGEDAckCtx *inGEDAckCtx)
{
	struct sembuf op[1]; op[0].sem_num = 0; op[0].sem_op = -1; op[0].sem_flg = 0;
	return semop (inGEDAckCtx->sem, op, 1);
}

//---------------------------------------------------------------------------------------------------------------------------------------
// connection ack context unlock
//---------------------------------------------------------------------------------------------------------------------------------------
int UnLockGEDAckCtx (TGEDAckCtx *inGEDAckCtx)
{
	struct sembuf op[1]; op[0].sem_num = 0; op[0].sem_op = 1; op[0].sem_flg = 0;
	return semop (inGEDAckCtx->sem, op, 1);
}

#ifdef __GED__

GCRY_THREAD_OPTION_PTHREAD_IMPL;

//---------------------------------------------------------------------------------------------------------------------------------------
// ged init
//---------------------------------------------------------------------------------------------------------------------------------------
bool GEDInit (TGEDCfg &outGEDCfg)
{
	static const char* myZVersion = ZLIB_VERSION;

	if (::zlibVersion()[0] != myZVersion[0]) 
	{
		::fprintf (stderr, "incompatible zlib version\n");
		return false;
	}
	else if (::strcmp (zlibVersion(), ZLIB_VERSION) != 0)
	{
		::fprintf (stderr, "warning : different zlib version\n");
	}

	::SSL_library_init();
        ::SSL_load_error_strings();

	::gcry_control (GCRYCTL_SET_THREAD_CBS, &gcry_threads_pthread);

	if (!::gcry_check_version (GCRYPT_VERSION))
	{
		::fprintf (stderr, "incompatible zlib version\n");
		return false;
	}
     
	::gcry_control (GCRYCTL_INITIALIZATION_FINISHED, 0);
	
	outGEDCfg.uid	   = getuid();
	outGEDCfg.gid	   = getgid();
	outGEDCfg.logloc   = GED_SYSLOG_LOC_DFT;
	outGEDCfg.loghloc  = 0L;
	outGEDCfg.loglev   = GED_SYSLOG_LEV_DFT;
	outGEDCfg.ackto	   = GED_ACK_TIMEOUT_DFT;
	outGEDCfg.tls.vfy  = GED_TLS_VERIFY_PEER_DFT;
	outGEDCfg.pool	   = GED_PKT_POOL_DFT;
	outGEDCfg.maxin	   = GED_MAX_CONN_IN_DFT;
	#ifdef __GED_TUN__
	outGEDCfg.maytun   = GED_TUN_ENABLE_DFT;
	outGEDCfg.maxtun   = GED_MAX_TUN_OUT_DFT;
	#endif
	outGEDCfg.http.zlv = Z_DEFAULT_COMPRESSION;

	return true;
}
#endif

#ifdef __GEDQ__

GCRY_THREAD_OPTION_PTHREAD_IMPL;

//---------------------------------------------------------------------------------------------------------------------------------------
// gedq init
//---------------------------------------------------------------------------------------------------------------------------------------
bool GEDQInit (TGEDQCfg &outGEDQCfg)
{
	static const char* myZVersion = ZLIB_VERSION;

	if (::zlibVersion()[0] != myZVersion[0]) {
		::fprintf (stderr, "incompatible zlib version\n");
		return false;
	}
	else if (::strcmp (zlibVersion(), ZLIB_VERSION) != 0)
	{
		::fprintf (stderr, "warning : different zlib version\n");
	}

	::SSL_library_init();
        ::SSL_load_error_strings();

	::gcry_control (GCRYCTL_SET_THREAD_CBS, &gcry_threads_pthread);

	if (!::gcry_check_version (GCRYPT_VERSION))
	{
		::fprintf (stderr, "incompatible zlib version\n");
		return false;
	}
     
	::gcry_control (GCRYCTL_INITIALIZATION_FINISHED, 0);
	
	outGEDQCfg.bind.port  = 0L;
	outGEDQCfg.tls.vfy    = GED_TLS_VERIFY_PEER_DFT;
	outGEDQCfg.ackto      = GED_ACK_TIMEOUT_DFT;
	outGEDQCfg.proxy.addr = "";
	outGEDQCfg.proxy.port = 0L;
	outGEDQCfg.proxy.auth = GED_HTTP_PROXY_AUTH_NONE;
	outGEDQCfg.kpalv      = GED_KEEP_ALIVE_DFT;
	outGEDQCfg.http.zlv   = Z_DEFAULT_COMPRESSION;
	outGEDQCfg.pmaxr      = 0L;

	return true;
}
#endif

#ifdef __GED__
//---------------------------------------------------------------------------------------------------------------------------------------
// ged configuration loading
//---------------------------------------------------------------------------------------------------------------------------------------
bool LoadGEDCfg (const CString &inFile, TGEDCfg &outGEDCfg, int inDepth)
{
	if (CFile::Exists(inFile) != FILE_REGULAR)
	{
		::fprintf (stderr, "ERROR : the specified configuration file \"%s\" does not exist or is not a regular one !\n", 
			   inFile.Get());
		return false;
	}

	FILE *f = ::fopen (inFile.Get(), "rb"); if (f == NULL)
	{
		::fprintf (stderr, "ERROR : the specified configuration file \"%s\" could not be opened !\n", inFile.Get());
		return false;
	}

	int Line = 0; char LineContent[2048]; 
	while (::fgets (LineContent, 2048-1, f))
        {
                Line++;
		CString inLine (LineContent); CString inLineClean (inLine - " " - "\t");
		if (*inLineClean[0] == '#' || *inLineClean[0] == ';' || *inLineClean[0] == '\0' || *inLineClean[0] == '\n') continue;

		inLine.Substitute (CString("\t"), CString(" ")); inLine -= "\n";
		CStrings inArgs (inLine.Cut(CString(" "), true)); 
		if (inArgs[inArgs.GetLength()-1]->GetLength() == 0) inArgs.Delete (inArgs.GetLength()-1, 1);

		if (*inArgs[0] == GEDCfgInclude)
		{
			if (inArgs.GetLength() != 2)
			{
				::fprintf (stderr, "ERROR line %d : unspecified parameter value for \"%s\" in file \"%s\"\n", 
					   Line, inArgs[0]->Get(), inFile.Get());
				::fclose (f);
				return false;
			}
			if (!::LoadGEDCfg (*inArgs[1], outGEDCfg, inDepth+1)) 
			{
				::fprintf (stderr, "(from include directive line %d in file \"%s\")\n", Line, inFile.Get());
				::fclose (f);
				return false;
			}
		}
		else if (*inArgs[0] == GEDCfgSysLogLocal)
		{
			if (inArgs.GetLength() >= 2)
			{
				switch (inArgs[1]->ToLong())
				{
					case 0  : outGEDCfg.logloc = LOG_LOCAL0; break;
					case 1  : outGEDCfg.logloc = LOG_LOCAL1; break;
					case 2  : outGEDCfg.logloc = LOG_LOCAL2; break;
					case 3  : outGEDCfg.logloc = LOG_LOCAL3; break;
					case 4  : outGEDCfg.logloc = LOG_LOCAL4; break;
					case 5  : outGEDCfg.logloc = LOG_LOCAL5; break;
					case 6  : outGEDCfg.logloc = LOG_LOCAL6; break;
					case 7  : outGEDCfg.logloc = LOG_LOCAL7; break;
					default :
						::fprintf (stderr, 
							   "ERROR line %d : unhandled parameter value for \"%s\" in file \"%s\" (%d)\n", 
							   Line, inArgs[0]->Get(), inFile.Get(), inArgs[1]->ToLong());
						::fclose (f);
						return false;
				}
			}
			else
				outGEDCfg.logloc = GED_SYSLOG_LOC_DFT;
		}
		else if (*inArgs[0] == GEDCfgSysLogHistoryLocal)
		{
			if (inArgs.GetLength() >= 2)
			{
				switch (inArgs[1]->ToLong())
				{
					case 0  : outGEDCfg.loghloc = LOG_LOCAL0; break;
					case 1  : outGEDCfg.loghloc = LOG_LOCAL1; break;
					case 2  : outGEDCfg.loghloc = LOG_LOCAL2; break;
					case 3  : outGEDCfg.loghloc = LOG_LOCAL3; break;
					case 4  : outGEDCfg.loghloc = LOG_LOCAL4; break;
					case 5  : outGEDCfg.loghloc = LOG_LOCAL5; break;
					case 6  : outGEDCfg.loghloc = LOG_LOCAL6; break;
					case 7  : outGEDCfg.loghloc = LOG_LOCAL7; break;
					default :
						::fprintf (stderr, 
							   "ERROR line %d : unhandled parameter value for \"%s\" in file \"%s\" (%d)\n", 
							   Line, inArgs[0]->Get(), inFile.Get(), inArgs[1]->ToLong());
						::fclose (f);
						return false;
				}
			}
		}
		else if (*inArgs[0] == GEDCfgSysLogLevel)
		{
			if (inArgs.GetLength() >= 2)
			{
				outGEDCfg.loglev = inArgs[1]->ToLong();
				if (outGEDCfg.loglev < 0 || 
				    outGEDCfg.loglev > (GED_SYSLOG_LEV_CONNECTION    | GED_SYSLOG_LEV_DETAIL_CONN | 
							GED_SYSLOG_LEV_PKT_HEADER    | GED_SYSLOG_LEV_PKT_CONTENT | 
							GED_SYSLOG_LEV_BKD_OPERATION | GED_SYSLOG_LEV_HTTP_HEADER | 
							GED_SYSLOG_LEV_SSL 
							#if defined(__GED_DEBUG_SQL__) || defined(__GED_DEBUG_SEM__)
							| GED_SYSLOG_LEV_DEBUG
							#endif
							))
				{
					::fprintf (stderr, "ERROR line %d : unhandled parameter value for \"%s\" in file \"%s\" (%d)\n", 
						   Line, inArgs[0]->Get(), inFile.Get(), inArgs[1]->ToLong());
					::fclose (f);
					return false;
				}
			}
			else
				outGEDCfg.loglev = GED_SYSLOG_LEV_DFT;
		}
		else if (*inArgs[0] == GEDCfgUser)
		{
			if (inArgs.GetLength() <= 1) continue;
			struct passwd *pwd = NULL;
			if (::isdigit (*inArgs[1]->Get()))
				pwd = ::getpwuid (inArgs[1]->ToLong());
			else
				pwd = ::getpwnam (inArgs[1]->Get());
			if (pwd == NULL)
			{
				::fprintf (stderr, "ERROR line %d : user \"%s\" unknown for parameter \"%s\" in file \"%s\"\n", 
					   Line, inArgs[1]->Get(), inArgs[0]->Get(), inFile.Get());
				::fclose (f);
				::endpwent();
				return false;
			}
			outGEDCfg.uid = pwd->pw_uid;
			::endpwent();
		}
		else if (*inArgs[0] == GEDCfgGroup)
		{
			if (inArgs.GetLength() <= 1) continue;
			struct group *grp = NULL;
			if (::isdigit (*inArgs[1]->Get()))
				grp = ::getgrgid (inArgs[1]->ToLong());
			else
				grp = ::getgrnam (inArgs[1]->Get());
			if (grp == NULL)
			{
				::fprintf (stderr, "ERROR line %d : group \"%s\" unknown for parameter \"%s\" in file \"%s\"\n", 
					   Line, inArgs[1]->Get(), inArgs[0]->Get(), inFile.Get());
				::fclose (f);
				::endgrent();
				return false;
			}
			outGEDCfg.gid = grp->gr_gid;
			::endgrent();
		}
		else if (*inArgs[0] == GEDCfgListen)
		{
			if (inArgs.GetLength() != 2)
			{
				::fprintf (stderr, "ERROR line %d : unspecified parameter value for \"%s\" in file \"%s\"\n", 
					   Line, inArgs[0]->Get(), inFile.Get());
				::fclose (f);
				return false;
			}
			TGEDBindCfg outBind; outBind.port=0L;
			if (inArgs[1]->Find (CString(":")))
			{
				outBind.addr = *(inArgs[1]->Cut(CString(":"))[0]);
				outBind.port = inArgs[1]->Cut(CString(":"))[1]->ToULong();
				if (outBind.port == 0L)
				{
					::fprintf (stderr, "ERROR line %d : bad parameter value for \"%s\" in file \"%s\", missing port number\n", 
						   Line, inArgs[0]->Get(), inFile.Get());
					::fclose (f);
					return false;
				}
			}
			else if (inArgs[1]->Find (CString("/")))
			{
				outBind.sock = *inArgs[1];
			}
			else
			{
				::fprintf (stderr, "ERROR line %d : bad parameter value for \"%s\" in file \"%s\", missing port number or socket name\n", 
					   Line, inArgs[0]->Get(), inFile.Get());
				::fclose (f);
				return false;
			} 
			outGEDCfg.lst += outBind;
		}
		else if (*inArgs[0] == GEDCfgFifo)
		{
			if (inArgs.GetLength() != 4)
			{
				::fprintf (stderr, "ERROR line %d : wrong parameter value for \"%s\" in file \"%s\"\n", 
					   Line, inArgs[0]->Get(), inFile.Get());
				::fclose (f);
				return false;
			}
			UInt32 n=0L;
			outGEDCfg.fifo = *inArgs[1];
			sscanf (inArgs[2]->Get(), "%x", &n);
			outGEDCfg.fifofs = CString((char)n);
			sscanf (inArgs[3]->Get(), "%x", &n);
			outGEDCfg.fifors = CString((char)n);
		}
		else if (*inArgs[0] == GEDCfgPacketsPool)
		{
			if (inArgs.GetLength() <= 1) continue;
			outGEDCfg.pool = inArgs[1]->ToULong();
			if (outGEDCfg.pool == 0)
			{
				::fprintf (stderr, "ERROR line %d : unhandled parameter value for \"%s\" in file \"%s\" (%d)\n", 
					   Line, inArgs[0]->Get(), inFile.Get(), inArgs[1]->ToULong());
				::fclose (f);
				return false;
			}
		}
		else if (*inArgs[0] == GEDCfgTlsCaCert)
		{
			if (inArgs.GetLength() <= 1) continue;
			outGEDCfg.tls.ca = *inArgs[1];
		}
		else if (*inArgs[0] == GEDCfgTlsCert)
		{
			if (inArgs.GetLength() <= 1) continue;
			outGEDCfg.tls.crt = *inArgs[1];
		}
		else if (*inArgs[0] == GEDCfgTlsCertKey)
		{
			if (inArgs.GetLength() <= 1) continue;
			outGEDCfg.tls.key = *inArgs[1];
		}
		else if (*inArgs[0] == GEDCfgTlsVerifyPeer)
		{
			if (inArgs.GetLength() <= 1) continue;
			outGEDCfg.tls.vfy = inArgs[1]->ToBool();
		}
		else if (*inArgs[0] == GEDCfgTlsDHParam)
		{
			if (inArgs.GetLength() <= 1) continue;
			outGEDCfg.tls.dhp = *inArgs[1];
		}
		else if (*inArgs[0] == GEDCfgTlsCipherSuite)
		{
			if (inArgs.GetLength() <= 1) continue;
			outGEDCfg.tls.cph = *inArgs[1];
		}
		#ifdef __GED_TUN__
		else if (*inArgs[0] == GEDCfgEnableTun)
		{
			if (inArgs.GetLength() <= 1) continue;
			outGEDCfg.maytun = (int)inArgs[1]->ToBool();
		}
		#endif
		else if (*inArgs[0] == GEDCfgAckTimeOut)
		{
			if (inArgs.GetLength() <= 1) continue;
			outGEDCfg.ackto = inArgs[1]->ToULong();
			if (outGEDCfg.ackto == 0L)
			{
				::fprintf (stderr, "ERROR line %d : unhandled parameter value for \"%s\" in file \"%s\" (%d)\n", 
					   Line, inArgs[0]->Get(), inFile.Get(), inArgs[1]->ToULong());
				::fclose (f);
				return false;
			}
		}
		else if (*inArgs[0] == GEDCfgHttpVersion)
		{
			if (inArgs.GetLength() != 2)
			{
				::fprintf (stderr, "ERROR line %d : unspecified parameter value for \"%s\" in file \"%s\"\n", 
					   Line, inArgs[0]->Get(), inFile.Get());
				::fclose (f);
				return false;
			}
			outGEDCfg.http.vrs = *inArgs[1];
		}
		else if (*inArgs[0] == GEDCfgHttpServer)
		{
			if (inArgs.GetLength() <= 1)
			{
				::fprintf (stderr, "ERROR line %d : unspecified parameter value for \"%s\" in file \"%s\"\n", 
					   Line, inArgs[0]->Get(), inFile.Get());
				::fclose (f);
				return false;
			}
			outGEDCfg.http.srv = *inArgs[1];
			for (size_t i=2; i<inArgs.GetLength(); i++) outGEDCfg.http.srv += " " + *inArgs[i];
		}
		else if (*inArgs[0] == GEDCfgHttpContentType)
		{
			if (inArgs.GetLength() < 2)
			{
				::fprintf (stderr, "ERROR line %d : unspecified parameter value for \"%s\" in file \"%s\"\n", 
					   Line, inArgs[0]->Get(), inFile.Get());
				::fclose (f);
				return false;
			}
			outGEDCfg.http.typ = *inArgs[1];
			if (CString(outGEDCfg.http.typ).ToLower().Find(CString("zip")))
			{
				if (inArgs.GetLength() >= 2)
				{
					outGEDCfg.http.zlv = CString(*inArgs[inArgs.GetLength()-1]).ToLong();
					if (outGEDCfg.http.zlv < 1 || outGEDCfg.http.zlv > 9)
					{
						::fprintf (stderr, "ERROR line %d : unhandled parameter value for \"%s\" in file \"%s\"\n", 
						   Line, inArgs[0]->Get(), inFile.Get());
						::fclose (f);
						return false;
					}
				}
			}
		}
		else if (*inArgs[0] == GEDCfgMaxConnIn)
		{
			if (inArgs.GetLength() <= 1) continue;
			outGEDCfg.maxin = inArgs[1]->ToLong();
			if (outGEDCfg.maxin < 0)
			{
				::fprintf (stderr, "ERROR line %d : unhandled parameter value for \"%s\" in file \"%s\"\n", 
						   Line, inArgs[0]->Get(), inFile.Get());
				::fclose (f);
				return false;
			}
		}
		#ifdef __GED_TUN__
		else if (*inArgs[0] == GEDCfgMaxTunConnOut)
		{
			if (inArgs.GetLength() <= 1) continue;
			outGEDCfg.maxtun = inArgs[1]->ToLong();
			if (outGEDCfg.maxtun < 0)
			{
				::fprintf (stderr, "ERROR line %d : unhandled parameter value for \"%s\" in file \"%s\"\n", 
						   Line, inArgs[0]->Get(), inFile.Get());
				::fclose (f);
				return false;
			}
		}
		else if (*inArgs[0] == GEDCfgAllowTunTo)
		{
			if (inArgs.GetLength() <= 1) continue;
			outGEDCfg.alwtunto += *inArgs[1];
		}
		else if (*inArgs[0] == GEDCfgAllowTunFrom)
		{
			if (inArgs.GetLength() <= 1) continue;
			outGEDCfg.alwtunfrm += *inArgs[1];
		}
		#endif
		else if (*inArgs[0] == GEDCfgRelayToBeg)
		{
			TGEDRelayToCfg outGEDRelayToCfg;
			outGEDRelayToCfg.kpalv = 0L;
			outGEDRelayToCfg.syncs = 0L;
			outGEDRelayToCfg.syncb = 0L;
			outGEDRelayToCfg.pool  = GED_PKT_POOL_DFT;
			outGEDRelayToCfg.proxy.addr = "";
			outGEDRelayToCfg.proxy.port = 0L;
			outGEDRelayToCfg.proxy.auth = GED_HTTP_PROXY_AUTH_NONE;
			outGEDRelayToCfg.http.zlv = Z_DEFAULT_COMPRESSION;
			outGEDRelayToCfg.tls.vfy = false;
			bool ended=false; while (::fgets (LineContent, 2048-1, f))
        		{
                		Line++;
				CString inLine (LineContent); CString inLineClean (inLine - " " - "\t");
				if (*inLineClean[0] == '#' || *inLineClean[0] == ';' || 
				    *inLineClean[0] == '\0' || *inLineClean[0] == '\n') continue;

				inLine.Substitute (CString("\t"), CString(" ")); inLine -= "\n";
				CStrings inArgs (inLine.Cut(CString(" "), true)); 
				if (inArgs[inArgs.GetLength()-1]->GetLength() == 0) inArgs.Delete (inArgs.GetLength()-1, 1);

				if (*inArgs[0] == GEDCfgConnect)
				{
					if (inArgs.GetLength() != 2)
					{
					::fprintf (stderr, "ERROR line %d : unspecified parameter value for \"%s\" in file \"%s\"\n", 
						   Line, inArgs[0]->Get(), inFile.Get());
					::fclose (f);
					return false;
					}
					if (!inArgs[1]->Find (CString(":")))
					{
					::fprintf (stderr, 
						   "ERROR line %d : bad parameter value for \"%s\" in file \"%s\", missing port number\n", 
					   	   Line, inArgs[0]->Get(), inFile.Get());
					::fclose (f);
					return false;
					}
					outGEDRelayToCfg.bind.addr = *(inArgs[1]->Cut(CString(":"))[0]);
					outGEDRelayToCfg.bind.port = inArgs[1]->Cut(CString(":"))[1]->ToULong();
					if (outGEDRelayToCfg.bind.port == 0L)
					{
					::fprintf (stderr, 
						   "ERROR line %d : bad parameter value for \"%s\" in file \"%s\", missing port number\n", 
					   	   Line, inArgs[0]->Get(), inFile.Get());
					::fclose (f);
					return false;
					}
					if (outGEDRelayToCfg.bind.addr == CString()) outGEDRelayToCfg.bind.addr = "localhost";
				}
				else if (*inArgs[0] == GEDCfgBind)
				{
					if (inArgs.GetLength() > 1)
						outGEDRelayToCfg.bind.bind = *inArgs[1];
				}
				else if (*inArgs[0] == GEDCfgHttpProxy)
				{
					if (inArgs.GetLength() == 2)
					{
					if (!inArgs[1]->Find (CString(":")))
					{
					::fprintf (stderr, 
						   "ERROR line %d : bad parameter value for \"%s\" in file \"%s\", missing port number\n", 
					   	   Line, inArgs[0]->Get(), inFile.Get());
					::fclose (f);
					return false;
					}
					outGEDRelayToCfg.proxy.addr = *(inArgs[1]->Cut(CString(":"))[0]);
					outGEDRelayToCfg.proxy.port = inArgs[1]->Cut(CString(":"))[1]->ToULong();
					if (outGEDRelayToCfg.proxy.port == 0L)
					{
					::fprintf (stderr, 
						   "ERROR line %d : bad parameter value for \"%s\" in file \"%s\", missing port number\n", 
					   	   Line, inArgs[0]->Get(), inFile.Get());
					::fclose (f);
					return false;
					}
					}
				}
				else if (*inArgs[0] == GEDCfgHttpProxyAuth)
				{
					if (inArgs.GetLength() == 2)
					{
						if (*inArgs[1] == CString("none"))
						{
							outGEDRelayToCfg.proxy.auth = GED_HTTP_PROXY_AUTH_NONE;
						}
						else if (*inArgs[1] == CString("basic"))
						{
							outGEDRelayToCfg.proxy.auth = GED_HTTP_PROXY_AUTH_BASIC;
						}
						#ifdef __GED_NTLM__
						else if (*inArgs[1] == CString("ntlm"))
						{
							outGEDRelayToCfg.proxy.auth = GED_HTTP_PROXY_AUTH_NTLM;
						}
						#endif
						else
						{
							::fprintf (stderr, 
							   "ERROR line %d : bad parameter value for \"%s\" in file \"%s\"\n", Line, inArgs[0]->Get(), inFile.Get());
							::fclose (f);
							return false;
						}
					}
				}
				else if (*inArgs[0] == GEDCfgHttpProxyUser)
				{
					if (inArgs.GetLength() >= 2)
						for (size_t k=1; k<inArgs.GetLength(); k++) outGEDRelayToCfg.proxy.user += *inArgs[k];
				}
				else if (*inArgs[0] == GEDCfgHttpProxyPass)
				{
					if (inArgs.GetLength() >= 2)
						for (size_t k=1; k<inArgs.GetLength(); k++) outGEDRelayToCfg.proxy.pass += *inArgs[k];
				}
				else if (*inArgs[0] == GEDCfgAckTimeOut)
				{
					if (inArgs.GetLength() <= 1)
					{
						outGEDRelayToCfg.ackto = GED_ACK_TIMEOUT_DFT;
						continue;
					}
					outGEDRelayToCfg.ackto = inArgs[1]->ToULong();
					if (outGEDRelayToCfg.ackto == 0L)
					{
						::fprintf (stderr, 
							   "ERROR line %d : unhandled parameter value for \"%s\" in file \"%s\" (%d)\n", 
					   		   Line, inArgs[0]->Get(), inFile.Get(), inArgs[1]->ToULong());
						::fclose (f);
						return false;
					}
				}
				else if (*inArgs[0] == GEDCfgPacketsPool)
				{
					if (inArgs.GetLength() <= 1) continue;
					outGEDRelayToCfg.pool = inArgs[1]->ToULong();
					if (outGEDRelayToCfg.pool == 0)
					{
						::fprintf (stderr, 
							   "ERROR line %d : unhandled parameter value for \"%s\" in file \"%s\" (%d)\n", 
					   		   Line, inArgs[0]->Get(), inFile.Get(), inArgs[1]->ToULong());
						::fclose (f);
						return false;
					}
				}
				else if (*inArgs[0] == GEDCfgKeepAlive)
				{
					if (inArgs.GetLength() <= 1)
					{
						outGEDRelayToCfg.kpalv = GED_KEEP_ALIVE_DFT;
						continue;
					}
					outGEDRelayToCfg.kpalv = inArgs[1]->ToULong();
				}
				else if (*inArgs[0] == GEDCfgDataSync)
				{
					if (inArgs.GetLength() <= 2)
					{
						outGEDRelayToCfg.syncs = GED_DATA_SYNCS_DFT;
						outGEDRelayToCfg.syncb = GED_DATA_SYNCB_DFT;
						continue;
					}
					outGEDRelayToCfg.syncs = inArgs[1]->ToULong();
					outGEDRelayToCfg.syncb = inArgs[2]->ToULong();
					if (outGEDRelayToCfg.syncs == 0L && outGEDRelayToCfg.syncb == 0L)
					{
						::fprintf (stderr, 
							   "ERROR line %d : unhandled parameter values for \"%s\" in file \"%s\"\n", 
					   		   Line, inArgs[0]->Get(), inFile.Get());
						::fclose (f);
						return false;
					}
				}
				else if (*inArgs[0] == GEDCfgTlsCaCert)
				{
					if (inArgs.GetLength() <= 1) continue;
					outGEDRelayToCfg.tls.ca = *inArgs[1];
				}
				else if (*inArgs[0] == GEDCfgTlsCert)
				{
					if (inArgs.GetLength() <= 1) continue;
					outGEDRelayToCfg.tls.crt = *inArgs[1];
				}
				else if (*inArgs[0] == GEDCfgTlsCertKey)
				{
					if (inArgs.GetLength() <= 1) continue;
					outGEDRelayToCfg.tls.key = *inArgs[1];
				}
				else if (*inArgs[0] == GEDCfgTlsVerifyPeer)
				{
					if (inArgs.GetLength() <= 1)
					{
						outGEDRelayToCfg.tls.vfy = false;
						continue;
					}
					outGEDRelayToCfg.tls.vfy = inArgs[1]->ToBool();
				}
				else if (*inArgs[0] == GEDCfgTlsDHParam)
				{
					if (inArgs.GetLength() <= 1) continue;
					outGEDRelayToCfg.tls.dhp = *inArgs[1];
				}
				else if (*inArgs[0] == GEDCfgTlsCipherSuite)
				{
					if (inArgs.GetLength() <= 1) continue;
					outGEDRelayToCfg.tls.cph = *inArgs[1];
				}
				else if (*inArgs[0] == GEDCfgHttpCommand)
				{
					if (inArgs.GetLength() != 2)
					{
						::fprintf (stderr, 
							   "ERROR line %d : unspecified parameter value for \"%s\" in file \"%s\"\n", 
					   		   Line, inArgs[0]->Get(), inFile.Get());
						::fclose (f);
						return false;
					}
					outGEDRelayToCfg.http.cmd = *inArgs[1];
				}
				else if (*inArgs[0] == GEDCfgHttpVersion)
				{
					if (inArgs.GetLength() != 2)
					{
						::fprintf (stderr, 
							   "ERROR line %d : unspecified parameter value for \"%s\" in file \"%s\"\n", 
					   		   Line, inArgs[0]->Get(), inFile.Get());
						::fclose (f);
						return false;
					}
					outGEDRelayToCfg.http.vrs = *inArgs[1];
				}
				else if (*inArgs[0] == GEDCfgHttpUserAgent)
				{
					if (inArgs.GetLength() <= 1) continue;
					outGEDRelayToCfg.http.agt = *inArgs[1];
					for (size_t i=2; i<inArgs.GetLength(); i++) outGEDRelayToCfg.http.agt += " " + *inArgs[i];
				}
				else if (*inArgs[0] == GEDCfgHttpContentType)
				{
					if (inArgs.GetLength() < 2)
					{
						::fprintf (stderr, 
							   "ERROR line %d : unspecified parameter value for \"%s\" in file \"%s\"\n", 
					   		   Line, inArgs[0]->Get(), inFile.Get());
						::fclose (f);
						return false;
					}
					outGEDRelayToCfg.http.typ = *inArgs[1];
					if (CString(outGEDRelayToCfg.http.typ).ToLower().Find(CString("zip")))
					{
						if (inArgs.GetLength() >= 2)
						{
							outGEDRelayToCfg.http.zlv = CString(*inArgs[inArgs.GetLength()-1]).ToLong();
							if (outGEDRelayToCfg.http.zlv < 1 || outGEDRelayToCfg.http.zlv > 9)
							{
								::fprintf (stderr, "ERROR line %d : unhandled parameter value for \"%s\" in file \"%s\"\n", 
								   Line, inArgs[0]->Get(), inFile.Get());
								::fclose (f);
								return false;
							}
						}
					}
				}
				else if (*inArgs[0] == GEDCfgRelayToEnd)
				{
					ended = true; break;
				}
				else
				{
					::fprintf (stderr, "ERROR line %d : unknown parameter \"%s\" in file \"%s\"\n", 
						Line, inArgs[0]->Get(), inFile.Get());
					::fclose (f);
					return false;
				}
			}
			if (!ended)
			{
				::fprintf (stderr, "ERROR : unclosed target definition for \"%s:%d\" in file \"%s\" !\n", 
					outGEDRelayToCfg.bind.addr.Get(), outGEDRelayToCfg.bind.port, inFile.Get());
				::fclose (f);
				return false;
			}
			if (outGEDRelayToCfg.bind.addr == CString())
			{
				::fprintf (stderr, "ERROR : missing \"%s\" parameter for relay in file \"%s\" !\n", 
					   GEDCfgConnect.Get(), inFile.Get());
				::fclose (f);
				return false;
			}
			if (outGEDRelayToCfg.http.cmd == CString())
			{
				::fprintf (stderr, "ERROR : missing \"%s\" parameter for relay \"%s:%d\" in file \"%s\" !\n", 
					   GEDCfgHttpCommand.Get(), outGEDRelayToCfg.bind.addr.Get(), outGEDRelayToCfg.bind.port, 
					   inFile.Get());
				::fclose (f);
				return false;
			}
			if (outGEDRelayToCfg.http.vrs == CString())
			{
				::fprintf (stderr, "ERROR : missing \"%s\" parameter for relay \"%s:%d\" in file \"%s\" !\n", 
					   GEDCfgHttpVersion.Get(), outGEDRelayToCfg.bind.addr.Get(), outGEDRelayToCfg.bind.port, 
					   inFile.Get());
				::fclose (f);
				return false;
			}
			if (outGEDRelayToCfg.http.typ == CString())
			{
				::fprintf (stderr, "ERROR : missing \"%s\" parameter for relay \"%s:%d\" in file \"%s\" !\n", 
					   GEDCfgHttpContentType.Get(), outGEDRelayToCfg.bind.addr.Get(), outGEDRelayToCfg.bind.port, 
					   inFile.Get());
				::fclose (f);
				return false;
			}
			if (outGEDRelayToCfg.kpalv == 0L && outGEDRelayToCfg.syncs == 0L && outGEDRelayToCfg.syncb == 0L)
			{
				::fprintf (stderr, "ERROR : unknown connection type for relay \"%s:%d\" in file \"%s\", "
					   "missing \"%s\" or \"%s\" directive !\n", 
					   outGEDRelayToCfg.bind.addr.Get(), outGEDRelayToCfg.bind.port, inFile.Get(), 
					   GEDCfgKeepAlive.Get(), GEDCfgDataSync.Get());
				::fclose (f);
				return false;
			}
			if (outGEDRelayToCfg.proxy.auth != GED_HTTP_PROXY_AUTH_NONE && (outGEDRelayToCfg.proxy.user == CString() || outGEDRelayToCfg.proxy.pass == CString()))
			{
				::fprintf (stderr, "ERROR : proxy auth method specified in file \"%s\" without user or password", inFile.Get());
				::fclose (f);
				return false;
			}
			outGEDCfg.rlyto += outGEDRelayToCfg;
		}
		else if (*inArgs[0] == GEDCfgAllowSyncFrom)
		{
			if (inArgs.GetLength() <= 1) continue;
			outGEDCfg.alwsyncfrm += *inArgs[1];
		}
		else if (*inArgs[0] == GEDCfgAllowRequestFrom)
		{
			if (inArgs.GetLength() <= 1) continue;
			outGEDCfg.alwreqfrm += *inArgs[1];
		}
		else if (*inArgs[0] == GEDCfgBackendBeg)
		{
			if (outGEDCfg.bkd != CString())
			{
				::fprintf (stderr, "ERROR line %d : backend already defined as \"%s\" in file \"%s\"\n", 
					   Line, outGEDCfg.bkd.Get(), inFile.Get());
				::fclose (f);
				return false;
			}
			if (inArgs.GetLength() != 2)
			{
				::fprintf (stderr, "ERROR line %d : wrong backend specification syntax for \"%s\" in file \"%s\"\n", 
					   Line, inArgs[0]->Get(), inFile.Get());
				::fclose (f);
				return false;
			}
			if (!inArgs[1]->Find (CString("=")) || *inArgs[1]->Cut(CString("="))[0] != GEDCfgBackendMod ||
			    ((*inArgs[1]->Cut(CString("="))[1])-"\""-">"-"/") == CString())
			{
				::fprintf (stderr, 
					   "ERROR line %d : bad parameter name or value for \"%s\" in file \"%s\"\n", 
					   Line, inArgs[1]->Get(), inFile.Get());
				::fclose (f);
				return false;
			}
			outGEDCfg.bkd = (*inArgs[1]->Cut(CString("="))[1]) - "\"" - ">";
			if (*outGEDCfg.bkd[outGEDCfg.bkd.GetLength()-1] == '/')
			{
				outGEDCfg.bkd.Delete (outGEDCfg.bkd.GetLength()-1, 1);
				continue;
			}
			bool ended=false; while (::fgets (LineContent, 2048-1, f))
        		{
                		Line++;
				CString inLine (LineContent); CString inLineClean (inLine - " " - "\t");
				if (*inLineClean[0] == '#' || *inLineClean[0] == ';' || 
				    *inLineClean[0] == '\0' || *inLineClean[0] == '\n') continue;

				inLine.Substitute (CString("\t"), CString(" ")); inLine -= "\n";
				CStrings inArgs (inLine.Cut(CString(" "), true)); 
				if (inArgs[inArgs.GetLength()-1]->GetLength() == 0) inArgs.Delete (inArgs.GetLength()-1, 1);

				if ((*inArgs[0])-">" == GEDCfgBackendEnd)
				{
					ended=true;
					break;
				}
				else
				{
					if (outGEDCfg.bkdcfg.Contains (*inArgs[0]))
					{
						::fprintf (stderr, "ERROR : parameter already defined for \"%s\" in file \"%s\" !\n", 
							   inArgs[0]->Get(), inFile.Get());
						::fclose (f);
						return false;
					}
					outGEDCfg.bkdcfg[*inArgs[0]]; for (size_t i=1; i<inArgs.GetLength(); i++)
						outGEDCfg.bkdcfg[*inArgs[0]] += *inArgs[i] - "\"";
				}
			}
			if (!ended)
			{
				::fprintf (stderr, "ERROR : unclosed backend definition for \"%s\" in file \"%s\" !\n", 
					   outGEDCfg.bkd.Get(), inFile.Get());
				::fclose (f);
				return false;
			}
			for (size_t i=0; i<outGEDCfg.bkdcfg.GetKeys().GetLength(); i++)
			{
				CString inKey (*outGEDCfg.bkdcfg.GetKeys()[i]);
				if (outGEDCfg.bkdcfg[inKey].GetLength() > 0 && (*outGEDCfg.bkdcfg[inKey][0]).Get()[0] == '$')
				{
					CString inForeignKey (*outGEDCfg.bkdcfg[inKey][0]-"$");
					if (!outGEDCfg.bkdcfg.Contains(inForeignKey) || outGEDCfg.bkdcfg[inForeignKey].GetLength() == 0)
					{
						::fprintf (stderr, "ERROR : empty variable definition for \"%s\" -> \"%s\" (?) in file \"%s\" !\n", 
						   inKey.Get(), inForeignKey.Get(), inFile.Get());
						::fclose (f);
						return false;
					}
					outGEDCfg.bkdcfg[inKey] = outGEDCfg.bkdcfg[inForeignKey];
				}
			}
		}
		else if (*inArgs[0] == GEDCfgPacketBeg)
		{
			if (inArgs.GetLength() != 3)
			{
				::fprintf (stderr, "ERROR line %d : wrong packet specification syntax for \"%s\" in file \"%s\"\n", 
					   Line, inArgs[0]->Get(), inFile.Get());
				::fclose (f);
				return false;
			}
			if (!inArgs[1]->Find (CString("=")))
			{
				::fprintf (stderr, 
					   "ERROR line %d : bad parameter value for \"%s\" in file \"%s\", missing =\n", 
					   Line, inArgs[1]->Get(), inFile.Get());
				::fclose (f);
				return false;
			}
			if (!inArgs[2]->Find (CString("=")))
			{
				::fprintf (stderr, 
					   "ERROR line %d : bad parameter value for \"%s\" in file \"%s\", missing =\n", 
					   Line, inArgs[2]->Get(), inFile.Get());
				::fclose (f);
				return false;
			}
			TGEDPktCfg outGEDPktCfg; outGEDPktCfg.type = 0;
			for (size_t i=1; i<inArgs.GetLength(); i++)
			{
				if (*(*inArgs[i]).Cut(CString("="))[0] == GEDCfgPacketType)
				{
					outGEDPktCfg.type = ((*(*inArgs[i]).Cut(CString("="))[1])-"\""-"'"-">").ToLong();
					if (outGEDPktCfg.type <= 0)
					{
						::fprintf (stderr, 
						   "ERROR line %d : wrong packet type id \"%d\" in file \"%s\"\n", 
						   Line, outGEDPktCfg.type, inFile.Get());
						::fclose (f);
						return false;
					}
					for (size_t j=0; j<outGEDCfg.pkts.GetLength(); j++)
					{
						if (outGEDCfg.pkts[j]->type == outGEDPktCfg.type)
						{
							::fprintf (stderr, 
							   "ERROR line %d : packet type \"%d\" already defined in file \"%s\"\n", 
							   Line, outGEDPktCfg.type, inFile.Get());
							::fclose (f);
							return false;
						}
					}
				}
				else if (*(*inArgs[i]).Cut(CString("="))[0] == GEDCfgPacketName)
				{
					outGEDPktCfg.name = (*(*inArgs[i]).Cut(CString("="))[1])-"\""-"'"-">";
					for (size_t j=0; j<outGEDCfg.pkts.GetLength(); j++)
					{
						if (outGEDCfg.pkts[j]->name == outGEDPktCfg.name)
						{
							::fprintf (stderr, 
							   "ERROR line %d : packet name \"%s\" already defined in file \"%s\"\n", 
							   Line, outGEDPktCfg.name.Get(), inFile.Get());
							::fclose (f);
							return false;
						}
					}
				}
				else
				{
					::fprintf (stderr, "ERROR line %d : unknown parameter \"%s\" in file \"%s\"\n", 
						Line, (*inArgs[i]).Cut(CString("="))[0]->Get(), inFile.Get());
					::fclose (f);
					return false;
				}
			}
			if (outGEDPktCfg.type == 0)
			{
				::fprintf (stderr, "ERROR line %d : missing packet type in file \"%s\"\n", Line, inFile.Get());
				::fclose (f);
				return false;
			}
			if (outGEDPktCfg.name == CString())
			{
				::fprintf (stderr, "ERROR line %d : missing packet name in file \"%s\"\n", Line, inFile.Get());
				::fclose (f);
				return false;
			}
			bool ended=false; while (::fgets (LineContent, 2048-1, f))
        		{
                		Line++;
				CString inLine (LineContent); CString inLineClean (inLine - " " - "\t");
				if (*inLineClean[0] == '#' || *inLineClean[0] == ';' || 
				    *inLineClean[0] == '\0' || *inLineClean[0] == '\n') continue;

				inLine.Substitute (CString("\t"), CString(" ")); inLine -= "\n";
				CStrings inArgs (inLine.Cut(CString(" "), true)); 
				if (inArgs[inArgs.GetLength()-1]->GetLength() == 0) inArgs.Delete (inArgs.GetLength()-1, 1);

				TGEDPktFieldCfg outGEDPktFieldCfg;
				if (*inArgs[0] == GEDCfgPacketFieldString)
				{
					if (inArgs.GetLength() < 2)
					{
						::fprintf (stderr, 
						"ERROR line %d : wrong packet specification syntax in file \"%s\"\n", Line, inFile.Get());
						::fclose (f);
						return false;
					}
					outGEDPktFieldCfg.type = DATA_STRING;
					outGEDPktFieldCfg.name = *inArgs[1] - "\"";
					outGEDPktFieldCfg.meta = ((inArgs.GetLength()>2) && (inArgs[2]->ToUpper() == GEDCfgPacketFieldMeta)) ? 1 : 0;
					outGEDPktCfg.fields += outGEDPktFieldCfg;
				}
				else if (*inArgs[0] == GEDCfgPacketFieldSInt32)
				{
					if (inArgs.GetLength() < 2)
					{
						::fprintf (stderr, 
						"ERROR line %d : wrong packet specification syntax in file \"%s\"\n", Line, inFile.Get());
						::fclose (f);
						return false;
					}
					outGEDPktFieldCfg.type = DATA_SINT32;
					outGEDPktFieldCfg.name = *inArgs[1] - "\"";
					outGEDPktFieldCfg.meta = ((inArgs.GetLength()>2) && (inArgs[2]->ToUpper() == GEDCfgPacketFieldMeta)) ? 1 : 0;
					outGEDPktCfg.fields += outGEDPktFieldCfg;
				}
				else if (*inArgs[0] == GEDCfgPacketFieldFloat64)
				{
					if (inArgs.GetLength() < 2)
					{
						::fprintf (stderr, 
						"ERROR line %d : wrong packet specification syntax in file \"%s\"\n", Line, inFile.Get());
						::fclose (f);
						return false;
					}
					outGEDPktFieldCfg.type = DATA_FLOAT64;
					outGEDPktFieldCfg.name = *inArgs[1] - "\"";
					outGEDPktFieldCfg.meta = ((inArgs.GetLength()>2) && (inArgs[2]->ToUpper() == GEDCfgPacketFieldMeta)) ? 1 : 0;
					outGEDPktCfg.fields += outGEDPktFieldCfg;
				}
				else if ((*inArgs[0])-">" == GEDCfgPacketEnd)
				{
					inArgs = inLine.Cut(CString("="), true);

					if (inArgs.GetLength() > 2)
					{
						::fprintf (stderr, "ERROR line %d : bad syntax for \"counton\" directive in file \"%s\"\n", 
						Line, inFile.Get());
						::fclose (f);
					}
					if (inArgs.GetLength() == 2)
					{
						inArgs = ((*inArgs[1]) - "\"" - " " - "\t" - ">").Cut(CString(","));
						for (size_t i=0; i<inArgs.GetLength(); i++)
						{
							bool found=false; for (size_t j=0; j<outGEDPktCfg.fields.GetLength(); j++)
							{
								if (*inArgs[i] == outGEDPktCfg.fields[j]->name)
								{
									outGEDPktCfg.keyidc += j;
									found = true;
									break;
								}
							}
							if (!found)
							{
								::fprintf (stderr, 
								"ERROR line %d : unknown field name \"%s\" in file \"%s\"\n", 
								Line, inArgs[i]->Get(), inFile.Get());
								::fclose (f);
								return false;
							}
						}
					}
					ended=true;
					break;
				}
				else
				{
					::fprintf (stderr, "ERROR line %d : unknown parameter \"%s\" in file \"%s\"\n", 
						Line, inArgs[0]->Get(), inFile.Get());
					::fclose (f);
					return false;
				}
			}
			if (outGEDPktCfg.fields.GetLength() == 0)
			{
				::fprintf (stderr, "ERROR : missing packet core definition for \"%s\" [%d] in file \"%s\" !\n", 
					outGEDPktCfg.name.Get(), outGEDPktCfg.type, inFile.Get());
				::fclose (f);
				return false;
			}
			if (!ended)
			{
				::fprintf (stderr, "ERROR : unclosed packet definition for \"%s\" [%d] in file \"%s\" !\n", 
					outGEDPktCfg.name.Get(), outGEDPktCfg.type, inFile.Get());
				::fclose (f);
				return false;
			}
			outGEDCfg.pkts += outGEDPktCfg;
			outGEDCfg.pkts[outGEDCfg.pkts.GetLength()-1]->hash = HashGEDPktCfg (outGEDCfg.pkts[outGEDCfg.pkts.GetLength()-1]);
		}
		else if (*inArgs[0] == GEDCfgTTLActive)
		{
			if (inArgs.GetLength() > 1)
			{
				if (inArgs.GetLength() > 2)
				{
					::fprintf (stderr, "ERROR : parameter syntax error for \"%s\" in file \"%s\" !\n", 
						   inArgs[0]->Get(), inFile.Get());
					::fclose (f);
					return false;
				}
				if (outGEDCfg.bkdcfg.Contains (GEDCfgTTLActive))
				{
					::fprintf (stderr, "ERROR : parameter already defined for \"%s\" in file \"%s\" !\n", 
						   inArgs[0]->Get(), inFile.Get());
					::fclose (f);
					return false;
				}
				outGEDCfg.bkdcfg[GEDCfgTTLActive] += *inArgs[1];
			}
		}
		else if (*inArgs[0] == GEDCfgTTLSync)
		{
			if (inArgs.GetLength() > 1)
			{
				if (inArgs.GetLength() > 2)
				{
					::fprintf (stderr, "ERROR : parameter syntax error for \"%s\" in file \"%s\" !\n", 
						   inArgs[0]->Get(), inFile.Get());
					::fclose (f);
					return false;
				}
				if (outGEDCfg.bkdcfg.Contains (GEDCfgTTLSync))
				{
					::fprintf (stderr, "ERROR : parameter already defined for \"%s\" in file \"%s\" !\n", 
						   inArgs[0]->Get(), inFile.Get());
					::fclose (f);
					return false;
				}
				outGEDCfg.bkdcfg[GEDCfgTTLSync] += *inArgs[1];
			}
		}
		else if (*inArgs[0] == GEDCfgTTLHistory)
		{
			if (inArgs.GetLength() > 1)
			{
				if (inArgs.GetLength() > 2)
				{
					::fprintf (stderr, "ERROR : parameter syntax error for \"%s\" in file \"%s\" !\n", 
						   inArgs[0]->Get(), inFile.Get());
					::fclose (f);
					return false;
				}
				if (outGEDCfg.bkdcfg.Contains (GEDCfgTTLHistory))
				{
					::fprintf (stderr, "ERROR : parameter already defined for \"%s\" in file \"%s\" !\n", 
						   inArgs[0]->Get(), inFile.Get());
					::fclose (f);
					return false;
				}
				outGEDCfg.bkdcfg[GEDCfgTTLHistory] += *inArgs[1];
			}
		}
		else if (*inArgs[0] == GEDCfgTTLTimer)
		{
			if (inArgs.GetLength() > 1)
			{
				if (inArgs.GetLength() > 2)
				{
					::fprintf (stderr, "ERROR : parameter syntax error for \"%s\" in file \"%s\" !\n", 
						   inArgs[0]->Get(), inFile.Get());
					::fclose (f);
					return false;
				}
				if (outGEDCfg.bkdcfg.Contains (GEDCfgTTLTimer))
				{
					::fprintf (stderr, "ERROR : parameter already defined for \"%s\" in file \"%s\" !\n", 
						   inArgs[0]->Get(), inFile.Get());
					::fclose (f);
					return false;
				}
				outGEDCfg.bkdcfg[GEDCfgTTLTimer] += *inArgs[1];
			}
		}
		else
		{
			::fprintf (stderr, "ERROR line %d : unknown parameter \"%s\" in file \"%s\"\n", 
				   Line, inArgs[0]->Get(), inFile.Get());
			::fclose (f);
			return false;
		}
	}

	::fclose (f);

	if (inDepth == 0)
	{
		if (outGEDCfg.lst.GetLength() == 0)
		{
			::fprintf (stderr, "ERROR : missing \"%s\" parameter in file \"%s\" !\n", GEDCfgListen.Get(), inFile.Get());
			return false;
		}
		if (outGEDCfg.http.vrs == CString())
		{
			::fprintf (stderr, "ERROR : missing \"%s\" parameter in file \"%s\" !\n", GEDCfgHttpVersion.Get(), inFile.Get());
			return false;
		}
		if (outGEDCfg.http.srv == CString())
		{
			::fprintf (stderr, "ERROR : missing \"%s\" parameter in file \"%s\" !\n", GEDCfgHttpServer.Get(), inFile.Get());
			return false;
		}
		if (outGEDCfg.http.typ == CString())
		{
			::fprintf (stderr, "ERROR : missing \"%s\" parameter in file \"%s\" !\n", GEDCfgHttpContentType.Get(), 
				   inFile.Get());
			return false;
		}
		if (outGEDCfg.pkts.GetLength() == 0)
		{
			::fprintf (stderr, "ERROR : not a single packet definition found in file \"%s\" !\n", inFile.Get());
			return false;
		}
		if (outGEDCfg.bkd == CString())
		{
			::fprintf (stderr, "ERROR : no backend definition found in file \"%s\" !\n", inFile.Get());
			return false;
		}

		outGEDCfg.pkts.Sort();
	}

	return true;
}
#endif

#ifdef __GEDQ__
//---------------------------------------------------------------------------------------------------------------------------------------
// gedq configuration loading
//---------------------------------------------------------------------------------------------------------------------------------------
bool LoadGEDQCfg (const CString &inFile, TGEDQCfg &outGEDQCfg, int inDepth)
{
	if (CFile::Exists(inFile) != FILE_REGULAR)
	{
		::fprintf (stderr, "ERROR : the specified configuration file \"%s\" does not exist or is not a regular one !\n", 
			   inFile.Get());
		return false;
	}

	FILE *f = ::fopen (inFile.Get(), "rb"); if (f == NULL)
	{
		::fprintf (stderr, "ERROR : the specified configuration file \"%s\" could not be opened !\n", inFile.Get());
		return false;
	}

	int Line = 0; char LineContent[2048]; 
	while (::fgets (LineContent, 2048-1, f))
        {
                Line++;
		CString inLine (LineContent); CString inLineClean (inLine - " " - "\t");
		if (*inLineClean[0] == '#' || *inLineClean[0] == ';' || *inLineClean[0] == '\0' || *inLineClean[0] == '\n') continue;

		inLine.Substitute (CString("\t"), CString(" ")); 
		inLine -= "\n";

		CStrings inArgs (inLine.Cut(CString(" "), true)); 
		if (inArgs[inArgs.GetLength()-1]->GetLength() == 0) inArgs.Delete (inArgs.GetLength()-1, 1);

		if (*inArgs[0] == GEDCfgInclude)
		{
			if (inArgs.GetLength() != 2)
			{
				::fprintf (stderr, "ERROR line %d : unspecified parameter value for \"%s\" in file \"%s\"\n", 
					   Line, inArgs[0]->Get(), inFile.Get());
				::fclose (f);
				return false;
			}
			if (!::LoadGEDQCfg (*inArgs[1], outGEDQCfg, inDepth+1)) 
			{
				::fprintf (stderr, "(from include directive line %d in file \"%s\")\n", Line, inFile.Get());
				::fclose (f);
				return false;
			}
		}
		else if (*inArgs[0] == GEDCfgConnect)
		{
			if (inArgs.GetLength() != 2)
			{
				::fprintf (stderr, "ERROR line %d : unspecified parameter value for \"%s\" in file \"%s\"\n", 
					   Line, inArgs[0]->Get(), inFile.Get());
				::fclose (f);
				return false;
			}
			if (inArgs[1]->Find (CString(":")))
			{
				outGEDQCfg.bind.addr = *(inArgs[1]->Cut(CString(":"))[0]);
				outGEDQCfg.bind.port = inArgs[1]->Cut(CString(":"))[1]->ToULong();
				if (outGEDQCfg.bind.port == 0L)
				{
					::fprintf (stderr, "ERROR line %d : bad parameter value for \"%s\" in file \"%s\", missing port number\n", 
						   Line, inArgs[0]->Get(), inFile.Get());
					::fclose (f);
					return false;
				}
			}
			else if (inArgs[1]->Find (CString("/")))
			{
				outGEDQCfg.bind.sock = *inArgs[1];
			}
			else
			{
				::fprintf (stderr, "ERROR line %d : bad parameter value for \"%s\" in file \"%s\", missing port number or socket name\n", 
					   Line, inArgs[0]->Get(), inFile.Get());
				::fclose (f);
				return false;
			}
			if (outGEDQCfg.bind.addr == CString() && outGEDQCfg.bind.port != 0L) outGEDQCfg.bind.addr = "localhost";
		}
		else if (*inArgs[0] == GEDCfgBind)
		{
			if (inArgs.GetLength() > 1)
				outGEDQCfg.bind.bind = *inArgs[1];	
		}
		else if (*inArgs[0] == GEDCfgTlsCaCert)
		{
			if (inArgs.GetLength() <= 1) continue;
			outGEDQCfg.tls.ca = *inArgs[1];
		}
		else if (*inArgs[0] == GEDCfgTlsCert)
		{
			if (inArgs.GetLength() <= 1) continue;
			outGEDQCfg.tls.crt = *inArgs[1];
		}
		else if (*inArgs[0] == GEDCfgTlsVerifyPeer)
		{
			if (inArgs.GetLength() <= 1) continue;
			outGEDQCfg.tls.vfy = inArgs[1]->ToBool();
		}
		else if (*inArgs[0] == GEDCfgTlsCertKey)
		{
			if (inArgs.GetLength() <= 1) continue;
			outGEDQCfg.tls.key = *inArgs[1];
		}
		else if (*inArgs[0] == GEDCfgTlsCipherSuite)
		{
			if (inArgs.GetLength() <= 1) continue;
			outGEDQCfg.tls.cph = *inArgs[1];
		}
		else if (*inArgs[0] == GEDCfgAckTimeOut)
		{
			if (inArgs.GetLength() <= 1) continue;
			outGEDQCfg.ackto = inArgs[1]->ToULong();
			if (outGEDQCfg.ackto == 0L)
			{
				::fprintf (stderr, "ERROR line %d : unhandled parameter value for \"%s\" in file \"%s\" (%d)\n", 
					   Line, inArgs[0]->Get(), inFile.Get(), inArgs[1]->ToULong());
				::fclose (f);
				return false;
			}
		}
		else if (*inArgs[0] == GEDQCfgPeekMaxRecords)
		{
			if (inArgs.GetLength() <= 1) continue;
			outGEDQCfg.pmaxr = inArgs[1]->ToULong();
		}
		else if (*inArgs[0] == GEDCfgKeepAlive)
		{
			if (inArgs.GetLength() <= 1) continue;
			outGEDQCfg.kpalv = inArgs[1]->ToULong();
			if (outGEDQCfg.kpalv == 0L)
			{
				::fprintf (stderr, "ERROR line %d : unhandled parameter value for \"%s\" in file \"%s\" (%d)\n", 
					   Line, inArgs[0]->Get(), inFile.Get(), inArgs[1]->ToULong());
				::fclose (f);
				return false;
			}
		}
		else if (*inArgs[0] == GEDCfgHttpProxy)
		{
			if (inArgs.GetLength() == 2)
			{
			if (!inArgs[1]->Find (CString(":")))
			{
				::fprintf (stderr, "ERROR line %d : bad parameter value for \"%s\" in file \"%s\", missing port number\n", 
					   Line, inArgs[0]->Get(), inFile.Get());
				::fclose (f);
				return false;
			}
			outGEDQCfg.proxy.addr = *(inArgs[1]->Cut(CString(":"))[0]);
			outGEDQCfg.proxy.port = inArgs[1]->Cut(CString(":"))[1]->ToULong();
			if (outGEDQCfg.proxy.port == 0L)
			{
				::fprintf (stderr, "ERROR line %d : bad parameter value for \"%s\" in file \"%s\", missing port number\n", 
					   Line, inArgs[0]->Get(), inFile.Get());
				::fclose (f);
				return false;
			}
			}
		}
		else if (*inArgs[0] == GEDCfgHttpProxyAuth)
		{
			if (inArgs.GetLength() == 2)
			{
				if (*inArgs[1] == CString("none"))
				{
					outGEDQCfg.proxy.auth = GED_HTTP_PROXY_AUTH_NONE;
				}
				else if (*inArgs[1] == CString("basic"))
				{
					outGEDQCfg.proxy.auth = GED_HTTP_PROXY_AUTH_BASIC;
				}
				#ifdef __GED_NTLM__
				else if (*inArgs[1] == CString("ntlm"))
				{
					outGEDQCfg.proxy.auth = GED_HTTP_PROXY_AUTH_NTLM;
				}
				#endif
				else
				{
					::fprintf (stderr, 
					   "ERROR line %d : bad parameter value for \"%s\" in file \"%s\"\n", Line, inArgs[0]->Get(), inFile.Get());
					::fclose (f);
					return false;
				}
			}
		}
		else if (*inArgs[0] == GEDCfgHttpProxyUser)
		{
			if (inArgs.GetLength() >= 2)
				for (size_t k=1; k<inArgs.GetLength(); k++) outGEDQCfg.proxy.user += *inArgs[k];
		}
		else if (*inArgs[0] == GEDCfgHttpProxyPass)
		{
			if (inArgs.GetLength() >= 2)
				for (size_t k=1; k<inArgs.GetLength(); k++) outGEDQCfg.proxy.pass += *inArgs[k];
		}
		else if (*inArgs[0] == GEDCfgHttpCommand)
		{
			if (inArgs.GetLength() != 2)
			{
				::fprintf (stderr, "ERROR line %d : unspecified parameter value for \"%s\" in file \"%s\"\n", 
					   Line, inArgs[0]->Get(), inFile.Get());
				::fclose (f);
				return false;
			}
			outGEDQCfg.http.cmd = *inArgs[1];
		}
		else if (*inArgs[0] == GEDCfgHttpVersion)
		{
			if (inArgs.GetLength() != 2)
			{
				::fprintf (stderr, "ERROR line %d : unspecified parameter value for \"%s\" in file \"%s\"\n", 
					   Line, inArgs[0]->Get(), inFile.Get());
				::fclose (f);
				return false;
			}
			outGEDQCfg.http.vrs = *inArgs[1];
		}
		else if (*inArgs[0] == GEDCfgHttpUserAgent)
		{
			if (inArgs.GetLength() <= 1) continue;
			outGEDQCfg.http.agt = *inArgs[1];
			for (size_t i=2; i<inArgs.GetLength(); i++) outGEDQCfg.http.agt += " " + *inArgs[i];
		}
		else if (*inArgs[0] == GEDCfgHttpContentType)
		{
			if (inArgs.GetLength() < 2)
			{
				::fprintf (stderr, "ERROR line %d : unspecified parameter value for \"%s\" in file \"%s\"\n", 
					   Line, inArgs[0]->Get(), inFile.Get());
				::fclose (f);
				return false;
			}
			outGEDQCfg.http.typ = *inArgs[1];
			if (CString(outGEDQCfg.http.typ).ToLower().Find(CString("zip")))
			{
				if (inArgs.GetLength() >= 2)
				{
					outGEDQCfg.http.zlv = CString(*inArgs[inArgs.GetLength()-1]).ToLong();
					if (outGEDQCfg.http.zlv < 1 || outGEDQCfg.http.zlv > 9)
					{
						::fprintf (stderr, "ERROR line %d : unhandled parameter value for \"%s\" in file \"%s\"\n", 
						   Line, inArgs[0]->Get(), inFile.Get());
						::fclose (f);
						return false;
					}
				}
			}
		}
		else if (*inArgs[0] == GEDCfgPacketBeg)
		{
			if (inArgs.GetLength() != 3)
			{
				::fprintf (stderr, "ERROR line %d : wrong packet specification syntax for \"%s\" in file \"%s\"\n", 
					   Line, inArgs[0]->Get(), inFile.Get());
				::fclose (f);
				return false;
			}
			if (!inArgs[1]->Find (CString("=")))
			{
				::fprintf (stderr, 
					   "ERROR line %d : bad parameter value for \"%s\" in file \"%s\", missing =\n", 
					   Line, inArgs[1]->Get(), inFile.Get());
				::fclose (f);
				return false;
			}
			if (!inArgs[2]->Find (CString("=")))
			{
				::fprintf (stderr, 
					   "ERROR line %d : bad parameter value for \"%s\" in file \"%s\", missing =\n", 
					   Line, inArgs[2]->Get(), inFile.Get());
				::fclose (f);
				return false;
			}
			TGEDPktCfg outGEDPktCfg; outGEDPktCfg.type = 0;
			for (size_t i=1; i<inArgs.GetLength(); i++)
			{
				if (*(*inArgs[i]).Cut(CString("="))[0] == GEDCfgPacketType)
				{
					outGEDPktCfg.type = ((*(*inArgs[i]).Cut(CString("="))[1])-"\""-"'"-">").ToLong();
					if (outGEDPktCfg.type <= 0)
					{
						::fprintf (stderr, 
						   "ERROR line %d : wrong packet type id \"%d\" in file \"%s\"\n", 
						   Line, outGEDPktCfg.type, inFile.Get());
						::fclose (f);
						return false;
					}
					for (size_t j=0; j<outGEDQCfg.pkts.GetLength(); j++)
					{
						if (outGEDQCfg.pkts[j]->type == outGEDPktCfg.type)
						{
							::fprintf (stderr, 
							   "ERROR line %d : packet type \"%d\" already defined in file \"%s\"\n", 
							   Line, outGEDPktCfg.type, inFile.Get());
							::fclose (f);
							return false;
						}
					}
				}
				else if (*(*inArgs[i]).Cut(CString("="))[0] == GEDCfgPacketName)
				{
					outGEDPktCfg.name = (*(*inArgs[i]).Cut(CString("="))[1])-"\""-"'"-">";
					for (size_t j=0; j<outGEDQCfg.pkts.GetLength(); j++)
					{
						if (outGEDQCfg.pkts[j]->name == outGEDPktCfg.name)
						{
							::fprintf (stderr, 
							   "ERROR line %d : packet name \"%s\" already defined in file \"%s\"\n", 
							   Line, outGEDPktCfg.name.Get(), inFile.Get());
							::fclose (f);
							return false;
						}
					}
				}
				else
				{
					::fprintf (stderr, "ERROR line %d : unknown parameter \"%s\" in file \"%s\"\n", 
						Line, (*inArgs[i]).Cut(CString("="))[0]->Get(), inFile.Get());
					::fclose (f);
					return false;
				}
			}
			if (outGEDPktCfg.type == 0)
			{
				::fprintf (stderr, "ERROR line %d : missing packet type in file \"%s\"\n", Line, inFile.Get());
				::fclose (f);
				return false;
			}
			if (outGEDPktCfg.name == CString())
			{
				::fprintf (stderr, "ERROR line %d : missing packet name in file \"%s\"\n", Line, inFile.Get());
				::fclose (f);
				return false;
			}
			bool ended=false; while (::fgets (LineContent, 2048-1, f))
        		{
                		Line++;
				CString inLine (LineContent); CString inLineClean (inLine - " " - "\t");
				if (*inLineClean[0] == '#' || *inLineClean[0] == ';' || 
				    *inLineClean[0] == '\0' || *inLineClean[0] == '\n') continue;

				inLine.Substitute (CString("\t"), CString(" ")); inLine -= "\n";
				CStrings inArgs (inLine.Cut(CString(" "), true)); 
				if (inArgs[inArgs.GetLength()-1]->GetLength() == 0) inArgs.Delete (inArgs.GetLength()-1, 1);

				TGEDPktFieldCfg outGEDPktFieldCfg;
				if (*inArgs[0] == GEDCfgPacketFieldString)
				{
					if (inArgs.GetLength() < 2)
					{
						::fprintf (stderr, 
						"ERROR line %d : wrong packet specification syntax in file \"%s\"\n", Line, inFile.Get());
						::fclose (f);
						return false;
					}
					outGEDPktFieldCfg.type = DATA_STRING;
					outGEDPktFieldCfg.name = *inArgs[1] - "\"";
					outGEDPktFieldCfg.meta = ((inArgs.GetLength()>2) && (inArgs[2]->ToUpper() == GEDCfgPacketFieldMeta)) ? 1 : 0;
					outGEDPktCfg.fields += outGEDPktFieldCfg;
				}
				else if (*inArgs[0] == GEDCfgPacketFieldSInt32)
				{
					if (inArgs.GetLength() < 2)
					{
						::fprintf (stderr, 
						"ERROR line %d : wrong packet specification syntax in file \"%s\"\n", Line, inFile.Get());
						::fclose (f);
						return false;
					}
					outGEDPktFieldCfg.type = DATA_SINT32;
					outGEDPktFieldCfg.name = *inArgs[1] - "\"";
					outGEDPktFieldCfg.meta = ((inArgs.GetLength()>2) && (inArgs[2]->ToUpper() == GEDCfgPacketFieldMeta)) ? 1 : 0;
					outGEDPktCfg.fields += outGEDPktFieldCfg;
				}
				else if (*inArgs[0] == GEDCfgPacketFieldFloat64)
				{
					if (inArgs.GetLength() < 2)
					{
						::fprintf (stderr, 
						"ERROR line %d : wrong packet specification syntax in file \"%s\"\n", Line, inFile.Get());
						::fclose (f);
						return false;
					}
					outGEDPktFieldCfg.type = DATA_FLOAT64;
					outGEDPktFieldCfg.name = *inArgs[1] - "\"";
					outGEDPktFieldCfg.meta = ((inArgs.GetLength()>2) && (inArgs[2]->ToUpper() == GEDCfgPacketFieldMeta)) ? 1 : 0;
					outGEDPktCfg.fields += outGEDPktFieldCfg;
				}
				else if ((*inArgs[0])-">" == GEDCfgPacketEnd)
				{
					inArgs = inLine.Cut(CString("="), true);

					if (inArgs.GetLength() > 2)
					{
						::fprintf (stderr, "ERROR line %d : bad syntax for \"counton\" directive in file \"%s\"\n", 
						Line, inFile.Get());
						::fclose (f);
					}
					if (inArgs.GetLength() == 2)
					{
						inArgs = ((*inArgs[1]) - "\"" - " " - "\t" - ">").Cut(CString(","));
						for (size_t i=0; i<inArgs.GetLength(); i++)
						{
							bool found=false; for (size_t j=0; j<outGEDPktCfg.fields.GetLength(); j++)
							{
								if (*inArgs[i] == outGEDPktCfg.fields[j]->name)
								{
									outGEDPktCfg.keyidc += j;
									found = true;
									break;
								}
							}
							if (!found)
							{
								::fprintf (stderr, 
								"ERROR line %d : unknown field name \"%s\" in file \"%s\"\n", 
								Line, inArgs[i]->Get(), inFile.Get());
								::fclose (f);
								return false;
							}
						}
					}
					ended=true;
					break;
				}
				else
				{
					::fprintf (stderr, "ERROR line %d : unknown parameter \"%s\" in file \"%s\"\n", 
						Line, inArgs[0]->Get(), inFile.Get());
					::fclose (f);
					return false;
				}
			}
			if (outGEDPktCfg.fields.GetLength() == 0)
			{
				::fprintf (stderr, "ERROR : missing packet core definition for \"%s\" [%d] in file \"%s\" !\n", 
					outGEDPktCfg.name.Get(), outGEDPktCfg.type, inFile.Get());
				::fclose (f);
				return false;
			}
			if (!ended)
			{
				::fprintf (stderr, "ERROR : unclosed packet definition for \"%s\" [%d] in file \"%s\" !\n", 
					outGEDPktCfg.name.Get(), outGEDPktCfg.type, inFile.Get());
				::fclose (f);
				return false;
			}
			outGEDQCfg.pkts += outGEDPktCfg;
			outGEDQCfg.pkts[outGEDQCfg.pkts.GetLength()-1]->hash = HashGEDPktCfg (outGEDQCfg.pkts[outGEDQCfg.pkts.GetLength()-1]);
		}
		else
		{
			::fprintf (stderr, "ERROR line %d : unknown parameter \"%s\" in file \"%s\"\n", 
				   Line, inArgs[0]->Get(), inFile.Get());
			::fclose (f);
			return false;
		}
	}

	::fclose (f);

	if (inDepth == 0)
	{
		if (outGEDQCfg.bind.addr == CString() && outGEDQCfg.bind.sock == CString() && outGEDQCfg.bind.port == 0L)
		{
			::fprintf (stderr, "ERROR : missing \"%s\" parameter in file \"%s\" !\n", GEDCfgConnect.Get(), inFile.Get());
			return false;
		}
		if (outGEDQCfg.http.vrs == CString())
		{
			::fprintf (stderr, "ERROR : missing \"%s\" parameter in file \"%s\" !\n", GEDCfgHttpVersion.Get(), inFile.Get());
			return false;
		}
		if (outGEDQCfg.http.cmd == CString())
		{
			::fprintf (stderr, "ERROR : missing \"%s\" parameter in file \"%s\" !\n", GEDCfgHttpCommand.Get(), inFile.Get());
			return false;
		}
		if (outGEDQCfg.http.typ == CString())
		{
			::fprintf (stderr, "ERROR : missing \"%s\" parameter in file \"%s\" !\n", GEDCfgHttpContentType.Get(), 
				   inFile.Get());
			return false;
		}
		if (outGEDQCfg.pkts.GetLength() == 0)
		{
			::fprintf (stderr, "ERROR : not a single packet definition found in file \"%s\" !\n", inFile.Get());
			return false;
		}
		if (outGEDQCfg.proxy.auth != GED_HTTP_PROXY_AUTH_NONE && (outGEDQCfg.proxy.user == CString() || outGEDQCfg.proxy.pass == CString()))
		{
			::fprintf (stderr, "ERROR : proxy auth method specified in file \"%s\" without user or password provided !\n", inFile.Get());
			return false;
		}
	
		outGEDQCfg.pkts.Sort();
	}

	return true;
}
#endif

//---------------------------------------------------------------------------------------------------------------------------------------
// str addr to addr in
//---------------------------------------------------------------------------------------------------------------------------------------
bool GetStrAddrToAddrIn (const CString &inAddr, struct in_addr *outAddr)
{
	if (inAddr == CString())
	{
		::inet_aton ("0.0.0.0", outAddr);
		return true;
	}

	if (!::inet_aton (inAddr.Get(), outAddr))
        {
                struct hostent *hp = ::gethostbyname (inAddr.Get());
                if (hp == NULL) return false;
                ::memcpy (outAddr, hp->h_addr, hp->h_length);
        }

	return true;
}

//---------------------------------------------------------------------------------------------------------------------------------------
// new contigous packet
//---------------------------------------------------------------------------------------------------------------------------------------
TGEDPktOut NewGEDPktOut (TGEDPktIn *inGEDPktIn)
{
	TGEDPktOut outGEDPktOut = reinterpret_cast <TGEDPktOut> (new char [GED_PKT_FIX_SIZE + inGEDPktIn->len]);
	::bzero (outGEDPktOut, GED_PKT_FIX_SIZE + inGEDPktIn->len);
	::memcpy (outGEDPktOut, inGEDPktIn, GED_PKT_FIX_SIZE);
	if (inGEDPktIn->len > 0)
		::memcpy (reinterpret_cast <char *> (outGEDPktOut) + GED_PKT_FIX_SIZE, inGEDPktIn->data, inGEDPktIn->len);
	#ifdef __GED__
	reinterpret_cast <TGEDPktIn *> (outGEDPktOut) -> req |= GED_PKT_REQ_SRC_RELAY;
	#endif
	return outGEDPktOut;
}

//---------------------------------------------------------------------------------------------------------------------------------------
// new contigous packet
//---------------------------------------------------------------------------------------------------------------------------------------
TGEDPktOut NewGEDPktOut (CString inAddr, long inType, long inRequest, void *inData, long inLength)
{
	char *outGEDPktOut = new char [GED_PKT_FIX_SIZE + inLength];
	::bzero (outGEDPktOut, GED_PKT_FIX_SIZE + inLength);
	TGEDPktIn *outGEDPktIn = reinterpret_cast <TGEDPktIn *> (outGEDPktOut);
	outGEDPktIn->vrs = GED_VERSION;
	::gettimeofday (&outGEDPktIn->tv, NULL);
	if (inAddr != CString()) 
		GetStrAddrToAddrIn (inAddr, &outGEDPktIn->addr);
	else 
		::inet_aton ("0.0.0.0", &(outGEDPktIn->addr));
	outGEDPktIn->typ = inType;
	outGEDPktIn->req = inRequest;
	#ifdef __GED__
	outGEDPktIn->req |= GED_PKT_REQ_SRC_RELAY;
	#endif
	outGEDPktIn->len = inLength;
	if (inLength > 0) ::memcpy (outGEDPktOut + GED_PKT_FIX_SIZE, inData, inLength);
	return outGEDPktOut;
}

//---------------------------------------------------------------------------------------------------------------------------------------
// new contigous packet
//---------------------------------------------------------------------------------------------------------------------------------------
TGEDPktOut NewGEDPktOut (struct in_addr inAddr, long inType, long inRequest, void *inData, long inLength)
{
	char *outGEDPktOut = new char [GED_PKT_FIX_SIZE + inLength];
	::bzero (outGEDPktOut, GED_PKT_FIX_SIZE + inLength);
	TGEDPktIn *outGEDPktIn = reinterpret_cast <TGEDPktIn *> (outGEDPktOut);
	outGEDPktIn->vrs = GED_VERSION;
	::gettimeofday (&outGEDPktIn->tv, NULL);
	outGEDPktIn->addr.s_addr = inAddr.s_addr;
	outGEDPktIn->typ = inType;
	outGEDPktIn->req = inRequest;
	#ifdef __GED__
	outGEDPktIn->req |= GED_PKT_REQ_SRC_RELAY;
	#endif
	outGEDPktIn->len = inLength;
	if (inLength > 0) ::memcpy (outGEDPktOut + GED_PKT_FIX_SIZE, inData, inLength);
	return outGEDPktOut;
}

#ifdef __GED__
//---------------------------------------------------------------------------------------------------------------------------------------
// new contigous packet
//---------------------------------------------------------------------------------------------------------------------------------------
TGEDPktOut NewGEDPktOut (TGEDRcd *inGEDRcd)
{
	char *outGEDPktOut = NULL;

	switch (inGEDRcd->queue & GED_PKT_REQ_BKD_MASK)
	{
		case GED_PKT_REQ_BKD_STAT :
		{
			TGEDStatRcd *inGEDStatRcd (reinterpret_cast <TGEDStatRcd *> (inGEDRcd));

			outGEDPktOut = new char [GED_PKT_FIX_SIZE + GED_STATRCD_FIX_SIZE];

			TGEDPktIn *GEDPktIn (reinterpret_cast <TGEDPktIn *> (outGEDPktOut));

                        ::bzero (outGEDPktOut, GED_PKT_FIX_SIZE + GED_STATRCD_FIX_SIZE);

			GEDPktIn->vrs           = GED_VERSION;
                        GEDPktIn->typ           = GED_PKT_TYP_RECORD;
                        GEDPktIn->req           = GED_PKT_REQ_NONE|GED_PKT_REQ_SRC_RELAY;
                        GEDPktIn->len           = GED_STATRCD_FIX_SIZE;		

			::memcpy (outGEDPktOut+GED_PKT_FIX_SIZE, inGEDStatRcd, GED_STATRCD_FIX_SIZE);
		}
		break;

		case GED_PKT_REQ_BKD_ACTIVE :
		{
			TGEDARcd *inGEDARcd (reinterpret_cast <TGEDARcd *> (inGEDRcd));
			
			outGEDPktOut = new char [GED_PKT_FIX_SIZE + GED_ARCD_FIX_SIZE + inGEDARcd->nsrc * sizeof(TGEDRcdSrc) + 
						 inGEDARcd->len];

			TGEDPktIn *GEDPktIn (reinterpret_cast <TGEDPktIn *> (outGEDPktOut));

			::bzero (outGEDPktOut, GED_PKT_FIX_SIZE + GED_ARCD_FIX_SIZE + inGEDARcd->nsrc * sizeof(TGEDRcdSrc) + 
				 inGEDARcd->len);

			GEDPktIn->vrs 		= GED_VERSION;
			GEDPktIn->tv   		= inGEDARcd->otv;
			GEDPktIn->addr.s_addr 	= 0;
			GEDPktIn->typ  		= GED_PKT_TYP_RECORD;
			GEDPktIn->req  		= GED_PKT_REQ_NONE|GED_PKT_REQ_SRC_RELAY;
			GEDPktIn->len  		= GED_ARCD_FIX_SIZE + inGEDARcd->nsrc * sizeof(TGEDRcdSrc) + inGEDARcd->len;

			::memcpy (outGEDPktOut+GED_PKT_FIX_SIZE, inGEDARcd, GED_ARCD_FIX_SIZE);
			::memcpy (outGEDPktOut+GED_PKT_FIX_SIZE+GED_ARCD_FIX_SIZE, inGEDARcd->src, inGEDARcd->nsrc*sizeof(TGEDRcdSrc));
			::memcpy (outGEDPktOut+GED_PKT_FIX_SIZE+GED_ARCD_FIX_SIZE+inGEDARcd->nsrc*sizeof(TGEDRcdSrc), inGEDARcd->data,
				  inGEDARcd->len);
		}
		break;

		case GED_PKT_REQ_BKD_HISTORY :
		{
			TGEDHRcd *inGEDHRcd (reinterpret_cast <TGEDHRcd *> (inGEDRcd));

			outGEDPktOut = new char [GED_PKT_FIX_SIZE + GED_HRCD_FIX_SIZE + inGEDHRcd->nsrc * sizeof(TGEDRcdSrc) + 
						 inGEDHRcd->len];

			TGEDPktIn *GEDPktIn (reinterpret_cast <TGEDPktIn *> (outGEDPktOut));

			::bzero (outGEDPktOut, GED_PKT_FIX_SIZE + GED_HRCD_FIX_SIZE + inGEDHRcd->nsrc * sizeof(TGEDRcdSrc) + 
				 inGEDHRcd->len);

			GEDPktIn->vrs		= GED_VERSION;
			GEDPktIn->tv   		= inGEDHRcd->otv;
			GEDPktIn->addr.s_addr 	= 0;
			GEDPktIn->typ  		= GED_PKT_TYP_RECORD;
			GEDPktIn->req  		= GED_PKT_REQ_NONE|GED_PKT_REQ_SRC_RELAY;
			GEDPktIn->len  		= GED_HRCD_FIX_SIZE + inGEDHRcd->nsrc * sizeof(TGEDRcdSrc) + inGEDHRcd->len;

			::memcpy (outGEDPktOut+GED_PKT_FIX_SIZE, inGEDHRcd,  GED_HRCD_FIX_SIZE);
			::memcpy (outGEDPktOut+GED_PKT_FIX_SIZE+GED_HRCD_FIX_SIZE, inGEDHRcd->src, inGEDHRcd->nsrc*sizeof(TGEDRcdSrc));
			::memcpy (outGEDPktOut+GED_PKT_FIX_SIZE+GED_HRCD_FIX_SIZE+inGEDHRcd->nsrc*sizeof(TGEDRcdSrc), inGEDHRcd->data,
				  inGEDHRcd->len);
		}
		break;

		case GED_PKT_REQ_BKD_SYNC :
		{
			TGEDSRcd *inGEDSRcd (reinterpret_cast <TGEDSRcd *> (inGEDRcd));

			outGEDPktOut = new char [GED_PKT_FIX_SIZE + GED_SRCD_FIX_SIZE + GED_PKT_FIX_SIZE + inGEDSRcd->pkt->len];

			TGEDPktIn *GEDPktIn (reinterpret_cast <TGEDPktIn *> (outGEDPktOut));

			::bzero (outGEDPktOut, GED_PKT_FIX_SIZE + GED_SRCD_FIX_SIZE + GED_PKT_FIX_SIZE + inGEDSRcd->pkt->len);

			GEDPktIn->vrs		= GED_VERSION;
			GEDPktIn->tv   		= inGEDSRcd->pkt->tv;
			GEDPktIn->addr.s_addr 	= 0;
			GEDPktIn->typ  		= GED_PKT_TYP_RECORD;
			GEDPktIn->req  		= GED_PKT_REQ_NONE|GED_PKT_REQ_SRC_RELAY;
			GEDPktIn->len  		= GED_SRCD_FIX_SIZE + GED_PKT_FIX_SIZE + inGEDSRcd->pkt->len;

			::memcpy (outGEDPktOut+GED_PKT_FIX_SIZE, inGEDSRcd,  GED_SRCD_FIX_SIZE);
			::memcpy (outGEDPktOut+GED_PKT_FIX_SIZE+GED_SRCD_FIX_SIZE, inGEDSRcd->pkt, GED_PKT_FIX_SIZE);
			::memcpy (outGEDPktOut+GED_PKT_FIX_SIZE+GED_SRCD_FIX_SIZE+GED_PKT_FIX_SIZE, inGEDSRcd->pkt->data,
				  inGEDSRcd->pkt->len);
		}
		break;
	}

	return outGEDPktOut;
}
#endif

//---------------------------------------------------------------------------------------------------------------------------------------
// non contigous packet copy
//---------------------------------------------------------------------------------------------------------------------------------------
TGEDPktIn * NewGEDPktIn (TGEDPktIn *inGEDPktIn)
{
	TGEDPktIn *outGEDPktIn = new TGEDPktIn;
	::memcpy (outGEDPktIn, inGEDPktIn, sizeof(TGEDPktIn));
	outGEDPktIn->data = inGEDPktIn->len > 0 ? new char [inGEDPktIn->len] : NULL;
	if (outGEDPktIn->data != NULL) 
		::memcpy (outGEDPktIn->data, inGEDPktIn->data, inGEDPktIn->len);
	return outGEDPktIn;
}

//---------------------------------------------------------------------------------------------------------------------------------------
// sizeof contigous packet
//---------------------------------------------------------------------------------------------------------------------------------------
long SizeOfGEDPktOut (TGEDPktOut inGEDPktOut)
{
	return GED_PKT_FIX_SIZE + reinterpret_cast <TGEDPktIn *> (inGEDPktOut) -> len;
}

//---------------------------------------------------------------------------------------------------------------------------------------
// contigous packet deletion
//---------------------------------------------------------------------------------------------------------------------------------------
void DeleteGEDPktOut (TGEDPktOut &ioGEDPktOut)
{
	delete [] reinterpret_cast <char *> (ioGEDPktOut);
	ioGEDPktOut = NULL;
}

//---------------------------------------------------------------------------------------------------------------------------------------
// non contigous packet deletion
//---------------------------------------------------------------------------------------------------------------------------------------
void DeleteGEDPktIn (TGEDPktIn *&ioGEDPktIn)
{
	if (ioGEDPktIn->data != NULL) delete [] reinterpret_cast <char *> (ioGEDPktIn->data);
	delete ioGEDPktIn; 
	ioGEDPktIn = NULL;
}

#ifdef __GED__
//---------------------------------------------------------------------------------------------------------------------------------------
// record copy
//---------------------------------------------------------------------------------------------------------------------------------------
TGEDRcd * NewGEDRcd (TGEDRcd *inGEDRcd)
{
	if (inGEDRcd == NULL) return NULL;

	switch (inGEDRcd->queue)
	{
		case GED_PKT_REQ_BKD_STAT :
		{
			TGEDStatRcd *newGEDStatRcd = new TGEDStatRcd;
			::memcpy (newGEDStatRcd, inGEDRcd, sizeof(TGEDStatRcd));
			return newGEDStatRcd;
		}
		break;

		case GED_PKT_REQ_BKD_ACTIVE :
		{
			TGEDARcd *newGEDARcd = new TGEDARcd;
			::memcpy (newGEDARcd, inGEDRcd, sizeof(TGEDARcd));
			newGEDARcd->src = new TGEDRcdSrc [newGEDARcd->nsrc];
			::memcpy (newGEDARcd->src, reinterpret_cast <TGEDARcd *> (inGEDRcd)->src, sizeof(TGEDRcdSrc)*newGEDARcd->nsrc);
			if (newGEDARcd->len > 0)
			{
				newGEDARcd->data = new char [newGEDARcd->len];
				::memcpy (newGEDARcd->data, reinterpret_cast <TGEDARcd *> (inGEDRcd)->data, newGEDARcd->len);
			}
			return newGEDARcd;
		}
		break;

		case GED_PKT_REQ_BKD_HISTORY :
		{
			TGEDHRcd *newGEDHRcd = new TGEDHRcd;
			::memcpy (newGEDHRcd, inGEDRcd, sizeof(TGEDHRcd));
			newGEDHRcd->src = new TGEDRcdSrc [newGEDHRcd->nsrc];
			::memcpy (newGEDHRcd->src, reinterpret_cast <TGEDHRcd *> (inGEDRcd)->src, sizeof(TGEDRcdSrc)*newGEDHRcd->nsrc);
			if (newGEDHRcd->len > 0)
			{
				newGEDHRcd->data = new char [newGEDHRcd->len];
				::memcpy (newGEDHRcd->data, reinterpret_cast <TGEDHRcd *> (inGEDRcd)->data, newGEDHRcd->len);
			}
			return newGEDHRcd;
		}
		break;

		case GED_PKT_REQ_BKD_SYNC :
		{
			TGEDSRcd *newGEDSRcd = new TGEDSRcd;
			::memcpy (newGEDSRcd, inGEDRcd, sizeof(TGEDSRcd));
			if (reinterpret_cast <TGEDSRcd *> (inGEDRcd) -> pkt != NULL)
			{
				newGEDSRcd->pkt = new TGEDPktIn;
				::memcpy (newGEDSRcd->pkt, reinterpret_cast <TGEDSRcd *> (inGEDRcd) -> pkt, sizeof(TGEDPktIn));
				if (newGEDSRcd->pkt->len > 0)
				{
					newGEDSRcd->pkt->data = new char [newGEDSRcd->pkt->len];
					::memcpy (newGEDSRcd->pkt->data, reinterpret_cast <TGEDSRcd *> (inGEDRcd)->pkt->data, 
						  newGEDSRcd->pkt->len); 
				}
			}
			return newGEDSRcd;
		}
		break;
	}

	return NULL;
}
#endif

#ifdef __GEDQ__
//---------------------------------------------------------------------------------------------------------------------------------------
// record copy
//---------------------------------------------------------------------------------------------------------------------------------------
TGEDRcd * NewGEDRcd (TGEDPktIn *inGEDPktIn)
{
	if (inGEDPktIn == NULL || inGEDPktIn->typ != GED_PKT_TYP_RECORD) return NULL;

	TGEDRcd *inGEDRcd = reinterpret_cast <TGEDRcd *> (inGEDPktIn->data);

	switch (inGEDRcd->queue & GED_PKT_REQ_BKD_MASK)
	{
		case GED_PKT_REQ_BKD_STAT :
		{
			TGEDStatRcd *newGEDStatRcd = new TGEDStatRcd;

			::memcpy (newGEDStatRcd, inGEDRcd, sizeof(TGEDStatRcd));

			return newGEDStatRcd;
		}
		break;

		case GED_PKT_REQ_BKD_ACTIVE :
		{
			TGEDARcd *inGEDARcd (static_cast <TGEDARcd *> (inGEDRcd));

			TGEDARcd *newGEDARcd = new TGEDARcd;
			::memcpy (newGEDARcd, inGEDRcd, GED_ARCD_FIX_SIZE);
			newGEDARcd->src = new TGEDRcdSrc [newGEDARcd->nsrc];
			::memcpy (newGEDARcd->src, reinterpret_cast<char*>(inGEDARcd)+GED_ARCD_FIX_SIZE, sizeof(TGEDRcdSrc)*newGEDARcd->nsrc);
			if (newGEDARcd->len > 0)
			{
				newGEDARcd->data = new char [newGEDARcd->len];
				::memcpy (newGEDARcd->data, reinterpret_cast <char*>(inGEDARcd)+GED_ARCD_FIX_SIZE+inGEDARcd->nsrc*sizeof(TGEDRcdSrc), newGEDARcd->len);
			}

			return newGEDARcd;
		}
		break;

		case GED_PKT_REQ_BKD_HISTORY :
		{
			TGEDHRcd *inGEDHRcd (static_cast <TGEDHRcd *> (inGEDRcd));

			TGEDHRcd *newGEDHRcd = new TGEDHRcd;
			::memcpy (newGEDHRcd, inGEDRcd, GED_HRCD_FIX_SIZE);
			newGEDHRcd->src = new TGEDRcdSrc [newGEDHRcd->nsrc];
			::memcpy (newGEDHRcd->src, reinterpret_cast<char*>(inGEDHRcd)+GED_HRCD_FIX_SIZE, sizeof(TGEDRcdSrc)*newGEDHRcd->nsrc);
			if (newGEDHRcd->len > 0)
			{
				newGEDHRcd->data = new char [newGEDHRcd->len];
				::memcpy (newGEDHRcd->data, reinterpret_cast <char*>(inGEDHRcd)+GED_HRCD_FIX_SIZE+inGEDHRcd->nsrc*sizeof(TGEDRcdSrc), newGEDHRcd->len);
			}

			return newGEDHRcd;
		}
		break;

		case GED_PKT_REQ_BKD_SYNC :
		{
			TGEDSRcd *inGEDSRcd (static_cast <TGEDSRcd *> (inGEDRcd));

			TGEDSRcd *newGEDSRcd = new TGEDSRcd;
			::memcpy (newGEDSRcd, inGEDRcd, GED_SRCD_FIX_SIZE);
			if (inGEDSRcd->pkt != NULL)
			{
				newGEDSRcd->pkt = new TGEDPktIn;
				::memcpy (newGEDSRcd->pkt, reinterpret_cast<char*>(inGEDSRcd)+GED_SRCD_FIX_SIZE, sizeof(TGEDPktIn));
				if (newGEDSRcd->pkt->len > 0)
				{
					newGEDSRcd->pkt->data = new char [newGEDSRcd->pkt->len];
					::memcpy (newGEDSRcd->pkt->data, reinterpret_cast<char*>(inGEDSRcd)+GED_SRCD_FIX_SIZE+GED_PKT_FIX_SIZE, 
						  newGEDSRcd->pkt->len);
				}
			}

			return newGEDSRcd;
		}
		break;
	}

	return  NULL;
}
#endif

//---------------------------------------------------------------------------------------------------------------------------------------
// record deletion
//---------------------------------------------------------------------------------------------------------------------------------------
void DeleteGEDRcd (TGEDRcd *&ioGEDRcd)
{
	if (ioGEDRcd == NULL) return;

	switch (ioGEDRcd->queue)
	{
		case GED_PKT_REQ_BKD_STAT :
		{
			TGEDStatRcd *inGEDStatRcd = static_cast <TGEDStatRcd *> (ioGEDRcd);
		}
		break;

		case GED_PKT_REQ_BKD_ACTIVE :
		{
			TGEDARcd *inGEDARcd = static_cast <TGEDARcd *> (ioGEDRcd);
			if (inGEDARcd->src  != NULL) delete [] inGEDARcd->src;
			if (inGEDARcd->data != NULL) delete [] reinterpret_cast <char *> (inGEDARcd->data);
		}
		break;

		case GED_PKT_REQ_BKD_HISTORY :
		{
			TGEDHRcd *inGEDHRcd = static_cast <TGEDHRcd *> (ioGEDRcd);
			if (inGEDHRcd->src  != NULL) delete [] inGEDHRcd->src;
			if (inGEDHRcd->data != NULL) delete [] reinterpret_cast <char *> (inGEDHRcd->data);
		}
		break;

		case GED_PKT_REQ_BKD_SYNC :
		{
			TGEDSRcd *inGEDSRcd = static_cast <TGEDSRcd *> (ioGEDRcd);
			if (inGEDSRcd->pkt != NULL)
			{
				if (inGEDSRcd->pkt->data != NULL) delete [] reinterpret_cast <char *> (inGEDSRcd->pkt->data);
				delete [] inGEDSRcd->pkt;
			}
		}
		break;
	}

	delete ioGEDRcd; ioGEDRcd = NULL;
}

//---------------------------------------------------------------------------------------------------------------------------------------
// packet input packet config retrieval
//---------------------------------------------------------------------------------------------------------------------------------------
TGEDPktCfg * GEDPktInToCfg (TGEDPktIn *inGEDPktIn, const TBuffer <TGEDPktCfg> &inGEDPktCfgBuf)
{
	if (inGEDPktIn == NULL) return NULL;

	for (size_t i=inGEDPktCfgBuf.GetLength(); i>0; i--)
		if (inGEDPktCfgBuf[i-1]->type == inGEDPktIn->typ) 
			return inGEDPktCfgBuf[i-1];

	return NULL;
}

//---------------------------------------------------------------------------------------------------------------------------------------
// packet input to string conversion
//---------------------------------------------------------------------------------------------------------------------------------------
CString GEDPktInToHeaderString (TGEDPktIn *inGEDPktIn)
{
	CString outStr; if (inGEDPktIn == NULL) return outStr;

	char inAddr[INET_ADDRSTRLEN]; ::inet_ntop (AF_INET, &(inGEDPktIn->addr), inAddr, INET_ADDRSTRLEN);
	outStr += "vrs:" + CString(inGEDPktIn->vrs) + " sec:" + CString((UInt32)inGEDPktIn->tv.tv_sec) + " usec:" + 
		  CString((UInt32)inGEDPktIn->tv.tv_usec) + " src:" + inAddr + " typ:" + CString(inGEDPktIn->typ) + " req:" + 
		  CString(inGEDPktIn->req) + " rly:" + CString((inGEDPktIn->req&GED_PKT_REQ_SRC_RELAY)?"true":"false") + " len:" + 
		  CString(inGEDPktIn->len) + " p1:" + CString(inGEDPktIn->p1) + " p2:" + CString(inGEDPktIn->p2) + " p3:" + 
		  CString(inGEDPktIn->p3) + " p4:" + CString(inGEDPktIn->p4) + " ";

	switch (inGEDPktIn->req&GED_PKT_REQ_MASK)
	{
		case GED_PKT_REQ_NONE :
		{
			switch (inGEDPktIn->typ)
			{
				case GED_PKT_TYP_ANY 	  : outStr += "[any]"; 		break;
				case GED_PKT_TYP_MD5 	  : outStr += "[md5sum]";	break;
				case GED_PKT_TYP_RECORD   : outStr += "[record]";	break;
				case GED_PKT_TYP_CLOSE 	  : outStr += "[close]"; 	break;
				case GED_PKT_TYP_ACK 	  : outStr += "[ack]"; 		break;
				case GED_PKT_TYP_PULSE 	  : outStr += "[pulse]"; 	break;
				#ifdef __GED_TUN__
				case GED_PKT_TYP_OPEN_TUN : outStr += "[opentun]"; 	break;
				case GED_PKT_TYP_DATA_TUN : outStr += "[datatun]"; 	break;
				case GED_PKT_TYP_SHUT_TUN : outStr += "[shuttun]"; 	break;
				#endif
			}
		}
		break;
		case GED_PKT_REQ_PUSH :
		{
			switch (inGEDPktIn->req&GED_PKT_REQ_PUSH_MASK)
			{
				case GED_PKT_REQ_PUSH_TMSP   : outStr += "[push]";   break;
				case GED_PKT_REQ_PUSH_NOTMSP : outStr += "[update]"; break;
			}
		}
		break;
		case GED_PKT_REQ_DROP : outStr += "[drop]"; break;
		case GED_PKT_REQ_PEEK : outStr += "[peek]"; break;
	}

	if (inGEDPktIn->req&GED_PKT_REQ_NO_SYNC) outStr += " (nosync)";

	return outStr;
}

//---------------------------------------------------------------------------------------------------------------------------------------
// packet input to string conversion
//---------------------------------------------------------------------------------------------------------------------------------------
CString GEDPktInToContentString (TGEDPktIn *inGEDPktIn, TGEDPktCfg *inGEDPktCfg)
{
	CString outStr ("pkt:"); 

	if (inGEDPktCfg == NULL)
	{
		outStr += " (private)";
		return outStr; 
	}

	if (inGEDPktIn == NULL || inGEDPktIn->data == NULL)
	{
		outStr += " (null)";
		return outStr; 
	}

	if (inGEDPktIn->len > 0)
	{
		CChunk inChunk (inGEDPktIn->data, inGEDPktIn->len, ::GEDPktCfgToTData(inGEDPktCfg), false);

		for (size_t i=0; i<inGEDPktCfg->fields.GetLength(); i++)
		{
			bool key=false; for (size_t j=0; j<inGEDPktCfg->keyidc.GetLength() && !key; j++)
				if (i == *inGEDPktCfg->keyidc[j]) key=true;

			outStr += " " + inGEDPktCfg->fields[i]->name + (key?"(*)=":"=");

			switch (inChunk.NextDataIs())
			{
				case DATA_SINT32 :
				{
					SInt32 inSint32=0L;
					inChunk >> inSint32;
					outStr += CString(inSint32);
				}
				break;
				case DATA_STRING :
				{
					SInt8 *inSint8=NULL;
					inChunk >> inSint8;
					outStr += inSint8;
					if (inSint8 != NULL) delete [] inSint8;
				}
				break;
				case DATA_FLOAT64 :
				{
					Float64 inFloat64=0.;
					inChunk >> inFloat64;
					outStr += CString(inFloat64);
				}
				break;
			}
		}
	}

	outStr.Substitute (CString("\n"), CString(" "));

	return outStr;
}

#ifdef __GEDQ__
//---------------------------------------------------------------------------------------------------------------------------------------
// record to xml node (gedq)
//---------------------------------------------------------------------------------------------------------------------------------------
bool GEDRcdToXmlNode (TGEDRcd *inGEDRcd, const TBuffer <TGEDPktCfg> &inGEDPktCfgs, xmlNodePtr &inXmlNodePtr)
{
	if (inGEDRcd == NULL) return false;

	TGEDPktCfg *inGEDPktCfg = NULL; CChunk *inChunk = NULL; xmlNodePtr newXmlNodePtr = NULL;

	switch (inGEDRcd->queue&GED_PKT_REQ_BKD_MASK)
	{
		case GED_PKT_REQ_BKD_STAT :
		{
			TGEDStatRcd *inGEDStatRcd (static_cast <TGEDStatRcd *> (inGEDRcd));

			::xmlNewProp (inXmlNodePtr, XML_ELEMENT_NODE_GED_RECORD_STAT_NTA_ATTR, (xmlChar*)CString(inGEDStatRcd->nta).Get());
			::xmlNewProp (inXmlNodePtr, XML_ELEMENT_NODE_GED_RECORD_STAT_NTS_ATTR, (xmlChar*)CString(inGEDStatRcd->nts).Get());
			::xmlNewProp (inXmlNodePtr, XML_ELEMENT_NODE_GED_RECORD_STAT_NTH_ATTR, (xmlChar*)CString(inGEDStatRcd->nth).Get());
			::xmlNewProp (inXmlNodePtr, XML_ELEMENT_NODE_GED_RECORD_STAT_NTR_ATTR, (xmlChar*)CString(inGEDStatRcd->ntr).Get());
		}
		break;

		case GED_PKT_REQ_BKD_ACTIVE :
		{
			newXmlNodePtr = ::xmlNewChild (inXmlNodePtr, NULL, XML_ELEMENT_NODE_GED_RECORD, NULL);

			TGEDARcd *inGEDARcd (static_cast <TGEDARcd *> (inGEDRcd));

			::xmlNewProp (newXmlNodePtr, XML_ELEMENT_NODE_GED_RECORD_QUEUE_ATTR, XML_ELEMENT_NODE_GED_RECORD_QUEUE_ATTR_ACTIVE);
			::xmlNewProp (newXmlNodePtr, XML_ELEMENT_NODE_GED_RECORD_TYPE_ATTR, (xmlChar*)CString(inGEDARcd->typ).Get());
			::xmlNewProp (newXmlNodePtr, XML_ELEMENT_NODE_GED_RECORD_OCC_ATTR,  (xmlChar*)CString(inGEDARcd->occ).Get());

			xmlNodePtr srcXmlNodePtr = ::xmlNewChild (newXmlNodePtr, NULL, XML_ELEMENT_NODE_GED_RECORD_SOURCES, NULL);

			TGEDRcdSrc *inGEDRcdSrc = reinterpret_cast<TGEDRcdSrc*>(reinterpret_cast<char*>(inGEDARcd)+GED_ARCD_FIX_SIZE);
			for (size_t i=0; i<inGEDARcd->nsrc; i++, inGEDRcdSrc++)
			{
				xmlNodePtr sXmlNodePtr = ::xmlNewChild (srcXmlNodePtr, NULL, XML_ELEMENT_NODE_GED_RECORD_SOURCE, (xmlChar*)CString(::inet_ntoa(inGEDRcdSrc->addr)).Get());
				::xmlNewProp (sXmlNodePtr, XML_ELEMENT_NODE_GED_RECORD_SOURCE_ATTR_RLY, (xmlChar*)CString(inGEDRcdSrc->rly?"true":"false").Get());
			}

			char otm[256]; struct tm sotm; ::localtime_r (&(inGEDARcd->otv.tv_sec), &sotm);
			char ltm[256]; struct tm sltm; ::localtime_r (&(inGEDARcd->ltv.tv_sec), &sltm);
			char rtm[256]; struct tm srtm; ::localtime_r (&(inGEDARcd->rtv.tv_sec), &srtm);
			char mtm[256]; struct tm smtm; ::localtime_r (&(inGEDARcd->mtv.tv_sec), &smtm);
      char ftm[256]; struct tm sftm; ::localtime_r (&(inGEDARcd->ftv.tv_sec), &sftm);

			::strftime (otm, 256, "%Ec", &sotm);
			::strftime (ltm, 256, "%Ec", &sltm);
			::strftime (rtm, 256, "%Ec", &srtm);
			::strftime (mtm, 256, "%Ec", &smtm);
      ::strftime (ftm, 256, "%Ec", &sftm);

			xmlNodePtr newXmlNodePtrS = ::xmlNewChild (newXmlNodePtr, NULL, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP, NULL);

			xmlNodePtr newXmlNodePtrO = ::xmlNewChild (newXmlNodePtrS, NULL, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_ORIGINAL,  (xmlChar*)(CString(otm)-"\n").Get());
			xmlNodePtr newXmlNodePtrL = ::xmlNewChild (newXmlNodePtrS, NULL, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_LAST,  	  (xmlChar*)(CString(ltm)-"\n").Get());
			xmlNodePtr newXmlNodePtrR = ::xmlNewChild (newXmlNodePtrS, NULL, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_RECEPTION, (xmlChar*)(CString(rtm)-"\n").Get());
			xmlNodePtr newXmlNodePtrM = ::xmlNewChild (newXmlNodePtrS, NULL, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_META,      (xmlChar*)(CString(mtm)-"\n").Get());
      xmlNodePtr newXmlNodePtrF = ::xmlNewChild (newXmlNodePtrS, NULL, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_FIRST,     (xmlChar*)(CString(ftm)-"\n").Get());

			::xmlNewProp (newXmlNodePtrO, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_SEC_ATTR,   (xmlChar*)CString((unsigned long)inGEDARcd->otv.tv_sec ).Get());
			::xmlNewProp (newXmlNodePtrO, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_USEC_ATTR,  (xmlChar*)CString((unsigned long)inGEDARcd->otv.tv_usec).Get());
			::xmlNewProp (newXmlNodePtrL, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_SEC_ATTR,   (xmlChar*)CString((unsigned long)inGEDARcd->ltv.tv_sec ).Get());
			::xmlNewProp (newXmlNodePtrL, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_USEC_ATTR,  (xmlChar*)CString((unsigned long)inGEDARcd->ltv.tv_usec).Get());
			::xmlNewProp (newXmlNodePtrM, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_SEC_ATTR,   (xmlChar*)CString((unsigned long)inGEDARcd->mtv.tv_sec ).Get());
			::xmlNewProp (newXmlNodePtrM, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_USEC_ATTR,  (xmlChar*)CString((unsigned long)inGEDARcd->mtv.tv_usec).Get());
			::xmlNewProp (newXmlNodePtrR, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_SEC_ATTR,   (xmlChar*)CString((unsigned long)inGEDARcd->rtv.tv_sec ).Get());
			::xmlNewProp (newXmlNodePtrR, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_USEC_ATTR,  (xmlChar*)CString((unsigned long)inGEDARcd->rtv.tv_usec).Get());
			::xmlNewProp (newXmlNodePtrF, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_SEC_ATTR,   (xmlChar*)CString((unsigned long)inGEDARcd->ftv.tv_sec ).Get());
			::xmlNewProp (newXmlNodePtrF, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_USEC_ATTR,  (xmlChar*)CString((unsigned long)inGEDARcd->ftv.tv_usec).Get());

			for (size_t i=0; i<inGEDPktCfgs.GetLength() && inGEDPktCfg == NULL; i++)
				if (inGEDPktCfgs[i]->type == inGEDARcd->typ) 
					inGEDPktCfg = inGEDPktCfgs[i];

			if (inGEDARcd->len > 0)
				inChunk = new CChunk (reinterpret_cast <char *> (inGEDARcd) + GED_ARCD_FIX_SIZE + 
						      inGEDARcd->nsrc * sizeof(TGEDRcdSrc), inGEDARcd->len, 
						      ::GEDPktCfgToTData(inGEDPktCfg), false);
		}
		break;

		case GED_PKT_REQ_BKD_HISTORY :
		{
			newXmlNodePtr = ::xmlNewChild (inXmlNodePtr, NULL, XML_ELEMENT_NODE_GED_RECORD, NULL);

			TGEDHRcd *inGEDHRcd (static_cast <TGEDHRcd *> (inGEDRcd));

			::xmlNewProp (newXmlNodePtr, XML_ELEMENT_NODE_GED_RECORD_QUEUE_ATTR, XML_ELEMENT_NODE_GED_RECORD_QUEUE_ATTR_HISTORY);
			::xmlNewProp (newXmlNodePtr, XML_ELEMENT_NODE_GED_RECORD_TYPE_ATTR, (xmlChar*)CString(inGEDHRcd->typ).Get());
			::xmlNewProp (newXmlNodePtr, XML_ELEMENT_NODE_GED_RECORD_OCC_ATTR,  (xmlChar*)CString(inGEDHRcd->occ).Get());
			::xmlNewProp (newXmlNodePtr, XML_ELEMENT_NODE_GED_RECORD_ID_ATTR,   (xmlChar*)CString(inGEDHRcd->hid).Get());

			if ((inGEDHRcd->queue & GED_PKT_REQ_BKD_HST_MASK) == GED_PKT_REQ_BKD_HST_TTL)
				::xmlNewProp (newXmlNodePtr, XML_ELEMENT_NODE_GED_RECORD_BKD_HST_ATTR_RSN, XML_ELEMENT_NODE_GED_RECORD_BKD_HST_ATTR_RSN_TTL);
			else
				::xmlNewProp (newXmlNodePtr, XML_ELEMENT_NODE_GED_RECORD_BKD_HST_ATTR_RSN, XML_ELEMENT_NODE_GED_RECORD_BKD_HST_ATTR_RSN_PKT);

			xmlNodePtr srcXmlNodePtr = ::xmlNewChild (newXmlNodePtr, NULL, XML_ELEMENT_NODE_GED_RECORD_SOURCES, NULL);

			TGEDRcdSrc *inGEDRcdSrc = reinterpret_cast<TGEDRcdSrc*>(reinterpret_cast<char*>(inGEDHRcd)+GED_HRCD_FIX_SIZE);
			for (size_t i=0; i<inGEDHRcd->nsrc; i++, inGEDRcdSrc++)
			{
				xmlNodePtr sXmlNodePtr = ::xmlNewChild (srcXmlNodePtr, NULL, XML_ELEMENT_NODE_GED_RECORD_SOURCE, (xmlChar*)CString(::inet_ntoa(inGEDRcdSrc->addr)).Get());
				::xmlNewProp (sXmlNodePtr, XML_ELEMENT_NODE_GED_RECORD_SOURCE_ATTR_RLY,  (xmlChar*)CString(inGEDRcdSrc->rly?"true":"false").Get());
			}

			char otm[256]; struct tm sotm; ::localtime_r (&(inGEDHRcd->otv.tv_sec), &sotm);
			char ltm[256]; struct tm sltm; ::localtime_r (&(inGEDHRcd->ltv.tv_sec), &sltm);
			char rtm[256]; struct tm srtm; ::localtime_r (&(inGEDHRcd->rtv.tv_sec), &srtm);
			char mtm[256]; struct tm smtm; ::localtime_r (&(inGEDHRcd->mtv.tv_sec), &smtm);
      char ftm[256]; struct tm sftm; ::localtime_r (&(inGEDHRcd->ftv.tv_sec), &sftm);
			char atm[256]; struct tm satm; ::localtime_r (&(inGEDHRcd->atv.tv_sec), &satm);

			::strftime (otm, 256, "%Ec", &sotm);
			::strftime (ltm, 256, "%Ec", &sltm);
			::strftime (rtm, 256, "%Ec", &srtm);
			::strftime (mtm, 256, "%Ec", &smtm);
      ::strftime (ftm, 256, "%Ec", &sftm);
			::strftime (atm, 256, "%Ec", &satm);

			xmlNodePtr newXmlNodePtrS = ::xmlNewChild (newXmlNodePtr, NULL, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP, NULL);

			xmlNodePtr newXmlNodePtrO = ::xmlNewChild (newXmlNodePtrS, NULL, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_ORIGINAL,    (xmlChar*)(CString(otm)-"\n").Get());
			xmlNodePtr newXmlNodePtrL = ::xmlNewChild (newXmlNodePtrS, NULL, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_LAST,  	    (xmlChar*)(CString(ltm)-"\n").Get());
			xmlNodePtr newXmlNodePtrR = ::xmlNewChild (newXmlNodePtrS, NULL, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_RECEPTION,   (xmlChar*)(CString(rtm)-"\n").Get());
			xmlNodePtr newXmlNodePtrM = ::xmlNewChild (newXmlNodePtrS, NULL, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_META,        (xmlChar*)(CString(mtm)-"\n").Get());
      xmlNodePtr newXmlNodePtrF = ::xmlNewChild (newXmlNodePtrS, NULL, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_FIRST,       (xmlChar*)(CString(ftm)-"\n").Get());
			xmlNodePtr newXmlNodePtrA = ::xmlNewChild (newXmlNodePtrS, NULL, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_ACKNOWLEGDE, (xmlChar*)(CString(atm)-"\n").Get());

			::xmlNewProp (newXmlNodePtrO, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_SEC_ATTR,   (xmlChar*)CString((unsigned long)inGEDHRcd->otv.tv_sec ).Get());
			::xmlNewProp (newXmlNodePtrO, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_USEC_ATTR,  (xmlChar*)CString((unsigned long)inGEDHRcd->otv.tv_usec).Get());
			::xmlNewProp (newXmlNodePtrL, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_SEC_ATTR,   (xmlChar*)CString((unsigned long)inGEDHRcd->ltv.tv_sec ).Get());
			::xmlNewProp (newXmlNodePtrL, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_USEC_ATTR,  (xmlChar*)CString((unsigned long)inGEDHRcd->ltv.tv_usec).Get());
			::xmlNewProp (newXmlNodePtrR, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_SEC_ATTR,   (xmlChar*)CString((unsigned long)inGEDHRcd->rtv.tv_sec ).Get());
			::xmlNewProp (newXmlNodePtrR, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_USEC_ATTR,  (xmlChar*)CString((unsigned long)inGEDHRcd->rtv.tv_usec).Get());
			::xmlNewProp (newXmlNodePtrM, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_SEC_ATTR,   (xmlChar*)CString((unsigned long)inGEDHRcd->mtv.tv_sec ).Get());
			::xmlNewProp (newXmlNodePtrM, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_USEC_ATTR,  (xmlChar*)CString((unsigned long)inGEDHRcd->mtv.tv_usec).Get());
      ::xmlNewProp (newXmlNodePtrF, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_SEC_ATTR,   (xmlChar*)CString((unsigned long)inGEDHRcd->ftv.tv_sec ).Get());
			::xmlNewProp (newXmlNodePtrF, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_USEC_ATTR,  (xmlChar*)CString((unsigned long)inGEDHRcd->ftv.tv_usec).Get());
			::xmlNewProp (newXmlNodePtrA, XML_ELEMENT_NODE_GED_RECORD_DURATION_ATTR,  	(xmlChar*)CString((unsigned long)inGEDHRcd->atv.tv_sec-inGEDHRcd->otv.tv_sec).Get());
			::xmlNewProp (newXmlNodePtrA, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_SEC_ATTR,   (xmlChar*)CString((unsigned long)inGEDHRcd->atv.tv_sec ).Get());
			::xmlNewProp (newXmlNodePtrA, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_USEC_ATTR,  (xmlChar*)CString((unsigned long)inGEDHRcd->atv.tv_usec).Get());

			for (size_t i=0; i<inGEDPktCfgs.GetLength() && inGEDPktCfg == NULL; i++)
				if (inGEDPktCfgs[i]->type == inGEDHRcd->typ) 
					inGEDPktCfg = inGEDPktCfgs[i];

			if (inGEDHRcd->len > 0)
				inChunk = new CChunk (reinterpret_cast <char *> (inGEDHRcd) + GED_HRCD_FIX_SIZE + 
						      inGEDHRcd->nsrc * sizeof(TGEDRcdSrc), inGEDHRcd->len, 
						      ::GEDPktCfgToTData(inGEDPktCfg), false);
		}
		break;

		case GED_PKT_REQ_BKD_SYNC :
		{
			newXmlNodePtr = ::xmlNewChild (inXmlNodePtr, NULL, XML_ELEMENT_NODE_GED_RECORD, NULL);

			TGEDSRcd *inGEDSRcd (static_cast <TGEDSRcd *> (inGEDRcd));

                        TGEDPktIn *inGEDPktIn (reinterpret_cast <TGEDPktIn *> (reinterpret_cast <char *> (inGEDSRcd) + GED_SRCD_FIX_SIZE));

			::xmlNewProp (newXmlNodePtr, XML_ELEMENT_NODE_GED_RECORD_QUEUE_ATTR, XML_ELEMENT_NODE_GED_RECORD_QUEUE_ATTR_SYNC);
			::xmlNewProp (newXmlNodePtr, XML_ELEMENT_NODE_GED_RECORD_TYPE_ATTR, (xmlChar*)CString(inGEDPktIn->typ).Get());

			::xmlNewProp (::xmlNewChild (newXmlNodePtr, NULL, XML_ELEMENT_NODE_GED_RECORD_SOURCE, (xmlChar*)CString(::inet_ntoa(inGEDPktIn->addr)).Get()), 
				      XML_ELEMENT_NODE_GED_RECORD_SOURCE_ATTR_RLY,  (xmlChar*)CString((inGEDPktIn->req&GED_PKT_REQ_SRC_RELAY)?"true":"false").Get());

			::xmlNewChild (newXmlNodePtr, NULL, XML_ELEMENT_NODE_GED_RECORD_TARGET, (xmlChar*)CString(::inet_ntoa(inGEDSRcd->tgt)).Get());

			char tm[256]; struct tm stm; ::localtime_r (&(inGEDPktIn->tv.tv_sec), &stm);

			::strftime (tm, 256, "%Ec", &stm);

			xmlNodePtr newXmlNodePtrS = ::xmlNewChild (newXmlNodePtr, NULL, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP, NULL);

			xmlNodePtr newXmlNodePtrO = ::xmlNewChild (newXmlNodePtrS, NULL, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_ORIGINAL, (xmlChar*)(CString(tm)-"\n").Get());

			::xmlNewProp (newXmlNodePtrO, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_SEC_ATTR,  (xmlChar*)CString((unsigned long)inGEDPktIn->tv.tv_sec).Get());
			::xmlNewProp (newXmlNodePtrO, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_USEC_ATTR, (xmlChar*)CString((unsigned long)inGEDPktIn->tv.tv_usec).Get());

                        switch (inGEDPktIn->req&GED_PKT_REQ_MASK)
                        {
                                case GED_PKT_REQ_PUSH : 
				{
					switch (inGEDPktIn->req&GED_PKT_REQ_PUSH_MASK)
					{
						case GED_PKT_REQ_PUSH_TMSP :
						{
							::xmlNewProp (newXmlNodePtr, XML_ELEMENT_NODE_GED_RECORD_REQ_ATTR, XML_ELEMENT_NODE_GED_RECORD_REQ_ATTR_PUSH); break;
						}
						break;
						case GED_PKT_REQ_PUSH_NOTMSP :
						{
							::xmlNewProp (newXmlNodePtr, XML_ELEMENT_NODE_GED_RECORD_REQ_ATTR, XML_ELEMENT_NODE_GED_RECORD_REQ_ATTR_UPDATE); break;
						}
						break;
					}
				}
				break;
                                case GED_PKT_REQ_DROP : ::xmlNewProp (newXmlNodePtr, XML_ELEMENT_NODE_GED_RECORD_REQ_ATTR, XML_ELEMENT_NODE_GED_RECORD_REQ_ATTR_DROP); break;
                        }

                        switch (inGEDPktIn->req&GED_PKT_REQ_BKD_MASK)
                        {
                                case GED_PKT_REQ_BKD_ACTIVE  : ::xmlNewProp (newXmlNodePtr, XML_ELEMENT_NODE_GED_RECORD_BKD_ATTR, XML_ELEMENT_NODE_GED_RECORD_QUEUE_ATTR_ACTIVE);  break;
                                case GED_PKT_REQ_BKD_HISTORY : ::xmlNewProp (newXmlNodePtr, XML_ELEMENT_NODE_GED_RECORD_BKD_ATTR, XML_ELEMENT_NODE_GED_RECORD_QUEUE_ATTR_HISTORY); break;
                                case GED_PKT_REQ_BKD_SYNC    : ::xmlNewProp (newXmlNodePtr, XML_ELEMENT_NODE_GED_RECORD_BKD_ATTR, XML_ELEMENT_NODE_GED_RECORD_QUEUE_ATTR_SYNC);    break;
                        }

                        for (size_t i=0; i<inGEDPktCfgs.GetLength() && inGEDPktCfg == NULL; i++)
                                if (inGEDPktCfgs[i]->type == inGEDPktIn->typ)
                                        inGEDPktCfg = inGEDPktCfgs[i];

                        if (inGEDPktIn->len > 0)
                                inChunk = new CChunk (reinterpret_cast <char *> (inGEDPktIn) + GED_PKT_FIX_SIZE, inGEDPktIn->len,
                                                      ::GEDPktCfgToTData(inGEDPktCfg), false);
		}
		break;
	}

	if (inGEDPktCfg == NULL) return true;

	::xmlNewProp (newXmlNodePtr, XML_ELEMENT_NODE_GED_RECORD_NAME_ATTR, (xmlChar*)inGEDPktCfg->name.Get());
	::xmlNewProp (newXmlNodePtr, XML_ELEMENT_NODE_GED_RECORD_HASH_ATTR, (xmlChar*)inGEDPktCfg->hash.Get());

	xmlNodePtr xmlNodePtrContent = ::xmlNewChild (newXmlNodePtr, NULL, XML_ELEMENT_NODE_GED_RECORD_CONTENT, NULL);

	if (inChunk != NULL)
	{
		for (size_t i=0; i<inGEDPktCfg->fields.GetLength(); i++)
		{
			xmlNodePtr xmlNodePtrChild = NULL;

			switch (inChunk->NextDataIs())
			{
				case DATA_SINT32 :
				{
					SInt32 inSint32=0L; 
					if (inChunk->ReadSInt32(inSint32))
						xmlNodePtrChild = ::xmlNewChild (xmlNodePtrContent, NULL, (xmlChar*)inGEDPktCfg->fields[i]->name.Get(), (xmlChar*)CString(inSint32).Get()); 
					else
						xmlNodePtrChild = ::xmlNewChild (xmlNodePtrContent, NULL, (xmlChar*)inGEDPktCfg->fields[i]->name.Get(), (xmlChar*)NULL); 
				}
				break;
				case DATA_STRING :
				{
					SInt8 *inSint8=NULL; 
					*inChunk >> inSint8;
					if (CString(inSint8).GetLength() > 0) 
						xmlNodePtrChild = ::xmlNewChild (xmlNodePtrContent, NULL, (xmlChar*)inGEDPktCfg->fields[i]->name.Get(), (xmlChar*)inSint8);
					else
						xmlNodePtrChild = ::xmlNewChild (xmlNodePtrContent, NULL, (xmlChar*)inGEDPktCfg->fields[i]->name.Get(), NULL);
					if (inSint8 != NULL) delete [] inSint8;
				}
				break;
				case DATA_FLOAT64 :
				{
					Float64 inFloat64=0.;
					if (inChunk->ReadFloat64(inFloat64))
						xmlNodePtrChild = ::xmlNewChild (xmlNodePtrContent, NULL, (xmlChar*)inGEDPktCfg->fields[i]->name.Get(), (xmlChar*)CString(inFloat64).Get());
					else
						xmlNodePtrChild = ::xmlNewChild (xmlNodePtrContent, NULL, (xmlChar*)inGEDPktCfg->fields[i]->name.Get(), (xmlChar*)NULL);
			
				}
				break;
			}

			if (xmlNodePtrChild != NULL)
			{
				::xmlNewProp (xmlNodePtrChild, XML_ELEMENT_NODE_GED_RECORD_CONTENT_KEY_ATTR, (xmlChar*)(inGEDPktCfg->keyidc.Find(i) ? "true" : "false"));
				::xmlNewProp (xmlNodePtrChild, XML_ELEMENT_NODE_GED_RECORD_CONTENT_META_ATTR, (xmlChar*)(inGEDPktCfg->fields[i]->meta ? "true" : "false"));
			}
		}

		delete inChunk;
	}
	else
		for (size_t i=inGEDPktCfg->fields.GetLength(), j=0; i>0; i--, j++)
		{
			xmlNodePtr xmlNodePtrChild = ::xmlNewChild (xmlNodePtrContent, NULL, (xmlChar*)inGEDPktCfg->fields[j]->name.Get(), NULL);
			::xmlNewProp (xmlNodePtrChild, XML_ELEMENT_NODE_GED_RECORD_CONTENT_KEY_ATTR, (xmlChar*)(inGEDPktCfg->keyidc.Find(j) ? "true" : "false"));
			::xmlNewProp (xmlNodePtrChild, XML_ELEMENT_NODE_GED_RECORD_CONTENT_META_ATTR, (xmlChar*)(inGEDPktCfg->fields[j]->meta ? "true" : "false"));
		}

	return true;
}

//---------------------------------------------------------------------------------------------------------------------------------------
// record to xml node light (gedq)
//---------------------------------------------------------------------------------------------------------------------------------------
bool GEDRcdToXmlNodeLight (TGEDRcd *inGEDRcd, const TBuffer <TGEDPktCfg> &inGEDPktCfgs, xmlNodePtr &inXmlNodePtr)
{
	if (inGEDRcd == NULL) return false;

	TGEDPktCfg *inGEDPktCfg = NULL; CChunk *inChunk = NULL; xmlNodePtr newXmlNodePtr = NULL;

	switch (inGEDRcd->queue&GED_PKT_REQ_BKD_MASK)
	{
		case GED_PKT_REQ_BKD_STAT :
		{
			TGEDStatRcd *inGEDStatRcd (static_cast <TGEDStatRcd *> (inGEDRcd));

			::xmlNewProp (inXmlNodePtr, XML_ELEMENT_NODE_GED_RECORD_STAT_NTA_ATTR_LIGHT, (xmlChar*)CString(inGEDStatRcd->nta).Get());
			::xmlNewProp (inXmlNodePtr, XML_ELEMENT_NODE_GED_RECORD_STAT_NTS_ATTR_LIGHT, (xmlChar*)CString(inGEDStatRcd->nts).Get());
			::xmlNewProp (inXmlNodePtr, XML_ELEMENT_NODE_GED_RECORD_STAT_NTH_ATTR_LIGHT, (xmlChar*)CString(inGEDStatRcd->nth).Get());
			::xmlNewProp (inXmlNodePtr, XML_ELEMENT_NODE_GED_RECORD_STAT_NTR_ATTR_LIGHT, (xmlChar*)CString(inGEDStatRcd->ntr).Get());
		}
		break;

		case GED_PKT_REQ_BKD_ACTIVE :
		{
			newXmlNodePtr = ::xmlNewChild (inXmlNodePtr, NULL, XML_ELEMENT_NODE_GED_RECORD_LIGHT, NULL);

			TGEDARcd *inGEDARcd (static_cast <TGEDARcd *> (inGEDRcd));

			::xmlNewProp (newXmlNodePtr, XML_ELEMENT_NODE_GED_RECORD_QUEUE_ATTR_LIGHT, XML_ELEMENT_NODE_GED_RECORD_QUEUE_ATTR_ACTIVE_LIGHT);
			::xmlNewProp (newXmlNodePtr, XML_ELEMENT_NODE_GED_RECORD_TYPE_ATTR_LIGHT, (xmlChar*)CString(inGEDARcd->typ).Get());
			::xmlNewProp (newXmlNodePtr, XML_ELEMENT_NODE_GED_RECORD_OCC_ATTR_LIGHT,  (xmlChar*)CString(inGEDARcd->occ).Get());

			xmlNodePtr srcXmlNodePtr = ::xmlNewChild (newXmlNodePtr, NULL, XML_ELEMENT_NODE_GED_RECORD_SOURCES_LIGHT, NULL);

			TGEDRcdSrc *inGEDRcdSrc = reinterpret_cast<TGEDRcdSrc*>(reinterpret_cast<char*>(inGEDARcd)+GED_ARCD_FIX_SIZE);
			for (size_t i=0; i<inGEDARcd->nsrc; i++, inGEDRcdSrc++)
			{
				xmlNodePtr sXmlNodePtr = ::xmlNewChild (srcXmlNodePtr, NULL, XML_ELEMENT_NODE_GED_RECORD_SOURCE_LIGHT, (xmlChar*)CString(::inet_ntoa(inGEDRcdSrc->addr)).Get());
				::xmlNewProp (sXmlNodePtr, XML_ELEMENT_NODE_GED_RECORD_SOURCE_ATTR_RLY_LIGHT, (xmlChar*)CString(inGEDRcdSrc->rly?"1":"0").Get());
			}

			xmlNodePtr newXmlNodePtrS = ::xmlNewChild (newXmlNodePtr, NULL, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_LIGHT, NULL);

			xmlNodePtr newXmlNodePtrO = ::xmlNewChild (newXmlNodePtrS, NULL, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_ORIGINAL_LIGHT,  NULL);
			xmlNodePtr newXmlNodePtrL = ::xmlNewChild (newXmlNodePtrS, NULL, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_LAST_LIGHT,  	NULL);
			xmlNodePtr newXmlNodePtrR = ::xmlNewChild (newXmlNodePtrS, NULL, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_RECEPTION_LIGHT, NULL);
			xmlNodePtr newXmlNodePtrM = ::xmlNewChild (newXmlNodePtrS, NULL, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_META_LIGHT,      NULL);
      xmlNodePtr newXmlNodePtrF = ::xmlNewChild (newXmlNodePtrS, NULL, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_FIRST_LIGHT,      NULL);

			::xmlNewProp (newXmlNodePtrO, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_SEC_ATTR_LIGHT,   (xmlChar*)CString((unsigned long)inGEDARcd->otv.tv_sec ).Get());
			::xmlNewProp (newXmlNodePtrO, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_USEC_ATTR_LIGHT,  (xmlChar*)CString((unsigned long)inGEDARcd->otv.tv_usec).Get());
			::xmlNewProp (newXmlNodePtrL, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_SEC_ATTR_LIGHT,   (xmlChar*)CString((unsigned long)inGEDARcd->ltv.tv_sec ).Get());
			::xmlNewProp (newXmlNodePtrL, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_USEC_ATTR_LIGHT,  (xmlChar*)CString((unsigned long)inGEDARcd->ltv.tv_usec).Get());
			::xmlNewProp (newXmlNodePtrM, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_SEC_ATTR_LIGHT,   (xmlChar*)CString((unsigned long)inGEDARcd->mtv.tv_sec ).Get());
			::xmlNewProp (newXmlNodePtrM, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_USEC_ATTR_LIGHT,  (xmlChar*)CString((unsigned long)inGEDARcd->mtv.tv_usec).Get());
			::xmlNewProp (newXmlNodePtrR, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_SEC_ATTR_LIGHT,   (xmlChar*)CString((unsigned long)inGEDARcd->rtv.tv_sec ).Get());
			::xmlNewProp (newXmlNodePtrR, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_USEC_ATTR_LIGHT,  (xmlChar*)CString((unsigned long)inGEDARcd->rtv.tv_usec).Get());
			::xmlNewProp (newXmlNodePtrF, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_SEC_ATTR_LIGHT,   (xmlChar*)CString((unsigned long)inGEDARcd->ftv.tv_sec ).Get());
			::xmlNewProp (newXmlNodePtrF, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_USEC_ATTR_LIGHT,  (xmlChar*)CString((unsigned long)inGEDARcd->ftv.tv_usec).Get());

			for (size_t i=0; i<inGEDPktCfgs.GetLength() && inGEDPktCfg == NULL; i++)
				if (inGEDPktCfgs[i]->type == inGEDARcd->typ) 
					inGEDPktCfg = inGEDPktCfgs[i];

			if (inGEDARcd->len > 0)
				inChunk = new CChunk (reinterpret_cast <char *> (inGEDARcd) + GED_ARCD_FIX_SIZE + 
						      inGEDARcd->nsrc * sizeof(TGEDRcdSrc), inGEDARcd->len, 
						      ::GEDPktCfgToTData(inGEDPktCfg), false);
		}
		break;

		case GED_PKT_REQ_BKD_HISTORY :
		{
			newXmlNodePtr = ::xmlNewChild (inXmlNodePtr, NULL, XML_ELEMENT_NODE_GED_RECORD_LIGHT, NULL);

			TGEDHRcd *inGEDHRcd (static_cast <TGEDHRcd *> (inGEDRcd));

			::xmlNewProp (newXmlNodePtr, XML_ELEMENT_NODE_GED_RECORD_QUEUE_ATTR_LIGHT, XML_ELEMENT_NODE_GED_RECORD_QUEUE_ATTR_HISTORY_LIGHT);
			::xmlNewProp (newXmlNodePtr, XML_ELEMENT_NODE_GED_RECORD_TYPE_ATTR_LIGHT, (xmlChar*)CString(inGEDHRcd->typ).Get());
			::xmlNewProp (newXmlNodePtr, XML_ELEMENT_NODE_GED_RECORD_OCC_ATTR_LIGHT,  (xmlChar*)CString(inGEDHRcd->occ).Get());
			::xmlNewProp (newXmlNodePtr, XML_ELEMENT_NODE_GED_RECORD_ID_ATTR_LIGHT,   (xmlChar*)CString(inGEDHRcd->hid).Get());

			if ((inGEDHRcd->queue & GED_PKT_REQ_BKD_HST_MASK) == GED_PKT_REQ_BKD_HST_TTL)
				::xmlNewProp (newXmlNodePtr, XML_ELEMENT_NODE_GED_RECORD_BKD_HST_ATTR_RSN_LIGHT, XML_ELEMENT_NODE_GED_RECORD_BKD_HST_ATTR_RSN_TTL_LIGHT);
			else
				::xmlNewProp (newXmlNodePtr, XML_ELEMENT_NODE_GED_RECORD_BKD_HST_ATTR_RSN_LIGHT, XML_ELEMENT_NODE_GED_RECORD_BKD_HST_ATTR_RSN_PKT_LIGHT);

			xmlNodePtr srcXmlNodePtr = ::xmlNewChild (newXmlNodePtr, NULL, XML_ELEMENT_NODE_GED_RECORD_SOURCES_LIGHT, NULL);

			TGEDRcdSrc *inGEDRcdSrc = reinterpret_cast<TGEDRcdSrc*>(reinterpret_cast<char*>(inGEDHRcd)+GED_HRCD_FIX_SIZE);
			for (size_t i=0; i<inGEDHRcd->nsrc; i++, inGEDRcdSrc++)
			{
				xmlNodePtr sXmlNodePtr = ::xmlNewChild (srcXmlNodePtr, NULL, XML_ELEMENT_NODE_GED_RECORD_SOURCE_LIGHT, (xmlChar*)CString(::inet_ntoa(inGEDRcdSrc->addr)).Get());
				::xmlNewProp (sXmlNodePtr, XML_ELEMENT_NODE_GED_RECORD_SOURCE_ATTR_RLY_LIGHT,  (xmlChar*)CString(inGEDRcdSrc->rly?"1":"0").Get());
			}

			xmlNodePtr newXmlNodePtrS = ::xmlNewChild (newXmlNodePtr, NULL, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_LIGHT, NULL);

			xmlNodePtr newXmlNodePtrO = ::xmlNewChild (newXmlNodePtrS, NULL, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_ORIGINAL_LIGHT,    NULL);
			xmlNodePtr newXmlNodePtrL = ::xmlNewChild (newXmlNodePtrS, NULL, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_LAST_LIGHT,  	  NULL);
			xmlNodePtr newXmlNodePtrR = ::xmlNewChild (newXmlNodePtrS, NULL, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_RECEPTION_LIGHT,   NULL);
			xmlNodePtr newXmlNodePtrM = ::xmlNewChild (newXmlNodePtrS, NULL, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_META_LIGHT,        NULL);
      xmlNodePtr newXmlNodePtrF = ::xmlNewChild (newXmlNodePtrS, NULL, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_FIRST_LIGHT,      NULL);
			xmlNodePtr newXmlNodePtrA = ::xmlNewChild (newXmlNodePtrS, NULL, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_ACKNOWLEGDE_LIGHT, NULL);

			::xmlNewProp (newXmlNodePtrO, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_SEC_ATTR_LIGHT,   (xmlChar*)CString((unsigned long)inGEDHRcd->otv.tv_sec ).Get());
			::xmlNewProp (newXmlNodePtrO, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_USEC_ATTR_LIGHT,  (xmlChar*)CString((unsigned long)inGEDHRcd->otv.tv_usec).Get());
			::xmlNewProp (newXmlNodePtrL, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_SEC_ATTR_LIGHT,   (xmlChar*)CString((unsigned long)inGEDHRcd->ltv.tv_sec ).Get());
			::xmlNewProp (newXmlNodePtrL, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_USEC_ATTR_LIGHT,  (xmlChar*)CString((unsigned long)inGEDHRcd->ltv.tv_usec).Get());
			::xmlNewProp (newXmlNodePtrR, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_SEC_ATTR_LIGHT,   (xmlChar*)CString((unsigned long)inGEDHRcd->rtv.tv_sec ).Get());
			::xmlNewProp (newXmlNodePtrR, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_USEC_ATTR_LIGHT,  (xmlChar*)CString((unsigned long)inGEDHRcd->rtv.tv_usec).Get());
			::xmlNewProp (newXmlNodePtrM, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_SEC_ATTR_LIGHT,   (xmlChar*)CString((unsigned long)inGEDHRcd->mtv.tv_sec ).Get());
			::xmlNewProp (newXmlNodePtrM, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_USEC_ATTR_LIGHT,  (xmlChar*)CString((unsigned long)inGEDHRcd->mtv.tv_usec).Get());
			::xmlNewProp (newXmlNodePtrF, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_SEC_ATTR_LIGHT,   (xmlChar*)CString((unsigned long)inGEDHRcd->ftv.tv_sec ).Get());
			::xmlNewProp (newXmlNodePtrF, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_USEC_ATTR_LIGHT,  (xmlChar*)CString((unsigned long)inGEDHRcd->ftv.tv_usec).Get());
			::xmlNewProp (newXmlNodePtrA, XML_ELEMENT_NODE_GED_RECORD_DURATION_ATTR_LIGHT,        (xmlChar*)CString((unsigned long)inGEDHRcd->atv.tv_sec-inGEDHRcd->otv.tv_sec).Get());
			::xmlNewProp (newXmlNodePtrA, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_SEC_ATTR_LIGHT,   (xmlChar*)CString((unsigned long)inGEDHRcd->atv.tv_sec ).Get());
			::xmlNewProp (newXmlNodePtrA, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_USEC_ATTR_LIGHT,  (xmlChar*)CString((unsigned long)inGEDHRcd->atv.tv_usec).Get());

			for (size_t i=0; i<inGEDPktCfgs.GetLength() && inGEDPktCfg == NULL; i++)
				if (inGEDPktCfgs[i]->type == inGEDHRcd->typ) 
					inGEDPktCfg = inGEDPktCfgs[i];

			if (inGEDHRcd->len > 0)
				inChunk = new CChunk (reinterpret_cast <char *> (inGEDHRcd) + GED_HRCD_FIX_SIZE + 
						      inGEDHRcd->nsrc * sizeof(TGEDRcdSrc), inGEDHRcd->len, 
						      ::GEDPktCfgToTData(inGEDPktCfg), false);
		}
		break;

		case GED_PKT_REQ_BKD_SYNC :
		{
			newXmlNodePtr = ::xmlNewChild (inXmlNodePtr, NULL, XML_ELEMENT_NODE_GED_RECORD_LIGHT, NULL);

			TGEDSRcd *inGEDSRcd (static_cast <TGEDSRcd *> (inGEDRcd));

                        TGEDPktIn *inGEDPktIn (reinterpret_cast <TGEDPktIn *> (reinterpret_cast <char *> (inGEDSRcd) + GED_SRCD_FIX_SIZE));

			::xmlNewProp (newXmlNodePtr, XML_ELEMENT_NODE_GED_RECORD_QUEUE_ATTR_LIGHT, XML_ELEMENT_NODE_GED_RECORD_QUEUE_ATTR_SYNC_LIGHT);
			::xmlNewProp (newXmlNodePtr, XML_ELEMENT_NODE_GED_RECORD_TYPE_ATTR_LIGHT, (xmlChar*)CString(inGEDPktIn->typ).Get());

			::xmlNewProp (::xmlNewChild (newXmlNodePtr, NULL, XML_ELEMENT_NODE_GED_RECORD_SOURCE_LIGHT, (xmlChar*)CString(::inet_ntoa(inGEDPktIn->addr)).Get()), 
				      XML_ELEMENT_NODE_GED_RECORD_SOURCE_ATTR_RLY_LIGHT,  (xmlChar*)CString((inGEDPktIn->req&GED_PKT_REQ_SRC_RELAY)?"1":"0").Get());

			::xmlNewChild (newXmlNodePtr, NULL, XML_ELEMENT_NODE_GED_RECORD_TARGET_LIGHT, (xmlChar*)CString(::inet_ntoa(inGEDSRcd->tgt)).Get());

			xmlNodePtr newXmlNodePtrS = ::xmlNewChild (newXmlNodePtr, NULL, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_LIGHT, NULL);

			xmlNodePtr newXmlNodePtrO = ::xmlNewChild (newXmlNodePtrS, NULL, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_ORIGINAL_LIGHT, NULL);

			::xmlNewProp (newXmlNodePtrO, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_SEC_ATTR_LIGHT,  (xmlChar*)CString((unsigned long)inGEDPktIn->tv.tv_sec).Get());
			::xmlNewProp (newXmlNodePtrO, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_USEC_ATTR_LIGHT, (xmlChar*)CString((unsigned long)inGEDPktIn->tv.tv_usec).Get());

                        switch (inGEDPktIn->req&GED_PKT_REQ_MASK)
                        {
                                case GED_PKT_REQ_PUSH : 
				{
					switch (inGEDPktIn->req&GED_PKT_REQ_PUSH_MASK)
					{
						case GED_PKT_REQ_PUSH_TMSP :
						{
							::xmlNewProp (newXmlNodePtr, XML_ELEMENT_NODE_GED_RECORD_REQ_ATTR_LIGHT, XML_ELEMENT_NODE_GED_RECORD_REQ_ATTR_PUSH_LIGHT); break;
						}
						break;
						case GED_PKT_REQ_PUSH_NOTMSP :
						{
							::xmlNewProp (newXmlNodePtr, XML_ELEMENT_NODE_GED_RECORD_REQ_ATTR_LIGHT, XML_ELEMENT_NODE_GED_RECORD_REQ_ATTR_UPDATE_LIGHT); break;
						}
						break;
					}
				}
				break;
                                case GED_PKT_REQ_DROP : ::xmlNewProp (newXmlNodePtr, XML_ELEMENT_NODE_GED_RECORD_REQ_ATTR_LIGHT, XML_ELEMENT_NODE_GED_RECORD_REQ_ATTR_DROP_LIGHT); break;
                        }

                        switch (inGEDPktIn->req&GED_PKT_REQ_BKD_MASK)
                        {
                                case GED_PKT_REQ_BKD_ACTIVE  : ::xmlNewProp (newXmlNodePtr, XML_ELEMENT_NODE_GED_RECORD_BKD_ATTR_LIGHT, XML_ELEMENT_NODE_GED_RECORD_QUEUE_ATTR_ACTIVE_LIGHT);  break;
                                case GED_PKT_REQ_BKD_HISTORY : ::xmlNewProp (newXmlNodePtr, XML_ELEMENT_NODE_GED_RECORD_BKD_ATTR_LIGHT, XML_ELEMENT_NODE_GED_RECORD_QUEUE_ATTR_HISTORY_LIGHT); break;
                                case GED_PKT_REQ_BKD_SYNC    : ::xmlNewProp (newXmlNodePtr, XML_ELEMENT_NODE_GED_RECORD_BKD_ATTR_LIGHT, XML_ELEMENT_NODE_GED_RECORD_QUEUE_ATTR_SYNC_LIGHT);    break;
                        }

                        for (size_t i=0; i<inGEDPktCfgs.GetLength() && inGEDPktCfg == NULL; i++)
                                if (inGEDPktCfgs[i]->type == inGEDPktIn->typ)
                                        inGEDPktCfg = inGEDPktCfgs[i];

                        if (inGEDPktIn->len > 0)
                                inChunk = new CChunk (reinterpret_cast <char *> (inGEDPktIn) + GED_PKT_FIX_SIZE, inGEDPktIn->len,
                                                      ::GEDPktCfgToTData(inGEDPktCfg), false);
		}
		break;
	}

	if (inGEDPktCfg == NULL) return true;

	::xmlNewProp (newXmlNodePtr, XML_ELEMENT_NODE_GED_RECORD_HASH_ATTR_LIGHT, (xmlChar*)inGEDPktCfg->hash.Get());

	xmlNodePtr xmlNodePtrContent = ::xmlNewChild (newXmlNodePtr, NULL, XML_ELEMENT_NODE_GED_RECORD_CONTENT_LIGHT, NULL);

	if (inChunk != NULL)
	{
		for (size_t i=0; i<inGEDPktCfg->fields.GetLength(); i++)
		{
			xmlNodePtr xmlNodePtrChild = NULL;

			switch (inChunk->NextDataIs())
			{
				case DATA_SINT32 :
				{
					SInt32 inSint32=0L; 
					if (inChunk->ReadSInt32(inSint32))
						xmlNodePtrChild = ::xmlNewChild (xmlNodePtrContent, NULL, (xmlChar*)inGEDPktCfg->fields[i]->name.Get(), (xmlChar*)CString(inSint32).Get()); 
					else
						xmlNodePtrChild = ::xmlNewChild (xmlNodePtrContent, NULL, (xmlChar*)inGEDPktCfg->fields[i]->name.Get(), (xmlChar*)NULL); 
				}
				break;
				case DATA_STRING :
				{
					SInt8 *inSint8=NULL; 
					*inChunk >> inSint8;
					if (CString(inSint8).GetLength() > 0) 
						xmlNodePtrChild = ::xmlNewChild (xmlNodePtrContent, NULL, (xmlChar*)inGEDPktCfg->fields[i]->name.Get(), (xmlChar*)inSint8);
					else
						xmlNodePtrChild = ::xmlNewChild (xmlNodePtrContent, NULL, (xmlChar*)inGEDPktCfg->fields[i]->name.Get(), NULL);
					if (inSint8 != NULL) delete [] inSint8;
				}
				break;
				case DATA_FLOAT64 :
				{
					Float64 inFloat64=0.;
					if (inChunk->ReadFloat64(inFloat64))
						xmlNodePtrChild = ::xmlNewChild (xmlNodePtrContent, NULL, (xmlChar*)inGEDPktCfg->fields[i]->name.Get(), (xmlChar*)CString(inFloat64).Get());
					else
						xmlNodePtrChild = ::xmlNewChild (xmlNodePtrContent, NULL, (xmlChar*)inGEDPktCfg->fields[i]->name.Get(), (xmlChar*)NULL);
			
				}
				break;
			}
		}

		delete inChunk;
	}
	else
		for (size_t i=inGEDPktCfg->fields.GetLength(), j=0; i>0; i--, j++)
		{
			xmlNodePtr xmlNodePtrChild = ::xmlNewChild (xmlNodePtrContent, NULL, (xmlChar*)inGEDPktCfg->fields[j]->name.Get(), NULL);
		}

	return true;
}
#endif

#ifdef __GED__
//---------------------------------------------------------------------------------------------------------------------------------------
// record to xml node (ged)
//---------------------------------------------------------------------------------------------------------------------------------------
bool GEDRcdToXmlNode (TGEDRcd *inGEDRcd, const TBuffer <TGEDPktCfg> &inGEDPktCfgs, xmlNodePtr &inXmlNodePtr)
{
	if (inGEDRcd == NULL) return false;

	TGEDPktCfg *inGEDPktCfg = NULL; CChunk *inChunk = NULL; xmlNodePtr newXmlNodePtr = NULL;

	switch (inGEDRcd->queue&GED_PKT_REQ_BKD_MASK)
	{
		case GED_PKT_REQ_BKD_STAT :
		{
			TGEDStatRcd *inGEDStatRcd (static_cast <TGEDStatRcd *> (inGEDRcd));

			::xmlNewProp (inXmlNodePtr, XML_ELEMENT_NODE_GED_RECORD_STAT_NTA_ATTR, (xmlChar*)CString(inGEDStatRcd->nta).Get());
			::xmlNewProp (inXmlNodePtr, XML_ELEMENT_NODE_GED_RECORD_STAT_NTS_ATTR, (xmlChar*)CString(inGEDStatRcd->nts).Get());
			::xmlNewProp (inXmlNodePtr, XML_ELEMENT_NODE_GED_RECORD_STAT_NTH_ATTR, (xmlChar*)CString(inGEDStatRcd->nth).Get());
			::xmlNewProp (inXmlNodePtr, XML_ELEMENT_NODE_GED_RECORD_STAT_NTR_ATTR, (xmlChar*)CString(inGEDStatRcd->ntr).Get());
		}
		break;

		case GED_PKT_REQ_BKD_ACTIVE :
		{
			newXmlNodePtr = ::xmlNewChild (inXmlNodePtr, NULL, XML_ELEMENT_NODE_GED_RECORD, NULL);

			TGEDARcd *inGEDARcd (static_cast <TGEDARcd *> (inGEDRcd));

			::xmlNewProp (newXmlNodePtr, XML_ELEMENT_NODE_GED_RECORD_QUEUE_ATTR, XML_ELEMENT_NODE_GED_RECORD_QUEUE_ATTR_ACTIVE);
			::xmlNewProp (newXmlNodePtr, XML_ELEMENT_NODE_GED_RECORD_TYPE_ATTR, (xmlChar*)CString(inGEDARcd->typ).Get());
			::xmlNewProp (newXmlNodePtr, XML_ELEMENT_NODE_GED_RECORD_OCC_ATTR,  (xmlChar*)CString(inGEDARcd->occ).Get());

			xmlNodePtr srcXmlNodePtr = ::xmlNewChild (newXmlNodePtr, NULL, XML_ELEMENT_NODE_GED_RECORD_SOURCES, NULL);

			for (size_t i=0; i<inGEDARcd->nsrc; i++)
			{
				xmlNodePtr sXmlNodePtr = ::xmlNewChild (srcXmlNodePtr, NULL, XML_ELEMENT_NODE_GED_RECORD_SOURCE, (xmlChar*)CString(::inet_ntoa(inGEDARcd->src[i].addr)).Get());
				::xmlNewProp (sXmlNodePtr, XML_ELEMENT_NODE_GED_RECORD_SOURCE_ATTR_RLY,  (xmlChar*)CString(inGEDARcd->src[i].rly?"true":"false").Get());
			}

			char otm[256]; struct tm sotm; ::localtime_r (&(inGEDARcd->otv.tv_sec), &sotm);
			char ltm[256]; struct tm sltm; ::localtime_r (&(inGEDARcd->ltv.tv_sec), &sltm);
			char rtm[256]; struct tm srtm; ::localtime_r (&(inGEDARcd->rtv.tv_sec), &srtm);
			char mtm[256]; struct tm smtm; ::localtime_r (&(inGEDARcd->mtv.tv_sec), &smtm);
			char ftm[256]; struct tm sftm; ::localtime_r (&(inGEDARcd->ftv.tv_sec), &sftm);

			::strftime (otm, 256, "%Ec", &sotm);
			::strftime (ltm, 256, "%Ec", &sltm);
			::strftime (rtm, 256, "%Ec", &srtm);
			::strftime (mtm, 256, "%Ec", &smtm);
			::strftime (ftm, 256, "%Ec", &sftm);

			xmlNodePtr newXmlNodePtrS = ::xmlNewChild (newXmlNodePtr, NULL, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP, NULL);

			xmlNodePtr newXmlNodePtrO = ::xmlNewChild (newXmlNodePtrS, NULL, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_ORIGINAL,  (xmlChar*)(CString(otm)-"\n").Get());
			xmlNodePtr newXmlNodePtrL = ::xmlNewChild (newXmlNodePtrS, NULL, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_LAST,  	  (xmlChar*)(CString(ltm)-"\n").Get());
			xmlNodePtr newXmlNodePtrR = ::xmlNewChild (newXmlNodePtrS, NULL, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_RECEPTION, (xmlChar*)(CString(rtm)-"\n").Get());
			xmlNodePtr newXmlNodePtrM = ::xmlNewChild (newXmlNodePtrS, NULL, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_META, 	  (xmlChar*)(CString(mtm)-"\n").Get());
			xmlNodePtr newXmlNodePtrF = ::xmlNewChild (newXmlNodePtrS, NULL, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_FIRST, 	  (xmlChar*)(CString(ftm)-"\n").Get());

			::xmlNewProp (newXmlNodePtrO, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_SEC_ATTR,   (xmlChar*)CString((unsigned long)inGEDARcd->otv.tv_sec ).Get());
			::xmlNewProp (newXmlNodePtrO, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_USEC_ATTR,  (xmlChar*)CString((unsigned long)inGEDARcd->otv.tv_usec).Get());
			::xmlNewProp (newXmlNodePtrL, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_SEC_ATTR,   (xmlChar*)CString((unsigned long)inGEDARcd->ltv.tv_sec ).Get());
			::xmlNewProp (newXmlNodePtrL, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_USEC_ATTR,  (xmlChar*)CString((unsigned long)inGEDARcd->ltv.tv_usec).Get());
			::xmlNewProp (newXmlNodePtrM, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_SEC_ATTR,   (xmlChar*)CString((unsigned long)inGEDARcd->mtv.tv_sec ).Get());
			::xmlNewProp (newXmlNodePtrM, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_USEC_ATTR,  (xmlChar*)CString((unsigned long)inGEDARcd->mtv.tv_usec).Get());
			::xmlNewProp (newXmlNodePtrR, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_SEC_ATTR,   (xmlChar*)CString((unsigned long)inGEDARcd->rtv.tv_sec ).Get());
			::xmlNewProp (newXmlNodePtrR, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_USEC_ATTR,  (xmlChar*)CString((unsigned long)inGEDARcd->rtv.tv_usec).Get());
			::xmlNewProp (newXmlNodePtrF, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_SEC_ATTR,   (xmlChar*)CString((unsigned long)inGEDARcd->ftv.tv_sec ).Get());
			::xmlNewProp (newXmlNodePtrF, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_USEC_ATTR,  (xmlChar*)CString((unsigned long)inGEDARcd->ftv.tv_usec).Get());

			for (size_t i=0; i<inGEDPktCfgs.GetLength() && inGEDPktCfg == NULL; i++)
				if (inGEDPktCfgs[i]->type == inGEDARcd->typ) 
					inGEDPktCfg = inGEDPktCfgs[i];

			if (inGEDARcd->len > 0)
				inChunk = new CChunk (inGEDARcd->data, inGEDARcd->len, ::GEDPktCfgToTData(inGEDPktCfg), false);
		}
		break;

		case GED_PKT_REQ_BKD_HISTORY :
		{
			newXmlNodePtr = ::xmlNewChild (inXmlNodePtr, NULL, XML_ELEMENT_NODE_GED_RECORD, NULL);

			TGEDHRcd *inGEDHRcd (static_cast <TGEDHRcd *> (inGEDRcd));

			::xmlNewProp (newXmlNodePtr, XML_ELEMENT_NODE_GED_RECORD_QUEUE_ATTR, XML_ELEMENT_NODE_GED_RECORD_QUEUE_ATTR_HISTORY);
			::xmlNewProp (newXmlNodePtr, XML_ELEMENT_NODE_GED_RECORD_TYPE_ATTR, (xmlChar*)CString(inGEDHRcd->typ).Get());
			::xmlNewProp (newXmlNodePtr, XML_ELEMENT_NODE_GED_RECORD_OCC_ATTR,  (xmlChar*)CString(inGEDHRcd->occ).Get());
			::xmlNewProp (newXmlNodePtr, XML_ELEMENT_NODE_GED_RECORD_ID_ATTR,   (xmlChar*)CString(inGEDHRcd->hid).Get());

			if ((inGEDHRcd->queue & GED_PKT_REQ_BKD_HST_MASK) == GED_PKT_REQ_BKD_HST_TTL)
				::xmlNewProp (newXmlNodePtr, XML_ELEMENT_NODE_GED_RECORD_BKD_HST_ATTR_RSN, XML_ELEMENT_NODE_GED_RECORD_BKD_HST_ATTR_RSN_TTL);
			else
				::xmlNewProp (newXmlNodePtr, XML_ELEMENT_NODE_GED_RECORD_BKD_HST_ATTR_RSN, XML_ELEMENT_NODE_GED_RECORD_BKD_HST_ATTR_RSN_PKT);

			xmlNodePtr srcXmlNodePtr = ::xmlNewChild (newXmlNodePtr, NULL, XML_ELEMENT_NODE_GED_RECORD_SOURCES, NULL);

			for (size_t i=0; i<inGEDHRcd->nsrc; i++)
			{
				xmlNodePtr sXmlNodePtr = ::xmlNewChild (srcXmlNodePtr, NULL, XML_ELEMENT_NODE_GED_RECORD_SOURCE, (xmlChar*)CString(::inet_ntoa(inGEDHRcd->src[i].addr)).Get());
				::xmlNewProp (sXmlNodePtr, XML_ELEMENT_NODE_GED_RECORD_SOURCE_ATTR_RLY,  (xmlChar*)CString(inGEDHRcd->src[i].rly?"true":"false").Get());
			}

			char otm[256]; struct tm sotm; ::localtime_r (&(inGEDHRcd->otv.tv_sec), &sotm);
			char ltm[256]; struct tm sltm; ::localtime_r (&(inGEDHRcd->ltv.tv_sec), &sltm);
			char rtm[256]; struct tm srtm; ::localtime_r (&(inGEDHRcd->rtv.tv_sec), &srtm);
			char mtm[256]; struct tm smtm; ::localtime_r (&(inGEDHRcd->mtv.tv_sec), &smtm);
			char ftm[256]; struct tm sftm; ::localtime_r (&(inGEDHRcd->ftv.tv_sec), &sftm);
			char atm[256]; struct tm satm; ::localtime_r (&(inGEDHRcd->atv.tv_sec), &satm);

			::strftime (otm, 256, "%Ec", &sotm);
			::strftime (ltm, 256, "%Ec", &sltm);
			::strftime (rtm, 256, "%Ec", &srtm);
			::strftime (mtm, 256, "%Ec", &smtm);
			::strftime (ftm, 256, "%Ec", &sftm);
			::strftime (atm, 256, "%Ec", &satm);

			xmlNodePtr newXmlNodePtrS = ::xmlNewChild (newXmlNodePtr, NULL, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP, NULL);

			xmlNodePtr newXmlNodePtrO = ::xmlNewChild (newXmlNodePtrS, NULL, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_ORIGINAL,    (xmlChar*)(CString(otm)-"\n").Get());
			xmlNodePtr newXmlNodePtrL = ::xmlNewChild (newXmlNodePtrS, NULL, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_LAST,  	    (xmlChar*)(CString(ltm)-"\n").Get());
			xmlNodePtr newXmlNodePtrR = ::xmlNewChild (newXmlNodePtrS, NULL, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_RECEPTION,   (xmlChar*)(CString(rtm)-"\n").Get());
			xmlNodePtr newXmlNodePtrM = ::xmlNewChild (newXmlNodePtrS, NULL, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_META,   	    (xmlChar*)(CString(mtm)-"\n").Get());
			xmlNodePtr newXmlNodePtrF = ::xmlNewChild (newXmlNodePtrS, NULL, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_FIRST,   	    (xmlChar*)(CString(ftm)-"\n").Get());
			xmlNodePtr newXmlNodePtrA = ::xmlNewChild (newXmlNodePtrS, NULL, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_ACKNOWLEGDE, (xmlChar*)(CString(atm)-"\n").Get());

			::xmlNewProp (newXmlNodePtrO, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_SEC_ATTR,   (xmlChar*)CString((unsigned long)inGEDHRcd->otv.tv_sec ).Get());
			::xmlNewProp (newXmlNodePtrO, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_USEC_ATTR,  (xmlChar*)CString((unsigned long)inGEDHRcd->otv.tv_usec).Get());
			::xmlNewProp (newXmlNodePtrL, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_SEC_ATTR,   (xmlChar*)CString((unsigned long)inGEDHRcd->ltv.tv_sec ).Get());
			::xmlNewProp (newXmlNodePtrL, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_USEC_ATTR,  (xmlChar*)CString((unsigned long)inGEDHRcd->ltv.tv_usec).Get());
			::xmlNewProp (newXmlNodePtrR, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_SEC_ATTR,   (xmlChar*)CString((unsigned long)inGEDHRcd->rtv.tv_sec ).Get());
			::xmlNewProp (newXmlNodePtrR, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_USEC_ATTR,  (xmlChar*)CString((unsigned long)inGEDHRcd->rtv.tv_usec).Get());
			::xmlNewProp (newXmlNodePtrM, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_SEC_ATTR,   (xmlChar*)CString((unsigned long)inGEDHRcd->mtv.tv_sec ).Get());
			::xmlNewProp (newXmlNodePtrM, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_USEC_ATTR,  (xmlChar*)CString((unsigned long)inGEDHRcd->mtv.tv_usec).Get());
			::xmlNewProp (newXmlNodePtrF, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_SEC_ATTR,   (xmlChar*)CString((unsigned long)inGEDHRcd->ftv.tv_sec ).Get());
			::xmlNewProp (newXmlNodePtrF, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_USEC_ATTR,  (xmlChar*)CString((unsigned long)inGEDHRcd->ftv.tv_usec).Get());
			::xmlNewProp (newXmlNodePtrA, XML_ELEMENT_NODE_GED_RECORD_DURATION_ATTR,  	(xmlChar*)CString((unsigned long)inGEDHRcd->atv.tv_sec-inGEDHRcd->otv.tv_sec).Get());
			::xmlNewProp (newXmlNodePtrA, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_SEC_ATTR,   (xmlChar*)CString((unsigned long)inGEDHRcd->atv.tv_sec ).Get());
			::xmlNewProp (newXmlNodePtrA, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_USEC_ATTR,  (xmlChar*)CString((unsigned long)inGEDHRcd->atv.tv_usec).Get());

			for (size_t i=0; i<inGEDPktCfgs.GetLength() && inGEDPktCfg == NULL; i++)
				if (inGEDPktCfgs[i]->type == inGEDHRcd->typ) 
					inGEDPktCfg = inGEDPktCfgs[i];

			if (inGEDHRcd->len > 0)
				inChunk = new CChunk (inGEDHRcd->data, inGEDHRcd->len, ::GEDPktCfgToTData(inGEDPktCfg), false);
		}
		break;

		case GED_PKT_REQ_BKD_SYNC :
		{
			newXmlNodePtr = ::xmlNewChild (inXmlNodePtr, NULL, XML_ELEMENT_NODE_GED_RECORD, NULL);

			TGEDSRcd *inGEDSRcd (static_cast <TGEDSRcd *> (inGEDRcd));

                        TGEDPktIn *inGEDPktIn (inGEDSRcd->pkt);

			::xmlNewProp (newXmlNodePtr, XML_ELEMENT_NODE_GED_RECORD_QUEUE_ATTR, XML_ELEMENT_NODE_GED_RECORD_QUEUE_ATTR_SYNC);
			::xmlNewProp (newXmlNodePtr, XML_ELEMENT_NODE_GED_RECORD_TYPE_ATTR, (xmlChar*)CString(inGEDPktIn->typ).Get());

			::xmlNewProp (::xmlNewChild (newXmlNodePtr, NULL, XML_ELEMENT_NODE_GED_RECORD_SOURCE, (xmlChar*)CString(::inet_ntoa(inGEDPktIn->addr)).Get()), 
				      XML_ELEMENT_NODE_GED_RECORD_SOURCE_ATTR_RLY,  (xmlChar*)CString((inGEDPktIn->req&GED_PKT_REQ_SRC_RELAY)?"true":"false").Get());

			::xmlNewChild (newXmlNodePtr, NULL, XML_ELEMENT_NODE_GED_RECORD_TARGET, (xmlChar*)CString(::inet_ntoa(inGEDSRcd->tgt)).Get());

			char tm[256]; struct tm stm; ::localtime_r (&(inGEDPktIn->tv.tv_sec), &stm);

			::strftime (tm, 256, "%Ec", &stm);

			xmlNodePtr newXmlNodePtrS = ::xmlNewChild (newXmlNodePtr, NULL, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP, NULL);

			xmlNodePtr newXmlNodePtrO = ::xmlNewChild (newXmlNodePtrS, NULL, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_ORIGINAL, (xmlChar*)(CString(tm)-"\n").Get());

			::xmlNewProp (newXmlNodePtrO, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_SEC_ATTR,  (xmlChar*)CString((unsigned long)inGEDPktIn->tv.tv_sec).Get());
			::xmlNewProp (newXmlNodePtrO, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_USEC_ATTR, (xmlChar*)CString((unsigned long)inGEDPktIn->tv.tv_usec).Get());

                        switch (inGEDPktIn->req&GED_PKT_REQ_MASK)
                        {
                                case GED_PKT_REQ_PUSH : 
				{
					switch (inGEDPktIn->req&GED_PKT_REQ_PUSH_MASK)
					{
						case GED_PKT_REQ_PUSH_TMSP :
						{
							::xmlNewProp (newXmlNodePtr, XML_ELEMENT_NODE_GED_RECORD_REQ_ATTR, XML_ELEMENT_NODE_GED_RECORD_REQ_ATTR_PUSH); break;
						}
						break;
						case GED_PKT_REQ_PUSH_NOTMSP :
						{
							::xmlNewProp (newXmlNodePtr, XML_ELEMENT_NODE_GED_RECORD_REQ_ATTR, XML_ELEMENT_NODE_GED_RECORD_REQ_ATTR_UPDATE); break;
						}
						break;
					}
				}
				break;
                                case GED_PKT_REQ_DROP : ::xmlNewProp (newXmlNodePtr, XML_ELEMENT_NODE_GED_RECORD_REQ_ATTR, XML_ELEMENT_NODE_GED_RECORD_REQ_ATTR_DROP); break;
                        }

                        switch (inGEDPktIn->req&GED_PKT_REQ_BKD_MASK)
                        {
                                case GED_PKT_REQ_BKD_ACTIVE  : ::xmlNewProp (newXmlNodePtr, XML_ELEMENT_NODE_GED_RECORD_BKD_ATTR, XML_ELEMENT_NODE_GED_RECORD_QUEUE_ATTR_ACTIVE);  break;
                                case GED_PKT_REQ_BKD_HISTORY : ::xmlNewProp (newXmlNodePtr, XML_ELEMENT_NODE_GED_RECORD_BKD_ATTR, XML_ELEMENT_NODE_GED_RECORD_QUEUE_ATTR_HISTORY); break;
                                case GED_PKT_REQ_BKD_SYNC    : ::xmlNewProp (newXmlNodePtr, XML_ELEMENT_NODE_GED_RECORD_BKD_ATTR, XML_ELEMENT_NODE_GED_RECORD_QUEUE_ATTR_SYNC);    break;
                        }

                        for (size_t i=0; i<inGEDPktCfgs.GetLength() && inGEDPktCfg == NULL; i++)
                                if (inGEDPktCfgs[i]->type == inGEDPktIn->typ)
                                        inGEDPktCfg = inGEDPktCfgs[i];

                        if (inGEDPktIn->len > 0)
                                inChunk = new CChunk (inGEDPktIn->data, inGEDPktIn->len, ::GEDPktCfgToTData(inGEDPktCfg), false);
		}
		break;
	}

	if (inGEDPktCfg == NULL) return true;

	::xmlNewProp (newXmlNodePtr, XML_ELEMENT_NODE_GED_RECORD_NAME_ATTR, (xmlChar*)inGEDPktCfg->name.Get());
	::xmlNewProp (newXmlNodePtr, XML_ELEMENT_NODE_GED_RECORD_HASH_ATTR, (xmlChar*)inGEDPktCfg->hash.Get());

	xmlNodePtr xmlNodePtrContent = ::xmlNewChild (newXmlNodePtr, NULL, XML_ELEMENT_NODE_GED_RECORD_CONTENT, NULL);

	if (inChunk != NULL)
	{
		for (size_t i=0; i<inGEDPktCfg->fields.GetLength(); i++)
		{
			xmlNodePtr xmlNodePtrChild = NULL;

			switch (inChunk->NextDataIs())
			{
				case DATA_SINT32 :
				{
					SInt32 inSint32=0L;
					if (inChunk->ReadSInt32(inSint32)) 
						xmlNodePtrChild = ::xmlNewChild (xmlNodePtrContent, NULL, (xmlChar*)inGEDPktCfg->fields[i]->name.Get(), (xmlChar*)CString(inSint32).Get());
					else
						xmlNodePtrChild = ::xmlNewChild (xmlNodePtrContent, NULL, (xmlChar*)inGEDPktCfg->fields[i]->name.Get(), (xmlChar*)NULL);  
				}
				break;
				case DATA_STRING :
				{
					SInt8 *inSint8=NULL; 
					*inChunk >> inSint8;
					if (CString(inSint8).GetLength() > 0) 
						xmlNodePtrChild = ::xmlNewChild (xmlNodePtrContent, NULL, (xmlChar*)inGEDPktCfg->fields[i]->name.Get(), (xmlChar*)inSint8);
					else
						xmlNodePtrChild = ::xmlNewChild (xmlNodePtrContent, NULL, (xmlChar*)inGEDPktCfg->fields[i]->name.Get(), NULL);
					if (inSint8 != NULL) delete [] inSint8;
				}
				break;
				case DATA_FLOAT64 :
				{
					Float64 inFloat64=0.;
					if (inChunk->ReadFloat64(inFloat64))
						xmlNodePtrChild = ::xmlNewChild (xmlNodePtrContent, NULL, (xmlChar*)inGEDPktCfg->fields[i]->name.Get(), (xmlChar*)CString(inFloat64).Get());
					else
						xmlNodePtrChild = ::xmlNewChild (xmlNodePtrContent, NULL, (xmlChar*)inGEDPktCfg->fields[i]->name.Get(), (xmlChar*)NULL);
				}
				break;
			}

			if (xmlNodePtrChild != NULL) 
			{
				::xmlNewProp (xmlNodePtrChild, XML_ELEMENT_NODE_GED_RECORD_CONTENT_KEY_ATTR, (xmlChar*)(inGEDPktCfg->keyidc.Find(i) ? "true" : "false"));
				::xmlNewProp (xmlNodePtrChild, XML_ELEMENT_NODE_GED_RECORD_CONTENT_META_ATTR, (xmlChar*)(inGEDPktCfg->fields[i]->meta ? "true" : "false"));
			}
		}

		delete inChunk;
	}
	else
		for (size_t i=inGEDPktCfg->fields.GetLength(), j=0; i>0; i--, j++)
		{
			xmlNodePtr xmlNodePtrChild = ::xmlNewChild (xmlNodePtrContent, NULL, (xmlChar*)inGEDPktCfg->fields[j]->name.Get(), NULL);
			::xmlNewProp (xmlNodePtrChild, XML_ELEMENT_NODE_GED_RECORD_CONTENT_KEY_ATTR, (xmlChar*)(inGEDPktCfg->keyidc.Find(j) ? "true" : "false"));
			::xmlNewProp (xmlNodePtrChild, XML_ELEMENT_NODE_GED_RECORD_CONTENT_META_ATTR, (xmlChar*)(inGEDPktCfg->fields[j]->meta ? "true" : "false"));
		}

	return true;
}

//---------------------------------------------------------------------------------------------------------------------------------------
// record to xml node (ged)
//---------------------------------------------------------------------------------------------------------------------------------------
bool GEDRcdToXmlNodeLight (TGEDRcd *inGEDRcd, const TBuffer <TGEDPktCfg> &inGEDPktCfgs, xmlNodePtr &inXmlNodePtr)
{
	if (inGEDRcd == NULL) return false;

	TGEDPktCfg *inGEDPktCfg = NULL; CChunk *inChunk = NULL;  xmlNodePtr newXmlNodePtr = NULL;

	switch (inGEDRcd->queue&GED_PKT_REQ_BKD_MASK)
	{
		case GED_PKT_REQ_BKD_STAT :
		{
			TGEDStatRcd *inGEDStatRcd (static_cast <TGEDStatRcd *> (inGEDRcd));

			::xmlNewProp (inXmlNodePtr, XML_ELEMENT_NODE_GED_RECORD_STAT_NTA_ATTR_LIGHT, (xmlChar*)CString(inGEDStatRcd->nta).Get());
			::xmlNewProp (inXmlNodePtr, XML_ELEMENT_NODE_GED_RECORD_STAT_NTS_ATTR_LIGHT, (xmlChar*)CString(inGEDStatRcd->nts).Get());
			::xmlNewProp (inXmlNodePtr, XML_ELEMENT_NODE_GED_RECORD_STAT_NTH_ATTR_LIGHT, (xmlChar*)CString(inGEDStatRcd->nth).Get());
			::xmlNewProp (inXmlNodePtr, XML_ELEMENT_NODE_GED_RECORD_STAT_NTR_ATTR_LIGHT, (xmlChar*)CString(inGEDStatRcd->ntr).Get());
		}
		break;

		case GED_PKT_REQ_BKD_ACTIVE :
		{
			newXmlNodePtr = ::xmlNewChild (inXmlNodePtr, NULL, XML_ELEMENT_NODE_GED_RECORD_LIGHT, NULL);

			TGEDARcd *inGEDARcd (static_cast <TGEDARcd *> (inGEDRcd));

			::xmlNewProp (newXmlNodePtr, XML_ELEMENT_NODE_GED_RECORD_QUEUE_ATTR_LIGHT, XML_ELEMENT_NODE_GED_RECORD_QUEUE_ATTR_ACTIVE_LIGHT);
			::xmlNewProp (newXmlNodePtr, XML_ELEMENT_NODE_GED_RECORD_TYPE_ATTR_LIGHT, (xmlChar*)CString(inGEDARcd->typ).Get());
			::xmlNewProp (newXmlNodePtr, XML_ELEMENT_NODE_GED_RECORD_OCC_ATTR_LIGHT,  (xmlChar*)CString(inGEDARcd->occ).Get());

			xmlNodePtr srcXmlNodePtr = ::xmlNewChild (newXmlNodePtr, NULL, XML_ELEMENT_NODE_GED_RECORD_SOURCES_LIGHT, NULL);

			for (size_t i=0; i<inGEDARcd->nsrc; i++)
			{
				xmlNodePtr sXmlNodePtr = ::xmlNewChild (srcXmlNodePtr, NULL, XML_ELEMENT_NODE_GED_RECORD_SOURCE_LIGHT, (xmlChar*)CString(::inet_ntoa(inGEDARcd->src[i].addr)).Get());
				::xmlNewProp (sXmlNodePtr, XML_ELEMENT_NODE_GED_RECORD_SOURCE_ATTR_RLY_LIGHT,  (xmlChar*)CString(inGEDARcd->src[i].rly?"1":"0").Get());
			}

			xmlNodePtr newXmlNodePtrS = ::xmlNewChild (newXmlNodePtr, NULL, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_LIGHT, NULL);

			xmlNodePtr newXmlNodePtrO = ::xmlNewChild (newXmlNodePtrS, NULL, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_ORIGINAL_LIGHT,  NULL);
			xmlNodePtr newXmlNodePtrL = ::xmlNewChild (newXmlNodePtrS, NULL, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_LAST_LIGHT,  	NULL);
			xmlNodePtr newXmlNodePtrR = ::xmlNewChild (newXmlNodePtrS, NULL, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_RECEPTION_LIGHT, NULL);
			xmlNodePtr newXmlNodePtrM = ::xmlNewChild (newXmlNodePtrS, NULL, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_META_LIGHT, 	NULL);
			xmlNodePtr newXmlNodePtrF = ::xmlNewChild (newXmlNodePtrS, NULL, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_FIRST_LIGHT, 	NULL);

			::xmlNewProp (newXmlNodePtrO, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_SEC_ATTR_LIGHT,   (xmlChar*)CString((unsigned long)inGEDARcd->otv.tv_sec ).Get());
			::xmlNewProp (newXmlNodePtrO, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_USEC_ATTR_LIGHT,  (xmlChar*)CString((unsigned long)inGEDARcd->otv.tv_usec).Get());
			::xmlNewProp (newXmlNodePtrL, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_SEC_ATTR_LIGHT,   (xmlChar*)CString((unsigned long)inGEDARcd->ltv.tv_sec ).Get());
			::xmlNewProp (newXmlNodePtrL, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_USEC_ATTR_LIGHT,  (xmlChar*)CString((unsigned long)inGEDARcd->ltv.tv_usec).Get());
			::xmlNewProp (newXmlNodePtrM, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_SEC_ATTR_LIGHT,   (xmlChar*)CString((unsigned long)inGEDARcd->mtv.tv_sec ).Get());
			::xmlNewProp (newXmlNodePtrM, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_USEC_ATTR_LIGHT,  (xmlChar*)CString((unsigned long)inGEDARcd->mtv.tv_usec).Get());
			::xmlNewProp (newXmlNodePtrR, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_SEC_ATTR_LIGHT,   (xmlChar*)CString((unsigned long)inGEDARcd->rtv.tv_sec ).Get());
			::xmlNewProp (newXmlNodePtrR, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_USEC_ATTR_LIGHT,  (xmlChar*)CString((unsigned long)inGEDARcd->rtv.tv_usec).Get());
			::xmlNewProp (newXmlNodePtrF, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_SEC_ATTR_LIGHT,   (xmlChar*)CString((unsigned long)inGEDARcd->ftv.tv_sec ).Get());
			::xmlNewProp (newXmlNodePtrF, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_USEC_ATTR_LIGHT,  (xmlChar*)CString((unsigned long)inGEDARcd->ftv.tv_usec).Get());

			for (size_t i=0; i<inGEDPktCfgs.GetLength() && inGEDPktCfg == NULL; i++)
				if (inGEDPktCfgs[i]->type == inGEDARcd->typ) 
					inGEDPktCfg = inGEDPktCfgs[i];

			if (inGEDARcd->len > 0)
				inChunk = new CChunk (inGEDARcd->data, inGEDARcd->len, ::GEDPktCfgToTData(inGEDPktCfg), false);
		}
		break;

		case GED_PKT_REQ_BKD_HISTORY :
		{
			newXmlNodePtr = ::xmlNewChild (inXmlNodePtr, NULL, XML_ELEMENT_NODE_GED_RECORD_LIGHT, NULL);

			TGEDHRcd *inGEDHRcd (static_cast <TGEDHRcd *> (inGEDRcd));

			::xmlNewProp (newXmlNodePtr, XML_ELEMENT_NODE_GED_RECORD_QUEUE_ATTR_LIGHT, XML_ELEMENT_NODE_GED_RECORD_QUEUE_ATTR_HISTORY_LIGHT);
			::xmlNewProp (newXmlNodePtr, XML_ELEMENT_NODE_GED_RECORD_TYPE_ATTR_LIGHT, (xmlChar*)CString(inGEDHRcd->typ).Get());
			::xmlNewProp (newXmlNodePtr, XML_ELEMENT_NODE_GED_RECORD_OCC_ATTR_LIGHT,  (xmlChar*)CString(inGEDHRcd->occ).Get());
			::xmlNewProp (newXmlNodePtr, XML_ELEMENT_NODE_GED_RECORD_ID_ATTR_LIGHT,   (xmlChar*)CString(inGEDHRcd->hid).Get());

			if ((inGEDHRcd->queue & GED_PKT_REQ_BKD_HST_MASK) == GED_PKT_REQ_BKD_HST_TTL)
				::xmlNewProp (newXmlNodePtr, XML_ELEMENT_NODE_GED_RECORD_BKD_HST_ATTR_RSN_LIGHT, XML_ELEMENT_NODE_GED_RECORD_BKD_HST_ATTR_RSN_TTL_LIGHT);
			else
				::xmlNewProp (newXmlNodePtr, XML_ELEMENT_NODE_GED_RECORD_BKD_HST_ATTR_RSN_LIGHT, XML_ELEMENT_NODE_GED_RECORD_BKD_HST_ATTR_RSN_PKT_LIGHT);

			xmlNodePtr srcXmlNodePtr = ::xmlNewChild (newXmlNodePtr, NULL, XML_ELEMENT_NODE_GED_RECORD_SOURCES_LIGHT, NULL);

			for (size_t i=0; i<inGEDHRcd->nsrc; i++)
			{
				xmlNodePtr sXmlNodePtr = ::xmlNewChild (srcXmlNodePtr, NULL, XML_ELEMENT_NODE_GED_RECORD_SOURCE_LIGHT, (xmlChar*)CString(::inet_ntoa(inGEDHRcd->src[i].addr)).Get());
				::xmlNewProp (sXmlNodePtr, XML_ELEMENT_NODE_GED_RECORD_SOURCE_ATTR_RLY_LIGHT,  (xmlChar*)CString(inGEDHRcd->src[i].rly?"1":"0").Get());
			}

			xmlNodePtr newXmlNodePtrS = ::xmlNewChild (newXmlNodePtr, NULL, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_LIGHT, NULL);

			xmlNodePtr newXmlNodePtrO = ::xmlNewChild (newXmlNodePtrS, NULL, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_ORIGINAL_LIGHT,    NULL);
			xmlNodePtr newXmlNodePtrL = ::xmlNewChild (newXmlNodePtrS, NULL, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_LAST_LIGHT,  	  NULL);
			xmlNodePtr newXmlNodePtrR = ::xmlNewChild (newXmlNodePtrS, NULL, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_RECEPTION_LIGHT,   NULL);
			xmlNodePtr newXmlNodePtrM = ::xmlNewChild (newXmlNodePtrS, NULL, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_META_LIGHT,   	  NULL);
			xmlNodePtr newXmlNodePtrF = ::xmlNewChild (newXmlNodePtrS, NULL, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_FIRST_LIGHT,   	  NULL);
			xmlNodePtr newXmlNodePtrA = ::xmlNewChild (newXmlNodePtrS, NULL, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_ACKNOWLEGDE_LIGHT, NULL);

			::xmlNewProp (newXmlNodePtrO, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_SEC_ATTR_LIGHT,   (xmlChar*)CString((unsigned long)inGEDHRcd->otv.tv_sec ).Get());
			::xmlNewProp (newXmlNodePtrO, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_USEC_ATTR_LIGHT,  (xmlChar*)CString((unsigned long)inGEDHRcd->otv.tv_usec).Get());
			::xmlNewProp (newXmlNodePtrL, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_SEC_ATTR_LIGHT,   (xmlChar*)CString((unsigned long)inGEDHRcd->ltv.tv_sec ).Get());
			::xmlNewProp (newXmlNodePtrL, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_USEC_ATTR_LIGHT,  (xmlChar*)CString((unsigned long)inGEDHRcd->ltv.tv_usec).Get());
			::xmlNewProp (newXmlNodePtrR, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_SEC_ATTR_LIGHT,   (xmlChar*)CString((unsigned long)inGEDHRcd->rtv.tv_sec ).Get());
			::xmlNewProp (newXmlNodePtrR, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_USEC_ATTR_LIGHT,  (xmlChar*)CString((unsigned long)inGEDHRcd->rtv.tv_usec).Get());
			::xmlNewProp (newXmlNodePtrM, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_SEC_ATTR_LIGHT,   (xmlChar*)CString((unsigned long)inGEDHRcd->mtv.tv_sec ).Get());
			::xmlNewProp (newXmlNodePtrM, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_USEC_ATTR_LIGHT,  (xmlChar*)CString((unsigned long)inGEDHRcd->mtv.tv_usec).Get());
			::xmlNewProp (newXmlNodePtrF, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_SEC_ATTR_LIGHT,   (xmlChar*)CString((unsigned long)inGEDHRcd->ftv.tv_sec ).Get());
			::xmlNewProp (newXmlNodePtrF, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_USEC_ATTR_LIGHT,  (xmlChar*)CString((unsigned long)inGEDHRcd->ftv.tv_usec).Get());
			::xmlNewProp (newXmlNodePtrA, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_SEC_ATTR_LIGHT,   (xmlChar*)CString((unsigned long)inGEDHRcd->atv.tv_sec ).Get());
			::xmlNewProp (newXmlNodePtrA, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_USEC_ATTR_LIGHT,  (xmlChar*)CString((unsigned long)inGEDHRcd->atv.tv_usec).Get());

			for (size_t i=0; i<inGEDPktCfgs.GetLength() && inGEDPktCfg == NULL; i++)
				if (inGEDPktCfgs[i]->type == inGEDHRcd->typ) 
					inGEDPktCfg = inGEDPktCfgs[i];

			if (inGEDHRcd->len > 0)
				inChunk = new CChunk (inGEDHRcd->data, inGEDHRcd->len, ::GEDPktCfgToTData(inGEDPktCfg), false);
		}
		break;

		case GED_PKT_REQ_BKD_SYNC :
		{
			newXmlNodePtr = ::xmlNewChild (inXmlNodePtr, NULL, XML_ELEMENT_NODE_GED_RECORD_LIGHT, NULL);

			TGEDSRcd *inGEDSRcd (static_cast <TGEDSRcd *> (inGEDRcd));

                        TGEDPktIn *inGEDPktIn (inGEDSRcd->pkt);

			::xmlNewProp (newXmlNodePtr, XML_ELEMENT_NODE_GED_RECORD_QUEUE_ATTR_LIGHT, XML_ELEMENT_NODE_GED_RECORD_QUEUE_ATTR_SYNC_LIGHT);
			::xmlNewProp (newXmlNodePtr, XML_ELEMENT_NODE_GED_RECORD_TYPE_ATTR_LIGHT, (xmlChar*)CString(inGEDPktIn->typ).Get());

			::xmlNewProp (::xmlNewChild (newXmlNodePtr, NULL, XML_ELEMENT_NODE_GED_RECORD_SOURCE_LIGHT, (xmlChar*)CString(::inet_ntoa(inGEDPktIn->addr)).Get()), 
				      XML_ELEMENT_NODE_GED_RECORD_SOURCE_ATTR_RLY_LIGHT,  (xmlChar*)CString((inGEDPktIn->req&GED_PKT_REQ_SRC_RELAY)?"1":"0").Get());

			::xmlNewChild (newXmlNodePtr, NULL, XML_ELEMENT_NODE_GED_RECORD_TARGET_LIGHT, (xmlChar*)CString(::inet_ntoa(inGEDSRcd->tgt)).Get());

			xmlNodePtr newXmlNodePtrS = ::xmlNewChild (newXmlNodePtr, NULL, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_LIGHT, NULL);

			xmlNodePtr newXmlNodePtrO = ::xmlNewChild (newXmlNodePtrS, NULL, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_ORIGINAL_LIGHT, NULL);

			::xmlNewProp (newXmlNodePtrO, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_SEC_ATTR_LIGHT,  (xmlChar*)CString((unsigned long)inGEDPktIn->tv.tv_sec).Get());
			::xmlNewProp (newXmlNodePtrO, XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_USEC_ATTR_LIGHT, (xmlChar*)CString((unsigned long)inGEDPktIn->tv.tv_usec).Get());

                        switch (inGEDPktIn->req&GED_PKT_REQ_MASK)
                        {
                                case GED_PKT_REQ_PUSH : 
				{
					switch (inGEDPktIn->req&GED_PKT_REQ_PUSH_MASK)
					{
						case GED_PKT_REQ_PUSH_TMSP :
						{
							::xmlNewProp (newXmlNodePtr, XML_ELEMENT_NODE_GED_RECORD_REQ_ATTR_LIGHT, XML_ELEMENT_NODE_GED_RECORD_REQ_ATTR_PUSH_LIGHT); break;
						}
						break;
						case GED_PKT_REQ_PUSH_NOTMSP :
						{
							::xmlNewProp (newXmlNodePtr, XML_ELEMENT_NODE_GED_RECORD_REQ_ATTR_LIGHT, XML_ELEMENT_NODE_GED_RECORD_REQ_ATTR_UPDATE_LIGHT); break;
						}
						break;
					}
				}
				break;
                                case GED_PKT_REQ_DROP : ::xmlNewProp (newXmlNodePtr, XML_ELEMENT_NODE_GED_RECORD_REQ_ATTR_LIGHT, XML_ELEMENT_NODE_GED_RECORD_REQ_ATTR_DROP_LIGHT); break;
                        }

                        switch (inGEDPktIn->req&GED_PKT_REQ_BKD_MASK)
                        {
                                case GED_PKT_REQ_BKD_ACTIVE  : ::xmlNewProp (newXmlNodePtr, XML_ELEMENT_NODE_GED_RECORD_BKD_ATTR_LIGHT, XML_ELEMENT_NODE_GED_RECORD_QUEUE_ATTR_ACTIVE_LIGHT);  break;
                                case GED_PKT_REQ_BKD_HISTORY : ::xmlNewProp (newXmlNodePtr, XML_ELEMENT_NODE_GED_RECORD_BKD_ATTR_LIGHT, XML_ELEMENT_NODE_GED_RECORD_QUEUE_ATTR_HISTORY_LIGHT); break;
                                case GED_PKT_REQ_BKD_SYNC    : ::xmlNewProp (newXmlNodePtr, XML_ELEMENT_NODE_GED_RECORD_BKD_ATTR_LIGHT, XML_ELEMENT_NODE_GED_RECORD_QUEUE_ATTR_SYNC_LIGHT);    break;
                        }

                        for (size_t i=0; i<inGEDPktCfgs.GetLength() && inGEDPktCfg == NULL; i++)
                                if (inGEDPktCfgs[i]->type == inGEDPktIn->typ)
                                        inGEDPktCfg = inGEDPktCfgs[i];

                        if (inGEDPktIn->len > 0)
                                inChunk = new CChunk (inGEDPktIn->data, inGEDPktIn->len, ::GEDPktCfgToTData(inGEDPktCfg), false);
		}
		break;
	}

	if (inGEDPktCfg == NULL) return true;

	::xmlNewProp (newXmlNodePtr, XML_ELEMENT_NODE_GED_RECORD_HASH_ATTR_LIGHT, (xmlChar*)inGEDPktCfg->hash.Get());

	xmlNodePtr xmlNodePtrContent = ::xmlNewChild (newXmlNodePtr, NULL, XML_ELEMENT_NODE_GED_RECORD_CONTENT_LIGHT, NULL);

	if (inChunk != NULL)
	{
		for (size_t i=0; i<inGEDPktCfg->fields.GetLength(); i++)
		{
			xmlNodePtr xmlNodePtrChild = NULL;

			switch (inChunk->NextDataIs())
			{
				case DATA_SINT32 :
				{
					SInt32 inSint32=0L;
					if (inChunk->ReadSInt32(inSint32)) 
						xmlNodePtrChild = ::xmlNewChild (xmlNodePtrContent, NULL, (xmlChar*)inGEDPktCfg->fields[i]->name.Get(), (xmlChar*)CString(inSint32).Get());
					else
						xmlNodePtrChild = ::xmlNewChild (xmlNodePtrContent, NULL, (xmlChar*)inGEDPktCfg->fields[i]->name.Get(), (xmlChar*)NULL);  
				}
				break;
				case DATA_STRING :
				{
					SInt8 *inSint8=NULL; 
					*inChunk >> inSint8;
					if (CString(inSint8).GetLength() > 0) 
						xmlNodePtrChild = ::xmlNewChild (xmlNodePtrContent, NULL, (xmlChar*)inGEDPktCfg->fields[i]->name.Get(), (xmlChar*)inSint8);
					else
						xmlNodePtrChild = ::xmlNewChild (xmlNodePtrContent, NULL, (xmlChar*)inGEDPktCfg->fields[i]->name.Get(), NULL);
					if (inSint8 != NULL) delete [] inSint8;
				}
				break;
				case DATA_FLOAT64 :
				{
					Float64 inFloat64=0.;
					if (inChunk->ReadFloat64(inFloat64))
						xmlNodePtrChild = ::xmlNewChild (xmlNodePtrContent, NULL, (xmlChar*)inGEDPktCfg->fields[i]->name.Get(), (xmlChar*)CString(inFloat64).Get());
					else
						xmlNodePtrChild = ::xmlNewChild (xmlNodePtrContent, NULL, (xmlChar*)inGEDPktCfg->fields[i]->name.Get(), (xmlChar*)NULL);
				}
				break;
			}
		}

		delete inChunk;
	}
	else
		for (size_t i=inGEDPktCfg->fields.GetLength(), j=0; i>0; i--, j++)
		{
			xmlNodePtr xmlNodePtrChild = ::xmlNewChild (xmlNodePtrContent, NULL, (xmlChar*)inGEDPktCfg->fields[j]->name.Get(), NULL);
		}

	return true;
}

//---------------------------------------------------------------------------------------------------------------------------------------
// xml to record (ged)
//---------------------------------------------------------------------------------------------------------------------------------------
TGEDRcd * GEDXmlNodeToRcd (const TBuffer <TGEDPktCfg> &inGEDPktCfgs, xmlNodePtr inXmlNodePtr, const bool inCheckHash)
{
	TGEDRcd *outGEDRcd = NULL; long outType = 0L;

	if (inXmlNodePtr->type != XML_ELEMENT_NODE || inXmlNodePtr->children == NULL) return NULL;

	if (CString((char*)inXmlNodePtr->name) != CString((char*)XML_ELEMENT_NODE_GED_RECORD) && 
	    CString((char*)inXmlNodePtr->name) != CString((char*)XML_ELEMENT_NODE_GED_RECORD_LIGHT)) 
		return NULL;

	for (xmlAttrPtr curXMLAttrPtr=inXmlNodePtr->properties; curXMLAttrPtr; curXMLAttrPtr=curXMLAttrPtr->next)
	{
		if ((CString((char*)curXMLAttrPtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_QUEUE_ATTR) || 
		     CString((char*)curXMLAttrPtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_QUEUE_ATTR_LIGHT)) && curXMLAttrPtr->children != NULL)
		{
			if (CString((char*)curXMLAttrPtr->children->content) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_QUEUE_ATTR_ACTIVE) || 
			    CString((char*)curXMLAttrPtr->children->content) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_QUEUE_ATTR_ACTIVE_LIGHT))
			{
				outGEDRcd = new TGEDARcd; bzero (outGEDRcd, sizeof(TGEDARcd)); outGEDRcd->queue = GED_PKT_REQ_BKD_ACTIVE;
			}
			else if (CString((char*)curXMLAttrPtr->children->content) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_QUEUE_ATTR_HISTORY) ||
				 CString((char*)curXMLAttrPtr->children->content) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_QUEUE_ATTR_HISTORY_LIGHT))
			{
				outGEDRcd = new TGEDHRcd; bzero (outGEDRcd, sizeof(TGEDHRcd)); outGEDRcd->queue = GED_PKT_REQ_BKD_HISTORY;
			}
			else if (CString((char*)curXMLAttrPtr->children->content) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_QUEUE_ATTR_SYNC) ||
				 CString((char*)curXMLAttrPtr->children->content) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_QUEUE_ATTR_SYNC_LIGHT))
			{
				outGEDRcd = new TGEDSRcd; bzero (outGEDRcd, sizeof(TGEDSRcd)); outGEDRcd->queue = GED_PKT_REQ_BKD_SYNC;
			}
			else
			{
				return NULL;
			}
		}
		else if ((CString((char*)curXMLAttrPtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_TYPE_ATTR) || 
			  CString((char*)curXMLAttrPtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_TYPE_ATTR_LIGHT)) && curXMLAttrPtr->children != NULL)
		{
			outType = CString((char*)curXMLAttrPtr->children->content).ToLong();
		}
	}

	if (outGEDRcd == NULL) return NULL; if (outType == 0L)
	{
		DeleteGEDRcd (outGEDRcd); return NULL;
	}

	TGEDPktCfg *inGEDPktCfg = NULL; for (size_t i=0; i<inGEDPktCfgs.GetLength() && inGEDPktCfg == NULL; i++)
		if (inGEDPktCfgs[i]->type == outType)
			inGEDPktCfg = inGEDPktCfgs[i];

	if (inGEDPktCfg == NULL)
	{
		DeleteGEDRcd (outGEDRcd); return NULL;
	}

	switch (outGEDRcd->queue)
	{
		case GED_PKT_REQ_BKD_ACTIVE :
		{
			TGEDARcd *outGEDARcd = static_cast <TGEDARcd *> (outGEDRcd); outGEDARcd->typ = outType;

			for (xmlAttrPtr curXMLAttrPtr=inXmlNodePtr->properties; curXMLAttrPtr; curXMLAttrPtr=curXMLAttrPtr->next)
			{
				if ((CString((char*)curXMLAttrPtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_OCC_ATTR) || 
				     CString((char*)curXMLAttrPtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_OCC_ATTR_LIGHT)) && curXMLAttrPtr->children != NULL)
				{
					outGEDARcd->occ = CString((char*)curXMLAttrPtr->children->content).ToLong();
				}
				else if ((CString((char*)curXMLAttrPtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_HASH_ATTR) || 
					  CString((char*)curXMLAttrPtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_HASH_ATTR_LIGHT))&& curXMLAttrPtr->children != NULL)
				{
					if (inCheckHash && (CString((char*)curXMLAttrPtr->children->content) != inGEDPktCfg->hash))
					{
						DeleteGEDRcd (outGEDRcd); return NULL;
					}
				}
			}

			for (xmlNodePtr curXMLNodePtr=inXmlNodePtr->children; curXMLNodePtr; curXMLNodePtr=curXMLNodePtr->next)
			{
				if (curXMLNodePtr->type != XML_ELEMENT_NODE) continue;

				if ((CString((char*)curXMLNodePtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_SOURCES) ||
				     CString((char*)curXMLNodePtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_SOURCES_LIGHT)) && curXMLNodePtr->children != NULL)
				{
					TBuffer <TGEDRcdSrc> outGEDRcdSrcs;

					for (xmlNodePtr locXMLNodePtr=curXMLNodePtr->children; locXMLNodePtr; locXMLNodePtr=locXMLNodePtr->next)
					{
						if (locXMLNodePtr->type != XML_ELEMENT_NODE) continue;

						if ((CString((char*)locXMLNodePtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_SOURCE) || 
						     CString((char*)locXMLNodePtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_SOURCE_LIGHT)) && locXMLNodePtr->children != NULL)
						{
							TGEDRcdSrc outGEDRcdSrc;

							for (xmlAttrPtr curXMLAttrPtr=locXMLNodePtr->properties; curXMLAttrPtr; curXMLAttrPtr=curXMLAttrPtr->next)
							{
								if ((CString((char*)curXMLAttrPtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_SOURCE_ATTR_RLY) || 
								     CString((char*)curXMLAttrPtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_SOURCE_ATTR_RLY_LIGHT)) && 
									curXMLAttrPtr->children != NULL)
								{
									outGEDRcdSrc.rly = CString((char*)curXMLAttrPtr->children->content).ToBool();
								}
								else
								{
									DeleteGEDRcd (outGEDRcd); return NULL;
								}
							}

							GetStrAddrToAddrIn (CString((char*)locXMLNodePtr->children->content), &outGEDRcdSrc.addr); 

							outGEDRcdSrcs += outGEDRcdSrc;
						}
						else
						{
							DeleteGEDRcd (outGEDRcd); return NULL;
						}
					}

					outGEDARcd->nsrc = outGEDRcdSrcs.GetLength(); outGEDARcd->src = new TGEDRcdSrc [outGEDARcd->nsrc];

					for (size_t i=0; i<outGEDRcdSrcs.GetLength(); i++) memcpy (&outGEDARcd->src[i], outGEDRcdSrcs[i], sizeof(TGEDRcdSrc));
				}
				else if ((CString((char*)curXMLNodePtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP) || 
					  CString((char*)curXMLNodePtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_LIGHT)) && curXMLNodePtr->children != NULL)
				{
					for (xmlNodePtr locXMLNodePtr=curXMLNodePtr->children; locXMLNodePtr; locXMLNodePtr=locXMLNodePtr->next)
					{
						if (locXMLNodePtr->type != XML_ELEMENT_NODE) continue;

						if ((CString((char*)locXMLNodePtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_ORIGINAL) || 
						     CString((char*)locXMLNodePtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_ORIGINAL_LIGHT)))
						{
							for (xmlAttrPtr curXMLAttrPtr=locXMLNodePtr->properties; curXMLAttrPtr; curXMLAttrPtr=curXMLAttrPtr->next)
							{
								if ((CString((char*)curXMLAttrPtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_SEC_ATTR) || 
								     CString((char*)curXMLAttrPtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_SEC_ATTR_LIGHT)) && 
									curXMLAttrPtr->children != NULL)
								{
									outGEDARcd->otv.tv_sec = CString((char*)curXMLAttrPtr->children->content).ToULong();
								}
								else if ((CString((char*)curXMLAttrPtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_USEC_ATTR) || 
									  CString((char*)curXMLAttrPtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_USEC_ATTR_LIGHT)) &&
									curXMLAttrPtr->children != NULL)
								{
									outGEDARcd->otv.tv_usec = CString((char*)curXMLAttrPtr->children->content).ToULong(); 
								}
								else
								{
									DeleteGEDRcd (outGEDRcd); return NULL;
								}
							}
						}
						else if ((CString((char*)locXMLNodePtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_LAST) || 
							  CString((char*)locXMLNodePtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_LAST_LIGHT)))
						{
							for (xmlAttrPtr curXMLAttrPtr=locXMLNodePtr->properties; curXMLAttrPtr; curXMLAttrPtr=curXMLAttrPtr->next)
							{
								if ((CString((char*)curXMLAttrPtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_SEC_ATTR) || 
								     CString((char*)curXMLAttrPtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_SEC_ATTR_LIGHT)) && 
									curXMLAttrPtr->children != NULL)
								{
									outGEDARcd->ltv.tv_sec = CString((char*)curXMLAttrPtr->children->content).ToULong();
								}
								else if ((CString((char*)curXMLAttrPtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_USEC_ATTR) || 
									  CString((char*)curXMLAttrPtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_USEC_ATTR_LIGHT)) &&
									curXMLAttrPtr->children != NULL)
								{
									outGEDARcd->ltv.tv_usec = CString((char*)curXMLAttrPtr->children->content).ToULong(); 
								}
								else
								{
									DeleteGEDRcd (outGEDRcd); return NULL;
								}
							}
						}
						else if ((CString((char*)locXMLNodePtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_RECEPTION) || 
							  CString((char*)locXMLNodePtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_RECEPTION_LIGHT)))
						{
							for (xmlAttrPtr curXMLAttrPtr=locXMLNodePtr->properties; curXMLAttrPtr; curXMLAttrPtr=curXMLAttrPtr->next)
							{
								if ((CString((char*)curXMLAttrPtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_SEC_ATTR) || 
								     CString((char*)curXMLAttrPtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_SEC_ATTR_LIGHT)) && 
									curXMLAttrPtr->children != NULL)
								{
									outGEDARcd->rtv.tv_sec = CString((char*)curXMLAttrPtr->children->content).ToULong();
								}
								else if ((CString((char*)curXMLAttrPtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_USEC_ATTR) || 
									  CString((char*)curXMLAttrPtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_USEC_ATTR_LIGHT)) &&
									curXMLAttrPtr->children != NULL)
								{
									outGEDARcd->rtv.tv_usec = CString((char*)curXMLAttrPtr->children->content).ToULong(); 
								}
								else
								{
									DeleteGEDRcd (outGEDRcd); return NULL;
								}
							}
						}
						else if ((CString((char*)locXMLNodePtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_META) || 
							  CString((char*)locXMLNodePtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_META_LIGHT)))
						{
							for (xmlAttrPtr curXMLAttrPtr=locXMLNodePtr->properties; curXMLAttrPtr; curXMLAttrPtr=curXMLAttrPtr->next)
							{
								if ((CString((char*)curXMLAttrPtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_SEC_ATTR) || 
								     CString((char*)curXMLAttrPtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_SEC_ATTR_LIGHT)) && 
									curXMLAttrPtr->children != NULL)
								{
									outGEDARcd->mtv.tv_sec = CString((char*)curXMLAttrPtr->children->content).ToULong();
								}
								else if ((CString((char*)curXMLAttrPtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_USEC_ATTR) || 
									  CString((char*)curXMLAttrPtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_USEC_ATTR_LIGHT)) &&
									curXMLAttrPtr->children != NULL)
								{
									outGEDARcd->mtv.tv_usec = CString((char*)curXMLAttrPtr->children->content).ToULong(); 
								}
								else
								{
									DeleteGEDRcd (outGEDRcd); return NULL;
								}
							}
						}
						else
						{
							DeleteGEDRcd (outGEDRcd); return NULL;
						}
					}
				}
				else if ((CString((char*)curXMLNodePtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_CONTENT) || 
					  CString((char*)curXMLNodePtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_CONTENT_LIGHT)) && curXMLNodePtr->children != NULL)
				{
					CChunk outChunk;

					for (size_t i=0; i<inGEDPktCfg->fields.GetLength(); i++)
					{
						bool found=false; for (xmlNodePtr locXMLNodePtr=curXMLNodePtr->children; locXMLNodePtr && !found; locXMLNodePtr=locXMLNodePtr->next)
						{
							if (locXMLNodePtr->type != XML_ELEMENT_NODE) continue;

							if (CString((char*)locXMLNodePtr->name) == inGEDPktCfg->fields[i]->name)
							{
								switch (inGEDPktCfg->fields[i]->type)
								{
									case DATA_SINT32 :
									{	
										if (locXMLNodePtr->children != NULL)
											outChunk << (SInt32) CString((char*)locXMLNodePtr->children->content).ToLong();
										else
											outChunk << 0L;
									}
									break;
									case DATA_STRING :
									{
										if (locXMLNodePtr->children != NULL)
											outChunk << (SInt8*) locXMLNodePtr->children->content;
										else
											outChunk << (SInt8*)"";
									}
									break;
									case DATA_FLOAT64 :
									{
										if (locXMLNodePtr->children != NULL)
											outChunk << (Float64) CString((char*)locXMLNodePtr->children->content).ToDouble();
										else
											outChunk << (Float64) 0.;
									}
									break;
								}

								found = true;
							}
						}

						if (!found && inCheckHash)
						{
							DeleteGEDRcd (outGEDRcd); return NULL;
						}
					}

					outGEDARcd->len  = outChunk.GetSize();
					outGEDARcd->data = new char [outGEDARcd->len];
					memcpy (outGEDARcd->data, outChunk.GetChunk(), outChunk.GetSize());
				}
				else
				{
					DeleteGEDRcd (outGEDRcd); return NULL;
				}
			}
		}
		break;

		case GED_PKT_REQ_BKD_HISTORY :
		{
			TGEDHRcd *outGEDHRcd = static_cast <TGEDHRcd *> (outGEDRcd); outGEDHRcd->typ = outType;

			for (xmlAttrPtr curXMLAttrPtr=inXmlNodePtr->properties; curXMLAttrPtr; curXMLAttrPtr=curXMLAttrPtr->next)
			{
				if ((CString((char*)curXMLAttrPtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_OCC_ATTR) || 
				     CString((char*)curXMLAttrPtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_OCC_ATTR_LIGHT)) && curXMLAttrPtr->children != NULL)
				{
					outGEDHRcd->occ = CString((char*)curXMLAttrPtr->children->content).ToLong();
				}
				else if ((CString((char*)curXMLAttrPtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_HASH_ATTR) || 
					  CString((char*)curXMLAttrPtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_HASH_ATTR_LIGHT)) && curXMLAttrPtr->children != NULL)
				{
					if (inCheckHash && (CString((char*)curXMLAttrPtr->children->content) != inGEDPktCfg->hash))
					{
						DeleteGEDRcd (outGEDRcd); return NULL;
					}
				}
				else if ((CString((char*)curXMLAttrPtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_BKD_HST_ATTR_RSN) || 
					  CString((char*)curXMLAttrPtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_BKD_HST_ATTR_RSN_LIGHT)) && curXMLAttrPtr->children != NULL)
				{
					if (CString((char*)curXMLAttrPtr->children->content) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_BKD_HST_ATTR_RSN_TTL) || 
					    CString((char*)curXMLAttrPtr->children->content) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_BKD_HST_ATTR_RSN_TTL_LIGHT))
					{
						outGEDHRcd->queue |= GED_PKT_REQ_BKD_HST_TTL;
					}
					else if (CString((char*)curXMLAttrPtr->children->content) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_BKD_HST_ATTR_RSN_PKT) ||
						 CString((char*)curXMLAttrPtr->children->content) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_BKD_HST_ATTR_RSN_PKT_LIGHT))
					{
						outGEDHRcd->queue |= GED_PKT_REQ_BKD_HST_PKT;
					}
					else
					{
						DeleteGEDRcd (outGEDRcd); return NULL;
					}
				}
				else if ((CString((char*)curXMLAttrPtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_ID_ATTR) || 
					  CString((char*)curXMLAttrPtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_ID_ATTR_LIGHT)) && curXMLAttrPtr->children != NULL)
				{
					outGEDHRcd->hid = CString((char*)curXMLAttrPtr->children->content).ToULong();
				}
			}

			for (xmlNodePtr curXMLNodePtr=inXmlNodePtr->children; curXMLNodePtr; curXMLNodePtr=curXMLNodePtr->next)
			{
				if (curXMLNodePtr->type != XML_ELEMENT_NODE) continue;

				if ((CString((char*)curXMLNodePtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_SOURCES) || 
				     CString((char*)curXMLNodePtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_SOURCES_LIGHT)) && curXMLNodePtr->children != NULL)
				{
					TBuffer <TGEDRcdSrc> outGEDRcdSrcs;

					for (xmlNodePtr locXMLNodePtr=curXMLNodePtr->children; locXMLNodePtr; locXMLNodePtr=locXMLNodePtr->next)
					{
						if (locXMLNodePtr->type != XML_ELEMENT_NODE) continue;

						if ((CString((char*)locXMLNodePtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_SOURCE) || 
						     CString((char*)locXMLNodePtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_SOURCE_LIGHT)) && locXMLNodePtr->children != NULL)
						{
							TGEDRcdSrc outGEDRcdSrc;

							for (xmlAttrPtr curXMLAttrPtr=locXMLNodePtr->properties; curXMLAttrPtr; curXMLAttrPtr=curXMLAttrPtr->next)
							{
								if ((CString((char*)curXMLAttrPtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_SOURCE_ATTR_RLY) || 
								     CString((char*)curXMLAttrPtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_SOURCE_ATTR_RLY_LIGHT)) && 
									curXMLAttrPtr->children != NULL)
								{
									outGEDRcdSrc.rly = CString((char*)curXMLAttrPtr->children->content).ToBool();
								}
								else
								{
									DeleteGEDRcd (outGEDRcd); return NULL;
								}
							}

							GetStrAddrToAddrIn (CString((char*)locXMLNodePtr->children->content), &outGEDRcdSrc.addr); 

							outGEDRcdSrcs += outGEDRcdSrc;
						}
						else
						{
							DeleteGEDRcd (outGEDRcd); return NULL;
						}
					}

					outGEDHRcd->nsrc = outGEDRcdSrcs.GetLength(); outGEDHRcd->src = new TGEDRcdSrc [outGEDHRcd->nsrc];

					for (size_t i=0; i<outGEDRcdSrcs.GetLength(); i++) memcpy (&outGEDHRcd->src[i], outGEDRcdSrcs[i], sizeof(TGEDRcdSrc));
				}
				else if ((CString((char*)curXMLNodePtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP) || 
					  CString((char*)curXMLNodePtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_LIGHT)) && curXMLNodePtr->children != NULL)
				{
					for (xmlNodePtr locXMLNodePtr=curXMLNodePtr->children; locXMLNodePtr; locXMLNodePtr=locXMLNodePtr->next)
					{
						if (locXMLNodePtr->type != XML_ELEMENT_NODE) continue;

						if ((CString((char*)locXMLNodePtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_ORIGINAL) || 
						     CString((char*)locXMLNodePtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_ORIGINAL_LIGHT)))
						{
							for (xmlAttrPtr curXMLAttrPtr=locXMLNodePtr->properties; curXMLAttrPtr; curXMLAttrPtr=curXMLAttrPtr->next)
							{
								if ((CString((char*)curXMLAttrPtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_SEC_ATTR) || 
								     CString((char*)curXMLAttrPtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_SEC_ATTR_LIGHT)) && 
									curXMLAttrPtr->children != NULL)
								{
									outGEDHRcd->otv.tv_sec = CString((char*)curXMLAttrPtr->children->content).ToULong();
								}
								else if ((CString((char*)curXMLAttrPtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_USEC_ATTR) || 
									  CString((char*)curXMLAttrPtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_USEC_ATTR_LIGHT)) &&
									curXMLAttrPtr->children != NULL)
								{
									outGEDHRcd->otv.tv_usec = CString((char*)curXMLAttrPtr->children->content).ToULong(); 
								}
								else
								{
									DeleteGEDRcd (outGEDRcd); return NULL;
								}
							}
						}
						else if ((CString((char*)locXMLNodePtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_LAST) || 
							  CString((char*)locXMLNodePtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_LAST_LIGHT)))
						{
							for (xmlAttrPtr curXMLAttrPtr=locXMLNodePtr->properties; curXMLAttrPtr; curXMLAttrPtr=curXMLAttrPtr->next)
							{
								if ((CString((char*)curXMLAttrPtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_SEC_ATTR) || 
								     CString((char*)curXMLAttrPtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_SEC_ATTR_LIGHT)) && 
									curXMLAttrPtr->children != NULL)
								{
									outGEDHRcd->ltv.tv_sec = CString((char*)curXMLAttrPtr->children->content).ToULong();
								}
								else if ((CString((char*)curXMLAttrPtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_USEC_ATTR) || 
									  CString((char*)curXMLAttrPtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_USEC_ATTR_LIGHT)) &&
									curXMLAttrPtr->children != NULL)
								{
									outGEDHRcd->ltv.tv_usec = CString((char*)curXMLAttrPtr->children->content).ToULong(); 
								}
								else
								{
									DeleteGEDRcd (outGEDRcd); return NULL;
								}
							}
						}
						else if ((CString((char*)locXMLNodePtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_RECEPTION) || 
							  CString((char*)locXMLNodePtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_RECEPTION_LIGHT)))
						{
							for (xmlAttrPtr curXMLAttrPtr=locXMLNodePtr->properties; curXMLAttrPtr; curXMLAttrPtr=curXMLAttrPtr->next)
							{
								if ((CString((char*)curXMLAttrPtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_SEC_ATTR) || 
								     CString((char*)curXMLAttrPtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_SEC_ATTR_LIGHT)) && 
									curXMLAttrPtr->children != NULL)
								{
									outGEDHRcd->rtv.tv_sec = CString((char*)curXMLAttrPtr->children->content).ToULong();
								}
								else if ((CString((char*)curXMLAttrPtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_USEC_ATTR) || 
									  CString((char*)curXMLAttrPtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_USEC_ATTR_LIGHT)) &&
									curXMLAttrPtr->children != NULL)
								{
									outGEDHRcd->rtv.tv_usec = CString((char*)curXMLAttrPtr->children->content).ToULong(); 
								}
								else
								{
									DeleteGEDRcd (outGEDRcd); return NULL;
								}
							}
						}
						else if ((CString((char*)locXMLNodePtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_META) || 
							  CString((char*)locXMLNodePtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_META_LIGHT)))
						{
							for (xmlAttrPtr curXMLAttrPtr=locXMLNodePtr->properties; curXMLAttrPtr; curXMLAttrPtr=curXMLAttrPtr->next)
							{
								if ((CString((char*)curXMLAttrPtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_SEC_ATTR) || 
								     CString((char*)curXMLAttrPtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_SEC_ATTR_LIGHT)) && 
									curXMLAttrPtr->children != NULL)
								{
									outGEDHRcd->mtv.tv_sec = CString((char*)curXMLAttrPtr->children->content).ToULong();
								}
								else if ((CString((char*)curXMLAttrPtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_USEC_ATTR) || 
									  CString((char*)curXMLAttrPtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_USEC_ATTR_LIGHT)) && 
									curXMLAttrPtr->children != NULL)
								{
									outGEDHRcd->mtv.tv_usec = CString((char*)curXMLAttrPtr->children->content).ToULong(); 
								}
								else
								{
									DeleteGEDRcd (outGEDRcd); return NULL;
								}
							}
						}
						else if ((CString((char*)locXMLNodePtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_ACKNOWLEGDE) || 
							  CString((char*)locXMLNodePtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_ACKNOWLEGDE_LIGHT)))
						{
							for (xmlAttrPtr curXMLAttrPtr=locXMLNodePtr->properties; curXMLAttrPtr; curXMLAttrPtr=curXMLAttrPtr->next)
							{
								if ((CString((char*)curXMLAttrPtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_SEC_ATTR) || 
								     CString((char*)curXMLAttrPtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_SEC_ATTR_LIGHT)) && 
									curXMLAttrPtr->children != NULL)
								{
									outGEDHRcd->atv.tv_sec = CString((char*)curXMLAttrPtr->children->content).ToULong();
								}
								else if ((CString((char*)curXMLAttrPtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_USEC_ATTR) || 
								    	  CString((char*)curXMLAttrPtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_USEC_ATTR_LIGHT)) && 
									curXMLAttrPtr->children != NULL)
								{
									outGEDHRcd->atv.tv_usec = CString((char*)curXMLAttrPtr->children->content).ToULong(); 
								}
								else if (CString((char*)curXMLAttrPtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_DURATION_ATTR))
								{
								}
								else
								{
									DeleteGEDRcd (outGEDRcd); return NULL;
								}
							}
						}
						else
						{
							DeleteGEDRcd (outGEDRcd); return NULL;
						}
					}
				}
				else if ((CString((char*)curXMLNodePtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_CONTENT) || 
					  CString((char*)curXMLNodePtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_CONTENT_LIGHT)) && curXMLNodePtr->children != NULL)
				{
					CChunk outChunk;

					for (size_t i=0; i<inGEDPktCfg->fields.GetLength(); i++)
					{
						bool found=false; for (xmlNodePtr locXMLNodePtr=curXMLNodePtr->children; locXMLNodePtr && !found; locXMLNodePtr=locXMLNodePtr->next)
						{
							if (locXMLNodePtr->type != XML_ELEMENT_NODE) continue;

							if (CString((char*)locXMLNodePtr->name) == inGEDPktCfg->fields[i]->name)
							{
								switch (inGEDPktCfg->fields[i]->type)
								{
									case DATA_SINT32 :
									{
										if (locXMLNodePtr->children != NULL)
											outChunk << (SInt32) CString((char*)locXMLNodePtr->children->content).ToLong();
										else
											outChunk << 0L;
									}
									break;
									case DATA_STRING :
									{
										if (locXMLNodePtr->children != NULL)
											outChunk << (SInt8*) locXMLNodePtr->children->content;
										else
											outChunk << (SInt8*)"";
									}
									break;
									case DATA_FLOAT64 :
									{
										if (locXMLNodePtr->children != NULL)
											outChunk << (Float64) CString((char*)locXMLNodePtr->children->content).ToDouble();
										else
											outChunk << (Float64) 0.;
									}
									break;
								}

								found = true;
							}
						}

						if (!found && inCheckHash)
						{
							DeleteGEDRcd (outGEDRcd); return NULL;
						}
					}

					outGEDHRcd->len  = outChunk.GetSize();
					outGEDHRcd->data = new char [outGEDHRcd->len];
					memcpy (outGEDHRcd->data, outChunk.GetChunk(), outChunk.GetSize());
				}
				else
				{
					DeleteGEDRcd (outGEDRcd); return NULL;
				}
			}
		}
		break;

		case GED_PKT_REQ_BKD_SYNC :
		{
			TGEDSRcd *outGEDSRcd = static_cast <TGEDSRcd *> (outGEDRcd); outGEDSRcd->pkt = new TGEDPktIn; bzero(outGEDSRcd->pkt,sizeof(TGEDPktIn)); 

			outGEDSRcd->pkt->typ = outType; outGEDSRcd->pkt->vrs = GED_VERSION;

			for (xmlAttrPtr curXMLAttrPtr=inXmlNodePtr->properties; curXMLAttrPtr; curXMLAttrPtr=curXMLAttrPtr->next)
			{
				if ((CString((char*)curXMLAttrPtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_HASH_ATTR) || 
				     CString((char*)curXMLAttrPtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_HASH_ATTR_LIGHT)) && curXMLAttrPtr->children != NULL)
				{
					if (inCheckHash && (CString((char*)curXMLAttrPtr->children->content) != inGEDPktCfg->hash))
					{
						DeleteGEDRcd (outGEDRcd); return NULL;
					}
				}
				else if ((CString((char*)curXMLAttrPtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_REQ_ATTR) || 
					  CString((char*)curXMLAttrPtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_REQ_ATTR_LIGHT)) && curXMLAttrPtr->children != NULL)
				{
					if (CString((char*)curXMLAttrPtr->children->content) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_REQ_ATTR_PUSH) ||
					    CString((char*)curXMLAttrPtr->children->content) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_REQ_ATTR_PUSH_LIGHT))
					{
						outGEDSRcd->pkt->req |= GED_PKT_REQ_PUSH|GED_PKT_REQ_PUSH_TMSP;
					}
					else if (CString((char*)curXMLAttrPtr->children->content) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_REQ_ATTR_UPDATE) ||
						 CString((char*)curXMLAttrPtr->children->content) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_REQ_ATTR_UPDATE_LIGHT))
					{
						outGEDSRcd->pkt->req |= GED_PKT_REQ_PUSH|GED_PKT_REQ_PUSH_NOTMSP;
					}
					else if (CString((char*)curXMLAttrPtr->children->content) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_REQ_ATTR_DROP) ||
						 CString((char*)curXMLAttrPtr->children->content) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_REQ_ATTR_DROP_LIGHT))
					{
						outGEDSRcd->pkt->req |= GED_PKT_REQ_DROP;
					}
					else
					{
						DeleteGEDRcd (outGEDRcd); return NULL;
					}
				}
				else if ((CString((char*)curXMLAttrPtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_BKD_ATTR) || 
					  CString((char*)curXMLAttrPtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_BKD_ATTR_LIGHT)) && curXMLAttrPtr->children != NULL)
				{
					if (CString((char*)curXMLAttrPtr->children->content) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_QUEUE_ATTR_ACTIVE) || 
					    CString((char*)curXMLAttrPtr->children->content) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_QUEUE_ATTR_ACTIVE_LIGHT))
					{
						outGEDSRcd->pkt->req |= GED_PKT_REQ_BKD_ACTIVE;
					}
					else if (CString((char*)curXMLAttrPtr->children->content) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_QUEUE_ATTR_HISTORY) ||
						 CString((char*)curXMLAttrPtr->children->content) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_QUEUE_ATTR_HISTORY_LIGHT))
					{
						outGEDSRcd->pkt->req |= GED_PKT_REQ_BKD_HISTORY;
					}
					else if (CString((char*)curXMLAttrPtr->children->content) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_QUEUE_ATTR_SYNC) ||
						 CString((char*)curXMLAttrPtr->children->content) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_QUEUE_ATTR_SYNC_LIGHT))
					{
						outGEDSRcd->pkt->req |= GED_PKT_REQ_BKD_SYNC;
					}
					else
					{
						DeleteGEDRcd (outGEDRcd); return NULL;
					}
				}
			}

			for (xmlNodePtr curXMLNodePtr=inXmlNodePtr->children; curXMLNodePtr; curXMLNodePtr=curXMLNodePtr->next)
			{
				if (curXMLNodePtr->type != XML_ELEMENT_NODE) continue;

				if ((CString((char*)curXMLNodePtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_SOURCE) || 
				     CString((char*)curXMLNodePtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_SOURCE_LIGHT)) && curXMLNodePtr->children != NULL)
				{
					for (xmlAttrPtr curXMLAttrPtr=curXMLNodePtr->properties; curXMLAttrPtr; curXMLAttrPtr=curXMLAttrPtr->next)
					{
						if ((CString((char*)curXMLAttrPtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_SOURCE_ATTR_RLY) || 
						     CString((char*)curXMLAttrPtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_SOURCE_ATTR_RLY_LIGHT)) && curXMLAttrPtr->children != NULL)
						{
							outGEDSRcd->pkt->req |= (CString((char*)curXMLAttrPtr->children->content).ToBool() ? GED_PKT_REQ_SRC_RELAY : 0L);
						}
						else
						{
							DeleteGEDRcd (outGEDRcd); return NULL;
						}
					}

					GetStrAddrToAddrIn (CString((char*)curXMLNodePtr->children->content), &outGEDSRcd->pkt->addr);
				}
				else if ((CString((char*)curXMLNodePtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_TARGET) || 
					  CString((char*)curXMLNodePtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_TARGET_LIGHT)) && curXMLNodePtr->children != NULL)
				{
					GetStrAddrToAddrIn (CString((char*)curXMLNodePtr->children->content), &outGEDSRcd->tgt);
				}
				else if ((CString((char*)curXMLNodePtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP) ||  
					  CString((char*)curXMLNodePtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_LIGHT)) && curXMLNodePtr->children != NULL)
				{
					for (xmlNodePtr locXMLNodePtr=curXMLNodePtr->children; locXMLNodePtr; locXMLNodePtr=locXMLNodePtr->next)
					{
						if (locXMLNodePtr->type != XML_ELEMENT_NODE) continue;

						if ((CString((char*)locXMLNodePtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_ORIGINAL) || 
						     CString((char*)locXMLNodePtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_ORIGINAL_LIGHT)))
						{
							for (xmlAttrPtr curXMLAttrPtr=locXMLNodePtr->properties; curXMLAttrPtr; curXMLAttrPtr=curXMLAttrPtr->next)
							{
								if ((CString((char*)curXMLAttrPtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_SEC_ATTR) || 
								     CString((char*)curXMLAttrPtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_SEC_ATTR_LIGHT)) && 
									curXMLAttrPtr->children != NULL)
								{
									outGEDSRcd->pkt->tv.tv_sec = CString((char*)curXMLAttrPtr->children->content).ToULong();
								}
								else if ((CString((char*)curXMLAttrPtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_USEC_ATTR) || 
									  CString((char*)curXMLAttrPtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_USEC_ATTR_LIGHT)) && 
									curXMLAttrPtr->children != NULL)
								{
									outGEDSRcd->pkt->tv.tv_usec = CString((char*)curXMLAttrPtr->children->content).ToULong(); 
								}
								else
								{
									DeleteGEDRcd (outGEDRcd); return NULL;
								}
							}
						}
						else
						{
							DeleteGEDRcd (outGEDRcd); return NULL;
						}
					}
				}
				else if ((CString((char*)curXMLNodePtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_CONTENT) || 
					  CString((char*)curXMLNodePtr->name) == CString((char*)XML_ELEMENT_NODE_GED_RECORD_CONTENT_LIGHT)) && curXMLNodePtr->children != NULL)
				{
					CChunk outChunk;

					for (size_t i=0; i<inGEDPktCfg->fields.GetLength(); i++)
					{
						bool found=false; for (xmlNodePtr locXMLNodePtr=curXMLNodePtr->children; locXMLNodePtr && !found; locXMLNodePtr=locXMLNodePtr->next)
						{
							if (locXMLNodePtr->type != XML_ELEMENT_NODE) continue;

							if (CString((char*)locXMLNodePtr->name) == inGEDPktCfg->fields[i]->name)
							{
								switch (inGEDPktCfg->fields[i]->type)
								{
									case DATA_SINT32 :
									{
										if (locXMLNodePtr->children != NULL)
											outChunk << (SInt32) CString((char*)locXMLNodePtr->children->content).ToLong();
										else
											outChunk << 0L;
									}
									break;
									case DATA_STRING :
									{
										if (locXMLNodePtr->children != NULL)
											outChunk << (SInt8*) locXMLNodePtr->children->content;
										else
											outChunk << (SInt8*) "";
									}
									break;
									case DATA_FLOAT64 :
									{
										if (locXMLNodePtr->children != NULL)
											outChunk << (Float64) CString((char*)locXMLNodePtr->children->content).ToDouble();
										else
											outChunk << (Float64) 0.;
									}
									break;
								}

								found = true;
							}
						}

						if (!found && inCheckHash)
						{
							DeleteGEDRcd (outGEDRcd); return NULL;
						}
					}

					outGEDSRcd->pkt->len  = outChunk.GetSize();
					outGEDSRcd->pkt->data = new char [outGEDSRcd->pkt->len];
					memcpy (outGEDSRcd->pkt->data, outChunk.GetChunk(), outChunk.GetSize());
				}
				else
				{
					DeleteGEDRcd (outGEDRcd); return NULL;
				}
			}
		}
		break;
	}

	return outGEDRcd;
}

//---------------------------------------------------------------------------------------------------------------------------------------
// ged record to string
//---------------------------------------------------------------------------------------------------------------------------------------
CString GEDRcdToString (TGEDRcd *inGEDRcd, const TBuffer <TGEDPktCfg> &inGEDPktCfgs)
{
	CString outString; CChunk *inChunk = NULL; TGEDPktCfg *inGEDPktCfg = NULL;

	if (inGEDRcd == NULL) return outString;

	switch (inGEDRcd->queue&GED_PKT_REQ_BKD_MASK)
	{
		case GED_PKT_REQ_BKD_STAT :
		{
			TGEDStatRcd *inGEDStatRcd (static_cast <TGEDStatRcd *> (inGEDRcd));

			outString += CString((char*)XML_ELEMENT_NODE_GED_RECORD_STAT_NTA_ATTR) + "=" + CString(inGEDStatRcd->nta) + " ";
			outString += CString((char*)XML_ELEMENT_NODE_GED_RECORD_STAT_NTS_ATTR) + "=" + CString(inGEDStatRcd->nts) + " ";
			outString += CString((char*)XML_ELEMENT_NODE_GED_RECORD_STAT_NTH_ATTR) + "=" + CString(inGEDStatRcd->nth) + " ";
			outString += CString((char*)XML_ELEMENT_NODE_GED_RECORD_STAT_NTR_ATTR) + "=" + CString(inGEDStatRcd->ntr);
		}
		break;

		case GED_PKT_REQ_BKD_ACTIVE :
		{
			TGEDARcd *inGEDARcd (static_cast <TGEDARcd *> (inGEDRcd));

			outString += CString((char*)XML_ELEMENT_NODE_GED_RECORD_QUEUE_ATTR) + "=" + CString((char*)XML_ELEMENT_NODE_GED_RECORD_QUEUE_ATTR_ACTIVE) + " ";
			outString += CString((char*)XML_ELEMENT_NODE_GED_RECORD_TYPE_ATTR)  + "=" + CString(inGEDARcd->typ) + " ";
			outString += CString((char*)XML_ELEMENT_NODE_GED_RECORD_OCC_ATTR)   + "=" + CString(inGEDARcd->occ) + " ";
			
			outString += " " + CString((char*)XML_ELEMENT_NODE_GED_RECORD_SOURCES) + "=";

			for (size_t i=0; i<inGEDARcd->nsrc; i++)
				outString += CString(::inet_ntoa(inGEDARcd->src[i].addr)) + "/" + CString(inGEDARcd->src[i].rly?"1":"0");

			outString += " ";

			char otm[256]; struct tm sotm; ::localtime_r (&(inGEDARcd->otv.tv_sec), &sotm);
			char ltm[256]; struct tm sltm; ::localtime_r (&(inGEDARcd->ltv.tv_sec), &sltm);
			char rtm[256]; struct tm srtm; ::localtime_r (&(inGEDARcd->rtv.tv_sec), &srtm);
			char mtm[256]; struct tm smtm; ::localtime_r (&(inGEDARcd->mtv.tv_sec), &smtm);
			char ftm[256]; struct tm sftm; ::localtime_r (&(inGEDARcd->ftv.tv_sec), &sftm);

			::strftime (otm, 256, "%Ec", &sotm);
			::strftime (ltm, 256, "%Ec", &sltm);
			::strftime (rtm, 256, "%Ec", &srtm);
			::strftime (mtm, 256, "%Ec", &smtm);
			::strftime (ftm, 256, "%Ec", &sftm);
      
			outString += CString((char*)XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_ORIGINAL) + "=" + (CString(otm)-"\n") + 
					" (" + CString((unsigned long)inGEDARcd->otv.tv_sec) + "/" + CString((unsigned long)inGEDARcd->otv.tv_usec) + ") ";
			outString += CString((char*)XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_LAST) + "=" + (CString(ltm)-"\n") + 
					" (" + CString((unsigned long)inGEDARcd->ltv.tv_sec) + "/" + CString((unsigned long)inGEDARcd->ltv.tv_usec) + ") ";
			outString += CString((char*)XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_RECEPTION) + "=" + (CString(rtm)-"\n") + 
					" (" + CString((unsigned long)inGEDARcd->rtv.tv_sec) + "/" + CString((unsigned long)inGEDARcd->rtv.tv_usec) + ") ";
			outString += CString((char*)XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_META) + "=" + (CString(mtm)-"\n") + 
					" (" + CString((unsigned long)inGEDARcd->mtv.tv_sec) + "/" + CString((unsigned long)inGEDARcd->mtv.tv_usec) + ") ";
     outString += CString((char*)XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_FIRST) + "=" + (CString(ftm)-"\n") + 
					" (" + CString((unsigned long)inGEDARcd->ftv.tv_sec) + "/" + CString((unsigned long)inGEDARcd->ftv.tv_usec) + ") ";

			for (size_t i=0; i<inGEDPktCfgs.GetLength() && inGEDPktCfg == NULL; i++)
				if (inGEDPktCfgs[i]->type == inGEDARcd->typ) 
					inGEDPktCfg = inGEDPktCfgs[i];

			if (inGEDARcd->len > 0)
				inChunk = new CChunk (inGEDARcd->data, inGEDARcd->len, ::GEDPktCfgToTData(inGEDPktCfg), false);
		}
		break;

		case GED_PKT_REQ_BKD_HISTORY :
		{
			TGEDHRcd *inGEDHRcd (static_cast <TGEDHRcd *> (inGEDRcd));

			outString += CString((char*)XML_ELEMENT_NODE_GED_RECORD_QUEUE_ATTR) + "=" + CString((char*)XML_ELEMENT_NODE_GED_RECORD_QUEUE_ATTR_HISTORY) + " ";
			outString += CString((char*)XML_ELEMENT_NODE_GED_RECORD_TYPE_ATTR)  + "=" + CString(inGEDHRcd->typ) + " ";
			outString += CString((char*)XML_ELEMENT_NODE_GED_RECORD_OCC_ATTR)   + "=" + CString(inGEDHRcd->occ) + " ";
			outString += CString((char*)XML_ELEMENT_NODE_GED_RECORD_ID_ATTR)    + "=" + CString(inGEDHRcd->hid) + " ";

			outString += CString((char*)XML_ELEMENT_NODE_GED_RECORD_BKD_HST_ATTR_RSN) + "=";
			if ((inGEDHRcd->queue & GED_PKT_REQ_BKD_HST_MASK) == GED_PKT_REQ_BKD_HST_TTL)
				outString += CString((char*)XML_ELEMENT_NODE_GED_RECORD_BKD_HST_ATTR_RSN_TTL);
			else
				outString += CString((char*)XML_ELEMENT_NODE_GED_RECORD_BKD_HST_ATTR_RSN_PKT);

			outString += " " + CString((char*)XML_ELEMENT_NODE_GED_RECORD_SOURCES) + "=";

			for (size_t i=0; i<inGEDHRcd->nsrc; i++)
				outString += CString(::inet_ntoa(inGEDHRcd->src[i].addr)) + "/" + CString(inGEDHRcd->src[i].rly?"1":"0");

			outString += " ";

			char otm[256]; struct tm sotm; ::localtime_r (&(inGEDHRcd->otv.tv_sec), &sotm);
			char ltm[256]; struct tm sltm; ::localtime_r (&(inGEDHRcd->ltv.tv_sec), &sltm);
			char rtm[256]; struct tm srtm; ::localtime_r (&(inGEDHRcd->rtv.tv_sec), &srtm);
			char mtm[256]; struct tm smtm; ::localtime_r (&(inGEDHRcd->mtv.tv_sec), &smtm);
			char ftm[256]; struct tm sftm; ::localtime_r (&(inGEDHRcd->ftv.tv_sec), &sftm);
			char atm[256]; struct tm satm; ::localtime_r (&(inGEDHRcd->atv.tv_sec), &satm);

			::strftime (otm, 256, "%Ec", &sotm);
			::strftime (ltm, 256, "%Ec", &sltm);
			::strftime (rtm, 256, "%Ec", &srtm);
			::strftime (mtm, 256, "%Ec", &smtm);
			::strftime (ftm, 256, "%Ec", &sftm);
      ::strftime (atm, 256, "%Ec", &satm);

			outString += CString((char*)XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_ORIGINAL) + "=" + (CString(otm)-"\n") + 
					" (" + CString((unsigned long)inGEDHRcd->otv.tv_sec) + "/" + CString((unsigned long)inGEDHRcd->otv.tv_usec) + ") ";
			outString += CString((char*)XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_LAST) + "=" + (CString(ltm)-"\n") + 
					" (" + CString((unsigned long)inGEDHRcd->ltv.tv_sec) + "/" + CString((unsigned long)inGEDHRcd->ltv.tv_usec) + ") ";
			outString += CString((char*)XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_RECEPTION) + "=" + (CString(rtm)-"\n") + 
					" (" + CString((unsigned long)inGEDHRcd->rtv.tv_sec) + "/" + CString((unsigned long)inGEDHRcd->rtv.tv_usec) + ") ";
			outString += CString((char*)XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_META) + "=" + (CString(mtm)-"\n") + 
					" (" + CString((unsigned long)inGEDHRcd->mtv.tv_sec) + "/" + CString((unsigned long)inGEDHRcd->mtv.tv_usec) + ") ";
			outString += CString((char*)XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_FIRST) + "=" + (CString(ftm)-"\n") + 
					" (" + CString((unsigned long)inGEDHRcd->ftv.tv_sec) + "/" + CString((unsigned long)inGEDHRcd->ftv.tv_usec) + ") ";
			outString += CString((char*)XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_ACKNOWLEGDE) + "=" + (CString(atm)-"\n") + 
					" (" + CString((unsigned long)inGEDHRcd->atv.tv_sec) + "/" + CString((unsigned long)inGEDHRcd->atv.tv_usec) + " " +
					CString((char*)XML_ELEMENT_NODE_GED_RECORD_DURATION_ATTR) + ":" + CString((unsigned long)inGEDHRcd->atv.tv_sec-inGEDHRcd->otv.tv_sec) + ") ";

			for (size_t i=0; i<inGEDPktCfgs.GetLength() && inGEDPktCfg == NULL; i++)
				if (inGEDPktCfgs[i]->type == inGEDHRcd->typ) 
					inGEDPktCfg = inGEDPktCfgs[i];

			if (inGEDHRcd->len > 0)
				inChunk = new CChunk (inGEDHRcd->data, inGEDHRcd->len, ::GEDPktCfgToTData(inGEDPktCfg), false);
		}
		break;

		case GED_PKT_REQ_BKD_SYNC :
		{
			TGEDSRcd *inGEDSRcd (static_cast <TGEDSRcd *> (inGEDRcd));
                        TGEDPktIn *inGEDPktIn (inGEDSRcd->pkt);

			outString += CString((char*)XML_ELEMENT_NODE_GED_RECORD_QUEUE_ATTR) + "=" + CString((char*)XML_ELEMENT_NODE_GED_RECORD_QUEUE_ATTR_SYNC) + " ";
			outString += CString((char*)XML_ELEMENT_NODE_GED_RECORD_TYPE_ATTR)  + "=" + CString(inGEDPktIn->typ) + " ";
			outString += CString((char*)XML_ELEMENT_NODE_GED_RECORD_SOURCE) + "=" + CString(::inet_ntoa(inGEDPktIn->addr)) + "/";
			outString += CString((inGEDPktIn->req&GED_PKT_REQ_SRC_RELAY)?"1":"0") + " ";
			outString += CString((char*)XML_ELEMENT_NODE_GED_RECORD_TARGET) + "=" + CString(::inet_ntoa(inGEDSRcd->tgt)) + " ";

			char otm[256]; struct tm sotm; ::localtime_r (&(inGEDPktIn->tv.tv_sec), &sotm); ::strftime (otm, 256, "%Ec", &sotm);

			outString += CString((char*)XML_ELEMENT_NODE_GED_RECORD_TIMESTAMP_ORIGINAL) + "=" + (CString(otm)-"\n") + 
					" (" + CString((unsigned long)inGEDPktIn->tv.tv_sec) + "/" + CString((unsigned long)inGEDPktIn->tv.tv_usec) + ") ";

                        switch (inGEDPktIn->req&GED_PKT_REQ_MASK)
                        {
                                case GED_PKT_REQ_PUSH : 
				{
					switch (inGEDPktIn->req&GED_PKT_REQ_PUSH_MASK)
					{
						case GED_PKT_REQ_PUSH_TMSP :
						{
							outString += CString((char*)XML_ELEMENT_NODE_GED_RECORD_REQ_ATTR) + "=" + CString((char*)XML_ELEMENT_NODE_GED_RECORD_REQ_ATTR_PUSH) + " ";
						}
						break;
						case GED_PKT_REQ_PUSH_NOTMSP :
						{
							outString += CString((char*)XML_ELEMENT_NODE_GED_RECORD_REQ_ATTR) + "=" + CString((char*)XML_ELEMENT_NODE_GED_RECORD_REQ_ATTR_UPDATE) + " ";
						}
						break;
					}
				}
				break;
                                case GED_PKT_REQ_DROP : outString += CString((char*)XML_ELEMENT_NODE_GED_RECORD_REQ_ATTR) + "=" + CString((char*)XML_ELEMENT_NODE_GED_RECORD_REQ_ATTR_DROP) + " ";
					break;
                        }

                        switch (inGEDPktIn->req&GED_PKT_REQ_BKD_MASK)
                        {
                                case GED_PKT_REQ_BKD_ACTIVE  : outString += CString((char*)XML_ELEMENT_NODE_GED_RECORD_BKD_ATTR) + "=" + 
									CString((char*)XML_ELEMENT_NODE_GED_RECORD_QUEUE_ATTR_ACTIVE);  break;
                                case GED_PKT_REQ_BKD_HISTORY : outString += CString((char*)XML_ELEMENT_NODE_GED_RECORD_BKD_ATTR) + "=" +
									CString((char*)XML_ELEMENT_NODE_GED_RECORD_QUEUE_ATTR_HISTORY); break;
                                case GED_PKT_REQ_BKD_SYNC    : outString += CString((char*)XML_ELEMENT_NODE_GED_RECORD_BKD_ATTR) + "=" +
									CString((char*)XML_ELEMENT_NODE_GED_RECORD_QUEUE_ATTR_SYNC); break;
                        }

                        for (size_t i=0; i<inGEDPktCfgs.GetLength() && inGEDPktCfg == NULL; i++)
                                if (inGEDPktCfgs[i]->type == inGEDPktIn->typ)
                                        inGEDPktCfg = inGEDPktCfgs[i];

                        if (inGEDPktIn->len > 0)
                                inChunk = new CChunk (inGEDPktIn->data, inGEDPktIn->len, ::GEDPktCfgToTData(inGEDPktCfg), false);
		}
		break;

	}

	if (inGEDPktCfg == NULL) return outString;

	outString += CString((char*)XML_ELEMENT_NODE_GED_RECORD_HASH_ATTR) + "=" + inGEDPktCfg->hash;

	if (inChunk != NULL)
	{
		for (size_t i=0; i<inGEDPktCfg->fields.GetLength(); i++)
		{
			switch (inChunk->NextDataIs())
			{
				case DATA_SINT32 :
				{
					SInt32 inSint32=0L;
					if (inChunk->ReadSInt32(inSint32)) 
						outString += " " + inGEDPktCfg->fields[i]->name + "=" + CString(inSint32);
					else
						outString += " " + inGEDPktCfg->fields[i]->name + "=";  
				}
				break;
				case DATA_STRING :
				{
					SInt8 *inSint8=NULL; 
					*inChunk >> inSint8;
					if (CString(inSint8).GetLength() > 0) 
						outString += " " + inGEDPktCfg->fields[i]->name + "=" + CString(inSint8);
					else
						outString += " " + inGEDPktCfg->fields[i]->name + "=";
					if (inSint8 != NULL) delete [] inSint8;
				}
				break;
				case DATA_FLOAT64 :
				{
					Float64 inFloat64=0.;
					if (inChunk->ReadFloat64(inFloat64))
						outString += " " + inGEDPktCfg->fields[i]->name + "=" + CString(inFloat64);
					else
						outString += " " + inGEDPktCfg->fields[i]->name + "=";
				}
				break;
			}
		}

		delete inChunk;
	}
	else
		for (size_t i=inGEDPktCfg->fields.GetLength(), j=0; i>0; i--, j++)
			outString += " " + inGEDPktCfg->fields[j]->name + "=";

	return outString;
}

#endif

//---------------------------------------------------------------------------------------------------------------------------------------
// packet cfg to tdata
//---------------------------------------------------------------------------------------------------------------------------------------
TBuffer <TData>	GEDPktCfgToTData (TGEDPktCfg *inGEDPktCfg)
{
	TBuffer <TData> outData; if (inGEDPktCfg == NULL) return outData;
	for (size_t i=inGEDPktCfg->fields.GetLength(), j=0; i>0; i--, j++)
		outData += inGEDPktCfg->fields[j]->type;
	return outData;
}

//---------------------------------------------------------------------------------------------------------------------------------------
// send raw pkts to specified dest
//---------------------------------------------------------------------------------------------------------------------------------------
int SendRawGEDPkt (int inDesc, TBuffer <TGEDPktOut> &inGEDPktOut)
{
	long inLen=0L; int outRes=0L;

	for (size_t i=inGEDPktOut.GetLength(), j=0; i>0; i--, j++) 
		inLen += ::SizeOfGEDPktOut(*inGEDPktOut[j]);

	outRes += CSocket::Send (inDesc, &inLen, sizeof(long));

	for (size_t i=inGEDPktOut.GetLength(), j=0; i>0; i--, j++)
	{
		TGEDPktOut theGEDPktOut = *inGEDPktOut[j];
		outRes += CSocket::Send (inDesc, theGEDPktOut, ::SizeOfGEDPktOut(theGEDPktOut));
	}

	return outRes;
}

//---------------------------------------------------------------------------------------------------------------------------------------
// Send an http request containing a set of ged packets with the following fields, the compression depends on the http Content-Type 
// specified context :
//---------------------------------------------------------------------------------------------------------------------------------------
//	GET|POST http://address HTTP/1.0|1.1
//	[User-Agent: Mozilla/5.0 (X11; U; Linux i686; en-US; rv:1.8.1.2) Gecko/20070223 Firefox/2.0.0.2]
//	Host: server
//	Connection: close | keep-alive
//	[Content-MD5: xxx]
//	Content-Type: image/jpeg | multipart/x-gzip
//	Content-Length: packets total length | compression size + compression itself
//	CRLF
//	packets themself
//---------------------------------------------------------------------------------------------------------------------------------------
int SendHttpGEDPktToTgt (int inDesc, SSL *inSSL, TGEDHttpReqCtx *inGEDHttpReqCtx, TBuffer <TGEDPktOut> &inGEDPktOut, CString inMd5)
{
	CString outHttp (inGEDHttpReqCtx->cmd + " http://" + inGEDHttpReqCtx->host + " HTTP/" + inGEDHttpReqCtx->vrs + GED_HTTP_REGEX_CRLF);
	if (inGEDHttpReqCtx->agt != CString()) outHttp += "User-Agent: " + inGEDHttpReqCtx->agt + GED_HTTP_REGEX_CRLF;
	outHttp += "Host: " + inGEDHttpReqCtx->host + GED_HTTP_REGEX_CRLF;
	if (inMd5 != CString()) outHttp += "Content-MD5: " + inMd5 + GED_HTTP_REGEX_CRLF;
	outHttp += inGEDHttpReqCtx->kpalv ? "Connection: keep-alive" + GED_HTTP_REGEX_CRLF : "Connection: close" + GED_HTTP_REGEX_CRLF; 
	outHttp += "Content-Type: " + inGEDHttpReqCtx->typ + GED_HTTP_REGEX_CRLF;

	unsigned long outContentLen=0L; int outRes=0;

	if (inGEDHttpReqCtx->typ.Find(CString("zip")))
	{
		long inLen=0L; for (size_t i=inGEDPktOut.GetLength(), j=0; i>0; i--, j++)
			inLen += ::SizeOfGEDPktOut(*inGEDPktOut[j]);

		SInt8 *inSrc = (SInt8 *) ::malloc (inLen);
		SInt8 *inDst = (SInt8 *) ::malloc ((int)(inLen+inLen*0.001+1+12));

		for (size_t i=inGEDPktOut.GetLength(), j=0, d=0; i>0; i--, j++)
		{
			TGEDPktOut theGEDPktOut = *inGEDPktOut[j];
			::memcpy ((char *)inSrc+d, theGEDPktOut, ::SizeOfGEDPktOut(theGEDPktOut));
			d += ::SizeOfGEDPktOut(theGEDPktOut);
		}

		outContentLen=(int)(inLen+inLen*0.001+1+12); int err; if ((err = ::compress2 ((Bytef*)inDst, &outContentLen, (Bytef*)inSrc, inLen, inGEDHttpReqCtx->zlv)) != Z_OK)
		{
			::free (inSrc);
			::free (inDst);

			return 0;
		}

		outHttp += "Content-Length: " + CString(outContentLen+sizeof(long)) + GED_HTTP_REGEX_CRLF + GED_HTTP_REGEX_CRLF;

		if (inSSL != NULL)
		{
			outRes  = CSocket::SendSSL (inSSL, outHttp.Get(), outHttp.GetLength());
			outRes += CSocket::SendSSL (inSSL, &inLen, sizeof(long));
			outRes += CSocket::SendSSL (inSSL, inDst, outContentLen);
		}
		else
		{
			outRes  = CSocket::Send (inDesc, outHttp.Get(), outHttp.GetLength());
			outRes += CSocket::Send (inDesc, &inLen, sizeof(long));
			outRes += CSocket::Send (inDesc, inDst, outContentLen);
		}

		outContentLen += sizeof(long);

		::free (inSrc);
		::free (inDst);
	}
	else
	{
		for (size_t i=inGEDPktOut.GetLength(), j=0; i>0; i--, j++)
			outContentLen += ::SizeOfGEDPktOut(*inGEDPktOut[j]);

		outHttp += "Content-Length: " + CString(outContentLen) + GED_HTTP_REGEX_CRLF + GED_HTTP_REGEX_CRLF;

		if (inSSL != NULL)
		{
			outRes = CSocket::SendSSL (inSSL, outHttp.Get(), outHttp.GetLength());
			for (size_t i=inGEDPktOut.GetLength(), j=0; i>0; i--, j++)
			{
				TGEDPktOut theGEDPktOut = *inGEDPktOut[j];
				outRes += CSocket::SendSSL (inSSL, theGEDPktOut, ::SizeOfGEDPktOut(theGEDPktOut));
			}
		}
		else
		{
			outRes = CSocket::Send (inDesc, outHttp.Get(), outHttp.GetLength());
			for (size_t i=inGEDPktOut.GetLength(), j=0; i>0; i--, j++)
			{
				TGEDPktOut theGEDPktOut = *inGEDPktOut[j];
				outRes += CSocket::Send (inDesc, theGEDPktOut, ::SizeOfGEDPktOut(theGEDPktOut));
			}
		}
	}

	return outRes == outHttp.GetLength() + outContentLen ? outContentLen : 0;
}

#ifdef __GED__
//---------------------------------------------------------------------------------------------------------------------------------------
// Send an http answer to given src with the following fields
//---------------------------------------------------------------------------------------------------------------------------------------
//	HTTP/1.0|1.1 200 OK
//	Date: Tue, 01 May 2007 18:37:03 GMT
//	Server: Apache/2.2.2 (Fedora) | any
//	Last-Modified: Tue, 01 May 2007 18:37:01 GMT
//	[Content-MD5: xxx]
//	Connection: keep-alive | close
//	Content-Type: image/jpeg | multipart/x-gzip
//	Content-Length: packets total length | compression size + compression itself
//	CRLF
//	packets themself
//---------------------------------------------------------------------------------------------------------------------------------------
int SendHttpGEDPktToSrc (int inDesc, SSL *inSSL, TGEDHttpAswCtx *inGEDHttpAswCtx, TBuffer <TGEDPktOut> &inGEDPktOut, CString inMd5)
{
	char inDate[256]; time_t stamp; ::time (&stamp); struct tm tm; ::gmtime_r (&stamp, &tm);
	::strftime (inDate, 256, "%Ec", &tm);

	CString outHttp ("HTTP/" + inGEDHttpAswCtx->vrs + " 200 OK" + GED_HTTP_REGEX_CRLF);
	outHttp += "Date: " + (CString(inDate)-"\n") + GED_HTTP_REGEX_CRLF;
	outHttp += "Server: " + inGEDHttpAswCtx->srv + GED_HTTP_REGEX_CRLF;
	outHttp += "Last-Modified: " + (CString(inDate)-"\n") + GED_HTTP_REGEX_CRLF;
	if (inMd5 != CString()) outHttp += "Content-MD5: " + inMd5 + GED_HTTP_REGEX_CRLF;
	outHttp += inGEDHttpAswCtx->kpalv ? "Connection: keep-alive" + GED_HTTP_REGEX_CRLF : "Connection: close" + GED_HTTP_REGEX_CRLF; 
	outHttp += "Content-Type: " + inGEDHttpAswCtx->typ + GED_HTTP_REGEX_CRLF;

	long outContentLen=0L; int outRes=0;

	if (inGEDHttpAswCtx->typ.Find(CString("zip")))
	{
		long inLen=0L; for (size_t i=inGEDPktOut.GetLength(), j=0; i>0; i--, j++)
			inLen += ::SizeOfGEDPktOut(*inGEDPktOut[j]);
		
		SInt8 *inSrc = (SInt8 *) ::malloc (inLen);
		SInt8 *inDst = (SInt8 *) ::malloc ((int)(inLen+inLen*0.001+1+12));

		for (size_t i=inGEDPktOut.GetLength(), j=0, d=0; i>0; i--, j++)
		{
			TGEDPktOut theGEDPktOut = *inGEDPktOut[j];
			::memcpy ((char *)inSrc+d, theGEDPktOut, ::SizeOfGEDPktOut(theGEDPktOut));
			d += ::SizeOfGEDPktOut(theGEDPktOut);
		}

		outContentLen=(int)(inLen+inLen*0.001+1+12); int err; if ((err = ::compress2 ((Bytef*)inDst, (uLongf*)&outContentLen, (Bytef*)inSrc, inLen, inGEDHttpAswCtx->zlv)) != Z_OK)
		{
			::free (inSrc);
			::free (inDst);

			return 0;
		}

		outHttp += "Content-Length: " + CString(outContentLen+sizeof(long)) + GED_HTTP_REGEX_CRLF + GED_HTTP_REGEX_CRLF;

		if (inSSL != NULL)
		{
			outRes  = CSocket::SendSSL (inSSL, outHttp.Get(), outHttp.GetLength());
			outRes += CSocket::SendSSL (inSSL, &inLen, sizeof(long));
			outRes += CSocket::SendSSL (inSSL, inDst, outContentLen);
		}
		else
		{
			outRes  = CSocket::Send (inDesc, outHttp.Get(), outHttp.GetLength());
			outRes += CSocket::Send (inDesc, &inLen, sizeof(long));
			outRes += CSocket::Send (inDesc, inDst, outContentLen);
		}

		outContentLen += sizeof(long);

		::free (inSrc);
		::free (inDst);
	}
	else
	{
		for (size_t i=inGEDPktOut.GetLength(), j=0; i>0; i--, j++)
			outContentLen += ::SizeOfGEDPktOut(*inGEDPktOut[j]);

		outHttp += "Content-Length: " + CString(outContentLen) + GED_HTTP_REGEX_CRLF + GED_HTTP_REGEX_CRLF;

		outRes=0; if (inSSL != NULL)
		{
			outRes = CSocket::SendSSL (inSSL, outHttp.Get(), outHttp.GetLength());
			for (size_t i=inGEDPktOut.GetLength(), j=0; i>0; i--, j++)
			{
				TGEDPktOut theGEDPktOut = *inGEDPktOut[j];
				outRes += CSocket::SendSSL (inSSL, theGEDPktOut, ::SizeOfGEDPktOut(theGEDPktOut));
			}
		}
		else
		{
			outRes = CSocket::Send (inDesc, outHttp.Get(), outHttp.GetLength());
			for (size_t i=inGEDPktOut.GetLength(), j=0; i>0; i--, j++)
			{
				TGEDPktOut theGEDPktOut = *inGEDPktOut[j];
				outRes += CSocket::Send (inDesc, theGEDPktOut, ::SizeOfGEDPktOut(theGEDPktOut));
			}
		}
	}

	return outRes == outHttp.GetLength() + outContentLen ? outContentLen : 0;
}
#endif

//---------------------------------------------------------------------------------------------------------------------------------------
// packet reception context is defined only within this section
//---------------------------------------------------------------------------------------------------------------------------------------
struct TGEDPktInCtx
{
	CString		header;
	long		length;
	long		totlbl;
	long		index;

	char *		pckto;
	char *		datao;
	char *		leno;
	long		pcktbl;
	TGEDPktIn *	packet;

	bool		lzo;
	char *		lzobuf;
	char *		lzoo;
};

//---------------------------------------------------------------------------------------------------------------------------------------
// packet reception context creation
//---------------------------------------------------------------------------------------------------------------------------------------
TGEDPktInCtx * NewGEDPktInCtx ()
{
	TGEDPktInCtx *outCtx = new TGEDPktInCtx;
	outCtx->length = -1;
	outCtx->pcktbl = -1;
	outCtx->totlbl = -1;
	outCtx->datao  = NULL;
	outCtx->packet = new TGEDPktIn;
	::bzero(outCtx->packet, sizeof(TGEDPktIn));
	outCtx->pckto  = reinterpret_cast <char *> (outCtx->packet);
	outCtx->packet->len = -1;
	outCtx->lzo    = false;
	outCtx->lzobuf = NULL;
	outCtx->lzoo   = NULL;
	outCtx->index  = 0;
	outCtx->leno   = reinterpret_cast <char *> (&(outCtx->length));
	return outCtx;
}

//---------------------------------------------------------------------------------------------------------------------------------------
// packet reception context deletion
//---------------------------------------------------------------------------------------------------------------------------------------
void DeleteGEDPktInCtx (TGEDPktInCtx *&ioCtx)
{
	::DeleteGEDPktIn (ioCtx->packet);
	if (ioCtx->lzobuf != NULL) delete [] ioCtx->lzobuf;
	delete ioCtx;
	ioCtx = NULL;
}

//---------------------------------------------------------------------------------------------------------------------------------------
// Receive raw containing a set of ged packets from given socket and specified timeout
//---------------------------------------------------------------------------------------------------------------------------------------
int RecvRawGEDPktFromSkt (int inDesc, TGEDPktInCtx *inGEDPktCtx, long inTimeOut, RecvGEDPktFromCb inCb, void *inCbData)
{
	int inRes=1; while (true)
	{
		if (inTimeOut > 0)
		{
			fd_set rfds; FD_ZERO(&rfds); FD_SET(inDesc, &rfds);

			struct timeval tv;
			tv.tv_sec  = inTimeOut;
			tv.tv_usec = 0;

			int ready = ::select (inDesc+1, &rfds, NULL, NULL, &tv);

			if (ready <= 0) return ready;
		}

		char inData[8192]; int inLen=8192; inRes = CSocket::Receive (inDesc, inData, inLen);
		if (inRes <= 0) break;

		inRes = ::RecvRawGEDPktFromBuf (inData, inRes, inGEDPktCtx, inCb, inCbData);
		if (inRes <= 0) break;

		if (!inGEDPktCtx->lzo) break;
	}

	return inRes;
}

//---------------------------------------------------------------------------------------------------------------------------------------
// Receive an http request containing a set of ged packets from given socket/ssl and specified timeout
//---------------------------------------------------------------------------------------------------------------------------------------
int RecvHttpGEDPktFromSkt (int inDesc, SSL *inSSL, TGEDPktInCtx *inGEDPktCtx, long inTimeOut, RecvGEDPktFromCb inCb, void *inCbData)
{
	int inRes=1; while (true)
	{
		if (inTimeOut > 0)
		{
			fd_set rfds; FD_ZERO(&rfds); FD_SET(inDesc, &rfds);

			struct timeval tv;
			tv.tv_sec  = inTimeOut;
			tv.tv_usec = 0;

			int ready = ::select (inDesc+1, &rfds, NULL, NULL, &tv);

			if (ready <= 0) return ready;
		}

		char inData[8192]; int inLen=8192; if (inSSL != NULL)
			inRes = CSocket::ReceiveSSL (inSSL, inData, inLen);
		else
			inRes = CSocket::Receive (inDesc, inData, inLen);
		if (inRes <= 0) break;

		inRes = ::RecvHttpGEDPktFromBuf (inData, inRes, inGEDPktCtx, inCb, inCbData);
		if (inRes <= 0) break;

		if (inGEDPktCtx->header == CString()) break;
	}

	return inRes;
}

//---------------------------------------------------------------------------------------------------------------------------------------
// Receive an http request containing a set of ged packets from given raw buffer
//---------------------------------------------------------------------------------------------------------------------------------------
int RecvHttpGEDPktFromBuf (void *inData, long inLen, TGEDPktInCtx *inGEDPktCtx, RecvGEDPktFromCb inCb, void *inCbData)
{
	int lengthleft = inLen; char *inBuffer = reinterpret_cast <char *> (inData); 

	while (lengthleft>0)
	{
		while (!inGEDPktCtx->header.Find(GED_HTTP_REGEX_CRLF+GED_HTTP_REGEX_CRLF) && lengthleft-- > 0)
			inGEDPktCtx->header += *inBuffer++;

		if (inGEDPktCtx->header.Find(GED_HTTP_REGEX_CRLF+GED_HTTP_REGEX_CRLF) && inGEDPktCtx->length < 0)
		{
			size_t n=0; if (!inGEDPktCtx->header.Find (CString("Content-Type:"), 0, &n)) return 0;
			if (CString(inGEDPktCtx->header.Get()+n).Find(CString("zip"))) inGEDPktCtx->lzo = true;
			if (!inGEDPktCtx->header.Find (CString("Content-Length:"), 0, &n)) return 0;
			n = ::sscanf (inGEDPktCtx->header.Get()+n, "%*s %ld", &(inGEDPktCtx->length));
			if (n<1 || inGEDPktCtx->length<0) return 0;
			inGEDPktCtx->totlbl = inGEDPktCtx->length;
		}

		if (inGEDPktCtx->lzo)
		{
			if (inGEDPktCtx->lzobuf == NULL)
			{
				inGEDPktCtx->lzobuf = inGEDPktCtx->length > 0 ? new char [inGEDPktCtx->length] : NULL;
				inGEDPktCtx->lzoo   = inGEDPktCtx->lzobuf;
			}

			for (; lengthleft>0 && inGEDPktCtx->totlbl>0; lengthleft--, inGEDPktCtx->totlbl--)
				*inGEDPktCtx->lzoo++ = *inBuffer++;

			if (inGEDPktCtx->totlbl == 0)
			{
				if (inGEDPktCtx->length > 4 && inCb != NULL)
				{
					SInt8 *inDst = (SInt8 *) ::malloc (*reinterpret_cast <long *> (inGEDPktCtx->lzobuf));
					unsigned long inDstLen = *reinterpret_cast <long *> (inGEDPktCtx->lzobuf);

					int err; if ((err = ::uncompress ((Bytef*)inDst, &inDstLen, (Bytef*)(inGEDPktCtx->lzobuf+sizeof(long)), inGEDPktCtx->length-sizeof(long))) != Z_OK)
					{
						inGEDPktCtx->header = CString();
						inGEDPktCtx->lzo    = false;
						inGEDPktCtx->lzoo   = NULL;
						inGEDPktCtx->length = -1;
						inGEDPktCtx->totlbl = -1;
						if (inGEDPktCtx->lzobuf != NULL) delete [] inGEDPktCtx->lzobuf; 
						inGEDPktCtx->lzobuf = NULL;
						inGEDPktCtx->index  = 0;

						::free (inDst);

						return 0;
					}

					inGEDPktCtx->lzoo = inDst;
					while (inGEDPktCtx->lzoo < inDst+inDstLen)
					{
						::memcpy (inGEDPktCtx->packet, inGEDPktCtx->lzoo, GED_PKT_FIX_SIZE);
						inGEDPktCtx->lzoo += GED_PKT_FIX_SIZE;

						if (inGEDPktCtx->packet->len > 0)
						{
							inGEDPktCtx->packet->data = inGEDPktCtx->lzoo;
							inGEDPktCtx->lzoo += inGEDPktCtx->packet->len;
						}

						inCb (inGEDPktCtx->header, inGEDPktCtx->index++, 
						      inGEDPktCtx->lzoo>=(char*)inDst+inDstLen?true:false, inGEDPktCtx->packet, inCbData);

						inGEDPktCtx->packet->data = NULL;
						inGEDPktCtx->packet->len  = -1;
					}

					::free (inDst);
				}

				inGEDPktCtx->header = CString();
				inGEDPktCtx->lzo    = false;
				inGEDPktCtx->lzoo   = NULL;
				inGEDPktCtx->length = -1;
				inGEDPktCtx->totlbl = -1;
				if (inGEDPktCtx->lzobuf != NULL) delete [] inGEDPktCtx->lzobuf; 
				inGEDPktCtx->lzobuf = NULL;
				inGEDPktCtx->index  = 0;
			}
		}
		else
		{
			for (; lengthleft>0 && (inGEDPktCtx->pckto-(char*)inGEDPktCtx->packet)<GED_PKT_FIX_SIZE;
				lengthleft--, inGEDPktCtx->totlbl--) *inGEDPktCtx->pckto++ = *inBuffer++;

			if (inGEDPktCtx->packet->data == NULL && (inGEDPktCtx->pckto-(char*)inGEDPktCtx->packet) >= GED_PKT_FIX_SIZE)
			{
				inGEDPktCtx->packet->data = inGEDPktCtx->packet->len > 0 ? new char [inGEDPktCtx->packet->len] : NULL;
				inGEDPktCtx->pcktbl	  = inGEDPktCtx->packet->len;
				inGEDPktCtx->datao	  = reinterpret_cast <char *> (inGEDPktCtx->packet->data);
			} 

			if (inGEDPktCtx->packet->data != NULL || inGEDPktCtx->packet->len == 0)
			{
				for (int i=lengthleft; i>0 && inGEDPktCtx->pcktbl>0; i--, inGEDPktCtx->pcktbl--, inGEDPktCtx->totlbl--, 
					lengthleft--) *inGEDPktCtx->datao++ = *inBuffer++;

				if (inGEDPktCtx->pcktbl == 0)
				{
					if (inCb != NULL) inCb (inGEDPktCtx->header, inGEDPktCtx->index++, 
								inGEDPktCtx->totlbl==0?true:false,
								inGEDPktCtx->packet, inCbData);

					if (inGEDPktCtx->packet->data != NULL) 
						delete [] reinterpret_cast <char *> (inGEDPktCtx->packet->data); 
					::bzero (inGEDPktCtx->packet, sizeof(TGEDPktIn));
					inGEDPktCtx->packet->len = -1;
					inGEDPktCtx->pckto  = reinterpret_cast <char *> (inGEDPktCtx->packet);
					inGEDPktCtx->pcktbl = -1;
					inGEDPktCtx->datao  = NULL;
				}

				if (inGEDPktCtx->totlbl == 0)
				{
					inGEDPktCtx->header = CString();
					inGEDPktCtx->lzo    = false;
					inGEDPktCtx->length = -1;
					inGEDPktCtx->totlbl = -1;
					inGEDPktCtx->index  = 0;
				}
			}
		}
	}

	return inLen;
}

//---------------------------------------------------------------------------------------------------------------------------------------
// receive raw data containing a set of ged packets from given raw buffer
//---------------------------------------------------------------------------------------------------------------------------------------
int RecvRawGEDPktFromBuf (void *inData, long inLen, TGEDPktInCtx *inGEDPktCtx, RecvGEDPktFromCb inCb, void *inCbData)
{
	int lengthleft = inLen; char *inBuffer = reinterpret_cast <char *> (inData); 

	while (lengthleft > 0)
	{
		for (; (inGEDPktCtx->leno-((char*)(&(inGEDPktCtx->length))) < sizeof(long)) && (lengthleft > 0); lengthleft--)
			*inGEDPktCtx->leno++ = *inBuffer++;

		if ((inGEDPktCtx->lzobuf == NULL) && (inGEDPktCtx->leno-((char*)(&(inGEDPktCtx->length))) >= sizeof(long)))
		{
			inGEDPktCtx->totlbl = inGEDPktCtx->length;
			inGEDPktCtx->lzobuf = new char [inGEDPktCtx->length];
			inGEDPktCtx->lzoo   = inGEDPktCtx->lzobuf;
		}

		for (; lengthleft>0 && inGEDPktCtx->totlbl>0; lengthleft--, inGEDPktCtx->totlbl--)
			*inGEDPktCtx->lzoo++ = *inBuffer++;

		if (inGEDPktCtx->totlbl == 0)
		{
			inGEDPktCtx->lzoo = inGEDPktCtx->lzobuf;

			while (inGEDPktCtx->lzoo < (inGEDPktCtx->lzobuf+inGEDPktCtx->length))
			{
				::memcpy (inGEDPktCtx->packet, inGEDPktCtx->lzoo, GED_PKT_FIX_SIZE);
				inGEDPktCtx->lzoo += GED_PKT_FIX_SIZE;

				if (inGEDPktCtx->packet->len > 0)
				{
					inGEDPktCtx->packet->data = inGEDPktCtx->lzoo;
					inGEDPktCtx->lzoo += inGEDPktCtx->packet->len;
				}

				inCb (inGEDPktCtx->header, inGEDPktCtx->index++, inGEDPktCtx->lzoo>=(char*)(inGEDPktCtx->lzobuf+inGEDPktCtx->length)?true:false, 
				      inGEDPktCtx->packet, inCbData);

				inGEDPktCtx->packet->data = NULL;
				inGEDPktCtx->packet->len  = -1;
			}

			inGEDPktCtx->lzo    = false;
			inGEDPktCtx->lzoo   = NULL;
			inGEDPktCtx->length = -1;
			inGEDPktCtx->totlbl = -1;
			if (inGEDPktCtx->lzobuf != NULL) delete [] inGEDPktCtx->lzobuf; 
			inGEDPktCtx->lzobuf = NULL;
			inGEDPktCtx->index  = 0;
			inGEDPktCtx->leno   = reinterpret_cast <char *> (&(inGEDPktCtx->length));
		}
	}

	return inLen;
}

//---------------------------------------------------------------------------------------------------------------------------------------
// < operator (used for pkt cfg hash coherency)
//---------------------------------------------------------------------------------------------------------------------------------------
bool TGEDPktCfg::operator < (const TGEDPktCfg &inGEDPktCfg) const
{
	return type < inGEDPktCfg.type;
}

// Table with characters for base64 transformation.
const static char ch[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

//---------------------------------------------------------------------------------------------------------------------------------------
// base64 conversion utility
//---------------------------------------------------------------------------------------------------------------------------------------
void b64_encode (GString *out, char *value, unsigned int vlen)
{
        unsigned int i=0, j=0;
        char chin[4], chout[5]; chin[0] = chin[1] = chin[2] = 0; chout[0] = chout[1] = chout[2] = chout[3] = chout[4] = 0;

        for (i=0; i<vlen; i++)
        {
                chin [j++] = value[i];
                chout[0] = ch[(chin [0] >> 2) & 0x3f];
                chout[1] = ch[((chin[0] << 4) & 0x30) | ((chin[1] >> 4) & 0x0f)];
                chout[2] = ch[((chin[1] << 2) & 0x3c) | ((chin[2] >> 6) & 0x03)];
                if (j == 3)
                {
                        chout[3] = ch[chin[2] & 0x3f];
                        g_string_append (out, chout);
                        chin [0] = chin [1] = chin [2] = 0;
                        chout[0] = chout[1] = chout[2] = chout[3] = 0;
                        j = 0;
                }
        }

        if (j == 1) chout[2] = chout[3] = '=';
        else if (j == 2) chout[3] = '=';

        g_string_append (out, chout);
}

//----------------------------------------------------------------------------------------------------------------------------------------
// md5 hash
//----------------------------------------------------------------------------------------------------------------------------------------
GByteArray * hash_md5 (gchar const* data, gsize length)
{
	static pthread_mutex_t hash_mutex;
	static bool init=false;

	if (!init)
	{
		::pthread_mutex_init (&hash_mutex, NULL); 
		init = true;
	}

	if (::pthread_mutex_lock (&hash_mutex) == 0)
	{
	        size_t hash_len = gcry_md_get_algo_dlen (GCRY_MD_MD5);
		gchar *buffer = g_new0 (gchar, hash_len);
		gcry_md_hash_buffer (GCRY_MD_MD5, buffer, data, length);

		GString *b64 = g_string_new ("");
		b64_encode (b64, buffer, hash_len);
		g_free (buffer);

		GByteArray *array = g_byte_array_new();
		g_byte_array_append (array, (guchar*)b64->str, b64->len);
		g_byte_array_append (array, (guchar*)"\0", 1);

		g_string_free (b64, true);

		::pthread_mutex_unlock (&hash_mutex);

	        return array;
	}

	return NULL;
}

//----------------------------------------------------------------------------------------------------------------------------------------
// packets cfg hash
//----------------------------------------------------------------------------------------------------------------------------------------
CString HashGEDPktCfg (const TBuffer <TGEDPktCfg> &inGEDPktCfg)
{
	CString inHash; for (size_t i=inGEDPktCfg.GetLength(); i>0; i--)
	{
		inHash += inGEDPktCfg[i-1]->name;
		inHash += CString((long)inGEDPktCfg[i-1]->type);
		for (size_t j=inGEDPktCfg[i-1]->fields.GetLength(); j>0; j--)
		{
			inHash += inGEDPktCfg[i-1]->fields[j-1]->name;
			inHash += CString((long)inGEDPktCfg[i-1]->fields[j-1]->type);
			inHash += CString((long)inGEDPktCfg[i-1]->fields[j-1]->meta);
		}
		for (size_t j=inGEDPktCfg[i-1]->keyidc.GetLength(); j>0; j--)
			inHash += CString((long)*inGEDPktCfg[i-1]->keyidc[j-1]);
	}

	GByteArray *inh = hash_md5 (inHash.Get(), inHash.GetLength());
	CString outHash ((inh!=NULL)?(char*)inh->data:NULL);
	if (inh != NULL) g_byte_array_free (inh, true);

	return outHash;
}

//----------------------------------------------------------------------------------------------------------------------------------------
// packet cfg hash
//----------------------------------------------------------------------------------------------------------------------------------------
CString HashGEDPktCfg (const TGEDPktCfg *inGEDPktCfg)
{
	if (inGEDPktCfg == NULL) return CString();

	CString inHash (inGEDPktCfg->name);
	inHash += CString((long)inGEDPktCfg->type);
	for (size_t j=inGEDPktCfg->fields.GetLength(); j>0; j--)
	{
		inHash += inGEDPktCfg->fields[j-1]->name;
		inHash += CString((long)inGEDPktCfg->fields[j-1]->type);
		inHash += CString((long)inGEDPktCfg->fields[j-1]->meta);
	}
	for (size_t j=inGEDPktCfg->keyidc.GetLength(); j>0; j--)
		inHash += CString((long)*inGEDPktCfg->keyidc[j-1]);

	GByteArray *inh = hash_md5 (inHash.Get(), inHash.GetLength());
	CString outHash ((inh!=NULL)?(char*)inh->data:NULL);
	if (inh != NULL) g_byte_array_free (inh, true);

	return outHash;
}

//----------------------------------------------------------------------------------------------------------------------------------------
// get a random hash
//----------------------------------------------------------------------------------------------------------------------------------------
CString GetGEDRandomHash ()
{
	char inSalt[9]; inSalt[8]=0; ::RAND_pseudo_bytes((unsigned char *)inSalt, 8);
	time_t tm; ::time (&tm); ::srandom (tm); for (size_t i=9; i>0; i--) ::srandom (::random()*inSalt[i-1]);
	CString inHash; for (size_t i=0; i<10; i++)
	{
		unsigned long n = ::random();
		inHash += CString(n);
	}
	GByteArray *inh = hash_md5 (inHash.Get(), inHash.GetLength());
	CString outHash ((inh!=NULL)?(char*)inh->data:NULL);
	if (inh != NULL) g_byte_array_free (inh, true);
	return outHash;
}

#ifdef __GED__

//----------------------------------------------------------------------------------------------------------------------------------------
// quick sort commodity functions (C.A.R. Hoare algorithm)
//----------------------------------------------------------------------------------------------------------------------------------------

int PartGEDARcdsAsc (TBuffer <TGEDRcd *> &ioGEDARcds, const int p, const int r)
{
	TGEDARcd *pv = static_cast <TGEDARcd *> (*ioGEDARcds[p]), *tmp; int i=p-1, j=r+1;

	while (true)
	{
		do
		{
			tmp = static_cast <TGEDARcd *> (*ioGEDARcds[--j]);
		}
		while (tmp->otv.tv_sec > pv->otv.tv_sec || (tmp->otv.tv_sec == pv->otv.tv_sec && tmp->otv.tv_usec > pv->otv.tv_usec));

		do
		{
			tmp = static_cast <TGEDARcd *> (*ioGEDARcds[++i]);
		} 
		while (tmp->otv.tv_sec < pv->otv.tv_sec || (tmp->otv.tv_sec == pv->otv.tv_sec && tmp->otv.tv_usec < pv->otv.tv_usec));

		if (i < j)
		{
			tmp = static_cast <TGEDARcd *> (*ioGEDARcds[i]);
			*ioGEDARcds[i] = *ioGEDARcds[j];
			*ioGEDARcds[j] = tmp;
		}
		else
			return j;
	}

	return j;
}

void QuickSortGEDARcdsAsc (TBuffer <TGEDRcd *> &ioGEDARcds, const int p, const int r)
{
	if (p < r)
	{
		int q = PartGEDARcdsAsc (ioGEDARcds, p, r);
		QuickSortGEDARcdsAsc (ioGEDARcds, p, q);
		QuickSortGEDARcdsAsc (ioGEDARcds, q+1, r);
        }
}

int PartGEDARcdsDsc (TBuffer <TGEDRcd *> &ioGEDARcds, const int p, const int r)
{
	TGEDARcd *pv = static_cast <TGEDARcd *> (*ioGEDARcds[p]), *tmp; int i=p-1, j=r+1;

	while (true)
	{
		do
		{
			tmp = static_cast <TGEDARcd *> (*ioGEDARcds[--j]);
		}
		while (tmp->otv.tv_sec < pv->otv.tv_sec || (tmp->otv.tv_sec == pv->otv.tv_sec && tmp->otv.tv_usec < pv->otv.tv_usec));

		do
		{
			tmp = static_cast <TGEDARcd *> (*ioGEDARcds[++i]);
		} 
		while (tmp->otv.tv_sec > pv->otv.tv_sec || (tmp->otv.tv_sec == pv->otv.tv_sec && tmp->otv.tv_usec > pv->otv.tv_usec));

		if (i < j)
		{
			tmp = static_cast <TGEDARcd *> (*ioGEDARcds[i]);
			*ioGEDARcds[i] = *ioGEDARcds[j];
			*ioGEDARcds[j] = tmp;
		}
		else
			return j;
	}

	return j;
}

void QuickSortGEDARcdsDsc (TBuffer <TGEDRcd *> &ioGEDARcds, const int p, const int r)
{
	if (p < r)
	{
		int q = PartGEDARcdsDsc (ioGEDARcds, p, r);
		QuickSortGEDARcdsDsc (ioGEDARcds, p, q);
		QuickSortGEDARcdsDsc (ioGEDARcds, q+1, r);
        }
}

int PartGEDSRcdsAsc (TBuffer <TGEDRcd *> &ioGEDSRcds, const int p, const int r)
{
	TGEDSRcd *pv = static_cast <TGEDSRcd *> (*ioGEDSRcds[p]), *tmp; int i=p-1, j=r+1;

	while (true)
	{
		do
		{
			tmp = static_cast <TGEDSRcd *> (*ioGEDSRcds[--j]);
		}
		while (tmp->pkt->tv.tv_sec > pv->pkt->tv.tv_sec || (tmp->pkt->tv.tv_sec == pv->pkt->tv.tv_sec && tmp->pkt->tv.tv_usec > pv->pkt->tv.tv_usec));

		do
		{
			tmp = static_cast <TGEDSRcd *> (*ioGEDSRcds[++i]);
		} 
		while (tmp->pkt->tv.tv_sec < pv->pkt->tv.tv_sec || (tmp->pkt->tv.tv_sec == pv->pkt->tv.tv_sec && tmp->pkt->tv.tv_usec < pv->pkt->tv.tv_usec));

		if (i < j)
		{
			tmp = static_cast <TGEDSRcd *> (*ioGEDSRcds[i]);
			*ioGEDSRcds[i] = *ioGEDSRcds[j];
			*ioGEDSRcds[j] = tmp;
		}
		else
			return j;
	}

	return j;
}

void QuickSortGEDSRcdsAsc (TBuffer <TGEDRcd *> &ioGEDSRcds, const int p, const int r)
{
	if (p < r)
	{
		int q = PartGEDSRcdsAsc (ioGEDSRcds, p, r);
		QuickSortGEDSRcdsAsc (ioGEDSRcds, p, q);
		QuickSortGEDSRcdsAsc (ioGEDSRcds, q+1, r);
        }
}

int PartGEDSRcdsDsc (TBuffer <TGEDRcd *> &ioGEDSRcds, const int p, const int r)
{
	TGEDSRcd *pv = static_cast <TGEDSRcd *> (*ioGEDSRcds[p]), *tmp; int i=p-1, j=r+1;

	while (true)
	{
		do
		{
			tmp = static_cast <TGEDSRcd *> (*ioGEDSRcds[--j]);
		}
		while (tmp->pkt->tv.tv_sec < pv->pkt->tv.tv_sec || (tmp->pkt->tv.tv_sec == pv->pkt->tv.tv_sec && tmp->pkt->tv.tv_usec < pv->pkt->tv.tv_usec));

		do
		{
			tmp = static_cast <TGEDSRcd *> (*ioGEDSRcds[++i]);
		} 
		while (tmp->pkt->tv.tv_sec > pv->pkt->tv.tv_sec || (tmp->pkt->tv.tv_sec == pv->pkt->tv.tv_sec && tmp->pkt->tv.tv_usec > pv->pkt->tv.tv_usec));

		if (i < j)
		{
			tmp = static_cast <TGEDSRcd *> (*ioGEDSRcds[i]);
			*ioGEDSRcds[i] = *ioGEDSRcds[j];
			*ioGEDSRcds[j] = tmp;
		}
		else
			return j;
	}

	return j;
}

void QuickSortGEDSRcdsDsc (TBuffer <TGEDRcd *> &ioGEDSRcds, const int p, const int r)
{
	if (p < r)
	{
		int q = PartGEDSRcdsDsc (ioGEDSRcds, p, r);
		QuickSortGEDSRcdsDsc (ioGEDSRcds, p, q);
		QuickSortGEDSRcdsDsc (ioGEDSRcds, q+1, r);
        }
}

int PartGEDHRcdsAsc (TBuffer <TGEDRcd *> &ioGEDHRcds, const int p, const int r)
{
	TGEDHRcd *pv = static_cast <TGEDHRcd *> (*ioGEDHRcds[p]), *tmp; int i=p-1, j=r+1;

	while (true)
	{
		do
		{
			tmp = static_cast <TGEDHRcd *> (*ioGEDHRcds[--j]);
		}
		while (tmp->otv.tv_sec > pv->otv.tv_sec || (tmp->otv.tv_sec == pv->otv.tv_sec && tmp->otv.tv_usec > pv->otv.tv_usec));

		do
		{
			tmp = static_cast <TGEDHRcd *> (*ioGEDHRcds[++i]);
		} 
		while (tmp->otv.tv_sec < pv->otv.tv_sec || (tmp->otv.tv_sec == pv->otv.tv_sec && tmp->otv.tv_usec < pv->otv.tv_usec));

		if (i < j)
		{
			tmp = static_cast <TGEDHRcd *> (*ioGEDHRcds[i]);
			*ioGEDHRcds[i] = *ioGEDHRcds[j];
			*ioGEDHRcds[j] = tmp;
		}
		else
			return j;
	}

	return j;
}

void QuickSortGEDHRcdsAsc (TBuffer <TGEDRcd *> &ioGEDHRcds, const int p, const int r)
{
	if (p < r)
	{
		int q = PartGEDHRcdsAsc (ioGEDHRcds, p, r);
		QuickSortGEDHRcdsAsc (ioGEDHRcds, p, q);
		QuickSortGEDHRcdsAsc (ioGEDHRcds, q+1, r);
        }
}

int PartGEDHRcdsDsc (TBuffer <TGEDRcd *> &ioGEDHRcds, const int p, const int r)
{
	TGEDHRcd *pv = static_cast <TGEDHRcd *> (*ioGEDHRcds[p]), *tmp; int i=p-1, j=r+1;

	while (true)
	{
		do
		{
			tmp = static_cast <TGEDHRcd *> (*ioGEDHRcds[--j]);
		}
		while (tmp->otv.tv_sec < pv->otv.tv_sec || (tmp->otv.tv_sec == pv->otv.tv_sec && tmp->otv.tv_usec < pv->otv.tv_usec));

		do
		{
			tmp = static_cast <TGEDHRcd *> (*ioGEDHRcds[++i]);
		} 
		while (tmp->otv.tv_sec > pv->otv.tv_sec || (tmp->otv.tv_sec == pv->otv.tv_sec && tmp->otv.tv_usec > pv->otv.tv_usec));

		if (i < j)
		{
			tmp = static_cast <TGEDHRcd *> (*ioGEDHRcds[i]);
			*ioGEDHRcds[i] = *ioGEDHRcds[j];
			*ioGEDHRcds[j] = tmp;
		}
		else
			return j;
	}

	return j;
}

void QuickSortGEDHRcdsDsc (TBuffer <TGEDRcd *> &ioGEDHRcds, const int p, const int r)
{
	if (p < r)
	{
		int q = PartGEDHRcdsDsc (ioGEDHRcds, p, r);
		QuickSortGEDHRcdsDsc (ioGEDHRcds, p, q);
		QuickSortGEDHRcdsDsc (ioGEDHRcds, q+1, r);
        }
}

#endif

//----------------------------------------------------------------------------------------------------------------------------------------
// base 64 utilities
//----------------------------------------------------------------------------------------------------------------------------------------

CString b64_encode (char *value, unsigned int vlen)
{
	char chin[4]; chin[0]=chin[1]=chin[2]=0; 
	char chout[5]; chout[0]=chout[1]=chout[2]=chout[3]=chout[4]=0;

     	unsigned int i=0,j=0; CString out; for (i=0; i<vlen; i++) 
	{
		chin [j++] = value[i];
		chout[0] = ch[(chin [0] >> 2) & 0x3f];
		chout[1] = ch[((chin[0] << 4) & 0x30) | ((chin[1] >> 4) & 0x0f)];
		chout[2] = ch[((chin[1] << 2) & 0x3c) | ((chin[2] >> 6) & 0x03)];
		if (j == 3)
		{
			chout[3] = ch[chin[2] & 0x3f];
			out += chout;
			chin [0] = chin [1] = chin [2] = 0;
			chout[0] = chout[1] = chout[2] = chout[3] = 0;
			j = 0;
	  	}
     	}

     	if (j == 1) 
		chout[2] = chout[3] = '=';
	else if (j == 2) 
		chout[3] = '=';

	out += chout;

	return out;
}

CString Base64Encode (const CString &inStr)
{
	return b64_encode (inStr.Get(), inStr.GetLength());
}

#ifdef __GEDQ__

//----------------------------------------------------------------------------------------------------------------------------------------
// UTF8 utility
//----------------------------------------------------------------------------------------------------------------------------------------
static int ShiftLeft (int c) 
{
	int n=0; for (n=0; (c&0x80)==0x80; c<<=1) ++n; return n;
}

int IsUtf8 (const CString &text)
{
	int rbytes=0; size_t i=0; while (i < text.GetLength())
	{
		int c = *text[i++];
		int n = ShiftLeft (c);
		if (rbytes > 0)
		{
			if (n == 1)
			{
				rbytes--;
			}
			else
			{
				return i;
			}
		}
		else if (n == 0)
		{
			continue;
		}
		else if (n == 1)
		{
			return i;
		}
		else
		{
			rbytes = n-1;
		}
	}
	
	if (rbytes > 0)
	{
		return i;
	}

	return 0;
}

#endif

//----------------------------------------------------------------------------------------------------------------------------------------
// NTLM utilities
//----------------------------------------------------------------------------------------------------------------------------------------

#ifdef __GED_NTLM__

/*  SECTION Copyright (C) 2004 William Preston */

static char base64_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static int pos (char c)
{
    char *p;
    for (p = base64_chars; *p; p++)
	if (*p == c)
	    return p - base64_chars;
    return -1;
}

#define DECODE_ERROR 0xffffffff

static unsigned int token_decode (const char *token)
{
    int i;
    unsigned int val = 0;
    int marker = 0;
    if (strlen(token) < 4)
	return DECODE_ERROR;
    for (i = 0; i < 4; i++) {
	val *= 64;
	if (token[i] == '=')
	    marker++;
	else if (marker > 0)
	    return DECODE_ERROR;
	else
	    val += pos(token[i]);
    }
    if (marker > 2)
	return DECODE_ERROR;
    return (marker << 24) | val;
}

int base64_decode (const char *str, void *data)
{
    const char *p;
    unsigned char *q;

    q = (unsigned char*)data;
    for (p = str; *p && (*p == '=' || strchr(base64_chars, *p)); p += 4) {
	unsigned int val = token_decode(p);
	unsigned int marker = (val >> 24) & 0xff;
	if (val == DECODE_ERROR)
	    return -1;
	*q++ = (val >> 16) & 0xff;
	if (marker < 2)
	    *q++ = (val >> 8) & 0xff;
	if (marker < 1)
	    *q++ = val & 0xff;
    }
    return q - (unsigned char *) data;
}

//----------------------------------------------------------------------------------------------------------------------------------------
// NTLM phase 1 challenge
//----------------------------------------------------------------------------------------------------------------------------------------
CString GetHttpProxyAuthNTLM1 ()
{
	// try a minimal NTLM handshake, see http://davenport.sourceforge.net/ntlm.html
	// the b64 str contains only the NTLMSSP signature, the NTLM message type, and the minimal 
	// set of flags (Negotiate NTLM and Negotiate OEM)
	return CString("TlRMTVNTUAABAAAAAgIAAA==");
}

static void gen_md4_hash (const char* data, int data_len, char *result)
{
	MD4_CTX c;
	char md[16];

	MD4_Init (&c);
	MD4_Update (&c, data, data_len);
	MD4_Final ((unsigned char *)md, &c);

	memcpy (result, md, 16);
}

static int unicodize (char *dst, const char *src)
{
	int i = 0;
	do
	{
		dst[i++] = *src;
		dst[i++] = 0;
	}
	while (*src++);

	return i;
}

static void create_des_keys(const unsigned char *hash, unsigned char *key)
{
	key[0] = hash[0];
	key[1] = ((hash[0]&1)<<7)|(hash[1]>>1);
	key[2] = ((hash[1]&3)<<6)|(hash[2]>>2);
	key[3] = ((hash[2]&7)<<5)|(hash[3]>>3);
	key[4] = ((hash[3]&15)<<4)|(hash[4]>>4);
	key[5] = ((hash[4]&31)<<3)|(hash[5]>>5);
	key[6] = ((hash[5]&63)<<2)|(hash[6]>>6);
	key[7] = ((hash[6]&127)<<1);
	des_set_odd_parity((DES_cblock *)key);
}

//----------------------------------------------------------------------------------------------------------------------------------------
// NTLM phase 3 auth
//----------------------------------------------------------------------------------------------------------------------------------------
CString GetHttpProxyAuthNTLM3 (const CString &inUser, const CString &inPass, const CString &inNTLM2)
{
	char pwbuf[inUser.GetLength()*2];
	char buf2[128];
	char phase3[146];

	char md4_hash[21];
	char challenge[8], response[24];
	int i, buflen;
	DES_cblock key1, key2, key3;
	des_key_schedule sched1, sched2, sched3;

	gen_md4_hash (pwbuf, unicodize (pwbuf, inPass.Get()) - 2, md4_hash);

	memset (md4_hash + 16, 0, 5);

	base64_decode (inNTLM2.Get(), (void *)buf2);

	for (i=0; i<8; i++) challenge[i] = buf2[i+24];

	create_des_keys ((unsigned char *)md4_hash, key1);
	des_set_key_unchecked ((des_cblock *)key1, sched1);
	des_ecb_encrypt ((des_cblock *)challenge, (des_cblock *)response, sched1, DES_ENCRYPT);

	create_des_keys ((unsigned char *)&(md4_hash[7]), key2);
	des_set_key_unchecked ((des_cblock *)key2, sched2);
	des_ecb_encrypt ((des_cblock *)challenge, (des_cblock *)&(response[8]), sched2, DES_ENCRYPT);

	create_des_keys ((unsigned char *)&(md4_hash[14]), key3);
	des_set_key_unchecked ((des_cblock *)key3, sched3);
	des_ecb_encrypt ((des_cblock *)challenge, (des_cblock *)&(response[16]), sched3, DES_ENCRYPT);

	memset (phase3, 0, sizeof (phase3));

	strcpy (phase3, "NTLMSSP\0");
	phase3[8] = 3;

	buflen = 0x58 + inUser.GetLength();
	if (buflen > (int) sizeof (phase3))
	buflen = sizeof (phase3);

	phase3[0x10] = buflen; 
	phase3[0x20] = buflen;
	phase3[0x30] = buflen;
	phase3[0x38] = buflen;

	phase3[0x14] = 24;
	phase3[0x16] = phase3[0x14];
	phase3[0x18] = 0x40;
	memcpy (&(phase3[0x40]), response, 24);

	phase3[0x24] = inUser.GetLength();
	phase3[0x26] = phase3[0x24];
	phase3[0x28] = 0x58;
	strncpy (&(phase3[0x58]), inUser.Get(), sizeof (phase3) - 0x58);

	phase3[0x3c] = 0x02; 
	phase3[0x3d] = 0x02;

	return b64_encode (phase3, buflen);
}

#endif


