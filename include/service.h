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
#ifndef _MAGICK_SERVICE_H
#define _MAGICK_SERVICE_H 1

#ifdef RCSID
RCSID(magick__service_h, "@(#) $Id$");
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

#include <set>
#include <string>

#include <mantra/core/algorithms.h>

#include <boost/format.hpp>
#include <boost/bind.hpp>

class LiveUser;

class Service
{
	std::string primary_;
	std::set<std::string, mantra::iless<std::string> > nicks_;
	std::set<boost::shared_ptr<LiveUser> > users_;

public:
	Service();
	virtual ~Service();

	void Set(const std::vector<std::string> &nicks);
	void Check();

	const std::string &Primary() const { return primary_; }
	const std::set<std::string, mantra::iless<std::string> > &Nicks() const { return nicks_; }

	bool IsNick(const std::string &nick) const
		{ return (nicks_.find(nick) != nicks_.end()); }

	void QUIT(const std::string &source, const std::string &message = std::string());
	void MASSQUIT(const std::string &message = std::string())
	{
		for_each(nicks_.begin(), nicks_.end(),
				 boost::bind(&Service::QUIT, this, _1, message));
	}

	void PRIVMSG(const std::string &source, const std::string &target, const boost::format &message);
	void PRIVMSG(const std::string &target, const boost::format &message)
		{ PRIVMSG(primary_, target, message); }

	void NOTICE(const std::string &source, const std::string &target, const boost::format &message);
	void NOTICE(const std::string &target, const boost::format &message)
		{ NOTICE(primary_, target, message); }

	void ANNOUNCE(const std::string &source, const boost::format &message);
	void ANNOUNCE(const boost::format &message)
		{ ANNOUNCE(primary_, message); }
};


#endif // _MAGICK_SERVICE_H
