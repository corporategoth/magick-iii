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
#ifndef _MAGICK_DEPENDENCY_H
#define _MAGICK_DEPENDENCY_H 1

#ifdef RCSID
RCSID(magick__dependency_h, "@(#) $Id$");
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
#include "message.h"

#include <map>

#include <boost/thread/mutex.hpp>
#include <boost/shared_ptr.hpp>

class Dependency
{
public:
	enum Type_t {
			ServerExists,
			ServerNotExists,
			NickExists,
			NickNotExists,
			ChannelExists,
			ChannelNotExists,
			NickInChannel,
			NickNotInChannel
		};

private:
	Message msg_;
	std::multimap<Type_t, std::string> outstanding_;
	unsigned int event_;

	void operator()();
public:
	Dependency(const Message &msg);
	~Dependency();

	void Update();

	void Satisfy(Type_t type, const std::string &meta);
	const std::multimap<Type_t, std::string> &Outstanding() const { return outstanding_; }

	const Message &MSG() const { return msg_; }
};

class DependencyEngine
{
	friend class Dependency;

	boost::mutex lock_;
	std::map<Dependency::Type_t, std::multimap<std::string, boost::shared_ptr<Dependency> > > outstanding_;

public:
	void Add(const Message &m);
	void Satisfy(Dependency::Type_t type, const std::string &meta);
	void Satisfy(Dependency::Type_t type, const std::string &meta1,
				 const std::string &meta2)
	{
		Satisfy(type, meta1 + ":" + meta2);
	}
};

#endif // _MAGICK_DEPENDENCY_H
