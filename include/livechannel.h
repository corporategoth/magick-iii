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

#include <mantra/core/sync.h>

#include <boost/thread/mutex.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>
#include <boost/noncopyable.hpp>
#include <boost/operators.hpp>
#include <boost/regex.hpp>

class StoredChannel;
class ServiceUser;

class rx_iless
{
public:
	inline bool operator()(const boost::regex &lhs, const boost::regex &rhs) const
	{
		static mantra::iless<std::string> cmp;
		return cmp(lhs.str(), rhs.str());
	}
};

class LiveChannel : private boost::noncopyable,
					public boost::totally_ordered1<LiveChannel>,
					public boost::totally_ordered2<LiveChannel, std::string>
{
	class ClearPart;
	class PendingModes;
	friend class if_LiveChannel_StoredChannel;
	friend class if_LiveChannel_LiveUser;
	friend class LiveChannel::ClearPart;
	friend class LiveChannel::PendingModes;

	class PendingModes
	{
		typedef std::set<char> modes_t;
		typedef std::set<std::string, mantra::iless<std::string> > params_t;
		typedef std::map<char, params_t> modes_params_t;

		NSYNC(lock);
		unsigned int event_;
		boost::shared_ptr<LiveChannel> channel_;
		boost::shared_ptr<LiveUser> user_;
		modes_t on_, off_;
		modes_params_t on_params_, off_params_;

		void operator()();
	public:
		PendingModes(const boost::shared_ptr<LiveChannel> &channel,
					 const boost::shared_ptr<LiveUser> &user);
		void Update(const std::string &in,
					const std::vector<std::string> &params);
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

		void operator()() const;
	};

public:
	typedef std::map<boost::shared_ptr<LiveUser>, std::set<char> > users_t;
	typedef std::map<std::string, std::pair<boost::posix_time::ptime, unsigned int>,
					 mantra::iless<std::string> > bans_t;
	typedef std::map<boost::regex, std::pair<boost::posix_time::ptime, unsigned int>,
					 rx_iless> rxbans_t;
	typedef std::set<std::string, mantra::iless<std::string> > exempts_t;
	typedef std::set<boost::regex, rx_iless> rxexempts_t;
	typedef std::map<boost::shared_ptr<LiveUser>, boost::shared_ptr<PendingModes> > pending_modes_t; 
	typedef std::map<boost::shared_ptr<LiveUser>, unsigned int> recent_parts_t;

private:

	boost::weak_ptr<LiveChannel> self;

	std::string name_;
	std::string id_;
	boost::posix_time::ptime created_;
	boost::posix_time::ptime seen_;

	boost::shared_ptr<StoredChannel> RWSYNC(stored_);

	users_t RWSYNC(users_);
	users_t splits_;

	bans_t RWSYNC(bans_);
	rxbans_t rxbans_;
	exempts_t RWSYNC(exempts_);
	rxexempts_t rxexempts_;

	std::string SYNC(topic_);
	std::string topic_setter_;
	boost::posix_time::ptime topic_set_time_;

	std::set<char> SYNC(modes_);
	std::string modes_key_;
	unsigned int modes_limit_;

	pending_modes_t SYNC(pending_modes_);
	recent_parts_t SYNC(recent_parts_);

	void CommonPart(const boost::shared_ptr<LiveUser> &user);

	// use if_LiveChannel_StoredChannel
	void Stored(const boost::shared_ptr<StoredChannel> &stored);

	// use if_LiveChannel_LiveUser
	void Quit(const boost::shared_ptr<LiveUser> &user);

	LiveChannel(const std::string &name,
				const boost::posix_time::ptime &created,
				const std::string &id);
public:
	static boost::shared_ptr<LiveChannel> create(const std::string &name,
			const boost::posix_time::ptime &created = mantra::GetCurrentDateTime(),
			const std::string &id = std::string());

	const std::string &Name() const { return name_; }
	const std::string &ID() const { return id_; }
	const boost::posix_time::ptime &Created() const { return created_; }
	const boost::posix_time::ptime &Seen() const { return seen_; }

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
	inline bool operator<(const LiveChannel &rhs) const { return *this < rhs.Name(); }
	inline bool operator==(const LiveChannel &rhs) const { return *this == rhs.Name(); }

	void Join(const boost::shared_ptr<LiveUser> &user);
	void Part(const boost::shared_ptr<LiveUser> &user,
			  const std::string &reason = std::string());
	void Kick(const boost::shared_ptr<LiveUser> &user,
			  const boost::shared_ptr<LiveUser> &kicker, const std::string &reason);

	bool RecentPart(const boost::shared_ptr<LiveUser> &user) const;

	// Its more efficient to populate in this instance.
	void Users(users_t &users) const;
	bool IsUser(const boost::shared_ptr<LiveUser> &user) const;
	std::set<char> User(const boost::shared_ptr<LiveUser> &user) const;
	bool User(const boost::shared_ptr<LiveUser> &user, char c) const
	{
		std::set<char> modes = User(user);
		return (modes.find(c) != modes.end());
	}
	void Splits(users_t &splitusers) const;
	bool IsSplit(const boost::shared_ptr<LiveUser> &user) const;
	std::set<char> Split(const boost::shared_ptr<LiveUser> &user) const;
	bool Split(const boost::shared_ptr<LiveUser> &user, char c) const
	{
		std::set<char> modes = Split(user);
		return (modes.find(c) != modes.end());
	}

	void Bans(bans_t &bans) const;
	void Bans(rxbans_t &bans) const;
	bool MatchBan(const std::string &in) const;
	bool MatchBan(const boost::shared_ptr<LiveUser> &in) const;
	void MatchBan(const std::string &in, bans_t &bans) const;
	void MatchBan(const boost::shared_ptr<LiveUser> &in, bans_t &bans) const;
	void MatchBan(const std::string &in, rxbans_t &bans) const;
	void MatchBan(const boost::shared_ptr<LiveUser> &in, rxbans_t &bans) const;
	void Exempts(exempts_t &exempts) const;
	void Exempts(rxexempts_t &exempts) const;
	bool MatchExempt(const std::string &in) const;
	bool MatchExempt(const boost::shared_ptr<LiveUser> &in) const;
	void MatchExempt(const std::string &in, exempts_t &bans) const;
	void MatchExempt(const boost::shared_ptr<LiveUser> &in, exempts_t &bans) const;
	void MatchExempt(const std::string &in, rxexempts_t &bans) const;
	void MatchExempt(const boost::shared_ptr<LiveUser> &in, rxexempts_t &bans) const;

	bool IsBanned(const std::string &in) const
		{ return (MatchBan(in) && !MatchExempt(in)); }
	bool IsBanned(const boost::shared_ptr<LiveUser> &in) const
		{ return (MatchBan(in) && !MatchExempt(in)); }

	void Topic(const std::string &topic, const std::string &setter, 
			   const boost::posix_time::ptime &set_time);
	std::string Topic() const;
	std::string Topic_Setter() const;
	boost::posix_time::ptime Topic_Set_Time() const;

	void Modes(const boost::shared_ptr<LiveUser> &user,
			   const std::string &in, const std::string &params = std::string());
	void Modes(const boost::shared_ptr<LiveUser> &user,
			   const std::string &in, const std::vector<std::string> &params);
	void SendModes(const ServiceUser *service,
				   const std::string &in, const std::string &params = std::string());
	void SendModes(const ServiceUser *service,
				   const std::string &in, const std::vector<std::string> &params);
	std::set<char> Modes() const;
	bool Mode(char c) const
	{
		std::set<char> modes = Modes();
		return (modes.find(c) != modes.end());
	}
	std::string Modes_Key() const;
	unsigned int Modes_Limit() const;

	void PRIVMSG(const ServiceUser *source,
				 const std::set<char> &modes,
				 const boost::format &message) const;
	void PRIVMSG(const std::set<char> &modes,
				 const boost::format &message);

	void NOTICE(const ServiceUser *source,
				const std::set<char> &modes,
				const boost::format &message) const;
	void NOTICE(const std::set<char> &modes,
				const boost::format &message);

/*
	void SEND(const ServiceUser *source,
			  const std::set<char> &modes,
			  const boost::format &message) const;
	void SEND(const std::set<char> &modes,
			  const boost::format &message);
*/
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

template<typename T>
inline bool operator<(const boost::shared_ptr<LiveChannel> &lhs, const T &rhs)
{
	return (*lhs < rhs);
}

inline bool operator<(const boost::shared_ptr<LiveChannel> &lhs,
					  const boost::shared_ptr<LiveChannel> &rhs)
{
	return (*lhs < *rhs);
}

#endif // _MAGICK_LIVECHANNEL_H
