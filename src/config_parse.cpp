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
#define RCSID(x,y) const char *rcsid_magick__config_parse_cpp_ ## x () { return y; }
RCSID(magick__config_parse_cpp, "@(#)$Id$");

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

#include <fstream>

#include <mantra/log/simple.h>
#include <mantra/log/file.h>
#include <mantra/log/net.h>
#include <mantra/log/syslog.h>

#include <boost/filesystem/operations.hpp>

namespace po = boost::program_options;

template<typename T>
inline bool check_old_new(const char *entry, const mantra::OptionsSet &old_vm,
						  const po::variables_map &new_vm)
{
	MT_EB
	MT_FUNC("check_old_new" << entry << old_vm << new_vm);

	if (old_vm.empty())
		MT_RET(false);

	if (old_vm[entry].empty() != new_vm[entry].empty())
		MT_RET(false);

	// No clue why (old_vm[entry].as<T>() != new_vm[entry].as<T>())
	// will not work, but the below is the same anyway.
	if (!new_vm[entry].empty() &&
		boost::any_cast<T>(old_vm[entry].value()) !=
		boost::any_cast<T>(new_vm[entry].value()))
		MT_RET(false);

	MT_RET(true);
	MT_EE
}

bool Magick::set_config(const po::variables_map &vm)
{
	MT_EB
	MT_FUNC("Magick::set_config" << vm);

	// First things first, lets get the right language going.
	// All errors from now on will then be localised.
	if (!check_old_new<std::string>("language-dir", opt_config, vm))
		bindtextdomain("magick", vm["language-dir"].as<std::string>().c_str());
	if (!check_old_new<std::string>("language", opt_config, vm))
		setlocale(LC_ALL, vm["language"].as<std::string>().c_str());

	// Now parse commaneline-only options.
	if (!vm["autoflush"].empty())
		MT_AUTOFLUSH(true);

	static mantra::iequal_to<std::string> iequal_to;

	if (!vm["trace"].empty())
	{
		std::vector<std::string> traces = vm["trace"].as<std::vector<std::string> >();
		for (size_t i=0; i<traces.size(); ++i)
		{
			std::string::size_type pos = traces[i].find(":");
			std::string type = traces[i].substr(0, pos);
			std::string svalue = mantra::dehex(traces[i].substr(pos + 1));
			boost::uint16_t value = (static_cast<unsigned char>(svalue[0]) << 8) +
									static_cast<unsigned char>(svalue[1]);

			if (iequal_to(type, "ALL"))
			{
				for (size_t j = 0; j < static_cast<size_t>(MAGICK_TRACE_SIZE); ++j)
					mantra::mtrace::instance().TurnSet(j, value);
			}
			else
			{
				size_t j;
				for (j = 0; j < static_cast<size_t>(MAGICK_TRACE_SIZE); ++j)
					if (boost::regex_match(type, TraceTypeRegex[j]))
					{
						mantra::mtrace::instance().TurnSet(j, value);
						break;
					}
				if (j >= static_cast<size_t>(MAGICK_TRACE_SIZE))
				{
					LOG(Error, _("Unknown trace level %1% specified."), type);
					MT_RET(false);
				}
			}
		}
	}

	// Simple syntax checkers first ...
	if (vm["max-level"].as<unsigned int>() <
		vm["level"].as<unsigned int>())
	{
		LOG(Error, _("CFG: %1% must be at least equal to %2%."),
			"max-level" % "level");
		MT_RET(false);
	}

	if (vm["filesystem.max-speed"].as<boost::uint64_t>() &&
		vm["filesystem.max-speed"].as<boost::uint64_t>() <
		vm["filesystem.min-speed"].as<boost::uint64_t>())
	{
		LOG(Error, _("CFG: %1% must be at least equal to %2%."),
			"filesystem.max-speed" % "filesystem.min-speed");
		MT_RET(false);
	}

	if (vm["general.max-list-size"].as<unsigned int>() <
		vm["general.list-size"].as<unsigned int>())
	{
		LOG(Error, _("CFG: %1% must be at least equal to %2%."),
			"general.max-list-size" % "general.list-size");
		MT_RET(false);
	}

	if (vm["general.max-threads"].as<unsigned int>() <
		vm["general.min-threads"].as<unsigned int>())
	{
		LOG(Error, _("CFG: %1% must be at least equal to %2%."),
			"general.max-threads" % "general.min-threads");
		MT_RET(false);
	}

	if (vm["general.high-water-mark"].as<unsigned int>() <
		vm["general.low-water-mark"].as<unsigned int>())
	{
		LOG(Error, _("CFG: %1% must be at least equal to %2%."),
			"general.high-water-mark" % "general.low-water-mark");
		MT_RET(false);
	}

	if (vm["chanserv.max-level"].as<int>() <
		vm["chanserv.min-level"].as<int>())
	{
		LOG(Error, _("CFG: %1% must be at least equal to %2%."),
			"chanserv.max-level" % "chanserv.min-level");
		MT_RET(false);
	}

	if (vm["operserv.clone.max-limit"].as<unsigned int>() <
		vm["operserv.clone.user-limit"].as<unsigned int>())
	{
		LOG(Error, _("CFG: %1% must be at least equal to %2%."),
			"operserv.clone.max-limit" % "operserv.clone.limit");
		MT_RET(false);
	}

	if (vm["operserv.clone.max-limit"].as<unsigned int>() <
		vm["operserv.clone.host-limit"].as<unsigned int>())
	{
		LOG(Error, _("CFG: %1% must be at least equal to %2%."),
			"operserv.clone.max-limit" % "operserv.clone.limit");
		MT_RET(false);
	}

	if (!vm["bind"].empty() && !check_old_new<std::string>("bind", opt_config, vm))
	{
		mantra::Socket s(mantra::Socket::STREAM, vm["bind"].as<std::string>());
		if (!s.Valid())
		{
			LOG(Error, _("CFG: Could not bind to %1% (%2%)."),
					vm["bind"].as<std::string>() % s.Local_Str());
			MT_RET(false);
		}
	}

	if (!check_old_new<std::string>("pidfile", opt_config, vm))
	{
		if (!opt_config["pidfile"].empty())
			boost::filesystem::remove(opt_config["pidfile"].as<std::string>());
		std::ofstream fs(vm["pidfile"].as<std::string>().c_str());
		fs << pid << std::endl;
	}

	if (!check_old_new<std::string>("protocol", opt_config, vm))
	{
		proto.reload(vm["protocol"].as<std::string>());
		Disconnect();
	}
	
	if (!check_old_new<std::string>("server-name", opt_config, vm) ||
		!check_old_new<std::string>("server-desc", opt_config, vm))
	{
		Disconnect();
	}

	if (level < vm["level"].as<unsigned int>())
		level = vm["level"].as<unsigned int>();

	if (!check_old_new<std::string>("fliesystem.flack-dir", opt_config, vm) ||
		!check_old_new<std::string>("filesystem.flack-memory", opt_config, vm) ||
		!check_old_new<std::string>("filesystem.flack-file-max", opt_config, vm))
	{
		Disconnect();
	}

	// Sign on/off services nicknames.
	if (!vm["services.nickserv"].empty())
	{
		boost::char_separator<char> sep(" \t");
		typedef boost::tokenizer<boost::char_separator<char>,
			std::string::const_iterator, std::string> tokenizer;
		tokenizer tokens(vm["services.nickserv"].as<std::string>(), sep);
		std::vector<std::string> ent(tokens.begin(), tokens.end());
		if_Service_Magick(nickserv).Set(ent, vm["services.nickserv-name"].as<std::string>());
	}
	else
 		if_Service_Magick(nickserv).Set(std::vector<std::string>());

	if (!vm["services.chanserv"].empty())
	{
		boost::char_separator<char> sep(" \t");
		typedef boost::tokenizer<boost::char_separator<char>,
			std::string::const_iterator, std::string> tokenizer;
		tokenizer tokens(vm["services.chanserv"].as<std::string>(), sep);
		std::vector<std::string> ent(tokens.begin(), tokens.end());
		if_Service_Magick(chanserv).Set(ent, vm["services.chanserv-name"].as<std::string>());
	}
	else
 		if_Service_Magick(chanserv).Set(std::vector<std::string>());

	if (!vm["services.memoserv"].empty())
	{
		boost::char_separator<char> sep(" \t");
		typedef boost::tokenizer<boost::char_separator<char>,
			std::string::const_iterator, std::string> tokenizer;
		tokenizer tokens(vm["services.memoserv"].as<std::string>(), sep);
		std::vector<std::string> ent(tokens.begin(), tokens.end());
		if_Service_Magick(memoserv).Set(ent, vm["services.memoserv-name"].as<std::string>());
	}
	else
 		if_Service_Magick(memoserv).Set(std::vector<std::string>());

	if (!vm["services.commserv"].empty())
	{
		boost::char_separator<char> sep(" \t");
		typedef boost::tokenizer<boost::char_separator<char>,
			std::string::const_iterator, std::string> tokenizer;
		tokenizer tokens(vm["services.commserv"].as<std::string>(), sep);
		std::vector<std::string> ent(tokens.begin(), tokens.end());
		if_Service_Magick(commserv).Set(ent, vm["services.commserv-name"].as<std::string>());
	}
	else
 		if_Service_Magick(commserv).Set(std::vector<std::string>());

	if (!vm["services.operserv"].empty())
	{
		boost::char_separator<char> sep(" \t");
		typedef boost::tokenizer<boost::char_separator<char>,
			std::string::const_iterator, std::string> tokenizer;
		tokenizer tokens(vm["services.operserv"].as<std::string>(), sep);
		std::vector<std::string> ent(tokens.begin(), tokens.end());
		if_Service_Magick(operserv).Set(ent, vm["services.operserv-name"].as<std::string>());
	}
	else
 		if_Service_Magick(operserv).Set(std::vector<std::string>());

	if (!vm["services.other"].empty())
	{
		boost::char_separator<char> sep(" \t");
		typedef boost::tokenizer<boost::char_separator<char>,
			std::string::const_iterator, std::string> tokenizer;
		tokenizer tokens(vm["services.other"].as<std::string>(), sep);
		std::vector<std::string> ent(tokens.begin(), tokens.end());
		if_Service_Magick(other).Set(ent, vm["services.other-name"].as<std::string>());
	}
	else
 		if_Service_Magick(other).Set(std::vector<std::string>());

	if (getUplink())
	{
		std::string quitmsg;
		if (!vm["services.quit-message"].empty())
			quitmsg = vm["services.quit-message"].as<std::string>();

		if (!check_old_new<std::string>("services.user", opt_config, vm) ||
			!check_old_new<std::string>("services.host", opt_config, vm))
		{
			nickserv.MASSQUIT(quitmsg);
			chanserv.MASSQUIT(quitmsg);
			memoserv.MASSQUIT(quitmsg);
			commserv.MASSQUIT(quitmsg);
			operserv.MASSQUIT(quitmsg);
			other.MASSQUIT(quitmsg);
		}
		else
		{
			if (!check_old_new<std::string>("services.nickserv-name", opt_config, vm))
				nickserv.MASSQUIT(quitmsg);

			if (!check_old_new<std::string>("services.chanserv-name", opt_config, vm))
				chanserv.MASSQUIT(quitmsg);

			if (!check_old_new<std::string>("services.memoserv-name", opt_config, vm))
				memoserv.MASSQUIT(quitmsg);

			if (!check_old_new<std::string>("services.commserv-name", opt_config, vm))
				commserv.MASSQUIT(quitmsg);

			if (!check_old_new<std::string>("services.operserv-name", opt_config, vm))
				operserv.MASSQUIT(quitmsg);

			if (!check_old_new<std::string>("services.other-name", opt_config, vm))
				other.MASSQUIT(quitmsg);
		}

		nickserv.Check();
		chanserv.Check();
		memoserv.Check();
		commserv.Check();
		operserv.Check();
		other.Check();

		if (!check_old_new<bool>("chanserv.hide", opt_config, vm))
		{
			// TODO: set or unset mode +h
		}
	}

	if (!data.init(vm, *opt_config))
		MT_RET(false);

	// Logging is the last thing, so that all previous errors are logged
	// through any existing means.
	if (!vm["log"].empty())
	{
		mantra::LogLevel::Level_t level = static_cast<mantra::LogLevel::Level_t>(vm["log-level"].as<unsigned int>());

		std::set<std::string> ov;
		if (!opt_config["log"].empty())
			ov = opt_config["log"].as<std::set<std::string> >();
		std::set<std::string> v = vm["log"].as<std::set<std::string> >();

		std::set<std::string> newv, oldv, samev;
		std::set_difference(v.begin(), v.end(), ov.begin(), ov.end(),
							std::inserter(newv, newv.end()));
		std::set_difference(ov.begin(), ov.end(), v.begin(), v.end(),
							std::inserter(oldv, oldv.end()));
		std::set_intersection(v.begin(), v.end(), ov.begin(), ov.end(),
							  std::inserter(samev, samev.end()));

		std::set<std::string>::const_iterator iter;

		static std::ostream *logstream = NULL;
		if (samev.size())
		{
			boost::read_write_mutex::scoped_read_lock lock(logger_lock);

			for (iter=samev.begin(); iter!=samev.end(); ++iter)
			{
				std::list<mantra::Logger<char> *>::iterator i = loggers.begin();
				for (i=loggers.begin(); i!=loggers.end(); ++i)
					if (*iter == (*i)->backend())
					{
						if (*iter == "file")
						{
							if (vm["log.file.name"].empty())
							{
								LOG(Error, _("CFG: %1% not specified."),
									"log.file.name");
								MT_RET(false);
							}

							mantra::FileLogger<char> *l = dynamic_cast<mantra::FileLogger<char> *>(*i);
							if (!l)
							{
								LOG(Error, _("CFG: Could not retrieve existing %1% log."),
									"file");
								MT_RET(false);
							}

							std::string str = vm["log.file.name"].as<std::string>();
							if (str != l->Handle().Name())
							{
								mantra::mfile of(str, O_WRONLY | O_CREAT | O_APPEND);
								if (!of.IsOpened())
								{
									LOG(Error, _("CFG: Could not open log file %1%."), str);
									MT_RET(false);
								}
								l->Handle().Close();
								l->Handle() = of;
							}

							if (level != l->Threshold())
								l->Threshold(level);

							unsigned int uint = vm["log.file.backup"].as<unsigned int>();
							if (uint != l->Backup())
								l->Backup(uint);
							uint = vm["log.file.max-size"].as<unsigned int>();
							if (uint != l->Max_Size())
								l->Max_Size(uint);

							std::string arch_ext, str2;
							if (!vm["log.file.archive-cmd"].empty())
								str = vm["log.file.archive-cmd"].as<std::string>();
							if (!vm["log.file.archive-ext"].empty())
								str2 = vm["log.file.archive-ext"].as<std::string>();
							if (str != l->Archive_Cmd() || str2 != l->Archive_Extension())
								l->Archive(str, str2);

							// Standard options (except UTC).
							str = vm["log.file.format"].as<std::string>();
							if (str != l->Format())
								l->Format(str);

							l->HexOpts(vm["log.file.hex-decimal"].as<bool>(),
									   vm["log.file.hex-width"].as<unsigned int>());

							str = vm["log.file.levels.fatal"].as<std::string>();
							if (str != l->Level(mantra::LogLevel::Fatal))
								l->Level(mantra::LogLevel::Fatal, str);
							str = vm["log.file.levels.critical"].as<std::string>();
							if (str != l->Level(mantra::LogLevel::Critical))
								l->Level(mantra::LogLevel::Critical, str);
							str = vm["log.file.levels.error"].as<std::string>();
							if (str != l->Level(mantra::LogLevel::Error))
								l->Level(mantra::LogLevel::Error, str);
							str = vm["log.file.levels.warning"].as<std::string>();
							if (str != l->Level(mantra::LogLevel::Warning))
								l->Level(mantra::LogLevel::Warning, str);
							str = vm["log.file.levels.notice"].as<std::string>();
							if (str != l->Level(mantra::LogLevel::Notice))
								l->Level(mantra::LogLevel::Notice, str);
							str = vm["log.file.levels.info"].as<std::string>();
							if (str != l->Level(mantra::LogLevel::Info))
								l->Level(mantra::LogLevel::Info, str);
							str = vm["log.file.levels.debug"].as<std::string>();
							if (str != l->Level(mantra::LogLevel::Debug))
								l->Level(mantra::LogLevel::Debug, str);
						}
						else if (*iter == "syslog")
						{
							if (vm["log.syslog.facility"].empty())
							{
								LOG(Error, _("CFG: %1% not specified."),
									"log.syslog.facility");
								MT_RET(false);
							}

							mantra::SystemLogger *l = dynamic_cast<mantra::SystemLogger *>(*i);
							if (!l)
							{
								LOG(Error, _("CFG: Could not retrieve existing %1% log."),
									"syslog");
								MT_RET(false);
							}

							if (level != l->Threshold())
								l->Threshold(level);
							l->HexOpts(vm["log.file.hex-decimal"].as<bool>(),
									   vm["log.file.hex-width"].as<unsigned int>());
						}
						else if (*iter == "simple")
						{
							if (vm["log.simple.name"].empty())
							{
								LOG(Error, _("CFG: %1% not specified."),
									"log.simple.name");
								MT_RET(false);
							}

							mantra::SimpleLogger<char> *l = dynamic_cast<mantra::SimpleLogger<char> *>(*i);
							if (!l)
							{
								LOG(Error, _("CFG: Could not retrieve existing %1% log."),
									"simple");
								MT_RET(false);
							}
							delete l;

							std::string name = vm["log.simple.name"].as<std::string>();
							if (iequal_to(name, "stdout"))
								l = new mantra::SimpleLogger<char>(std::cout, level,
										vm["log.simple.utc"].as<bool>());
							else if (iequal_to(name, "stderr"))
								l = new mantra::SimpleLogger<char>(std::cerr, level,
										vm["log.simple.utc"].as<bool>());
							else
							{
								logstream = new std::ofstream(name.c_str(),
														std::ios_base::app |
														std::ios_base::out);
								if (!*logstream)
								{
									LOG(Error, _("CFG: Could not open log file %1%."), vm["log.simple.name"].as<std::string>());
									MT_RET(false);
								}
								l = new mantra::SimpleLogger<char>(*logstream, level,
										vm["log.simple.utc"].as<bool>());
							}

							// Standard options (except UTC).
							std::string str = vm["log.simple.format"].as<std::string>();
							if (str != l->Format())
								l->Format(str);

							l->HexOpts(vm["log.simple.hex-decimal"].as<bool>(),
									   vm["log.simple.hex-width"].as<unsigned int>());

							str = vm["log.simple.levels.fatal"].as<std::string>();
							if (str != l->Level(mantra::LogLevel::Fatal))
								l->Level(mantra::LogLevel::Fatal, str);
							str = vm["log.simple.levels.critical"].as<std::string>();
							if (str != l->Level(mantra::LogLevel::Critical))
								l->Level(mantra::LogLevel::Critical, str);
							str = vm["log.simple.levels.error"].as<std::string>();
							if (str != l->Level(mantra::LogLevel::Error))
								l->Level(mantra::LogLevel::Error, str);
							str = vm["log.simple.levels.warning"].as<std::string>();
							if (str != l->Level(mantra::LogLevel::Warning))
								l->Level(mantra::LogLevel::Warning, str);
							str = vm["log.simple.levels.notice"].as<std::string>();
							if (str != l->Level(mantra::LogLevel::Notice))
								l->Level(mantra::LogLevel::Notice, str);
							str = vm["log.simple.levels.info"].as<std::string>();
							if (str != l->Level(mantra::LogLevel::Info))
								l->Level(mantra::LogLevel::Info, str);
							str = vm["log.simple.levels.debug"].as<std::string>();
							if (str != l->Level(mantra::LogLevel::Debug))
								l->Level(mantra::LogLevel::Debug, str);
						}
						else if (*iter == "net")
						{
							if (vm["log.net.host"].empty())
							{
								LOG(Error, _("CFG: %1% not specified."),
									"log.net.host");
								MT_RET(false);
							}

							if (vm["log.net.port"].empty())
							{
								LOG(Error, _("CFG: %1% not specified."),
									"log.net.port");
								MT_RET(false);
							}

							mantra::NetLogger<char> *l = dynamic_cast<mantra::NetLogger<char> *>(*i);
							if (!l)
							{
								LOG(Error, _("CFG: Could not retrieve existing %1% log."),
									"net");
								MT_RET(false);
							}

							std::string str = vm["log.net.host"].as<std::string>();
							boost::uint16_t port = vm["log.net.port"].as<boost::uint16_t>();

							if ((str != l->Handle().Remote_Str() && 
								 str != l->Handle().Remote_Name()) ||
								port != l->Handle().Remote_Port())
							{
								mantra::Socket s;
								if (!vm["bind"].empty())
									s = mantra::Socket(mantra::Socket::STREAM, vm["bind"].as<std::string>());
								else
								{
									mantra::Socket::SockDomain_t domain = mantra::Socket::RemoteDomain(str);
									s = mantra::Socket(domain, mantra::Socket::STREAM);
								}
								if (!s.Valid())
								{
									NLOG(Error, _("CFG: Could not open logging socket."));
									MT_RET(false);
								}
								
								if (!s.StartConnect(str, port) || !s.CompleteConnect())
								{
									LOG(Error, _("CFG: Could not connect to remote logging host ([%1%]:%2%)."), str % port);
									MT_RET(false);
								}
								l->Handle().Close();
								l->Handle() = s;
							}

							if (level != l->Threshold())
								l->Threshold(level);

							// Standard options (except UTC).
							str = vm["log.net.format"].as<std::string>();
							if (str != l->Format())
								l->Format(str);

							l->HexOpts(vm["log.net.hex-decimal"].as<bool>(),
									   vm["log.net.hex-width"].as<unsigned int>());

							str = vm["log.net.levels.fatal"].as<std::string>();
							if (str != l->Level(mantra::LogLevel::Fatal))
								l->Level(mantra::LogLevel::Fatal, str);
							str = vm["log.net.levels.critical"].as<std::string>();
							if (str != l->Level(mantra::LogLevel::Critical))
								l->Level(mantra::LogLevel::Critical, str);
							str = vm["log.net.levels.error"].as<std::string>();
							if (str != l->Level(mantra::LogLevel::Error))
								l->Level(mantra::LogLevel::Error, str);
							str = vm["log.net.levels.warning"].as<std::string>();
							if (str != l->Level(mantra::LogLevel::Warning))
								l->Level(mantra::LogLevel::Warning, str);
							str = vm["log.net.levels.notice"].as<std::string>();
							if (str != l->Level(mantra::LogLevel::Notice))
								l->Level(mantra::LogLevel::Notice, str);
							str = vm["log.net.levels.info"].as<std::string>();
							if (str != l->Level(mantra::LogLevel::Info))
								l->Level(mantra::LogLevel::Info, str);
							str = vm["log.net.levels.debug"].as<std::string>();
							if (str != l->Level(mantra::LogLevel::Debug))
								l->Level(mantra::LogLevel::Debug, str);
						}
						break;
					}
			}
		}
		if (newv.size() || oldv.size())
		{
			boost::read_write_mutex::scoped_write_lock lock(logger_lock);

			for (iter = newv.begin(); iter != newv.end(); ++iter)
			{
				if (*iter == "file")
				{
					if (vm["log.file.name"].empty())
					{
						LOG(Error, _("CFG: %1% not specified."),
							"log.file.name");
						MT_RET(false);
					}

					mantra::mfile of(vm["log.file.name"].as<std::string>(), O_WRONLY | O_CREAT | O_APPEND);
					if (!of.IsOpened())
					{
						LOG(Error, _("CFG: Could not open log file %1%."), vm["log.file.name"].as<std::string>());
						MT_RET(false);
					}
					mantra::FileLogger<char> *l = new mantra::FileLogger<char>(of, level,
							vm["log.file.utc"].as<bool>());
					l->Backup(vm["log.file.backup"].as<unsigned int>());
					l->Max_Size(vm["log.file.max-size"].as<unsigned int>());
					if (!vm["archive-cmd"].empty())
						l->Archive(vm["log.file.archive-cmd"].as<std::string>(),
								   vm["archive-ext"].empty() ? std::string() :
								   vm["log.file.archive-ext"].as<std::string>());

					// Standard options (except UTC).
					l->Format(vm["log.file.format"].as<std::string>());
					l->HexOpts(vm["log.file.hex-decimal"].as<bool>(),
							   vm["log.file.hex-width"].as<unsigned int>());
					l->Level(mantra::LogLevel::Fatal, vm["log.file.levels.fatal"].as<std::string>());
					l->Level(mantra::LogLevel::Critical, vm["log.file.levels.critical"].as<std::string>());
					l->Level(mantra::LogLevel::Error, vm["log.file.levels.error"].as<std::string>());
					l->Level(mantra::LogLevel::Warning, vm["log.file.levels.warning"].as<std::string>());
					l->Level(mantra::LogLevel::Notice, vm["log.file.levels.notice"].as<std::string>());
					l->Level(mantra::LogLevel::Info, vm["log.file.levels.info"].as<std::string>());
					l->Level(mantra::LogLevel::Debug, vm["log.file.levels.debug"].as<std::string>());
					loggers.push_back(l);
				}
				else if (*iter == "syslog")
				{
					if (vm["log.syslog.facility"].empty())
					{
						LOG(Error, _("CFG: %1% not specified."),
								"log.syslog.facility");
						MT_RET(false);
					}

					mantra::SystemLogger *l = new mantra::SystemLogger(
								static_cast<mantra::SystemLogger::Facility_t>(vm["log.syslog.facility"].as<unsigned int>()), level);

					l->HexOpts(vm["log.syslog.hex-decimal"].as<bool>(),
							   vm["log.syslog.hex-width"].as<unsigned int>());

					loggers.push_back(l);
				}
				else if (*iter == "simple")
				{
					if (vm["log.simple.name"].empty())
					{
						LOG(Error, _("CFG: %1% not specified."),
							"log.simple.name");
						MT_RET(false);
					}

					std::string name = vm["log.simple.name"].as<std::string>();
					mantra::SimpleLogger<char> *l = NULL;

					if (boost::algorithm::iequals(name, "stdout"))
						l = new mantra::SimpleLogger<char>(std::cout, level,
								vm["log.simple.utc"].as<bool>());
					else if (boost::algorithm::iequals(name, "stderr"))
						l = new mantra::SimpleLogger<char>(std::cerr, level,
								vm["log.simple.utc"].as<bool>());
					else
					{
						logstream = new std::ofstream(name.c_str(),
												std::ios_base::app |
												std::ios_base::out);
						if (!*logstream)
						{
							LOG(Error, _("CFG: Could not open log file %1%."), vm["log.simple.name"].as<std::string>());
							MT_RET(false);
						}
						l = new mantra::SimpleLogger<char>(*logstream, level,
								vm["log.simple.utc"].as<bool>());
					}

					// Standard options (except UTC).
					l->Format(vm["log.simple.format"].as<std::string>());
					l->HexOpts(vm["log.simple.hex-decimal"].as<bool>(),
							   vm["log.simple.hex-width"].as<unsigned int>());
					l->Level(mantra::LogLevel::Fatal, vm["log.simple.levels.fatal"].as<std::string>());
					l->Level(mantra::LogLevel::Critical, vm["log.simple.levels.critical"].as<std::string>());
					l->Level(mantra::LogLevel::Error, vm["log.simple.levels.error"].as<std::string>());
					l->Level(mantra::LogLevel::Warning, vm["log.simple.levels.warning"].as<std::string>());
					l->Level(mantra::LogLevel::Notice, vm["log.simple.levels.notice"].as<std::string>());
					l->Level(mantra::LogLevel::Info, vm["log.simple.levels.info"].as<std::string>());
					l->Level(mantra::LogLevel::Debug, vm["log.simple.levels.debug"].as<std::string>());
					loggers.push_back(l);
				}
				else if (*iter == "net")
				{
					if (vm["log.net.host"].empty())
					{
						LOG(Error, _("CFG: %1% not specified."),
							"log.net.host");
						MT_RET(false);
					}

					if (vm["log.net.port"].empty())
					{
						LOG(Error, _("CFG: %1% not specified."),
							"log.net.port");
						MT_RET(false);
					}

					std::string host = vm["log.net.host"].as<std::string>();

					mantra::Socket s;
					if (!vm["bind"].empty())
						s = mantra::Socket(mantra::Socket::STREAM, vm["bind"].as<std::string>());
					else
					{
						mantra::Socket::SockDomain_t domain = mantra::Socket::RemoteDomain(host);
						s = mantra::Socket(domain, mantra::Socket::STREAM);
					}
					if (!s.Valid())
					{
						NLOG(Error, _("CFG: Could not open logging socket."));
						MT_RET(false);
					}
						
					if (!s.StartConnect(vm["log.net.host"].as<std::string>(),
										vm["log.net.port"].as<boost::uint16_t>())
						|| !s.CompleteConnect())
					{
						LOG(Error, _("CFG: Could not connect to remote logging host ([%1%]:%2%)."),
							vm["log.net.host"].as<std::string>() % vm["log.net.port"].as<boost::uint16_t>());
						MT_RET(false);
					}
										
					mantra::NetLogger<char> *l = new mantra::NetLogger<char>(s, level,
							vm["log.net.utc"].as<bool>());

					// Standard options (except UTC).
					l->Format(vm["log.net.format"].as<std::string>());
					l->HexOpts(vm["log.net.hex-decimal"].as<bool>(),
							   vm["log.net.hex-width"].as<unsigned int>());
					l->Level(mantra::LogLevel::Fatal, vm["log.net.levels.fatal"].as<std::string>());
					l->Level(mantra::LogLevel::Critical, vm["log.net.levels.critical"].as<std::string>());
					l->Level(mantra::LogLevel::Error, vm["log.net.levels.error"].as<std::string>());
					l->Level(mantra::LogLevel::Warning, vm["log.net.levels.warning"].as<std::string>());
					l->Level(mantra::LogLevel::Notice, vm["log.net.levels.notice"].as<std::string>());
					l->Level(mantra::LogLevel::Info, vm["log.net.levels.info"].as<std::string>());
					l->Level(mantra::LogLevel::Debug, vm["log.net.levels.debug"].as<std::string>());
					loggers.push_back(l);
				}
			}
			for (iter = oldv.begin(); iter != oldv.end(); ++iter)
			{
				std::list<mantra::Logger<char> *>::iterator i = loggers.begin();
				for (i=loggers.begin(); i!=loggers.end(); ++i)
					if (*iter == (*i)->backend())
					{
						delete (*i);
						loggers.erase(i);
						break;
					}
				if (*iter == "simple")
				{
					delete logstream;
					logstream = NULL;
				}
			}
		}
	}
	else if (!opt_config["log"].empty())
	{
		boost::read_write_mutex::scoped_write_lock lock(logger_lock);
		std::list<mantra::Logger<char> *>::iterator i;
		for (i=loggers.begin(); i!=loggers.end(); ++i)
			delete (*i);
		loggers.clear();
	}

	if (check_old_new<unsigned int>("operserv.clone.limit", opt_config, vm))
	{
		// TODO: Re-check clone limits
	}

	// Set the default levels ...
	StoredChannel::Level::Default(StoredChannel::Level::LVL_AutoDeop,
								  vm["chanserv.levels.autodeop"].as<int>(),
								  boost::regex("AUTOOP"), N_("Auto Deop"));
	StoredChannel::Level::Default(StoredChannel::Level::LVL_AutoVoice,
								  vm["chanserv.levels.autovoice"].as<int>(),
								  boost::regex("AUTOVOICE"), N_("Auto Voice"));
	StoredChannel::Level::Default(StoredChannel::Level::LVL_AutoHalfOp,
								  vm["chanserv.levels.autohalfop"].as<int>(),
								  boost::regex("AUTOHALFOP"), N_("Auto HalfOp"));
	StoredChannel::Level::Default(StoredChannel::Level::LVL_AutoOp,
								  vm["chanserv.levels.autoop"].as<int>(),
								  boost::regex("AUTOOP"), N_("Auto Op"));
	StoredChannel::Level::Default(StoredChannel::Level::LVL_ReadMemo,
								  vm["chanserv.levels.readmemo"].as<int>(),
								  boost::regex("READMEMO"), N_("Read News"));
	StoredChannel::Level::Default(StoredChannel::Level::LVL_WriteMemo,
								  vm["chanserv.levels.writememo"].as<int>(),
								  boost::regex("WRITEMEMO"), N_("Write News"));
	StoredChannel::Level::Default(StoredChannel::Level::LVL_DelMemo,
								  vm["chanserv.levels.delmemo"].as<int>(),
								  boost::regex("DELMEMO"), N_("Delete News"));
	StoredChannel::Level::Default(StoredChannel::Level::LVL_Greet,
								  vm["chanserv.levels.greet"].as<int>(),
								  boost::regex("GREET"), N_("Set Own Greeting"));
	StoredChannel::Level::Default(StoredChannel::Level::LVL_OverGreet,
								  vm["chanserv.levels.overgreet"].as<int>(),
								  boost::regex("OVERGREET"), N_("Set Any Greeting"));
	StoredChannel::Level::Default(StoredChannel::Level::LVL_Message,
								  vm["chanserv.levels.message"].as<int>(),
								  boost::regex("MESSAGE"), N_("Create On-Join Message"));
	StoredChannel::Level::Default(StoredChannel::Level::LVL_AutoKick,
								  vm["chanserv.levels.autokick"].as<int>(),
								  boost::regex("AKICK"), N_("Manage AutoKicks"));
	StoredChannel::Level::Default(StoredChannel::Level::LVL_Super,
								  vm["chanserv.levels.super"].as<int>(),
								  boost::regex("SUPER"), N_("'Super' Op"));
	StoredChannel::Level::Default(StoredChannel::Level::LVL_Unban,
								  vm["chanserv.levels.unban"].as<int>(),
								  boost::regex("UNBAN"), N_("Unban Users"));
	StoredChannel::Level::Default(StoredChannel::Level::LVL_Access,
								  vm["chanserv.levels.access"].as<int>(),
								  boost::regex("ACCESS"), N_("Manage Access List"));
	StoredChannel::Level::Default(StoredChannel::Level::LVL_Set,
								  vm["chanserv.levels.set"].as<int>(),
								  boost::regex("SET"), N_("Alter Channel Settings"));
	StoredChannel::Level::Default(StoredChannel::Level::LVL_View,
								  vm["chanserv.levels.view"].as<int>(),
								  boost::regex("VIEW"), N_("View Current Channel Information"));
	StoredChannel::Level::Default(StoredChannel::Level::LVL_CMD_Invite,
								  vm["chanserv.levels.cmd-invite"].as<int>(),
								  boost::regex("CMDINVITE"), N_("Use the INVITE command"));
	StoredChannel::Level::Default(StoredChannel::Level::LVL_CMD_Unban,
								  vm["chanserv.levels.cmd-unban"].as<int>(),
								  boost::regex("CMDUNBAN"), N_("Use the UNBAN command"));
	StoredChannel::Level::Default(StoredChannel::Level::LVL_CMD_Voice,
								  vm["chanserv.levels.cmd-voice"].as<int>(),
								  boost::regex("CMDVOICE"), N_("Use the VOICE/DEVOICE command"));
	StoredChannel::Level::Default(StoredChannel::Level::LVL_CMD_HalfOp,
								  vm["chanserv.levels.cmd-halfop"].as<int>(),
								  boost::regex("CMDHALFOP"), N_("Use the HALFOP/DEHALFOP command"));
	StoredChannel::Level::Default(StoredChannel::Level::LVL_CMD_Op,
								  vm["chanserv.levels.cmd-op"].as<int>(),
								  boost::regex("CMDOP"), N_("Use the OP/DEOP command"));
	StoredChannel::Level::Default(StoredChannel::Level::LVL_CMD_Kick,
								  vm["chanserv.levels.cmd-kick"].as<int>(),
								  boost::regex("CMDKICK"), N_("Use the KICK/ANONKICK command"));
	StoredChannel::Level::Default(StoredChannel::Level::LVL_CMD_Mode,
								  vm["chanserv.levels.cmd-mode"].as<int>(),
								  boost::regex("CMDMODE"), N_("Use the MODE/TOPIC command"));
	StoredChannel::Level::Default(StoredChannel::Level::LVL_CMD_Clear,
								  vm["chanserv.levels.cmd-clear"].as<int>(),
								  boost::regex("CMDCLEAR"), N_("Use the CLEAR command"));

	MT_RET(true);
	MT_EE
}
