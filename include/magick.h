#ifndef WIN32
#pragma interface
#endif

/* Magick IRC Services
**
** (c) 2004 The Neuromancy Society <info@neuromancy.net>
**
** The above copywright may not be removed under any circumstances,
** however it may be added to if any modifications are made to this
** file.  All modified code must be clearly documented and labelled.
**
** This code is released under the GNU General Public License v2.0 or better.
** The full text of this license should be contained in a file called
** COPYING distributed with this code.  If this file does not exist,
** it may be viewed here: http://www.neuromancy.net/license/gpl.html
**
** ======================================================================= */
#ifndef _MAGICK_MAGICK_H
#define _MAGICK_MAGICK_H 1

#ifdef RCSID
RCSID(magick__magick_h, "@(#) $Id$");
#endif // RCSID
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

#include "config.h"
#include "server.h"
#include "service.h"
#include "protocol.h"
#include "storage.h"

#include <istream>

#include <mantra/core/utils.h>
#include <mantra/core/translate.h>
#include <mantra/service/flowcontrol.h>
#include <mantra/service/events.h>
#include <mantra/log/interface.h>

#include <boost/program_options.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/read_write_mutex.hpp>

inline boost::format format(const std::string &in)
{
	boost::format fmt(in);
	fmt.exceptions(fmt.exceptions() ^ boost::io::too_many_args_bit);
	return fmt;
}

#define LOG(x,y,z) do { \
		if (ROOT) \
			ROOT->Log((mantra::LogLevel::Level_t) (mantra::LogLevel::x), format(y) % z); \
		else \
			std::cerr << (format(y) % z).str() << std::endl; \
	} while (0)

#define NLOG(x,y) do { \
		if (ROOT) \
			ROOT->Log((mantra::LogLevel::Level_t) (mantra::LogLevel::x), boost::format("%1%") % (y)); \
		else \
			std::cerr << (y) << std::endl; \
	} while (0)

#define LOGHEX(x,y,z) do { \
		if (ROOT) \
			ROOT->LogHex(mantra::LogLevel::x, (y), (z)); \
		else \
		{ \
			std::vector<std::string> out; \
			mantra::hexdump((y), (z), out); \
			for (size_t i=0; i<out.size(); ++i) \
				std::cerr << out[i] << std::endl; \
		} \
	} while (0)

#define PLOGHEX(x,y,z,p) do { \
		if (ROOT) \
			ROOT->LogHex(mantra::LogLevel::x, (y), (z), (p)); \
		else \
		{ \
			std::vector<std::string> out; \
			mantra::hexdump((y), (z), out); \
			for (size_t i=0; i<out.size(); ++i) \
				std::cerr << (p) << out[i] << std::endl; \
		} \
	} while (0)

enum TraceTypes_t
	{
		MAGICK_TRACE_LOST,
		MAGICK_TRACE_MAIN,
		MAGICK_TRACE_SIZE
	};

class Magick
{
	friend class Protocol;

	boost::posix_time::ptime start;
	pid_t pid;
	unsigned int level;

	boost::program_options::options_description opt_command_line_only;
	boost::program_options::options_description opt_common;
	boost::program_options::options_description opt_config_file_only;
	boost::program_options::variables_map opt_config;

	boost::read_write_mutex logger_lock;
	std::list<mantra::Logger<char> *> loggers;

	bool disconnect, shutdown;
	Uplink *uplink;

	Service nickserv, chanserv, memoserv, commserv, operserv, other;

	void init_config();	
	bool set_config(const boost::program_options::variables_map &vm);
public:
	Magick();
	~Magick();

	void Log(mantra::LogLevel::Level_t level, const boost::format &out);
	void LogHex(mantra::LogLevel::Level_t level, const void *buf, size_t len,
				const std::string &prefix = std::string());

	bool parse_config(const std::vector<std::string> &args);
	void run(const boost::function0<bool> &check);

	const boost::posix_time::ptime &Start() const { return start; }

	bool ConfigExists(const char *key) const
		{ return opt_config.count(key) != 0; }
	template<typename T>
	T ConfigValue(const char *key) const
		{ return boost::any_cast<T>(opt_config[key].value()); }

	Uplink *getUplink() const { return uplink; }
	Protocol proto;

	mantra::Events *event;
	Storage data;

	void Disconnect() { disconnect = true; }
	bool Shutdown() const { return shutdown; }
	void Shutdown(bool in) { shutdown = in; }
};

extern Magick *ROOT;
extern mantra::FlowControl *flow;

#endif // _MAGICK_MAGICK_H