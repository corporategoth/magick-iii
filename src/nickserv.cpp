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


	MT_RET(true);
	MT_EE
}

static bool ns_Drop(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("ns_Drop" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);


	MT_RET(true);
	MT_EE
}

static bool ns_Link(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("ns_Link" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);


	MT_RET(true);
	MT_EE
}

static bool ns_Unlink(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("ns_Unlink" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);


	MT_RET(true);
	MT_EE
}

static bool ns_Identify(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("ns_Identify" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);


	MT_RET(true);
	MT_EE
}

static bool ns_Info(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("ns_Info" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);


	MT_RET(true);
	MT_EE
}

static bool ns_Ghost(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("ns_Ghost" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);


	MT_RET(true);
	MT_EE
}

static bool ns_Recover(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("ns_Recover" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);


	MT_RET(true);
	MT_EE
}

static bool ns_Send(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("ns_Send" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);


	MT_RET(true);
	MT_EE
}

static bool ns_Suspend(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("ns_Suspend" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);


	MT_RET(true);
	MT_EE
}

static bool ns_Unsuspend(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("ns_Unsuspend" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);


	MT_RET(true);
	MT_EE
}

static bool ns_Forbid(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("ns_Forbid" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);


	MT_RET(true);
	MT_EE
}

static bool ns_Setpass(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("ns_Setpass" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);


	MT_RET(true);
	MT_EE
}

static bool ns_Access_Current(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("ns_Access_Current" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);


	MT_RET(true);
	MT_EE
}

static bool ns_Access_Add(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("ns_Access_Add" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);


	MT_RET(true);
	MT_EE
}

static bool ns_Access_Del(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("ns_Access_Del" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);


	MT_RET(true);
	MT_EE
}

static bool ns_Access_List(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("ns_Access_List" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);


	MT_RET(true);
	MT_EE
}

static bool ns_Ignore_Add(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("ns_Ignore_Add" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);


	MT_RET(true);
	MT_EE
}

static bool ns_Ignore_Del(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("ns_Ignore_Del" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);


	MT_RET(true);
	MT_EE
}

static bool ns_Ignore_List(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("ns_Ignore_List" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);


	MT_RET(true);
	MT_EE
}

static bool ns_Set_Password(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("ns_Set_Password" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);


	MT_RET(true);
	MT_EE
}

static bool ns_Set_Email(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("ns_Set_Email" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);


	MT_RET(true);
	MT_EE
}

static bool ns_Set_Website(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("ns_Set_Website" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);


	MT_RET(true);
	MT_EE
}

static bool ns_Set_Icq(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("ns_Set_Icq" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);


	MT_RET(true);
	MT_EE
}

static bool ns_Set_Aim(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("ns_Set_Aim" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);


	MT_RET(true);
	MT_EE
}

static bool ns_Set_Msn(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("ns_Set_Msn" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);


	MT_RET(true);
	MT_EE
}

static bool ns_Set_Jabber(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("ns_Set_Jabber" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);


	MT_RET(true);
	MT_EE
}

static bool ns_Set_Yahoo(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("ns_Set_Yahoo" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);


	MT_RET(true);
	MT_EE
}

static bool ns_Set_Description(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("ns_Set_Description" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);


	MT_RET(true);
	MT_EE
}

static bool ns_Set_Comment(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("ns_Set_Comment" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);


	MT_RET(true);
	MT_EE
}

static bool ns_Set_Language(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("ns_Set_Language" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);


	MT_RET(true);
	MT_EE
}

static bool ns_Set_Protect(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("ns_Set_Protect" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);


	MT_RET(true);
	MT_EE
}

static bool ns_Set_Secure(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("ns_Set_Secure" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);


	MT_RET(true);
	MT_EE
}

static bool ns_Set_Nomemo(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("ns_Set_Nomemo" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);


	MT_RET(true);
	MT_EE
}

static bool ns_Set_Private(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("ns_Set_Private" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);


	MT_RET(true);
	MT_EE
}

static bool ns_Set_Privmsg(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("ns_Set_Privmsg" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);


	MT_RET(true);
	MT_EE
}

static bool ns_Set_Noexpire(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("ns_Set_Noexpire" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);


	MT_RET(true);
	MT_EE
}

static bool ns_Set_Picture(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("ns_Set_Picture" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);


	MT_RET(true);
	MT_EE
}

static bool ns_Lock_Language(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("ns_Lock_Language" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);


	MT_RET(true);
	MT_EE
}

static bool ns_Lock_Protect(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("ns_Lock_Protect" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);


	MT_RET(true);
	MT_EE
}

static bool ns_Lock_Secure(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("ns_Lock_Secure" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);


	MT_RET(true);
	MT_EE
}

static bool ns_Lock_Nomemo(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("ns_Lock_Nomemo" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);


	MT_RET(true);
	MT_EE
}

static bool ns_Lock_Private(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("ns_Lock_Private" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);


	MT_RET(true);
	MT_EE
}

static bool ns_Lock_Privmsg(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("ns_Lock_Privmsg" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);


	MT_RET(true);
	MT_EE
}

static bool ns_Unlock_Language(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("ns_Unlock_Language" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);


	MT_RET(true);
	MT_EE
}

static bool ns_Unlock_Protect(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("ns_Unlock_Protect" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);


	MT_RET(true);
	MT_EE
}

static bool ns_Unlock_Secure(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("ns_Unlock_Secure" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);


	MT_RET(true);
	MT_EE
}

static bool ns_Unlock_Nomemo(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("ns_Unlock_Nomemo" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);


	MT_RET(true);
	MT_EE
}

static bool ns_Unlock_Private(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("ns_Unlock_Private" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);


	MT_RET(true);
	MT_EE
}

static bool ns_Unlock_Privmsg(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("ns_Unlock_Privmsg" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);


	MT_RET(true);
	MT_EE
}

static bool ns_StoredList(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("ns_StoredList" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);


	MT_RET(true);
	MT_EE
}

static bool ns_LiveList(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("ns_LiveList" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);


	MT_RET(true);
	MT_EE
}

void init_nickserv_functions()
{
	MT_EB
	MT_FUNC("init_nickserv_functions");

	std::vector<std::string> comm_oper, comm_sop, comm_opersop;
	comm_oper.push_back(ROOT->ConfigValue<std::string>("commserv.oper.name"));
	comm_sop.push_back(ROOT->ConfigValue<std::string>("commserv.sop.name"));
	comm_opersop.push_back(ROOT->ConfigValue<std::string>("commserv.oper.name"));
	comm_opersop.push_back(ROOT->ConfigValue<std::string>("commserv.sop.name"));

	ROOT->nickserv.PushCommand("^HELP$", boost::bind(&Service::Help, &ROOT->nickserv,
													 _1, _2, _3));

	ROOT->nickserv.PushCommand("^REGISTER$", &ns_Register);
	ROOT->nickserv.PushCommand("^DROP$", &ns_Drop);
	ROOT->nickserv.PushCommand("^LINK$", &ns_Link);
	ROOT->nickserv.PushCommand("^UNLINK$", &ns_Unlink);
	ROOT->nickserv.PushCommand("^ID(ENT(IFY)?)?$", &ns_Identify);
	ROOT->nickserv.PushCommand("^INFO$", &ns_Info);
	ROOT->nickserv.PushCommand("^GHOST$", &ns_Ghost);
	ROOT->nickserv.PushCommand("^REC(OVER)?$", &ns_Recover);
	ROOT->nickserv.PushCommand("^SEND$", &ns_Send);
	ROOT->nickserv.PushCommand("^SUSPEND$", &ns_Suspend, comm_sop);
	ROOT->nickserv.PushCommand("^UN?SUSPEND$", &ns_Unsuspend, comm_sop);
	ROOT->nickserv.PushCommand("^FORBID$", &ns_Forbid, comm_sop);
	ROOT->nickserv.PushCommand("^SETPASS(WORD)?$", &ns_Setpass, comm_sop);

	ROOT->nickserv.PushCommand("^ACC(ESS)?$",
				   Service::CommandMerge(ROOT->nickserv, 1, 2));
	ROOT->nickserv.PushCommand("^ACC(ESS)?\\s+CUR(R(ENT)?)?$",
							   &ns_Access_Current);
	ROOT->nickserv.PushCommand("^ACC(ESS)?\\s+ADD$",
							   &ns_Access_Add);
	ROOT->nickserv.PushCommand("^ACC(ESS)?\\s+(ERASE|DEL(ETE)?)$",
							   &ns_Access_Del);
	ROOT->nickserv.PushCommand("^ACC(ESS)?\\s+(LIST|VIEW)$",
							   &ns_Access_List);
	ROOT->nickserv.PushCommand("^ACC(ESS)?\\s+HELP$",
							   boost::bind(&Service::AuxHelp, &ROOT->nickserv,
										   _1, _2, _3));

	ROOT->nickserv.PushCommand("^IGNORE$",
				   Service::CommandMerge(ROOT->nickserv, 1, 2));
	ROOT->nickserv.PushCommand("^IGNORE\\s+ADD$",
							   &ns_Ignore_Add);
	ROOT->nickserv.PushCommand("^IGNORE\\s+(ERASE|DEL(ETE)?)$",
							   &ns_Ignore_Del);
	ROOT->nickserv.PushCommand("^IGNORE\\s+(LIST|VIEW)$",
							   &ns_Ignore_List);
	ROOT->nickserv.PushCommand("^IGNORE\\s+HELP$",
							   boost::bind(&Service::AuxHelp, &ROOT->nickserv,
										   _1, _2, _3));

	ROOT->nickserv.PushCommand("^SET$",
				   Service::CommandMerge(ROOT->nickserv, 1, 2));
	ROOT->nickserv.PushCommand("^SET\\s+PASS(W(OR)?D)?$",
							   &ns_Set_Password);
	ROOT->nickserv.PushCommand("^SET\\s+E-?MAIL$",
							   &ns_Set_Email);
	ROOT->nickserv.PushCommand("^SET\\s+(URL|WWW|WEB(SITE)?|HTTP)$",
							   &ns_Set_Website);
	ROOT->nickserv.PushCommand("^SET\\s+(UIN|ICQ)$",
							   &ns_Set_Icq);
	ROOT->nickserv.PushCommand("^SET\\s+(AIM)$",
							   &ns_Set_Aim);
	ROOT->nickserv.PushCommand("^SET\\s+MSN$",
							   &ns_Set_Msn);
	ROOT->nickserv.PushCommand("^SET\\s+JABBER$",
							   &ns_Set_Jabber);
	ROOT->nickserv.PushCommand("^SET\\s+YAHOO$",
							   &ns_Set_Yahoo);
	ROOT->nickserv.PushCommand("^SET\\s+DESC(RIPT(ION)?)?$",
							   &ns_Set_Description);
	ROOT->nickserv.PushCommand("^SET\\s+COMMENT$",
							   &ns_Set_Comment);
	ROOT->nickserv.PushCommand("^SET\\s+LANG(UAGE)?$",
							   &ns_Set_Language);
	ROOT->nickserv.PushCommand("^SET\\s+PROT(ECT)?$",
							   &ns_Set_Protect);
	ROOT->nickserv.PushCommand("^SET\\s+SECURE$",
							   &ns_Set_Secure);
	ROOT->nickserv.PushCommand("^SET\\s+NOMEMO$",
							   &ns_Set_Nomemo);
	ROOT->nickserv.PushCommand("^SET\\s+PRIV(ATE)?$",
							   &ns_Set_Private);
	ROOT->nickserv.PushCommand("^SET\\s+((PRIV)?MSG)$",
							   &ns_Set_Privmsg);
	ROOT->nickserv.PushCommand("^SET\\s+NO?EX(PIRE)?$",
							   &ns_Set_Noexpire, comm_sop);
	ROOT->nickserv.PushCommand("^SET\\s+PIC(TURE)?$",
							   &ns_Set_Picture);
	ROOT->nickserv.PushCommand("^SET\\s+HELP$",
							   boost::bind(&Service::AuxHelp, &ROOT->nickserv,
										   _1, _2, _3));

	ROOT->nickserv.PushCommand("^LOCK$",
				   Service::CommandMerge(ROOT->nickserv, 1, 2), comm_sop);
	ROOT->nickserv.PushCommand("^LOCK\\s+LANG(UAGE)?$",
							   &ns_Lock_Language, comm_sop);
	ROOT->nickserv.PushCommand("^LOCK\\s+PROT(ECT)?$",
							   &ns_Lock_Protect, comm_sop);
	ROOT->nickserv.PushCommand("^LOCK\\s+SECURE$",
							   &ns_Lock_Secure, comm_sop);
	ROOT->nickserv.PushCommand("^LOCK\\s+NOMEMO$",
							   &ns_Lock_Nomemo, comm_sop);
	ROOT->nickserv.PushCommand("^LOCK\\s+PRIV(ATE)?$",
							   &ns_Lock_Private, comm_sop);
	ROOT->nickserv.PushCommand("^LOCK\\s+((PRIV)?MSG)$",
							   &ns_Lock_Privmsg, comm_sop);
	ROOT->nickserv.PushCommand("^LOCK\\s+HELP$",
							   boost::bind(&Service::AuxHelp, &ROOT->nickserv,
										   _1, _2, _3), comm_sop);

	ROOT->nickserv.PushCommand("^UN?LOCK$",
				   Service::CommandMerge(ROOT->nickserv, 1, 2), comm_sop);
	ROOT->nickserv.PushCommand("^UN?LOCK\\s+LANG(UAGE)?$",
							   &ns_Unlock_Language, comm_sop);
	ROOT->nickserv.PushCommand("^UN?LOCK\\s+PROT(ECT)?$",
							   &ns_Unlock_Protect, comm_sop);
	ROOT->nickserv.PushCommand("^UN?LOCK\\s+SECURE$",
							   &ns_Unlock_Secure, comm_sop);
	ROOT->nickserv.PushCommand("^UN?LOCK\\s+NOMEMO$",
							   &ns_Unlock_Nomemo, comm_sop);
	ROOT->nickserv.PushCommand("^UN?LOCK\\s+PRIV(ATE)?$",
							   &ns_Unlock_Private, comm_sop);
	ROOT->nickserv.PushCommand("^UN?LOCK\\s+((PRIV)?MSG)$",
							   &ns_Unlock_Privmsg, comm_sop);
	ROOT->nickserv.PushCommand("^UN?LOCK\\s+HELP$",
							   boost::bind(&Service::AuxHelp, &ROOT->nickserv,
										   _1, _2, _3), comm_sop);

	// These commands don't operate on any nickname.
	ROOT->nickserv.PushCommand("^(STORED)?LIST$", &ns_StoredList);
	ROOT->nickserv.PushCommand("^LIVE(LIST)?$", &ns_LiveList, comm_oper);

	MT_EE
}

