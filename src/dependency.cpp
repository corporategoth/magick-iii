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
#define RCSID(x,y) const char *rcsid_magick__dependency_cpp_ ## x () { return y; }
RCSID(magick__dependency_cpp, "@(#)$Id$");

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
#include "livechannel.h"

#include <mantra/core/trace.h>

void Dependency::operator()()
{
	MT_EB
	MT_FUNC("Dependency::operator()");

	// So we don't try to cancel ourselves anymore.
	event_ = 0;

	boost::shared_ptr<Uplink> uplink = ROOT->getUplink();
	if (uplink)
	{
		boost::mutex::scoped_lock scoped_lock(uplink->de.lock_);
		std::multimap<Type_t, std::string>::const_iterator i;
		for (i=outstanding_.begin(); i!=outstanding_.end(); ++i)
		{
			std::multimap<std::string, boost::shared_ptr<Dependency> >::iterator k;
			for (k = uplink->de.outstanding_[i->first].lower_bound(i->second);
				 k != uplink->de.outstanding_[i->first].upper_bound(i->second); ++k)
			{
				if (this == k->second.get())
				{
					uplink->de.outstanding_[i->first].erase(k);
					break;
				}
			}
		}
		outstanding_.clear();
	}

	MT_EE
}

Dependency::Dependency(const Message &msg) : msg_(msg), event_(0)
{
	MT_EB
	MT_FUNC("Dependency::Dependency" << msg);

	Update();
	if (!outstanding_.empty())
		event_ = ROOT->event->Schedule(boost::bind(&Dependency::operator(), this),
									   ROOT->ConfigValue<mantra::duration>("general.message-expire"));

	MT_EE
}

Dependency::~Dependency()
{
	MT_EB
	MT_FUNC("Dependency::~Dependency");

	if (event_)
		ROOT->event->Cancel(event_);

	boost::shared_ptr<Uplink> uplink = ROOT->getUplink();
	if (uplink)
		uplink->Push(msg_);

	MT_EE
}

bool Dependency::Exists(Type_t type, const std::string &meta) const
{
	MT_EB
	MT_FUNC("Dependency::Exists" << type << meta);

	outstanding_t::const_iterator begin = outstanding_.lower_bound(type);
	if (begin == outstanding_.end() || begin->first != type)
		MT_RET(false);

	outstanding_t::const_iterator iter, end = outstanding_.upper_bound(type);
	for (iter = begin; iter != end; ++iter)
	{
		if (iter->second == meta)
			MT_RET(true);
	}
	MT_RET(false);

	MT_EE
}

bool Dependency::Add(Type_t type, const std::string &meta)
{
	MT_EB
	MT_FUNC("Dependency::Add" << type << meta);

	if (Exists(type, meta))
		MT_RET(false);

	boost::shared_ptr<Uplink> uplink = ROOT->getUplink();
	switch (type)
	{
	case ServerExists:
		if (uplink->Find(meta))
			MT_RET(false);
		outstanding_.insert(std::make_pair(type, meta));
		break;
	case ServerNotExists:
		if (!uplink->Find(meta))
			MT_RET(false);
		outstanding_.insert(std::make_pair(type, meta));
		break;
	case NickExists:
		if (ROOT->data.Get_LiveUser(meta))
			MT_RET(false);
		outstanding_.insert(std::make_pair(type, meta));
		break;
	case NickNotExists:
		if (!ROOT->data.Get_LiveUser(meta))
			MT_RET(false);
		outstanding_.insert(std::make_pair(type, meta));
		break;
	case ChannelExists:
		if (ROOT->data.Get_LiveChannel(meta))
			MT_RET(false);
		outstanding_.insert(std::make_pair(type, meta));
		break;
	case ChannelNotExists:
		if (!ROOT->data.Get_LiveChannel(meta))
			MT_RET(false);
		outstanding_.insert(std::make_pair(type, meta));
		break;
	default:
		MT_RET(false);
	}

	MT_RET(true);
	MT_EE
}

bool Dependency::Add(Type_t type, const std::string &meta1, const std::string &meta2)
{
	MT_EB
	MT_FUNC("Dependency::Add" << type << meta1 << meta2);

	if (Exists(type, meta1 + ":" + meta2))
		MT_RET(false);

	boost::shared_ptr<Uplink> uplink = ROOT->getUplink();
	boost::shared_ptr<LiveChannel> channel = ROOT->data.Get_LiveChannel(meta1);
	boost::shared_ptr<LiveUser> user = ROOT->data.Get_LiveUser(meta2);

	switch (type)
	{
	case NickInChannel:
		if (channel && user && channel->IsUser(user))
			MT_RET(false);
		outstanding_.insert(std::make_pair(type, meta1 + ":" + meta2));
		break;
	case NickNotInChannel:
		if (!channel || !user || !channel->IsUser(user))
			MT_RET(false);
		outstanding_.insert(std::make_pair(type, meta1 + ":" + meta2));
		break;
	default:
		MT_RET(false);
	}

	MT_RET(true);
	MT_EE
}

void Dependency::Update()
{
	MT_EB
	MT_FUNC("Dependency::Update");

	if (!msg_.Source().empty())
	{
		if (ROOT->proto.IsServer(msg_.Source()))
			Add(ServerExists, msg_.Source());
		else if (!ROOT->proto.IsChannel(msg_.Source()))
			Add(NickExists, msg_.Source());
	}

	if (msg_.Params().empty())
		return;

	boost::shared_ptr<Uplink> uplink = ROOT->getUplink();
	if (!uplink)
		return;

	if (msg_.ID() == "CREATE")
	{
		boost::char_separator<char> sep(",");
		typedef boost::tokenizer<boost::char_separator<char>,
			std::string::const_iterator, std::string> tokenizer;
		tokenizer tokens(msg_.Params()[0], sep);
		tokenizer::iterator i;
		for (i = tokens.begin(); i != tokens.end(); ++i)
			Add(ChannelNotExists, *i);
	}
	else if (msg_.ID() == "CYCLE")
	{
		if (ROOT->proto.IsServer(msg_.Source()) ||
			ROOT->proto.IsChannel(msg_.Source()))
			return;

		Add(ChannelExists, msg_.Params()[0]);
		Add(NickInChannel, msg_.Source(), msg_.Params()[0]);
	}
	else if (msg_.ID() == "JOIN")
	{
		boost::char_separator<char> sep(",");
		typedef boost::tokenizer<boost::char_separator<char>,
			std::string::const_iterator, std::string> tokenizer;
		tokenizer tokens(msg_.Params()[0], sep);
		tokenizer::iterator i;
		for (i = tokens.begin(); i != tokens.end(); ++i)
			Add(NickNotInChannel, msg_.Source(), *i);
	}
	else if (msg_.ID() == "KICK")
	{
		Add(ChannelExists, msg_.Params()[0]);
		Add(NickExists, msg_.Params()[1]);
		Add(NickInChannel, msg_.Params()[1], msg_.Params()[0]);
	}
	else if (msg_.ID() == "KILL")
	{
		Add(NickExists, msg_.Params()[0]);
	}
	else if (msg_.ID() == "MODE")
	{
		if (ROOT->proto.IsChannel(msg_.Params()[0]))
		{
			Add(ChannelExists, msg_.Params()[0]);

			if (msg_.Params().size() > 2)
			{
				boost::char_separator<char> sep(" \t");
				typedef boost::tokenizer<boost::char_separator<char>,
					std::string::const_iterator, std::string> tokenizer;
				tokenizer tokens(msg_.Params()[msg_.Params().size()-1], sep);
				tokenizer::iterator j = tokens.begin();

				std::string::const_iterator i;
				for (i = msg_.Params()[1].begin(); i != msg_.Params()[1].end(); ++i)
				{
					if (ROOT->proto.ConfigValue<std::string>("channel-mode-params").find(*i) != std::string::npos)
					{
						switch (*i)
						{
						case 'o':
						case 'h':
						case 'v':
						case 'q':
						case 'u':
						case 'a':
							Add(NickInChannel, *j);
							break;
						}
						++j;
					}
				}
			}
		}
		else if (!ROOT->proto.IsServer(msg_.Params()[0]))
			Add(NickExists, msg_.Params()[0]);
	}
	else if (msg_.ID() == "NICK")
	{
		if (msg_.Params().size() > ROOT->proto.Param_Nick_Name())
			Add(NickNotExists, msg_.Params()[ROOT->proto.Param_Nick_Name()]);
//        else if (msg_.Params().size() > ROOT->proto.Param_Nick_ID())
//            Add(NickNotExists, msg_.Params()[ROOT->proto.Param_Nick_ID()]);
		else if (msg_.Params().size() > ROOT->proto.Param_Nick_Server())
			Add(ServerExists, msg_.Params()[ROOT->proto.Param_Nick_Server()]);
	}
	else if (msg_.ID() == "PART")
	{
		Add(ChannelExists, msg_.Params()[0]);
		Add(NickInChannel, msg_.Source(), msg_.Params()[0]);
	}
	else if (msg_.ID() == "PING")
	{
		if (ROOT->proto.IsServer(msg_.Params()[0]))
			Add(ServerExists, msg_.Params()[0]);
		else if (!ROOT->proto.IsChannel(msg_.Params()[0]))
			Add(NickExists, msg_.Params()[0]);

		if (msg_.Params().size() > 1)
		{
			if (ROOT->proto.IsServer(msg_.Params()[1]))
				Add(ServerExists, msg_.Params()[1]);
			else if (!ROOT->proto.IsChannel(msg_.Params()[1]))
				Add(NickExists, msg_.Params()[1]);
		}
	}
	else if (msg_.ID() == "PONG")
	{
		if (ROOT->proto.IsServer(msg_.Params()[0]))
			Add(ServerExists, msg_.Params()[0]);
		else if (!ROOT->proto.IsChannel(msg_.Params()[0]))
			Add(NickExists, msg_.Params()[0]);
	}
	else if (msg_.ID() == "PRIVMSG")
	{
		if (ROOT->proto.IsServer(msg_.Params()[0]))
			Add(ServerExists, msg_.Params()[0]);
		else if (!ROOT->proto.IsChannel(msg_.Params()[0]))
			Add(NickExists, msg_.Params()[0]);
		else
			Add(ChannelExists, msg_.Params()[0]);
	}
	else if (msg_.ID() == "SERVER")
	{
		if (msg_.Params().size() > ROOT->proto.Param_Server_Name())
			Add(ServerNotExists, msg_.Params()[ROOT->proto.Param_Server_Name()]);
//        else if (msg_.Params().size() > ROOT->proto.Param_Server_ID())
//            Add(ServerNotExists, msg_.Params()[ROOT->proto.Param_Server_ID()]);
	}
	else if (msg_.ID() == "SJOIN")
	{
		if (ROOT->proto.IsServer(msg_.Source()))
		{
			if (msg_.Params().size() > 3)
			{
				Add(ChannelNotExists, msg_.Params()[1]);
				boost::char_separator<char> sep(" \t");
				typedef boost::tokenizer<boost::char_separator<char>,
					std::string::const_iterator, std::string> tokenizer;
				tokenizer tokens(msg_.Params()[msg_.Params().size()-1], sep);
				tokenizer::iterator i;
				for (i = tokens.begin(); i != tokens.end(); ++i)
				{
					if (i->find("&\"") == std::string::npos)
					{
						std::string::size_type j = i->find_first_not_of("@%+*.~");
						if (j == std::string::npos)
							j = 0;
						std::string user = i->substr(j);
						Add(NickExists, user);
						Add(NickNotInChannel, user);
					}
				}
			}
		}
		else if (!ROOT->proto.IsChannel(msg_.Source()))
		{
			for (size_t i=1; i<msg_.Params().size(); ++i)
				Add(NickNotInChannel, msg_.Params()[i]);
		}
	}
	else if (msg_.ID() == "SQUIT")
	{
		Add(ServerExists, msg_.Params()[0]);
	}
	else if (msg_.ID() == "TMODE")
	{
		if (ROOT->proto.IsChannel(msg_.Params()[0]))
		{
			Add(ChannelExists, msg_.Params()[0]);

			if (msg_.Params().size() > 3)
			{
				boost::char_separator<char> sep(" \t");
				typedef boost::tokenizer<boost::char_separator<char>,
					std::string::const_iterator, std::string> tokenizer;
				tokenizer tokens(msg_.Params()[msg_.Params().size()-1], sep);
				tokenizer::iterator j = tokens.begin();

				std::string::const_iterator i;
				for (i = msg_.Params()[2].begin(); i != msg_.Params()[2].end(); ++i)
				{
					if (ROOT->proto.ConfigValue<std::string>("channel-mode-params").find(*i) != std::string::npos)
					{
						switch (*i)
						{
						case 'o':
						case 'h':
						case 'v':
						case 'q':
						case 'u':
						case 'a':
							Add(NickInChannel, *j);
							break;
						}
						++j;
					}
				}
			}
		}
		else if (!ROOT->proto.IsServer(msg_.Params()[0]))
			Add(NickExists, msg_.Params()[0]);
	}
	else if (msg_.ID() == "TOPIC")
	{
		Add(ChannelExists, msg_.Params()[0]);
	}
	else if (msg_.ID() == "USER")
	{
		if (msg_.Params().size() > ROOT->proto.Param_Nick_Name())
			Add(NickNotExists, msg_.Params()[ROOT->proto.Param_Nick_Name()]);
//        else if (msg_.Params().size() > ROOT->proto.Param_Nick_ID())
//            Add(NickNotExists, msg_.Params()[ROOT->proto.Param_Nick_ID()]);
		else if (msg_.Params().size() > ROOT->proto.Param_Nick_Server())
			Add(ServerExists, msg_.Params()[ROOT->proto.Param_Nick_Server()]);
	}

	MT_EE
}

void Dependency::Satisfy(Dependency::Type_t type, const std::string &meta)
{
	MT_EB
	MT_FUNC("Satisfy");

	// We don't need to be locked, since this will only
	// be called by DependencyEngine, under its lock.
	std::multimap<Type_t, std::string>::iterator i;
	for (i=outstanding_.lower_bound(type); i != outstanding_.upper_bound(type); ++i)
	{
		if (i->second == meta)
		{
			outstanding_.erase(i);
			break;
		}
	}

#if 0
	if (outstanding_.empty() && !msg_.ID().empty())
	{
		if (event_)
		{
			ROOT->event->Cancel(event_);
			event_ = 0;
		}

		boost::shared_ptr<Uplink> uplink = ROOT->getUplink();
		if (uplink)
		{
			uplink->Push(msg_);
			msg_ = Message();
		}
	}
#endif

	MT_EE
}

void DependencyEngine::Add(const Message &m)
{
	MT_EB
	MT_FUNC("DependencyEngine::Add" << m);

	boost::shared_ptr<Dependency> ptr(new Dependency(m));
	if (!ptr->Outstanding().empty())
	{
		boost::mutex::scoped_lock scoped_lock(lock_);
		std::multimap<Dependency::Type_t, std::string>::const_iterator i;
		for (i=ptr->Outstanding().begin(); i!=ptr->Outstanding().end(); ++i)
			outstanding_[i->first].insert(std::make_pair(i->second, ptr));
	}

	MT_EE
}

void DependencyEngine::Satisfy(Dependency::Type_t type, const std::string &meta)
{
	MT_EB
	MT_FUNC("DependencyEngine::Satisfy" << type << meta);

	boost::mutex::scoped_lock scoped_lock(lock_);
	std::multimap<std::string, boost::shared_ptr<Dependency> >::iterator j =
		outstanding_[type].lower_bound(meta);
	while (j != outstanding_[type].upper_bound(meta))
	{
		j->second->Satisfy(type, meta);
		outstanding_[type].erase(j++);
	}

	MT_EE
}

