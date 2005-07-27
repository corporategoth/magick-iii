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
#include "livechannel.h"
#include "storednick.h"
#include "storeduser.h"
#include "storedchannel.h"
#include "committee.h"

#include <mantra/core/trace.h>

static bool biREGISTER(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biREGISTER" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);

	if (params.size() < 4)
	{
		SEND(service, user,
			 N_("Insufficient parameters for %1% command."),
			 boost::algorithm::to_upper_copy(params[0]));
		MT_RET(false);
	}

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

static bool biDROP(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biDROP" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);

	if (params.size() < 2)
	{
		SEND(service, user,
			 N_("Insufficient parameters for %1% command."),
			 boost::algorithm::to_upper_copy(params[0]));
		MT_RET(false);
	}

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

static bool biIDENTIFY(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biIDENTIFY" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);

	if (params.size() < 3)
	{
		SEND(service, user,
			 N_("Insufficient parameters for %1% command."),
			 boost::algorithm::to_upper_copy(params[0]));
		MT_RET(false);
	}

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

	MT_RET(false);
	MT_EE
}

static bool biINFO(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biINFO" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);

	if (params.size() < 2)
	{
		SEND(service, user,
			 N_("Insufficient parameters for %1% command."),
			 boost::algorithm::to_upper_copy(params[0]));
		MT_RET(false);
	}

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

	MT_RET(false);
	MT_EE
}

static bool biSUSPEND(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biSUSPEND" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biUNSUSPEND(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biUNSUSPEND" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biSETPASS(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biSETPASS" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biMODE(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biMODE" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biOP(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biOP" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biDEOP(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biDEOP" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biHOP(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biHOP" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biDEHOP(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biDEHOP" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biVOICE(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biVOICE" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biDEVOICE(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biDEVOICE" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biTOPIC(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biTOPIC" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biKICK(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biKICK" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biANONKICK(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biANONKICK" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biUSERS(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biUSERS" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biINVITE(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biINVITE" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biUNBAN(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biUNBAN" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biCLEAR_USERS(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biCLEAR_USERS" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biCLEAR_OPS(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biCLEAR_OPS" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biCLEAR_HOPS(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biCLEAR_HOPS" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biCLEAR_VOICES(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biCLEAR_VOICES" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biCLEAR_MODES(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biCLEAR_MODES" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biCLEAR_BANS(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biCLEAR_BANS" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biCLEAR_ALL(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biCLEAR_ALL" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biFORBID_ADD(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biFORBID_ADD" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biFORBID_DEL(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biFORBID_DEL" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biFORBID_LIST(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biFORBID_LIST" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biLEVEL_SET(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biLEVEL_SET" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biLEVEL_UNSET(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biLEVEL_UNSET" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biLEVEL_LIST(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biLEVEL_LIST" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biACCESS_ADD(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biACCESS_ADD" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biACCESS_DEL(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biACCESS_DEL" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biACCESS_LIST(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biACCESS_LIST" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biACCESS_REINDEX(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biACCESS_REINDEX" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biAKICK_ADD(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biAKICK_ADD" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biAKICK_DEL(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biAKICK_DEL" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biAKICK_LIST(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biAKICK_LIST" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biAKICK_REINDEX(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biAKICK_REINDEX" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biGREET_SET(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biGREET_SET" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biGREET_UNSET(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biGREET_UNSET" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biGREET_LOCK(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biGREET_LOCK" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biGREET_UNLOCK(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biGREET_UNLOCK" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biGREET_LIST(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biGREET_LIST" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biMESSAGE_ADD(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biMESSAGE_ADD" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biMESSAGE_DEL(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biMESSAGE_DEL" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biMESSAGE_LIST(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biMESSAGE_LIST" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biMESSAGE_REINDEX(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biMESSAGE_REINDEX" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biNEWS_ADD(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biNEWS_ADD" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biNEWS_DEL(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biNEWS_DEL" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biNEWS_LIST(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biNEWS_LIST" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biNEWS_REINDEX(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biNEWS_REINDEX" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biSET_FOUNDER(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biSET_FOUNDER" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biSET_COFOUNDER(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biSET_COFOUNDER" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biSET_PASSWORD(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biSET_PASSWORD" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biSET_EMAIL(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biSET_EMAIL" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biSET_WEBSITE(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biSET_WEBSITE" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biSET_DESCRIPTION(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biSET_DESCRIPTION" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biSET_KEEPTOPIC(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biSET_KEEPTOPIC" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biSET_TOPICLOCK(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biSET_TOPICLOCK" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biSET_PRIVATE(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biSET_PRIVATE" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biSET_SECUREOPS(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biSET_SECUREOPS" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biSET_SECURE(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biSET_SECURE" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biSET_ANARCHY(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biSET_ANARCHY" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biSET_KICKONBAN(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biSET_KICKONBAN" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biSET_RESTRICTED(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biSET_RESTRICTED" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biSET_CJOIN(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biSET_CJOIN" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biSET_BANTIME(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biSET_BANTIME" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biSET_PARTTIME(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biSET_PARTTIME" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biSET_REVENGE(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biSET_REVENGE" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biSET_MLOCK(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biSET_MLOCK" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biUNSET_COFOUNDER(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biUNSET_COFOUNDER" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biUNSET_EMAIL(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biUNSET_EMAIL" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biUNSET_WEBSITE(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biUNSET_WEBSITE" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biUNSET_KEEPTOPIC(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biUNSET_KEEPTOPIC" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biUNSET_TOPICLOCK(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biUNSET_TOPICLOCK" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biUNSET_PRIVATE(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biUNSET_PRIVATE" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biUNSET_SECUREOPS(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biUNSET_SECUREOPS" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biUNSET_SECURE(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biUNSET_SECURE" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biUNSET_ANARCHY(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biUNSET_ANARCHY" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biUNSET_KICKONBAN(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biUNSET_KICKONBAN" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biUNSET_RESTRICTED(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biUNSET_RESTRICTED" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biUNSET_CJOIN(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biUNSET_CJOIN" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biUNSET_BANTIME(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biUNSET_BANTIME" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biUNSET_PARTTIME(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biUNSET_PARTTIME" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biUNSET_REVENGE(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biUNSET_REVENGE" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biUNSET_MLOCK(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biUNSET_MLOCK" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biSET_COMMENT(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biSET_COMMENT" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biSET_NOEXPIRE(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biSET_NOEXPIRE" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biUNSET_COMMENT(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biUNSET_COMMENT" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biUNSET_NOEXPIRE(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biUNSET_NOEXPIRE" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biLOCK_KEEPTOPIC(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biLOCK_KEEPTOPIC" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biLOCK_TOPICLOCK(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biLOCK_TOPICLOCK" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biLOCK_PRIVATE(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biLOCK_PRIVATE" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biLOCK_SECUREOPS(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biLOCK_SECUREOPS" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biLOCK_SECURE(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biLOCK_SECURE" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biLOCK_ANARCHY(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biLOCK_ANARCHY" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biLOCK_KICKONBAN(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biLOCK_KICKONBAN" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biLOCK_RESTRICTED(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biLOCK_RESTRICTED" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biLOCK_CJOIN(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biLOCK_CJOIN" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biLOCK_BANTIME(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biLOCK_BANTIME" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biLOCK_PARTTIME(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biLOCK_PARTTIME" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biLOCK_REVENGE(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biLOCK_REVENGE" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biLOCK_MLOCK(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biLOCK_MLOCK" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biUNLOCK_KEEPTOPIC(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biUNLOCK_KEEPTOPIC" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biUNLOCK_TOPICUNLOCK(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biUNLOCK_TOPICUNLOCK" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biUNLOCK_PRIVATE(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biUNLOCK_PRIVATE" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biUNLOCK_SECUREOPS(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biUNLOCK_SECUREOPS" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biUNLOCK_SECURE(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biUNLOCK_SECURE" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biUNLOCK_ANARCHY(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biUNLOCK_ANARCHY" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biUNLOCK_KICKONBAN(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biUNLOCK_KICKONBAN" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biUNLOCK_RESTRICTED(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biUNLOCK_RESTRICTED" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biUNLOCK_CJOIN(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biUNLOCK_CJOIN" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biUNLOCK_BANTIME(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biUNLOCK_BANTIME" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biUNLOCK_PARTTIME(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biUNLOCK_PARTTIME" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biUNLOCK_REVENGE(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biUNLOCK_REVENGE" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biUNLOCK_MLOCK(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biUNLOCK_MLOCK" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biSTOREDLIST(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biSTOREDLIST" << service << user << params);

	MT_RET(false);
	MT_EE
}

static bool biLIVELIST(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biLIVELIST" << service << user << params);

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
										   _1, _2, _3));

	serv.PushCommand("^REGISTER$", &biREGISTER, comm_regd);
	serv.PushCommand("^DROP$", &biDROP, comm_regd);
	serv.PushCommand("^ID(ENT(IFY)?)?$", &biIDENTIFY);
	serv.PushCommand("^INFO$", &biINFO);
	serv.PushCommand("^SUSPEND$", &biSUSPEND, comm_sop);
	serv.PushCommand("^UN?SUSPEND$", &biUNSUSPEND, comm_sop);
	serv.PushCommand("^SETPASS(WORD)?$", &biSETPASS, comm_sop);

	// Everything from here to FORBID acts on a LIVE channel.
	serv.PushCommand("^MODES?$", &biMODE, comm_regd);
	serv.PushCommand("^OP$", &biOP, comm_regd);
	serv.PushCommand("^DE?-?OP$", &biDEOP, comm_regd);
	serv.PushCommand("^H(ALF)?OP$", &biHOP, comm_regd);
	serv.PushCommand("^DE?-?H(ALF)?OP$", &biDEHOP, comm_regd);
	serv.PushCommand("^VOICE$", &biVOICE, comm_regd);
	serv.PushCommand("^DE?-?VOICE$", &biDEVOICE, comm_regd);
	serv.PushCommand("^(SET)?TOPIC$", &biTOPIC, comm_regd);
	serv.PushCommand("^KICK(USER)?$", &biKICK, comm_regd);
	serv.PushCommand("^(REM(OVE)?|ANON(YMOUS)?KICK(USER)?)$",
					 &biANONKICK, comm_regd);
	serv.PushCommand("^USERS?$", &biUSERS, comm_regd);
	serv.PushCommand("^INVITE$", &biINVITE, comm_regd);
	serv.PushCommand("^UN?BAN$", &biUNBAN, comm_regd);

	serv.PushCommand("^CL(EA)?R$",
					 Service::CommandMerge(serv, 0, 2), comm_regd);
	serv.PushCommand("^CL(EA)?R\\s+USERS?$", &biCLEAR_USERS, comm_regd);
	serv.PushCommand("^CL(EA)?R\\s+OPS?$", &biCLEAR_OPS, comm_regd);
	serv.PushCommand("^CL(EA)?R\\s+H(ALF)?OPS?$", &biCLEAR_HOPS, comm_regd);
	serv.PushCommand("^CL(EA)?R\\s+VOICES?$", &biCLEAR_VOICES, comm_regd);
	serv.PushCommand("^CL(EA)?R\\s+MODES?$", &biCLEAR_MODES, comm_regd);
	serv.PushCommand("^CL(EA)?R\\s+BANS?$", &biCLEAR_BANS, comm_regd);
	serv.PushCommand("^CL(EA)?R\\s+ALL$", &biCLEAR_ALL, comm_regd);
	serv.PushCommand("^CL(EA)?R\\s+HELP$",
					 boost::bind(&Service::AuxHelp, &serv,
								 _1, _2, _3), comm_regd);

	serv.PushCommand("^FORBID$",
					 Service::CommandMerge(serv, 0, 1), comm_sop);
	serv.PushCommand("^FORBID\\s+(ADD|NEW|CREATE)$", &biFORBID_ADD, comm_sop);
	serv.PushCommand("^FORBID\\s+(ERASE|DEL(ETE)?|REM(OVE)?)$", &biFORBID_DEL, comm_sop);
	serv.PushCommand("^FORBID\\s+(LIST|VIEW)$", &biFORBID_LIST, comm_sop);
	serv.PushCommand("^FORBID\\s+HELP$",
					 boost::bind(&Service::AuxHelp, &serv,
								 _1, _2, _3), comm_sop);

	serv.PushCommand("^L(V|EVE)L$",
					 Service::CommandMerge(serv, 0, 2), comm_regd);
	serv.PushCommand("^L(V|EVE)L\\s+SET$",
					 &biLEVEL_SET, comm_regd);
	serv.PushCommand("^L(V|EVE)L\\s+(UN?|RE)SET$",
					 &biLEVEL_UNSET, comm_regd);
	serv.PushCommand("^L(V|EVE)L\\s+(LIST|VIEW)$",
					 &biLEVEL_LIST, comm_regd);
	serv.PushCommand("^L(V|EVE)L\\s+HELP$",
					 boost::bind(&Service::AuxHelp, &serv,
								 _1, _2, _3), comm_sop);

	serv.PushCommand("^ACC(ESS)?$",
					 Service::CommandMerge(serv, 0, 2), comm_regd);
	serv.PushCommand("^ACC(ESS)?\\s+(ADD|NEW|CREATE)$",
					 &biACCESS_ADD, comm_regd);
	serv.PushCommand("^ACC(ESS)?\\s+(ERASE|DEL(ETE)?|REM(OVE)?)$",
					 &biACCESS_DEL, comm_regd);
	serv.PushCommand("^ACC(ESS)?\\s+(LIST|VIEW)$",
					 &biACCESS_LIST, comm_regd);
	serv.PushCommand("^ACC(ESS)?\\s+RE(NUMBER|INDEX)$",
					 &biACCESS_REINDEX, comm_regd);
	serv.PushCommand("^ACC(ESS)?\\s+HELP$",
					 boost::bind(&Service::AuxHelp, &serv,
								 _1, _2, _3), comm_regd);

	serv.PushCommand("^A(UTO)?KICK?$",
					 Service::CommandMerge(serv, 0, 2), comm_regd);
	serv.PushCommand("^A(UTO)?KICK?\\s+(ADD|NEW|CREATE)$",
					 &biAKICK_ADD, comm_regd);
	serv.PushCommand("^A(UTO)?KICK?\\s+(ERASE|DEL(ETE)?|REM(OVE)?)$",
					 &biAKICK_DEL, comm_regd);
	serv.PushCommand("^A(UTO)?KICK?\\s+(LIST|VIEW)$",
					 &biAKICK_LIST, comm_regd);
	serv.PushCommand("^A(UTO)?KICK?\\s+RE(NUMBER|INDEX)$",
					 &biAKICK_REINDEX, comm_regd);
	serv.PushCommand("^A(UTO)?KICK?\\s+HELP$",
					 boost::bind(&Service::AuxHelp, &serv,
								 _1, _2, _3), comm_regd);

	serv.PushCommand("^GREET$",
					 Service::CommandMerge(serv, 0, 2), comm_regd);
	serv.PushCommand("^GREET\\s+SET$",
					 &biGREET_SET, comm_regd);
	serv.PushCommand("^GREET\\s+(UN?|RE)SET$",
					 &biGREET_UNSET, comm_regd);
	serv.PushCommand("^GREET\\s+LOCK$",
					 &biGREET_LOCK, comm_regd);
	serv.PushCommand("^GREET\\s+UN?LOCK$",
					 &biGREET_UNLOCK, comm_regd);
	serv.PushCommand("^GREET\\s+(LIST|VIEW)$",
					 &biGREET_LIST, comm_regd);
	serv.PushCommand("^GREET\\s+HELP$",
					 boost::bind(&Service::AuxHelp, &serv,
								 _1, _2, _3), comm_regd);

	serv.PushCommand("^MESSAGE$",
					 Service::CommandMerge(serv, 0, 2), comm_regd);
	serv.PushCommand("^MESSAGE\\s+(ADD|NEW|CREATE)$",
					 &biMESSAGE_ADD, comm_regd);
	serv.PushCommand("^MESSAGE\\s+(ERASE|DEL(ETE)?|REM(OVE)?)$",
					 &biMESSAGE_DEL, comm_regd);
	serv.PushCommand("^MESSAGE\\s+(LIST|VIEW)$",
					 &biMESSAGE_LIST, comm_regd);
	serv.PushCommand("^MESSAGE\\s+RE(NUMBER|INDEX)$",
					 &biMESSAGE_REINDEX, comm_regd);
	serv.PushCommand("^MESSAGE\\s+HELP$",
					 boost::bind(&Service::AuxHelp, &serv,
								 _1, _2, _3), comm_regd);

	serv.PushCommand("^NEWS$",
					 Service::CommandMerge(serv, 0, 2), comm_regd);
	serv.PushCommand("^NEWS\\s+(ADD|NEW|CREATE)$",
					 &biNEWS_ADD, comm_regd);
	serv.PushCommand("^NEWS\\s+(ERASE|DEL(ETE)?|REM(OVE)?)$",
					 &biNEWS_DEL, comm_regd);
	serv.PushCommand("^NEWS\\s+(LIST|VIEW)$",
					 &biNEWS_LIST, comm_regd);
	serv.PushCommand("^NEWS\\s+RE(NUMBER|INDEX)$",
					 &biNEWS_REINDEX, comm_regd);
	serv.PushCommand("^NEWS\\s+HELP$",
					 boost::bind(&Service::AuxHelp, &serv,
								 _1, _2, _3), comm_regd);

	serv.PushCommand("^SET$",
					 Service::CommandMerge(serv, 0, 2), comm_regd);
	serv.PushCommand("^SET\\s+(HEAD|FOUND(ER)?)$",
					 &biSET_FOUNDER, comm_regd);
	serv.PushCommand("^SET\\s+(SUCCESSOR|CO-?FOUND(ER)?)$",
					 &biSET_COFOUNDER, comm_regd);
	serv.PushCommand("^SET\\s+PASS(W(OR)?D)?$",
					 &biSET_PASSWORD, comm_regd);
	serv.PushCommand("^SET\\s+E-?MAIL$",
					 &biSET_EMAIL, comm_regd);
	serv.PushCommand("^SET\\s+(URL|WWW|WEB(PAGE|SITE)?|HTTPS?)$",
					 &biSET_WEBSITE, comm_regd);
	serv.PushCommand("^SET\\s+DESC(RIPT(ION)?)?$",
					 &biSET_DESCRIPTION, comm_regd);
	serv.PushCommand("^SET\\s+KEEPTOPIC$",
					 &biSET_KEEPTOPIC, comm_regd);
	serv.PushCommand("^SET\\s+T(OPIC)?LOCK$",
					 &biSET_TOPICLOCK, comm_regd);
	serv.PushCommand("^SET\\s+PRIV(ATE)?$",
					 &biSET_PRIVATE, comm_regd);
	serv.PushCommand("^SET\\s+SECUREOPS$",
					 &biSET_SECUREOPS, comm_regd);
	serv.PushCommand("^SET\\s+SECURE$",
					 &biSET_SECURE, comm_regd);
	serv.PushCommand("^SET\\s+ANARCHY$",
					 &biSET_ANARCHY, comm_regd);
	serv.PushCommand("^SET\\s+(KOB|KICK[-_]?ON[-_]?BAN)$",
					 &biSET_KICKONBAN, comm_regd);
	serv.PushCommand("^SET\\s+RESTRICT(ED)?$",
					 &biSET_RESTRICTED, comm_regd);
	serv.PushCommand("^SET\\s+(C(HAN(NEL)?)?)?JOIN$",
					 &biSET_CJOIN, comm_regd);
	serv.PushCommand("^SET\\s+BAN[-_]?TIME$",
					 &biSET_BANTIME, comm_regd);
	serv.PushCommand("^SET\\s+PART[-_]?TIME$",
					 &biSET_PARTTIME, comm_regd);
	serv.PushCommand("^SET\\s+REVENGE$",
					 &biSET_REVENGE, comm_regd);
	serv.PushCommand("^SET\\s+M(ODE)?LOCK$",
					 &biSET_MLOCK, comm_regd);
	serv.PushCommand("^SET\\s+HELP$",
					 boost::bind(&Service::AuxHelp, &serv,
								 _1, _2, _3), comm_regd);

	serv.PushCommand("^(UN|RE)SET$",
					 Service::CommandMerge(serv, 0, 2), comm_regd);
	serv.PushCommand("^(UN|RE)SET\\s+(SUCCESSOR|CO-?FOUND(ER)?)$",
					 &biUNSET_COFOUNDER, comm_regd);
	serv.PushCommand("^(UN|RE)SET\\s+E-?MAIL$",
					 &biUNSET_EMAIL, comm_regd);
	serv.PushCommand("^(UN|RE)SET\\s+(URL|WWW|WEB(PAGE|SITE)?|HTTPS?)$",
					 &biUNSET_WEBSITE, comm_regd);
	serv.PushCommand("^(UN|RE)SET\\s+KEEPTOPIC$",
					 &biUNSET_KEEPTOPIC, comm_regd);
	serv.PushCommand("^(UN|RE)SET\\s+T(OPIC)?LOCK$",
					 &biUNSET_TOPICLOCK, comm_regd);
	serv.PushCommand("^(UN|RE)SET\\s+PRIV(ATE)?$",
					 &biUNSET_PRIVATE, comm_regd);
	serv.PushCommand("^(UN|RE)SET\\s+SECUREOPS$",
					 &biUNSET_SECUREOPS, comm_regd);
	serv.PushCommand("^(UN|RE)SET\\s+SECURE$",
					 &biUNSET_SECURE, comm_regd);
	serv.PushCommand("^(UN|RE)SET\\s+ANARCHY$",
					 &biUNSET_ANARCHY, comm_regd);
	serv.PushCommand("^(UN|RE)SET\\s+(KOB|KICK[-_]?ON[-_]?BAN)$",
					 &biUNSET_KICKONBAN, comm_regd);
	serv.PushCommand("^(UN|RE)SET\\s+RESTRICT(ED)?$",
					 &biUNSET_RESTRICTED, comm_regd);
	serv.PushCommand("^(UN|RE)SET\\s+(C(HAN(NEL)?)?)?JOIN$",
					 &biUNSET_CJOIN, comm_regd);
	serv.PushCommand("^(UN|RE)SET\\s+BAN[-_]?TIME$",
					 &biUNSET_BANTIME, comm_regd);
	serv.PushCommand("^(UN|RE)SET\\s+PART[-_]?TIME$",
					 &biUNSET_PARTTIME, comm_regd);
	serv.PushCommand("^(UN|RE)SET\\s+REVENGE$",
					 &biUNSET_REVENGE, comm_regd);
	serv.PushCommand("^(UN|RE)SET\\s+M(ODE)?LOCK$",
					 &biUNSET_MLOCK, comm_regd);
	serv.PushCommand("^(UN|RE)SET\\s+HELP$",
					 boost::bind(&Service::AuxHelp, &serv,
								 _1, _2, _3), comm_regd);

	serv.PushCommand("^SET\\s+COMMENT$",
					 &biSET_COMMENT, comm_opersop);
	serv.PushCommand("^SET\\s+NO?EX(PIRE)?$",
					 &biSET_NOEXPIRE, comm_sop);
	serv.PushCommand("^(UN|RE)SET\\s+COMMENT$",
					 &biUNSET_COMMENT, comm_opersop);
	serv.PushCommand("^(UN|RE)SET\\s+NO?EX(PIRE)?$",
					 &biUNSET_NOEXPIRE, comm_sop);

	serv.PushCommand("^LOCK$",
					 Service::CommandMerge(serv, 0, 2), comm_sop);
	serv.PushCommand("^LOCK\\s+KEEPTOPIC$",
					 &biLOCK_KEEPTOPIC, comm_sop);
	serv.PushCommand("^LOCK\\s+T(OPIC)?LOCK$",
					 &biLOCK_TOPICLOCK, comm_sop);
	serv.PushCommand("^LOCK\\s+PRIV(ATE)?$",
					 &biLOCK_PRIVATE, comm_sop);
	serv.PushCommand("^LOCK\\s+SECUREOPS$",
					 &biLOCK_SECUREOPS, comm_sop);
	serv.PushCommand("^LOCK\\s+SECURE$",
					 &biLOCK_SECURE, comm_sop);
	serv.PushCommand("^LOCK\\s+ANARCHY$",
					 &biLOCK_ANARCHY, comm_sop);
	serv.PushCommand("^LOCK\\s+(KOB|KICK[-_]?ON[-_]?BAN)$",
					 &biLOCK_KICKONBAN, comm_sop);
	serv.PushCommand("^LOCK\\s+RESTRICT(ED)?$",
					 &biLOCK_RESTRICTED, comm_sop);
	serv.PushCommand("^LOCK\\s+(C(HAN(NEL)?)?)?JOIN$",
					 &biLOCK_CJOIN, comm_sop);
	serv.PushCommand("^LOCK\\s+BAN[-_]?TIME$",
					 &biLOCK_BANTIME, comm_sop);
	serv.PushCommand("^LOCK\\s+PART[-_]?TIME$",
					 &biLOCK_PARTTIME, comm_sop);
	serv.PushCommand("^LOCK\\s+REVENGE$",
					 &biLOCK_REVENGE, comm_sop);
	serv.PushCommand("^LOCK\\s+M(ODE)?LOCK$",
					 &biLOCK_MLOCK, comm_sop);
	serv.PushCommand("^LOCK\\s+HELP$",
					 boost::bind(&Service::AuxHelp, &serv,
								 _1, _2, _3), comm_sop);

	serv.PushCommand("^UN?LOCK$",
					 Service::CommandMerge(serv, 0, 2), comm_sop);
	serv.PushCommand("^UN?LOCK\\s+KEEPTOPIC$",
					 &biUNLOCK_KEEPTOPIC, comm_sop);
	serv.PushCommand("^UN?LOCK\\s+T(OPIC)?UNLOCK$",
					 &biUNLOCK_TOPICUNLOCK, comm_sop);
	serv.PushCommand("^UN?LOCK\\s+PRIV(ATE)?$",
					 &biUNLOCK_PRIVATE, comm_sop);
	serv.PushCommand("^UN?LOCK\\s+SECUREOPS$",
					 &biUNLOCK_SECUREOPS, comm_sop);
	serv.PushCommand("^UN?LOCK\\s+SECURE$",
					 &biUNLOCK_SECURE, comm_sop);
	serv.PushCommand("^UN?LOCK\\s+ANARCHY$",
					 &biUNLOCK_ANARCHY, comm_sop);
	serv.PushCommand("^UN?LOCK\\s+(KOB|KICK[-_]?ON[-_]?BAN)$",
					 &biUNLOCK_KICKONBAN, comm_sop);
	serv.PushCommand("^UN?LOCK\\s+RESTRICT(ED)?$",
					 &biUNLOCK_RESTRICTED, comm_sop);
	serv.PushCommand("^UN?LOCK\\s+(C(HAN(NEL)?)?)?JOIN$",
					 &biUNLOCK_CJOIN, comm_sop);
	serv.PushCommand("^UN?LOCK\\s+BAN[-_]?TIME$",
					 &biUNLOCK_BANTIME, comm_sop);
	serv.PushCommand("^UN?LOCK\\s+PART[-_]?TIME$",
					 &biUNLOCK_PARTTIME, comm_sop);
	serv.PushCommand("^UN?LOCK\\s+REVENGE$",
					 &biUNLOCK_REVENGE, comm_sop);
	serv.PushCommand("^UN?LOCK\\s+M(ODE)?LOCK$",
					 &biUNLOCK_MLOCK, comm_sop);
	serv.PushCommand("^UN?LOCK\\s+HELP$",
					 boost::bind(&Service::AuxHelp, &serv,
								 _1, _2, _3), comm_sop);

	// These commands don't operate on any nickname.
	serv.PushCommand("^(STORED)?LIST$", &biSTOREDLIST);
	serv.PushCommand("^LIVE(LIST)?$", &biLIVELIST, comm_oper);

	MT_EE
}

