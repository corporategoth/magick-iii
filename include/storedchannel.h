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
#include "storageinterface.h"
#include "memo.h"

#include <boost/thread/mutex.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>
#include <boost/noncopyable.hpp>
#include <boost/operators.hpp>

class LiveUser;
class LiveChannel;
class StoredUser;
class StoredNick;
class Committee;

class StoredChannel;
class StoredChannel : private boost::noncopyable,
					  public boost::totally_ordered1<StoredChannel>,
					  public boost::totally_ordered2<StoredChannel, boost::uint32_t>,
					  public boost::totally_ordered2<StoredChannel, std::string>
{
//	friend class StoredChannel::Level;
//	friend class StoredChannel::Access;
//	friend class StoredChannel::AutoKick;
//	friend class StoredChannel::Greet;
//	friend class StoredChannel::Message;
	friend class if_StoredChannel_LiveUser;
	friend class if_StoredChannel_LiveChannel;
	friend class if_StoredChannel_Storage;

	typedef std::set<boost::shared_ptr<LiveUser> > identified_users_t;

	boost::weak_ptr<StoredChannel> self;
	static StorageInterface storage;
	mutable CachedRecord cache;

	boost::uint32_t id_;
	std::string name_; // Too damn convenient :)

	boost::shared_ptr<LiveChannel> RWSYNC(live_);

	boost::mutex lock_;
	identified_users_t identified_users_;

	boost::mutex id_lock_access;
	boost::mutex id_lock_autokick;
	boost::mutex id_lock_message;

	enum RevengeType_t { RT_DeOp, RT_DeHalfOp, RT_DeVoice,
						 RT_Kick, RT_Ban };
	// Figure out if a 'wrong' has occured, and if so, take
	// revenge against target for wrongs done to victim.
	void DoRevenge(RevengeType_t action,
				   const boost::shared_ptr<LiveUser> &target,
				   const boost::shared_ptr<LiveUser> &victim) const;

	// use if_StoredChannel_LiveUser
	bool Identify(const boost::shared_ptr<LiveUser> &user,
				  const std::string &password);
	void UnIdentify(const boost::shared_ptr<LiveUser> &user);

	// use if_StoredChannel_LiveChannel
	void Topic(const std::string &topic, const std::string &setter,
			   const boost::posix_time::ptime &set_time);
	void Modes(const boost::shared_ptr<LiveUser> &user,
			   const std::string &in, const std::vector<std::string> &params);
	void Join(const boost::shared_ptr<LiveChannel> &channel,
			  const boost::shared_ptr<LiveUser> &user);
	void Part(const boost::shared_ptr<LiveUser> &user);
	void Kick(const boost::shared_ptr<LiveUser> &user,
			  const boost::shared_ptr<LiveUser> &kicker);

	// use if_StoredChannel_Storage
	void DropInternal();
	static boost::shared_ptr<StoredChannel> load(boost::uint32_t id, const std::string &name);
	static void expire();

	StoredChannel(const boost::uint32_t id, const std::string &name);
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
			R_Ban5, // *!*@*.host
			R_MAX
		};

	static Revenge_t RevengeID(const std::string &in);
	static std::string RevengeDesc(Revenge_t id);

	static boost::shared_ptr<StoredChannel> create(const std::string &name,
		  const std::string &password, const boost::shared_ptr<StoredUser> &founder);

	inline boost::uint32_t ID() const { return id_; }
	inline const std::string &Name() const { return name_; }
	const boost::shared_ptr<LiveChannel> &Live() const;

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
	inline bool operator<(boost::uint32_t rhs) const { return id_ < rhs; }
	inline bool operator==(boost::uint32_t rhs) const { return id_ == rhs; }
	inline bool operator<(const StoredChannel &rhs) const { return *this < rhs.ID(); }
	inline bool operator==(const StoredChannel &rhs) const { return *this == rhs.ID(); }

	boost::posix_time::ptime Registered() const
		{ return boost::get<boost::posix_time::ptime>(cache.Get("registered")); }
	boost::posix_time::ptime Last_Update() const
		{ return boost::get<boost::posix_time::ptime>(cache.Get("last_update")); }
	boost::posix_time::ptime Last_Used() const
		{ return boost::get<boost::posix_time::ptime>(cache.Get("last_used")); }
	
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

	void Topic(const boost::shared_ptr<LiveUser> &user,
			   const std::string &topic);
	std::string Topic() const;
	std::string Topic_Setter() const;
	boost::posix_time::ptime Topic_Set_Time() const;

	bool KeepTopic(const boost::logic::tribool &in);
	bool KeepTopic() const;
	bool TopicLock(const boost::logic::tribool &in);
	bool TopicLock() const;
	bool Private(const boost::logic::tribool &in);
	bool Private() const;
	bool SecureOps(const boost::logic::tribool &in);
	bool SecureOps() const;
	bool Secure(const boost::logic::tribool &in);
	bool Secure() const;
	bool Anarchy(const boost::logic::tribool &in);
	bool Anarchy() const;
	bool KickOnBan(const boost::logic::tribool &in);
	bool KickOnBan() const;
	bool Restricted(const boost::logic::tribool &in);
	bool Restricted() const;
	bool CJoin(const boost::logic::tribool &in);
	bool CJoin() const;
	bool NoExpire(const boost::logic::tribool &in);
	bool NoExpire() const;
	bool BanTime(const mantra::duration &in);
	mantra::duration BanTime() const;
	bool PartTime(const mantra::duration &in);
	mantra::duration PartTime() const;
	bool Revenge(Revenge_t in);
	Revenge_t Revenge() const;

	std::string ModeLock(const std::string &in);
	std::string ModeLock_On() const;
	std::string ModeLock_Off() const;
	std::string ModeLock_Key() const;
	boost::uint32_t ModeLock_Limit() const;
	
	bool LOCK_KeepTopic(const bool &in);
	bool LOCK_KeepTopic() const;
	bool LOCK_TopicLock(const bool &in);
	bool LOCK_TopicLock() const;
	bool LOCK_Private(const bool &in);
	bool LOCK_Private() const;
	bool LOCK_SecureOps(const bool &in);
	bool LOCK_SecureOps() const;
	bool LOCK_Secure(const bool &in);
	bool LOCK_Secure() const;
	bool LOCK_Anarchy(const bool &in);
	bool LOCK_Anarchy() const;
	bool LOCK_KickOnBan(const bool &in);
	bool LOCK_KickOnBan() const;
	bool LOCK_Restricted(const bool &in);
	bool LOCK_Restricted() const;
	bool LOCK_CJoin(const bool &in);
	bool LOCK_CJoin() const;
	bool LOCK_BanTime(const bool &in);
	bool LOCK_BanTime() const;
	bool LOCK_PartTime(const bool &in);
	bool LOCK_PartTime() const;
	bool LOCK_Revenge(const bool &in);
	bool LOCK_Revenge() const;

	std::string LOCK_ModeLock(const std::string &in);
	std::string LOCK_ModeLock_On() const;
	std::string LOCK_ModeLock_Off() const;

	void Suspend(const boost::shared_ptr<StoredNick> &user,
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
		mutable CachedRecord cache;

		struct DefaultLevel
		{
			boost::int32_t value;
			boost::regex name;
			std::string desc;
		};
		static boost::read_write_mutex DefaultLevelsLock;
		static std::map<boost::uint32_t, DefaultLevel> DefaultLevels;

		boost::shared_ptr<StoredChannel> owner_;
		boost::uint32_t id_;

		Level(const boost::shared_ptr<StoredChannel> &owner, boost::uint32_t id);
	public:
		enum Default_t {
				LVL_AutoDeop = 1, LVL_AutoVoice, LVL_AutoHalfOp, LVL_AutoOp,
				LVL_ReadMemo, LVL_WriteMemo, LVL_DelMemo, LVL_Greet,
				LVL_OverGreet, LVL_Message, LVL_AutoKick, LVL_Super, LVL_Unban,
				LVL_Access, LVL_Set, LVL_View, LVL_CMD_Invite, LVL_CMD_Unban,
				LVL_CMD_Voice, LVL_CMD_HalfOp, LVL_CMD_Op, LVL_CMD_Kick,
				LVL_CMD_Mode, LVL_CMD_Clear, LVL_MAX
			};

		static void Default(boost::uint32_t level, boost::int32_t value,
							const boost::regex &name, const std::string &desc);
		static boost::int32_t Default(boost::uint32_t level);
		static boost::uint32_t DefaultName(const std::string &name);
		static std::string DefaultDesc(boost::uint32_t level,
									   const boost::shared_ptr<LiveUser> &user = 
									   		boost::shared_ptr<LiveUser>());

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

	bool LEVEL_Exists(boost::uint32_t id) const;
	void LEVEL_Del(boost::uint32_t id);
	// Will insert on write.
	Level LEVEL_Get(boost::uint32_t id) const;
	void LEVEL_Get(std::set<Level> &fill) const;

	class Access : public boost::totally_ordered1<Access>
	{
		friend class StoredChannel;
		static StorageInterface storage;
		mutable CachedRecord cache;

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

	std::list<Access> ACCESS_Matches(const std::string &in) const;
	std::list<Access> ACCESS_Matches(const boost::shared_ptr<LiveUser> &in) const;
	std::list<Access> ACCESS_Matches(const boost::regex &in) const;
	boost::int32_t ACCESS_Max(const std::string &in) const;
	boost::int32_t ACCESS_Max(const boost::shared_ptr<LiveUser> &in) const;
	bool ACCESS_Matches(const std::string &in, boost::uint32_t level) const;
	bool ACCESS_Matches(const boost::shared_ptr<LiveUser> &in, boost::uint32_t level) const;
	bool ACCESS_Exists(boost::uint32_t num) const;
	Access ACCESS_Add(const std::string &entry, boost::uint32_t value,
					  const boost::shared_ptr<StoredNick> &updater);
	Access ACCESS_Add(const boost::shared_ptr<StoredUser> &entry, boost::uint32_t value,
					  const boost::shared_ptr<StoredNick> &updater);
	Access ACCESS_Add(const boost::shared_ptr<Committee> &entry, boost::uint32_t value,
					  const boost::shared_ptr<StoredNick> &updater);
	void ACCESS_Del(boost::uint32_t num);
	void ACCESS_Reindex(boost::uint32_t num, boost::uint32_t newnum);
	Access ACCESS_Get(boost::uint32_t num) const;
	Access ACCESS_Get(const boost::shared_ptr<StoredUser> &in) const;
	Access ACCESS_Get(const boost::shared_ptr<Committee> &in) const;
	Access ACCESS_Get(const std::string &in) const;
	void ACCESS_Get(std::set<Access> &fill) const;

	class AutoKick : public boost::totally_ordered1<AutoKick>
	{
		friend class StoredChannel;
		static StorageInterface storage;
		mutable CachedRecord cache;

		boost::shared_ptr<StoredChannel> owner_;
		boost::uint32_t number_;

		AutoKick(const boost::shared_ptr<StoredChannel> &owner, boost::uint32_t num);
		AutoKick(const boost::shared_ptr<StoredChannel> &owner, const std::string &entry,
				 const std::string &reason, const mantra::duration &length,
				 const boost::shared_ptr<StoredNick> &updater);
		AutoKick(const boost::shared_ptr<StoredChannel> &owner,
				 const boost::shared_ptr<StoredUser> &entry,
				 const std::string &reason, const mantra::duration &length,
				 const boost::shared_ptr<StoredNick> &updater);
		AutoKick(const boost::shared_ptr<StoredChannel> &owner,
				 const boost::shared_ptr<Committee> &entry,
				 const std::string &reason, const mantra::duration &length,
				 const boost::shared_ptr<StoredNick> &updater);
	public:
		const boost::shared_ptr<StoredChannel> &Owner() const { return owner_; }
		boost::uint32_t Number() const { return number_; }

		void ChangeEntry(const std::string &entry, const boost::shared_ptr<StoredNick> &updater);
		void ChangeEntry(const boost::shared_ptr<StoredUser> &entry, const boost::shared_ptr<StoredNick> &updater);
		void ChangeEntry(const boost::shared_ptr<Committee> &entry, const boost::shared_ptr<StoredNick> &updater);
		void ChangeReason(const std::string &value, const boost::shared_ptr<StoredNick> &updater);
		void ChangeLength(const mantra::duration &length, const boost::shared_ptr<StoredNick> &updater);
		std::string Mask() const;
		boost::shared_ptr<StoredUser> User() const;
		boost::shared_ptr<Committee> GetCommittee() const;
		std::string Reason() const;
		boost::posix_time::ptime Creation() const;
		mantra::duration Length() const;
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

	std::set<AutoKick> AKICK_Matches(const std::string &in) const;
	std::set<AutoKick> AKICK_Matches(const boost::shared_ptr<LiveUser> &in) const;
	std::set<AutoKick> AKICK_Matches(const boost::regex &in) const;
	bool AKICK_Exists(boost::uint32_t num) const;
	AutoKick AKICK_Add(const std::string &entry, const std::string &reason,
					   const mantra::duration &length,
					   const boost::shared_ptr<StoredNick> &updater);
	AutoKick AKICK_Add(const boost::shared_ptr<StoredUser> &entry,
					   const std::string &reason, const mantra::duration &length,
					   const boost::shared_ptr<StoredNick> &updater);
	AutoKick AKICK_Add(const boost::shared_ptr<Committee> &entry,
					   const std::string &reason, const mantra::duration &length,
					   const boost::shared_ptr<StoredNick> &updater);
	void AKICK_Del(boost::uint32_t num);
	void AKICK_Reindex(boost::uint32_t num, boost::uint32_t newnum);
	AutoKick AKICK_Get(boost::uint32_t num) const;
	AutoKick AKICK_Get(const boost::shared_ptr<StoredUser> &in) const;
	AutoKick AKICK_Get(const boost::shared_ptr<Committee> &in) const;
	AutoKick AKICK_Get(const std::string &in) const;
	void AKICK_Get(std::set<AutoKick> &fill) const;

	class Greet : public boost::totally_ordered1<Greet>
	{
		friend class StoredChannel;
		static StorageInterface storage;
		mutable CachedRecord cache;

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
		mutable CachedRecord cache;

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
	void MESSAGE_Reindex(boost::uint32_t num, boost::uint32_t newnum);
	Message MESSAGE_Get(boost::uint32_t num) const;
	void MESSAGE_Get(std::set<Message> &fill) const;

	bool NEWS_Exists(boost::uint32_t num) const;
	void NEWS_Del(boost::uint32_t num);
	News NEWS_Get(boost::uint32_t num) const;
	void NEWS_Get(std::set<News> &fill) const;

	void Drop();

	void SendInfo(const boost::shared_ptr<LiveUser> &service,
				  const boost::shared_ptr<LiveUser> &user) const;
	std::string FilterModes(const boost::shared_ptr<LiveUser> &user,
							const std::string &modes,
							std::vector<std::string> &params) const;
};

// Special interface used by LiveUser.
class if_StoredChannel_LiveUser
{
	friend class LiveUser;
	StoredChannel &base;

	// This is INTENTIONALLY private ...
	if_StoredChannel_LiveUser(StoredChannel &b) : base(b) {}
	if_StoredChannel_LiveUser(const boost::shared_ptr<StoredChannel> &b) : base(*(b.get())) {}

	inline bool Identify(const boost::shared_ptr<LiveUser> &user,
						 const std::string &password)
		{ return base.Identify(user, password); }
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
	inline void Modes(const boost::shared_ptr<LiveUser> &user,
					  const std::string &in, const std::vector<std::string> &params)
		{ base.Modes(user, in, params); }
	inline void Join(const boost::shared_ptr<LiveChannel> &live,
					 const boost::shared_ptr<LiveUser> &user)
		{ base.Join(live, user); }
	inline void Part(const boost::shared_ptr<LiveUser> &user)
		{ base.Part(user); }
	inline void Kick(const boost::shared_ptr<LiveUser> &user,
					 const boost::shared_ptr<LiveUser> &kicker)
		{ base.Kick(user, kicker); }
};

// Special interface used by Storage.
class if_StoredChannel_Storage
{
	friend class Storage;
	StoredChannel &base;

	// This is INTENTIONALLY private ...
	if_StoredChannel_Storage(StoredChannel &b) : base(b) {}
	if_StoredChannel_Storage(const boost::shared_ptr<StoredChannel> &b) : base(*(b.get())) {}

	static inline boost::shared_ptr<StoredChannel> load(boost::uint32_t id, const std::string &name)
		{ return StoredChannel::load(id, name); }
	static inline void expire()
		{ StoredChannel::expire(); }
	inline void DropInternal()
		{ base.DropInternal(); }
};

// Used for tracing mainly.
inline std::ostream &operator<<(std::ostream &os, const StoredChannel &in)
{
	return (os << in.Name());
}

template<typename T>
inline bool operator<(const boost::shared_ptr<StoredChannel> &lhs, const T &rhs)
{
	return (*lhs < rhs);
}

inline bool operator<(const boost::shared_ptr<StoredChannel> &lhs,
					  const boost::shared_ptr<StoredChannel> &rhs)
{
	return (*lhs < *rhs);
}

#endif // _MAGICK_STOREDCHANNEL_H
