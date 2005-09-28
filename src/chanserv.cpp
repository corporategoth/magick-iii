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
#define RCSID(x,y) const char *rcsid_magick__chanserv_cpp_ ## x () { return y; }
RCSID(magick__chanserv_cpp, "@(#)$Id$");

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
#include "livechannel.h"
#include "storednick.h"
#include "storeduser.h"
#include "storedchannel.h"
#include "committee.h"

#include <mantra/core/trace.h>

static bool biREGISTER(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biREGISTER" << service << user << params);

	boost::shared_ptr<StoredNick> nick = ROOT->data.Get_StoredNick(user->Name());
	if (!nick)
	{
		NSEND(service, user, N_("Your nickname is not registered."));
		MT_RET(false);
	}

	boost::shared_ptr<StoredChannel> channel = ROOT->data.Get_StoredChannel(params[1]);
	if (channel)
	{
		SEND(service, user, N_("Channel %1% is already registered."), params[1]);
		MT_RET(false);
	}

	if (ROOT->data.Matches_Forbidden(params[1], true))
	{
		SEND(service, user, N_("Channel %1% is forbidden."), params[1]);
		MT_RET(false);
	}

	if (!user->InChannel(params[1]))
	{
		SEND(service, user, N_("You are not in channel %1%."), params[1]);
		MT_RET(false);
	}

	boost::shared_ptr<LiveChannel> live = ROOT->data.Get_LiveChannel(params[1]);
	if (!live)
	{
		// This should never really happen due to the above ...
		SEND(service, user, N_("Channel %1% does not exist."), params[1]);
		MT_RET(false);
	}

	if (!live->User(user, 'o'))
	{
		SEND(service, user, N_("You are not opped in channel %1%."), live->Name());
		MT_RET(false);
	}

	static mantra::iequal_to<std::string> cmp;
	if (params[2].size() < 5 || cmp(params[2], user->Name()) ||
		cmp(params[1], params[2]))
	{
		NSEND(service, user, N_("Password is not complex enough."));
		MT_RET(false);
	}

	std::string desc(params[3]);
	for (size_t i = 4; i < params.size(); ++i)
		desc += ' ' + params[i];

	channel = StoredChannel::create(params[1], params[2], nick->User());
	channel->Description(desc);
	user->Identify(channel, params[2]);

	SEND(service, user, N_("Channel %1% has been registered with password \002%2%\017."),
		 params[1] % params[2].substr(0, 32));

	MT_RET(true);
	MT_EE
}

static bool biDROP(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biDROP" << service << user << params);

	boost::shared_ptr<StoredChannel> channel = ROOT->data.Get_StoredChannel(params[1]);
	if (!channel)
	{
		SEND(service, user, N_("Channel %1% is not registered."), params[1]);
		MT_RET(false);
	}

	if (!user->Identified(channel))
	{
		SEND(service, user, N_("You must be identified to channel %1% to use the %2% command."),
			 channel->Name() % boost::algorithm::to_upper_copy(params[0]));
		MT_RET(false);
	}

	if (params.size() < 3)
	{
		std::string token;
		for (size_t i = 0; i < ROOT->ConfigValue<unsigned int>("chanserv.drop-length"); ++i)
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
		user->DropToken(channel, token);
		SEND(service, user, N_("Please re-issue your %1% command within %2% with the following parameter: %3%"),
			 boost::algorithm::to_upper_copy(params[0]) %
			 mantra::DurationToString(ROOT->ConfigValue<mantra::duration>("chanserv.drop"),
									  mantra::Second) % token);
	}
	else
	{
		std::string token = user->DropToken(channel);
		if (token.empty())
		{
			NSEND(service, user, N_("Drop token has expired (channel not dropped)."));
			MT_RET(false);
		}

		if (params[2] != token)
		{
			NSEND(service, user, N_("Drop token incorrect (channel not dropped)."));
			MT_RET(false);
		}

		user->DropToken(channel, std::string());
		channel->Drop();
		SEND(service, user, N_("Channel %1% has been dropped."), channel->Name());
	}

	MT_RET(true);
	MT_EE
}

static bool biIDENTIFY(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biIDENTIFY" << service << user << params);

	boost::shared_ptr<StoredChannel> channel = ROOT->data.Get_StoredChannel(params[1]);
	if (!channel)
	{
		SEND(service, user, N_("Channel %1% is not registered."), params[1]);
		MT_RET(false);
	}

	if (user->Identified(channel))
	{
		SEND(service, user, N_("You are already identified to channel %1%."),
			 channel->Name());
		MT_RET(false);
	}

	if (!user->Identify(channel, params[2]))
	{
		NSEND(service, user,
			  N_("Password incorrect, your attempt has been logged."));
		MT_RET(false);
	}

	SEND(service, user, N_("Password correct, you are now identified to channel %1%."),
		  channel->Name());

	MT_RET(true);
	MT_EE
}

static bool biINFO(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biINFO" << service << user << params);

	if (ROOT->data.Matches_Forbidden(params[1], true))
	{
		SEND(service, user, N_("Channel %1% is forbidden."), params[1]);
		MT_RET(false);
	}

	boost::shared_ptr<StoredChannel> channel = ROOT->data.Get_StoredChannel(params[1]);
	if (!channel)
	{
		SEND(service, user, N_("Channel %1% is not registered."), params[1]);
		MT_RET(false);
	}

	channel->SendInfo(service, user);

	MT_RET(true);
	MT_EE
}

static bool biSUSPEND(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biSUSPEND" << service << user << params);

	boost::shared_ptr<StoredNick> nick = user->Stored();
	if (!nick)
	{
		SEND(service, user,
			 N_("Nickname %1% is not registered or you are not recognized as this nickname."), user->Name());
		MT_RET(false);
	}

	boost::shared_ptr<StoredChannel> channel = ROOT->data.Get_StoredChannel(params[1]);
	if (!channel)
	{
		SEND(service, user, N_("Channel %1% is not registered."), params[1]);
		MT_RET(false);
	}

	std::string reason(params[2]);
	for (size_t i = 3; i < params.size(); ++i)
		reason += ' ' + reason;

	channel->Suspend(nick, reason);
	SEND(service, user, N_("Channel %1% has been suspended."), channel->Name());

	MT_RET(true);
	MT_EE
}

static bool biUNSUSPEND(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biUNSUSPEND" << service << user << params);

	boost::shared_ptr<StoredChannel> channel = ROOT->data.Get_StoredChannel(params[1]);
	if (!channel)
	{
		SEND(service, user, N_("Channel %1% is not registered."), params[1]);
		MT_RET(false);
	}

	if (channel->Suspend_Reason().empty())
	{
		SEND(service, user, N_("Channel %1% is not suspended."), channel->Name());
		MT_RET(false);
	}

	channel->Unsuspend();
	SEND(service, user, N_("Channel %1% has been unsuspended."), channel->Name());

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

	boost::shared_ptr<StoredChannel> channel = ROOT->data.Get_StoredChannel(params[1]);
	if (!channel)
	{
		SEND(service, user, N_("Channel %1% is not registered."), params[1]);
		MT_RET(false);
	}

	if (nick->User() == channel->Founder())
	{
		NSEND(service, user,
			 N_("Use standard SET PASSWORD on your own channels."));
		MT_RET(false);
	}

	boost::shared_ptr<Committee> comm = ROOT->data.Get_Committee(
			ROOT->ConfigValue<std::string>("commserv.sop.name"));
	if (comm && comm->MEMBER_Exists(channel->Founder()))
	{
		comm = ROOT->data.Get_Committee(
			ROOT->ConfigValue<std::string>("commserv.sadmin.name"));
		if (comm && !comm->MEMBER_Exists(nick->User()))
		{
			SEND(service, user, N_("Only a member of the %1% committee may set the password of a channel owned by a member of the %2% committee."),
				 ROOT->ConfigValue<std::string>("commserv.sadmin.name") %
				 ROOT->ConfigValue<std::string>("commserv.sop.name"));
			MT_RET(false);
		}
	}

	channel->Password(params[2]);
    SEND(service, user, N_("Password for channel %1% has been set to \002%2%\017."),
		 channel->Name() % params[2].substr(0, 32));
	service->ANNOUNCE(format(_("%1% has changed the password for %2%.")) %
									user->Name() % channel->Name());

	MT_RET(true);
	MT_EE
}

static bool biOP(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biOP" << service << user << params);

	boost::shared_ptr<LiveChannel> channel = ROOT->data.Get_LiveChannel(params[1]);
	if (!channel)
	{
		SEND(service, user, N_("Channel %1% is not in use."), params[1]);
		MT_RET(false);
	}

	boost::shared_ptr<StoredChannel> stored = ROOT->data.Get_StoredChannel(channel->Name());
	if (!user->InCommittee(ROOT->ConfigValue<std::string>("commserv.overrides.op")))
	{
		if (!stored)
		{
			SEND(service, user, N_("Channel %1% is not registered."), channel->Name());
			MT_RET(false);
		}

		if (!stored->ACCESS_Matches(user, StoredChannel::Level::LVL_CMD_Op))
		{
			SEND(service, user, N_("You don't have access to op users in channel %1%."),
				 channel->Name());
			MT_RET(false);
		}
	}

	bool secureops = (stored && stored->SecureOps());

	std::string modes;
	std::vector<std::string> p;
	if (params.size() < 3)
	{
		if (!channel->IsUser(user))
		{
			SEND(service, user, N_("You are not in channel %1%."), channel->Name());
			MT_RET(false);
		}

		if (channel->User(user, 'o'))
		{
			SEND(service, user, N_("You are already oped in channel %1%."),
				 channel->Name());
			MT_RET(false);
		}

		modes += 'o';
		p.push_back(user->Name());
	}
	else
	{
		for (size_t i=2; i < params.size(); ++i)
		{
			boost::shared_ptr<LiveUser> lu = ROOT->data.Get_LiveUser(params[i]);
			if (!lu)
			{
				SEND(service, user, N_("User %1% is not currently online."), params[i]);
				continue;
			}

			if (!channel->IsUser(lu))
			{
				SEND(service, user, N_("User %1% is not in channel %2%."),
					 lu->Name() % channel->Name());
				continue;
			}

			if (channel->User(lu, 'o'))
			{
				SEND(service, user, N_("User %1% is already oped in channel %2%."),
					 lu->Name() % channel->Name());
				continue;
			}

			if (secureops && !(stored->ACCESS_Matches(lu, StoredChannel::Level::LVL_AutoOp) ||
							   stored->ACCESS_Matches(lu, StoredChannel::Level::LVL_CMD_Op)))
			{
				SEND(service, user, N_("User %1% does not have op access on channel %2% and Secure Ops is enabled."),
					 lu->Name() % channel->Name());
				continue;
			}

			modes += 'o';
			p.push_back(lu->Name());
		}
	}

	if (modes.empty())
	{
		SEND(service, user, N_("No users to op on channel %1%."), channel->Name());
		MT_RET(false);
	}

	channel->SendModes(service, '+' + modes, p);

	MT_RET(true);
	MT_EE
}

static bool biDEOP(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biDEOP" << service << user << params);

	boost::shared_ptr<LiveChannel> channel = ROOT->data.Get_LiveChannel(params[1]);
	if (!channel)
	{
		SEND(service, user, N_("Channel %1% is not in use."), params[1]);
		MT_RET(false);
	}

	boost::shared_ptr<StoredChannel> stored = ROOT->data.Get_StoredChannel(channel->Name());
	if (!user->InCommittee(ROOT->ConfigValue<std::string>("commserv.overrides.op")))
	{
		if (!stored)
		{
			SEND(service, user, N_("Channel %1% is not registered."), channel->Name());
			MT_RET(false);
		}

		if (!stored->ACCESS_Matches(user, StoredChannel::Level::LVL_CMD_Op))
		{
			SEND(service, user, N_("You don't have access to deop users in channel %1%."),
				 channel->Name());
			MT_RET(false);
		}
	}

	std::string modes;
	std::vector<std::string> p;
	if (params.size() < 3)
	{
		if (!channel->IsUser(user))
		{
			SEND(service, user, N_("You are not in channel %1%."), channel->Name());
			MT_RET(false);
		}

		if (!channel->User(user, 'o'))
		{
			SEND(service, user, N_("You are not oped in channel %1%."),
				 channel->Name());
			MT_RET(false);
		}

		modes += 'o';
		p.push_back(user->Name());
	}
	else
	{
		for (size_t i=2; i < params.size(); ++i)
		{
			boost::shared_ptr<LiveUser> lu = ROOT->data.Get_LiveUser(params[i]);
			if (!lu)
			{
				SEND(service, user, N_("User %1% is not currently online."), params[i]);
				continue;
			}

			if (!channel->IsUser(lu))
			{
				SEND(service, user, N_("User %1% is not in channel %2%."),
					 lu->Name() % channel->Name());
				continue;
			}

			if (!channel->User(lu, 'o'))
			{
				SEND(service, user, N_("User %1% is not oped in channel %2%."),
					 lu->Name() % channel->Name());
				continue;
			}

			modes += 'o';
			p.push_back(lu->Name());
		}
	}

	if (modes.empty())
	{
		SEND(service, user, N_("No users to deop on channel %1%."), channel->Name());
		MT_RET(false);
	}

	channel->SendModes(service, '-' + modes, p);

	MT_RET(true);
	MT_EE
}

static bool biHOP(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biHOP" << service << user << params);

	std::string chanmodeparams = ROOT->proto.ConfigValue<std::string>("channel-mode-params");
	if (chanmodeparams.find("h") == std::string::npos)
	{
		SEND(service, user,
			 N_("The %1% command is not supported by the IRC server software in use on this network."),
			 boost::algorithm::to_upper_copy(params[0]));
		MT_RET(false);
	}

	boost::shared_ptr<LiveChannel> channel = ROOT->data.Get_LiveChannel(params[1]);
	if (!channel)
	{
		SEND(service, user, N_("Channel %1% is not in use."), params[1]);
		MT_RET(false);
	}

	boost::shared_ptr<StoredChannel> stored = ROOT->data.Get_StoredChannel(channel->Name());
	if (!user->InCommittee(ROOT->ConfigValue<std::string>("commserv.overrides.halfop")))
	{
		if (!stored)
		{
			SEND(service, user, N_("Channel %1% is not registered."), channel->Name());
			MT_RET(false);
		}

		if (!stored->ACCESS_Matches(user, StoredChannel::Level::LVL_CMD_HalfOp))
		{
			SEND(service, user, N_("You don't have access to half op users in channel %1%."),
				 channel->Name());
			MT_RET(false);
		}
	}

	bool secureops = (stored && stored->SecureOps());

	std::string modes;
	std::vector<std::string> p;
	if (params.size() < 3)
	{
		if (!channel->IsUser(user))
		{
			SEND(service, user, N_("You are not in channel %1%."), channel->Name());
			MT_RET(false);
		}

		if (channel->User(user, 'h'))
		{
			SEND(service, user, N_("You are already half oped in channel %1%."),
				 channel->Name());
			MT_RET(false);
		}

		modes += 'h';
		p.push_back(user->Name());
	}
	else
	{
		for (size_t i=2; i < params.size(); ++i)
		{
			boost::shared_ptr<LiveUser> lu = ROOT->data.Get_LiveUser(params[i]);
			if (!lu)
			{
				SEND(service, user, N_("User %1% is not currently online."), params[i]);
				continue;
			}

			if (!channel->IsUser(lu))
			{
				SEND(service, user, N_("User %1% is not in channel %2%."),
					 lu->Name() % channel->Name());
				continue;
			}

			if (channel->User(lu, 'h'))
			{
				SEND(service, user, N_("User %1% is already half oped in channel %2%."),
					 lu->Name() % channel->Name());
				continue;
			}

			if (secureops && !(stored->ACCESS_Matches(lu, StoredChannel::Level::LVL_AutoHalfOp) ||
							   stored->ACCESS_Matches(lu, StoredChannel::Level::LVL_CMD_HalfOp)))
			{
				SEND(service, user, N_("User %1% does not have half op access on channel %2% and Secure Ops is enabled."),
					 lu->Name() % channel->Name());
				continue;
			}

			modes += 'h';
			p.push_back(lu->Name());
		}
	}

	if (modes.empty())
	{
		SEND(service, user, N_("No users to half op on channel %1%."), channel->Name());
		MT_RET(false);
	}

	channel->SendModes(service, '+' + modes, p);

	MT_RET(true);
	MT_EE
}

static bool biDEHOP(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biDEHOP" << service << user << params);

	std::string chanmodeparams = ROOT->proto.ConfigValue<std::string>("channel-mode-params");
	if (chanmodeparams.find("h") == std::string::npos)
	{
		SEND(service, user,
			 N_("The %1% command is not supported by the IRC server software in use on this network."),
			 boost::algorithm::to_upper_copy(params[0]));
		MT_RET(false);
	}

	boost::shared_ptr<LiveChannel> channel = ROOT->data.Get_LiveChannel(params[1]);
	if (!channel)
	{
		SEND(service, user, N_("Channel %1% is not in use."), params[1]);
		MT_RET(false);
	}

	boost::shared_ptr<StoredChannel> stored = ROOT->data.Get_StoredChannel(channel->Name());
	if (!user->InCommittee(ROOT->ConfigValue<std::string>("commserv.overrides.halfop")))
	{
		if (!stored)
		{
			SEND(service, user, N_("Channel %1% is not registered."), channel->Name());
			MT_RET(false);
		}

		if (!stored->ACCESS_Matches(user, StoredChannel::Level::LVL_CMD_HalfOp))
		{
			SEND(service, user, N_("You don't have access to de-half op users in channel %1%."),
				 channel->Name());
			MT_RET(false);
		}
	}

	std::string modes;
	std::vector<std::string> p;
	if (params.size() < 3)
	{
		if (!channel->IsUser(user))
		{
			SEND(service, user, N_("You are not in channel %1%."), channel->Name());
			MT_RET(false);
		}

		if (!channel->User(user, 'h'))
		{
			SEND(service, user, N_("You are not half oped in channel %1%."),
				 channel->Name());
			MT_RET(false);
		}

		modes += 'h';
		p.push_back(user->Name());
	}
	else
	{
		for (size_t i=2; i < params.size(); ++i)
		{
			boost::shared_ptr<LiveUser> lu = ROOT->data.Get_LiveUser(params[i]);
			if (!lu)
			{
				SEND(service, user, N_("User %1% is not currently online."), params[i]);
				continue;
			}

			if (!channel->IsUser(lu))
			{
				SEND(service, user, N_("User %1% is not in channel %2%."),
					 lu->Name() % channel->Name());
				continue;
			}

			if (!channel->User(lu, 'h'))
			{
				SEND(service, user, N_("User %1% is not half oped in channel %2%."),
					 lu->Name() % channel->Name());
				continue;
			}

			modes += 'h';
			p.push_back(lu->Name());
		}
	}

	if (modes.empty())
	{
		SEND(service, user, N_("No users to de-half op on channel %1%."), channel->Name());
		MT_RET(false);
	}

	channel->SendModes(service, '-' + modes, p);

	MT_RET(true);
	MT_EE
}

static bool biVOICE(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biVOICE" << service << user << params);

	boost::shared_ptr<LiveChannel> channel = ROOT->data.Get_LiveChannel(params[1]);
	if (!channel)
	{
		SEND(service, user, N_("Channel %1% is not in use."), params[1]);
		MT_RET(false);
	}

	boost::shared_ptr<StoredChannel> stored = ROOT->data.Get_StoredChannel(channel->Name());
	if (!user->InCommittee(ROOT->ConfigValue<std::string>("commserv.overrides.voice")))
	{
		if (!stored)
		{
			SEND(service, user, N_("Channel %1% is not registered."), channel->Name());
			MT_RET(false);
		}

		if (!stored->ACCESS_Matches(user, StoredChannel::Level::LVL_CMD_Voice))
		{
			SEND(service, user, N_("You don't have access to voice users in channel %1%."),
				 channel->Name());
			MT_RET(false);
		}
	}

	bool secureops = (stored && stored->SecureOps());

	std::string modes;
	std::vector<std::string> p;
	if (params.size() < 3)
	{
		if (!channel->IsUser(user))
		{
			SEND(service, user, N_("You are not in channel %1%."), channel->Name());
			MT_RET(false);
		}

		if (channel->User(user, 'v'))
		{
			SEND(service, user, N_("You are already voiced in channel %1%."),
				 channel->Name());
			MT_RET(false);
		}

		modes += 'v';
		p.push_back(user->Name());
	}
	else
	{
		for (size_t i=2; i < params.size(); ++i)
		{
			boost::shared_ptr<LiveUser> lu = ROOT->data.Get_LiveUser(params[i]);
			if (!lu)
			{
				SEND(service, user, N_("User %1% is not currently online."), params[i]);
				continue;
			}

			if (!channel->IsUser(lu))
			{
				SEND(service, user, N_("User %1% is not in channel %2%."),
					 lu->Name() % channel->Name());
				continue;
			}

			if (channel->User(lu, 'v'))
			{
				SEND(service, user, N_("User %1% is already voiced in channel %2%."),
					 lu->Name() % channel->Name());
				continue;
			}

			if (secureops && !(stored->ACCESS_Matches(lu, StoredChannel::Level::LVL_AutoVoice) ||
							   stored->ACCESS_Matches(lu, StoredChannel::Level::LVL_CMD_Voice)))
			{
				SEND(service, user, N_("User %1% does not have voice access on channel %2% and Secure Ops is enabled."),
					 lu->Name() % channel->Name());
				continue;
			}

			modes += 'v';
			p.push_back(lu->Name());
		}
	}

	if (modes.empty())
	{
		SEND(service, user, N_("No users to voice on channel %1%."), channel->Name());
		MT_RET(false);
	}

	channel->SendModes(service, '+' + modes, p);

	MT_RET(true);
	MT_EE
}

static bool biDEVOICE(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biDEVOICE" << service << user << params);

	boost::shared_ptr<LiveChannel> channel = ROOT->data.Get_LiveChannel(params[1]);
	if (!channel)
	{
		SEND(service, user, N_("Channel %1% is not in use."), params[1]);
		MT_RET(false);
	}

	boost::shared_ptr<StoredChannel> stored = ROOT->data.Get_StoredChannel(channel->Name());
	if (!user->InCommittee(ROOT->ConfigValue<std::string>("commserv.overrides.voice")))
	{
		if (!stored)
		{
			SEND(service, user, N_("Channel %1% is not registered."), channel->Name());
			MT_RET(false);
		}

		if (!stored->ACCESS_Matches(user, StoredChannel::Level::LVL_CMD_Voice))
		{
			SEND(service, user, N_("You don't have access to devoice users in channel %1%."),
				 channel->Name());
			MT_RET(false);
		}
	}

	std::string modes;
	std::vector<std::string> p;
	if (params.size() < 3)
	{
		if (!channel->IsUser(user))
		{
			SEND(service, user, N_("You are not in channel %1%."), channel->Name());
			MT_RET(false);
		}

		if (!channel->User(user, 'v'))
		{
			SEND(service, user, N_("You are not voiced in channel %1%."),
				 channel->Name());
			MT_RET(false);
		}

		modes += 'v';
		p.push_back(user->Name());
	}
	else
	{
		for (size_t i=2; i < params.size(); ++i)
		{
			boost::shared_ptr<LiveUser> lu = ROOT->data.Get_LiveUser(params[i]);
			if (!lu)
			{
				SEND(service, user, N_("User %1% is not currently online."), params[i]);
				continue;
			}

			if (!channel->IsUser(lu))
			{
				SEND(service, user, N_("User %1% is not in channel %2%."),
					 lu->Name() % channel->Name());
				continue;
			}

			if (!channel->User(lu, 'v'))
			{
				SEND(service, user, N_("User %1% is not voiced in channel %2%."),
					 lu->Name() % channel->Name());
				continue;
			}

			modes += 'v';
			p.push_back(lu->Name());
		}
	}

	if (modes.empty())
	{
		SEND(service, user, N_("No users to devoice on channel %1%."), channel->Name());
		MT_RET(false);
	}

	channel->SendModes(service, '-' + modes, p);

	MT_RET(true);
	MT_EE
}

static bool biUSERS(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biUSERS" << service << user << params);

	boost::shared_ptr<LiveChannel> channel = ROOT->data.Get_LiveChannel(params[1]);
	if (!channel)
	{
		SEND(service, user, N_("Channel %1% is not in use."), params[1]);
		MT_RET(false);
	}

	if (!user->InCommittee(ROOT->ConfigValue<std::string>("commserv.overrides.view")))
	{
		boost::shared_ptr<StoredChannel> stored = ROOT->data.Get_StoredChannel(channel->Name());
		if (!stored)
		{
			SEND(service, user, N_("Channel %1% is not registered."), channel->Name());
			MT_RET(false);
		}

		if (!stored->ACCESS_Matches(user, StoredChannel::Level::LVL_View))
		{
			SEND(service, user, N_("You don't have access to view the current modes for channel %1%."),
				 channel->Name());
			MT_RET(false);
		}
	}

	LiveChannel::users_t users;
	channel->Users(users);

	std::string out;
	LiveChannel::users_t::iterator i;
	for (i = users.begin(); i != users.end(); ++i)
	{
		std::string name = i->first->Name();
		if (channel->Name().size() + out.size() + name.size() + 3 >
			ROOT->proto.ConfigValue<unsigned int>("max-line"))
		{
			SEND(service, user, N_("%1%:%2%"), channel->Name() % out);
			out.clear();
		}

		if (i->second.find('o') != i->second.end())
			out += " @" + name;
		else if (i->second.find('h') != i->second.end())
			out += " %" + name;
		else if (i->second.find('v') != i->second.end())
			out += " +" + name;
		// "q" (*)
		// "u" (.)
		// "a" (~)
		else
			out += " " + name;
	}
	if (!out.empty())
		SEND(service, user, N_("%1%:%2%"), channel->Name() % out);

	MT_RET(true);
	MT_EE
}

static bool biMODE(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biMODE" << service << user << params);

	boost::shared_ptr<LiveChannel> channel = ROOT->data.Get_LiveChannel(params[1]);
	if (!channel)
	{
		SEND(service, user, N_("Channel %1% is not in use."), params[1]);
		MT_RET(false);
	}

	if (params.size() < 3)
	{
		if (!user->InCommittee(ROOT->ConfigValue<std::string>("commserv.overrides.view")))
		{
			boost::shared_ptr<StoredChannel> stored = ROOT->data.Get_StoredChannel(channel->Name());
			if (!stored)
			{
				SEND(service, user, N_("Channel %1% is not registered."), channel->Name());
				MT_RET(false);
			}

			if (!stored->ACCESS_Matches(user, StoredChannel::Level::LVL_View))
			{
				SEND(service, user, N_("You don't have access to view the current modes for channel %1%."),
					 channel->Name());
				MT_RET(false);
			}
		}

		std::set<char> modes = channel->Modes();
		if (modes.empty())
		{
			SEND(service, user, N_("Channel %1% has no modes set."), channel->Name());
		}
		else
		{
			std::string out(modes.begin(), modes.end());
			if (modes.find('k') != modes.end())
				out += " " + channel->Modes_Key();
			if (modes.find('l') != modes.end())
				out += " " + boost::lexical_cast<std::string>(channel->Modes_Limit());
			SEND(service, user, N_("Modes for %1%: +%2%"), channel->Name() % out);
		}
	}
	else
	{
		boost::shared_ptr<StoredChannel> stored = ROOT->data.Get_StoredChannel(channel->Name());
		if (!user->InCommittee(ROOT->ConfigValue<std::string>("commserv.overrides.mode")))
		{
			if (!stored)
			{
				SEND(service, user, N_("Channel %1% is not registered."), channel->Name());
				MT_RET(false);
			}

			if (!stored->ACCESS_Matches(user, StoredChannel::Level::LVL_CMD_Mode))
			{
				SEND(service, user, N_("You don't have access to change the current modes for channel %1%."),
					 channel->Name());
				MT_RET(false);
			}
		}

		std::vector<std::string> p;
		if (params.size() > 3)
			p.assign(params.begin() + 3, params.end());
		std::string modes;
		if (stored)
			modes = stored->FilterModes(user, params[2], p);
		else
			modes = params[2];

		if (modes.empty())
		{
			SEND(service, user, N_("No valid modes to change on channel %1%."), channel->Name());
			MT_RET(false);
		}

		channel->SendModes(service, modes, p);
	}

	MT_RET(true);
	MT_EE
}

static bool biTOPIC(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biTOPIC" << service << user << params);

	if (params.size() < 3)
	{
		boost::shared_ptr<LiveChannel> channel = ROOT->data.Get_LiveChannel(params[1]);
		if (!channel)
		{
			SEND(service, user, N_("Channel %1% is not in use."), params[1]);
			MT_RET(false);
		}

		if (!user->InCommittee(ROOT->ConfigValue<std::string>("commserv.overrides.view")))
		{
			boost::shared_ptr<StoredChannel> stored = ROOT->data.Get_StoredChannel(channel->Name());
			if (!stored)
			{
				SEND(service, user, N_("Channel %1% is not registered."), channel->Name());
				MT_RET(false);
			}

			if (!stored->ACCESS_Matches(user, StoredChannel::Level::LVL_View))
			{
				SEND(service, user, N_("You don't have access to view the current modes for channel %1%."),
					 channel->Name());
				MT_RET(false);
			}
		}

		std::string topic = channel->Topic();
		if (topic.empty())
		{
			SEND(service, user, N_("Channel %1% has no topic set."), channel->Name());
		}
		else
		{
			SEND(service, user, N_("Topic for %1%: +%2%"), channel->Name() % topic);
			std::string setter = channel->Topic_Setter();
			boost::posix_time::ptime settime = channel->Topic_Set_Time();
			if (!setter.empty() && !settime.is_special())
			{
				SEND(service, user, N_("    Set By %1% %2% ago."), setter %
					 DurationToString(mantra::duration(settime,
													   mantra::GetCurrentDateTime()), mantra::Second));
			}
		}
	}
	else
	{
		boost::shared_ptr<StoredChannel> stored = ROOT->data.Get_StoredChannel(params[1]);
		if (!user->InCommittee(ROOT->ConfigValue<std::string>("commserv.overrides.mode")))
		{
			if (!stored)
			{
				SEND(service, user, N_("Channel %1% is not registered."), stored->Name());
				MT_RET(false);
			}

			if (!stored->ACCESS_Matches(user, StoredChannel::Level::LVL_CMD_Mode))
			{
				SEND(service, user, N_("You don't have access to change the topic on channel %1%."),
					 stored->Name());
				MT_RET(false);
			}
		}

		std::string topic = params[2];
		for (size_t i = 3; i < params.size(); ++i)
			topic += ' ' + params[i];

		stored->Topic(user, topic);

		boost::shared_ptr<LiveChannel> channel = ROOT->data.Get_LiveChannel(params[1]);
		if (channel)
			service->TOPIC(channel, topic);
	}

	MT_RET(true);
	MT_EE
}

static bool biKICK(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biKICK" << service << user << params);

	boost::shared_ptr<LiveChannel> channel = ROOT->data.Get_LiveChannel(params[1]);
	if (!channel)
	{
		SEND(service, user, N_("Channel %1% is not in use."), params[1]);
		MT_RET(false);
	}

	boost::shared_ptr<LiveUser> lu = ROOT->data.Get_LiveUser(params[2]);
	if (!lu)
	{
		SEND(service, user, N_("User %1% is not currently online."), params[2]);
		MT_RET(false);
	}

	if (!channel->IsUser(lu))
	{
		SEND(service, user, N_("User %1% is not in channel %2%."),
			 lu->Name() % channel->Name());
		MT_RET(false);
	}

	if (!user->InCommittee(ROOT->ConfigValue<std::string>("commserv.overrides.kick")))
	{
		boost::shared_ptr<StoredChannel> stored = ROOT->data.Get_StoredChannel(channel->Name());
		if (!stored)
		{
			SEND(service, user, N_("Channel %1% is not registered."), channel->Name());
			MT_RET(false);
		}

		boost::int32_t user_level = stored->ACCESS_Max(user);
		if (user_level < stored->LEVEL_Get(StoredChannel::Level::LVL_CMD_Kick).Value())
		{
			SEND(service, user, N_("You don't have access to kick users in channel %1%."),
				 channel->Name());
			MT_RET(false);
		}

		boost::int32_t lu_level = stored->ACCESS_Max(lu);
		if (user_level < lu_level)
		{
			SEND(service, user, N_("%1% is higher than you in the access list for channel %2%."),
				 lu->Name() % channel->Name());
			MT_RET(false);
		}
	}

	std::string reason;
	for (size_t i = 3; i < params.size(); ++i)
		reason += " " + params[i];

	if (reason.empty())
		service->KICK(channel, lu, boost::format(_("Kicked by %1%")) %
								   user->Name());
	else
		service->KICK(channel, lu, boost::format(_("Kicked by %1%:%2%")) %
								   user->Name() % reason);

	MT_RET(true);
	MT_EE
}

static bool biANONKICK(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biANONKICK" << service << user << params);

	boost::shared_ptr<LiveChannel> channel = ROOT->data.Get_LiveChannel(params[1]);
	if (!channel)
	{
		SEND(service, user, N_("Channel %1% is not in use."), params[1]);
		MT_RET(false);
	}

	boost::shared_ptr<LiveUser> lu = ROOT->data.Get_LiveUser(params[2]);
	if (!lu)
	{
		SEND(service, user, N_("User %1% is not currently online."), params[2]);
		MT_RET(false);
	}

	if (!channel->IsUser(lu))
	{
		SEND(service, user, N_("User %1% is not in channel %2%."),
			 lu->Name() % channel->Name());
		MT_RET(false);
	}

	if (!user->InCommittee(ROOT->ConfigValue<std::string>("commserv.overrides.kick")))
	{
		boost::shared_ptr<StoredChannel> stored = ROOT->data.Get_StoredChannel(channel->Name());
		if (!stored)
		{
			SEND(service, user, N_("Channel %1% is not registered."), channel->Name());
			MT_RET(false);
		}

		boost::int32_t user_level = stored->ACCESS_Max(user);
		if (user_level < stored->LEVEL_Get(StoredChannel::Level::LVL_Super).Value())
		{
			SEND(service, user, N_("You don't have access to anonymously kick users in channel %1%."),
				 channel->Name());
			MT_RET(false);
		}

		boost::int32_t lu_level = stored->ACCESS_Max(lu);
		if (user_level < lu_level)
		{
			SEND(service, user, N_("%1% is higher than you in the access list for channel %2%."),
				 lu->Name() % channel->Name());
			MT_RET(false);
		}
	}

	std::string reason = params[3];
	for (size_t i = 4; i < params.size(); ++i)
		reason += " " + params[i];

	service->KICK(channel, lu, boost::format(_("%2%")) % reason);

	MT_RET(false);
	MT_EE
}

static bool biINVITE(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biINVITE" << service << user << params);

	boost::shared_ptr<LiveChannel> channel = ROOT->data.Get_LiveChannel(params[1]);
	if (!channel)
	{
		SEND(service, user, N_("Channel %1% is not in use."), params[1]);
		MT_RET(false);
	}

	boost::shared_ptr<StoredChannel> stored = ROOT->data.Get_StoredChannel(channel->Name());
	if (!user->InCommittee(ROOT->ConfigValue<std::string>("commserv.overrides.invite")))
	{
		if (!stored)
		{
			SEND(service, user, N_("Channel %1% is not registered."), channel->Name());
			MT_RET(false);
		}

		if (!stored->ACCESS_Matches(user, StoredChannel::Level::LVL_CMD_Invite))
		{
			SEND(service, user, N_("You don't have access to invite users to channel %1%."),
				 channel->Name());
			MT_RET(false);
		}
	}

	bool restricted = (stored && stored->Restricted());

	if (params.size() < 3)
	{
		if (channel->IsUser(user))
		{
			SEND(service, user, N_("You are already in channel %1%."), channel->Name());
			MT_RET(false);
		}

		service->INVITE(channel, user);
	}
	else
	{
		for (size_t i=2; i < params.size(); ++i)
		{
			boost::shared_ptr<LiveUser> lu = ROOT->data.Get_LiveUser(params[i]);
			if (!lu)
			{
				SEND(service, user, N_("User %1% is not currently online."), params[i]);
				continue;
			}

			if (channel->IsUser(lu))
			{
				SEND(service, user, N_("User %1% is already in channel %2%."),
					 lu->Name() % channel->Name());
				continue;
			}

			// They must have a positive entry on the access list, regardless of what
			// it is.
			if (restricted && !stored->ACCESS_Matches(lu, 1))
			{
				SEND(service, user, N_("User %1% is not on the access list for channel %2% and Restricted is enabled."),
					 lu->Name() % channel->Name());
				continue;
			}

			service->INVITE(channel, lu);
		}
	}

	MT_RET(false);
	MT_EE
}

static bool biUNBAN(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biUNBAN" << service << user << params);

	boost::shared_ptr<LiveChannel> channel = ROOT->data.Get_LiveChannel(params[1]);
	if (!channel)
	{
		SEND(service, user, N_("Channel %1% is not in use."), params[1]);
		MT_RET(false);
	}

	boost::shared_ptr<StoredChannel> stored = ROOT->data.Get_StoredChannel(channel->Name());
	if (!user->InCommittee(ROOT->ConfigValue<std::string>("commserv.overrides.unban")))
	{
		if (!stored)
		{
			SEND(service, user, N_("Channel %1% is not registered."), channel->Name());
			MT_RET(false);
		}

		if (!stored->ACCESS_Matches(user, StoredChannel::Level::LVL_CMD_Unban))
		{
			SEND(service, user, N_("You don't have access to unban users from channel %1%."),
				 channel->Name());
			MT_RET(false);
		}
	}

	std::string modes;
	std::vector<std::string> p;

	if (params.size() < 3)
	{
		LiveChannel::bans_t bans;
		LiveChannel::rxbans_t rxbans;
		channel->MatchBan(user, bans);
		channel->MatchBan(user, rxbans);

		if (bans.empty() && rxbans.empty())
		{
			SEND(service, user, N_("You are not banned from channel %1%."),
				 channel->Name());
			MT_RET(false);
		}

		LiveChannel::bans_t::const_iterator j;
		for (j = bans.begin(); j != bans.end(); ++j)
		{
			modes += 'b';
			p.push_back(j->first);
		}
		LiveChannel::rxbans_t::const_iterator k;
		for (k = rxbans.begin(); k != rxbans.end(); ++k)
		{
			modes += 'd';
			p.push_back(k->first.str());
		}
	}
	else
	{
		for (size_t i=2; i < params.size(); ++i)
		{
			boost::shared_ptr<LiveUser> lu = ROOT->data.Get_LiveUser(params[i]);
			if (!lu)
			{
				SEND(service, user, N_("User %1% is not currently online."), params[i]);
				continue;
			}

			LiveChannel::bans_t bans;
			LiveChannel::rxbans_t rxbans;
			channel->MatchBan(lu, bans);
			channel->MatchBan(lu, rxbans);

			if (bans.empty() && rxbans.empty())
			{
				SEND(service, user, N_("User %1% is not banned from channel %2%."),
					 lu->Name() % channel->Name());
				continue;
			}

			LiveChannel::bans_t::const_iterator j;
			for (j = bans.begin(); j != bans.end(); ++j)
			{
				modes += 'b';
				p.push_back(j->first);
			}
			LiveChannel::rxbans_t::const_iterator k;
			for (k = rxbans.begin(); k != rxbans.end(); ++k)
			{
				modes += 'd';
				p.push_back(k->first.str());
			}
		}
	}

	if (modes.empty())
	{
		SEND(service, user, N_("No bans to be removed from channel %1%."), channel->Name());
		MT_RET(false);
	}

	channel->SendModes(service, '-' + modes, p);

	MT_RET(true);
	MT_EE
}

static bool biCLEAR_USERS(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biCLEAR_USERS" << service << user << params);

	boost::shared_ptr<LiveChannel> channel = ROOT->data.Get_LiveChannel(params[1]);
	if (!channel)
	{
		SEND(service, user, N_("Channel %1% is not in use."), params[1]);
		MT_RET(false);
	}

	boost::shared_ptr<StoredChannel> stored = ROOT->data.Get_StoredChannel(channel->Name());
	boost::int32_t user_level = 0;
	if (user->InCommittee(ROOT->ConfigValue<std::string>("commserv.overrides.clear")))
		user_level = ROOT->ConfigValue<boost::int32_t>("chanserv.max-level") + 2;
	else
	{
		if (!stored)
		{
			SEND(service, user, N_("Channel %1% is not registered."), channel->Name());
			MT_RET(false);
		}

		user_level = stored->ACCESS_Max(user);
		if (user_level < stored->LEVEL_Get(StoredChannel::Level::LVL_CMD_Clear).Value())
		{
			SEND(service, user, N_("You don't have access to use the %1% command in channel %2%."),
				 boost::algorithm::to_upper_copy(params[0]) % channel->Name());
			MT_RET(false);
		}
	}

	LiveChannel::users_t users;
	channel->Users(users);
	LiveChannel::users_t::iterator i;
	for (i = users.begin(); i != users.end(); ++i)
	{
		if (i->first == user)
			continue;
		else if (stored)
		{
			boost::int32_t level = stored->ACCESS_Max(i->first);
			if (level >= user_level)
				continue;
		}

		service->KICK(channel, i->first,
									format(_("%1% command used by %2%")) %
										   boost::algorithm::to_upper_copy(params[0]) %
										   user->Name());
	}

	MT_RET(true);
	MT_EE
}

static bool biCLEAR_OPS(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biCLEAR_OPS" << service << user << params);

	boost::shared_ptr<LiveChannel> channel = ROOT->data.Get_LiveChannel(params[1]);
	if (!channel)
	{
		SEND(service, user, N_("Channel %1% is not in use."), params[1]);
		MT_RET(false);
	}

	boost::shared_ptr<StoredChannel> stored = ROOT->data.Get_StoredChannel(channel->Name());
	boost::int32_t user_level = 0;
	if (user->InCommittee(ROOT->ConfigValue<std::string>("commserv.overrides.clear")))
		user_level = ROOT->ConfigValue<boost::int32_t>("chanserv.max-level") + 2;
	else
	{
		if (!stored)
		{
			SEND(service, user, N_("Channel %1% is not registered."), channel->Name());
			MT_RET(false);
		}

		user_level = stored->ACCESS_Max(user);
		if (user_level < stored->LEVEL_Get(StoredChannel::Level::LVL_CMD_Clear).Value())
		{
			SEND(service, user, N_("You don't have access to use the %1% command in channel %2%."),
				 boost::algorithm::to_upper_copy(params[0]) % channel->Name());
			MT_RET(false);
		}
	}

	std::string modes(1, '-');
	std::vector<std::string> params;

	LiveChannel::users_t users;
	channel->Users(users);
	LiveChannel::users_t::iterator i;
	for (i = users.begin(); i != users.end(); ++i)
	{
		if (i->first == user)
			continue;
		else if (i->second.find('o') == i->second.end())
			continue;
		else if (stored)
		{
			boost::int32_t level = stored->ACCESS_Max(i->first);
			if (level >= user_level)
				continue;
		}

		modes += 'o';
		params.push_back(i->first->Name());
	}

	if (!params.empty())
		channel->SendModes(service, modes, params);

	MT_RET(true);
	MT_EE
}

static bool biCLEAR_HOPS(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biCLEAR_HOPS" << service << user << params);

	std::string chanmodeparams = ROOT->proto.ConfigValue<std::string>("channel-mode-params");
	if (chanmodeparams.find("h") == std::string::npos)
	{
		SEND(service, user,
			 N_("The %1% command is not supported by the IRC server software in use on this network."),
			 boost::algorithm::to_upper_copy(params[0]));
		MT_RET(false);
	}

	boost::shared_ptr<LiveChannel> channel = ROOT->data.Get_LiveChannel(params[1]);
	if (!channel)
	{
		SEND(service, user, N_("Channel %1% is not in use."), params[1]);
		MT_RET(false);
	}

	boost::shared_ptr<StoredChannel> stored = ROOT->data.Get_StoredChannel(channel->Name());
	boost::int32_t user_level = 0;
	if (user->InCommittee(ROOT->ConfigValue<std::string>("commserv.overrides.clear")))
		user_level = ROOT->ConfigValue<boost::int32_t>("chanserv.max-level") + 2;
	else
	{
		if (!stored)
		{
			SEND(service, user, N_("Channel %1% is not registered."), channel->Name());
			MT_RET(false);
		}

		user_level = stored->ACCESS_Max(user);
		if (user_level < stored->LEVEL_Get(StoredChannel::Level::LVL_CMD_Clear).Value())
		{
			SEND(service, user, N_("You don't have access to use the %1% command in channel %2%."),
				 boost::algorithm::to_upper_copy(params[0]) % channel->Name());
			MT_RET(false);
		}
	}

	std::string modes(1, '-');
	std::vector<std::string> params;

	LiveChannel::users_t users;
	channel->Users(users);
	LiveChannel::users_t::iterator i;
	for (i = users.begin(); i != users.end(); ++i)
	{
		if (i->first == user)
			continue;
		else if (i->second.find('h') == i->second.end())
			continue;
		else if (stored)
		{
			boost::int32_t level = stored->ACCESS_Max(i->first);
			if (level >= user_level)
				continue;
		}

		modes += 'h';
		params.push_back(i->first->Name());
	}

	if (!params.empty())
		channel->SendModes(service, modes, params);

	MT_RET(true);
	MT_EE
}

static bool biCLEAR_VOICES(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biCLEAR_VOICES" << service << user << params);

	boost::shared_ptr<LiveChannel> channel = ROOT->data.Get_LiveChannel(params[1]);
	if (!channel)
	{
		SEND(service, user, N_("Channel %1% is not in use."), params[1]);
		MT_RET(false);
	}

	boost::shared_ptr<StoredChannel> stored = ROOT->data.Get_StoredChannel(channel->Name());
	boost::int32_t user_level = 0;
	if (user->InCommittee(ROOT->ConfigValue<std::string>("commserv.overrides.clear")))
		user_level = ROOT->ConfigValue<boost::int32_t>("chanserv.max-level") + 2;
	else
	{
		if (!stored)
		{
			SEND(service, user, N_("Channel %1% is not registered."), channel->Name());
			MT_RET(false);
		}

		user_level = stored->ACCESS_Max(user);
		if (user_level < stored->LEVEL_Get(StoredChannel::Level::LVL_CMD_Clear).Value())
		{
			SEND(service, user, N_("You don't have access to use the %1% command in channel %2%."),
				 boost::algorithm::to_upper_copy(params[0]) % channel->Name());
			MT_RET(false);
		}
	}

	std::string modes(1, '-');
	std::vector<std::string> params;

	LiveChannel::users_t users;
	channel->Users(users);
	LiveChannel::users_t::iterator i;
	for (i = users.begin(); i != users.end(); ++i)
	{
		if (i->first == user)
			continue;
		else if (i->second.find('v') == i->second.end())
			continue;
		else if (stored)
		{
			boost::int32_t level = stored->ACCESS_Max(i->first);
			if (level >= user_level)
				continue;
		}

		modes += 'v';
		params.push_back(i->first->Name());
	}

	if (!params.empty())
		channel->SendModes(service, modes, params);

	MT_RET(true);
	MT_EE
}

static bool biCLEAR_MODES(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biCLEAR_MODES" << service << user << params);

	boost::shared_ptr<LiveChannel> channel = ROOT->data.Get_LiveChannel(params[1]);
	if (!channel)
	{
		SEND(service, user, N_("Channel %1% is not in use."), params[1]);
		MT_RET(false);
	}

	boost::shared_ptr<StoredChannel> stored = ROOT->data.Get_StoredChannel(channel->Name());
	if (!user->InCommittee(ROOT->ConfigValue<std::string>("commserv.overrides.clear")))
	{
		if (!stored)
		{
			SEND(service, user, N_("Channel %1% is not registered."), channel->Name());
			MT_RET(false);
		}

		if (!stored->ACCESS_Matches(user, StoredChannel::Level::LVL_CMD_Clear))
		{
			SEND(service, user, N_("You don't have access to use the %1% command in channel %2%."),
				 boost::algorithm::to_upper_copy(params[0]) % channel->Name());
			MT_RET(false);
		}
	}

	std::set<char> m = channel->Modes();
	if (m.empty())
	{
		SEND(service, user, N_("Channel %1% has no modes set."), channel->Name());
		MT_RET(false);
	}

	channel->SendModes(service, '-' + std::string(m.begin(), m.end()));

	MT_RET(true);
	MT_EE
}

static bool biCLEAR_BANS(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biCLEAR_BANS" << service << user << params);

	boost::shared_ptr<LiveChannel> channel = ROOT->data.Get_LiveChannel(params[1]);
	if (!channel)
	{
		SEND(service, user, N_("Channel %1% is not in use."), params[1]);
		MT_RET(false);
	}

	boost::shared_ptr<StoredChannel> stored = ROOT->data.Get_StoredChannel(channel->Name());
	if (!user->InCommittee(ROOT->ConfigValue<std::string>("commserv.overrides.clear")))
	{
		if (!stored)
		{
			SEND(service, user, N_("Channel %1% is not registered."), channel->Name());
			MT_RET(false);
		}

		if (!stored->ACCESS_Matches(user, StoredChannel::Level::LVL_CMD_Clear))
		{
			SEND(service, user, N_("You don't have access to use the %1% command in channel %2%."),
				 boost::algorithm::to_upper_copy(params[0]) % channel->Name());
			MT_RET(false);
		}
	}

	std::string modes(1, '-');
	std::vector<std::string> params;

	LiveChannel::bans_t bans;
	channel->Bans(bans);
	LiveChannel::bans_t::iterator i;
	for (i = bans.begin(); i != bans.end(); ++i)
	{
		modes += 'b';
		params.push_back(i->first);
	}

	if (ROOT->proto.ConfigValue<std::string>("channel-mode-params").find('d') != std::string::npos)
	{
		LiveChannel::rxbans_t rxbans;
		channel->Bans(rxbans);
		LiveChannel::rxbans_t::iterator j;
		for (j = rxbans.begin(); j != rxbans.end(); ++j)
		{
			modes += 'd';
			params.push_back(j->first.str());
		}
	}

	if (!params.empty())
		channel->SendModes(service, modes, params);

	MT_RET(true);
	MT_EE
}

static bool biCLEAR_EXEMPTS(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biCLEAR_EXEMPTS" << service << user << params);

	std::string chanmodeparams = ROOT->proto.ConfigValue<std::string>("channel-mode-params");
	if (chanmodeparams.find("e") == std::string::npos)
	{
		SEND(service, user,
			 N_("The %1% command is not supported by the IRC server software in use on this network."),
			 boost::algorithm::to_upper_copy(params[0]));
		MT_RET(false);
	}

	boost::shared_ptr<LiveChannel> channel = ROOT->data.Get_LiveChannel(params[1]);
	if (!channel)
	{
		SEND(service, user, N_("Channel %1% is not in use."), params[1]);
		MT_RET(false);
	}

	boost::shared_ptr<StoredChannel> stored = ROOT->data.Get_StoredChannel(channel->Name());
	if (!user->InCommittee(ROOT->ConfigValue<std::string>("commserv.overrides.clear")))
	{
		if (!stored)
		{
			SEND(service, user, N_("Channel %1% is not registered."), channel->Name());
			MT_RET(false);
		}

		if (!stored->ACCESS_Matches(user, StoredChannel::Level::LVL_CMD_Clear))
		{
			SEND(service, user, N_("You don't have access to use the %1% command in channel %2%."),
				 boost::algorithm::to_upper_copy(params[0]) % channel->Name());
			MT_RET(false);
		}
	}

	std::string modes(1, '-');
	std::vector<std::string> params;

	LiveChannel::exempts_t exempts;
	channel->Exempts(exempts);
	LiveChannel::exempts_t::iterator i;
	for (i = exempts.begin(); i != exempts.end(); ++i)
	{
		modes += 'e';
		params.push_back(*i);
	}

	/*
	if (chanmodeparams.find("e") == std::string::npos)
	{
		LiveChannel::rxexempts_t rxexempts;
		channel->Exempts(rxexempts);
		LiveChannel::rxexempts_t::iterator j;
		for (j = rxexempts.begin(); j != rxexempts.end(); ++j)
		{
			modes += 'd';
			params.push_back(j->str());
		}
	}
	*/

	if (!params.empty())
		channel->SendModes(service, modes, params);

	MT_RET(true);
	MT_EE
}

static bool biCLEAR_ALL(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biCLEAR_ALL" << service << user << params);

	std::string chanmodeparams = ROOT->proto.ConfigValue<std::string>("channel-mode-params");

	biCLEAR_OPS(service, user, params);
	if (chanmodeparams.find("h") != std::string::npos)
		biCLEAR_HOPS(service, user, params);
	biCLEAR_VOICES(service, user, params);
	biCLEAR_MODES(service, user, params);
	biCLEAR_BANS(service, user, params);
	if (chanmodeparams.find("e") != std::string::npos)
		biCLEAR_EXEMPTS(service, user, params);

	MT_RET(false);
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

	static boost::regex channel_rx("^[&#+][^[:space:][:cntrl:],]*$");
	if (!boost::regex_match(params[1], channel_rx))
	{
		NSEND(service, user,
			  N_("Channel mask specified is not a valid."));
		MT_RET(false);
	}

	std::string reason(params[2]);
	for (size_t i = 3; i < params.size(); ++i)
		reason += ' ' + params[i];

	Forbidden f = Forbidden::create(params[1], reason, nick);
	ROOT->data.Add(f);
	SEND(service, user,
		 N_("Channel mask \002%1%\017 has been added to the forbidden list."),
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
		ROOT->data.Get_Forbidden(ent, true);

		for (size_t i = 1; i < params.size(); ++i)
		{
			if (!ROOT->proto.IsChannel(params[i]))
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
				 N_("No channels matching \002%1%\017 are forbidden."),
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
			 N_("%1% entries removed from the forbidden channel list."),
			 v.size());
	}
	else if (v.size() != skipped)
	{
		SEND(service, user,
			 N_("%1% entries removed from the forbidden channel list and %2% entries specified could not be found."),
			 (v.size() - skipped) % skipped);
	}
	else
	{
		NSEND(service, user,
			  N_("No specified entries could be found on the forbidden channel list."));
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
	ROOT->data.Get_Forbidden(ent, true);

	if (ent.empty())
		NSEND(service, user, N_("The forbidden channel list is empty"));
	else
	{
		NSEND(service, user, N_("Forbidden Channels:"));
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

static bool biLEVEL_SET(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biLEVEL_SET" << service << user << params);

	boost::shared_ptr<StoredNick> nick = ROOT->data.Get_StoredNick(user->Name());
	if (!nick)
	{
		NSEND(service, user, N_("Your nickname is not registered."));
		MT_RET(false);
	}

	boost::shared_ptr<StoredChannel> channel = ROOT->data.Get_StoredChannel(params[1]);
	if (!channel)
	{
		SEND(service, user, N_("Channel %1% is not registered."), params[1]);
		MT_RET(false);
	}

	if (!user->Identified(channel) && (channel->Secure() ||  nick->User() != channel->Founder()))
	{
		SEND(service, user, N_("You don't have access to alter levels for channel %1%."),
			 channel->Name());
		MT_RET(false);
	}

	boost::uint32_t level;
	try
	{
		level = boost::lexical_cast<boost::uint32_t>(params[2]);

		if (StoredChannel::Level::DefaultDesc(level).empty())
		{
			SEND(service, user, N_("No such level %1% exists."), params[2]);
			MT_RET(false);
		}
	}
	catch (const boost::bad_lexical_cast &e)
	{

		level = StoredChannel::Level::DefaultName(params[2]);

		if (!level)
		{
			SEND(service, user, N_("No such level %1% exists."), params[2]);
			MT_RET(false);
		}
	}

	boost::int32_t value;
	try
	{
		value = boost::lexical_cast<boost::int32_t>(params[3]);

		if (value < ROOT->ConfigValue<boost::int32_t>("chanserv.min-level") ||
			value > ROOT->ConfigValue<boost::int32_t>("chanserv.max-level"))
		{
			SEND(service, user, N_("New level value must be between %1% and %2%."),
				 ROOT->ConfigValue<boost::int32_t>("chanserv.min-level") %
				 ROOT->ConfigValue<boost::int32_t>("chanserv.max-level"));
			MT_RET(false);
		}
	}
	catch (const boost::bad_lexical_cast &e)
	{
		static boost::regex founder("^(HEAD|FOUNDER|OWNER)$", boost::regex::normal | boost::regex::icase);
		static boost::regex disable("^(DISABLED?|OFF)$", boost::regex::normal | boost::regex::icase);

		if (boost::regex_match(params[3], founder))
		{
			value = ROOT->ConfigValue<boost::int32_t>("chanserv.max-level") + 1;
		}
		else if (boost::regex_match(params[3], disable))
		{
			value = ROOT->ConfigValue<boost::int32_t>("chanserv.max-level") + 2;
		}
		else
		{
			NSEND(service, user, N_("New level value must be an integer, FOUNDER or DISABLE."));
			MT_RET(false);
		}
	}

	StoredChannel::Level entry = channel->LEVEL_Get(level);
	entry.Change(value, nick);

	if (value <= ROOT->ConfigValue<boost::int32_t>("chanserv.max-level"))
	{
		SEND(service, user, N_("%1% level for channel %2% has been set to %3%."),
			 StoredChannel::Level::DefaultDesc(level, user) % channel->Name() % value);
	}
	else if (value == ROOT->ConfigValue<boost::int32_t>("chanserv.max-level") + 1)
	{
		SEND(service, user, N_("%1% level for channel %2% has been set to founder only."),
			 StoredChannel::Level::DefaultDesc(level, user) % channel->Name());
	}
	else
	{
		SEND(service, user, N_("%1% level for channel %2% has been disabled."),
			 StoredChannel::Level::DefaultDesc(level, user) % channel->Name());
	}

	MT_RET(false);
	MT_EE
}

static bool biLEVEL_UNSET(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biLEVEL_UNSET" << service << user << params);

	boost::shared_ptr<StoredNick> nick = ROOT->data.Get_StoredNick(user->Name());
	if (!nick)
	{
		NSEND(service, user, N_("Your nickname is not registered."));
		MT_RET(false);
	}

	boost::shared_ptr<StoredChannel> channel = ROOT->data.Get_StoredChannel(params[1]);
	if (!channel)
	{
		SEND(service, user, N_("Channel %1% is not registered."), params[1]);
		MT_RET(false);
	}

	if (!user->Identified(channel) && (channel->Secure() ||  nick->User() != channel->Founder()))
	{
		SEND(service, user, N_("You don't have access to alter levels for channel %1%."),
			 channel->Name());
		MT_RET(false);
	}

	boost::uint32_t level;
	try
	{
		level = boost::lexical_cast<boost::uint32_t>(params[2]);

		if (StoredChannel::Level::DefaultDesc(level).empty())
		{
			SEND(service, user, N_("No such level %1% exists."), params[2]);
			MT_RET(false);
		}
	}
	catch (const boost::bad_lexical_cast &e)
	{

		level = StoredChannel::Level::DefaultName(params[2]);

		if (!level)
		{
			SEND(service, user, N_("No such level %1% exists."), params[2]);
			MT_RET(false);
		}
	}

	channel->LEVEL_Del(level);
	SEND(service, user, N_("%1% level for channel %2% has been reset to the default value."),
		 StoredChannel::Level::DefaultDesc(level, user) % channel->Name());

	MT_RET(false);
	MT_EE
}

static bool biLEVEL_LIST(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biLEVEL_LIST" << service << user << params);

	boost::shared_ptr<StoredNick> nick = ROOT->data.Get_StoredNick(user->Name());
	if (!nick)
	{
		NSEND(service, user, N_("Your nickname is not registered."));
		MT_RET(false);
	}

	boost::shared_ptr<StoredChannel> channel = ROOT->data.Get_StoredChannel(params[1]);
	if (!channel)
	{
		SEND(service, user, N_("Channel %1% is not registered."), params[1]);
		MT_RET(false);
	}

	if (user->Identified(channel) || (nick->User() == channel->Founder() &&
		(!channel->Secure() || user->Identified())))
	{
		std::set<StoredChannel::Level> levels;
		channel->LEVEL_Get(levels);

		NSEND(service, user, N_("    DESCRIPTION                         LEVEL"));
		std::set<StoredChannel::Level>::iterator i;
		for (i = levels.begin(); i != levels.end(); ++i)
		{
			std::string updater = i->Last_UpdaterName();
			std::string value;
			if (i->Value() <= ROOT->ConfigValue<boost::int32_t>("chanserv.max-level"))
			{
				value = boost::lexical_cast<std::string>(i->Value());
			}
			else if (i->Value() == ROOT->ConfigValue<boost::int32_t>("chanserv.max-level") + 1)
			{
				value = U_(user, "FOUNDER");
			}
			else
			{
				value = U_(user, "DISABLED");
			}

			if (updater.empty())
				SEND(service, user, N_("%1$_2d. %2$-35s %3$s (default)"), i->ID() %
					 StoredChannel::Level::DefaultDesc(i->ID(), user) % value);
			else
				SEND(service, user, N_("%1$_2d. %2$-35s %3$s (set %4$s ago by %5$s)"), i->ID() %
					 StoredChannel::Level::DefaultDesc(i->ID(), user) % value %
					 DurationToString(mantra::duration(i->Last_Update(),
													   mantra::GetCurrentDateTime()),
									  mantra::Second) % updater);
		}
	}
	else
	{
		boost::int32_t level = channel->ACCESS_Max(user);
		if (level <= 0)
		{
			SEND(service, user, N_("You do not have any access on channel %1%."),
				 channel->Name());
			MT_RET(false);
		}

		std::set<StoredChannel::Level> levels;
		channel->LEVEL_Get(levels);

		std::set<StoredChannel::Level>::iterator i;
		for (i = levels.begin(); i != levels.end(); ++i)
		{
			if (level >= i->Value())
			{
				SEND(service, user, N_("You have access to %1%."),
					 StoredChannel::Level::DefaultDesc(level, user));
			}
		}
	}

	MT_RET(false);
	MT_EE
}

static bool biACCESS_ADD(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biACCESS_ADD" << service << user << params);

	boost::shared_ptr<StoredNick> nick = ROOT->data.Get_StoredNick(user->Name());
	if (!nick)
	{
		NSEND(service, user, N_("Your nickname is not registered."));
		MT_RET(false);
	}

	boost::shared_ptr<StoredChannel> channel = ROOT->data.Get_StoredChannel(params[1]);
	if (!channel)
	{
		SEND(service, user, N_("Channel %1% is not registered."), params[1]);
		MT_RET(false);
	}

	boost::int32_t level = channel->ACCESS_Max(user);
	if (level < channel->LEVEL_Get(StoredChannel::Level::LVL_Access).Value())
	{
		SEND(service, user, N_("You do not have sufficient access to modify the access list for channel %1%."),
			 channel->Name());
		MT_RET(false);
	}

	boost::int32_t newlevel;
	try
	{
		newlevel = boost::lexical_cast<boost::int32_t>(params[3]);
		if (newlevel == 0)
		{
			NSEND(service, user, N_("New access level must not be 0."));
			MT_RET(false);
		}

		if (newlevel < ROOT->ConfigValue<boost::int32_t>("chanserv.min-level") ||
			newlevel > ROOT->ConfigValue<boost::int32_t>("chanserv.max-level"))
		{
			SEND(service, user, N_("New access level must be between %1% and %2%."),
				 ROOT->ConfigValue<boost::int32_t>("chanserv.min-level") %
				 ROOT->ConfigValue<boost::int32_t>("chanserv.max-level"));
			MT_RET(false);
		}

		if (newlevel >= level)
		{
			NSEND(service, user, N_("You may only add entries to the access list below your own level."));
			MT_RET(false);
		}
	}
	catch (const boost::bad_lexical_cast &e)
	{
		NSEND(service, user, N_("New access level must be an integer."));
		MT_RET(false);
	}

	static boost::regex nick_rx("^[[:alpha:]\\x5B-\\x60\\x7B-\\x7D][-[:alnum:]\\x5B-\\x60\\x7B-\\x7D]*$");
	static boost::regex committee_rx("^@[[:print:]]+$");
	static boost::regex mask_rx("^([[:alpha:]\\x5B-\\x60\\x7B-\\x7D*?][-[:alnum:]\\x5B-\\x60\\x7B-\\x7D*?]*!)?"
								"[^[:space:][:cntrl:]@]+@(([[:alnum:]*?]([-[:alnum:]*?]{0,61}[[:alnum:]*?])?\\.)*"
								"[[:alnum:]*?]([-[:alnum:]*?]{0,61}[[:alnum:]*?])?|[[:xdigit:]:*?]*)$");
	if (boost::regex_match(params[2], nick_rx))
	{
		boost::shared_ptr<StoredNick> target = ROOT->data.Get_StoredNick(params[2]);
		if (!target)
		{
			SEND(service, user, N_("Nickname %1% is not registered."), params[2]);	
			MT_RET(false);
		}

		StoredChannel::Access acc(channel->ACCESS_Get(target->User()));
		if (acc.Number())
		{
			if (acc.Level() >= level)
			{
				SEND(service, user, N_("Nickname %1% is higher than you or equal to you on the channel %2% access list."),
					 target->Name() % channel->Name());
				MT_RET(false);
			}

			acc.ChangeValue(newlevel, nick);
			SEND(service, user, N_("Nickname %1% has been changed to access level %2% on channel %3%."),
				 target->Name() % newlevel % channel->Name());
		}
		else
		{
			StoredChannel::Access acc2(channel->ACCESS_Add(target->User(), newlevel, nick));
			if (!acc2.Number())
			{
				SEND(service, user, N_("Could not add nickname %1% to channel %2% access list."),
					 target->Name() % channel->Name());
				MT_RET(false);
			}

			SEND(service, user, N_("Nickname %1% has been added at access level %2% to channel %3%."),
				 target->Name() % newlevel % channel->Name());
		}
	}
	else if (boost::regex_match(params[2], committee_rx))
	{
		boost::shared_ptr<Committee> target = ROOT->data.Get_Committee(params[2].substr(1));
		if (!target)
		{
			SEND(service, user, N_("Committee %1% is not registered."), params[2].substr(1));	
			MT_RET(false);
		}

		StoredChannel::Access acc(channel->ACCESS_Get(target));
		if (acc.Number())
		{
			if (acc.Level() >= level)
			{
				SEND(service, user, N_("Committee %1% is higher than you or equal to you on the channel %2% access list."),
					 target->Name() % channel->Name());
				MT_RET(false);
			}

			acc.ChangeValue(newlevel, nick);
			SEND(service, user, N_("Committee %1% has been changed to access level %2% on channel %3%."),
				 target->Name() % newlevel % channel->Name());
		}
		else
		{
			StoredChannel::Access acc2(channel->ACCESS_Add(target, newlevel, nick));
			if (!acc2.Number())
			{
				SEND(service, user, N_("Could not add committee %1% to channel %2% access list."),
					 target->Name() % channel->Name());
				MT_RET(false);
			}

			SEND(service, user, N_("Committee %1% has been added at access level %2% to channel %3%."),
				 target->Name() % newlevel % channel->Name());
		}
	}
	else if (boost::regex_match(params[2], mask_rx))
	{
		std::string entry;
		if (params[2].find('!') == std::string::npos)
			entry = "*!" + params[2];
		else
			entry = params[2];

		StoredChannel::Access acc(channel->ACCESS_Get(entry));
		if (acc.Number())
		{
			if (acc.Level() >= level)
			{
				SEND(service, user, N_("Mask %1% is higher than you or equal to you on the channel %2% access list."),
					 acc.Mask() % channel->Name());
				MT_RET(false);
			}

			acc.ChangeValue(newlevel, nick);
			SEND(service, user, N_("Mask %1% has been changed to access level %2% on channel %3%."),
				 acc.Mask() % newlevel % channel->Name());
		}
		else
		{
			StoredChannel::Access acc2(channel->ACCESS_Add(entry, newlevel, nick));
			if (!acc2.Number())
			{
				SEND(service, user, N_("Could not add mask %1% to channel %2% access list."),
					 entry % channel->Name());
				MT_RET(false);
			}

			SEND(service, user, N_("Mask %1% has been added at access level %2% to channel %3%."),
				 entry % newlevel % channel->Name());
		}
	}
	else
	{
		try
		{
			boost::uint32_t entry = boost::lexical_cast<boost::uint32_t>(params[2]);

			StoredChannel::Access acc(channel->ACCESS_Get(entry));
			if (acc.Number())
			{
				if (acc.Level() >= level)
				{
					SEND(service, user, N_("Entry #%1% is higher than you or equal to you on the channel %2% access list."),
						 entry % channel->Name());
					MT_RET(false);
				}

				acc.ChangeValue(newlevel, nick);
				SEND(service, user, N_("Entry #%1% has been changed to access level %2% on channel %3%."),
					 entry % newlevel % channel->Name());
			}
			else
			{
				SEND(service, user, N_("Could not find entry #%1% on access list for channel %2%."),
					  entry % channel->Name());
				MT_RET(false);
			}
		}
		catch (const boost::bad_lexical_cast &e)
		{
			NSEND(service, user, N_("Invalid mask, nickname, committee or existing entry specified."));
			MT_RET(false);
		}
	}

	MT_RET(true);
	MT_EE
}

static bool biACCESS_DEL(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biACCESS_DEL" << service << user << params);

	boost::shared_ptr<StoredNick> nick = ROOT->data.Get_StoredNick(user->Name());
	if (!nick)
	{
		NSEND(service, user, N_("Your nickname is not registered."));
		MT_RET(false);
	}

	boost::shared_ptr<StoredChannel> channel = ROOT->data.Get_StoredChannel(params[1]);
	if (!channel)
	{
		SEND(service, user, N_("Channel %1% is not registered."), params[1]);
		MT_RET(false);
	}

	boost::int32_t level = channel->ACCESS_Max(user);
	if (level < channel->LEVEL_Get(StoredChannel::Level::LVL_Access).Value())
	{
		SEND(service, user, N_("You do not have sufficient access to modify the access list for channel %1%."),
			 channel->Name());
		MT_RET(false);
	}

	size_t deleted = 0, skipped = 0, notfound = 0;

	static boost::regex nick_rx("^[[:alpha:]\\x5B-\\x60\\x7B-\\x7D][-[:alnum:]\\x5B-\\x60\\x7B-\\x7D]*$");
	static boost::regex committee_rx("^@[[:print:]]+$");
	static boost::regex mask_rx("^([[:alpha:]\\x5B-\\x60\\x7B-\\x7D*?][-[:alnum:]\\x5B-\\x60\\x7B-\\x7D*?]*!)?"
								"[^[:space:][:cntrl:]@]+@(([[:alnum:]*?]([-[:alnum:]*?]{0,61}[[:alnum:]*?])?\\.)*"
								"[[:alnum:]*?]([-[:alnum:]*?]{0,61}[[:alnum:]*?])?|[[:xdigit:]:*?]*)$");
	for (size_t i = 2; i < params.size(); ++i)
	{
		if (boost::regex_match(params[i], nick_rx))
		{
			boost::shared_ptr<StoredNick> target = ROOT->data.Get_StoredNick(params[i]);
			if (!target)
			{
				++notfound;
				continue;
			}

			StoredChannel::Access acc(channel->ACCESS_Get(target->User()));
			if (acc.Number())
			{
				if (acc.Level() >= level)
				{
					++skipped;
					continue;
				}

				channel->ACCESS_Del(acc.Number());
				++deleted;
			}
			else
			{
				++notfound;
			}
		}
		else if (boost::regex_match(params[i], committee_rx))
		{
			boost::shared_ptr<Committee> target = ROOT->data.Get_Committee(params[i].substr(1));
			if (!target)
			{
				++notfound;
				continue;
			}

			StoredChannel::Access acc(channel->ACCESS_Get(target));
			if (acc.Number())
			{
				if (acc.Level() >= level)
				{
					++skipped;
					continue;
				}

				channel->ACCESS_Del(acc.Number());
				++deleted;
			}
			else
			{
				++notfound;
			}
		}
		else if (boost::regex_match(params[i], mask_rx))
		{
			std::string entry;
			if (params[i].find('!') == std::string::npos)
				entry = "*!" + params[i];
			else
				entry = params[i];

			std::list<StoredChannel::Access> acc(channel->ACCESS_Matches(
					mantra::make_globrx(entry, boost::regex_constants::normal |
												   boost::regex_constants::icase)));
			if (!acc.empty())
			{
				std::list<StoredChannel::Access>::iterator j;
				for (j = acc.begin(); j != acc.end(); ++j)
				{
					if (j->Level() >= level)
					{
						++skipped;
						continue;
					}

					channel->ACCESS_Del(j->Number());
					++deleted;
				}
			}
			else
			{
				++notfound;
			}
		}
		else
		{
			std::vector<unsigned int> nums;
			if (mantra::ParseNumbers(params[i], nums))
			{
				std::vector<unsigned int>::iterator j;
				for (j = nums.begin(); j != nums.end(); ++j)
				{
					StoredChannel::Access acc = channel->ACCESS_Get(*j);
					if (acc.Number())
					{
						if (acc.Level() >= level)
						{
							++skipped;
							continue;
						}

						channel->ACCESS_Del(acc.Number());
						++deleted;
					}
					else
						++notfound;
				}
			}
			else
			{
				++notfound;
			}
		}
	}

	if (deleted)
	{
		if (skipped)
			SEND(service, user,
				 N_("%1% entries removed (%2% had higher access and skipped) from the access list for channel %3%."),
				 deleted % skipped % channel->Name());
		else
			SEND(service, user,
				 N_("%1% entries removed from the access list for channel %2%."),
				 deleted % channel->Name());
	}
	else if (notfound)
	{
		if (skipped)
			SEND(service, user,
				  N_("No specified entries could be found (%1% had higher access and skipped) on the access list for channel %2%."),
				  skipped % channel->Name());
		else
			SEND(service, user,
				  N_("No specified entries could be found on the access list for channel %1%."),
				  channel->Name());
	}
	else
	{
		SEND(service, user,
			  N_("All specified entries had higher access and were skipped on the access list for channel %1%."),
			  channel->Name());
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

	boost::shared_ptr<StoredChannel> channel = ROOT->data.Get_StoredChannel(params[1]);
	if (!channel)
	{
		SEND(service, user, N_("Channel %1% is not registered."), params[1]);
		MT_RET(false);
	}

	if (!channel->ACCESS_Matches(user, StoredChannel::Level::LVL_Access))
	{
		SEND(service, user, N_("You do not have sufficient access to view the access list for channel %1%."),
			 channel->Name());
		MT_RET(false);
	}

	std::set<StoredChannel::Access> acc;
	if (params.size() > 2)
	{
		static boost::regex nick_rx("^[[:alpha:]\\x5B-\\x60\\x7B-\\x7D][-[:alnum:]\\x5B-\\x60\\x7B-\\x7D]*$");
		static boost::regex committee_rx("^@[[:print:]]+$");
		static boost::regex mask_rx("^([[:alpha:]\\x5B-\\x60\\x7B-\\x7D*?][-[:alnum:]\\x5B-\\x60\\x7B-\\x7D*?]*!)?"
									"[^[:space:][:cntrl:]@]+@(([[:alnum:]*?]([-[:alnum:]*?]{0,61}[[:alnum:]*?])?\\.)*"
									"[[:alnum:]*?]([-[:alnum:]*?]{0,61}[[:alnum:]*?])?|[[:xdigit:]:*?]*)$");
		for (size_t i = 2; i < params.size(); ++i)
		{
			if (boost::regex_match(params[i], nick_rx))
			{
				boost::shared_ptr<StoredNick> target = ROOT->data.Get_StoredNick(params[i]);
				if (!target)
					continue;

				StoredChannel::Access tmp(channel->ACCESS_Get(target->User()));
				if (tmp.Number())
					acc.insert(tmp);
			}
			else if (boost::regex_match(params[i], committee_rx))
			{
				boost::shared_ptr<Committee> target = ROOT->data.Get_Committee(params[i].substr(1));
				if (!target)
					continue;

				StoredChannel::Access tmp(channel->ACCESS_Get(target));
				if (tmp.Number())
					acc.insert(tmp);
			}
			else if (boost::regex_match(params[i], mask_rx))
			{
				std::string entry;
				if (params[i].find('!') == std::string::npos)
					entry = "*!" + params[i];
				else
					entry = params[i];

				std::list<StoredChannel::Access> tmp(channel->ACCESS_Matches(
						mantra::make_globrx(entry, boost::regex_constants::normal |
													   boost::regex_constants::icase)));
				if (!tmp.empty())
					acc.insert(tmp.begin(), tmp.end());
			}
			else
			{
				std::vector<unsigned int> nums;
				if (mantra::ParseNumbers(params[i], nums))
				{
					std::vector<unsigned int>::iterator j;
					for (j = nums.begin(); j != nums.end(); ++j)
					{
						StoredChannel::Access tmp = channel->ACCESS_Get(*j);
						if (tmp.Number())
							acc.insert(tmp);
					}
				}
			}
		}
	}
	else
	{
		channel->ACCESS_Get(acc);
	}

	if (acc.empty())
	{
		if (params.size() > 2)
		{
			SEND(service, user, N_("No entries matching your specification on the access list for channel %1%."),
				 channel->Name());
			MT_RET(false);
		}
		else
		{
			SEND(service, user, N_("The access list for channel %1% is empty."),
				 channel->Name());
			MT_RET(false);
		}
	}
	else
	{
		NSEND(service, user, N_("     SPECIFICATION                                      LEVEL"));
		std::set<StoredChannel::Access>::iterator i;
		for (i = acc.begin(); i != acc.end(); ++i)
		{
			if (!i->Number())
				continue;

			if (i->User())
			{
				StoredUser::my_nicks_t nicks = i->User()->Nicks();
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

				SEND(service, user, N_("%1$ 3d. %2$-50s %3$ 2d (last modified by %4$s %5$s ago)"),
					 i->Number() % str % i->Level() % i->Last_UpdaterName() %
					 DurationToString(mantra::duration(i->Last_Update(),
													   mantra::GetCurrentDateTime()), mantra::Second));
			}
			else if (i->GetCommittee())
			{
				SEND(service, user, N_("%1$ 3d. %2$-50s %3$ 2d (last modified by %4$s %5$s ago)"),
					 i->Number() % ('@' + i->GetCommittee()->Name()) % i->Level() % i->Last_UpdaterName() %
					 DurationToString(mantra::duration(i->Last_Update(),
													   mantra::GetCurrentDateTime()), mantra::Second));
			}
			else
			{
				SEND(service, user, N_("%1$ 3d. %2$-50s %3$ 2d (last modified by %4$s %5$s ago)"),
					 i->Number() % i->Mask() % i->Level() % i->Last_UpdaterName() %
					 DurationToString(mantra::duration(i->Last_Update(),
													   mantra::GetCurrentDateTime()), mantra::Second));
			}
		}
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

	boost::shared_ptr<StoredChannel> channel = ROOT->data.Get_StoredChannel(params[1]);
	if (!channel)
	{
		SEND(service, user, N_("Channel %1% is not registered."), params[1]);
		MT_RET(false);
	}

	if (!channel->ACCESS_Matches(user, StoredChannel::Level::LVL_Access))
	{
		SEND(service, user, N_("You do not have sufficient access to modify the access list for channel %1%."),
			 channel->Name());
		MT_RET(false);
	}

	std::set<StoredChannel::Access> acc;
	channel->ACCESS_Get(acc);

	if (acc.empty())
	{
		SEND(service, user, N_("The access list for channel %1% is empty."),
			 channel->Name());
		MT_RET(false);
	}

	size_t count = 0, changed = 0;
	std::set<StoredChannel::Access>::iterator i;
	for (i = acc.begin(); i != acc.end(); ++i)
	{
		if (i->Number() != ++count)
		{
			channel->ACCESS_Reindex(i->Number(), count);
			++changed;
		}
	}

	SEND(service, user, N_("Reindexing of access list for channel %1% complete, \002%2%\017 entries changed."),
		 channel->Name() % changed);

	MT_RET(true);
	MT_EE
}

static bool biAKICK_ADD(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biAKICK_ADD" << service << user << params);

	boost::shared_ptr<StoredNick> nick = ROOT->data.Get_StoredNick(user->Name());
	if (!nick)
	{
		NSEND(service, user, N_("Your nickname is not registered."));
		MT_RET(false);
	}

	boost::shared_ptr<StoredChannel> channel = ROOT->data.Get_StoredChannel(params[1]);
	if (!channel)
	{
		SEND(service, user, N_("Channel %1% is not registered."), params[1]);
		MT_RET(false);
	}

	boost::int32_t level = channel->ACCESS_Max(user);
	if (level < channel->LEVEL_Get(StoredChannel::Level::LVL_AutoKick).Value())
	{
		SEND(service, user, N_("You do not have sufficient access to modify the access list for channel %1%."),
			 channel->Name());
		MT_RET(false);
	}

	mantra::duration length("");

	size_t offs = 3;
	if (params[offs][0] == '+')
	{
		try
		{
			mantra::StringToDuration(params[3].substr(1), length);
		}
		catch (const mantra::mdatetime_format &e)
		{
			NSEND(service, user, N_("Invalid AKILL length specified."));
			MT_RET(false);
		}
		++offs;
	}
	/* The following when I have some limits ...
	else
	{
	}
	*/

	if (offs >= params.size())
	{
		SEND(service, user,
			 N_("Insufficient parameters for %1% command."),
			 boost::algorithm::to_upper_copy(params[0]));
		MT_RET(false);
	}

	std::string reason(params[offs]);
	for (++offs; offs < params.size(); ++offs)
		reason += ' ' + params[offs];

	static boost::regex nick_rx("^[[:alpha:]\\x5B-\\x60\\x7B-\\x7D][-[:alnum:]\\x5B-\\x60\\x7B-\\x7D]*$");
	static boost::regex committee_rx("^@[[:print:]]+$");
	static boost::regex mask_rx("^([[:alpha:]\\x5B-\\x60\\x7B-\\x7D*?][-[:alnum:]\\x5B-\\x60\\x7B-\\x7D*?]*!)?"
								"[^[:space:][:cntrl:]@]+@(([[:alnum:]*?]([-[:alnum:]*?]{0,61}[[:alnum:]*?])?\\.)*"
								"[[:alnum:]*?]([-[:alnum:]*?]{0,61}[[:alnum:]*?])?|[[:xdigit:]:*?]*)$");
	if (boost::regex_match(params[2], nick_rx))
	{
		boost::shared_ptr<StoredNick> target = ROOT->data.Get_StoredNick(params[2]);
		if (!target)
		{
			SEND(service, user, N_("Nickname %1% is not registered."), params[2]);	
			MT_RET(false);
		}

		StoredChannel::Access acc(channel->ACCESS_Get(target->User()));
		if (acc.Number() && acc.Level() >= level)
		{
			SEND(service, user, N_("Nickname %1% is higher than you or equal to you on the channel %2% access list."),
				 target->Name() % channel->Name());
			MT_RET(false);
		}

		StoredChannel::AutoKick akick(channel->AKICK_Get(target->User()));
		if (akick.Number())
		{
			akick.ChangeReason(reason, nick);

			mantra::duration oldlength = akick.Length();
			if (length)
			{
				if (oldlength)
				{
					oldlength = mantra::duration(akick.Creation(),
												 mantra::GetCurrentDateTime()) + length;
					akick.ChangeLength(oldlength, nick);
				}
				else
					akick.ChangeLength(length, nick);

				SEND(service, user, N_("Nickname %1% has had their akick extended by %2% on channel %3%."),
					 target->Name() % mantra::DurationToString(length, mantra::Second) % channel->Name());
			}
			else if (oldlength)
			{
				// Make it permanent ...
				akick.ChangeLength(length, nick);
				SEND(service, user, N_("Nickname %1% has had their akick made permanent on channel %3%."),
					 target->Name() % channel->Name());
			}
			else
			{
				SEND(service, user, N_("Nickname %1% has had their akick reason changed on channel %3%."),
					 target->Name() % channel->Name());
			}
		}
		else
		{
			StoredChannel::AutoKick akick2(channel->AKICK_Add(target->User(), reason, length, nick));
			if (!akick2.Number())
			{
				SEND(service, user, N_("Could not add nickname %1% to channel %2% akick list."),
					 target->Name() % channel->Name());
				MT_RET(false);
			}

			if (length)
				SEND(service, user, N_("Nickname %1% has been added to the akick list of channel %2% for %3%."),
					 target->Name() % channel->Name() % mantra::DurationToString(length, mantra::Second));
			else
				SEND(service, user, N_("Nickname %1% has been added to the akick list of channel %2% permanently."),
					 target->Name() % channel->Name());
		}
	}
	else if (boost::regex_match(params[2], committee_rx))
	{
		boost::shared_ptr<Committee> target = ROOT->data.Get_Committee(params[2].substr(1));
		if (!target)
		{
			SEND(service, user, N_("Committee %1% is not registered."), params[2].substr(1));	
			MT_RET(false);
		}

		StoredChannel::Access acc(channel->ACCESS_Get(target));
		if (acc.Number() && acc.Level() >= level)
		{
			SEND(service, user, N_("Committee %1% is higher than you or equal to you on the channel %2% access list."),
				 target->Name() % channel->Name());
			MT_RET(false);
		}

		StoredChannel::AutoKick akick(channel->AKICK_Get(target));
		if (akick.Number())
		{
			akick.ChangeReason(reason, nick);

			mantra::duration oldlength = akick.Length();
			if (length)
			{
				if (oldlength)
				{
					oldlength = mantra::duration(akick.Creation(),
												 mantra::GetCurrentDateTime()) + length;
					akick.ChangeLength(oldlength, nick);
				}
				else
					akick.ChangeLength(length, nick);

				SEND(service, user, N_("Committee %1% has had their akick extended by %2% on channel %3%."),
					 target->Name() % mantra::DurationToString(length, mantra::Second) % channel->Name());
			}
			else if (oldlength)
			{
				// Make it permanent ...
				akick.ChangeLength(length, nick);
				SEND(service, user, N_("Committee %1% has had their akick made permanent on channel %3%."),
					 target->Name() % channel->Name());
			}
			else
			{
				SEND(service, user, N_("Committee %1% has had their akick reason changed on channel %3%."),
					 target->Name() % channel->Name());
			}
		}
		else
		{
			StoredChannel::AutoKick akick2(channel->AKICK_Add(target, reason, length, nick));
			if (!akick2.Number())
			{
				SEND(service, user, N_("Could not add committee %1% to channel %2% akick list."),
					 target->Name() % channel->Name());
				MT_RET(false);
			}

			if (length)
				SEND(service, user, N_("Committee %1% has been added to the akick list of channel %2% for %3%."),
					 target->Name() % channel->Name() % mantra::DurationToString(length, mantra::Second));
			else
				SEND(service, user, N_("Committee %1% has been added to the akick list of channel %2% permanently."),
					 target->Name() % channel->Name());
		}
	}
	else if (boost::regex_match(params[2], mask_rx))
	{
		std::string entry;
		if (params[2].find('!') == std::string::npos)
			entry = "*!" + params[2];
		else
			entry = params[2];

		std::list<StoredChannel::Access> acc(channel->ACCESS_Matches(
				mantra::make_globrx(entry, boost::regex_constants::normal |
											   boost::regex_constants::icase)));
		if (!acc.empty() && acc.front().Level() >= level)
		{
				SEND(service, user, N_("Mask %1% is higher than you or equal to you on the channel %2% access list."),
					 acc.front().Mask() % channel->Name());
				MT_RET(false);
		}

		StoredChannel::AutoKick akick(channel->AKICK_Get(entry));
		if (akick.Number())
		{
			akick.ChangeReason(reason, nick);

			mantra::duration oldlength = akick.Length();
			if (length)
			{
				if (oldlength)
				{
					oldlength = mantra::duration(akick.Creation(),
												 mantra::GetCurrentDateTime()) + length;
					akick.ChangeLength(oldlength, nick);
				}
				else
					akick.ChangeLength(length, nick);

				SEND(service, user, N_("Mask %1% has had their akick extended by %2% on channel %3%."),
					 entry % mantra::DurationToString(length, mantra::Second) % channel->Name());
			}
			else if (oldlength)
			{
				// Make it permanent ...
				akick.ChangeLength(length, nick);
				SEND(service, user, N_("Mask %1% has had their akick made permanent on channel %3%."),
					 entry % channel->Name());
			}
			else
			{
				SEND(service, user, N_("Mask %1% has had their akick reason changed on channel %3%."),
					 entry % channel->Name());
			}
		}
		else
		{
			StoredChannel::AutoKick akick2(channel->AKICK_Add(entry, reason, length, nick));
			if (!akick2.Number())
			{
				SEND(service, user, N_("Could not add mask %1% to channel %2% akick list."),
					 entry % channel->Name());
				MT_RET(false);
			}

			if (length)
				SEND(service, user, N_("Mask %1% has been added to the akick list of channel %2% for %3%."),
					 entry % channel->Name() % mantra::DurationToString(length, mantra::Second));
			else
				SEND(service, user, N_("Mask %1% has been added to the akick list of channel %2% permanently."),
					 entry % channel->Name());
		}
	}
	else
	{
		try
		{
			boost::uint32_t entry = boost::lexical_cast<boost::uint32_t>(params[2]);

			StoredChannel::AutoKick akick(channel->AKICK_Get(entry));
			if (akick.Number())
			{
				akick.ChangeReason(reason, nick);

				mantra::duration oldlength = akick.Length();
				if (length)
				{
					if (oldlength)
					{
						oldlength = mantra::duration(akick.Creation(),
													 mantra::GetCurrentDateTime()) + length;
						akick.ChangeLength(oldlength, nick);
					}
					else
						akick.ChangeLength(length, nick);

					SEND(service, user, N_("Entry #%1% has had their akick extended by %2% on channel %3%."),
						 entry % mantra::DurationToString(length, mantra::Second) % channel->Name());
				}
				else if (oldlength)
				{
					// Make it permanent ...
					akick.ChangeLength(length, nick);
					SEND(service, user, N_("Entry #%1% has had their akick made permanent on channel %3%."),
						 entry % channel->Name());
				}
				else
				{
					SEND(service, user, N_("Entry #%1% has had their akick reason changed on channel %3%."),
						 entry % channel->Name());
				}
			}
			else
			{
				SEND(service, user, N_("Could not find entry #%1% on akick list for channel %2%."),
					  entry % channel->Name());
				MT_RET(false);
			}
		}
		catch (const boost::bad_lexical_cast &e)
		{
			NSEND(service, user, N_("Invalid mask, nickname, committee or existing entry specified."));
			MT_RET(false);
		}
	}

	MT_RET(true);
	MT_EE
}

static bool biAKICK_DEL(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biAKICK_DEL" << service << user << params);

	boost::shared_ptr<StoredChannel> channel = ROOT->data.Get_StoredChannel(params[1]);
	if (!channel)
	{
		SEND(service, user, N_("Channel %1% is not registered."), params[1]);
		MT_RET(false);
	}

	if (!channel->ACCESS_Matches(user, StoredChannel::Level::LVL_AutoKick))
	{
		SEND(service, user, N_("You do not have sufficient access to view the akick list for channel %1%."),
			 channel->Name());
		MT_RET(false);
	}

	size_t deleted = 0, notfound = 0;

	static boost::regex nick_rx("^[[:alpha:]\\x5B-\\x60\\x7B-\\x7D][-[:alnum:]\\x5B-\\x60\\x7B-\\x7D]*$");
	static boost::regex committee_rx("^@[[:print:]]+$");
	static boost::regex mask_rx("^([[:alpha:]\\x5B-\\x60\\x7B-\\x7D*?][-[:alnum:]\\x5B-\\x60\\x7B-\\x7D*?]*!)?"
								"[^[:space:][:cntrl:]@]+@(([[:alnum:]*?]([-[:alnum:]*?]{0,61}[[:alnum:]*?])?\\.)*"
								"[[:alnum:]*?]([-[:alnum:]*?]{0,61}[[:alnum:]*?])?|[[:xdigit:]:*?]*)$");
	for (size_t i = 2; i < params.size(); ++i)
	{
		if (boost::regex_match(params[i], nick_rx))
		{
			boost::shared_ptr<StoredNick> target = ROOT->data.Get_StoredNick(params[i]);
			if (!target)
			{
				++notfound;
				continue;
			}

			StoredChannel::AutoKick akick(channel->AKICK_Get(target->User()));
			if (akick.Number())
			{
				channel->AKICK_Del(akick.Number());
				++deleted;
			}
			else
			{
				++notfound;
			}
		}
		else if (boost::regex_match(params[i], committee_rx))
		{
			boost::shared_ptr<Committee> target = ROOT->data.Get_Committee(params[i].substr(1));
			if (!target)
			{
				++notfound;
				continue;
			}

			StoredChannel::AutoKick akick(channel->AKICK_Get(target));
			if (akick.Number())
			{
				channel->AKICK_Del(akick.Number());
				++deleted;
			}
			else
			{
				++notfound;
			}
		}
		else if (boost::regex_match(params[i], mask_rx))
		{
			std::string entry;
			if (params[i].find('!') == std::string::npos)
				entry = "*!" + params[i];
			else
				entry = params[i];

			std::set<StoredChannel::AutoKick> akick(channel->AKICK_Matches(
					mantra::make_globrx(entry, boost::regex_constants::normal |
												   boost::regex_constants::icase)));
			if (!akick.empty())
			{
				std::set<StoredChannel::AutoKick>::iterator j;
				for (j = akick.begin(); j != akick.end(); ++j)
				{
					channel->AKICK_Del(j->Number());
					++deleted;
				}
			}
			else
			{
				++notfound;
			}
		}
		else
		{
			std::vector<unsigned int> nums;
			if (mantra::ParseNumbers(params[i], nums))
			{
				std::vector<unsigned int>::iterator j;
				for (j = nums.begin(); j != nums.end(); ++j)
				{
					StoredChannel::AutoKick akick = channel->AKICK_Get(*j);
					if (akick.Number())
					{
						channel->AKICK_Del(akick.Number());
						++deleted;
					}
					else
						++notfound;
				}
			}
			else
			{
				++notfound;
			}
		}
	}

	if (deleted)
	{
		SEND(service, user,
			 N_("%1% entries removed from the akick list for channel %2%."),
			 deleted % channel->Name());
	}
	else
	{
		SEND(service, user,
			  N_("No specified entries could be found on the akick list for channel %1%."),
			  channel->Name());
	}

	MT_RET(true);
	MT_EE
}

static bool biAKICK_LIST(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biAKICK_LIST" << service << user << params);

	boost::shared_ptr<StoredChannel> channel = ROOT->data.Get_StoredChannel(params[1]);
	if (!channel)
	{
		SEND(service, user, N_("Channel %1% is not registered."), params[1]);
		MT_RET(false);
	}

	if (!channel->ACCESS_Matches(user, StoredChannel::Level::LVL_AutoKick))
	{
		SEND(service, user, N_("You do not have sufficient access to view the akick list for channel %1%."),
			 channel->Name());
		MT_RET(false);
	}

	std::set<StoredChannel::AutoKick> akick;
	if (params.size() > 2)
	{
		static boost::regex nick_rx("^[[:alpha:]\\x5B-\\x60\\x7B-\\x7D][-[:alnum:]\\x5B-\\x60\\x7B-\\x7D]*$");
		static boost::regex committee_rx("^@[[:print:]]+$");
		static boost::regex mask_rx("^([[:alpha:]\\x5B-\\x60\\x7B-\\x7D*?][-[:alnum:]\\x5B-\\x60\\x7B-\\x7D*?]*!)?"
									"[^[:space:][:cntrl:]@]+@(([[:alnum:]*?]([-[:alnum:]*?]{0,61}[[:alnum:]*?])?\\.)*"
									"[[:alnum:]*?]([-[:alnum:]*?]{0,61}[[:alnum:]*?])?|[[:xdigit:]:*?]*)$");
		for (size_t i = 2; i < params.size(); ++i)
		{
			if (boost::regex_match(params[i], nick_rx))
			{
				boost::shared_ptr<StoredNick> target = ROOT->data.Get_StoredNick(params[i]);
				if (!target)
					continue;

				StoredChannel::AutoKick tmp(channel->AKICK_Get(target->User()));
				if (tmp.Number())
					akick.insert(tmp);
			}
			else if (boost::regex_match(params[i], committee_rx))
			{
				boost::shared_ptr<Committee> target = ROOT->data.Get_Committee(params[i].substr(1));
				if (!target)
					continue;

				StoredChannel::AutoKick tmp(channel->AKICK_Get(target));
				if (tmp.Number())
					akick.insert(tmp);
			}
			else if (boost::regex_match(params[i], mask_rx))
			{
				std::string entry;
				if (params[i].find('!') == std::string::npos)
					entry = "*!" + params[i];
				else
					entry = params[i];

				std::set<StoredChannel::AutoKick> tmp(channel->AKICK_Matches(
						mantra::make_globrx(entry, boost::regex_constants::normal |
													   boost::regex_constants::icase)));
				if (!tmp.empty())
					akick.insert(tmp.begin(), tmp.end());
			}
			else
			{
				std::vector<unsigned int> nums;
				if (mantra::ParseNumbers(params[i], nums))
				{
					std::vector<unsigned int>::iterator j;
					for (j = nums.begin(); j != nums.end(); ++j)
					{
						StoredChannel::AutoKick tmp = channel->AKICK_Get(*j);
						if (tmp.Number())
							akick.insert(tmp);
					}
				}
			}
		}
	}
	else
	{
		channel->AKICK_Get(akick);
	}

	if (akick.empty())
	{
		if (params.size() > 2)
		{
			SEND(service, user, N_("No entries matching your specification on the akick list for channel %1%."),
				 channel->Name());
			MT_RET(false);
		}
		else
		{
			SEND(service, user, N_("The akick list for channel %1% is empty."),
				 channel->Name());
			MT_RET(false);
		}
	}
	else
	{
//		NSEND(service, user, N_("     SPECIFICATION                                      LEVEL"));
		std::set<StoredChannel::AutoKick>::iterator i;
		for (i = akick.begin(); i != akick.end(); ++i)
		{
			if (!i->Number())
				continue;

			if (i->User())
			{
				StoredUser::my_nicks_t nicks = i->User()->Nicks();
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

				SEND(service, user, N_("%1$ 3d. %2$s (last modified by %3$s %4$s ago)"),
					 i->Number() % str % i->Last_UpdaterName() %
					 DurationToString(mantra::duration(i->Last_Update(),
													   mantra::GetCurrentDateTime()), mantra::Second));
			}
			else if (i->GetCommittee())
			{
				SEND(service, user, N_("%1$ 3d. %2$s (last modified by %3$s %4$s ago)"),
					 i->Number() % ('@' + i->GetCommittee()->Name()) % i->Last_UpdaterName() %
					 DurationToString(mantra::duration(i->Last_Update(),
													   mantra::GetCurrentDateTime()), mantra::Second));
			}
			else
			{
				SEND(service, user, N_("%1$ 3d. %2$s (last modified by %3$s %4$s ago)"),
					 i->Number() % i->Mask() % i->Last_UpdaterName() %
					 DurationToString(mantra::duration(i->Last_Update(),
													   mantra::GetCurrentDateTime()), mantra::Second));
			}

			if (i->Length())
				SEND(service, user, N_("     [%1% left of %2%] %3%"),
					 DurationToString(i->Length() - mantra::duration(i->Creation(),
									  mantra::GetCurrentDateTime()), mantra::Second) %
					 DurationToString(i->Length(), mantra::Second) % i->Reason());
			else
				SEND(service, user, N_("     [PERMANENT] %1%"), i->Reason());
		}
	}

	MT_RET(true);
	MT_EE
}

static bool biAKICK_REINDEX(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biAKICK_REINDEX" << service << user << params);

	boost::shared_ptr<StoredChannel> channel = ROOT->data.Get_StoredChannel(params[1]);
	if (!channel)
	{
		SEND(service, user, N_("Channel %1% is not registered."), params[1]);
		MT_RET(false);
	}

	if (!channel->ACCESS_Matches(user, StoredChannel::Level::LVL_AutoKick))
	{
		SEND(service, user, N_("You do not have sufficient access to modify the akick list for channel %1%."),
			 channel->Name());
		MT_RET(false);
	}

	std::set<StoredChannel::AutoKick> akick;
	channel->AKICK_Get(akick);

	if (akick.empty())
	{
		SEND(service, user, N_("The akick list for channel %1% is empty."),
			 channel->Name());
		MT_RET(false);
	}

	size_t count = 0, changed = 0;
	std::set<StoredChannel::AutoKick>::iterator i;
	for (i = akick.begin(); i != akick.end(); ++i)
	{
		if (i->Number() != ++count)
		{
			channel->AKICK_Reindex(i->Number(), count);
			++changed;
		}
	}

	SEND(service, user, N_("Reindexing of akick list for channel %1% complete, \002%2%\017 entries changed."),
		 channel->Name() % changed);

	MT_RET(true);
	MT_EE
}

static bool biGREET_SET(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biGREET_SET" << service << user << params);

	boost::shared_ptr<StoredNick> nick = ROOT->data.Get_StoredNick(user->Name());
	if (!nick)
	{
		NSEND(service, user, N_("Your nickname is not registered."));
		MT_RET(false);
	}

	boost::shared_ptr<StoredChannel> channel = ROOT->data.Get_StoredChannel(params[1]);
	if (!channel)
	{
		SEND(service, user, N_("Channel %1% is not registered."), params[1]);
		MT_RET(false);
	}

	if (!channel->ACCESS_Matches(user, StoredChannel::Level::LVL_Greet))
	{
		SEND(service, user, N_("You do not have sufficient access to set a greeting for channel %1%."),
			 channel->Name());
		MT_RET(false);
	}

	std::string greeting(params[2]);
	for (size_t i = 3; i < params.size(); ++i)
		greeting += ' ' + params[i];

	StoredChannel::Greet greet(channel->GREET_Get(nick->User()));
	if (greet.Greeting().empty())
	{
		StoredChannel::Greet greet2(channel->GREET_Add(nick->User(), greeting, nick));
		if (greet2.Greeting().empty())
		{
			SEND(service, user, N_("Could not add greeting to channel %1%!"),
				 channel->Name());
			MT_RET(false);
		}

		SEND(service, user, N_("Your greeting for channel %1% has been set."),
			 channel->Name());
	}
	else
	{
		if (greet.Locked() && !channel->ACCESS_Matches(user, StoredChannel::Level::LVL_OverGreet))
		{
			SEND(service, user, N_("Your greeting for channel %1% is locked and cannot be changed."),
				 channel->Name());
			MT_RET(false);
		}

		greet.Change(greeting, nick);
		SEND(service, user, N_("Your greeting for channel %1% has been changed."),
			 channel->Name());
	}

	MT_RET(true);
	MT_EE
}

static bool biGREET_UNSET(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biGREET_UNSET" << service << user << params);

	boost::shared_ptr<StoredChannel> channel = ROOT->data.Get_StoredChannel(params[1]);
	if (!channel)
	{
		SEND(service, user, N_("Channel %1% is not registered."), params[1]);
		MT_RET(false);
	}

	if (params.size() > 2)
	{
		if (!channel->ACCESS_Matches(user, StoredChannel::Level::LVL_OverGreet))
		{
			SEND(service, user, N_("You do not have sufficient access to set other peoples greetings for channel %1%."),
				 channel->Name());
			MT_RET(false);
		}

		size_t deleted = 0, skipped = 0;
		for (size_t i = 2; i != params.size(); ++i)
		{
			boost::shared_ptr<StoredNick> nick = ROOT->data.Get_StoredNick(params[i]);
			if (!nick)
			{
				++skipped;
				continue;
			}

			StoredChannel::Greet greet(channel->GREET_Get(nick->User()));
			if (greet.Greeting().empty())
			{
				++skipped;
				continue;
			}

			channel->GREET_Del(nick->User());
			++deleted;
		}
		if (!skipped)
			SEND(service, user, N_("%1% greetings removed from channel %2%."),
				 deleted % channel->Name());
		else if (deleted)
			SEND(service, user, N_("%1% greetings removed (%2% not found) from channel %3%."),
				 deleted % skipped % channel->Name());
		else
			SEND(service, user, N_("Could not find any specified greetings on channel %1%."),
				 channel->Name());
	}
	else
	{
		boost::shared_ptr<StoredNick> nick = ROOT->data.Get_StoredNick(user->Name());
		if (!nick)
		{
			NSEND(service, user, N_("Your nickname is not registered."));
			MT_RET(false);
		}

		if (!channel->ACCESS_Matches(user, StoredChannel::Level::LVL_Greet))
		{
			SEND(service, user, N_("You do not have sufficient access to set a greeting for channel %1%."),
				 channel->Name());
			MT_RET(false);
		}

		StoredChannel::Greet greet(channel->GREET_Get(nick->User()));
		if (greet.Greeting().empty())
		{
			SEND(service, user, N_("You did not have a greeting set for channel %1%."),
				 channel->Name());
			MT_RET(false);
		}

		channel->GREET_Del(nick->User());
		SEND(service, user, N_("Your greeting for channel %1% has been removed."),
			 channel->Name());
	}

	MT_RET(true);
	MT_EE
}

static bool biGREET_LOCK(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biGREET_LOCK" << service << user << params);

	boost::shared_ptr<StoredChannel> channel = ROOT->data.Get_StoredChannel(params[1]);
	if (!channel)
	{
		SEND(service, user, N_("Channel %1% is not registered."), params[1]);
		MT_RET(false);
	}

	if (!channel->ACCESS_Matches(user, StoredChannel::Level::LVL_OverGreet))
	{
		SEND(service, user, N_("You do not have sufficient access to set another person's greeting for channel %1%."),
			 channel->Name());
		MT_RET(false);
	}

	boost::shared_ptr<StoredNick> nick = ROOT->data.Get_StoredNick(params[2]);
	if (!nick)
	{
		SEND(service, user, N_("Nickname %1% is not registered."), params[2]);
		MT_RET(false);
	}

	std::string greeting(params[3]);
	for (size_t i = 4; i < params.size(); ++i)
		greeting += ' ' + params[i];

	StoredChannel::Greet greet(channel->GREET_Get(nick->User()));
	if (greet.Greeting().empty())
	{
		StoredChannel::Greet greet2(channel->GREET_Add(nick->User(), greeting, nick));
		if (greet2.Greeting().empty())
		{
			SEND(service, user, N_("Could not add greeting for %1% to channel %2%!"),
				 nick->Name() % channel->Name());
			MT_RET(false);
		}

		greet2.Locked(true);
		SEND(service, user, N_("Greeting for nickname %1% in channel %2% has been set."),
			 nick->Name() % channel->Name());
	}
	else
	{
		greet.Change(greeting, nick);
		if (!greet.Locked())
			greet.Locked(true);
		SEND(service, user, N_("Greeting for nickname %1% in channel %2% has been changed."),
			 nick->Name() % channel->Name());
	}

	MT_RET(true);
	MT_EE
}

static bool biGREET_UNLOCK(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biGREET_UNLOCK" << service << user << params);

	boost::shared_ptr<StoredChannel> channel = ROOT->data.Get_StoredChannel(params[1]);
	if (!channel)
	{
		SEND(service, user, N_("Channel %1% is not registered."), params[1]);
		MT_RET(false);
	}

	if (!channel->ACCESS_Matches(user, StoredChannel::Level::LVL_OverGreet))
	{
		SEND(service, user, N_("You do not have sufficient access to set other peoples greetings for channel %1%."),
			 channel->Name());
		MT_RET(false);
	}

	size_t unlocked = 0, skipped = 0;
	for (size_t i = 2; i != params.size(); ++i)
	{
		boost::shared_ptr<StoredNick> nick = ROOT->data.Get_StoredNick(params[i]);
		if (!nick)
		{
			++skipped;
			continue;
		}

		StoredChannel::Greet greet(channel->GREET_Get(nick->User()));
		if (greet.Greeting().empty())
		{
			++skipped;
			continue;
		}

		greet.Locked(false);
		++unlocked;
	}
	if (!skipped)
		SEND(service, user, N_("%1% greetings unlocked in channel %2%."),
			 unlocked % channel->Name());
	else if (unlocked)
		SEND(service, user, N_("%1% greetings unlocked (%2% not found) in channel %3%."),
			 unlocked % skipped % channel->Name());
	else
		SEND(service, user, N_("Could not find any specified greetings on channel %1%."),
			 channel->Name());

	MT_RET(true);
	MT_EE
}

static bool biGREET_LIST(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biGREET_LIST" << service << user << params);

	boost::shared_ptr<StoredChannel> channel = ROOT->data.Get_StoredChannel(params[1]);
	if (!channel)
	{
		SEND(service, user, N_("Channel %1% is not registered."), params[1]);
		MT_RET(false);
	}

	if (params.size() > 2)
	{
		if (!channel->ACCESS_Matches(user, StoredChannel::Level::LVL_OverGreet))
		{
			SEND(service, user, N_("You do not have sufficient access to set other peoples greetings for channel %1%."),
				 channel->Name());
			MT_RET(false);
		}

		static mantra::iless<std::string> cmp;
		if (cmp("ALL", params[2]))
		{
			std::set<StoredChannel::Greet> all;
			channel->GREET_Get(all);
			if (all.empty())
			{
				SEND(service, user, N_("No greetings are set for channel %1%."), channel->Name());
				MT_RET(true);
			}

			SEND(service, user, N_("All greeings for channel %1%:"), channel->Name());
			std::set<StoredChannel::Greet>::iterator i;
			for (i = all.begin(); i != all.end(); ++i)
			{
				StoredUser::my_nicks_t nicks = i->Entry()->Nicks();
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

				SEND(service, user, N_("[%1%] %2% (last modified by %3% %4% ago)"), 
					 str % i->Greeting() % i->Last_UpdaterName() %
					 DurationToString(mantra::duration(i->Last_Update(),
									  mantra::GetCurrentDateTime()), mantra::Second));
			}
		}
		else
		{
			bool found = false;
			for (size_t i = 2; i != params.size(); ++i)
			{
				boost::shared_ptr<StoredNick> nick = ROOT->data.Get_StoredNick(params[i]);
				if (!nick)
					continue;

				StoredChannel::Greet greet(channel->GREET_Get(nick->User()));
				if (greet.Greeting().empty())
					continue;

				StoredUser::my_nicks_t nicks = nick->User()->Nicks();
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

				if (str.empty())
					continue;

				if (!found)
				{
					SEND(service, user, N_("Greeings for channel %1%:"), channel->Name());
					found = true;
				}

				SEND(service, user, N_("[%1%] %2% (last modified by %3% %4% ago)"), 
					 str % greet.Greeting() % greet.Last_UpdaterName() %
					 DurationToString(mantra::duration(greet.Last_Update(),
									  mantra::GetCurrentDateTime()), mantra::Second));
			}
			if (!found)
				SEND(service, user, N_("None of the specified greetings were set on channel %1%."), channel->Name());
		}
	}
	else
	{
		boost::shared_ptr<StoredNick> nick = ROOT->data.Get_StoredNick(user->Name());
		if (!nick)
		{
			NSEND(service, user, N_("Your nickname is not registered."));
			MT_RET(false);
		}

		if (!channel->ACCESS_Matches(user, StoredChannel::Level::LVL_Greet))
		{
			SEND(service, user, N_("You do not have sufficient access to set a greeting for channel %1%."),
				 channel->Name());
			MT_RET(false);
		}

		StoredChannel::Greet greet(channel->GREET_Get(nick->User()));
		if (greet.Greeting().empty())
		{
			SEND(service, user, N_("You did not have a greeting set for channel %1%."),
				 channel->Name());
			MT_RET(false);
		}

		if (greet.Locked())
			SEND(service, user, "[\002%1%\017] %2%", channel->Name() % greet.Greeting());
		else
			SEND(service, user, "[%1%] %2%", channel->Name() % greet.Greeting());
	}

	MT_RET(true);
	MT_EE
}

static bool biMESSAGE_ADD(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biMESSAGE_ADD" << service << user << params);

	boost::shared_ptr<StoredNick> nick = ROOT->data.Get_StoredNick(user->Name());
	if (!nick)
	{
		NSEND(service, user, N_("Your nickname is not registered."));
		MT_RET(false);
	}

	boost::shared_ptr<StoredChannel> channel = ROOT->data.Get_StoredChannel(params[1]);
	if (!channel)
	{
		SEND(service, user, N_("Channel %1% is not registered."), params[1]);
		MT_RET(false);
	}

	if (!channel->ACCESS_Matches(user, StoredChannel::Level::LVL_Message))
	{
		SEND(service, user, N_("You do not have sufficient access to add an on-join message for channel %1%."),
			 channel->Name());
		MT_RET(false);
	}

	std::string msg(params[2]);
	for (size_t i = 3; i < params.size(); ++i)
		msg += ' ' + params[i];

	StoredChannel::Message message(channel->MESSAGE_Add(msg, nick));
	if (!message.Number())
	{
		SEND(service, user, N_("Could not add on-join message for channel %1%."), channel->Name());
		MT_RET(false);
	}

	SEND(service, user, N_("Added on-join message #%1% to channel %1%."), message.Number() % channel->Name());

	MT_RET(true);
	MT_EE
}

static bool biMESSAGE_DEL(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biMESSAGE_DEL" << service << user << params);

	boost::shared_ptr<StoredChannel> channel = ROOT->data.Get_StoredChannel(params[1]);
	if (!channel)
	{
		SEND(service, user, N_("Channel %1% is not registered."), params[1]);
		MT_RET(false);
	}

	if (!channel->ACCESS_Matches(user, StoredChannel::Level::LVL_Message))
	{
		SEND(service, user, N_("You do not have sufficient access to remove an on-join message for channel %1%."),
			 channel->Name());
		MT_RET(false);
	}

	std::string str(params[2]);
	for (size_t i = 3; i < params.size(); ++i)
		str += ' ' + params[i];

	std::vector<unsigned int> nums;
	if (!mantra::ParseNumbers(str, nums))
	{
		SEND(service, user, N_("You may only specify message numbers as parameters to the %1% command."),
			 boost::algorithm::to_upper_copy(params[0]));
		MT_RET(false);
	}

	size_t deleted = 0, skipped = 0;
	std::vector<unsigned int>::iterator j;
	for (j = nums.begin(); j != nums.end(); ++j)
	{
		StoredChannel::Message message = channel->MESSAGE_Get(*j);
		if (message.Number())
		{
			channel->MESSAGE_Del(message.Number());
			++deleted;
		}
		else
			++skipped;
	}

	if (!skipped)
		SEND(service, user, N_("%1% on-join messages removed from channel %2%."),
			 deleted % channel->Name());
	else if (deleted)
		SEND(service, user, N_("%1% on-join messages removed (%2% not found) from channel %3%."),
			 deleted % skipped % channel->Name());
	else
		SEND(service, user, N_("Could not find any specified on-join messages on channel %1%."),
			 channel->Name());

	MT_RET(true);
	MT_EE
}

static bool biMESSAGE_LIST(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biMESSAGE_LIST" << service << user << params);

	boost::shared_ptr<StoredChannel> channel = ROOT->data.Get_StoredChannel(params[1]);
	if (!channel)
	{
		SEND(service, user, N_("Channel %1% is not registered."), params[1]);
		MT_RET(false);
	}

	if (!channel->ACCESS_Matches(user, StoredChannel::Level::LVL_Message))
	{
		SEND(service, user, N_("You do not have sufficient access to list on-join messages for channel %1%."),
			 channel->Name());
		MT_RET(false);
	}

	if (params.size() > 2)
	{
		std::string str(params[2]);
		for (size_t i = 3; i < params.size(); ++i)
			str += ' ' + params[i];

		std::vector<unsigned int> nums;
		if (!mantra::ParseNumbers(str, nums))
		{
			SEND(service, user, N_("You may only specify message numbers as parameters to the %1% command."),
				 boost::algorithm::to_upper_copy(params[0]));
			MT_RET(false);
		}

		bool found = false;
		std::vector<unsigned int>::iterator j;
		for (j = nums.begin(); j != nums.end(); ++j)
		{
			StoredChannel::Message message = channel->MESSAGE_Get(*j);
			if (message.Number())
			{
				if (!found)
				{
					SEND(service, user, N_("On-Join mesages for channel %1%"), channel->Name());
					found = true;
				}

				SEND(service, user, N_("%1%. %2% (set by %1% %2% ago)"),
					 message.Number() % message.Entry() % message.Last_UpdaterName() %
					 DurationToString(mantra::duration(message.Last_Update(),
									  mantra::GetCurrentDateTime()), mantra::Second));
			}
		}
		if (!found)
		{
			SEND(service, user, N_("None of the specified on-join messages were found for channel %1%."), channel->Name());
		}
	}
	else
	{
		std::set<StoredChannel::Message> msgs;
		channel->MESSAGE_Get(msgs);

		if (msgs.empty())
		{
			SEND(service, user, N_("No on-join messages are set for channel %1%."), channel->Name());
			MT_RET(true);
		}

		SEND(service, user, N_("All on-join mesages for channel %1%"), channel->Name());
		std::set<StoredChannel::Message>::iterator i;
		for (i = msgs.begin(); i != msgs.end(); ++i)
		{
			if (!i->Number())
				continue;

			SEND(service, user, N_("%1%. %2% (set by %1% %2% ago)"),
				 i->Number() % i->Entry() % i->Last_UpdaterName() %
				 DurationToString(mantra::duration(i->Last_Update(),
								  mantra::GetCurrentDateTime()), mantra::Second));
		}
	}

	MT_RET(true);
	MT_EE
}

static bool biMESSAGE_REINDEX(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biMESSAGE_REINDEX" << service << user << params);

	boost::shared_ptr<StoredChannel> channel = ROOT->data.Get_StoredChannel(params[1]);
	if (!channel)
	{
		SEND(service, user, N_("Channel %1% is not registered."), params[1]);
		MT_RET(false);
	}

	if (!channel->ACCESS_Matches(user, StoredChannel::Level::LVL_Message))
	{
		SEND(service, user, N_("You do not have sufficient access to modify the on-join messages for channel %1%."),
			 channel->Name());
		MT_RET(false);
	}

	std::set<StoredChannel::Message> msgs;
	channel->MESSAGE_Get(msgs);

	if (msgs.empty())
	{
		SEND(service, user, N_("There are no on-join messages for channel %1%."),
			 channel->Name());
		MT_RET(false);
	}

	size_t count = 0, changed = 0;
	std::set<StoredChannel::Message>::iterator i;
	for (i = msgs.begin(); i != msgs.end(); ++i)
	{
		if (i->Number() != ++count)
		{
			channel->MESSAGE_Reindex(i->Number(), count);
			++changed;
		}
	}

	SEND(service, user, N_("Reindexing of on-join messages for channel %1% complete, \002%2%\017 entries changed."),
		 channel->Name() % changed);

	MT_RET(true);
	MT_EE
}

static bool biNEWS_ADD(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biNEWS_ADD" << service << user << params);

	// TODO: To be implemented.
	SEND(service, user,
		 N_("The %1% command has not yet been implemented."),
		 boost::algorithm::to_upper_copy(params[0]));

	MT_RET(false);
	MT_EE
}

static bool biNEWS_DEL(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biNEWS_DEL" << service << user << params);

	// TODO: To be implemented.
	SEND(service, user,
		 N_("The %1% command has not yet been implemented."),
		 boost::algorithm::to_upper_copy(params[0]));

	MT_RET(false);
	MT_EE
}

static bool biNEWS_LIST(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biNEWS_LIST" << service << user << params);

	// TODO: To be implemented.
	SEND(service, user,
		 N_("The %1% command has not yet been implemented."),
		 boost::algorithm::to_upper_copy(params[0]));

	MT_RET(false);
	MT_EE
}

static bool biNEWS_REINDEX(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biNEWS_REINDEX" << service << user << params);

	// TODO: To be implemented.
	SEND(service, user,
		 N_("The %1% command has not yet been implemented."),
		 boost::algorithm::to_upper_copy(params[0]));

	MT_RET(false);
	MT_EE
}

static bool biSET_FOUNDER(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biSET_FOUNDER" << service << user << params);

	boost::shared_ptr<StoredChannel> channel = ROOT->data.Get_StoredChannel(params[1]);
	if (!channel)
	{
		SEND(service, user, N_("Channel %1% is not registered."), params[1]);
		MT_RET(false);
	}

	if (!user->Identified(channel))
	{
		SEND(service, user, N_("You must identify to the channel to execute the %1% command."),
			 boost::algorithm::to_upper_copy(params[0]));
		MT_RET(false);
	}

	boost::shared_ptr<StoredNick> nick = ROOT->data.Get_StoredNick(params[2]);
	if (!nick)
	{
		SEND(service, user, N_("Nickname %1% is not registered."), params[2]);
		MT_RET(false);
	}

	if (channel->Founder() == nick->User())
	{
		SEND(service, user, N_("Nickname %1% is already the founder of channel %2%."),
			 nick->Name() % channel->Name());
		MT_RET(false);
	}

	channel->Founder(nick->User());
	SEND(service, user, N_("Founder of for channel %1% has been set to \002%2%\017."),
		 channel->Name() % nick->Name());

	MT_RET(true);
	MT_EE
}

static bool biSET_COFOUNDER(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biSET_COFOUNDER" << service << user << params);

	boost::shared_ptr<StoredChannel> channel = ROOT->data.Get_StoredChannel(params[1]);
	if (!channel)
	{
		SEND(service, user, N_("Channel %1% is not registered."), params[1]);
		MT_RET(false);
	}

	if (!user->Identified(channel))
	{
		SEND(service, user, N_("You must identify to the channel to execute the %1% command."),
			 boost::algorithm::to_upper_copy(params[0]));
		MT_RET(false);
	}

	boost::shared_ptr<StoredNick> nick = ROOT->data.Get_StoredNick(params[2]);
	if (!nick)
	{
		SEND(service, user, N_("Nickname %1% is not registered."), params[2]);
		MT_RET(false);
	}

	if (channel->Founder() == nick->User())
	{
		SEND(service, user, N_("Nickname %1% is the founder of channel %2% and as such cannot also be its successor."),
			 nick->Name() % channel->Name());
		MT_RET(false);
	}

	if (channel->Successor() == nick->User())
	{
		SEND(service, user, N_("Nickname %1% is already the successor of channel %2%."),
			 nick->Name() % channel->Name());
		MT_RET(false);
	}

	channel->Successor(nick->User());
	SEND(service, user, N_("Successor of for channel %1% has been set to \002%2%\017."),
		 channel->Name() % nick->Name());

	MT_RET(true);
	MT_EE
}

static bool biSET_PASSWORD(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biSET_PASSWORD" << service << user << params);

	boost::shared_ptr<StoredChannel> channel = ROOT->data.Get_StoredChannel(params[1]);
	if (!channel)
	{
		SEND(service, user, N_("Channel %1% is not registered."), params[1]);
		MT_RET(false);
	}

	if (!user->Identified(channel))
	{
		SEND(service, user, N_("You must identify to the channel to execute the %1% command."),
			 boost::algorithm::to_upper_copy(params[0]));
		MT_RET(false);
	}

	static mantra::iequal_to<std::string> cmp;
	if (params[2].size() < 5 || cmp(params[2], user->Name()) ||
		cmp(params[1], params[2]))
	{
		NSEND(service, user, N_("Password is not complex enough."));
		MT_RET(false);
	}

	channel->Password(params[2]);
	SEND(service, user, N_("Password for channel %1% has been set to \002%2%\017."),
		 channel->Name() % params[2].substr(0, 32));

	MT_RET(true);
	MT_EE
}

static bool biSET_EMAIL(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biSET_EMAIL" << service << user << params);

	boost::shared_ptr<StoredChannel> channel = ROOT->data.Get_StoredChannel(params[1]);
	if (!channel)
	{
		SEND(service, user, N_("Channel %1% is not registered."), params[1]);
		MT_RET(false);
	}

	if (!channel->ACCESS_Matches(user, StoredChannel::Level::LVL_Set))
	{
		SEND(service, user, N_("You do not have sufficient access to modify settings for channel %1%."),
			 channel->Name());
		MT_RET(false);
	}

	static boost::regex rx("^[^[:space:][:cntrl:]@]+@"
						   "([[:alnum:]][-[:alnum:]]*\\.)*"
						   "[[:alnum:]][-[:alnum:]]*$");
	if (!boost::regex_match(params[2], rx))
	{
		NSEND(service, user, N_("Invalid e-mail address specified."));
		MT_RET(false);
	}

	channel->Email(params[2]);
	SEND(service, user, N_("E-mail address for channel %1% has been set to \002%2%\017."),
		 channel->Name() % params[2]);
	
	MT_RET(true);
	MT_EE
}

static bool biSET_WEBSITE(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biSET_WEBSITE" << service << user << params);

	boost::shared_ptr<StoredChannel> channel = ROOT->data.Get_StoredChannel(params[1]);
	if (!channel)
	{
		SEND(service, user, N_("Channel %1% is not registered."), params[1]);
		MT_RET(false);
	}

	if (!channel->ACCESS_Matches(user, StoredChannel::Level::LVL_Set))
	{
		SEND(service, user, N_("You do not have sufficient access to modify settings for channel %1%."),
			 channel->Name());
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
	if (!boost::regex_match(params[2], rx))
	{
		NSEND(service, user, N_("Invalid website address specified."));
		MT_RET(false);
	}
	std::string url(params[2]);
	if (params[1].find("http") != 0)
		url.insert(0, "http://", 7);

	channel->Website(url);
	SEND(service, user, N_("Website for channel %1% has been set to \002%2%\017."),
		 channel->Name() % url);

	MT_RET(true);
	MT_EE
}

static bool biSET_DESCRIPTION(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biSET_DESCRIPTION" << service << user << params);

	boost::shared_ptr<StoredChannel> channel = ROOT->data.Get_StoredChannel(params[1]);
	if (!channel)
	{
		SEND(service, user, N_("Channel %1% is not registered."), params[1]);
		MT_RET(false);
	}

	if (!channel->ACCESS_Matches(user, StoredChannel::Level::LVL_Set))
	{
		SEND(service, user, N_("You do not have sufficient access to modify settings for channel %1%."),
			 channel->Name());
		MT_RET(false);
	}

	std::string desc(params[2]);
	for (size_t i = 3; i < params.size(); ++i)
		desc += ' ' + params[i];

	channel->Description(desc);
	SEND(service, user, N_("Description of channel %1% has been set to \002%2%\017."),
		 channel->Name() % desc);

	MT_RET(true);
	MT_EE
}

static bool biSET_KEEPTOPIC(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biSET_KEEPTOPIC" << service << user << params);

	boost::shared_ptr<StoredChannel> channel = ROOT->data.Get_StoredChannel(params[1]);
	if (!channel)
	{
		SEND(service, user, N_("Channel %1% is not registered."), params[1]);
		MT_RET(false);
	}

	if (!channel->ACCESS_Matches(user, StoredChannel::Level::LVL_Set))
	{
		SEND(service, user, N_("You do not have sufficient access to modify settings for channel %1%."),
			 channel->Name());
		MT_RET(false);
	}

	boost::logic::tribool v = mantra::get_bool(params[2]);
	if (boost::logic::indeterminate(v))
	{
		NSEND(service, user, N_("You may only specify ON or OFF."));
		MT_RET(false);
	}

	if (channel->KeepTopic(v))
		SEND(service, user, (v
			  ? N_("Keep Topic for channel %1% has been \002enabled\017.")
			  : N_("Keep Topic for channel %1% has been \002disabled\017.")),
			 channel->Name());
	else
		SEND(service, user, N_("The keep topic setting for channel %1% is locked and cannot be changed."),
			  channel->Name());

	MT_RET(true);
	MT_EE
}

static bool biSET_TOPICLOCK(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biSET_TOPICLOCK" << service << user << params);

	boost::shared_ptr<StoredChannel> channel = ROOT->data.Get_StoredChannel(params[1]);
	if (!channel)
	{
		SEND(service, user, N_("Channel %1% is not registered."), params[1]);
		MT_RET(false);
	}

	if (!channel->ACCESS_Matches(user, StoredChannel::Level::LVL_Set))
	{
		SEND(service, user, N_("You do not have sufficient access to modify settings for channel %1%."),
			 channel->Name());
		MT_RET(false);
	}

	boost::logic::tribool v = mantra::get_bool(params[2]);
	if (boost::logic::indeterminate(v))
	{
		NSEND(service, user, N_("You may only specify ON or OFF."));
		MT_RET(false);
	}

	if (channel->TopicLock(v))
		SEND(service, user, (v
			  ? N_("Topic Lock for channel %1% has been \002enabled\017.")
			  : N_("Topic Lock for channel %1% has been \002disabled\017.")),
			 channel->Name());
	else
		SEND(service, user, N_("The topic lock setting for channel %1% is locked and cannot be changed."),
			  channel->Name());

	MT_RET(true);
	MT_EE
}

static bool biSET_PRIVATE(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biSET_PRIVATE" << service << user << params);

	boost::shared_ptr<StoredChannel> channel = ROOT->data.Get_StoredChannel(params[1]);
	if (!channel)
	{
		SEND(service, user, N_("Channel %1% is not registered."), params[1]);
		MT_RET(false);
	}

	if (!channel->ACCESS_Matches(user, StoredChannel::Level::LVL_Set))
	{
		SEND(service, user, N_("You do not have sufficient access to modify settings for channel %1%."),
			 channel->Name());
		MT_RET(false);
	}

	boost::logic::tribool v = mantra::get_bool(params[2]);
	if (boost::logic::indeterminate(v))
	{
		NSEND(service, user, N_("You may only specify ON or OFF."));
		MT_RET(false);
	}

	if (channel->Private(v))
		SEND(service, user, (v
			  ? N_("Private for channel %1% has been \002enabled\017.")
			  : N_("Private for channel %1% has been \002disabled\017.")),
			 channel->Name());
	else
		SEND(service, user, N_("The private setting for channel %1% is locked and cannot be changed."),
			  channel->Name());

	MT_RET(true);
	MT_EE
}

static bool biSET_SECUREOPS(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biSET_SECUREOPS" << service << user << params);

	boost::shared_ptr<StoredChannel> channel = ROOT->data.Get_StoredChannel(params[1]);
	if (!channel)
	{
		SEND(service, user, N_("Channel %1% is not registered."), params[1]);
		MT_RET(false);
	}

	if (!channel->ACCESS_Matches(user, StoredChannel::Level::LVL_Set))
	{
		SEND(service, user, N_("You do not have sufficient access to modify settings for channel %1%."),
			 channel->Name());
		MT_RET(false);
	}

	boost::logic::tribool v = mantra::get_bool(params[2]);
	if (boost::logic::indeterminate(v))
	{
		NSEND(service, user, N_("You may only specify ON or OFF."));
		MT_RET(false);
	}

	if (channel->SecureOps(v))
		SEND(service, user, (v
			  ? N_("Secure Ops for channel %1% has been \002enabled\017.")
			  : N_("Secure Ops for channel %1% has been \002disabled\017.")),
			 channel->Name());
	else
		SEND(service, user, N_("The secure ops setting for channel %1% is locked and cannot be changed."),
			  channel->Name());

	MT_RET(true);
	MT_EE
}

static bool biSET_SECURE(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biSET_SECURE" << service << user << params);

	boost::shared_ptr<StoredChannel> channel = ROOT->data.Get_StoredChannel(params[1]);
	if (!channel)
	{
		SEND(service, user, N_("Channel %1% is not registered."), params[1]);
		MT_RET(false);
	}

	if (!channel->ACCESS_Matches(user, StoredChannel::Level::LVL_Set))
	{
		SEND(service, user, N_("You do not have sufficient access to modify settings for channel %1%."),
			 channel->Name());
		MT_RET(false);
	}

	boost::logic::tribool v = mantra::get_bool(params[2]);
	if (boost::logic::indeterminate(v))
	{
		NSEND(service, user, N_("You may only specify ON or OFF."));
		MT_RET(false);
	}

	if (channel->Secure(v))
		SEND(service, user, (v
			  ? N_("Secure for channel %1% has been \002enabled\017.")
			  : N_("Secure for channel %1% has been \002disabled\017.")),
			 channel->Name());
	else
		SEND(service, user, N_("The secure setting for channel %1% is locked and cannot be changed."),
			  channel->Name());

	MT_RET(true);
	MT_EE
}

static bool biSET_ANARCHY(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biSET_ANARCHY" << service << user << params);

	boost::shared_ptr<StoredChannel> channel = ROOT->data.Get_StoredChannel(params[1]);
	if (!channel)
	{
		SEND(service, user, N_("Channel %1% is not registered."), params[1]);
		MT_RET(false);
	}

	if (!channel->ACCESS_Matches(user, StoredChannel::Level::LVL_Set))
	{
		SEND(service, user, N_("You do not have sufficient access to modify settings for channel %1%."),
			 channel->Name());
		MT_RET(false);
	}

	boost::logic::tribool v = mantra::get_bool(params[2]);
	if (boost::logic::indeterminate(v))
	{
		NSEND(service, user, N_("You may only specify ON or OFF."));
		MT_RET(false);
	}

	if (channel->Anarchy(v))
		SEND(service, user, (v
			  ? N_("Anarchy for channel %1% has been \002enabled\017.")
			  : N_("Anarchy for channel %1% has been \002disabled\017.")),
			 channel->Name());
	else
		SEND(service, user, N_("The anarchy setting for channel %1% is locked and cannot be changed."),
			  channel->Name());

	MT_RET(true);
	MT_EE
}

static bool biSET_KICKONBAN(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biSET_KICKONBAN" << service << user << params);

	boost::shared_ptr<StoredChannel> channel = ROOT->data.Get_StoredChannel(params[1]);
	if (!channel)
	{
		SEND(service, user, N_("Channel %1% is not registered."), params[1]);
		MT_RET(false);
	}

	if (!channel->ACCESS_Matches(user, StoredChannel::Level::LVL_Set))
	{
		SEND(service, user, N_("You do not have sufficient access to modify settings for channel %1%."),
			 channel->Name());
		MT_RET(false);
	}

	boost::logic::tribool v = mantra::get_bool(params[2]);
	if (boost::logic::indeterminate(v))
	{
		NSEND(service, user, N_("You may only specify ON or OFF."));
		MT_RET(false);
	}

	if (channel->KickOnBan(v))
		SEND(service, user, (v
			  ? N_("Kick On Ban for channel %1% has been \002enabled\017.")
			  : N_("Kick On Ban for channel %1% has been \002disabled\017.")),
			 channel->Name());
	else
		SEND(service, user, N_("The kick on ban setting for channel %1% is locked and cannot be changed."),
			  channel->Name());

	MT_RET(true);
	MT_EE
}

static bool biSET_RESTRICTED(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biSET_RESTRICTED" << service << user << params);

	boost::shared_ptr<StoredChannel> channel = ROOT->data.Get_StoredChannel(params[1]);
	if (!channel)
	{
		SEND(service, user, N_("Channel %1% is not registered."), params[1]);
		MT_RET(false);
	}

	if (!channel->ACCESS_Matches(user, StoredChannel::Level::LVL_Set))
	{
		SEND(service, user, N_("You do not have sufficient access to modify settings for channel %1%."),
			 channel->Name());
		MT_RET(false);
	}

	boost::logic::tribool v = mantra::get_bool(params[2]);
	if (boost::logic::indeterminate(v))
	{
		NSEND(service, user, N_("You may only specify ON or OFF."));
		MT_RET(false);
	}

	if (channel->Restricted(v))
		SEND(service, user, (v
			  ? N_("Restricted for channel %1% has been \002enabled\017.")
			  : N_("Restricted for channel %1% has been \002disabled\017.")),
			 channel->Name());
	else
		SEND(service, user, N_("The restricted setting for channel %1% is locked and cannot be changed."),
			  channel->Name());

	MT_RET(true);
	MT_EE
}

static bool biSET_CJOIN(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biSET_CJOIN" << service << user << params);

	boost::shared_ptr<StoredChannel> channel = ROOT->data.Get_StoredChannel(params[1]);
	if (!channel)
	{
		SEND(service, user, N_("Channel %1% is not registered."), params[1]);
		MT_RET(false);
	}

	if (!channel->ACCESS_Matches(user, StoredChannel::Level::LVL_Set))
	{
		SEND(service, user, N_("You do not have sufficient access to modify settings for channel %1%."),
			 channel->Name());
		MT_RET(false);
	}

	boost::logic::tribool v = mantra::get_bool(params[2]);
	if (boost::logic::indeterminate(v))
	{
		NSEND(service, user, N_("You may only specify ON or OFF."));
		MT_RET(false);
	}

	if (channel->CJoin(v))
		SEND(service, user, (v
			  ? N_("Channel Join for channel %1% has been \002enabled\017.")
			  : N_("Channel Join for channel %1% has been \002disabled\017.")),
			 channel->Name());
	else
		SEND(service, user, N_("The channel join setting for channel %1% is locked and cannot be changed."),
			  channel->Name());

	MT_RET(true);
	MT_EE
}

static bool biSET_BANTIME(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biSET_BANTIME" << service << user << params);

	boost::shared_ptr<StoredChannel> channel = ROOT->data.Get_StoredChannel(params[1]);
	if (!channel)
	{
		SEND(service, user, N_("Channel %1% is not registered."), params[1]);
		MT_RET(false);
	}

	if (!channel->ACCESS_Matches(user, StoredChannel::Level::LVL_Set))
	{
		SEND(service, user, N_("You do not have sufficient access to modify settings for channel %1%."),
			 channel->Name());
		MT_RET(false);
	}

	std::string tmp(params[2]);
	for (size_t i = 3; i < params.size(); ++i)
		tmp += ' ' + params[i];

	mantra::duration length("");

	try
	{
		mantra::StringToDuration(tmp, length);
	}
	catch (const mantra::mdatetime_format &e)
	{
		NSEND(service, user, N_("Invalid length specified for ban removal time."));
		MT_RET(false);
	}

	if (channel->BanTime(length))
		SEND(service, user, N_("Ban removal time for channel %1% has been \002%2%\017."),
			 channel->Name() % mantra::DurationToString(length, mantra::Second));
	else
		SEND(service, user, N_("The ban removal time setting for channel %1% is locked and cannot be changed."),
			  channel->Name());

	MT_RET(true);
	MT_EE
}

static bool biSET_PARTTIME(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biSET_PARTTIME" << service << user << params);

	boost::shared_ptr<StoredChannel> channel = ROOT->data.Get_StoredChannel(params[1]);
	if (!channel)
	{
		SEND(service, user, N_("Channel %1% is not registered."), params[1]);
		MT_RET(false);
	}

	if (!channel->ACCESS_Matches(user, StoredChannel::Level::LVL_Set))
	{
		SEND(service, user, N_("You do not have sufficient access to modify settings for channel %1%."),
			 channel->Name());
		MT_RET(false);
	}

	std::string tmp(params[2]);
	for (size_t i = 3; i < params.size(); ++i)
		tmp += ' ' + params[i];

	mantra::duration length("");

	try
	{
		mantra::StringToDuration(tmp, length);
	}
	catch (const mantra::mdatetime_format &e)
	{
		NSEND(service, user, N_("Invalid length specified for part remember time."));
		MT_RET(false);
	}

	if (channel->PartTime(length))
		SEND(service, user, N_("Part remember time for channel %1% has been \002%2%\017."),
			 channel->Name() % mantra::DurationToString(length, mantra::Second));
	else
		SEND(service, user, N_("The part remember time setting for channel %1% is locked and cannot be changed."),
			  channel->Name());

	MT_RET(true);
	MT_EE
}

static bool biSET_REVENGE(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biSET_REVENGE" << service << user << params);

	boost::shared_ptr<StoredChannel> channel = ROOT->data.Get_StoredChannel(params[1]);
	if (!channel)
	{
		SEND(service, user, N_("Channel %1% is not registered."), params[1]);
		MT_RET(false);
	}

	StoredChannel::Revenge_t r = StoredChannel::RevengeID(params[2]);
	if (r < StoredChannel::R_None || r >= StoredChannel::R_MAX)
	{
		SEND(service, user, N_("Invalid revenge type %1% specified."), params[2]);
		MT_RET(false);
	}

	if (!channel->ACCESS_Matches(user, StoredChannel::Level::LVL_Set))
	{
		SEND(service, user, N_("You do not have sufficient access to modify settings for channel %1%."),
			 channel->Name());
		MT_RET(false);
	}

	if (channel->Revenge(r))
		SEND(service, user, N_("Revenge for channel %1% has been set to \002%2%\017."),
			 channel->Name() % StoredChannel::RevengeDesc(r));
	else
		SEND(service, user, N_("Revenge setting for channel %1% is locked and cannot be changed."),
			  channel->Name());

	MT_RET(true);
	MT_EE
}

static bool biSET_MLOCK(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biSET_MLOCK" << service << user << params);

	// TODO: To be implemented.
	SEND(service, user,
		 N_("The %1% command has not yet been implemented."),
		 boost::algorithm::to_upper_copy(params[0]));

	MT_RET(false);
	MT_EE
}

static bool biUNSET_COFOUNDER(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biUNSET_COFOUNDER" << service << user << params);

	boost::shared_ptr<StoredChannel> channel = ROOT->data.Get_StoredChannel(params[1]);
	if (!channel)
	{
		SEND(service, user, N_("Channel %1% is not registered."), params[1]);
		MT_RET(false);
	}

	if (!user->Identified(channel))
	{
		SEND(service, user, N_("You must identify to the channel to execute the %1% command."),
			 boost::algorithm::to_upper_copy(params[0]));
		MT_RET(false);
	}

	channel->Successor(boost::shared_ptr<StoredUser>());
	SEND(service, user, N_("Successor of for channel %1% has been unset."),
		 channel->Name());

	MT_RET(true);
	MT_EE
}

static bool biUNSET_EMAIL(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biUNSET_EMAIL" << service << user << params);

	boost::shared_ptr<StoredChannel> channel = ROOT->data.Get_StoredChannel(params[1]);
	if (!channel)
	{
		SEND(service, user, N_("Channel %1% is not registered."), params[1]);
		MT_RET(false);
	}

	if (!channel->ACCESS_Matches(user, StoredChannel::Level::LVL_Set))
	{
		SEND(service, user, N_("You do not have sufficient access to modify settings for channel %1%."),
			 channel->Name());
		MT_RET(false);
	}

	channel->Email(std::string());
	SEND(service, user, N_("E-mail address for channel %1% has been unset."),
		 channel->Name());
	
	MT_RET(true);
	MT_EE
}

static bool biUNSET_WEBSITE(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biUNSET_WEBSITE" << service << user << params);

	boost::shared_ptr<StoredChannel> channel = ROOT->data.Get_StoredChannel(params[1]);
	if (!channel)
	{
		SEND(service, user, N_("Channel %1% is not registered."), params[1]);
		MT_RET(false);
	}

	if (!channel->ACCESS_Matches(user, StoredChannel::Level::LVL_Set))
	{
		SEND(service, user, N_("You do not have sufficient access to modify settings for channel %1%."),
			 channel->Name());
		MT_RET(false);
	}

	channel->Website(std::string());
	SEND(service, user, N_("Website address for channel %1% has been unset."),
		 channel->Name());
	
	MT_RET(true);
	MT_EE
}

static bool biUNSET_KEEPTOPIC(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biUNSET_KEEPTOPIC" << service << user << params);

	boost::shared_ptr<StoredChannel> channel = ROOT->data.Get_StoredChannel(params[1]);
	if (!channel)
	{
		SEND(service, user, N_("Channel %1% is not registered."), params[1]);
		MT_RET(false);
	}

	if (!channel->ACCESS_Matches(user, StoredChannel::Level::LVL_Set))
	{
		SEND(service, user, N_("You do not have sufficient access to modify settings for channel %1%."),
			 channel->Name());
		MT_RET(false);
	}

	if (channel->KeepTopic(boost::logic::indeterminate))
		SEND(service, user, N_("Keep topic for channel %1% has been reset to the default"),
			 channel->Name());
	else
		SEND(service, user, N_("The keep topic setting for channel %1% is locked and cannot be changed."),
			  channel->Name());

	MT_RET(true);
	MT_EE
}

static bool biUNSET_TOPICLOCK(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biUNSET_TOPICLOCK" << service << user << params);

	boost::shared_ptr<StoredChannel> channel = ROOT->data.Get_StoredChannel(params[1]);
	if (!channel)
	{
		SEND(service, user, N_("Channel %1% is not registered."), params[1]);
		MT_RET(false);
	}

	if (!channel->ACCESS_Matches(user, StoredChannel::Level::LVL_Set))
	{
		SEND(service, user, N_("You do not have sufficient access to modify settings for channel %1%."),
			 channel->Name());
		MT_RET(false);
	}

	if (channel->TopicLock(boost::logic::indeterminate))
		SEND(service, user, N_("Topic lock for channel %1% has been reset to the default"),
			 channel->Name());
	else
		SEND(service, user, N_("The topic lock setting for channel %1% is locked and cannot be changed."),
			  channel->Name());

	MT_RET(true);
	MT_EE
}

static bool biUNSET_PRIVATE(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biUNSET_PRIVATE" << service << user << params);

	boost::shared_ptr<StoredChannel> channel = ROOT->data.Get_StoredChannel(params[1]);
	if (!channel)
	{
		SEND(service, user, N_("Channel %1% is not registered."), params[1]);
		MT_RET(false);
	}

	if (!channel->ACCESS_Matches(user, StoredChannel::Level::LVL_Set))
	{
		SEND(service, user, N_("You do not have sufficient access to modify settings for channel %1%."),
			 channel->Name());
		MT_RET(false);
	}

	if (channel->Private(boost::logic::indeterminate))
		SEND(service, user, N_("Private for channel %1% has been reset to the default"),
			 channel->Name());
	else
		SEND(service, user, N_("The private setting for channel %1% is locked and cannot be changed."),
			  channel->Name());

	MT_RET(true);
	MT_EE
}

static bool biUNSET_SECUREOPS(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biUNSET_SECUREOPS" << service << user << params);

	boost::shared_ptr<StoredChannel> channel = ROOT->data.Get_StoredChannel(params[1]);
	if (!channel)
	{
		SEND(service, user, N_("Channel %1% is not registered."), params[1]);
		MT_RET(false);
	}

	if (!channel->ACCESS_Matches(user, StoredChannel::Level::LVL_Set))
	{
		SEND(service, user, N_("You do not have sufficient access to modify settings for channel %1%."),
			 channel->Name());
		MT_RET(false);
	}

	if (channel->SecureOps(boost::logic::indeterminate))
		SEND(service, user, N_("Secure ops for channel %1% has been reset to the default"),
			 channel->Name());
	else
		SEND(service, user, N_("The secure ops setting for channel %1% is locked and cannot be changed."),
			  channel->Name());

	MT_RET(true);
	MT_EE
}

static bool biUNSET_SECURE(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biUNSET_SECURE" << service << user << params);

	boost::shared_ptr<StoredChannel> channel = ROOT->data.Get_StoredChannel(params[1]);
	if (!channel)
	{
		SEND(service, user, N_("Channel %1% is not registered."), params[1]);
		MT_RET(false);
	}

	if (!channel->ACCESS_Matches(user, StoredChannel::Level::LVL_Set))
	{
		SEND(service, user, N_("You do not have sufficient access to modify settings for channel %1%."),
			 channel->Name());
		MT_RET(false);
	}

	if (channel->Secure(boost::logic::indeterminate))
		SEND(service, user, N_("Secure for channel %1% has been reset to the default"),
			 channel->Name());
	else
		SEND(service, user, N_("The secure setting for channel %1% is locked and cannot be changed."),
			  channel->Name());

	MT_RET(true);
	MT_EE
}

static bool biUNSET_ANARCHY(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biUNSET_ANARCHY" << service << user << params);

	boost::shared_ptr<StoredChannel> channel = ROOT->data.Get_StoredChannel(params[1]);
	if (!channel)
	{
		SEND(service, user, N_("Channel %1% is not registered."), params[1]);
		MT_RET(false);
	}

	if (!channel->ACCESS_Matches(user, StoredChannel::Level::LVL_Set))
	{
		SEND(service, user, N_("You do not have sufficient access to modify settings for channel %1%."),
			 channel->Name());
		MT_RET(false);
	}

	if (channel->Anarchy(boost::logic::indeterminate))
		SEND(service, user, N_("Anarchy for channel %1% has been reset to the default"),
			 channel->Name());
	else
		SEND(service, user, N_("The anarchy setting for channel %1% is locked and cannot be changed."),
			  channel->Name());

	MT_RET(true);
	MT_EE
}

static bool biUNSET_KICKONBAN(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biUNSET_KICKONBAN" << service << user << params);

	boost::shared_ptr<StoredChannel> channel = ROOT->data.Get_StoredChannel(params[1]);
	if (!channel)
	{
		SEND(service, user, N_("Channel %1% is not registered."), params[1]);
		MT_RET(false);
	}

	if (!channel->ACCESS_Matches(user, StoredChannel::Level::LVL_Set))
	{
		SEND(service, user, N_("You do not have sufficient access to modify settings for channel %1%."),
			 channel->Name());
		MT_RET(false);
	}

	if (channel->KickOnBan(boost::logic::indeterminate))
		SEND(service, user, N_("Kick on ban for channel %1% has been reset to the default"),
			 channel->Name());
	else
		SEND(service, user, N_("The kick on ban setting for channel %1% is locked and cannot be changed."),
			  channel->Name());

	MT_RET(true);
	MT_EE
}

static bool biUNSET_RESTRICTED(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biUNSET_RESTRICTED" << service << user << params);

	boost::shared_ptr<StoredChannel> channel = ROOT->data.Get_StoredChannel(params[1]);
	if (!channel)
	{
		SEND(service, user, N_("Channel %1% is not registered."), params[1]);
		MT_RET(false);
	}

	if (!channel->ACCESS_Matches(user, StoredChannel::Level::LVL_Set))
	{
		SEND(service, user, N_("You do not have sufficient access to modify settings for channel %1%."),
			 channel->Name());
		MT_RET(false);
	}

	if (channel->Restricted(boost::logic::indeterminate))
		SEND(service, user, N_("Restricted for channel %1% has been reset to the default"),
			 channel->Name());
	else
		SEND(service, user, N_("The restricted setting for channel %1% is locked and cannot be changed."),
			  channel->Name());

	MT_RET(true);
	MT_EE
}

static bool biUNSET_CJOIN(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biUNSET_CJOIN" << service << user << params);

	boost::shared_ptr<StoredChannel> channel = ROOT->data.Get_StoredChannel(params[1]);
	if (!channel)
	{
		SEND(service, user, N_("Channel %1% is not registered."), params[1]);
		MT_RET(false);
	}

	if (!channel->ACCESS_Matches(user, StoredChannel::Level::LVL_Set))
	{
		SEND(service, user, N_("You do not have sufficient access to modify settings for channel %1%."),
			 channel->Name());
		MT_RET(false);
	}

	if (channel->CJoin(boost::logic::indeterminate))
		SEND(service, user, N_("Channel join for channel %1% has been reset to the default"),
			 channel->Name());
	else
		SEND(service, user, N_("The channel join setting for channel %1% is locked and cannot be changed."),
			  channel->Name());

	MT_RET(true);
	MT_EE
}

static bool biUNSET_BANTIME(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biUNSET_BANTIME" << service << user << params);

	boost::shared_ptr<StoredChannel> channel = ROOT->data.Get_StoredChannel(params[1]);
	if (!channel)
	{
		SEND(service, user, N_("Channel %1% is not registered."), params[1]);
		MT_RET(false);
	}

	if (!channel->ACCESS_Matches(user, StoredChannel::Level::LVL_Set))
	{
		SEND(service, user, N_("You do not have sufficient access to modify settings for channel %1%."),
			 channel->Name());
		MT_RET(false);
	}

	if (channel->BanTime(mantra::duration("")))
		SEND(service, user, N_("Ban removal time for channel %1% has been reset to the default"),
			 channel->Name());
	else
		SEND(service, user, N_("The ban removal time setting for channel %1% is locked and cannot be changed."),
			  channel->Name());

	MT_RET(true);
	MT_EE
}

static bool biUNSET_PARTTIME(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biUNSET_PARTTIME" << service << user << params);

	boost::shared_ptr<StoredChannel> channel = ROOT->data.Get_StoredChannel(params[1]);
	if (!channel)
	{
		SEND(service, user, N_("Channel %1% is not registered."), params[1]);
		MT_RET(false);
	}

	if (!channel->ACCESS_Matches(user, StoredChannel::Level::LVL_Set))
	{
		SEND(service, user, N_("You do not have sufficient access to modify settings for channel %1%."),
			 channel->Name());
		MT_RET(false);
	}

	if (channel->PartTime(mantra::duration("")))
		SEND(service, user, N_("Part remember time for channel %1% has been reset to the default"),
			 channel->Name());
	else
		SEND(service, user, N_("The part remember time setting for channel %1% is locked and cannot be changed."),
			  channel->Name());

	MT_RET(true);
	MT_EE
}

static bool biUNSET_REVENGE(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biUNSET_REVENGE" << service << user << params);

	boost::shared_ptr<StoredChannel> channel = ROOT->data.Get_StoredChannel(params[1]);
	if (!channel)
	{
		SEND(service, user, N_("Channel %1% is not registered."), params[1]);
		MT_RET(false);
	}

	if (!channel->ACCESS_Matches(user, StoredChannel::Level::LVL_Set))
	{
		SEND(service, user, N_("You do not have sufficient access to modify settings for channel %1%."),
			 channel->Name());
		MT_RET(false);
	}

	if (channel->Revenge(StoredChannel::R_MAX))
		SEND(service, user, N_("Revenge for channel %1% has been reset to the default"),
			 channel->Name());
	else
		SEND(service, user, N_("The revenge setting for channel %1% is locked and cannot be changed."),
			  channel->Name());

	MT_RET(true);
	MT_EE
}

static bool biUNSET_MLOCK(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biUNSET_MLOCK" << service << user << params);

	// TODO: To be implemented.
	SEND(service, user,
		 N_("The %1% command has not yet been implemented."),
		 boost::algorithm::to_upper_copy(params[0]));

	MT_RET(false);
	MT_EE
}

static bool biSET_COMMENT(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biSET_COMMENT" << service << user << params);

	boost::shared_ptr<StoredChannel> channel = ROOT->data.Get_StoredChannel(params[1]);
	if (!channel)
	{
		SEND(service, user, N_("Channel %1% is not registered."), params[1]);
		MT_RET(false);
	}

	std::string comment(params[2]);
	for (size_t i = 3; i < params.size(); ++i)
		comment += ' ' + params[i];

	channel->Comment(comment);
	SEND(service, user, N_("Comment of channel %1% has been set to \002%2%\017."),
		 channel->Name() % comment);

	MT_RET(true);
	MT_EE
}

static bool biSET_NOEXPIRE(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biSET_NOEXPIRE" << service << user << params);

	boost::shared_ptr<StoredChannel> channel = ROOT->data.Get_StoredChannel(params[1]);
	if (!channel)
	{
		SEND(service, user, N_("Channel %1% is not registered."), params[1]);
		MT_RET(false);
	}

	boost::logic::tribool v = mantra::get_bool(params[2]);
	if (boost::logic::indeterminate(v))
	{
		NSEND(service, user, N_("You may only specify ON or OFF."));
		MT_RET(false);
	}

	if (channel->NoExpire(v))
		SEND(service, user, (v
			  ? N_("No expire for channel %1% has been \002enabled\017.")
			  : N_("No expire for channel %1% has been \002disabled\017.")),
			 channel->Name());
	else
		SEND(service, user, N_("The no expire setting for channel %1% is locked and cannot be changed."),
			  channel->Name());

	MT_RET(true);
	MT_EE
}

static bool biUNSET_COMMENT(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biUNSET_COMMENT" << service << user << params);

	boost::shared_ptr<StoredChannel> channel = ROOT->data.Get_StoredChannel(params[1]);
	if (!channel)
	{
		SEND(service, user, N_("Channel %1% is not registered."), params[1]);
		MT_RET(false);
	}

	channel->Comment(std::string());
	SEND(service, user, N_("Comment for channel %1% has been unset."),
		 channel->Name());
	
	MT_RET(true);
	MT_EE
}

static bool biUNSET_NOEXPIRE(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biUNSET_NOEXPIRE" << service << user << params);

	boost::shared_ptr<StoredChannel> channel = ROOT->data.Get_StoredChannel(params[1]);
	if (!channel)
	{
		SEND(service, user, N_("Channel %1% is not registered."), params[1]);
		MT_RET(false);
	}

	if (channel->NoExpire(boost::logic::indeterminate))
		SEND(service, user, N_("No expire for channel %1% has been reset to the default"),
			 channel->Name());
	else
		SEND(service, user, N_("The no expire setting for channel %1% is locked and cannot be changed."),
			  channel->Name());

	MT_RET(true);
	MT_EE
}

static bool biLOCK_KEEPTOPIC(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biLOCK_KEEPTOPIC" << service << user << params);

	boost::shared_ptr<StoredChannel> channel = ROOT->data.Get_StoredChannel(params[1]);
	if (!channel)
	{
		SEND(service, user, N_("Channel %1% is not registered."), params[1]);
		MT_RET(false);
	}

	boost::logic::tribool v = mantra::get_bool(params[2]);
	if (boost::logic::indeterminate(v))
	{
		NSEND(service, user, N_("You may only specify ON or OFF."));
		MT_RET(false);
	}

	if (!channel->LOCK_KeepTopic(false))
		NSEND(service, user, N_("The keep topic setting is globally locked and cannot be locked."));

	channel->KeepTopic(v);
	channel->LOCK_KeepTopic(true);
	SEND(service, user, (v
		  ? N_("Keep topic for channel %1% has been \002enabled\017 and locked.")
		  : N_("Keep topic for channel %1% has been \002disabled\017 and locked.")),
		 channel->Name());

	MT_RET(true);
	MT_EE
}

static bool biLOCK_TOPICLOCK(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biLOCK_TOPICLOCK" << service << user << params);

	boost::shared_ptr<StoredChannel> channel = ROOT->data.Get_StoredChannel(params[1]);
	if (!channel)
	{
		SEND(service, user, N_("Channel %1% is not registered."), params[1]);
		MT_RET(false);
	}

	boost::logic::tribool v = mantra::get_bool(params[2]);
	if (boost::logic::indeterminate(v))
	{
		NSEND(service, user, N_("You may only specify ON or OFF."));
		MT_RET(false);
	}

	if (!channel->LOCK_TopicLock(false))
		NSEND(service, user, N_("The topic lock setting is globally locked and cannot be locked."));

	channel->TopicLock(v);
	channel->LOCK_TopicLock(true);
	SEND(service, user, (v
		  ? N_("Topic lock for channel %1% has been \002enabled\017 and locked.")
		  : N_("Topic lock for channel %1% has been \002disabled\017 and locked.")),
		 channel->Name());

	MT_RET(true);
	MT_EE
}

static bool biLOCK_PRIVATE(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biLOCK_PRIVATE" << service << user << params);

	boost::shared_ptr<StoredChannel> channel = ROOT->data.Get_StoredChannel(params[1]);
	if (!channel)
	{
		SEND(service, user, N_("Channel %1% is not registered."), params[1]);
		MT_RET(false);
	}

	boost::logic::tribool v = mantra::get_bool(params[2]);
	if (boost::logic::indeterminate(v))
	{
		NSEND(service, user, N_("You may only specify ON or OFF."));
		MT_RET(false);
	}

	if (!channel->LOCK_Private(false))
		NSEND(service, user, N_("The keep topic setting is globally locked and cannot be locked."));

	channel->Private(v);
	channel->LOCK_Private(true);
	SEND(service, user, (v
		  ? N_("Private for channel %1% has been \002enabled\017 and locked.")
		  : N_("Private for channel %1% has been \002disabled\017 and locked.")),
		 channel->Name());

	MT_RET(true);
	MT_EE
}

static bool biLOCK_SECUREOPS(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biLOCK_SECUREOPS" << service << user << params);

	boost::shared_ptr<StoredChannel> channel = ROOT->data.Get_StoredChannel(params[1]);
	if (!channel)
	{
		SEND(service, user, N_("Channel %1% is not registered."), params[1]);
		MT_RET(false);
	}

	boost::logic::tribool v = mantra::get_bool(params[2]);
	if (boost::logic::indeterminate(v))
	{
		NSEND(service, user, N_("You may only specify ON or OFF."));
		MT_RET(false);
	}

	if (!channel->LOCK_SecureOps(false))
		NSEND(service, user, N_("The keep topic setting is globally locked and cannot be locked."));

	channel->SecureOps(v);
	channel->LOCK_SecureOps(true);
	SEND(service, user, (v
		  ? N_("Secure ops for channel %1% has been \002enabled\017 and locked.")
		  : N_("Secure ops for channel %1% has been \002disabled\017 and locked.")),
		 channel->Name());

	MT_RET(true);
	MT_EE
}

static bool biLOCK_SECURE(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biLOCK_SECURE" << service << user << params);

	boost::shared_ptr<StoredChannel> channel = ROOT->data.Get_StoredChannel(params[1]);
	if (!channel)
	{
		SEND(service, user, N_("Channel %1% is not registered."), params[1]);
		MT_RET(false);
	}

	boost::logic::tribool v = mantra::get_bool(params[2]);
	if (boost::logic::indeterminate(v))
	{
		NSEND(service, user, N_("You may only specify ON or OFF."));
		MT_RET(false);
	}

	if (!channel->LOCK_Secure(false))
		NSEND(service, user, N_("The secure setting is globally locked and cannot be locked."));

	channel->Secure(v);
	channel->LOCK_Secure(true);
	SEND(service, user, (v
		  ? N_("Secure for channel %1% has been \002enabled\017 and locked.")
		  : N_("Secure for channel %1% has been \002disabled\017 and locked.")),
		 channel->Name());

	MT_RET(true);
	MT_EE
}

static bool biLOCK_ANARCHY(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biLOCK_ANARCHY" << service << user << params);

	boost::shared_ptr<StoredChannel> channel = ROOT->data.Get_StoredChannel(params[1]);
	if (!channel)
	{
		SEND(service, user, N_("Channel %1% is not registered."), params[1]);
		MT_RET(false);
	}

	boost::logic::tribool v = mantra::get_bool(params[2]);
	if (boost::logic::indeterminate(v))
	{
		NSEND(service, user, N_("You may only specify ON or OFF."));
		MT_RET(false);
	}

	if (!channel->LOCK_Anarchy(false))
		NSEND(service, user, N_("The anarchy setting is globally locked and cannot be locked."));

	channel->Anarchy(v);
	channel->LOCK_Anarchy(true);
	SEND(service, user, (v
		  ? N_("Anarchy for channel %1% has been \002enabled\017 and locked.")
		  : N_("Anarchy for channel %1% has been \002disabled\017 and locked.")),
		 channel->Name());

	MT_RET(true);
	MT_EE
}

static bool biLOCK_KICKONBAN(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biLOCK_KICKONBAN" << service << user << params);

	boost::shared_ptr<StoredChannel> channel = ROOT->data.Get_StoredChannel(params[1]);
	if (!channel)
	{
		SEND(service, user, N_("Channel %1% is not registered."), params[1]);
		MT_RET(false);
	}

	boost::logic::tribool v = mantra::get_bool(params[2]);
	if (boost::logic::indeterminate(v))
	{
		NSEND(service, user, N_("You may only specify ON or OFF."));
		MT_RET(false);
	}

	if (!channel->LOCK_KickOnBan(false))
		NSEND(service, user, N_("The keep topic setting is globally locked and cannot be locked."));

	channel->KickOnBan(v);
	channel->LOCK_KickOnBan(true);
	SEND(service, user, (v
		  ? N_("Kick on ban for channel %1% has been \002enabled\017 and locked.")
		  : N_("Kick on ban for channel %1% has been \002disabled\017 and locked.")),
		 channel->Name());

	MT_RET(true);
	MT_EE
}

static bool biLOCK_RESTRICTED(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biLOCK_RESTRICTED" << service << user << params);

	boost::shared_ptr<StoredChannel> channel = ROOT->data.Get_StoredChannel(params[1]);
	if (!channel)
	{
		SEND(service, user, N_("Channel %1% is not registered."), params[1]);
		MT_RET(false);
	}

	boost::logic::tribool v = mantra::get_bool(params[2]);
	if (boost::logic::indeterminate(v))
	{
		NSEND(service, user, N_("You may only specify ON or OFF."));
		MT_RET(false);
	}

	if (!channel->LOCK_Restricted(false))
		NSEND(service, user, N_("The restricted setting is globally locked and cannot be locked."));

	channel->Restricted(v);
	channel->LOCK_Restricted(true);
	SEND(service, user, (v
		  ? N_("Restricted for channel %1% has been \002enabled\017 and locked.")
		  : N_("Restricted for channel %1% has been \002disabled\017 and locked.")),
		 channel->Name());

	MT_RET(true);
	MT_EE
}

static bool biLOCK_CJOIN(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biLOCK_CJOIN" << service << user << params);

	boost::shared_ptr<StoredChannel> channel = ROOT->data.Get_StoredChannel(params[1]);
	if (!channel)
	{
		SEND(service, user, N_("Channel %1% is not registered."), params[1]);
		MT_RET(false);
	}

	boost::logic::tribool v = mantra::get_bool(params[2]);
	if (boost::logic::indeterminate(v))
	{
		NSEND(service, user, N_("You may only specify ON or OFF."));
		MT_RET(false);
	}

	if (!channel->LOCK_CJoin(false))
		NSEND(service, user, N_("The channel join setting is globally locked and cannot be locked."));

	channel->CJoin(v);
	channel->LOCK_CJoin(true);
	SEND(service, user, (v
		  ? N_("Channel join for channel %1% has been \002enabled\017 and locked.")
		  : N_("Channel join for channel %1% has been \002disabled\017 and locked.")),
		 channel->Name());

	MT_RET(true);
	MT_EE
}

static bool biLOCK_BANTIME(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biLOCK_BANTIME" << service << user << params);

	boost::shared_ptr<StoredChannel> channel = ROOT->data.Get_StoredChannel(params[1]);
	if (!channel)
	{
		SEND(service, user, N_("Channel %1% is not registered."), params[1]);
		MT_RET(false);
	}

	std::string tmp(params[2]);
	for (size_t i = 3; i < params.size(); ++i)
		tmp += ' ' + params[i];

	mantra::duration length("");

	try
	{
		mantra::StringToDuration(tmp, length);
	}
	catch (const mantra::mdatetime_format &e)
	{
		NSEND(service, user, N_("Invalid length specified for ban removal time."));
		MT_RET(false);
	}

	if (!channel->LOCK_BanTime(false))
		NSEND(service, user, N_("The ban removal time setting is globally locked and cannot be locked."));

	channel->BanTime(length);
	channel->LOCK_BanTime(true);
	SEND(service, user, N_("Ban removal time for channel %1% has been \002%2%\017 and locked."),
		 channel->Name() % mantra::DurationToString(length, mantra::Second));

	MT_RET(true);
	MT_EE
}

static bool biLOCK_PARTTIME(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biLOCK_PARTTIME" << service << user << params);

	boost::shared_ptr<StoredChannel> channel = ROOT->data.Get_StoredChannel(params[1]);
	if (!channel)
	{
		SEND(service, user, N_("Channel %1% is not registered."), params[1]);
		MT_RET(false);
	}

	std::string tmp(params[2]);
	for (size_t i = 3; i < params.size(); ++i)
		tmp += ' ' + params[i];

	mantra::duration length("");

	try
	{
		mantra::StringToDuration(tmp, length);
	}
	catch (const mantra::mdatetime_format &e)
	{
		NSEND(service, user, N_("Invalid length specified for ban removal time."));
		MT_RET(false);
	}

	if (!channel->LOCK_PartTime(false))
		NSEND(service, user, N_("The part remember time setting is globally locked and cannot be locked."));

	channel->PartTime(length);
	channel->LOCK_PartTime(true);
	SEND(service, user, N_("Part remember time for channel %1% has been \002%2%\017 and locked."),
		 channel->Name() % mantra::DurationToString(length, mantra::Second));

	MT_RET(true);
	MT_EE
}

static bool biLOCK_REVENGE(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biLOCK_REVENGE" << service << user << params);

	boost::shared_ptr<StoredChannel> channel = ROOT->data.Get_StoredChannel(params[1]);
	if (!channel)
	{
		SEND(service, user, N_("Channel %1% is not registered."), params[1]);
		MT_RET(false);
	}

	StoredChannel::Revenge_t r = StoredChannel::RevengeID(params[2]);
	if (r < StoredChannel::R_None || r >= StoredChannel::R_MAX)
	{
		SEND(service, user, N_("Invalid revenge type %1% specified."), params[2]);
		MT_RET(false);
	}

	if (!channel->LOCK_Revenge(false))
		NSEND(service, user, N_("The revenge setting is globally locked and cannot be locked."));

	channel->Revenge(r);
	channel->LOCK_Revenge(true);
	SEND(service, user, N_("Revenge for channel %1% has been set to \002%2%\017 and locked."),
		 channel->Name() % StoredChannel::RevengeDesc(r));

	MT_RET(true);
	MT_EE
}

static bool biLOCK_MLOCK(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biLOCK_MLOCK" << service << user << params);

	// TODO: To be implemented.
	SEND(service, user,
		 N_("The %1% command has not yet been implemented."),
		 boost::algorithm::to_upper_copy(params[0]));

	MT_RET(false);
	MT_EE
}

static bool biUNLOCK_KEEPTOPIC(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biUNLOCK_KEEPTOPIC" << service << user << params);

	boost::shared_ptr<StoredChannel> channel = ROOT->data.Get_StoredChannel(params[1]);
	if (!channel)
	{
		SEND(service, user, N_("Channel %1% is not registered."), params[1]);
		MT_RET(false);
	}

	if (channel->LOCK_KeepTopic(false))
		SEND(service, user, N_("Keep topic for channel %1% has been unlocked."),
			 channel->Name());
	else
		NSEND(service, user, N_("The keep topic setting is globally locked and cannot be unlocked."));

	MT_RET(true);
	MT_EE
}

static bool biUNLOCK_TOPICUNLOCK(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biUNLOCK_TOPICUNLOCK" << service << user << params);

	boost::shared_ptr<StoredChannel> channel = ROOT->data.Get_StoredChannel(params[1]);
	if (!channel)
	{
		SEND(service, user, N_("Channel %1% is not registered."), params[1]);
		MT_RET(false);
	}

	if (channel->LOCK_TopicLock(false))
		SEND(service, user, N_("Topic Lock for channel %1% has been unlocked."),
			 channel->Name());
	else
		NSEND(service, user, N_("The topic lock setting is globally locked and cannot be unlocked."));

	MT_RET(true);
	MT_EE
}

static bool biUNLOCK_PRIVATE(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biUNLOCK_PRIVATE" << service << user << params);

	boost::shared_ptr<StoredChannel> channel = ROOT->data.Get_StoredChannel(params[1]);
	if (!channel)
	{
		SEND(service, user, N_("Channel %1% is not registered."), params[1]);
		MT_RET(false);
	}

	if (channel->LOCK_Private(false))
		SEND(service, user, N_("Private for channel %1% has been unlocked."),
			 channel->Name());
	else
		NSEND(service, user, N_("The private setting is globally locked and cannot be unlocked."));

	MT_RET(true);
	MT_EE
}

static bool biUNLOCK_SECUREOPS(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biUNLOCK_SECUREOPS" << service << user << params);

	boost::shared_ptr<StoredChannel> channel = ROOT->data.Get_StoredChannel(params[1]);
	if (!channel)
	{
		SEND(service, user, N_("Channel %1% is not registered."), params[1]);
		MT_RET(false);
	}

	if (channel->LOCK_SecureOps(false))
		SEND(service, user, N_("Secure ops for channel %1% has been unlocked."),
			 channel->Name());
	else
		NSEND(service, user, N_("The secure ops setting is globally locked and cannot be unlocked."));

	MT_RET(true);
	MT_EE
}

static bool biUNLOCK_SECURE(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biUNLOCK_SECURE" << service << user << params);

	boost::shared_ptr<StoredChannel> channel = ROOT->data.Get_StoredChannel(params[1]);
	if (!channel)
	{
		SEND(service, user, N_("Channel %1% is not registered."), params[1]);
		MT_RET(false);
	}

	if (channel->LOCK_Secure(false))
		SEND(service, user, N_("Secure for channel %1% has been unlocked."),
			 channel->Name());
	else
		NSEND(service, user, N_("The secure setting is globally locked and cannot be unlocked."));

	MT_RET(true);
	MT_EE
}

static bool biUNLOCK_ANARCHY(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biUNLOCK_ANARCHY" << service << user << params);

	boost::shared_ptr<StoredChannel> channel = ROOT->data.Get_StoredChannel(params[1]);
	if (!channel)
	{
		SEND(service, user, N_("Channel %1% is not registered."), params[1]);
		MT_RET(false);
	}

	if (channel->LOCK_Anarchy(false))
		SEND(service, user, N_("Anarchy for channel %1% has been unlocked."),
			 channel->Name());
	else
		NSEND(service, user, N_("The anarchy setting is globally locked and cannot be unlocked."));

	MT_RET(true);
	MT_EE
}

static bool biUNLOCK_KICKONBAN(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biUNLOCK_KICKONBAN" << service << user << params);

	boost::shared_ptr<StoredChannel> channel = ROOT->data.Get_StoredChannel(params[1]);
	if (!channel)
	{
		SEND(service, user, N_("Channel %1% is not registered."), params[1]);
		MT_RET(false);
	}

	if (channel->LOCK_KickOnBan(false))
		SEND(service, user, N_("Kick on ban for channel %1% has been unlocked."),
			 channel->Name());
	else
		NSEND(service, user, N_("The kick on ban setting is globally locked and cannot be unlocked."));

	MT_RET(true);
	MT_EE
}

static bool biUNLOCK_RESTRICTED(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biUNLOCK_RESTRICTED" << service << user << params);

	boost::shared_ptr<StoredChannel> channel = ROOT->data.Get_StoredChannel(params[1]);
	if (!channel)
	{
		SEND(service, user, N_("Channel %1% is not registered."), params[1]);
		MT_RET(false);
	}

	if (channel->LOCK_Restricted(false))
		SEND(service, user, N_("Restricted for channel %1% has been unlocked."),
			 channel->Name());
	else
		NSEND(service, user, N_("The restricted setting is globally locked and cannot be unlocked."));

	MT_RET(true);
	MT_EE
}

static bool biUNLOCK_CJOIN(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biUNLOCK_CJOIN" << service << user << params);

	boost::shared_ptr<StoredChannel> channel = ROOT->data.Get_StoredChannel(params[1]);
	if (!channel)
	{
		SEND(service, user, N_("Channel %1% is not registered."), params[1]);
		MT_RET(false);
	}

	if (channel->LOCK_CJoin(false))
		SEND(service, user, N_("Channel join for channel %1% has been unlocked."),
			 channel->Name());
	else
		NSEND(service, user, N_("The channel join setting is globally locked and cannot be unlocked."));

	MT_RET(true);
	MT_EE
}

static bool biUNLOCK_BANTIME(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biUNLOCK_BANTIME" << service << user << params);

	boost::shared_ptr<StoredChannel> channel = ROOT->data.Get_StoredChannel(params[1]);
	if (!channel)
	{
		SEND(service, user, N_("Channel %1% is not registered."), params[1]);
		MT_RET(false);
	}

	if (channel->LOCK_BanTime(false))
		SEND(service, user, N_("ban removal time for channel %1% has been unlocked."),
			 channel->Name());
	else
		NSEND(service, user, N_("The ban removal time setting is globally locked and cannot be unlocked."));

	MT_RET(true);
	MT_EE
}

static bool biUNLOCK_PARTTIME(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biUNLOCK_PARTTIME" << service << user << params);

	boost::shared_ptr<StoredChannel> channel = ROOT->data.Get_StoredChannel(params[1]);
	if (!channel)
	{
		SEND(service, user, N_("Channel %1% is not registered."), params[1]);
		MT_RET(false);
	}

	if (channel->LOCK_PartTime(false))
		SEND(service, user, N_("Part remember time for channel %1% has been unlocked."),
			 channel->Name());
	else
		NSEND(service, user, N_("The Part remember time setting is globally locked and cannot be unlocked."));

	MT_RET(true);
	MT_EE
}

static bool biUNLOCK_REVENGE(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biUNLOCK_REVENGE" << service << user << params);

	boost::shared_ptr<StoredChannel> channel = ROOT->data.Get_StoredChannel(params[1]);
	if (!channel)
	{
		SEND(service, user, N_("Channel %1% is not registered."), params[1]);
		MT_RET(false);
	}

	if (channel->LOCK_Revenge(false))
		SEND(service, user, N_("Revenge for channel %1% has been unlocked."),
			 channel->Name());
	else
		NSEND(service, user, N_("The revenge setting is globally locked and cannot be unlocked."));

	MT_RET(true);
	MT_EE
}

static bool biUNLOCK_MLOCK(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biUNLOCK_MLOCK" << service << user << params);

	// TODO: To be implemented.
	SEND(service, user,
		 N_("The %1% command has not yet been implemented."),
		 boost::algorithm::to_upper_copy(params[0]));

	MT_RET(false);
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

	MT_RET(false);
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

	MT_RET(false);
	MT_EE
}

void init_chanserv_functions(Service &serv)
{
	MT_EB
	MT_FUNC("init_chanserv_functions" << serv);

	std::vector<std::string> comm_regd, comm_oper, comm_sop, comm_opersop;
	comm_regd.push_back(ROOT->ConfigValue<std::string>("commserv.regd.name"));
	comm_oper.push_back(ROOT->ConfigValue<std::string>("commserv.oper.name"));
	comm_sop.push_back(ROOT->ConfigValue<std::string>("commserv.sop.name"));
	comm_opersop.push_back(ROOT->ConfigValue<std::string>("commserv.oper.name"));
	comm_opersop.push_back(ROOT->ConfigValue<std::string>("commserv.sop.name"));

	serv.PushCommand("^HELP$", boost::bind(&Service::Help, &serv,
										   _1, _2, _3), 1);

	serv.PushCommand("^REGISTER$", &biREGISTER, 4, comm_regd);
	serv.PushCommand("^DROP$", &biDROP, 2, comm_regd);
	serv.PushCommand("^ID(ENT(IFY)?)?$", &biIDENTIFY, 3);
	serv.PushCommand("^INFO$", &biINFO, 2);
	serv.PushCommand("^SUSPEND$", &biSUSPEND, 3, comm_sop);
	serv.PushCommand("^UN?SUSPEND$", &biUNSUSPEND, 2, comm_sop);
	serv.PushCommand("^SETPASS(WORD)?$", &biSETPASS, 3, comm_sop);

	// Everything from here to FORBID acts on a LIVE channel.
	serv.PushCommand("^MODES?$", &biMODE, 2, comm_regd);
	serv.PushCommand("^OP$", &biOP, 2, comm_regd);
	serv.PushCommand("^DE?-?OP$", &biDEOP, 2, comm_regd);
	serv.PushCommand("^H(ALF)?OP$", &biHOP, 2, comm_regd);
	serv.PushCommand("^DE?-?H(ALF)?OP$", &biDEHOP, 2, comm_regd);
	serv.PushCommand("^VOICE$", &biVOICE, 2, comm_regd);
	serv.PushCommand("^DE?-?VOICE$", &biDEVOICE, 2, comm_regd);
	serv.PushCommand("^(SET)?TOPIC$", &biTOPIC, 2, comm_regd);
	serv.PushCommand("^KICK(USER)?$", &biKICK, 3, comm_regd);
	serv.PushCommand("^(REM(OVE)?|ANON(YMOUS)?KICK(USER)?)$",
					 &biANONKICK, 4, comm_regd);
	serv.PushCommand("^USERS?$", &biUSERS, 2, comm_regd);
	serv.PushCommand("^INVITE$", &biINVITE, 2, comm_regd);
	serv.PushCommand("^UN?BAN$", &biUNBAN, 2, comm_regd);

	serv.PushCommand("^CL(EA)?R$",
					 Service::CommandMerge(serv, 0, 2), 3, comm_regd);
	serv.PushCommand("^CL(EA)?R\\s+USERS?$", &biCLEAR_USERS, 2, comm_regd);
	serv.PushCommand("^CL(EA)?R\\s+OPS?$", &biCLEAR_OPS, 2, comm_regd);
	serv.PushCommand("^CL(EA)?R\\s+H(ALF)?OPS?$", &biCLEAR_HOPS, 2, comm_regd);
	serv.PushCommand("^CL(EA)?R\\s+VOICES?$", &biCLEAR_VOICES, 2, comm_regd);
	serv.PushCommand("^CL(EA)?R\\s+MODES?$", &biCLEAR_MODES, 2, comm_regd);
	serv.PushCommand("^CL(EA)?R\\s+BANS?$", &biCLEAR_BANS, 2, comm_regd);
	serv.PushCommand("^CL(EA)?R\\s+EXEMPTS?$", &biCLEAR_EXEMPTS, 2, comm_regd);
	serv.PushCommand("^CL(EA)?R\\s+ALL$", &biCLEAR_ALL, 2, comm_regd);
	serv.PushCommand("^CL(EA)?R\\s+HELP$",
					 boost::bind(&Service::AuxHelp, &serv,
								 _1, _2, _3), 1, comm_regd);

	serv.PushCommand("^FORBID$",
					 Service::CommandMerge(serv, 0, 1), 2, comm_sop);
	serv.PushCommand("^FORBID\\s+(ADD|NEW|CREATE)$", &biFORBID_ADD, 2, comm_sop);
	serv.PushCommand("^FORBID\\s+(ERASE|DEL(ETE)?|REM(OVE)?)$", &biFORBID_DEL, 2, comm_sop);
	serv.PushCommand("^FORBID\\s+(LIST|VIEW)$", &biFORBID_LIST, 1, comm_sop);
	serv.PushCommand("^FORBID\\s+HELP$",
					 boost::bind(&Service::AuxHelp, &serv,
								 _1, _2, _3), 1, comm_sop);

	serv.PushCommand("^L(V|EVE)L$",
					 Service::CommandMerge(serv, 0, 2), 3, comm_regd);
	serv.PushCommand("^L(V|EVE)L\\s+SET$",
					 &biLEVEL_SET, 3, comm_regd);
	serv.PushCommand("^L(V|EVE)L\\s+(UN?|RE)SET$",
					 &biLEVEL_UNSET, 2, comm_regd);
	serv.PushCommand("^L(V|EVE)L\\s+(LIST|VIEW)$",
					 &biLEVEL_LIST, 2, comm_regd);
	serv.PushCommand("^L(V|EVE)L\\s+HELP$",
					 boost::bind(&Service::AuxHelp, &serv,
								 _1, _2, _3), 1, comm_sop);

	serv.PushCommand("^ACC(ESS)?$",
					 Service::CommandMerge(serv, 0, 2), 3, comm_regd);
	serv.PushCommand("^ACC(ESS)?\\s+(ADD|NEW|CREATE)$",
					 &biACCESS_ADD, 4, comm_regd);
	serv.PushCommand("^ACC(ESS)?\\s+(ERASE|DEL(ETE)?|REM(OVE)?)$",
					 &biACCESS_DEL, 3, comm_regd);
	serv.PushCommand("^ACC(ESS)?\\s+(LIST|VIEW)$",
					 &biACCESS_LIST, 2, comm_regd);
	serv.PushCommand("^ACC(ESS)?\\s+RE(NUMBER|INDEX)$",
					 &biACCESS_REINDEX, 2, comm_regd);
	serv.PushCommand("^ACC(ESS)?\\s+HELP$",
					 boost::bind(&Service::AuxHelp, &serv,
								 _1, _2, _3), 1, comm_regd);

	serv.PushCommand("^A(UTO)?KICK?$",
					 Service::CommandMerge(serv, 0, 2), 3, comm_regd);
	serv.PushCommand("^A(UTO)?KICK?\\s+(ADD|NEW|CREATE)$",
					 &biAKICK_ADD, 4, comm_regd);
	serv.PushCommand("^A(UTO)?KICK?\\s+(ERASE|DEL(ETE)?|REM(OVE)?)$",
					 &biAKICK_DEL, 3, comm_regd);
	serv.PushCommand("^A(UTO)?KICK?\\s+(LIST|VIEW)$",
					 &biAKICK_LIST, 2, comm_regd);
	serv.PushCommand("^A(UTO)?KICK?\\s+RE(NUMBER|INDEX)$",
					 &biAKICK_REINDEX, 2, comm_regd);
	serv.PushCommand("^A(UTO)?KICK?\\s+HELP$",
					 boost::bind(&Service::AuxHelp, &serv,
								 _1, _2, _3), 1, comm_regd);

	serv.PushCommand("^GREET$",
					 Service::CommandMerge(serv, 0, 2), 3, comm_regd);
	serv.PushCommand("^GREET\\s+SET$",
					 &biGREET_SET, 4, comm_regd);
	serv.PushCommand("^GREET\\s+(UN?|RE)SET$",
					 &biGREET_UNSET, 3, comm_regd);
	serv.PushCommand("^GREET\\s+LOCK$",
					 &biGREET_LOCK, 4, comm_regd);
	serv.PushCommand("^GREET\\s+UN?LOCK$",
					 &biGREET_UNLOCK, 3, comm_regd);
	serv.PushCommand("^GREET\\s+(LIST|VIEW)$",
					 &biGREET_LIST, 2, comm_regd);
	serv.PushCommand("^GREET\\s+HELP$",
					 boost::bind(&Service::AuxHelp, &serv,
								 _1, _2, _3), 1, comm_regd);

	serv.PushCommand("^MESSAGE$",
					 Service::CommandMerge(serv, 0, 2), 3, comm_regd);
	serv.PushCommand("^MESSAGE\\s+(ADD|NEW|CREATE)$",
					 &biMESSAGE_ADD, 3, comm_regd);
	serv.PushCommand("^MESSAGE\\s+(ERASE|DEL(ETE)?|REM(OVE)?)$",
					 &biMESSAGE_DEL, 3, comm_regd);
	serv.PushCommand("^MESSAGE\\s+(LIST|VIEW)$",
					 &biMESSAGE_LIST, 2, comm_regd);
	serv.PushCommand("^MESSAGE\\s+RE(NUMBER|INDEX)$",
					 &biMESSAGE_REINDEX, 2, comm_regd);
	serv.PushCommand("^MESSAGE\\s+HELP$",
					 boost::bind(&Service::AuxHelp, &serv,
								 _1, _2, _3), 1, comm_regd);

	serv.PushCommand("^NEWS$",
					 Service::CommandMerge(serv, 0, 2), 3, comm_regd);
	serv.PushCommand("^NEWS\\s+(ADD|NEW|CREATE)$",
					 &biNEWS_ADD, 3, comm_regd);
	serv.PushCommand("^NEWS\\s+(ERASE|DEL(ETE)?|REM(OVE)?)$",
					 &biNEWS_DEL, 3, comm_regd);
	serv.PushCommand("^NEWS\\s+(LIST|VIEW)$",
					 &biNEWS_LIST, 2, comm_regd);
	serv.PushCommand("^NEWS\\s+RE(NUMBER|INDEX)$",
					 &biNEWS_REINDEX, 2, comm_regd);
	serv.PushCommand("^NEWS\\s+HELP$",
					 boost::bind(&Service::AuxHelp, &serv,
								 _1, _2, _3), 1, comm_regd);

	serv.PushCommand("^SET$",
					 Service::CommandMerge(serv, 0, 2), 3, comm_regd);
	serv.PushCommand("^SET\\s+(HEAD|FOUND(ER)?)$",
					 &biSET_FOUNDER, 3, comm_regd);
	serv.PushCommand("^SET\\s+(SUCCESSOR|CO-?FOUND(ER)?)$",
					 &biSET_COFOUNDER, 3, comm_regd);
	serv.PushCommand("^SET\\s+PASS(W(OR)?D)?$",
					 &biSET_PASSWORD, 3, comm_regd);
	serv.PushCommand("^SET\\s+E-?MAIL$",
					 &biSET_EMAIL, 3, comm_regd);
	serv.PushCommand("^SET\\s+(URL|WWW|WEB(PAGE|SITE)?|HTTPS?)$",
					 &biSET_WEBSITE, 3, comm_regd);
	serv.PushCommand("^SET\\s+DESC(RIPT(ION)?)?$",
					 &biSET_DESCRIPTION, 3, comm_regd);
	serv.PushCommand("^SET\\s+KEEPTOPIC$",
					 &biSET_KEEPTOPIC, 3, comm_regd);
	serv.PushCommand("^SET\\s+T(OPIC)?LOCK$",
					 &biSET_TOPICLOCK, 3, comm_regd);
	serv.PushCommand("^SET\\s+PRIV(ATE)?$",
					 &biSET_PRIVATE, 3, comm_regd);
	serv.PushCommand("^SET\\s+SECUREOPS$",
					 &biSET_SECUREOPS, 3, comm_regd);
	serv.PushCommand("^SET\\s+SECURE$",
					 &biSET_SECURE, 3, comm_regd);
	serv.PushCommand("^SET\\s+ANARCHY$",
					 &biSET_ANARCHY, 3, comm_regd);
	serv.PushCommand("^SET\\s+(KOB|KICK[-_]?ON[-_]?BAN)$",
					 &biSET_KICKONBAN, 3, comm_regd);
	serv.PushCommand("^SET\\s+RESTRICT(ED)?$",
					 &biSET_RESTRICTED, 3, comm_regd);
	serv.PushCommand("^SET\\s+(C(HAN(NEL)?)?)?JOIN$",
					 &biSET_CJOIN, 3, comm_regd);
	serv.PushCommand("^SET\\s+BAN[-_]?TIME$",
					 &biSET_BANTIME, 3, comm_regd);
	serv.PushCommand("^SET\\s+PART[-_]?TIME$",
					 &biSET_PARTTIME, 3, comm_regd);
	serv.PushCommand("^SET\\s+REVENGE$",
					 &biSET_REVENGE, 3, comm_regd);
	serv.PushCommand("^SET\\s+M(ODE)?LOCK$",
					 &biSET_MLOCK, 3, comm_regd);
	serv.PushCommand("^SET\\s+HELP$",
					 boost::bind(&Service::AuxHelp, &serv,
								 _1, _2, _3), 1, comm_regd);

	serv.PushCommand("^(UN|RE)SET$",
					 Service::CommandMerge(serv, 0, 2), 3, comm_regd);
	serv.PushCommand("^(UN|RE)SET\\s+(SUCCESSOR|CO-?FOUND(ER)?)$",
					 &biUNSET_COFOUNDER, 2, comm_regd);
	serv.PushCommand("^(UN|RE)SET\\s+E-?MAIL$",
					 &biUNSET_EMAIL, 2, comm_regd);
	serv.PushCommand("^(UN|RE)SET\\s+(URL|WWW|WEB(PAGE|SITE)?|HTTPS?)$",
					 &biUNSET_WEBSITE, 2, comm_regd);
	serv.PushCommand("^(UN|RE)SET\\s+KEEPTOPIC$",
					 &biUNSET_KEEPTOPIC, 2, comm_regd);
	serv.PushCommand("^(UN|RE)SET\\s+T(OPIC)?LOCK$",
					 &biUNSET_TOPICLOCK, 2, comm_regd);
	serv.PushCommand("^(UN|RE)SET\\s+PRIV(ATE)?$",
					 &biUNSET_PRIVATE, 2, comm_regd);
	serv.PushCommand("^(UN|RE)SET\\s+SECUREOPS$",
					 &biUNSET_SECUREOPS, 2, comm_regd);
	serv.PushCommand("^(UN|RE)SET\\s+SECURE$",
					 &biUNSET_SECURE, 2, comm_regd);
	serv.PushCommand("^(UN|RE)SET\\s+ANARCHY$",
					 &biUNSET_ANARCHY, 2, comm_regd);
	serv.PushCommand("^(UN|RE)SET\\s+(KOB|KICK[-_]?ON[-_]?BAN)$",
					 &biUNSET_KICKONBAN, 2, comm_regd);
	serv.PushCommand("^(UN|RE)SET\\s+RESTRICT(ED)?$",
					 &biUNSET_RESTRICTED, 2, comm_regd);
	serv.PushCommand("^(UN|RE)SET\\s+(C(HAN(NEL)?)?)?JOIN$",
					 &biUNSET_CJOIN, 2, comm_regd);
	serv.PushCommand("^(UN|RE)SET\\s+BAN[-_]?TIME$",
					 &biUNSET_BANTIME, 2, comm_regd);
	serv.PushCommand("^(UN|RE)SET\\s+PART[-_]?TIME$",
					 &biUNSET_PARTTIME, 2, comm_regd);
	serv.PushCommand("^(UN|RE)SET\\s+REVENGE$",
					 &biUNSET_REVENGE, 2, comm_regd);
	serv.PushCommand("^(UN|RE)SET\\s+M(ODE)?LOCK$",
					 &biUNSET_MLOCK, 2, comm_regd);
	serv.PushCommand("^(UN|RE)SET\\s+HELP$",
					 boost::bind(&Service::AuxHelp, &serv,
								 _1, _2, _3), 1, comm_regd);

	serv.PushCommand("^SET\\s+COMMENT$",
					 &biSET_COMMENT, 3, comm_opersop);
	serv.PushCommand("^SET\\s+NO?EX(PIRE)?$",
					 &biSET_NOEXPIRE, 3, comm_sop);
	serv.PushCommand("^(UN|RE)SET\\s+COMMENT$",
					 &biUNSET_COMMENT, 2, comm_opersop);
	serv.PushCommand("^(UN|RE)SET\\s+NO?EX(PIRE)?$",
					 &biUNSET_NOEXPIRE, 2, comm_sop);

	serv.PushCommand("^LOCK$",
					 Service::CommandMerge(serv, 0, 2), 3, comm_sop);
	serv.PushCommand("^LOCK\\s+KEEPTOPIC$",
					 &biLOCK_KEEPTOPIC, 3, comm_sop);
	serv.PushCommand("^LOCK\\s+T(OPIC)?LOCK$",
					 &biLOCK_TOPICLOCK, 3, comm_sop);
	serv.PushCommand("^LOCK\\s+PRIV(ATE)?$",
					 &biLOCK_PRIVATE, 3, comm_sop);
	serv.PushCommand("^LOCK\\s+SECUREOPS$",
					 &biLOCK_SECUREOPS, 3, comm_sop);
	serv.PushCommand("^LOCK\\s+SECURE$",
					 &biLOCK_SECURE, 3, comm_sop);
	serv.PushCommand("^LOCK\\s+ANARCHY$",
					 &biLOCK_ANARCHY, 3, comm_sop);
	serv.PushCommand("^LOCK\\s+(KOB|KICK[-_]?ON[-_]?BAN)$",
					 &biLOCK_KICKONBAN, 3, comm_sop);
	serv.PushCommand("^LOCK\\s+RESTRICT(ED)?$",
					 &biLOCK_RESTRICTED, 3, comm_sop);
	serv.PushCommand("^LOCK\\s+(C(HAN(NEL)?)?)?JOIN$",
					 &biLOCK_CJOIN, 3, comm_sop);
	serv.PushCommand("^LOCK\\s+BAN[-_]?TIME$",
					 &biLOCK_BANTIME, 3, comm_sop);
	serv.PushCommand("^LOCK\\s+PART[-_]?TIME$",
					 &biLOCK_PARTTIME, 3, comm_sop);
	serv.PushCommand("^LOCK\\s+REVENGE$",
					 &biLOCK_REVENGE, 3, comm_sop);
	serv.PushCommand("^LOCK\\s+M(ODE)?LOCK$",
					 &biLOCK_MLOCK, 3, comm_sop);
	serv.PushCommand("^LOCK\\s+HELP$",
					 boost::bind(&Service::AuxHelp, &serv,
								 _1, _2, _3), 1, comm_sop);

	serv.PushCommand("^UN?LOCK$",
					 Service::CommandMerge(serv, 0, 2), 3, comm_sop);
	serv.PushCommand("^UN?LOCK\\s+KEEPTOPIC$",
					 &biUNLOCK_KEEPTOPIC, 2, comm_sop);
	serv.PushCommand("^UN?LOCK\\s+T(OPIC)?UNLOCK$",
					 &biUNLOCK_TOPICUNLOCK, 2, comm_sop);
	serv.PushCommand("^UN?LOCK\\s+PRIV(ATE)?$",
					 &biUNLOCK_PRIVATE, 2, comm_sop);
	serv.PushCommand("^UN?LOCK\\s+SECUREOPS$",
					 &biUNLOCK_SECUREOPS, 2, comm_sop);
	serv.PushCommand("^UN?LOCK\\s+SECURE$",
					 &biUNLOCK_SECURE, 2, comm_sop);
	serv.PushCommand("^UN?LOCK\\s+ANARCHY$",
					 &biUNLOCK_ANARCHY, 2, comm_sop);
	serv.PushCommand("^UN?LOCK\\s+(KOB|KICK[-_]?ON[-_]?BAN)$",
					 &biUNLOCK_KICKONBAN, 2, comm_sop);
	serv.PushCommand("^UN?LOCK\\s+RESTRICT(ED)?$",
					 &biUNLOCK_RESTRICTED, 2, comm_sop);
	serv.PushCommand("^UN?LOCK\\s+(C(HAN(NEL)?)?)?JOIN$",
					 &biUNLOCK_CJOIN, 2, comm_sop);
	serv.PushCommand("^UN?LOCK\\s+BAN[-_]?TIME$",
					 &biUNLOCK_BANTIME, 2, comm_sop);
	serv.PushCommand("^UN?LOCK\\s+PART[-_]?TIME$",
					 &biUNLOCK_PARTTIME, 2, comm_sop);
	serv.PushCommand("^UN?LOCK\\s+REVENGE$",
					 &biUNLOCK_REVENGE, 2, comm_sop);
	serv.PushCommand("^UN?LOCK\\s+M(ODE)?LOCK$",
					 &biUNLOCK_MLOCK, 2, comm_sop);
	serv.PushCommand("^UN?LOCK\\s+HELP$",
					 boost::bind(&Service::AuxHelp, &serv,
								 _1, _2, _3), 1, comm_sop);

	// These commands don't operate on any nickname.
	serv.PushCommand("^(STORED)?LIST$", &biSTOREDLIST, 1);
	serv.PushCommand("^LIVE(LIST)?$", &biLIVELIST, 1, comm_oper);

	MT_EE
}

