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

#include "committee.h"

#include <mantra/core/sync.h>

#include <boost/thread/mutex.hpp>
#include <boost/thread/read_write_mutex.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>
#include <boost/noncopyable.hpp>
#include <boost/operators.hpp>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/mem_fun.hpp>

class Server;
class StoredNick;
class ServiceUser;

// These macros are used to send a notice/message to a user in their
// own language, and using their preferred communication mechanism.

#define	U_(x,y) (x)->translate(y)
#define	UP_(w,x,y,z) (w)->translate((x),(y),(z))

// Used as:
// SEND(service, user, format_string, format_args);
// NSEND(service, user, format_string);
#define SEND(w,x,y,z) (x)->send((w), (x)->format(y) % z)
#define NSEND(w,x,z) (x)->send((w), (x)->format(z))

class LiveUser : private boost::noncopyable,
				 public boost::totally_ordered1<LiveUser>,
				 public boost::totally_ordered2<LiveUser, std::string>
{
	friend class if_LiveUser_Storage;
	friend class if_LiveUser_StoredNick;
	friend class if_LiveUser_LiveChannel;
	friend class if_LiveUser_StoredChannel;
	friend class if_LiveUser_LiveMemo;

	struct id {};
	struct name {};

	typedef boost::multi_index::multi_index_container<
				boost::shared_ptr<Committee>,
				boost::multi_index::indexed_by<
					boost::multi_index::ordered_unique<boost::multi_index::identity<Committee> >,
					boost::multi_index::ordered_unique<boost::multi_index::tag<id>,
													   BOOST_MULTI_INDEX_CONST_MEM_FUN(Committee, boost::uint32_t, ID)>,
					boost::multi_index::ordered_unique<boost::multi_index::tag<name>,
													   BOOST_MULTI_INDEX_CONST_MEM_FUN(Committee, const std::string &, Name),
													   mantra::iless<std::string> >
				> > committees_t;
	typedef committees_t::index<id>::type committees_by_id_t;
	typedef committees_t::index<name>::type committees_by_name_t;

	typedef std::set<boost::shared_ptr<LiveChannel> > channel_joined_t;
	typedef std::queue<boost::posix_time::ptime> messages_t;
	typedef std::set<boost::shared_ptr<StoredChannel> > channel_identified_t;
	typedef std::map<boost::shared_ptr<StoredChannel>, unsigned int> channel_password_fails_t;
	typedef std::map<boost::shared_ptr<StoredChannel>, std::pair<std::string, unsigned int> > channel_drop_token_t;
	typedef std::map<unsigned int, LiveMemo> pending_memos_t;

protected:
	// This is used so we can send a shared_ptr to others.
	boost::weak_ptr<LiveUser> self;

private:
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

	// From here on, they do.
	// This is the 'general' lock, some stuff has a more specific
	// lock (using the SYNC mechanism), but for stuff that is only
	// to be locked for VERY brief times, the general lock is fine.
	NSYNC(LiveUser);

	std::string alt_host_;
	std::string away_;

	std::set<char> SYNC(modes_);

	// Sync here includes flood_triggers.
	messages_t SYNC(messages_);
	unsigned int flood_triggers_;
	bool ignored_;
	unsigned int ignore_timer_;

	// If this is set, we're recognized.
	unsigned int ident_timer_;
	bool identified_;
	boost::shared_ptr<StoredNick> RWSYNC(stored_);
	unsigned int password_fails_;
	std::pair<std::string, unsigned int> SYNC(drop_token_);

	channel_joined_t SYNC(channel_joined_);
	channel_identified_t SYNC(channel_identified_);
	channel_password_fails_t channel_password_fails_;
	channel_drop_token_t SYNC(channel_drop_token_);

	committees_t SYNC(committees_);
	committees_by_id_t &committees_by_id_;
	committees_by_name_t &committees_by_name_;

	boost::posix_time::ptime last_nick_reg_, last_channel_reg_;
	boost::posix_time::ptime last_memo_;

	pending_memos_t SYNC(pending_memos_);

	// use if_LiveUser_Storage
	void Name(const std::string &in);

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
	void protect();
	LiveUser(const std::string &name, const std::string &real,
			 const std::string &user, const std::string &host,
			 const boost::shared_ptr<Server> &server,
			 const boost::posix_time::ptime &signon,
			 const std::string &id);

protected:
	LiveUser(const std::string &name, const std::string &real,
			 const boost::shared_ptr<Server> &server,
			 const std::string &id);

public:
	virtual ~LiveUser();

	static boost::shared_ptr<LiveUser> create(const std::string &name,
			const std::string &real, const std::string &user,
			const std::string &host, const boost::shared_ptr<Server> &server,
			const boost::posix_time::ptime &signon,
			const std::string &id = std::string());

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
	bool Matches(const std::string &mask) const
		{ return mantra::glob_match(mask, Name() + "!" + User() + "@" + Host(), true); }
	bool Matches(const boost::regex &mask) const
	{
		boost::regex rx(mask.str(), mask.flags() | boost::regex_constants::icase);
		return boost::regex_match(Name() + "!" + User() + "@" + Host(), rx);
	}

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
	inline bool operator<(const LiveUser &rhs) const { return *this < rhs.Name(); }
	inline bool operator==(const LiveUser &rhs) const { return *this == rhs.Name(); }

	// Everything below needs locking ...

	boost::shared_ptr<StoredNick> Stored() const;
	void Quit(const std::string &reason = std::string());
	void Kill(const boost::shared_ptr<LiveUser> &killer,
			  const std::string &reason);

	void AltHost(const std::string &in);
	std::string AltHost() const;
	bool AltMatches(const std::string &mask) const
		{ return mantra::glob_match(mask, Name() + "!" + User() + "@" + AltHost(), true); }
	bool AltMatches(const boost::regex &mask) const
	{
		boost::regex rx(mask.str(), mask.flags() | boost::regex_constants::icase);
		return boost::regex_match(Name() + "!" + User() + "@" + AltHost(), rx);
	}
	inline std::string PrefHost() const
	{
		std::string rv = AltHost();
		if (rv.empty())
			return Host();
		return rv;
	}

	void Away(const std::string &in);
	std::string Away() const;

	// The following alters modes, it doesn't outright set them.
	void Modes(const std::string &in);
	std::set<char> Modes() const;
	inline bool Mode(const char c) const
	{
		std::set<char> modes = Modes();
		return (modes.find(c) != modes.end());
	}

	bool Action();
	bool Ignored() const;

	bool Identify(const std::string &in);
	void UnIdentify();
	bool Recognized() const;
	bool Identified() const;

	void DropToken(const std::string &in);
	std::string DropToken() const;

	bool Identify(const boost::shared_ptr<StoredChannel> &channel,
				  const std::string &in);
	void UnIdentify(const boost::shared_ptr<StoredChannel> &channel);
	bool Identified(const boost::shared_ptr<StoredChannel> &channel) const;
	void DropToken(const boost::shared_ptr<StoredChannel> &chan, const std::string &in);
	std::string DropToken(const boost::shared_ptr<StoredChannel> &chan) const;

	bool InChannel(const boost::shared_ptr<LiveChannel> &channel) const;
	bool InChannel(const std::string &channel) const;
	bool InCommittee(const boost::shared_ptr<Committee> &committee) const;
	bool InCommittee(const std::string &committee) const;

	boost::posix_time::ptime Last_Nick_Reg() const;
	boost::posix_time::ptime Last_Channel_Reg() const;
	boost::posix_time::ptime Last_Memo() const;

	std::string translate(const std::string &in) const;
	std::string translate(const std::string &single,
						  const std::string &plural,
						  unsigned long n) const;

	boost::format format(const std::string &in) const;
	void send(const ServiceUser *service, const boost::format &fmt) const;
};

// Special interface used by Storage.
class if_LiveUser_Storage
{
	friend class Storage;
	LiveUser &base;

	// This is INTENTIONALLY private ...
	if_LiveUser_Storage(LiveUser &b) : base(b) {}
	if_LiveUser_Storage(const boost::shared_ptr<LiveUser> &b) : base(*(b.get())) {}

	inline void Name(const std::string &in)
		{ base.Name(in); }
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

template<typename T>
inline bool operator<(const boost::shared_ptr<LiveUser> &lhs, const T &rhs)
{
	if (!lhs)
		return true;
	else
		return (*lhs < rhs);
}

inline bool operator<(const boost::shared_ptr<LiveUser> &lhs,
					  const boost::shared_ptr<LiveUser> &rhs)
{
	if (!lhs)
		return true;
	else if (!rhs)
		return false;
	else
		return (*lhs < *rhs);
}

#endif // _MAGICK_LIVEUSER_H

