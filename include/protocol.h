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

class Protocol
{
	friend class Magick;

	boost::program_options::options_description opt_protocol_config_file;
	boost::program_options::variables_map opt_protocol;

	std::map<std::string, std::string> fwd_tokens;
	std::map<std::string, std::string, mantra::iless<std::string> > rev_tokens;
	std::map<std::string, Message::Class_t> classes;
	unsigned char enc_bits, encoding[128], rev_encoding[256];

	Protocol();
	void init();
	bool reload(const std::string &file);

	const std::string &tokenise(const std::string &in) const;
	void addline(std::string &out, const std::string &in) const;
	void addline(const Jupe &server, std::string &out, const std::string &in) const;
	void addline(const Uplink &server, std::string &out, const std::string &in) const;
	bool send(const char *buf, boost::uint64_t len);
	bool send(const std::string &in) { return send(in.data(), in.size()); }
public:

	bool IsServer(const std::string &in);
	bool IsChannel(const std::string &in);

	std::string NumericToID(unsigned int numeric);
	unsigned int IDToNumeric(const std::string &id);

	void Decode(std::string &in, std::deque<Message> &out, bool usetokens = false);

	bool Connect(const Uplink &s, const std::string &password);
	bool Connect(const Jupe &s);

	bool SQUIT(const Jupe &s, const std::string &reason = std::string());
	bool SQUIT(const Uplink &s, const std::string &reason = std::string());

};

#endif // _MAGICK_PROTOCOL_H
