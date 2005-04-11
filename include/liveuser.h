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
#ifndef _MAGICK_LIVEUSER_H
#define _MAGICK_LIVEUSER_H 1

#ifdef RCSID
RCSID(magick__liveuser_h, "@(#) $Id$");
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
#include "livememo.h"

#include <mantra/core/sync.h>

#include <boost/thread/mutex.hpp>
#include <boost/thread/read_write_mutex.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>
#include <boost/noncopyable.hpp>
#include <boost/operators.hpp>

class Server;
class Service;

class LiveUser : private boost::noncopyable, public boost::totally_ordered1<LiveUser>
{
	friend class if_LiveUser_StoredNick;
	friend class if_LiveUser_LiveChannel;
	friend class if_LiveUser_StoredChannel;
	friend class if_LiveUser_LiveMemo;

	typedef std::queue<boost::posix_time::ptime> messages_t;
	typedef std::set<boost::shared_ptr<LiveChannel> > channel_joined_t;
	typedef std::set<boost::shared_ptr<StoredChannel> > channel_identified_t;
	typedef std::map<boost::shared_ptr<StoredChannel> , unsigned int> channel_password_fails_t;
	typedef std::set<boost::shared_ptr<Committee> > committees_t;
	typedef std::map<unsigned int, LiveMemo> pending_memos_t;

	// This is used so we can send a shared_ptr to others.
	boost::weak_ptr<LiveUser> self;

	// This is the only class with a changable name.
	std::string RWSYNC(name_);

	// These values don't need to be locked ...
	std::string id_;
	std::string real_;
	std::string user_;
	std::string host_;
	boost::shared_ptr<Server> server_;
	boost::posix_time::ptime signon_;
	boost::posix_time::ptime seen_;
	const Service *service_;

	// From here on, they do.
	// This is the 'general' lock, some stuff has a more specific
	// lock (using the SYNC mechanism), but for stuff that is only
	// to be locked for VERY brief times, the general lock is fine.
	NSYNC(LiveUser);

	std::string alt_host_;
	std::string away_;

	std::string SYNC(modes_);

	// Sync here includes flood_triggers.
	messages_t SYNC(messages_);
	unsigned int flood_triggers_;
	bool ignored_;

	// If this is set, we're recognized.
	bool identified_;
	boost::shared_ptr<StoredNick> RWSYNC(stored_);
	unsigned int password_fails_;

	channel_joined_t SYNC(channel_joined_);
	channel_identified_t SYNC(channel_identified_);
	channel_password_fails_t channel_password_fails_;
	committees_t SYNC(committees_);

	boost::posix_time::ptime last_nick_reg_, last_channel_reg_;
	boost::posix_time::ptime last_memo_;

	pending_memos_t SYNC(pending_memos_);

	// use if_LiveUser_StoredNick
	void Stored(const boost::shared_ptr<StoredNick> &nick);
	void Nick_Reg();

	// use if_LiveUser_LiveChannel
	void Join(const boost::shared_ptr<LiveChannel> &channel);
	void Part(const boost::shared_ptr<LiveChannel> &channel);

	// use if_LiveUser_StoredChannel
	void Channel_Reg();

	// use if_LiveUser_LiveMemo
	void Memo();

	void unignore();
	LiveUser(const Service *service, const std::string &name,
			 const boost::shared_ptr<Server> &server,
			 const std::string &id);
	LiveUser(const std::string &name, const std::string &real,
			 const std::string &user, const std::string &host,
			 const boost::shared_ptr<Server> &server,
			 const boost::posix_time::ptime &signon,
			 const std::string &id);
public:
	static inline boost::shared_ptr<LiveUser> create(Service *s,
			const std::string &name,
			const boost::shared_ptr<Server> &server,
			const std::string &id = std::string())
	{
		boost::shared_ptr<LiveUser> rv(new LiveUser(s, name, server, id));
		rv->self = rv;
		return rv;
	}

	static inline boost::shared_ptr<LiveUser> create(const std::string &name,
			const std::string &real, const std::string &user,
			const std::string &host, const boost::shared_ptr<Server> &server,
			boost::posix_time::ptime signon,
			const std::string &id = std::string())
	{
		boost::shared_ptr<LiveUser> rv(new LiveUser(name, real, user, host,
													server, signon, id));
		rv->self = rv;
		return rv;
	}

	void Name(const std::string &in);
	inline std::string Name() const
	{
		SYNC_RLOCK(name_);
		return name_;
	}

	const std::string &ID() const { return id_; }
	const std::string &Real() const { return real_; }
	const std::string &User() const { return user_; }
	const std::string &Host() const { return host_; }
	const boost::shared_ptr<Server> &GetServer() const { return server_; }
	const boost::posix_time::ptime &Signon() const { return signon_; }
	const boost::posix_time::ptime &Seen() const { return seen_; }
	const Service *GetService() const { return service_; }
	bool Matches(const std::string &mask) const
		{ return mantra::glob_match(mask, Name() + "!" + User() + "@" + Host(), true); }

	bool operator<(const LiveUser &rhs) const { return Name() < rhs.Name(); }
	bool operator==(const LiveUser &rhs) const { return Name() == rhs.Name(); }

	// Everything below needs locking ...

	boost::shared_ptr<StoredNick> Stored() const;
	void Quit(const std::string &reason);

	void AltHost(const std::string &in);
	std::string AltHost() const;
	bool AltMatches(const std::string &mask) const
		{ return mantra::glob_match(mask, Name() + "!" + User() + "@" + AltHost(), true); }

	void Away(const std::string &in);
	std::string Away() const;

	// The following alters modes, it doesn't outright set them.
	void Modes(const std::string &in);
	std::string Modes() const;

	bool Action();
	bool Ignored() const;

	bool Identify(const std::string &in);
	void UnIdentify();
	bool Recognized() const;
	bool Identified() const;

	bool Identify(const boost::shared_ptr<StoredChannel> &channel,
				  const std::string &in);
	void UnIdentify(const boost::shared_ptr<StoredChannel> &channel);
	bool Identified(const boost::shared_ptr<StoredChannel> &channel) const;

	bool InChannel(const boost::shared_ptr<LiveChannel> &channel) const;

	boost::posix_time::ptime Last_Nick_Reg() const;
	boost::posix_time::ptime Last_Channel_Reg() const;
	boost::posix_time::ptime Last_Memo() const;
};

// Special interface used by StoredNick.
class if_LiveUser_StoredNick
{
	friend class StoredNick;
	LiveUser &base;

	// This is INTENTIONALLY private ...
	if_LiveUser_StoredNick(LiveUser &b) : base(b) {}
	if_LiveUser_StoredNick(const boost::shared_ptr<LiveUser> &b) : base(*(b.get())) {}

	inline void Stored(const boost::shared_ptr<StoredNick> &nick)
		{ base.Stored(nick); }
	inline void Nick_Reg()
		{ base.Nick_Reg(); }
};

// Special interface used by LiveChannel.
class if_LiveUser_LiveChannel
{
	friend class LiveChannel;
	LiveUser &base;

	// This is INTENTIONALLY private ...
	if_LiveUser_LiveChannel(LiveUser &b) : base(b) {}
	if_LiveUser_LiveChannel(const boost::shared_ptr<LiveUser> &b) : base(*(b.get())) {}

	inline void Join(const boost::shared_ptr<LiveChannel> &channel)
		{ base.Join(channel); }
	inline void Part(const boost::shared_ptr<LiveChannel> &channel)
		{ base.Part(channel); }
};

// Special interface used by StoredChannel.
class if_LiveUser_StoredChannel
{
	friend class StoredChannel;
	LiveUser &base;

	// This is INTENTIONALLY private ...
	if_LiveUser_StoredChannel(LiveUser &b) : base(b) {}
	if_LiveUser_StoredChannel(const boost::shared_ptr<LiveUser> &b) : base(*(b.get())) {}

	inline void Channel_Reg()
		{ base.Channel_Reg(); }
};

// Special interface used by LiveMemo.
class if_LiveUser_LiveMemo
{
	friend class LiveMemo;
	LiveUser &base;

	// This is INTENTIONALLY private ...
	if_LiveUser_LiveMemo(LiveUser &b) : base(b) {}
	if_LiveUser_LiveMemo(const boost::shared_ptr<LiveUser> &b) : base(*(b.get())) {}

	inline void Memo()
		{ base.Memo(); }
};

// Used for tracing mainly.
inline std::ostream &operator<<(std::ostream &os, const LiveUser &in)
{
	return (os << in.Name());
}

#endif // _MAGICK_LIVEUSER_H

