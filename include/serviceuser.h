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
#ifndef _MAGICK_SERVICEUSER_H
#define _MAGICK_SERVICEUSER_H 1

#ifdef RCSID
RCSID(magick__serviceuser_h, "@(#) $Id$");
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
#include <liveuser.h>
#include <service.h>

class ServiceUser : public LiveUser
{
	Service *service_;

	ServiceUser(Service *service, const std::string &name,
			 const std::string &real,
			 const boost::shared_ptr<Server> &server,
			 const std::string &id);
public:
	~ServiceUser() {}

	static boost::shared_ptr<LiveUser> create(Service *s,
			const std::string &name, const std::string &real,
			const boost::shared_ptr<Server> &server,
			const std::string &id = std::string());

	inline Service *GetService() const { return service_; }
	inline boost::shared_ptr<LiveUser> GetUser() const { return self.lock(); }

	bool Execute(const boost::shared_ptr<LiveUser> &user,
				 const std::vector<std::string> &params,
				 unsigned int key = 0) const
		{ return service_->Execute(this, user, params, key); }
	bool Execute(const boost::shared_ptr<LiveUser> &user,
				 const std::string &params, unsigned int key = 0) const
		{ return service_->Execute(this, user, params, key); }

	void QUIT(const std::string &message = std::string()) const
		{ service_->QUIT(self.lock(), message); }

	void KILL(const boost::shared_ptr<LiveUser> &target,
			  const std::string &message = std::string()) const
		{ service_->KILL(this, target, message); }
	void PRIVMSG(const boost::shared_ptr<LiveUser> &target,
				 const boost::format &message) const
		{ service_->PRIVMSG(this, target, message); }
	void NOTICE(const boost::shared_ptr<LiveUser> &target,
				const boost::format &message) const
		{ service_->NOTICE(this, target, message); }
	void ANNOUNCE(const boost::format &message) const
		{ service_->ANNOUNCE(this, message); }
	void SVSNICK(const boost::shared_ptr<LiveUser> &target,
				 const std::string &newnick) const
		{ service_->SVSNICK(this, target, newnick); }
	void JOIN(const boost::shared_ptr<LiveChannel> &channel) const
		{ service_->JOIN(this, channel); }
	void PART(const boost::shared_ptr<LiveChannel> &channel,
			  const std::string &reason = std::string()) const
		{ service_->PART(this, channel, reason); }
	void TOPIC(const boost::shared_ptr<LiveChannel> &channel,
			   const std::string &topic) const
		{ service_->TOPIC(this, channel, topic); }
	void KICK(const boost::shared_ptr<LiveChannel> &channel,
			  const boost::shared_ptr<LiveUser> &target,
			  const boost::format &reason) const
		{ service_->KICK(this, channel, target, reason); }
	void INVITE(const boost::shared_ptr<LiveChannel> &channel,
				const boost::shared_ptr<LiveUser> &target) const
		{ service_->INVITE(this, channel, target); }
	void MODE(const std::string &in) const
		{ service_->MODE(this, in); }
	void MODE(const boost::shared_ptr<LiveChannel> &channel,
			  const std::string &in,
			  const std::vector<std::string> &params = std::vector<std::string>()) const
		{ service_->MODE(this, channel, in, params); }
};

inline bool IsService(const boost::shared_ptr<LiveUser> &in)
{
	ServiceUser *su = dynamic_cast<ServiceUser *>(in.get());
	return (su != NULL);
}

// Used for tracing mainly.
inline std::ostream &operator<<(std::ostream &os, const ServiceUser *in)
{
	return (os << *in);
}

#endif // _MAGICK_SERVICEUSER_H

