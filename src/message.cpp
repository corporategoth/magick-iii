#ifdef WIN32
#pragma hdrstop
#else
#pragma implementation
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
** it may be viewed here: http://www.neuromaancy.net/license/gpl.html
**
** ======================================================================= */
#define RCSID(x,y) const char *rcsid_magick__message_cpp_ ## x () { return y; }
RCSID(magick__message_cpp, "@(#)$Id$");

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

#include "magick.h"

#include <mantra/core/trace.h>

#include <sstream>

// This function CAN NOT BE TRACED!
std::string Message::print() const
{
	std::stringstream os;
	if (!source_.empty())
		os << ":" << source_ << " ";
	os << id_;
	if (!params_.empty())
	{
		size_t i;
		for (i=0; i<params_.size()-1; ++i)
			os << " " << params_[i];
		if (params_[i].find(' ') != std::string::npos)
			os << " :";
		else
			os << " ";
		os << params_[i];
	}
		
	return os.str();
}

bool Message::Process()
{
	MT_EB
	MT_FUNC("Message::Process");

	LOG(Debug + 1, _("Processing message: %1%"), print());

	MT_RET(true);
	MT_EE
}

