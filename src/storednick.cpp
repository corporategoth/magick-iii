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
#define RCSID(x,y) const char *rcsid_magick__storednick_cpp_ ## x () { return y; }
RCSID(magick__storednick_cpp, "@(#)$Id$");

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
#include "storednick.h"
#include "storeduser.h"
#include "liveuser.h"

#include <mantra/core/trace.h>

StorageInterface StoredNick::storage("nicks", "name");

boost::shared_ptr<StoredNick> StoredNick::create(const std::string &name,
												 const std::string &password)
{
	MT_EB
	MT_FUNC("StoredNick::create" << name << password);

	boost::shared_ptr<StoredNick> rv =
		create(name, if_StoredUser_StoredNick::create(password));
	ROOT->data.Add(rv->user_);

	MT_RET(rv);
	MT_EE
}

boost::shared_ptr<StoredNick> StoredNick::create(const std::string &name,
												 const boost::shared_ptr<StoredUser> &user)
{
	MT_EB
	MT_FUNC("StoredNick::create" << name << user);

	mantra::Storage::RecordMap rec;
	rec["name"] = name;
	rec["id"] = user->ID();
	storage.InsertRow(rec);

	boost::shared_ptr<StoredNick> rv = load(name, user);

	MT_RET(rv);
	MT_EE
}

boost::shared_ptr<StoredNick> StoredNick::load(const std::string &name,
											   const boost::shared_ptr<StoredUser> &user)
{
	MT_EB
	MT_FUNC("StoredNick::load" << name << user);

	boost::shared_ptr<StoredNick> rv(new StoredNick(name, user));
	rv->self = rv;

	if_StoredUser_StoredNick(user).Add(rv);
	boost::shared_ptr<LiveUser> live = ROOT->data.Get_LiveUser(name);
	if (live)
	{
		if (user->ACCESS_Matches(live) && !user->Secure())
		{
			if_LiveUser_StoredNick(live).Stored(rv);
			rv->Live(live);
		}
	}

	MT_RET(rv);
	MT_EE
}

void StoredNick::Live(const boost::shared_ptr<LiveUser> &live)
{
	MT_EB
	MT_FUNC("StoredNick::Live" << live);

	if (live)
	{
		mantra::Storage::RecordMap data;
		data["last_realname"] = live->Real();
		data["last_mask"] = live->User() + "@" + live->Host();
		data["last_seen"] = mantra::GetCurrentDateTime();
		storage.ChangeRow(data, mantra::Comparison<mantra::C_EqualToNC>::make("name", name_));
		if_StoredUser_StoredNick(user_).Online(live_);
	}
	else
		if_StoredUser_StoredNick(user_).Offline(live_);
	SYNC_LOCK(live_);
	live_ = live;

	MT_EE
}

void StoredNick::Quit(const std::string &reason)
{
	MT_EB
	MT_FUNC("StoredNick::Quit" << reason);

	mantra::Storage::RecordMap data;
	{
		data["last_quit"] = reason;
		data["last_seen"] = mantra::GetCurrentDateTime();
		SYNC_LOCK(live_);
		if (live_)
		{
			if_StoredUser_StoredNick(user_).Offline(live_);
			live_.reset();
		}
	}
	storage.ChangeRow(data, mantra::Comparison<mantra::C_EqualToNC>::make("name", name_));

	MT_EE
}

boost::shared_ptr<StoredNick> StoredNick::Last_Seen(const boost::shared_ptr<StoredUser> &user)
{
	MT_EB
	MT_FUNC("StoredNick::Last_Seen" << user);

	mantra::Storage::DataSet data;
	mantra::Storage::FieldSet fields;
	fields.insert("name");
	fields.insert("last_seen");
	storage.RetrieveRow(data, mantra::Comparison<mantra::C_EqualTo>::make("id", user->ID()), fields);

	boost::shared_ptr<StoredNick> ret;
	if (data.empty())
		MT_RET(ret);

	std::string maxname;
	boost::posix_time::ptime maxtime(boost::date_time::not_a_date_time);

	mantra::Storage::DataSet::const_iterator i = data.begin();
	for (i = data.begin(); i != data.end(); ++i)
	{
		mantra::Storage::RecordMap::const_iterator j = i->find("last_seen");
		if (j == i->end() || j->second.type() == typeid(mantra::NullValue))
			continue;

		boost::posix_time::ptime val = boost::get<boost::posix_time::ptime>(j->second);
		if (maxtime.is_special() || maxtime < val)
		{
			j = i->find("name");
			if (j == i->end() || j->second.type() == typeid(mantra::NullValue))
				continue;

			maxname = boost::get<std::string>(j->second);
			maxtime = val;
		}
	}

	if (maxname.empty())
		MT_RET(ret);

	ret = ROOT->data.Get_StoredNick(maxname);
	MT_RET(ret);

	MT_EE
}

boost::shared_ptr<LiveUser> StoredNick::Live() const
{
	MT_EB
	MT_FUNC("StoredNick::Live");

	SYNC_LOCK(live_);
	MT_RET(live_);

	MT_EE
}

std::string StoredNick::Last_RealName() const
{
	MT_EB
	MT_FUNC("StoredNick::Last_RealName");

	std::string ret;
	mantra::StorageValue rv = storage.GetField(name_, "last_realname");
	if (rv.type() == typeid(mantra::NullValue))
		MT_RET(ret);
	ret = boost::get<std::string>(rv);
	MT_RET(ret);

	MT_EE
}

std::string StoredNick::Last_Mask() const
{
	MT_EB
	MT_FUNC("StoredNick::Last_Mask");

	std::string ret;
	mantra::StorageValue rv = storage.GetField(name_, "last_mask");
	if (rv.type() == typeid(mantra::NullValue))
		MT_RET(ret);
	ret = boost::get<std::string>(rv);
	MT_RET(ret);

	MT_EE
}

std::string StoredNick::Last_Quit() const
{
	MT_EB
	MT_FUNC("StoredNick::Last_Quit");

	std::string ret;
	mantra::StorageValue rv = storage.GetField(name_, "last_quit");
	if (rv.type() == typeid(mantra::NullValue))
		MT_RET(ret);
	ret = boost::get<std::string>(rv);
	MT_RET(ret);

	MT_EE
}

boost::posix_time::ptime StoredNick::Last_Seen() const
{
	MT_EB
	MT_FUNC("StoredNick::Last_Seen");

	boost::posix_time::ptime ret(boost::date_time::not_a_date_time);
	mantra::StorageValue rv = storage.GetField(name_, "last_seen");
	if (rv.type() == typeid(mantra::NullValue))
		MT_RET(ret);
	ret = boost::get<boost::posix_time::ptime>(rv);
	MT_RET(ret);

	MT_EE
}

void StoredNick::DropInternal()
{
	MT_EB
	MT_FUNC("StoredNick::DropInternal");

	SYNC_LOCK(live_);
	if (live_)
	{
		if_LiveUser_StoredNick(live_).Stored(boost::shared_ptr<StoredNick>());
		live_.reset();
	}

	MT_EE
}

void StoredNick::Drop()
{
	MT_EB
	MT_FUNC("StoredNick::Drop");

	if_StoredUser_StoredNick(user_).Del(self.lock());
	if_StorageDeleter<StoredNick>(ROOT->data).Del(self.lock());

	MT_EE
}
