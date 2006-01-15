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
#define RCSID(x,y) const char *rcsid_magick__operserv_cpp_ ## x () { return y; }
RCSID(magick__operserv_cpp, "@(#)$Id: template.cpp,v 1.1 2005/04/06 03:36:46 prez Exp $");

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
#include "storednick.h"
#include "storeduser.h"
#include "committee.h"

#include <mantra/core/trace.h>

static bool biJUPE(const ServiceUser *service,
				   const boost::shared_ptr<LiveUser> &user,
				   const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biJUPE" << service << user << params);

	boost::shared_ptr<Uplink> uplink = ROOT->getUplink();
	if (!uplink)
		MT_RET(false);

	boost::shared_ptr<Server> jupe = uplink->Find(params[0]);
	if (jupe)
		jupe->SQUIT(_("Server Juped"));

	std::string desc(params[1]);
	for (size_t i = 2; i < params.size(); ++i)
		desc += ' ' + params[i];

	jupe = Jupe::create(params[0], desc);
	uplink->Connect(jupe);
	ROOT->proto.Connect(*(dynamic_cast<Jupe *>(jupe.get())));

	MT_RET(true);
	MT_EE
}

class PingServers : public Server::ChildrenAction
{
	virtual bool visitor(const boost::shared_ptr<Server> &s)
	{
		MT_EB
		MT_FUNC("PingServers::visitor" << s);

		s->PING();

		MT_RET(true);
		MT_EE
	}

public:
	PingServers() : Server::ChildrenAction(true) {}
	~PingServers() {}
};

static bool biPING(const ServiceUser *service,
				   const boost::shared_ptr<LiveUser> &user,
				   const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biPING" << service << user << params);

	boost::shared_ptr<Uplink> uplink = ROOT->getUplink();
	if (!uplink)
		MT_RET(false);

	static mantra::iequal_to<std::string> cmp;
	if (cmp(params[1], "ALL"))
	{
		PingServers ps;
		ps(uplink);
		NSEND(service, user, N_("PING sent to ALL servers."));
	}
	else
	{
		boost::shared_ptr<Server> s = uplink->Find(params[1]);
		if (!s)
		{
			SEND(service, user, N_("Could not find server %1%."), params[1]);
			MT_RET(false);
		}

		s->PING();
		SEND(service, user, N_("PING sent to server %1%."), s->Name());
	}

	MT_RET(true);
	MT_EE
}

static bool biSAVE(const ServiceUser *service,
				   const boost::shared_ptr<LiveUser> &user,
				   const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biSAVE" << service << user << params);

	ROOT->data.Save();
	NSEND(service, user, "Databases saved.");

	MT_RET(true);
	MT_EE
}

static bool biON(const ServiceUser *service,
				 const boost::shared_ptr<LiveUser> &user,
				 const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biON" << service << user << params);

	static mantra::iequal_to<std::string> cmp;
	if (cmp(params[1], "ALL"))
	{
		ROOT->nickserv.Processing(true);
		ROOT->chanserv.Processing(true);
		ROOT->memoserv.Processing(true);
		ROOT->commserv.Processing(true);
		ROOT->other.Processing(true);
		NSEND(service, user, N_("Interactive mode on all services has been turned \002ON\017."));
		service->ANNOUNCE(format(_("%1% has turned on interactive mode of all services.")) %
						  user->Name());
	}
	else
	{
		boost::shared_ptr<LiveUser> target = ROOT->data.Get_LiveUser(params[1]);
		if (!target)
		{
			SEND(service, user, N_("User %1% is not currently online."), params[1]);
			MT_RET(false);
		}

		ServiceUser *su = dynamic_cast<ServiceUser *>(target.get());
		if (!su)
		{
			SEND(service, user, N_("User %1% is not a service."), target->Name());
			MT_RET(false);
		}

		if (su->GetService() == &ROOT->operserv)
		{
			NSEND(service, user, N_("The operator service may not be turned off or on."));
			MT_RET(false);
		}

		if (su->GetService()->Processing())
		{
			SEND(service, user, N_("%1% is already in interactive mode."), target->Name());
			MT_RET(false);
		}

		su->GetService()->Processing(true);
		SEND(service, user, N_("Interactive mode on service %1% has been turned \002ON\017."),
			 target->Name());
		service->ANNOUNCE(format(_("%1% has turned on interactive mode of service %2%.")) %
						  user->Name() % target->Name());
	}

	MT_RET(true);
	MT_EE
}

static bool biOFF(const ServiceUser *service,
				  const boost::shared_ptr<LiveUser> &user,
				  const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biOFF" << service << user << params);

	std::string reason(params[1]);
	for (size_t i = 2; i < params.size(); ++i)
		reason += ' ' + params[i];

	static mantra::iequal_to<std::string> cmp;
	if (cmp(params[1], "ALL"))
	{
		ROOT->nickserv.Processing(false);
		ROOT->chanserv.Processing(false);
		ROOT->memoserv.Processing(false);
		ROOT->commserv.Processing(false);
		ROOT->other.Processing(false);
		NSEND(service, user, N_("Interactive mode on all services has been turned \002OFF\017."));
		service->ANNOUNCE(format(_("%1% has turned on interactive mode of all services (%2%).")) %
						  user->Name() % reason);
	}
	else
	{
		boost::shared_ptr<LiveUser> target = ROOT->data.Get_LiveUser(params[1]);
		if (!target)
		{
			SEND(service, user, N_("User %1% is not currently online."), params[1]);
			MT_RET(false);
		}

		ServiceUser *su = dynamic_cast<ServiceUser *>(target.get());
		if (!su)
		{
			SEND(service, user, N_("User %1% is not a service."), target->Name());
			MT_RET(false);
		}

		if (su->GetService() == &ROOT->operserv)
		{
			NSEND(service, user, N_("The operator service may not be turned off or on."));
			MT_RET(false);
		}

		if (!su->GetService()->Processing())
		{
			SEND(service, user, N_("%1% is already in interactive mode."), target->Name());
			MT_RET(false);
		}

		su->GetService()->Processing(false);
		SEND(service, user, N_("Interactive mode on service %1% has been turned \002OFF\017."),
			 target->Name());
		service->ANNOUNCE(format(_("%1% has turned off interactive mode of service %2% (%3%).")) %
						  user->Name() % target->Name() % reason);
	}

	MT_RET(true);
	MT_EE
}

static bool biRESTART(const ServiceUser *service,
					  const boost::shared_ptr<LiveUser> &user,
					  const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biRESTART" << service << user << params);

	std::string reason(params[1]);
	for (size_t i = 2; i < params.size(); ++i)
		reason += ' ' + params[i];

//	ROOT->Shutdown(true);
	NSEND(service, user, N_("Restarting."));
	service->ANNOUNCE(format(_("%1% has initiated a services restart (%2%).")) %
					  user->Name() % reason);

	MT_RET(false);
	MT_EE
}

static bool biSHUTDOWN(const ServiceUser *service,
					   const boost::shared_ptr<LiveUser> &user,
					   const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biSHUTDOWN" << service << user << params);

	std::string reason(params[1]);
	for (size_t i = 2; i < params.size(); ++i)
		reason += ' ' + params[i];

	ROOT->Shutdown(true);
	NSEND(service, user, N_("Shutting down."));
	service->ANNOUNCE(format(_("%1% has shut down services (%2%).")) %
					  user->Name() % reason);

	MT_RET(true);
	MT_EE
}

static bool biHTM_ON(const ServiceUser *service,
					 const boost::shared_ptr<LiveUser> &user,
					 const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biHTM_ON" << service << user << params);

/*
	if (ROOT->HTM_Gap())
	{
		NSEND(service, user, N_("High Traffic Mode is already in effect."));
		MT_RET(false);
	}

	ROOT->HTM_Gap(ROOT->ConfigValue<mantra::duration>("operserv.manual-gap"));
	service->ANNOUNCE(format(_("%1% has forced High Traffic Mode on.")) % user->Name());
*/

	MT_RET(true);
	MT_EE
}

static bool biHTM_OFF(const ServiceUser *service,
					  const boost::shared_ptr<LiveUser> &user,
					  const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biHTM_OFF" << service << user << params);

/*
	if (!ROOT->HTM_Gap())
	{
		NSEND(service, user, N_("High Traffic Mode is not in effect."));
		MT_RET(false);
	}

	ROOT->HTM_Gap(mantra::duration());
	service->ANNOUNCE(format(_("%1% has forced High Traffic Mode off.")) % user->Name());
*/

	MT_RET(true);
	MT_EE
}

static bool biRELOAD_CONFIG(const ServiceUser *service,
							const boost::shared_ptr<LiveUser> &user,
							const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biRELOAD_CONFIG" << service << user << params);


	MT_RET(false);
	MT_EE
}

static bool biRELOAD_STORAGE(const ServiceUser *service,
							 const boost::shared_ptr<LiveUser> &user,
							 const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biRELOAD_STORAGE" << service << user << params);

	ROOT->data.Load();
	service->ANNOUNCE(format(_("%1% reloaded the database from base storage.")) % user->Name());

	MT_RET(true);
	MT_EE
}

static bool biKILL(const ServiceUser *service,
				   const boost::shared_ptr<LiveUser> &user,
				   const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biKILL" << service << user << params);

	boost::shared_ptr<LiveUser> target = ROOT->data.Get_LiveUser(params[1]);
	if (!target)
	{
		SEND(service, user, N_("User %1% is not currently online."), params[1]);
		MT_RET(false);
	}

	std::string reason(params[1]);
	for (size_t i = 2; i < params.size(); ++i)
		reason += ' ' + params[i];

	service->KILL(target, (format(_("Killed by %1% (%2%)")) % user->Name() % reason).str());
	SEND(service, user, N_("User %1% has been killed."), target->Name());

	MT_RET(false);
	MT_EE
}

static bool biANONKILL(const ServiceUser *service,
					   const boost::shared_ptr<LiveUser> &user,
					   const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biANONKILL" << service << user << params);

	boost::shared_ptr<LiveUser> target = ROOT->data.Get_LiveUser(params[1]);
	if (!target)
	{
		SEND(service, user, N_("User %1% is not currently online."), params[1]);
		MT_RET(false);
	}

	std::string reason(params[1]);
	for (size_t i = 2; i < params.size(); ++i)
		reason += ' ' + params[i];

	service->KILL(target, reason);
	SEND(service, user, N_("User %1% has been killed."), target->Name());

	MT_RET(false);
	MT_EE
}

static bool biQLINE(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biQLINE" << service << user << params);


	MT_RET(false);
	MT_EE
}

static bool biUNQLINE(const ServiceUser *service,
					  const boost::shared_ptr<LiveUser> &user,
					  const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biUNQLINE" << service << user << params);


	MT_RET(false);
	MT_EE
}

static bool biNOOP(const ServiceUser *service,
				   const boost::shared_ptr<LiveUser> &user,
				   const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biNOOP" << service << user << params);


	MT_RET(false);
	MT_EE
}

static bool biHIDE(const ServiceUser *service,
				   const boost::shared_ptr<LiveUser> &user,
				   const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biHIDE" << service << user << params);


	MT_RET(false);
	MT_EE
}


#ifdef MANTRA_TRACING

#define MANTRA_TRACE_LEVELS 16
static boost::regex TraceLevelRegex[MANTRA_TRACE_LEVELS] =
	{
		boost::regex("^C(HECK)?P(OINT)?$", boost::regex_constants::icase),
		boost::regex("^COMM(ENT)?$", boost::regex_constants::icase),
		boost::regex("^$", boost::regex_constants::icase),
		boost::regex("^$", boost::regex_constants::icase),
		boost::regex("^EXCEPT(ION)?$", boost::regex_constants::icase),
		boost::regex("^FUNC(TION)?$", boost::regex_constants::icase),
		boost::regex("^$", boost::regex_constants::icase),
		boost::regex("^$", boost::regex_constants::icase),
		boost::regex("^CHANG(ES?|ING)$", boost::regex_constants::icase),
		boost::regex("^MOD(IFY)?$", boost::regex_constants::icase),
		boost::regex("^S(OU)?RCE?$", boost::regex_constants::icase),
		boost::regex("^BIND$", boost::regex_constants::icase),
		boost::regex("^STAT(US|ISTICS)?$", boost::regex_constants::icase),
		boost::regex("^SOCK(ET)?$", boost::regex_constants::icase),
		boost::regex("^CHAT(TER)?$", boost::regex_constants::icase),
		boost::regex("^EXT(ERN(AL)?)?$", boost::regex_constants::icase)
	};
static std::string TraceLevels[MANTRA_TRACE_LEVELS] =
	{
		std::string("CheckPoint"),
		std::string("Comment"),
		std::string(),
		std::string(),
		std::string("Exception"),
		std::string("Function"),
		std::string(),
		std::string(),
		std::string("Changing"),
		std::string("Modify"),
		std::string("Source"),
		std::string("Bind"),
		std::string("Statistics"),
		std::string("Socket"),
		std::string("Chatter"),
		std::string("External")
	};

static bool biTRACE_SET(const ServiceUser *service,
						const boost::shared_ptr<LiveUser> &user,
						const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biTRACE_SET" << service << user << params);

	std::string svalue;
	try
	{
		svalue = mantra::dehex(params[2]);
	}
	catch (mantra::hex_format &e)
	{
		NSEND(service, user, N_("You may only use hexadecimal characters (0-9, a-f, A-F) in the trace level."));
		MT_RET(false);
	}

	boost::uint16_t value = 0;
	if (svalue.size() > 2)
	{
		NSEND(service, user, N_("You may only specify a trace level from 0x0000 - 0xffff."));
		MT_RET(false);
	}
	else if (svalue.size() == 2)
	{
		value = (static_cast<unsigned char>(svalue[0]) << 8) + static_cast<unsigned char>(svalue[1]);
	}
	else
	{
		value = static_cast<unsigned char>(svalue[0]);
	}

	static mantra::iequal_to<std::string> cmp;
	if (cmp(params[1], "ALL"))
	{
		for (size_t j = 0; j < static_cast<size_t>(MAGICK_TRACE_SIZE); ++j)
			mantra::mtrace::instance().TurnSet(j, value);
		SEND(service, user, N_("All trace types have been set to %1$#04x."), value);
	}
	else
	{
		size_t j;
		for (j = 0; j < static_cast<size_t>(MAGICK_TRACE_SIZE); ++j)
			if (boost::regex_match(params[1], TraceTypeRegex[j]))
			{
				mantra::mtrace::instance().TurnSet(j, value);
				SEND(service, user, N_("Trace for %1$s has been set to %2$#04x."),
					 TraceTypes[j] % value);
				break;
			}
		if (j >= static_cast<size_t>(MAGICK_TRACE_SIZE))
		{
			SEND(service, user, N_("Unknown trace type %1% specified."), params[1]);
			MT_RET(false);
		}
	}

	MT_RET(true);
	MT_EE
}

static bool biTRACE_UP(const ServiceUser *service,
					   const boost::shared_ptr<LiveUser> &user,
					   const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biTRACE_UP" << service << user << params);

	static mantra::iequal_to<std::string> cmp;
	boost::uint16_t value = 0;
	for (size_t i = 2; i < params.size(); ++i)
	{
		try
		{
			std::string svalue = mantra::dehex(params[i]);

			if (svalue.size() > 2)
			{
				NSEND(service, user, N_("You may only specify a trace level from 0x0000 - 0xffff."));
				MT_RET(false);
			}
			else if (svalue.size() == 2)
			{
				value |= (static_cast<unsigned char>(svalue[0]) << 8) + static_cast<unsigned char>(svalue[1]);
			}
			else
			{
				value |= static_cast<unsigned char>(svalue[0]);
			}
		}
		catch (mantra::hex_format &e)
		{
			if (cmp(params[i], "FULL") || cmp(params[i], "ALL"))
			{
				value = mantra::TT_Full;
				break;
			}

			size_t j;
			for (j = 0; j < MANTRA_TRACE_LEVELS; ++j)
				if (boost::regex_match(params[i], TraceLevelRegex[j]))
				{
					value |= (1 << j);
					break;
				}
			if (j >= MANTRA_TRACE_LEVELS)
			{
				SEND(service, user, N_("Unknown trace level %1% specified."), params[i]);
				MT_RET(false);
			}
		}
	}

	if (cmp(params[1], "ALL"))
	{
		for (size_t j = 0; j < static_cast<size_t>(MAGICK_TRACE_SIZE); ++j)
			mantra::mtrace::instance().TurnSet(j,
					   mantra::mtrace::instance().TraceTypeMask(j) | value);
		SEND(service, user, N_("All trace types have been turned up by %1$#04x."), value);
	}
	else
	{
		size_t j;
		for (j = 0; j < static_cast<size_t>(MAGICK_TRACE_SIZE); ++j)
			if (boost::regex_match(params[1], TraceTypeRegex[j]))
			{
				mantra::mtrace::instance().TurnSet(j,
						   mantra::mtrace::instance().TraceTypeMask(j) | value);
				SEND(service, user, N_("Trace for %1$s has been turned up by %2$#04x."),
					 TraceTypes[j] % value);
				break;
			}
		if (j >= static_cast<size_t>(MAGICK_TRACE_SIZE))
		{
			SEND(service, user, N_("Unknown trace type %1% specified."), params[1]);
			MT_RET(false);
		}
	}

	MT_RET(true);
	MT_EE
}

static bool biTRACE_DOWN(const ServiceUser *service,
						 const boost::shared_ptr<LiveUser> &user,
						 const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biTRACE_DOWN" << service << user << params);

	static mantra::iequal_to<std::string> cmp;
	boost::uint16_t value = 0;
	for (size_t i = 2; i < params.size(); ++i)
	{
		try
		{
			std::string svalue = mantra::dehex(params[i]);

			if (svalue.size() > 2)
			{
				NSEND(service, user, N_("You may only specify a trace level from 0x0000 - 0xffff."));
				MT_RET(false);
			}
			else if (svalue.size() == 2)
			{
				value |= (static_cast<unsigned char>(svalue[0]) << 8) + static_cast<unsigned char>(svalue[1]);
			}
			else
			{
				value |= static_cast<unsigned char>(svalue[0]);
			}
		}
		catch (mantra::hex_format &e)
		{
			if (cmp(params[i], "FULL") || cmp(params[i], "ALL"))
			{
				value = mantra::TT_Full;
				break;
			}

			size_t j;
			for (j = 0; j < MANTRA_TRACE_LEVELS; ++j)
				if (boost::regex_match(params[i], TraceLevelRegex[j]))
				{
					value |= (1 << j);
					break;
				}
			if (j >= MANTRA_TRACE_LEVELS)
			{
				SEND(service, user, N_("Unknown trace level %1% specified."), params[i]);
				MT_RET(false);
			}
		}
	}

	if (cmp(params[1], "ALL"))
	{
		for (size_t j = 0; j < static_cast<size_t>(MAGICK_TRACE_SIZE); ++j)
			mantra::mtrace::instance().TurnSet(j,
					   mantra::mtrace::instance().TraceTypeMask(j) & ~value);
		SEND(service, user, N_("All trace types have been turned down by %1$#04x."), value);
	}
	else
	{
		size_t j;
		for (j = 0; j < static_cast<size_t>(MAGICK_TRACE_SIZE); ++j)
			if (boost::regex_match(params[1], TraceTypeRegex[j]))
			{
				mantra::mtrace::instance().TurnSet(j,
						   mantra::mtrace::instance().TraceTypeMask(j) & ~value);
				SEND(service, user, N_("Trace for %1$s has been turned down by %2$#04x."),
					 TraceTypes[j] % value);
				break;
			}
		if (j >= static_cast<size_t>(MAGICK_TRACE_SIZE))
		{
			SEND(service, user, N_("Unknown trace type %1% specified."), params[1]);
			MT_RET(false);
		}
	}

	MT_RET(true);
	MT_EE
}

static bool biTRACE_LIST(const ServiceUser *service,
						 const boost::shared_ptr<LiveUser> &user,
						 const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biTRACE_LIST" << service << user << params);

	for (size_t j = 0; j < static_cast<size_t>(MAGICK_TRACE_SIZE); ++j)
	{
		SEND(service, user, N_("Trace for %1$s is set to %2$#04x."),
			 TraceTypes[j] % mantra::mtrace::instance().TraceTypeMask(j));
	}

	MT_RET(true);
	MT_EE
}

#endif // MANTRA_TRACING

static bool biCLONE_ADD(const ServiceUser *service,
						const boost::shared_ptr<LiveUser> &user,
						const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biCLONE_ADD" << service << user << params);

	boost::shared_ptr<StoredNick> nick = user->Stored();
	if (!nick)
	{
		SEND(service, user,
			 N_("Nickname %1% is not registered or you are not recognized as this nickname."), user->Name());
		MT_RET(false);
	}

	static boost::regex rx("^([^[:space:][:cntrl:]@]+@)?"
						   "([-[:alnum:]*?.]+|[[:xdigit:]*?:]+)+$");
	boost::uint32_t limit;
	try
	{
		limit = boost::lexical_cast<boost::uint32_t>(params[2]);
	}
	catch (const boost::bad_lexical_cast &e)
	{
		NSEND(service, user, N_("Limit must be an integer."));
		MT_RET(false);
	}

	if (limit == 0)
	{
		NSEND(service, user, N_("Limit may not be zero."));
		MT_RET(false);
	}
	else if (limit > ROOT->ConfigValue<unsigned int>("operserv.clone.max-limit"))
	{
		SEND(service, user, N_("You may not set a clone limit greater than %1%."),
			 ROOT->ConfigValue<unsigned int>("operserv.clone.max-limit"));
		MT_RET(false);
	}

	std::string reason = params[3];
	for (size_t i = 4; i < params.size(); ++i)
		reason += ' ' + params[i];

	if (boost::regex_match(params[1], rx))
	{
		std::set<Clone> ent;
		ROOT->data.Get_Clone(ent);

		Clone c;
		std::set<Clone>::iterator j;
		for (j = ent.begin(); j != ent.end(); ++j)
		{
			if (!*j)
				continue;

			if (mantra::glob_match(params[1], j->Mask(), true))
			{
				if (c.Number())
				{
					SEND(service, user, N_("Multiple clones match the mask %1%, please specify a more specific mask or an entry by number."),
						 params[1]);
					MT_RET(false);
				}
				c = *j;
			}
		}
		if (c)
		{
			c.Reason(reason, nick);
			c.Value(limit, nick);
			SEND(service, user, N_("Mask %1% has had their clone limit changed to %2%."),
				 params[1] % limit);
		}
		else
		{
			c = Clone::create(params[1], reason, limit, nick);
			SEND(service, user, N_("Clone entry #%1% created for \002%2%\017 with a limit of %3%."),
				 c.Number() % params[1] % limit);
		}

	}
	else
	{
		try
		{
			boost::uint32_t entry = boost::lexical_cast<boost::uint32_t>(params[1]);
			Clone c = ROOT->data.Get_Clone(entry);
			if (c)
			{
				c.Reason(reason, nick);
				c.Value(limit, nick);
				SEND(service, user, N_("Mask %1% has had their clone limit changed to %2%."),
					 params[1] % limit);
			}
			else
			{
				SEND(service, user, N_("Could not find entry #%1% on clone override list."),
					  entry);
				MT_RET(false);
			}
		}
		catch (boost::bad_lexical_cast &e)
		{
			SEND(service, user, N_("Invalid hostmask %1%."), params[1]);
			MT_RET(false);
		}
	}

	MT_RET(true);
	MT_EE
}

static bool biCLONE_DEL(const ServiceUser *service,
						const boost::shared_ptr<LiveUser> &user,
						const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biCLONE_DEL" << service << user << params);

	static boost::regex rx("^([^[:space:][:cntrl:]@]+@)?"
						   "([-[:alnum:]*?.]+|[[:xdigit:]*?:]+)+$");

	std::string numbers = params[1];
	for (size_t i = 2; i < params.size(); ++i)
		numbers += ' ' + params[i];

	size_t deleted = 0, notfound = 0;

	std::vector<unsigned int> nums;
	if (!mantra::ParseNumbers(numbers, nums))
	{
		std::set<Clone> ent;
		ROOT->data.Get_Clone(ent);

		for (size_t i = 1; i < params.size(); ++i)
		{
			if (boost::regex_match(params[i], rx))
			{
				bool found = false;
				std::set<Clone>::iterator j = ent.begin();
				while (j != ent.end())
				{
					if (!*j)
					{
						ent.erase(j++);
						continue;
					}

					if (mantra::glob_match(params[i], j->Mask(), true))
					{
						found = true;
						ROOT->data.Del(*j);
						++deleted;
						ent.erase(j++);
					}
					else
						++j;
				}
				if (!found)
					++notfound;
			}
			else
				++notfound;
		}
	}
	else
	{
		for (size_t j = 0; j < nums.size(); ++j)
		{
			Clone c = ROOT->data.Get_Clone(nums[j]);
			if (c)
			{
				ROOT->data.Del(c);
				++deleted;
			}
			else
				++notfound;
		}
	}

	if (!deleted)
	{
		NSEND(service, user, N_("No specified entries found on the clone override list."));
	}
	else if (!notfound)
	{
		SEND(service, user, N_("%1% entries removed from the clone override list."),
			 deleted);
	}
	else
	{
		SEND(service, user, N_("%1% entries removed (%2% not found) from the clone override list."),
			 deleted % notfound);
	}

	MT_RET(true);
	MT_EE
}

static bool biCLONE_LIST(const ServiceUser *service,
						 const boost::shared_ptr<LiveUser> &user,
						 const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biCLONE_LIST" << service << user << params);

	static boost::regex rx("^([^[:space:][:cntrl:]@]+@)?"
						   "([-[:alnum:]*?.]+|[[:xdigit:]*?:]+)+$");

	std::string numbers = params[1];
	for (size_t i = 2; i < params.size(); ++i)
		numbers += ' ' + params[i];

	std::set<Clone> clones;
	if (params.size() > 1)
	{
		std::vector<unsigned int> nums;
		if (!mantra::ParseNumbers(numbers, nums))
		{
			std::set<Clone> ent;
			ROOT->data.Get_Clone(ent);
			if (ent.empty())
			{
				NSEND(service, user, N_("The clone override list is empty."));
				MT_RET(false);
			}

			for (size_t i = 1; i < params.size(); ++i)
			{
				if (boost::regex_match(params[i], rx))
				{
					std::set<Clone>::iterator j;
					for (j = ent.begin(); j != ent.end(); ++j)
					{
						if (!*j)
							continue;

						if (mantra::glob_match(params[i], j->Mask(), true))
							clones.insert(*j);
					}
				}
			}
		}
		else
		{
			for (size_t j = 0; j < nums.size(); ++j)
			{
				Clone c = ROOT->data.Get_Clone(nums[j]);
				if (c)
					clones.insert(c);
			}
		}
	}
	else
	{
		ROOT->data.Get_Clone(clones);
		if (clones.empty())
		{
			NSEND(service, user, N_("The clone override list is empty."));
			MT_RET(false);
		}
	}

	if (clones.empty())
	{
		NSEND(service, user, N_("No entries matching your specification on the clone override list."));
		MT_RET(false);
	}
	else
	{
		std::set<Clone>::iterator i;
		NSEND(service, user, N_("     MASK                                                        LIMIT"));
		for (i = clones.begin(); i != clones.end(); ++i)
		{
			if (!*i)
				continue;

			SEND(service, user, N_("%1$ 3d. %2$-50s %3$ 3d (last modified by %4$s %5$s ago)"),
				 i->Number() % i->Mask() % i->Value() % i->Last_UpdaterName() %
				 DurationToString(mantra::duration(i->Last_Update(),
												   mantra::GetCurrentDateTime()), mantra::Second));
			SEND(service, user, N_("     %1%"), i->Reason());
		}
	}

	MT_RET(true);
	MT_EE
}

static bool biCLONE_REINDEX(const ServiceUser *service,
							const boost::shared_ptr<LiveUser> &user,
							const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biCLONE_REINDEX" << service << user << params);

	std::set<Clone> ent;
	ROOT->data.Get_Clone(ent);
	if (ent.empty())
	{
		NSEND(service, user, N_("The clone override list is empty."));
		MT_RET(true);
	}

	size_t changed = 0, count = 1;
	std::set<Clone>::iterator i;
	for (i = ent.begin(); i != ent.end(); ++i, ++count)
	{
		if (i->Number() != count && ROOT->data.Reindex(*i, count))
			++changed;
	}

	SEND(service, user, N_("Reindexing of clone override list complete, \002%1%\017 entries changed."),
		 changed);

	MT_RET(true);
	MT_EE
}

static bool biCLONE_LIVELIST(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biCLONE_LIVELIST" << service << user << params);

	// TODO: To be implemented.
	SEND(service, user,
		 N_("The %1% command has not yet been implemented."),
		 boost::algorithm::to_upper_copy(params[0]));

	MT_RET(true);
	MT_EE
}

static bool biAKILL_ADD(const ServiceUser *service,
						const boost::shared_ptr<LiveUser> &user,
						const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biAKILL_ADD" << service << user << params);

	boost::shared_ptr<StoredNick> nick = user->Stored();
	if (!nick)
	{
		SEND(service, user,
			 N_("Nickname %1% is not registered or you are not recognized as this nickname."), user->Name());
		MT_RET(false);
	}

	static boost::regex rx("^[^[:space:][:cntrl:]@]+@"
						   "([-[:alnum:]*?.]+|[[:xdigit:]*?:]+)+$");

	size_t offs = 2;
	mantra::duration length;
	if (params[offs][0] == '+')
	{
		try
		{
			mantra::StringToDuration(params[2].substr(1), length);
			if (++offs >= params.size())
			{
				SEND(service, user,
						 N_("Insufficient parameters for %1% command."),
						 boost::algorithm::to_upper_copy(params[0]));
				MT_RET(false);
			}
		}
		catch (const mantra::mdatetime_format &e)
		{
			NSEND(service, user, N_("Invalid duration specified."));
			MT_RET(false);
		}
	}
	else
		length = ROOT->ConfigValue<mantra::duration>("operserv.akill.expire");

	const char *limit_cfg = NULL;
	if (user->InCommittee(ROOT->ConfigValue<std::string>("commserv.sadmin.name")))
		limit_cfg = "operserv.akill.expire-sadmin";
	else if (user->InCommittee(ROOT->ConfigValue<std::string>("commserv.sop.name")))
		limit_cfg = "operserv.akill.expire-sop";
	else if (user->InCommittee(ROOT->ConfigValue<std::string>("commserv.admin.name")))
		limit_cfg = "operserv.akill.expire-admin";
	else if (user->InCommittee(ROOT->ConfigValue<std::string>("commserv.oper.name")))
		limit_cfg = "operserv.akill.expire-oper";
	else
	{
		// Should NOT happen.
		NSEND(service, user, N_("You do not have permissions to set an autokill."));
		MT_RET(false);
	}

	mantra::duration d = ROOT->ConfigValue<mantra::duration>(limit_cfg);
	if (!length)
	{
		if (!d)
		{
			SEND(service, user, N_("You may not set a permanent autokill, the maximum time you may set an autokill for is %1%."),
				 mantra::DurationToString(d, mantra::Second));
			MT_RET(false);
		}
	}
	else if (length > d)
	{
		SEND(service, user, N_("You may not set an autokill for %1%, the maximum time you may set an autokill for is %2%."),
			 mantra::DurationToString(length, mantra::Second) %
			 mantra::DurationToString(d, mantra::Second));
		MT_RET(false);
	}

	std::string reason(params[offs]);
	for (++offs; offs < params.size(); ++offs)
		reason += ' ' + params[offs];

	if (boost::regex_match(params[1], rx))
	{

		std::set<Akill> ent;
		ROOT->data.Get_Akill(ent);

		Akill a;
		std::set<Akill>::iterator j;
		for (j = ent.begin(); j != ent.end(); ++j)
		{
			if (!*j)
				continue;

			if (mantra::glob_match(params[1], j->Mask(), true))
			{
				if (a.Number())
				{
					SEND(service, user, N_("Multiple autokills match the mask %1%, please specify a more specific mask or an entry by number."),
						 params[1]);
					MT_RET(false);
				}
				a = *j;
			}
		}
		if (a)
		{
			a.Reason(reason, nick);

			mantra::duration oldlength = a.Length();
			if (length)
			{
				if (oldlength)
				{
					oldlength = mantra::duration(a.Creation(),
												 mantra::GetCurrentDateTime()) + length;
					a.Length(oldlength, nick);
				}
				else
					a.Length(length, nick);

				SEND(service, user, N_("Mask %1% has had their autokill extended by %2%."),
					 params[1] % mantra::DurationToString(length, mantra::Second));
			}
			else if (oldlength)
			{
				// Make it permanent ...
				a.Length(length, nick);
				SEND(service, user, N_("Mask %1% has had their autokill made permanent."),
					 params[1]);
			}
			else
			{
				SEND(service, user, N_("Mask %1% has had their autokill reason changed."),
					 params[1]);
			}
		}
		else
		{
			// TODO: Check for X% rule.

			a = Akill::create(params[1], reason, length, nick);
			if (length)
				SEND(service, user, N_("AutoKill entry #%1% created for \002%2%\017 for %3%."),
					 a.Number() % params[1] % length);
			else
				SEND(service, user, N_("AutoKill entry #%1% created for \002%2%\017 permanently."),
					 a.Number() % params[1]);
		}

	}
	else
	{
		try
		{
			boost::uint32_t entry = boost::lexical_cast<boost::uint32_t>(params[1]);
			Akill a = ROOT->data.Get_Akill(entry);
			if (a)
			{
				a.Reason(reason, nick);

				mantra::duration oldlength = a.Length();
				if (length)
				{
					if (oldlength)
					{
						oldlength = mantra::duration(a.Creation(),
													 mantra::GetCurrentDateTime()) + length;
						a.Length(oldlength, nick);
					}
					else
						a.Length(length, nick);

					SEND(service, user, N_("Entry #%1% has had their autokill extended by %2%."),
						 entry % mantra::DurationToString(length, mantra::Second));
				}
				else if (oldlength)
				{
					// Make it permanent ...
					a.Length(length, nick);
					SEND(service, user, N_("Entry #%1% has had their autokill made permanent."),
						 entry);
				}
				else
				{
					SEND(service, user, N_("Entry #%1% has had their autokill reason changed."),
						 entry);
				}
			}
			else
			{
				SEND(service, user, N_("Could not find entry #%1% on autokill list."),
					  entry);
				MT_RET(false);
			}
		}
		catch (boost::bad_lexical_cast &e)
		{
			SEND(service, user, N_("Invalid hostmask %1%."), params[1]);
			MT_RET(false);
		}
	}

	MT_RET(true);
	MT_EE
}

static bool biAKILL_DEL(const ServiceUser *service,
						const boost::shared_ptr<LiveUser> &user,
						const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biAKILL_DEL" << service << user << params);

	static boost::regex rx("^[^[:space:][:cntrl:]@]+@"
						   "([-[:alnum:]*?.]+|[[:xdigit:]*?:]+)+$");

	std::string numbers = params[1];
	for (size_t i = 2; i < params.size(); ++i)
		numbers += ' ' + params[i];

	size_t deleted = 0, notfound = 0;

	std::vector<unsigned int> nums;
	if (!mantra::ParseNumbers(numbers, nums))
	{
		std::set<Akill> ent;
		ROOT->data.Get_Akill(ent);

		for (size_t i = 1; i < params.size(); ++i)
		{
			if (boost::regex_match(params[i], rx))
			{
				bool found = false;
				std::set<Akill>::iterator j = ent.begin();
				while (j != ent.end())
				{
					if (!*j)
					{
						ent.erase(j++);
						continue;
					}

					if (mantra::glob_match(params[i], j->Mask(), true))
					{
						found = true;
						ROOT->data.Del(*j);
						++deleted;
						ent.erase(j++);
					}
					else
						++j;
				}
				if (!found)
					++notfound;
			}
			else
				++notfound;
		}
	}
	else
	{
		for (size_t j = 0; j < nums.size(); ++j)
		{
			Akill a = ROOT->data.Get_Akill(nums[j]);
			if (a)
			{
				ROOT->data.Del(a);
				++deleted;
			}
			else
				++notfound;
		}
	}

	if (!deleted)
	{
		NSEND(service, user, N_("No specified entries found on the autokill list."));
	}
	else if (!notfound)
	{
		SEND(service, user, N_("%1% entries removed from the autokill list."),
			 deleted);
	}
	else
	{
		SEND(service, user, N_("%1% entries removed (%2% not found) from the autokill list."),
			 deleted % notfound);
	}

	MT_RET(true);
	MT_EE
}

static bool biAKILL_LIST(const ServiceUser *service,
						 const boost::shared_ptr<LiveUser> &user,
						 const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biAKILL_LIST" << service << user << params);

	static boost::regex rx("^[^[:space:][:cntrl:]@]+@"
						   "([-[:alnum:]*?.]+|[[:xdigit:]*?:]+)+$");

	std::string numbers = params[1];
	for (size_t i = 2; i < params.size(); ++i)
		numbers += ' ' + params[i];

	std::set<Akill> akills;
	if (params.size() > 1)
	{
		std::vector<unsigned int> nums;
		if (!mantra::ParseNumbers(numbers, nums))
		{
			std::set<Akill> ent;
			ROOT->data.Get_Akill(ent);
			if (ent.empty())
			{
				NSEND(service, user, N_("The autokill list is empty."));
				MT_RET(false);
			}

			for (size_t i = 1; i < params.size(); ++i)
			{
				if (boost::regex_match(params[i], rx))
				{
					std::set<Akill>::iterator j;
					for (j = ent.begin(); j != ent.end(); ++j)
					{
						if (!*j)
							continue;

						if (mantra::glob_match(params[i], j->Mask(), true))
							akills.insert(*j);
					}
				}
			}
		}
		else
		{
			for (size_t j = 0; j < nums.size(); ++j)
			{
				Akill a = ROOT->data.Get_Akill(nums[j]);
				if (a)
					akills.insert(a);
			}
		}
	}
	else
	{
		ROOT->data.Get_Akill(akills);
		if (akills.empty())
		{
			NSEND(service, user, N_("The autokill list is empty."));
			MT_RET(false);
		}
	}

	if (akills.empty())
	{
		NSEND(service, user, N_("No entries matching your specification on the autokill list."));
		MT_RET(false);
	}
	else
	{
		std::set<Akill>::iterator i;
		NSEND(service, user, N_("     MASK                                                        LIMIT"));
		for (i = akills.begin(); i != akills.end(); ++i)
		{
			if (!*i)
				continue;

			SEND(service, user, N_("%1$ 3d. %2$s (last modified by %3$s %4$s ago)"),
				 i->Number() % i->Mask() % i->Last_UpdaterName() %
				 DurationToString(mantra::duration(i->Last_Update(),
												   mantra::GetCurrentDateTime()), mantra::Second));
			if (i->Length())
				SEND(service, user, N_("     [%1% left of %2%] %3%"),
						 DurationToString(i->Length() - mantra::duration(i->Creation(),
										  mantra::GetCurrentDateTime()), mantra::Second) %
						 DurationToString(i->Length(), mantra::Second) % i->Reason());
			else
				SEND(service, user, N_("     [PERMANENT] %1%"), i->Reason());
		}
	}

	MT_RET(true);
	MT_EE
}

static bool biAKILL_REINDEX(const ServiceUser *service,
							const boost::shared_ptr<LiveUser> &user,
							const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biAKILL_REINDEX" << service << user << params);

	std::set<Akill> ent;
	ROOT->data.Get_Akill(ent);
	if (ent.empty())
	{
		NSEND(service, user, N_("The autokill list is empty."));
		MT_RET(true);
	}

	size_t changed = 0, count = 1;
	std::set<Akill>::iterator i;
	for (i = ent.begin(); i != ent.end(); ++i, ++count)
	{
		if (i->Number() != count && ROOT->data.Reindex(*i, count))
			++changed;
	}

	SEND(service, user, N_("Reindexing of autokill list complete, \002%1%\017 entries changed."),
		 changed);

	MT_RET(true);
	MT_EE
}

static bool biOPERDENY_ADD(const ServiceUser *service,
						   const boost::shared_ptr<LiveUser> &user,
						   const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biOPERDENY_ADD" << service << user << params);

	boost::shared_ptr<StoredNick> nick = user->Stored();
	if (!nick)
	{
		SEND(service, user,
			 N_("Nickname %1% is not registered or you are not recognized as this nickname."), user->Name());
		MT_RET(false);
	}

	static boost::regex rx("^[^[:space:][:cntrl:]@]+@"
						   "([-[:alnum:]*?.]+|[[:xdigit:]*?:]+)+$");

	std::string reason = params[2];
	for (size_t i = 3; i < params.size(); ++i)
		reason += ' ' + params[i];

	if (boost::regex_match(params[1], rx))
	{
		std::set<OperDeny> ent;
		ROOT->data.Get_OperDeny(ent);

		OperDeny od;
		std::set<OperDeny>::iterator j;
		for (j = ent.begin(); j != ent.end(); ++j)
		{
			if (!*j)
				continue;

			if (mantra::glob_match(params[1], j->Mask(), true))
			{
				if (od.Number())
				{
					SEND(service, user, N_("Multiple oper denies match the mask %1%, please specify a more specific mask or an entry by number."),
						 params[1]);
					MT_RET(false);
				}
				od = *j;
			}
		}
		if (od)
		{
			od.Reason(reason, nick);
			SEND(service, user, N_("Mask %1% has had their oper deny reason changed."),
				 params[1]);
		}
		else
		{
			od = OperDeny::create(params[1], reason, nick);
			SEND(service, user, N_("OperDeny entry #%1% created for \002%2%\017."),
				 od.Number() % params[1]);
		}

	}
	else
	{
		try
		{
			boost::uint32_t entry = boost::lexical_cast<boost::uint32_t>(params[1]);
			OperDeny od = ROOT->data.Get_OperDeny(entry);
			if (od)
			{
				od.Reason(reason, nick);
				SEND(service, user, N_("Mask %1% has had their oper deny reason changed."),
					 params[1]);
			}
			else
			{
				SEND(service, user, N_("Could not find entry #%1% on oper deny list."),
					  entry);
				MT_RET(false);
			}
		}
		catch (boost::bad_lexical_cast &e)
		{
			SEND(service, user, N_("Invalid hostmask %1%."), params[1]);
			MT_RET(false);
		}
	}

	MT_RET(true);
	MT_EE
}

static bool biOPERDENY_DEL(const ServiceUser *service,
						   const boost::shared_ptr<LiveUser> &user,
						   const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biOPERDENY_DEL" << service << user << params);

	static boost::regex rx("^([^[:space:][:cntrl:]@]+@)?"
						   "([-[:alnum:]*?.]+|[[:xdigit:]*?:]+)+$");

	std::string numbers = params[1];
	for (size_t i = 2; i < params.size(); ++i)
		numbers += ' ' + params[i];

	size_t deleted = 0, notfound = 0;

	std::vector<unsigned int> nums;
	if (!mantra::ParseNumbers(numbers, nums))
	{
		std::set<OperDeny> ent;
		ROOT->data.Get_OperDeny(ent);

		for (size_t i = 1; i < params.size(); ++i)
		{
			if (boost::regex_match(params[i], rx))
			{
				bool found = false;
				std::set<OperDeny>::iterator j = ent.begin();
				while (j != ent.end())
				{
					if (!*j)
					{
						ent.erase(j++);
						continue;
					}

					if (mantra::glob_match(params[i], j->Mask(), true))
					{
						found = true;
						ROOT->data.Del(*j);
						++deleted;
						ent.erase(j++);
					}
					else
						++j;
				}
				if (!found)
					++notfound;
			}
			else
				++notfound;
		}
	}
	else
	{
		for (size_t j = 0; j < nums.size(); ++j)
		{
			OperDeny od = ROOT->data.Get_OperDeny(nums[j]);
			if (od)
			{
				ROOT->data.Del(od);
				++deleted;
			}
			else
				++notfound;
		}
	}

	if (!deleted)
	{
		NSEND(service, user, N_("No specified entries found on the oper deny list."));
	}
	else if (!notfound)
	{
		SEND(service, user, N_("%1% entries removed from the oper deny list."),
			 deleted);
	}
	else
	{
		SEND(service, user, N_("%1% entries removed (%2% not found) from the oper deny list."),
			 deleted % notfound);
	}

	MT_RET(true);
	MT_EE
}

static bool biOPERDENY_LIST(const ServiceUser *service,
							const boost::shared_ptr<LiveUser> &user,
							const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biOPERDENY_LIST" << service << user << params);

	static boost::regex rx("^([^[:space:][:cntrl:]@]+@)?"
						   "([-[:alnum:]*?.]+|[[:xdigit:]*?:]+)+$");

	std::string numbers = params[1];
	for (size_t i = 2; i < params.size(); ++i)
		numbers += ' ' + params[i];

	std::set<OperDeny> operdenies;
	if (params.size() > 1)
	{
		std::vector<unsigned int> nums;
		if (!mantra::ParseNumbers(numbers, nums))
		{
			std::set<OperDeny> ent;
			ROOT->data.Get_OperDeny(ent);
			if (ent.empty())
			{
				NSEND(service, user, N_("The oper deny list is empty."));
				MT_RET(false);
			}

			for (size_t i = 1; i < params.size(); ++i)
			{
				if (boost::regex_match(params[i], rx))
				{
					std::set<OperDeny>::iterator j;
					for (j = ent.begin(); j != ent.end(); ++j)
					{
						if (!*j)
							continue;

						if (mantra::glob_match(params[i], j->Mask(), true))
							operdenies.insert(*j);
					}
				}
			}
		}
		else
		{
			for (size_t j = 0; j < nums.size(); ++j)
			{
				OperDeny od = ROOT->data.Get_OperDeny(nums[j]);
				if (od)
					operdenies.insert(od);
			}
		}
	}
	else
	{
		ROOT->data.Get_OperDeny(operdenies);
		if (operdenies.empty())
		{
			NSEND(service, user, N_("The oper deny list is empty."));
			MT_RET(false);
		}
	}

	if (operdenies.empty())
	{
		NSEND(service, user, N_("No entries matching your specification on the oper deny list."));
		MT_RET(false);
	}
	else
	{
		std::set<OperDeny>::iterator i;
		NSEND(service, user, N_("     MASK"));
		for (i = operdenies.begin(); i != operdenies.end(); ++i)
		{
			if (!*i)
				continue;

			SEND(service, user, N_("%1$ 3d. %2$s (last modified by %3$s %4$s ago)"),
				 i->Number() % i->Mask() % i->Last_UpdaterName() %
				 DurationToString(mantra::duration(i->Last_Update(),
												   mantra::GetCurrentDateTime()), mantra::Second));
			SEND(service, user, N_("     %1%"), i->Reason());
		}
	}

	MT_RET(true);
	MT_EE
}

static bool biOPERDENY_REINDEX(const ServiceUser *service,
							   const boost::shared_ptr<LiveUser> &user,
							   const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biOPERDENY_REINDEX" << service << user << params);

	std::set<OperDeny> ent;
	ROOT->data.Get_OperDeny(ent);
	if (ent.empty())
	{
		NSEND(service, user, N_("The oper deny list is empty."));
		MT_RET(true);
	}

	size_t changed = 0, count = 1;
	std::set<OperDeny>::iterator i;
	for (i = ent.begin(); i != ent.end(); ++i, ++count)
	{
		if (i->Number() != count && ROOT->data.Reindex(*i, count))
			++changed;
	}

	SEND(service, user, N_("Reindexing of oper deny list complete, \002%1%\017 entries changed."),
		 changed);

	MT_RET(true);
	MT_EE
}

static bool biIGNORE_ADD(const ServiceUser *service,
						 const boost::shared_ptr<LiveUser> &user,
						 const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biIGNORE_ADD" << service << user << params);

	boost::shared_ptr<StoredNick> nick = user->Stored();
	if (!nick)
	{
		SEND(service, user,
			 N_("Nickname %1% is not registered or you are not recognized as this nickname."), user->Name());
		MT_RET(false);
	}

	static boost::regex rx("^[^[:space:][:cntrl:]@]+@"
						   "([-[:alnum:]*?.]+|[[:xdigit:]*?:]+)+$");

	std::string reason = params[2];
	for (size_t i = 3; i < params.size(); ++i)
		reason += ' ' + params[i];

	if (boost::regex_match(params[1], rx))
	{
		std::set<Ignore> ent;
		ROOT->data.Get_Ignore(ent);

		Ignore ign;
		std::set<Ignore>::iterator j;
		for (j = ent.begin(); j != ent.end(); ++j)
		{
			if (!*j)
				continue;

			if (mantra::glob_match(params[1], j->Mask(), true))
			{
				if (ign.Number())
				{
					SEND(service, user, N_("Multiple ignores match the mask %1%, please specify a more specific mask or an entry by number."),
						 params[1]);
					MT_RET(false);
				}
				ign = *j;
			}
		}
		if (ign)
		{
			ign.Reason(reason, nick);
			SEND(service, user, N_("Mask %1% has had their ignore reason changed."),
				 params[1]);
		}
		else
		{
			ign = Ignore::create(params[1], reason, nick);
			SEND(service, user, N_("Ignore entry #%1% created for \002%2%\017."),
				 ign.Number() % params[1]);
		}

	}
	else
	{
		try
		{
			boost::uint32_t entry = boost::lexical_cast<boost::uint32_t>(params[1]);
			Ignore ign = ROOT->data.Get_Ignore(entry);
			if (ign)
			{
				ign.Reason(reason, nick);
				SEND(service, user, N_("Mask %1% has had their ignore reason changed."),
					 params[1]);
			}
			else
			{
				SEND(service, user, N_("Could not find entry #%1% on ignore list."),
					  entry);
				MT_RET(false);
			}
		}
		catch (boost::bad_lexical_cast &e)
		{
			SEND(service, user, N_("Invalid hostmask %1%."), params[1]);
			MT_RET(false);
		}
	}

	MT_RET(true);
	MT_EE
}

static bool biIGNORE_DEL(const ServiceUser *service,
						 const boost::shared_ptr<LiveUser> &user,
						 const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biIGNORE_DEL" << service << user << params);

	static boost::regex rx("^([^[:space:][:cntrl:]@]+@)?"
						   "([-[:alnum:]*?.]+|[[:xdigit:]*?:]+)+$");

	std::string numbers = params[1];
	for (size_t i = 2; i < params.size(); ++i)
		numbers += ' ' + params[i];

	size_t deleted = 0, notfound = 0;

	std::vector<unsigned int> nums;
	if (!mantra::ParseNumbers(numbers, nums))
	{
		std::set<Ignore> ent;
		ROOT->data.Get_Ignore(ent);

		for (size_t i = 1; i < params.size(); ++i)
		{
			if (boost::regex_match(params[i], rx))
			{
				bool found = false;
				std::set<Ignore>::iterator j = ent.begin();
				while (j != ent.end())
				{
					if (!*j)
					{
						ent.erase(j++);
						continue;
					}

					if (mantra::glob_match(params[i], j->Mask(), true))
					{
						found = true;
						ROOT->data.Del(*j);
						++deleted;
						ent.erase(j++);
					}
					else
						++j;
				}
				if (!found)
					++notfound;
			}
			else
				++notfound;
		}
	}
	else
	{
		for (size_t j = 0; j < nums.size(); ++j)
		{
			Ignore ign = ROOT->data.Get_Ignore(nums[j]);
			if (ign)
			{
				ROOT->data.Del(ign);
				++deleted;
			}
			else
				++notfound;
		}
	}

	if (!deleted)
	{
		NSEND(service, user, N_("No specified entries found on the ignore list."));
	}
	else if (!notfound)
	{
		SEND(service, user, N_("%1% entries removed from the ignore list."),
			 deleted);
	}
	else
	{
		SEND(service, user, N_("%1% entries removed (%2% not found) from the ignore list."),
			 deleted % notfound);
	}

	MT_RET(true);
	MT_EE
}

static bool biIGNORE_LIST(const ServiceUser *service,
						  const boost::shared_ptr<LiveUser> &user,
						  const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biIGNORE_LIST" << service << user << params);

	static boost::regex rx("^([^[:space:][:cntrl:]@]+@)?"
						   "([-[:alnum:]*?.]+|[[:xdigit:]*?:]+)+$");

	std::string numbers = params[1];
	for (size_t i = 2; i < params.size(); ++i)
		numbers += ' ' + params[i];

	std::set<Ignore> ignores;
	if (params.size() > 1)
	{
		std::vector<unsigned int> nums;
		if (!mantra::ParseNumbers(numbers, nums))
		{
			std::set<Ignore> ent;
			ROOT->data.Get_Ignore(ent);
			if (ent.empty())
			{
				NSEND(service, user, N_("The ignore list is empty."));
				MT_RET(false);
			}

			for (size_t i = 1; i < params.size(); ++i)
			{
				if (boost::regex_match(params[i], rx))
				{
					std::set<Ignore>::iterator j;
					for (j = ent.begin(); j != ent.end(); ++j)
					{
						if (!*j)
							continue;

						if (mantra::glob_match(params[i], j->Mask(), true))
							ignores.insert(*j);
					}
				}
			}
		}
		else
		{
			for (size_t j = 0; j < nums.size(); ++j)
			{
				Ignore ign = ROOT->data.Get_Ignore(nums[j]);
				if (ign)
					ignores.insert(ign);
			}
		}
	}
	else
	{
		ROOT->data.Get_Ignore(ignores);
		if (ignores.empty())
		{
			NSEND(service, user, N_("The ignore list is empty."));
			MT_RET(false);
		}
	}

	if (ignores.empty())
	{
		NSEND(service, user, N_("No entries matching your specification on the ignore list."));
		MT_RET(false);
	}
	else
	{
		std::set<Ignore>::iterator i;
		NSEND(service, user, N_("     MASK"));
		for (i = ignores.begin(); i != ignores.end(); ++i)
		{
			if (!*i)
				continue;

			SEND(service, user, N_("%1$ 3d. %2$s (last modified by %3$s %4$s ago)"),
				 i->Number() % i->Mask() % i->Last_UpdaterName() %
				 DurationToString(mantra::duration(i->Last_Update(),
												   mantra::GetCurrentDateTime()), mantra::Second));
			SEND(service, user, N_("     %1%"), i->Reason());
		}
	}

	MT_RET(true);
	MT_EE
}

static bool biIGNORE_REINDEX(const ServiceUser *service,
							 const boost::shared_ptr<LiveUser> &user,
							 const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biIGNORE_REINDEX" << service << user << params);

	std::set<Ignore> ent;
	ROOT->data.Get_Ignore(ent);
	if (ent.empty())
	{
		NSEND(service, user, N_("The ignore list is empty."));
		MT_RET(true);
	}

	size_t changed = 0, count = 1;
	std::set<Ignore>::iterator i;
	for (i = ent.begin(); i != ent.end(); ++i, ++count)
	{
		if (i->Number() != count && ROOT->data.Reindex(*i, count))
			++changed;
	}

	SEND(service, user, N_("Reindexing of ignore list complete, \002%1%\017 entries changed."),
		 changed);

	MT_RET(true);
	MT_EE
}

static bool biIGNORE_LIVELIST(const ServiceUser *service,
					const boost::shared_ptr<LiveUser> &user,
					const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biIGNORE_LIVELIST" << service << user << params);

	// TODO: To be implemented.
	SEND(service, user,
		 N_("The %1% command has not yet been implemented."),
		 boost::algorithm::to_upper_copy(params[0]));

	MT_RET(true);
	MT_EE
}

static bool biKILLCHAN_ADD(const ServiceUser *service,
						   const boost::shared_ptr<LiveUser> &user,
						   const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biKILLCHAN_ADD" << service << user << params);

	boost::shared_ptr<StoredNick> nick = user->Stored();
	if (!nick)
	{
		SEND(service, user,
			 N_("Nickname %1% is not registered or you are not recognized as this nickname."), user->Name());
		MT_RET(false);
	}

	std::string reason = params[2];
	for (size_t i = 3; i < params.size(); ++i)
		reason += ' ' + params[i];

	if (ROOT->proto.IsChannel(params[1]))
	{
		std::set<KillChannel> ent;
		ROOT->data.Get_KillChannel(ent);

		KillChannel kc;
		std::set<KillChannel>::iterator j;
		for (j = ent.begin(); j != ent.end(); ++j)
		{
			if (!*j)
				continue;

			if (mantra::glob_match(params[1], j->Mask(), true))
			{
				if (kc.Number())
				{
					SEND(service, user, N_("Multiple kill channels match the mask %1%, please specify a more specific mask or an entry by number."),
						 params[1]);
					MT_RET(false);
				}
				kc = *j;
			}
		}
		if (kc)
		{
			kc.Reason(reason, nick);
			SEND(service, user, N_("Mask %1% has had their kill channel reason changed."),
				 params[1]);
		}
		else
		{
			kc = KillChannel::create(params[1], reason, nick);
			SEND(service, user, N_("KillChannel entry #%1% created for \002%2%\017."),
				 kc.Number() % params[1]);
		}

	}
	else
	{
		try
		{
			boost::uint32_t entry = boost::lexical_cast<boost::uint32_t>(params[1]);
			KillChannel kc = ROOT->data.Get_KillChannel(entry);
			if (kc)
			{
				kc.Reason(reason, nick);
				SEND(service, user, N_("Mask %1% has had their kill channel reason changed."),
					 params[1]);
			}
			else
			{
				SEND(service, user, N_("Could not find entry #%1% on kill channel list."),
					  entry);
				MT_RET(false);
			}
		}
		catch (boost::bad_lexical_cast &e)
		{
			SEND(service, user, N_("Invalid hostmask %1%."), params[1]);
			MT_RET(false);
		}
	}

	MT_RET(true);
	MT_EE
}

static bool biKILLCHAN_DEL(const ServiceUser *service,
						   const boost::shared_ptr<LiveUser> &user,
						   const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biKILLCHAN_DEL" << service << user << params);

	std::string numbers = params[1];
	for (size_t i = 2; i < params.size(); ++i)
		numbers += ' ' + params[i];

	size_t deleted = 0, notfound = 0;

	std::vector<unsigned int> nums;
	if (!mantra::ParseNumbers(numbers, nums))
	{
		std::set<KillChannel> ent;
		ROOT->data.Get_KillChannel(ent);

		for (size_t i = 1; i < params.size(); ++i)
		{
			if (ROOT->proto.IsChannel(params[i]))
			{
				bool found = false;
				std::set<KillChannel>::iterator j = ent.begin();
				while (j != ent.end())
				{
					if (!*j)
					{
						ent.erase(j++);
						continue;
					}

					if (mantra::glob_match(params[i], j->Mask(), true))
					{
						found = true;
						ROOT->data.Del(*j);
						++deleted;
						ent.erase(j++);
					}
					else
						++j;
				}
				if (!found)
					++notfound;
			}
			else
				++notfound;
		}
	}
	else
	{
		for (size_t j = 0; j < nums.size(); ++j)
		{
			KillChannel kc = ROOT->data.Get_KillChannel(nums[j]);
			if (kc)
			{
				ROOT->data.Del(kc);
				++deleted;
			}
			else
				++notfound;
		}
	}

	if (!deleted)
	{
		NSEND(service, user, N_("No specified entries found on the kill channel list."));
	}
	else if (!notfound)
	{
		SEND(service, user, N_("%1% entries removed from the kill channel list."),
			 deleted);
	}
	else
	{
		SEND(service, user, N_("%1% entries removed (%2% not found) from the kill channel list."),
			 deleted % notfound);
	}

	MT_RET(true);
	MT_EE
}

static bool biKILLCHAN_LIST(const ServiceUser *service,
							const boost::shared_ptr<LiveUser> &user,
							const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biKILLCHAN_LIST" << service << user << params);

	std::string numbers = params[1];
	for (size_t i = 2; i < params.size(); ++i)
		numbers += ' ' + params[i];

	std::set<KillChannel> killchans;
	if (params.size() > 1)
	{
		std::vector<unsigned int> nums;
		if (!mantra::ParseNumbers(numbers, nums))
		{
			std::set<KillChannel> ent;
			ROOT->data.Get_KillChannel(ent);
			if (ent.empty())
			{
				NSEND(service, user, N_("The kill channel list is empty."));
				MT_RET(false);
			}

			for (size_t i = 1; i < params.size(); ++i)
			{
				if (ROOT->proto.IsChannel(params[i]))
				{
					std::set<KillChannel>::iterator j;
					for (j = ent.begin(); j != ent.end(); ++j)
					{
						if (!*j)
							continue;

						if (mantra::glob_match(params[i], j->Mask(), true))
							killchans.insert(*j);
					}
				}
			}
		}
		else
		{
			for (size_t j = 0; j < nums.size(); ++j)
			{
				KillChannel kc = ROOT->data.Get_KillChannel(nums[j]);
				if (kc)
					killchans.insert(kc);
			}
		}
	}
	else
	{
		ROOT->data.Get_KillChannel(killchans);
		if (killchans.empty())
		{
			NSEND(service, user, N_("The kill channel list is empty."));
			MT_RET(false);
		}
	}

	if (killchans.empty())
	{
		NSEND(service, user, N_("No entries matching your specification on the kill channel list."));
		MT_RET(false);
	}
	else
	{
		std::set<KillChannel>::iterator i;
		NSEND(service, user, N_("     MASK"));
		for (i = killchans.begin(); i != killchans.end(); ++i)
		{
			if (!*i)
				continue;

			SEND(service, user, N_("%1$ 3d. %2$s (last modified by %3$s %4$s ago)"),
				 i->Number() % i->Mask() % i->Last_UpdaterName() %
				 DurationToString(mantra::duration(i->Last_Update(),
												   mantra::GetCurrentDateTime()), mantra::Second));
			SEND(service, user, N_("     %1%"), i->Reason());
		}
	}

	MT_RET(true);
	MT_EE
}

static bool biKILLCHAN_REINDEX(const ServiceUser *service,
							   const boost::shared_ptr<LiveUser> &user,
							   const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biKILLCHAN_REINDEX" << service << user << params);

	std::set<KillChannel> ent;
	ROOT->data.Get_KillChannel(ent);
	if (ent.empty())
	{
		NSEND(service, user, N_("The kill channel list is empty."));
		MT_RET(true);
	}

	size_t changed = 0, count = 1;
	std::set<KillChannel>::iterator i;
	for (i = ent.begin(); i != ent.end(); ++i, ++count)
	{
		if (i->Number() != count && ROOT->data.Reindex(*i, count))
			++changed;
	}

	SEND(service, user, N_("Reindexing of kill channel list complete, \002%1%\017 entries changed."),
		 changed);

	MT_RET(true);
	MT_EE
}

void init_operserv_functions(Service &serv)
{
	MT_EB
	MT_FUNC("init_operserv_functions" << serv);

    std::vector<std::string> comm_sadmin, comm_admin, comm_oper, comm_sop, comm_opersop;
    comm_oper.push_back(ROOT->ConfigValue<std::string>("commserv.oper.name"));
    comm_sop.push_back(ROOT->ConfigValue<std::string>("commserv.sop.name"));
    comm_opersop.push_back(ROOT->ConfigValue<std::string>("commserv.oper.name"));
    comm_opersop.push_back(ROOT->ConfigValue<std::string>("commserv.sop.name"));
    comm_admin.push_back(ROOT->ConfigValue<std::string>("commserv.admin.name"));
    comm_sadmin.push_back(ROOT->ConfigValue<std::string>("commserv.sadmin.name"));

	serv.PushCommand("^HELP$", boost::bind(&Service::Help, &serv,
										   _1, _2, _3));

	// Do I want: MODE, SIGNON, UNLOAD ??

	// Aliases for NickServ commands.
//	serv.PushCommand("^REGISTER$", &biREGISTER, 1);
//	serv.PushCommand("^ID(ENT(IFY)?)?$", &biIDENTIFY, 1);

	serv.PushCommand("^(JUPE|JUPITER)$", &biJUPE, 2, comm_admin);
	serv.PushCommand("^(FORCE)?PING$", &biPING, 1, comm_admin);
	serv.PushCommand("^SAVE$", &biSAVE, 0, comm_sadmin);
	serv.PushCommand("^ON$", &biON, 1, comm_sadmin);
	serv.PushCommand("^OFF$", &biOFF, 1, comm_sadmin);
	serv.PushCommand("^RE?START$", &biRESTART, 1, comm_sadmin);
	serv.PushCommand("^(SHUTDOWN|DIE)$", &biSHUTDOWN, 1, comm_sadmin);

	serv.PushCommand("^HTM$",
					 Service::CommandMerge(serv, 0, 1), 1, comm_sadmin);
	serv.PushCommand("^HTM\\s+ON$", &biHTM_ON, 0, comm_sadmin);
	serv.PushCommand("^HTM\\s+OFF$", &biHTM_OFF, 0, comm_sadmin);
	serv.PushCommand("^HTM\\s+HELP$",
					 boost::bind(&Service::AuxHelp, &serv,
								 _1, _2, _3), 0, comm_sadmin);

	serv.PushCommand("^RELOAD$",
					 Service::CommandMerge(serv, 0, 1), 1, comm_sadmin);
	serv.PushCommand("^RELOAD\\s+(CFG|CONFIG)$",
					 &biRELOAD_CONFIG, 0, comm_sadmin);
	serv.PushCommand("^RELOAD\\s+(STORAGE|DATABASES?)$",
					 &biRELOAD_STORAGE, 0, comm_sadmin);
	serv.PushCommand("^RELOAD\\s+HELP$",
					 boost::bind(&Service::AuxHelp, &serv,
								 _1, _2, _3), 0, comm_sadmin);

	serv.PushCommand("^KILL$",
					 &biKILL, 2, comm_admin);
	serv.PushCommand("^ANON(YMOUS)?KILL$",
					 &biANONKILL, 2, comm_admin);

	// These depend on server support
	serv.PushCommand("^(QUARENTINE|QLINE)$",
					 &biQLINE, 1, comm_admin);
	serv.PushCommand("^(UN?QUARENTINE|UN?QLINE)$",
					 &biUNQLINE, 1, comm_admin);
	serv.PushCommand("^NO?OP(ER(ATORS?)?)?$",
					 &biNOOP, 1, comm_admin);
	serv.PushCommand("^HIDE(HOST)?$",
					 &biHIDE, 1, comm_admin);

#ifdef MANTRA_TRACING
	serv.PushCommand("^TRACE$",
					 Service::CommandMerge(serv, 0, 1), 1, comm_sadmin);
	serv.PushCommand("^TRACE\\s+(SET|TO)$",
					 &biTRACE_SET, 2, comm_sadmin);
	serv.PushCommand("^TRACE\\s+(TURN)?UP$",
					 &biTRACE_UP, 2, comm_sadmin);
	serv.PushCommand("^TRACE\\s+(TURN)?DOWN$",
					 &biTRACE_DOWN, 2, comm_sadmin);
	serv.PushCommand("^TRACE\\s+(LIST|VIEW)$",
					 &biTRACE_LIST, 0, comm_sadmin);
	serv.PushCommand("^TRACE\\s+HELP$",
					 boost::bind(&Service::AuxHelp, &serv,
								 _1, _2, _3), 0, comm_sadmin);
#endif


	serv.PushCommand("^CLONES?$",
					 Service::CommandMerge(serv, 0, 1), 1, comm_opersop);
	serv.PushCommand("^CLONES?\\s+(ADD|NEW|CREATE)$",
					 &biCLONE_ADD, 3, comm_sop);
	serv.PushCommand("^CLONES?\\s+(ERASE|DEL(ETE)?|REM(OVE)?)$",
					 &biCLONE_DEL, 1, comm_sop);
	serv.PushCommand("^CLONES?\\s+(LIST|VIEW)$",
					 &biCLONE_LIST, 0, comm_opersop);
	serv.PushCommand("^CLONES?\\s+RE(NUMBER|INDEX)$",
					 &biCLONE_REINDEX, 0, comm_sop);
	serv.PushCommand("^CLONES?\\s+LIVE(LIST)?$",
					 &biCLONE_LIVELIST, 0, comm_opersop);
	serv.PushCommand("^CLONES?\\s+HELP$",
					 boost::bind(&Service::AuxHelp, &serv,
								 _1, _2, _3), 0, comm_opersop);

	serv.PushCommand("^A(UTO)?KILL$",
					 Service::CommandMerge(serv, 0, 1), 1, comm_opersop);
	serv.PushCommand("^A(UTO)?KILL\\s+(ADD|NEW|CREATE)$",
					 &biAKILL_ADD, 3, comm_opersop);
	serv.PushCommand("^A(UTO)?KILL\\s+(ERASE|DEL(ETE)?|REM(OVE)?)$",
					 &biAKILL_DEL, 1, comm_opersop);
	serv.PushCommand("^A(UTO)?KILL\\s+(LIST|VIEW)$",
					 &biAKILL_LIST, 0, comm_opersop);
	serv.PushCommand("^A(UTO)?KILL\\s+RE(NUMBER|INDEX)$",
					 &biAKILL_REINDEX, 0, comm_sop);
	serv.PushCommand("^A(UTO)?KILL\\s+HELP$",
					 boost::bind(&Service::AuxHelp, &serv,
								 _1, _2, _3), 0, comm_opersop);

	serv.PushCommand("^OPERDENY$",
					 Service::CommandMerge(serv, 0, 1), 1, comm_sop);
	serv.PushCommand("^OPERDENY\\s+(ADD|NEW|CREATE)$",
					 &biOPERDENY_ADD, 3, comm_sadmin);
	serv.PushCommand("^OPERDENY\\s+(ERASE|DEL(ETE)?|REM(OVE)?)$",
					 &biOPERDENY_DEL, 1, comm_sadmin);
	serv.PushCommand("^OPERDENY\\s+(LIST|VIEW)$",
					 &biOPERDENY_LIST, 0, comm_sop);
	serv.PushCommand("^OPERENY\\s+RE(NUMBER|INDEX)$",
					 &biOPERDENY_REINDEX, 0, comm_sadmin);
	serv.PushCommand("^OPERDENY\\s+HELP$",
					 boost::bind(&Service::AuxHelp, &serv,
								 _1, _2, _3), 0, comm_sop);

	serv.PushCommand("^IGNORE$",
					 Service::CommandMerge(serv, 0, 1), 1, comm_opersop);
	serv.PushCommand("^IGNORE\\s+(ADD|NEW|CREATE)$",
					 &biIGNORE_ADD, 3, comm_sop);
	serv.PushCommand("^IGNORE\\s+(ERASE|DEL(ETE)?|REM(OVE)?)$",
					 &biIGNORE_DEL, 1, comm_sop);
	serv.PushCommand("^IGNORE\\s+(LIST|VIEW)$",
					 &biIGNORE_LIST, 0, comm_opersop);
	serv.PushCommand("^IGNORE\\s+RE(NUMBER|INDEX)$",
					 &biIGNORE_REINDEX, 0, comm_sop);
	serv.PushCommand("^IGNORE\\s+LIVE(LIST)?$",
					 &biIGNORE_LIVELIST, 0, comm_sop);
	serv.PushCommand("^IGNORE\\s+HELP$",
					 boost::bind(&Service::AuxHelp, &serv,
								 _1, _2, _3), 0, comm_opersop);

	serv.PushCommand("^KILLCHAN(NEL)?$",
					 Service::CommandMerge(serv, 0, 1), 1, comm_opersop);
	serv.PushCommand("^KILLCHAN(NEL)?\\s+(ADD|NEW|CREATE)$",
					 &biKILLCHAN_ADD, 3, comm_sop);
	serv.PushCommand("^KILLCHAN(NEL)?\\s+(ERASE|DEL(ETE)?|REM(OVE)?)$",
					 &biKILLCHAN_DEL, 1, comm_sop);
	serv.PushCommand("^KILLCHAN(NEL)?\\s+(LIST|VIEW)$",
					 &biKILLCHAN_LIST, 0, comm_opersop);
	serv.PushCommand("^KILLCHAN(NEL)?\\s+RE(NUMBER|INDEX)$",
					 &biKILLCHAN_REINDEX, 0, comm_sop);
	serv.PushCommand("^KILLCHAN(NEL)?\\s+HELP$",
					 boost::bind(&Service::AuxHelp, &serv,
								 _1, _2, _3), 0, comm_opersop);

	MT_EE
}

