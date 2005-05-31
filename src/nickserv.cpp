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
#include "storednick.h"
#include "storeduser.h"

#include <mantra/core/trace.h>

static bool ns_Register(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("ns_Register" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);

	if (params.size() < 2)
	{
		SEND(service, user,
			 N_("Insufficient parameters for %1% command."),
			 params[0]);
		MT_RET(false);
	}

	boost::shared_ptr<StoredNick> nick = ROOT->data.Get_StoredNick(user->Name());
	if (nick)
	{
		SEND(service, user,
			 N_("Nickname %1% is already registered."), user->Name());
		MT_RET(false);
	}

	static mantra::iequal_to<std::string> cmp;
	if (params[1].size() < 5 || cmp(params[1], user->Name()))
	{
		NSEND(service, user, N_("Password is not complex enough."));
		MT_RET(false);
	}

	if (params.size() > 2)
	{
		boost::regex rx("^[^[:space:]@]+@[^[:space:]@]+$");
		if (!boost::regex_match(params[2], rx))
		{
			NSEND(service, user, N_("Invalid e-mail address specified."));
			MT_RET(false);
		}
	}

	nick = StoredNick::create(user->Name(), params[1]);
	if (!nick)
	{
		SEND(service, user, N_("Coult not register nickname %1%."),
			 user->Name());
		MT_RET(false);
	}

	ROOT->data.Add(nick);
	if (params.size() > 2)
	{
		nick->User()->Email(params[2]);
		if (params.size() > 3)
		{
			std::string desc(params[3]);
			for (size_t i = 4; i < params.size(); ++i)
				desc += " " + params[i];
			nick->User()->Description(desc);
		}
	}

	std::string mask(user->User());
	if (mask[0u] == '~')
		mask[0u] = '*';
	else
		mask.insert(0, 1, '*');

	if (mantra::is_inet_address(user->Host()))
		mask += '@' + user->Host().substr(0, user->Host().rfind('.')+1) + '*';
	else if (mantra::is_inet6_address(user->Host()))
		mask += '@' + user->Host().substr(0, user->Host().rfind(':')+1) + '*';
	else
	{
		std::string::size_type pos = user->Host().find('.');
		if (pos == std::string::npos)
			mask += '@' + user->Host();
		else
			mask += "@*" + user->Host().substr(pos);
	}
	nick->User()->ACCESS_Add(mask);
	user->Identify(params[1]);

    SEND(service, user, N_("Nickname %1% has been registered with password \x02%2%\x0f."),
		 user->Name() % params[1].substr(0, 32));

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

	if (!user->Identified())
	{
		SEND(service, user, N_("You must be identified to use the %1% command."),
			 params[0]);
		MT_RET(false);
	}

	user->Stored()->Drop();
	NSEND(service, user, N_("Your nickname has been dropped."));

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

	if (params.size() < 3)
	{
		SEND(service, user,
			 N_("Insufficient parameters for %1% command."),
			 params[0]);
		MT_RET(false);
	}

	boost::shared_ptr<StoredNick> nick = ROOT->data.Get_StoredNick(params[1]);
	if (!nick)
	{
		SEND(service, user,
			 N_("Nickname %1% is not registered."), params[1]);
		MT_RET(false);
	}

	if (!nick->User()->CheckPassword(params[2]))
	{
		NSEND(service, user,
			  N_("Password incorrect, your attempt has been logged."));
		MT_RET(false);
	}

	nick = StoredNick::create(user->Name(), nick->User());
	user->Identify(params[2]);
    SEND(service, user, N_("Nickname %1% has been linked to %2%."),
		 user->Name() % nick->Name());

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

	if (params.size() < 2)
	{
		SEND(service, user,
			 N_("Insufficient parameters for %1% command."),
			 params[0]);
		MT_RET(false);
	}

	boost::shared_ptr<StoredNick> nick = ROOT->data.Get_StoredNick(user->Name());
	if (!nick)
	{
		SEND(service, user,
			 N_("Nickname %1% is not registered."), user->Name());
		MT_RET(false);
	}

	if (user->Identified())
	{
		NSEND(service, user, N_("You are already identified."));
		MT_RET(true);
	}

	if (!user->Identify(params[1]))
	{
		NSEND(service, user,
			  N_("Password incorrect, your attempt has been logged."));
		MT_RET(false);
	}

	NSEND(service, user, N_("Password correct, you are now identified."));

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

	// TODO: To be implemented.
	SEND(service, user,
		 N_("The %1% command has not yet been implemented."), params[0]);

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

	if (params.size() < 3)
	{
		SEND(service, user,
			 N_("Insufficient parameters for %1% command."),
			 params[0]);
		MT_RET(false);
	}

	static mantra::iequal_to<std::string> cmp;
	if (cmp(user->Name(), params[1]))
	{
		NSEND(service, user, N_("Are you nuts?"));
		MT_RET(false);
	}

	boost::shared_ptr<LiveUser> target = ROOT->data.Get_LiveUser(params[1]);
	if (!target)
	{
		SEND(service, user,
			 N_("User %1% is not currently online."), params[1]);
		MT_RET(false);
	}

	boost::shared_ptr<StoredNick> nick = ROOT->data.Get_StoredNick(params[1]);
	if (!nick)
	{
		SEND(service, user,
			 N_("Nickname %1% is not registered."), params[1]);
		MT_RET(false);
	}

	if (!nick->User()->CheckPassword(params[2]))
	{
		NSEND(service, user,
			  N_("Password incorrect, your attempt has been logged."));
		MT_RET(false);
	}

	service->GetService()->KILL(service, target,
		(boost::format(_("GHOST command used by %1%")) %
					   user->Name()).str());
	SEND(service, user, N_("User using nickname %1% has been killed."),
		 params[1]);

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

	// TODO: To be implemented.
	SEND(service, user,
		 N_("The %1% command has not yet been implemented."), params[0]);

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

	// TODO: To be implemented.
	SEND(service, user,
		 N_("The %1% command has not yet been implemented."), params[0]);

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

	if (params.size() < 3)
	{
		SEND(service, user,
			 N_("Insufficient parameters for %1% command."),
			 params[0]);
		MT_RET(false);
	}

	static mantra::iequal_to<std::string> cmp;
	if (cmp(user->Name(), params[1]))
	{
		NSEND(service, user, N_("Are you nuts?"));
		MT_RET(false);
	}

	boost::shared_ptr<StoredNick> nick = user->Stored();
	if (!nick)
	{
		SEND(service, user,
			 N_("Nickname %1% is not registered or you are not recognized as this nickname."), user->Name());
		MT_RET(false);
	}

	boost::shared_ptr<StoredNick> target = ROOT->data.Get_StoredNick(params[1]);
	if (!target)
	{
		SEND(service, user,
			 N_("Nickname %1% is not registered."), params[1]);
		MT_RET(false);
	}

	std::string reason(params[2]);
	for (size_t i = 3; i < params.size(); ++i)
		reason += ' ' + reason;

	target->User()->Suspend(nick, reason);
	SEND(service, user, N_("Nickname %1% has been suspended."), target->Name());

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

	if (params.size() < 2)
	{
		SEND(service, user,
			 N_("Insufficient parameters for %1% command."),
			 params[0]);
		MT_RET(false);
	}

	static mantra::iequal_to<std::string> cmp;
	if (cmp(user->Name(), params[1]))
	{
		NSEND(service, user, N_("You cannot unsuspend yourself."));
		MT_RET(false);
	}

	boost::shared_ptr<StoredNick> target = ROOT->data.Get_StoredNick(params[1]);
	if (!target)
	{
		SEND(service, user,
			 N_("Nickname %1% is not registered."), params[1]);
		MT_RET(false);
	}

	target->User()->Unsuspend();
	SEND(service, user, N_("Nickname %1% has been unsuspended."), target->Name());

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

	// TODO: To be implemented.
	SEND(service, user,
		 N_("The %1% command has not yet been implemented."), params[0]);

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

	if (params.size() < 3)
	{
		SEND(service, user,
			 N_("Insufficient parameters for %1% command."),
			 params[0]);
		MT_RET(false);
	}

	static mantra::iequal_to<std::string> cmp;
	if (cmp(user->Name(), params[1]))
	{
		NSEND(service, user, N_("Use the standard SET PASSWORD on yourself."));
		MT_RET(false);
	}

	if (params[2].size() < 5 || cmp(params[1], params[2]))
	{
		NSEND(service, user, N_("Password is not complex enough."));
		MT_RET(false);
	}

	boost::shared_ptr<StoredNick> target = ROOT->data.Get_StoredNick(params[1]);
	if (!target)
	{
		SEND(service, user,
			 N_("Nickname %1% is not registered."), params[1]);
		MT_RET(false);
	}

	target->User()->Password(params[2]);
    SEND(service, user, N_("Password for nickname %1% has been set to \x02%2%\x0f."),
		 target->Name() % params[2].substr(0, 32));

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

	if (!user->Identified())
	{
		SEND(service, user, N_("You must identify before using the %1% command."),
			 params[0]);
		MT_RET(false);
	}

	boost::shared_ptr<StoredNick> nick = user->Stored();
	if (!nick)
	{
		SEND(service, user,
			 N_("Nickname %1% is not registered or you are not recognized as this nickname."), user->Name());
		MT_RET(false);
	}

	std::string mask(user->User());
	if (mask[0u] == '~')
		mask[0u] = '*';
	else
		mask.insert(0, 1, '*');

	if (mantra::is_inet_address(user->Host()))
		mask += '@' + user->Host().substr(0, user->Host().rfind('.')+1) + '*';
	else if (mantra::is_inet6_address(user->Host()))
		mask += '@' + user->Host().substr(0, user->Host().rfind(':')+1) + '*';
	else
	{
		std::string::size_type pos = user->Host().find('.');
		if (pos == std::string::npos)
			mask += '@' + user->Host();
		else
			mask += "@*" + user->Host().substr(pos);
	}

	if (nick->User()->ACCESS_Matches(mask))
	{
		NSEND(service, user,
			 N_("A mask already matching your current hostmask already exists."));
		MT_RET(false);
	}

	nick->User()->ACCESS_Add(mask);
	SEND(service, user, N_("Mask %1% has been added to your nickname."),
		 mask);

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

	if (params.size() < 2)
	{
		SEND(service, user,
			 N_("Insufficient parameters for %1% command."),
			 params[0]);
		MT_RET(false);
	}

	if (!user->Identified())
	{
		SEND(service, user, N_("You must identify before using the %1% command."),
			 params[0]);
		MT_RET(false);
	}

	boost::shared_ptr<StoredNick> nick = user->Stored();
	if (!nick)
	{
		SEND(service, user,
			 N_("Nickname %1% is not registered or you are not recognized as this nickname."), user->Name());
		MT_RET(false);
	}

	static boost::regex rx("^[^[:space:][:cntrl:]@]+@"
						   "([-[:alnum:]*?.]+|[[:xdigit:]*?:]+)$");
	if (!boost::regex_match(params[1], rx))
	{
		SEND(service, user, N_("Invalid hostmask %1%."), params[1]);
		MT_RET(false);
	}

	if (nick->User()->ACCESS_Matches(params[1]))
	{
		NSEND(service, user,
			 N_("A mask already matching your current hostmask already exists."));
		MT_RET(false);
	}

	nick->User()->ACCESS_Add(params[1]);
	SEND(service, user, N_("Mask %1% has been added to your nickname."),
		 params[1]);

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

	if (params.size() < 2)
	{
		SEND(service, user,
			 N_("Insufficient parameters for %1% command."),
			 params[0]);
		MT_RET(false);
	}

	if (!user->Identified())
	{
		SEND(service, user, N_("You must identify before using the %1% command."),
			 params[0]);
		MT_RET(false);
	}

	boost::shared_ptr<StoredNick> nick = user->Stored();
	if (!nick)
	{
		SEND(service, user,
			 N_("Nickname %1% is not registered or you are not recognized as this nickname."), user->Name());
		MT_RET(false);
	}

	std::vector<unsigned int> v;
	if (!mantra::ParseNumbers(params[1], v))
	{
		std::map<boost::uint32_t, std::pair<std::string, boost::posix_time::ptime> > ent;
		nick->User()->ACCESS_Get(ent);
		std::map<boost::uint32_t, std::pair<std::string, boost::posix_time::ptime> >::iterator i;
		for (i = ent.begin(); i != ent.end(); ++i)
		{
			if (mantra::glob_match(params[1], i->second.first, true))
				v.push_back(i->first);
		}

		if (v.empty())
		{
			SEND(service, user,
				 N_("No entries matching %1% could be found on your access list."),
				 params[1]);
		}
	}

	std::vector<unsigned int> ne;
	for (size_t i = 0; i < v.size(); ++i)
	{
		if (!nick->User()->ACCESS_Exists(v[i]))
		{
			ne.push_back(v[i]);
			continue;
		}

		nick->User()->ACCESS_Del(v[i]);
	}
	if (ne.empty())
	{
		SEND(service, user,
			 N_("%1% entries removed from your access list."),
			 v.size());
	}
	else if (v.size() != ne.size())
	{
		SEND(service, user,
			 N_("%1% entries removed from your access list and %2% entries specified could not be found."),
			 (v.size() - ne.size()) % ne.size());
	}
	else
	{
		NSEND(service, user,
			  N_("No specified entries could be found on your access list."));
	}

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

	boost::shared_ptr<StoredNick> nick = user->Stored();
	if (!nick)
	{
		SEND(service, user,
			 N_("Nickname %1% is not registered or you are not recognized as this nickname."), user->Name());
		MT_RET(false);
	}

	std::map<boost::uint32_t, std::pair<std::string, boost::posix_time::ptime> > ent;
	nick->User()->ACCESS_Get(ent);
	if (ent.empty())
	{
		NSEND(service, user, N_("Your access list is empty."));
		MT_RET(true);
	}

	SEND(service, user, N_("Access list for nickname %1%:"), user->Name());
	std::map<boost::uint32_t, std::pair<std::string, boost::posix_time::ptime> >::iterator i;
	for (i = ent.begin(); i != ent.end(); ++i)
	{
		if (params.size() > 1 &&
			!mantra::glob_match(params[1], i->second.first, true))
			continue;

		SEND(service, user, N_("%1%. %2% [%3%]"),
			 i->first % i->second.first % i->second.second);
	}

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

void init_nickserv_functions(Service &serv)
{
	MT_EB
	MT_FUNC("init_nickserv_functions");

	std::vector<std::string> comm_oper, comm_sop, comm_opersop;
	comm_oper.push_back(ROOT->ConfigValue<std::string>("commserv.oper.name"));
	comm_sop.push_back(ROOT->ConfigValue<std::string>("commserv.sop.name"));
	comm_opersop.push_back(ROOT->ConfigValue<std::string>("commserv.oper.name"));
	comm_opersop.push_back(ROOT->ConfigValue<std::string>("commserv.sop.name"));

	serv.PushCommand("^HELP$", boost::bind(&Service::Help, &serv,
										   _1, _2, _3));

	serv.PushCommand("^REGISTER$", &ns_Register);
	serv.PushCommand("^DROP$", &ns_Drop);
	serv.PushCommand("^LINK$", &ns_Link);
	serv.PushCommand("^ID(ENT(IFY)?)?$", &ns_Identify);
	serv.PushCommand("^INFO$", &ns_Info);
	serv.PushCommand("^GHOST$", &ns_Ghost);
	serv.PushCommand("^REC(OVER)?$", &ns_Recover);
	serv.PushCommand("^SEND$", &ns_Send);
	serv.PushCommand("^SUSPEND$", &ns_Suspend, comm_sop);
	serv.PushCommand("^UN?SUSPEND$", &ns_Unsuspend, comm_sop);
	serv.PushCommand("^FORBID$", &ns_Forbid, comm_sop);
	serv.PushCommand("^SETPASS(WORD)?$", &ns_Setpass, comm_sop);

	serv.PushCommand("^ACC(ESS)?$",
					 Service::CommandMerge(serv, 0, 1));
	serv.PushCommand("^ACC(ESS)?\\s+CUR(R(ENT)?)?$",
					 &ns_Access_Current);
	serv.PushCommand("^ACC(ESS)?\\s+ADD$",
					 &ns_Access_Add);
	serv.PushCommand("^ACC(ESS)?\\s+(ERASE|DEL(ETE)?)$",
					 &ns_Access_Del);
	serv.PushCommand("^ACC(ESS)?\\s+(LIST|VIEW)$",
					 &ns_Access_List);
	serv.PushCommand("^ACC(ESS)?\\s+HELP$",
					 boost::bind(&Service::AuxHelp, &serv,
								 _1, _2, _3));

	serv.PushCommand("^IGNORE$",
					 Service::CommandMerge(serv, 0, 1));
	serv.PushCommand("^IGNORE\\s+ADD$",
					 &ns_Ignore_Add);
	serv.PushCommand("^IGNORE\\s+(ERASE|DEL(ETE)?)$",
					 &ns_Ignore_Del);
	serv.PushCommand("^IGNORE\\s+(LIST|VIEW)$",
					 &ns_Ignore_List);
	serv.PushCommand("^IGNORE\\s+HELP$",
					 boost::bind(&Service::AuxHelp, &serv,
								 _1, _2, _3));

	serv.PushCommand("^SET$",
					 Service::CommandMerge(serv, 0, 1));
	serv.PushCommand("^SET\\s+PASS(W(OR)?D)?$",
					 &ns_Set_Password);
	serv.PushCommand("^SET\\s+E-?MAIL$",
					 &ns_Set_Email);
	serv.PushCommand("^SET\\s+(URL|WWW|WEB(SITE)?|HTTP)$",
					 &ns_Set_Website);
	serv.PushCommand("^SET\\s+(UIN|ICQ)$",
					 &ns_Set_Icq);
	serv.PushCommand("^SET\\s+(AIM)$",
					 &ns_Set_Aim);
	serv.PushCommand("^SET\\s+MSN$",
					 &ns_Set_Msn);
	serv.PushCommand("^SET\\s+JABBER$",
					 &ns_Set_Jabber);
	serv.PushCommand("^SET\\s+YAHOO$",
					 &ns_Set_Yahoo);
	serv.PushCommand("^SET\\s+DESC(RIPT(ION)?)?$",
					 &ns_Set_Description);
	serv.PushCommand("^SET\\s+COMMENT$",
					 &ns_Set_Comment);
	serv.PushCommand("^SET\\s+LANG(UAGE)?$",
					 &ns_Set_Language);
	serv.PushCommand("^SET\\s+PROT(ECT)?$",
					 &ns_Set_Protect);
	serv.PushCommand("^SET\\s+SECURE$",
					 &ns_Set_Secure);
	serv.PushCommand("^SET\\s+NOMEMO$",
					 &ns_Set_Nomemo);
	serv.PushCommand("^SET\\s+PRIV(ATE)?$",
					 &ns_Set_Private);
	serv.PushCommand("^SET\\s+((PRIV)?MSG)$",
					 &ns_Set_Privmsg);
	serv.PushCommand("^SET\\s+NO?EX(PIRE)?$",
					 &ns_Set_Noexpire, comm_sop);
	serv.PushCommand("^SET\\s+PIC(TURE)?$",
					 &ns_Set_Picture);
	serv.PushCommand("^SET\\s+HELP$",
					 boost::bind(&Service::AuxHelp, &serv,
								 _1, _2, _3));

	serv.PushCommand("^LOCK$",
					 Service::CommandMerge(serv, 0, 1), comm_sop);
	serv.PushCommand("^LOCK\\s+LANG(UAGE)?$",
					 &ns_Lock_Language, comm_sop);
	serv.PushCommand("^LOCK\\s+PROT(ECT)?$",
					 &ns_Lock_Protect, comm_sop);
	serv.PushCommand("^LOCK\\s+SECURE$",
					 &ns_Lock_Secure, comm_sop);
	serv.PushCommand("^LOCK\\s+NOMEMO$",
					 &ns_Lock_Nomemo, comm_sop);
	serv.PushCommand("^LOCK\\s+PRIV(ATE)?$",
					 &ns_Lock_Private, comm_sop);
	serv.PushCommand("^LOCK\\s+((PRIV)?MSG)$",
					 &ns_Lock_Privmsg, comm_sop);
	serv.PushCommand("^LOCK\\s+HELP$",
					 boost::bind(&Service::AuxHelp, &serv,
								 _1, _2, _3), comm_sop);

	serv.PushCommand("^UN?LOCK$",
					 Service::CommandMerge(serv, 0, 1), comm_sop);
	serv.PushCommand("^UN?LOCK\\s+LANG(UAGE)?$",
					 &ns_Unlock_Language, comm_sop);
	serv.PushCommand("^UN?LOCK\\s+PROT(ECT)?$",
					 &ns_Unlock_Protect, comm_sop);
	serv.PushCommand("^UN?LOCK\\s+SECURE$",
					 &ns_Unlock_Secure, comm_sop);
	serv.PushCommand("^UN?LOCK\\s+NOMEMO$",
					 &ns_Unlock_Nomemo, comm_sop);
	serv.PushCommand("^UN?LOCK\\s+PRIV(ATE)?$",
					 &ns_Unlock_Private, comm_sop);
	serv.PushCommand("^UN?LOCK\\s+((PRIV)?MSG)$",
					 &ns_Unlock_Privmsg, comm_sop);
	serv.PushCommand("^UN?LOCK\\s+HELP$",
					 boost::bind(&Service::AuxHelp, &serv,
								 _1, _2, _3), comm_sop);

	// These commands don't operate on any nickname.
	serv.PushCommand("^(STORED)?LIST$", &ns_StoredList);
	serv.PushCommand("^LIVE(LIST)?$", &ns_LiveList, comm_oper);

	MT_EE
}

