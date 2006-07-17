#ifndef WIN32
#pragma interface
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

class LiveUser;
class Protocol
{
	friend class Magick;

	// These need to access our stuff directly, as they
	// send all kinds of messages.
	friend class Service;
	friend class Server;
	friend class Uplink;
	friend class Jupe;

	boost::program_options::options_description opt_protocol_config_file;
	boost::program_options::variables_map opt_protocol;

	bool usetokens;
	std::map<std::string, std::string> fwd_tokens;
	std::map<std::string, std::string, mantra::iless<std::string> > rev_tokens;
	unsigned char enc_bits, encoding[128], rev_encoding[256];
	size_t param_server_name, param_server_id, param_nick_name, param_nick_id, param_nick_server;

	Protocol();
	void init();
	bool reload(const std::string &file);

	const std::string &tokenise(const std::string &in) const;
	std::string assemble(const std::vector<std::string> &in,
						 bool forcecolon = false) const;

	void addline(std::string &out, const std::string &in) const;
	void addline(const Jupe &server, std::string &out, const std::string &in) const;
	void addline(const LiveUser &user, std::string &out, const std::string &in) const;

	bool send(const char *buf, boost::uint64_t len) const;
	inline bool send(const std::string &in) const
		{ return send(in.data(), in.size()); }
public:
	bool ConfigExists(const char *key) const
		{ return opt_protocol.count(key) != 0; }
	template<typename T>
	T ConfigValue(const char *key) const
		{ return opt_protocol[key].template as<T>(); }

	size_t Param_Server_Name() const { return param_server_name; }
	size_t Param_Server_ID() const { return param_server_id; }
	size_t Param_Nick_Name() const { return param_nick_name; }
	size_t Param_Nick_ID() const { return param_nick_id; }
	size_t Param_Nick_Server() const { return param_nick_server; }

	bool IsServer(const std::string &in) const;
	bool IsChannel(const std::string &in) const;

	std::string NumericToID(unsigned int numeric) const;
	unsigned int IDToNumeric(const std::string &id) const;

	void UseTokens() { usetokens = true; }
	void Decode(std::string &in, std::deque<Message> &out) const;

	bool Connect(const Uplink &s);
	bool Connect(const Jupe &s);

	void BurstEnd() const;
	boost::shared_ptr<Server> ParseServer(const Message &in) const;
	boost::shared_ptr<LiveUser> ParseUser(const Message &in) const;
};

#endif // _MAGICK_PROTOCOL_H
