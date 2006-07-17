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

#include <mantra/core/sync.h>
#include <mantra/net/socket.h>
#include <mantra/net/socketgroup.h>
#include <mantra/file/filebuffer.h>
#include <mantra/service/messagequeue.h>

#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>
#include <boost/date_time/posix_time/ptime.hpp>
#include <boost/thread/thread.hpp>
#include <boost/thread/condition.hpp>

class LiveUser;

struct remote_connection
{
	std::string host;
	boost::uint16_t port;
	std::string password;
	unsigned int numeric;

	remote_connection() : port(0), numeric(0) {}
	~remote_connection() {}

	// These keep GCC4 happy ..
	remote_connection(const remote_connection &in) { *this = in; }
	remote_connection &operator=(const remote_connection &in)
	{
		host = in.host;
		port = in.port;;
		password = in.password;
		numeric = in.numeric;
		return *this;
	}
};

class Server : private boost::noncopyable,
			   public boost::totally_ordered1<Server>,
			   public boost::totally_ordered2<Server, std::string>
{
	friend class if_Server_LiveUser;
	friend class ChildrenAction;
	friend class UsersAction;

public:
	typedef std::set<boost::shared_ptr<Server> > children_t;
	typedef std::set<boost::shared_ptr<LiveUser> > users_t;

private:
	std::string name_;
	std::string description_;
	std::string id_;
	std::string altname_;

	boost::posix_time::time_duration SYNC(lag_);
	boost::posix_time::ptime last_ping_;
	unsigned int ping_event_;

	boost::shared_ptr<Server> parent_;
	children_t SYNC(children_);
	users_t RWSYNC(users_);

	void Signon(const boost::shared_ptr<LiveUser> &lu);
	void Signoff(const boost::shared_ptr<LiveUser> &lu);

protected:
	boost::weak_ptr<Server> self;

	Server(const std::string &name, const std::string &desc,
			const std::string &id = std::string(),
			const std::string &altname = std::string());

	virtual void Disconnect();

public:
	static boost::shared_ptr<Server> create(const std::string &name,
			const std::string &desc, const std::string &id = std::string(),
			const std::string &altname = std::string())
	{
		boost::shared_ptr<Server> rv(new Server(name, desc, id, altname));
		rv->self = rv;
		return rv;
	}
	virtual ~Server();

	inline bool operator<(const std::string &rhs) const
	{
#ifdef CASE_SPECIFIC_SORT
		static mantra::less<std::string> cmp;
#else
		static mantra::iless<std::string> cmp;
#endif
		return cmp(Name(), rhs);
	}
	inline bool operator==(const std::string &rhs) const
	{
#ifdef CASE_SPECIFIC_SORT
		static mantra::equal_to<std::string> cmp;
#else
		static mantra::iequal_to<std::string> cmp;
#endif
		return cmp(Name(), rhs);
	}
	inline bool operator<(const Server &rhs) const { return *this < rhs.Name(); }
	inline bool operator==(const Server &rhs) const { return *this == rhs.Name(); }

	class ChildrenAction
	{
		bool recursive_;
	protected:
		virtual bool visitor(const boost::shared_ptr<Server> &) = 0;

	public:
		ChildrenAction(bool recursive) : recursive_(recursive) {}
		virtual ~ChildrenAction() {}

		bool operator()(const boost::shared_ptr<Server> &start);
	};

	class UsersAction
	{
		bool recursive_;
	protected:
		virtual bool visitor(const boost::shared_ptr<LiveUser> &) = 0;

	public:
		UsersAction(bool recursive) : recursive_(recursive) {}
		virtual ~UsersAction() {}

		bool operator()(const boost::shared_ptr<Server> &start);
	};

	const std::string &Name() const { return name_; }
	const std::string &ID() const { return id_; }
	const std::string &AltName() const { return altname_; }
	const std::string &Description() const { return description_; }
	const boost::shared_ptr<Server> Parent() const { return parent_; }

	boost::posix_time::ptime Last_Ping() const;
	boost::posix_time::time_duration Lag() const;
	
	void Connect(const boost::shared_ptr<Server> &s);
	bool Disconnect(const boost::shared_ptr<Server> &s);
	bool Disconnect(const std::string &name);

	size_t Children() const;
	size_t Users() const;
	size_t Opers() const;

	children_t getChildren() const;
	users_t getUsers() const;

	virtual void PING();
	virtual void PONG();
	virtual void SQUIT(const std::string &reason = std::string());
};

class Jupe : public Server
{
	Jupe(const std::string &name, const std::string &reason,
		const std::string id = std::string())
		: Server(name, "JUPED (" + reason + ")", id) {}
protected:
	using Server::self;

	// Passthrough for Uplink :)
	Jupe(const std::string &name, const std::string &desc,
		 const std::string &id, const std::string &altname)
		 : Server(name, desc, id, altname) {}

public:
	static boost::shared_ptr<Server> create(const std::string &name,
		const std::string &reason, const std::string id = std::string())
	{
		Jupe *j = new Jupe(name, reason, id);
		boost::shared_ptr<Server> rv(j);
		j->self = rv;
		return rv;
	}
	virtual ~Jupe() {}

	// PING is disabled for juped servers
	virtual void PING() {}
	virtual void PONG() {}
	virtual void SQUIT(const std::string &reason = std::string());

	bool RAW(const std::string &cmd,
			 const std::vector<std::string> &args,
			 bool forcecolon = false) const;
	inline bool RAW(const std::string &cmd,
					const std::string &arg,
					bool forcecolon = false) const
	{
		std::vector<std::string> v;
		v.push_back(arg);
		return RAW(cmd, v, forcecolon);
	}

};

class Uplink : public Jupe
{
	friend class Protocol;
	friend class Dependency;

	mantra::Socket sock_;
	mantra::SocketGroup group_;
	bool disconnect_;

	std::string password_;
	bool SYNC(burst_);

	mantra::MessageQueue<Message> queue_;
	mantra::FileBuffer flack_;

	bool Write(const char *buf, size_t sz);
	Uplink(const mantra::Socket &sock, const std::string &password,
		   const std::string &id);

public:
	static boost::shared_ptr<Uplink> create(const remote_connection &rc,
											const boost::function0<bool> &check);

	virtual ~Uplink() { this->Disconnect(); }

	boost::shared_ptr<Server> Find(const std::string &name) const;
	boost::shared_ptr<Server> FindID(const std::string &id) const;

	using Server::Disconnect;
	void Disconnect();

	bool CheckPassword(const std::string &password) const;

	DependencyEngine de;

	// Pushes it onto the stack to be classified.
//	void Push(const std::deque<Message> &in);
	bool operator()(const boost::function0<bool> &check);

	bool KILL(const boost::shared_ptr<LiveUser> &user, const std::string &reason) const;

	enum Numeric_t
		{
			nSTATSEND = 219,
			nADMINME = 256,
			nADMINLOC1 = 257,
			nADMINLOC2 = 258,
			nADMINEMAIL = 259,
			nTRACEEND = 262,
			nUSERHOST = 302,
			nISON = 303,
			nVERSION = 351,
			nLINKS = 365,
			nLINKSEND = 365,
			nNAMESEND = 366,
			nINFO = 371,
			nMOTD = 372,
			nINFOSTART = 373,
			nINFOEND = 374,
			nMOTDSTART = 375,
			nMOTDEND = 376,
			nTIME = 391,
			nNOSUCHNICK = 401,
			nNOSUCHSERVER = 402,
			nNOSUCHCHANNEL = 403,
			nNEED_MORE_PARAMS = 461,
			nINCORRECT_PASSWORD = 464,
			nUSERNOTINCHANNEL = 442,
			nSUMMONDISABLED = 445,
			nUSERSDISABLED = 446
		};

	bool NUMERIC(Numeric_t num, const std::string &target,
				 const std::vector<std::string> &args,
				 bool forcecolon = false) const;
	inline bool NUMERIC(Numeric_t num, const std::string &target,
						const std::string &arg,
						bool forcecolon = false) const
	{
		std::vector<std::string> v;
		v.push_back(arg);
		return NUMERIC(num, target, v, forcecolon);
	}

	bool ERROR(const std::string &arg) const;
};

// Special interface used by LiveUser.
class if_Server_LiveUser
{
	friend class LiveUser;
	friend class ServiceUser;
	Server &base;

	// This is INTENTIONALLY private ...
	if_Server_LiveUser(Server &b) : base(b) {}
	if_Server_LiveUser(const boost::shared_ptr<Server> &b) : base(*(b.get())) {}

	inline void Signon(const boost::shared_ptr<LiveUser> &lu)
		{ base.Signon(lu); }
	inline void Signoff(const boost::shared_ptr<LiveUser> &lu)
		{ base.Signoff(lu); }
};

// Used for tracing mainly.
inline std::ostream &operator<<(std::ostream &os, const Server &in)
{
	return (os << in.Name());
}

#endif // _MAGICK_SERVER_H
