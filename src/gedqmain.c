/****************************************************************************************************************************************
 gedqmain.c
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

//---------------------------------------------------------------------------------------------------------------------------------------
// gedq usage
//---------------------------------------------------------------------------------------------------------------------------------------
void DisplayHelp (const CString &inCmd)
{
	::fprintf (stdout, "%s [--help | -h] [-c <gedq.cfg>] [options] command [<fields>]\n", inCmd.Get());
	::fprintf (stdout, "-> where options might be :\n");
	::fprintf (stdout, "\t-v           print version and exit\n");
	::fprintf (stdout, "\t-b           addr:port or socket file to bind to\n");
	::fprintf (stdout, "\t-http-proxy  addr:port auth [user pass] where auth might be none|basic"
	#ifdef __GED_NTLM__
	"|ntlm"
	#endif
	"\n");
	::fprintf (stdout, "\t-ack         seconds\n");
	::fprintf (stdout, "\t-s           asc*|desc (peek only)\n");
	::fprintf (stdout, "\t-tm          secmin secmax (peek only)\n");
	::fprintf (stdout, "\t-n           offset number (peek only)\n");
	::fprintf (stdout, "\t-l           light output  (peek only)\n");
	::fprintf (stdout, "\t-nosync      flag packet not to be sync (push/update/drop only)\n");
	::fprintf (stdout, "\t-ca          file\n");
	::fprintf (stdout, "\t-crt         file\n");
	::fprintf (stdout, "\t-key         file\n");
	::fprintf (stdout, "\t-cipher      chain\n");
	::fprintf (stdout, "-> where command might be :\n");
	::fprintf (stdout, "\t-push   -type x  <fields>\n");
	::fprintf (stdout, "\t-update -type x  <fields>\n");
	::fprintf (stdout, "\t-drop   -type x  <-queue active*|sync|history> [key_fields]\n");
	::fprintf (stdout, "\t-drop   -id   x  <-queue history> where x might be a coma separated list\n");
	::fprintf (stdout, "\t-peek  [-type x] [-queue active*|sync|history] [key_fields]\n");
	#ifdef __GED_TUN__
	::fprintf (stdout, "\t-tun     localport:foreignhost:foreignport\n");
	#endif
}

//---------------------------------------------------------------------------------------------------------------------------------------
// execution entry point
//---------------------------------------------------------------------------------------------------------------------------------------
int main (int argc, char **argv)
{
	CString inCfgFile (GEDQCfgFileDefault); TGEDQCfg inGEDQCfg;
	CString inAddr, inPAddr; UInt16 inPort=0L, inPPort=0L; int inPAuth=GED_HTTP_PROXY_AUTH_NONE; CString inPUser, inPPass, inSock;
	CString inCa, inCrt, inKey, inCipher, inAck, inTun; CString inTm1, inTm2, inOff, inNum;
	long inReq=GED_PKT_REQ_NONE, inReqSrt=GED_PKT_REQ_PEEK_SORT_ASC, inType=GED_PKT_TYP_ANY, inReqNoSync=0L; TBuffer <UInt32> inId;

	CString inArgv0 (argv[0]);

	size_t i=1; for (; i<argc; i++)
	{
		CString inArgv (argv[i]);

		if (inArgv == CString("-c"))
		{
			if (i<argc-1) 
			{
				inCfgFile = argv[++i]; 
			}
			else
			{
				DisplayHelp (inArgv0);
				return 1;
			}
		}
		else if (inArgv == CString("-s"))
		{
			if (i<argc-1)
			{
				CString inArgv1 (argv[++i]);
				if (inArgv1 == CString("asc"))
				{
					inReqSrt = GED_PKT_REQ_PEEK_SORT_ASC;
				}
				else if (inArgv1 == CString("desc"))
				{
					inReqSrt = GED_PKT_REQ_PEEK_SORT_DSC;
				}
				else
				{
					DisplayHelp (inArgv0);
					return 1;
				}
			}
			else
			{
				DisplayHelp (inArgv0);
				return 1;
			}
		}
		else if (inArgv == CString("-o"))
		{
			if (i<argc-1)
			{
				::fprintf (stdout, "the -o option is deprecated, xml output only is handled since ged v1.2\n");
			}
			else
			{
				DisplayHelp (inArgv0);
				return 1;
			}
		}
		else if (inArgv == CString("-nosync"))
		{
			inReqNoSync |= GED_PKT_REQ_NO_SYNC;
		}
		else if (inArgv == CString("-tm"))
		{
			if (i<argc-2)
			{
				inTm1 = argv[++i];
				inTm2 = argv[++i];
			}
			else
			{
				DisplayHelp (inArgv0);
				return 1;
			}
		}
		else if (inArgv == CString("-n"))
		{
			if (i<argc-2)
			{
				inOff = argv[++i];
				inNum = argv[++i];
			}
			else
			{
				DisplayHelp (inArgv0);
				return 1;
			}
		}
		else if (inArgv == CString("-l"))
		{
			CGEDQCtx::m_Light = true;
		}
		else if (inArgv == CString("-b"))
		{
			if (i<argc-1)
			{
				CString inArgv1 (argv[++i]);
				if (inArgv1.Find(CString(":")))
				{
					inAddr = *inArgv1.Cut (CString(":"))[0];
					inPort =  inArgv1.Cut (CString(":"))[1]->ToULong();
				}
				else if (inArgv1.Find(CString("/")))
				{
					inSock = inArgv1;
				}
				else
				{
					DisplayHelp (inArgv0);
					return 1;
				}
			}
			else
			{
				DisplayHelp (inArgv0);
				return 1;
			}
		}
		else if (inArgv == CString("-http-proxy"))
		{
			if (i<argc-2)
			{
				CString inArgv1 (argv[++i]);
				if (!inArgv1.Find(CString(":")))
				{
					DisplayHelp (inArgv0);
					return 1;
				}
				inPAddr = *inArgv1.Cut (CString(":"))[0];
				inPPort =  inArgv1.Cut (CString(":"))[1]->ToULong();
				CString inArgv2 (argv[++i]);
				if (inArgv2 == "basic")
				{
					inPAuth = GED_HTTP_PROXY_AUTH_BASIC;
					if (i < argc-2)
					{
						inPUser = argv[++i];
						inPPass = argv[++i];
					}
					else
					{
						DisplayHelp (inArgv0);
						return 1;
					}
				}
				#ifdef __GED_NTLM__
				else if (inArgv2 == "ntlm")
				{
					inPAuth = GED_HTTP_PROXY_AUTH_NTLM;
					if (i < argc-2)
					{
						inPUser = argv[++i];
						inPPass = argv[++i];
					}
					else
					{
						DisplayHelp (inArgv0);
						return 1;
					}
				}
				#endif
				else if (inArgv2 != "none")
				{
					DisplayHelp (inArgv0);
					return 1;
				}
			}
			else
			{
				DisplayHelp (inArgv0);
				return 1;
			}
		}
		else if (inArgv == CString("-ack"))
		{
			if (i<argc-1)
			{
				inAck = CString(argv[++i]);
			}
			else
			{
				DisplayHelp (inArgv0);
				return 1;
			}
		}
		else if (inArgv == CString("-v"))
		{
			::fprintf (stdout, "%s %s "
			#if defined(__GED_NTLM__)
			"[__GED_NTLM__] "
			#endif
			#if defined(__GED_TUN__)
			"[__GED_TUN__] "
			#endif
			"[gcc %s]\n", argv[0], GED_VERSION_STR.Get(), __VERSION__);
			return 0;
		}
		else if (inArgv == CString("-ca"))
		{
			if (i<argc-1) 
			{
				inCa = argv[++i];
			}
			else
			{
				DisplayHelp (inArgv0);
				return 1;
			}
		}
		else if (inArgv == CString("-crt"))
		{
			if (i<argc-1) 
			{
				inCrt = argv[++i];
			}
			else
			{
				DisplayHelp (inArgv0);
				return 1;
			}
		}
		else if (inArgv == CString("-key"))
		{
			if (i<argc-1) 
			{
				inKey = argv[++i];
			}
			else
			{
				DisplayHelp (inArgv0);
				return 1;
			}
		}
		else if (inArgv == CString("-cipher"))
		{
			if (i<argc-1) 
			{
				inCipher = argv[++i];
			}
			else
			{
				DisplayHelp (inArgv0);
				return 1;
			}
		}
		else if (inArgv == CString("-push"))
		{
			if (i<argc-2) 
			{
				CString inArgv1 (argv[++i]);
				CString inArgv2 (argv[++i]);

				inReq = GED_PKT_REQ_PUSH|GED_PKT_REQ_PUSH_TMSP|GED_PKT_REQ_BKD_ACTIVE;
				
				if (inArgv1 != CString("-type"))
				{
					DisplayHelp (inArgv0);
					return 1;
				}

				inType = inArgv2.ToLong();

				if (inType == 0L)
				{
					DisplayHelp (inArgv0);
					return 1;
				}

				break;
			}
			else
			{
				DisplayHelp (inArgv0);
				return 1;
			}
		}
		else if (inArgv == CString("-update"))
		{
			if (i<argc-2) 
			{
				CString inArgv1 (argv[++i]);
				CString inArgv2 (argv[++i]);

				inReq = GED_PKT_REQ_PUSH|GED_PKT_REQ_PUSH_NOTMSP|GED_PKT_REQ_BKD_ACTIVE;
				
				if (inArgv1 != CString("-type"))
				{
					DisplayHelp (inArgv0);
					return 1;
				}

				inType = inArgv2.ToLong();

				if (inType == 0L)
				{
					DisplayHelp (inArgv0);
					return 1;
				}

				break;
			}
			else
			{
				DisplayHelp (inArgv0);
				return 1;
			}
		}
		else if (inArgv == CString("-drop"))
		{
			inReq = GED_PKT_REQ_DROP|GED_PKT_REQ_BKD_ACTIVE;

			if (i<argc-1)
			{
				CString inArgv1 (argv[i+1]);

				if (inArgv1 == CString("-type"))
				{
					i++; if (i<argc-1)
					{
						CString inArgv2 (argv[++i]);

						inType = inArgv2.ToLong();

						if (inType == 0L)
						{
							DisplayHelp (inArgv0);
							return 1;
						}

						if (i<argc-1)
						{
							CString inArgv3 (argv[i+1]);

							if (inArgv3 == CString("-queue"))
							{
								i++; if (i<argc-1)
								{
									CString inArgv4 (argv[++i]);

									if (inArgv4 == CString("active"))
										inReq = GED_PKT_REQ_DROP|GED_PKT_REQ_BKD_ACTIVE|GED_PKT_REQ_DROP_DATA;
									else if (inArgv4 == CString("sync"))
										inReq = GED_PKT_REQ_DROP|GED_PKT_REQ_BKD_SYNC|GED_PKT_REQ_DROP_DATA;
									else if (inArgv4 == CString("history"))
										inReq = GED_PKT_REQ_DROP|GED_PKT_REQ_BKD_HISTORY|GED_PKT_REQ_DROP_DATA;
									else
									{
										DisplayHelp (inArgv0);
										return 1;
									}
								}
								else
								{
									DisplayHelp (inArgv0);
									return 1;
								}
							}
						}
					}
					else
					{
						DisplayHelp (inArgv0);
						return 1;
					}
				}
				else if (inArgv1 == "-id")
				{
					i++; if (i<argc-1)
					{
						CString inArgv2 (argv[++i]);

						CStrings inArgv22 (inArgv2.Cut(CString(",")));
						for (size_t k=0; k<inArgv22.GetLength(); k++)
							inId += inArgv22[k]->ToULong();

						if (i<argc-1)
						{
							CString inArgv3 (argv[i+1]);

							if (inArgv3 == CString("-queue"))
							{
								i++; if (i<argc-1)
								{
									CString inArgv4 (argv[++i]);

									if (inArgv4 == CString("history"))
										inReq = GED_PKT_REQ_DROP|GED_PKT_REQ_BKD_HISTORY|GED_PKT_REQ_DROP_ID;
									else
									{
										DisplayHelp (inArgv0);
										return 1;
									}
								}
								else
								{
									DisplayHelp (inArgv0);
									return 1;
								}
							}
						}
						else
						{
							DisplayHelp (inArgv0);
							return 1;
						}
					}
					else
					{
						DisplayHelp (inArgv0);
						return 1;
					}
				}
				else
				{
					DisplayHelp (inArgv0);
					return 1;
				}
			}
			else
			{
				DisplayHelp (inArgv0);
				return 1;
			}

			break;
		}
		else if (inArgv == CString("-peek"))
		{
			inReq = GED_PKT_REQ_PEEK|GED_PKT_REQ_BKD_ACTIVE;

			if (i<argc-1)
			{
				CString inArgv1 (argv[i+1]);

				if (inArgv1 == CString("-type"))
				{
					i++; if (i<argc-1)
					{
						CString inArgv2 (argv[++i]);

						inType = inArgv2.ToLong();

						if (inType == 0L)
						{
							DisplayHelp (inArgv0);
							return 1;
						}

						if (i<argc-1)
						{
							CString inArgv3 (argv[i+1]);

							if (inArgv3 == CString("-queue"))
							{
								i++; if (i<argc-1)
								{
									CString inArgv4 (argv[++i]);

									if (inArgv4 == CString("active"))
										inReq = GED_PKT_REQ_PEEK|GED_PKT_REQ_BKD_ACTIVE;
									else if (inArgv4 == CString("sync"))
										inReq = GED_PKT_REQ_PEEK|GED_PKT_REQ_BKD_SYNC;
									else if (inArgv4 == CString("history"))
										inReq = GED_PKT_REQ_PEEK|GED_PKT_REQ_BKD_HISTORY;
									else
									{
										DisplayHelp (inArgv0);
										return 1;
									}
								}
								else
								{
									DisplayHelp (inArgv0);
									return 1;
								}
							}
						}
					}
					else
					{
						DisplayHelp (inArgv0);
						return 1;
					}
				}
				else if (inArgv1 == CString("-queue"))
				{
					i++; if (i<argc-1)
					{
						CString inArgv2 (argv[++i]);

						if (inArgv2 == CString("active"))
							inReq = GED_PKT_REQ_PEEK|GED_PKT_REQ_BKD_ACTIVE;
						else if (inArgv2 == CString("sync"))
							inReq = GED_PKT_REQ_PEEK|GED_PKT_REQ_BKD_SYNC;
						else if (inArgv2 == CString("history"))
							inReq = GED_PKT_REQ_PEEK|GED_PKT_REQ_BKD_HISTORY;
						else
						{
							DisplayHelp (inArgv0);
							return 1;
						}
					}
					else
					{
						DisplayHelp (inArgv0);
						return 1;
					}
				}
				else
				{
					DisplayHelp (inArgv0);
					return 1;
				}
			}

			break;
		}
		#ifdef __GED_TUN__
		else if (inArgv == CString("-tun"))
		{
			if (i<argc-1) 
			{
				inTun = argv[++i];
				if (inTun.Cut(CString(":")).GetLength() != 3)
				{
					DisplayHelp (inArgv0);
					return 1;
				}
			}
			else
			{
				DisplayHelp (inArgv0);
				return 1;
			}
		}
		#endif
		else
		{
			DisplayHelp (inArgv0);
			return 1;
		}
	}

	if ((inReq & GED_PKT_REQ_MASK) == GED_PKT_REQ_NONE && inType == GED_PKT_TYP_ANY && inTun == CString())
	{
		DisplayHelp (CString(argv[0]));
		return 1;
	}
	
	if ((inReq & GED_PKT_REQ_MASK) == GED_PKT_REQ_PEEK)
		inReq |= inReqSrt;

	inReq |= inReqNoSync;

	if (!::GEDQInit (inGEDQCfg))
	{
		::fprintf (stdout, "ERROR : could not initialize gedq related libraries\n");
		return 1;
	}

	if (!::LoadGEDQCfg (inCfgFile, inGEDQCfg)) return 1;

	::setlocale (LC_ALL, "");

	if (inAck  != CString()) inGEDQCfg.ackto = inAck.ToULong();

	if (inPAddr != CString()) inGEDQCfg.proxy.addr = inPAddr;
	if (inPPort != 0L	) inGEDQCfg.proxy.port = inPPort;

	if (inPAuth != GED_HTTP_PROXY_AUTH_NONE)
	{
		inGEDQCfg.proxy.auth = inPAuth;
		inGEDQCfg.proxy.user = inPUser;
		inGEDQCfg.proxy.pass = inPPass;
	}

	if (inAddr != CString()) inGEDQCfg.bind.addr = inAddr;
	if (inPort != 0L       ) inGEDQCfg.bind.port = inPort;
	if (inSock != CString()) inGEDQCfg.bind.sock = inSock;

	if (inGEDQCfg.bind.addr == CString() && inGEDQCfg.bind.port != 0L && inGEDQCfg.bind.sock == CString()) 
		inGEDQCfg.bind.addr = CString("localhost");

	if (inCa     != CString()) inGEDQCfg.tls.ca  = inCa;
	if (inCrt    != CString()) inGEDQCfg.tls.crt = inCrt;
	if (inKey    != CString()) inGEDQCfg.tls.key = inKey;
	if (inCipher != CString()) inGEDQCfg.tls.cph = inCipher;

	CChunk outChunk; if (inType != GED_PKT_TYP_CLOSE && inType != GED_PKT_TYP_ANY)
	{
		TGEDPktCfg *inGEDPktCfg=NULL; for (size_t j=0; j<inGEDQCfg.pkts.GetLength() && inGEDPktCfg==NULL; j++)
			if (inGEDQCfg.pkts[j]->type == inType) inGEDPktCfg = inGEDQCfg.pkts[j];

		if (inGEDPktCfg == NULL)
		{
			::fprintf (stderr, "ERROR : unknown packet type \"%d\"\n", inType);
			return 1;
		}

		i++; 

		for (size_t j=0; j<inGEDPktCfg->fields.GetLength() && i<argc; j++, i++)
			switch (inGEDPktCfg->fields[j]->type)
			{
				case DATA_SINT32  : outChunk << CString(argv[i]).ToLong  (); break;
				case DATA_FLOAT64 : outChunk << CString(argv[i]).ToDouble(); break;
				case DATA_STRING  : 
				{
					CString str(argv[i]);
					int n; while ((n = IsUtf8(str)) != 0) str.Delete (n-1, 1);
					if (str.GetLength() != ::strlen(argv[i])) 
						::fprintf (stderr, "warning : found a non utf8 sequence for \"%s\" field number \"%d\", trying to remove dummy chars...\n", 
							   inGEDPktCfg->fields[j]->name.Get(), j);
					outChunk << str.Get();
				}
				break;
			}
	}
	
	if ((inReq&GED_PKT_REQ_PUSH) && outChunk.GetSize() == 0)
	{
		::fprintf (stderr, "ERROR : empty push !\n", inType);
		return 1;
	}

	try
	{
		CGEDQCtx GEDQCtx (inGEDQCfg, inTun);

		if (inTun == CString())
		{
			TBuffer <TGEDPktOut> inGEDPktOutBuf;
		
			if ((inReq&GED_PKT_REQ_DROP_MASK) != GED_PKT_REQ_DROP_ID)
			{
				TGEDPktOut inGEDPktOut = ::NewGEDPktOut (CGEDQCtx::m_GEDQCtx->m_GEDQSocketClient->GetAddr(), inType, inReq, outChunk.GetChunk(), outChunk.GetSize());

				TGEDPktIn *gedPktIn = (TGEDPktIn *)inGEDPktOut;

				if (inGEDQCfg.pmaxr > 0L)
				{
					gedPktIn->req |= GED_PKT_REQ_PEEK_PARM_4_NUMBER;
					gedPktIn->p4 = inGEDQCfg.pmaxr;
				}

				if (inTm1 != CString())
				{
					gedPktIn->req |= GED_PKT_REQ_PEEK_PARM_1_TM;
					gedPktIn->p1 = inTm1.ToULong();
				}

				if (inTm2 != CString())
				{
					gedPktIn->req |= GED_PKT_REQ_PEEK_PARM_3_TM;
					gedPktIn->p3 = inTm2.ToULong();
				}

				if (inOff != CString())
				{
					gedPktIn->req |= GED_PKT_REQ_PEEK_PARM_2_OFFSET;
					gedPktIn->p2 = inOff.ToULong();
				}

				if (inNum != CString())
				{
					gedPktIn->req |= GED_PKT_REQ_PEEK_PARM_4_NUMBER;
					gedPktIn->p4 = inNum.ToULong();
				}

				inGEDPktOutBuf += inGEDPktOut;
			}
			else
			{
				for (size_t i=0; i<inId.GetLength(); i++)
				{
					TGEDPktOut inGEDPktOut = ::NewGEDPktOut (CGEDQCtx::m_GEDQCtx->m_GEDQSocketClient->GetAddr(), inType, inReq, inId[i], sizeof(UInt32));

					TGEDPktIn *gedPktIn = (TGEDPktIn *)inGEDPktOut;

					if (inGEDQCfg.pmaxr > 0L)
					{
						gedPktIn->req |= GED_PKT_REQ_PEEK_PARM_4_NUMBER;
						gedPktIn->p4 = inGEDQCfg.pmaxr;
					}

					if (inTm1 != CString())
					{
						gedPktIn->req |= GED_PKT_REQ_PEEK_PARM_1_TM;
						gedPktIn->p1 = inTm1.ToULong();
					}

					if (inTm2 != CString())
					{
						gedPktIn->req |= GED_PKT_REQ_PEEK_PARM_3_TM;
						gedPktIn->p3 = inTm2.ToULong();
					}

					if (inOff != CString())
					{
						gedPktIn->req |= GED_PKT_REQ_PEEK_PARM_2_OFFSET;
						gedPktIn->p2 = inOff.ToULong();
					}

					if (inNum != CString())
					{
						gedPktIn->req |= GED_PKT_REQ_PEEK_PARM_4_NUMBER;
						gedPktIn->p4 = inNum.ToULong();
					}

					inGEDPktOutBuf += inGEDPktOut;
				}
			}

			GEDQCtx.Run (inGEDPktOutBuf);
		}
		else
		{
			GEDQCtx.Run ();
		}
	}
	catch (CException *inException)
	{
		::fprintf (stderr, "ERROR : %s\n", inException->GetMessage().Get());

		return 1;
	}	

	return CGEDQCtx::m_Res;
}


