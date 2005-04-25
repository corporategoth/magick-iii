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
#ifndef _MAGICK_MESSAGE_H
#define _MAGICK_MESSAGE_H 1

#ifdef RCSID
RCSID(magick__message_h, "@(#) $Id$");
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

#include <string>
#include <vector>
#include <map>
#include <ostream>

#include <mantra/core/algorithms.h>

#include <boost/thread/mutex.hpp>
#include <boost/function/function1.hpp>

class Message
{
	friend class Protocol;
	friend std::ostream &operator<<(std::ostream &os, const Message &in);

public:
	typedef boost::function1<bool, const Message &> functor;

private:
	typedef std::map<std::string, functor, mantra::iless<std::string> > func_map_t;
	static func_map_t func_map;

	std::string source_;
	std::string id_;
	std::vector<std::string> params_;

	Message(const std::string &source, const std::string &id)
		: source_(source), id_(id) {}
	std::string print() const;
public:
	Message() {}
	Message(const Message &in) { *this = in; }
	~Message() {}

	const std::string &Source() const { return source_; }
	const std::string &ID() const { return id_; }
	const std::vector<std::string> &Params() const { return params_; }

	static void ResetCommands();
	bool Process();
};

inline std::ostream &operator<<(std::ostream &os, const Message &in)
{
	return (os << in.print());
}

#endif // _MAGICK_MESSAGE_H
