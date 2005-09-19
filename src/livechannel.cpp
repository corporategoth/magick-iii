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

boost::shared_ptr<LiveChannel> LiveChannel::create(const std::string &name,
		const boost::posix_time::ptime &created, const std::string &id)
{
	MT_EB
	MT_FUNC("LiveChannel::create" << name << created << id);

	boost::shared_ptr<LiveChannel> rv(new LiveChannel(name, created, id));
	rv->self = rv;
	ROOT->data.Add(rv);
	return rv;

	MT_EE
}

LiveChannel::PendingModes::PendingModes(const boost::shared_ptr<LiveChannel> &channel,
										const boost::shared_ptr<LiveUser> &user)
	: event_(0), channel_(channel), user_(user)
{
	MT_EB
	MT_FUNC("LiveChannel::PendingModes::PendingModes" << channel << user);

	MT_EE
}

void LiveChannel::PendingModes::operator()()
{
	MT_EB
	unsigned long codetype = MT_ASSIGN(MAGICK_TRACE_EVENT);
	MT_FUNC("LiveChannel::PendingModes::operator()");

	std::string modes;
	std::vector<std::string> params;

	SYNC_LOCK(lock);
	MT_CP("Changing Modes: -%1%+%2%",
		  std::string(off_.begin(), off_.end()) %
		  std::string(on_.begin(), on_.end()));

	if (!user_)
	{
		user_ = ROOT->data.Get_LiveUser(ROOT->chanserv.Primary());
		if (!user_)
		{
			event_ = ROOT->event->Schedule(boost::bind(&PendingModes::operator(), this),
										   boost::posix_time::seconds(1));
			return;
		}
	}

	event_ = 0;
	if (!channel_ || !user_->GetService())
	{
		on_.clear();
		on_params_.clear();
		off_.clear();
		off_params_.clear();
		return;
	}

	if (!(off_.empty() && off_params_.empty()))
	{
		modes.append(1, '-');

		if (!off_.empty())
			modes.append(off_.begin(), off_.end());

		if (!off_params_.empty())
		{
			modes_params_t::iterator i;
			for (i = off_params_.begin(); i != off_params_.end(); ++i)
			{
				if (i->second.empty())
					continue;
				modes.append(i->second.size(), i->first);
				params.insert(params.end(), i->second.begin(), i->second.end());
			}
		}
		on_.clear();
		on_params_.clear();
	}

	if (!(on_.empty() && on_params_.empty()))
	{
		modes.append(1, '+');

		if (!on_.empty())
			modes.append(on_.begin(), on_.end());

		if (!on_params_.empty())
		{
			modes_params_t::iterator i;
			for (i = on_params_.begin(); i != on_params_.end(); ++i)
			{
				if (i->second.empty())
					continue;
				modes.append(i->second.size(), i->first);
				params.insert(params.end(), i->second.begin(), i->second.end());
			}
		}
		off_.clear();
		off_params_.clear();
	}

	SYNC_UNLOCK(lock);

	user_->GetService()->MODE(user_, channel_, modes, params);

	MT_ASSIGN(codetype);
	MT_EE
}

void LiveChannel::PendingModes::Update(const std::string &in,
									   const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("LiveChannel::PendingModes::Update" << in << params);

	std::string chanmodeargs = ROOT->proto.ConfigValue<std::string>("channel-mode-params");
	bool add = true;
	std::string::const_iterator i;
	size_t m = 0;

	SYNC_LOCK(lock);
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
		case 'l':
			if (!add)
			{
				on_params_.erase(*i);
				off_.insert(*i);
				break;
			}
			// Don't break, this should fall through if its 'add'.
		default:
			if (chanmodeargs.find(*i) != std::string::npos)
			{
				if (m >= params.size())
				{
					// LOG an error!
					break;
				}

				switch (*i)
				{
				case 'k':
					if (add)
					{
						// To turn on a key, it has to be turned off first.
						if (!channel_->Modes_Key().empty())
						{
							modes_params_t::iterator mpi = off_params_.find(*i);
							if (mpi != off_params_.end())
								break;
						}
						std::pair<modes_params_t::iterator, bool> rv =
							on_params_.insert(std::make_pair(*i, params_t()));
						params_t tmp;
						tmp.insert(params[m]);
						rv.first->second.swap(tmp);
					}
					else
					{
						// To turn off a key, it must match the current key.
						if (params[m] != channel_->Modes_Key())
							break;
						modes_params_t::iterator mpi = on_params_.find(*i);
						if (mpi != on_params_.end())
							on_params_.erase(mpi);
						std::pair<modes_params_t::iterator, bool> rv =
							off_params_.insert(std::make_pair(*i, params_t()));
						params_t tmp;
						tmp.insert(params[m]);
						rv.first->second.swap(tmp);
					}
					break;

				case 'l':
					// Has to be add because of the previous check.
					try
					{
						boost::lexical_cast<unsigned int>(params[m]);
						off_.erase(*i);
						params_t tmp;
						tmp.insert(params[m]);
						on_params_[*i].swap(tmp);
					}
					catch (boost::bad_lexical_cast &e)
					{
						// LOG an error!
					}
					break;

				case 'o':
				case 'h':
				case 'v':
				case 'q':
				case 'u':
				case 'a':
				case 'b':
				case 'd':
				case 'e':
					if (add)
					{
						modes_params_t::iterator mpi = off_params_.find(*i);
						if (mpi != off_params_.end())
						{
							params_t::iterator pi = mpi->second.find(params[m]);
							if (pi != mpi->second.end())
								mpi->second.erase(pi);
							if (mpi->second.empty())
								off_params_.erase(mpi);
						}
						std::pair<modes_params_t::iterator, bool> rv =
							on_params_.insert(std::make_pair(*i, params_t()));
						rv.first->second.insert(params[m]);
					}
					else
					{
						modes_params_t::iterator mpi = on_params_.find(*i);
						if (mpi != on_params_.end())
						{
							params_t::iterator pi = mpi->second.find(params[m]);
							if (pi != mpi->second.end())
								mpi->second.erase(pi);
							if (mpi->second.empty())
								on_params_.erase(mpi);
						}
						std::pair<modes_params_t::iterator, bool> rv =
							off_params_.insert(std::make_pair(*i, params_t()));
						rv.first->second.insert(params[m]);
					}
					break;

				default:
					if (add)
					{
						std::pair<modes_params_t::iterator, bool> rv =
							on_params_.insert(std::make_pair(*i, params_t()));
						rv.first->second.insert(params[m]);
					}
					else
					{
						std::pair<modes_params_t::iterator, bool> rv =
							off_params_.insert(std::make_pair(*i, params_t()));
						rv.first->second.insert(params[m]);
					}
					break;
				}

				++m;
			}
			else
			{
				if (add)
				{
					off_.erase(*i);
					on_.insert(*i);
				}
				else
				{
					on_.erase(*i);
					off_.insert(*i);
				}
			}
		}
	}

	if (!event_)
	{
		event_ = ROOT->event->Schedule(boost::bind(&PendingModes::operator(), this),
									   boost::posix_time::seconds(1));
	}

	MT_EE
}

void LiveChannel::PendingModes::Cancel()
{
	MT_EB
	MT_FUNC("LiveChannel::PendingModes::Cancel");

	SYNC_LOCK(lock);
	ROOT->event->Cancel(event_);
	event_ = 0;

	MT_EE
}

void LiveChannel::ClearPart::operator()() const
{
	MT_EB
	unsigned long codetype = MT_ASSIGN(MAGICK_TRACE_EVENT);
	MT_FUNC("LiveChannel::ClearPart::operator()");

	SYNCP_LOCK(channel_, recent_parts_);
	channel_->recent_parts_.erase(user_);

	MT_ASSIGN(codetype);
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

	{
		SYNC_WLOCK(stored_);
		stored_ = stored;
	}

	{
		SYNC_LOCK(topic_);
		if (!topic_.empty())
			if_StoredChannel_LiveChannel(stored).Topic(topic_, topic_setter_, topic_set_time_);
	}

	std::string modes("+");
	std::vector<std::string> mode_params;
	{
		SYNC_LOCK(modes_);
		std::set<char>::const_iterator i;
		for (i = modes_.begin(); i != modes_.end(); ++i)
			modes += *i;
		if (!modes_key_.empty())
		{
			modes += 'k';
			mode_params.push_back(modes_key_);
		}
		if (modes_limit_)
		{
			modes += 'l';
			mode_params.push_back(boost::lexical_cast<std::string>(modes_limit_));
		}
	}
	{
		SYNC_RLOCK(users_);
		users_t::const_iterator i;
		for (i = users_.begin(); i != users_.end(); ++i)
		{
			if_StoredChannel_LiveChannel(stored).Join(self.lock(), i->first);
			std::set<char>::const_iterator j;
			for (j = i->second.begin(); j != i->second.end(); ++j)
			{
				modes += *j;
				mode_params.push_back(i->first->Name());
			}
		}
	}

	std::string cmp = ROOT->proto.ConfigValue<std::string>("channel-mode-params");
	{
		SYNC_RLOCK(bans_);
		bans_t::const_iterator i;
		for (i = bans_.begin(); i != bans_.end(); ++i)
		{
			modes += 'b';
			mode_params.push_back(i->first);
		}
		if (cmp.find('d') != std::string::npos)
		{
			rxbans_t::const_iterator j;
			for (j = rxbans_.begin(); j != rxbans_.end(); ++j)
			{
				modes += 'd';
				mode_params.push_back(j->first.str());
			}
		}
	}
	{
		SYNC_RLOCK(exempts_);
		exempts_t::const_iterator i;
		for (i = exempts_.begin(); i != exempts_.end(); ++i)
		{
			modes += 'e';
			mode_params.push_back(*i);
		}
#if 0
		if (cmp.find('?') != std::string::npos)
		{
			rxexempts_t::const_iterator j;
			for (j = rxexempts_.begin(); j != rxexempts_.end(); ++j)
			{
				modes += '?';
				mode_params.push_back(j->str());
			}
		}
#endif
	}
	if_StoredChannel_LiveChannel(stored).Modes(boost::shared_ptr<LiveUser>(),
											   modes, mode_params);

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
			i->second->Cancel();
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
		users_[user] = std::set<char>();
	}
	if_LiveUser_LiveChannel(user).Join(self.lock());
	SYNC_RLOCK(stored_);
	if (stored_)
		if_StoredChannel_LiveChannel(stored_).Join(self.lock(), user);

	MT_EE
}

void LiveChannel::CommonPart(const boost::shared_ptr<LiveUser> &user)
{
	MT_EB
	MT_FUNC("LiveChannel::CommonPart" << user);

	bool empty;
	{
		SYNC_WLOCK(users_);
		users_.erase(user);
		empty = users_.empty();
	}

	if_LiveUser_LiveChannel(user).Part(self.lock());

	if (empty)
	{
		{
			SYNC_LOCK(recent_parts_);
			recent_parts_t::iterator i;
			for (i = recent_parts_.begin(); i != recent_parts_.end(); ++i)
				ROOT->event->Cancel(i->second);
			recent_parts_.clear();
		}

		{
			SYNC_LOCK(pending_modes_);
			pending_modes_t::iterator i;
			for (i = pending_modes_.begin(); i != pending_modes_.end(); ++i)
				i->second->Cancel();
			pending_modes_.clear();
		}

		if_StorageDeleter<LiveChannel>(ROOT->data).Del(self.lock());
	}
	else
	{
		SYNC_RLOCK(stored_);
		if (stored_)
		{
			mantra::duration d = stored_->PartTime();
			if (d)
			{
				SYNC_LOCK(recent_parts_);
				recent_parts_[user] = ROOT->event->Schedule(ClearPart(self.lock(), user), d);
			}
		}
	}

	MT_EE
}

void LiveChannel::Part(const boost::shared_ptr<LiveUser> &user,
					   const std::string &reason)
{
	MT_EB
	MT_FUNC("LiveChannel::Part" << user << reason);

	CommonPart(user);
	SYNC_RLOCK(stored_);
	if_StoredChannel_LiveChannel(stored_).Part(user);

	MT_EE
}

void LiveChannel::Kick(const boost::shared_ptr<LiveUser> &user,
					   const boost::shared_ptr<LiveUser> &kicker,
					   const std::string &reason)
{
	MT_EB
	MT_FUNC("LiveChannel::Kick" << user << kicker << reason);

	CommonPart(user);
	SYNC_RLOCK(stored_);
	if_StoredChannel_LiveChannel(stored_).Kick(user, kicker);

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

void LiveChannel::Users(LiveChannel::users_t &users) const
{
	MT_EB
	MT_FUNC("LiveChannel::Users" << users);

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

std::set<char> LiveChannel::User(const boost::shared_ptr<LiveUser> &user) const
{
	MT_EB
	MT_FUNC("LiveChannel::User" << user);

	std::set<char> rv;
	SYNC_RLOCK(users_);
	users_t::const_iterator i = users_.find(user);
	if (i != users_.end())
		rv = i->second;

	MT_RET(rv);
	MT_EE
}

void LiveChannel::Splits(LiveChannel::users_t &splits) const
{
	MT_EB
	MT_FUNC("LiveChannel::Splits" << splits);

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

std::set<char> LiveChannel::Split(const boost::shared_ptr<LiveUser> &split) const
{
	MT_EB
	MT_FUNC("LiveChannel::Split" << split);

	std::set<char> rv;
	SYNC_RLOCK(users_);
	users_t::const_iterator i = splits_.find(split);
	if (i != splits_.end())
		rv = i->second;

	MT_RET(rv);
	MT_EE
}

void LiveChannel::Bans(LiveChannel::bans_t &bans) const
{
	MT_EB
	MT_FUNC("LiveChannel::Bans" << bans);

	SYNC_RLOCK(bans_);
	bans = bans_;

	MT_EE
}

void LiveChannel::Bans(LiveChannel::rxbans_t &bans) const
{
	MT_EB
	MT_FUNC("LiveChannel::Bans" << bans);

	SYNC_RLOCK(bans_);
	bans = rxbans_;

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
	rxbans_t::const_iterator j;
	for (j = rxbans_.begin(); j != rxbans_.end(); ++j)
	{
		if (boost::regex_match(in, j->first))
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

void LiveChannel::MatchBan(const std::string &in, LiveChannel::bans_t &bans) const
{
	MT_EB
	MT_FUNC("LiveChannel::MatchBan" << in << bans);

	SYNC_RLOCK(bans_);
	bans_t::const_iterator i;
	for (i = bans_.begin(); i != bans_.end(); ++i)
	{
		if (mantra::glob_match(i->first, in, true))
			bans[i->first] = i->second;
	}

	MT_EE
}

void LiveChannel::MatchBan(const boost::shared_ptr<LiveUser> &in, LiveChannel::bans_t &bans) const
{
	MT_EB
	MT_FUNC("LiveChannel::MatchBan" << in << bans);

	MatchBan(in->Name() + "!" + in->User() + "@" + in->Host(), bans);

	MT_EE
}

void LiveChannel::MatchBan(const std::string &in, LiveChannel::rxbans_t &bans) const
{
	MT_EB
	MT_FUNC("LiveChannel::MatchBan" << in << bans);

	SYNC_RLOCK(bans_);
	rxbans_t::const_iterator j;
	for (j = rxbans_.begin(); j != rxbans_.end(); ++j)
	{
		if (boost::regex_match(in, j->first))
			bans[j->first] = j->second;
	}

	MT_EE
}

void LiveChannel::MatchBan(const boost::shared_ptr<LiveUser> &in, LiveChannel::rxbans_t &bans) const
{
	MT_EB
	MT_FUNC("LiveChannel::MatchBan" << in << bans);

	MatchBan(in->Name() + "!" + in->User() + "@" + in->Host(), bans);

	MT_EE
}

void LiveChannel::Exempts(LiveChannel::exempts_t &exempts) const
{
	MT_EB
	MT_FUNC("LiveChannel::Exempts" << exempts);

	SYNC_RLOCK(exempts_);
	exempts = exempts_;

	MT_EE
}

void LiveChannel::Exempts(LiveChannel::rxexempts_t &exempts) const
{
	MT_EB
	MT_FUNC("LiveChannel::Exempts" << exempts);

	SYNC_RLOCK(exempts_);
	exempts = rxexempts_;

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
	rxexempts_t::const_iterator j;
	for (j = rxexempts_.begin(); j != rxexempts_.end(); ++j)
	{
		if (boost::regex_match(in, *j))
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

void LiveChannel::MatchExempt(const std::string &in, LiveChannel::exempts_t &exempt) const
{
	MT_EB
	MT_FUNC("LiveChannel::MatchExempt" << in << exempt);

	SYNC_RLOCK(exempts_);
	exempts_t::const_iterator i;
	for (i = exempts_.begin(); i != exempts_.end(); ++i)
	{
		if (mantra::glob_match(*i, in, true))
			exempt.insert(*i);
	}

	MT_EE
}

void LiveChannel::MatchExempt(const boost::shared_ptr<LiveUser> &in, LiveChannel::exempts_t &exempt) const
{
	MT_EB
	MT_FUNC("LiveChannel::MatchExempt" << in << exempt);

	MatchExempt(in->Name() + "!" + in->User() + "@" + in->Host(), exempt);

	MT_EE
}

void LiveChannel::MatchExempt(const std::string &in, LiveChannel::rxexempts_t &exempt) const
{
	MT_EB
	MT_FUNC("LiveChannel::MatchExempt" << in << exempt);

	SYNC_RLOCK(exempts_);
	rxexempts_t::const_iterator j;
	for (j = rxexempts_.begin(); j != rxexempts_.end(); ++j)
	{
		if (boost::regex_match(in, *j))
			exempt.insert(*j);
	}

	MT_EE
}

void LiveChannel::MatchExempt(const boost::shared_ptr<LiveUser> &in, LiveChannel::rxexempts_t &exempt) const
{
	MT_EB
	MT_FUNC("LiveChannel::MatchExempt" << in << exempt);

	MatchExempt(in->Name() + "!" + in->User() + "@" + in->Host(), exempt);

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
						const std::string &in, const std::string &params)
{
	MT_EB
	MT_FUNC("LiveChannel::Modes" << user << in << params);

	boost::char_separator<char> sep(" \t");
	typedef boost::tokenizer<boost::char_separator<char>,
		std::string::const_iterator, std::string> tokenizer;
	tokenizer tokens(params, sep);
	std::vector<std::string> v(tokens.begin(), tokens.end());
	Modes(user, in, v);

	MT_EE
}

static bool operator<(const LiveChannel::users_t::value_type &lhs,
					  const std::string &rhs)
{
	return (lhs.first < rhs);
}

void LiveChannel::Modes(const boost::shared_ptr<LiveUser> &user,
						const std::string &in, const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("LiveChannel::Modes" << user << in << params);

	bool add = true;
	std::string::const_iterator i;
	users_t::iterator j;
	exempts_t::iterator l;
	size_t m = 0;

	SYNC_LOCK(modes_);
	MT_CB(0, modes_);
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
		case 'l':
			if (!add)
			{
				modes_limit_ = 0;
				break;
			}
			// Don't break, this should fall through if its 'add'.
		default:
			if (ROOT->proto.ConfigValue<std::string>("channel-mode-params").find(*i) != std::string::npos)
			{
				if (m >= params.size())
				{
					// LOG an error!
					break;
				}

				switch (*i)
				{
				case 'o':
				case 'h':
				case 'v':
				case 'q':
				case 'u':
				case 'a':
					{
						SYNC_RWLOCK(users_);
						j = std::lower_bound(users_.begin(), users_.end(),
											 params[m]);
						if (j == users_.end() || *(j->first) != params[m])
						{
							// LOG an error!
							break;
						}

						SYNC_PROMOTE(users_);
						if (add)
							j->second.insert(*i);
						else
							j->second.erase(*i);
					}
					break;

				case 'b':
					if (add)
					{
						SYNC_WLOCK(bans_);
						bans_[params[m]] = std::make_pair(mantra::GetCurrentDateTime(), 0);
					}
					else
					{
						SYNC_RWLOCK(bans_);
						bans_t::iterator k = bans_.find(params[m]);
						if (k == bans_.end())
						{
							// LOG an error!
							break;
						}

						SYNC_PROMOTE(bans_);
						bans_.erase(k);
					}
					break;

				case 'd':
					if (add)
					{
						SYNC_WLOCK(bans_);
						rxbans_[boost::regex(params[m],
											 boost::regex_constants::normal |
											 boost::regex_constants::icase)] =
							std::make_pair(mantra::GetCurrentDateTime(), 0);
					}
					else
					{
						SYNC_RWLOCK(bans_);
						rxbans_t::iterator k;
						for (k = rxbans_.begin(); k != rxbans_.end(); ++k)
						{
							static mantra::iless<std::string> cmp;
							if (cmp(k->first.str(), params[m]))
								break;
						}

						if (k == rxbans_.end())
						{
							// LOG an error!
							break;
						}

						SYNC_PROMOTE(bans_);
						rxbans_.erase(k);
					}
					break;

				case 'e':
					if (add)
					{
						SYNC_WLOCK(exempts_);
						exempts_.insert(params[m]);
					}
					else
					{
						SYNC_RWLOCK(exempts_);
						exempts_t::iterator k = exempts_.find(params[m]);
						if (k == exempts_.end())
						{
							// LOG an error!
							break;
						}

						SYNC_PROMOTE(exempts_);
						exempts_.erase(k);
					}
					break;

#if 0
				case '?': // Who knows ...
					if (add)
					{
						SYNC_WLOCK(exempts_);
						rxexempts_.insert(boost::regex(params[m],
													   boost::regex_constants::normal |
													   boost::regex_constants::icase));
					}
					else
					{
						SYNC_RWLOCK(exempts_);
						rxexempts_t::iterator k = std::lower_bound(rxexempts_.begin(),
																   rxexempts_.end(),
																   params[m]);

						static mantra::iless<std::string> cmp;
						if (k == rxexempts_.end() || !cmp(k->str(), params[m]))
						{
							// LOG an error!
							break;
						}

						SYNC_PROMOTE(exempts_);
						rxexempts_.erase(k);
					}
					break;
#endif

				case 'k':
					if (add)
					{
						if (!modes_key_.empty())
						{
							// LOG an error!
							break;
						}
						modes_key_ = params[m];
					}
					else
					{
						if (modes_key_ != params[m])
						{
							// LOG an error!
							break;
						}
						modes_key_.clear();
					}
					break;

				case 'l':
					// Has to be add because of the previos check.
					try
					{
						modes_limit_ = boost::lexical_cast<unsigned int>(params[m]);
					}
					catch (boost::bad_lexical_cast &e)
					{
						// LOG an error!
					}
					break;

				default:
					// LOG an error!
					break;
				}

				++m;
			}
			else
			{
				if (add)
					modes_.insert(*i);
				else
					modes_.erase(*i);
			}
		}
	}
	MT_CE(0, modes_);

	SYNC_RLOCK(stored_);
	if (stored_)
		if_StoredChannel_LiveChannel(stored_).Modes(user, in, params);

	MT_EE
}

void LiveChannel::SendModes(const boost::shared_ptr<LiveUser> &user,
							const std::string &in, const std::string &params)
{
	MT_EB
	MT_FUNC("LiveChannel::SendModes" << user << in << params);

	boost::char_separator<char> sep(" \t");
	typedef boost::tokenizer<boost::char_separator<char>,
		std::string::const_iterator, std::string> tokenizer;
	tokenizer tokens(params, sep);
	std::vector<std::string> v(tokens.begin(), tokens.end());
	SendModes(user, in, v);

	MT_EE
}

void LiveChannel::SendModes(const boost::shared_ptr<LiveUser> &user,
							const std::string &in, const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("LiveChannel::SendModes" << user << in << params);

	// Bloody useless ...
	if (in.find_first_not_of("+-") == std::string::npos)
		return;

	SYNC_LOCK(pending_modes_);
	pending_modes_t::iterator i = pending_modes_.lower_bound(user);
	if (i == pending_modes_.end() || i->first != user)
		i = pending_modes_.insert(i, std::make_pair(user,
								  new PendingModes(self.lock(), user)));

	i->second->Update(in, params);

	MT_EE
}

std::set<char> LiveChannel::Modes() const
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

