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
#define RCSID(x,y) const char *rcsid_magick__message_cpp_ ## x () { return y; }
RCSID(magick__message_cpp, "@(#)$Id$");

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
#include "text.h"

#include <mantra/core/trace.h>
#include <mantra/file/mfile.h>

#include <sstream>

Message::func_map_t Message::func_map;

// This function CAN NOT BE TRACED!
std::string Message::print() const
{
	std::stringstream os;
	if (!source_.empty())
		os << ":" << source_ << " ";
	os << id_;
	if (!params_.empty())
	{
		size_t i;
		for (i=0; i<params_.size()-1; ++i)
			os << " " << params_[i];
		if (params_[i].find(' ') != std::string::npos)
			os << " :";
		else
			os << " ";
		os << params_[i];
	}
		
	return os.str();
}

bool Message::Process()
{
	MT_EB
	MT_FUNC("Message::Process");

	LOG(Debug + 1, _("Processing message: %1%."), print());

	bool rv = true;
	func_map_t::const_iterator i = func_map.find(id_);
	if (i != func_map.end())
	{
		if (i->second)
			rv = i->second(*this);
	}
	else
		LOG(Warning, _("No message handler found for %1%."), id_);

	MT_RET(rv);
	MT_EE
}

struct LogMessage
{
	mantra::LogLevel::Level_t level;
public:
	LogMessage(mantra::LogLevel::Level_t l) : level(l) {}
	bool operator()(const Message &m)
	{
		MT_EB
		MT_FUNC("LogMessage::operator()" << m);

		std::string args;
		for (size_t i=0; i<m.Params().size(); ++i)
			args += " " + m.Params()[i];
		ROOT->Log(level, format("[%1%] %2%:%3%") % m.Source() % m.ID() % args);

		MT_RET(true);
		MT_EE
	}
};

static bool biADMIN(const Message &m)
{
	MT_EB
	MT_FUNC("biADMIN" << m);

	ROOT->proto.NUMERIC(Protocol::nADMINME, m.Source(),
						(boost::format(_("Administrative info about %1%")) %
						 ROOT->ConfigValue<std::string>("server-name")).str(), true);

	ROOT->proto.NUMERIC(Protocol::nADMINLOC1, m.Source(),
						ROOT->ConfigValue<std::string>("server-desc"), true);

	std::vector<std::string> v = ROOT->ConfigValue<std::vector<std::string> >("operserv.services-admin");
	std::string admins;
	for (size_t i=0; i<v.size(); ++i)
		admins += " " + v[i];
	ROOT->proto.NUMERIC(Protocol::nADMINLOC2, m.Source(),
						(boost::format(_("Admins -%1%")) % admins).str(), true);

	ROOT->proto.NUMERIC(Protocol::nADMINEMAIL, m.Source(), PACKAGE_STRING, true);

	MT_RET(true);
	MT_EE
}

static bool biAWAY(const Message &m)
{
	MT_EB
	MT_FUNC("biAWAY" << m);

	if (ROOT->proto.IsServer(m.Source()) ||
		ROOT->proto.IsChannel(m.Source()))
	{
		std::vector<std::string> v;
		v.push_back(m.Source());
		v.push_back(_("No such nickname"));
		ROOT->proto.NUMERIC(Protocol::nNOSUCHNICK, m.Source(), v, true);
		MT_RET(false);
	}

	boost::shared_ptr<LiveUser> user = ROOT->data.Get_LiveUser(m.Source());
	if (!user)
	{
		std::vector<std::string> v;
		v.push_back(m.Source());
		v.push_back(_("No such nickname"));
		ROOT->proto.NUMERIC(Protocol::nNOSUCHNICK, m.Source(), v, true);
		MT_RET(false);
	}

	if (m.Params().empty())
		user->Away(std::string());
	else
		user->Away(m.Params()[0]);

	MT_RET(true);
	MT_EE
}

static bool biBURST(const Message &m)
{
	MT_EB
	MT_FUNC("biBURST" << m);

	if (m.Params().size() > 2)
	{
		// TODO: Sign on users
	}
	else if (m.Params().size() == 1)
	{
		ROOT->proto.BurstEnd();
	}

	MT_RET(true);
	MT_EE
}

static bool biCAPAB(const Message &m)
{
	MT_EB
	MT_FUNC("biCAPAB" << m);

	for (size_t i = 0; i < m.Params().size(); ++i)
		if (m.Params()[i] == "TOKEN" ||
			m.Params()[i] == "TOKEN1")
			ROOT->proto.UseTokens();

	MT_RET(true);
	MT_EE
}

static bool biCHGHOST(const Message &m)
{
	MT_EB
	MT_FUNC("biCHGHOST" << m);

	if (m.Params().size() < 2)
	{
		std::vector<std::string> v;
		v.push_back(m.ID());
		v.push_back(_("Not enough parameters"));
		ROOT->proto.NUMERIC(Protocol::nNEED_MORE_PARAMS, m.Source(), v, true);
		MT_RET(false);
	}

	if (ROOT->proto.IsServer(m.Params()[0]) ||
		ROOT->proto.IsChannel(m.Params()[0]))
	{
		std::vector<std::string> v;
		v.push_back(m.Params()[0]);
		v.push_back(_("No such nickname"));
		ROOT->proto.NUMERIC(Protocol::nNOSUCHNICK, m.Params()[0], v, true);
		MT_RET(false);
	}

	boost::shared_ptr<LiveUser> user = ROOT->data.Get_LiveUser(m.Params()[0]);
	if (!user)
	{
		std::vector<std::string> v;
		v.push_back(m.Params()[0]);
		v.push_back(_("No such nickname"));
		ROOT->proto.NUMERIC(Protocol::nNOSUCHNICK, m.Params()[0], v, true);
		MT_RET(false);
	}

	user->AltHost(m.Params()[1]);

	MT_RET(true);
	MT_EE
}

static bool biCLIENT(const Message &m)
{
	MT_EB
	MT_FUNC("biCLIENT" << m);

	boost::shared_ptr<Uplink> uplink = ROOT->getUplink();
	if (!uplink)
		MT_RET(false);

	boost::shared_ptr<LiveUser> user = ROOT->proto.ParseUser(m);
	if (!user)
	{
		LOG(Error, _("Failed to parse signon message: %1%"), m);
		MT_RET(false);
	}

	ROOT->data.Add(user);
	uplink->de.Satisfy(Dependency::NickExists, user->Name());

	MT_RET(true);
	MT_EE
}

static bool biCREATE(const Message &m)
{
	MT_EB
	MT_FUNC("biCREATE" << m);

	boost::shared_ptr<Uplink> uplink = ROOT->getUplink();
	if (!uplink)
		MT_RET(false);

	if (m.Params().size() < 1)
	{
		std::vector<std::string> v;
		v.push_back(m.ID());
		v.push_back(_("Not enough parameters"));
		ROOT->proto.NUMERIC(Protocol::nNEED_MORE_PARAMS, m.Source(), v, true);
		MT_RET(false);
	}

	// No use sending back a message they won't get.
	if (ROOT->proto.IsServer(m.Source()) ||
		ROOT->proto.IsChannel(m.Source()))
		MT_RET(false);

	boost::shared_ptr<LiveUser> user = ROOT->data.Get_LiveUser(m.Source());
	if (!user)
		MT_RET(false);

	boost::char_separator<char> sep(",");
	typedef boost::tokenizer<boost::char_separator<char>,
		std::string::const_iterator, std::string> tokenizer;
	tokenizer tokens(m.Params()[0], sep);
	tokenizer::iterator i;
	for (i = tokens.begin(); i != tokens.end(); ++i)
	{
		if (!ROOT->proto.IsChannel(*i))
		{
			std::vector<std::string> v;
			v.push_back(*i);
			v.push_back(_("No such channel"));
			ROOT->proto.NUMERIC(Protocol::nNOSUCHCHANNEL, m.Source(), v, true);
		}

		boost::shared_ptr<LiveChannel> channel = ROOT->data.Get_LiveChannel(m.Params()[0]);
		if (!channel)
		{
			channel = LiveChannel::create(*i);
			ROOT->data.Add(channel);
			uplink->de.Satisfy(Dependency::ChannelExists, channel->Name());
		}

		channel->Join(user);
		uplink->de.Satisfy(Dependency::NickInChannel, user->Name(),
						   channel->Name());
		channel->Modes(boost::shared_ptr<LiveUser>(), "+o", user->Name());
	}


	MT_RET(true);
	MT_EE
}

static bool biCYCLE(const Message &m)
{
	MT_EB
	MT_FUNC("biCYCLE" << m);

	boost::shared_ptr<Uplink> uplink = ROOT->getUplink();
	if (!uplink)
		MT_RET(false);

	if (m.Params().size() < 1)
	{
		std::vector<std::string> v;
		v.push_back(m.ID());
		v.push_back(_("Not enough parameters"));
		ROOT->proto.NUMERIC(Protocol::nNEED_MORE_PARAMS, m.Source(), v, true);
		MT_RET(false);
	}

	// No use sending back a message they won't get.
	if (ROOT->proto.IsServer(m.Source()) ||
		ROOT->proto.IsChannel(m.Source()))
		MT_RET(false);

	boost::shared_ptr<LiveUser> user = ROOT->data.Get_LiveUser(m.Source());
	if (!user)
		MT_RET(false);

	if (!ROOT->proto.IsChannel(m.Params()[0]))
	{
		std::vector<std::string> v;
		v.push_back(m.Params()[0]);
		v.push_back(_("No such channel"));
		ROOT->proto.NUMERIC(Protocol::nNOSUCHCHANNEL, m.Source(), v, true);
		MT_RET(false);
	}

	boost::shared_ptr<LiveChannel> channel = ROOT->data.Get_LiveChannel(m.Params()[0]);
	if (!channel)
	{
		std::vector<std::string> v;
		v.push_back(m.Params()[0]);
		v.push_back(_("No such channel"));
		ROOT->proto.NUMERIC(Protocol::nNOSUCHCHANNEL, m.Source(), v, true);
		MT_RET(false);
	}

	if (!channel->IsUser(user))
	{
		std::vector<std::string> v;
		v.push_back(m.Params()[0]);
		v.push_back(_("You're not on that channel"));
		ROOT->proto.NUMERIC(Protocol::nUSERNOTINCHANNEL, m.Source(), v, true);
		MT_RET(false);
	}

	channel->Part(user);
	uplink->de.Satisfy(Dependency::NickNotInChannel, user->Name(),
					   channel->Name());
	channel->Join(user);
	uplink->de.Satisfy(Dependency::NickInChannel, user->Name(),
					   channel->Name());

	MT_RET(true);
	MT_EE
}

static bool biEOB(const Message &m)
{
	MT_EB
	MT_FUNC("biEOB" << m);

	ROOT->proto.BurstEnd();

	MT_RET(true);
	MT_EE
}

static bool biINFO(const Message &m)
{ 
	MT_EB
	MT_FUNC("biINFO" << m);

	ROOT->proto.NUMERIC(Protocol::nINFOSTART, m.Source(), _("Server INFO"), true);
	const char **iter = contrib;
	while (*iter)
		ROOT->proto.NUMERIC(Protocol::nINFO, m.Source(), *iter++, true);
	ROOT->proto.NUMERIC(Protocol::nINFOEND, m.Source(), _("End of /INFO list"), true);

	MT_RET(true);
	MT_EE
}

static bool biISON(const Message &m)
{
	MT_EB
	MT_FUNC("biISON" << m);

	std::vector<std::string> v;
	for (size_t i=0; i<m.Params().size(); ++i)
	{
		boost::shared_ptr<LiveUser> user = ROOT->data.Get_LiveUser(m.Params()[i]);
		if (user)
			v.push_back(m.Params()[i]);
	}
	ROOT->proto.NUMERIC(Protocol::nISON, m.Source(), v);

	MT_RET(true);
	MT_EE
}

static bool biJOIN(const Message &m)
{
	MT_EB
	MT_FUNC("biJOIN" << m);

	boost::shared_ptr<Uplink> uplink = ROOT->getUplink();
	if (!uplink)
		MT_RET(false);

	if (m.Params().size() < 1)
	{
		std::vector<std::string> v;
		v.push_back(m.ID());
		v.push_back(_("Not enough parameters"));
		ROOT->proto.NUMERIC(Protocol::nNEED_MORE_PARAMS, m.Source(), v, true);
		MT_RET(false);
	}

	// No use sending back a message they won't get.
	if (ROOT->proto.IsServer(m.Source()) ||
		ROOT->proto.IsChannel(m.Source()))
		MT_RET(false);

	boost::shared_ptr<LiveUser> user = ROOT->data.Get_LiveUser(m.Source());
	if (!user)
		MT_RET(false);

	boost::char_separator<char> sep(",");
	typedef boost::tokenizer<boost::char_separator<char>,
		std::string::const_iterator, std::string> tokenizer;
	tokenizer tokens(m.Params()[0], sep);
	tokenizer::iterator i;
	for (i = tokens.begin(); i != tokens.end(); ++i)
	{
		if (!ROOT->proto.IsChannel(*i))
		{
			std::vector<std::string> v;
			v.push_back(*i);
			v.push_back(_("No such channel"));
			ROOT->proto.NUMERIC(Protocol::nNOSUCHCHANNEL, m.Source(), v, true);
		}

		boost::shared_ptr<LiveChannel> channel = ROOT->data.Get_LiveChannel(m.Params()[0]);
		if (!channel)
		{
			channel = LiveChannel::create(*i);
			ROOT->data.Add(channel);
			uplink->de.Satisfy(Dependency::ChannelExists, channel->Name());
		}

		channel->Join(user);
		uplink->de.Satisfy(Dependency::NickInChannel, user->Name(),
						   channel->Name());
	}

	MT_RET(true);
	MT_EE
}

static bool biKICK(const Message &m)
{
	MT_EB
	MT_FUNC("biKICK" << m);

	if (m.Params().size() < 3)
	{
		std::vector<std::string> v;
		v.push_back(m.ID());
		v.push_back(_("Not enough parameters"));
		ROOT->proto.NUMERIC(Protocol::nNEED_MORE_PARAMS, m.Source(), v, true);
		MT_RET(false);
	}

	// No use sending back a message they won't get.
	boost::shared_ptr<LiveUser> user;
	if (ROOT->proto.IsChannel(m.Source()))
		MT_RET(false);
	else if (!ROOT->proto.IsServer(m.Source()))
		user = ROOT->data.Get_LiveUser(m.Source());

	if (!ROOT->proto.IsChannel(m.Params()[0]))
	{
		std::vector<std::string> v;
		v.push_back(m.Params()[0]);
		v.push_back(_("No such channel"));
		ROOT->proto.NUMERIC(Protocol::nNOSUCHCHANNEL, m.Source(), v, true);
		MT_RET(false);
	}

	boost::shared_ptr<LiveChannel> channel = ROOT->data.Get_LiveChannel(m.Params()[0]);
	if (!channel)
	{
		std::vector<std::string> v;
		v.push_back(m.Params()[0]);
		v.push_back(_("No such channel"));
		ROOT->proto.NUMERIC(Protocol::nNOSUCHCHANNEL, m.Source(), v, true);
		MT_RET(false);
	}

	if (!channel->IsUser(user))
		LOG(Warning, _("Possible data corruption, %1% is not in channel %2%."),
			user % channel);

	// No use sending back a message they won't get.
	if (ROOT->proto.IsServer(m.Params()[1]) ||
		ROOT->proto.IsChannel(m.Params()[1]))
	{
		std::vector<std::string> v;
		v.push_back(m.Params()[1]);
		v.push_back(_("No such nickname"));
		ROOT->proto.NUMERIC(Protocol::nNOSUCHNICK, m.Params()[1], v, true);
		MT_RET(false);
	}

	boost::shared_ptr<LiveUser> target = ROOT->data.Get_LiveUser(m.Params()[1]);
	if (!target)
	{
		std::vector<std::string> v;
		v.push_back(m.Params()[1]);
		v.push_back(_("No such nickname"));
		ROOT->proto.NUMERIC(Protocol::nNOSUCHNICK, m.Params()[1], v, true);
		MT_RET(false);
	}

	channel->Kick(target, user, m.Params()[m.Params().size()-1]);

	MT_RET(true);
	MT_EE
}

static bool biKILL(const Message &m)
{
	MT_EB
	MT_FUNC("biKILL" << m);

	if (m.Params().size() < 2)
	{
		std::vector<std::string> v;
		v.push_back(m.ID());
		v.push_back(_("Not enough parameters"));
		ROOT->proto.NUMERIC(Protocol::nNEED_MORE_PARAMS, m.Source(), v, true);
		MT_RET(false);
	}

	// No use sending back a message they won't get.
	boost::shared_ptr<LiveUser> user;
	if (ROOT->proto.IsChannel(m.Source()))
		MT_RET(false);
	else if (!ROOT->proto.IsServer(m.Source()))
		user = ROOT->data.Get_LiveUser(m.Source());

	// No use sending back a message they won't get.
	if (ROOT->proto.IsServer(m.Params()[0]) ||
		ROOT->proto.IsChannel(m.Params()[0]))
	{
		std::vector<std::string> v;
		v.push_back(m.Params()[0]);
		v.push_back(_("No such nickname"));
		ROOT->proto.NUMERIC(Protocol::nNOSUCHNICK, m.Params()[0], v, true);
		MT_RET(false);
	}

	boost::shared_ptr<LiveUser> target = ROOT->data.Get_LiveUser(m.Params()[0]);
	if (!target)
	{
		std::vector<std::string> v;
		v.push_back(m.Params()[0]);
		v.push_back(_("No such nickname"));
		ROOT->proto.NUMERIC(Protocol::nNOSUCHNICK, m.Params()[0], v, true);
		MT_RET(false);
	}

	target->Kill(user, m.Params()[m.Params().size()-1]);
	MT_RET(true);
	MT_EE
}

static bool biLINKS(const Message &m)
{
	MT_EB
	MT_FUNC("biLINKS" << m);

	// TODO: Fix the links :)

	MT_RET(true);
	MT_EE
}

static bool biLIST(const Message &m)
{
	MT_EB
	MT_FUNC("biLIST" << m);

	// TODO: This is much more complex.
	// do we ever even get it?

	MT_RET(true);
	MT_EE
}

static bool biMODE(const Message &m)
{
	MT_EB
	MT_FUNC("biMODE" << m);

	if (m.Params().size() < 2)
	{
		std::vector<std::string> v;
		v.push_back(m.ID());
		v.push_back(_("Not enough parameters"));
		ROOT->proto.NUMERIC(Protocol::nNEED_MORE_PARAMS, m.Source(), v, true);
		MT_RET(false);
	}

	boost::shared_ptr<LiveUser> user;
	if (ROOT->proto.IsChannel(m.Source()))
		MT_RET(false);
	else if (!ROOT->proto.IsServer(m.Source()))
		user = ROOT->data.Get_LiveUser(m.Source());

	if (ROOT->proto.IsChannel(m.Params()[0]))
	{
		boost::shared_ptr<LiveChannel> channel = ROOT->data.Get_LiveChannel(m.Params()[0]);
		if (!channel)
		{
			std::vector<std::string> v;
			v.push_back(m.Params()[0]);
			v.push_back(_("No such channel"));
			ROOT->proto.NUMERIC(Protocol::nNOSUCHCHANNEL, m.Source(), v, true);
			MT_RET(false);
		}

		if (m.Params().size() > 2)
			channel->Modes(user, m.Params()[1], m.Params()[m.Params().size()-1]);
		else
			channel->Modes(user, m.Params()[1]);
	}
	else if (!ROOT->proto.IsServer(m.Params()[0]))
	{
		boost::shared_ptr<LiveUser> target = ROOT->data.Get_LiveUser(m.Params()[0]);
		if (!target)
		{
			std::vector<std::string> v;
			v.push_back(m.Params()[0]);
			v.push_back(_("No such nickname"));
			ROOT->proto.NUMERIC(Protocol::nNOSUCHNICK, m.Params()[0], v, true);
			MT_RET(false);
		}

		target->Modes(m.Params()[1]);
	}
	else
	{
		std::vector<std::string> v;
		v.push_back(m.Params()[0]);
		v.push_back(_("No such nickname"));
		ROOT->proto.NUMERIC(Protocol::nNOSUCHNICK, m.Params()[0], v, true);
		MT_RET(false);
	}

	MT_RET(true);
	MT_EE
}

static bool biMOTD(const Message &m)
{
	MT_EB
	MT_FUNC("biMOTD" << m);

	ROOT->proto.NUMERIC(Protocol::nMOTDSTART, m.Source(), _("Message of the Day"), true);
	std::vector<std::string> motd;
	if (mantra::mfile::UnDump(ROOT->ConfigValue<std::string>("motdfile"), motd) > 0)
		for (size_t i=0; i<motd.size(); ++i)
			ROOT->proto.NUMERIC(Protocol::nMOTD, m.Source(), motd[i], true);
	ROOT->proto.NUMERIC(Protocol::nMOTDEND, m.Source(), _("End of /MOTD list"), true);

	MT_RET(true);
	MT_EE
}

static bool biNAMES(const Message &m)
{
	MT_EB
	MT_FUNC("biNAMES" << m);


	MT_RET(true);
	MT_EE
}

static bool biNICK(const Message &m)
{
	MT_EB
	MT_FUNC("biNICK" << m);

	boost::shared_ptr<Uplink> uplink = ROOT->getUplink();
	if (!uplink)
		MT_RET(false);

	boost::shared_ptr<LiveUser> user = ROOT->proto.ParseUser(m);
	if (!user)
	{
		LOG(Error, _("Failed to parse signon message: %1%"), m);
		MT_RET(false);
	}

	ROOT->data.Add(user);
	uplink->de.Satisfy(Dependency::NickExists, user->Name());

	MT_RET(true);
	MT_EE
}

static bool biPART(const Message &m)
{
	MT_EB
	MT_FUNC("biPART" << m);

	boost::shared_ptr<Uplink> uplink = ROOT->getUplink();
	if (!uplink)
		MT_RET(false);

	if (m.Params().size() < 1)
	{
		std::vector<std::string> v;
		v.push_back(m.ID());
		v.push_back(_("Not enough parameters"));
		ROOT->proto.NUMERIC(Protocol::nNEED_MORE_PARAMS, m.Source(), v, true);
		MT_RET(false);
	}

	// No use sending back a message they won't get.
	if (ROOT->proto.IsServer(m.Source()) ||
		ROOT->proto.IsChannel(m.Source()))
		MT_RET(false);

	boost::shared_ptr<LiveUser> user = ROOT->data.Get_LiveUser(m.Source());
	if (!user)
		MT_RET(false);

	if (!ROOT->proto.IsChannel(m.Params()[0]))
	{
		std::vector<std::string> v;
		v.push_back(m.Params()[0]);
		v.push_back(_("No such channel"));
		ROOT->proto.NUMERIC(Protocol::nNOSUCHCHANNEL, m.Source(), v, true);
		MT_RET(false);
	}

	boost::shared_ptr<LiveChannel> channel = ROOT->data.Get_LiveChannel(m.Params()[0]);
	if (!channel)
	{
		std::vector<std::string> v;
		v.push_back(m.Params()[0]);
		v.push_back(_("No such channel"));
		ROOT->proto.NUMERIC(Protocol::nNOSUCHCHANNEL, m.Source(), v, true);
		MT_RET(false);
	}

	if (!channel->IsUser(user))
	{
		std::vector<std::string> v;
		v.push_back(m.Params()[0]);
		v.push_back(_("You're not on that channel"));
		ROOT->proto.NUMERIC(Protocol::nUSERNOTINCHANNEL, m.Source(), v, true);
		MT_RET(false);
	}

	channel->Part(user);
	uplink->de.Satisfy(Dependency::NickNotInChannel, user->Name(),
					   channel->Name());

	MT_RET(true);
	MT_EE
}

static bool biPASS(const Message &m)
{
	MT_EB
	MT_FUNC("biPASS" << m);

	boost::shared_ptr<Uplink> uplink = ROOT->getUplink();
	if (!uplink)
		MT_RET(false);

	if (m.Params().size() < 1)
	{
		std::vector<std::string> v;
		v.push_back(m.ID());
		v.push_back(_("Not enough parameters"));
		ROOT->proto.NUMERIC(Protocol::nNEED_MORE_PARAMS, m.Source(), v, true);
		MT_RET(false);
	}

	if (!uplink->CheckPassword(m.Params()[0]))
	{
		ROOT->proto.ERROR(_("No Access (password mismatch)"));
		ROOT->proto.NUMERIC(Protocol::nINCORRECT_PASSWORD, m.Source(),
							_("Password incorrect"), true);
		MT_RET(false);
	}

	MT_RET(true);
	MT_EE
}

static bool biPING(const Message &m)
{
	MT_EB
	MT_FUNC("biPING" << m);

	if (m.Params().size() < 1)
	{
		std::vector<std::string> v;
		v.push_back(m.ID());
		v.push_back(_("Not enough parameters"));
		ROOT->proto.NUMERIC(Protocol::nNEED_MORE_PARAMS, m.Source(), v, true);
		MT_RET(false);
	}

	boost::shared_ptr<Uplink> uplink = ROOT->getUplink();
	if (!uplink)
		MT_RET(false);

	boost::shared_ptr<Server> local, remote = uplink->Find(m.Params()[0]);
	Jupe *jupe = dynamic_cast<Jupe *>(remote.get());
	if (!remote || jupe)
	{
		std::vector<std::string> v;
		v.push_back(m.Params()[0]);
		v.push_back(_("No such server"));
		ROOT->proto.NUMERIC(Protocol::nNOSUCHSERVER, m.Source(), v, true);
		MT_RET(false);
	}

	if (m.Params().size() < 2)
	{
		local = uplink;
		jupe = dynamic_cast<Jupe *>(local.get());
	}
	else
	{
		local = uplink->Find(m.Params()[1]);
		jupe = dynamic_cast<Jupe *>(local.get());
	}
	if (!jupe)
	{
		std::vector<std::string> v;
		v.push_back(m.Params()[1]);
		v.push_back(_("No such server"));
		ROOT->proto.NUMERIC(Protocol::nNOSUCHSERVER, m.Source(), v, true);
		MT_RET(false);
	}

	std::vector<std::string> v;
	v.push_back(jupe->Name());
    v.push_back(m.Params()[0]);
	ROOT->proto.RAW(*jupe, "PONG", v);

	MT_RET(true);
	MT_EE
}

static bool biPONG(const Message &m)
{
	MT_EB
	MT_FUNC("biPONG" << m);

	if (m.Params().size() < 2)
	{
		std::vector<std::string> v;
		v.push_back(m.ID());
		v.push_back(_("Not enough parameters"));
		ROOT->proto.NUMERIC(Protocol::nNEED_MORE_PARAMS, m.Source(), v, true);
		MT_RET(false);
	}

	boost::shared_ptr<Uplink> uplink = ROOT->getUplink();
	if (!uplink)
		MT_RET(false);

	boost::shared_ptr<Server> remote = uplink->Find(m.Params()[0]);
	Jupe *ptr = dynamic_cast<Jupe *>(remote.get());
	if (!remote || ptr)
	{
		std::vector<std::string> v;
		v.push_back(m.Params()[0]);
		v.push_back(_("No such server"));
		ROOT->proto.NUMERIC(Protocol::nNOSUCHSERVER, m.Source(), v, true);
		MT_RET(false);
	}

	remote->Pong();
	if (uplink.get() == remote->Parent() &&
		ROOT->proto.ConfigValue<std::string>("burst-end").empty())
		ROOT->proto.BurstEnd();

	MT_RET(true);
	MT_EE
}

static bool biPRIVMSG(const Message &m)
{
	MT_EB
	MT_FUNC("biPRIVMSG" << m);

	if (m.Params().size() < 2)
	{
		std::vector<std::string> v;
		v.push_back(m.ID());
		v.push_back(_("Not enough parameters"));
		ROOT->proto.NUMERIC(Protocol::nNEED_MORE_PARAMS, m.Source(), v, true);
		MT_RET(false);
	}

	// No use sending back a message they won't get.
	boost::shared_ptr<LiveUser> user;
	if (ROOT->proto.IsChannel(m.Source()))
		MT_RET(false);
	else if (!ROOT->proto.IsServer(m.Source()))
		user = ROOT->data.Get_LiveUser(m.Source());

	// No use sending back a message they won't get.
	if (ROOT->proto.IsServer(m.Params()[0]) ||
		ROOT->proto.IsChannel(m.Params()[0]))
	{
		std::vector<std::string> v;
		v.push_back(m.Params()[0]);
		v.push_back(_("No such nickname"));
		ROOT->proto.NUMERIC(Protocol::nNOSUCHNICK, m.Params()[0], v, true);
		MT_RET(false);
	}

	boost::shared_ptr<LiveUser> target = ROOT->data.Get_LiveUser(m.Params()[0]);
	if (!target)
	{
		std::vector<std::string> v;
		v.push_back(m.Params()[0]);
		v.push_back(_("No such nickname"));
		ROOT->proto.NUMERIC(Protocol::nNOSUCHNICK, m.Params()[0], v, true);
		MT_RET(false);
	}

	Service *service = const_cast<Service *>(target->GetService());
	if (service)
		service->Execute(target, user, m.Params()[m.Params().size()-1]);

	MT_RET(true);
	MT_EE
}

static bool biQUIT(const Message &m)
{
	MT_EB
	MT_FUNC("biQUIT" << m);

	// No use sending back a message they won't get.
	if (ROOT->proto.IsChannel(m.Source()))
		MT_RET(false);
	else if (!ROOT->proto.IsServer(m.Source()))
	{
		boost::shared_ptr<LiveUser> user = ROOT->data.Get_LiveUser(m.Source());
		if (!user)
			MT_RET(false);

		if (m.Params().empty())
			user->Quit();
		else
			user->Quit(m.Params()[m.Params().size()-1]);
	}
	else
	{
		boost::shared_ptr<Server> s = ROOT->getUplink()->Find(m.Source());
		if (s && s->Parent())
			const_cast<Server *>(s->Parent())->Disconnect(s->Name());
	}

	MT_RET(true);
	MT_EE
}

static bool biSETHOST(const Message &m)
{
	MT_EB
	MT_FUNC("biSETHOST" << m);

	if (m.Params().size() < 1)
	{
		std::vector<std::string> v;
		v.push_back(m.ID());
		v.push_back(_("Not enough parameters"));
		ROOT->proto.NUMERIC(Protocol::nNEED_MORE_PARAMS, m.Source(), v, true);
		MT_RET(false);
	}

	// No use sending back a message they won't get.
	if (ROOT->proto.IsChannel(m.Source()) ||
		ROOT->proto.IsServer(m.Source()))
		MT_RET(false);

	boost::shared_ptr<LiveUser> user = ROOT->data.Get_LiveUser(m.Source());
	if (!user)
			MT_RET(false);
	
	user->AltHost(m.Params()[0]);

	MT_RET(true);
	MT_EE
}

static bool biSERVER(const Message &m)
{
	MT_EB
	MT_FUNC("biSERVER" << m);

	boost::shared_ptr<Uplink> uplink = ROOT->getUplink();
	if (!uplink)
		MT_RET(false);

	boost::shared_ptr<Server> serv = ROOT->proto.ParseServer(m);
	if (!serv)
		MT_RET(false);

	if (m.Source().empty())
	{
		uplink->Connect(serv);
		LOG(Info, _("Server %1% linked to network via. %2%."), serv->Name() %
															   uplink->Name());
	}
	else
	{
		boost::shared_ptr<Server> parent = uplink->Find(m.Source());
		if (!parent)
			MT_RET(false);
		parent->Connect(serv);
		LOG(Info, _("Server %1% linked to network via. %2%."), serv->Name() %
															   parent->Name());
	}

	uplink->de.Satisfy(Dependency::ServerExists, serv->Name());

	MT_RET(true);
	MT_EE
}

static bool biSERVICE(const Message &m)
{
	MT_EB
	MT_FUNC("biSERVICE" << m);

	// TODO: Sign on

	MT_RET(true);
	MT_EE
}

static bool biSJOIN(const Message &m)
{
	MT_EB
	MT_FUNC("biSJOIN" << m);

	// TODO: Join on steroids.

	MT_RET(true);
	MT_EE
}

static bool biSQUIT(const Message &m)
{
	MT_EB
	MT_FUNC("biSQUIT" << m);

	if (m.Params().size() < 1)
	{
		std::vector<std::string> v;
		v.push_back(m.ID());
		v.push_back(_("Not enough parameters"));
		ROOT->proto.NUMERIC(Protocol::nNEED_MORE_PARAMS, m.Source(), v, true);
		MT_RET(false);
	}

	// No use sending back a message they won't get.
	if (!ROOT->proto.IsServer(m.Params()[0]))
	{
		std::vector<std::string> v;
		v.push_back(m.Params()[0]);
		v.push_back(_("No such server"));
		ROOT->proto.NUMERIC(Protocol::nNOSUCHSERVER, m.Source(), v, true);
		MT_RET(false);
	}

	boost::shared_ptr<Server> s = ROOT->getUplink()->Find(m.Params()[0]);
	if (!s)
	{
		std::vector<std::string> v;
		v.push_back(m.Params()[0]);
		v.push_back(_("No such server"));
		ROOT->proto.NUMERIC(Protocol::nNOSUCHSERVER, m.Source(), v, true);
		MT_RET(false);
	}

	if (s->Parent())
		const_cast<Server *>(s->Parent())->Disconnect(s->Name());

	MT_RET(true);
	MT_EE
}

static bool biSTATS(const Message &m)
{
	MT_EB
	MT_FUNC("biSTATS" << m);

	ROOT->proto.NUMERIC(Protocol::nSUMMONDISABLED, m.Source(),
						_("End of /STATS report"), true);

	MT_RET(true);
	MT_EE
}

static bool biSUMMON(const Message &m)
{
	MT_EB
	MT_FUNC("biSUMMON" << m);

	ROOT->proto.NUMERIC(Protocol::nSUMMONDISABLED, m.Source(),
						_("SUMMON has been removed"), true);

	MT_RET(true);
	MT_EE
}

static bool biTIME(const Message &m)
{
	MT_EB
	MT_FUNC("biTIME" << m);

	ROOT->proto.NUMERIC(Protocol::nTIME, m.Source(),
						mantra::DateTimeToString<char>(mantra::GetCurrentDateTime()), true);

	MT_RET(true);
	MT_EE
}

static bool biTMODE(const Message &m)
{
	MT_EB
	MT_FUNC("biTMODE" << m);

	if (m.Params().size() < 3)
	{
		std::vector<std::string> v;
		v.push_back(m.ID());
		v.push_back(_("Not enough parameters"));
		ROOT->proto.NUMERIC(Protocol::nNEED_MORE_PARAMS, m.Source(), v, true);
		MT_RET(false);
	}

	boost::shared_ptr<LiveUser> user;
	if (ROOT->proto.IsChannel(m.Source()))
		MT_RET(false);
	else if (!ROOT->proto.IsServer(m.Source()))
		user = ROOT->data.Get_LiveUser(m.Source());

	if (ROOT->proto.IsChannel(m.Params()[0]))
	{
		boost::shared_ptr<LiveChannel> channel = ROOT->data.Get_LiveChannel(m.Params()[0]);
		if (!channel)
		{
			std::vector<std::string> v;
			v.push_back(m.Params()[0]);
			v.push_back(_("No such channel"));
			ROOT->proto.NUMERIC(Protocol::nNOSUCHCHANNEL, m.Source(), v, true);
			MT_RET(false);
		}

		if (m.Params().size() > 3)
			channel->Modes(user, m.Params()[2], m.Params()[m.Params().size()-1]);
		else
			channel->Modes(user, m.Params()[2]);
	}
	else if (!ROOT->proto.IsServer(m.Params()[0]))
	{
		boost::shared_ptr<LiveUser> target = ROOT->data.Get_LiveUser(m.Params()[0]);
		if (!target)
		{
			std::vector<std::string> v;
			v.push_back(m.Params()[0]);
			v.push_back(_("No such nickname"));
			ROOT->proto.NUMERIC(Protocol::nNOSUCHNICK, m.Params()[0], v, true);
			MT_RET(false);
		}

		target->Modes(m.Params()[2]);
	}
	else
	{
		std::vector<std::string> v;
		v.push_back(m.Params()[0]);
		v.push_back(_("No such nickname"));
		ROOT->proto.NUMERIC(Protocol::nNOSUCHNICK, m.Params()[0], v, true);
		MT_RET(false);
	}
	MT_RET(true);
	MT_EE
}

static bool biTOPIC(const Message &m)
{
	MT_EB
	MT_FUNC("biTOPIC" << m);

	if (m.Params().size() < ROOT->proto.ConfigValue<bool>("extended-topic") ? 2 : 1)
	{
		std::vector<std::string> v;
		v.push_back(m.ID());
		v.push_back(_("Not enough parameters"));
		ROOT->proto.NUMERIC(Protocol::nNEED_MORE_PARAMS, m.Source(), v, true);
		MT_RET(false);
	}

	// No use sending back a message they won't get.
	if (ROOT->proto.IsServer(m.Source()) ||
		ROOT->proto.IsChannel(m.Source()))
		MT_RET(false);

	boost::shared_ptr<LiveUser> user = ROOT->data.Get_LiveUser(m.Source());
	if (!user)
		MT_RET(false);

	if (!ROOT->proto.IsChannel(m.Params()[0]))
	{
		std::vector<std::string> v;
		v.push_back(m.Params()[0]);
		v.push_back(_("No such channel"));
		ROOT->proto.NUMERIC(Protocol::nNOSUCHCHANNEL, m.Source(), v, true);
		MT_RET(false);
	}

	boost::shared_ptr<LiveChannel> channel = ROOT->data.Get_LiveChannel(m.Params()[0]);
	if (!channel)
	{
		std::vector<std::string> v;
		v.push_back(m.Params()[0]);
		v.push_back(_("No such channel"));
		ROOT->proto.NUMERIC(Protocol::nNOSUCHCHANNEL, m.Source(), v, true);
		MT_RET(false);
	}

	std::string topic, setter = m.Source();
	boost::posix_time::ptime set_time = mantra::GetCurrentDateTime();
	if (ROOT->proto.ConfigValue<bool>("extended-topic"))
	{
		setter = m.Params()[1];
		if (m.Params().size() > 3)
		{
			set_time = boost::posix_time::from_time_t(boost::lexical_cast<time_t>(m.Params()[2]));
			topic = m.Params()[m.Params().size() - 1];
		}
	}
	else if (m.Params().size() > 1)
		topic = m.Params()[m.Params().size() - 1];

	channel->Topic(topic, setter, set_time);

	MT_RET(true);
	MT_EE
}

static bool biTRACE(const Message &m)
{
	MT_EB
	MT_FUNC("biTRACE" << m);

	std::vector<std::string> v;
	v.push_back(ROOT->ConfigValue<std::string>("server-name"));
	v.push_back(_("End of TRACE"));
	ROOT->proto.NUMERIC(Protocol::nTRACEEND, m.Source(), v, true);

	MT_RET(true);
	MT_EE
}

static bool biUMODE2(const Message &m)
{
	MT_EB
	MT_FUNC("biUMODE2" << m);

	if (m.Params().size() < 1)
	{
		std::vector<std::string> v;
		v.push_back(m.ID());
		v.push_back(_("Not enough parameters"));
		ROOT->proto.NUMERIC(Protocol::nNEED_MORE_PARAMS, m.Source(), v, true);
		MT_RET(false);
	}

	if (ROOT->proto.IsChannel(m.Source()) ||
		ROOT->proto.IsServer(m.Source()))
		MT_RET(false);

	boost::shared_ptr<LiveUser> user = ROOT->data.Get_LiveUser(m.Source());
	if (!user)
		MT_RET(false);

	user->Modes(m.Params()[1]);

	MT_RET(true);
	MT_EE
}

static bool biUSER(const Message &m)
{
	MT_EB
	MT_FUNC("biUSER" << m);

	boost::shared_ptr<Uplink> uplink = ROOT->getUplink();
	if (!uplink)
		MT_RET(false);

	boost::shared_ptr<LiveUser> user = ROOT->proto.ParseUser(m);
	if (!user)
	{
		LOG(Error, _("Failed to parse signon message: %1%"), m);
		MT_RET(false);
	}

	ROOT->data.Add(user);
	uplink->de.Satisfy(Dependency::NickExists, user->Name());

	MT_RET(true);
	MT_EE
}

static bool biUSERHOST(const Message &m)
{
	MT_EB
	MT_FUNC("biUSERHOST" << m);

	if (m.Params().size() < 1)
	{
		std::vector<std::string> v;
		v.push_back(m.ID());
		v.push_back(_("Not enough parameters"));
		ROOT->proto.NUMERIC(Protocol::nNEED_MORE_PARAMS, m.Source(), v, true);
		MT_RET(false);
	}

	for (size_t i=0; i<m.Params().size(); ++i)
	{
		if (ROOT->proto.IsServer(m.Params()[i]) ||
			ROOT->proto.IsChannel(m.Params()[i]))
		{
			std::vector<std::string> v;
			v.push_back(m.Params()[i]);
			v.push_back(_("No such nickname"));
			ROOT->proto.NUMERIC(Protocol::nNOSUCHNICK, m.Params()[i], v, true);
			continue;
		}

		boost::shared_ptr<LiveUser> user = ROOT->data.Get_LiveUser(m.Params()[i]);
		if (!user)
		{
			std::vector<std::string> v;
			v.push_back(m.Params()[i]);
			v.push_back(_("No such nickname"));
			ROOT->proto.NUMERIC(Protocol::nNOSUCHNICK, m.Source(), v, true);
			continue;
		}

		ROOT->proto.NUMERIC(Protocol::nUSERHOST, m.Source(), 
							user->Name() + (user->Mode('o') ? "*=" : "=") +
							(user->Away().empty() ? '+' : '-') + user->User() +
							"@" + user->PrefHost(), true);
	}

	MT_RET(true);
	MT_EE
}

static bool biUSERS(const Message &m)
{
	MT_EB
	MT_FUNC("biUSERS" << m);

	ROOT->proto.NUMERIC(Protocol::nUSERSDISABLED, m.Source(),
						_("USERS has been removed"), true);

	MT_RET(true);
	MT_EE
}

static bool biVERSION(const Message &m)
{
	MT_EB
	MT_FUNC("biVERSION" << m);

	struct utsname un;
	uname(&un);

	std::vector<std::string> v;
	v.push_back(PACKAGE + std::string("-") + PACKAGE_VERSION);
	v.push_back(ROOT->ConfigValue<std::string>("server-name"));
	v.push_back(un.sysname + std::string("/") + un.release +
				std::string(" ") + un.release);
	ROOT->proto.NUMERIC(Protocol::nVERSION, m.Source(), v, true);

	MT_RET(true);
	MT_EE
}

static bool biWHO(const Message &m)
{
	MT_EB
	MT_FUNC("biWHO" << m);

	// TODO: Long and arduous task ...

	MT_RET(true);
	MT_EE
}

static bool biWHOIS(const Message &m)
{
	MT_EB
	MT_FUNC("biWHOIS" << m);

	// TODO: Long and arduous task ...

	MT_RET(true);
	MT_EE
}

static bool bi436(const Message &m)
{
	MT_EB
	MT_FUNC("bi436" << m);

	// TODO: Nick collision, handle somehow?

	MT_RET(true);
	MT_EE
}

static bool bi464(const Message &m)
{
	MT_EB
	MT_FUNC("bi464" << m);

	LOG(Error, _("Server %1% has informed us our link password is wrong, disconnecting."),
		m.Source());
	ROOT->Disconnect();

	MT_RET(true);
	MT_EE
}

static bool bi465(const Message &m)
{
	MT_EB
	MT_FUNC("bi465" << m);

	LOG(Error, _("Server %1% has informed us we are banned, disconnecting."),
		m.Source());
	ROOT->Disconnect();

	MT_RET(true);
	MT_EE
}

void Message::ResetCommands()
{
	MT_EB
	MT_FUNC("Message::ResetCommands");

	func_map.clear();

	// We ignore these ...
	func_map["ADCHAT"] = functor();
	func_map["AKILL"] = functor();
	func_map["CHATOPS"] = functor();
	func_map["CHGIDENT"] = functor();
	func_map["CHGNAME"] = functor();
	func_map["DUMMY"] = functor();
	func_map["EOB_ACK"] = functor();
	func_map["EXCEPTION"] = functor();
	func_map["GLINE"] = functor();
	func_map["GLOBOPS"] = functor();
	func_map["GOPER"] = functor();
	func_map["GZLINE"] = functor();
	func_map["HELP"] = functor();
	func_map["HELPOPS"] = functor();
	func_map["HTM"] = functor();
	func_map["INVITE"] = functor();
	func_map["JUPITER"] = functor();
	func_map["KNOCK"] = functor();
	func_map["LAG"] = functor();
	func_map["LUSERSLOCK"] = functor();
	func_map["MKPASSWD"] = functor();
	func_map["MODNICK"] = functor();
	func_map["MODULE"] = functor();
	func_map["MYID"] = functor();
	func_map["NACHAT"] = functor();
	func_map["NETINFO"] = functor();
	func_map["NOTICE"] = functor();
	func_map["OPER"] = functor();
	func_map["RAKILL"] = functor();
	func_map["REHASH"] = functor();
	func_map["RESTART"] = functor();
	func_map["REXCEPTION"] = functor();
	func_map["RQLINE"] = functor();
	func_map["SDESC"] = functor();
	func_map["SENDUMODE"] = functor();
	func_map["SETIDENT"] = functor();
	func_map["SETNAME"] = functor();
	func_map["SETTIME"] = functor();
	func_map["SGLINE"] = functor();
	func_map["SHUN"] = functor();
	func_map["SILENCE"] = functor();
	func_map["SMO"] = functor();
	func_map["SQLINE"] = functor();
	func_map["SUMMON"] = functor();
	func_map["SVINFO"] = functor();
	func_map["SVSKILL"] = functor();
	func_map["SVSMOTD"] = functor();
	func_map["SVSNICK"] = functor();
	func_map["SVSNLINE"] = functor();
	func_map["SVSNOOP"] = functor();
	func_map["SVSPART"] = functor();
	func_map["SWHOIS"] = functor();
	func_map["SZLINE"] = functor();
	func_map["TSCTL"] = functor();
	func_map["TKL"] = functor();
	func_map["TKLINE"] = functor();
	func_map["UNGLINE"] = functor();
	func_map["UNGZLINE"] = functor();
	func_map["UNJUPITER"] = functor();
	func_map["UNRQLINE"] = functor();
	func_map["UNSQLINE"] = functor();
	func_map["UNZLINE"] = functor();
	func_map["WALLOPS"] = functor();
	func_map["WHOWAS"] = functor();
	func_map["ZLINE"] = functor();

	// We log these ...
	func_map["ERROR"] = LogMessage(mantra::LogLevel::Warning);
	func_map["GNOTICE"] = LogMessage(mantra::LogLevel::Info);

	// We process these ...
	func_map["ADMIN"] = &biADMIN;
	func_map["AWAY"] = &biAWAY;
	func_map["BURST"] = &biBURST;
	func_map["CAPAB"] = &biCAPAB;
	func_map["CHGHOST"] = &biCHGHOST;
	func_map["CLIENT"] = &biCLIENT;
	func_map["CREATE"] = &biCREATE;
	func_map["CYCLE"] = &biCYCLE;
	func_map["EOB"] = &biEOB;
	func_map["END_OF_BURST"] = &biEOB;
	func_map["INFO"] = &biINFO;
	func_map["ISON"] = &biISON;
	func_map["JOIN"] = &biJOIN;
	func_map["KICK"] = &biKICK;
	func_map["KILL"] = &biKILL;
	func_map["LINKS"] = &biLINKS;
	func_map["LIST"] = &biLIST;
	func_map["MODE"] = &biMODE;
	func_map["MOTD"] = &biMOTD;
	func_map["NAMES"] = &biNAMES;
	func_map["NICK"] = &biNICK;
	func_map["PART"] = &biPART;
	func_map["PASS"] = &biPASS;
	func_map["PING"] = &biPING;
	func_map["PONG"] = &biPONG;
	func_map["PRIVMSG"] = &biPRIVMSG;
	func_map["PROTOCTL"] = &biCAPAB;
	func_map["QUIT"] = &biQUIT;
	func_map["SETHOST"] = &biSETHOST;
	func_map["SERVER"] = &biSERVER;
	func_map["SERVICE"] = &biSERVICE;
	func_map["SJOIN"] = &biSJOIN;
	func_map["SQUIT"] = &biSQUIT;
	func_map["STATS"] = &biSTATS;
	func_map["SUMMON"] = &biSUMMON;
	func_map["SVSHOST"] = &biCHGHOST;
	func_map["SVSMODE"] = &biMODE;
	func_map["SVS2MODE"] = &biMODE;
	func_map["TIME"] = &biTIME;
	func_map["TMODE"] = &biTMODE;
	func_map["TOPIC"] = &biTOPIC;
	func_map["TRACE"] = &biTRACE;
	func_map["UMODE2"] = &biUMODE2;
	func_map["USER"] = &biUSER;
	func_map["USERHOST"] = &biUSERHOST;
	func_map["USERS"] = &biUSERS;
	func_map["VERSION"] = &biVERSION;
	func_map["WHO"] = &biWHO;
	func_map["WHOIS"] = &biWHOIS;

	func_map["436"] = &bi436;
	func_map["464"] = &bi464;
	func_map["465"] = &bi465;

	MT_EE
}


