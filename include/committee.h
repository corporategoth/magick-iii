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
#ifndef _MAGICK_COMMITTEE_H
#define _MAGICK_COMMITTEE_H 1

#ifdef RCSID
RCSID(magick__committee_h, "@(#) $Id$");
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
class StoredUser;

class Committee : private boost::noncopyable,
				  public boost::totally_ordered1<Committee>,
				  public boost::totally_ordered2<Committee, boost::uint32_t>,
				  public boost::totally_ordered2<Committee, std::string>
{
	friend class if_Committee_LiveUser;
	friend class if_Committee_Storage;

	typedef std::set<boost::shared_ptr<LiveUser> > online_members_t;

	boost::weak_ptr<Committee> self;
	static StorageInterface storage;
	mutable CachedRecord cache;

	boost::uint32_t id_;
	std::string name_;

	NSYNC(message_number);
	NSYNC(member_number);

	online_members_t RWSYNC(online_members_);

	// use if_Committee_LiveUser
	void Online(const boost::shared_ptr<LiveUser> &in);
	void Offline(const boost::shared_ptr<LiveUser> &in);

	// use if_Committee_Storage
	static boost::shared_ptr<Committee> load(boost::uint32_t id, const std::string &name);
	void DropInternal();

	Committee(boost::uint32_t id, const std::string &name);

public:
	static boost::shared_ptr<Committee> create(const std::string &name);
	static boost::shared_ptr<Committee> create(const std::string &name,
			const boost::shared_ptr<Committee> &head);
	static boost::shared_ptr<Committee> create(const std::string &name,
			const boost::shared_ptr<StoredUser> &head);

	inline boost::uint32_t ID() const { return id_; }
	const std::string &Name() const { return name_; }

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
	inline bool operator<(const Committee &rhs) const { return *this < rhs.ID(); }
	inline bool operator==(const Committee &rhs) const { return *this == rhs.ID(); }

	void Head(const boost::shared_ptr<StoredUser> &in);
	void Head(const boost::shared_ptr<Committee> &in);
	boost::shared_ptr<StoredUser> HeadUser() const;
	boost::shared_ptr<Committee> HeadCommittee() const;

	static std::set<boost::shared_ptr<Committee> > FindCommittees(const boost::shared_ptr<StoredUser> &in);

	boost::posix_time::ptime Registered() const
		{ return boost::get<boost::posix_time::ptime>(cache.Get("registered")); }
	boost::posix_time::ptime Last_Update() const
		{ return boost::get<boost::posix_time::ptime>(cache.Get("last_update")); }

	void Description(const std::string &in);
	std::string Description() const;
	void Email(const std::string &in);
	std::string Email() const;
	void Website(const std::string &in);
	std::string Website() const;
	void Comment(const std::string &in);
	std::string Comment() const;

	bool Private(const boost::logic::tribool &in);
	bool Private() const;
	bool OpenMemos(const boost::logic::tribool &in);
	bool OpenMemos() const;
	bool Secure(const boost::logic::tribool &in);
	bool Secure() const;

	bool LOCK_Private(const bool &in);
	bool LOCK_Private() const;
	bool LOCK_OpenMemos(const bool &in);
	bool LOCK_OpenMemos() const;
	bool LOCK_Secure(const bool &in);
	bool LOCK_Secure() const;

	class Member : public boost::totally_ordered1<Member>
	{
		friend class Committee;
		static StorageInterface storage;
		mutable CachedRecord cache;

		boost::shared_ptr<Committee> owner_;
		boost::uint32_t number_;
		boost::shared_ptr<StoredUser> entry_;

		Member();
		Member(const boost::shared_ptr<Committee> &owner,
			   boost::uint32_t number);
		Member(const boost::shared_ptr<Committee> &owner,
			   const boost::shared_ptr<StoredUser> &entry);
	public:
		const boost::shared_ptr<Committee> &Owner() const { return owner_; }
		boost::uint32_t Number() const { return number_; }
		boost::shared_ptr<StoredUser> Entry() const { return entry_; }

		std::string Last_UpdaterName() const;
		boost::shared_ptr<StoredUser> Last_Updater() const;
		boost::posix_time::ptime Last_Update() const;

		bool operator<(const Member &rhs) const
		{
			return (Owner() < rhs.Owner() || 
					(Owner() == rhs.Owner() && Entry() < rhs.Entry()));
		}
		bool operator==(const Member &rhs) const
		{
			return (Owner() == rhs.Owner() && Entry() == rhs.Entry());
		}
	};

	bool MEMBER_Exists(boost::uint32_t num) const;
	bool MEMBER_Exists(const boost::shared_ptr<StoredUser> &entry) const;
	Member MEMBER_Add(const boost::shared_ptr<StoredUser> &entry,
					  const boost::shared_ptr<StoredNick> &updater);
	void MEMBER_Del(boost::uint32_t num);
	void MEMBER_Del(const boost::shared_ptr<StoredUser> &entry);
	Member MEMBER_Get(boost::uint32_t num) const;
	Member MEMBER_Get(const boost::shared_ptr<StoredUser> &entry) const;
	void MEMBER_Get(std::set<Member> &fill) const;

	class Message : public boost::totally_ordered1<Memo>
	{
		friend class Committee;
		static StorageInterface storage;
		mutable CachedRecord cache;

		boost::shared_ptr<Committee> owner_;
		boost::uint32_t number_;

		Message();
		Message(const boost::shared_ptr<Committee> &owner,
				boost::uint32_t number);
	public:
		const boost::shared_ptr<Committee> &Owner() const { return owner_; }
		boost::uint32_t Number() const { return number_; }

		void Change(const std::string &entry,
					const boost::shared_ptr<StoredNick> &updater);
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

	// Is this a committee we can't delete.
	bool IsSpecial() const;

	// Is this a committee conveying special privileges.
	bool IsPrivileged() const;

	// Recursive functions!

	// These check 'current' status (ie. identified & secure).
	bool IsMember(const boost::shared_ptr<LiveUser> &user) const;
	bool IsHead(const boost::shared_ptr<LiveUser> &user) const;
	// These check 'potential' status (ie. IF identified ...).
	bool IsMember(const boost::shared_ptr<StoredUser> &user) const;
	bool IsHead(const boost::shared_ptr<StoredUser> &user) const;

	void Drop();

	void SendInfo(const ServiceUser *service,
				  const boost::shared_ptr<LiveUser> &user) const;
};

// Special interface used by LiveUser.
class if_Committee_LiveUser
{
	friend class LiveUser;
	Committee &base;

	// This is INTENTIONALLY private ...
	if_Committee_LiveUser(Committee &b) : base(b) {}
	if_Committee_LiveUser(const boost::shared_ptr<Committee> &b) : base(*(b.get())) {}

	inline void Online(const boost::shared_ptr<LiveUser> &user)
		{ base.Online(user); }
	inline void Offline(const boost::shared_ptr<LiveUser> &user)
		{ base.Offline(user); }
};

// Special interface used by Storage.
class if_Committee_Storage
{
	friend class Storage;
	Committee &base;

	// This is INTENTIONALLY private ...
	if_Committee_Storage(Committee &b) : base(b) {}
	if_Committee_Storage(const boost::shared_ptr<Committee> &b) : base(*(b.get())) {}

	static inline boost::shared_ptr<Committee> load(boost::uint32_t id, const std::string &name)
		{ return Committee::load(id, name); }
	inline void DropInternal()
		{ base.DropInternal(); }
};

// Used for tracing mainly.
inline std::ostream &operator<<(std::ostream &os, const Committee &in)
{
	return (os << in.Name());
}

template<typename T>
inline bool operator<(const boost::shared_ptr<Committee> &lhs, const T &rhs)
{
	if (!lhs)
		return true;
	else
		return (*lhs < rhs);
}

inline bool operator<(const boost::shared_ptr<Committee> &lhs,
					  const boost::shared_ptr<Committee> &rhs)
{
	if (!lhs)
		return true;
	else if (!rhs)
		return false;
	else
		return (*lhs < *rhs);
}

#endif // _MAGICK_COMMITTEE_H
