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
#define RCSID(x,y) const char *rcsid_magick__livechannel_cpp_ ## x () { return y; }
RCSID(magick__livechannel_cpp, "@(#)$Id$");

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
#include "liveuser.h"
#include "storedchannel.h"

#include <mantra/core/trace.h>

LiveChannel::PendingModes::PendingModes(const boost::shared_ptr<LiveChannel> &channel,
										const boost::shared_ptr<LiveUser> &user)
	: channel_(channel), user_(user)
{
	MT_EB
	MT_FUNC("LiveChannel::PendingModes::PendingModes" << user);

	event_ = ROOT->event->Schedule(boost::bind(&PendingModes::operator(), this),
								   boost::posix_time::seconds(1));

	MT_EE
}

void LiveChannel::PendingModes::operator()() const
{
	MT_EB
	MT_FUNC("LiveChannel::PendingModes::operator()");


	MT_EE
}

void LiveChannel::PendingModes::Update(const std::string &in)
{
	MT_EB
	MT_FUNC("LiveChannel::PendingModes::Update" << in);


	MT_EE
}

void LiveChannel::PendingModes::Cancel()
{
	MT_EB
	MT_FUNC("LiveChannel::PendingModes::Cancel");

	ROOT->event->Cancel(event_);

	MT_EE
}

LiveChannel::LiveChannel(const std::string &name,
						 const boost::posix_time::ptime &created,
						 const std::string &id)
	: name_(name), id_(id), created_(created),
	  seen_(mantra::GetCurrentDateTime()),
	  SYNC_NRWINIT(stored_, reader_priority),
	  SYNC_NRWINIT(users_, reader_priority),
	  SYNC_NRWINIT(bans_, reader_priority),
	  SYNC_NRWINIT(exempts_, reader_priority), modes_limit_(0)
{
	MT_EB
	MT_FUNC("LiveChannel::LiveChannel" << name << created << id);

	stored_ = ROOT->data.Get_StoredChannel(name);

	MT_EE
}

void LiveChannel::Stored(const boost::shared_ptr<StoredChannel> &stored)
{
	MT_EB
	MT_FUNC("LiveChannel::Stored" << stored);

	SYNC_WLOCK(stored_);
	stored_ = stored;

	MT_EE
}

void LiveChannel::Quit(const boost::shared_ptr<LiveUser> &user)
{
	MT_EB
	MT_FUNC("LiveChannel::Quit" << user);

	{
		SYNC_WLOCK(users_);
		users_.erase(user);
	}
	{
		SYNC_LOCK(recent_parts_);
		recent_parts_.erase(user);
	}
	{
		SYNC_LOCK(pending_modes_);
		pending_modes_t::iterator i = pending_modes_.find(user);
		if (i != pending_modes_.end())
		{
			i->second.Cancel();
			pending_modes_.erase(i);
		}
	}

	MT_EE
}

void LiveChannel::Join(const boost::shared_ptr<LiveUser> &user)
{
	MT_EB
	MT_FUNC("LiveChannel::Join" << user);

	{
		SYNC_WLOCK(users_);
		users_[user] = std::string();
	}
	if_LiveUser_LiveChannel(user).Join(self.lock());
	SYNC_RLOCK(stored_);
	if (stored_)
		if_StoredChannel_LiveChannel(stored_).Join(user);

	MT_EE
}

void LiveChannel::Part(const boost::shared_ptr<LiveUser> &user,
					   const std::string &reason)
{
	MT_EB
	MT_FUNC("LiveChannel::Part" << user << reason);

	{
		SYNC_WLOCK(users_);
		users_.erase(user);
	}
	if_LiveUser_LiveChannel(user).Part(self.lock());
	SYNC_RLOCK(stored_);
	if (stored_)
	{
		mantra::duration d = stored_->PartTime();
		if (d)
		{
			SYNC_LOCK(recent_parts_);
			recent_parts_[user] = ROOT->event->Schedule(ClearPart(self.lock(), user), d);
		}
		if_StoredChannel_LiveChannel(stored_).Part(user);
	}

	MT_EE
}

void LiveChannel::Kick(const boost::shared_ptr<LiveUser> &user,
					   const boost::shared_ptr<LiveUser> &kicker,
					   const std::string &reason)
{
	MT_EB
	MT_FUNC("LiveChannel::Kick" << user << kicker << reason);

	{
		SYNC_WLOCK(users_);
		users_.erase(user);
	}
	if_LiveUser_LiveChannel(user).Part(self.lock());
	SYNC_RLOCK(stored_);
	if (stored_)
	{
		mantra::duration d = stored_->PartTime();
		if (d)
		{
			SYNC_LOCK(recent_parts_);
			recent_parts_[user] = ROOT->event->Schedule(ClearPart(self.lock(), user), d);
		}
		if_StoredChannel_LiveChannel(stored_).Kick(user, kicker);
	}

	MT_EE
}

bool LiveChannel::RecentPart(const boost::shared_ptr<LiveUser> &user) const
{
	MT_EB
	MT_FUNC("LiveChannel::RecentPart" << user);

	SYNC_LOCK(recent_parts_);
	bool rv = (recent_parts_.find(user) != recent_parts_.end());

	MT_RET(rv);
	MT_EE
}

void LiveChannel::Users(std::map<boost::shared_ptr<LiveUser>, std::string> &users) const
{
	MT_EB
	MT_FUNC("LiveChannel::Users" << "(std::map<boost::shared_ptr<LiveUser>, std::string> &) users");

	SYNC_RLOCK(users_);
	users = users_;

	MT_EE
}

bool LiveChannel::IsUser(const boost::shared_ptr<LiveUser> &user) const
{
	MT_EB
	MT_FUNC("LiveChannel::IsUser" << user);

	SYNC_RLOCK(users_);
	bool rv = (users_.find(user) != users_.end());

	MT_RET(rv);
	MT_EE
}

std::string LiveChannel::User(const boost::shared_ptr<LiveUser> &user) const
{
	MT_EB
	MT_FUNC("LiveChannel::User" << user);

	SYNC_RLOCK(users_);
	users_t::const_iterator i = users_.find(user);
	if (i != users_.end())
		MT_RET(i->second);

	std::string rv;
	MT_RET(rv);
	MT_EE
}

void LiveChannel::Splits(std::map<boost::shared_ptr<LiveUser>, std::string> &splits) const
{
	MT_EB
	MT_FUNC("LiveChannel::Splits" << "(std::map<boost::shared_ptr<LiveSplit>, std::string> &) splits");

	SYNC_RLOCK(users_);
	splits = splits_;

	MT_EE
}

bool LiveChannel::IsSplit(const boost::shared_ptr<LiveUser> &split) const
{
	MT_EB
	MT_FUNC("LiveChannel::IsSplit" << split);

	SYNC_RLOCK(users_);
	bool rv = (splits_.find(split) != splits_.end());

	MT_RET(rv);
	MT_EE
}

std::string LiveChannel::Split(const boost::shared_ptr<LiveUser> &split) const
{
	MT_EB
	MT_FUNC("LiveChannel::Split" << split);

	SYNC_RLOCK(users_);
	users_t::const_iterator i = splits_.find(split);
	if (i != splits_.end())
		MT_RET(i->second);

	std::string rv;
	MT_RET(rv);
	MT_EE
}

void LiveChannel::Bans(std::map<std::string, boost::posix_time::ptime> &bans) const
{
	MT_EB
	MT_FUNC("LiveChannel::Bans" << "(std::map<std::string, boost::posix_time::ptime> &) bans");

	SYNC_RLOCK(bans_);
	bans_t::const_iterator i;
	for (i = bans_.begin(); i != bans_.end(); ++i)
		bans[i->first] = i->second.first;

	MT_EE
}

bool LiveChannel::MatchBan(const std::string &in) const
{
	MT_EB
	MT_FUNC("LiveChannel::MatchBan" << in);

	SYNC_RLOCK(bans_);
	bans_t::const_iterator i;
	for (i = bans_.begin(); i != bans_.end(); ++i)
	{
		if (mantra::glob_match(i->first, in, true))
			MT_RET(true);
	}
	MT_RET(false);
	MT_EE
}

bool LiveChannel::MatchBan(const boost::shared_ptr<LiveUser> &in) const
{
	MT_EB
	MT_FUNC("LiveChannel::MatchBan" << in);

	bool rv = MatchBan(in->Name() + "!" + in->User() + "@" + in->Host());

	MT_RET(rv);
	MT_EE
}

void LiveChannel::Exempts(std::set<std::string> &exempts) const
{
	MT_EB
	MT_FUNC("LiveChannel::Exempts" << "(std::map<std::string, boost::posix_time::ptime> &) exempts");

	SYNC_RLOCK(exempts_);
	exempts = exempts_;

	MT_EE
}

bool LiveChannel::MatchExempt(const std::string &in) const
{
	MT_EB
	MT_FUNC("LiveChannel::MatchExempt" << in);

	SYNC_RLOCK(exempts_);
	exempts_t::const_iterator i;
	for (i = exempts_.begin(); i != exempts_.end(); ++i)
	{
		if (mantra::glob_match(*i, in, true))
			MT_RET(true);
	}
	MT_RET(false);

	MT_EE
}

bool LiveChannel::MatchExempt(const boost::shared_ptr<LiveUser> &in) const
{
	MT_EB
	MT_FUNC("LiveChannel::MatchExempt" << in);

	bool rv = MatchExempt(in->Name() + "!" + in->User() + "@" + in->Host());

	MT_RET(rv);
	MT_EE
}

void LiveChannel::Topic(const std::string &topic, const std::string &setter,
						const boost::posix_time::ptime &set_time)
{
	MT_EB
	MT_FUNC("LiveChannel::Topic" << topic << setter << set_time);

	{
		SYNC_LOCK(topic_);
		topic_ = topic;
		topic_setter_ = setter;
		topic_set_time_ = set_time;
	}
	SYNC_RLOCK(stored_);
	if (stored_)
		if_StoredChannel_LiveChannel(stored_).Topic(topic, setter, set_time);

	MT_EE
}

std::string LiveChannel::Topic() const
{
	MT_EB
	MT_FUNC("LiveChannel::Topic");

	SYNC_LOCK(topic_);
	MT_RET(topic_);

	MT_EE
}


std::string LiveChannel::Topic_Setter() const
{
	MT_EB
	MT_FUNC("LiveChannel::Topic_Setter");

	SYNC_LOCK(topic_);
	MT_RET(topic_setter_);

	MT_EE
}

boost::posix_time::ptime LiveChannel::Topic_Set_Time() const
{
	MT_EB
	MT_FUNC("LiveChannel::Topic_Set_Time");

	SYNC_LOCK(topic_);
	MT_RET(topic_set_time_);

	MT_EE
}

void LiveChannel::Modes(const boost::shared_ptr<LiveUser> &user,
						const std::string &in)
{
	MT_EB
	MT_FUNC("LiveChannel::Modes" << user << in);


	MT_EE
}

void LiveChannel::SendModes(const boost::shared_ptr<LiveUser> &user,
							const std::string &in)
{
	MT_EB
	MT_FUNC("LiveChannel::SendModes" << user << in);


	MT_EE
}

std::string LiveChannel::Modes() const
{
	MT_EB
	MT_FUNC("LiveChannel::Modes");

	SYNC_LOCK(modes_);
	MT_RET(modes_);

	MT_EE
}

std::string LiveChannel::Modes_Key() const
{
	MT_EB
	MT_FUNC("LiveChannel::Modes_Key");

	SYNC_LOCK(modes_);
	MT_RET(modes_key_);

	MT_EE
}

unsigned int LiveChannel::Modes_Limit() const
{
	MT_EB
	MT_FUNC("LiveChannel::Modes_Limit");

	SYNC_LOCK(modes_);
	MT_RET(modes_limit_);

	MT_EE
}
