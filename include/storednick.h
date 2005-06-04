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
#ifndef _MAGICK_STOREDNICK_H
#define _MAGICK_STOREDNICK_H 1

#ifdef RCSID
RCSID(magick__storednick_h, "@(#) $Id$");
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

#include <boost/thread/mutex.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>
#include <boost/noncopyable.hpp>
#include <boost/operators.hpp>

class StoredNick : public boost::noncopyable, public boost::totally_ordered1<StoredNick>
{
	friend class if_StoredNick_LiveUser;
	friend class if_StoredNick_StoredUser;
	friend class if_StoredNick_Storage;

	boost::weak_ptr<StoredNick> self;
	static StorageInterface storage;

	std::string name_;
	boost::shared_ptr<StoredUser> user_;

	boost::shared_ptr<LiveUser> SYNC(live_);

	// use if_StoredNick_LiveUser
	void Online(const boost::shared_ptr<LiveUser> &live);
	void Offline(const std::string &reason);

	// use if_StoredNick_StoredUser
	static boost::shared_ptr<StoredNick> Last_Seen(const boost::shared_ptr<StoredUser> &user);

	// use if_StoredNick_Storage
	void DropInternal();
	static boost::shared_ptr<StoredNick> load(const std::string &name,
											  const boost::shared_ptr<StoredUser> &user);

	StoredNick(const std::string &name, const boost::shared_ptr<StoredUser> &user)
		: name_(name), user_(user) {}
public:
	// This will also create the relevant StoredUser
	static boost::shared_ptr<StoredNick> create(const std::string &name,
												const std::string &password);
	static boost::shared_ptr<StoredNick> create(const std::string &name,
												const boost::shared_ptr<StoredUser> &user);

	const std::string &Name() const { return name_; }
	const boost::shared_ptr<StoredUser> &User() const { return user_; }

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
	inline bool operator<(const StoredNick &rhs) const { return *this < rhs.Name(); }
	inline bool operator==(const StoredNick &rhs) const { return *this == rhs.Name(); }

	boost::shared_ptr<LiveUser> Live() const;

	boost::posix_time::ptime Registered() const
		{ return boost::get<boost::posix_time::ptime>(storage.GetField(name_, "registered")); }

	std::string Last_RealName() const;
	std::string Last_Mask() const;
	std::string Last_Quit() const;
	boost::posix_time::ptime Last_Seen() const;

	void Drop();

	void SendInfo(const boost::shared_ptr<LiveUser> &service,
				  const boost::shared_ptr<LiveUser> &user) const;
};

// Special interface used by LiveUser.
class if_StoredNick_LiveUser
{
	friend class LiveUser;
	StoredNick &base;

	// This is INTENTIONALLY private ...
	if_StoredNick_LiveUser(StoredNick &b) : base(b) {}
	if_StoredNick_LiveUser(const boost::shared_ptr<StoredNick> &b) : base(*(b.get())) {}

	inline void Online(const boost::shared_ptr<LiveUser> &user)
		{ base.Online(user); }
	inline void Offline(const std::string &in)
		{ base.Offline(in); }
};

// Special interface used by LiveUser.
class if_StoredNick_StoredUser
{
	friend class StoredUser;
	StoredNick &base;

	// This is INTENTIONALLY private ...
	if_StoredNick_StoredUser(StoredNick &b) : base(b) {}
	if_StoredNick_StoredUser(const boost::shared_ptr<StoredNick> &b) : base(*(b.get())) {}

	static inline boost::shared_ptr<StoredNick> Last_Seen(const boost::shared_ptr<StoredUser> &user)
		{ return StoredNick::Last_Seen(user); }
};

// Special interface used by Storage.
class if_StoredNick_Storage
{
	friend class Storage;
	StoredNick &base;

	// This is INTENTIONALLY private ...
	if_StoredNick_Storage(StoredNick &b) : base(b) {}
	if_StoredNick_Storage(const boost::shared_ptr<StoredNick> &b) : base(*(b.get())) {}

	static inline boost::shared_ptr<StoredNick> load(const std::string &name,
													 const boost::shared_ptr<StoredUser> &user)
		{ return StoredNick::load(name, user); }
	inline void DropInternal()
		{ base.DropInternal(); }
};

// Used for tracing mainly.
inline std::ostream &operator<<(std::ostream &os, const StoredNick &in)
{
	return (os << in.Name());
}

#endif // _MAGICK_STOREDNICK_H

