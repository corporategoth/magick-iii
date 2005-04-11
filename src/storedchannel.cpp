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

StorageInterface StoredChannel::storage("channels", "name", "last_update");

StoredChannel::StoredChannel(const std::string &name,
							 const std::string &password,
							 const boost::shared_ptr<StoredUser> &founder)
	: name_(name)
{
	MT_EB
	MT_FUNC("StoredChannel::StoredChannel" << name << password << founder);

	mantra::Storage::RecordMap rec;
	rec["name"] = name;
	rec["password"] = ROOT->data.CryptPassword(password);
	rec["founder"] = founder->ID();
	storage.InsertRow(rec);

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

	mantra::Storage::DataSet data;
	mantra::Storage::FieldSet fields;
	fields.insert("topic");
	fields.insert("topic_setter");
	fields.insert("topic_set_time");
	fields.insert("topiclock");
	storage.RetrieveRow(data, mantra::Comparison<mantra::C_EqualToNC>::make("name", name_), fields);

	if (data.size() != 1)
		return; // ??

	bool topiclock;
	mantra::Storage::RecordMap::const_iterator i = data.front().find("topiclock");
	if (i == data.front().end())
		topiclock = ROOT->ConfigValue<bool>("chanserv.defaults.topiclock");
	else
		topiclock = boost::get<bool>(i->second);

	if (topiclock)
	{
		mantra::Storage::RecordMap::const_iterator i = data.front().find("topic");
		if (i == data.front().end() || boost::get<std::string>(i->second) != topic)
		{
			// Revert it back, we're locked!
		}
	}
	else
	{
		mantra::Storage::RecordMap rec;
		rec["topic"] = topic;
		rec["topic_setter"] = setter;
		rec["topic_set_time"] = set_time;
		storage.ChangeRow(rec, mantra::Comparison<mantra::C_EqualToNC>::make("name", name_));
	}

	MT_EE
}

void StoredChannel::Join(const boost::shared_ptr<LiveUser> &user)
{
	MT_EB
	MT_FUNC("StoredChannel::Join" << user);


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

	storage.PutField(name_, "password",
					 ROOT->data.CryptPassword(password));

	MT_EE
}

bool StoredChannel::CheckPassword(const std::string &password) const
{
	MT_EB
	MT_FUNC("StoredChannel::CheckPassword" << password);

	mantra::StorageValue rv = storage.GetField(name_, "password");
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

	storage.PutField(name_, "founder", in->ID());

	MT_EE
}

boost::shared_ptr<StoredUser> StoredChannel::Founder() const
{
	MT_EB
	MT_FUNC("StoredChannel::Founder");

	boost::shared_ptr<StoredUser> ret;
	mantra::StorageValue rv = storage.GetField(name_, "founder");
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
		storage.PutField(name_, "successor", mantra::NullValue());
	else	
		storage.PutField(name_, "successor", in->ID());

	MT_EE
}

boost::shared_ptr<StoredUser> StoredChannel::Successor() const
{
	MT_EB
	MT_FUNC("StoredChannel::Successor");

	boost::shared_ptr<StoredUser> ret;
	mantra::StorageValue rv = storage.GetField(name_, "successor");
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

	storage.PutField(name_, "description", in);

	MT_EE
}

std::string StoredChannel::Description() const
{
	MT_EB
	MT_FUNC("StoredChannel::Description");

	std::string ret;
	mantra::StorageValue rv = storage.GetField(name_, "description");
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

	storage.PutField(name_, "email", in);

	MT_EE
}

std::string StoredChannel::Email() const
{
	MT_EB
	MT_FUNC("StoredChannel::Email");

	std::string ret;
	mantra::StorageValue rv = storage.GetField(name_, "email");
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

	storage.PutField(name_, "website", in);

	MT_EE
}

std::string StoredChannel::Website() const
{
	MT_EB
	MT_FUNC("StoredChannel::Website");

	std::string ret;
	mantra::StorageValue rv = storage.GetField(name_, "website");
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

	storage.PutField(name_, "comment", in);

	MT_EE
}

std::string StoredChannel::Comment() const
{
	MT_EB
	MT_FUNC("StoredChannel::Comment");

	std::string ret;
	mantra::StorageValue rv = storage.GetField(name_, "comment");
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
	storage.ChangeRow(rec, mantra::Comparison<mantra::C_EqualToNC>::make("name", name_));

	MT_EE
}

std::string StoredChannel::Topic() const
{
	MT_EB
	MT_FUNC("StoredChannel::Topic");

	std::string ret;
	mantra::StorageValue rv = storage.GetField(name_, "topic");
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
	mantra::StorageValue rv = storage.GetField(name_, "topic_setter");
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
	mantra::StorageValue rv = storage.GetField(name_, "topic_set_time");
	if (rv.type() == typeid(mantra::NullValue))
		MT_RET(ret);
	ret = boost::get<boost::posix_time::ptime>(rv);

	MT_RET(ret);
	MT_EE
}

void StoredChannel::KeepTopic(const boost::logic::tribool &in)
{
	MT_EB
	MT_FUNC("StoredChannel::KeepTopic" << in);

	if (LOCK_KeepTopic())
		return;

	if (boost::logic::indeterminate(in))
		storage.PutField(name_, "keeptopic", mantra::NullValue::instance());
	else
		storage.PutField(name_, "keeptopic", (bool) in);

	MT_EE
}

bool StoredChannel::KeepTopic() const
{
	MT_EB
	MT_FUNC("StoredChannel::KeepTopic");

	bool ret;
	mantra::StorageValue rv = storage.GetField(name_, "keeptopic");
	if (rv.type() == typeid(mantra::NullValue))
		ret = ROOT->ConfigValue<bool>("chanserv.defaults.keeptopic");
	else
		ret = boost::get<bool>(rv);

	MT_RET(ret);
	MT_EE
}

void StoredChannel::TopicLock(const boost::logic::tribool &in)
{
	MT_EB
	MT_FUNC("StoredChannel::TopicLock" << in);

	if (LOCK_TopicLock())
		return;

	if (boost::logic::indeterminate(in))
		storage.PutField(name_, "topiclock", mantra::NullValue::instance());
	else
		storage.PutField(name_, "topiclock", (bool) in);

	MT_EE
}

bool StoredChannel::TopicLock() const
{
	MT_EB
	MT_FUNC("StoredChannel::TopicLock");

	bool ret;
	mantra::StorageValue rv = storage.GetField(name_, "topiclock");
	if (rv.type() == typeid(mantra::NullValue))
		ret = ROOT->ConfigValue<bool>("chanserv.defaults.topiclock");
	else
		ret = boost::get<bool>(rv);

	MT_RET(ret);
	MT_EE
}

void StoredChannel::Private(const boost::logic::tribool &in)
{
	MT_EB
	MT_FUNC("StoredChannel::Private" << in);

	if (LOCK_Private())
		return;

	if (boost::logic::indeterminate(in))
		storage.PutField(name_, "private", mantra::NullValue::instance());
	else
		storage.PutField(name_, "private", (bool) in);

	MT_EE
}

bool StoredChannel::Private() const
{
	MT_EB
	MT_FUNC("StoredChannel::Private");

	bool ret;
	mantra::StorageValue rv = storage.GetField(name_, "private");
	if (rv.type() == typeid(mantra::NullValue))
		ret = ROOT->ConfigValue<bool>("chanserv.defaults.private");
	else
		ret = boost::get<bool>(rv);

	MT_RET(ret);
	MT_EE
}

void StoredChannel::SecureOps(const boost::logic::tribool &in)
{
	MT_EB
	MT_FUNC("StoredChannel::SecureOps" << in);

	if (LOCK_SecureOps())
		return;

	if (boost::logic::indeterminate(in))
		storage.PutField(name_, "secureops", mantra::NullValue::instance());
	else
		storage.PutField(name_, "secureops", (bool) in);

	MT_EE
}

bool StoredChannel::SecureOps() const
{
	MT_EB
	MT_FUNC("StoredChannel::SecureOps");

	bool ret;
	mantra::StorageValue rv = storage.GetField(name_, "secureops");
	if (rv.type() == typeid(mantra::NullValue))
		ret = ROOT->ConfigValue<bool>("chanserv.defaults.secureops");
	else
		ret = boost::get<bool>(rv);

	MT_RET(ret);
	MT_EE
}

void StoredChannel::Secure(const boost::logic::tribool &in)
{
	MT_EB
	MT_FUNC("StoredChannel::Secure" << in);

	if (LOCK_Secure())
		return;

	if (boost::logic::indeterminate(in))
		storage.PutField(name_, "secure", mantra::NullValue::instance());
	else
		storage.PutField(name_, "secure", (bool) in);

	MT_EE
}

bool StoredChannel::Secure() const
{
	MT_EB
	MT_FUNC("StoredChannel::Secure");

	bool ret;
	mantra::StorageValue rv = storage.GetField(name_, "secure");
	if (rv.type() == typeid(mantra::NullValue))
		ret = ROOT->ConfigValue<bool>("chanserv.defaults.secure");
	else
		ret = boost::get<bool>(rv);

	MT_RET(ret);
	MT_EE
}

void StoredChannel::Anarchy(const boost::logic::tribool &in)
{
	MT_EB
	MT_FUNC("StoredChannel::Anarchy" << in);

	if (LOCK_Anarchy())
		return;

	if (boost::logic::indeterminate(in))
		storage.PutField(name_, "anarchy", mantra::NullValue::instance());
	else
		storage.PutField(name_, "anarchy", (bool) in);

	MT_EE
}

bool StoredChannel::Anarchy() const
{
	MT_EB
	MT_FUNC("StoredChannel::Anarchy");

	bool ret;
	mantra::StorageValue rv = storage.GetField(name_, "anarchy");
	if (rv.type() == typeid(mantra::NullValue))
		ret = ROOT->ConfigValue<bool>("chanserv.defaults.anarchy");
	else
		ret = boost::get<bool>(rv);

	MT_RET(ret);
	MT_EE
}

void StoredChannel::KickOnBan(const boost::logic::tribool &in)
{
	MT_EB
	MT_FUNC("StoredChannel::KickOnBan" << in);

	if (LOCK_KickOnBan())
		return;

	if (boost::logic::indeterminate(in))
		storage.PutField(name_, "kickonban", mantra::NullValue::instance());
	else
		storage.PutField(name_, "kickonban", (bool) in);

	MT_EE
}

bool StoredChannel::KickOnBan() const
{
	MT_EB
	MT_FUNC("StoredChannel::KickOnBan");

	bool ret;
	mantra::StorageValue rv = storage.GetField(name_, "kickonban");
	if (rv.type() == typeid(mantra::NullValue))
		ret = ROOT->ConfigValue<bool>("chanserv.defaults.kickonban");
	else
		ret = boost::get<bool>(rv);

	MT_RET(ret);
	MT_EE
}

void StoredChannel::Restricted(const boost::logic::tribool &in)
{
	MT_EB
	MT_FUNC("StoredChannel::Restricted" << in);

	if (LOCK_Restricted())
		return;

	if (boost::logic::indeterminate(in))
		storage.PutField(name_, "restricted", mantra::NullValue::instance());
	else
		storage.PutField(name_, "restricted", (bool) in);

	MT_EE
}

bool StoredChannel::Restricted() const
{
	MT_EB
	MT_FUNC("StoredChannel::Restricted");

	bool ret;
	mantra::StorageValue rv = storage.GetField(name_, "restricted");
	if (rv.type() == typeid(mantra::NullValue))
		ret = ROOT->ConfigValue<bool>("chanserv.defaults.restricted");
	else
		ret = boost::get<bool>(rv);

	MT_RET(ret);
	MT_EE
}

void StoredChannel::CJoin(const boost::logic::tribool &in)
{
	MT_EB
	MT_FUNC("StoredChannel::CJoin" << in);

	if (LOCK_CJoin())
		return;

	if (boost::logic::indeterminate(in))
		storage.PutField(name_, "cjoin", mantra::NullValue::instance());
	else
		storage.PutField(name_, "cjoin", (bool) in);

	MT_EE
}

bool StoredChannel::CJoin() const
{
	MT_EB
	MT_FUNC("StoredChannel::CJoin");

	bool ret;
	mantra::StorageValue rv = storage.GetField(name_, "cjoin");
	if (rv.type() == typeid(mantra::NullValue))
		ret = ROOT->ConfigValue<bool>("chanserv.defaults.cjoin");
	else
		ret = boost::get<bool>(rv);

	MT_RET(ret);
	MT_EE
}

void StoredChannel::NoExpire(const boost::logic::tribool &in)
{
	MT_EB
	MT_FUNC("StoredChannel::NoExpire" << in);

	if (ROOT->ConfigValue<bool>("chanserv.lock.noexpire"))
		return;

	if (boost::logic::indeterminate(in))
		storage.PutField(name_, "noexpire", mantra::NullValue::instance());
	else
		storage.PutField(name_, "noexpire", (bool) in);

	MT_EE
}

bool StoredChannel::NoExpire() const
{
	MT_EB
	MT_FUNC("StoredChannel::NoExpire");

	if (ROOT->ConfigValue<bool>("chanserv.lock.noexpire"))
		MT_RET(true);

	bool ret;
	mantra::StorageValue rv = storage.GetField(name_, "noexpire");
	if (rv.type() == typeid(mantra::NullValue))
		ret = ROOT->ConfigValue<bool>("chanserv.defaults.noexpire");
	else
		ret = boost::get<bool>(rv);

	MT_RET(ret);
	MT_EE
}

void StoredChannel::BanTime(const mantra::duration &in)
{
	MT_EB
	MT_FUNC("StoredChannel::BanTime" << in);

	if (LOCK_BanTime())
		return;

	if (!in)
		storage.PutField(name_, "bantime", mantra::NullValue::instance());
	else
		storage.PutField(name_, "bantime", in);

	MT_EE
}

mantra::duration StoredChannel::BanTime() const
{
	MT_EB
	MT_FUNC("StoredChannel::BanTime");

	mantra::duration ret("");
	mantra::StorageValue rv = storage.GetField(name_, "bantime");
	if (rv.type() == typeid(mantra::NullValue))
		ret = ROOT->ConfigValue<mantra::duration>("chanserv.defaults.bantime");
	else
		ret = boost::get<mantra::duration>(rv);

	MT_RET(ret);
	MT_EE
}

void StoredChannel::PartTime(const mantra::duration &in)
{
	MT_EB
	MT_FUNC("StoredChannel::PartTime" << in);

	if (LOCK_PartTime())
		return;

	if (!in)
		storage.PutField(name_, "parttime", mantra::NullValue::instance());
	else
		storage.PutField(name_, "parttime", in);

	MT_EE
}

mantra::duration StoredChannel::PartTime() const
{
	MT_EB
	MT_FUNC("StoredChannel::PartTime");

	mantra::duration ret("");
	mantra::StorageValue rv = storage.GetField(name_, "parttime");
	if (rv.type() == typeid(mantra::NullValue))
		ret = ROOT->ConfigValue<mantra::duration>("chanserv.defaults.parttime");
	else
		ret = boost::get<mantra::duration>(rv);

	MT_RET(ret);
	MT_EE
}

void StoredChannel::Revenge(StoredChannel::Revenge_t in)
{
	MT_EB
	MT_FUNC("StoredChannel::Revenge" << in);

	if (LOCK_Revenge())
		return;

	if (in < R_None || in >= R_MAX)
		storage.PutField(name_, "revenge", mantra::NullValue::instance());
	else
		storage.PutField(name_, "revenge", (boost::uint8_t) in);

	MT_EE
}

StoredChannel::Revenge_t StoredChannel::Revenge() const
{
	MT_EB
	MT_FUNC("StoredChannel::Revenge");

	Revenge_t ret;
	mantra::StorageValue rv = storage.GetField(name_, "revenge");
	if (rv.type() == typeid(mantra::NullValue))
		ret = (Revenge_t) ROOT->ConfigValue<boost::uint8_t>("chanserv.defaults.revenge");
	else
		ret = (Revenge_t) boost::get<boost::uint8_t>(rv);

	MT_RET(ret);
	MT_EE
}

void StoredChannel::ModeLock(const std::string &in)
{
	MT_EB
	MT_FUNC("ModeLock" << in);


	MT_EE
}

std::string StoredChannel::ModeLock_On() const
{
	MT_EB
	MT_FUNC("StoredChannel::ModeLock_On");

	std::string ret;
	mantra::StorageValue rv = storage.GetField(name_, "mlock_on");
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
	mantra::StorageValue rv = storage.GetField(name_, "mlock_off");
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
	mantra::StorageValue rv = storage.GetField(name_, "mlock_key");
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
	mantra::StorageValue rv = storage.GetField(name_, "mlock_limit");
	if (rv.type() != typeid(mantra::NullValue))
		ret = boost::get<boost::uint32_t>(rv);

	MT_RET(ret);
	MT_EE
}

void StoredChannel::LOCK_KeepTopic(const bool &in)
{
	MT_EB
	MT_FUNC("StoredChannel::LOCK_KeepTopic" << in);

	if (ROOT->ConfigValue<bool>("chanserv.lock.keeptopic"))
		return;

	storage.PutField(name_, "lock_keeptopic", in);

	MT_EE
}

bool StoredChannel::LOCK_KeepTopic() const
{
	MT_EB
	MT_FUNC("StoredChannel::LOCK_KeepTopic");

	if (ROOT->ConfigValue<bool>("chanserv.lock.keeptopic"))
		MT_RET(true);

	mantra::StorageValue rv = storage.GetField(name_, "lock_keeptopic");
	if (rv.type() == typeid(mantra::NullValue))
		MT_RET(false);
	bool ret = boost::get<bool>(rv);

	MT_RET(ret);
	MT_EE
}

void StoredChannel::LOCK_TopicLock(const bool &in)
{
	MT_EB
	MT_FUNC("StoredChannel::LOCK_TopicLock" << in);

	if (ROOT->ConfigValue<bool>("chanserv.lock.topiclock"))
		return;

	storage.PutField(name_, "lock_topiclock", in);

	MT_EE
}

bool StoredChannel::LOCK_TopicLock() const
{
	MT_EB
	MT_FUNC("StoredChannel::LOCK_TopicLock");

	if (ROOT->ConfigValue<bool>("chanserv.lock.topiclock"))
		MT_RET(true);

	mantra::StorageValue rv = storage.GetField(name_, "lock_topiclock");
	if (rv.type() == typeid(mantra::NullValue))
		MT_RET(false);
	bool ret = boost::get<bool>(rv);

	MT_RET(ret);
	MT_EE
}

void StoredChannel::LOCK_Private(const bool &in)
{
	MT_EB
	MT_FUNC("StoredChannel::LOCK_Private" << in);

	if (ROOT->ConfigValue<bool>("chanserv.lock.private"))
		return;

	storage.PutField(name_, "lock_private", in);

	MT_EE
}

bool StoredChannel::LOCK_Private() const
{
	MT_EB
	MT_FUNC("StoredChannel::LOCK_Private");

	if (ROOT->ConfigValue<bool>("chanserv.lock.private"))
		MT_RET(true);

	mantra::StorageValue rv = storage.GetField(name_, "lock_private");
	if (rv.type() == typeid(mantra::NullValue))
		MT_RET(false);
	bool ret = boost::get<bool>(rv);

	MT_RET(ret);
	MT_EE
}

void StoredChannel::LOCK_SecureOps(const bool &in)
{
	MT_EB
	MT_FUNC("StoredChannel::LOCK_SecureOps" << in);

	if (ROOT->ConfigValue<bool>("chanserv.lock.secureops"))
		return;

	storage.PutField(name_, "lock_secureops", in);

	MT_EE
}

bool StoredChannel::LOCK_SecureOps() const
{
	MT_EB
	MT_FUNC("StoredChannel::LOCK_SecureOps");

	if (ROOT->ConfigValue<bool>("chanserv.lock.secureops"))
		MT_RET(true);

	mantra::StorageValue rv = storage.GetField(name_, "lock_secureops");
	if (rv.type() == typeid(mantra::NullValue))
		MT_RET(false);
	bool ret = boost::get<bool>(rv);

	MT_RET(ret);
	MT_EE
}

void StoredChannel::LOCK_Secure(const bool &in)
{
	MT_EB
	MT_FUNC("StoredChannel::LOCK_Secure" << in);

	if (ROOT->ConfigValue<bool>("chanserv.lock.secure"))
		return;

	storage.PutField(name_, "lock_secure", in);

	MT_EE
}

bool StoredChannel::LOCK_Secure() const
{
	MT_EB
	MT_FUNC("StoredChannel::LOCK_Secure");

	if (ROOT->ConfigValue<bool>("chanserv.lock.secure"))
		MT_RET(true);

	mantra::StorageValue rv = storage.GetField(name_, "lock_secure");
	if (rv.type() == typeid(mantra::NullValue))
		MT_RET(false);
	bool ret = boost::get<bool>(rv);

	MT_RET(ret);
	MT_EE
}

void StoredChannel::LOCK_Anarchy(const bool &in)
{
	MT_EB
	MT_FUNC("StoredChannel::LOCK_Anarchy" << in);

	if (ROOT->ConfigValue<bool>("chanserv.lock.anarchy"))
		return;

	storage.PutField(name_, "lock_anarchy", in);

	MT_EE
}

bool StoredChannel::LOCK_Anarchy() const
{
	MT_EB
	MT_FUNC("StoredChannel::LOCK_Anarchy");

	if (ROOT->ConfigValue<bool>("chanserv.lock.anarchy"))
		MT_RET(true);

	mantra::StorageValue rv = storage.GetField(name_, "lock_anarchy");
	if (rv.type() == typeid(mantra::NullValue))
		MT_RET(false);
	bool ret = boost::get<bool>(rv);

	MT_RET(ret);
	MT_EE
}

void StoredChannel::LOCK_KickOnBan(const bool &in)
{
	MT_EB
	MT_FUNC("StoredChannel::LOCK_KickOnBan" << in);

	if (ROOT->ConfigValue<bool>("chanserv.lock.kickonban"))
		return;

	storage.PutField(name_, "lock_kickonban", in);

	MT_EE
}

bool StoredChannel::LOCK_KickOnBan() const
{
	MT_EB
	MT_FUNC("StoredChannel::LOCK_KickOnBan");

	if (ROOT->ConfigValue<bool>("chanserv.lock.kickonban"))
		MT_RET(true);

	mantra::StorageValue rv = storage.GetField(name_, "lock_kickonban");
	if (rv.type() == typeid(mantra::NullValue))
		MT_RET(false);
	bool ret = boost::get<bool>(rv);

	MT_RET(ret);
	MT_EE
}

void StoredChannel::LOCK_Restricted(const bool &in)
{
	MT_EB
	MT_FUNC("StoredChannel::LOCK_Restricted" << in);

	if (ROOT->ConfigValue<bool>("chanserv.lock.restricted"))
		return;

	storage.PutField(name_, "lock_restricted", in);

	MT_EE
}

bool StoredChannel::LOCK_Restricted() const
{
	MT_EB
	MT_FUNC("StoredChannel::LOCK_Restricted");

	if (ROOT->ConfigValue<bool>("chanserv.lock.restricted"))
		MT_RET(true);

	mantra::StorageValue rv = storage.GetField(name_, "lock_restricted");
	if (rv.type() == typeid(mantra::NullValue))
		MT_RET(false);
	bool ret = boost::get<bool>(rv);

	MT_RET(ret);
	MT_EE
}

void StoredChannel::LOCK_CJoin(const bool &in)
{
	MT_EB
	MT_FUNC("StoredChannel::LOCK_CJoin" << in);

	if (ROOT->ConfigValue<bool>("chanserv.lock.cjoin"))
		return;

	storage.PutField(name_, "lock_cjoin", in);

	MT_EE
}

bool StoredChannel::LOCK_CJoin() const
{
	MT_EB
	MT_FUNC("StoredChannel::LOCK_CJoin");

	if (ROOT->ConfigValue<bool>("chanserv.lock.cjoin"))
		MT_RET(true);

	mantra::StorageValue rv = storage.GetField(name_, "lock_cjoin");
	if (rv.type() == typeid(mantra::NullValue))
		MT_RET(false);
	bool ret = boost::get<bool>(rv);

	MT_RET(ret);
	MT_EE
}

void StoredChannel::LOCK_BanTime(const bool &in)
{
	MT_EB
	MT_FUNC("StoredChannel::LOCK_BanTime" << in);

	if (ROOT->ConfigValue<bool>("chanserv.lock.bantime"))
		return;

	storage.PutField(name_, "lock_bantime", in);

	MT_EE
}

bool StoredChannel::LOCK_BanTime() const
{
	MT_EB
	MT_FUNC("StoredChannel::LOCK_BanTime");

	if (ROOT->ConfigValue<bool>("chanserv.lock.bantime"))
		MT_RET(true);

	mantra::StorageValue rv = storage.GetField(name_, "lock_bantime");
	if (rv.type() == typeid(mantra::NullValue))
		MT_RET(false);
	bool ret = boost::get<bool>(rv);

	MT_RET(ret);
	MT_EE
}

void StoredChannel::LOCK_PartTime(const bool &in)
{
	MT_EB
	MT_FUNC("StoredChannel::LOCK_PartTime" << in);

	if (ROOT->ConfigValue<bool>("chanserv.lock.parttime"))
		return;

	storage.PutField(name_, "lock_parttime", in);

	MT_EE
}

bool StoredChannel::LOCK_PartTime() const
{
	MT_EB
	MT_FUNC("StoredChannel::LOCK_PartTime");

	if (ROOT->ConfigValue<bool>("chanserv.lock.parttime"))
		MT_RET(true);

	mantra::StorageValue rv = storage.GetField(name_, "lock_parttime");
	if (rv.type() == typeid(mantra::NullValue))
		MT_RET(false);
	bool ret = boost::get<bool>(rv);

	MT_RET(ret);
	MT_EE
}

void StoredChannel::LOCK_Revenge(const bool &in)
{
	MT_EB
	MT_FUNC("StoredChannel::LOCK_Revenge" << in);

	if (ROOT->ConfigValue<bool>("chanserv.lock.revenge"))
		return;

	storage.PutField(name_, "lock_revenge", in);

	MT_EE
}

bool StoredChannel::LOCK_Revenge() const
{
	MT_EB
	MT_FUNC("StoredChannel::LOCK_Revenge");

	if (ROOT->ConfigValue<bool>("chanserv.lock.revenge"))
		MT_RET(true);

	mantra::StorageValue rv = storage.GetField(name_, "lock_revenge");
	if (rv.type() == typeid(mantra::NullValue))
		MT_RET(false);
	bool ret = boost::get<bool>(rv);

	MT_RET(ret);
	MT_EE
}

void StoredChannel::LOCK_ModeLock(const std::string &in)
{
	MT_EB
	MT_FUNC("StoredChannel::LOCK_ModeLock" << in);


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
	storage.ChangeRow(rec, mantra::Comparison<mantra::C_EqualToNC>::make("namr", name_));

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
	storage.ChangeRow(rec, mantra::Comparison<mantra::C_EqualToNC>::make("name", name_));

	MT_EE
}

std::string StoredChannel::Suspended_By() const
{
	MT_EB
	MT_FUNC("StoredChannel::Suspended_By");

	std::string ret;
	mantra::StorageValue rv = storage.GetField(name_, "suspend_by");
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
	mantra::StorageValue rv = storage.GetField(name_, "suspend_by_id");
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
	mantra::StorageValue rv = storage.GetField(name_, "suspend_reason");
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
	mantra::StorageValue rv = storage.GetField(name_, "suspend_time");
	if (rv.type() == typeid(mantra::NullValue))
		MT_RET(ret);
	ret = boost::get<boost::posix_time::ptime>(rv);

	MT_RET(ret);
	MT_EE
}
