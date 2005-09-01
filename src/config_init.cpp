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
#define RCSID(x,y) const char *rcsid_magick__config_init_cpp_ ## x () { return y; }
RCSID(magick__config_init_cpp, "@(#)$Id$");

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

#include <mantra/core/algorithms.h>
#include <mantra/core/utils.h>
#include <mantra/core/typed_value.h>

#include <boost/algorithm/string.hpp>

// Need the following to activate various crypts
#include <mantra/storage/cryptstage.h>
#include <mantra/storage/compressstage.h>

#ifdef HAVE_SYSLOG
# include <mantra/log/syslog.h>
#endif

static const char *required_fields[] = {
	"server-name", "protocol", "storage", "remote", "operserv.services-admin",
	NULL };

namespace po = boost::program_options;

static void add_storage_options(po::options_description &opts);
static void add_log_options(po::options_description &opts);
static void add_services_options(po::options_description &opts);
static void add_filesystem_options(po::options_description &opts);
static void add_general_options(po::options_description &opts);
static void add_nickserv_options(po::options_description &opts);
static void add_chanserv_options(po::options_description &opts);
static void add_memoserv_options(po::options_description &opts);
static void add_operserv_options(po::options_description &opts);
static void add_commserv_options(po::options_description &opts);

template<typename C, typename T>
std::basic_istream<C,T> &operator>>(std::basic_istream<C,T> &is,
									remote_connection &rc)
{
	MT_EB
	MT_FUNC("&operator>>" << &is << rc);

	std::ios_base::fmtflags orig_flags = is.flags();
	is.flags(orig_flags | std::ios_base::skipws);
	is >> rc.host >> rc.port >> rc.password >> std::ws;
	if (is.eof())
		rc.numeric = 0;
	else
		is >> rc.numeric >> std::ws;
	if (rc.host.empty() || !rc.port || rc.password.empty() ||
		!(mantra::is_hostname(rc.host) ||
		  mantra::is_inet_address(rc.host) ||
		  mantra::is_inet6_address(rc.host)))
		is.setstate(std::ios_base::failbit);
	is.flags(orig_flags);

	MT_RET(is);
	MT_EE
}

class validate_allow
{
public:
	template<typename charT>
	boost::any operator()(const std::basic_string<charT> &s) const
	{
		MT_EB
		MT_FUNC("validate_allow::operator()" << s);

		std::vector<std::basic_string<charT> > ent;
		typedef typename boost::tokenizer<boost::char_separator<charT>,
				typename std::basic_string<charT>::const_iterator,
				std::basic_string<charT> > tokenizer;
		boost::char_separator<charT> sep(mantra::convert_string<charT>(" \t").c_str());
		tokenizer tokens(s, sep);
		ent.assign(tokens.begin(), tokens.end());
		if (ent.empty())
			throw po::invalid_option_value(s);

		if (!(mantra::is_hostname(ent[0]) ||
			  mantra::is_inet_address(ent[0]) ||
			  mantra::is_inet6_address(ent[0])))
			throw po::invalid_option_value(s);

		boost::any rv = boost::any(s);
		MT_RET(rv);
		MT_EE
	}
};

class validate_channel
{
public:
	template<typename charT>
	boost::any operator()(const std::basic_string<charT> &s) const
	{
		MT_EB
		MT_FUNC("validate_channel::operator()" << s);

		static boost::basic_regex<charT> rx(mantra::convert_string<charT>("^[&#+][^[:space:][:cntrl:],]*$"));
		if (!boost::regex_match(s, rx))
			throw po::invalid_option_value(s);

		if (s[0] == '&')
		{
			std::basic_string<charT> rv = s;
			rv.replace(0, 1, 1, '#');
			boost::any ret = boost::any(rv);
			MT_RET(ret);
		}

		boost::any ret = boost::any(s);
		MT_RET(ret);
		MT_EE
	}
};

class validate_mlock
{
public:
	template<typename charT>
	boost::any operator()(const std::basic_string<charT> &s) const
	{
		MT_EB
		MT_FUNC("validate_mlock::operator()" << s);

		std::vector<std::basic_string<charT> > ent;
		typedef typename boost::tokenizer<boost::char_separator<charT>,
				typename std::basic_string<charT>::const_iterator,
				std::basic_string<charT> > tokenizer;
		boost::char_separator<charT> sep(mantra::convert_string<charT>(" \t").c_str());
		tokenizer tokens(s, sep);
		ent.assign(tokens.begin(), tokens.end());
		if (ent.empty())
			throw po::invalid_option_value(s);

		bool add = true;
		std::set<char> had;
		size_t i=0, j=0;
		for (; i<ent[0].size(); ++i)
		{
			switch (ent[0][i])
			{
			case '+':
				add = true;
				break;
			case '-':
				add = false;
				break;
			case 'o': // These cannot be in an mlock.
			case 'v':
			case 'b':
			case 'e':
			case 'h':
				throw po::invalid_option_value(s);
			case 'k':
				if (had.find((char) ent[0][i]) != had.end())
					throw po::invalid_option_value(s);
				had.insert((char) ent[0][i]);
				if (add)
				{
					if (++j >= ent.size())
						throw po::invalid_option_value(s);
				}
				break;
			case 'l':
				if (had.find((char) ent[0][i]) != had.end())
					throw po::invalid_option_value(s);
				had.insert((char) ent[0][i]);
				if (add)
				{
					if (++j >= ent.size())
						throw po::invalid_option_value(s);

					try
					{
						boost::lexical_cast<unsigned int>(ent[j]);
					}
					catch (const boost::bad_lexical_cast &)
					{
						throw po::invalid_option_value(s);
					}
				}
				break;
			default:
				if (ent[0][i] < 'A' || ent[0][i] > 'z' ||
					(ent[0][i] > 'Z' && ent[0][i] < 'a') ||
					had.find((char) ent[0][i]) != had.end())
					throw po::invalid_option_value(s);
				had.insert((char) ent[0][i]);
			}
		}
		if (j + 1 < ent.size())
			throw po::invalid_option_value(s);

		boost::any rv = boost::any(s);
		MT_RET(rv);
		MT_EE
	}
};

void Magick::init_config()
{
	MT_EB
	MT_FUNC("Magick::init_config");

	opt_command_line_only.add_options()
		("help,h", "produce help message")
		("version,v", "print software version")
		("nofork", "do not become a background process") // handled elsewhere.
		("config,C", mantra::value<std::string>()->default_value("magick.ini"),
					"configuration file name")
		("dir,D", mantra::value<std::string>(),
					"root directory for relative paths")
#ifdef WIN32
		("service", mantra::value<std::vector<std::string> >()->multitoken(),
					"manage NT service")
#endif
#ifdef MANTRA_TRACING
		("trace", mantra::value<std::vector<std::string> >()->composing()->parser(mantra::validate_regex<std::string, char>("^[[:alpha:]]+:[[:xdigit:]]{4}$")),
					"alter startup trace levels")
#endif
		("disable-dcc", mantra::value<bool>()->implicit(), "disable all file transfers")
	;

	opt_common.add_options()
		("pidfile,P", mantra::value<std::string>()->default_value("magick.pid"),
					"file to store the process id")
		("motdfile,M", mantra::value<std::string>()->default_value("magick.motd"),
					"file to read the message of the day from")
		("log,L", mantra::value<std::set<std::string> >()->composing()->parser(
					mantra::validate_oneof<std::string, mantra::iequal_to<std::string> >("file")
#ifdef HAVE_SYSLOG
					+ "syslog"
#endif
					+ "simple" + "net"), "type of log output to use")
		("log-level,l", mantra::value<unsigned int>()->default_value(mantra::LogLevel::Info, "info")->parser(
					mantra::validate_mapped<std::string, unsigned int, mantra::iless<std::string> >("Fatal", mantra::LogLevel::Fatal)
						+ std::make_pair("Critical", mantra::LogLevel::Critical)
						+ std::make_pair("Error", mantra::LogLevel::Error)
						+ std::make_pair("Warning", mantra::LogLevel::Warning)
						+ std::make_pair("Notice", mantra::LogLevel::Notice)
						+ std::make_pair("Info", mantra::LogLevel::Info)
						+ std::make_pair("Debug", mantra::LogLevel::Debug)
						+ std::make_pair("Debug+1", mantra::LogLevel::Debug + 1)
						+ std::make_pair("Debug+2", mantra::LogLevel::Debug + 2)
						+ std::make_pair("Debug+3", mantra::LogLevel::Debug + 3)
						+ std::make_pair("Debug+4", mantra::LogLevel::Debug + 4)
						+ std::make_pair("Debug+5", mantra::LogLevel::Debug + 5)
					), "default logging level")
		("server-name,n", mantra::value<std::string>()->parser(mantra::validate_host()),
					"server name we use on the network")
		("server-desc", mantra::value<std::string>()->default_value("Magick IRC Services"),
					"server description we use on the network")
		("bind,b", mantra::value<std::string>()->parser(mantra::validate_host()),
					"ip address to bind to")
		("protocol,p", mantra::value<std::string>(),
					"file name containing protocol definition")
		("log-channel", mantra::value<std::string>()->parser(validate_channel()),
					"channel to send all log messages")
		("storage,s", mantra::value<std::string>()->parser(
					mantra::validate_oneof<std::string, mantra::iequal_to<std::string> >("inifile")
#ifdef MANTRA_STORAGE_BERKELEYDB_SUPPORT
						+ "berkeleydb"
#endif
#ifdef MANTRA_STORAGE_XML_SUPPORT
						+ "xml"
#endif
#ifdef MANTRA_STORAGE_MYSQL_SUPPORT
						+ "mysql"
#endif
#ifdef MANTRA_STORAGE_POSTGRESQL_SUPPORT
						+ "postgresql"
#endif
#ifdef MANTRA_STORAGE_SQLITE_SUPPORT
						+ "sqlite"
#endif
					), "storage mechanism to use")
		("save-time", mantra::value<mantra::duration>()->default_value(mantra::duration("3n"), "3n"),
					"time between database saves")
		("language-dir", mantra::value<std::string>()->default_value("lang"),
					"directory where the language files are contained")
		("language", mantra::value<std::string>()->default_value("en_GB"),
					"default language to use")
		("level", mantra::value<unsigned int>()->default_value(1)->parser(mantra::validate_min<unsigned int>(1u)),
					"minimum level these services will operate at")
		("max-level", mantra::value<unsigned int>()->default_value(5)->parser(mantra::validate_min<unsigned int>(1u)),
					"maximum level these services will operate at")
		("lag-time", mantra::value<mantra::duration>()->default_value(mantra::duration("15s"), "15s"),
					"lag time required to increase the service level")
		("remote,r", mantra::value<std::vector<remote_connection> >()->composing(),
					"entry describing a connection to a remote host")
		("allow,a", mantra::value<std::vector<std::string> >()->composing()->parser(validate_allow()),
					"entry defining who is allowed on the network")
	;

	opt_config_file_only.add_options()
		("stop", mantra::value<bool>()->default_value(true),
					"fail to start services unless this is disabled")
	;
	add_storage_options(opt_config_file_only);
	add_log_options(opt_config_file_only);
	add_services_options(opt_config_file_only);
	add_filesystem_options(opt_config_file_only);
	add_general_options(opt_config_file_only);
	add_nickserv_options(opt_config_file_only);
	add_chanserv_options(opt_config_file_only);
	add_memoserv_options(opt_config_file_only);
	add_operserv_options(opt_config_file_only);
	add_commserv_options(opt_config_file_only);

	MT_EE
}

static void add_stage_options(po::options_description &opts, const std::string &prefix)
{
	MT_EB
	MT_FUNC("add_stage_options" << opts << prefix);

	opts.add_options()
		((prefix + ".stage.verify.offset").c_str(), mantra::value<boost::uint64_t>()->default_value(0u),
					"how far in the verification tag should be")
		((prefix + ".stage.verify.string").c_str(), mantra::value<std::string>(),
					"string to check for in the stream")
		((prefix + ".stage.verify.nice").c_str(), mantra::value<bool>()->default_value(false),
					"will we ignore a failure to verify")

#ifdef MANTRA_STORAGE_STAGE_COMPRESS_SUPPORT
		((prefix + ".stage.compress.level").c_str(), mantra::value<int>()->default_value(Z_DEFAULT_COMPRESSION)->parser(mantra::validate_range<int>(0, 9)),
					"compression level (0-9)")
#endif

		((prefix + ".stage.crypt.type").c_str(), mantra::value<unsigned int>()->parser(
					mantra::validate_mapped<std::string, unsigned int, mantra::iless<std::string> >("XOR", mantra::CryptStage::XOR)
#ifdef HAVE_AES_H
						+ std::make_pair("AES", mantra::CryptStage::AES)
#endif
#ifdef HAVE_DES_H
						+ std::make_pair("DES", mantra::CryptStage::DES)
						+ std::make_pair("DES3", mantra::CryptStage::DES3)
#endif
#ifdef HAVE_BLOWFISH_H
						+ std::make_pair("BLOWFISH", mantra::CryptStage::BlowFish)
#endif
#ifdef HAVE_IDEA_H
						+ std::make_pair("IDEA", mantra::CryptStage::IDEA)
#endif
#ifdef HAVE_CAST_H
						+ std::make_pair("CAST", mantra::CryptStage::CAST)
#endif
#ifdef HAVE_RC2_H
						+ std::make_pair("RC2", mantra::CryptStage::RC2)
#endif
#ifdef HAVE_RC5_H
						+ std::make_pair("RC5", mantra::CryptStage::RC5)
#endif
					), "type of encryption to use")
		((prefix + ".stage.crypt.bits").c_str(), mantra::value<unsigned int>(),
					"strength of encryption to use")
		((prefix + ".stage.crypt.hash").c_str(), mantra::value<unsigned int>()->default_value(mantra::Hasher::NONE)->parser(
					mantra::validate_mapped<std::string,unsigned int, mantra::iless<std::string> >("NONE", mantra::Hasher::NONE)
#ifdef HAVE_MD5_H
						+ std::make_pair("MD5", mantra::Hasher::MD5)
#endif
#ifdef HAVE_MD4_H
						+ std::make_pair("MD4", mantra::Hasher::MD4)
#endif
#ifdef HAVE_MD2_H
						+ std::make_pair("MD2", mantra::Hasher::MD2)
#endif
#ifdef HAVE_MDC2_H
						+ std::make_pair("MDC2", mantra::Hasher::MDC2)
#endif
#ifdef HAVE_SHA_H
						+ std::make_pair("SHA0", mantra::Hasher::SHA0)
						+ std::make_pair("SHA1", mantra::Hasher::SHA1)
#endif
#ifdef HAVE_RIPEMD_H
						+ std::make_pair("RIPEMD", mantra::Hasher::RIPEMD)
#endif
					), "hash algorithm to use on the key")
		((prefix + ".stage.crypt.key").c_str(), mantra::value<std::string>(),
					"the encryption key")
		((prefix + ".stage.crypt.keyfile").c_str(), mantra::value<std::string>(),
					"a file containing the encryption key")

		((prefix + ".stage.file.name").c_str(), mantra::value<std::string>(),
					"file name to for the final destination")

		((prefix + ".stage.net.host").c_str(), mantra::value<std::string>()->parser(mantra::validate_host()),
					"host or ip address of final destination")
		((prefix + ".stage.net.port").c_str(), mantra::value<unsigned short>(),
					"port of final destination")
	;

	MT_EE
}

static void add_storage_options(po::options_description &opts)
{
	MT_EB
	MT_FUNC("add_storage_options" << opts);

	opts.add_options()
		("storage.password-hash", mantra::value<unsigned int>()->default_value(mantra::Hasher::NONE)->parser(
					mantra::validate_mapped<std::string,unsigned int, mantra::iless<std::string> >("NONE", mantra::Hasher::NONE)
#ifdef HAVE_MD5_H
						+ std::make_pair("MD5", mantra::Hasher::MD5)
#endif
#ifdef HAVE_MD4_H
						+ std::make_pair("MD4", mantra::Hasher::MD4)
#endif
#ifdef HAVE_MD2_H
						+ std::make_pair("MD2", mantra::Hasher::MD2)
#endif
#ifdef HAVE_MDC2_H
						+ std::make_pair("MDC2", mantra::Hasher::MDC2)
#endif
#ifdef HAVE_SHA_H
						+ std::make_pair("SHA0", mantra::Hasher::SHA0)
						+ std::make_pair("SHA1", mantra::Hasher::SHA1)
#endif
#ifdef HAVE_RIPEMD_H
						+ std::make_pair("RIPEMD", mantra::Hasher::RIPEMD)
#endif
					), "hash algorithm to use on stored passwords")
		("storage.deep-lookup", mantra::value<bool>()->default_value(false),
		 			"will we try to do a DB lookup if a map lookup fails")
		("storage.load-after-save", mantra::value<bool>()->default_value(false),
		 			"reload the database after the 'save' is done")
	;
	
	opts.add_options()
		("storage.inifile.stage", mantra::value<std::vector<std::string> >()->composing()->parser(
					mantra::validate_oneof<std::string, mantra::iequal_to<std::string> >("file") + "net" + "verify"
#ifdef MANTRA_STORAGE_STAGE_COMPRESS_SUPPORT
					+ "compress"
#endif
					+ "crypt"
					), "stages this storage mechanism will invoke")
		("storage.inifile.tollerant", mantra::value<bool>()->default_value(false),
					"will we allow config values we are not expecting")
	;
	add_stage_options(opts, "storage.inifile");

#ifdef MANTRA_STORAGE_XML_SUPPORT
	opts.add_options()
		("storage.xml.stage", mantra::value<std::vector<std::string> >()->composing()->parser(
					mantra::validate_oneof<std::string, mantra::iequal_to<std::string> >("file") + "net" + "verify"
#ifdef MANTRA_STORAGE_STAGE_COMPRESS_SUPPORT
					+ "compress"
#endif
					+ "crypt"
					), "stages this storage mechanism will invoke")
		("storage.xml.tollerant", mantra::value<bool>()->default_value(false),
					"will we allow config values we are not expecting")
		("storage.xml.encode", mantra::value<std::string>()->default_value("UTF-8"),
					"encoding for XML data")
	;
	add_stage_options(opts, "storage.xml");
#endif

	opts.add_options()
#ifdef MANTRA_STORAGE_POSTGRESQL_SUPPORT
		("storage.postgresql.db-name", mantra::value<std::string>(),
					"database name we will connect to")
		("storage.postgresql.user", mantra::value<std::string>(),
					"user name we will connect with")
		("storage.postgresql.password", mantra::value<std::string>(),
					"password we will connect with")
		("storage.postgresql.host", mantra::value<std::string>()->parser(mantra::validate_host()),
					"host we will connect to")
		("storage.postgresql.port", mantra::value<unsigned short>(),
					"port we will connect to")
		("storage.postgresql.tollerant", mantra::value<bool>()->default_value(false),
					"will we allow config values we are not expecting")
		("storage.postgresql.timeout", mantra::value<unsigned int>()->default_value(0),
					"maximum time to wait for an available connection")
		("storage.postgresql.ssl-only", mantra::value<bool>()->default_value(false),
					"require an SSL connection to the database")
		("storage.postgresql.max-conn-count", mantra::value<unsigned int>()->default_value(0),
					"maximum number of connections to the database to keep")
#endif

#ifdef MANTRA_STORAGE_MYSQL_SUPPORT
		("storage.mysql.db-name", mantra::value<std::string>(),
					"database name we will connect to")
		("storage.mysql.user", mantra::value<std::string>(),
					"user name we will connect with")
		("storage.mysql.password", mantra::value<std::string>(),
					"password we will connect with")
		("storage.mysql.host", mantra::value<std::string>()->parser(mantra::validate_host()),
					"host we will connect to")
		("storage.mysql.port", mantra::value<unsigned short>(),
					"port we will connect to")
		("storage.mysql.tollerant", mantra::value<bool>()->default_value(false),
					"will we allow config values we are not expecting")
		("storage.mysql.timeout", mantra::value<unsigned int>()->default_value(0),
					"maximum time to wait for an available connection")
		("storage.mysql.compression", mantra::value<bool>()->default_value(false),
					"compress communications to/from the database")
		("storage.mysql.max-conn-count", mantra::value<unsigned int>()->default_value(0),
					"maximum number of connections to the database to keep")
#endif

#ifdef MANTRA_STORAGE_SQLITE_SUPPORT
		("storage.sqlite.db-name", mantra::value<std::string>(),
					"database name we will connect to")
		("storage.sqlite.tollerant", mantra::value<bool>()->default_value(false),
					"will we allow config values we are not expecting")
		("storage.sqlite.max-conn-count", mantra::value<unsigned int>()->default_value(0),
					"maximum number of connections to the database to keep")
#endif

#ifdef MANTRA_STORAGE_BERKELEYDB_SUPPORT
		("storage.berkeleydb.db-dir", mantra::value<std::string>(),
					"directory where databases reside.")
		("storage.berkeleydb.tollerant", mantra::value<bool>()->default_value(false),
					"will we allow config values we are not expecting")
		("storage.berkeleydb.private", mantra::value<bool>()->default_value(true),
					"are the DB's shared with other processes")
		("storage.berkeleydb.password", mantra::value<std::string>(),
					"password used to protect databases.")
		("storage.berkeleydb.btree", mantra::value<bool>()->default_value(false),
					"use binary tree (instead of hash) for storage")
#endif
	;

	MT_EE
}

static void add_standard_log_options(const std::string &prefix,
									 po::options_description &opts)
{
	MT_EB
	MT_FUNC("add_standard_log_options" << prefix << opts);

	opts.add_options()
		((prefix + ".utc").c_str(), mantra::value<bool>()->default_value(false),
					"should log entries be in 'universal' time")
		((prefix + ".format").c_str(), mantra::value<std::string>()->default_value("<DATE:C> <LEVEL:-10s> <MESSAGE>"),
					"format of log entries")
		((prefix + ".hex-decimal").c_str(), mantra::value<bool>()->default_value(false),
					"whether the offset for hex values is boolean")
		((prefix + ".hex-width").c_str(), mantra::value<unsigned int>()->default_value(16),
					"number of hex chars per output line")
		((prefix + ".levels.fatal").c_str(), mantra::value<std::string>()->default_value("FATAL"),
					"textual version of log level")
		((prefix + ".levels.critical").c_str(), mantra::value<std::string>()->default_value("CRITICAL"),
					"textual version of log level")
		((prefix + ".levels.error").c_str(), mantra::value<std::string>()->default_value("ERROR"),
					"textual version of log level")
		((prefix + ".levels.warning").c_str(), mantra::value<std::string>()->default_value("WARNING"),
					"textual version of log level")
		((prefix + ".levels.notice").c_str(), mantra::value<std::string>()->default_value("NOTICE"),
					"textual version of log level")
		((prefix + ".levels.info").c_str(), mantra::value<std::string>()->default_value("INFO"),
					"textual version of log level")
		((prefix + ".levels.debug").c_str(), mantra::value<std::string>()->default_value("DEBUG"),
					"textual version of log level")
	;

	MT_EE
}

static void add_log_options(po::options_description &opts)
{
	MT_EB
	MT_FUNC("add_log_options" << opts);

	opts.add_options()
		("log.file.name", mantra::value<std::string>(),
					"file name for log file")
		("log.file.backup", mantra::value<unsigned int>()->default_value(0),
					"number of cycled log files to keep")
		("log.file.max-size", mantra::value<unsigned int>()->default_value(0),
					"size at which to cycle the log file")
		("log.file.archive-cmd", mantra::value<std::string>(),
					"command to execute on archived log file")
		("log.file.archive-ext", mantra::value<std::string>(),
					"extension of archived log file after command is run")
	;
	add_standard_log_options("log.file", opts);

#ifdef HAVE_SYSLOG
	opts.add_options()
		("log.syslog.facility", mantra::value<unsigned int>()->parser(
						mantra::validate_mapped<std::string,unsigned int, mantra::iless<std::string> >("Kernel", mantra::SystemLogger::Kernel)
						+ std::make_pair("User", mantra::SystemLogger::User)
						+ std::make_pair("Mail", mantra::SystemLogger::Mail)
						+ std::make_pair("Daemon", mantra::SystemLogger::Daemon)
						+ std::make_pair("Auth", mantra::SystemLogger::Auth)
						+ std::make_pair("Syslog", mantra::SystemLogger::Syslog)
						+ std::make_pair("Lpr", mantra::SystemLogger::Lpr)
						+ std::make_pair("News", mantra::SystemLogger::News)
						+ std::make_pair("UUCP", mantra::SystemLogger::UUCP)
						+ std::make_pair("Cron", mantra::SystemLogger::Cron)
#if LOG_AUTHPRIV
						+ std::make_pair("AuthPriv", mantra::SystemLogger::AuthPriv)
#endif
#if LOG_FTP
						+ std::make_pair("Ftp", mantra::SystemLogger::Ftp)
#endif
						+ std::make_pair("Local0", mantra::SystemLogger::Local0)
						+ std::make_pair("Local1", mantra::SystemLogger::Local1)
						+ std::make_pair("Local2", mantra::SystemLogger::Local2)
						+ std::make_pair("Local3", mantra::SystemLogger::Local3)
						+ std::make_pair("Local4", mantra::SystemLogger::Local4)
						+ std::make_pair("Local5", mantra::SystemLogger::Local5)
						+ std::make_pair("Local6", mantra::SystemLogger::Local6)
						+ std::make_pair("Local7", mantra::SystemLogger::Local7)
					), "facility to log all messages in")
#endif
	;

	opts.add_options()
		("log.simple.name", mantra::value<std::string>(),
					"file name for log file")
	;
	add_standard_log_options("log.simple", opts);

	opts.add_options()
		("log.net.host", mantra::value<std::string>()->parser(mantra::validate_host()),
					"host or location to send log output")
		("log.net.port", mantra::value<std::string>(),
					"port to send log output")
	;
	add_standard_log_options("log.net", opts);

	MT_EE
}

static void add_services_options(po::options_description &opts)
{
	MT_EB
	MT_FUNC("add_services_options" << opts);

	static boost::regex nick_rx("^([[:alpha:]\\x5B-\\x60\\x7B-\\x7D][-[:alnum:]\\x5B-\\x60\\x7B-\\x7D]*[[:space:]]*)*$");

	opts.add_options()
		("services.user", mantra::value<std::string>()->parser(mantra::validate_regex<std::string, char>("^[^[:space:][:cntrl:]@]+$")),
					"username services should use")
		("services.host", mantra::value<std::string>()->parser(mantra::validate_host()),
					"hostname services should use")
		("services.mode", mantra::value<std::string>()->parser(mantra::validate_regex<std::string, char>("^\\+?[[:alpha:]]+$")),
					"modes services should set on themselves")
		("services.nickserv", mantra::value<std::string>()->parser(mantra::validate_regex<std::string, char>(nick_rx)),
					"aliases for service")
		("services.nickserv-name", mantra::value<std::string>()->default_value("Nickname Service"),
					"name for service aliases")
		("services.enforcer-name", mantra::value<std::string>()->default_value("Nickname Enforcer"),
					"name for services user holding nicknames")
		("services.chanserv", mantra::value<std::string>()->parser(mantra::validate_regex<std::string, char>(nick_rx)),
					"aliases for service")
		("services.chanserv-name", mantra::value<std::string>()->default_value("Channel Service"),
					"name for service aliases")
		("services.memoserv", mantra::value<std::string>()->parser(mantra::validate_regex<std::string, char>(nick_rx)),
					"aliases for service")
		("services.memoserv-name", mantra::value<std::string>()->default_value("Memo Service"),
					"name for service aliases")
		("services.operserv", mantra::value<std::string>()->parser(mantra::validate_regex<std::string, char>(nick_rx)),
					"aliases for service")
		("services.operserv-name", mantra::value<std::string>()->default_value("Operator Service"),
					"name for service aliases")
		("services.commserv", mantra::value<std::string>()->parser(mantra::validate_regex<std::string, char>(nick_rx)),
					"aliases for service")
		("services.commserv-name", mantra::value<std::string>()->default_value("Committee Service"),
					"name for service aliases")
		("services.other", mantra::value<std::string>()->parser(mantra::validate_regex<std::string, char>(nick_rx)),
					"aliases for service")
		("services.other-name", mantra::value<std::string>()->default_value("Help Service"),
					"name for service aliases")
		("services.quit-message", mantra::value<std::string>(),
					"signoff message for services")
	;

	MT_EE
}

static void add_filesystem_options(po::options_description &opts)
{
	MT_EB
	MT_FUNC("add_filesystem_options" << opts);

	opts.add_options()
		("filesystem.picture-dir", mantra::value<std::string>()->default_value("files/pic"),
					"directory containing user's pictures")
		("filesystem.picture-size", mantra::value<boost::uint64_t>()->default_value(0u)->parser(mantra::validate_space()),
					"maximum storage allocation for all pictures")
		("filesystem.attach-dir", mantra::value<std::string>()->default_value("files/memo"),
					"directory containing memo attachments")
		("filesystem.attach-size", mantra::value<boost::uint64_t>()->default_value(0u)->parser(mantra::validate_space()),
					"maximum storage allocation for all memo attachments")
		("filesystem.public-dir", mantra::value<std::string>()->default_value("files/pub"),
					"directory containing public files")
		("filesystem.public-size", mantra::value<boost::uint64_t>()->default_value(0u)->parser(mantra::validate_space()),
					"maximum storage allocation for all public files")
		("filesystem.temp-dir", mantra::value<std::string>()->default_value("files/temp"),
					"directory containing temporary files")
		("filesystem.temp-size", mantra::value<boost::uint64_t>()->default_value(0u)->parser(mantra::validate_space()),
					"maximum storage allocation for all temporary files")
		("filesystem.blocksize", mantra::value<boost::uint64_t>()->default_value(1024u)->parser(mantra::validate_space()),
					"DCC block size")
		("filesystem.timeout", mantra::value<mantra::duration>()->default_value(mantra::duration("2n")),
					"How long to wait for DCC to establish")
		("filesystem.min-speed", mantra::value<boost::uint64_t>()->default_value(0u)->parser(mantra::validate_space()),
					"Minimum speed of DCC for it to be sustained")
		("filesystem.max-speed", mantra::value<boost::uint64_t>()->default_value(0u)->parser(mantra::validate_space()),
					"Maximum speed a DCC may be sent")
		("filesystem.sample-time", mantra::value<mantra::duration>()->default_value(mantra::duration("30s")),
					"Size of rotating window used for speed sampling")
		("filesystem.flack-dir", mantra::value<std::string>()->default_value("flack"),
					"directory where disk-backup of outbound data is")
		("filesystem.flack-memory", mantra::value<unsigned int>()->default_value(65536),
					"memory to use for flack before falling back to disk")
		("filesystem.flack-file-max", mantra::value<boost::uint64_t>()->default_value(1048576),
					"maximum size of a flack file before rolling it over")
	;

	MT_EE
}

static void add_general_options(po::options_description &opts)
{
	MT_EB
	MT_FUNC("add_general_options" << opts);

	opts.add_options()
		("general.connection-timeout", mantra::value<mantra::duration>()->default_value(mantra::duration("30s")),
					"how long to attempt to create a connection")
		("general.server-relink", mantra::value<mantra::duration>()->default_value(mantra::duration("5s")),
					"how long to wait before attempting a connection")
		("general.squit-protect", mantra::value<mantra::duration>()->default_value(mantra::duration("2n")),
					"how long to remember info about a squit server")
		("general.squit-cancel", mantra::value<mantra::duration>()->default_value(mantra::duration("10s")),
					"how long to wait for SQUIT message before signing off user")
		("general.ping-frequency", mantra::value<mantra::duration>()->default_value(mantra::duration("30s")),
					"how often to check the network lag")
		("general.starthresh", mantra::value<unsigned int>()->default_value(4),
					"minimum non-wildcard chars required in various places")
		("general.list-size", mantra::value<unsigned int>()->default_value(50)->parser(mantra::validate_min<unsigned int>(1u)),
					"default amount of results to a list function")
		("general.max-list-size", mantra::value<unsigned int>()->default_value(250)->parser(mantra::validate_min<unsigned int>(1u)),
					"maximum number of list entries allowd to be requested")
		("general.min-threads", mantra::value<unsigned int>()->default_value(2)->parser(mantra::validate_min<unsigned int>(1u)),
					"minimum number of worker threads to maintain")
		("general.max-threads", mantra::value<unsigned int>()->default_value(10)->parser(mantra::validate_min<unsigned int>(1u)),
					"maximum number of worker threads to allow")
		("general.low-water-mark", mantra::value<unsigned int>()->default_value(50)->parser(mantra::validate_min<unsigned int>(1u)),
					"minimum message queue size to decrease worker threads")
		("general.high-water-mark", mantra::value<unsigned int>()->default_value(75)->parser(mantra::validate_min<unsigned int>(1u)),
					"maximum message queue size to increase worker threads")
		("general.watchdog-check", mantra::value<mantra::duration>()->default_value(mantra::duration("10s")),
					"how often the watchdog should check for dead threads")
		("general.watchdog-dead", mantra::value<mantra::duration>()->default_value(mantra::duration("30s")),
					"how long a thread must have not checked in to be dead")
		("general.message-expire", mantra::value<mantra::duration>()->default_value(mantra::duration("30s")),
					"maximum time to delay a message with a dependancy")
		("general.expire-check", mantra::value<mantra::duration>()->default_value(mantra::duration("5n")),
					"how often to check if anything needs to be expired")
		("general.cache-expire", mantra::value<mantra::duration>()->default_value(mantra::duration("5n")),
					"how long to remember cached preferenes before going back to the DB.")
	;

	MT_EE
}

static void add_nickserv_options(po::options_description &opts)
{
	MT_EB
	MT_FUNC("add_nickserv_options" << opts);

	opts.add_options()
		("nickserv.append", mantra::value<bool>()->default_value(false),
					"append characters to nickname instead of renaming")
		("nickserv.prefix", mantra::value<std::string>()->default_value("Guest")->parser(mantra::validate_regex<std::string, char>("^[[:alpha:]\\x5B-\\x60\\x7B-\\x7D][-[:alnum:]\\x5B-\\x60\\x7B-\\x7D]*$")),
					"prefix for name when append mode is used")
		("nickserv.suffixes", mantra::value<std::string>()->default_value("_-^`")->parser(mantra::validate_regex<std::string, char>("^[-[:alnum:]\\x5B-\\x60\\x7B-\\x7D]*$")),
					"suffix characters to try when rename mode is used")
		("nickserv.expire", mantra::value<mantra::duration>()->default_value(mantra::duration("4w")),
					"how long before a nickname is expired for non-use")
		("nickserv.delay", mantra::value<mantra::duration>()->default_value(mantra::duration("30s")),
					"minimum time between nickname registrations/links")
		("nickserv.ident", mantra::value<mantra::duration>()->default_value(mantra::duration("2n")),
					"how long the user has to ident before being renamed")
		("nickserv.release", mantra::value<mantra::duration>()->default_value(mantra::duration("1n")),
					"how long to keep a nickname being recovered")
		("nickserv.drop", mantra::value<mantra::duration>()->default_value(mantra::duration("1n")),
					"how long to keep a nickname drop token relevant")
		("nickserv.drop-length", mantra::value<unsigned int>()->default_value(8)->parser(mantra::validate_min<unsigned int>(1u)),
					"length of the token to generate for dropping nicknames")
		("nickserv.password-fail", mantra::value<unsigned int>()->default_value(5),
					"how many password faliures before killing the user")
		("nickserv.picture-maxsize", mantra::value<boost::uint64_t>()->default_value(0)->parser(mantra::validate_space()),
					"maximum size a user's picture may be")
		("nickserv.picture-extensions", mantra::value<std::string>()->default_value("jpg png gif bmp tif"),
					"valid extensions for an image")

		("nickserv.defaults.protect", mantra::value<bool>()->default_value(true), "")
		("nickserv.defaults.secure", mantra::value<bool>()->default_value(false), "")
		("nickserv.defaults.noexpire", mantra::value<bool>()->default_value(false), "")
		("nickserv.defaults.nomemo", mantra::value<bool>()->default_value(false), "")
		("nickserv.defaults.private", mantra::value<bool>()->default_value(false), "")
		("nickserv.defaults.privmsg", mantra::value<bool>()->default_value(false), "")
		("nickserv.defaults.language", mantra::value<std::string>()->default_value("english"), "")

		("nickserv.lock.protect", mantra::value<bool>()->default_value(false), "")
		("nickserv.lock.secure", mantra::value<bool>()->default_value(false), "")
		("nickserv.lock.noexpire", mantra::value<bool>()->default_value(false), "")
		("nickserv.lock.nomemo", mantra::value<bool>()->default_value(false), "")
		("nickserv.lock.private", mantra::value<bool>()->default_value(false), "")
		("nickserv.lock.privmsg", mantra::value<bool>()->default_value(false), "")
		("nickserv.lock.language", mantra::value<bool>()->default_value(false), "")
	;

	MT_EE
}

static void add_chanserv_options(po::options_description &opts)
{
	MT_EB
	MT_FUNC("add_chanserv_options" << opts);

	opts.add_options()
		("chanserv.hide", mantra::value<bool>()->default_value(false),
					"set mode +s, so we don't show up in channels")
		("chanserv.expire", mantra::value<mantra::duration>()->default_value(mantra::duration("4w")),
					"how long before a channel is unregistered for inactivity")
		("chanserv.delay", mantra::value<mantra::duration>()->default_value(mantra::duration("30s")),
					"minimum amount of time between registrations")
		("chanserv.drop", mantra::value<mantra::duration>()->default_value(mantra::duration("1n")),
					"how long to keep a channel drop token relevant")
		("chanserv.drop-length", mantra::value<unsigned int>()->default_value(8)->parser(mantra::validate_min<unsigned int>(1u)),
					"length of the token to generate for dropping channels")
		("chanserv.max-per-nick", mantra::value<unsigned int>()->default_value(15),
					"maximum channels to be registered for a nickname")
		("chanserv.ovr-per-nick", mantra::value<std::string>(),
					"committees that can bypass the registration limit")
		("chanserv.max-messages", mantra::value<unsigned int>()->default_value(15),
					"maximum number of on-join messages a channel may have")
		("chanserv.ovr-messages", mantra::value<std::string>(),
					"committees that can bypass the message limit")
		("chanserv.default-akick-reason", mantra::value<std::string>()->default_value("You have been banned from channel"),
					"kick reason when none is specified")
		("chanserv.password-fail", mantra::value<unsigned int>()->default_value(5),
					"number password failures before the user is killed")
		("chanserv.channel-keep", mantra::value<mantra::duration>()->default_value(mantra::duration("15s")),
					"how long chanserv stays in a +i/+k channel after last part")
		("chanserv.min-level", mantra::value<int>()->default_value(-1),
					"minimum level available on the access list")
		("chanserv.max-level", mantra::value<int>()->default_value(30),
					"maximum level available on the access list")

		("chanserv.defaults.mlock", mantra::value<std::string>()->default_value("+nt")->parser(validate_mlock()), "")
		("chanserv.defaults.bantime", mantra::value<mantra::duration>()->default_value(mantra::duration("0s")), "")
		("chanserv.defaults.parttime", mantra::value<mantra::duration>()->default_value(mantra::duration("3n")), "")
		("chanserv.defaults.keeptopic", mantra::value<bool>()->default_value(true), "")
		("chanserv.defaults.topiclock", mantra::value<bool>()->default_value(false), "")
		("chanserv.defaults.private", mantra::value<bool>()->default_value(false), "")
		("chanserv.defaults.secureops", mantra::value<bool>()->default_value(false), "")
		("chanserv.defaults.secure", mantra::value<bool>()->default_value(false), "")
		("chanserv.defaults.noexpire", mantra::value<bool>()->default_value(false), "")
		("chanserv.defaults.anarchy", mantra::value<bool>()->default_value(false), "")
		("chanserv.defaults.kickonban", mantra::value<bool>()->default_value(false), "")
		("chanserv.defaults.restricted", mantra::value<bool>()->default_value(false), "")
		("chanserv.defaults.cjoin", mantra::value<bool>()->default_value(false), "")
		("chanserv.defaults.revenge", mantra::value<unsigned int>()->default_value(0u)->parser(
					mantra::validate_mapped<std::string, unsigned int, mantra::iless<std::string> >("NONE", StoredChannel::R_None)
						+ std::make_pair("REVERSE",StoredChannel::R_Reverse)
						+ std::make_pair("MIRROR",StoredChannel::R_Mirror)
						+ std::make_pair("DEOP",StoredChannel::R_DeOp)
						+ std::make_pair("KICK",StoredChannel::R_Kick)
						+ std::make_pair("BAN1",StoredChannel::R_Ban1)
						+ std::make_pair("BAN2",StoredChannel::R_Ban2)
						+ std::make_pair("BAN3",StoredChannel::R_Ban3)
						+ std::make_pair("BAN4",StoredChannel::R_Ban4)
						+ std::make_pair("BAN5",StoredChannel::R_Ban5) ), "")

		("chanserv.lock.mlock", mantra::value<bool>()->default_value(false), "")
		("chanserv.lock.bantime", mantra::value<bool>()->default_value(false), "")
		("chanserv.lock.parttime", mantra::value<bool>()->default_value(false), "")
		("chanserv.lock.keeptopic", mantra::value<bool>()->default_value(false), "")
		("chanserv.lock.topiclock", mantra::value<bool>()->default_value(false), "")
		("chanserv.lock.private", mantra::value<bool>()->default_value(false), "")
		("chanserv.lock.secureops", mantra::value<bool>()->default_value(false), "")
		("chanserv.lock.secure", mantra::value<bool>()->default_value(false), "")
		("chanserv.lock.noexpire", mantra::value<bool>()->default_value(false), "")
		("chanserv.lock.anarchy", mantra::value<bool>()->default_value(false), "")
		("chanserv.lock.kickonban", mantra::value<bool>()->default_value(false), "")
		("chanserv.lock.restricted", mantra::value<bool>()->default_value(false), "")
		("chanserv.lock.cjoin", mantra::value<bool>()->default_value(false), "")
		("chanserv.lock.revenge", mantra::value<bool>()->default_value(false), "")

		("chanserv.levels.autodeop", mantra::value<int>()->default_value(-1), "")
		("chanserv.levels.autovoice", mantra::value<int>()->default_value(5), "")
		("chanserv.levels.autohalfop", mantra::value<int>()->default_value(10), "")
		("chanserv.levels.autoop", mantra::value<int>()->default_value(10), "")
		("chanserv.levels.readmemo", mantra::value<int>()->default_value(0), "")
		("chanserv.levels.writememo", mantra::value<int>()->default_value(15), "")
		("chanserv.levels.delmemo", mantra::value<int>()->default_value(25), "")
		("chanserv.levels.greet", mantra::value<int>()->default_value(1), "")
		("chanserv.levels.overgreet", mantra::value<int>()->default_value(25), "")
		("chanserv.levels.message", mantra::value<int>()->default_value(20), "")
		("chanserv.levels.autokick", mantra::value<int>()->default_value(20), "")
		("chanserv.levels.super", mantra::value<int>()->default_value(25), "")
		("chanserv.levels.unban", mantra::value<int>()->default_value(10), "")
		("chanserv.levels.access", mantra::value<int>()->default_value(5), "")
		("chanserv.levels.set", mantra::value<int>()->default_value(25), "")
		("chanserv.levels.view", mantra::value<int>()->default_value(1), "")
		("chanserv.levels.cmd-invite", mantra::value<int>()->default_value(5), "")
		("chanserv.levels.cmd-unban", mantra::value<int>()->default_value(10), "")
		("chanserv.levels.cmd-voice", mantra::value<int>()->default_value(5), "")
		("chanserv.levels.cmd-halfop", mantra::value<int>()->default_value(10), "")
		("chanserv.levels.cmd-op", mantra::value<int>()->default_value(10), "")
		("chanserv.levels.cmd-kick", mantra::value<int>()->default_value(15), "")
		("chanserv.levels.cmd-mode", mantra::value<int>()->default_value(15), "")
		("chanserv.levels.cmd-clear", mantra::value<int>()->default_value(20), "")
	;

	MT_EE
}

static void add_memoserv_options(po::options_description &opts)
{
	MT_EB
	MT_FUNC("add_memoserv_options" << opts);

	opts.add_options()
		("memoserv.news-expire", mantra::value<mantra::duration>()->default_value(mantra::duration("3w")),
					"how long before non-sticky news articles expire")
		("memoserv.inflight", mantra::value<mantra::duration>()->default_value(mantra::duration("2n")),
					"how long before a memo is actually sent")
		("memoserv.delay", mantra::value<mantra::duration>()->default_value(mantra::duration("10s")),
					"minimum time between memos")
		("memoserv.files", mantra::value<unsigned int>()->default_value(0),
					"maximum amount of file attachments")
		("memoserv.max-file-size", mantra::value<boost::uint64_t>()->default_value(0)->parser(mantra::validate_space()),
					"maximum size of any one file attachment")
	;

	MT_EE
}

static void add_operserv_options(po::options_description &opts)
{
	MT_EB
	MT_FUNC("add_operserv_options" << opts);

	opts.add_options()
		("operserv.services-admin", mantra::value<std::vector<std::string> >()->composing()->parser(mantra::validate_regex<std::string, char>("^[[:alpha:]\\x5B-\\x60\\x7B-\\x7D][-[:alnum:]\\x5B-\\x60\\x7B-\\x7D]*$")),
					"members of the SADMIN committee")
		("operserv.secure", mantra::value<bool>()->default_value(false),
					"only IRC operators (mode +o) can access operserv")
		("operserv.secure-oper", mantra::value<bool>()->default_value(false),
					"only members of the OPER committee may set mode +o")
		("operserv.htm-gap", mantra::value<mantra::duration>()->default_value(mantra::duration("5s")),
					"minimum time for average bandwidth calculation")
		("operserv.htm-threshold", mantra::value<boost::uint64_t>()->default_value(16384)->parser(mantra::validate_space()),
					"bandwidth at which to switch into high traffic mode")
		("operserv.max-htm-gap", mantra::value<mantra::duration>()->default_value(mantra::duration("1n")),
					"maximum gap period before disconnecting server")
		("operserv.manual-gap", mantra::value<mantra::duration>()->default_value(mantra::duration("30s")),
					"value of the gap when HTM is activated manually")

		("operserv.akill.reject", mantra::value<float>()->default_value(10.00),
					"percentage of network at which to reject an akill")
		("operserv.akill.expire", mantra::value<mantra::duration>()->default_value(mantra::duration("3h")),
					"default expiration time for an AKILL")
		("operserv.akill.expire-oper", mantra::value<mantra::duration>()->default_value(mantra::duration("1d")),
					"default expiration time for an AKILL set by OPER")
		("operserv.akill.expire-admin", mantra::value<mantra::duration>()->default_value(mantra::duration("1w")),
					"default expiration time for an AKILL set by ADMIN")
		("operserv.akill.expire-sop", mantra::value<mantra::duration>()->default_value(mantra::duration("2m")),
					"default expiration time for an AKILL set by SOP")
		("operserv.akill.expire-sadmin", mantra::value<mantra::duration>()->default_value(mantra::duration("1y")),
					"default expiration time for an AKILL set by SADMIN")

		("operserv.clone.limit", mantra::value<unsigned int>()->default_value(2),
					"default maximum number of connections for the same host")
		("operserv.clone.max-limit", mantra::value<unsigned int>()->default_value(50),
					"maximum amount the clone limit may be overridden to")
		("operserv.clone.kill-reason", mantra::value<std::string>()->default_value("Maximum connections from one host exceeded"),
					"kill reason when a user exceeds their clone limit")
		("operserv.clone.expire", mantra::value<mantra::duration>()->default_value(mantra::duration("3h")),
					"how long to remember old connections from the same host")
		("operserv.clone.akill-trigger", mantra::value<unsigned int>()->default_value(10),
					"number of times a clone limit may be hit before an AKILL")
		("operserv.clone.akill-reason", mantra::value<std::string>()->default_value("Clone trigger exceeded, Automatic AKILL"),
					"reason for an akill set by too much cloning")
		("operserv.clone.akill-time", mantra::value<mantra::duration>()->default_value(mantra::duration("30n")),
					"how long should a clone akill last")

		("operserv.ignore.flood-time", mantra::value<mantra::duration>()->default_value(mantra::duration("10s")),
					"sample window for flood detection")
		("operserv.ignore.flood-messages", mantra::value<unsigned int>()->default_value(5),
					"number of messages in the sample window considered a flood")
		("operserv.ignore.expire", mantra::value<mantra::duration>()->default_value(mantra::duration("20s")),
					"how long to ignore a user for flooding")
		("operserv.ignore.limit", mantra::value<unsigned int>()->default_value(5),
					"how many times ignore can be triggered before its permanent")
		("operserv.ignore.remove", mantra::value<mantra::duration>()->default_value(mantra::duration("5n")),
					"how long to remember old flood ignores")
		("operserv.ignore.method", mantra::value<unsigned int>()->default_value(5)->parser(
					mantra::validate_mapped<std::string, unsigned int, mantra::iless<std::string> >("nick!user@port.host", 0)
						+ std::make_pair("nick!user@*.host", 1)
						+ std::make_pair("nick!*@port.host", 2)
						+ std::make_pair("nick!*@*.host", 3)
						+ std::make_pair("nick!*@*", 4)
						+ std::make_pair("*!user@port.host", 5)
						+ std::make_pair("*!user@*.host", 6)
						+ std::make_pair("*!*@port.host", 7)
						+ std::make_pair("*!*@*.host", 8)
					), "method used for ignoring")

	;

	MT_EE
}

static void add_commserv_options(po::options_description &opts)
{
	MT_EB
	MT_FUNC("add_commserv_options" << opts);

	static boost::regex usermode_rx("^\\+?[[:alpha:]]+$");
	static boost::regex committee_rx("^[[:print:]]+$");

	opts.add_options()
		("commserv.max-logon", mantra::value<unsigned int>()->default_value(5),
					"maximum number of logon messages a committee may have")
		("commserv.ovr-logon", mantra::value<std::string>(),
					"committees allowed to override the maximum logon messages")

		("commserv.defaults.secure", mantra::value<bool>()->default_value(false), "")
		("commserv.defaults.private", mantra::value<bool>()->default_value(false), "")
		("commserv.defaults.openmemos", mantra::value<bool>()->default_value(true), "")

		("commserv.lock.secure", mantra::value<bool>()->default_value(false), "")
		("commserv.lock.private", mantra::value<bool>()->default_value(false), "")
		("commserv.lock.openmemos", mantra::value<bool>()->default_value(false), "")

		("commserv.all.name", mantra::value<std::string>()->default_value("ALL")->parser(mantra::validate_regex<std::string, char>(committee_rx)),
					"name (tag) of the committee")
		("commserv.all.set-mode", mantra::value<std::string>()->parser(mantra::validate_regex<std::string, char>(usermode_rx)),
					"user modes to set on members of the committee")

		("commserv.regd.name", mantra::value<std::string>()->default_value("REGD")->parser(mantra::validate_regex<std::string, char>(committee_rx)),
					"name (tag) of the committee")
		("commserv.regd.set-mode", mantra::value<std::string>()->parser(mantra::validate_regex<std::string, char>(usermode_rx)),
					"user modes to set on members of the committee")

		("commserv.sadmin.name", mantra::value<std::string>()->default_value("SADMIN")->parser(mantra::validate_regex<std::string, char>(committee_rx)),
					"name (tag) of the committee")
		("commserv.sadmin.set-mode", mantra::value<std::string>()->parser(mantra::validate_regex<std::string, char>(usermode_rx)),
					"user modes to set on members of the committee")
		("commserv.sadmin.mode-o", mantra::value<bool>()->default_value(true),
					"require users to be an IRC operator to be recognized")

		("commserv.sop.name", mantra::value<std::string>()->default_value("SOP")->parser(mantra::validate_regex<std::string, char>(committee_rx)),
					"name (tag) of the committee")
		("commserv.sop.set-mode", mantra::value<std::string>()->parser(mantra::validate_regex<std::string, char>(usermode_rx)),
					"user modes to set on members of the committee")
		("commserv.sop.mode-o", mantra::value<bool>()->default_value(true),
					"require users to be an IRC operator to be recognized")

		("commserv.admin.name", mantra::value<std::string>()->default_value("ADMIN")->parser(mantra::validate_regex<std::string, char>(committee_rx)),
					"name (tag) of the committee")
		("commserv.admin.set-mode", mantra::value<std::string>()->parser(mantra::validate_regex<std::string, char>(usermode_rx)),
					"user modes to set on members of the committee")
		("commserv.admin.mode-o", mantra::value<bool>()->default_value(true),
					"require users to be an IRC operator to be recognized")

		("commserv.oper.name", mantra::value<std::string>()->default_value("OPER")->parser(mantra::validate_regex<std::string, char>(committee_rx)),
					"name (tag) of the committee")
		("commserv.oper.set-mode", mantra::value<std::string>()->parser(mantra::validate_regex<std::string, char>(usermode_rx)),
					"user modes to set on members of the committee")
		("commserv.oper.mode-o", mantra::value<bool>()->default_value(true),
					"require users to be an IRC operator to be recognized")

		("commserv.overrides.view", mantra::value<std::string>()->default_value("OPER"), "")
		("commserv.overrides.owner", mantra::value<std::string>()->default_value("SADMIN"), "")
		("commserv.overrides.mode", mantra::value<std::string>()->default_value("SOP"), "")
		("commserv.overrides.op", mantra::value<std::string>()->default_value("SOP"), "")
		("commserv.overrides.halfop", mantra::value<std::string>()->default_value("ADMIN"), "")
		("commserv.overrides.voice", mantra::value<std::string>()->default_value("OPER"), "")
		("commserv.overrides.invite", mantra::value<std::string>()->default_value("SOP"), "")
		("commserv.overrides.kick", mantra::value<std::string>()->default_value("SOP"), "")
		("commserv.overrides.unban", mantra::value<std::string>()->default_value("SOP"), "")
		("commserv.overrides.clear", mantra::value<std::string>()->default_value("SADMIN"), "")
	;

	MT_EE
}

bool Magick::parse_config(const std::vector<std::string> &args)
{
	MT_EB
	MT_FUNC("Magick::parse_config" << args);

	po::options_description cmdline;
	cmdline.add(opt_command_line_only).add(opt_common);

	po::options_description cfgfile;
	cfgfile.add(opt_common).add(opt_config_file_only);

	po::variables_map vm;

	try
	{
		if (!args.empty())
		{
			po::store(po::command_line_parser(args).options(cmdline).run(), vm);
			if (vm.count("help"))
			{
				std::cout << "Syntax: " << args[0] << " [options]\n" <<
							 cmdline <<  std::endl;
				flow->Shutdown();
				MT_RET(true);
			}
			else if (vm.count("version"))
			{
				std::cout << "Magick IRC Services v" << MAGICK_VERSION_MAJOR <<
					"." << MAGICK_VERSION_MINOR << "." << MAGICK_VERSION_PATCH << "\n" <<
					"(c) 2005 The Neuromancy Society <magick@neuromancy.net>" << "\n" <<
					"Distributed under the GNU General Public License,\n" <<
					"available at http://www.neuromancy.net/license/gpl.html." <<
					std::endl;
				flow->Shutdown();
				MT_RET(true);
			}
			else if (vm.count("dir"))
			{
				if (chdir(vm["dir"].as<std::string>().c_str()) < 0)
				{
					LOG(Fatal, N_("Failed to set working directory to %1%."),
						vm["dir"].as<std::string>());
				}
			}
		}

		std::ifstream fs(vm["config"].as<std::string>().c_str());
		if (!fs.is_open())
		{
			LOG(Error, _("Could not open configuration file %1%."), vm["config"].as<std::string>());
			MT_RET(false);
		}
		else
		{
			po::store(po::parse_config_file(fs, cfgfile), vm);
			fs.close();
		}
		po::notify(vm); 
	}
	catch (po::error &e)
	{
		NLOG(Error, e.what());
		MT_RET(false);
	}

	for (size_t i=0; required_fields[i]; ++i)
	{
		if (!vm.count(required_fields[i]))
		{
			LOG(Error, _("Missing field %1%."), required_fields[i]);
			MT_RET(false);
		}
	}

	if (vm["stop"].as<bool>())
	{
		NLOG(Error, _("Stop value not disabled in config.\n"
					  "Please READ and EDIT your configuration file."));
		MT_RET(false);
	}

	bool rv = set_config(vm);
	MT_RET(rv);
	MT_EE
}
