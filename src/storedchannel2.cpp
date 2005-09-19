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
#define RCSID(x,y) const char *rcsid_magick__storedchannel2_cpp_ ## x () { return y; }
RCSID(magick__storedchannel2_cpp, "@(#)$Id$");

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

boost::read_write_mutex StoredChannel::Level::DefaultLevelsLock(reader_priority);
std::map<boost::uint32_t, StoredChannel::Level::DefaultLevel> StoredChannel::Level::DefaultLevels;

StorageInterface StoredChannel::Level::storage("channels_level", std::string(), "last_update");
StorageInterface StoredChannel::Access::storage("channels_access", std::string(), "last_update");
StorageInterface StoredChannel::AutoKick::storage("channels_akick", std::string(), "last_update");
StorageInterface StoredChannel::Greet::storage("channels_greet", std::string(), "last_update");
StorageInterface StoredChannel::Message::storage("channels_message", std::string(), "last_update");
//StorageInterface News::storage("channels_news", std::string(), "last_update");

void StoredChannel::Level::Default(boost::uint32_t level, boost::int32_t value,
								   const boost::regex &name, const std::string &desc)
{
	MT_EB
	MT_FUNC("StoredChannel::Level::Default" << level << value);

	DefaultLevel dl;
	dl.value = value;
	dl.name = boost::regex(name.str(), name.flags() | boost::regex_constants::icase);
	dl.desc = desc;

	boost::read_write_mutex::scoped_write_lock scoped_lock(DefaultLevelsLock);
	DefaultLevels[level] = dl;

	MT_EE
}

boost::int32_t StoredChannel::Level::Default(boost::uint32_t level)
{
	MT_EB
	MT_FUNC("StoredChannel::Level::Default" << level);

	boost::int32_t rv = ROOT->ConfigValue<int>("chanserv.min-level") - 1;
	boost::read_write_mutex::scoped_read_lock scoped_lock(DefaultLevelsLock);
	std::map<boost::uint32_t, DefaultLevel>::iterator i = DefaultLevels.find(level);
	if (i != DefaultLevels.end())
		rv = i->second.value;

	MT_RET(rv);
	MT_EE
}

boost::uint32_t StoredChannel::Level::DefaultName(const std::string &name)
{
	MT_EB
	MT_FUNC("StoredChannel::Level::Default" << name);

	boost::read_write_mutex::scoped_read_lock scoped_lock(DefaultLevelsLock);
	std::map<boost::uint32_t, DefaultLevel>::iterator i;
	for (i = DefaultLevels.begin(); i != DefaultLevels.end(); ++i)
		if (boost::regex_match(name, i->second.name))
			MT_RET(i->first);

	MT_RET(0);
	MT_EE
}

std::string StoredChannel::Level::DefaultDesc(boost::uint32_t level,
											  const boost::shared_ptr<LiveUser> &user)
{
	MT_EB
	MT_FUNC("StoredChannel::Level::Default" << level << user);

	std::string rv;
	boost::read_write_mutex::scoped_read_lock scoped_lock(DefaultLevelsLock);
	std::map<boost::uint32_t, DefaultLevel>::iterator i = DefaultLevels.find(level);
	if (i != DefaultLevels.end())
	{
		if (user)
			rv = user->translate(i->second.desc);
		else
			rv = mantra::translate::get(i->second.desc);
	}

	MT_RET(rv);
	MT_EE
}

StoredChannel::Level::Level(const boost::shared_ptr<StoredChannel> &owner,
							boost::uint32_t id)
	: cache(storage, ROOT->ConfigValue<mantra::duration>("general.cache-expire"),
			mantra::Comparison<mantra::C_EqualTo>::make("id", owner->ID()) &&
			mantra::Comparison<mantra::C_EqualTo>::make("level", id)),
	  owner_(owner), id_(id)
{
	MT_EB
	MT_FUNC("StoredChannel::Level::Level" << owner << id);


	MT_EE
}

void StoredChannel::Level::Change(boost::int32_t value, const boost::shared_ptr<StoredNick> &updater)
{
	MT_EB
	MT_FUNC("StoredChannel::Level::Change" << value << updater);

	mantra::Storage::RecordMap data;
	data["value"] = value;
	data["last_update"] = mantra::GetCurrentDateTime();
	if (updater)
	{
		data["last_updater"] = updater->Name();
		data["last_updater_id"] = updater->User()->ID();
	}
	else
	{
		data["last_updater"] = mantra::NullValue();
		data["last_updater_id"] = mantra::NullValue();
	}

	if (storage.ChangeRow(data,
				mantra::Comparison<mantra::C_EqualTo>::make("id", owner_->ID()) &&
				mantra::Comparison<mantra::C_EqualTo>::make("level", id_)) == 0)
	{
		data["id"] = owner_->ID();
		data["level"] = id_;
		storage.InsertRow(data);
	}
	cache.Clear();

	MT_EE
}

boost::int32_t StoredChannel::Level::Value() const
{
	MT_EB
	MT_FUNC("StoredChannel::Level::Value");

	mantra::StorageValue rv = cache.Get("value");

	boost::int32_t ret;
	if (rv.type() != typeid(mantra::NullValue))
		ret = boost::get<boost::int32_t>(rv);
	else
		ret = Default(id_);

	MT_RET(ret);
	MT_EE
}

std::string StoredChannel::Level::Last_UpdaterName() const
{
	MT_EB
	MT_FUNC("StoredChannel::Level::Last_UpdaterName");

	mantra::StorageValue rv = cache.Get("last_updater");

	std::string ret;
	if (rv.type() != typeid(mantra::NullValue))
		ret = boost::get<std::string>(rv);

	MT_RET(ret);
	MT_EE
}

boost::shared_ptr<StoredUser> StoredChannel::Level::Last_Updater() const
{
	MT_EB
	MT_FUNC("StoredChannel::Level::Last_Updater");

	mantra::StorageValue rv = cache.Get("last_updater_id");

	boost::shared_ptr<StoredUser> ret;
	if (rv.type() != typeid(mantra::NullValue))
		ret = ROOT->data.Get_StoredUser(boost::get<boost::uint32_t>(rv));

	MT_RET(ret);
	MT_EE
}

boost::posix_time::ptime StoredChannel::Level::Last_Update() const
{
	MT_EB
	MT_FUNC("StoredChannel::Level::Last_Update");

	mantra::StorageValue rv = cache.Get("last_update");

	boost::posix_time::ptime ret;
	if (rv.type() != typeid(mantra::NullValue))
		ret = boost::get<boost::posix_time::ptime>(rv);

	MT_RET(ret);
	MT_EE
}

bool StoredChannel::LEVEL_Exists(boost::uint32_t id) const
{
	MT_EB
	MT_FUNC("StoredChannel::LEVEL_Exists" << id);

	bool rv = (Level::storage.RowExists(mantra::Comparison<mantra::C_EqualTo>::make("id", id_) &&
										mantra::Comparison<mantra::C_EqualTo>::make("level", id)) == 1);

	MT_RET(rv);
	MT_EE
}

void StoredChannel::LEVEL_Del(boost::uint32_t id)
{
	MT_EB
	MT_FUNC("StoredChannel::LEVEL_Del" << id);

	Level::storage.RemoveRow(mantra::Comparison<mantra::C_EqualTo>::make("id", id_) &&
							 mantra::Comparison<mantra::C_EqualTo>::make("level", id));

	MT_EE
}

StoredChannel::Level StoredChannel::LEVEL_Get(boost::uint32_t id) const
{
	MT_EB
	MT_FUNC("StoredChannel::LEVEL_Get" << id);

	Level rv(self.lock(), id);

	MT_RET(rv);
	MT_EE
}

void StoredChannel::LEVEL_Get(std::set<StoredChannel::Level> &fill) const
{
	MT_EB
	MT_FUNC("StoredChannel::LEVEL_Get" << fill);

	boost::read_write_mutex::scoped_read_lock scoped_lock(Level::DefaultLevelsLock);
	std::map<boost::uint32_t, Level::DefaultLevel>::iterator i;
	for (i = Level::DefaultLevels.begin(); i != Level::DefaultLevels.end(); ++i)
		fill.insert(Level(self.lock(), i->first));

	MT_EE
}

StoredChannel::Access::Access(const boost::shared_ptr<StoredChannel> &owner,
							  boost::uint32_t num)
	: cache(storage, ROOT->ConfigValue<mantra::duration>("general.cache-expire"),
			mantra::Comparison<mantra::C_EqualTo>::make("id", owner->ID()) &&
			mantra::Comparison<mantra::C_EqualTo>::make("number", num)),
	  owner_(owner), number_(num)
{
	MT_EB
	MT_FUNC("StoredChannel::Access::Access" << owner << num);


	MT_EE
}

StoredChannel::Access::Access(const boost::shared_ptr<StoredChannel> &owner,
							  const std::string &entry, boost::int32_t level,
							  const boost::shared_ptr<StoredNick> &updater)
	: cache(storage, ROOT->ConfigValue<mantra::duration>("general.cache-expire")),
	  owner_(owner)
{
	MT_EB
	MT_FUNC("StoredChannel::Access::Access" << owner << entry << level << updater);

	{
		boost::mutex::scoped_lock sl(owner->id_lock_access);
		mantra::StorageValue v = storage.Maximum("number",
			 mantra::Comparison<mantra::C_EqualTo>::make("id", owner->ID()));
		if (v.type() == typeid(mantra::NullValue))
			number_ = 1;
		else
			number_ = boost::get<boost::uint32_t>(v) + 1;

		mantra::Storage::RecordMap rec;
		rec["id"] = owner->ID();
		rec["number"] = number_;
		rec["entry_mask"] = entry;
		rec["level"] = level;
		rec["last_updater"] = updater->Name();
		rec["last_updater_id"] = updater->User()->ID();
		rec["last_update"] = mantra::GetCurrentDateTime();

		if (storage.InsertRow(rec))
			cache.Search(mantra::Comparison<mantra::C_EqualTo>::make("id", owner->ID()) &&
						 mantra::Comparison<mantra::C_EqualTo>::make("number", number_));
		else
			number_ = 0;
	}

	MT_EE
}

StoredChannel::Access::Access(const boost::shared_ptr<StoredChannel> &owner,
							  const boost::shared_ptr<StoredUser> &entry,
							  boost::int32_t level,
							  const boost::shared_ptr<StoredNick> &updater)
	: cache(storage, ROOT->ConfigValue<mantra::duration>("general.cache-expire")),
	  owner_(owner)
{
	MT_EB
	MT_FUNC("StoredChannel::Access::Access" << owner << entry << level << updater);

	{
		boost::mutex::scoped_lock sl(owner->id_lock_access);
		mantra::StorageValue v = storage.Maximum("number",
			 mantra::Comparison<mantra::C_EqualTo>::make("id", owner->ID()));
		if (v.type() == typeid(mantra::NullValue))
			number_ = 1;
		else
			number_ = boost::get<boost::uint32_t>(v) + 1;

		mantra::Storage::RecordMap rec;
		rec["id"] = owner->ID();
		rec["number"] = number_;
		rec["entry_user"] = entry->ID();
		rec["level"] = level;
		rec["last_updater"] = updater->Name();
		rec["last_updater_id"] = updater->User()->ID();
		rec["last_update"] = mantra::GetCurrentDateTime();

		if (storage.InsertRow(rec))
			cache.Search(mantra::Comparison<mantra::C_EqualTo>::make("id", owner->ID()) &&
						 mantra::Comparison<mantra::C_EqualTo>::make("number", number_));
		else
			number_ = 0;
	}

	MT_EE
}

StoredChannel::Access::Access(const boost::shared_ptr<StoredChannel> &owner,
							  const boost::shared_ptr<Committee> &entry,
							  boost::int32_t level,
							  const boost::shared_ptr<StoredNick> &updater)
	: cache(storage, ROOT->ConfigValue<mantra::duration>("general.cache-expire")),
	  owner_(owner)
{
	MT_EB
	MT_FUNC("StoredChannel::Access::Access" << owner << entry << level << updater);

	{
		boost::mutex::scoped_lock sl(owner->id_lock_access);
		mantra::StorageValue v = storage.Maximum("number",
			 mantra::Comparison<mantra::C_EqualTo>::make("id", owner->ID()));
		if (v.type() == typeid(mantra::NullValue))
			number_ = 1;
		else
			number_ = boost::get<boost::uint32_t>(v) + 1;

		mantra::Storage::RecordMap rec;
		rec["id"] = owner->ID();
		rec["number"] = number_;
		rec["entry_committee"] = entry->ID();
		rec["level"] = level;
		rec["last_updater"] = updater->Name();
		rec["last_updater_id"] = updater->User()->ID();
		rec["last_update"] = mantra::GetCurrentDateTime();

		if (storage.InsertRow(rec))
			cache.Search(mantra::Comparison<mantra::C_EqualTo>::make("id", owner->ID()) &&
						 mantra::Comparison<mantra::C_EqualTo>::make("number", number_));
		else
			number_ = 0;
	}

	MT_EE
}

void StoredChannel::Access::ChangeEntry(const std::string &entry,
										const boost::shared_ptr<StoredNick> &updater)
{
	MT_EB
	MT_FUNC("StoredChannel::Access::ChangeEntry" << entry << updater);

	mantra::Storage::RecordMap rec;
	rec["entry_mask"] = entry;
	rec["entry_user"] = mantra::NullValue();
	rec["entry_committee"] = mantra::NullValue();
	rec["last_updater"] = updater->Name();
	rec["last_updater_id"] = updater->User()->ID();
	rec["last_update"] = mantra::GetCurrentDateTime();

	cache.Put(rec);

	MT_EE
}

void StoredChannel::Access::ChangeEntry(const boost::shared_ptr<StoredUser> &entry,
										const boost::shared_ptr<StoredNick> &updater)
{
	MT_EB
	MT_FUNC("StoredChannel::Access::ChangeEntry" << entry << updater);

	mantra::Storage::RecordMap rec;
	rec["entry_mask"] = mantra::NullValue();
	rec["entry_user"] = entry->ID();
	rec["entry_committee"] = mantra::NullValue();
	rec["last_updater"] = updater->Name();
	rec["last_updater_id"] = updater->User()->ID();
	rec["last_update"] = mantra::GetCurrentDateTime();

	cache.Put(rec);

	MT_EE
}

void StoredChannel::Access::ChangeEntry(const boost::shared_ptr<Committee> &entry,
										const boost::shared_ptr<StoredNick> &updater)
{
	MT_EB
	MT_FUNC("StoredChannel::Access::ChangeEntry" << entry << updater);

	mantra::Storage::RecordMap rec;
	rec["entry_mask"] = mantra::NullValue();
	rec["entry_user"] = mantra::NullValue();
	rec["entry_committee"] = entry->ID();
	rec["last_updater"] = updater->Name();
	rec["last_updater_id"] = updater->User()->ID();
	rec["last_update"] = mantra::GetCurrentDateTime();

	cache.Put(rec);

	MT_EE
}

void StoredChannel::Access::ChangeValue(boost::int32_t value,
										const boost::shared_ptr<StoredNick> &updater)
{
	MT_EB
	MT_FUNC("StoredChannel::Access::ChangeValue" << value << updater);

	mantra::Storage::RecordMap rec;
	rec["level"] = value;
	rec["last_updater"] = updater->Name();
	rec["last_updater_id"] = updater->User()->ID();
	rec["last_update"] = mantra::GetCurrentDateTime();

	cache.Put(rec);

	MT_EE
}

std::string StoredChannel::Access::Mask() const
{
	MT_EB
	MT_FUNC("StoredChannel::Access::Mask");

	mantra::StorageValue rv = cache.Get("entry_mask");

	std::string ret;
	if (rv.type() != typeid(mantra::NullValue))
		ret = boost::get<std::string>(rv);

	MT_RET(ret);
	MT_EE
}

boost::shared_ptr<StoredUser> StoredChannel::Access::User() const
{
	MT_EB
	MT_FUNC("StoredChannel::Access::User");

	mantra::StorageValue rv = cache.Get("entry_user");

	boost::shared_ptr<StoredUser> ret;
	if (rv.type() != typeid(mantra::NullValue))
		ret = ROOT->data.Get_StoredUser(boost::get<boost::uint32_t>(rv));

	MT_RET(ret);
	MT_EE
}

boost::shared_ptr<Committee> StoredChannel::Access::GetCommittee() const
{
	MT_EB
	MT_FUNC("StoredChannel::Access::GetCommittee");

	mantra::StorageValue rv = cache.Get("entry_committee");

	boost::shared_ptr<Committee> ret;
	if (rv.type() != typeid(mantra::NullValue))
		ret = ROOT->data.Get_Committee(boost::get<boost::uint32_t>(rv));

	MT_RET(ret);
	MT_EE
}

boost::int32_t StoredChannel::Access::Level() const
{
	MT_EB
	MT_FUNC("StoredChannel::Access::Level");

	mantra::StorageValue rv = cache.Get("level");

	boost::int32_t ret = 0;
	if (rv.type() != typeid(mantra::NullValue))
		ret = boost::get<boost::int32_t>(rv);

	MT_RET(ret);
	MT_EE
}

std::string StoredChannel::Access::Last_UpdaterName() const
{
	MT_EB
	MT_FUNC("StoredChannel::Access::Last_UpdaterName");

	mantra::StorageValue rv = cache.Get("last_updater");

	std::string ret;
	if (rv.type() != typeid(mantra::NullValue))
		ret = boost::get<std::string>(rv);

	MT_RET(ret);
	MT_EE
}

boost::shared_ptr<StoredUser> StoredChannel::Access::Last_Updater() const
{
	MT_EB
	MT_FUNC("StoredChannel::Access::Last_Updater");

	mantra::StorageValue rv = cache.Get("last_updater_id");

	boost::shared_ptr<StoredUser> ret;
	if (rv.type() != typeid(mantra::NullValue))
		ret = ROOT->data.Get_StoredUser(boost::get<boost::uint32_t>(rv));

	MT_RET(ret);
	MT_EE
}

boost::posix_time::ptime StoredChannel::Access::Last_Update() const
{
	MT_EB
	MT_FUNC("StoredChannel::Access::Last_Update");

	mantra::StorageValue rv = cache.Get("last_update");

	boost::posix_time::ptime ret;
	if (rv.type() != typeid(mantra::NullValue))
		ret = boost::get<boost::posix_time::ptime>(rv);

	MT_RET(ret);
	MT_EE
}

static inline bool level_greater(const StoredChannel::Access &lhs,
					   const StoredChannel::Access &rhs)
{
	MT_EB
	MT_FUNC("level_less" << lhs << rhs);

	bool rv = (lhs.Level() > rhs.Level());

	MT_RET(rv);
	MT_EE
}

std::list<StoredChannel::Access> StoredChannel::ACCESS_Matches(const std::string &in) const
{
	MT_EB
	MT_FUNC("StoredChannel::ACCESS_Matches" << in);

	std::list<Access> rv;

	mantra::Storage::DataSet data;
	mantra::Storage::FieldSet fields;
	fields.insert("number");
	fields.insert("entry_mask");

	Access::storage.RetrieveRow(data,
		mantra::Comparison<mantra::C_EqualTo>::make("entry_mask", mantra::NullValue::instance(), true) &&
		mantra::Comparison<mantra::C_EqualTo>::make("id", ID()), fields);

	if (data.empty())
		MT_RET(rv);

	mantra::Storage::DataSet::iterator i;
	for (i = data.begin(); i != data.end(); ++i)
	{
		if (mantra::glob_match(boost::get<std::string>((*i)["entry_mask"]), in, true))
			rv.push_back(Access(self.lock(), boost::get<boost::uint32_t>((*i)["number"])));
	}

	// Highest access first.
	rv.sort(&level_greater);

	MT_RET(rv);
	MT_EE
}

std::list<StoredChannel::Access> StoredChannel::ACCESS_Matches(const boost::shared_ptr<LiveUser> &in) const
{
	MT_EB
	MT_FUNC("StoredChannel::ACCESS_Matches" << in);


	boost::shared_ptr<StoredNick> nick = in->Stored();
	if (!nick || (Secure() && !in->Identified()))
	{
		std::list<Access> rv(ACCESS_Matches(in->Name() + '!' + in->User() + '!' + in->Host()));
		MT_RET(rv);
	}

	mantra::Storage::DataSet data;
	mantra::Storage::FieldSet fields;
	fields.insert("number");
	fields.insert("entry_mask");
	fields.insert("entry_user");
	fields.insert("entry_committee");

	Access::storage.RetrieveRow(data,
		mantra::Comparison<mantra::C_EqualTo>::make("id", ID()), fields);

	std::list<Access> rv;
	if (data.empty())
		MT_RET(rv);

	mantra::Storage::DataSet::iterator i;
	for (i = data.begin(); i != data.end(); ++i)
	{
		mantra::Storage::RecordMap::iterator j = i->find("entry_user");
		if (j != i->end() && j->second.type() != typeid(mantra::NullValue))
		{
			if (nick->User()->ID() == boost::get<boost::uint32_t>(j->second))
				rv.push_back(Access(self.lock(), boost::get<boost::uint32_t>((*i)["number"])));
			continue;
		}

		j = i->find("entry_committee");
		if (j != i->end() && j->second.type() != typeid(mantra::NullValue))
		{
			if (in->InCommittee(ROOT->data.Get_Committee(boost::get<boost::uint32_t>(j->second))))
				rv.push_back(Access(self.lock(), boost::get<boost::uint32_t>((*i)["number"])));
			continue;
		}

		j = i->find("entry_mask");
		if (j != i->end() && j->second.type() != typeid(mantra::NullValue))
		{
			if (mantra::glob_match(boost::get<std::string>(j->second),
								   in->Name() + '!' + in->User() + '!' + in->Host(), true))
				rv.push_back(Access(self.lock(), boost::get<boost::uint32_t>((*i)["number"])));
		}
	}

	// Highest access first.
	rv.sort(&level_greater);

	MT_RET(rv);
	MT_EE
}

std::list<StoredChannel::Access> StoredChannel::ACCESS_Matches(const boost::regex &in) const
{
	MT_EB
	MT_FUNC("StoredChannel::ACCESS_Matches" << in);

	std::list<Access> rv;

	mantra::Storage::DataSet data;
	mantra::Storage::FieldSet fields;
	fields.insert("number");
	fields.insert("entry_mask");

	Access::storage.RetrieveRow(data,
		mantra::Comparison<mantra::C_EqualTo>::make("entry_mask", mantra::NullValue::instance(), true) &&
		mantra::Comparison<mantra::C_EqualTo>::make("id", ID()), fields);

	if (data.empty())
		MT_RET(rv);

	mantra::Storage::DataSet::iterator i;
	for (i = data.begin(); i != data.end(); ++i)
	{
		if (boost::regex_match(boost::get<std::string>((*i)["entry_mask"]), in))
			rv.push_back(Access(self.lock(), boost::get<boost::uint32_t>((*i)["number"])));
	}

	// Highest access first.
	rv.sort(&level_greater);

	MT_RET(rv);
	MT_EE
}

boost::int32_t StoredChannel::ACCESS_Max(const std::string &in) const
{
	MT_EB
	MT_FUNC("StoredChannel::ACCESS_Max" << in);

	std::list<Access> acc(ACCESS_Matches(in));
	if (acc.empty())
		MT_RET(0);

	boost::int32_t rv = acc.front().Level();

	MT_RET(rv);
	MT_EE
}

boost::int32_t StoredChannel::ACCESS_Max(const boost::shared_ptr<LiveUser> &in) const
{
	MT_EB
	MT_FUNC("StoredChannel::ACCESS_Max" << in);

	boost::int32_t rv = 0;
	if (in->Identified(self.lock()))
		rv = ROOT->ConfigValue<boost::int32_t>("chanserv.max-level") + 1;
	else
	{
		boost::shared_ptr<StoredNick> nick = in->Stored();
		if (nick && nick->User() == Founder() &&
			(in->Identified() || (!Secure() && !nick->User()->Secure())))
		{
			rv = ROOT->ConfigValue<boost::int32_t>("chanserv.max-level") + 1;
		}
		else
		{
			std::list<Access> acc(ACCESS_Matches(in));
			if (!acc.empty())
				rv = acc.front().Level();
		}
	}

	MT_RET(rv);
	MT_EE
}

bool StoredChannel::ACCESS_Matches(const std::string &in, boost::uint32_t level) const
{
	MT_EB
	MT_FUNC("StoredChannel::ACCESS_Matches" << in << level);

	Level lvl(LEVEL_Get(level));
	if (!lvl.ID())
		MT_RET(false);

	if (lvl.Value() == 0)
		MT_RET(true);

	if (lvl.Value() > ROOT->ConfigValue<boost::int32_t>("chanserv.max-level") + 1)
		MT_RET(false);

	bool rv;
	if (lvl.Value() > 0)
		rv = (ACCESS_Max(in) >= lvl.Value());
	else
		rv = (ACCESS_Max(in) <= lvl.Value());

	MT_RET(rv);
	MT_EE
}

bool StoredChannel::ACCESS_Matches(const boost::shared_ptr<LiveUser> &in, boost::uint32_t level) const
{
	MT_EB
	MT_FUNC("StoredChannel::ACCESS_Matches" << in << level);

	Level lvl(LEVEL_Get(level));
	if (!lvl.ID())
		MT_RET(false);

	if (lvl.Value() == 0)
		MT_RET(true);

	if (lvl.Value() > ROOT->ConfigValue<boost::int32_t>("chanserv.max-level") + 1)
		MT_RET(false);

	bool rv;
	if (lvl.Value() > 0)
		rv = (ACCESS_Max(in) >= lvl.Value());
	else
		rv = (ACCESS_Max(in) <= lvl.Value());

	MT_RET(rv);
	MT_EE
}

bool StoredChannel::ACCESS_Exists(boost::uint32_t num) const
{
	MT_EB
	MT_FUNC("StoredChannel::ACCESS_Exists" << num);

	bool rv = (Access::storage.RowExists(
			mantra::Comparison<mantra::C_EqualTo>::make("id", id_) &&
			mantra::Comparison<mantra::C_EqualTo>::make("number", num)) == 1u);

	MT_RET(rv);
	MT_EE
}

StoredChannel::Access StoredChannel::ACCESS_Add(const std::string &entry, boost::uint32_t value,
												const boost::shared_ptr<StoredNick> &updater)
{
	MT_EB
	MT_FUNC("ACCESS_Add" << entry << value << updater);

	Access rv(self.lock(), entry, value, updater);

	MT_RET(rv);
	MT_EE
}

StoredChannel::Access StoredChannel::ACCESS_Add(const boost::shared_ptr<StoredUser> &entry,
												boost::uint32_t value,
												const boost::shared_ptr<StoredNick> &updater)
{
	MT_EB
	MT_FUNC("ACCESS_Add" << entry << value << updater);

	Access rv(self.lock(), entry, value, updater);

	MT_RET(rv);
	MT_EE
}

StoredChannel::Access StoredChannel::ACCESS_Add(const boost::shared_ptr<Committee> &entry,
												boost::uint32_t value,
												const boost::shared_ptr<StoredNick> &updater)
{
	MT_EB
	MT_FUNC("ACCESS_Add" << entry << value << updater);

	Access rv(self.lock(), entry, value, updater);

	MT_RET(rv);
	MT_EE
}

void StoredChannel::ACCESS_Del(boost::uint32_t num)
{
	MT_EB
	MT_FUNC("StoredChannel::ACCESS_Del" << num);

	if (!ACCESS_Exists(num))
		return;

	Access::storage.RemoveRow(
			mantra::Comparison<mantra::C_EqualTo>::make("id", id_) &&
			mantra::Comparison<mantra::C_EqualTo>::make("number", num));

	MT_EE
}

void StoredChannel::ACCESS_Reindex(boost::uint32_t num, boost::uint32_t newnum)
{
	MT_EB
	MT_FUNC("StoredChannel::ACCESS_Reindex" << num);

	mantra::Storage::RecordMap data;
	data["number"] = newnum;

	Access::storage.ChangeRow(data,
			mantra::Comparison<mantra::C_EqualTo>::make("id", id_) &&
			mantra::Comparison<mantra::C_EqualTo>::make("number", num));

	MT_EE
}

StoredChannel::Access StoredChannel::ACCESS_Get(boost::uint32_t num) const
{
	MT_EB
	MT_FUNC("StoredChannel::ACCESS_Exists" << num);

	Access rv(self.lock(), num);

	MT_RET(rv);
	MT_EE
}

StoredChannel::Access StoredChannel::ACCESS_Get(const boost::shared_ptr<StoredUser> &in) const
{
	MT_EB
	MT_FUNC("StoredChannel::ACCESS_Get" << in);

	mantra::Storage::DataSet data;
	mantra::Storage::FieldSet fields;
	fields.insert("number");

	Access::storage.RetrieveRow(data,
		mantra::Comparison<mantra::C_EqualTo>::make("entry_user", in->ID()) &&
		mantra::Comparison<mantra::C_EqualTo>::make("id", ID()), fields);

	if (data.size() != 1u)
	{
		Access rv(self.lock(), 0);
		MT_RET(rv);
	}

	Access rv(self.lock(), boost::get<boost::uint32_t>(data.front()["number"]));
	MT_RET(rv);
	MT_EE
}

StoredChannel::Access StoredChannel::ACCESS_Get(const boost::shared_ptr<Committee> &in) const
{
	MT_EB
	MT_FUNC("StoredChannel::ACCESS_Get" << in);

	mantra::Storage::DataSet data;
	mantra::Storage::FieldSet fields;
	fields.insert("number");

	Access::storage.RetrieveRow(data,
		mantra::Comparison<mantra::C_EqualTo>::make("entry_committee", in->ID()) &&
		mantra::Comparison<mantra::C_EqualTo>::make("id", ID()), fields);

	if (data.size() != 1u)
	{
		Access rv(self.lock(), 0);
		MT_RET(rv);
	}

	Access rv(self.lock(), boost::get<boost::uint32_t>(data.front()["number"]));
	MT_RET(rv);
	MT_EE
}

StoredChannel::Access StoredChannel::ACCESS_Get(const std::string &in) const
{
	MT_EB
	MT_FUNC("StoredChannel::ACCESS_Get" << in);

	mantra::Storage::DataSet data;
	mantra::Storage::FieldSet fields;
	fields.insert("number");

	Access::storage.RetrieveRow(data,
		mantra::Comparison<mantra::C_EqualToNC>::make("entry_mask", in) &&
		mantra::Comparison<mantra::C_EqualTo>::make("id", ID()), fields);

	if (data.size() != 1u)
	{
		Access rv(self.lock(), 0);
		MT_RET(rv);
	}

	Access rv(self.lock(), boost::get<boost::uint32_t>(data.front()["number"]));
	MT_RET(rv);
	MT_EE
}

void StoredChannel::ACCESS_Get(std::set<Access> &fill) const
{
	MT_EB
	MT_FUNC("StoredChannel::ACCESS_Get" << fill);

	mantra::Storage::DataSet data;
	mantra::Storage::FieldSet fields;
	fields.insert("number");

	Access::storage.RetrieveRow(data,
			mantra::Comparison<mantra::C_EqualTo>::make("id", id_), fields);
	if (data.empty())
		return;

	mantra::Storage::DataSet::iterator i;
	for (i = data.begin(); i != data.end(); ++i)
		fill.insert(Access(self.lock(), boost::get<boost::uint32_t>((*i)["number"])));

	MT_EE
}

StoredChannel::AutoKick::AutoKick(const boost::shared_ptr<StoredChannel> &owner,
							  boost::uint32_t num)
	: cache(storage, ROOT->ConfigValue<mantra::duration>("general.cache-expire"),
			mantra::Comparison<mantra::C_EqualTo>::make("id", owner->ID()) &&
			mantra::Comparison<mantra::C_EqualTo>::make("number", num)),
	  owner_(owner), number_(num)
{
	MT_EB
	MT_FUNC("StoredChannel::AutoKick::AutoKick" << owner << num);


	MT_EE
}

StoredChannel::AutoKick::AutoKick(const boost::shared_ptr<StoredChannel> &owner,
							  const std::string &entry, const std::string &reason,
							  const mantra::duration &length,
							  const boost::shared_ptr<StoredNick> &updater)
	: cache(storage, ROOT->ConfigValue<mantra::duration>("general.cache-expire")),
	  owner_(owner)
{
	MT_EB
	MT_FUNC("StoredChannel::AutoKick::AutoKick" << owner << entry << reason << length << updater);

	{
		boost::mutex::scoped_lock sl(owner->id_lock_access);
		mantra::StorageValue v = storage.Maximum("number",
			 mantra::Comparison<mantra::C_EqualTo>::make("id", owner->ID()));
		if (v.type() == typeid(mantra::NullValue))
			number_ = 1;
		else
			number_ = boost::get<boost::uint32_t>(v) + 1;

		mantra::Storage::RecordMap rec;
		rec["id"] = owner->ID();
		rec["number"] = number_;
		rec["entry_mask"] = entry;
		rec["reason"] = reason;
		rec["creation"] = mantra::GetCurrentDateTime();
		if (length)
			rec["length"] = length;
		rec["last_updater"] = updater->Name();
		rec["last_updater_id"] = updater->User()->ID();
		rec["last_update"] = mantra::GetCurrentDateTime();

		if (storage.InsertRow(rec))
			cache.Search(mantra::Comparison<mantra::C_EqualTo>::make("id", owner->ID()) &&
						 mantra::Comparison<mantra::C_EqualTo>::make("number", number_));
		else
			number_ = 0;
	}

	MT_EE
}

StoredChannel::AutoKick::AutoKick(const boost::shared_ptr<StoredChannel> &owner,
							  const boost::shared_ptr<StoredUser> &entry,
							  const std::string &reason, const mantra::duration &length,
							  const boost::shared_ptr<StoredNick> &updater)
	: cache(storage, ROOT->ConfigValue<mantra::duration>("general.cache-expire")),
	  owner_(owner)
{
	MT_EB
	MT_FUNC("StoredChannel::AutoKick::AutoKick" << owner << entry << reason << length << updater);

	{
		boost::mutex::scoped_lock sl(owner->id_lock_access);
		mantra::StorageValue v = storage.Maximum("number",
			 mantra::Comparison<mantra::C_EqualTo>::make("id", owner->ID()));
		if (v.type() == typeid(mantra::NullValue))
			number_ = 1;
		else
			number_ = boost::get<boost::uint32_t>(v) + 1;

		mantra::Storage::RecordMap rec;
		rec["id"] = owner->ID();
		rec["number"] = number_;
		rec["entry_user"] = entry->ID();
		rec["reason"] = reason;
		rec["creation"] = mantra::GetCurrentDateTime();
		if (length)
			rec["length"] = length;
		rec["last_updater"] = updater->Name();
		rec["last_updater_id"] = updater->User()->ID();
		rec["last_update"] = mantra::GetCurrentDateTime();

		if (storage.InsertRow(rec))
			cache.Search(mantra::Comparison<mantra::C_EqualTo>::make("id", owner->ID()) &&
						 mantra::Comparison<mantra::C_EqualTo>::make("number", number_));
		else
			number_ = 0;
	}

	MT_EE
}

StoredChannel::AutoKick::AutoKick(const boost::shared_ptr<StoredChannel> &owner,
							  const boost::shared_ptr<Committee> &entry,
							  const std::string &reason, const mantra::duration &length,
							  const boost::shared_ptr<StoredNick> &updater)
	: cache(storage, ROOT->ConfigValue<mantra::duration>("general.cache-expire")),
	  owner_(owner)
{
	MT_EB
	MT_FUNC("StoredChannel::AutoKick::AutoKick" << owner << entry << reason << length << updater);

	{
		boost::mutex::scoped_lock sl(owner->id_lock_access);
		mantra::StorageValue v = storage.Maximum("number",
			 mantra::Comparison<mantra::C_EqualTo>::make("id", owner->ID()));
		if (v.type() == typeid(mantra::NullValue))
			number_ = 1;
		else
			number_ = boost::get<boost::uint32_t>(v) + 1;

		mantra::Storage::RecordMap rec;
		rec["id"] = owner->ID();
		rec["number"] = number_;
		rec["entry_committee"] = entry->ID();
		rec["reason"] = reason;
		rec["creation"] = mantra::GetCurrentDateTime();
		if (length)
			rec["length"] = length;
		rec["last_updater"] = updater->Name();
		rec["last_updater_id"] = updater->User()->ID();
		rec["last_update"] = mantra::GetCurrentDateTime();

		if (storage.InsertRow(rec))
			cache.Search(mantra::Comparison<mantra::C_EqualTo>::make("id", owner->ID()) &&
						 mantra::Comparison<mantra::C_EqualTo>::make("number", number_));
		else
			number_ = 0;
	}

	MT_EE
}

void StoredChannel::AutoKick::ChangeEntry(const std::string &entry,
										const boost::shared_ptr<StoredNick> &updater)
{
	MT_EB
	MT_FUNC("StoredChannel::AutoKick::ChangeEntry" << entry << updater);

	mantra::Storage::RecordMap rec;
	rec["entry_mask"] = entry;
	rec["entry_user"] = mantra::NullValue();
	rec["entry_committee"] = mantra::NullValue();
	rec["last_updater"] = updater->Name();
	rec["last_updater_id"] = updater->User()->ID();
	rec["last_update"] = mantra::GetCurrentDateTime();

	cache.Put(rec);

	MT_EE
}

void StoredChannel::AutoKick::ChangeEntry(const boost::shared_ptr<StoredUser> &entry,
										const boost::shared_ptr<StoredNick> &updater)
{
	MT_EB
	MT_FUNC("StoredChannel::AutoKick::ChangeEntry" << entry << updater);

	mantra::Storage::RecordMap rec;
	rec["entry_mask"] = mantra::NullValue();
	rec["entry_user"] = entry->ID();
	rec["entry_committee"] = mantra::NullValue();
	rec["last_updater"] = updater->Name();
	rec["last_updater_id"] = updater->User()->ID();
	rec["last_update"] = mantra::GetCurrentDateTime();

	cache.Put(rec);

	MT_EE
}

void StoredChannel::AutoKick::ChangeEntry(const boost::shared_ptr<Committee> &entry,
										const boost::shared_ptr<StoredNick> &updater)
{
	MT_EB
	MT_FUNC("StoredChannel::AutoKick::ChangeEntry" << entry << updater);

	mantra::Storage::RecordMap rec;
	rec["entry_mask"] = mantra::NullValue();
	rec["entry_user"] = mantra::NullValue();
	rec["entry_committee"] = entry->ID();
	rec["last_updater"] = updater->Name();
	rec["last_updater_id"] = updater->User()->ID();
	rec["last_update"] = mantra::GetCurrentDateTime();

	cache.Put(rec);

	MT_EE
}

void StoredChannel::AutoKick::ChangeReason(const std::string &reason,
										   const boost::shared_ptr<StoredNick> &updater)
{
	MT_EB
	MT_FUNC("StoredChannel::AutoKick::ChangeValue" << reason << updater);

	mantra::Storage::RecordMap rec;
	rec["reason"] = reason;
	rec["last_updater"] = updater->Name();
	rec["last_updater_id"] = updater->User()->ID();
	rec["last_update"] = mantra::GetCurrentDateTime();

	cache.Put(rec);

	MT_EE
}

void StoredChannel::AutoKick::ChangeLength(const mantra::duration &length,
										   const boost::shared_ptr<StoredNick> &updater)
{
	MT_EB
	MT_FUNC("StoredChannel::AutoKick::ChangeValue" << length << updater);

	mantra::Storage::RecordMap rec;
	if (length)
		rec["length"] = length;
	else
		rec["length"] = mantra::NullValue();
	rec["last_updater"] = updater->Name();
	rec["last_updater_id"] = updater->User()->ID();
	rec["last_update"] = mantra::GetCurrentDateTime();

	cache.Put(rec);

	MT_EE
}

std::string StoredChannel::AutoKick::Mask() const
{
	MT_EB
	MT_FUNC("StoredChannel::AutoKick::Mask");

	mantra::StorageValue rv = cache.Get("entry_mask");

	std::string ret;
	if (rv.type() != typeid(mantra::NullValue))
		ret = boost::get<std::string>(rv);

	MT_RET(ret);
	MT_EE
}

boost::shared_ptr<StoredUser> StoredChannel::AutoKick::User() const
{
	MT_EB
	MT_FUNC("StoredChannel::AutoKick::User");

	mantra::StorageValue rv = cache.Get("entry_user");

	boost::shared_ptr<StoredUser> ret;
	if (rv.type() != typeid(mantra::NullValue))
		ret = ROOT->data.Get_StoredUser(boost::get<boost::uint32_t>(rv));

	MT_RET(ret);
	MT_EE
}

boost::shared_ptr<Committee> StoredChannel::AutoKick::GetCommittee() const
{
	MT_EB
	MT_FUNC("StoredChannel::AutoKick::GetCommittee");

	mantra::StorageValue rv = cache.Get("entry_committee");

	boost::shared_ptr<Committee> ret;
	if (rv.type() != typeid(mantra::NullValue))
		ret = ROOT->data.Get_Committee(boost::get<boost::uint32_t>(rv));

	MT_RET(ret);
	MT_EE
}

std::string StoredChannel::AutoKick::Reason() const
{
	MT_EB
	MT_FUNC("StoredChannel::AutoKick::Reason");

	mantra::StorageValue rv = cache.Get("reason");

	std::string ret;
	if (rv.type() != typeid(mantra::NullValue))
		ret = boost::get<std::string>(rv);

	MT_RET(ret);
	MT_EE
}

boost::posix_time::ptime StoredChannel::AutoKick::Creation() const
{
	MT_EB
	MT_FUNC("StoredChannel::AutoKick::Creation");

	mantra::StorageValue rv = cache.Get("creation");

	boost::posix_time::ptime ret;
	if (rv.type() != typeid(mantra::NullValue))
		ret = boost::get<boost::posix_time::ptime>(rv);

	MT_RET(ret);
	MT_EE
}

mantra::duration StoredChannel::AutoKick::Length() const
{
	MT_EB
	MT_FUNC("StoredChannel::AutoKick::Length");

	mantra::StorageValue rv = cache.Get("length");

	mantra::duration ret("");
	if (rv.type() != typeid(mantra::NullValue))
		ret = boost::get<mantra::duration>(rv);

	MT_RET(ret);
	MT_EE
}

std::string StoredChannel::AutoKick::Last_UpdaterName() const
{
	MT_EB
	MT_FUNC("StoredChannel::AutoKick::Last_UpdaterName");

	mantra::StorageValue rv = cache.Get("last_updater");

	std::string ret;
	if (rv.type() != typeid(mantra::NullValue))
		ret = boost::get<std::string>(rv);

	MT_RET(ret);
	MT_EE
}

boost::shared_ptr<StoredUser> StoredChannel::AutoKick::Last_Updater() const
{
	MT_EB
	MT_FUNC("StoredChannel::AutoKick::Last_Updater");

	mantra::StorageValue rv = cache.Get("last_updater_id");

	boost::shared_ptr<StoredUser> ret;
	if (rv.type() != typeid(mantra::NullValue))
		ret = ROOT->data.Get_StoredUser(boost::get<boost::uint32_t>(rv));

	MT_RET(ret);
	MT_EE
}

boost::posix_time::ptime StoredChannel::AutoKick::Last_Update() const
{
	MT_EB
	MT_FUNC("StoredChannel::AutoKick::Last_Update");

	mantra::StorageValue rv = cache.Get("last_update");

	boost::posix_time::ptime ret;
	if (rv.type() != typeid(mantra::NullValue))
		ret = boost::get<boost::posix_time::ptime>(rv);

	MT_RET(ret);
	MT_EE
}

std::set<StoredChannel::AutoKick> StoredChannel::AKICK_Matches(const std::string &in) const
{
	MT_EB
	MT_FUNC("StoredChannel::AKICK_Matches" << in);

	std::set<AutoKick> rv;

	mantra::Storage::DataSet data;
	mantra::Storage::FieldSet fields;
	fields.insert("number");
	fields.insert("entry_mask");

	AutoKick::storage.RetrieveRow(data,
		mantra::Comparison<mantra::C_EqualTo>::make("entry_mask", mantra::NullValue::instance(), true) &&
		mantra::Comparison<mantra::C_EqualTo>::make("id", ID()), fields);

	if (data.empty())
		MT_RET(rv);

	mantra::Storage::DataSet::iterator i;
	for (i = data.begin(); i != data.end(); ++i)
	{
		if (mantra::glob_match(boost::get<std::string>((*i)["entry_mask"]), in, true))
			rv.insert(AutoKick(self.lock(), boost::get<boost::uint32_t>((*i)["number"])));
	}

	MT_RET(rv);
	MT_EE
}

std::set<StoredChannel::AutoKick> StoredChannel::AKICK_Matches(const boost::shared_ptr<LiveUser> &in) const
{
	MT_EB
	MT_FUNC("StoredChannel::AKICK_Matches" << in);


	boost::shared_ptr<StoredNick> nick = in->Stored();
	if (!nick || (Secure() && !in->Identified()))
	{
		std::set<AutoKick> rv(AKICK_Matches(in->Name() + '!' + in->User() + '!' + in->Host()));
		MT_RET(rv);
	}

	mantra::Storage::DataSet data;
	mantra::Storage::FieldSet fields;
	fields.insert("number");
	fields.insert("entry_mask");
	fields.insert("entry_user");
	fields.insert("entry_committee");

	AutoKick::storage.RetrieveRow(data,
		mantra::Comparison<mantra::C_EqualTo>::make("id", ID()), fields);

	std::set<AutoKick> rv;
	if (data.empty())
		MT_RET(rv);

	mantra::Storage::DataSet::iterator i;
	for (i = data.begin(); i != data.end(); ++i)
	{
		mantra::Storage::RecordMap::iterator j = i->find("entry_user");
		if (j != i->end() && j->second.type() != typeid(mantra::NullValue))
		{
			if (nick->User()->ID() == boost::get<boost::uint32_t>(j->second))
				rv.insert(AutoKick(self.lock(), boost::get<boost::uint32_t>((*i)["number"])));
			continue;
		}

		j = i->find("entry_committee");
		if (j != i->end() && j->second.type() != typeid(mantra::NullValue))
		{
			if (in->InCommittee(ROOT->data.Get_Committee(boost::get<boost::uint32_t>(j->second))))
				rv.insert(AutoKick(self.lock(), boost::get<boost::uint32_t>((*i)["number"])));
			continue;
		}

		j = i->find("entry_mask");
		if (j != i->end() && j->second.type() != typeid(mantra::NullValue))
		{
			if (mantra::glob_match(boost::get<std::string>(j->second),
								   in->Name() + '!' + in->User() + '!' + in->Host(), true))
				rv.insert(AutoKick(self.lock(), boost::get<boost::uint32_t>((*i)["number"])));
		}
	}

	MT_RET(rv);
	MT_EE
}

std::set<StoredChannel::AutoKick> StoredChannel::AKICK_Matches(const boost::regex &in) const
{
	MT_EB
	MT_FUNC("StoredChannel::AKICK_Matches" << in);

	std::set<AutoKick> rv;

	mantra::Storage::DataSet data;
	mantra::Storage::FieldSet fields;
	fields.insert("number");
	fields.insert("entry_mask");

	AutoKick::storage.RetrieveRow(data,
		mantra::Comparison<mantra::C_EqualTo>::make("entry_mask", mantra::NullValue::instance(), true) &&
		mantra::Comparison<mantra::C_EqualTo>::make("id", ID()), fields);

	if (data.empty())
		MT_RET(rv);

	mantra::Storage::DataSet::iterator i;
	for (i = data.begin(); i != data.end(); ++i)
	{
		if (boost::regex_match(boost::get<std::string>((*i)["entry_mask"]), in))
			rv.insert(AutoKick(self.lock(), boost::get<boost::uint32_t>((*i)["number"])));
	}

	MT_RET(rv);
	MT_EE
}

bool StoredChannel::AKICK_Exists(boost::uint32_t num) const
{
	MT_EB
	MT_FUNC("StoredChannel::AKICK_Exists" << num);

	bool rv = (AutoKick::storage.RowExists(
			mantra::Comparison<mantra::C_EqualTo>::make("id", id_) &&
			mantra::Comparison<mantra::C_EqualTo>::make("number", num)) == 1u);

	MT_RET(rv);
	MT_EE
}

StoredChannel::AutoKick StoredChannel::AKICK_Add(const std::string &entry, const std::string &reason,
												 const mantra::duration &length,
												 const boost::shared_ptr<StoredNick> &updater)
{
	MT_EB
	MT_FUNC("AKICK_Add" << entry << reason << length << updater);

	AutoKick rv(self.lock(), entry, reason, length, updater);

	MT_RET(rv);
	MT_EE
}

StoredChannel::AutoKick StoredChannel::AKICK_Add(const boost::shared_ptr<StoredUser> &entry,
												 const std::string &reason, const mantra::duration &length,
												 const boost::shared_ptr<StoredNick> &updater)
{
	MT_EB
	MT_FUNC("AKICK_Add" << entry << reason << length << updater);

	AutoKick rv(self.lock(), entry, reason, length, updater);

	MT_RET(rv);
	MT_EE
}

StoredChannel::AutoKick StoredChannel::AKICK_Add(const boost::shared_ptr<Committee> &entry,
												 const std::string &reason, const mantra::duration &length,
												 const boost::shared_ptr<StoredNick> &updater)
{
	MT_EB
	MT_FUNC("AKICK_Add" << entry << reason << length << updater);

	AutoKick rv(self.lock(), entry, reason, length, updater);

	MT_RET(rv);
	MT_EE
}

void StoredChannel::AKICK_Del(boost::uint32_t num)
{
	MT_EB
	MT_FUNC("StoredChannel::AKICK_Del" << num);

	if (!AKICK_Exists(num))
		return;

	AutoKick::storage.RemoveRow(
			mantra::Comparison<mantra::C_EqualTo>::make("id", id_) &&
			mantra::Comparison<mantra::C_EqualTo>::make("number", num));

	MT_EE
}

void StoredChannel::AKICK_Reindex(boost::uint32_t num, boost::uint32_t newnum)
{
	MT_EB
	MT_FUNC("StoredChannel::AKICK_Reindex" << num);

	mantra::Storage::RecordMap data;
	data["number"] = newnum;

	AutoKick::storage.ChangeRow(data,
			mantra::Comparison<mantra::C_EqualTo>::make("id", id_) &&
			mantra::Comparison<mantra::C_EqualTo>::make("number", num));

	MT_EE
}

StoredChannel::AutoKick StoredChannel::AKICK_Get(boost::uint32_t num) const
{
	MT_EB
	MT_FUNC("StoredChannel::AKICK_Exists" << num);

	AutoKick rv(self.lock(), num);

	MT_RET(rv);
	MT_EE
}

StoredChannel::AutoKick StoredChannel::AKICK_Get(const boost::shared_ptr<StoredUser> &in) const
{
	MT_EB
	MT_FUNC("StoredChannel::AKICK_Get" << in);

	mantra::Storage::DataSet data;
	mantra::Storage::FieldSet fields;
	fields.insert("number");

	AutoKick::storage.RetrieveRow(data,
		mantra::Comparison<mantra::C_EqualTo>::make("entry_user", in->ID()) &&
		mantra::Comparison<mantra::C_EqualTo>::make("id", ID()), fields);

	if (data.size() != 1u)
	{
		AutoKick rv(self.lock(), 0);
		MT_RET(rv);
	}

	AutoKick rv(self.lock(), boost::get<boost::uint32_t>(data.front()["number"]));
	MT_RET(rv);
	MT_EE
}

StoredChannel::AutoKick StoredChannel::AKICK_Get(const boost::shared_ptr<Committee> &in) const
{
	MT_EB
	MT_FUNC("StoredChannel::AKICK_Get" << in);

	mantra::Storage::DataSet data;
	mantra::Storage::FieldSet fields;
	fields.insert("number");

	AutoKick::storage.RetrieveRow(data,
		mantra::Comparison<mantra::C_EqualTo>::make("entry_committee", in->ID()) &&
		mantra::Comparison<mantra::C_EqualTo>::make("id", ID()), fields);

	if (data.size() != 1u)
	{
		AutoKick rv(self.lock(), 0);
		MT_RET(rv);
	}

	AutoKick rv(self.lock(), boost::get<boost::uint32_t>(data.front()["number"]));
	MT_RET(rv);
	MT_EE
}

StoredChannel::AutoKick StoredChannel::AKICK_Get(const std::string &in) const
{
	MT_EB
	MT_FUNC("StoredChannel::AKICK_Get" << in);

	mantra::Storage::DataSet data;
	mantra::Storage::FieldSet fields;
	fields.insert("number");

	AutoKick::storage.RetrieveRow(data,
		mantra::Comparison<mantra::C_EqualToNC>::make("entry_mask", in) &&
		mantra::Comparison<mantra::C_EqualTo>::make("id", ID()), fields);

	if (data.size() != 1u)
	{
		AutoKick rv(self.lock(), 0);
		MT_RET(rv);
	}

	AutoKick rv(self.lock(), boost::get<boost::uint32_t>(data.front()["number"]));
	MT_RET(rv);
	MT_EE
}

void StoredChannel::AKICK_Get(std::set<AutoKick> &fill) const
{
	MT_EB
	MT_FUNC("StoredChannel::AKICK_Get" << fill);

	mantra::Storage::DataSet data;
	mantra::Storage::FieldSet fields;
	fields.insert("number");

	AutoKick::storage.RetrieveRow(data,
			mantra::Comparison<mantra::C_EqualTo>::make("id", id_), fields);
	if (data.empty())
		return;

	mantra::Storage::DataSet::iterator i;
	for (i = data.begin(); i != data.end(); ++i)
		fill.insert(AutoKick(self.lock(), boost::get<boost::uint32_t>((*i)["number"])));

	MT_EE
}

StoredChannel::Greet::Greet(const boost::shared_ptr<StoredChannel> &owner, const boost::shared_ptr<StoredUser> &entry)
	: cache(storage, ROOT->ConfigValue<mantra::duration>("general.cache-expire"),
			mantra::Comparison<mantra::C_EqualTo>::make("id", owner->ID()) &&
			mantra::Comparison<mantra::C_EqualTo>::make("entry", entry->ID())),
	  owner_(owner), entry_(entry)
{
	MT_EB
	MT_FUNC("StoredChannel::Greet::Greet" << owner << entry);


	MT_EE
}

StoredChannel::Greet::Greet(const boost::shared_ptr<StoredChannel> &owner, const boost::shared_ptr<StoredUser> &entry,
							const std::string &greeting, const boost::shared_ptr<StoredNick> &updater)
	: cache(storage, ROOT->ConfigValue<mantra::duration>("general.cache-expire"),
			mantra::Comparison<mantra::C_EqualTo>::make("id", owner->ID()) &&
			mantra::Comparison<mantra::C_EqualTo>::make("entry", entry->ID())),
	  owner_(owner), entry_(entry)
{
	MT_EB
	MT_FUNC("StoredChannel::Greet::Greet" << owner << entry << greeting << updater);

	mantra::Storage::RecordMap rec;
	rec["id"] = owner->ID();
	rec["entry"] = entry->ID();
	rec["greeting"] = greeting;
	rec["last_updater"] = updater->Name();
	rec["last_updater_id"] = updater->User()->ID();
	rec["last_update"] = mantra::GetCurrentDateTime();

	if (!storage.InsertRow(rec))
		Change(greeting, updater);

	MT_EE
}

void StoredChannel::Greet::Change(const std::string &greeting, const boost::shared_ptr<StoredNick> &updater)
{
	MT_EB
	MT_FUNC("StoredChannel::Greet::Change" << greeting << updater);

	mantra::Storage::RecordMap rec;
	rec["greeting"] = greeting;
	rec["last_updater"] = updater->Name();
	rec["last_updater_id"] = updater->User()->ID();
	rec["last_update"] = mantra::GetCurrentDateTime();
	cache.Put(rec);

	MT_EE
}

std::string StoredChannel::Greet::Greeting() const
{
	MT_EB
	MT_FUNC("StoredChannel::Greet::Greeting");

	mantra::StorageValue rv = cache.Get("greeting");

	std::string ret;
	if (rv.type() != typeid(mantra::NullValue))
		ret = boost::get<std::string>(rv);

	MT_RET(ret);
	MT_EE
}

void StoredChannel::Greet::Locked(bool in)
{
	MT_EB
	MT_FUNC("StoredChannel::Greet::Locked" << in);

	mantra::Storage::RecordMap rec;
	rec["locked"] = in;
	cache.Put(rec);

	MT_EE
}

bool StoredChannel::Greet::Locked() const
{
	MT_EB
	MT_FUNC("StoredChannel::Greet::Locked");

	mantra::StorageValue rv = cache.Get("locked");

	bool ret = false;
	if (rv.type() != typeid(mantra::NullValue))
		ret = boost::get<bool>(rv);

	MT_RET(ret);
	MT_EE
}

std::string StoredChannel::Greet::Last_UpdaterName() const
{
	MT_EB
	MT_FUNC("StoredChannel::Greet::Last_UpdaterName");

	mantra::StorageValue rv = cache.Get("last_updater");

	std::string ret;
	if (rv.type() != typeid(mantra::NullValue))
		ret = boost::get<std::string>(rv);

	MT_RET(ret);
	MT_EE
}

boost::shared_ptr<StoredUser> StoredChannel::Greet::Last_Updater() const
{
	MT_EB
	MT_FUNC("StoredChannel::Greet::Last_Updater");

	mantra::StorageValue rv = cache.Get("last_updater_id");

	boost::shared_ptr<StoredUser> ret;
	if (rv.type() != typeid(mantra::NullValue))
		ret = ROOT->data.Get_StoredUser(boost::get<boost::uint32_t>(rv));

	MT_RET(ret);
	MT_EE
}

boost::posix_time::ptime StoredChannel::Greet::Last_Update() const
{
	MT_EB
	MT_FUNC("StoredChannel::Greet::Last_Update");

	mantra::StorageValue rv = cache.Get("last_update");

	boost::posix_time::ptime ret;
	if (rv.type() != typeid(mantra::NullValue))
		ret = boost::get<boost::posix_time::ptime>(rv);

	MT_RET(ret);
	MT_EE
}

bool StoredChannel::GREET_Exists(const boost::shared_ptr<StoredUser> &entry) const
{
	MT_EB
	MT_FUNC("StoredChannel::GREET_Exists" << entry);

	bool rv = !GREET_Get(entry).Greeting().empty();

	MT_RET(rv);
	MT_EE
}

StoredChannel::Greet StoredChannel::GREET_Add(const boost::shared_ptr<StoredUser> &entry, const std::string &greeting,
				const boost::shared_ptr<StoredNick> &updater)
{
	MT_EB
	MT_FUNC("StoredChannel::GREET_Add" << entry << greeting << updater);

	Greet rv(self.lock(), entry, greeting, updater);

	MT_RET(rv);
	MT_EE
}

void StoredChannel::GREET_Del(const boost::shared_ptr<StoredUser> &entry)
{
	MT_EB
	MT_FUNC("StoredChannel::GREET_Del" << entry);

	if (!GREET_Exists(entry))
		return;

	Greet::storage.RemoveRow(
			mantra::Comparison<mantra::C_EqualTo>::make("id", id_) &&
			mantra::Comparison<mantra::C_EqualTo>::make("entry", entry->ID()));

	MT_EE
}

StoredChannel::Greet StoredChannel::GREET_Get(const boost::shared_ptr<StoredUser> &entry) const
{
	MT_EB
	MT_FUNC("StoredChannel::GREET_Get" << entry);

	Greet rv(self.lock(), entry);

	MT_RET(rv);
	MT_EE
}

void StoredChannel::GREET_Get(std::set<Greet> &fill) const
{
	MT_EB
	MT_FUNC("StoredChannel::GREET_Get" << fill);

	mantra::Storage::DataSet data;
	mantra::Storage::FieldSet fields;
	fields.insert("entry");

	Greet::storage.RetrieveRow(data,
			mantra::Comparison<mantra::C_EqualTo>::make("id", id_), fields);
	if (data.empty())
		return;

	mantra::Storage::DataSet::iterator i;
	for (i = data.begin(); i != data.end(); ++i)
	{
		boost::shared_ptr<StoredUser> user =
			ROOT->data.Get_StoredUser(boost::get<boost::uint32_t>((*i)["entry"]));
		fill.insert(Greet(self.lock(), user));
	}

	MT_EE
}

StoredChannel::Message::Message(const boost::shared_ptr<StoredChannel> &owner,
							  boost::uint32_t num)
	: cache(storage, ROOT->ConfigValue<mantra::duration>("general.cache-expire"),
			mantra::Comparison<mantra::C_EqualTo>::make("id", owner->ID()) &&
			mantra::Comparison<mantra::C_EqualTo>::make("number", num)),
	  owner_(owner), number_(num)
{
	MT_EB
	MT_FUNC("StoredChannel::Message::Message" << owner << num);


	MT_EE
}

StoredChannel::Message::Message(const boost::shared_ptr<StoredChannel> &owner,
								const std::string &message,
								const boost::shared_ptr<StoredNick> &updater)
	: cache(storage, ROOT->ConfigValue<mantra::duration>("general.cache-expire")),
	  owner_(owner)
{
	MT_EB
	MT_FUNC("StoredChannel::Message::Message" << owner << message << updater);

	{
		boost::mutex::scoped_lock sl(owner->id_lock_access);
		mantra::StorageValue v = storage.Maximum("number",
			 mantra::Comparison<mantra::C_EqualTo>::make("id", owner->ID()));
		if (v.type() == typeid(mantra::NullValue))
			number_ = 1;
		else
			number_ = boost::get<boost::uint32_t>(v) + 1;

		mantra::Storage::RecordMap rec;
		rec["id"] = owner->ID();
		rec["number"] = number_;
		rec["message"] = message;
		rec["last_updater"] = updater->Name();
		rec["last_updater_id"] = updater->User()->ID();
		rec["last_update"] = mantra::GetCurrentDateTime();

		if (storage.InsertRow(rec))
			cache.Search(mantra::Comparison<mantra::C_EqualTo>::make("id", owner->ID()) &&
						 mantra::Comparison<mantra::C_EqualTo>::make("number", number_));
		else
			number_ = 0;
	}

	MT_EE
}

void StoredChannel::Message::Change(const std::string &message,
									const boost::shared_ptr<StoredNick> &updater)
{
	MT_EB
	MT_FUNC("StoredChannel::Message::ChangeEntry" << message << updater);

	mantra::Storage::RecordMap rec;
	rec["message"] = message;
	rec["last_updater"] = updater->Name();
	rec["last_updater_id"] = updater->User()->ID();
	rec["last_update"] = mantra::GetCurrentDateTime();

	cache.Put(rec);

	MT_EE
}

std::string StoredChannel::Message::Entry() const
{
	MT_EB
	MT_FUNC("StoredChannel::Message::Entry");

	mantra::StorageValue rv = cache.Get("message");

	std::string ret;
	if (rv.type() != typeid(mantra::NullValue))
		ret = boost::get<std::string>(rv);

	MT_RET(ret);
	MT_EE
}

std::string StoredChannel::Message::Last_UpdaterName() const
{
	MT_EB
	MT_FUNC("StoredChannel::Message::Last_UpdaterName");

	mantra::StorageValue rv = cache.Get("last_updater");

	std::string ret;
	if (rv.type() != typeid(mantra::NullValue))
		ret = boost::get<std::string>(rv);

	MT_RET(ret);
	MT_EE
}

boost::shared_ptr<StoredUser> StoredChannel::Message::Last_Updater() const
{
	MT_EB
	MT_FUNC("StoredChannel::Message::Last_Updater");

	mantra::StorageValue rv = cache.Get("last_updater_id");

	boost::shared_ptr<StoredUser> ret;
	if (rv.type() != typeid(mantra::NullValue))
		ret = ROOT->data.Get_StoredUser(boost::get<boost::uint32_t>(rv));

	MT_RET(ret);
	MT_EE
}

boost::posix_time::ptime StoredChannel::Message::Last_Update() const
{
	MT_EB
	MT_FUNC("StoredChannel::Message::Last_Update");

	mantra::StorageValue rv = cache.Get("last_update");

	boost::posix_time::ptime ret;
	if (rv.type() != typeid(mantra::NullValue))
		ret = boost::get<boost::posix_time::ptime>(rv);

	MT_RET(ret);
	MT_EE
}

bool StoredChannel::MESSAGE_Exists(boost::uint32_t num) const
{
	MT_EB
	MT_FUNC("StoredChannel::MESSAGE_Exists" << num);

	bool rv = (Message::storage.RowExists(
			mantra::Comparison<mantra::C_EqualTo>::make("id", id_) &&
			mantra::Comparison<mantra::C_EqualTo>::make("number", num)) == 1u);

	MT_RET(rv);
	MT_EE
}

StoredChannel::Message StoredChannel::MESSAGE_Add(const std::string &message,
												 const boost::shared_ptr<StoredNick> &updater)
{
	MT_EB
	MT_FUNC("MESSAGE_Add" << message << updater);

	Message rv(self.lock(), message, updater);

	MT_RET(rv);
	MT_EE
}

void StoredChannel::MESSAGE_Del(boost::uint32_t num)
{
	MT_EB
	MT_FUNC("StoredChannel::MESSAGE_Del" << num);

	if (!MESSAGE_Exists(num))
		return;

	Message::storage.RemoveRow(
			mantra::Comparison<mantra::C_EqualTo>::make("id", id_) &&
			mantra::Comparison<mantra::C_EqualTo>::make("number", num));

	MT_EE
}

void StoredChannel::MESSAGE_Reindex(boost::uint32_t num, boost::uint32_t newnum)
{
	MT_EB
	MT_FUNC("StoredChannel::MESSAGE_Reindex" << num);

	mantra::Storage::RecordMap data;
	data["number"] = newnum;

	Message::storage.ChangeRow(data,
			mantra::Comparison<mantra::C_EqualTo>::make("id", id_) &&
			mantra::Comparison<mantra::C_EqualTo>::make("number", num));

	MT_EE
}

StoredChannel::Message StoredChannel::MESSAGE_Get(boost::uint32_t num) const
{
	MT_EB
	MT_FUNC("StoredChannel::MESSAGE_Exists" << num);

	Message rv(self.lock(), num);

	MT_RET(rv);
	MT_EE
}

void StoredChannel::MESSAGE_Get(std::set<Message> &fill) const
{
	MT_EB
	MT_FUNC("StoredChannel::MESSAGE_Get" << fill);

	mantra::Storage::DataSet data;
	mantra::Storage::FieldSet fields;
	fields.insert("number");

	Message::storage.RetrieveRow(data,
			mantra::Comparison<mantra::C_EqualTo>::make("id", id_), fields);
	if (data.empty())
		return;

	mantra::Storage::DataSet::iterator i;
	for (i = data.begin(); i != data.end(); ++i)
		fill.insert(Message(self.lock(), boost::get<boost::uint32_t>((*i)["number"])));

	MT_EE
}

