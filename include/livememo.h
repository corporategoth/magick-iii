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
#ifndef _MAGICK_LIVEMEMO_H
#define _MAGICK_LIVEMEMO_H 1

#ifdef RCSID
RCSID(magick__livememo_h, "@(#) $Id$");
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

class LiveUser;
class StoredNick;
class StoredUser;
class StoredChannel;

class LiveMemo : private boost::noncopyable,
				 public boost::totally_ordered1<LiveMemo>
{
	unsigned int id_;
	unsigned int event_;
	boost::shared_ptr<LiveUser> originator_;
	boost::shared_ptr<StoredNick> sender_;

	// Only one of these will be populated.
	boost::shared_ptr<StoredUser> user_;
	boost::shared_ptr<StoredChannel> channel_;

	boost::mutex lock_;
	std::string text_;

	// Need to add stuff to link to active DCC ...

	// Used by timer to deliver memo, time's up!
	void operator()() const;
public:
	LiveMemo(unsigned int id, const boost::shared_ptr<LiveUser> &user,
			 const boost::shared_ptr<StoredUser> &sender,
			 const boost::shared_ptr<StoredUser> &recipient);
	LiveMemo(unsigned int id, const boost::shared_ptr<LiveUser> &user,
			 const boost::shared_ptr<StoredUser> &sender,
			 const boost::shared_ptr<StoredChannel> &recipient);

	unsigned int ID() const { return id_; }
	const boost::shared_ptr<LiveUser> &Originator() const { return originator_; }
	const boost::shared_ptr<StoredNick> &Sender() const { return sender_; }
	const boost::shared_ptr<StoredUser> &User() const { return user_; }
	const boost::shared_ptr<StoredChannel> &Channel() const { return channel_; }

	void Append(const std::string &text);
	const std::string &Text() const;

	// void DCC

	bool operator<(const LiveMemo &rhs) const
	{
		return (Originator() < rhs.Originator() || 
				(Originator() == rhs.Originator() && ID() < rhs.ID()));
	}
	bool operator==(const LiveMemo &rhs) const
	{
		return (Originator() == rhs.Originator() && ID() == rhs.ID());
	}
};

void deliver(const LiveMemo &in);

#endif // _MAGICK_LIVEMEMO_H

