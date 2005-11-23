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
#include "serviceuser.h"
#include "storednick.h"
#include "storeduser.h"
#include "committee.h"

#include <mantra/core/trace.h>

static bool biREGISTER(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biREGISTER" << service << user << params);

	boost::shared_ptr<StoredNick> nick = ROOT->data.Get_StoredNick(user->Name());
	if (nick)
	{
		SEND(service, user,
			 N_("Nickname %1% is already registered."), user->Name());
		MT_RET(false);
	}

	if (ROOT->data.Matches_Forbidden(user->Name(), false))
	{
		SEND(service, user, N_("Nickname %1% is forbidden."), user->Name());
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
		static boost::regex rx("^[^[:space:][:cntrl:]@]+@"
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

	if (params.size() > 2)
	{
		nick->User()->Email(params[2]);
		if (params.size() > 3)
		{
			std::string desc(params[3]);
			for (size_t i = 4; i < params.size(); ++i)
				desc += ' ' + params[i];
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

static bool biDROP(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biDROP" << service << user << params);

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
									  mantra::Second) % token);
	}
	else
	{
		std::string token = user->DropToken();
		if (token.empty())
		{
			NSEND(service, user, N_("Drop token has expired (nickname not dropped)."));
			MT_RET(false);
		}

		if (params[1] != token)
		{
			NSEND(service, user, N_("Drop token incorrect (nickname not dropped)."));
			MT_RET(false);
		}

		user->DropToken(std::string());
		user->Stored()->Drop();
		NSEND(service, user, N_("Your nickname has been dropped."));
	}

	MT_RET(true);
	MT_EE
}

static bool biLINK(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biLINK" << service << user << params);

	boost::shared_ptr<StoredNick> nick = ROOT->data.Get_StoredNick(user->Name());
	if (nick)
	{
		SEND(service, user,
			 N_("Nickname %1% is already registered."), user->Name());
		MT_RET(false);
	}

	if (ROOT->data.Matches_Forbidden(user->Name()))
	{
		SEND(service, user, N_("Nickname %1% is forbidden."), user->Name());
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
	user->Identify(params[2]);
    SEND(service, user, N_("Nickname %1% has been linked to %2%."),
		 user->Name() % other);

	MT_RET(true);
	MT_EE
}

static bool biLINKS(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biIGNORE_LIST" << service << user << params);

	boost::shared_ptr<StoredNick> nick;
	if (params.size() < 2)
	{
		nick = user->Stored();
		if (!nick)
		{
			SEND(service, user,
				 N_("Nickname %1% is not registered or you are not recognized as this nickname."), user->Name());
			MT_RET(false);
		}
	}
	else
	{
		nick = ROOT->data.Get_StoredNick(params[1]);
		if (!nick)
		{
			SEND(service, user,
				 N_("Nickname %1% is not registered."), params[1]);
			MT_RET(false);
		}
	}

	StoredUser::my_nicks_t nicks = nick->User()->Nicks();
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

	if (params.size() < 2)
		SEND(service, user, N_("Your nicknames: %1%"), str);
	else
		SEND(service, user, N_("Linked nicknames for %1%: %2%"),
			 nick->Name() % str);

	MT_RET(true);
	MT_EE
}

static bool biIDENTIFY(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biIDENTIFY" << service << user << params);

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

static bool biINFO(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biINFO" << service << user << params);

	if (ROOT->data.Matches_Forbidden(params[1], false))
	{
		SEND(service, user, N_("Nickname %1% is forbidden."), params[1]);
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
	boost::posix_time::ptime next_update = ROOT->data.SaveTime();
	if (!next_update.is_special())
		SEND(service, user, N_("Next database save is in %1%."),
				 DurationToString(mantra::duration(next_update,
								  mantra::GetCurrentDateTime()), mantra::Second));

	MT_RET(true);
	MT_EE
}

static bool biGHOST(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biGHOST" << service << user << params);

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

	service->KILL(target,
		(boost::format(_("GHOST command used by %1%")) %
					   user->Name()).str());
	SEND(service, user, N_("User using nickname %1% has been killed."),
		 params[1]);

	MT_RET(true);
	MT_EE
}

static bool biRECOVER(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biRECOVER" << service << user << params);

	// TODO: To be implemented.
	SEND(service, user,
		 N_("The %1% command has not yet been implemented."),
		 boost::algorithm::to_upper_copy(params[0]));

	MT_RET(true);
	MT_EE
}

static bool biSEND(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biSEND" << service << user << params);

	// TODO: To be implemented.
	SEND(service, user,
		 N_("The %1% command has not yet been implemented."),
		 boost::algorithm::to_upper_copy(params[0]));

	MT_RET(true);
	MT_EE
}

static bool biSUSPEND(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biSUSPEND" << service << user << params);

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

static bool biUNSUSPEND(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biUNSUSPEND" << service << user << params);

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

	if (target->User()->Suspend_Reason().empty())
	{
		SEND(service, user,
			 N_("Nickname %1% is not suspended."), target->Name());
		MT_RET(false);
	}

	target->User()->Unsuspend();
	SEND(service, user, N_("Nickname %1% has been unsuspended."), target->Name());

	MT_RET(true);
	MT_EE
}

static bool biFORBID_ADD(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biFORBID_ADD" << service << user << params);

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

	std::string reason(params[2]);
	for (size_t i = 3; i < params.size(); ++i)
		reason += ' ' + params[i];

	Forbidden f = Forbidden::create(params[1], reason, nick);
	ROOT->data.Add(f);
	SEND(service, user,
		 N_("Nickname mask \002%1%\017 has been added to the forbidden list."),
		 params[1]);

	MT_RET(true);
	MT_EE
}

static bool biFORBID_DEL(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biFORBID_DEL" << service << user << params);

	std::string numbers(params[1]);
	for (size_t i=2; i<params.size(); ++i)
		numbers += ' ' + params[i];

	size_t skipped = 0;
	std::vector<unsigned int> v;
	if (!mantra::ParseNumbers(numbers, v))
	{
		std::vector<Forbidden> ent;
		ROOT->data.Get_Forbidden(ent, false);

		for (size_t i = 1; i < params.size(); ++i)
		{
			if (ROOT->proto.IsChannel(params[i]))
			{
				++skipped;
				continue;
			}

			bool found = false;
			std::vector<Forbidden>::iterator j;
			for (j = ent.begin(); j != ent.end(); ++j)
			{
				if (mantra::glob_match(params[i], j->Mask(), true))
				{
					v.push_back(j->Number());
					found = true;
				}
			}
			if (!found)
				++skipped;
		}

		if (v.empty())
		{
			SEND(service, user,
				 N_("No nicknames matching \002%1%\017 are forbidden."),
				 numbers);
			MT_RET(false);
		}
	}

	for (size_t i = 0; i < v.size(); ++i)
	{
		Forbidden f = ROOT->data.Get_Forbidden(v[i]);
		if (f.Number())
			ROOT->data.Del(f);
		else
			++skipped;
	}
	if (!skipped)
	{
		SEND(service, user,
			 N_("%1% entries removed from the forbidden nickname list."),
			 v.size());
	}
	else if (v.size() != skipped)
	{
		SEND(service, user,
			 N_("%1% entries removed from the forbidden nickname list and %2% entries specified could not be found."),
			 (v.size() - skipped) % skipped);
	}
	else
	{
		NSEND(service, user,
			  N_("No specified entries could be found on the forbidden nickname list."));
	}

	MT_RET(true);
	MT_EE
}

static bool biFORBID_LIST(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biFORBID_LIST" << service << user << params);

	std::vector<Forbidden> ent;
	ROOT->data.Get_Forbidden(ent, false);

	if (ent.empty())
		NSEND(service, user, N_("The forbidden nickname list is empty"));
	else
	{
		NSEND(service, user, N_("Forbidden Nicknames:"));
		std::vector<Forbidden>::const_iterator i;
		for (i = ent.begin(); i != ent.end(); ++i)
		{
			SEND(service, user, N_("%1$ 3d. %2$s [Added by %3$s, %4$s ago]"),
				 i->Number() % i->Mask() % i->Last_UpdaterName() % 
				 DurationToString(mantra::duration(i->Last_Update(),
								  mantra::GetCurrentDateTime()), mantra::Second));
			SEND(service, user, N_("     %1%"), i->Reason());
		}
	}

	MT_RET(true);
	MT_EE
}

static bool biSETPASS(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biSETPASS" << service << user << params);

	static mantra::iequal_to<std::string> cmp;
	if (params[2].size() < 5 || cmp(params[1], params[2]))
	{
		NSEND(service, user, N_("Password is not complex enough."));
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

	if (nick->User() == target->User())
	{
		NSEND(service, user,
			 N_("Use standard SET PASSWORD on your own nicknames."));
		MT_RET(false);
	}

	boost::shared_ptr<Committee> comm = ROOT->data.Get_Committee(
			ROOT->ConfigValue<std::string>("commserv.sop.name"));
	if (comm && comm->MEMBER_Exists(target->User()))
	{
		comm = ROOT->data.Get_Committee(
			ROOT->ConfigValue<std::string>("commserv.sadmin.name"));
		if (comm && !comm->MEMBER_Exists(nick->User()))
		{
			SEND(service, user, N_("Only a member of the %1% committee may set the password of a member of the %2% committee."),
				 ROOT->ConfigValue<std::string>("commserv.sadmin.name") %
				 ROOT->ConfigValue<std::string>("commserv.sop.name"));
			MT_RET(false);
		}
	}

	target->User()->Password(params[2]);
    SEND(service, user, N_("Password for nickname %1% has been set to \002%2%\017."),
		 target->Name() % params[2].substr(0, 32));
	service->ANNOUNCE(format(_("%1% has changed the password for %2%.")) %
									user->Name() % target->Name());

	MT_RET(true);
	MT_EE
}

static bool biACCESS_CURRENT(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biACCESS_CURRENT" << service << user << params);

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

static bool biACCESS_ADD(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biACCESS_ADD" << service << user << params);

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

static bool biACCESS_DEL(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biACCESS_DEL" << service << user << params);

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

	std::string numbers(params[1]);
	for (size_t i=2; i<params.size(); ++i)
		numbers += ' ' + params[i];

	size_t skipped = 0;
	std::vector<unsigned int> v;
	if (!mantra::ParseNumbers(numbers, v))
	{
		std::map<boost::uint32_t, std::pair<std::string, boost::posix_time::ptime> > ent;
		nick->User()->ACCESS_Get(ent);
		for (size_t i = 1; i < params.size(); ++i)
		{
			bool found = false;
			std::map<boost::uint32_t, std::pair<std::string, boost::posix_time::ptime> >::iterator j;
			for (j = ent.begin(); j != ent.end(); ++j)
			{
				if (mantra::glob_match(params[i], j->second.first, true))
				{
					v.push_back(j->first);
					found = true;
				}
			}
			if (!found)
				++skipped;
		}

		if (v.empty())
		{
			SEND(service, user,
				 N_("No entries matching \002%1%\017 could be found on your access list."),
				 numbers);
			MT_RET(false);
		}
	}

	for (size_t i = 0; i < v.size(); ++i)
	{
		if (!nick->User()->ACCESS_Exists(v[i]))
			++skipped;
		else
			nick->User()->ACCESS_Del(v[i]);
	}
	if (!skipped)
	{
		SEND(service, user,
			 N_("%1% entries removed from your access list."),
			 v.size());
	}
	else if (v.size() != skipped)
	{
		SEND(service, user,
			 N_("%1% entries removed from your access list and %2% entries specified could not be found."),
			 (v.size() - skipped) % skipped);
	}
	else
	{
		NSEND(service, user,
			  N_("No specified entries could be found on your access list."));
	}

	MT_RET(true);
	MT_EE
}

static bool biACCESS_LIST(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biACCESS_LIST" << service << user << params);

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
		SEND(service, user, N_("%1$ 3d. %2$s [Added %3$s ago]"),
			 i->first % i->second.first %
			 DurationToString(mantra::duration(i->second.second,
							  mantra::GetCurrentDateTime()), mantra::Second));
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

static bool biACCESS_REINDEX(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biACCESS_REINDEX" << service << user << params);

	boost::shared_ptr<StoredNick> nick = user->Stored();
	if (!nick)
	{
		SEND(service, user,
			 N_("Nickname %1% is not registered or you are not recognized as this nickname."), user->Name());
		MT_RET(false);
	}

	std::vector<boost::uint32_t> ent;
	nick->User()->ACCESS_Get(ent);
	if (ent.empty())
	{
		NSEND(service, user, N_("Your access list is empty."));
		MT_RET(true);
	}

	size_t count = 0, changed = 0;
	std::vector<boost::uint32_t>::iterator i;
	for (i = ent.begin(); i != ent.end(); ++i)
	{
		if (*i != ++count)
		{
			nick->User()->ACCESS_Reindex(*i, count);
			++changed;
		}
	}

	SEND(service, user, N_("Reindexing of your access list complete, \002%1%\017 entries changed."),
		 changed);

	MT_RET(true);
	MT_EE
}

static bool biIGNORE_ADD(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biIGNORE_ADD" << service << user << params);

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

	if (nick->User()->IGNORE_Exists(target->User()))
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

static bool biIGNORE_DEL(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biIGNORE_DEL" << service << user << params);

	boost::shared_ptr<StoredNick> nick = user->Stored();
	if (!nick)
	{
		SEND(service, user,
			 N_("Nickname %1% is not registered or you are not recognized as this nickname."), user->Name());
		MT_RET(false);
	}

	std::string numbers(params[1]);
	for (size_t i=2; i<params.size(); ++i)
		numbers += ' ' + params[i];

	size_t del = 0, skipped = 0;
	std::vector<unsigned int> v;
	if (!mantra::ParseNumbers(numbers, v))
	{
		for (size_t i = 1; i < params.size(); ++i)
		{
			boost::shared_ptr<StoredNick> target = ROOT->data.Get_StoredNick(params[i]);
			if (!target)
			{
				++skipped;
				continue;
			}

			if (!nick->User()->IGNORE_Exists(target->User()))
			{
				++skipped;
				continue;
			}

			nick->User()->IGNORE_Del(target->User());
			++del;
		}
	}
	else
	{
		for (size_t i = 0; i < v.size(); ++i)
		{
			if (!nick->User()->IGNORE_Exists(v[i]))
			{
				++skipped;
				continue;
			}
			
			nick->User()->IGNORE_Del(v[i]);
			++del;
		}
	}

	if (del)
	{
		if (skipped)
			SEND(service, user,
				 N_("%1% entries removed from your memo ignore list and %2% entries specified could not be found."),
				 del % skipped);
		else
			SEND(service, user,
				 N_("%1% entries removed from your memo ignore list."), del);
	}
	else
	{
		NSEND(service, user,
			  N_("No specified entries could be found on your memo ignore list."));
	}

	MT_RET(true);
	MT_EE
}

static bool biIGNORE_LIST(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biIGNORE_LIST" << service << user << params);

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
		SEND(service, user, N_("%1$ 3d. %2$s [Added %3$s ago]"),
			 i->first % str %
			 DurationToString(mantra::duration(i->second.second,
							  mantra::GetCurrentDateTime()), mantra::Second));
	}

	if (!first)
	{
		NSEND(service, user, N_("Your memo ignore list is empty."));
		MT_RET(false);
	}

	MT_RET(true);
	MT_EE
}

static bool biIGNORE_REINDEX(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biIGNORE_REINDEX" << service << user << params);

	boost::shared_ptr<StoredNick> nick = user->Stored();
	if (!nick)
	{
		SEND(service, user,
			 N_("Nickname %1% is not registered or you are not recognized as this nickname."), user->Name());
		MT_RET(false);
	}

	std::vector<boost::uint32_t> ent;
	nick->User()->IGNORE_Get(ent);
	if (ent.empty())
	{
		NSEND(service, user, N_("Your access list is empty."));
		MT_RET(true);
	}

	size_t count = 0, changed = 0;
	std::vector<boost::uint32_t>::iterator i;
	for (i = ent.begin(); i != ent.end(); ++i)
	{
		if (*i != ++count)
		{
			nick->User()->IGNORE_Reindex(*i, count);
			++changed;
		}
	}

	SEND(service, user, N_("Reindexing of your memo ignore list complete, \002%1%\017 entries changed."),
		 changed);

	MT_RET(true);
	MT_EE
}

static bool biSET_PASSWORD(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biSET_PASSWORD" << service << user << params);

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

static bool biSET_EMAIL(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biSET_EMAIL" << service << user << params);

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

static bool biSET_WEBSITE(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biSET_WEBSITE" << service << user << params);

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

static bool biSET_ICQ(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biSET_ICQ" << service << user << params);

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

static bool biSET_AIM(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biSET_AIM" << service << user << params);

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

static bool biSET_MSN(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biSET_MSN" << service << user << params);

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

static bool biSET_JABBER(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biSET_JABBER" << service << user << params);

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

static bool biSET_YAHOO(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biSET_YAHOO" << service << user << params);

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

static bool biSET_DESCRIPTION(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biSET_DESCRIPTION" << service << user << params);

	boost::shared_ptr<StoredNick> nick = user->Stored();
	if (!nick)
	{
		SEND(service, user,
			 N_("Nickname %1% is not registered or you are not recognized as this nickname."), user->Name());
		MT_RET(false);
	}

	std::string desc(params[1]);
	for (size_t i = 2; i < params.size(); ++i)
		desc += ' ' + params[i];
	nick->User()->Description(desc);
	SEND(service, user, N_("Your description has been set to \002%1%\017."),
		 desc);

	MT_RET(true);
	MT_EE
}

static bool biSET_LANGUAGE(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biSET_LANGUAGE" << service << user << params);

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

static bool biSET_PROTECT(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biSET_PROTECT" << service << user << params);

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

static bool biSET_SECURE(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biSET_SECURE" << service << user << params);

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

static bool biSET_NOMEMO(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biSET_NOMEMO" << service << user << params);

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

static bool biSET_PRIVATE(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biSET_PRIVATE" << service << user << params);

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

static bool biSET_PRIVMSG(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biSET_PRIVMSG" << service << user << params);

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

static bool biSET_PICTURE(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biSET_PICTURE" << service << user << params);

	// TODO: To be implemented.
	SEND(service, user,
		 N_("The %1% command has not yet been implemented."),
		 boost::algorithm::to_upper_copy(params[0]));

	MT_RET(true);
	MT_EE
}

static bool biUNSET_EMAIL(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biUNSET_EMAIL" << service << user << params);

	boost::shared_ptr<StoredNick> nick = user->Stored();
	if (!nick)
	{
		SEND(service, user,
			 N_("Nickname %1% is not registered or you are not recognized as this nickname."), user->Name());
		MT_RET(false);
	}

	nick->User()->Email(std::string());
	NSEND(service, user, N_("Your e-mail address has been unset."));

	MT_RET(true);
	MT_EE
}

static bool biUNSET_WEBSITE(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biUNSET_WEBSITE" << service << user << params);

	boost::shared_ptr<StoredNick> nick = user->Stored();
	if (!nick)
	{
		SEND(service, user,
			 N_("Nickname %1% is not registered or you are not recognized as this nickname."), user->Name());
		MT_RET(false);
	}

	nick->User()->Website(std::string());
	NSEND(service, user, N_("Your website address has been unset."));

	MT_RET(true);
	MT_EE
}

static bool biUNSET_ICQ(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biUNSET_ICQ" << service << user << params);

	boost::shared_ptr<StoredNick> nick = user->Stored();
	if (!nick)
	{
		SEND(service, user,
			 N_("Nickname %1% is not registered or you are not recognized as this nickname."), user->Name());
		MT_RET(false);
	}

	nick->User()->ICQ(0);
	NSEND(service, user, N_("Your ICQ UIN has been unset."));

	MT_RET(true);
	MT_EE
}

static bool biUNSET_AIM(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biUNSET_AIM" << service << user << params);

	boost::shared_ptr<StoredNick> nick = user->Stored();
	if (!nick)
	{
		SEND(service, user,
			 N_("Nickname %1% is not registered or you are not recognized as this nickname."), user->Name());
		MT_RET(false);
	}

	nick->User()->AIM(std::string());
	NSEND(service, user, N_("Your AIM screen name has been unset."));

	MT_RET(true);
	MT_EE
}

static bool biUNSET_MSN(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biUNSET_MSN" << service << user << params);

	boost::shared_ptr<StoredNick> nick = user->Stored();
	if (!nick)
	{
		SEND(service, user,
			 N_("Nickname %1% is not registered or you are not recognized as this nickname."), user->Name());
		MT_RET(false);
	}

	nick->User()->MSN(std::string());
	NSEND(service, user, N_("Your MSN account has been unset."));

	MT_RET(true);
	MT_EE
}

static bool biUNSET_JABBER(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biUNSET_JABBER" << service << user << params);

	boost::shared_ptr<StoredNick> nick = user->Stored();
	if (!nick)
	{
		SEND(service, user,
			 N_("Nickname %1% is not registered or you are not recognized as this nickname."), user->Name());
		MT_RET(false);
	}

	nick->User()->Jabber(std::string());
	NSEND(service, user, N_("Your Jabber ID has been unset."));

	MT_RET(true);
	MT_EE
}

static bool biUNSET_YAHOO(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biUNSET_YAHOO" << service << user << params);

	boost::shared_ptr<StoredNick> nick = user->Stored();
	if (!nick)
	{
		SEND(service, user,
			 N_("Nickname %1% is not registered or you are not recognized as this nickname."), user->Name());
		MT_RET(false);
	}

	nick->User()->Yahoo(std::string());
	NSEND(service, user, N_("Your Yahoo! ID has been unset."));

	MT_RET(true);
	MT_EE
}

static bool biUNSET_DESCRIPTION(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biUNSET_DESCRIPTION" << service << user << params);

	boost::shared_ptr<StoredNick> nick = user->Stored();
	if (!nick)
	{
		SEND(service, user,
			 N_("Nickname %1% is not registered or you are not recognized as this nickname."), user->Name());
		MT_RET(false);
	}

	nick->User()->Description(std::string());
	NSEND(service, user, N_("Your description has been unset."));

	MT_RET(true);
	MT_EE
}

static bool biUNSET_LANGUAGE(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biUNSET_LANGUAGE" << service << user << params);

	boost::shared_ptr<StoredNick> nick = user->Stored();
	if (!nick)
	{
		SEND(service, user,
			 N_("Nickname %1% is not registered or you are not recognized as this nickname."), user->Name());
		MT_RET(false);
	}

	if (nick->User()->Language(std::string()))
		NSEND(service, user, N_("Your language has been unset."));
	else
		NSEND(service, user, N_("Your language setting is locked and cannot be changed."));

	MT_RET(true);
	MT_EE
}

static bool biUNSET_PROTECT(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biUNSET_PROTECT" << service << user << params);

	boost::shared_ptr<StoredNick> nick = user->Stored();
	if (!nick)
	{
		SEND(service, user,
			 N_("Nickname %1% is not registered or you are not recognized as this nickname."), user->Name());
		MT_RET(false);
	}

	if (nick->User()->Protect(boost::logic::indeterminate))
		NSEND(service, user, N_("Protect has been reset to the default."));
	else
		NSEND(service, user, N_("Your protect setting is locked and cannot be changed."));

	MT_RET(true);
	MT_EE
}

static bool biUNSET_SECURE(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biUNSET_SECURE" << service << user << params);

	boost::shared_ptr<StoredNick> nick = user->Stored();
	if (!nick)
	{
		SEND(service, user,
			 N_("Nickname %1% is not registered or you are not recognized as this nickname."), user->Name());
		MT_RET(false);
	}

	if (nick->User()->Secure(boost::logic::indeterminate))
		NSEND(service, user, N_("Secure has been reset to the default."));
	else
		NSEND(service, user, N_("Your secure setting is locked and cannot be changed."));

	MT_RET(true);
	MT_EE
}

static bool biUNSET_NOMEMO(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biUNSET_NOMEMO" << service << user << params);

	boost::shared_ptr<StoredNick> nick = user->Stored();
	if (!nick)
	{
		SEND(service, user,
			 N_("Nickname %1% is not registered or you are not recognized as this nickname."), user->Name());
		MT_RET(false);
	}

	if (nick->User()->NoMemo(boost::logic::indeterminate))
		NSEND(service, user, N_("No Memo has been reset to the default."));
	else
		NSEND(service, user, N_("Your no memo setting is locked and cannot be changed."));

	MT_RET(true);
	MT_EE
}

static bool biUNSET_PRIVATE(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biUNSET_PRIVATE" << service << user << params);

	boost::shared_ptr<StoredNick> nick = user->Stored();
	if (!nick)
	{
		SEND(service, user,
			 N_("Nickname %1% is not registered or you are not recognized as this nickname."), user->Name());
		MT_RET(false);
	}

	if (nick->User()->Private(boost::logic::indeterminate))
		NSEND(service, user, N_("Private has been reset to the default."));
	else
		NSEND(service, user, N_("Your private setting is locked and cannot be changed."));

	MT_RET(true);
	MT_EE
}

static bool biUNSET_PRIVMSG(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biUNSET_PRIVMSG" << service << user << params);

	boost::shared_ptr<StoredNick> nick = user->Stored();
	if (!nick)
	{
		SEND(service, user,
			 N_("Nickname %1% is not registered or you are not recognized as this nickname."), user->Name());
		MT_RET(false);
	}

	if (nick->User()->PRIVMSG(boost::logic::indeterminate))
		NSEND(service, user, N_("Private Messaging has been reset to the default."));
	else
		NSEND(service, user, N_("Your private messaging setting is locked and cannot be changed."));

	MT_RET(true);
	MT_EE
}

static bool biUNSET_PICTURE(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biUNSET_PICTURE" << service << user << params);

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

static bool biSET_COMMENT(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biSET_COMMENT" << service << user << params);

	boost::shared_ptr<StoredNick> nick = ROOT->data.Get_StoredNick(params[1]);
	if (!nick)
	{
		SEND(service, user,
			 N_("Nickname %1% is not registered."), params[1]);
		MT_RET(false);
	}

	std::string comment(params[2]);
	for (size_t i = 3; i < params.size(); ++i)
		comment += ' ' + params[i];
	nick->User()->Comment(comment);
	SEND(service, user, N_("Comment for user %1% has been set to \002%2%\017."),
		 nick->Name() % comment);

	MT_RET(true);
	MT_EE
}

static bool biSET_NOEXPIRE(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biSET_NOEXPIRE" << service << user << params);

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

static bool biUNSET_COMMENT(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biUNSET_COMMENT" << service << user << params);

	boost::shared_ptr<StoredNick> nick = ROOT->data.Get_StoredNick(params[1]);
	if (!nick)
	{
		SEND(service, user,
			 N_("Nickname %1% is not registered."), params[1]);
		MT_RET(false);
	}

	nick->User()->Comment(std::string());
	SEND(service, user, N_("The comment address for %1% has been unset."),
		 user->Name());

	MT_RET(true);
	MT_EE
}

static bool biUNSET_NOEXPIRE(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biUNSET_NOEXPIRE" << service << user << params);

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

static bool biLOCK_LANGUAGE(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biLOCK_LANGUAGE" << service << user << params);

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

static bool biLOCK_PROTECT(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biLOCK_PROTECT" << service << user << params);

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

static bool biLOCK_SECURE(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biLOCK_SECURE" << service << user << params);

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

static bool biLOCK_NOMEMO(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biLOCK_NOMEMO" << service << user << params);

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

static bool biLOCK_PRIVATE(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biLOCK_PRIVATE" << service << user << params);

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

static bool biLOCK_PRIVMSG(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biLOCK_PRIVMSG" << service << user << params);

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

static bool biUNLOCK_LANGUAGE(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biUNLOCK_LANGUAGE" << service << user << params);

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

static bool biUNLOCK_PROTECT(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biUNLOCK_PROTECT" << service << user << params);

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

static bool biUNLOCK_SECURE(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biUNLOCK_SECURE" << service << user << params);

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

static bool biUNLOCK_NOMEMO(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biUNLOCK_NOMEMO" << service << user << params);

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

static bool biUNLOCK_PRIVATE(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biUNLOCK_PRIVATE" << service << user << params);

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

static bool biUNLOCK_PRIVMSG(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biUNLOCK_PRIVMSG" << service << user << params);

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

static bool biSTOREDLIST(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biSTOREDLIST" << service << user << params);

	// TODO: To be implemented.
	SEND(service, user,
		 N_("The %1% command has not yet been implemented."),
		 boost::algorithm::to_upper_copy(params[0]));

	MT_RET(true);
	MT_EE
}

static bool biLIVELIST(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biLIVELIST" << service << user << params);

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
	MT_FUNC("init_nickserv_functions" << serv);

	std::vector<std::string> comm_regd, comm_oper, comm_sop, comm_opersop;
	comm_regd.push_back(ROOT->ConfigValue<std::string>("commserv.regd.name"));
	comm_oper.push_back(ROOT->ConfigValue<std::string>("commserv.oper.name"));
	comm_sop.push_back(ROOT->ConfigValue<std::string>("commserv.sop.name"));
	comm_opersop.push_back(ROOT->ConfigValue<std::string>("commserv.oper.name"));
	comm_opersop.push_back(ROOT->ConfigValue<std::string>("commserv.sop.name"));

	serv.PushCommand("^HELP$", boost::bind(&Service::Help, &serv,
										   _1, _2, _3));

	serv.PushCommand("^REGISTER$", &biREGISTER, 1);
	serv.PushCommand("^DROP$", &biDROP, 0, comm_regd);
	serv.PushCommand("^LINK$", &biLINK, 2);
	serv.PushCommand("^LINK(S|LIST)$", &biLINKS);
	serv.PushCommand("^ID(ENT(IFY)?)?$", &biIDENTIFY, 1);
	serv.PushCommand("^INFO$", &biINFO, 1);
	serv.PushCommand("^GHOST$", &biGHOST, 2);
	serv.PushCommand("^REC(OVER)?$", &biRECOVER, 2);
	serv.PushCommand("^SEND$", &biSEND, 1);
	serv.PushCommand("^SUSPEND$", &biSUSPEND, 2, comm_sop);
	serv.PushCommand("^UN?SUSPEND$", &biUNSUSPEND, 1, comm_sop);
	serv.PushCommand("^SETPASS(WORD)?$", &biSETPASS, 2, comm_sop);

	serv.PushCommand("^FORBID$",
					 Service::CommandMerge(serv, 0, 1), 1, comm_sop);
	serv.PushCommand("^FORBID\\s+(ADD|NEW|CREATE)$", &biFORBID_ADD, 2, comm_sop);
	serv.PushCommand("^FORBID\\s+(ERASE|DEL(ETE)?|REM(OVE)?)$", &biFORBID_DEL, 1, comm_sop);
	serv.PushCommand("^FORBID\\s+(LIST|VIEW)$", &biFORBID_LIST, 1, comm_sop);
	serv.PushCommand("^FORBID\\s+HELP$",
					 boost::bind(&Service::AuxHelp, &serv,
								 _1, _2, _3), 0, comm_sop);

	serv.PushCommand("^ACC(ESS)?$",
					 Service::CommandMerge(serv, 0, 1), 1, comm_regd);
	serv.PushCommand("^ACC(ESS)?\\s+CUR(R(ENT)?)?$",
					 &biACCESS_CURRENT, 0, comm_regd);
	serv.PushCommand("^ACC(ESS)?\\s+(ADD|NEW|CREATE)$",
					 &biACCESS_ADD, 1, comm_regd);
	serv.PushCommand("^ACC(ESS)?\\s+(ERASE|DEL(ETE)?|REM(OVE)?)$",
					 &biACCESS_DEL, 1, comm_regd);
	serv.PushCommand("^ACC(ESS)?\\s+(LIST|VIEW)$",
					 &biACCESS_LIST, 0, comm_regd);
	serv.PushCommand("^ACC(ESS)?\\s+RE(NUMBER|INDEX)$",
					 &biACCESS_REINDEX, 0, comm_regd);
	serv.PushCommand("^ACC(ESS)?\\s+HELP$",
					 boost::bind(&Service::AuxHelp, &serv,
								 _1, _2, _3), 0, comm_regd);

	serv.PushCommand("^IGNORE$",
					 Service::CommandMerge(serv, 0, 1), 1, comm_regd);
	serv.PushCommand("^IGNORE\\s+(ADD|NEW|CREATE)$",
					 &biIGNORE_ADD, 1, comm_regd);
	serv.PushCommand("^IGNORE\\s+(ERASE|DEL(ETE)?|REM(OVE)?)$",
					 &biIGNORE_DEL, 1, comm_regd);
	serv.PushCommand("^IGNORE\\s+(LIST|VIEW)$",
					 &biIGNORE_LIST, 0, comm_regd);
	serv.PushCommand("^IGNORE\\s+RE(NUMBER|INDEX)$",
					 &biIGNORE_REINDEX, 0, comm_regd);
	serv.PushCommand("^IGNORE\\s+HELP$",
					 boost::bind(&Service::AuxHelp, &serv,
								 _1, _2, _3), 0, comm_regd);

	serv.PushCommand("^SET$",
					 Service::CommandMerge(serv, 0, 1), 1, comm_regd);
	serv.PushCommand("^SET\\s+PASS(W(OR)?D)?$",
					 &biSET_PASSWORD, 1, comm_regd);
	serv.PushCommand("^SET\\s+E-?MAIL$",
					 &biSET_EMAIL, 1, comm_regd);
	serv.PushCommand("^SET\\s+(URL|WWW|WEB(PAGE|SITE)?|HTTPS?)$",
					 &biSET_WEBSITE, 1, comm_regd);
	serv.PushCommand("^SET\\s+(UIN|ICQ)$",
					 &biSET_ICQ, 1, comm_regd);
	serv.PushCommand("^SET\\s+(AIM)$",
					 &biSET_AIM, 1, comm_regd);
	serv.PushCommand("^SET\\s+MSN$",
					 &biSET_MSN, 1, comm_regd);
	serv.PushCommand("^SET\\s+JABBER$",
					 &biSET_JABBER, 1, comm_regd);
	serv.PushCommand("^SET\\s+YAHOO$",
					 &biSET_YAHOO, 1, comm_regd);
	serv.PushCommand("^SET\\s+DESC(RIPT(ION)?)?$",
					 &biSET_DESCRIPTION, 1, comm_regd);
	serv.PushCommand("^SET\\s+LANG(UAGE)?$",
					 &biSET_LANGUAGE, 1, comm_regd);
	serv.PushCommand("^SET\\s+PROT(ECT)?$",
					 &biSET_PROTECT, 1, comm_regd);
	serv.PushCommand("^SET\\s+SECURE$",
					 &biSET_SECURE, 1, comm_regd);
	serv.PushCommand("^SET\\s+NOMEMOS?$",
					 &biSET_NOMEMO, 1, comm_regd);
	serv.PushCommand("^SET\\s+PRIV(ATE)?$",
					 &biSET_PRIVATE, 1, comm_regd);
	serv.PushCommand("^SET\\s+((PRIV)?MSG)$",
					 &biSET_PRIVMSG, 1, comm_regd);
	serv.PushCommand("^SET\\s+PIC(TURE)?$",
					 &biSET_PICTURE, 1, comm_regd);
	serv.PushCommand("^SET\\s+HELP$",
					 boost::bind(&Service::AuxHelp, &serv,
								 _1, _2, _3), 0, comm_regd);

	serv.PushCommand("^(UN|RE)SET$",
					 Service::CommandMerge(serv, 0, 1), 1, comm_regd);
	serv.PushCommand("^(UN|RE)SET\\s+E-?MAIL$",
					 &biUNSET_EMAIL, 0, comm_regd);
	serv.PushCommand("^(UN|RE)SET\\s+(URL|WWW|WEB(PAGE|SITE)?|HTTPS?)$",
					 &biUNSET_WEBSITE, 0, comm_regd);
	serv.PushCommand("^(UN|RE)SET\\s+(UIN|ICQ)$",
					 &biUNSET_ICQ, 0, comm_regd);
	serv.PushCommand("^(UN|RE)SET\\s+(AIM)$",
					 &biUNSET_AIM, 0, comm_regd);
	serv.PushCommand("^(UN|RE)SET\\s+MSN$",
					 &biUNSET_MSN, 0, comm_regd);
	serv.PushCommand("^(UN|RE)SET\\s+JABBER$",
					 &biUNSET_JABBER, 0, comm_regd);
	serv.PushCommand("^(UN|RE)SET\\s+YAHOO$",
					 &biUNSET_YAHOO, 0, comm_regd);
	serv.PushCommand("^(UN|RE)SET\\s+DESC(RIPT(ION)?)?$",
					 &biUNSET_DESCRIPTION, 0, comm_regd);
	serv.PushCommand("^(UN|RE)SET\\s+LANG(UAGE)?$",
					 &biUNSET_LANGUAGE, 0, comm_regd);
	serv.PushCommand("^(UN|RE)SET\\s+PROT(ECT)?$",
					 &biUNSET_PROTECT, 0, comm_regd);
	serv.PushCommand("^(UN|RE)SET\\s+SECURE$",
					 &biUNSET_SECURE, 0, comm_regd);
	serv.PushCommand("^(UN|RE)SET\\s+NOMEMOS?$",
					 &biUNSET_NOMEMO, 0, comm_regd);
	serv.PushCommand("^(UN|RE)SET\\s+PRIV(ATE)?$",
					 &biUNSET_PRIVATE, 0, comm_regd);
	serv.PushCommand("^(UN|RE)SET\\s+((PRIV)?MSG)$",
					 &biUNSET_PRIVMSG, 0, comm_regd);
	serv.PushCommand("^(UN|RE)SET\\s+PIC(TURE)?$",
					 &biUNSET_PICTURE, 0, comm_regd);
	serv.PushCommand("^(UN|RE)SET\\s+HELP$",
					 boost::bind(&Service::AuxHelp, &serv,
								 _1, _2, _3), 0, comm_regd);

	serv.PushCommand("^SET\\s+COMMENT$",
					 &biSET_COMMENT, 2, comm_opersop);
	serv.PushCommand("^SET\\s+NO?EX(PIRE)?$",
					 &biSET_NOEXPIRE, 2, comm_sop);
	serv.PushCommand("^(UN|RE)SET\\s+COMMENT$",
					 &biUNSET_COMMENT, 1, comm_opersop);
	serv.PushCommand("^(UN|RE)SET\\s+NO?EX(PIRE)?$",
					 &biUNSET_NOEXPIRE, 1, comm_sop);

	serv.PushCommand("^LOCK$",
					 Service::CommandMerge(serv, 0, 1), 1, comm_sop);
	serv.PushCommand("^LOCK\\s+LANG(UAGE)?$",
					 &biLOCK_LANGUAGE, 2, comm_sop);
	serv.PushCommand("^LOCK\\s+PROT(ECT)?$",
					 &biLOCK_PROTECT, 2, comm_sop);
	serv.PushCommand("^LOCK\\s+SECURE$",
					 &biLOCK_SECURE, 2, comm_sop);
	serv.PushCommand("^LOCK\\s+NOMEMOS?$",
					 &biLOCK_NOMEMO, 2, comm_sop);
	serv.PushCommand("^LOCK\\s+PRIV(ATE)?$",
					 &biLOCK_PRIVATE, 2, comm_sop);
	serv.PushCommand("^LOCK\\s+((PRIV)?MSG)$",
					 &biLOCK_PRIVMSG, 2, comm_sop);
	serv.PushCommand("^LOCK\\s+HELP$",
					 boost::bind(&Service::AuxHelp, &serv,
								 _1, _2, _3), 0, comm_sop);

	serv.PushCommand("^UN?LOCK$",
					 Service::CommandMerge(serv, 0, 1), 1, comm_sop);
	serv.PushCommand("^UN?LOCK\\s+LANG(UAGE)?$",
					 &biUNLOCK_LANGUAGE, 1, comm_sop);
	serv.PushCommand("^UN?LOCK\\s+PROT(ECT)?$",
					 &biUNLOCK_PROTECT, 1, comm_sop);
	serv.PushCommand("^UN?LOCK\\s+SECURE$",
					 &biUNLOCK_SECURE, 1, comm_sop);
	serv.PushCommand("^UN?LOCK\\s+NOMEMOS?$",
					 &biUNLOCK_NOMEMO, 1, comm_sop);
	serv.PushCommand("^UN?LOCK\\s+PRIV(ATE)?$",
					 &biUNLOCK_PRIVATE, 1, comm_sop);
	serv.PushCommand("^UN?LOCK\\s+((PRIV)?MSG)$",
					 &biUNLOCK_PRIVMSG, 1, comm_sop);
	serv.PushCommand("^UN?LOCK\\s+HELP$",
					 boost::bind(&Service::AuxHelp, &serv,
								 _1, _2, _3), 0, comm_sop);

	// These commands don't operate on any nickname.
	serv.PushCommand("^(STORED)?LIST$", &biSTOREDLIST);
	serv.PushCommand("^LIVE(LIST)?$", &biLIVELIST, 0, comm_oper);

	MT_EE
}

