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
#define RCSID(x,y) const char *rcsid_magick__storedchannel_cpp_ ## x () { return y; }
RCSID(magick__storedchannel_cpp, "@(#)$Id$");

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
#include "storedchannel.h"
#include "livechannel.h"
#include "storeduser.h"
#include "storednick.h"
#include "liveuser.h"

#include <mantra/core/trace.h>

StorageInterface StoredChannel::storage("channels", "id", "last_update");

StoredChannel::Revenge_t StoredChannel::RevengeID(const std::string &in)
{
	MT_EB
	MT_FUNC("ChanServ::RevengeID" << in);

	static boost::regex revenge_rx[StoredChannel::R_MAX] =
		{
			boost::regex("^(NONE)$", boost::regex_constants::icase),
			boost::regex("^(REVERSE)$", boost::regex_constants::icase),
			boost::regex("^(MIRROR)$", boost::regex_constants::icase),
			boost::regex("^(DEOP)$", boost::regex_constants::icase),
			boost::regex("^(KICK)$", boost::regex_constants::icase),
			boost::regex("^(BAN1)$", boost::regex_constants::icase),
			boost::regex("^(BAN2)$", boost::regex_constants::icase),
			boost::regex("^(BAN3)$", boost::regex_constants::icase),
			boost::regex("^(BAN4)$", boost::regex_constants::icase),
			boost::regex("^(BAN5)$", boost::regex_constants::icase)
		};

	size_t i;
	for (i=0; i < R_MAX; ++i)
		if (boost::regex_match(in, revenge_rx[i]))
			break;

	MT_RET((Revenge_t) i);
	MT_EE
}

std::string StoredChannel::RevengeDesc(StoredChannel::Revenge_t id)
{
	MT_EB
	MT_FUNC("ChanServ::RevengeDesc" << id);

	static const char *revenge_desc[StoredChannel::R_MAX] =
		{
			"No Action",
			"Reverse Action",
			"Mirror Action"
			"DeOp User",
			"Kick User",
			"Nick Ban (nick!*@*)",
			"User Port Ban (*!*user@port.host)",
			"User Host Ban (*!*user@*.host)",
			"Port Ban (*!*@port.host)",
			"Host Ban (*!*@*.host)"
		};

	MT_RET(revenge_desc[id]);
	MT_EE
}

boost::shared_ptr<StoredChannel> StoredChannel::create(const std::string &name,
	  const std::string &password, const boost::shared_ptr<StoredUser> &founder)
{
	MT_EB
	MT_FUNC("StoredChannel::create" << name << password << founder);

	static boost::mutex id_lock;

	boost::shared_ptr<StoredChannel> rv;
	boost::uint32_t id;
	{
		boost::mutex::scoped_lock sl(id_lock);
		mantra::StorageValue v = storage.Maximum("id");
		if (v.type() == typeid(mantra::NullValue))
			id = 1;
		else
			id = boost::get<boost::uint32_t>(v) + 1;

		mantra::Storage::RecordMap rec;
		rec["id"] = id;
		rec["name"] = name;
		rec["password"] = ROOT->data.CryptPassword(password);
		rec["founder"] = founder->ID();
		rec["last_used"] = mantra::GetCurrentDateTime();
		if (!storage.InsertRow(rec))
			MT_RET(rv);
	}

	rv = load(id, name);
	ROOT->data.Add(rv);

	MT_RET(rv);
	MT_EE
}

boost::shared_ptr<StoredChannel> StoredChannel::load(boost::uint32_t id, const std::string &name)
{
	MT_EB
	MT_FUNC("StoredChannel::load" << id << name);

	boost::shared_ptr<StoredChannel> rv(new StoredChannel(id, name));
	rv->self = rv;

	if (rv->live_)
		if_LiveChannel_StoredChannel(rv->live_).Stored(rv);

	MT_RET(rv);
	MT_EE
}

void StoredChannel::expire()
{
	MT_EB
	MT_FUNC("StoredChannel::expire");

	bool locked = false;
	if (ROOT->ConfigValue<bool>("chanserv.lock.noexpire"))
	{
		if (ROOT->ConfigValue<bool>("chanserv.defaults.noexpire"))
			return;
		else
			locked = true;
	}

	boost::posix_time::ptime exptime = mantra::GetCurrentDateTime() - 
					ROOT->ConfigValue<mantra::duration>("chanserv.expire");

	mantra::Storage::DataSet data;
	mantra::Storage::FieldSet fields;
	fields.insert("id");

	mantra::ComparisonSet cs = mantra::Comparison<mantra::C_LessThan>::make("last_used", exptime);
	if (!locked)
		cs.And(mantra::Comparison<mantra::C_EqualTo>::make("noexpire", true, true));
	storage.RetrieveRow(data, cs, fields);

	if (data.empty())
		return;

	mantra::Storage::DataSet::const_iterator i = data.begin();
	for (i = data.begin(); i != data.end(); ++i)
	{
		mantra::Storage::RecordMap::const_iterator j = i->find("id");
		if (j == i->end() || j->second.type() == typeid(mantra::NullValue))
			continue;

		boost::shared_ptr<StoredChannel> channel = ROOT->data.Get_StoredChannel(
									boost::get<boost::uint32_t>(j->second));
		if (!channel)
			continue;

		LOG(Notice, "Expiring channel %1%.", channel->Name());
		channel->Drop();
	}

	MT_EE
}

StoredChannel::StoredChannel(boost::uint32_t id, const std::string &name)
	: cache(storage, ROOT->ConfigValue<mantra::duration>("general.cache-expire"),
			mantra::Comparison<mantra::C_EqualTo>::make("id", id)),
	  id_(id), name_(name)
{
	MT_EB
	MT_FUNC("StoredChannel::StoredChannel" << id << name);

	live_ = ROOT->data.Get_LiveChannel(name);

	MT_EE
}

bool StoredChannel::Identify(const boost::shared_ptr<LiveUser> &user,
							 const std::string &password)
{
	MT_EB
	MT_FUNC("StoredChannel::Identify" << user << password);

	if (CheckPassword(password))
	{
		boost::mutex::scoped_lock sl(lock_);
		identified_users_.insert(user);
		MT_RET(true);
	}

	MT_RET(false);
	MT_EE
}

void StoredChannel::UnIdentify(const boost::shared_ptr<LiveUser> &user)
{
	MT_EB
	MT_FUNC("StoredChannel::UnIdentify" << user);

	boost::mutex::scoped_lock sl(lock_);
	identified_users_.erase(user);

	MT_EE
}

void StoredChannel::Topic(const std::string &topic, const std::string &setter,
						  const boost::posix_time::ptime &set_time)
{
	MT_EB
	MT_FUNC("StoredChannel::Topic" << topic << setter << set_time);

	mantra::Storage::RecordMap data;
	mantra::Storage::FieldSet fields;
	fields.insert("topic");
	fields.insert("topic_setter");
	fields.insert("topic_set_time");
	fields.insert("topiclock");
	cache.Get(data, fields);

	bool topiclock;
	mantra::Storage::RecordMap::const_iterator i = data.find("topiclock");
	if (i == data.end() || i->second.type() == typeid(mantra::NullValue))
		topiclock = ROOT->ConfigValue<bool>("chanserv.defaults.topiclock");
	else
		topiclock = boost::get<bool>(i->second);

	if (topiclock)
	{
		i = data.find("topic");
		if (i == data.end() || i->second.type() == typeid(mantra::NullValue) ||
			boost::get<std::string>(i->second) != topic)
		{
			// TODO: Revert it back, we're locked!
		}
	}
	else
	{
		mantra::Storage::RecordMap rec;
		rec["topic"] = topic;
		rec["topic_setter"] = setter;
		rec["topic_set_time"] = set_time;
		cache.Put(rec);
	}

	MT_EE
}


void StoredChannel::Modes(const boost::shared_ptr<LiveUser> &user,
						  const std::string &in,
						  const std::vector<std::string> &params)
{
	MT_EB
	MT_FUNC("StoredChannel::Modes" << user << in << params);


	MT_EE
}

void StoredChannel::Join(const boost::shared_ptr<LiveUser> &user)
{
	MT_EB
	MT_FUNC("StoredChannel::Join" << user);

	boost::int32_t level = 0;
	if (user->Identified(self.lock()))
		level = ROOT->ConfigValue<boost::int32_t>("chanserv.max-level");
	else if (user->Identified() || !Secure())
	{
		boost::shared_ptr<StoredNick> nick = user->Stored();
		if (nick && nick->User() == Founder())
		{
			level = ROOT->ConfigValue<boost::int32_t>("chanserv.max-level");
		}
		else
		{
			std::list<Access> acc(ACCESS_Matches(user));
			if (!acc.empty())
				level = acc.front().Level();
		}
	}

	if (level > 0)
		cache.Put("last_used", mantra::GetCurrentDateTime());

	MT_EE
}

void StoredChannel::Part(const boost::shared_ptr<LiveUser> &user)
{
	MT_EB
	MT_FUNC("StoredChannel::Part" << user);


	MT_EE
}

void StoredChannel::Kick(const boost::shared_ptr<LiveUser> &user,
						 const boost::shared_ptr<LiveUser> &kicker)
{
	MT_EB
	MT_FUNC("StoredChannel::Kick" << user << kicker);


	MT_EE
}

void StoredChannel::Password(const std::string &password)
{
	MT_EB
	MT_FUNC("StoredChannel::Password" << password);

	cache.Put("password",
					 ROOT->data.CryptPassword(password));

	MT_EE
}

bool StoredChannel::CheckPassword(const std::string &password) const
{
	MT_EB
	MT_FUNC("StoredChannel::CheckPassword" << password);

	mantra::StorageValue rv = cache.Get("password");
	if (rv.type() == typeid(mantra::NullValue))
		MT_RET(false);
	bool ret = (boost::get<std::string>(rv) == ROOT->data.CryptPassword(password));

	MT_RET(ret);
	MT_EE
}

void StoredChannel::Founder(const boost::shared_ptr<StoredUser> &in)
{
	MT_EB
	MT_FUNC("StoredChannel::Founder" << in);

	if (!in)
		return;

	mantra::Storage::RecordMap rec;
	rec["founder"] = in->ID();

	mantra::StorageValue rv = cache.Get("successor");
	if (rv.type() != typeid(mantra::NullValue) &&
		boost::get<boost::uint32_t>(rv) == in->ID())
		rec["successor"] = mantra::NullValue();

	cache.Put(rec);

	MT_EE
}

boost::shared_ptr<StoredUser> StoredChannel::Founder() const
{
	MT_EB
	MT_FUNC("StoredChannel::Founder");

	boost::shared_ptr<StoredUser> ret;
	mantra::StorageValue rv = cache.Get("founder");
	if (rv.type() == typeid(mantra::NullValue))
		MT_RET(ret);
	ret = ROOT->data.Get_StoredUser(boost::get<boost::uint32_t>(rv));

	MT_RET(ret);
	MT_EE
}

void StoredChannel::Successor(const boost::shared_ptr<StoredUser> &in)
{
	MT_EB
	MT_FUNC("StoredChannel::Successor" << in);

	if (!in)
		cache.Put("successor", mantra::NullValue());
	else	
		cache.Put("successor", in->ID());

	MT_EE
}

boost::shared_ptr<StoredUser> StoredChannel::Successor() const
{
	MT_EB
	MT_FUNC("StoredChannel::Successor");

	boost::shared_ptr<StoredUser> ret;
	mantra::StorageValue rv = cache.Get("successor");
	if (rv.type() == typeid(mantra::NullValue))
		MT_RET(ret);
	ret = ROOT->data.Get_StoredUser(boost::get<boost::uint32_t>(rv));

	MT_RET(ret);
	MT_EE
}

void StoredChannel::Description(const std::string &in)
{
	MT_EB
	MT_FUNC("StoredChannel::Description" << in);

	cache.Put("description", in);

	MT_EE
}

std::string StoredChannel::Description() const
{
	MT_EB
	MT_FUNC("StoredChannel::Description");

	std::string ret;
	mantra::StorageValue rv = cache.Get("description");
	if (rv.type() == typeid(mantra::NullValue))
		MT_RET(ret);
	ret = boost::get<std::string>(rv);

	MT_RET(ret);
	MT_EE
}

void StoredChannel::Email(const std::string &in)
{
	MT_EB
	MT_FUNC("StoredChannel::Email" << in);

	if (in.empty())
		cache.Put("email", mantra::NullValue());
	else
		cache.Put("email", in);

	MT_EE
}

std::string StoredChannel::Email() const
{
	MT_EB
	MT_FUNC("StoredChannel::Email");

	std::string ret;
	mantra::StorageValue rv = cache.Get("email");
	if (rv.type() == typeid(mantra::NullValue))
		MT_RET(ret);
	ret = boost::get<std::string>(rv);

	MT_RET(ret);
	MT_EE
}

void StoredChannel::Website(const std::string &in)
{
	MT_EB
	MT_FUNC("StoredChannel::Website" << in);

	if (in.empty())
		cache.Put("website", mantra::NullValue());
	else
		cache.Put("website", in);

	MT_EE
}

std::string StoredChannel::Website() const
{
	MT_EB
	MT_FUNC("StoredChannel::Website");

	std::string ret;
	mantra::StorageValue rv = cache.Get("website");
	if (rv.type() == typeid(mantra::NullValue))
		MT_RET(ret);
	ret = boost::get<std::string>(rv);

	MT_RET(ret);
	MT_EE
}

void StoredChannel::Comment(const std::string &in)
{
	MT_EB
	MT_FUNC("StoredChannel::Comment" << in);

	if (in.empty())
		cache.Put("comment", mantra::NullValue());
	else
		cache.Put("comment", in);

	MT_EE
}

std::string StoredChannel::Comment() const
{
	MT_EB
	MT_FUNC("StoredChannel::Comment");

	std::string ret;
	mantra::StorageValue rv = cache.Get("comment");
	if (rv.type() == typeid(mantra::NullValue))
		MT_RET(ret);
	ret = boost::get<std::string>(rv);

	MT_RET(ret);
	MT_EE
}

void StoredChannel::Topic(const boost::shared_ptr<LiveUser> &user,
						  const std::string &topic)
{
	MT_EB
	MT_FUNC("StoredChannel::Topic" << user << topic);

	mantra::Storage::RecordMap rec;
	rec["topic"] = topic;
	rec["topic_setter"] = user->Name();
	rec["topic_set_time"] = mantra::GetCurrentDateTime();
	cache.Put(rec);

	MT_EE
}

std::string StoredChannel::Topic() const
{
	MT_EB
	MT_FUNC("StoredChannel::Topic");

	std::string ret;
	mantra::StorageValue rv = cache.Get("topic");
	if (rv.type() == typeid(mantra::NullValue))
		MT_RET(ret);
	ret = boost::get<std::string>(rv);

	MT_RET(ret);
	MT_EE
}

std::string StoredChannel::Topic_Setter() const
{
	MT_EB
	MT_FUNC("StoredChannel::Topici_Setter");

	std::string ret;
	mantra::StorageValue rv = cache.Get("topic_setter");
	if (rv.type() == typeid(mantra::NullValue))
		MT_RET(ret);
	ret = boost::get<std::string>(rv);

	MT_RET(ret);
	MT_EE
}

boost::posix_time::ptime StoredChannel::Topic_Set_Time() const
{
	MT_EB
	MT_FUNC("StoredChannel::Topic_Set_Time");

	boost::posix_time::ptime ret(boost::date_time::not_a_date_time);
	mantra::StorageValue rv = cache.Get("topic_set_time");
	if (rv.type() == typeid(mantra::NullValue))
		MT_RET(ret);
	ret = boost::get<boost::posix_time::ptime>(rv);

	MT_RET(ret);
	MT_EE
}

bool StoredChannel::KeepTopic(const boost::logic::tribool &in)
{
	MT_EB
	MT_FUNC("StoredChannel::KeepTopic" << in);

	if (LOCK_KeepTopic())
		MT_RET(false);

	if (boost::logic::indeterminate(in))
		cache.Put("keeptopic", mantra::NullValue::instance());
	else
		cache.Put("keeptopic", (bool) in);

	MT_RET(true);
	MT_EE
}

bool StoredChannel::KeepTopic() const
{
	MT_EB
	MT_FUNC("StoredChannel::KeepTopic");

	bool ret;
	if (ROOT->ConfigValue<bool>("chanserv.lock.keeptopic"))
		ret = ROOT->ConfigValue<bool>("chanserv.defaults.keeptopic");
	else
	{
		mantra::StorageValue rv = cache.Get("keeptopic");
		if (rv.type() == typeid(mantra::NullValue))
			ret = ROOT->ConfigValue<bool>("chanserv.defaults.keeptopic");
		else
			ret = boost::get<bool>(rv);
	}

	MT_RET(ret);
	MT_EE
}

bool StoredChannel::TopicLock(const boost::logic::tribool &in)
{
	MT_EB
	MT_FUNC("StoredChannel::TopicLock" << in);

	if (LOCK_TopicLock())
		MT_RET(false);

	if (boost::logic::indeterminate(in))
		cache.Put("topiclock", mantra::NullValue::instance());
	else
		cache.Put("topiclock", (bool) in);

	MT_RET(true);
	MT_EE
}

bool StoredChannel::TopicLock() const
{
	MT_EB
	MT_FUNC("StoredChannel::TopicLock");

	bool ret;
	if (ROOT->ConfigValue<bool>("chanserv.lock.topiclock"))
		ret = ROOT->ConfigValue<bool>("chanserv.defaults.topiclock");
	else
	{
		mantra::StorageValue rv = cache.Get("topiclock");
		if (rv.type() == typeid(mantra::NullValue))
			ret = ROOT->ConfigValue<bool>("chanserv.defaults.topiclock");
		else
			ret = boost::get<bool>(rv);
	}

	MT_RET(ret);
	MT_EE
}

bool StoredChannel::Private(const boost::logic::tribool &in)
{
	MT_EB
	MT_FUNC("StoredChannel::Private" << in);

	if (LOCK_Private())
		MT_RET(false);

	if (boost::logic::indeterminate(in))
		cache.Put("private", mantra::NullValue::instance());
	else
		cache.Put("private", (bool) in);

	MT_RET(true);
	MT_EE
}

bool StoredChannel::Private() const
{
	MT_EB
	MT_FUNC("StoredChannel::Private");

	bool ret;
	if (ROOT->ConfigValue<bool>("chanserv.lock.private"))
		ret = ROOT->ConfigValue<bool>("chanserv.defaults.private");
	else
	{
		mantra::StorageValue rv = cache.Get("private");
		if (rv.type() == typeid(mantra::NullValue))
			ret = ROOT->ConfigValue<bool>("chanserv.defaults.private");
		else
			ret = boost::get<bool>(rv);
	}

	MT_RET(ret);
	MT_EE
}

bool StoredChannel::SecureOps(const boost::logic::tribool &in)
{
	MT_EB
	MT_FUNC("StoredChannel::SecureOps" << in);

	if (LOCK_SecureOps())
		MT_RET(false);

	if (boost::logic::indeterminate(in))
		cache.Put("secureops", mantra::NullValue::instance());
	else
		cache.Put("secureops", (bool) in);

	MT_RET(true);
	MT_EE
}

bool StoredChannel::SecureOps() const
{
	MT_EB
	MT_FUNC("StoredChannel::SecureOps");

	bool ret;
	if (ROOT->ConfigValue<bool>("chanserv.lock.secureops"))
		ret = ROOT->ConfigValue<bool>("chanserv.defaults.secureops");
	else
	{
		mantra::StorageValue rv = cache.Get("secureops");
		if (rv.type() == typeid(mantra::NullValue))
			ret = ROOT->ConfigValue<bool>("chanserv.defaults.secureops");
		else
			ret = boost::get<bool>(rv);
	}

	MT_RET(ret);
	MT_EE
}

bool StoredChannel::Secure(const boost::logic::tribool &in)
{
	MT_EB
	MT_FUNC("StoredChannel::Secure" << in);

	if (LOCK_Secure())
		MT_RET(false);

	if (boost::logic::indeterminate(in))
		cache.Put("secure", mantra::NullValue::instance());
	else
		cache.Put("secure", (bool) in);

	MT_RET(true);
	MT_EE
}

bool StoredChannel::Secure() const
{
	MT_EB
	MT_FUNC("StoredChannel::Secure");

	bool ret;
	if (ROOT->ConfigValue<bool>("chanserv.lock.secure"))
		ret = ROOT->ConfigValue<bool>("chanserv.defaults.secure");
	else
	{
		mantra::StorageValue rv = cache.Get("secure");
		if (rv.type() == typeid(mantra::NullValue))
			ret = ROOT->ConfigValue<bool>("chanserv.defaults.secure");
		else
			ret = boost::get<bool>(rv);
	}

	MT_RET(ret);
	MT_EE
}

bool StoredChannel::Anarchy(const boost::logic::tribool &in)
{
	MT_EB
	MT_FUNC("StoredChannel::Anarchy" << in);

	if (LOCK_Anarchy())
		MT_RET(false);

	if (boost::logic::indeterminate(in))
		cache.Put("anarchy", mantra::NullValue::instance());
	else
		cache.Put("anarchy", (bool) in);

	MT_RET(true);
	MT_EE
}

bool StoredChannel::Anarchy() const
{
	MT_EB
	MT_FUNC("StoredChannel::Anarchy");

	bool ret;
	if (ROOT->ConfigValue<bool>("chanserv.lock.anarchy"))
		ret = ROOT->ConfigValue<bool>("chanserv.defaults.anarchy");
	else
	{
		mantra::StorageValue rv = cache.Get("anarchy");
		if (rv.type() == typeid(mantra::NullValue))
			ret = ROOT->ConfigValue<bool>("chanserv.defaults.anarchy");
		else
			ret = boost::get<bool>(rv);
	}

	MT_RET(ret);
	MT_EE
}

bool StoredChannel::KickOnBan(const boost::logic::tribool &in)
{
	MT_EB
	MT_FUNC("StoredChannel::KickOnBan" << in);

	if (LOCK_KickOnBan())
		MT_RET(false);

	if (boost::logic::indeterminate(in))
		cache.Put("kickonban", mantra::NullValue::instance());
	else
		cache.Put("kickonban", (bool) in);

	MT_RET(true);
	MT_EE
}

bool StoredChannel::KickOnBan() const
{
	MT_EB
	MT_FUNC("StoredChannel::KickOnBan");

	bool ret;
	if (ROOT->ConfigValue<bool>("chanserv.lock.kickonban"))
		ret = ROOT->ConfigValue<bool>("chanserv.defaults.kickonban");
	else
	{
		mantra::StorageValue rv = cache.Get("kickonban");
		if (rv.type() == typeid(mantra::NullValue))
			ret = ROOT->ConfigValue<bool>("chanserv.defaults.kickonban");
		else
			ret = boost::get<bool>(rv);
	}

	MT_RET(ret);
	MT_EE
}

bool StoredChannel::Restricted(const boost::logic::tribool &in)
{
	MT_EB
	MT_FUNC("StoredChannel::Restricted" << in);

	if (LOCK_Restricted())
		MT_RET(false);

	if (boost::logic::indeterminate(in))
		cache.Put("restricted", mantra::NullValue::instance());
	else
		cache.Put("restricted", (bool) in);

	MT_RET(true);
	MT_EE
}

bool StoredChannel::Restricted() const
{
	MT_EB
	MT_FUNC("StoredChannel::Restricted");

	bool ret;
	if (ROOT->ConfigValue<bool>("chanserv.lock.restricted"))
		ret = ROOT->ConfigValue<bool>("chanserv.defaults.restricted");
	else
	{
		mantra::StorageValue rv = cache.Get("restricted");
		if (rv.type() == typeid(mantra::NullValue))
			ret = ROOT->ConfigValue<bool>("chanserv.defaults.restricted");
		else
			ret = boost::get<bool>(rv);
	}

	MT_RET(ret);
	MT_EE
}

bool StoredChannel::CJoin(const boost::logic::tribool &in)
{
	MT_EB
	MT_FUNC("StoredChannel::CJoin" << in);

	if (LOCK_CJoin())
		MT_RET(false);

	if (boost::logic::indeterminate(in))
		cache.Put("cjoin", mantra::NullValue::instance());
	else
		cache.Put("cjoin", (bool) in);

	MT_RET(true);
	MT_EE
}

bool StoredChannel::CJoin() const
{
	MT_EB
	MT_FUNC("StoredChannel::CJoin");

	bool ret;
	if (ROOT->ConfigValue<bool>("chanserv.lock.cjoin"))
		ret = ROOT->ConfigValue<bool>("chanserv.defaults.cjoin");
	else
	{
		mantra::StorageValue rv = cache.Get("cjoin");
		if (rv.type() == typeid(mantra::NullValue))
			ret = ROOT->ConfigValue<bool>("chanserv.defaults.cjoin");
		else
			ret = boost::get<bool>(rv);
	}

	MT_RET(ret);
	MT_EE
}

bool StoredChannel::NoExpire(const boost::logic::tribool &in)
{
	MT_EB
	MT_FUNC("StoredChannel::NoExpire" << in);

	if (ROOT->ConfigValue<bool>("chanserv.lock.noexpire"))
		MT_RET(false);

	if (boost::logic::indeterminate(in))
		cache.Put("noexpire", mantra::NullValue::instance());
	else
		cache.Put("noexpire", (bool) in);

	MT_RET(true);
	MT_EE
}

bool StoredChannel::NoExpire() const
{
	MT_EB
	MT_FUNC("StoredChannel::NoExpire");

	bool ret;
	if (ROOT->ConfigValue<bool>("chanserv.lock.noexpire"))
		ret = ROOT->ConfigValue<bool>("chanserv.defaults.noexpire");
	else
	{
		mantra::StorageValue rv = cache.Get("noexpire");
		if (rv.type() == typeid(mantra::NullValue))
			ret = ROOT->ConfigValue<bool>("chanserv.defaults.noexpire");
		else
			ret = boost::get<bool>(rv);
	}

	MT_RET(ret);
	MT_EE
}

bool StoredChannel::BanTime(const mantra::duration &in)
{
	MT_EB
	MT_FUNC("StoredChannel::BanTime" << in);

	if (LOCK_BanTime())
		MT_RET(false);

	if (!in)
		cache.Put("bantime", mantra::NullValue::instance());
	else
		cache.Put("bantime", in);

	MT_RET(true);
	MT_EE
}

mantra::duration StoredChannel::BanTime() const
{
	MT_EB
	MT_FUNC("StoredChannel::BanTime");

	mantra::duration ret("");
	if (ROOT->ConfigValue<bool>("chanserv.lock.bantime"))
		ret = ROOT->ConfigValue<mantra::duration>("chanserv.defaults.bantime");
	else
	{
		mantra::StorageValue rv = cache.Get("bantime");
		if (rv.type() == typeid(mantra::NullValue))
			ret = ROOT->ConfigValue<mantra::duration>("chanserv.defaults.bantime");
		else
			ret = boost::get<mantra::duration>(rv);
	}

	MT_RET(ret);
	MT_EE
}

bool StoredChannel::PartTime(const mantra::duration &in)
{
	MT_EB
	MT_FUNC("StoredChannel::PartTime" << in);

	if (LOCK_PartTime())
		MT_RET(false);

	if (!in)
		cache.Put("parttime", mantra::NullValue::instance());
	else
		cache.Put("parttime", in);

	MT_RET(true);
	MT_EE
}

mantra::duration StoredChannel::PartTime() const
{
	MT_EB
	MT_FUNC("StoredChannel::PartTime");

	mantra::duration ret("");
	if (ROOT->ConfigValue<bool>("chanserv.lock.keeptopic"))
		ret = ROOT->ConfigValue<mantra::duration>("chanserv.defaults.parttime");
	else
	{
		mantra::StorageValue rv = cache.Get("parttime");
		if (rv.type() == typeid(mantra::NullValue))
			ret = ROOT->ConfigValue<mantra::duration>("chanserv.defaults.parttime");
		else
			ret = boost::get<mantra::duration>(rv);
	}

	MT_RET(ret);
	MT_EE
}

bool StoredChannel::Revenge(StoredChannel::Revenge_t in)
{
	MT_EB
	MT_FUNC("StoredChannel::Revenge" << in);

	if (LOCK_Revenge())
		MT_RET(false);

	if (in < R_None || in >= R_MAX)
		cache.Put("revenge", mantra::NullValue::instance());
	else
		cache.Put("revenge", (boost::uint8_t) in);

	MT_RET(true);
	MT_EE
}

StoredChannel::Revenge_t StoredChannel::Revenge() const
{
	MT_EB
	MT_FUNC("StoredChannel::Revenge");

	Revenge_t ret;
	if (ROOT->ConfigValue<bool>("chanserv.lock.revenge"))
		ret = (Revenge_t) ROOT->ConfigValue<boost::uint8_t>("chanserv.defaults.revenge");
	else
	{
		mantra::StorageValue rv = cache.Get("revenge");
		if (rv.type() == typeid(mantra::NullValue))
			ret = (Revenge_t) ROOT->ConfigValue<boost::uint8_t>("chanserv.defaults.revenge");
		else
			ret = (Revenge_t) boost::get<boost::uint8_t>(rv);
	}

	MT_RET(ret);
	MT_EE
}

std::string StoredChannel::ModeLock(const std::string &in)
{
	MT_EB
	MT_FUNC("ModeLock" << in);


	std::string rv;
	MT_RET(rv);
	MT_EE
}

std::string StoredChannel::ModeLock_On() const
{
	MT_EB
	MT_FUNC("StoredChannel::ModeLock_On");

	std::string ret;
	mantra::StorageValue rv = cache.Get("mlock_on");
	if (rv.type() == typeid(mantra::NullValue))
		ret = ROOT->ConfigValue<mantra::duration>("chanserv.defaults.mlock_on");
	else
		ret = boost::get<std::string>(rv);

	MT_RET(ret);
	MT_EE
}

std::string StoredChannel::ModeLock_Off() const
{
	MT_EB
	MT_FUNC("StoredChannel::ModeLock_Off");

	std::string ret;
	mantra::StorageValue rv = cache.Get("mlock_off");
	if (rv.type() == typeid(mantra::NullValue))
		ret = ROOT->ConfigValue<mantra::duration>("chanserv.defaults.mlock_off");
	else
		ret = boost::get<std::string>(rv);

	MT_RET(ret);
	MT_EE
}

std::string StoredChannel::ModeLock_Key() const
{
	MT_EB
	MT_FUNC("StoredChannel::ModeLock_Key");

	std::string ret;
	mantra::StorageValue rv = cache.Get("mlock_key");
	if (rv.type() != typeid(mantra::NullValue))
		ret = boost::get<std::string>(rv);

	MT_RET(ret);
	MT_EE
}

boost::uint32_t StoredChannel::ModeLock_Limit() const
{
	MT_EB
	MT_FUNC("StoredChannel::ModeLock_Limit");

	boost::uint32_t ret = 0;
	mantra::StorageValue rv = cache.Get("mlock_limit");
	if (rv.type() != typeid(mantra::NullValue))
		ret = boost::get<boost::uint32_t>(rv);

	MT_RET(ret);
	MT_EE
}

bool StoredChannel::LOCK_KeepTopic(const bool &in)
{
	MT_EB
	MT_FUNC("StoredChannel::LOCK_KeepTopic" << in);

	if (ROOT->ConfigValue<bool>("chanserv.lock.keeptopic"))
		MT_RET(false);

	cache.Put("lock_keeptopic", in);

	MT_RET(true);
	MT_EE
}

bool StoredChannel::LOCK_KeepTopic() const
{
	MT_EB
	MT_FUNC("StoredChannel::LOCK_KeepTopic");

	if (ROOT->ConfigValue<bool>("chanserv.lock.keeptopic"))
		MT_RET(true);

	mantra::StorageValue rv = cache.Get("lock_keeptopic");
	if (rv.type() == typeid(mantra::NullValue))
		MT_RET(false);
	bool ret = boost::get<bool>(rv);

	MT_RET(ret);
	MT_EE
}

bool StoredChannel::LOCK_TopicLock(const bool &in)
{
	MT_EB
	MT_FUNC("StoredChannel::LOCK_TopicLock" << in);

	if (ROOT->ConfigValue<bool>("chanserv.lock.topiclock"))
		MT_RET(false);

	cache.Put("lock_topiclock", in);

	MT_RET(true);
	MT_EE
}

bool StoredChannel::LOCK_TopicLock() const
{
	MT_EB
	MT_FUNC("StoredChannel::LOCK_TopicLock");

	if (ROOT->ConfigValue<bool>("chanserv.lock.topiclock"))
		MT_RET(true);

	mantra::StorageValue rv = cache.Get("lock_topiclock");
	if (rv.type() == typeid(mantra::NullValue))
		MT_RET(false);
	bool ret = boost::get<bool>(rv);

	MT_RET(ret);
	MT_EE
}

bool StoredChannel::LOCK_Private(const bool &in)
{
	MT_EB
	MT_FUNC("StoredChannel::LOCK_Private" << in);

	if (ROOT->ConfigValue<bool>("chanserv.lock.private"))
		MT_RET(false);

	cache.Put("lock_private", in);

	MT_RET(true);
	MT_EE
}

bool StoredChannel::LOCK_Private() const
{
	MT_EB
	MT_FUNC("StoredChannel::LOCK_Private");

	if (ROOT->ConfigValue<bool>("chanserv.lock.private"))
		MT_RET(true);

	mantra::StorageValue rv = cache.Get("lock_private");
	if (rv.type() == typeid(mantra::NullValue))
		MT_RET(false);
	bool ret = boost::get<bool>(rv);

	MT_RET(ret);
	MT_EE
}

bool StoredChannel::LOCK_SecureOps(const bool &in)
{
	MT_EB
	MT_FUNC("StoredChannel::LOCK_SecureOps" << in);

	if (ROOT->ConfigValue<bool>("chanserv.lock.secureops"))
		MT_RET(false);

	cache.Put("lock_secureops", in);

	MT_RET(true);
	MT_EE
}

bool StoredChannel::LOCK_SecureOps() const
{
	MT_EB
	MT_FUNC("StoredChannel::LOCK_SecureOps");

	if (ROOT->ConfigValue<bool>("chanserv.lock.secureops"))
		MT_RET(true);

	mantra::StorageValue rv = cache.Get("lock_secureops");
	if (rv.type() == typeid(mantra::NullValue))
		MT_RET(false);
	bool ret = boost::get<bool>(rv);

	MT_RET(ret);
	MT_EE
}

bool StoredChannel::LOCK_Secure(const bool &in)
{
	MT_EB
	MT_FUNC("StoredChannel::LOCK_Secure" << in);

	if (ROOT->ConfigValue<bool>("chanserv.lock.secure"))
		MT_RET(false);

	cache.Put("lock_secure", in);

	MT_RET(true);
	MT_EE
}

bool StoredChannel::LOCK_Secure() const
{
	MT_EB
	MT_FUNC("StoredChannel::LOCK_Secure");

	if (ROOT->ConfigValue<bool>("chanserv.lock.secure"))
		MT_RET(true);

	mantra::StorageValue rv = cache.Get("lock_secure");
	if (rv.type() == typeid(mantra::NullValue))
		MT_RET(false);
	bool ret = boost::get<bool>(rv);

	MT_RET(ret);
	MT_EE
}

bool StoredChannel::LOCK_Anarchy(const bool &in)
{
	MT_EB
	MT_FUNC("StoredChannel::LOCK_Anarchy" << in);

	if (ROOT->ConfigValue<bool>("chanserv.lock.anarchy"))
		MT_RET(false);

	cache.Put("lock_anarchy", in);

	MT_RET(true);
	MT_EE
}

bool StoredChannel::LOCK_Anarchy() const
{
	MT_EB
	MT_FUNC("StoredChannel::LOCK_Anarchy");

	if (ROOT->ConfigValue<bool>("chanserv.lock.anarchy"))
		MT_RET(true);

	mantra::StorageValue rv = cache.Get("lock_anarchy");
	if (rv.type() == typeid(mantra::NullValue))
		MT_RET(false);
	bool ret = boost::get<bool>(rv);

	MT_RET(ret);
	MT_EE
}

bool StoredChannel::LOCK_KickOnBan(const bool &in)
{
	MT_EB
	MT_FUNC("StoredChannel::LOCK_KickOnBan" << in);

	if (ROOT->ConfigValue<bool>("chanserv.lock.kickonban"))
		MT_RET(false);

	cache.Put("lock_kickonban", in);

	MT_RET(true);
	MT_EE
}

bool StoredChannel::LOCK_KickOnBan() const
{
	MT_EB
	MT_FUNC("StoredChannel::LOCK_KickOnBan");

	if (ROOT->ConfigValue<bool>("chanserv.lock.kickonban"))
		MT_RET(true);

	mantra::StorageValue rv = cache.Get("lock_kickonban");
	if (rv.type() == typeid(mantra::NullValue))
		MT_RET(false);
	bool ret = boost::get<bool>(rv);

	MT_RET(ret);
	MT_EE
}

bool StoredChannel::LOCK_Restricted(const bool &in)
{
	MT_EB
	MT_FUNC("StoredChannel::LOCK_Restricted" << in);

	if (ROOT->ConfigValue<bool>("chanserv.lock.restricted"))
		MT_RET(false);

	cache.Put("lock_restricted", in);

	MT_RET(true);
	MT_EE
}

bool StoredChannel::LOCK_Restricted() const
{
	MT_EB
	MT_FUNC("StoredChannel::LOCK_Restricted");

	if (ROOT->ConfigValue<bool>("chanserv.lock.restricted"))
		MT_RET(true);

	mantra::StorageValue rv = cache.Get("lock_restricted");
	if (rv.type() == typeid(mantra::NullValue))
		MT_RET(false);
	bool ret = boost::get<bool>(rv);

	MT_RET(ret);
	MT_EE
}

bool StoredChannel::LOCK_CJoin(const bool &in)
{
	MT_EB
	MT_FUNC("StoredChannel::LOCK_CJoin" << in);

	if (ROOT->ConfigValue<bool>("chanserv.lock.cjoin"))
		MT_RET(false);

	cache.Put("lock_cjoin", in);

	MT_RET(true);
	MT_EE
}

bool StoredChannel::LOCK_CJoin() const
{
	MT_EB
	MT_FUNC("StoredChannel::LOCK_CJoin");

	if (ROOT->ConfigValue<bool>("chanserv.lock.cjoin"))
		MT_RET(true);

	mantra::StorageValue rv = cache.Get("lock_cjoin");
	if (rv.type() == typeid(mantra::NullValue))
		MT_RET(false);
	bool ret = boost::get<bool>(rv);

	MT_RET(ret);
	MT_EE
}

bool StoredChannel::LOCK_BanTime(const bool &in)
{
	MT_EB
	MT_FUNC("StoredChannel::LOCK_BanTime" << in);

	if (ROOT->ConfigValue<bool>("chanserv.lock.bantime"))
		MT_RET(false);

	cache.Put("lock_bantime", in);

	MT_RET(true);
	MT_EE
}

bool StoredChannel::LOCK_BanTime() const
{
	MT_EB
	MT_FUNC("StoredChannel::LOCK_BanTime");

	if (ROOT->ConfigValue<bool>("chanserv.lock.bantime"))
		MT_RET(true);

	mantra::StorageValue rv = cache.Get("lock_bantime");
	if (rv.type() == typeid(mantra::NullValue))
		MT_RET(false);
	bool ret = boost::get<bool>(rv);

	MT_RET(ret);
	MT_EE
}

bool StoredChannel::LOCK_PartTime(const bool &in)
{
	MT_EB
	MT_FUNC("StoredChannel::LOCK_PartTime" << in);

	if (ROOT->ConfigValue<bool>("chanserv.lock.parttime"))
		MT_RET(false);

	cache.Put("lock_parttime", in);

	MT_RET(true);
	MT_EE
}

bool StoredChannel::LOCK_PartTime() const
{
	MT_EB
	MT_FUNC("StoredChannel::LOCK_PartTime");

	if (ROOT->ConfigValue<bool>("chanserv.lock.parttime"))
		MT_RET(true);

	mantra::StorageValue rv = cache.Get("lock_parttime");
	if (rv.type() == typeid(mantra::NullValue))
		MT_RET(false);
	bool ret = boost::get<bool>(rv);

	MT_RET(ret);
	MT_EE
}

bool StoredChannel::LOCK_Revenge(const bool &in)
{
	MT_EB
	MT_FUNC("StoredChannel::LOCK_Revenge" << in);

	if (ROOT->ConfigValue<bool>("chanserv.lock.revenge"))
		MT_RET(false);

	cache.Put("lock_revenge", in);

	MT_RET(true);
	MT_EE
}

bool StoredChannel::LOCK_Revenge() const
{
	MT_EB
	MT_FUNC("StoredChannel::LOCK_Revenge");

	if (ROOT->ConfigValue<bool>("chanserv.lock.revenge"))
		MT_RET(true);

	mantra::StorageValue rv = cache.Get("lock_revenge");
	if (rv.type() == typeid(mantra::NullValue))
		MT_RET(false);
	bool ret = boost::get<bool>(rv);

	MT_RET(ret);
	MT_EE
}

std::string StoredChannel::LOCK_ModeLock(const std::string &in)
{
	MT_EB
	MT_FUNC("StoredChannel::LOCK_ModeLock" << in);

	std::string rv;
	MT_RET(rv);
	MT_EE
}

std::string StoredChannel::LOCK_ModeLock_On() const
{
	MT_EB
	MT_FUNC("StoredChannel::LOCK_ModeLock_On");

	std::string ret;

	MT_RET(ret);
	MT_EE
}

std::string StoredChannel::LOCK_ModeLock_Off() const
{
	MT_EB
	MT_FUNC("StoredChannel::LOCK_ModeLock_Off");

	std::string ret;

	MT_RET(ret);
	MT_EE
}

void StoredChannel::Suspend(const boost::shared_ptr<StoredNick> &nick,
						 const std::string &reason)
{
	MT_EB
	MT_FUNC("StoredChannel::Suspend" << nick << reason);

	mantra::Storage::RecordMap rec;
	rec["suspend_by"] = nick->Name();
	rec["suspend_by_id"] = nick->User()->ID();
	rec["suspend_reason"] = reason;
	rec["suspend_time"] = mantra::GetCurrentDateTime();
	cache.Put(rec);

	MT_EE
}

void StoredChannel::Unsuspend()
{
	MT_EB
	MT_FUNC("StoredChannel::Unsuspend");

	mantra::Storage::RecordMap rec;
	rec["suspend_by"] = mantra::NullValue();
	rec["suspend_by_id"] = mantra::NullValue();
	rec["suspend_reason"] = mantra::NullValue();
	rec["suspend_time"] = mantra::NullValue();
	cache.Put(rec);

	MT_EE
}

std::string StoredChannel::Suspended_By() const
{
	MT_EB
	MT_FUNC("StoredChannel::Suspended_By");

	std::string ret;
	mantra::StorageValue rv = cache.Get("suspend_by");
	if (rv.type() == typeid(mantra::NullValue))
		MT_RET(ret);
	ret = boost::get<std::string>(rv);

	MT_RET(ret);
	MT_EE
}

boost::shared_ptr<StoredUser> StoredChannel::Suspended_ByShared() const
{
	MT_EB
	MT_FUNC("StoredChannel::Suspended_ByShared");

	boost::shared_ptr<StoredUser> ret;
	mantra::StorageValue rv = cache.Get("suspend_by_id");
	if (rv.type() == typeid(mantra::NullValue))
		MT_RET(ret);
	ret = ROOT->data.Get_StoredUser(boost::get<boost::uint32_t>(rv));

	MT_RET(ret);
	MT_EE
}

std::string StoredChannel::Suspend_Reason() const
{
	MT_EB
	MT_FUNC("StoredChannel::Suspend_Reason");

	std::string ret;
	mantra::StorageValue rv = cache.Get("suspend_reason");
	if (rv.type() == typeid(mantra::NullValue))
		MT_RET(ret);
	ret = boost::get<std::string>(rv);

	MT_RET(ret);
	MT_EE
}

boost::posix_time::ptime StoredChannel::Suspend_Time() const
{
	MT_EB
	MT_FUNC("StoredChannel::Suspend_Time");

	boost::posix_time::ptime ret(boost::date_time::not_a_date_time);
	mantra::StorageValue rv = cache.Get("suspend_time");
	if (rv.type() == typeid(mantra::NullValue))
		MT_RET(ret);
	ret = boost::get<boost::posix_time::ptime>(rv);

	MT_RET(ret);
	MT_EE
}

void StoredChannel::DropInternal()
{
	MT_EB
	MT_FUNC("StoredChannel::DropInternal");

	boost::mutex::scoped_lock sl(lock_);
	if (live_)
		if_LiveChannel_StoredChannel(live_).Stored(boost::shared_ptr<StoredChannel>());

	identified_users_t::const_iterator i;
	for (i=identified_users_.begin(); i!=identified_users_.end(); ++i)
		(*i)->UnIdentify(self.lock());
	identified_users_.clear();

	MT_EE
}

void StoredChannel::Drop()
{
	MT_EB
	MT_FUNC("StoredChannel::Drop");

	if_StorageDeleter<StoredChannel>(ROOT->data).Del(self.lock());

	MT_EE
}

static void check_option(std::string &str, const mantra::Storage::RecordMap &data,
						 const std::string &key, const std::string &description)
{
	MT_EB
	MT_FUNC("check_option");

	if (ROOT->ConfigValue<bool>(("chanserv.lock." + key).c_str()))
	{
		if (ROOT->ConfigValue<bool>(("chanserv.defaults." + key).c_str()))
		{
			if (!str.empty())
				str += ", ";
			str += '\002' + description + '\017';
		}
	}
	else
	{
		mantra::Storage::RecordMap::const_iterator i = data.find(key);
		if (i != data.end())
		{
			if (boost::get<bool>(i->second))
			{
				i = data.find("lock_" + key);
				if (i != data.end() && boost::get<bool>(i->second))
				{
					if (!str.empty())
						str += ", ";
					str += '\002' + description + '\017';
				}
				else
				{
					if (!str.empty())
						str += ", ";
					str += description;
				}
			}
		}
		else if (ROOT->ConfigValue<bool>(("chanserv.defaults." + key).c_str()))
		{
			if (!str.empty())
				str += ", ";
			str += description;
		}
	}

	MT_EE
}

void StoredChannel::SendInfo(const boost::shared_ptr<LiveUser> &service,
							 const boost::shared_ptr<LiveUser> &user) const
{
	MT_EB
	MT_FUNC("StoredChannel::SendInfo" << service << user);

	if (!service || !user || !service->GetService())
		return;

	mantra::Storage::RecordMap data;
	mantra::Storage::RecordMap::const_iterator i, j;
	cache.Get(data);

	bool opersop = (user->InCommittee(ROOT->ConfigValue<std::string>("commserv.oper.name")) ||
					user->InCommittee(ROOT->ConfigValue<std::string>("commserv.sop.name")));

	boost::shared_ptr<StoredNick> nick = user->Stored();

	i = data.find("secure");
	bool secure = (i != data.end() && !ROOT->ConfigValue<bool>("chanserv.lock.secure") ?
				   boost::get<bool>(i->second) : ROOT->ConfigValue<bool>("chanserv.defaults.secure"));

	boost::int32_t level = 0;
	if (user->Identified(self.lock()) ||
		(nick && nick->User()->ID() == boost::get<boost::uint32_t>(data["founder"]) &&
		 (!secure || user->Identified())))
	{
		level = ROOT->ConfigValue<int>("chanserv.max-level") + 1;
	}
	else if (!secure || user->Identified())
	{
		std::list<Access> acc(ACCESS_Matches(user));
		if (!acc.empty())
			level = acc.front().Level();
	}

	bool priv = false;
	if (!opersop && level <= 0)
	{
		i = data.find("private");
		priv = boost::get<bool>(i->second);
	}

	boost::shared_ptr<LiveChannel> live;
	{
		// SYNC_LOCK(live_);
		live = live_;
	}

	SEND(service, user, N_("Information on channel \002%1%\017:"), name_);

	i = data.find("description");
	if (i != data.end())
		SEND(service, user, N_("Description    : %1%"), boost::get<std::string>(i->second));

	SEND(service, user, N_("Registered     : %1% ago"),
		 DurationToString(mantra::duration(boost::get<boost::posix_time::ptime>(data["registered"]),
										   mantra::GetCurrentDateTime()), mantra::Second));

	boost::shared_ptr<StoredUser> stored = ROOT->data.Get_StoredUser(
			boost::get<boost::uint32_t>(data["founder"]));
	if (stored)
	{
		StoredUser::my_nicks_t nicks = stored->Nicks();
		std::string str;
		StoredUser::my_nicks_t::const_iterator k;
		for (k = nicks.begin(); k != nicks.end(); ++k)
		{
			if (!*k)
				continue;

			if (!str.empty())
				str += ", ";
			str += (*k)->Name();
		}
		SEND(service, user, N_("Founder        : %1%"), str);
	}

	i = data.find("successor");
	if (i != data.end())
	{
		stored = ROOT->data.Get_StoredUser(boost::get<boost::uint32_t>(i->second));
		if (stored)
		{
			StoredUser::my_nicks_t nicks = stored->Nicks();
			std::string str;
			StoredUser::my_nicks_t::const_iterator k;
			for (k = nicks.begin(); k != nicks.end(); ++k)
			{
				if (!*k)
					continue;

				if (!str.empty())
					str += ", ";
				str += (*k)->Name();
			}
			SEND(service, user, N_("Successor      : %1%"), str);
		}
	}

	i = data.find("email");
	if (i != data.end())
		SEND(service, user, N_("E-Mail         : %1%"), boost::get<std::string>(i->second));

	i = data.find("website");
	if (i != data.end())
		SEND(service, user, N_("Website        : %1%"), boost::get<std::string>(i->second));

	i = data.find("suspend_by");
	j = data.find("suspend_time");
	if (i != data.end() && j != data.end())
	{
		SEND(service, user, N_("Suspended By   : %1%, %2% ago."),
			 boost::get<std::string>(i->second) %
			 DurationToString(mantra::duration(boost::get<boost::posix_time::ptime>(j->second),
											   mantra::GetCurrentDateTime()), mantra::Second));
		i = data.find("suspend_reason");
		if (i != data.end())
		SEND(service, user, N_("Suspended For  : %1%"),
			 boost::get<std::string>(i->second));
	}

	if (!priv)
	{
		if (live)
		{
			LiveChannel::users_t users;
			live->Users(users);

			if (!users.empty())
			{
				size_t count = 0, ops = 0, halfops = 0, voices = 0;

				LiveChannel::users_t::iterator k;
				for (k = users.begin(); k != users.end(); ++k)
				{
					++count;
					if (k->second.find('o') != k->second.end())
						++ops;
					if (k->second.find('h') != k->second.end())
						++halfops;
					if (k->second.find('v') != k->second.end())
						++voices;
				}

				std::stringstream ss;
				ss << count << ' ' << UP_(user, "user", "users", count);
				if (ops)
					ss << ", " << ops << ' ' << UP_(user, "op", "ops", ops);
				if (halfops)
					ss << ", " << halfops << ' ' << UP_(user, "halfop", "halfops", halfops);
				if (voices)
					ss << ", " << voices << ' ' << UP_(user, "voice", "voices", voices);

				SEND(service, user, N_("In Use By      : %1%"), ss.str());

				if (!live->Topic().empty())
				{
					SEND(service, user, N_("Topic          : %1%"), live->Topic());
					SEND(service, user, N_("Topic Set By   : %1%, %2% ago."), live->Topic_Setter() %
						 DurationToString(mantra::duration(boost::get<boost::posix_time::ptime>(live->Topic_Set_Time()),
														   mantra::GetCurrentDateTime()), mantra::Second));
				}
			}
			else
			{
				// This should NOT happen ...
			}
		}
		else
		{
			i = data.find("last_used");
			if (i != data.end())
				SEND(service, user, N_("Last Used      : %1% ago."),
					 DurationToString(mantra::duration(boost::get<boost::posix_time::ptime>(i->second),
													   mantra::GetCurrentDateTime()), mantra::Second));
			i = data.find("topic");
			if (i != data.end())
			{
				SEND(service, user, N_("Last Topic     : %1%"), boost::get<std::string>(i->second));
				i = data.find("topic_setter");
				j = data.find("topic_set_time");
				if (i != data.end() && j != data.end())
					SEND(service, user, N_("Topic Set By   : %1%, %2% ago."),
						 boost::get<std::string>(i->second) %
						 DurationToString(mantra::duration(boost::get<boost::posix_time::ptime>(j->second),
														   mantra::GetCurrentDateTime()), mantra::Second));
			}
		}

		std::string str;

		i = data.find("mlock_on");
		j = data.find("mlock_off");
		if (i != data.end())
			str += '+' + boost::get<std::string>(i->second);
		if (j != data.end())
			str += '-' + boost::get<std::string>(j->second);
		if (!str.empty())
			SEND(service, user, N_("Mode Lock      : %1%"), str);

		Revenge_t r = (Revenge_t) ROOT->ConfigValue<unsigned int>("chanserv.defaults.revenge");
		if (ROOT->ConfigValue<bool>("chanserv.lock.revenge"))
		{
			if (r > R_None && r < R_MAX)
				SEND(service, user, N_("Revenge        : %1%"),
					 ('\002' + RevengeDesc(r) + '\017'));
		}
		else
		{
			i = data.find("revenge");
			if (i != data.end())
			{
				r = (Revenge_t) boost::get<boost::uint8_t>(i->second);
				if (r > R_None && r < R_MAX)
				{
					j = data.find("lock_revenge");
					if (j != data.end() && boost::get<bool>(j->second))
						SEND(service, user, N_("Revenge        : %1%"),
							 ('\002' + RevengeDesc(r) + '\017'));
					else
						SEND(service, user, N_("Revenge        : %1%"),
							 RevengeDesc(r));
				}
			}
			else if (r > R_None && r < R_MAX)
				SEND(service, user, N_("Revenge        : %1%"),
					 RevengeDesc(r));
		}

		mantra::duration d = ROOT->ConfigValue<mantra::duration>("chanserv.defaults.bantime");
		if (ROOT->ConfigValue<bool>("chanserv.lock.bantime"))
		{
			if (d)
				SEND(service, user, N_("Ban Expiry     : %1%"),
					 ('\002' + DurationToString(d, mantra::Second) + '\017'));
		}
		else
		{
			i = data.find("bantime");
			if (i != data.end())
			{
				d = boost::get<mantra::duration>(i->second);
				if (d)
				{
					j = data.find("lock_bantime");
					if (j != data.end() && boost::get<bool>(j->second))
						SEND(service, user, N_("Ban Expiry     : %1%"),
							 ('\002' + DurationToString(d, mantra::Second) + '\017'));
					else
						SEND(service, user, N_("Ban Expiry     : %1%"),
							 DurationToString(d, mantra::Second));
				}
			}
			else if (d)
				SEND(service, user, N_("Ban Expiry     : %1%"),
					 DurationToString(d, mantra::Second));
		}

		d = ROOT->ConfigValue<mantra::duration>("chanserv.defaults.parttime");
		if (ROOT->ConfigValue<bool>("chanserv.lock.parttime"))
		{
			if (d)
				SEND(service, user, N_("Greet Frequency: %1%"),
					 ('\002' + DurationToString(d, mantra::Second) + '\017'));
		}
		else
		{
			i = data.find("parttime");
			if (i != data.end())
			{
				d = boost::get<mantra::duration>(i->second);
				if (d)
				{
					j = data.find("lock_parttime");
					if (j != data.end() && boost::get<bool>(j->second))
						SEND(service, user, N_("Greet Frequency: %1%"),
							 ('\002' + DurationToString(d, mantra::Second) + '\017'));
					else
						SEND(service, user, N_("Greet Frequency: %1%"),
							 DurationToString(d, mantra::Second));
				}
			}
			else if (d)
				SEND(service, user, N_("Greet Frequency: %1%"),
					 DurationToString(d, mantra::Second));
		}

		str.clear();
		check_option(str, data, "keeptopic", U_(user, "Keep Topic"));
		check_option(str, data, "topiclock", U_(user, "Topic Lock"));
		check_option(str, data, "private", U_(user, "Private"));
		check_option(str, data, "secureops", U_(user, "Secure Ops"));
		check_option(str, data, "secure", U_(user, "Secure"));
		check_option(str, data, "anarchy", U_(user, "Anarchy"));
		check_option(str, data, "kickonban", U_(user, "Kick On Ban"));
		check_option(str, data, "restricted", U_(user, "Restricted"));
		check_option(str, data, "cjoin", U_(user, "Join"));
		if (opersop)
			check_option(str, data, "noexpire", U_(user, "No Expire"));
		if (!str.empty())
			SEND(service, user, N_("Options        : %1%"), str);
	}

	if (opersop)
	{
		i = data.find("comment");
		if (i != data.end())
			SEND(service, user, N_("OPER Comment   : %1%"),
				 boost::get<std::string>(i->second));
	}

	MT_EE
}

std::string StoredChannel::FilterModes(const boost::shared_ptr<LiveUser> &user,
									   const std::string &modes,
									   std::vector<std::string> &params) const
{
	MT_EB
	MT_FUNC("FilterModes" << user << modes << params);

	std::string ret = modes;
	// TODO: actually filter modes (duh)!

	MT_RET(ret);
	MT_EE
}

