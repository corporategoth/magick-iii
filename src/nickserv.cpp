#ifdef WIN32
#pragma hdrstop
#else
#pragma implementation
#endif

/* Magick IRC Services
**
** (c) 2005 The Neuromancy Society <info@neuromancy.net>
**
** The above copywright may not be removed under any circumstances,
** however it may be added to if any modifications are made to this
** file.  All modified code must be clearly documented and labelled.
**
** This code is released under the GNU General Public License v2.0 or better.
** The full text of this license should be contained in a file called
** COPYING distributed with this code.  If this file does not exist,
** it may be viewed here: http://www.neuromaancy.net/license/gpl.html
**
** ======================================================================= */
#define RCSID(x,y) const char *rcsid_magick__nickserv_cpp_ ## x () { return y; }
RCSID(magick__nickserv_cpp, "@(#)$Id$");

/* ======================================================================= **
**
** For official changes (by The Neuromancy Society), please
** check the ChangeLog* files that come with this distribution.
**
** Third Party Changes (please include e-mail address):
**
** N/A
**
** ======================================================================= */

#include "magick.h"
#include "liveuser.h"

#include <mantra/core/trace.h>

static bool ns_Register(const boost::shared_ptr<LiveUser> &service,
						const boost::shared_ptr<LiveUser> &user,
						const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("ns_Register" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);

	service->GetService()->PRIVMSG(service, user,
								   boost::format("%1%") % "HI!");

	MT_RET(true);
	MT_EE
}


void init_nickserv_functions()
{
	MT_EB
	MT_FUNC("init_nickserv_functions");

	ROOT->nickserv.PushCommand("^HELP$", &ns_Help);

	ROOT->nickserv.PushCommand("^REGISTER$", &ns_Register);
	ROOT->nickserv.PushCommand("^DROP$", &ns_Drop);
	ROOT->nickserv.PushCommand("^LINK$", &ns_Link);
	ROOT->nickserv.PushCommand("^UNLINK$", &ns_Unlink);
	ROOT->nickserv.PushCommand("^ID(?:ENT(?:IFY)?)?$", &ns_Identify);
	ROOT->nickserv.PushCommand("^INFO$", &ns_Info);
	ROOT->nickserv.PushCommand("^GHOST$", &ns_Ghost);
	ROOT->nickserv.PushCommand("^REC(?:OVER)?$", &ns_Recover);
	ROOT->nickserv.PushCommand("^SEND$", &ns_Send);
	ROOT->nickserv.PushCommand("^SUSPEND$", &ns_Suspend);
	ROOT->nickserv.PushCommand("^UN?SUSPEND$", &ns_Unsuspend);
	ROOT->nickserv.PushCommand("^FORBID$", &ns_Forbid);
	ROOT->nickserv.PushCommand("^SETPASS(?:WORD)?$", &ns_Setpass);

	ROOT->nickserv.PushCommand("^ACC(?:ESS)?$",
				   Service::CommandMerge(ROOT->nickserv, 1, 2));
	ROOT->nickserv.PushCommand("^ACC(?:ESS)?\\s+CUR(?:R(?:ENT)?)?$",
							   &ns_Access_Current);
	ROOT->nickserv.PushCommand("^ACC(?:ESS)?\\s+ADD$",
							   &ns_Access_Add);
	ROOT->nickserv.PushCommand("^ACC(?:ESS)?\\s+(?:ERASE|DEL(?:ETE)?)$",
							   &ns_Access_Del);
	ROOT->nickserv.PushCommand("^ACC(?:ESS)?\\s+(?:LIST|VIEW)$",
							   &ns_Access_List);
	ROOT->nickserv.PushCommand("^ACC(?:ESS)?\\s+HELP$",
							   &ns_Access_Help);

	ROOT->nickserv.PushCommand("^IGNORE$",
				   Service::CommandMerge(ROOT->nickserv, 1, 2));
	ROOT->nickserv.PushCommand("^IGNORE\\s+ADD$",
							   &ns_Ignore_Add);
	ROOT->nickserv.PushCommand("^IGNORE\\s+(?:ERASE|DEL(?:ETE)?)$",
							   &ns_Ignore_Del);
	ROOT->nickserv.PushCommand("^IGNORE\\s+(?:LIST|VIEW)$",
							   &ns_Ignore_List);
	ROOT->nickserv.PushCommand("^IGNORE\\s+HELP$",
							   &ns_Ignore_Help);

	ROOT->nickserv.PushCommand("^SET$",
				   Service::CommandMerge(ROOT->nickserv, 1, 2));
	ROOT->nickserv.PushCommand("^SET\\s+PASS(?:W(?:OR)?D)?$",
							   &ns_Set_Password);
	ROOT->nickserv.PushCommand("^SET\\s+E-?MAIL$",
							   &ns_Set_Email);
	ROOT->nickserv.PushCommand("^SET\\s+(?:URL|WWW|WEB(?:SITE)?|HTTP)$",
							   &ns_Set_Website);
	ROOT->nickserv.PushCommand("^SET\\s+(?:UIN|ICQ)$",
							   &ns_Set_Icq);
	ROOT->nickserv.PushCommand("^SET\\s+(?:AIM)$",
							   &ns_Set_Aim);
	ROOT->nickserv.PushCommand("^SET\\s+MSN$",
							   &ns_Set_Msn);
	ROOT->nickserv.PushCommand("^SET\\s+JABBER$",
							   &ns_Set_Jabber);
	ROOT->nickserv.PushCommand("^SET\\s+YAHOO$",
							   &ns_Set_Yahoo);
	ROOT->nickserv.PushCommand("^SET\\s+DESC(?:RIPT(?:ION)?)?$",
							   &ns_Set_Description);
	ROOT->nickserv.PushCommand("^SET\\s+COMMENT$",
							   &ns_Set_Comment);
	ROOT->nickserv.PushCommand("^SET\\s+LANG(?:UAGE)?$",
							   &ns_Set_Language);
	ROOT->nickserv.PushCommand("^SET\\s+PROT(?:ECT)?$",
							   &ns_Set_Protect);
	ROOT->nickserv.PushCommand("^SET\\s+SECURE$",
							   &ns_Set_Secure);
	ROOT->nickserv.PushCommand("^SET\\s+NOMEMO$",
							   &ns_Set_Nomemo);
	ROOT->nickserv.PushCommand("^SET\\s+PRIV(?:ATE)?$",
							   &ns_Set_Private);
	ROOT->nickserv.PushCommand("^SET\\s+(?:(?:PRIV)?MSG)$",
							   &ns_Set_Privmsg);
	ROOT->nickserv.PushCommand("^SET\\s+NO?EX(?:PIRE)?$",
							   &ns_Set_Noexpire);
	ROOT->nickserv.PushCommand("^SET\\s+PIC(?:TURE)?$",
							   &ns_Set_Picture);
	ROOT->nickserv.PushCommand("^SET\\s+HELP$",
							   &ns_Set_Help);

	ROOT->nickserv.PushCommand("^LOCK$",
				   Service::CommandMerge(ROOT->nickserv, 1, 2));

	ROOT->nickserv.PushCommand("^UN?LOCK$",
				   Service::CommandMerge(ROOT->nickserv, 1, 2));

	// These commands don't operate on any nickname.
	ROOT->nickserv.PushCommand("^(?:STORED)?LIST$", &ns_StoredList);
	ROOT->nickserv.PushCommand("^LIVE(?:LIST)?$", &ns_LiveList);

	MT_EE
}

