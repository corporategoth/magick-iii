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
#ifndef _MAGICK_STOREDCHANNEL_H
#define _MAGICK_STOREDCHANNEL_H 1

#ifdef RCSID
RCSID(magick__storedchannel_h, "@(#) $Id$");
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
#include "memo.h"

#include <boost/thread/mutex.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>
#include <boost/noncopyable.hpp>
#include <boost/operators.hpp>

class StoredChannel : private boost::noncopyable, public boost::totally_ordered1<StoredChannel>
{
	friend class if_StoredChannel_LiveUser;
	friend class if_StoredChannel_LiveChannel;

	boost::weak_ptr<StoredChannel> self;
	static StorageInterface storage;

	std::string name_;

	boost::mutex lock_;
	boost::shared_ptr<LiveChannel> live_;
	std::set<boost::shared_ptr<LiveUser> > identified_users_;

	// use if_StoredChannel_LiveUser
	void Identify(const boost::shared_ptr<LiveUser> &user);
	void UnIdentify(const boost::shared_ptr<LiveUser> &user);

	// use if_StoredChannel_LiveChannel
	void Topic(const std::string &topic, const std::string &setter,
			   const boost::posix_time::ptime &set_time);
	void Join(const boost::shared_ptr<LiveUser> &user);
	void Part(const boost::shared_ptr<LiveUser> &user);
	void Kick(const boost::shared_ptr<LiveUser> &user,
			  const boost::shared_ptr<LiveUser> &kicker);

	StoredChannel(const std::string &name, const std::string &password,
				  const boost::shared_ptr<StoredUser> &founder);
public:
	enum Revenge_t {
			R_None,
			R_Reverse,
			R_Mirror,
			R_DeOp,
			R_Kick,
			R_Ban1, // nick!*@*
			R_Ban2, // *!user@port.host
			R_Ban3, // *!user@*.host
			R_Ban4, // *!*@port.host
			R_Ban5  // *!*@*.host
		};

	static inline boost::shared_ptr<StoredChannel> create(const std::string &name,
		  const std::string &password, const boost::shared_ptr<StoredUser> &founder)
	{
		boost::shared_ptr<StoredChannel> rv(new StoredChannel(name, password, founder));
		rv->self = rv;
		return rv;
	}

	const std::string &Name() const { return name_; }
	const boost::shared_ptr<LiveChannel> &Live() const;

	boost::posix_time::ptime Registered() const
		{ return boost::get<boost::posix_time::ptime>(storage.GetField(name_, "registered")); }
	boost::posix_time::ptime Last_Update() const
		{ return boost::get<boost::posix_time::ptime>(storage.GetField(name_, "last_update")); }
	boost::posix_time::ptime Last_Used() const
		{ return boost::get<boost::posix_time::ptime>(storage.GetField(name_, "last_used")); }
	
	void Password(const std::string &password);
	bool CheckPassword(const std::string &password) const;

	void Founder(const boost::shared_ptr<StoredUser> &in);
	boost::shared_ptr<StoredUser> Founder() const;
	void Successor(const boost::shared_ptr<StoredUser> &in);
	boost::shared_ptr<StoredUser> Successor() const;

	void Description(const std::string &in);
	std::string Description() const;
	void Email(const std::string &in);
	std::string Email() const;
	void Website(const std::string &in);
	std::string Website() const;
	void Comment(const std::string &in);
	std::string Comment() const;

	std::string Topic() const;
	std::string Topic_Setter() const;
	boost::posix_time::ptime Topic_Set_Time() const;

	void KeepTopic(const boost::logic::tribool &in);
	bool KeepTopic() const;
	void TopicLock(const boost::logic::tribool &in);
	bool TopicLock() const;
	void Private(const boost::logic::tribool &in);
	bool Private() const;
	void SecureOps(const boost::logic::tribool &in);
	bool SecureOps() const;
	void Secure(const boost::logic::tribool &in);
	bool Secure() const;
	void Anarchy(const boost::logic::tribool &in);
	bool Anarchy() const;
	void KickOnBan(const boost::logic::tribool &in);
	bool KickOnBan() const;
	void Restricted(const boost::logic::tribool &in);
	bool Restricted() const;
	void CJoin(const boost::logic::tribool &in);
	bool CJoin() const;
	void NoExpire(const bool &in);
	bool NoExpire() const;
	void BanTime(const mantra::duration &in);
	mantra::duration BanTime() const;
	void PartTime(const mantra::duration &in);
	mantra::duration PartTime() const;
	void Revenge(Revenge_t in);
	Revenge_t Revenge() const;

	void ModeLock(const std::string &in);
	std::string ModeLock_On() const;
	std::string ModeLock_Off() const;
	std::string ModeLock_Key() const;
	boost::uint32_t ModeLock_Limit() const;
	
	void LOCK_KeepTopic(const bool &in);
	bool LOCK_KeepTopic() const;
	void LOCK_TopicLock(const bool &in);
	bool LOCK_TopicLock() const;
	void LOCK_Private(const bool &in);
	bool LOCK_Private() const;
	void LOCK_SecureOps(const bool &in);
	bool LOCK_SecureOps() const;
	void LOCK_Secure(const bool &in);
	bool LOCK_Secure() const;
	void LOCK_Anarchy(const bool &in);
	bool LOCK_Anarchy() const;
	void LOCK_KickOnBan(const bool &in);
	bool LOCK_KickOnBan() const;
	void LOCK_Restricted(const bool &in);
	bool LOCK_Restricted() const;
	void LOCK_CJoin(const bool &in);
	bool LOCK_CJoin() const;
	void LOCK_BanTime(const bool &in);
	bool LOCK_BanTime() const;
	void LOCK_PartTime(const bool &in);
	bool LOCK_PartTime() const;

	void LOCK_ModeLock(const std::string &in);
	std::string LOCK_ModeLock_On() const;
	std::string LOCK_ModeLock_Off() const;

	void Suspend(const boost::shared_ptr<LiveUser> &user,
				 const std::string &reason);
	void Unsuspend();
	std::string Suspended_By() const;
	boost::shared_ptr<StoredUser> Suspended_ByShared() const;
	std::string Suspend_Reason() const;
	boost::posix_time::ptime Suspend_Time() const;

	class Level : public boost::totally_ordered1<Level>
	{
		friend class StoredChannel;
		static StorageInterface storage;

		boost::shared_ptr<StoredChannel> owner_;
		boost::uint32_t id_;

		Level(const boost::shared_ptr<StoredChannel> &owner, boost::uint32_t id);
	public:
		const boost::shared_ptr<StoredChannel> &Owner() const { return owner_; }
		boost::uint32_t ID() const { return id_; }

		void Change(boost::int32_t value, const boost::shared_ptr<StoredNick> &updater);
		boost::int32_t Value() const;
		std::string Last_UpdaterName() const;
		boost::shared_ptr<StoredUser> Last_Updater() const;
		boost::posix_time::ptime Last_Update() const;

		bool operator<(const Level &rhs) const
		{
			return (Owner() < rhs.Owner() || 
					(Owner() == rhs.Owner() && ID() < rhs.ID()));
		}
		bool operator==(const Level &rhs) const
		{
			return (Owner() == rhs.Owner() && ID() == rhs.ID());
		}
	};

	bool LEVEL_Exists(boost::uint32_t id);
	void LEVEL_Del(boost::uint32_t id);
	// Will insert on write.
	Level LEVEL_Get(boost::uint32_t id);
	void LEVEL_Get(std::set<Level> &fill);

	class Access : public boost::totally_ordered1<Access>
	{
		friend class StoredChannel;
		static StorageInterface storage;

		boost::shared_ptr<StoredChannel> owner_;
		boost::uint32_t number_;

		Access(const boost::shared_ptr<StoredChannel> &owner, boost::uint32_t num);
		Access(const boost::shared_ptr<StoredChannel> &owner, const std::string &entry,
			   boost::int32_t level, const boost::shared_ptr<StoredNick> &updater);
		Access(const boost::shared_ptr<StoredChannel> &owner,
			   const boost::shared_ptr<StoredUser> &entry, boost::int32_t level,
			   const boost::shared_ptr<StoredNick> &updater);
		Access(const boost::shared_ptr<StoredChannel> &owner,
			   const boost::shared_ptr<Committee> &entry, boost::int32_t level,
			   const boost::shared_ptr<StoredNick> &updater);
	public:
		const boost::shared_ptr<StoredChannel> &Owner() const { return owner_; }
		boost::uint32_t Number() const { return number_; }

		void ChangeEntry(const std::string &entry, const boost::shared_ptr<StoredNick> &updater);
		void ChangeEntry(const boost::shared_ptr<StoredUser> &entry, const boost::shared_ptr<StoredNick> &updater);
		void ChangeEntry(const boost::shared_ptr<Committee> &entry, const boost::shared_ptr<StoredNick> &updater);
		void ChangeValue(boost::int32_t value, const boost::shared_ptr<StoredNick> &updater);
		std::string Mask() const;
		boost::shared_ptr<StoredUser> User() const;
		boost::shared_ptr<Committee> GetCommittee() const;
		boost::int32_t Level() const;
		std::string Last_UpdaterName() const;
		boost::shared_ptr<StoredUser> Last_Updater() const;
		boost::posix_time::ptime Last_Update() const;

		bool operator<(const Access &rhs) const
		{
			return (Owner() < rhs.Owner() || 
					(Owner() == rhs.Owner() && Number() < rhs.Number()));
		}
		bool operator==(const Access &rhs) const
		{
			return (Owner() == rhs.Owner() && Number() == rhs.Number());
		}
	};

	Access ACCESS_Matched(const std::string &in) const;
	Access ACCESS_Matched(const boost::shared_ptr<LiveUser> &in) const;
	bool ACCESS_Exists(boost::uint32_t num) const;
	Access ACCESS_Add(const std::string &entry, boost::uint32_t value,
					  const boost::shared_ptr<StoredNick> &updater);
	Access ACCESS_Add(const boost::shared_ptr<StoredUser> &entry, boost::uint32_t value,
					  const boost::shared_ptr<StoredNick> &updater);
	Access ACCESS_Add(const boost::shared_ptr<Committee> &entry, boost::uint32_t value,
					  const boost::shared_ptr<StoredNick> &updater);
	void ACCESS_Del(boost::uint32_t num);
	Access ACCESS_Get(boost::uint32_t num) const;
	void ACCESS_Get(std::set<Access> &fill) const;

	class AutoKick : public boost::totally_ordered1<AutoKick>
	{
		friend class StoredChannel;
		static StorageInterface storage;

		boost::shared_ptr<StoredChannel> owner_;
		boost::uint32_t number_;

		AutoKick(const boost::shared_ptr<StoredChannel> &owner, boost::uint32_t num);
		AutoKick(const boost::shared_ptr<StoredChannel> &owner, const std::string &entry,
				 const std::string &reason, const boost::shared_ptr<StoredNick> &updater);
		AutoKick(const boost::shared_ptr<StoredChannel> &owner,
				 const boost::shared_ptr<StoredUser> &entry,
				 const std::string &reason, const boost::shared_ptr<StoredNick> &updater);
		AutoKick(const boost::shared_ptr<StoredChannel> &owner,
				 const boost::shared_ptr<Committee> &entry,
				 const std::string &reason, const boost::shared_ptr<StoredNick> &updater);
	public:
		const boost::shared_ptr<StoredChannel> &Owner() const { return owner_; }
		boost::uint32_t Number() const { return number_; }

		void ChangeEntry(const std::string &entry, const boost::shared_ptr<StoredNick> &updater);
		void ChangeEntry(const boost::shared_ptr<StoredUser> &entry, const boost::shared_ptr<StoredNick> &updater);
		void ChangeEntry(const boost::shared_ptr<Committee> &entry, const boost::shared_ptr<StoredNick> &updater);
		void ChangeReason(const std::string &value, const boost::shared_ptr<StoredNick> &updater);
		std::string Mask() const;
		boost::shared_ptr<StoredUser> User() const;
		boost::shared_ptr<Committee> GetCommittee() const;
		std::string Reason() const;
		std::string Last_UpdaterName() const;
		boost::shared_ptr<StoredUser> Last_Updater() const;
		boost::posix_time::ptime Last_Update() const;

		bool operator<(const AutoKick &rhs) const
		{
			return (Owner() < rhs.Owner() || 
					(Owner() == rhs.Owner() && Number() < rhs.Number()));
		}
		bool operator==(const AutoKick &rhs) const
		{
			return (Owner() == rhs.Owner() && Number() == rhs.Number());
		}
	};

	AutoKick AKICK_Matched(const std::string &in) const;
	AutoKick AKICK_Matched(const boost::shared_ptr<LiveUser> &in) const;
	bool AKICK_Exists(boost::uint32_t num) const;
	AutoKick AKICK_Add(const std::string &entry, const std::string &reason,
					  const boost::shared_ptr<StoredNick> &updater);
	AutoKick AKICK_Add(const boost::shared_ptr<StoredUser> &entry, const std::string &reason,
					  const boost::shared_ptr<StoredNick> &updater);
	AutoKick AKICK_Add(const boost::shared_ptr<Committee> &entry, const std::string &reason,
					  const boost::shared_ptr<StoredNick> &updater);
	void AKICK_Del(boost::uint32_t num);
	AutoKick AKICK_Get(boost::uint32_t num) const;
	void AKICK_Get(std::set<AutoKick> &fill) const;

	class Greet : public boost::totally_ordered1<Greet>
	{
		friend class StoredChannel;
		static StorageInterface storage;

		boost::shared_ptr<StoredChannel> owner_;
		boost::shared_ptr<StoredUser> entry_;

		Greet(const boost::shared_ptr<StoredChannel> &owner, const boost::shared_ptr<StoredUser> &entry);
		Greet(const boost::shared_ptr<StoredChannel> &owner, const boost::shared_ptr<StoredUser> &entry,
			  const std::string &greeting, const boost::shared_ptr<StoredNick> &updater);
	public:
		const boost::shared_ptr<StoredChannel> &Owner() const { return owner_; }
		const boost::shared_ptr<StoredUser> &Entry() const { return entry_; }

		void Change(const std::string &greeting, const boost::shared_ptr<StoredNick> &updater);
		std::string Greeting() const;
		void Locked(bool in);
		bool Locked() const;
		std::string Last_UpdaterName() const;
		boost::shared_ptr<StoredUser> Last_Updater() const;
		boost::posix_time::ptime Last_Update() const;

		bool operator<(const Greet &rhs) const
		{
			return (Owner() < rhs.Owner() || 
					(Owner() == rhs.Owner() && Entry() < rhs.Entry()));
		}
		bool operator==(const Greet &rhs) const
		{
			return (Owner() == rhs.Owner() && Entry() == rhs.Entry());
		}
	};

	bool GREET_Exists(const boost::shared_ptr<StoredUser> &entry) const;
	Greet GREET_Add(const boost::shared_ptr<StoredUser> &entry, const std::string &greeting,
					const boost::shared_ptr<StoredNick> &updater);
	void GREET_Del(const boost::shared_ptr<StoredUser> &entry);
	Greet GREET_Get(const boost::shared_ptr<StoredUser> &entry) const;
	void GREET_Get(std::set<Greet> &fill) const;

	class Message : public boost::totally_ordered1<Message>
	{
		friend class StoredChannel;
		static StorageInterface storage;

		boost::shared_ptr<StoredChannel> owner_;
		boost::uint32_t number_;

		Message(const boost::shared_ptr<StoredChannel> &owner, boost::uint32_t num);
		Message(const boost::shared_ptr<StoredChannel> &owner, const std::string &message,
				const boost::shared_ptr<StoredNick> &updater);
	public:
		const boost::shared_ptr<StoredChannel> &Owner() const { return owner_; }
		boost::uint32_t Number() const { return number_; }

		void Change(const std::string &entry, const boost::shared_ptr<StoredNick> &updater);
		std::string Entry() const;
		std::string Last_UpdaterName() const;
		boost::shared_ptr<StoredUser> Last_Updater() const;
		boost::posix_time::ptime Last_Update() const;

		bool operator<(const Message &rhs) const
		{
			return (Owner() < rhs.Owner() || 
					(Owner() == rhs.Owner() && Number() < rhs.Number()));
		}
		bool operator==(const Message &rhs) const
		{
			return (Owner() == rhs.Owner() && Number() == rhs.Number());
		}
	};

	bool MESSAGE_Exists(boost::uint32_t num) const;
	Message MESSAGE_Add(const std::string &message, 
					  const boost::shared_ptr<StoredNick> &updater);
	void MESSAGE_Del(boost::uint32_t num);
	Message MESSAGE_Get(boost::uint32_t num) const;
	void MESSAGE_Get(std::set<Message> &fill) const;

	bool NEWS_Exists(boost::uint32_t num) const;
	void NEWS_Del(boost::uint32_t num);
	News NEWS_Get(boost::uint32_t num) const;
	void NEWS_Get(std::set<News> &fill) const;
};

// Special interface used by LiveUser.
class if_StoredChannel_LiveUser
{
	friend class LiveUser;
	StoredChannel &base;

	// This is INTENTIONALLY private ...
	if_StoredChannel_LiveUser(StoredChannel &b) : base(b) {}
	if_StoredChannel_LiveUser(const boost::shared_ptr<StoredChannel> &b) : base(*(b.get())) {}

	inline void Identify(const boost::shared_ptr<LiveUser> &user)
		{ base.Identify(user); }
	inline void UnIdentify(const boost::shared_ptr<LiveUser> &user)
		{ base.UnIdentify(user); }
};

// Special interface used by LiveChannel.
class if_StoredChannel_LiveChannel
{
	friend class LiveChannel;
	StoredChannel &base;

	// This is INTENTIONALLY private ...
	if_StoredChannel_LiveChannel(StoredChannel &b) : base(b) {}
	if_StoredChannel_LiveChannel(const boost::shared_ptr<StoredChannel> &b) : base(*(b.get())) {}

	inline void Topic(const std::string &topic, const std::string &setter,
			   		  const boost::posix_time::ptime &set_time)
		{ base.Topic(topic, setter, set_time); }
	inline void Join(const boost::shared_ptr<LiveUser> &user)
		{ base.Join(user); }
	inline void Part(const boost::shared_ptr<LiveUser> &user)
		{ base.Part(user); }
	inline void Kick(const boost::shared_ptr<LiveUser> &user,
					 const boost::shared_ptr<LiveUser> &kicker)
		{ base.Kick(user, kicker); }
};

// Used for tracing mainly.
inline std::ostream &operator<<(std::ostream &os, const StoredChannel &in)
{
	return (os << in.Name());
}

#endif // _MAGICK_STOREDCHANNEL_H
