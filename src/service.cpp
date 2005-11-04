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
#include "serviceuser.h"
#include "livechannel.h"
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
				if (j == users_.end() || **j != *i)
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

	user = ServiceUser::create(this, nick, real_, uplink);

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
			ServiceUser *su = dynamic_cast<ServiceUser *>(u.get());
			if (su->GetService() == this)
			{
				users.insert(u);
				continue;
			}

			if (u->GetServer() == uplink)
				su->QUIT(_("Signing on as different service."));
			else
				uplink->KILL(u, _("Nickname reserved for services."));
			u = SIGNON(*i);
		}
		users.insert(u);
	}

	{
		SYNC_WLOCK(users_);
		users_.swap(users);
	}

	{
		SYNC_LOCK(pending_privmsg);
		pending_t::const_iterator j;
		for (j = pending_privmsg.begin(); j != pending_privmsg.end(); ++j)
			PRIVMSG(j->first, j->second);
		pending_privmsg.clear();
	}

	{
		SYNC_LOCK(pending_notice);
		pending_t::const_iterator j;
		for (j = pending_notice.begin(); j != pending_notice.end(); ++j)
			NOTICE(j->first, j->second);
		pending_notice.clear();
	}

	MT_EE
}

unsigned int Service::PushCommand(const boost::regex &rx,
								  const Service::functor &func,
								  unsigned int min_param,
								  const std::vector<std::string> &perms)
{
	MT_EB
	MT_FUNC("Service::PushCommand" << rx << func << min_param << perms);

	SYNC_WLOCK(func_map_);
	static unsigned int id = 0;
	func_map_.push_front(Command_t());
	func_map_.front().id = ++id;
	func_map_.front().min_param = min_param;
	func_map_.front().perms = perms;
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

	ServiceUser *su = dynamic_cast<ServiceUser *>(source.get());
	if (!su || su->GetService() != this)
		return;

	std::string out;
	ROOT->proto.addline(*source, out, ROOT->proto.tokenise("QUIT") +
						" :" + message);
	bool rv = ROOT->proto.send(out);
	if (rv)
	{
		SIGNOFF(source);
		source->Quit(message);
	}

	MT_EE
}

void Service::KILL(const ServiceUser *source,
				   const boost::shared_ptr<LiveUser> &target,
				   const std::string &message) const
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
		target->Kill(source->GetUser(), message);

	MT_EE
}

void Service::KILL(const boost::shared_ptr<LiveUser> &target,
				   const std::string &message) const
{
	MT_EB
	MT_FUNC("Service::KILL" << target << message);

	SYNC_RLOCK(users_);
	users_t::const_iterator j = std::lower_bound(users_.begin(),
												 users_.end(),
												 primary_);
	if (j == users_.end() || **j != primary_)
		return;

	KILL(dynamic_cast<ServiceUser *>(j->get()), target, message);

	MT_EE
}

void Service::PRIVMSG(const ServiceUser *source,
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
					  const boost::format &message)
{
	MT_EB
	MT_FUNC("Service::PRIVMSG" << target << message);

	SYNC_RLOCK(users_);
	users_t::const_iterator j = std::lower_bound(users_.begin(),
												 users_.end(),
												 primary_);
	if (j == users_.end() || **j != primary_)
	{
		SYNC_LOCK(pending_privmsg);
		pending_privmsg.push_back(std::make_pair(target, message));
	}
	else
		PRIVMSG(dynamic_cast<ServiceUser *>(j->get()), target, message);

	MT_EE
}

void Service::NOTICE(const ServiceUser *source,
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
					 const boost::format &message)
{
	MT_EB
	MT_FUNC("Service::NOTICE" << target << message);

	SYNC_RLOCK(users_);
	users_t::const_iterator j = std::lower_bound(users_.begin(),
												 users_.end(),
												 primary_);

	if (j == users_.end() || **j != primary_)
	{
		SYNC_LOCK(pending_notice);
		pending_notice.push_back(std::make_pair(target, message));
	}
	else
		NOTICE(dynamic_cast<ServiceUser *>(j->get()), target, message);

	MT_EE
}

void Service::HELPOP(const ServiceUser *source,
					 const boost::format &message) const
{
	MT_EB
	MT_FUNC("Service::HELPOP" << source << message);

	if (!ROOT->proto.ConfigValue<bool>("helpops"))
	{
		WALLOP(source, message);
		return;
	}

	if (!source || source->GetService() != this)
		return;

	std::string out;
	ROOT->proto.addline(*source, out, ROOT->proto.tokenise("HELPOPS") + " :" +
						message.str());
	ROOT->proto.send(out);

	MT_EE
}

void Service::HELPOP(const boost::format &message) const
{
	MT_EB
	MT_FUNC("Service::HELPOP" << message);

	SYNC_RLOCK(users_);
	users_t::const_iterator j = std::lower_bound(users_.begin(),
												 users_.end(),
												 primary_);
	if (j == users_.end() || **j != primary_)
		return;

	HELPOP(dynamic_cast<ServiceUser *>(j->get()), message);

	MT_EE
}

void Service::WALLOP(const ServiceUser *source,
					 const boost::format &message) const
{
	MT_EB
	MT_FUNC("Service::WALLOP" << source << message);

	if (!source || source->GetService() != this)
		return;

	std::string out;
	ROOT->proto.addline(*source, out, ROOT->proto.tokenise("WALLOPS") + " :" +
						message.str());
	ROOT->proto.send(out);

	MT_EE
}

void Service::WALLOP(const boost::format &message) const
{
	MT_EB
	MT_FUNC("Service::WALLOP" << message);

	SYNC_RLOCK(users_);
	users_t::const_iterator j = std::lower_bound(users_.begin(),
												 users_.end(),
												 primary_);
	if (j == users_.end() || **j != primary_)
		return;

	WALLOP(dynamic_cast<ServiceUser *>(j->get()), message);

	MT_EE
}

void Service::GLOBOP(const ServiceUser *source,
					 const boost::format &message) const
{
	MT_EB
	MT_FUNC("Service::GLOBOP" << source << message);

	if (!source || source->GetService() != this)
		return;

	std::string out;
	ROOT->proto.addline(*source, out, ROOT->proto.tokenise("GLOBOPS") + " :" +
						message.str());
	ROOT->proto.send(out);

	MT_EE
}

void Service::GLOBOP(const boost::format &message) const
{
	MT_EB
	MT_FUNC("Service::GLOBOP" << message);

	SYNC_RLOCK(users_);
	users_t::const_iterator j = std::lower_bound(users_.begin(),
												 users_.end(),
												 primary_);
	if (j == users_.end() || **j != primary_)
		return;

	GLOBOP(dynamic_cast<ServiceUser *>(j->get()), message);

	MT_EE
}

void Service::ANNOUNCE(const ServiceUser *source,
					   const boost::format &message) const
{
	MT_EB
	MT_FUNC("Service::ANNOUNCE" << source << message);

	if (ROOT->proto.ConfigValue<bool>("globops"))
		GLOBOP(source, message);
	else
		WALLOP(source, message);

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
	if (j == users_.end() || **j != primary_)
		return;

	ANNOUNCE(dynamic_cast<ServiceUser *>(j->get()), message);

	MT_EE
}

void Service::SVSNICK(const ServiceUser *source,
					  const boost::shared_ptr<LiveUser> &target,
					  const std::string &newnick) const
{
	MT_EB
	MT_FUNC("Service::SVSNICK" << source << target << newnick);

	if (!source || source->GetService() != this)
		return;

	std::string out;
	ROOT->proto.addline(*source, out,
						(boost::format(ROOT->proto.ConfigValue<std::string>("svsnick")) %
									   target->Name() % newnick % mantra::GetCurrentDateTime()).str());
	ROOT->proto.send(out);

	MT_EE
}

void Service::SVSNICK(const boost::shared_ptr<LiveUser> &target,
					  const std::string &newnick) const
{
	MT_EB
	MT_FUNC("Service::SVSNICK" << target << newnick);

	SYNC_RLOCK(users_);
	users_t::const_iterator j = std::lower_bound(users_.begin(),
												 users_.end(),
												 primary_);
	if (j == users_.end() || **j != primary_)
		return;

	SVSNICK(dynamic_cast<ServiceUser *>(j->get()), target, newnick);

	MT_EE
}

void Service::JOIN(const ServiceUser *source,
				   const boost::shared_ptr<LiveChannel> &channel) const
{
	MT_EB
	MT_FUNC("Service::JOIN" << source << channel);

	if (!source || source->GetService() != this)
		return;

	std::string out;
	switch (ROOT->proto.ConfigValue<unsigned int>("join"))
	{
	case 0:
		ROOT->proto.addline(*source, out, ROOT->proto.tokenise("TOPIC") + " :" +
							channel->Name());
		break;
	case 1:
		ROOT->proto.addline(*source, out, ROOT->proto.tokenise("SJOIN") + ' ' +
							boost::lexical_cast<std::string>(time(NULL)) + ' ' +
							channel->Name());
		break;
	case 2:
		ROOT->proto.addline(out, ROOT->proto.tokenise("SJOIN") + ' ' +
							boost::lexical_cast<std::string>(time(NULL)) + ' ' +
							channel->Name() + " :@" + source->Name());
		break;
	}

	ROOT->proto.send(out);
	channel->Join(source->GetUser());

	MT_EE
}

void Service::JOIN(const boost::shared_ptr<LiveChannel> &channel) const
{
	MT_EB
	MT_FUNC("Service::JOIN" << channel);

	SYNC_RLOCK(users_);
	users_t::const_iterator j = std::lower_bound(users_.begin(),
												 users_.end(),
												 primary_);
	if (j == users_.end() || **j != primary_)
		return;

	JOIN(dynamic_cast<ServiceUser *>(j->get()), channel);

	MT_EE
}

void Service::PART(const ServiceUser *source,
				   const boost::shared_ptr<LiveChannel> &channel,
				   const std::string &reason) const
{
	MT_EB
	MT_FUNC("Service::PART" << source << channel << reason);

	if (!source || source->GetService() != this)
		return;

	std::string out;
	ROOT->proto.addline(*source, out, ROOT->proto.tokenise("PART") + " " +
						channel->Name() + (reason.empty() ? std::string() :
															" :" + reason));
	ROOT->proto.send(out);
	channel->Part(source->GetUser());

	MT_EE
}

void Service::PART(const boost::shared_ptr<LiveChannel> &channel,
				   const std::string &reason) const
{
	MT_EB
	MT_FUNC("Service::PART" << channel << reason);

	SYNC_RLOCK(users_);
	users_t::const_iterator j = std::lower_bound(users_.begin(),
												 users_.end(),
												 primary_);
	if (j == users_.end() || **j != primary_)
		return;

	PART(dynamic_cast<ServiceUser *>(j->get()), channel, reason);

	MT_EE
}

void Service::TOPIC(const ServiceUser *source,
					const boost::shared_ptr<LiveChannel> &channel,
				    const std::string &topic) const
{
	MT_EB
	MT_FUNC("Service::TOPIC" << source << channel << topic);

	if (!source || source->GetService() != this)
		return;

	std::string out, msg = ROOT->proto.tokenise("TOPIC") + ' ' + channel->Name();
	if (ROOT->proto.ConfigValue<bool>("extended-topic"))
	{
		msg += ' ' + source->Name();
		if (!topic.empty())
			msg += ' ' + boost::lexical_cast<std::string>(time(NULL));
	}
	if (!topic.empty())
		msg += " :" + topic;
	ROOT->proto.addline(*source, out, msg);

	bool didjoin = false;
	const ServiceUser *sender = source;
	if (ROOT->proto.ConfigValue<bool>("topic-join"))
	{
		if (*source != primary_)
		{
			SYNC_RLOCK(users_);
			users_t::const_iterator j = std::lower_bound(users_.begin(),
														 users_.end(),
														 primary_);
			if (j != users_.end() && **j == primary_)
				sender = dynamic_cast<ServiceUser *>(j->get());
			if (!sender)
				return;
		}

		if (!sender->InChannel(channel))
		{
			JOIN(sender, channel);
			didjoin = true;
		}
	}

	ROOT->proto.send(out);
	channel->Topic(topic, sender->Name(), mantra::GetCurrentDateTime());
	if (didjoin)
		PART(sender, channel);

	MT_EE
}

void Service::TOPIC(const boost::shared_ptr<LiveChannel> &channel,
				    const std::string &topic) const
{
	MT_EB
	MT_FUNC("Service::TOPIC" << channel << topic);

	SYNC_RLOCK(users_);
	users_t::const_iterator j = std::lower_bound(users_.begin(),
												 users_.end(),
												 primary_);
	if (j == users_.end() || **j != primary_)
		return;

	TOPIC(dynamic_cast<ServiceUser *>(j->get()), channel, topic);

	MT_EE
}

void Service::KICK(const ServiceUser *source,
				   const boost::shared_ptr<LiveChannel> &channel,
				   const boost::shared_ptr<LiveUser> &target,
				   const boost::format &reason) const
{
	MT_EB
	MT_FUNC("Service::KICK" << source << channel << target << reason);

	if (!source || source->GetService() != this)
		return;

	std::string out;
	ROOT->proto.addline(*source, out, ROOT->proto.tokenise("KICK") + ' ' +
						channel->Name() + ' ' + target->Name() + " :" + reason.str());
	ROOT->proto.send(out);
	channel->Kick(target, source->GetUser(), reason.str());

	MT_EE
}

void Service::KICK(const boost::shared_ptr<LiveChannel> &channel,
				   const boost::shared_ptr<LiveUser> &target,
				   const boost::format &reason) const
{
	MT_EB
	MT_FUNC("Service::KICK" << channel << target << reason);

	SYNC_RLOCK(users_);
	users_t::const_iterator j = std::lower_bound(users_.begin(),
												 users_.end(),
												 primary_);
	if (j == users_.end() || **j != primary_)
		return;

	KICK(dynamic_cast<ServiceUser *>(j->get()), channel, target, reason);

	MT_EE
}

void Service::INVITE(const ServiceUser *source,
					 const boost::shared_ptr<LiveChannel> &channel,
					 const boost::shared_ptr<LiveUser> &target) const
{
	MT_EB
	MT_FUNC("Service::INVITE" << source << channel << target);

	if (!source || source->GetService() != this)
		return;

	std::string out;
	ROOT->proto.addline(*source, out, ROOT->proto.tokenise("INVITE") + ' ' +
						target->Name() + " :" + channel->Name());
	ROOT->proto.send(out);

	MT_EE
}

void Service::INVITE(const boost::shared_ptr<LiveChannel> &channel,
					 const boost::shared_ptr<LiveUser> &target) const
{
	MT_EB
	MT_FUNC("Service::INVITE" << channel << target);

	SYNC_RLOCK(users_);
	users_t::const_iterator j = std::lower_bound(users_.begin(),
												 users_.end(),
												 primary_);
	if (j == users_.end() || **j != primary_)
		return;

	INVITE(dynamic_cast<ServiceUser *>(j->get()), channel, target);

	MT_EE
}

void Service::MODE(const ServiceUser *source,
				   const std::string &in) const
{
	MT_EB
	MT_FUNC("Service::MODE" << source << in);

	if (!source || source->GetService() != this)
		return;

	std::string out;
	ROOT->proto.addline(*source, out, ROOT->proto.tokenise("MODE") + ' ' + in);
	ROOT->proto.send(out);
	source->GetUser()->Modes(in);

	MT_EE
}

void Service::MODE(const ServiceUser *source,
				   const boost::shared_ptr<LiveChannel> &channel,
				   const std::string &in,
				   const std::vector<std::string> &params) const
{
	MT_EB
	MT_FUNC("Service::MODE" << source << channel << in << params);

	if (!source || source->GetService() != this)
		return;

	std::string out, omodes, oparams;

	bool add = true;
	size_t arg = 0, count = 0;
	size_t maxmodes = ROOT->proto.ConfigValue<unsigned int>("max-channel-mode-params");
	std::string modeparams = ROOT->proto.ConfigValue<std::string>("channel-mode-params");
	for (std::string::const_iterator i = in.begin(); i < in.end(); ++i)
	{
		if (count >= maxmodes)
		{
			ROOT->proto.addline(*source, out, ROOT->proto.tokenise("MODE") + ' ' +
								omodes + " :" + oparams);
			omodes.clear();
			oparams.clear();
			count = 0;
		}

		omodes.append(1, *i);
		switch (*i)
		{
		case '-':
			add = false;
			break;
		case '+':
			add = true;
			break;
		case 'l':
			if (!add)
				break;
		default:
			if (modeparams.find(*i) != std::string::npos)
			{
				if (!oparams.empty())
					oparams.append(1, ' ');
				oparams.append(params[arg++]);
				++count;
			}
		}
	}

	if (!omodes.empty())
	{
		if (oparams.empty())
			ROOT->proto.addline(*source, out, ROOT->proto.tokenise("MODE") + ' ' +
								channel->Name() + ' ' + omodes);
		else
			ROOT->proto.addline(*source, out, ROOT->proto.tokenise("MODE") + ' ' +
								channel->Name() + ' ' + omodes + " :" + oparams);
	}

	ROOT->proto.send(out);
	channel->Modes(source->GetUser(), in, params);

	MT_EE
}

bool Service::CommandMerge::operator()(const ServiceUser *serv,
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

	bool rv = serv->Execute(user, p, primary);

	MT_RET(rv);
	MT_EE
}

bool Service::Help(const ServiceUser *service,
				   const boost::shared_ptr<LiveUser> &user,
				   const std::vector<std::string> &params) const
{
	MT_EB
	MT_FUNC("Service::Help" << service << user << params);

	if (!service)
		MT_RET(false);

	NSEND(service, user, N_("HELP system has not yet been written."));

	MT_RET(true);
	MT_EE
}

bool Service::AuxHelp(const ServiceUser *service,
					  const boost::shared_ptr<LiveUser> &user,
					  const std::vector<std::string> &params) const
{
	MT_EB
	MT_FUNC("Service::AuxHelp" << service << user << params);

	if (!service)
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

bool Service::Execute(const ServiceUser *service,
					  const boost::shared_ptr<LiveUser> &user,
					  const std::vector<std::string> &params,
					  unsigned int key) const
{
	MT_EB
	MT_FUNC("ServiceExecute" << service << user << params << key);

	if (!user)
	{
		LOG(Error, _("Received %1% call, but I've no idea who it came from!"),
			boost::algorithm::to_upper_copy(params[key]));
		MT_RET(false);
	}

	if (!service)
	{
		LOG(Error, _("Received %1% call from %2%, but it was not sent to a service!"),
			boost::algorithm::to_upper_copy(params[key]) % user->Name());
		MT_RET(false);
	}

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
			if (i != func_map_.end())
			{
				// Add one to the min params to accomodate for the command
				// name itself (which is params[0]).
				if (params.size() < i->min_param + 1)
				{
					SEND(service, user,
						 N_("Insufficient parameters for %1% command."),
						 boost::algorithm::to_upper_copy(params[key]));
					MT_ASSIGN(codetype);
					MT_RET(false);
				}
			}
		}
		// This does NOT mean it wasn't found, just that it wasn't defined.
		// When something is not defined, it is used to disable a command.
		if (!f)
		{
			SEND(service, user, N_("No such command %1%."),
				 boost::algorithm::to_upper_copy(params[key]));

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

