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

#include <mantra/core/trace.h>

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

	ROOT->proto.NUMERIC(Protocol::ADMINME, m.Source(),
						(boost::format("Administrative info about %1%") %
						 ROOT->ConfigValue<std::string>("server-name")).str());

	ROOT->proto.NUMERIC(Protocol::ADMINLOC1, m.Source(),
						ROOT->ConfigValue<std::string>("server-desc"));

	std::vector<std::string> v = ROOT->ConfigValue<std::vector<std::string> >("operserv.services-admin");
	std::string admins;
	for (size_t i=0; i<v.size(); ++i)
		admins += " " + v[i];
	ROOT->proto.NUMERIC(Protocol::ADMINLOC2, m.Source(),
						(boost::format("Admins -%1%") % admins).str());

	ROOT->proto.NUMERIC(Protocol::ADMINEMAIL, m.Source(), PACKAGE_STRING);

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
		ROOT->proto.NUMERIC(Protocol::NOSUCHNICK, m.Source(), v);
		MT_RET(false);
	}

	boost::shared_ptr<LiveUser> user = ROOT->data.Get_LiveUser(m.Source());
	if (!user)
	{
		std::vector<std::string> v;
		v.push_back(m.Source());
		v.push_back(_("No such nickname"));
		ROOT->proto.NUMERIC(Protocol::NOSUCHNICK, m.Source(), v);
		MT_RET(false);
	}

	if (m.Params().empty())
		user->Away();
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
		// TODO: End of Burst
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
		ROOT->proto.NUMERIC(Protocol::NEED_MORE_PARAMS, m.Source(), v);
		MT_RET(false);
	}

	if (ROOT->proto.IsServer(m.Params()[0]) ||
		ROOT->proto.IsChannel(m.Params()[0]))
	{
		std::vector<std::string> v;
		v.push_back(m.Params()[0]);
		v.push_back(_("No such nickname"));
		ROOT->proto.NUMERIC(Protocol::NOSUCHNICK, m.Params()[0], v);
		MT_RET(false);
	}

	boost::shared_ptr<LiveUser> user = ROOT->data.Get_LiveUser(m.Params()[0]);
	if (!user)
	{
		std::vector<std::string> v;
		v.push_back(m.Params()[0]);
		v.push_back(_("No such nickname"));
		ROOT->proto.NUMERIC(Protocol::NOSUCHNICK, m.Params()[0], v);
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

	// TODO: Sign on user.

	MT_RET(true);
	MT_EE
}

static bool biCREATE(const Message &m)
{
	MT_EB
	MT_FUNC("biCREATE" << m);

	// TODO: Create a channel.

	MT_RET(true);
	MT_EE
}

static bool biCYCLE(const Message &m)
{
	MT_EB
	MT_FUNC("biCYCLE" << m);

	// TODO: Part/Join

	MT_RET(true);
	MT_EE
}

static bool biEOB(const Message &m)
{
	MT_EB
	MT_FUNC("biEOB" << m);

	// TODO: End of Burst

	MT_RET(true);
	MT_EE
}

static bool biINFO(const Message &m)
{
	MT_EB
	MT_FUNC("biINFO" << m);

	MT_RET(true);
	MT_EE
}

static bool biISON(const Message &m)
{
	MT_EB
	MT_FUNC("biISON" << m);

	MT_RET(true);
	MT_EE
}

static bool biJOIN(const Message &m)
{
	MT_EB
	MT_FUNC("biJOIN" << m);

	MT_RET(true);
	MT_EE
}

static bool biKICK(const Message &m)
{
	MT_EB
	MT_FUNC("biKICK" << m);

	MT_RET(true);
	MT_EE
}

static bool biKILL(const Message &m)
{
	MT_EB
	MT_FUNC("biKILL" << m);

	MT_RET(true);
	MT_EE
}

static bool biLINKS(const Message &m)
{
	MT_EB
	MT_FUNC("biLINKS" << m);

	MT_RET(true);
	MT_EE
}

static bool biLIST(const Message &m)
{
	MT_EB
	MT_FUNC("biLIST" << m);

	MT_RET(true);
	MT_EE
}

static bool biMODE(const Message &m)
{
	MT_EB
	MT_FUNC("biMODE" << m);

	MT_RET(true);
	MT_EE
}

static bool biMOTD(const Message &m)
{
	MT_EB
	MT_FUNC("biMOTD" << m);

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

	MT_RET(true);
	MT_EE
}

static bool biPASS(const Message &m)
{
	MT_EB
	MT_FUNC("biPASS" << m);

	if (m.Params().size() < 1)
	{
		std::vector<std::string> v;
		v.push_back(m.ID());
		v.push_back(_("Not enough parameters"));
		ROOT->proto.NUMERIC(Protocol::NEED_MORE_PARAMS, m.Source(), v);
		MT_RET(false);
	}

	if (!ROOT->getUplink()->CheckPassword(m.Params()[0]))
	{
		ROOT->proto.ERROR(_("No Access (password mismatch)"));
		ROOT->proto.NUMERIC(Protocol::INCORRECT_PASSWORD, m.Source(),
							_("Password incorrect"));
		MT_RET(false);
	}

	MT_RET(true);
	MT_EE
}

static bool biPING(const Message &m)
{
	MT_EB
	MT_FUNC("biPING" << m);

	if (m.Params().size() < 2)
	{
		std::vector<std::string> v;
		v.push_back(m.ID());
		v.push_back(_("Not enough parameters"));
		ROOT->proto.NUMERIC(Protocol::NEED_MORE_PARAMS, m.Source(), v);
		MT_RET(false);
	}

	boost::shared_ptr<Server> remote = ROOT->getUplink()->Find(m.Params()[0]);
	Jupe *ptr = dynamic_cast<Jupe *>(remote.get());
	if (!remote || ptr)
	{
		std::vector<std::string> v;
		v.push_back(m.Params()[0]);
		v.push_back(_("No such server"));
		ROOT->proto.NUMERIC(Protocol::NOSUCHSERVER, m.Source(), v);
		MT_RET(false);
	}

	boost::shared_ptr<Server> local = ROOT->getUplink()->Find(m.Params()[1]);
	Jupe *jupe = dynamic_cast<Jupe *>(local.get());
	if (!jupe)
	{
		std::vector<std::string> v;
		v.push_back(m.Params()[1]);
		v.push_back(_("No such server"));
		ROOT->proto.NUMERIC(Protocol::NOSUCHSERVER, m.Source(), v);
		MT_RET(false);
	}

	std::vector<std::string> v;
	v.push_back(m.Params()[1]);
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
		ROOT->proto.NUMERIC(Protocol::NEED_MORE_PARAMS, m.Source(), v);
		MT_RET(false);
	}

	boost::shared_ptr<Server> remote = ROOT->getUplink()->Find(m.Params()[0]);
	Jupe *ptr = dynamic_cast<Jupe *>(remote.get());
	if (!remote || ptr)
	{
		std::vector<std::string> v;
		v.push_back(m.Params()[0]);
		v.push_back(_("No such server"));
		ROOT->proto.NUMERIC(Protocol::NOSUCHSERVER, m.Source(), v);
		MT_RET(false);
	}

	remote->Pong();

	MT_RET(true);
	MT_EE
}

static bool biPRIVMSG(const Message &m)
{
	MT_EB
	MT_FUNC("biPRIVMSG" << m);

	MT_RET(true);
	MT_EE
}

static bool biQUIT(const Message &m)
{
	MT_EB
	MT_FUNC("biQUIT" << m);

	MT_RET(true);
	MT_EE
}

static bool biRPING(const Message &m)
{
	MT_EB
	MT_FUNC("biRPING" << m);

	MT_RET(true);
	MT_EE
}

static bool biSETHOST(const Message &m)
{
	MT_EB
	MT_FUNC("biSETHOST" << m);

	MT_RET(true);
	MT_EE
}

static bool biSERVER(const Message &m)
{
	MT_EB
	MT_FUNC("biSERVER" << m);

	MT_RET(true);
	MT_EE
}

static bool biSERVICE(const Message &m)
{
	MT_EB
	MT_FUNC("biSERVICE" << m);

	MT_RET(true);
	MT_EE
}

static bool biSJOIN(const Message &m)
{
	MT_EB
	MT_FUNC("biSJOIN" << m);

	MT_RET(true);
	MT_EE
}

static bool biSQUIT(const Message &m)
{
	MT_EB
	MT_FUNC("biSQUIT" << m);

	MT_RET(true);
	MT_EE
}

static bool biSTATS(const Message &m)
{
	MT_EB
	MT_FUNC("biSTATS" << m);

	MT_RET(true);
	MT_EE
}

static bool biSUMMON(const Message &m)
{
	MT_EB
	MT_FUNC("biSUMMON" << m);

	MT_RET(true);
	MT_EE
}

static bool biSVSHOST(const Message &m)
{
	MT_EB
	MT_FUNC("biSVSHOST" << m);

	MT_RET(true);
	MT_EE
}

static bool biSVSMODE(const Message &m)
{
	MT_EB
	MT_FUNC("biSVSMODE" << m);

	MT_RET(true);
	MT_EE
}

static bool biTIME(const Message &m)
{
	MT_EB
	MT_FUNC("biTIME" << m);

	MT_RET(true);
	MT_EE
}

static bool biTMODE(const Message &m)
{
	MT_EB
	MT_FUNC("biTMODE" << m);

	MT_RET(true);
	MT_EE
}

static bool biTOPIC(const Message &m)
{
	MT_EB
	MT_FUNC("biTOPIC" << m);

	MT_RET(true);
	MT_EE
}

static bool biTRACE(const Message &m)
{
	MT_EB
	MT_FUNC("biTRACE" << m);

	MT_RET(true);
	MT_EE
}

static bool biUMODE2(const Message &m)
{
	MT_EB
	MT_FUNC("biUMODE2" << m);

	MT_RET(true);
	MT_EE
}

static bool biUSER(const Message &m)
{
	MT_EB
	MT_FUNC("biUSER" << m);

	MT_RET(true);
	MT_EE
}

static bool biUSERS(const Message &m)
{
	MT_EB
	MT_FUNC("biUSERS" << m);

	MT_RET(true);
	MT_EE
}

static bool biVERSION(const Message &m)
{
	MT_EB
	MT_FUNC("biVERSION" << m);

	MT_RET(true);
	MT_EE
}

static bool biWHO(const Message &m)
{
	MT_EB
	MT_FUNC("biWHO" << m);

	MT_RET(true);
	MT_EE
}

static bool biWHOIS(const Message &m)
{
	MT_EB
	MT_FUNC("biWHOIS" << m);

	MT_RET(true);
	MT_EE
}

static bool bi303(const Message &m)
{
	MT_EB
	MT_FUNC("bi303" << m);

	MT_RET(true);
	MT_EE
}

static bool bi436(const Message &m)
{
	MT_EB
	MT_FUNC("bi436" << m);

	MT_RET(true);
	MT_EE
}

static bool bi464(const Message &m)
{
	MT_EB
	MT_FUNC("bi464" << m);

	MT_RET(true);
	MT_EE
}

static bool bi465(const Message &m)
{
	MT_EB
	MT_FUNC("bi465" << m);

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
	func_map["RPONG"] = functor();
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
	func_map["PASS"] = &biPASS;
	func_map["PING"] = &biPING;
	func_map["PONG"] = &biPONG;
	func_map["PRIVMSG"] = &biPRIVMSG;
	func_map["PROTOCTL"] = &biCAPAB;
	func_map["QUIT"] = &biQUIT;
	func_map["RPING"] = &biRPING;
	func_map["SETHOST"] = &biSETHOST;
	func_map["SERVER"] = &biSERVER;
	func_map["SERVICE"] = &biSERVICE;
	func_map["SJOIN"] = &biSJOIN;
	func_map["SQUIT"] = &biSQUIT;
	func_map["STATS"] = &biSTATS;
	func_map["SUMMON"] = &biSUMMON;
	func_map["SVSHOST"] = &biSVSHOST;
	func_map["SVSMODE"] = &biSVSMODE;
	func_map["SVS2MODE"] = &biSVSMODE;
	func_map["TIME"] = &biTIME;
	func_map["TMODE"] = &biTMODE;
	func_map["TOPIC"] = &biTOPIC;
	func_map["TRACE"] = &biTRACE;
	func_map["UMODE2"] = &biUMODE2;
	func_map["USER"] = &biUSER;
	func_map["USERS"] = &biUSERS;
	func_map["VERSION"] = &biVERSION;
	func_map["WHO"] = &biWHO;
	func_map["WHOIS"] = &biWHOIS;

	func_map["303"] = &bi303;
	func_map["436"] = &bi436;
	func_map["464"] = &bi464;
	func_map["465"] = &bi465;

	MT_EE
}


