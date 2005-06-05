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
			 boost::algorithm::to_upper_copy(params[0]));
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
		boost::regex rx("^[^[:space:][:cntrl:]@]+@"
						"([[:alnum:]][-[:alnum:]]*\\.)*"
						"[[:alnum:]][-[:alnum:]]*$");
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

    SEND(service, user, N_("Nickname %1% has been registered with password \002%2%\017."),
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
			 boost::algorithm::to_upper_copy(params[0]));
		MT_RET(false);
	}

	if (params.size() < 2)
	{
		std::string token;
		for (size_t i = 0; i < ROOT->ConfigValue<unsigned int>("nickserv.drop-length"); ++i)
		{
			char c = (rand() % 62);
			if (c < 10)
				c += '0';
			else if (c < 36)
				c += 'A' - 10;
			else
				c += 'a' - 36;
			token.append(1, c);
		}
		user->DropToken(token);
		SEND(service, user, N_("Please re-issue your %1% command within %2% with the following parameter: %3%"),
			 boost::algorithm::to_upper_copy(params[0]) %
			 mantra::DurationToString(ROOT->ConfigValue<mantra::duration>("nickserv.drop"),
									  mantra::Second) %
			 token);
	}
	else
	{
		std::string token = user->DropToken();
		if (token.empty())
		{
			NSEND(service, user, N_("Drop token has expired (nickname not dropped)."));
		}
		else if (params[1] == token)
		{
			user->DropToken(std::string());
			user->Stored()->Drop();
			NSEND(service, user, N_("Your nickname has been dropped."));
		}
		else
		{
			NSEND(service, user, N_("Drop token incorrect (nickname not dropped)."));
		}
	}

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
			 boost::algorithm::to_upper_copy(params[0]));
		MT_RET(false);
	}

	boost::shared_ptr<StoredNick> nick = ROOT->data.Get_StoredNick(user->Name());
	if (nick)
	{
		SEND(service, user,
			 N_("Nickname %1% is already registered."), user->Name());
		MT_RET(false);
	}

	nick = ROOT->data.Get_StoredNick(params[1]);
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

	std::string other = nick->Name();
	nick = StoredNick::create(user->Name(), nick->User());
	ROOT->data.Add(nick);
	user->Identify(params[2]);
    SEND(service, user, N_("Nickname %1% has been linked to %2%."),
		 user->Name() % other);

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
			 boost::algorithm::to_upper_copy(params[0]));
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

	if (params.size() < 2)
	{
		SEND(service, user,
			 N_("Insufficient parameters for %1% command."),
			 boost::algorithm::to_upper_copy(params[0]));
		MT_RET(false);
	}

	boost::shared_ptr<StoredNick> nick = ROOT->data.Get_StoredNick(params[1]);
	if (!nick)
	{
		SEND(service, user,
			 N_("Nickname %1% is not registered."), params[1]);
		MT_RET(false);
	}

	nick->SendInfo(service, user);

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
			 boost::algorithm::to_upper_copy(params[0]));
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
		 N_("The %1% command has not yet been implemented."),
		 boost::algorithm::to_upper_copy(params[0]));

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
		 N_("The %1% command has not yet been implemented."),
		 boost::algorithm::to_upper_copy(params[0]));

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
			 boost::algorithm::to_upper_copy(params[0]));
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
			 boost::algorithm::to_upper_copy(params[0]));
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

static bool ns_Forbid_Add(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("ns_Forbid_Add" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);

	if (params.size() < 2)
	{
		SEND(service, user,
			 N_("Insufficient parameters for %1% command."),
			 boost::algorithm::to_upper_copy(params[0]));
		MT_RET(false);
	}

	boost::shared_ptr<StoredNick> nick = user->Stored();
	if (!nick)
	{
		SEND(service, user,
			 N_("Nickname %1% is not registered or you are not recognized as this nickname."), user->Name());
		MT_RET(false);
	}

	static boost::regex nick_rx("^[[:alpha:]\\x5B-\\x60\\x7B-\\x7D?*][-[:alnum:]\\x5B-\\x60\\x7B-\\x7D?*]*$");
	if (!boost::regex_match(params[1], nick_rx))
	{
		NSEND(service, user,
			  N_("Nickname mask specified is not a valid."));
		MT_RET(false);
	}

	ROOT->data.Forbid_Add(params[1], nick);
	SEND(service, user,
		 N_("Nickname mask \002%1%\017 has been added to the forbidden list."),
		 params[1]);

	MT_RET(true);
	MT_EE
}

static bool ns_Forbid_Del(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("ns_Forbid_Del" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);

	if (params.size() < 2)
	{
		SEND(service, user,
			 N_("Insufficient parameters for %1% command."),
			 boost::algorithm::to_upper_copy(params[0]));
		MT_RET(false);
	}

	if (ROOT->data.Forbid_Del(params[1]))
		SEND(service, user,
			 N_("Nickname mask \002%1%\017 has been removed from the forbidden list."),
			 params[1]);
	else
		SEND(service, user,
			 N_("Nickname mask \002%1%\017 was not found on the forbidden list."),
			 params[1]);

	MT_RET(true);
	MT_EE
}

static bool ns_Forbid_List(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("ns_Forbid_List" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);

	std::vector<std::string> forbid = ROOT->data.Forbid_List_Nick();

	if (forbid.empty())
		NSEND(service, user, N_("The forbidden nickname list is empty"));
	else
	{
		NSEND(service, user, N_("Forbidden Nicknames:"));
		for (size_t i=0; i<forbid.size(); ++i)
		{
//			SEND(service, user, N_("%1% [Added by %2% at %3%]"),
//				 forbid[i].entry, forbid[i].added_by, forbid[i].added);
			SEND(service, user, N_("%1%"), forbid[i]);
		}
	}

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
			 boost::algorithm::to_upper_copy(params[0]));
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
    SEND(service, user, N_("Password for nickname %1% has been set to \002%2%\017."),
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
			 boost::algorithm::to_upper_copy(params[0]));
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
	SEND(service, user, N_("Mask \002%1%\017 has been added to your nickname."),
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
			 boost::algorithm::to_upper_copy(params[0]));
		MT_RET(false);
	}

	if (!user->Identified())
	{
		SEND(service, user, N_("You must identify before using the %1% command."),
			 boost::algorithm::to_upper_copy(params[0]));
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
		SEND(service, user,
			 N_("A mask already matching %1% already exists."),
			 params[1]);
		MT_RET(false);
	}

	nick->User()->ACCESS_Add(params[1]);
	SEND(service, user, N_("Mask \002%1%\017 has been added to your nickname."),
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
			 boost::algorithm::to_upper_copy(params[0]));
		MT_RET(false);
	}

	if (!user->Identified())
	{
		SEND(service, user, N_("You must identify before using the %1% command."),
			 boost::algorithm::to_upper_copy(params[0]));
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
				 N_("No entries matching \002%1%\017 could be found on your access list."),
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

	bool first = false;
	std::map<boost::uint32_t, std::pair<std::string, boost::posix_time::ptime> >::iterator i;
	for (i = ent.begin(); i != ent.end(); ++i)
	{
		if (params.size() > 1 &&
			!mantra::glob_match(params[1], i->second.first, true))
			continue;

		if (!first)
		{
			SEND(service, user, N_("Access list for nickname \002%1%\017:"),
				 user->Name());
			first = true;
		}
		SEND(service, user, N_("%1%. %2% [%3%]"),
			 i->first % i->second.first % i->second.second);
	}

	if (!first)
	{
		if (params.size() > 1)
			SEND(service, user, N_("No entries matching \002%1%\017 are on your access list."),
				 params[1]);
		else
			NSEND(service, user, N_("Your access list is empty."));
		MT_RET(false);
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

	if (params.size() < 2)
	{
		SEND(service, user,
			 N_("Insufficient parameters for %1% command."),
			 boost::algorithm::to_upper_copy(params[0]));
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

	if (nick == target)
	{
		NSEND(service, user, N_("Are you nuts?"));
		MT_RET(false);
	}

	if (nick->User()->IGNORE_Matches(target->User()))
	{
		NSEND(service, user,
			 N_("The nickname %1% is already on your memo ignore list."));
		MT_RET(false);
	}

	nick->User()->IGNORE_Add(target->User());
	SEND(service, user, N_("Nickname \002%1%\017 has been added to your memo ignore list."),
		 target->Name());

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

	if (params.size() < 2)
	{
		SEND(service, user,
			 N_("Insufficient parameters for %1% command."),
			 boost::algorithm::to_upper_copy(params[0]));
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
		std::map<boost::uint32_t, std::pair<boost::shared_ptr<StoredUser>, boost::posix_time::ptime> > ent;
		nick->User()->IGNORE_Get(ent);
		std::map<boost::uint32_t, std::pair<boost::shared_ptr<StoredUser>, boost::posix_time::ptime> >::iterator i;
		for (i = ent.begin(); i != ent.end(); ++i)
		{
			StoredUser::my_nicks_t nicks = i->second.first->Nicks();
			if (nicks.empty())
				continue;
			StoredUser::my_nicks_t::const_iterator j;
			for (j = nicks.begin(); j != nicks.end(); ++j)
				if (**j == params[1])
				{
					v.push_back(i->first);
					break;
				}
			if (j != nicks.end())
				break;
		}

		if (v.empty())
		{
			SEND(service, user,
				 N_("No entries matching \002%1%\017 could be found on your memo ignore list."),
				 params[1]);
		}
	}

	std::vector<unsigned int> ne;
	for (size_t i = 0; i < v.size(); ++i)
	{
		if (!nick->User()->IGNORE_Exists(v[i]))
		{
			ne.push_back(v[i]);
			continue;
		}

		nick->User()->IGNORE_Del(v[i]);
	}
	if (ne.empty())
	{
		SEND(service, user,
			 N_("%1% entries removed from your memo ignore list."),
			 v.size());
	}
	else if (v.size() != ne.size())
	{
		SEND(service, user,
			 N_("%1% entries removed from your memo ignore list and %2% entries specified could not be found."),
			 (v.size() - ne.size()) % ne.size());
	}
	else
	{
		NSEND(service, user,
			  N_("No specified entries could be found on your memo ignore list."));
	}

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

	boost::shared_ptr<StoredNick> nick = user->Stored();
	if (!nick)
	{
		SEND(service, user,
			 N_("Nickname %1% is not registered or you are not recognized as this nickname."), user->Name());
		MT_RET(false);
	}

	std::map<boost::uint32_t, std::pair<boost::shared_ptr<StoredUser>, boost::posix_time::ptime> > ent;
	nick->User()->IGNORE_Get(ent);
	if (ent.empty())
	{
		NSEND(service, user, N_("Your memo ignore list is empty."));
		MT_RET(true);
	}

	bool first = false;
	std::map<boost::uint32_t, std::pair<boost::shared_ptr<StoredUser>, boost::posix_time::ptime> >::iterator i;
	for (i = ent.begin(); i != ent.end(); ++i)
	{
		StoredUser::my_nicks_t nicks = i->second.first->Nicks();
		if (nicks.empty())
			continue;

		std::string str;
		StoredUser::my_nicks_t::const_iterator j;
		for (j = nicks.begin(); j != nicks.end(); ++j)
		{
			if (!*j)
				continue;

			if (!str.empty())
				str += ", ";
			str += (*j)->Name();
		}

		if (!first)
		{
			SEND(service, user, N_("Memo ignore list for nickname \002%1%\017:"),
				 user->Name());
			first = true;
		}
		SEND(service, user, N_("%1%. %2% [%3%]"),
			 i->first % str % i->second.second);
	}

	if (!first)
	{
		NSEND(service, user, N_("Your memo ignore list is empty."));
		MT_RET(false);
	}


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

	if (params.size() < 2)
	{
		SEND(service, user,
			 N_("Insufficient parameters for %1% command."),
			 boost::algorithm::to_upper_copy(params[0]));
		MT_RET(false);
	}

	if (!user->Identified())
	{
		SEND(service, user, N_("You must identify before using the %1% command."),
			 boost::algorithm::to_upper_copy(params[0]));
		MT_RET(false);
	}

	boost::shared_ptr<StoredNick> nick = user->Stored();
	if (!nick)
	{
		SEND(service, user,
			 N_("Nickname %1% is not registered or you are not recognized as this nickname."), user->Name());
		MT_RET(false);
	}

	static mantra::iequal_to<std::string> cmp;
	if (params[1].size() < 5 || cmp(params[1], user->Name()))
	{
		NSEND(service, user, N_("Password is not complex enough."));
		MT_RET(false);
	}

	nick->User()->Password(params[1]);
    SEND(service, user, N_("Your password has been set to \002%1%\017."),
		 params[1].substr(0, 32));

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

	if (params.size() < 2)
	{
		SEND(service, user,
			 N_("Insufficient parameters for %1% command."),
			 boost::algorithm::to_upper_copy(params[0]));
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
						   "([[:alnum:]][-[:alnum:]]*\\.)*"
						   "[[:alnum:]][-[:alnum:]]*$");
	if (!boost::regex_match(params[1], rx))
	{
		NSEND(service, user, N_("Invalid e-mail address specified."));
		MT_RET(false);
	}

	nick->User()->Email(params[1]);
	SEND(service, user, N_("Your e-mail address has been set to \002%1%\017."),
		 params[1]);

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

	if (params.size() < 2)
	{
		SEND(service, user,
			 N_("Insufficient parameters for %1% command."),
			 boost::algorithm::to_upper_copy(params[0]));
		MT_RET(false);
	}

	boost::shared_ptr<StoredNick> nick = user->Stored();
	if (!nick)
	{
		SEND(service, user,
			 N_("Nickname %1% is not registered or you are not recognized as this nickname."), user->Name());
		MT_RET(false);
	}

	static boost::regex rx("^(https?://)?"
						   "(("
								"((25[0-5]|2[0-4][0-9]|[01]?[0-9]{1,2})\\.){3}"
								"(25[0-5]|2[0-4][0-9]|[01]?[0-9]{1,2})"
						   ")|("
								"([[:alnum:]][-[:alnum:]]*\\.)+"
								"[[:alnum:]][-[:alnum:]]*"
						   "))(/[[:print:]]*)?$");
	if (!boost::regex_match(params[1], rx))
	{
		NSEND(service, user, N_("Invalid website address specified."));
		MT_RET(false);
	}
	std::string url(params[1]);
	if (params[1].find("http") != 0)
		url.insert(0, "http://", 7);

	nick->User()->Website(url);
	SEND(service, user, N_("Your website address has been set to \002%1%\017."),
		 url);

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

	if (params.size() < 2)
	{
		SEND(service, user,
			 N_("Insufficient parameters for %1% command."),
			 boost::algorithm::to_upper_copy(params[0]));
		MT_RET(false);
	}

	boost::shared_ptr<StoredNick> nick = user->Stored();
	if (!nick)
	{
		SEND(service, user,
			 N_("Nickname %1% is not registered or you are not recognized as this nickname."), user->Name());
		MT_RET(false);
	}

	if (params[1].find_first_not_of("0123456789") != std::string::npos)
	{
		NSEND(service, user, N_("Invalid ICQ UIN specified."));
		MT_RET(false);
	}

	nick->User()->ICQ(boost::lexical_cast<boost::uint32_t>(params[1]));
	SEND(service, user, N_("Your ICQ UIN has been set to \002%1%\017."),
		 params[1]);

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

	if (params.size() < 2)
	{
		SEND(service, user,
			 N_("Insufficient parameters for %1% command."),
			 boost::algorithm::to_upper_copy(params[0]));
		MT_RET(false);
	}

	boost::shared_ptr<StoredNick> nick = user->Stored();
	if (!nick)
	{
		SEND(service, user,
			 N_("Nickname %1% is not registered or you are not recognized as this nickname."), user->Name());
		MT_RET(false);
	}

	static boost::regex rx("^[[:alpha:]][[:alnun:]]{2-15}$");
	if (!boost::regex_match(params[1], rx))
	{
		NSEND(service, user, N_("Invalid AIM screen name specified."));
		MT_RET(false);
	}

	nick->User()->AIM(params[1]);
	SEND(service, user, N_("Your AIM screen name has been set to \002%1%\017."),
		 params[1]);

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

	if (params.size() < 2)
	{
		SEND(service, user,
			 N_("Insufficient parameters for %1% command."),
			 boost::algorithm::to_upper_copy(params[0]));
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
						   "([[:alnum:]][-[:alnum:]]*\\.)*"
						   "[[:alnum:]][-[:alnum:]]*$");
	if (!boost::regex_match(params[1], rx))
	{
		NSEND(service, user, N_("Invalid MSN account specified."));
		MT_RET(false);
	}

	nick->User()->MSN(params[1]);
	SEND(service, user, N_("Your MSN account has been set to \002%1%\017."),
		 params[1]);

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

	if (params.size() < 2)
	{
		SEND(service, user,
			 N_("Insufficient parameters for %1% command."),
			 boost::algorithm::to_upper_copy(params[0]));
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
						   "([[:alnum:]][-[:alnum:]]*\\.)*"
						   "[[:alnum:]][-[:alnum:]]*$");
	if (!boost::regex_match(params[1], rx))
	{
		NSEND(service, user, N_("Invalid Jabber ID specified."));
		MT_RET(false);
	}

	nick->User()->Jabber(params[1]);
	SEND(service, user, N_("Your Jabber ID has been set to \002%1%\017."),
		 params[1]);

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

	if (params.size() < 2)
	{
		SEND(service, user,
			 N_("Insufficient parameters for %1% command."),
			 boost::algorithm::to_upper_copy(params[0]));
		MT_RET(false);
	}

	boost::shared_ptr<StoredNick> nick = user->Stored();
	if (!nick)
	{
		SEND(service, user,
			 N_("Nickname %1% is not registered or you are not recognized as this nickname."), user->Name());
		MT_RET(false);
	}

	static boost::regex rx("^[[:alpha:]][[:alnun:]_]{0-31}$");
	if (!boost::regex_match(params[1], rx))
	{
		NSEND(service, user, N_("Invalid Yahoo! ID specified."));
		MT_RET(false);
	}

	nick->User()->Yahoo(params[1]);
	SEND(service, user, N_("Your Yahoo! ID has been set to \002%1%\017."),
		 params[1]);

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

	if (params.size() < 2)
	{
		SEND(service, user,
			 N_("Insufficient parameters for %1% command."),
			 boost::algorithm::to_upper_copy(params[0]));
		MT_RET(false);
	}

	boost::shared_ptr<StoredNick> nick = user->Stored();
	if (!nick)
	{
		SEND(service, user,
			 N_("Nickname %1% is not registered or you are not recognized as this nickname."), user->Name());
		MT_RET(false);
	}

	std::string desc(params[1]);
	for (size_t i = 2; i < params.size(); ++i)
		desc += " " + params[i];
	nick->User()->Description(desc);
	SEND(service, user, N_("Your description has been set to \002%1%\017."),
		 desc);

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

	if (params.size() < 2)
	{
		SEND(service, user,
			 N_("Insufficient parameters for %1% command."),
			 boost::algorithm::to_upper_copy(params[0]));
		MT_RET(false);
	}

	boost::shared_ptr<StoredNick> nick = user->Stored();
	if (!nick)
	{
		SEND(service, user,
			 N_("Nickname %1% is not registered or you are not recognized as this nickname."), user->Name());
		MT_RET(false);
	}

	try
	{
		std::locale(params[1]);
	}
	catch (std::exception &e)
	{
		NSEND(service, user, N_("Invalid language specified."));
		MT_RET(false);
	}

	if (nick->User()->Language(params[1]))
		SEND(service, user, N_("Your language has been set to \002%1%\017."),
			 params[1]);
	else
		NSEND(service, user, N_("Your language setting is locked and cannot be changed."));

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

	if (params.size() < 2)
	{
		SEND(service, user,
			 N_("Insufficient parameters for %1% command."),
			 boost::algorithm::to_upper_copy(params[0]));
		MT_RET(false);
	}

	boost::shared_ptr<StoredNick> nick = user->Stored();
	if (!nick)
	{
		SEND(service, user,
			 N_("Nickname %1% is not registered or you are not recognized as this nickname."), user->Name());
		MT_RET(false);
	}

	boost::logic::tribool v = mantra::get_bool(params[1]);
	if (boost::logic::indeterminate(v))
	{
		NSEND(service, user, N_("You may only specify ON or OFF."));
		MT_RET(false);
	}

	if (nick->User()->Protect(v))
		NSEND(service, user, (v
			  ? N_("Protect has been \002enabled\017.")
			  : N_("Protect has been \002disabled\017.")));
	else
		NSEND(service, user, N_("Your protect setting is locked and cannot be changed."));

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

	if (params.size() < 2)
	{
		SEND(service, user,
			 N_("Insufficient parameters for %1% command."),
			 boost::algorithm::to_upper_copy(params[0]));
		MT_RET(false);
	}

	boost::shared_ptr<StoredNick> nick = user->Stored();
	if (!nick)
	{
		SEND(service, user,
			 N_("Nickname %1% is not registered or you are not recognized as this nickname."), user->Name());
		MT_RET(false);
	}

	boost::logic::tribool v = mantra::get_bool(params[1]);
	if (boost::logic::indeterminate(v))
	{
		NSEND(service, user, N_("You may only specify ON or OFF."));
		MT_RET(false);
	}

	if (nick->User()->Secure(v))
		NSEND(service, user, (v
			  ? N_("Secure has been \002enabled\017.")
			  : N_("Secure has been \002disabled\017.")));
	else
		NSEND(service, user, N_("Your secure setting is locked and cannot be changed."));

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

	if (params.size() < 2)
	{
		SEND(service, user,
			 N_("Insufficient parameters for %1% command."),
			 boost::algorithm::to_upper_copy(params[0]));
		MT_RET(false);
	}

	boost::shared_ptr<StoredNick> nick = user->Stored();
	if (!nick)
	{
		SEND(service, user,
			 N_("Nickname %1% is not registered or you are not recognized as this nickname."), user->Name());
		MT_RET(false);
	}

	boost::logic::tribool v = mantra::get_bool(params[1]);
	if (boost::logic::indeterminate(v))
	{
		NSEND(service, user, N_("You may only specify ON or OFF."));
		MT_RET(false);
	}

	if (nick->User()->NoMemo(v))
		NSEND(service, user, (v
			  ? N_("No Memo has been \002enabled\017.")
			  : N_("No Memo has been \002disabled\017.")));
	else
		NSEND(service, user, N_("Your no memo setting is locked and cannot be changed."));

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

	if (params.size() < 2)
	{
		SEND(service, user,
			 N_("Insufficient parameters for %1% command."),
			 boost::algorithm::to_upper_copy(params[0]));
		MT_RET(false);
	}

	boost::shared_ptr<StoredNick> nick = user->Stored();
	if (!nick)
	{
		SEND(service, user,
			 N_("Nickname %1% is not registered or you are not recognized as this nickname."), user->Name());
		MT_RET(false);
	}

	boost::logic::tribool v = mantra::get_bool(params[1]);
	if (boost::logic::indeterminate(v))
	{
		NSEND(service, user, N_("You may only specify ON or OFF."));
		MT_RET(false);
	}

	if (nick->User()->Private(v))
		NSEND(service, user, (v
			  ? N_("Private has been \002enabled\017.")
			  : N_("Private has been \002disabled\017.")));
	else
		NSEND(service, user, N_("Your private setting is locked and cannot be changed."));

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

	if (params.size() < 2)
	{
		SEND(service, user,
			 N_("Insufficient parameters for %1% command."),
			 boost::algorithm::to_upper_copy(params[0]));
		MT_RET(false);
	}

	boost::shared_ptr<StoredNick> nick = user->Stored();
	if (!nick)
	{
		SEND(service, user,
			 N_("Nickname %1% is not registered or you are not recognized as this nickname."), user->Name());
		MT_RET(false);
	}

	boost::logic::tribool v = mantra::get_bool(params[1]);
	if (boost::logic::indeterminate(v))
	{
		NSEND(service, user, N_("You may only specify ON or OFF."));
		MT_RET(false);
	}

	if (nick->User()->PRIVMSG(v))
		NSEND(service, user, (v
			  ? N_("Private messaging has been \002enabled\017.")
			  : N_("Private messaging has been \002disabled\017.")));
	else
		NSEND(service, user, N_("Your private messaging setting is locked and cannot be changed."));

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

	// TODO: To be implemented.
	SEND(service, user,
		 N_("The %1% command has not yet been implemented."),
		 boost::algorithm::to_upper_copy(params[0]));

	MT_RET(true);
	MT_EE
}

static bool ns_Unset_Email(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("ns_Unset_Email" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);

	boost::shared_ptr<StoredNick> nick = user->Stored();
	if (!nick)
	{
		SEND(service, user,
			 N_("Nickname %1% is not registered or you are not recognized as this nickname."), user->Name());
		MT_RET(false);
	}

	nick->User()->Email(std::string());
	SEND(service, user, N_("Your e-mail address has been unset."),
		 params[1]);

	MT_RET(true);
	MT_EE
}

static bool ns_Unset_Website(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("ns_Unset_Website" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);

	boost::shared_ptr<StoredNick> nick = user->Stored();
	if (!nick)
	{
		SEND(service, user,
			 N_("Nickname %1% is not registered or you are not recognized as this nickname."), user->Name());
		MT_RET(false);
	}

	nick->User()->Website(std::string());
	SEND(service, user, N_("Your website address has been unset."),
		 params[1]);

	MT_RET(true);
	MT_EE
}

static bool ns_Unset_Icq(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("ns_Unset_Icq" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);

	boost::shared_ptr<StoredNick> nick = user->Stored();
	if (!nick)
	{
		SEND(service, user,
			 N_("Nickname %1% is not registered or you are not recognized as this nickname."), user->Name());
		MT_RET(false);
	}

	nick->User()->ICQ(0);
	SEND(service, user, N_("Your ICQ UIN has been unset."),
		 params[1]);

	MT_RET(true);
	MT_EE
}

static bool ns_Unset_Aim(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("ns_Unset_Aim" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);

	boost::shared_ptr<StoredNick> nick = user->Stored();
	if (!nick)
	{
		SEND(service, user,
			 N_("Nickname %1% is not registered or you are not recognized as this nickname."), user->Name());
		MT_RET(false);
	}

	nick->User()->AIM(std::string());
	SEND(service, user, N_("Your AIM screen name has been unset."),
		 params[1]);

	MT_RET(true);
	MT_EE
}

static bool ns_Unset_Msn(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("ns_Unset_Msn" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);

	boost::shared_ptr<StoredNick> nick = user->Stored();
	if (!nick)
	{
		SEND(service, user,
			 N_("Nickname %1% is not registered or you are not recognized as this nickname."), user->Name());
		MT_RET(false);
	}

	nick->User()->MSN(std::string());
	SEND(service, user, N_("Your MSN account has been unset."),
		 params[1]);

	MT_RET(true);
	MT_EE
}

static bool ns_Unset_Jabber(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("ns_Unset_Jabber" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);

	boost::shared_ptr<StoredNick> nick = user->Stored();
	if (!nick)
	{
		SEND(service, user,
			 N_("Nickname %1% is not registered or you are not recognized as this nickname."), user->Name());
		MT_RET(false);
	}

	nick->User()->Jabber(std::string());
	SEND(service, user, N_("Your Jabber ID has been unset."),
		 params[1]);

	MT_RET(true);
	MT_EE
}

static bool ns_Unset_Yahoo(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("ns_Unset_Yahoo" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);

	boost::shared_ptr<StoredNick> nick = user->Stored();
	if (!nick)
	{
		SEND(service, user,
			 N_("Nickname %1% is not registered or you are not recognized as this nickname."), user->Name());
		MT_RET(false);
	}

	nick->User()->Yahoo(std::string());
	SEND(service, user, N_("Your Yahoo! ID has been unset."),
		 params[1]);

	MT_RET(true);
	MT_EE
}

static bool ns_Unset_Description(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("ns_Unset_Description" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);

	boost::shared_ptr<StoredNick> nick = user->Stored();
	if (!nick)
	{
		SEND(service, user,
			 N_("Nickname %1% is not registered or you are not recognized as this nickname."), user->Name());
		MT_RET(false);
	}

	nick->User()->Description(std::string());
	SEND(service, user, N_("Your description has been unset."),
		 params[1]);

	MT_RET(true);
	MT_EE
}

static bool ns_Unset_Language(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("ns_Unset_Language" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);

	boost::shared_ptr<StoredNick> nick = user->Stored();
	if (!nick)
	{
		SEND(service, user,
			 N_("Nickname %1% is not registered or you are not recognized as this nickname."), user->Name());
		MT_RET(false);
	}

	if (nick->User()->Language(std::string()))
		SEND(service, user, N_("Your language has been unset."),
			 params[1]);
	else
		NSEND(service, user, N_("Your language setting is locked and cannot be changed."));

	MT_RET(true);
	MT_EE
}

static bool ns_Unset_Protect(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("ns_Unset_Protect" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);

	boost::shared_ptr<StoredNick> nick = user->Stored();
	if (!nick)
	{
		SEND(service, user,
			 N_("Nickname %1% is not registered or you are not recognized as this nickname."), user->Name());
		MT_RET(false);
	}

	if (nick->User()->Protect(boost::logic::indeterminate))
		SEND(service, user, N_("Protect has been reset to the default."),
			 params[1]);
	else
		NSEND(service, user, N_("Your protect setting is locked and cannot be changed."));

	MT_RET(true);
	MT_EE
}

static bool ns_Unset_Secure(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("ns_Unset_Secure" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);

	boost::shared_ptr<StoredNick> nick = user->Stored();
	if (!nick)
	{
		SEND(service, user,
			 N_("Nickname %1% is not registered or you are not recognized as this nickname."), user->Name());
		MT_RET(false);
	}

	if (nick->User()->Secure(boost::logic::indeterminate))
		SEND(service, user, N_("Secure has been reset to the default."),
			 params[1]);
	else
		NSEND(service, user, N_("Your secure setting is locked and cannot be changed."));

	MT_RET(true);
	MT_EE
}

static bool ns_Unset_Nomemo(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("ns_Unset_Nomemo" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);

	boost::shared_ptr<StoredNick> nick = user->Stored();
	if (!nick)
	{
		SEND(service, user,
			 N_("Nickname %1% is not registered or you are not recognized as this nickname."), user->Name());
		MT_RET(false);
	}

	if (nick->User()->NoMemo(boost::logic::indeterminate))
		SEND(service, user, N_("No Memo has been reset to the default."),
			 params[1]);
	else
		NSEND(service, user, N_("Your no memo setting is locked and cannot be changed."));

	MT_RET(true);
	MT_EE
}

static bool ns_Unset_Private(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("ns_Unset_Private" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);

	boost::shared_ptr<StoredNick> nick = user->Stored();
	if (!nick)
	{
		SEND(service, user,
			 N_("Nickname %1% is not registered or you are not recognized as this nickname."), user->Name());
		MT_RET(false);
	}

	if (nick->User()->Private(boost::logic::indeterminate))
		SEND(service, user, N_("Private has been reset to the default."),
			 params[1]);
	else
		NSEND(service, user, N_("Your private setting is locked and cannot be changed."));

	MT_RET(true);
	MT_EE
}

static bool ns_Unset_Privmsg(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("ns_Unset_Privmsg" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);

	boost::shared_ptr<StoredNick> nick = user->Stored();
	if (!nick)
	{
		SEND(service, user,
			 N_("Nickname %1% is not registered or you are not recognized as this nickname."), user->Name());
		MT_RET(false);
	}

	if (nick->User()->PRIVMSG(boost::logic::indeterminate))
		SEND(service, user, N_("Private Messaging has been reset to the default."),
			 params[1]);
	else
		NSEND(service, user, N_("Your private messaging setting is locked and cannot be changed."));

	MT_RET(true);
	MT_EE
}

static bool ns_Unset_Picture(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("ns_Unset_Picture" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);

	boost::shared_ptr<StoredNick> nick = user->Stored();
	if (!nick)
	{
		SEND(service, user,
			 N_("Nickname %1% is not registered or you are not recognized as this nickname."), user->Name());
		MT_RET(false);
	}

	nick->User()->ClearPicture();
	SEND(service, user, N_("Stored picture has been erased."),
		 params[1]);

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

	if (params.size() < 3)
	{
		SEND(service, user,
			 N_("Insufficient parameters for %1% command."),
			 boost::algorithm::to_upper_copy(params[0]));
		MT_RET(false);
	}

	boost::shared_ptr<StoredNick> nick = ROOT->data.Get_StoredNick(params[1]);
	if (!nick)
	{
		SEND(service, user,
			 N_("Nickname %1% is not registered."), params[1]);
		MT_RET(false);
	}

	std::string comment(params[2]);
	for (size_t i = 3; i < params.size(); ++i)
		comment += " " + params[i];
	nick->User()->Comment(comment);
	SEND(service, user, N_("Comment for user %1% has been set to \002%2%\017."),
		 nick->Name() % comment);

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

	if (params.size() < 3)
	{
		SEND(service, user,
			 N_("Insufficient parameters for %1% command."),
			 boost::algorithm::to_upper_copy(params[0]));
		MT_RET(false);
	}

	boost::shared_ptr<StoredNick> nick = ROOT->data.Get_StoredNick(params[1]);
	if (!nick)
	{
		SEND(service, user,
			 N_("Nickname %1% is not registered."), params[1]);
		MT_RET(false);
	}

	boost::logic::tribool v = mantra::get_bool(params[2]);
	if (boost::logic::indeterminate(v))
	{
		NSEND(service, user, N_("You may only specify ON or OFF."));
		MT_RET(false);
	}

	if (nick->User()->NoExpire(v))
		SEND(service, user, (v
			 ? N_("No expire for %1% has been \002enabled\017.")
			 : N_("No expire for %1% has been \002disabled\017.")),
			 nick->Name());
	else
		NSEND(service, user, N_("The no expire setting is locked and cannot be changed."));

	MT_RET(true);
	MT_EE
}

static bool ns_Unset_Comment(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("ns_Unset_Comment" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);

	if (params.size() < 2)
	{
		SEND(service, user,
			 N_("Insufficient parameters for %1% command."),
			 boost::algorithm::to_upper_copy(params[0]));
		MT_RET(false);
	}

	boost::shared_ptr<StoredNick> nick = ROOT->data.Get_StoredNick(params[1]);
	if (!nick)
	{
		SEND(service, user,
			 N_("Nickname %1% is not registered."), params[1]);
		MT_RET(false);
	}

	nick->User()->Comment(std::string());
	SEND(service, user, N_("The e-mail address for %1% has been unset."),
		 user->Name());

	MT_RET(true);
	MT_EE
}

static bool ns_Unset_Noexpire(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("ns_Unset_Noexpire" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);

	if (params.size() < 2)
	{
		SEND(service, user,
			 N_("Insufficient parameters for %1% command."),
			 boost::algorithm::to_upper_copy(params[0]));
		MT_RET(false);
	}

	boost::shared_ptr<StoredNick> nick = ROOT->data.Get_StoredNick(params[1]);
	if (!nick)
	{
		SEND(service, user,
			 N_("Nickname %1% is not registered."), params[1]);
		MT_RET(false);
	}

	if (nick->User()->NoExpire(boost::logic::indeterminate))
		SEND(service, user, N_("No expire for %1% has been reset to the default."),
			 nick->Name());
	else
		NSEND(service, user, N_("The no expire setting is locked and cannot be changed."));

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

	if (params.size() < 3)
	{
		SEND(service, user,
			 N_("Insufficient parameters for %1% command."),
			 boost::algorithm::to_upper_copy(params[0]));
		MT_RET(false);
	}

	boost::shared_ptr<StoredNick> nick = ROOT->data.Get_StoredNick(params[1]);
	if (!nick)
	{
		SEND(service, user,
			 N_("Nickname %1% is not registered."), params[1]);
		MT_RET(false);
	}

	try
	{
		std::locale(params[2]);
	}
	catch (std::exception &e)
	{
		NSEND(service, user, N_("Invalid language specified."));
		MT_RET(false);
	}

	if (!nick->User()->LOCK_Language(false))
		NSEND(service, user, N_("The language setting is globally locked and cannot be locked."));

	nick->User()->Language(params[2]);
	nick->User()->LOCK_Language(true);
	SEND(service, user, N_("Language for %1% has been locked at \002%1%\017."),
		 nick->Name() % params[2]);

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

	if (params.size() < 3)
	{
		SEND(service, user,
			 N_("Insufficient parameters for %1% command."),
			 boost::algorithm::to_upper_copy(params[0]));
		MT_RET(false);
	}

	boost::shared_ptr<StoredNick> nick = ROOT->data.Get_StoredNick(params[1]);
	if (!nick)
	{
		SEND(service, user,
			 N_("Nickname %1% is not registered."), params[1]);
		MT_RET(false);
	}

	boost::logic::tribool v = mantra::get_bool(params[2]);
	if (boost::logic::indeterminate(v))
	{
		NSEND(service, user, N_("You may only specify ON or OFF."));
		MT_RET(false);
	}

	if (!nick->User()->LOCK_Protect(false))
		NSEND(service, user, N_("The nick protect setting is globally locked and cannot be locked."));

	nick->User()->Protect(v);
	nick->User()->LOCK_Protect(true);
	SEND(service, user, (v
		  ? N_("Nick protect for %1% has been \002enabled\017 and locked.")
		  : N_("Nick protect for %1% has been \002disabled\017 and locked.")),
		  nick->Name());

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

	if (params.size() < 3)
	{
		SEND(service, user,
			 N_("Insufficient parameters for %1% command."),
			 boost::algorithm::to_upper_copy(params[0]));
		MT_RET(false);
	}

	boost::shared_ptr<StoredNick> nick = ROOT->data.Get_StoredNick(params[1]);
	if (!nick)
	{
		SEND(service, user,
			 N_("Nickname %1% is not registered."), params[1]);
		MT_RET(false);
	}

	boost::logic::tribool v = mantra::get_bool(params[2]);
	if (boost::logic::indeterminate(v))
	{
		NSEND(service, user, N_("You may only specify ON or OFF."));
		MT_RET(false);
	}

	if (!nick->User()->LOCK_Secure(false))
		NSEND(service, user, N_("The secure setting is globally locked and cannot be locked."));

	nick->User()->Secure(v);
	nick->User()->LOCK_Secure(true);
	SEND(service, user, (v
		  ? N_("Secure for %1% has been \002enabled\017 and locked.")
		  : N_("Secure for %1% has been \002disabled\017 and locked.")),
		  nick->Name());

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

	if (params.size() < 3)
	{
		SEND(service, user,
			 N_("Insufficient parameters for %1% command."),
			 boost::algorithm::to_upper_copy(params[0]));
		MT_RET(false);
	}

	boost::shared_ptr<StoredNick> nick = ROOT->data.Get_StoredNick(params[1]);
	if (!nick)
	{
		SEND(service, user,
			 N_("Nickname %1% is not registered."), params[1]);
		MT_RET(false);
	}

	boost::logic::tribool v = mantra::get_bool(params[2]);
	if (boost::logic::indeterminate(v))
	{
		NSEND(service, user, N_("You may only specify ON or OFF."));
		MT_RET(false);
	}

	if (!nick->User()->LOCK_NoMemo(false))
		NSEND(service, user, N_("The no memo setting is globally locked and cannot be locked."));

	nick->User()->NoMemo(v);
	nick->User()->LOCK_NoMemo(true);
	SEND(service, user, (v
		  ? N_("No memo for %1% has been \002enabled\017 and locked.")
		  : N_("No memo for %1% has been \002disabled\017 and locked.")),
		  nick->Name());

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

	if (params.size() < 3)
	{
		SEND(service, user,
			 N_("Insufficient parameters for %1% command."),
			 boost::algorithm::to_upper_copy(params[0]));
		MT_RET(false);
	}

	boost::shared_ptr<StoredNick> nick = ROOT->data.Get_StoredNick(params[1]);
	if (!nick)
	{
		SEND(service, user,
			 N_("Nickname %1% is not registered."), params[1]);
		MT_RET(false);
	}

	boost::logic::tribool v = mantra::get_bool(params[2]);
	if (boost::logic::indeterminate(v))
	{
		NSEND(service, user, N_("You may only specify ON or OFF."));
		MT_RET(false);
	}

	if (!nick->User()->LOCK_Private(false))
		NSEND(service, user, N_("The private setting is globally locked and cannot be locked."));

	nick->User()->Private(v);
	nick->User()->LOCK_Private(true);
	SEND(service, user, (v
		  ? N_("Private for %1% has been \002enabled\017 and locked.")
		  : N_("Private for %1% has been \002disabled\017 and locked.")),
		  nick->Name());

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

	if (params.size() < 3)
	{
		SEND(service, user,
			 N_("Insufficient parameters for %1% command."),
			 boost::algorithm::to_upper_copy(params[0]));
		MT_RET(false);
	}

	boost::shared_ptr<StoredNick> nick = ROOT->data.Get_StoredNick(params[1]);
	if (!nick)
	{
		SEND(service, user,
			 N_("Nickname %1% is not registered."), params[1]);
		MT_RET(false);
	}

	boost::logic::tribool v = mantra::get_bool(params[2]);
	if (boost::logic::indeterminate(v))
	{
		NSEND(service, user, N_("You may only specify ON or OFF."));
		MT_RET(false);
	}

	if (!nick->User()->LOCK_PRIVMSG(false))
		NSEND(service, user, N_("The private messaging setting is globally locked and cannot be locked."));

	nick->User()->PRIVMSG(v);
	nick->User()->LOCK_PRIVMSG(true);
	SEND(service, user, (v
		  ? N_("Private messaging for %1% has been \002enabled\017 and locked.")
		  : N_("Private messaging for %1% has been \002disabled\017 and locked.")),
		  nick->Name());

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

	if (params.size() < 2)
	{
		SEND(service, user,
			 N_("Insufficient parameters for %1% command."),
			 boost::algorithm::to_upper_copy(params[0]));
		MT_RET(false);
	}

	boost::shared_ptr<StoredNick> nick = ROOT->data.Get_StoredNick(params[1]);
	if (!nick)
	{
		SEND(service, user,
			 N_("Nickname %1% is not registered."), params[1]);
		MT_RET(false);
	}

	if (nick->User()->LOCK_Language(false))
		SEND(service, user, N_("Language for %1% has been unlocked."),
			 nick->Name());
	else
		NSEND(service, user, N_("The language setting is globally locked and unlocked."));

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

	if (params.size() < 2)
	{
		SEND(service, user,
			 N_("Insufficient parameters for %1% command."),
			 boost::algorithm::to_upper_copy(params[0]));
		MT_RET(false);
	}

	boost::shared_ptr<StoredNick> nick = ROOT->data.Get_StoredNick(params[1]);
	if (!nick)
	{
		SEND(service, user,
			 N_("Nickname %1% is not registered."), params[1]);
		MT_RET(false);
	}

	if (nick->User()->LOCK_Protect(false))
		SEND(service, user, N_("Nick protect for %1% has been unlocked."),
			 nick->Name());
	else
		NSEND(service, user, N_("The nick protect setting is globally locked and unlocked."));


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

	if (params.size() < 2)
	{
		SEND(service, user,
			 N_("Insufficient parameters for %1% command."),
			 boost::algorithm::to_upper_copy(params[0]));
		MT_RET(false);
	}

	boost::shared_ptr<StoredNick> nick = ROOT->data.Get_StoredNick(params[1]);
	if (!nick)
	{
		SEND(service, user,
			 N_("Nickname %1% is not registered."), params[1]);
		MT_RET(false);
	}

	if (nick->User()->LOCK_Secure(false))
		SEND(service, user, N_("Secure for %1% has been unlocked."),
			 nick->Name());
	else
		NSEND(service, user, N_("The secure setting is globally locked and unlocked."));

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

	if (params.size() < 2)
	{
		SEND(service, user,
			 N_("Insufficient parameters for %1% command."),
			 boost::algorithm::to_upper_copy(params[0]));
		MT_RET(false);
	}

	boost::shared_ptr<StoredNick> nick = ROOT->data.Get_StoredNick(params[1]);
	if (!nick)
	{
		SEND(service, user,
			 N_("Nickname %1% is not registered."), params[1]);
		MT_RET(false);
	}

	if (nick->User()->LOCK_NoMemo(false))
		SEND(service, user, N_("No memo for %1% has been unlocked."),
			 nick->Name());
	else
		NSEND(service, user, N_("The no memo setting is globally locked and unlocked."));

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

	if (params.size() < 2)
	{
		SEND(service, user,
			 N_("Insufficient parameters for %1% command."),
			 boost::algorithm::to_upper_copy(params[0]));
		MT_RET(false);
	}

	boost::shared_ptr<StoredNick> nick = ROOT->data.Get_StoredNick(params[1]);
	if (!nick)
	{
		SEND(service, user,
			 N_("Nickname %1% is not registered."), params[1]);
		MT_RET(false);
	}

	if (nick->User()->Private(false))
		SEND(service, user, N_("Private for %1% has been unlocked."),
			 nick->Name());
	else
		NSEND(service, user, N_("The private setting is globally locked and unlocked."));

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

	if (params.size() < 2)
	{
		SEND(service, user,
			 N_("Insufficient parameters for %1% command."),
			 boost::algorithm::to_upper_copy(params[0]));
		MT_RET(false);
	}

	boost::shared_ptr<StoredNick> nick = ROOT->data.Get_StoredNick(params[1]);
	if (!nick)
	{
		SEND(service, user,
			 N_("Nickname %1% is not registered."), params[1]);
		MT_RET(false);
	}

	if (nick->User()->PRIVMSG(false))
		SEND(service, user, N_("Private messaging for %1% has been unlocked."),
			 nick->Name());
	else
		NSEND(service, user, N_("The private messaging setting is globally locked and unlocked."));

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

	// TODO: To be implemented.
	SEND(service, user,
		 N_("The %1% command has not yet been implemented."),
		 boost::algorithm::to_upper_copy(params[0]));

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

	// TODO: To be implemented.
	SEND(service, user,
		 N_("The %1% command has not yet been implemented."),
		 boost::algorithm::to_upper_copy(params[0]));

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
	serv.PushCommand("^SETPASS(WORD)?$", &ns_Setpass, comm_sop);

	serv.PushCommand("^FORBID$",
					 Service::CommandMerge(serv, 0, 1), comm_sop);
	serv.PushCommand("^FORBID\\s+ADD$", &ns_Forbid_Add, comm_sop);
	serv.PushCommand("^FORBID\\s+(ERASE|DEL(ETE)?)$", &ns_Forbid_Del, comm_sop);
	serv.PushCommand("^FORBID\\s+(LIST|VIEW)$", &ns_Forbid_List, comm_sop);
	serv.PushCommand("^FORBID\\s+HELP$",
					 boost::bind(&Service::AuxHelp, &serv,
								 _1, _2, _3), comm_sop);

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
	serv.PushCommand("^SET\\s+(URL|WWW|WEB(PAGE|SITE)?|HTTPS?)$",
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
	serv.PushCommand("^SET\\s+PIC(TURE)?$",
					 &ns_Set_Picture);
	serv.PushCommand("^SET\\s+HELP$",
					 boost::bind(&Service::AuxHelp, &serv,
								 _1, _2, _3));

	serv.PushCommand("^(UN|RE)SET$",
					 Service::CommandMerge(serv, 0, 1));
	serv.PushCommand("^(UN|RE)SET\\s+E-?MAIL$",
					 &ns_Unset_Email);
	serv.PushCommand("^(UN|RE)SET\\s+(URL|WWW|WEB(PAGE|SITE)?|HTTPS?)$",
					 &ns_Unset_Website);
	serv.PushCommand("^(UN|RE)SET\\s+(UIN|ICQ)$",
					 &ns_Unset_Icq);
	serv.PushCommand("^(UN|RE)SET\\s+(AIM)$",
					 &ns_Unset_Aim);
	serv.PushCommand("^(UN|RE)SET\\s+MSN$",
					 &ns_Unset_Msn);
	serv.PushCommand("^(UN|RE)SET\\s+JABBER$",
					 &ns_Unset_Jabber);
	serv.PushCommand("^(UN|RE)SET\\s+YAHOO$",
					 &ns_Unset_Yahoo);
	serv.PushCommand("^(UN|RE)SET\\s+DESC(RIPT(ION)?)?$",
					 &ns_Unset_Description);
	serv.PushCommand("^(UN|RE)SET\\s+LANG(UAGE)?$",
					 &ns_Unset_Language);
	serv.PushCommand("^(UN|RE)SET\\s+PROT(ECT)?$",
					 &ns_Unset_Protect);
	serv.PushCommand("^(UN|RE)SET\\s+SECURE$",
					 &ns_Unset_Secure);
	serv.PushCommand("^(UN|RE)SET\\s+NOMEMO$",
					 &ns_Unset_Nomemo);
	serv.PushCommand("^(UN|RE)SET\\s+PRIV(ATE)?$",
					 &ns_Unset_Private);
	serv.PushCommand("^(UN|RE)SET\\s+((PRIV)?MSG)$",
					 &ns_Unset_Privmsg);
	serv.PushCommand("^(UN|RE)SET\\s+PIC(TURE)?$",
					 &ns_Unset_Picture);
	serv.PushCommand("^(UN|RE)SET\\s+HELP$",
					 boost::bind(&Service::AuxHelp, &serv,
								 _1, _2, _3));

	serv.PushCommand("^SET\\s+COMMENT$",
					 &ns_Set_Comment, comm_opersop);
	serv.PushCommand("^SET\\s+NO?EX(PIRE)?$",
					 &ns_Set_Noexpire, comm_sop);
	serv.PushCommand("^(UN|RE)SET\\s+COMMENT$",
					 &ns_Unset_Comment, comm_opersop);
	serv.PushCommand("^(UN|RE)SET\\s+NO?EX(PIRE)?$",
					 &ns_Unset_Noexpire, comm_sop);

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

