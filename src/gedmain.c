/****************************************************************************************************************************************
 gedmain.c
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

//----------------------------------------------------------------------------------------------------------------------------------------
// avoid zombies while daemonizing
//----------------------------------------------------------------------------------------------------------------------------------------
/*
static void ged_reap_zomb (int)
{
	while (::waitpid (-1, NULL, WNOHANG) > 0) { }
}
*/

//----------------------------------------------------------------------------------------------------------------------------------------
// ged usage
//----------------------------------------------------------------------------------------------------------------------------------------
void ged_display_help (const CString &inCmd)
{
	::fprintf (stdout, "%s [--help | -h] [-c <ged.cfg>] [-nodaemon] [-dump [active|sync|history*|all]] [-recover [-f] <file>]\n", inCmd.Get());
}

//----------------------------------------------------------------------------------------------------------------------------------------
// recovering callback
//----------------------------------------------------------------------------------------------------------------------------------------
void ged_recover_cb (const UInt32 i, const UInt32 n, const TGEDRcd *)
{
	::fprintf (stdout, "[4/6] Recovering records... [%d/%d] (%d%%)\r", i, n, i*100/n);	
}

//---------------------------------------------------------------------------------------------------------------------------------------
// execution entry point
//---------------------------------------------------------------------------------------------------------------------------------------
int main (int argc, char **argv)
{
	TGEDCfg inGEDCfg; CString inCfgFile (GEDCfgFileDefault); 
	int doDump=0; bool doRecover=false, doCheckHash=true, doDaemon=true; 
	CString inRecoverFile, inDumpFile;

	for (size_t i=1; i<argc; i++)
	{
		if (CString(argv[i]) == CString("-c"))
		{
			if (i<argc-1) 
			{
				inCfgFile = argv[++i]; 
			}
			else
			{
				ged_display_help (CString(argv[0]));
				return 1;
			}
		}
		else if (CString(argv[i]) == CString("-nodaemon"))
		{
			doDaemon = false;
		}
		else if (CString(argv[i]) == CString("-dump"))
		{
			if (i<argc-1)
			{
				i++;
				if (CString(argv[i]) == CString("active"))
				{
					doDump = GED_PKT_REQ_BKD_ACTIVE;
				}
				else if (CString(argv[i]) == CString("sync"))
				{
					doDump = GED_PKT_REQ_BKD_SYNC;
				}
				else if (CString(argv[i]) == CString("history"))
				{
					doDump = GED_PKT_REQ_BKD_HISTORY;
				}
				else if (CString(argv[i]) == CString("all"))
				{
					doDump = GED_PKT_REQ_BKD_ACTIVE|GED_PKT_REQ_BKD_HISTORY|GED_PKT_REQ_BKD_SYNC;
				}
				else
				{
					ged_display_help (CString(argv[0]));
					return 1;
				}
			}
			else
			{
				doDump = GED_PKT_REQ_BKD_HISTORY;
			}
		}
		else if (CString(argv[i]) == CString("-recover"))
		{
			if (i<argc-2)
			{
				i++;
				if (CString(argv[i]) == CString("-f"))
				{
					doCheckHash = false;
				}
				else
				{
					ged_display_help (CString(argv[0]));
					return 1;
				}
				inRecoverFile = argv[++i]; 
				doRecover = true;
			}
			else if (i<argc-1) 
			{
				inRecoverFile = argv[++i]; 
				doRecover = true;
			}
			else
			{
				ged_display_help (CString(argv[0]));
				return 1;
			}
		}
		else
		{
			ged_display_help (CString(argv[0]));
			return 1;
		}
	}

	if (!::GEDInit (inGEDCfg))
	{
		::fprintf (stderr, "ERROR : could not initialize ged related libraries\n");
		return 1;
	}

	if (!::LoadGEDCfg (inCfgFile, inGEDCfg)) return 1;

	::setlocale (LC_ALL, "");

	try
	{
		if (doDump)
		{
			CStrings nullStr; nullStr += CString("0");
			inGEDCfg.bkdcfg[GEDCfgTTLTimer]   = nullStr;
			inGEDCfg.bkdcfg[GEDCfgTTLActive]  = nullStr;
			inGEDCfg.bkdcfg[GEDCfgTTLSync]    = nullStr;
			inGEDCfg.bkdcfg[GEDCfgTTLHistory] = nullStr;

			CGEDCtx gedCtx (inGEDCfg, false);

			TBuffer <TGEDRcd *> inGEDRcds; if (gedCtx.m_BackEnd->Lock() == 0)
			{
				inGEDRcds = gedCtx.m_BackEnd->Dump (doDump);

				gedCtx.m_BackEnd->UnLock();
			}
			else
			{
			}

			gedCtx.m_BackEnd->Finalize();

			xmlDocPtr  xmlDoc  = xmlNewDoc  ((xmlChar*)"1.0");
			xmlNodePtr xmlRoot = xmlNewNode (NULL, (xmlChar*)"ged");
			::xmlNewProp (xmlRoot, XML_ELEMENT_NODE_GED_ROOT_VERSION_ATTR, (xmlChar*)GED_VERSION_STR.Get());
			xmlDocSetRootElement (xmlDoc, xmlRoot);

			for (size_t i=inGEDRcds.GetLength(), j=0; i>0; i--, j++) 
				GEDRcdToXmlNodeLight (*inGEDRcds[j], gedCtx.m_GEDCfg.pkts, xmlRoot);
			xmlSaveFormatFileEnc ((inDumpFile == CString()) ? "-" : inDumpFile.Get(), xmlDoc, "UTF-8", 0);//1);
			xmlFreeDoc (xmlDoc);

			for (size_t i=inGEDRcds.GetLength(); i>0; i--) DeleteGEDRcd (*inGEDRcds[i-1]);
		}
		else if (doRecover)
		{
			::fprintf (stdout, "[1/6] Reading archive \"%s\"...\n", inRecoverFile.Get());

			xmlLineNumbersDefault (1);
			xmlDocPtr xmlDoc = xmlParseFile (inRecoverFile.Get());
			if (xmlDoc == NULL) 
				throw new CException ("could not open " + inRecoverFile);

			xmlNodePtr xmlRoot = xmlDocGetRootElement (xmlDoc);
			if (xmlStrcmp (xmlRoot->name, (xmlChar*)"ged")) 
				throw new CException ("the loaded archive is not an xml root ged version " + GED_VERSION_STR);

			if (doCheckHash)
			{
				for (xmlAttrPtr curXMLAttrPtr=xmlRoot->properties; curXMLAttrPtr; curXMLAttrPtr=curXMLAttrPtr->next)
				{
					if (CString((char*)curXMLAttrPtr->name) == CString((char*)XML_ELEMENT_NODE_GED_ROOT_VERSION_ATTR) && curXMLAttrPtr->children != NULL)
					{
						CString inVrs ((char*)curXMLAttrPtr->children->content);

						if (inVrs.Find(CString(".")))
						{
							CStrings inVrRePa (inVrs.Cut(CString(".")));

							if (inVrRePa.GetLength() >= 2)
							{
								if (inVrRePa[0]->ToULong() != GED_MAJOR || inVrRePa[1]->ToULong() != GED_MINOR)
									throw new CException ("the loaded archive is not an xml root ged version " + GED_VERSION_STR);

							}
							else
							{
								throw new CException ("the loaded archive is not an xml root ged version " + GED_VERSION_STR);
							}
						}
						else
						{
							throw new CException ("the loaded archive is not an xml root ged version " + GED_VERSION_STR);
						}
					}
				}
			}

			CStrings nullStr; nullStr += CString("0");
			inGEDCfg.bkdcfg[GEDCfgTTLTimer]   = nullStr;
			inGEDCfg.bkdcfg[GEDCfgTTLActive]  = nullStr;
			inGEDCfg.bkdcfg[GEDCfgTTLSync]    = nullStr;
			inGEDCfg.bkdcfg[GEDCfgTTLHistory] = nullStr;

			::fprintf (stdout, "[2/6] Opening backend...\n");

			CGEDCtx gedCtx (inGEDCfg, false);

			::fprintf (stdout, "[3/6] Reading records...\n");

			TBuffer <TGEDRcd *> gedRcds; gedRcds.SetInc (BUFFER_INC_65536); UInt32 num=0L;

			for (xmlNodePtr curXMLNodePtr=xmlRoot->children; curXMLNodePtr; curXMLNodePtr=curXMLNodePtr->next)
			{
				if (curXMLNodePtr->type != XML_ELEMENT_NODE) continue;
				
				num++;

				TGEDRcd *inGEDRcd = GEDXmlNodeToRcd (inGEDCfg.pkts, curXMLNodePtr, doCheckHash);

				if (inGEDRcd != NULL)
				{
					gedRcds += inGEDRcd;
				}
				else
				{
					gedCtx.SysLog (GED_SYSLOG_ERROR, GED_SYSLOG_LEV_BKD_OPERATION, "recover bypassing record number " + CString(num) + 
						       " from " + inRecoverFile + " (line " + CString(xmlGetLineNo(curXMLNodePtr)) + ") : the record format is not correct");
				}
			}

			if (gedCtx.m_BackEnd->Lock() == 0)
			{
				gedCtx.m_BackEnd->Recover (gedRcds, ged_recover_cb);

				gedCtx.m_BackEnd->UnLock();
			}
			else
			{
			}

			::fprintf (stdout, "\n[5/6] Closing backend...\n");

			gedCtx.m_BackEnd->Finalize();

			::fprintf (stdout, "[6/6] Done, check your syslog for details.\n");

			for (size_t i=gedRcds.GetLength(); i>0; i--) DeleteGEDRcd (*gedRcds[i-1]);

			xmlFreeDoc (xmlDoc);
		}
		else
		{
			if (!doDaemon)
			{
				CGEDCtx gedCtx (inGEDCfg, true);

				gedCtx.Run();
			}
			else
			{
				int frk = fork();

				if (frk > 0) exit(0);
				if (frk < 0) exit(1);
		
				setsid();

				for (int i=getdtablesize(); i>=0; --i) close(i);

				int f=open("/dev/null",O_RDWR); 
				dup(f); /* stdout */
				dup(f); /* stderr */

				CGEDCtx gedCtx (inGEDCfg, true);

				/*struct sigaction sa; bzero(&sa,sizeof(sa)); sa.sa_handler = ged_reap_zomb; sa.sa_flags = SA_NOCLDSTOP;
	                	::sigaction (SIGCHLD, &sa, NULL);*/

				signal (SIGCHLD, SIG_IGN); /* ignore child */
				signal (SIGTSTP, SIG_IGN); /* ignore tty signals */
				signal (SIGTTOU, SIG_IGN);
				signal (SIGTTIN, SIG_IGN);

				try
				{
					gedCtx.Run();
				}
				catch (CException *inException)
				{
					time_t intime; ::time (&intime); struct tm tm; ::localtime_r (&intime, &tm);

					::fprintf (stderr, "[%.2d/%.2d/%.2d %.2d:%.2d:%.2d] ERROR   : %s\n", tm.tm_mday, tm.tm_mon+1, 
					   tm.tm_year%100, tm.tm_hour, tm.tm_min, tm.tm_sec, inException->GetMessage().Get());
				}
			}
		}
	} 
	catch (CException *inException)
	{
		time_t intime; ::time (&intime); struct tm tm; ::localtime_r (&intime, &tm);

		::fprintf (stderr, "[%.2d/%.2d/%.2d %.2d:%.2d:%.2d] ERROR   : %s\n", tm.tm_mday, tm.tm_mon+1, 
			   tm.tm_year%100, tm.tm_hour, tm.tm_min, tm.tm_sec, inException->GetMessage().Get());
	}

	return 0;
}

