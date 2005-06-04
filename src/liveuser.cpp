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

LiveUser::LiveUser(Service *service, const std::string &name,
				   const std::string &real,
				   const boost::shared_ptr<Server> &server,
				   const std::string &id)
	: SYNC_RWINIT(name_, reader_priority, name), id_(id), real_(real),
	  server_(server), signon_(mantra::GetCurrentDateTime()),
	  seen_(mantra::GetCurrentDateTime()), service_(service),
	  flood_triggers_(0), ignored_(false), identified_(true),
	  SYNC_NRWINIT(stored_, reader_priority), password_fails_(0),
	  drop_token_(std::make_pair(std::string(), 0)),
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
	  drop_token_(std::make_pair(std::string(), 0)),
	  last_nick_reg_(boost::date_time::not_a_date_time),
	  last_channel_reg_(boost::date_time::not_a_date_time),
	  last_memo_(boost::date_time::not_a_date_time)
{
	MT_EB
	MT_FUNC("LiveUser::LiveUser" << name << real << user << host << server << signon << id);

	MT_EE
}

boost::shared_ptr<LiveUser> LiveUser::create(const std::string &name,
				const std::string &real, const std::string &user,
				const std::string &host,
				const boost::shared_ptr<Server> &server,
				const boost::posix_time::ptime &signon,
				const std::string &id)
{
	MT_EB
	MT_FUNC("LiveUser::create" << name << real << user << host << server << signon << id);

	boost::shared_ptr<LiveUser> rv(new LiveUser(name, real, user, host,
												server, signon, id));
	rv->self = rv;

	boost::shared_ptr<StoredNick> stored = ROOT->data.Get_StoredNick(name);
	if (stored && !stored->User()->Secure() &&
		stored->User()->ACCESS_Matches(user + "@" + host))
	{
		rv->stored_ = stored;
		if_StoredNick_LiveUser(stored).Online(rv);

		std::set<boost::shared_ptr<Committee> > comm =
			Committee::FindCommittees(stored->User());
		std::set<boost::shared_ptr<Committee> >::const_iterator i;
		for (i = comm.begin(); i != comm.end(); ++i)
			if (!(*i)->Secure())
			{
				rv->committees_.insert(*i);
				if_Committee_LiveUser(*i).Online(rv);
			}
	}

	MT_RET(rv);
	MT_EE
}

void LiveUser::Stored(const boost::shared_ptr<StoredNick> &nick)
{
	MT_EB
	MT_FUNC("LiveUser::Stored" << nick);

	// This only happens because of a new registration / link.
	SYNC_WLOCK(stored_);
	if (nick)
	{
		stored_ = nick;
		identified_ = true;

		SYNC_LOCK(committees_);
		std::set<boost::shared_ptr<Committee> > comm =
			Committee::FindCommittees(nick->User());
		std::set<boost::shared_ptr<Committee> >::const_iterator i;
		for (i = comm.begin(); i != comm.end(); ++i)
			if (committees_.find(*i) == committees_.end())
			{
				committees_.insert(*i);
				if_Committee_LiveUser(*i).Online(self.lock());
			}
	}
	else
	{
		stored_ = nick;
		identified_ = false;

		{
			SYNC_LOCK(drop_token_);
			if (drop_token_.second)
			{
				ROOT->event->Cancel(drop_token_.second);
				drop_token_ = std::make_pair(std::string(), 0);
			}
		}

		SYNC_LOCK(committees_);
		committees_t::iterator i;
		for (i=committees_.begin(); i!=committees_.end(); ++i)
			if_Committee_LiveUser(*i).Offline(self.lock());
		committees_.clear();
	}

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

	std::string origname;
	{
		SYNC_WLOCK(name_);
		origname = name_;
		name_ = in;
	}

	// Case change, who cares!
	static mantra::iequal_to<std::string> cmp;
	if (cmp(origname, in))
		return;

	{
		SYNC_LOCK(drop_token_);
		if (drop_token_.second)
		{
			ROOT->event->Cancel(drop_token_.second);
			drop_token_ = std::make_pair(std::string(), 0);
		}
	}

	boost::shared_ptr<StoredNick> stored = ROOT->data.Get_StoredNick(in);
	{
		SYNC_WLOCK(stored_);
		if (stored_)
			if_StoredNick_LiveUser(stored_).Offline(
				(boost::format(_("NICK CHANGE -> %1%")) % in).str());
		if (stored)
		{
			if (!stored_ || stored->User() != stored_->User())
			{
				identified_ = false;

				if (!stored->User()->Secure() &&
					stored->User()->ACCESS_Matches(User() + "@" + Host()))
				{
					SYNC_LOCK(committees_);
					committees_t::iterator i;
					for (i=committees_.begin(); i!=committees_.end(); ++i)
						if_Committee_LiveUser(*i).Offline(self.lock());
					committees_.clear();

					stored_ = stored;

					std::set<boost::shared_ptr<Committee> > comm =
						Committee::FindCommittees(stored->User());
					std::set<boost::shared_ptr<Committee> >::const_iterator j;
					for (j = comm.begin(); j != comm.end(); ++j)
						if (!(*j)->Secure())
						{
							committees_.insert(*j);
							if_Committee_LiveUser(*j).Online(self.lock());
						}
				}
				else
				{
					SYNC_LOCK(committees_);
					committees_t::iterator i;
					for (i=committees_.begin(); i!=committees_.end(); ++i)
						if_Committee_LiveUser(*i).Offline(self.lock());
					committees_.clear();

					stored_.reset();
				}
			}
			else
				stored_ = stored;
			if (stored_)
				if_StoredNick_LiveUser(stored_).Online(self.lock());
		}
		else
		{
			SYNC_LOCK(committees_);
			committees_t::iterator i;
			for (i=committees_.begin(); i!=committees_.end(); ++i)
				if_Committee_LiveUser(*i).Offline(self.lock());
			committees_.clear();

			identified_ = false;
			stored_.reset();
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

	// Cancel timer
	{
		SYNC_LOCK(drop_token_);
		if (drop_token_.second)
		{
			ROOT->event->Cancel(drop_token_.second);
			drop_token_ = std::make_pair(std::string(), 0);
		}
	}

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
	// Sign off committees
	{
		SYNC_LOCK(committees_);
		committees_t::iterator i;
		for (i=committees_.begin(); i!=committees_.end(); ++i)
			if_Committee_LiveUser(*i).Offline(self.lock());
		committees_.clear();
	}
	// Tell nickname I quit
	{
		SYNC_WLOCK(stored_);
		if (stored_)
		{
			if_StoredNick_LiveUser(stored_).Offline(reason);
			stored_.reset();
		}
	}

	if (service_)
		if_Service_LiveUser(service_).SIGNOFF(self.lock());

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
		{
			if_StoredNick_LiveUser(stored).Online(self.lock());
		}
		stored_ = stored;
		password_fails_ = 0;

		SYNC_LOCK(committees_);
		std::set<boost::shared_ptr<Committee> > comm =
			Committee::FindCommittees(stored->User());
		std::set<boost::shared_ptr<Committee> >::const_iterator i;
		for (i = comm.begin(); i != comm.end(); ++i)
			if (committees_.find(*i) == committees_.end())
			{
				committees_.insert(*i);
				if_Committee_LiveUser(*i).Online(self.lock());
			}
	}
	else
	{
		if (++password_fails_ >= ROOT->ConfigValue<unsigned int>("nickserv.password-fail"))
		{
			LOG(Notice, _("Password failed tryint to identify user %1%."), Name());
			ROOT->nickserv.KILL(self.lock(), _("Too many password failures."));
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
	if (stored_->User()->Secure() ||
		!stored_->User()->ACCESS_Matches(User() + "@" + Host()))
	{
		if_StoredNick_LiveUser(stored_).Offline(std::string());
		stored_.reset();

		SYNC_LOCK(committees_);
		committees_t::iterator i;
		for (i=committees_.begin(); i!=committees_.end(); ++i)
			if_Committee_LiveUser(*i).Offline(self.lock());
		committees_.clear();
	}
	else
	{
		SYNC_LOCK(committees_);
		committees_t::iterator i = committees_.begin();
		while (i != committees_.end())
			if ((*i)->Secure())
			{
				if_Committee_LiveUser(*i).Offline(self.lock());
				committees_.erase(i++);
			}
			else
				++i;
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

void LiveUser::DropToken(const std::string &in)
{
	MT_EB
	MT_FUNC("LiveUser::DropToken" << in);

	SYNC_LOCK(drop_token_);
	if (drop_token_.second)
	{
		ROOT->event->Cancel(drop_token_.second);
		drop_token_ = std::make_pair(std::string(), 0);
	}

	if (in.empty())
		return;

	unsigned int id = ROOT->event->Schedule(
		boost::bind((void (LiveUser::*)(const std::string &))
						&LiveUser::DropToken, this, std::string()),
					ROOT->ConfigValue<mantra::duration>("nickserv.drop"));

	drop_token_ = std::make_pair(in, id);

	MT_EE
}

std::string LiveUser::DropToken() const
{
	MT_EB
	MT_FUNC("LiveUser::DropToken");

	SYNC_LOCK(drop_token_);
	MT_RET(drop_token_.first);

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

void LiveUser::DropToken(const boost::shared_ptr<StoredChannel> &chan,
						 const std::string &in)
{
	MT_EB
	MT_FUNC("LiveUser::DropToken" << chan << in);

	SYNC_LOCK(channel_drop_token_);
	channel_drop_token_t::iterator i = channel_drop_token_.find(chan);
	if (i != channel_drop_token_.end())
	{
		if (i->second.second)
			ROOT->event->Cancel(i->second.second);
		channel_drop_token_.erase(i);
	}

	if (in.empty())
		return;

	unsigned int id = ROOT->event->Schedule(
		boost::bind((void (LiveUser::*)(const boost::shared_ptr<StoredChannel> &, const std::string &))
						&LiveUser::DropToken, this, chan, std::string()),
					ROOT->ConfigValue<mantra::duration>("chanserv.drop"));

	channel_drop_token_[chan] = std::make_pair(in, id);

	MT_EE
}

std::string LiveUser::DropToken(const boost::shared_ptr<StoredChannel> &chan) const
{
	MT_EB
	MT_FUNC("LiveUser::DropToken" << chan);

	SYNC_LOCK(channel_drop_token_);
	channel_drop_token_t::const_iterator i = channel_drop_token_.find(chan);
	if (i != channel_drop_token_.end())
		MT_RET(i->second.first);

	MT_RET(std::string());
	MT_EE
}

static bool operator<(const LiveUser::channel_joined_t::value_type &lhs,
			   const std::string &rhs)
{
	return (*lhs < rhs);
}

static bool operator<(const LiveUser::committees_t::value_type &lhs,
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
#if 0
	LiveUser::committees_t::const_iterator i =
		std::lower_bound(committees_.begin(), committees_.end(), committee);
	bool rv = (i != committees_.end());
	MT_RET(rv);
#else
	LiveUser::committees_t::const_iterator i;
	for (i = committees_.begin(); i != committees_.end(); ++i)
		if (**i == committee)
			MT_RET(true);
	MT_RET(false);
#endif

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

