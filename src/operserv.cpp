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
		value = ((unsigned char) svalue[0] << 8) + (unsigned char) svalue[1];
	}
	else
	{
		value = (unsigned char) svalue[0];
	}

	static mantra::iequal_to<std::string> cmp;
	if (cmp(params[1], "ALL"))
	{
		for (size_t j = 0; j < (size_t) MAGICK_TRACE_SIZE; ++j)
			mantra::mtrace::instance().TurnSet(j, value);
		SEND(service, user, N_("All trace types have been set to %1$#04x."), value);
	}
	else
	{
		size_t j;
		for (j = 0; j < (size_t) MAGICK_TRACE_SIZE; ++j)
			if (boost::regex_match(params[1], TraceTypeRegex[j]))
			{
				mantra::mtrace::instance().TurnSet(j, value);
				SEND(service, user, N_("Trace for %1$s has been set to %2$#04x."),
					 TraceTypes[j] % value);
				break;
			}
		if (j >= (size_t) MAGICK_TRACE_SIZE)
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
				value |= ((unsigned char) svalue[0] << 8) + (unsigned char) svalue[1];
			}
			else
			{
				value |= (unsigned char) svalue[0];
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
		for (size_t j = 0; j < (size_t) MAGICK_TRACE_SIZE; ++j)
			mantra::mtrace::instance().TurnSet(j,
					   mantra::mtrace::instance().TraceTypeMask(j) | value);
		SEND(service, user, N_("All trace types have been turned up by %1$#04x."), value);
	}
	else
	{
		size_t j;
		for (j = 0; j < (size_t) MAGICK_TRACE_SIZE; ++j)
			if (boost::regex_match(params[1], TraceTypeRegex[j]))
			{
				mantra::mtrace::instance().TurnSet(j,
						   mantra::mtrace::instance().TraceTypeMask(j) | value);
				SEND(service, user, N_("Trace for %1$s has been turned up by %2$#04x."),
					 TraceTypes[j] % value);
				break;
			}
		if (j >= (size_t) MAGICK_TRACE_SIZE)
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
				value |= ((unsigned char) svalue[0] << 8) + (unsigned char) svalue[1];
			}
			else
			{
				value |= (unsigned char) svalue[0];
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
		for (size_t j = 0; j < (size_t) MAGICK_TRACE_SIZE; ++j)
			mantra::mtrace::instance().TurnSet(j,
					   mantra::mtrace::instance().TraceTypeMask(j) & ~value);
		SEND(service, user, N_("All trace types have been turned down by %1$#04x."), value);
	}
	else
	{
		size_t j;
		for (j = 0; j < (size_t) MAGICK_TRACE_SIZE; ++j)
			if (boost::regex_match(params[1], TraceTypeRegex[j]))
			{
				mantra::mtrace::instance().TurnSet(j,
						   mantra::mtrace::instance().TraceTypeMask(j) & ~value);
				SEND(service, user, N_("Trace for %1$s has been turned down by %2$#04x."),
					 TraceTypes[j] % value);
				break;
			}
		if (j >= (size_t) MAGICK_TRACE_SIZE)
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

	for (size_t j = 0; j < (size_t) MAGICK_TRACE_SIZE; ++j)
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
						   "([-[:alnum:]*?.]+|[[:xdigit:]*?:]+)$");
	if (!boost::regex_match(params[1], rx))
	{
		SEND(service, user, N_("Invalid hostmask %1%."), params[1]);
		MT_RET(false);
	}

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

	Clone c = Clone::create(params[1], reason, limit, nick);
	SEND(service, user, N_("Clone entry #%1% created for \002%2%\017 with a limit of %3%."),
		 c.Number() % params[1] % limit);

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
						   "([-[:alnum:]*?.]+|[[:xdigit:]*?:]+)$");

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
						   "([-[:alnum:]*?.]+|[[:xdigit:]*?:]+)$");

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

static bool biAKILL_ADD(const ServiceUser *service,
						const boost::shared_ptr<LiveUser> &user,
						const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biAKILL_ADD" << service << user << params);


	MT_RET(false);
	MT_EE
}

static bool biAKILL_DEL(const ServiceUser *service,
						const boost::shared_ptr<LiveUser> &user,
						const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biAKILL_DEL" << service << user << params);


	MT_RET(false);
	MT_EE
}

static bool biAKILL_LIST(const ServiceUser *service,
						 const boost::shared_ptr<LiveUser> &user,
						 const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biAKILL_LIST" << service << user << params);


	MT_RET(false);
	MT_EE
}

static bool biAKILL_REINDEX(const ServiceUser *service,
							const boost::shared_ptr<LiveUser> &user,
							const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biAKILL_REINDEX" << service << user << params);


	MT_RET(false);
	MT_EE
}

static bool biOPERDENY_ADD(const ServiceUser *service,
						   const boost::shared_ptr<LiveUser> &user,
						   const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biOPERDENY_ADD" << service << user << params);


	MT_RET(false);
	MT_EE
}

static bool biOPERDENY_DEL(const ServiceUser *service,
						   const boost::shared_ptr<LiveUser> &user,
						   const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biOPERDENY_DEL" << service << user << params);


	MT_RET(false);
	MT_EE
}

static bool biOPERDENY_LIST(const ServiceUser *service,
							const boost::shared_ptr<LiveUser> &user,
							const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biOPERDENY_LIST" << service << user << params);


	MT_RET(false);
	MT_EE
}

static bool biOPERDENY_REINDEX(const ServiceUser *service,
							   const boost::shared_ptr<LiveUser> &user,
							   const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biOPERDENY_REINDEX" << service << user << params);


	MT_RET(false);
	MT_EE
}

static bool biIGNORE_ADD(const ServiceUser *service,
						 const boost::shared_ptr<LiveUser> &user,
						 const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biIGNORE_ADD" << service << user << params);


	MT_RET(false);
	MT_EE
}

static bool biIGNORE_DEL(const ServiceUser *service,
						 const boost::shared_ptr<LiveUser> &user,
						 const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biIGNORE_DEL" << service << user << params);


	MT_RET(false);
	MT_EE
}

static bool biIGNORE_LIST(const ServiceUser *service,
						  const boost::shared_ptr<LiveUser> &user,
						  const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biIGNORE_LIST" << service << user << params);


	MT_RET(false);
	MT_EE
}

static bool biIGNORE_REINDEX(const ServiceUser *service,
							 const boost::shared_ptr<LiveUser> &user,
							 const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biIGNORE_REINDEX" << service << user << params);


	MT_RET(false);
	MT_EE
}

static bool biKILLCHAN_ADD(const ServiceUser *service,
						   const boost::shared_ptr<LiveUser> &user,
						   const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biKILLCHAN_ADD" << service << user << params);


	MT_RET(false);
	MT_EE
}

static bool biKILLCHAN_DEL(const ServiceUser *service,
						   const boost::shared_ptr<LiveUser> &user,
						   const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biKILLCHAN_DEL" << service << user << params);


	MT_RET(false);
	MT_EE
}

static bool biKILLCHAN_LIST(const ServiceUser *service,
							const boost::shared_ptr<LiveUser> &user,
							const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biKILLCHAN_LIST" << service << user << params);


	MT_RET(false);
	MT_EE
}

static bool biKILLCHAN_REINDEX(const ServiceUser *service,
							   const boost::shared_ptr<LiveUser> &user,
							   const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biKILLCHAN_REINDEX" << service << user << params);


	MT_RET(false);
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
	serv.PushCommand("^OPERENY\\s+RE(NUMBER|INDEX)$",
					 &biIGNORE_REINDEX, 0, comm_sop);
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
	serv.PushCommand("^OPERENY\\s+RE(NUMBER|INDEX)$",
					 &biKILLCHAN_REINDEX, 0, comm_sop);
	serv.PushCommand("^KILLCHAN(NEL)?\\s+HELP$",
					 boost::bind(&Service::AuxHelp, &serv,
								 _1, _2, _3), 0, comm_opersop);

	MT_EE
}

