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
#define RCSID(x,y) const char *rcsid_magick__service_cpp_ ## x () { return y; }
RCSID(magick__service_cpp, "@(#)$Id$");

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

Service::Service()
{
}

Service::~Service()
{
}

void Service::Set(const std::vector<std::string> &nicks)
{
	MT_EB
	MT_FUNC("Service::Set" << "(const std::vector<std::string> &) nicks");

	std::set<std::string, mantra::iless<std::string> > signoff;
	if (!nicks.empty())
	{
		std::set_difference(nicks_.begin(), nicks_.end(),
							nicks.begin(), nicks.end(),
							std::inserter(signoff, signoff.end()));

		primary_ = nicks[0];
		nicks_.clear();
		nicks_.insert(nicks.begin(), nicks.end());
	}
	else
	{
		signoff = nicks_;

		primary_.clear();
		nicks_.clear();
	}

	if (ROOT->getUplink() && !signoff.empty())
	{
		std::string quitmsg;
		try
		{
			quitmsg = ROOT->ConfigValue<std::string>("services.quit-message");
		}
		catch (const boost::bad_any_cast &)
		{
		}

		for_each(signoff.begin(), signoff.end(), 
				 boost::bind(&Service::QUIT, this, _1, quitmsg));
	}

	MT_EE
}

void Service::Check()
{
	MT_EB
	MT_FUNC("Service::Check");

	if (!ROOT->getUplink())
		return;


	MT_EE
}

void Service::QUIT(const std::string &source, const std::string &message)
{
	MT_EB
	MT_FUNC("Service::QUIT" << source << message);

	if (!ROOT->getUplink())
		return;


	MT_EE
}

void Service::PRIVMSG(const std::string &source, const std::string &target, const boost::format &message)
{
	MT_EB
	MT_FUNC("Service::PRIVMSG" << source << target << message);

	if (!ROOT->getUplink())
		return;


	MT_EE
}

void Service::NOTICE(const std::string &source, const std::string &target, const boost::format &message)
{
	MT_EB
	MT_FUNC("Service::NOTICE" << source << target << message);

	if (!ROOT->getUplink())
		return;


	MT_EE
}

void Service::ANNOUNCE(const std::string &source, const boost::format &message)
{
	MT_EB
	MT_FUNC("Service::ANNOUNCE" << source << message);

	if (!ROOT->getUplink())
		return;


	MT_EE
}
