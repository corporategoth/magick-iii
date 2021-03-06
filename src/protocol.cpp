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
#include "liveuser.h"

#include <fstream>

#include <mantra/core/trace.h>
#include <mantra/core/typed_value.h>
#include <mantra/core/unformat.h>

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

Protocol::Protocol() : usetokens(false), enc_bits(0)
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

		// %1% = server     %5% = set time         %9% = duration (mins)
		// %2% = user       %6% = expiry time      %10% = reason
		// %3% = host       %7% = current time     %11% = remaining (secs)
		// %4% = setter     %8% = duration (secs)  %12% = remaining (mins)
		("akill", mantra::value<std::string>(),
					"syntax of the AKILL command to use.")
		("unakill", mantra::value<std::string>(),
					"syntax used to reverse an AKILL (if any).")
		("kill-after-akill", mantra::value<bool>()->default_value(true),
					"is a KILL required after an AKILL message")

		// %1% = nick     %5% = signon time       %9%  = realname
		// %2% = user     %6% = hops              %10% = service flag
		// %3% = host     %7% = ident             %11% = alt. host
		// %4% = server   %8% = modes             %12% = IP address
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
	if (!opt_protocol["token"].empty())
	{
		std::vector<std::pair<std::string, std::string> > tmp =
			opt_protocol["token"].as<std::vector<std::pair<std::string, std::string> > >();
		std::vector<std::pair<std::string, std::string> >::iterator i;
		for (i=tmp.begin(); i!=tmp.end(); ++i)
		{
			fwd_tokens[i->first] = i->second;
			rev_tokens[i->second] = i->first;
		}
	}

	if (!opt_protocol["numeric.encoding"].empty())
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
			rev_encoding[static_cast<size_t>(*(i->data()))] = j;
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

	param_server_name = param_server_id = 0;
	typedef boost::tokenizer<boost::char_separator<char>,
			std::string::const_iterator, std::string> tokenizer;
	boost::char_separator<char> sep(" \t");
	tokenizer tokens(opt_protocol["server"].as<std::string>(), sep);
	tokenizer::const_iterator i = tokens.begin();
	size_t j = 0;
	for (++i; i!=tokens.end(); ++i, ++j)
	{
		if (*i == "%1%")
			param_server_name = j;
		else if (*i == "%4%")
			param_server_id = j;
	}

	param_nick_name = param_nick_id = param_nick_server = 0;
	tokenizer tokens2(opt_protocol["nick"].as<std::string>(), sep);
	i = tokens.begin();
	j = 0;
	for (++i; i!=tokens.end(); ++i, ++j)
	{
		if (*i == "%1%")
			param_nick_name = j;
		else if (*i == "%4%")
			param_nick_server = j;
		else if (*i == "%7%")
			param_nick_id = j;
	}

	MT_RET(true);
	MT_EE
}

const std::string &Protocol::tokenise(const std::string &in) const
{
	MT_EB
	MT_FUNC("Protocol::tokenise");

	if (rev_tokens.empty() || !usetokens || !ROOT->getUplink())
		MT_RET(in);

	std::map<std::string, std::string, mantra::iless<std::string> >::const_iterator iter =
		rev_tokens.find(in);
	if (iter == rev_tokens.end())
		MT_RET(in);

	MT_RET(iter->second);
	MT_EE
}

std::string Protocol::assemble(const std::vector<std::string> &in, bool forcecolon) const
{
	MT_EB
	MT_FUNC("Protocol::assemble" << in << forcecolon);

	if (in.empty())
		return std::string();

	std::string rv;
	for (size_t i=0; i<in.size() - 1; ++i)
		rv += " " + in[i];
	if (forcecolon || in.back().find(" ") != std::string::npos)
		rv += " :" + in.back();
	else
		rv += " " + in.back();

	MT_RET(rv);
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

void Protocol::addline(const Jupe &s, std::string &out, const std::string &in) const
{
	MT_EB
	MT_FUNC("Protocol::addline" << s << &out << in);

	addline(out, ":" + s.Name() + " " + in);

	MT_EE
}

void Protocol::addline(const LiveUser &u, std::string &out, const std::string &in) const
{
	MT_EB
	MT_FUNC("Protocol::addline" << u << &out << in);

	addline(out, ":" + u.Name() + " " + in);

	MT_EE
}

bool Protocol::send(const char *buf, boost::uint64_t len) const
{
	MT_EB
	MT_FUNC("Protocol::send" << &buf << len);

	boost::shared_ptr<Uplink> uplink = ROOT->getUplink();
	if (!uplink)
		MT_RET(false);

	bool rv = uplink->Write(buf, len);
	if (!rv)
		LOG(Warning, _("Failed to write to outbound flack with error #%1%."),
			uplink->flack_.Last_Write_Error());

	MT_RET(rv);
	MT_EE
}

bool Protocol::IsServer(const std::string &in) const
{
	MT_EB
	MT_FUNC("Protocol::IsServer" << in);

	bool rv = (in.find('.') != std::string::npos);
	MT_RET(rv);

	MT_EE
}

bool Protocol::IsChannel(const std::string &in) const
{
	MT_EB
	MT_FUNC("Protocol::IsChannel" << in);

	std::string::const_iterator i = in.begin();
	switch (*i)
	{
	case '#':
	case '+':
		for (; i != in.end(); ++i)
			if (isspace(*i) || iscntrl(*i))
				MT_RET(false);
		MT_RET(true);
	}

	MT_RET(false);

	MT_EE
}

std::string Protocol::NumericToID(unsigned int numeric) const
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
			rv.assign(reinterpret_cast<char *>(buf + i), 128 - i);
		else
			rv.assign(reinterpret_cast<char *>(buf + (128 - (1 << (8-enc_bits)))),
					  1 << (8-enc_bits));
	}
	MT_RET(rv);

	MT_EE
}

unsigned int Protocol::IDToNumeric(const std::string &id) const
{
	MT_EB
	MT_FUNC("Protocol::IDToNumeric" << id);

	unsigned int rv = 0;
	for (size_t i=0; i<id.size(); ++i)
	{
		rv <<= enc_bits;
		rv += rev_encoding[static_cast<size_t>(id[i])];
	}
	MT_RET(rv);
	MT_EE
}

void Protocol::Decode(std::string &in, std::deque<Message> &out) const
{
	MT_EB
	MT_FUNC("Protocol::Decode" << in << out);

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
		Message m(source, id);

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

bool Protocol::Connect(const Jupe &s)
{
	MT_EB
	MT_FUNC("Protocol::Connect" << s);

	std::string out;

	boost::shared_ptr<Server> up = s.Parent();
	Jupe *j = dynamic_cast<Jupe *>(up.get());
	if (!j)
		MT_RET(false);

	size_t count = 1;
	while (up)
	{
		++count;
		up = s.Parent();
	}

	boost::posix_time::ptime curr = boost::posix_time::second_clock::local_time();
	struct tm tm_curr = boost::posix_time::to_tm(curr);
	struct tm tm_start = boost::posix_time::to_tm(ROOT->Start());

	addline(*j, out, (format(opt_protocol["server"].as<std::string>()) % s.Name() % count %
				  s.Description() % (opt_protocol["numeric.server-numeric"].as<bool>()
					? boost::lexical_cast<std::string>(IDToNumeric(s.ID()))
					: s.ID()) % std::mktime(&tm_curr) % std::mktime(&tm_start)).str());

	bool rv = send(out);
	MT_RET(rv);
	MT_EE
}

bool Protocol::Connect(const Uplink &s)
{
	MT_EB
	MT_FUNC("Protocol::Connect" << s);

	std::string out;

	usetokens = false;
	if (!opt_protocol["capabilities"].empty())
		addline(out, opt_protocol["capabilities"].as<std::string>());

	std::string tmp;
	if (!opt_protocol["tsora"].empty())
	{
		unsigned int ts = opt_protocol["tsora"].as<unsigned int>();
		if (ts > 7)
			tmp = (boost::format(" :TS%1%") % ts).str();
		else if (ts)
			tmp = " :TS";
	}

	addline(out, "PASS " + s.password_ + tmp);

	boost::posix_time::ptime curr = boost::posix_time::second_clock::local_time();
	struct tm tm_curr = boost::posix_time::to_tm(curr);
	struct tm tm_start = boost::posix_time::to_tm(ROOT->Start());

	addline(out, (format(opt_protocol["server"].as<std::string>()) % s.Name() % 1 %
				  s.Description() % (opt_protocol["numeric.server-numeric"].as<bool>()
					? boost::lexical_cast<std::string>(IDToNumeric(s.ID()))
					: s.ID()) % std::mktime(&tm_curr) % std::mktime(&tm_start)).str());

	if (!opt_protocol["tsora"].empty())
	{
		unsigned int ts = opt_protocol["tsora"].as<unsigned int>();

		addline(out, (boost::format("SVINFO %1% 1 0 :%2%") % ts %
				std::mktime(&tm_curr)).str());
	}

	if (opt_protocol["burst-end"].as<std::string>().empty())
		addline(out, "PING :" + s.Name());

	bool rv = send(out);
	MT_RET(rv);

	MT_EE
}

void Protocol::BurstEnd() const
{
	MT_EB
	MT_FUNC("Protocol::BurstEnd");

	boost::shared_ptr<Uplink> uplink = ROOT->getUplink();
	if (!uplink)
		return;

	{
		SYNCP_LOCK(uplink, burst_);
		if (uplink->burst_)
			return;

		uplink->burst_ = true;
	}

	ROOT->nickserv.Check();
	ROOT->chanserv.Check();
	ROOT->memoserv.Check();
	ROOT->operserv.Check();
	ROOT->commserv.Check();
	ROOT->other.Check();

	MT_EE
}

boost::shared_ptr<Server> Protocol::ParseServer(const Message &in) const
{
	MT_EB
	MT_FUNC("Protocol::ParseServer" << in);

	static mantra::unformat unformatter;
	if (unformatter.EmptyElementRegex())
	{
		static boost::mutex lock;
		boost::mutex::scoped_lock scoped_lock(lock);
		if (unformatter.EmptyElementRegex())
		{
			unformatter.ElementRegex(1, "(?:[[:alnum:]][-[:alnum:]]*\\.)*"
									    "[[:alnum:]][-[:alnum:]]*"); // host
			unformatter.ElementRegex(2, "[[:digit:]]+"); // hops
			// unformatter.ElementRegex(3, ".*"); // desc
			unformatter.ElementRegex(4, "[^[:space:]]+"); // numeric
			unformatter.ElementRegex(5, "[[:digit:]]+"); // start time
			unformatter.ElementRegex(6, "[[:digit:]]+"); // current time
		}
	}

	boost::shared_ptr<Server> serv;
	mantra::unformat::elem_map elems;
	if (!unformatter(opt_protocol["server"].as<std::string>(),
					 in.ID() + assemble(in.Params(), true), elems,
					 mantra::unformat::MergeSpaces))
		MT_RET(serv);

	mantra::unformat::elem_map::iterator i = elems.find(1);
	if (i == elems.end())
		MT_RET(serv);
	std::string name = i->second;

	i = elems.find(3);
	if (i == elems.end())
		MT_RET(serv);
	std::string desc = i->second;

	std::string id;
	i = elems.find(4);
	if (i != elems.end())
	{
		if (opt_protocol["numeric.server-numeric"].as<bool>())
			id = NumericToID(boost::lexical_cast<unsigned int>(i->second));
		else
			id = i->second;
	}

	serv = Server::create(name, desc, id);
	MT_RET(serv);
	MT_EE
}

boost::shared_ptr<LiveUser> Protocol::ParseUser(const Message &in) const
{
	MT_EB
	MT_FUNC("Protocol::ParseUser" << in);

	static mantra::unformat unformatter;
	if (unformatter.EmptyElementRegex())
	{
		static boost::mutex lock;
		boost::mutex::scoped_lock scoped_lock(lock);
		if (unformatter.EmptyElementRegex())
		{
			unformatter.ElementRegex(1, "[[:alpha:]\\\\x5B-\\\\x60\\\\x7B-\\\\x7D]"
										"[-[:alnum:]\\\\x5B-\\\\x60\\\\x7B-\\\\x7D]*"); // nick
			unformatter.ElementRegex(2, "[^[:space:][:cntrl:]@]+"); // user
			unformatter.ElementRegex(3, "[-.[:alnum:]]+"); // host (verify later)
			unformatter.ElementRegex(4, "(?:[[:alnum:]][-[:alnum:]]*[.])*"
										"[[:alnum:]][-[:alnum:]]*"); // server
			unformatter.ElementRegex(5, "[[:digit:]]+"); // signon time
			unformatter.ElementRegex(6, "[[:digit:]]+"); // hops
			unformatter.ElementRegex(7, "[^[:space:]]+"); // numeric
			unformatter.ElementRegex(8, "[+]?[[:alpha:]]*"); // modes
			// unformatter.ElementRegex(9, ".*"); // real name
			unformatter.ElementRegex(10, "[[:digit:]]+"); // service flag
			unformatter.ElementRegex(11, "[-.[:alnum:]]+"); // alt. host (verify later)
			unformatter.ElementRegex(12, "[.:[:xdigit:]]+"); // current time
		}
	}

	boost::shared_ptr<LiveUser> liveuser;
	mantra::unformat::elem_map elems;
	if (!unformatter(opt_protocol["nick"].as<std::string>(),
					 in.ID() + assemble(in.Params(), true), elems,
					 mantra::unformat::MergeSpaces))
		MT_RET(liveuser);

	mantra::unformat::elem_map::iterator i = elems.find(1);
	if (i == elems.end())
		MT_RET(liveuser);
	std::string name = i->second;

	i = elems.find(2);
	if (i == elems.end())
		MT_RET(liveuser);
	std::string user = i->second;

	i = elems.find(3);
	if (i == elems.end())
		MT_RET(liveuser);
	std::string host = i->second;
	if (!(mantra::is_hostname(host) ||
		  mantra::is_inet_address(host) ||
		  mantra::is_inet6_address(host)))
		MT_RET(liveuser);

	i = elems.find(9);
	if (i == elems.end())
		MT_RET(liveuser);
	std::string real = i->second;

	i = elems.find(4);
	if (i == elems.end())
		MT_RET(liveuser);
	boost::shared_ptr<Server> serv = ROOT->getUplink()->Find(i->second);
	if (!serv)
		MT_RET(liveuser);

	boost::posix_time::ptime signon(boost::date_time::not_a_date_time);
	i = elems.find(5);
	if (i != elems.end())
		signon = boost::posix_time::from_time_t(boost::lexical_cast<time_t>(i->second));
	else
		signon = mantra::GetCurrentDateTime();

	std::string id;
	i = elems.find(7);
	if (i != elems.end())
	{
		if (opt_protocol["numeric.server-numeric"].as<bool>())
			id = NumericToID(boost::lexical_cast<unsigned int>(i->second));
		else
			id = i->second;
	}

	std::string modes;
	i = elems.find(8);
	if (i != elems.end())
		modes = i->second;

	std::string althost;
	i = elems.find(11);
	if (i != elems.end())
	{
		althost = i->second;
		if (!(mantra::is_hostname(althost) ||
			  mantra::is_inet_address(althost) ||
			  mantra::is_inet6_address(althost)))
			MT_RET(liveuser);
	}

	liveuser = LiveUser::create(name, real, user, host, serv, signon, id);
	if (!modes.empty())
		liveuser->Modes(modes);
	if (!althost.empty())
		liveuser->AltHost(althost);

	MT_RET(liveuser);
	MT_EE
}

