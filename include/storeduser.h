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
#include "storage.h"
#include "memo.h"

#include <boost/thread/mutex.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>
#include <boost/noncopyable.hpp>
#include <boost/operators.hpp>

class StoredUser : private boost::noncopyable, public boost::totally_ordered1<StoredUser>
{
	friend class if_StoredUser_LiveUser;
	friend class if_StoredUser_StoredNick;
	friend class if_StoredUser_StoredChannel;

	boost::weak_ptr<StoredUser> self;
	static StorageInterface storage;
	static StorageInterface storage_access;
	static StorageInterface storage_ignore;

	boost::uint32_t id_;

	boost::mutex lock_;
	std::set<boost::shared_ptr<LiveUser> > online_users_;
	std::set<boost::shared_ptr<StoredNick> > my_nicks_;
	std::set<boost::shared_ptr<StoredChannel> > my_channels_;

	// use if_StoredUser_DCC
	void Picture(boost::uint32_t in);

	// use if_StoredUser_StoredNick
	StoredUser(const std::string &password,
			   const boost::shared_ptr<StoredNick> &nick);
	void Online(const boost::shared_ptr<LiveUser> &user);
	void Offline(const boost::shared_ptr<LiveUser> &user);
	void Add(const boost::shared_ptr<StoredNick> &nick);
	void Del(const boost::shared_ptr<StoredNick> &nick);

	// use if_StoredUser_StoredChannel
	void Add(const boost::shared_ptr<StoredChannel> &channel);
	void Del(const boost::shared_ptr<StoredChannel> &channel);

public:

	boost::uint32_t ID() const { return id_; }

	boost::posix_time::ptime Last_Update() const
		{ return boost::get<boost::posix_time::ptime>(storage.GetField(id_, "last_update")); }
	boost::shared_ptr<StoredNick> Last_Online() const;

	bool operator<(const StoredUser &rhs) const { return ID() < rhs.ID(); }
	bool operator==(const StoredUser &rhs) const { return ID() == rhs.ID(); }

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

	void Suspend(const boost::shared_ptr<LiveUser> &user,
				 const std::string &reason);
	void Unsuspend();
	std::string Suspended_By() const;
	boost::shared_ptr<StoredUser> Suspended_ByShared() const;
	std::string Suspend_Reason() const;
	boost::posix_time::ptime Suspend_Time() const;

	void Language(const std::string &in);
	std::string Language() const;
	void Protect(const boost::logic::tribool &in);
	bool Protect() const;
	void Secure(const boost::logic::tribool &in);
	bool Secure() const;
	void NoMemo(const boost::logic::tribool &in);
	bool NoMemo() const;
	void Private(const boost::logic::tribool &in);
	bool Private() const;
	void PRIVMSG(const boost::logic::tribool &in);
	bool PRIVMSG() const;
	void NoExpire(const bool &in);
	bool NoExpire() const;
	void ClearPicture();
	boost::uint32_t Picture() const;
	void LOCK_Language(const bool &in);
	bool LOCK_Language() const;
	void LOCK_Protect(const bool &in);
	bool LOCK_Protect() const;
	void LOCK_Secure(const bool &in);
	bool LOCK_Secure() const;
	void LOCK_NoMemo(const bool &in);
	bool LOCK_NoMemo() const;
	void LOCK_Private(const bool &in);
	bool LOCK_Private() const;
	void LOCK_PRIVMSG(const bool &in);
	bool LOCK_PRIVMSG() const;

	bool ACCESS_Matches(const std::string &in) const;
	bool ACCESS_Matches(const boost::shared_ptr<LiveUser> &in) const;
	bool ACCESS_Exists(boost::uint32_t num) const;
	void ACCESS_Add(const std::string &in);
	void ACCESS_Del(boost::uint32_t num);
	void ACCESS_Change(boost::uint32_t num, const std::string &in);
	std::pair<std::string, boost::posix_time::ptime> ACCESS_Get(boost::uint32_t num) const;
	void ACCESS_Get(std::map<boost::uint32_t, std::pair<std::string, boost::posix_time::ptime> > &fill) const;

	bool IGNORE_Matches(const std::string &in) const;
	bool IGNORE_Matches(const boost::shared_ptr<LiveUser> &in) const;
	bool IGNORE_Exists(boost::uint32_t num) const;
	void IGNORE_Add(const std::string &in);
	void IGNORE_Del(boost::uint32_t num);
	void IGNORE_Change(boost::uint32_t num, const std::string &in);
	std::pair<std::string, boost::posix_time::ptime> IGNORE_Get(boost::uint32_t num) const;
	void IGNORE_Get(std::map<boost::uint32_t, std::pair<std::string, boost::posix_time::ptime> > &fill) const;

	bool MEMO_Exists(boost::uint32_t num) const;
	void MEMO_Del(boost::uint32_t num);
	Memo MEMO_Get(boost::uint32_t num) const;
	void MEMO_Get(std::set<Memo> &fill) const;
};

// Special interface used by StoredNick.
class if_StoredUser_StoredNick
{
	friend class StoredNick;
	StoredUser &base;

	// This is INTENTIONALLY private ...
	if_StoredUser_StoredNick(StoredUser &b) : base(b) {}
	if_StoredUser_StoredNick(const boost::shared_ptr<StoredUser> &b) : base(*(b.get())) {}

	static inline boost::shared_ptr<StoredUser> create(const std::string &password,
													   const boost::shared_ptr<StoredNick> &nick)
	{
		boost::shared_ptr<StoredUser> rv(new StoredUser(password, nick));
		rv->self = rv;
		return rv;
	}

	inline void Online(const boost::shared_ptr<LiveUser> &user)
		{ base.Online(user); }
	inline void Offline(const boost::shared_ptr<LiveUser> &user)
		{ base.Offline(user); }
	inline void Add(const boost::shared_ptr<StoredNick> &nick)
		{ base.Add(nick); }
	inline void Del(const boost::shared_ptr<StoredNick> &nick)
		{ base.Del(nick); }
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

// Used for tracing mainly.
inline std::ostream &operator<<(std::ostream &os, const StoredUser &in)
{
	return (os << in.ID());
}

#endif // _MAGICK_STOREDUSER_H

