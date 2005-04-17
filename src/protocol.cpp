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
#define RCSID(x,y) const char *rcsid_magick__protocol_cpp_ ## x () { return y; }
RCSID(magick__protocol_cpp, "@(#)$Id$");

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

#include <fstream>

#include <mantra/core/typed_value.h>

template<typename C, typename T>
std::basic_istream<C,T> &operator>>(std::basic_istream<C,T> &is,
									std::pair<std::string, std::string> &t)
{
	MT_EB
	MT_FUNC("operator>>" << is << t);

	std::ios::fmtflags orig_flags = is.flags();
	is.flags(orig_flags | std::ios::skipws);
	is >> t.first >> t.second >> std::ws;
	if (t.first.empty() || t.second.empty())
		is.setstate(std::ios::failbit);
	is.flags(orig_flags);
	MT_RET(is);

	MT_EE
}

namespace po = boost::program_options;

Protocol::Protocol() : enc_bits(0)
{
	MT_EB
	MT_FUNC("Protocol::Protocol");

	static boost::regex modes_rx("^\\+?[[:alpha:]]+$");

	opt_protocol_config_file.add_options()
		("nick-length", mantra::value<unsigned int>()->default_value(9)->parser(mantra::validate_range<unsigned int>(1u,32u)),
					"maximum length of a nickname")
		("max-line", mantra::value<unsigned int>()->default_value(450)->parser(mantra::validate_min<unsigned int>(50)),
					"maximum length of the data portion of any line")

		("globops", mantra::value<bool>()->default_value(false),
					"is the GLOBOPS command supported")
		("helpops", mantra::value<bool>()->default_value(false),
					"is the HELPOPS command supported")
		("chatops", mantra::value<bool>()->default_value(false),
					"is the CHATOPS command supported")

		("tokens", mantra::value<bool>()->default_value(false),
					"should we FORCE token usage on")
		("tsora", mantra::value<unsigned int>(),
					"version of TSora we support")
		("extended-topic", mantra::value<bool>()->default_value(false),
					"does the TOPIC command contain setter and set time")
		("topic-join", mantra::value<bool>()->default_value(false),
					"do we have to be IN channel to set the topic")
		("topic-current", mantra::value<bool>()->default_value(false),
					"do we have to use the current time when setting topic")
		("server-modes", mantra::value<bool>()->default_value(false),
					"do we have to use the server to set modes")
		("ison", mantra::value<bool>()->default_value(true),
					"does this protocol have a working ISON command")

		("join", mantra::value<unsigned int>()->default_value(0)->parser(mantra::validate_range<unsigned int>(0u, 2u)),
					"syntax of the JOIN command to use")

		// %1% = server   %4% = setter            %7% = current time
		// %2% = user     %5% = set time          %8% = expiry time
		// %3% = host     %6% = akill duration    %9% = reason
		("akill", mantra::value<std::string>(),
					"syntax of the AKILL command to use.")
		("unakill", mantra::value<std::string>(),
					"syntax used to reverse an AKILL (if any).")
		("kill-after-akill", mantra::value<bool>()->default_value(true),
					"is a KILL required after an AKILL message")

		// %1% = nick     %4% = server            %7% = ident
		// %2% = user     %5% = signon time       %8% = modes
		// %3% = host     %6% = hops              %9% = realname
		("nick", mantra::value<std::string>()->default_value("NICK %1% %2% %3% %4% :%9%"),
					"syntax of the NICK command to use.")

		// %1% = name     %3% = description       %5% = current time
		// %2% = hops     %4% = numeric           %6% = start time
		("server", mantra::value<std::string>()->default_value("SERVER %1% %2% :%3%"),
					"syntax of the SERVER command to use.")

		("channel-mode-params", mantra::value<std::string>()->default_value("ovbkl")->parser(mantra::validate_regex<std::string, char>(modes_rx)),
					"modes that will take an argument.")
		("max-channel-mode-params", mantra::value<unsigned int>()->default_value(3)->parser(mantra::validate_min<unsigned int>(1u)),
					"maximum allowed channel mode parameters.")
		("founder-mode", mantra::value<std::string>()->parser(mantra::validate_regex<std::string, char>(modes_rx)),
					"modes (each taking an argument) to set on the founder.")
		("services-modes", mantra::value<std::string>()->parser(mantra::validate_regex<std::string, char>(modes_rx)),
					"modes set on services to identify them as services.")

		("burst-start", mantra::value<std::string>(),
					"syntax of the start of burst command to use.")
		("burst-end", mantra::value<std::string>(),
					"syntax of the end of burst command to use.")

		("capabilities", mantra::value<std::string>(),
					"syntax of the PROTOCOL/CAPAB command to use.")

		// %1% = old nickname      %2% = new nickname      %3% = current time
		("svsnick", mantra::value<std::string>(),
					"syntax of the SVSNICK command to use.")
		// %1% = nickname          %2% = modes             %3% = current time
		// %4% = current time
		("svsmode", mantra::value<std::string>(),
					"syntax of the SVSMODE command to use.")
		// %1% = nickname          %2% = reason            %3% = current time
		("svskill", mantra::value<std::string>(),
					"syntax of the SVSKILL command to use.")
		// %1% = server            %3% = reason            %3% = current time
		("svsnoop", mantra::value<std::string>(),
					"syntax of the SVSNOOP command to use.")
		// %1% = server            %2% = reason            %3% = current time
		("unsvsnoop", mantra::value<std::string>(),
					"syntax of the UNSVSNOOP command to use.")
		// %1% = nick mask         %2% = reason            %3% = current time
		("sqline", mantra::value<std::string>(),
					"syntax of the SQLINE command to use.")
		// %1% = nick mask         %2% = reason            %3% = current time
		("unsqline", mantra::value<std::string>(),
					"syntax of the UNSQLINE command to use.")
		// %1% = nickname         %2% = new host           %3% = current time
		("svshost", mantra::value<std::string>(),
					"syntax of the SVSHOST command to use.")

		("token", mantra::value<std::vector<std::pair<std::string, std::string> > >()->composing(),
					"characters to use for encoding numberic.")

		("numeric.trim", mantra::value<bool>()->default_value(false),
					"Remove 0-padding of numeric.")
		("numeric.server-numeric", mantra::value<bool>()->default_value(false),
					"Send an un-encoded numeric in the SERVER line.")
		("numeric.combine", mantra::value<bool>()->default_value(false),
					"Combine nick and server numerics for messages with a source.")
		("numeric.prefix-all", mantra::value<bool>()->default_value(false),
					"Prefix all server messages with our server numeric.")
		("numeric.server", mantra::value<unsigned int>()->parser(mantra::validate_min<unsigned int>(1u)),
					"Maximum length of SERVER numeric.")
		("numeric.server-field", mantra::value<unsigned int>(),
					"field in the SERVER message containing the numeric.")
		("numeric.nick", mantra::value<unsigned int>()->parser(mantra::validate_min<unsigned int>(1u)),
					"Maximum length of NICK numeric.")
		("numeric.channel", mantra::value<unsigned int>()->parser(mantra::validate_min<unsigned int>(1u)),
					"Maximum length of CHANNEL numeric.")

		("numeric.encoding", mantra::value<std::string>()->parser(mantra::validate_regex<std::string, char>("^[[:print:]]([[:space:]]+[[:print:]])*$")),
					"characters to use for encoding numeric.")
	;

	classes["PASS"] = Message::ServerSignon;
	classes["SERVER"] = Message::ServerSignon;

	MT_EE
}

bool Protocol::reload(const std::string &file)
{
	MT_EB
	MT_FUNC("Protocol::reload" << file);

	std::ifstream fs(file.c_str());
	if (!fs.is_open())
	{
		LOG(Error, _("Could not open protocol file %1%."), file);
		MT_RET(false);
	}
	else
	{
		po::store(po::parse_config_file(fs, opt_protocol_config_file), opt_protocol);
		fs.close();
	}
	po::notify(opt_protocol); 

	fwd_tokens.clear();
	rev_tokens.clear();
	if (opt_protocol.count("token"))
	{
		std::vector<std::pair<std::string, std::string> > tmp =
			opt_protocol["token"].as<std::vector<std::pair<std::string, std::string> > >();
		std::vector<std::pair<std::string, std::string> >::iterator i;
		for (i=tmp.begin(); i!=tmp.end(); i++)
		{
			fwd_tokens[i->first] = i->second;
			rev_tokens[i->second] = i->first;
		}
	}

	if (opt_protocol.count("numeric.encoding"))
	{
		typedef boost::tokenizer<boost::char_separator<char>,
				std::string::const_iterator, std::string> tokenizer;
		boost::char_separator<char> sep(" \t");
		tokenizer tokens(opt_protocol["numeric.encoding"].as<std::string>(), sep);
		size_t j = 0;
		tokenizer::const_iterator i;
		for (i=tokens.begin(); i!=tokens.end(); ++i, ++j)
		{
			encoding[j] = *(i->data());
			rev_encoding[(size_t) *(i->data())] = j;
		}

		enc_bits = 0;
		while (j >= 2)
		{
			if (j % 2 != 0)
			{
				NLOG(Error, _("Encoding character set is not a power of 2 in size."));
				MT_RET(false);
			}
			++enc_bits;
			j /= 2;
		}
	}

	MT_RET(true);
	MT_EE
}

const std::string &Protocol::tokenise(const std::string &in) const
{
	MT_EB
	MT_FUNC("Protocol::tokenise");

	if (rev_tokens.empty() || !ROOT->uplink || !ROOT->uplink->Tokens())
		MT_RET(in);

	std::map<std::string, std::string, mantra::iless<std::string> >::const_iterator iter =
		rev_tokens.find(in);
	if (iter == rev_tokens.end())
		MT_RET(in);

	MT_RET(iter->second);
	MT_EE
}

void Protocol::addline(std::string &out, const std::string &in) const
{
	MT_EB
	MT_FUNC("Protocol::addline" << &out << in);

	LOG(Debug+1, "Sending message: %1%", in);
	out.append(in);
	out.append("\r\n");

	MT_EE
}

void Protocol::addline(const Uplink &s, std::string &out, const std::string &in) const
{
	MT_EB
	MT_FUNC("Protocol::addline" << s << &out << in);

	addline(out, ":" + s.Name() + " " + in);

	MT_EE
}

void Protocol::addline(const Jupe &s, std::string &out, const std::string &in) const
{
	MT_EB
	MT_FUNC("Protocol::addline" << s << &out << in);

	addline(out, ":" + s.Name() + " " + in);

	MT_EE
}

bool Protocol::send(const char *buf, boost::uint64_t len)
{
	MT_EB
	MT_FUNC("Protocol::send" << &buf << len);

	if (!ROOT->uplink)
		MT_RET(false);

	bool rv = ROOT->uplink->flack_.Write(buf, len);
	if (!rv)
		LOG(Warning, _("Failed to write to outbound flack with error #%1%."),
			ROOT->uplink->flack_.Last_Write_Error());
	ROOT->uplink->write_ = true;

	MT_RET(rv);
	MT_EE
}

bool Protocol::IsServer(const std::string &in)
{
	MT_EB
	MT_FUNC("Protocol::IsServer" << in);

	bool rv (in.find('.') != std::string::npos);
	MT_RET(rv);

	MT_EE
}

bool Protocol::IsChannel(const std::string &in)
{
	MT_EB
	MT_FUNC("Protocol::IsChannel" << in);

	switch (in[0])
	{
	case '#':
	case '+':
		MT_RET(true);
	}

	MT_RET(false);

	MT_EE
}

std::string Protocol::NumericToID(unsigned int numeric)
{
	MT_EB
	MT_FUNC("Protocol::NumericToID" << numeric);

	std::string rv;
	if (enc_bits)
	{
		unsigned char buf[128];
		memset(buf, encoding[0], 128);

		int i = 128, enc_sz = 1 << enc_bits;
		do
		{
			buf[i--] = encoding[numeric & (enc_sz - 1)];
		}
		while (i >= 0 && (numeric >>= enc_bits));

		if (opt_protocol["trim"].as<bool>())
			rv.assign((char *) buf + i, 128 - i);
		else
			rv.assign((char *) buf + (128 - (1 << (8-enc_bits))),
					  1 << (8-enc_bits));
	}
	MT_RET(rv);

	MT_EE
}

unsigned int Protocol::IDToNumeric(const std::string &id)
{
	MT_EB
	MT_FUNC("Protocol::IDToNumeric" << id);

	unsigned int rv = 0;
	for (size_t i=0; i<id.size(); ++i)
	{
		rv <<= enc_bits;
		rv += rev_encoding[id[i]];
	}
	MT_RET(rv);
	MT_EE
}

void Protocol::Decode(std::string &in, std::deque<Message> &out, bool usetokens)
{
	MT_EB
	MT_FUNC("Protocol::Decode" << in << out << usetokens);

	static boost::char_separator<char> sep("\r\n");
	static boost::char_separator<char> sep2(" \t");

	std::string::size_type offs = in.find_last_of("\r\n");
	if (offs == std::string::npos)
		return;

	typedef boost::tokenizer<boost::char_separator<char>,
			std::string::const_iterator, std::string> tokenizer;
	tokenizer tokens(in.begin(), in.begin() + offs + 1, sep);
	for (tokenizer::const_iterator i = tokens.begin(); i != tokens.end(); ++i)
	{
		LOG(Debug+1, "Received message: %1%", *i);

		std::string source, id;
		Message::Class_t c;
		std::string::size_type epos, pos = 0;
		
		// Source ...
		if ((*i)[0] == ':')
		{
			++pos;
			epos = i->find_first_of(" \t", pos);
			if (epos == std::string::npos)
			{
				LOG(Error, _("Invalid message from server \"%1%\", ignored."), *i);
				continue;
			}
			source = i->substr(pos, epos-pos);
			pos = i->find_first_not_of(" \t", epos);
		}

		epos = i->find_first_of(" \t", pos);
		if (epos == std::string::npos)
			id = i->substr(pos);
		else
			id = i->substr(pos, epos-pos);
		if (usetokens)
		{
			std::map<std::string, std::string>::const_iterator j = fwd_tokens.find(id);
			if (j != fwd_tokens.end())
				id = j->second;
		}

		boost::algorithm::to_upper(id);
		std::map<std::string, Message::Class_t>::const_iterator j = classes.find(id);
		Message m((j != classes.end() ? j->second : Message::Unknown), source, id);

		if (epos != std::string::npos)
			pos = i->find_first_not_of(" \t", epos);
		else
			pos = std::string::npos;
		if (pos != std::string::npos)
		{
			epos = i->find(":", pos);
			std::string str(i->substr(pos, epos-pos));
			tokenizer tokens2(str, sep2);
			m.params_.assign(tokens2.begin(), tokens2.end());
			if (epos != std::string::npos && epos < i->size() - 1)
				m.params_.push_back(i->substr(epos+1));
		}

		out.push_back(m);
	}
	in.erase(0, offs + 1);

	MT_EE
}

bool Protocol::Connect(const Uplink &s, const std::string &password)
{
	MT_EB
	MT_FUNC("Protocol::Connect" << s << password);

	std::string out;

	if (opt_protocol.count("capabilities"))
		addline(out, opt_protocol["capabilities"].as<std::string>());

	std::string tmp;
	if (opt_protocol.count("tsora"))
	{
		unsigned int ts = opt_protocol["tsora"].as<unsigned int>();
		if (ts > 7)
			tmp = (boost::format(" :TS%1%") % ts).str();
		else if (ts)
			tmp = " :TS";
	}

	addline(out, "PASS " + password + tmp);

	boost::posix_time::ptime curr = boost::posix_time::second_clock::local_time();
	struct tm tm_curr = boost::posix_time::to_tm(curr);
	struct tm tm_start = boost::posix_time::to_tm(ROOT->Start());

	addline(out, (format(opt_protocol["server"].as<std::string>()) %
			s.Name() % 1 % s.Description() %
			(opt_protocol["numeric.server-numeric"].as<bool>()
				? boost::lexical_cast<std::string>(IDToNumeric(s.ID()))
				: s.ID()) %
			std::mktime(&tm_curr) % std::mktime(&tm_start)).str());

	if (opt_protocol.count("tsora"))
	{
		unsigned int ts = opt_protocol["tsora"].as<unsigned int>();

		addline(out, (boost::format("SVINFO %1% 1 0 :%2%") % ts %
				std::mktime(&tm_curr)).str());
	}

	bool rv = send(out);
	MT_RET(rv);

	MT_EE
}

bool Protocol::SQUIT(const Uplink &s, const std::string &reason)
{
	MT_EB
	MT_FUNC("Protocol::SQUIT" << s << reason);

	std::string out;
	addline(out, tokenise("SQUIT") + " :" + reason);
	bool rv = send(out);
	MT_RET(rv);

	MT_EE
}

