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
#define RCSID(x,y) const char *rcsid_magick__magick_cpp_ ## x () { return y; }
RCSID(magick__magick_cpp, "@(#)$Id$");

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

Magick::Magick()
	: start(boost::posix_time::second_clock::local_time()),
	  pid(getpid()), level(0),
	  opt_command_line_only("Available Options"),
	  opt_common("Config File Overrides"),
	  opt_config_file_only("Config File Only Options"),
	  logger_lock(boost::read_write_scheduling_policy::reader_priority),
	  disconnect(false), shutdown(false), event(NULL),
	  nickserv(MAGICK_TRACE_NICKSERV), chanserv(MAGICK_TRACE_CHANSERV),
	  memoserv(MAGICK_TRACE_MEMOSERV), commserv(MAGICK_TRACE_COMMSERV),
	  operserv(MAGICK_TRACE_OPERSERV), other(MAGICK_TRACE_OTHER)
{
	init_config();
}

Magick::~Magick()
{
}

void Magick::Log(mantra::LogLevel::Level_t level, const boost::format &out)
{
	MT_EB
	MT_FUNC("Magick::Log" << level << out);

	boost::read_write_mutex::scoped_read_lock lock(logger_lock);
	if (loggers.empty())
		std::cerr << out.str() << std::endl;
	else
	{
		std::list<mantra::Logger<char> *>::iterator iter;
		for (iter=loggers.begin(); iter!=loggers.end(); ++iter)
			
			(*iter)->Write(level, out);
	}

	MT_EE
}

void Magick::LogHex(mantra::LogLevel::Level_t level, const void *buf,
					size_t len, const std::string &prefix)
{
	MT_EB
	MT_FUNC("Magick::LogHex" << level << buf << len << prefix);

	boost::read_write_mutex::scoped_read_lock lock(logger_lock);
	if (loggers.empty())
	{
		std::vector<std::string> out;
		mantra::hexdump(buf, len, out);
		for (size_t i=0; i<out.size(); ++i)
			std::cerr << out[i] << std::endl;
	}
	else
	{
		std::list<mantra::Logger<char> *>::iterator iter;
		for (iter=loggers.begin(); iter!=loggers.end(); ++iter)
			(*iter)->WriteHex(level, buf, len, prefix);
	}

	MT_EE
}

void Magick::run(const boost::function0<bool> &check)
{
	MT_EB
	MT_FUNC("Magick::run" << check);

	init_nickserv_functions(nickserv);
    init_chanserv_functions(chanserv);
//    init_memoserv_functions(memoserv);
    init_commserv_functions(commserv);
    init_operserv_functions(operserv);
    init_other_functions(other);

	size_t next_conn = 0;
	do
	{
		std::vector<remote_connection> all_remote = opt_config["remote"].as<std::vector<remote_connection> >();
		if (next_conn >= all_remote.size())
			next_conn = 0;

		uplink = Uplink::create(all_remote[next_conn], check);
		if (uplink)
		{
			proto.Connect(*uplink);
			bool rv = (*uplink)(check);
			uplink->Disconnect();
			uplink.reset();
			if (rv)
				break;
		}

		++next_conn;
		MT_FLUSH();
		boost::posix_time::ptime now = mantra::GetCurrentDateTime();
		boost::posix_time::ptime expire = now +
				opt_config["general.server-relink"].as<mantra::duration>();
		while (now < expire && check())
		{
			sleep(1);
			now = mantra::GetCurrentDateTime();
		}
	}
	while (check());

	MT_EE
}
