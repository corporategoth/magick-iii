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

#include <mantra/core/trace.h>

static bool biJUPE(const ServiceUser *service,
				   const boost::shared_ptr<LiveUser> &user,
				   const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biJUPE" << service << user << params);

	boost::shared_ptr<Uplink> uplink = ROOT->uplink;
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
	ROOT->proto.Connect(*jupe);

	MT_RET(false);
	MT_EE
}

static bool biPING(const ServiceUser *service,
				   const boost::shared_ptr<LiveUser> &user,
				   const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biPING" << service << user << params);


	MT_RET(false);
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


	MT_RET(false);
	MT_EE
}

static bool biOFF(const ServiceUser *service,
				  const boost::shared_ptr<LiveUser> &user,
				  const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biOFF" << service << user << params);


	MT_RET(false);
	MT_EE
}

static bool biRESTART(const ServiceUser *service,
					  const boost::shared_ptr<LiveUser> &user,
					  const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biRESTART" << service << user << params);


	MT_RET(false);
	MT_EE
}

static bool biSHUTDOWN(const ServiceUser *service,
					   const boost::shared_ptr<LiveUser> &user,
					   const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biSHUTDOWN" << service << user << params);


	MT_RET(false);
	MT_EE
}

static bool biHTM_ON(const ServiceUser *service,
					 const boost::shared_ptr<LiveUser> &user,
					 const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biHTM_ON" << service << user << params);


	MT_RET(false);
	MT_EE
}

static bool biHTM_OFF(const ServiceUser *service,
					  const boost::shared_ptr<LiveUser> &user,
					  const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biHTM_OFF" << service << user << params);


	MT_RET(false);
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


	MT_RET(false);
	MT_EE
}

static bool biKILL(const ServiceUser *service,
				   const boost::shared_ptr<LiveUser> &user,
				   const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biKILL" << service << user << params);


	MT_RET(false);
	MT_EE
}

static bool biANONKILL(const ServiceUser *service,
					   const boost::shared_ptr<LiveUser> &user,
					   const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biANONKILL" << service << user << params);


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

static bool biTRACE_SET(const ServiceUser *service,
						const boost::shared_ptr<LiveUser> &user,
						const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biTRACE_SET" << service << user << params);


	MT_RET(false);
	MT_EE
}

static bool biTRACE_UP(const ServiceUser *service,
					   const boost::shared_ptr<LiveUser> &user,
					   const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biTRACE_UP" << service << user << params);


	MT_RET(false);
	MT_EE
}

static bool biTRACE_DOWN(const ServiceUser *service,
						 const boost::shared_ptr<LiveUser> &user,
						 const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biTRACE_DOWN" << service << user << params);


	MT_RET(false);
	MT_EE
}

static bool biTRACE_LIST(const ServiceUser *service,
						 const boost::shared_ptr<LiveUser> &user,
						 const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biTRACE_LIST" << service << user << params);


	MT_RET(false);
	MT_EE
}

#endif // MANTRA_TRACING

static bool biCLONE_ADD(const ServiceUser *service,
						const boost::shared_ptr<LiveUser> &user,
						const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biCLONE_ADD" << service << user << params);


	MT_RET(false);
	MT_EE
}

static bool biCLONE_DEL(const ServiceUser *service,
						const boost::shared_ptr<LiveUser> &user,
						const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biCLONE_DEL" << service << user << params);


	MT_RET(false);
	MT_EE
}

static bool biCLONE_LIST(const ServiceUser *service,
						 const boost::shared_ptr<LiveUser> &user,
						 const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biCLONE_LIST" << service << user << params);


	MT_RET(false);
	MT_EE
}

static bool biCLONE_REINDEX(const ServiceUser *service,
							const boost::shared_ptr<LiveUser> &user,
							const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("biCLONE_REINDEX" << service << user << params);


	MT_RET(false);
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
	serv.PushCommand("^ON$", &biON, 0, comm_sadmin);
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
	serv.PushCommand("^TRACE$\\s+(SET|TO)$",
					 &biTRACE_SET, 2, comm_sadmin);
	serv.PushCommand("^TRACE$\\s+(TURN)?UP$",
					 &biTRACE_UP, 2, comm_sadmin);
	serv.PushCommand("^TRACE$\\s+(TURN)?DOWN$",
					 &biTRACE_DOWN, 2, comm_sadmin);
	serv.PushCommand("^TRACE$\\s+(LIST|VIEW)$",
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

