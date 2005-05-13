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
#ifndef _MAGICK_LIVECHANNEL_H
#define _MAGICK_LIVECHANNEL_H 1

#ifdef RCSID
RCSID(magick__livechannel_h, "@(#) $Id$");
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
#include "storage.h"

#include <mantra/core/sync.h>

#include <boost/thread/mutex.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>
#include <boost/noncopyable.hpp>
#include <boost/operators.hpp>

class LiveChannel : private boost::noncopyable, public boost::totally_ordered1<LiveChannel>
{
	class ClearPart;
	friend class if_LiveChannel_StoredChannel;
	friend class if_LiveChannel_LiveUser;
	friend class LiveChannel::ClearPart;

	class PendingModes
	{
		unsigned int event_;
		boost::shared_ptr<LiveChannel> channel_;
		boost::shared_ptr<LiveUser> user_;
		std::string on_, off_;
		std::set<std::string> on_params_, off_params_;

		void operator()() const;
	public:
		PendingModes(const boost::shared_ptr<LiveChannel> &channel,
					 const boost::shared_ptr<LiveUser> &user);
		void Update(const std::string &in);
		void Cancel();
	};

	class ClearPart
	{
		boost::shared_ptr<LiveChannel> channel_;
		boost::shared_ptr<LiveUser> user_;
	public:
		ClearPart(const boost::shared_ptr<LiveChannel> &channel,
				  const boost::shared_ptr<LiveUser> &user)
			: channel_(channel), user_(user) {}

		void operator()() const
		{
			SYNCP_LOCK(channel_, recent_parts_);
			channel_->recent_parts_.erase(user_);
		}
	};

	typedef std::map<boost::shared_ptr<LiveUser>, std::string> users_t;
	typedef std::map<std::string, std::pair<boost::posix_time::ptime, unsigned int> > bans_t;
	typedef std::set<std::string> exempts_t;
	typedef std::map<boost::shared_ptr<LiveUser>, PendingModes> pending_modes_t; 
	typedef std::map<boost::shared_ptr<LiveUser>, unsigned int> recent_parts_t;

	boost::weak_ptr<LiveChannel> self;

	std::string name_;
	std::string id_;
	boost::posix_time::ptime created_;
	boost::posix_time::ptime seen_;

	boost::shared_ptr<StoredChannel> RWSYNC(stored_);

	users_t RWSYNC(users_);
	users_t splits_;

	bans_t RWSYNC(bans_);
	exempts_t RWSYNC(exempts_);

	std::string SYNC(topic_);
	std::string topic_setter_;
	boost::posix_time::ptime topic_set_time_;

	std::string SYNC(modes_);
	std::string modes_key_;
	unsigned int modes_limit_;

	pending_modes_t SYNC(pending_modes_);
	recent_parts_t SYNC(recent_parts_);

	// use if_LiveChannel_StoredChannel
	void Stored(const boost::shared_ptr<StoredChannel> &stored);

	// use if_LiveChannel_LiveUser
	void Quit(const boost::shared_ptr<LiveUser> &user);

	LiveChannel(const std::string &name,
				const boost::posix_time::ptime &created,
				const std::string &id);
public:
	static inline boost::shared_ptr<LiveChannel> create(const std::string &name,
			const boost::posix_time::ptime &created = mantra::GetCurrentDateTime(),
			const std::string &id = std::string())
	{
		boost::shared_ptr<LiveChannel> rv(new LiveChannel(name, created, id));
		rv->self = rv;
		return rv;
	}

	const std::string &Name() const { return name_; }
	const std::string &ID() const { return id_; }
	const boost::posix_time::ptime &Created() const { return created_; }
	const boost::posix_time::ptime &Seen() const { return seen_; }

	bool operator<(const LiveChannel &rhs) const { return Name() < rhs.Name(); }
	bool operator==(const LiveChannel &rhs) const { return Name() == rhs.Name(); }

	void Join(const boost::shared_ptr<LiveUser> &user);
	void Part(const boost::shared_ptr<LiveUser> &user,
			  const std::string &reason = std::string());
	void Kick(const boost::shared_ptr<LiveUser> &user,
			  const boost::shared_ptr<LiveUser> &kicker, const std::string &reason);

	bool RecentPart(const boost::shared_ptr<LiveUser> &user) const;

	// Its more efficient to populate in this instance.
	void Users(std::map<boost::shared_ptr<LiveUser>, std::string> &users) const;
	bool IsUser(const boost::shared_ptr<LiveUser> &user) const;
	std::string User(const boost::shared_ptr<LiveUser> &user) const;
	void Splits(std::map<boost::shared_ptr<LiveUser>, std::string> &splitusers) const;
	bool IsSplit(const boost::shared_ptr<LiveUser> &user) const;
	std::string Split(const boost::shared_ptr<LiveUser> &user) const;

	void Bans(std::map<std::string, boost::posix_time::ptime> &bans) const;
	bool MatchBan(const std::string &in) const;
	bool MatchBan(const boost::shared_ptr<LiveUser> &in) const;
	void Exempts(std::set<std::string> &exempts) const;
	bool MatchExempt(const std::string &in) const;
	bool MatchExempt(const boost::shared_ptr<LiveUser> &in) const;

	bool IsBanned(const boost::shared_ptr<LiveUser> &in) const
		{ return (MatchBan(in) && !MatchExempt(in)); }

	void Topic(const std::string &topic, const std::string &setter, 
			   const boost::posix_time::ptime &set_time);
	std::string Topic() const;
	std::string Topic_Setter() const;
	boost::posix_time::ptime Topic_Set_Time() const;

	void Modes(const boost::shared_ptr<LiveUser> &user,
			   const std::string &in, const std::string &params = std::string());
	void SendModes(const boost::shared_ptr<LiveUser> &user,
				   const std::string &in, const std::string &params = std::string());
	std::string Modes() const;
	std::string Modes_Key() const;
	unsigned int Modes_Limit() const;
};

// Special interface used by StoredChannel.
class if_LiveChannel_StoredChannel
{
	friend class StoredChannel;
	LiveChannel &base;

	// This is INTENTIONALLY private ...
	if_LiveChannel_StoredChannel(LiveChannel &b) : base(b) {}
	if_LiveChannel_StoredChannel(const boost::shared_ptr<LiveChannel> &b) : base(*(b.get())) {}

	inline void Stored(const boost::shared_ptr<StoredChannel> &channel)
		{ base.Stored(channel); }
};

// Special interface used by LiveUser.
class if_LiveChannel_LiveUser
{
	friend class LiveUser;
	LiveChannel &base;

	// This is INTENTIONALLY private ...
	if_LiveChannel_LiveUser(LiveChannel &b) : base(b) {}
	if_LiveChannel_LiveUser(const boost::shared_ptr<LiveChannel> &b) : base(*(b.get())) {}

	inline void Quit(const boost::shared_ptr<LiveUser> &user)
		{ base.Quit(user); }
};

// Used for tracing mainly.
inline std::ostream &operator<<(std::ostream &os, const LiveChannel &in)
{
	return (os << in.Name());
}

#endif // _MAGICK_LIVECHANNEL_H
