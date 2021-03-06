#ifndef WIN32
#pragma interface
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
** it may be viewed here: http://www.neuromancy.net/license/gpl.html
**
** ======================================================================= */
#ifndef _MAGICK_SERVICE_H
#define _MAGICK_SERVICE_H 1

#ifdef RCSID
RCSID(magick__service_h, "@(#) $Id$");
#endif // RCSID
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

#include "config.h"
#include "otherdata.h"

#include <set>
#include <string>
#include <vector>

#include <mantra/core/algorithms.h>
#include <mantra/core/sync.h>

#include <boost/format.hpp>
#include <boost/bind.hpp>
#include <boost/tokenizer.hpp>

class LiveUser;
class ServiceUser;
class LiveChannel;

enum TraceTypes_t
	{
		MAGICK_TRACE_LOST,
		MAGICK_TRACE_MAIN,
		MAGICK_TRACE_WORKER,
		MAGICK_TRACE_NICKSERV,	// All of these are PRIVMSG stuff.
		MAGICK_TRACE_CHANSERV,
		MAGICK_TRACE_MEMOSERV,
		MAGICK_TRACE_COMMSERV,
		MAGICK_TRACE_OPERSERV,
		MAGICK_TRACE_OTHER,		// Other services, still PRIVMSG
		MAGICK_TRACE_DCC,
		MAGICK_TRACE_EVENT,
		MAGICK_TRACE_SIZE
	};
extern boost::regex TraceTypeRegex[MAGICK_TRACE_SIZE];
extern std::string TraceTypes[MAGICK_TRACE_SIZE];

class Service
{
	friend class ServiceUser;
	friend class if_Service_Magick;

public:
	typedef boost::function3<bool, const ServiceUser *,
							 const boost::shared_ptr<LiveUser> &,
							 const std::vector<std::string> &> functor;
	typedef std::set<std::string, mantra::iless<std::string> > nicks_t;
	typedef std::set<boost::shared_ptr<LiveUser> > users_t;
	typedef std::vector<std::pair<boost::shared_ptr<LiveUser>, boost::format> > pending_t;

private:
	struct Command_t
	{
		unsigned int id;
		unsigned int min_param;
		std::vector<std::string> perms;
		boost::regex rx;
		functor func;
	};

	typedef std::deque<Command_t> func_map_t;

	TraceTypes_t trace_;
	bool processing_;
	std::string real_;
	std::string primary_;
	nicks_t nicks_;
	users_t RWSYNC(users_);

	func_map_t RWSYNC(func_map_);

	pending_t SYNC(pending_privmsg);
	pending_t SYNC(pending_notice);

	void Set(const std::vector<std::string> &nicks, const std::string &real = std::string());
	void SIGNOFF(const boost::shared_ptr<LiveUser> &user);
	boost::shared_ptr<LiveUser> SIGNON(const std::string &nick);

	// This should NOT be passed a ServiceUser.
	void QUIT(const boost::shared_ptr<LiveUser> &source,
			  const std::string &message = std::string());

	void PRIVMSG(const ServiceUser *source,
				 const boost::shared_ptr<LiveUser> &target,
				 const boost::format &message) const;
	void PRIVMSG(const ServiceUser *source,
				 const boost::shared_ptr<Server> &target,
				 const boost::format &message) const;
	void NOTICE(const ServiceUser *source,
				const boost::shared_ptr<LiveUser> &target,
				const boost::format &message) const;
	void NOTICE(const ServiceUser *source,
				const boost::shared_ptr<Server> &target,
				const boost::format &message) const;
	void JOIN(const ServiceUser *source,
			  const boost::shared_ptr<LiveChannel> &channel) const;
	void PART(const ServiceUser *source,
			  const boost::shared_ptr<LiveChannel> &channel,
			  const std::string &reason = std::string()) const;
	void TOPIC(const ServiceUser *source,
			   const boost::shared_ptr<LiveChannel> &channel,
			   const std::string &topic) const;
	void KICK(const ServiceUser *source,
			  const boost::shared_ptr<LiveChannel> &channel,
			  const boost::shared_ptr<LiveUser> &target,
			  const boost::format &reason) const;
	void INVITE(const ServiceUser *source,
				const boost::shared_ptr<LiveChannel> &channel,
				const boost::shared_ptr<LiveUser> &target) const;
	void MODE(const ServiceUser *source,
			  const std::string &in) const;
	void MODE(const ServiceUser *source,
			  const boost::shared_ptr<LiveChannel> &channel,
			  const std::string &in,
			  const std::vector<std::string> &params = std::vector<std::string>()) const;

	void KILL(const ServiceUser *source,
			  const boost::shared_ptr<LiveUser> &target,
			  const std::string &message = std::string()) const;
	void HELPOP(const ServiceUser *source,
				  const boost::format &message) const;
	void WALLOP(const ServiceUser *source,
				  const boost::format &message) const;
	void GLOBOP(const ServiceUser *source,
				  const boost::format &message) const;
	void ANNOUNCE(const ServiceUser *source,
				  const boost::format &message) const;

	void SVSNICK(const ServiceUser *source,
				 const boost::shared_ptr<LiveUser> &target,
				 const std::string &newnick) const;
	void AKILL(const ServiceUser *source, const Akill &a) const;
	void RAKILL(const ServiceUser *source, const Akill &a) const;

	bool Execute(const ServiceUser *service,
				 const boost::shared_ptr<LiveUser> &user,
				 const std::vector<std::string> &params,
				 unsigned int key = 0) const;

	bool Execute(const ServiceUser *service,
				 const boost::shared_ptr<LiveUser> &user,
				 const std::string &params, unsigned int key = 0) const
	{
		boost::char_separator<char> sep(" \t");
		typedef boost::tokenizer<boost::char_separator<char>,
			std::string::const_iterator, std::string> tokenizer;
		tokenizer tokens(params, sep);
		std::vector<std::string> v(tokens.begin(), tokens.end());
		return Execute(service, user, v, key);
	}
public:
	Service(TraceTypes_t trace);
	virtual ~Service();

	void Check();

	bool Processing() const { return processing_; }
	void Processing(bool in) { processing_ = in; }

	const std::string &Primary() const { return primary_; }
	const nicks_t &Nicks() const { return nicks_; }

	bool IsNick(const std::string &nick) const
		{ return (nicks_.find(nick) != nicks_.end()); }

	/** 
	 * @brief Add a command to the execution 'chain'.
	 * This will make a command recognized only for this service.  The regular
	 * expression will automatically be made to ignore case.  The min_param
	 * value does not include the command itself, even though the params
	 * argument to the function in question has the command as params[0].
	 * The 'perms' list is an 'OR' list, ie. user must be in perms[0] OR
	 * perms[1] OR perms[2].  There is no way to enforce an AND relationship.
	 * 
	 * @param rx Regular expression to match for this command.
	 * @param func Function to call when this command is matched.
	 * @param min_param Minimum number of parameters required.
	 * @param perms List of committees that the user must be a member of one of
	 * 				to be able to execute this function (an empty list means
	 * 				anyone may execute this command).
	 * 
	 * @return A unique identifier for this command.
	 */
	unsigned int PushCommand(const boost::regex &rx, const functor &func, unsigned int min_param = 0,
							 const std::vector<std::string> &perms = std::vector<std::string>());
	unsigned int PushCommand(const std::string &rx, const functor &func, unsigned int min_param = 0,
							 const std::vector<std::string> &perms = std::vector<std::string>())
		{ return PushCommand(boost::regex(rx, boost::regex_constants::icase), func, min_param, perms); }

	/** 
	 * @brief Remove a command from the execution 'chain'.
	 * 
	 * @param id The ID returned by PushCommand.
	 */
	void DelCommand(unsigned int id);

	void MASSQUIT(const std::string &message = std::string())
	{
		for_each(users_.begin(), users_.end(),
				 boost::bind(&Service::QUIT, this, _1, message));
	}

	void PRIVMSG(const boost::shared_ptr<LiveUser> &target,
				 const boost::format &message);
	void PRIVMSG(const boost::shared_ptr<Server> &target,
				 const boost::format &message);
	void NOTICE(const boost::shared_ptr<LiveUser> &target,
				const boost::format &message);
	void NOTICE(const boost::shared_ptr<Server> &target,
				const boost::format &message);
	void JOIN(const boost::shared_ptr<LiveChannel> &channel) const;
	void PART(const boost::shared_ptr<LiveChannel> &channel,
			  const std::string &reason = std::string()) const;
	void TOPIC(const boost::shared_ptr<LiveChannel> &channel,
			   const std::string &topic) const;
	void KICK(const boost::shared_ptr<LiveChannel> &channel,
			  const boost::shared_ptr<LiveUser> &target,
			  const boost::format &reason) const;
	void INVITE(const boost::shared_ptr<LiveChannel> &channel,
				const boost::shared_ptr<LiveUser> &target) const;
	
	void KILL(const boost::shared_ptr<LiveUser> &target,
			  const std::string &message = std::string()) const;
	void HELPOP(const boost::format &message) const;
	void WALLOP(const boost::format &message) const;
	void GLOBOP(const boost::format &message) const;
	void ANNOUNCE(const boost::format &message) const;

	void SVSNICK(const boost::shared_ptr<LiveUser> &target,
				 const std::string &newnick) const;
	void AKILL(const Akill &a) const;
	void RAKILL(const Akill &a) const;

	class CommandMerge
	{
		const Service &service;
		unsigned int primary, secondary;

	public:
		CommandMerge(const Service &serv, unsigned int p, unsigned int s)
			: service(serv), primary(p), secondary(s) {}

		bool operator()(const ServiceUser *service,
						const boost::shared_ptr<LiveUser> &user,
						const std::vector<std::string> &params);
	};

	bool Help(const ServiceUser *service,
			  const boost::shared_ptr<LiveUser> &user,
			  const std::vector<std::string> &params) const;
	bool AuxHelp(const ServiceUser *service,
				 const boost::shared_ptr<LiveUser> &user,
				 const std::vector<std::string> &params) const;
};

// Special interface used by Magick.
class if_Service_Magick
{
	friend class Magick;
	Service &base;

	// This is INTENTIONALLY private ...
	if_Service_Magick(Service &b) : base(b) {}
	if_Service_Magick(Service *b) : base(*b) {}

	void Set(const std::vector<std::string> &nicks, const std::string &real = std::string())
		{ base.Set(nicks, real); }
};

void init_nickserv_functions(Service &serv);
void init_chanserv_functions(Service &serv);
void init_memoserv_functions(Service &serv);
void init_commserv_functions(Service &serv);
void init_operserv_functions(Service &serv);
void init_other_functions(Service &serv);

#endif // _MAGICK_SERVICE_H
