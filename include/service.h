#ifndef WIN32
#pragma interface
#endif

/* Magick IRC Services
**
** (c) 2004 The Neuromancy Society <info@neuromancy.net>
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

#include <set>
#include <string>
#include <vector>

#include <mantra/core/algorithms.h>
#include <mantra/core/sync.h>

#include <boost/format.hpp>
#include <boost/bind.hpp>
#include <boost/tokenizer.hpp>

class LiveUser;

class Service
{
	friend class if_Service_LiveUser;
	friend class if_Service_Magick;

public:
	typedef boost::function3<bool, const boost::shared_ptr<LiveUser> &,
							 const boost::shared_ptr<LiveUser> &,
							 const std::vector<std::string> &> functor;
	typedef std::set<std::string, mantra::iless<std::string> > nicks_t;
	typedef std::set<boost::shared_ptr<LiveUser> > users_t;

private:
	struct Command_t
	{
		unsigned int id;
		std::vector<std::string> perms;
		boost::regex rx;
		functor func;
	};

	typedef std::deque<Command_t> func_map_t;

	std::string real_;
	std::string primary_;
	nicks_t nicks_;
	users_t RWSYNC(users_);

	func_map_t RWSYNC(func_map_);

	void Set(const std::vector<std::string> &nicks, const std::string &real = std::string());
	void SIGNOFF(const boost::shared_ptr<LiveUser> &user);
	boost::shared_ptr<LiveUser> SIGNON(const std::string &nick);
public:
	Service();
	virtual ~Service();

	void Check();

	const std::string &Primary() const { return primary_; }
	const nicks_t &Nicks() const { return nicks_; }

	bool IsNick(const std::string &nick) const
		{ return (nicks_.find(nick) != nicks_.end()); }

	unsigned int PushCommand(const boost::regex &rx, const functor &func,
							 const std::vector<std::string> &perms = std::vector<std::string>());
	unsigned int PushCommand(const std::string &rx, const functor &func,
							 const std::vector<std::string> &perms = std::vector<std::string>())
		{ return PushCommand(boost::regex(rx, boost::regex_constants::icase), func, perms); }
	void DelCommand(unsigned int id);

	void QUIT(const boost::shared_ptr<LiveUser> &source,
			  const std::string &message = std::string());
	void MASSQUIT(const std::string &message = std::string())
	{
		for_each(users_.begin(), users_.end(),
				 boost::bind(&Service::QUIT, this, _1, message));
	}
	void KILL(const boost::shared_ptr<LiveUser> &source,
			  const boost::shared_ptr<LiveUser> &target,
			  const std::string &message = std::string());
	void KILL(const boost::shared_ptr<LiveUser> &target,
			  const std::string &message = std::string());

	void PRIVMSG(const boost::shared_ptr<LiveUser> &source,
				 const boost::shared_ptr<LiveUser> &target,
				 const boost::format &message) const;
	void PRIVMSG(const boost::shared_ptr<LiveUser> &target,
				 const boost::format &message) const;

	void NOTICE(const boost::shared_ptr<LiveUser> &source,
				const boost::shared_ptr<LiveUser> &target,
				const boost::format &message) const;
	void NOTICE(const boost::shared_ptr<LiveUser> &target,
				const boost::format &message) const;

	void ANNOUNCE(const boost::shared_ptr<LiveUser> &source,
				  const boost::format &message) const;
	void ANNOUNCE(const boost::format &message) const;

	class CommandMerge
	{
		const Service &service;
		unsigned int primary, secondary;

	public:
		CommandMerge(const Service &serv, unsigned int p, unsigned int s)
			: service(serv), primary(p), secondary(s) {}

		bool operator()(const boost::shared_ptr<LiveUser> &service,
						const boost::shared_ptr<LiveUser> &user,
						const std::vector<std::string> &params);
	};

	bool Help(const boost::shared_ptr<LiveUser> &service,
			  const boost::shared_ptr<LiveUser> &user,
			  const std::vector<std::string> &params) const;
	bool AuxHelp(const boost::shared_ptr<LiveUser> &service,
				 const boost::shared_ptr<LiveUser> &user,
				 const std::vector<std::string> &params) const;

	bool Execute(const boost::shared_ptr<LiveUser> &service,
				 const boost::shared_ptr<LiveUser> &user,
				 const std::vector<std::string> &params,
				 unsigned int key = 0) const;

	bool Execute(const boost::shared_ptr<LiveUser> &service,
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

class if_Service_LiveUser
{
	friend class LiveUser;
	Service &base;

	// This is INTENTIONALLY private ...
	if_Service_LiveUser(Service &b) : base(b) {}
	if_Service_LiveUser(Service *b) : base(*b) {}

	void SIGNOFF(const boost::shared_ptr<LiveUser> &user)
		{ base.SIGNOFF(user); }
};

void init_nickserv_functions(Service &serv);
void init_chanserv_functions(Service &serv);
void init_memoserv_functions(Service &serv);
void init_commserv_functions(Service &serv);
void init_operserv_functions(Service &serv);
void init_other_functions(Service &serv);

#endif // _MAGICK_SERVICE_H
