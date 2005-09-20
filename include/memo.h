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
#ifndef _MAGICK_MEMO_H
#define _MAGICK_MEMO_H 1

#ifdef RCSID
RCSID(magick__memo_h, "@(#) $Id$");
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

#include <boost/thread/mutex.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/noncopyable.hpp>
#include <boost/operators.hpp>

class StoredUser;
class StoredChannel;

class BaseMemo : private boost::noncopyable
{
	StorageInterface &si;
	boost::uint32_t num_;

protected:
	BaseMemo(StorageInterface &si, boost::uint32_t num);
	virtual void FillKey(mantra::ComparisonSet &conditions) = 0;

public:
	virtual ~BaseMemo() {}
	boost::uint32_t Number() const { return num_; }

	boost::posix_time::ptime Sent() const;
	std::string SenderName() const;
	boost::shared_ptr<StoredUser> Sender() const;
	std::string Text() const;
};

class Memo : public BaseMemo, public boost::totally_ordered1<Memo>
{
	friend class StoredUser;

	static StorageInterface storage;
	boost::shared_ptr<StoredUser> owner_;

	void FillKey(mantra::ComparisonSet &conditions);
	Memo(const boost::shared_ptr<StoredUser> &owner, boost::uint32_t num);
public:
	virtual ~Memo() {}

	const boost::shared_ptr<StoredUser> &Owner() const { return owner_; }
	void Read(bool in);
	bool Read() const;
	boost::uint32_t Attachment() const;

	bool operator<(const Memo &rhs) const
	{
		return (Owner() < rhs.Owner() || 
				(Owner() == rhs.Owner() && Number() < rhs.Number()));
	}
	bool operator==(const Memo &rhs) const
	{
		return (Owner() == rhs.Owner() && Number() == rhs.Number());
	}
};

class News : public BaseMemo, public boost::totally_ordered1<News>
{
	friend class StoredChannel;

	static StorageInterface storage;
	static StorageInterface storage_read;
	boost::shared_ptr<StoredChannel> owner_;

	void FillKey(mantra::ComparisonSet &conditions);
	News(const boost::shared_ptr<StoredChannel> &owner, boost::uint32_t num);
public:
	virtual ~News() {}

	const boost::shared_ptr<StoredChannel> &Owner() const { return owner_; }
	void Read(const boost::shared_ptr<StoredUser> &user, bool in);
	bool Read(const boost::shared_ptr<StoredUser> &user) const;

	bool operator<(const News &rhs) const
	{
		return (Owner() < rhs.Owner() || 
				(Owner() == rhs.Owner() && Number() < rhs.Number()));
	}
	bool operator==(const News &rhs) const
	{
		return (Owner() == rhs.Owner() && Number() == rhs.Number());
	}
};

#endif // _MAGICK_MEMO_H

