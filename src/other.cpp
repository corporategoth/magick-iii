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

	std::string message(params[1]);
	for (size_t i = 2; i < params.size(); ++i)
		message += ' ' + params[i];

	service->HELPOP(format("[ASK from %1%]: %2%") % user->Name() % message);

	MT_RET(true);
	MT_EE
}

static void GraphChildren(const ServiceUser *service,
						  const boost::shared_ptr<LiveUser> &user, size_t count,
						  const Server &server, const std::string &prefix)
{
	std::set<boost::shared_ptr<Server> > children = server.getChildren();
	std::set<boost::shared_ptr<Server> >::iterator i = children.begin();
	while (i != children.end())
	{
		// Increment and get the current one back, so we can check if the next
		// entry is end() but keep working on the current entry.
		std::set<boost::shared_ptr<Server> >::iterator work = i++;

		if (i != children.end())
		{
			SEND(service, user, _("%1$-50s %2$ 3.03fs %3$ 5u (%4$ 3u) %5$ 3.2f%%"),
				 (prefix + "|- " + (*work)->Name()) %
				 (double((*work)->Lag().total_milliseconds()) / 1000.0) %
				 (*work)->Users() % (*work)->Opers() %
				 (double((*work)->Users()) / double(count) * 100.0));
			GraphChildren(service, user, count, **work, prefix + "|  ");
		}
		else
		{
			SEND(service, user, _("%1$-50s %2$ 3.03fs %3$ 5u (%4$ 3u) %5$ 3.2f%%"),
				 (prefix + "`- " + (*work)->Name()) %
				 (double((*work)->Lag().total_milliseconds()) / 1000.0) %
				 (*work)->Users() % (*work)->Opers() %
				 (double((*work)->Users()) / double(count) * 100.0));
			GraphChildren(service, user, count, **work, prefix + "   ");
		}
	}
}

static bool biMAP(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biMAP" << service << user << params);

	boost::shared_ptr<Uplink> uplink = ROOT->getUplink();
	if (!uplink)
		MT_RET(false);

	size_t count = ROOT->data.Users();

	NSEND(service, user, N_("SERVER                                                  LAG USERS OPERS"));
	SEND(service, user, _("%1$-50s %2$ 3.03fs %3$ 5u (%4$ 3u) %5$ 3.2f%%"),
		 uplink->Name() % 0.0 % uplink->Users() % uplink->Opers() %
		 (double(uplink->Users()) / double(count) * 100.0));
	GraphChildren(service, user, count, *uplink, std::string());

	MT_RET(true);
	MT_EE
}

class SendMessage : public Server::ChildrenAction
{
	const ServiceUser *service_;
	boost::shared_ptr<LiveUser> user_;
	std::string message_;

	virtual bool visitor(const boost::shared_ptr<Server> &s)
	{
		MT_EB
		MT_FUNC("SendMessage::visitor" << s);

		service_->NOTICE(s, format(_("[\002GLOBAL\017 from %1%] %2%")) %
								   user_->Name() % message_);

		MT_RET(true);
		MT_EE
	}

public:
	SendMessage(const ServiceUser *service,
				const boost::shared_ptr<LiveUser> &user,
				const std::string &message)
		: Server::ChildrenAction(true), service_(service),
		  user_(user), message_(message) {}
	virtual ~SendMessage() {}
};

static bool biGLOBAL(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biGLOBAL" << service << user << params);

	std::string message(params[1]);
	for (size_t i = 2; i < params.size(); ++i)
		message += ' ' + params[i];

	SendMessage sm(service, user, message);
	sm(ROOT->getUplink());

	MT_RET(true);
	MT_EE
}

void init_other_functions(Service &serv)
{
	MT_EB
	MT_FUNC("init_other_functions" << serv);

    std::vector<std::string> comm_admin;
//    std::vector<std::string> comm_regd, comm_oper, comm_sop, comm_opersop;
//    comm_regd.push_back(ROOT->ConfigValue<std::string>("commserv.regd.name"));
//    comm_oper.push_back(ROOT->ConfigValue<std::string>("commserv.oper.name"));
//    comm_sop.push_back(ROOT->ConfigValue<std::string>("commserv.sop.name"));
//    comm_opersop.push_back(ROOT->ConfigValue<std::string>("commserv.oper.name"));
//    comm_opersop.push_back(ROOT->ConfigValue<std::string>("commserv.sop.name"));
    comm_admin.push_back(ROOT->ConfigValue<std::string>("commserv.admin.name"));

	serv.PushCommand("^HELP$", boost::bind(&Service::Help, &serv,
										   _1, _2, _3));

	serv.PushCommand("^(CONTRIB|CREDITS?)$", &biCONTRIB);
	serv.PushCommand("^(ASK|QUSTION)$", &biASK, 1);
	serv.PushCommand("^(MAP|BREAKDOWN)$", &biMAP);
	serv.PushCommand("^GLOBAL$", &biGLOBAL, 1, comm_admin);

	MT_EE
}

