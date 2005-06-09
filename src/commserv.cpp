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
#define RCSID(x,y) const char *rcsid_magick__commserv_cpp_ ## x () { return y; }
RCSID(magick__commserv_cpp, "@(#)$Id$");

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
#include "storednick.h"
#include "storeduser.h"
#include "committee.h"

#include <mantra/core/trace.h>

static bool biREGISTER(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biREGISTER" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);

	if (params.size() < 3)
	{
		SEND(service, user,
			 N_("Insufficient parameters for %1% command."),
			 boost::algorithm::to_upper_copy(params[0]));
		MT_RET(false);
	}

	static boost::regex rx("^[[:alpha:]][[:alnum:]]*$");
	if (!boost::regex_match(params[1], rx))
	{
		NSEND(service, user, N_("Invalid committee name specified."));
		MT_RET(false);
	}

	boost::shared_ptr<Committee> comm = ROOT->data.Get_Committee(params[1]);
	if (comm)
	{
		SEND(service, user,
			 N_("Committee %1% is already registered."), params[1]);
		MT_RET(false);
	}

	boost::shared_ptr<StoredNick> nick;
	if (params[2][0] == '@')
	{
		comm = ROOT->data.Get_Committee(params[2].substr(1));
		if (!comm)
		{
			SEND(service, user,
				 N_("Committee %1% is not registered."), params[2].substr(1));
			MT_RET(false);
		}
	}
	else
	{
		nick = ROOT->data.Get_StoredNick(params[2]);
		if (!nick)
		{
			SEND(service, user,
				 N_("Nickname %1% is not registered."), params[2]);
			MT_RET(false);
		}
	}

	boost::shared_ptr<Committee> newcomm;
	if (comm)
	{
		newcomm = Committee::create(params[1], comm);
		SEND(service, user, N_("Committee \002%1%\017 has been created with the \002%2%\017 committee as its head."),
			 newcomm->Name() % comm->Name());
	}
	else
	{
		newcomm = Committee::create(params[1], nick->User());
		SEND(service, user, N_("Committee \002%1%\017 has been created with \002%2%\017 as its head."),
			 newcomm->Name() % nick->Name());
	}

	if (params.size() > 3)
	{
		std::string desc(params[3]);
		for (size_t i = 4; i < params.size(); ++i)
			desc += " " + params[i];
		newcomm->Description(desc);
	}

	MT_RET(true);
	MT_EE
}

static bool biDROP(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biDROP" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);

	if (params.size() < 2)
	{
		SEND(service, user,
			 N_("Insufficient parameters for %1% command."),
			 boost::algorithm::to_upper_copy(params[0]));
		MT_RET(false);
	}

	boost::shared_ptr<Committee> comm = ROOT->data.Get_Committee(params[1]);
	if (!comm)
	{
		SEND(service, user,
			 N_("Committee %1% is not registered."), params[1]);
		MT_RET(false);
	}

	comm->Drop();
	SEND(service, user, N_("Committee %1% has been dropped."), comm->Name());

	MT_RET(true);
	MT_EE
}

static bool biINFO(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biINFO" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);

	if (params.size() < 2)
	{
		SEND(service, user,
			 N_("Insufficient parameters for %1% command."),
			 boost::algorithm::to_upper_copy(params[0]));
		MT_RET(false);
	}

	boost::shared_ptr<Committee> comm = ROOT->data.Get_Committee(params[1]);
	if (!comm)
	{
		SEND(service, user,
			 N_("Committee %1% is not registered."), params[1]);
		MT_RET(false);
	}

	comm->SendInfo(service, user);

	MT_RET(true);
	MT_EE
}

static bool biMEMO(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biMEMO" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);

	// TODO: To be implemented.
	SEND(service, user,
		 N_("The %1% command has not yet been implemented."),
		 boost::algorithm::to_upper_copy(params[0]));
	
	MT_RET(true);
	MT_EE
}

static bool biMEMBER_ADD(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biMEMBER_ADD" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);

	if (params.size() < 3)
	{
		SEND(service, user,
			 N_("Insufficient parameters for %1% command."),
			 boost::algorithm::to_upper_copy(params[0]));
		MT_RET(false);
	}

	boost::shared_ptr<Committee> comm = ROOT->data.Get_Committee(params[1]);
	if (!comm)
	{
		SEND(service, user,
			 N_("Committee %1% is not registered."), params[1]);
		MT_RET(false);
	}

	boost::shared_ptr<StoredNick> nick = user->Stored();
	if (!nick)
	{
		SEND(service, user,
			 N_("Nickname %1% is not registered or you are not recognized as this nickname."), user->Name());
		MT_RET(false);
	}

	boost::shared_ptr<StoredNick> target = ROOT->data.Get_StoredNick(params[2]);
	if (!target)
	{
		SEND(service, user,
			 N_("Nickname %1% is not registered."), params[2]);
		MT_RET(false);
	}

	if (!comm->IsHead(user))
	{
		SEND(service, user,
			 N_("You are not a head of committee %1%."), comm->Name());
		MT_RET(false);
	}

	if (comm->IsMember(target->User()))
	{
		SEND(service, user,
			 N_("%1% is already a member of committee %2% (or its head)."),
			 target->Name() % comm->Name());
		MT_RET(false);
	}

	comm->MEMBER_Add(target->User(), nick);
	SEND(service, user, N_("%1% has been added to committee %2%."),
		 target->Name() % comm->Name());

	MT_RET(true);
	MT_EE
}

static bool biMEMBER_DEL(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biMEMBER_DEL" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);

	if (params.size() < 3)
	{
		SEND(service, user,
			 N_("Insufficient parameters for %1% command."),
			 boost::algorithm::to_upper_copy(params[0]));
		MT_RET(false);
	}

	boost::shared_ptr<Committee> comm = ROOT->data.Get_Committee(params[1]);
	if (!comm)
	{
		SEND(service, user,
			 N_("Committee %1% is not registered."), params[1]);
		MT_RET(false);
	}

	boost::shared_ptr<StoredNick> nick = ROOT->data.Get_StoredNick(params[2]);
	if (!nick)
	{
		SEND(service, user,
			 N_("Nickname %1% is not registered."), params[2]);
		MT_RET(false);
	}
		
	if (!comm->IsHead(user))
	{
		SEND(service, user,
			 N_("You are not a head of committee %1%."), comm->Name());
		MT_RET(false);
	}

	if (!comm->MEMBER_Exists(nick->User()))
	{
		SEND(service, user,
			 N_("%1% is not a member of committee %2%."),
			 nick->Name() % comm->Name());
		MT_RET(false);
	}

	comm->MEMBER_Del(nick->User());
	SEND(service, user, N_("%1% has been removed from committee %2%."),
		 nick->Name() % comm->Name());

	MT_RET(true);
	MT_EE
}

static bool biMEMBER_LIST(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biMEMBER_LIST" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);

	if (params.size() < 2)
	{
		SEND(service, user,
			 N_("Insufficient parameters for %1% command."),
			 boost::algorithm::to_upper_copy(params[0]));
		MT_RET(false);
	}

	boost::shared_ptr<Committee> comm = ROOT->data.Get_Committee(params[1]);
	if (!comm)
	{
		SEND(service, user,
			 N_("Committee %1% is not registered."), params[1]);
		MT_RET(false);
	}

	if (!comm->IsMember(user) && comm->Private())
	{
		SEND(service, user, N_("Committee %1% is private."), comm->Name());
		MT_RET(false);
	}
	
	std::set<Committee::Member> ent;
	comm->MEMBER_Get(ent);
	if (ent.empty())
	{
		SEND(service, user, N_("Committee %1% has no members."), comm->Name());
		MT_RET(true);
	}

	bool first = false;
	std::set<Committee::Member>::iterator i;
	for (i = ent.begin(); i != ent.end(); ++i)
	{
		StoredUser::my_nicks_t nicks = i->Entry()->Nicks();
		if (nicks.empty())
			continue;

		std::string str;
		StoredUser::my_nicks_t::const_iterator j;
		for (j = nicks.begin(); j != nicks.end(); ++j)
		{
			if (!*j)
				continue;

			if (!str.empty())
				str += ", ";
			str += (*j)->Name();
		}

		if (!first)
		{
			SEND(service, user, N_("Members of committee \002%1%\017:"),
				 comm->Name());
			first = true;
		}
		SEND(service, user, N_("%1% [added by %2% %3% ago]"),
			 str % i->Last_UpdaterName() %
			 DurationToString(mantra::duration(i->Last_Update(),
							  mantra::GetCurrentDateTime()), mantra::Second));
	}

	if (!first)
	{
		SEND(service, user, N_("Committee %1% has no members."), comm->Name());
		MT_RET(false);
	}

	MT_RET(true);
	MT_EE
}

static bool biMESSAGE_ADD(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biMESSAGE_ADD" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);

	if (params.size() < 3)
	{
		SEND(service, user,
			 N_("Insufficient parameters for %1% command."),
			 boost::algorithm::to_upper_copy(params[0]));
		MT_RET(false);
	}

	boost::shared_ptr<Committee> comm = ROOT->data.Get_Committee(params[1]);
	if (!comm)
	{
		SEND(service, user,
			 N_("Committee %1% is not registered."), params[1]);
		MT_RET(false);
	}

	boost::shared_ptr<StoredNick> nick = user->Stored();
	if (!nick)
	{
		SEND(service, user,
			 N_("Nickname %1% is not registered or you are not recognized as this nickname."), user->Name());
		MT_RET(false);
	}

	if (!comm->IsHead(user))
	{
		SEND(service, user,
			 N_("You are not a head of committee %1%."), comm->Name());
		MT_RET(false);
	}

	std::string message(params[2]);
	for (size_t i=3; i < params.size(); ++i)
		message += ' ' + params[i];

	Committee::Message m = comm->MESSAGE_Add(message, nick);
	SEND(service, user,
		 N_("Message added to committee %1% as #%2%."),
		 comm->Name() % m.Number());

	MT_RET(true);
	MT_EE
}

static bool biMESSAGE_DEL(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biMESSAGE_DEL" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);

	if (params.size() < 3)
	{
		SEND(service, user,
			 N_("Insufficient parameters for %1% command."),
			 boost::algorithm::to_upper_copy(params[0]));
		MT_RET(false);
	}

	boost::shared_ptr<Committee> comm = ROOT->data.Get_Committee(params[1]);
	if (!comm)
	{
		SEND(service, user,
			 N_("Committee %1% is not registered."), params[1]);
		MT_RET(false);
	}

	boost::shared_ptr<StoredNick> nick = user->Stored();
	if (!nick)
	{
		SEND(service, user,
			 N_("Nickname %1% is not registered or you are not recognized as this nickname."), user->Name());
		MT_RET(false);
	}

	if (!comm->IsHead(user))
	{
		SEND(service, user,
			 N_("You are not a head of committee %1%."), comm->Name());
		MT_RET(false);
	}

	std::vector<unsigned int> v;
	if (!mantra::ParseNumbers(params[1], v))
	{
		NSEND(service, user,
			  N_("Invalid number, number sequence, or number range specified."));
		MT_RET(false);
	}

	std::vector<unsigned int> ne;
	for (size_t i = 0; i < v.size(); ++i)
	{
		if (!comm->MESSAGE_Exists(v[i]))
		{
			ne.push_back(v[i]);
			continue;
		}

		comm->MESSAGE_Del(v[i]);
	}
	if (ne.empty())
	{
		SEND(service, user,
			 N_("%1% entries removed from committee %2%'s message list."),
			 v.size() % comm->Name());
	}
	else if (v.size() != ne.size())
	{
		SEND(service, user,
			 N_("%1% entries removed from committee %2%'s message list and %3% entries specified could not be found."),
			 (v.size() - ne.size()) % comm->Name() % ne.size());
	}
	else
	{
		SEND(service, user,
			 N_("No specified entries could be found on committee %1%'s message list."),
			 comm->Name());
	}

	MT_RET(true);
	MT_EE
}

static bool biMESSAGE_LIST(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biMESSAGE_LIST" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);

	if (params.size() < 2)
	{
		SEND(service, user,
			 N_("Insufficient parameters for %1% command."),
			 boost::algorithm::to_upper_copy(params[0]));
		MT_RET(false);
	}

	boost::shared_ptr<Committee> comm = ROOT->data.Get_Committee(params[1]);
	if (!comm)
	{
		SEND(service, user,
			 N_("Committee %1% is not registered."), params[1]);
		MT_RET(false);
	}

	if (!comm->IsMember(user))
	{
		SEND(service, user, N_("You are not a member of committee %1%."), comm->Name());
		MT_RET(false);
	}
	
	std::set<Committee::Message> ent;
	comm->MESSAGE_Get(ent);
	if (ent.empty())
	{
		SEND(service, user, N_("Committee %1% has no messages."), comm->Name());
		MT_RET(true);
	}

	bool first = false;
	std::set<Committee::Message>::iterator i;
	for (i = ent.begin(); i != ent.end(); ++i)
	{
		if (!first)
		{
			SEND(service, user, N_("Messages for committee \002%1%\017:"),
				 comm->Name());
			first = true;
		}

		SEND(service, user, N_("%1$ 3d. %2%"), i->Number() % i->Entry());
		SEND(service, user, N_("     Added by %1% %2% ago."),
			 i->Last_UpdaterName() %
			 DurationToString(mantra::duration(i->Last_Update(),
							  mantra::GetCurrentDateTime()), mantra::Second));
	}

	if (!first)
	{
		SEND(service, user, N_("Committee %1% has no messages."), comm->Name());
		MT_RET(false);
	}

	MT_RET(true);
	MT_EE
}

static bool biSET_HEAD(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biSET_HEAD" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);

	if (params.size() < 3)
	{
		SEND(service, user,
			 N_("Insufficient parameters for %1% command."),
			 boost::algorithm::to_upper_copy(params[0]));
		MT_RET(false);
	}

	boost::shared_ptr<Committee> comm = ROOT->data.Get_Committee(params[1]);
	if (!comm)
	{
		SEND(service, user,
			 N_("Committee %1% is not registered."), params[1]);
		MT_RET(false);
	}

	if (!comm->IsHead(user))
	{
		SEND(service, user,
			 N_("You are not a head of committee %1%."), comm->Name());
		MT_RET(false);
	}

	if (params[2][0] == '@')
	{
		boost::shared_ptr<Committee> head_comm = ROOT->data.Get_Committee(params[2].substr(1));
		if (!head_comm)
		{
			SEND(service, user,
				 N_("Committee %1% is not registered."), params[2].substr(1));
			MT_RET(false);
		}

		comm->Head(head_comm);
		SEND(service, user, N_("Head of committee %1% changed to committee %2%."),
			 comm->Name() % head_comm->Name());
	}
	else
	{
		boost::shared_ptr<StoredNick> head_nick = ROOT->data.Get_StoredNick(params[2]);
		if (!head_nick)
		{
			SEND(service, user,
				 N_("Nickname %1% is not registered."), params[2]);
			MT_RET(false);
		}
		
		comm->Head(head_nick->User());
		SEND(service, user, N_("Head of committee %1% changed to %2%."),
			 comm->Name() % head_nick->Name());
	}

	MT_RET(true);
	MT_EE
}

static bool biSET_EMAIL(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biSET_EMAIL" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);

	MT_RET(true);
	MT_EE
}

static bool biSET_WEBSITE(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biSET_WEBSITE" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);

	MT_RET(true);
	MT_EE
}

static bool biSET_DESCRIPTION(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biSET_DESCRIPTION" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);

	MT_RET(true);
	MT_EE
}

static bool biSET_SECURE(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biSET_SECURE" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);

	MT_RET(true);
	MT_EE
}

static bool biSET_OPENMEMO(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biSET_OPENMEMO" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);

	MT_RET(true);
	MT_EE
}

static bool biSET_PRIVATE(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biSET_PRIVATE" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);

	MT_RET(true);
	MT_EE
}

static bool biUNSET_EMAIL(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biUNSET_EMAIL" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);

	MT_RET(true);
	MT_EE
}

static bool biUNSET_WEBSITE(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biUNSET_WEBSITE" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);

	MT_RET(true);
	MT_EE
}

static bool biUNSET_DESCRIPTION(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biUNSET_DESCRIPTION" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);

	MT_RET(true);
	MT_EE
}

static bool biUNSET_SECURE(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biUNSET_SECURE" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);

	MT_RET(true);
	MT_EE
}

static bool biUNSET_OPENMEMO(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biUNSET_OPENMEMO" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);

	MT_RET(true);
	MT_EE
}

static bool biUNSET_PRIVATE(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biUNSET_PRIVATE" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);

	MT_RET(true);
	MT_EE
}

static bool biSET_COMMENT(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biSET_COMMENT" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);

	MT_RET(true);
	MT_EE
}

static bool biUNSET_COMMENT(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biUNSET_COMMENT" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);

	MT_RET(true);
	MT_EE
}

static bool biLOCK_SECURE(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biLOCK_SECURE" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);

	MT_RET(true);
	MT_EE
}

static bool biLOCK_OPENMEMO(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biLOCK_OPENMEMO" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);

	MT_RET(true);
	MT_EE
}

static bool biLOCK_PRIVATE(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biLOCK_PRIVATE" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);

	MT_RET(true);
	MT_EE
}

static bool biUNLOCK_SECURE(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biUNLOCK_SECURE" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);

	MT_RET(true);
	MT_EE
}

static bool biUNLOCK_OPENMEMO(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biUNLOCK_OPENMEMO" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);

	MT_RET(true);
	MT_EE
}

static bool biUNLOCK_PRIVATE(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biUNLOCK_PRIVATE" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);

	MT_RET(true);
	MT_EE
}

static bool biLIST(const boost::shared_ptr<LiveUser> &service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biLIST" << service << user << params);

	if (!service || !service->GetService())
		MT_RET(false);

	MT_RET(true);
	MT_EE
}

void init_commserv_functions(Service &serv)
{
	MT_EB
	MT_FUNC("init_commserv_functions" << serv);

	std::vector<std::string> comm_regd, comm_sop, comm_opersop;
	comm_regd.push_back(ROOT->ConfigValue<std::string>("commserv.regd.name"));
	comm_sop.push_back(ROOT->ConfigValue<std::string>("commserv.sop.name"));
	comm_opersop.push_back(ROOT->ConfigValue<std::string>("commserv.oper.name"));
	comm_opersop.push_back(ROOT->ConfigValue<std::string>("commserv.sop.name"));

	serv.PushCommand("^HELP$", boost::bind(&Service::Help, &serv,
										   _1, _2, _3));

	serv.PushCommand("^REGISTER$", &biREGISTER, comm_sop);
	serv.PushCommand("^DROP$", &biDROP, comm_sop);
	serv.PushCommand("^INFO$", &biINFO);
	serv.PushCommand("^(SEND)?MEMO$", &biMEMO, comm_regd);

	serv.PushCommand("^MEM(BER)?$",
					 Service::CommandMerge(serv, 0, 2), comm_regd);
	serv.PushCommand("^MEM(BER)?\\s+ADD$",
					 &biMEMBER_ADD, comm_regd);
	serv.PushCommand("^MEM(BER)?\\s+(ERASE|DEL(ETE)?)$",
					 &biMEMBER_DEL, comm_regd);
	serv.PushCommand("^MEM(BER)?\\s+(LIST|VIEW)$",
					 &biMEMBER_LIST, comm_regd);
	serv.PushCommand("^MEM(BER)?\\s+HELP$",
					 boost::bind(&Service::AuxHelp, &serv,
								 _1, _2, _3), comm_regd);

	serv.PushCommand("^((LOG|SIGN)ON)?(MESSAGE|MSG)$",
					 Service::CommandMerge(serv, 0, 2), comm_regd);
	serv.PushCommand("^((LOG|SIGN)ON)?(MESSAGE|MSG)\\s+ADD$",
					 &biMESSAGE_ADD, comm_regd);
	serv.PushCommand("^((LOG|SIGN)ON)?(MESSAGE|MSG)\\s+(ERASE|DEL(ETE)?)$",
					 &biMESSAGE_DEL, comm_regd);
	serv.PushCommand("^((LOG|SIGN)ON)?(MESSAGE|MSG)\\s+(LIST|VIEW)$",
					 &biMESSAGE_LIST, comm_regd);
	serv.PushCommand("^((LOG|SIGN)ON)?(MESSAGE|MSG)\\s+HELP$",
					 boost::bind(&Service::AuxHelp, &serv,
								 _1, _2, _3), comm_regd);

	serv.PushCommand("^SET$",
					 Service::CommandMerge(serv, 0, 2), comm_regd);
	serv.PushCommand("^SET\\s+HEAD$",
					 &biSET_HEAD, comm_regd);
	serv.PushCommand("^SET\\s+E-?MAIL$",
					 &biSET_EMAIL, comm_regd);
	serv.PushCommand("^SET\\s+(URL|WWW|WEB(PAGE|SITE)?|HTTPS?)$",
					 &biSET_WEBSITE, comm_regd);
	serv.PushCommand("^SET\\s+DESC(RIPT(ION)?)?$",
					 &biSET_DESCRIPTION, comm_regd);
	serv.PushCommand("^SET\\s+SECURE$",
					 &biSET_SECURE, comm_regd);
	serv.PushCommand("^SET\\s+OPENMEMOS?$",
					 &biSET_OPENMEMO, comm_regd);
	serv.PushCommand("^SET\\s+PRIV(ATE)?$",
					 &biSET_PRIVATE, comm_regd);
	serv.PushCommand("^SET\\s+HELP$",
					 boost::bind(&Service::AuxHelp, &serv,
								 _1, _2, _3), comm_regd);

	serv.PushCommand("^(UN|RE)SET$",
					 Service::CommandMerge(serv, 0, 2), comm_regd);
	serv.PushCommand("^(UN|RE)SET\\s+E-?MAIL$",
					 &biUNSET_EMAIL, comm_regd);
	serv.PushCommand("^(UN|RE)SET\\s+(URL|WWW|WEB(PAGE|SITE)?|HTTPS?)$",
					 &biUNSET_WEBSITE, comm_regd);
	serv.PushCommand("^(UN|RE)SET\\s+DESC(RIPT(ION)?)?$",
					 &biUNSET_DESCRIPTION, comm_regd);
	serv.PushCommand("^(UN|RE)SET\\s+SECURE$",
					 &biUNSET_SECURE, comm_regd);
	serv.PushCommand("^(UN|RE)SET\\s+OPENMEMOS?$",
					 &biUNSET_OPENMEMO, comm_regd);
	serv.PushCommand("^(UN|RE)SET\\s+PRIV(ATE)?$",
					 &biUNSET_PRIVATE, comm_regd);
	serv.PushCommand("^(UN|RE)SET\\s+HELP$",
					 boost::bind(&Service::AuxHelp, &serv,
								 _1, _2, _3), comm_regd);

	serv.PushCommand("^SET\\s+COMMENT$",
					 &biSET_COMMENT, comm_opersop);
	serv.PushCommand("^(UN|RE)SET\\s+COMMENT$",
					 &biUNSET_COMMENT, comm_opersop);

	serv.PushCommand("^LOCK$",
					 Service::CommandMerge(serv, 0, 2), comm_sop);
	serv.PushCommand("^LOCK\\s+SECURE$",
					 &biLOCK_SECURE, comm_sop);
	serv.PushCommand("^LOCK\\s+OPENMEMOS?$",
					 &biLOCK_OPENMEMO, comm_sop);
	serv.PushCommand("^LOCK\\s+PRIV(ATE)?$",
					 &biLOCK_PRIVATE, comm_sop);
	serv.PushCommand("^LOCK\\s+HELP$",
					 boost::bind(&Service::AuxHelp, &serv,
								 _1, _2, _3), comm_sop);

	serv.PushCommand("^UN?LOCK$",
					 Service::CommandMerge(serv, 0, 2), comm_sop);
	serv.PushCommand("^UN?LOCK\\s+SECURE$",
					 &biUNLOCK_SECURE, comm_sop);
	serv.PushCommand("^UN?LOCK\\s+OPENMEMOS$",
					 &biUNLOCK_OPENMEMO, comm_sop);
	serv.PushCommand("^UN?LOCK\\s+PRIV(ATE)?$",
					 &biUNLOCK_PRIVATE, comm_sop);
	serv.PushCommand("^UN?LOCK\\s+HELP$",
					 boost::bind(&Service::AuxHelp, &serv,
								 _1, _2, _3), comm_sop);

	// These commands don't operate on any nickname.
	serv.PushCommand("^LIST$", &biLIST);

	MT_EE
}


