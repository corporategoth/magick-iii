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
#ifndef _MAGICK_SERVER_H
#define _MAGICK_SERVER_H 1

#ifdef RCSID
RCSID(magick__server_h, "@(#) $Id$");
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
#include "dependency.h"

#include <string>
#include <queue>

#include <mantra/net/socket.h>
#include <mantra/net/socketgroup.h>
#include <mantra/file/filebuffer.h>

#include <boost/shared_ptr.hpp>
#include <boost/date_time/posix_time/ptime.hpp>
#include <boost/thread/thread.hpp>
#include <boost/thread/condition.hpp>

class Server
{
	std::string name_;
	std::string description_;
	std::string id_;
	std::string altname_;

	boost::posix_time::ptime last_ping_;
	boost::posix_time::time_duration lag_;

	Server *parent_;
	std::list<boost::shared_ptr<Server> > children_;

protected:
	void Disconnect();

public:
	Server(const std::string &name, const std::string &desc,
			const std::string &id = std::string(),
			const std::string &altname = std::string());
	virtual ~Server();

	const std::string &Name() const { return name_; }
	const std::string &ID() const { return id_; }
	const std::string &AltName() const { return altname_; }
	const std::string &Description() const { return description_; }

	boost::posix_time::ptime Last_Ping() const { return last_ping_; }
	boost::posix_time::time_duration Lag() const { return lag_; }
	
	const Server *Parent() const { return parent_; }
	const std::list<boost::shared_ptr<Server> > &Children() const { return children_; }

	void Connect(const boost::shared_ptr<Server> &s);
	void Disconnect(const std::string &name);

	virtual void Ping();
	void Pong();

	size_t Users() const;
	size_t Opers() const;
};

class Jupe : public Server
{
	void Connect(const boost::shared_ptr<Server> &s);
protected:
	// Passthrough for Uplink :)
	Jupe(const std::string &name, const std::string &desc,
		 const std::string &id, const std::string &altname)
		 : Server(name, desc, id, altname) {}

public:
	Jupe(const std::string &name, const std::string &reason,
		const std::string id = std::string())
		: Server(name, "JUPED (" + reason + ")", id) {}
	virtual ~Jupe() {}

	// PING is disabled for juped servers
	virtual void Ping() {}

	void Connect(const boost::shared_ptr<Jupe> &s)
		{ Server::Connect(s); }
};

class Uplink : public Jupe
{
	friend class Protocol;
	friend class Magick;
	friend class Dependency;

	boost::mutex lock_;
	boost::condition cond_;
	std::deque<Message> pending_;
	std::list<boost::thread *> workers_;
	std::string password_;

	DependencyEngine de_;

	bool write_;
	mantra::FileBuffer flack_;

	void operator()();

	// Assumes classification is done, and pushes it for processing.
	void Push(const Message &in);
public:
	Uplink(const std::string &password,
		   const std::string &id = std::string());
	virtual ~Uplink();

	boost::shared_ptr<Server> Find(const std::string &name) const;
	boost::shared_ptr<Server> FindID(const std::string &id) const;

	virtual void Ping();
	void Connect(const boost::shared_ptr<Server> &s)
		{ Server::Connect(s); }

	bool Write();
	bool CheckPassword(const std::string &password) const;

	// Pushes it onto the stack to be classified.
	void Push(const std::deque<Message> &in);
};

// Used for tracing mainly.
inline std::ostream &operator<<(std::ostream &os, const Server &in)
{
	return (os << in.Name());
}

#endif // _MAGICK_SERVER_H
