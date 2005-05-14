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
#ifndef _MAGICK_PROTOCOL_H
#define _MAGICK_PROTOCOL_H 1

#ifdef RCSID
RCSID(magick__protocol_h, "@(#) $Id$");
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
#include "message.h"

#include <map>

#include <mantra/core/algorithms.h>

#include <boost/program_options.hpp>

struct remote_connection
{
	std::string host;
	boost::uint16_t port;
	std::string password;
	unsigned int numeric;
};

class LiveUser;
class Protocol
{
	friend class Magick;

	boost::program_options::options_description opt_protocol_config_file;
	boost::program_options::variables_map opt_protocol;

	bool usetokens;
	std::map<std::string, std::string> fwd_tokens;
	std::map<std::string, std::string, mantra::iless<std::string> > rev_tokens;
	unsigned char enc_bits, encoding[128], rev_encoding[256];

	Protocol();
	void init();
	bool reload(const std::string &file);

	const std::string &tokenise(const std::string &in) const;
	std::string assemble(const std::vector<std::string> &in,
						 bool forcecolon = false) const;

	void addline(std::string &out, const std::string &in) const;
	void addline(const Jupe &server, std::string &out, const std::string &in) const;

	bool send(const char *buf, boost::uint64_t len) const;
	inline bool send(const std::string &in) const
		{ return send(in.data(), in.size()); }
public:
	enum Numeric_t
		{
			nSTATSEND = 219,
			nADMINME = 256,
			nADMINLOC1 = 257,
			nADMINLOC2 = 258,
			nADMINEMAIL = 259,
			nTRACEEND = 262,
			nUSERHOST = 302,
			nISON = 303,
			nVERSION = 351,
			nLINKS = 365,
			nLINKSEND = 365,
			nNAMESEND = 366,
			nINFO = 371,
			nMOTD = 372,
			nINFOSTART = 373,
			nINFOEND = 374,
			nMOTDSTART = 375,
			nMOTDEND = 376,
			nTIME = 391,
			nNOSUCHNICK = 401,
			nNOSUCHSERVER = 402,
			nNOSUCHCHANNEL = 403,
			nNEED_MORE_PARAMS = 461,
			nINCORRECT_PASSWORD = 464,
			nUSERNOTINCHANNEL = 442,
			nSUMMONDISABLED = 445,
			nUSERSDISABLED = 446
		};

	bool ConfigExists(const char *key) const
		{ return opt_protocol.count(key) != 0; }
	template<typename T>
	T ConfigValue(const char *key) const
		{ return opt_protocol[key].template as<T>(); }

	bool IsServer(const std::string &in) const;
	bool IsChannel(const std::string &in) const;

	std::string NumericToID(unsigned int numeric) const;
	unsigned int IDToNumeric(const std::string &id) const;

	void UseTokens() { usetokens = true; }
	void Decode(std::string &in, std::deque<Message> &out) const;

	bool Connect(const Uplink &s);
	bool Connect(const Jupe &s);

	bool SQUIT(const Jupe &s, const std::string &reason = std::string()) const;

	bool RAW(const Jupe &s, const std::string &cmd,
			 const std::vector<std::string> &args,
			 bool forcecolon = false) const;
	inline bool RAW(const Jupe &s, const std::string &cmd,
					const std::string &arg, bool forcecolon = false) const
	{
		std::vector<std::string> v;
		v.push_back(arg);
		return RAW(s, cmd, v, forcecolon);
	}

	bool NUMERIC(Numeric_t num, const std::string &target,
				 const std::vector<std::string> &args,
				 bool forcecolon = false) const;
	inline bool NUMERIC(Numeric_t num, const std::string &target,
						const std::string &arg,
						bool forcecolon = false) const
	{
		std::vector<std::string> v;
		v.push_back(arg);
		return NUMERIC(num, target, v, forcecolon);
	}

	bool ERROR(const std::string &arg) const;

	void BurstEnd() const;
	boost::shared_ptr<Server> ParseServer(const Message &in) const;
	boost::shared_ptr<LiveUser> ParseUser(const Message &in) const;
};

#endif // _MAGICK_PROTOCOL_H
