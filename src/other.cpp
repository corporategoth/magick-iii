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
#define RCSID(x,y) const char *rcsid_magick__other_cpp_ ## x () { return y; }
RCSID(magick__other_cpp, "@(#)$Id$");

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
#include "liveuser.h"
#include "serviceuser.h"

#include <mantra/core/trace.h>

static bool biCONTRIB(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biCONTRIB" << service << user << params);

	const char * const *p = contrib;
	while (p)
	{
		NSEND(service, user, *p);
		++p;
	}

	MT_RET(true);
	MT_EE
}

static bool biASK(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biASK" << service << user << params);

	service->HELPOP(format("[ASK from %1%]: %2%") % user->Name() % params[1]);

	MT_RET(true);
	MT_EE
}

static bool biMAP(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biMAP" << service << user << params);



	MT_RET(true);
	MT_EE
}

void init_other_functions(Service &serv)
{
	MT_EB
	MT_FUNC("init_other_functions" << serv);

//    std::vector<std::string> comm_regd, comm_oper, comm_sop, comm_opersop;
//    comm_regd.push_back(ROOT->ConfigValue<std::string>("commserv.regd.name"));
//    comm_oper.push_back(ROOT->ConfigValue<std::string>("commserv.oper.name"));
//    comm_sop.push_back(ROOT->ConfigValue<std::string>("commserv.sop.name"));
//    comm_opersop.push_back(ROOT->ConfigValue<std::string>("commserv.oper.name"));
//    comm_opersop.push_back(ROOT->ConfigValue<std::string>("commserv.sop.name"));

	serv.PushCommand("^HELP$", boost::bind(&Service::Help, &serv,
										   _1, _2, _3));

	serv.PushCommand("^(CONTRIB|CREDITS?)$", &biCONTRIB);
	serv.PushCommand("^(ASK|QUSTION)$", &biASK, 1);
	serv.PushCommand("^(MAP|BREAKDOWN)$", &biMAP);

	MT_EE
}

