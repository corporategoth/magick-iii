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

#include "storage.h"
#include "memo.h"

#include <boost/thread/mutex.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>
#include <boost/noncopyable.hpp>
#include <boost/operators.hpp>

class LiveUser;
class StoredUser;

class Committee : private boost::noncopyable, public boost::totally_ordered1<Committee>
{
	friend class if_Committee_LiveUser;

	boost::weak_ptr<Committee> self;
	static StorageInterface storage;

	std::string name_;

	boost::mutex lock_;
	std::set<boost::shared_ptr<LiveUser> > online_members_;

	// use if_Committee_LiveUser
	void Online(const boost::shared_ptr<LiveUser> &in);
	void Offline(const boost::shared_ptr<LiveUser> &in);

	Committee(const std::string &name);
	Committee(const std::string &name, const boost::shared_ptr<Committee> &head);
	Committee(const std::string &name, const boost::shared_ptr<StoredUser> &head);

public:
	static inline boost::shared_ptr<Committee> create(const std::string &name)
	{
		boost::shared_ptr<Committee> rv(new Committee(name));
		rv->self = rv;
		return rv;
	}

	static inline boost::shared_ptr<Committee> create(const std::string &name,
			const boost::shared_ptr<Committee> &head)
	{
		boost::shared_ptr<Committee> rv(new Committee(name, head));
		rv->self = rv;
		return rv;
	}

	static inline boost::shared_ptr<Committee> create(const std::string &name,
			const boost::shared_ptr<StoredUser> &head)
	{
		boost::shared_ptr<Committee> rv(new Committee(name, head));
		rv->self = rv;
		return rv;
	}

	const std::string &Name() const { return name_; }

	bool operator<(const Committee &rhs) const { return Name() < rhs.Name(); }
	bool operator==(const Committee &rhs) const { return Name() == rhs.Name(); }

	void Head(const boost::shared_ptr<StoredUser> &in);
	void Head(const boost::shared_ptr<Committee> &in);
	boost::shared_ptr<StoredUser> &HeadUser() const;
	boost::shared_ptr<Committee> &HeadCommittee() const;

	boost::posix_time::ptime Registered() const;
	boost::posix_time::ptime Last_Update() const;

	void Description(const std::string &in);
	std::string Description() const;
	void Email(const std::string &in);
	std::string Email() const;
	void Webpage(const std::string &in);
	std::string Webpage() const;

	void Private(const boost::logic::tribool &in);
	bool Private() const;
	void OpenMemos(const boost::logic::tribool &in);
	bool OpenMemos() const;
	void Secure(const boost::logic::tribool &in);
	bool Secure() const;

	void LOCK_Private(const bool &in);
	bool LOCK_Private() const;
	void LOCK_OpenMemos(const bool &in);
	bool LOCK_OpenMemos() const;
	void LOCK_Secure(const bool &in);
	bool LOCK_Secure() const;

	class Member : public boost::totally_ordered1<Member>
	{
		friend class Committee;
		static StorageInterface storage;

		boost::shared_ptr<Committee> owner_;
		boost::shared_ptr<StoredUser> entry_;

	public:
		const boost::shared_ptr<Committee> &Owner() const { return owner_; }
		const boost::shared_ptr<StoredUser> &Entry() const { return entry_; }

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

	bool MEMBER_Exists(const boost::shared_ptr<StoredUser> &entry) const;
	Member MEMBER_Add(const boost::shared_ptr<StoredUser> &entry,
					  const boost::shared_ptr<StoredNick> &updater);
	void MEMBER_Del(const boost::shared_ptr<StoredUser> &entry);
	Member MEMBER_Get(const boost::shared_ptr<StoredUser> &entry) const;
	void MEMBER_Fill(std::set<boost::shared_ptr<StoredUser> > &fill) const;

	class Message : public boost::totally_ordered1<Memo>
	{
		friend class Committee;
		static StorageInterface storage;

		boost::shared_ptr<Committee> owner_;
		boost::uint32_t number_;

		Message(const boost::shared_ptr<Committee> &owner, boost::uint32_t num);
		Message(const boost::shared_ptr<Committee> &owner, const std::string &message,
				const boost::shared_ptr<StoredNick> &updater);
	public:
		const boost::shared_ptr<Committee> &Owner() const { return owner_; }
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

	bool IsMember(const boost::shared_ptr<LiveUser> &user) const;
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

// Used for tracing mainly.
inline std::ostream &operator<<(std::ostream &os, const Committee &in)
{
	return (os << in.Name());
}

#endif // _MAGICK_COMMITTEE_H