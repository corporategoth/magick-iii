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
#ifndef _MAGICK_STOREDUSER_H
#define _MAGICK_STOREDUSER_H 1

#ifdef RCSID
RCSID(magick__storeduser_h, "@(#) $Id$");
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

#include <mantra/core/sync.h>

#include <boost/thread/mutex.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>
#include <boost/noncopyable.hpp>
#include <boost/operators.hpp>

class StoredUser : private boost::noncopyable,
				   public boost::totally_ordered1<StoredUser>,
				   public boost::totally_ordered2<StoredUser, boost::uint32_t>
{
	friend class if_StoredUser_LiveUser;
	friend class if_StoredUser_StoredNick;
	friend class if_StoredUser_StoredChannel;
	friend class if_StoredUser_Storage;

public:
	typedef std::set<boost::shared_ptr<LiveUser> > online_users_t;
	typedef std::set<boost::shared_ptr<StoredNick> > my_nicks_t;
	typedef std::set<boost::shared_ptr<StoredChannel> > my_channels_t;

private:
	boost::weak_ptr<StoredUser> self;
	static StorageInterface storage;
	static StorageInterface storage_access;
	static StorageInterface storage_ignore;

	boost::uint32_t id_;

	NSYNC(access_number);
	NSYNC(ignore_number);

	online_users_t RWSYNC(online_users_);
	my_nicks_t SYNC(my_nicks_);
	my_channels_t SYNC(my_channels_);

	mutable std::string RWSYNC(cached_language_);
	mutable boost::posix_time::ptime cached_language_update_;
	mutable bool RWSYNC(cached_privmsg_);
	mutable boost::posix_time::ptime cached_privmsg_update_;

	// use if_StoredUser_DCC
	void Picture(boost::uint32_t in, const std::string &ext);

	// use if_StoredUser_StoredNick
	static boost::shared_ptr<StoredUser> create(const std::string &password);
	void Online(const boost::shared_ptr<LiveUser> &user);
	void Offline(const boost::shared_ptr<LiveUser> &user);
	void Add(const boost::shared_ptr<StoredNick> &nick);
	void Del(const boost::shared_ptr<StoredNick> &nick);
	void SendInfo(const boost::shared_ptr<LiveUser> &service,
				  const boost::shared_ptr<LiveUser> &user) const;

	// use if_StoredUser_StoredChannel
	void Add(const boost::shared_ptr<StoredChannel> &channel);
	void Del(const boost::shared_ptr<StoredChannel> &channel);

	// use if_StoredUser_Storage
	static boost::shared_ptr<StoredUser> load(boost::uint32_t id);
	void DropInternal();

	StoredUser(boost::uint32_t id);
public:
	inline boost::uint32_t ID() const { return id_; }

	boost::posix_time::ptime Last_Update() const
		{ return boost::get<boost::posix_time::ptime>(storage.GetField(id_, "last_update")); }
	boost::shared_ptr<StoredNick> Last_Online() const;
	online_users_t Online() const;
	my_nicks_t Nicks() const;

	inline bool operator<(boost::uint32_t rhs) const { return id_ < rhs; }
	inline bool operator==(boost::uint32_t rhs) const { return id_ == rhs; }
	inline bool operator<(const StoredUser &rhs) const { return id_ < rhs.ID(); }
	inline bool operator==(const StoredUser &rhs) const { return id_ == rhs.ID(); }

	void Password(const std::string &password);
	bool CheckPassword(const std::string &password) const;

	void Email(const std::string &in);
	std::string Email() const;
	void Website(const std::string &in);
	std::string Website() const;
	void ICQ(const boost::uint32_t &in);
	boost::uint32_t ICQ() const;
	void AIM(const std::string &in);
	std::string AIM() const;
	void MSN(const std::string &in);
	std::string MSN() const;
	void Jabber(const std::string &in);
	std::string Jabber() const;
	void Yahoo(const std::string &in);
	std::string Yahoo() const;
	void Description(const std::string &in);
	std::string Description() const;
	void Comment(const std::string &in);
	std::string Comment() const;

	void Suspend(const boost::shared_ptr<StoredNick> &nick,
				 const std::string &reason);
	void Unsuspend();
	std::string Suspended_By() const;
	boost::shared_ptr<StoredUser> Suspended_ByShared() const;
	std::string Suspend_Reason() const;
	boost::posix_time::ptime Suspend_Time() const;

	bool Language(const std::string &in);
	const std::string &Language() const;
	bool Protect(const boost::logic::tribool &in);
	bool Protect() const;
	bool Secure(const boost::logic::tribool &in);
	bool Secure() const;
	bool NoMemo(const boost::logic::tribool &in);
	bool NoMemo() const;
	bool Private(const boost::logic::tribool &in);
	bool Private() const;
	bool PRIVMSG(const boost::logic::tribool &in);
	bool PRIVMSG() const;
	bool NoExpire(const boost::logic::tribool &in);
	bool NoExpire() const;
	void ClearPicture();
	std::string PictureExt() const;
	boost::uint32_t Picture() const;
	bool LOCK_Language(const bool &in);
	bool LOCK_Language() const;
	bool LOCK_Protect(const bool &in);
	bool LOCK_Protect() const;
	bool LOCK_Secure(const bool &in);
	bool LOCK_Secure() const;
	bool LOCK_NoMemo(const bool &in);
	bool LOCK_NoMemo() const;
	bool LOCK_Private(const bool &in);
	bool LOCK_Private() const;
	bool LOCK_PRIVMSG(const bool &in);
	bool LOCK_PRIVMSG() const;

	bool ACCESS_Matches(const std::string &in) const;
	bool ACCESS_Matches(const boost::shared_ptr<LiveUser> &in) const;
	bool ACCESS_Exists(boost::uint32_t num) const;
	boost::uint32_t ACCESS_Add(const std::string &in);
	void ACCESS_Del(boost::uint32_t num);
	void ACCESS_Change(boost::uint32_t num, const std::string &in);
	void ACCESS_Reindex(boost::uint32_t num, boost::uint32_t newnum);
	std::pair<std::string, boost::posix_time::ptime> ACCESS_Get(boost::uint32_t num) const;
	void ACCESS_Get(std::map<boost::uint32_t, std::pair<std::string, boost::posix_time::ptime> > &fill) const;
	void ACCESS_Get(std::vector<boost::uint32_t> &fill) const;

	bool IGNORE_Exists(const boost::shared_ptr<StoredUser> &in) const;
	bool IGNORE_Exists(boost::uint32_t num) const;
	boost::uint32_t IGNORE_Add(const boost::shared_ptr<StoredUser> &in);
	void IGNORE_Del(const boost::shared_ptr<StoredUser> &in);
	void IGNORE_Del(boost::uint32_t num);
	void IGNORE_Change(boost::uint32_t num, const boost::shared_ptr<StoredUser> &in);
	void IGNORE_Reindex(boost::uint32_t num, boost::uint32_t newnum);
	std::pair<boost::shared_ptr<StoredUser>, boost::posix_time::ptime> IGNORE_Get(boost::uint32_t num) const;
	void IGNORE_Get(std::map<boost::uint32_t, std::pair<boost::shared_ptr<StoredUser>, boost::posix_time::ptime> > &fill) const;
	void IGNORE_Get(std::vector<boost::uint32_t> &fill) const;

	bool MEMO_Exists(boost::uint32_t num) const;
	void MEMO_Del(boost::uint32_t num);
	Memo MEMO_Get(boost::uint32_t num) const;
	void MEMO_Get(std::set<Memo> &fill) const;

	std::string translate(const std::string &in) const;
	std::string translate(const std::string &single,
						  const std::string &plural,
						  unsigned long n) const;
};

// Special interface used by StoredNick.
class if_StoredUser_StoredNick
{
	friend class StoredNick;
	StoredUser &base;

	// This is INTENTIONALLY private ...
	if_StoredUser_StoredNick(StoredUser &b) : base(b) {}
	if_StoredUser_StoredNick(const boost::shared_ptr<StoredUser> &b) : base(*(b.get())) {}

	static inline boost::shared_ptr<StoredUser> create(const std::string &password)
		{ return StoredUser::create(password); }
	inline void Online(const boost::shared_ptr<LiveUser> &user)
		{ base.Online(user); }
	inline void Offline(const boost::shared_ptr<LiveUser> &user)
		{ base.Offline(user); }
	inline void Add(const boost::shared_ptr<StoredNick> &nick)
		{ base.Add(nick); }
	inline void Del(const boost::shared_ptr<StoredNick> &nick)
		{ base.Del(nick); }
	inline void SendInfo(const boost::shared_ptr<LiveUser> &service,
						 const boost::shared_ptr<LiveUser> &user) const
		{ base.SendInfo(service, user); }
};

// Special interface used by StoredChannel.
class if_StoredUser_StoredChannel
{
	friend class StoredChannel;
	StoredUser &base;

	// This is INTENTIONALLY private ...
	if_StoredUser_StoredChannel(StoredUser &b) : base(b) {}
	if_StoredUser_StoredChannel(const boost::shared_ptr<StoredUser> &b) : base(*(b.get())) {}

	inline void Add(const boost::shared_ptr<StoredChannel> &channel)
		{ base.Add(channel); }
	inline void Del(const boost::shared_ptr<StoredChannel> &channel)
		{ base.Del(channel); }
};

// Special interface used by StoredChannel.
class if_StoredUser_Storage
{
	friend class Storage;
	StoredUser &base;

	// This is INTENTIONALLY private ...
	if_StoredUser_Storage(StoredUser &b) : base(b) {}
	if_StoredUser_Storage(const boost::shared_ptr<StoredUser> &b) : base(*(b.get())) {}

	static inline boost::shared_ptr<StoredUser> load(boost::uint32_t id)
		{ return StoredUser::load(id); }
	inline void DropInternal()
		{ base.DropInternal(); }
	inline void Add(const boost::shared_ptr<StoredChannel> &channel)
		{ base.Add(channel); }
	inline void Del(const boost::shared_ptr<StoredChannel> &channel)
		{ base.Del(channel); }
};

// Used for tracing mainly.
inline std::ostream &operator<<(std::ostream &os, const StoredUser &in)
{
	return (os << in.ID());
}

template<typename T>
inline bool operator<(const boost::shared_ptr<StoredUser> &lhs, const T &rhs)
{
	return (*lhs < rhs);
}

inline bool operator<(const boost::shared_ptr<StoredUser> &lhs,
					  const boost::shared_ptr<StoredUser> &rhs)
{
	return (*lhs < *rhs);
}

#endif // _MAGICK_STOREDUSER_H

