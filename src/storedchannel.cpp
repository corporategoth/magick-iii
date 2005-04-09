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
#define RCSID(x,y) const char *rcsid_magick__storedchannel_cpp_ ## x () { return y; }
RCSID(magick__storedchannel_cpp, "@(#)$Id$");

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
#include "storedchannel.h"
#include "livechannel.h"
#include "storednick.h"
#include "livenick.h"

#include <mantra/core/trace.h>

StoredChannel::StoredChannel(const std::string &name,
							 const std::string &password,
							 const boost::shared_ptr<StoredUser> &founder)
	: name_(name)
{
	MT_EB
	MT_FUNC("StoredChannel::StoredChannel" << name << password << founder);

	mantra::Storage::RecordMap rec;
	rec["name"] = name;
	rec["password"] = ROOT->data.CryptPassword(password);
	rec["founder"] = founder->ID();
	storage.InsertRow(rec);

	MT_EE
}

