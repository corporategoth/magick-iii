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
#define RCSID(x,y) const char *rcsid_magick__service_cpp_ ## x () { return y; }
RCSID(magick__service_cpp, "@(#)$Id$");

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

Service::Service(TraceTypes_t trace)
	: trace_(trace), SYNC_NRWINIT(users_, reader_priority),
	  SYNC_NRWINIT(func_map_, reader_priority)
{
}

Service::~Service()
{
}

static bool operator<(const Service::users_t::value_type &lhs,
			   const std::string &rhs)
{
	return (*lhs < rhs);
}

void Service::Set(const std::vector<std::string> &nicks, const std::string &real)
{
	MT_EB
	MT_FUNC("Service::Set" << nicks);

	real_ = real;
	nicks_t signoff;
	if (!nicks.empty())
	{
		std::set_difference(nicks_.begin(), nicks_.end(),
							nicks.begin(), nicks.end(),
							std::inserter(signoff, signoff.end()));

		primary_ = nicks[0];
		nicks_.clear();
		nicks_.insert(nicks.begin(), nicks.end());
	}
	else
	{
		signoff = nicks_;

		primary_.clear();
		nicks_.clear();
	}

	if (ROOT->getUplink() && !signoff.empty())
	{
		std::string quitmsg;
		if (ROOT->ConfigExists("services.quit-message"))
			quitmsg = ROOT->ConfigValue<std::string>("services.quit-message");

		nicks_t::const_iterator i;
		for (i = signoff.begin(); i != signoff.end(); ++i)
		{
			users_t::const_iterator j;
			{
				SYNC_RLOCK(users_);
				j = std::lower_bound(users_.begin(), users_.end(), *i);
				if (j == users_.end())
					continue;
			}
			QUIT(*j, quitmsg);
		}
	}

	MT_EE
}

boost::shared_ptr<LiveUser> Service::SIGNON(const std::string &nick)
{
	MT_EB
	MT_FUNC("Service::SIGNON" << nick);

	boost::shared_ptr<LiveUser> user;
	boost::shared_ptr<Uplink> uplink = ROOT->getUplink();
	if (!uplink)
		MT_RET(user);

	std::string user_str = nick;
	if (ROOT->ConfigExists("services.user"))
		user_str = ROOT->ConfigValue<std::string>("services.user");

	std::string out;
	ROOT->proto.addline(out, (format(ROOT->proto.ConfigValue<std::string>("nick")) %
							  nick % user_str %
							  ROOT->ConfigValue<std::string>("services.host") %
							  uplink->Name() % time(NULL) % 1 % "" % "" % real_ % 1 %
							  ROOT->ConfigValue<std::string>("services.host") %
							  0x7F000001).str());

	if (!ROOT->proto.send(out))
		MT_RET(user);

	user = LiveUser::create(this, nick, real_, uplink);

	MT_RET(user);
	MT_EE
}

void Service::SIGNOFF(const boost::shared_ptr<LiveUser> &user)
{
	MT_EB
	MT_FUNC("Service::SIGNOFF" << user);

	SYNC_WLOCK(users_);
	users_.erase(user);

	MT_EE
}

void Service::Check()
{
	MT_EB
	MT_FUNC("Service::Check");

	boost::shared_ptr<Uplink> uplink = ROOT->getUplink();
	if (!uplink)
		return;

	users_t users;
	nicks_t::const_iterator i;
	for (i = nicks_.begin(); i != nicks_.end(); ++i)
	{
		boost::shared_ptr<LiveUser> u = ROOT->data.Get_LiveUser(*i);
		if (!u)
		{
			u = SIGNON(*i);
		}
		else
		{
			const Service *s = u->GetService();
			if (s == this)
			{
				users.insert(u);
				continue;
			}

			if (u->GetServer() == uplink)
				QUIT(u, _("Signing on as different service."));
			else
				uplink->KILL(u, _("Nickname reserved for services."));
			u = SIGNON(*i);
		}
		ROOT->data.Add(u);
		users.insert(u);
	}
	SYNC_WLOCK(users_);
	users_.swap(users);

	MT_EE
}

unsigned int Service::PushCommand(const boost::regex &rx,
								  const Service::functor &func,
								  const std::vector<std::string> &perms)
{
	MT_EB
	MT_FUNC("Service::PushCommand" << rx << func);

	SYNC_WLOCK(func_map_);
	static unsigned int id = 0;
	func_map_.push_front(Command_t());
	func_map_.front().id = ++id;
	func_map_.front().perms = perms;
	if (rx.flags() & boost::regex_constants::icase)
		func_map_.front().rx = rx;
	else
		func_map_.front().rx = boost::regex(rx.str(),
			rx.flags() | boost::regex_constants::icase);
	func_map_.front().func = func;

	MT_RET(id);
	MT_EE
}

void Service::DelCommand(unsigned int id)
{
	MT_EB
	MT_FUNC("Service::DelCommand" << id);

	SYNC_WLOCK(func_map_);
	func_map_t::iterator i;
	for (i = func_map_.begin(); i != func_map_.end(); ++i)
		if (i->id == id)
		{
			func_map_.erase(i);
			break;
		}

	MT_EE
}

void Service::QUIT(const boost::shared_ptr<LiveUser> &source,
				   const std::string &message)
{
	MT_EB
	MT_FUNC("Service::QUIT" << source << message);

	if (!source || source->GetService() != this)
		return;

	std::string out;
	ROOT->proto.addline(*source, out, ROOT->proto.tokenise("QUIT") +
						" :" + message);
	bool rv = ROOT->proto.send(out);
	if (rv)
		source->Quit(message);

	MT_EE
}

void Service::KILL(const boost::shared_ptr<LiveUser> &source,
				   const boost::shared_ptr<LiveUser> &target,
				   const std::string &message)
{
	MT_EB
	MT_FUNC("Service::KILL" << source << target << message);

	if (!source || source->GetService() != this)
		return;

	std::string out;
	ROOT->proto.addline(*source, out, ROOT->proto.tokenise("KILL") +
						" " + target->Name() + " :" + message);
	bool rv = ROOT->proto.send(out);
	if (rv)
		target->Kill(source, message);

	MT_EE
}

void Service::KILL(const boost::shared_ptr<LiveUser> &target,
				   const std::string &message)
{
	MT_EB
	MT_FUNC("Service::KILL" << target << message);

	SYNC_RLOCK(users_);
	users_t::const_iterator j = std::lower_bound(users_.begin(),
												 users_.end(),
												 primary_);
	if (j == users_.end())
		return;

	KILL(*j, target, message);

	MT_EE
}

void Service::PRIVMSG(const boost::shared_ptr<LiveUser> &source,
					  const boost::shared_ptr<LiveUser> &target,
					  const boost::format &message) const
{
	MT_EB
	MT_FUNC("Service::PRIVMSG" << source << target << message);

	if (!source || source->GetService() != this)
		return;

	std::string out;
	ROOT->proto.addline(*source, out, ROOT->proto.tokenise("PRIVMSG") +
						" " + target->Name() + " :" + message.str());
	ROOT->proto.send(out);

	MT_EE
}


void Service::PRIVMSG(const boost::shared_ptr<LiveUser> &target,
					  const boost::format &message) const
{
	MT_EB
	MT_FUNC("Service::PRIVMSG" << target << message);

	SYNC_RLOCK(users_);
	users_t::const_iterator j = std::lower_bound(users_.begin(),
												 users_.end(),
												 primary_);
	if (j == users_.end())
		return;

	PRIVMSG(*j, target, message);

	MT_EE
}

void Service::NOTICE(const boost::shared_ptr<LiveUser> &source,
					 const boost::shared_ptr<LiveUser> &target,
					 const boost::format &message) const
{
	MT_EB
	MT_FUNC("Service::NOTICE" << source << target << message);

	if (!source || source->GetService() != this)
		return;

	std::string out;
	ROOT->proto.addline(*source, out, ROOT->proto.tokenise("NOTICE") +
						" " + target->Name() + " :" + message.str());
	ROOT->proto.send(out);

	MT_EE
}

void Service::NOTICE(const boost::shared_ptr<LiveUser> &target,
					 const boost::format &message) const
{
	MT_EB
	MT_FUNC("Service::NOTICE" << target << message);

	SYNC_RLOCK(users_);
	users_t::const_iterator j = std::lower_bound(users_.begin(),
												 users_.end(),
												 primary_);
	if (j == users_.end())
		return;

	NOTICE(*j, target, message);

	MT_EE
}

void Service::ANNOUNCE(const boost::shared_ptr<LiveUser> &source,
					   const boost::format &message) const
{
	MT_EB
	MT_FUNC("Service::ANNOUNCE" << source << message);

	if (!source || source->GetService() != this)
		return;

	std::string out;
	ROOT->proto.addline(*source, out, ROOT->proto.tokenise(
						ROOT->proto.ConfigValue<bool>("globops")
							? "GLOBOPS" : "WALLOPS") + " :" +
						message.str());
	ROOT->proto.send(out);

	MT_EE
}

void Service::ANNOUNCE(const boost::format &message) const
{
	MT_EB
	MT_FUNC("Service::ANNOUNCE" << message);

	SYNC_RLOCK(users_);
	users_t::const_iterator j = std::lower_bound(users_.begin(),
												 users_.end(),
												 primary_);
	if (j == users_.end())
		return;

	ANNOUNCE(*j, message);

	MT_EE
}

bool Service::CommandMerge::operator()(const boost::shared_ptr<LiveUser> &serv,
									   const boost::shared_ptr<LiveUser> &user,
									   const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("Service::CommandMerge::operator()" << serv << user << params);

	if (params.size() <= primary)
		MT_RET(false);

	if (params.size() <= secondary)
	{
		SEND(serv, user, N_("Insufficient parameters for %1% command."),
			 boost::algorithm::to_upper_copy(params[primary]));
		MT_RET(false);
	}

	std::vector<std::string> p = params;
	p[primary].append(" " + p[secondary]);
	p.erase(p.begin() + secondary);

	bool rv = service.Execute(serv, user, p, primary);

	MT_RET(rv);
	MT_EE
}

bool Service::Help(const boost::shared_ptr<LiveUser> &service,
				   const boost::shared_ptr<LiveUser> &user,
				   const std::vector<std::string> &params) const
{
	MT_EB
	MT_FUNC("Service::Help" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);

	NSEND(service, user, N_("HELP system has not yet been written."));

	MT_RET(true);
	MT_EE
}

bool Service::AuxHelp(const boost::shared_ptr<LiveUser> &service,
					  const boost::shared_ptr<LiveUser> &user,
					  const std::vector<std::string> &params) const
{
	MT_EB
	MT_FUNC("Service::AuxHelp" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);

	boost::char_separator<char> sep(" \\t");
	typedef boost::tokenizer<boost::char_separator<char>,
		std::string::const_iterator, std::string> tokenizer;
	tokenizer tokens(params[0], sep);
	std::vector<std::string> v(tokens.begin(), tokens.end());

	std::vector<std::string> p;
	p.push_back(v[v.size() - 1]);
	p.insert(p.end(), v.begin(), v.begin() + (v.size() - 1));
	if (params.size() > 1)
		p.insert(p.end(), params.begin() + 1, params.end());

	bool rv = Help(service, user, p);

	MT_RET(rv);
	MT_EE
}

bool Service::Execute(const boost::shared_ptr<LiveUser> &service,
					  const boost::shared_ptr<LiveUser> &user,
					  const std::vector<std::string> &params,
					  unsigned int key) const
{
	MT_EB
	MT_FUNC("ServiceExecute" << service << user << params << key);
	unsigned int codetype = MT_ASSIGN(trace_);

	try
	{
		functor f;
		{
			SYNC_RLOCK(func_map_);
			func_map_t::const_iterator i;
			for (i = func_map_.begin(); i != func_map_.end(); ++i)
			{
				if (boost::regex_match(params[key], i->rx))
				{
					if (!i->perms.empty())
					{
						std::vector<std::string>::const_iterator j;
						for (j = i->perms.begin(); j != i->perms.end(); ++j)
						{
							static mantra::iequal_to<std::string> cmp;
							if (cmp(*j, ROOT->ConfigValue<std::string>("commserv.all.name")))
								break;
							else if (cmp(*j, ROOT->ConfigValue<std::string>("commserv.regd.name")))
							{
								if (user->Recognized())
									break;
							}
							else if (user->InCommittee(*j))
								break;
						}
						// Not in any required committee, skip.
						if (j == i->perms.end())
							continue;
					}
					f = i->func;
					break;
				}
			}
		}
		if (!f)
		{
			SEND(service, user, N_("No such command %1%."),
				 boost::algorithm::to_upper_copy(params[0]));

			MT_ASSIGN(codetype);
			MT_RET(true);
		}

		bool rv = f(service, user, params);
		MT_ASSIGN(codetype);
		MT_RET(rv);
	}
	catch (...)
	{
		MT_ASSIGN(codetype);
		throw;
	}

	MT_EE
}

