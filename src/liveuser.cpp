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
#define RCSID(x,y) const char *rcsid_magick__liveuser_cpp_ ## x () { return y; }
RCSID(magick__liveuser_cpp, "@(#)$Id$");

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

#include "livechannel.h"
#include "storednick.h"
#include "storeduser.h"
#include "storedchannel.h"
#include "committee.h"

#include <mantra/core/trace.h>

#include <boost/algorithm/string.hpp>

LiveUser::LiveUser(const Service *service, const std::string &name,
				   const std::string &real,
				   const boost::shared_ptr<Server> &server,
				   const std::string &id)
	: SYNC_RWINIT(name_, reader_priority, name), id_(id), real_(real),
	  server_(server), signon_(mantra::GetCurrentDateTime()),
	  seen_(mantra::GetCurrentDateTime()), service_(service),
	  flood_triggers_(0), ignored_(false), identified_(true),
	  SYNC_NRWINIT(stored_, reader_priority), password_fails_(0),
	  last_nick_reg_(boost::date_time::not_a_date_time),
	  last_channel_reg_(boost::date_time::not_a_date_time),
	  last_memo_(boost::date_time::not_a_date_time)
{
	MT_EB
	MT_FUNC("LiveUser::LiveUser" << service << name << server << id);

	if (ROOT->ConfigExists("services.user"))
		user_ = ROOT->ConfigValue<std::string>("services.user");
	else
		user_ = boost::algorithm::to_lower_copy(name_);

	if (ROOT->ConfigExists("services.host"))
		host_ = ROOT->ConfigValue<std::string>("services.host");
	else
		host_ = ROOT->ConfigValue<std::string>("server-name");

	MT_EE
}

LiveUser::LiveUser(const std::string &name, const std::string &real,
				   const std::string &user, const std::string &host,
				   const boost::shared_ptr<Server> &server,
				   const boost::posix_time::ptime &signon,
				   const std::string &id)
	: SYNC_RWINIT(name_, reader_priority, name),
	  id_(id), real_(real), user_(user), host_(host),
	  server_(server), signon_(signon), seen_(mantra::GetCurrentDateTime()),
	  service_(NULL), flood_triggers_(0), ignored_(false), identified_(false),
	  SYNC_NRWINIT(stored_, reader_priority), password_fails_(0),
	  last_nick_reg_(boost::date_time::not_a_date_time),
	  last_channel_reg_(boost::date_time::not_a_date_time),
	  last_memo_(boost::date_time::not_a_date_time)
{
	MT_EB
	MT_FUNC("LiveUser::LiveUser" << name << real << user << host << server << signon << id);

	boost::shared_ptr<StoredNick> stored = ROOT->data.Get_StoredNick(name);
	if (stored && stored->User()->ACCESS_Matches(user + "@" + host))
	{
		stored_ = stored;
		if_StoredNick_LiveUser(stored_).Live(self.lock());
	}

	MT_EE
}

void LiveUser::Stored(const boost::shared_ptr<StoredNick> &nick)
{
	MT_EB
	MT_FUNC("LiveUser::Stored" << nick);

	// This only happens because of a new registration / link.
	SYNC_WLOCK(stored_);
	stored_ = nick;
	identified_ = true;

	MT_EE
}

void LiveUser::Join(const boost::shared_ptr<LiveChannel> &channel)
{
	MT_EB
	MT_FUNC("LiveUser::Join" << channel);

	SYNC_LOCK(channel_joined_);
	channel_joined_.insert(channel);

	MT_EE
}

void LiveUser::Part(const boost::shared_ptr<LiveChannel> &channel)
{
	MT_EB
	MT_FUNC("LiveUser::Part" << channel);

	SYNC_LOCK(channel_joined_);
	channel_joined_.erase(channel);

	MT_EE
}

void LiveUser::Name(const std::string &in)
{
	MT_EB
	MT_FUNC("LiveUser::Name" << in);

	{
		SYNC_WLOCK(name_);
		name_ = in;
	}
	boost::shared_ptr<StoredNick> stored = ROOT->data.Get_StoredNick(in);
	{
		SYNC_WLOCK(stored_);
		if (stored_)
			if_StoredNick_LiveUser(stored_).Live(boost::shared_ptr<LiveUser>());
		if (stored)
		{
			if (!stored_ || stored->User() != stored_->User())
			{
				identified_ = false;
				if (stored->User()->ACCESS_Matches(User() + "@" + Host()))
					stored_ = stored;
				else
					stored_.reset();
			}
			else
			{
				stored_ = stored;
			}
			if (stored_)
				if_StoredNick_LiveUser(stored_).Live(self.lock());
		}
		else
		{
			identified_ = false;
			stored_ = stored;
		}
	}

	MT_EE
}

boost::shared_ptr<StoredNick> LiveUser::Stored() const
{
	MT_EB
	MT_FUNC("LiveUser::Stored");

	SYNC_RLOCK(stored_);
	boost::shared_ptr<StoredNick> rv = stored_;

	MT_RET(rv);
	MT_EE
}

void LiveUser::Quit(const std::string &reason)
{
	MT_EB
	MT_FUNC("LiveUser::Quit" << reason);

	// Part channels
	{
		SYNC_LOCK(channel_joined_);
		channel_joined_t::const_iterator i;
		for (i=channel_joined_.begin(); i!=channel_joined_.end(); ++i)
			if_LiveChannel_LiveUser(*i).Quit(self.lock());
		channel_joined_.clear();
	}
	// UnIdentify channels
	{
		SYNC_LOCK(channel_identified_);
		channel_identified_t::const_iterator i;
		for (i=channel_identified_.begin(); i!=channel_identified_.end(); ++i)
			if_StoredChannel_LiveUser(*i).UnIdentify(self.lock());
		channel_identified_.clear();
		channel_password_fails_.clear();
	}
	// Tell nickname I quit
	{
		SYNC_WLOCK(stored_);
		if (stored_)
		{
			if_StoredNick_LiveUser(stored_).Quit(reason);
			stored_.reset();
		}
	}

	if (service_)
	{
		Service *serv = const_cast<Service *>(service_);
		SYNCP_WLOCK(serv, users_);
		serv->users_.erase(self.lock());
	}

	if_StorageDeleter<LiveUser>(ROOT->data).Del(self.lock());

	MT_EE
}

void LiveUser::Kill(const boost::shared_ptr<LiveUser> &killer,
					const std::string &reason)
{
	MT_EB
	MT_FUNC("LiveUser::Kill" << killer << reason);

	Quit("Killed: " + reason);

	MT_EE
}

void LiveUser::AltHost(const std::string &in)
{
	MT_EB
	MT_FUNC("LiveUser::AltHost" << in);

	SYNC_LOCK(LiveUser);
	alt_host_ = in;

	MT_EE
}

std::string LiveUser::AltHost() const
{
	MT_EB
	MT_FUNC("LiveUser::AltHost");

	SYNC_LOCK(LiveUser);
	std::string rv = alt_host_;
	MT_RET(rv);

	MT_EE
}

void LiveUser::Away(const std::string &in)
{
	MT_EB
	MT_FUNC("LiveUser::Away" << in);

	SYNC_LOCK(LiveUser);
	away_ = in;

	MT_EE
}

std::string LiveUser::Away() const
{
	MT_EB
	MT_FUNC("LiveUser::Away");

	SYNC_LOCK(LiveUser);
	std::string rv = away_;
	MT_RET(rv);

	MT_EE
}

void LiveUser::Modes(const std::string &in)
{
	MT_EB
	MT_FUNC("LiveUser::Modes" << in);

	SYNC_LOCK(modes_);

	MT_CB(0, modes_);
	bool add = true;
	std::string::const_iterator i;
	for (i = in.begin(); i != in.end(); ++i)
	{
		switch (*i)
		{
		case '+':
			add = true;
			break;
		case '-':
			add = false;
			break;
		default:
			if (add)
				modes_.insert(*i);
			else
				modes_.erase(*i);
		}
	}
	MT_CE(0, modes_);

	MT_EE
}

std::set<char> LiveUser::Modes() const
{
	MT_EB
	MT_FUNC("LiveUser::Modes");

	std::set<char> rv;
	SYNC_LOCK(modes_);
	rv = modes_;
	MT_RET(rv);

	MT_EE
}

void LiveUser::unignore()
{
	MT_EB
	MT_FUNC("LiveUser::unignore");

	SYNC_LOCK(messages_);
	ignored_ = false;

	MT_EE
}

bool LiveUser::Action()
{
	MT_EB
	MT_FUNC("LiveUser::Action");

	SYNC_LOCK(messages_);
	boost::posix_time::ptime now = mantra::GetCurrentDateTime();
	boost::posix_time::ptime thresh = (now -
			ROOT->ConfigValue<mantra::duration>("operserv.ignore.flood-time"));
	while (!messages_.empty())
	{
		if (thresh < messages_.front())
			break;
		messages_.pop();
	}

	messages_.push(mantra::GetCurrentDateTime());
	if (messages_.size() >= ROOT->ConfigValue<unsigned int>("operserv.ignore.flood-messages"))
	{
		if (++flood_triggers_ >= ROOT->ConfigValue<unsigned int>("operserv.ignore.limit"))
		{
			// Add to permanent ignore ...
			// Message to use re: permanent ignore.
		}
		else
		{
			ROOT->event->Schedule(boost::bind(&LiveUser::unignore, this),
								 ROOT->ConfigValue<mantra::duration>("operserv.ignore.expire"));
			// Message to user re: temp ignore.
		}
		ignored_ = true;
	}
	MT_RET(ignored_);

	MT_EE
}

bool LiveUser::Ignored() const
{
	MT_EB
	MT_FUNC("LiveUser::Ignored");

	{
		SYNC_LOCK(messages_);
		if (ignored_)
			MT_RET(true);
	}
	// Look up permanent ignore ...

	MT_RET(false);
	MT_EE
}

bool LiveUser::Identify(const std::string &in)
{
	MT_EB
	MT_FUNC("LiveUser::Identify" << in);

	SYNC_WLOCK(stored_);
	if (identified_)
		MT_RET(true);

	boost::shared_ptr<StoredNick> stored = stored_;
	if (!stored)
		stored = ROOT->data.Get_StoredNick(Name());
	if (!stored)
		MT_RET(false);

	identified_ = stored->User()->CheckPassword(in);
	if (identified_)
	{
		if (!stored_)
			if_StoredNick_LiveUser(stored).Live(self.lock());
		stored_ = stored;
		password_fails_ = 0;
	}
	else
	{
		if (++password_fails_ >= ROOT->ConfigValue<unsigned int>("nickserv.password-fail"))
		{
			// Kill user ...
		}
	}

	MT_RET(identified_);

	MT_EE
}

void LiveUser::UnIdentify()
{
	MT_EB
	MT_FUNC("LiveUser::UnIdentify");

	SYNC_WLOCK(stored_);
	identified_ = false;
	if (!stored_->User()->ACCESS_Matches(User() + "@" + Host()))
	{
		if_StoredNick_LiveUser(stored_).Live(boost::shared_ptr<LiveUser>());
		stored_.reset();
	}

	MT_EE
}

bool LiveUser::Recognized() const
{
	MT_EB
	MT_FUNC("LiveUser::Recognized");

	SYNC_RLOCK(stored_);
	bool rv = !!stored_;

	MT_RET(rv);
	MT_EE
}

bool LiveUser::Identified() const
{
	MT_EB
	MT_FUNC("LiveUser::Identified");

	SYNC_RLOCK(stored_);
	MT_RET(identified_);

	MT_EE
}

bool LiveUser::Identify(const boost::shared_ptr<StoredChannel> &channel,
						const std::string &in)
{
	MT_EB
	MT_FUNC("LiveUser::Identify" << channel << in);

	SYNC_LOCK(channel_identified_);
	if (channel_identified_.find(channel) != channel_identified_.end())
		MT_RET(true);
	if (if_StoredChannel_LiveUser(channel).Identify(self.lock(), in))
	{
		channel_identified_.insert(channel);
		channel_password_fails_.erase(channel);
		MT_RET(true);
	}
	else
	{
		std::pair<channel_password_fails_t::iterator, bool> rv =
			channel_password_fails_.insert(std::make_pair(channel, 0));
		if (++rv.first->second >= ROOT->ConfigValue<unsigned int>("chanserv.password-fail"))
		{
			// Kill user ...
		}
		MT_RET(false);
	}

	MT_EE
}

void LiveUser::UnIdentify(const boost::shared_ptr<StoredChannel> &channel)
{
	MT_EB
	MT_FUNC("LiveUser::UnIdentify" << channel);

	{
		SYNC_LOCK(channel_identified_);
		channel_identified_.erase(channel);
	}
	if_StoredChannel_LiveUser(channel).UnIdentify(self.lock());

	MT_EE
}

bool LiveUser::Identified(const boost::shared_ptr<StoredChannel> &channel) const
{
	MT_EB
	MT_FUNC("LiveUser::Identified" << channel);

	SYNC_LOCK(channel_identified_);
	bool rv = (channel_identified_.find(channel) != channel_identified_.end());

	MT_RET(rv);
	MT_EE
}

bool operator<(const LiveUser::channel_joined_t::value_type &lhs,
			   const std::string &rhs)
{
	return (*lhs < rhs);
}

bool operator<(const LiveUser::committees_t::value_type &lhs,
			   const std::string &rhs)
{
	return (*lhs < rhs);
}

bool LiveUser::InChannel(const boost::shared_ptr<LiveChannel> &channel) const
{
	MT_EB
	MT_FUNC("LiveUser::InChannel" << channel);

	SYNC_LOCK(channel_joined_);
	bool rv = (channel_joined_.find(channel) != channel_joined_.end());

	MT_RET(rv);
	MT_EE
}

bool LiveUser::InChannel(const std::string &channel) const
{
	MT_EB
	MT_FUNC("LiveUser::InChannel" << channel);

	SYNC_LOCK(channel_joined_);
	LiveUser::channel_joined_t::const_iterator i =
		std::lower_bound(channel_joined_.begin(), channel_joined_.end(), channel);
	bool rv = (i != channel_joined_.end());

	MT_RET(rv);
	MT_EE
}

bool LiveUser::InCommittee(const boost::shared_ptr<Committee> &committee) const
{
	MT_EB
	MT_FUNC("LiveUser::InCommittee" << committee);

	SYNC_LOCK(committees_);
	bool rv = (committees_.find(committee) != committees_.end());

	MT_RET(rv);
	MT_EE
}

bool LiveUser::InCommittee(const std::string &committee) const
{
	MT_EB
	MT_FUNC("LiveUser::InCommittee" << committee);

	SYNC_LOCK(committees_);
	LiveUser::committees_t::const_iterator i =
		std::lower_bound(committees_.begin(), committees_.end(), committee);
	bool rv = (i != committees_.end());

	MT_RET(rv);
	MT_EE
}

void LiveUser::Nick_Reg()
{
	MT_EB
	MT_FUNC("LiveUser::Nick_Reg");

	SYNC_LOCK(LiveUser);
	last_nick_reg_ = mantra::GetCurrentDateTime();

	MT_EE
}

boost::posix_time::ptime LiveUser::Last_Nick_Reg() const
{
	MT_EB
	MT_FUNC("LiveUser::Last_Nick_Reg");

	SYNC_LOCK(LiveUser);
	MT_RET(last_nick_reg_);

	MT_EE
}

void LiveUser::Channel_Reg()
{
	MT_EB
	MT_FUNC("LiveUser::Channel_Reg");

	SYNC_LOCK(LiveUser);
	last_channel_reg_ = mantra::GetCurrentDateTime();

	MT_EE
}

boost::posix_time::ptime LiveUser::Last_Channel_Reg() const
{
	MT_EB
	MT_FUNC("Last_Channel_Reg");

	SYNC_LOCK(LiveUser);
	MT_RET(last_channel_reg_);

	MT_EE
}

void LiveUser::Memo()
{
	MT_EB
	MT_FUNC("LiveUser::Memo");

	SYNC_LOCK(LiveUser);
	last_memo_ = mantra::GetCurrentDateTime();

	MT_EE
}

boost::posix_time::ptime LiveUser::Last_Memo() const
{
	MT_EB
	MT_FUNC("Last_Memo");

	SYNC_LOCK(LiveUser);
	MT_RET(last_memo_);

	MT_EE
}

