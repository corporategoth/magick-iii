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
#include "committee.h"
#include "liveuser.h"

#include <mantra/core/trace.h>

StorageInterface StoredNick::storage("nicks", "name");

StoredNick::StoredNick(const std::string &name,
					   const boost::shared_ptr<StoredUser> &user)
	: cache(storage, ROOT->ConfigValue<mantra::duration>("storage.cache-expire"),
			mantra::Comparison<mantra::C_EqualToNC>::make("name", name)),
	  name_(name), user_(user)
{
	MT_EB
	MT_FUNC("StoredNick::StoredNick" << name << user);


	MT_EE
}

boost::shared_ptr<StoredNick> StoredNick::create(const std::string &name,
												 const std::string &password)
{
	MT_EB
	MT_FUNC("StoredNick::create" << name << password);

	boost::shared_ptr<StoredNick> rv =
		create(name, if_StoredUser_StoredNick::create(password));

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

	// We have to do this registration check to ensure that we recognize
	// sadmins on their first registration.  We must also do it before 'load'
	// since that is where we call LiveUser's 'Online' call.
	std::vector<std::string> members = ROOT->ConfigValue<std::vector<std::string> >("operserv.services-admin");
	static mantra::iequal_to<std::string> cmp;
	for (size_t i = 0; i < members.size(); ++i)
		if (cmp(name, members[i]))
		{
			boost::shared_ptr<Committee> sadmin = ROOT->data.Get_Committee(
					ROOT->ConfigValue<std::string>("commserv.sadmin.name"));
			if (sadmin)
				if_Committee_StoredNick(sadmin).MEMBER_Add(user);
			break;
		}

	boost::shared_ptr<StoredNick> rv = load(name, user);
	if (rv->live_)
		if_LiveUser_StoredNick(rv->live_).Nick_Reg();
	ROOT->data.Add(rv);

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
		if (!user->Secure() && user->ACCESS_Matches(live))
		{
			if_LiveUser_StoredNick(live).Stored(rv);
			rv->Online(live);
		}
	}

	MT_RET(rv);
	MT_EE
}

void StoredNick::Online(const boost::shared_ptr<LiveUser> &live)
{
	MT_EB
	MT_FUNC("StoredNick::Live" << live);

	if(!live)
		return;

	mantra::Storage::RecordMap data;
	data["last_realname"] = live->Real();
	data["last_mask"] = live->User() + "@" + live->Host();
	data["last_seen"] = mantra::GetCurrentDateTime();
	cache.Put(data);

	if_StoredUser_StoredNick(user_).Online(live);
	SYNC_LOCK(live_);
	live_ = live;

	MT_EE
}

void StoredNick::Offline(const std::string &reason)
{
	MT_EB
	MT_FUNC("StoredNick::Quit" << reason);

	mantra::Storage::RecordMap data;
	if (reason.empty())
		data["last_quit"] = mantra::NullValue();
	else
		data["last_quit"] = reason;
	data["last_seen"] = mantra::GetCurrentDateTime();
	cache.Put(data);

	SYNC_LOCK(live_);
	if (live_)
	{
		if_StoredUser_StoredNick(user_).Offline(live_);
		live_.reset();
	}

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

void StoredNick::expire()
{
	MT_EB
	MT_FUNC("StoredNick::expire");

	bool locked = false;
	if (ROOT->ConfigValue<bool>("nickserv.lock.noexpire"))
	{
		if (ROOT->ConfigValue<bool>("nickserv.defaults.noexpire"))
			return;
		else
			locked = true;
	}

	boost::posix_time::ptime exptime = mantra::GetCurrentDateTime() - 
					ROOT->ConfigValue<mantra::duration>("nickserv.expire");

	mantra::Storage::DataSet data;
	mantra::Storage::FieldSet fields;
	fields.insert("name");
	fields.insert("id");

	mantra::ComparisonSet cs = mantra::Comparison<mantra::C_LessThan>::make("last_seen", exptime);
	storage.RetrieveRow(data, cs, fields);

	if (data.empty())
		return;

	std::map<boost::uint32_t, std::vector<std::string> > toexp;
	mantra::Storage::DataSet::iterator i = data.begin();
	for (i = data.begin(); i != data.end(); ++i)
	{
		toexp[boost::get<boost::uint32_t>((*i)["id"])].push_back(
				boost::get<std::string>((*i)["name"]));
	}

	std::map<boost::uint32_t, std::vector<std::string> >::iterator j;
	for (j = toexp.begin(); j != toexp.end(); ++j)
	{
		boost::shared_ptr<StoredUser> user = ROOT->data.Get_StoredUser(j->first);
		if (!user)
			continue;

		if (!locked && user->NoExpire())
			continue;

		std::vector<std::string>::iterator k;
		for (k = j->second.begin(); k != j->second.end(); ++k)
		{
			boost::shared_ptr<StoredNick> nick = ROOT->data.Get_StoredNick(*k);
			if (!nick)
				continue;

			LOG(Notice, "Expiring nickname %1%.", nick->Name());
			nick->Drop();
		}
	}

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
	mantra::StorageValue rv = cache.Get("last_realname");
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
	mantra::StorageValue rv = cache.Get("last_mask");
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
	mantra::StorageValue rv = cache.Get("last_quit");
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
	mantra::StorageValue rv = cache.Get("last_seen");
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

void StoredNick::SendInfo(const ServiceUser *service,
						  const boost::shared_ptr<LiveUser> &user) const
{
	MT_EB
	MT_FUNC("StoredNick::SendInfo" << service << user);

	if (!service || !user)
		return;

	bool priv = false;
	if (!(user->InCommittee(ROOT->ConfigValue<std::string>("commserv.oper.name")) ||
		  user->InCommittee(ROOT->ConfigValue<std::string>("commserv.sop.name"))))
	{
		if (!user->Stored() || user->Stored()->User() != user_)
			priv = user_->Private();
	}

	mantra::Storage::RecordMap data, last_data;
	mantra::Storage::RecordMap::const_iterator i, j;
	cache.Get(data);

	boost::shared_ptr<StoredNick> last = Last_Seen(user_);
	if (last != self.lock())
		last->cache.Get(last_data);

	boost::shared_ptr<LiveUser> live;
	{
		SYNC_LOCK(live_);
		live = live_;
	}
	if (live)
	{
		SEND(service, user, N_("%1% is %2%"), live->Name() % live->Real());
	}
	else
	{
		mantra::Storage::RecordMap::const_iterator j, i = data.find("last_realname");
		if (i != data.end())
			SEND(service, user, N_("%1% was %2%"), name_ %
				 boost::get<std::string>(i->second));
		else
		{
			i = last_data.find("last_realname");
			if (i != last_data.end())
				SEND(service, user, N_("%1% was %2%"), name_ %
					 boost::get<std::string>(i->second));
			else
				SEND(service, user, N_("%1% has never signed on."), name_);
		}
	}

	SEND(service, user, N_("Registered     : %1% ago"),
		 DurationToString(mantra::duration(boost::get<boost::posix_time::ptime>(data["registered"]),
										   mantra::GetCurrentDateTime()), mantra::Second));

	if (!live && !priv)
	{
		i = data.find("last_seen");
		j = data.find("last_mask");
		if (i != data.end() && j != data.end())
			SEND(service, user, N_("Last Used      : %1% ago from %2%"),
				 DurationToString(mantra::duration(boost::get<boost::posix_time::ptime>(i->second),
												   mantra::GetCurrentDateTime()), mantra::Second) %
				 boost::get<std::string>(j->second));
		i = data.find("last_quit");
		if (i != data.end())
			SEND(service, user, N_("Signoff Reason : %1%"),
				 boost::get<std::string>(i->second));
	}

	StoredUser::online_users_t online = user_->Online();
	if (online.empty())
	{
		if (last != self.lock() && !priv)
		{
			i = last_data.find("last_seen");
			j = last_data.find("last_mask");
			if (i != last_data.end() && j != last_data.end())
			{
				SEND(service, user, N_("Last Online    : %1% ago from %2%!%3%"),
					 DurationToString(mantra::duration(boost::get<boost::posix_time::ptime>(i->second),
													   mantra::GetCurrentDateTime()), mantra::Second) %
					 last->name_ % boost::get<std::string>(j->second));
				i = last_data.find("last_quit");
				if (i != last_data.end())
					SEND(service, user, N_("Signoff Reason : %1%"),
						 boost::get<std::string>(i->second));
			}
		}
	}
	else
	{
		std::string str;
		StoredUser::online_users_t::const_iterator k;
		for (k = online.begin(); k != online.end(); ++k)
		{
			if (!(*k))
				continue;
			if (!str.empty())
				str += ", ";
			str += (*k)->Name();
		}
		SEND(service, user, N_("Online As      : %1%"), str);
	}

	if_StoredUser_StoredNick(user_).SendInfo(service, user);

	MT_EE
}

std::string StoredNick::translate(const std::string &in) const
{
	return user_->translate(in);
}

std::string StoredNick::translate(const std::string &single,
								  const std::string &plural,
								  unsigned long n) const
{
	return user_->translate(single, plural, n);
}

