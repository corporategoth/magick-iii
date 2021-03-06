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
#define RCSID(x,y) const char *rcsid_magick__committee_cpp_ ## x () { return y; }
RCSID(magick__committee_cpp, "@(#)$Id$");

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
#include "committee.h"
#include "storeduser.h"
#include "storednick.h"
#include "liveuser.h"

#include <mantra/core/trace.h>

StorageInterface Committee::storage("committees", "id", "last_update");
StorageInterface Committee::Member::storage("committees_member", std::string(), "last_update");
StorageInterface Committee::Message::storage("committees_message", std::string(), "last_update");

Committee::Committee(boost::uint32_t id, const std::string &name)
	: cache(storage, ROOT->ConfigValue<mantra::duration>("storage.cache-expire"),
			mantra::Comparison<mantra::C_EqualTo>::make("id", id)),
	  id_(id), name_(name), SYNC_NRWINIT(online_members_, reader_priority)
{
	MT_EB
	MT_FUNC("Committee::Committee" << name);

	MT_EE
}

static boost::mutex id_lock;

boost::shared_ptr<Committee> Committee::create(const std::string &name)
{
	MT_EB
	MT_FUNC("Committee::create" << name);

	boost::shared_ptr<Committee> rv;
	static mantra::iequal_to<std::string> cmp;
	if (cmp(name, ROOT->ConfigValue<std::string>("commserv.oper.name")) ||
		cmp(name, ROOT->ConfigValue<std::string>("commserv.sop.name")) ||
		cmp(name, ROOT->ConfigValue<std::string>("commserv.admin.name")))
		MT_RET(rv);

	boost::uint32_t id;
	{
		boost::mutex::scoped_lock sl(id_lock);
		mantra::StorageValue v = storage.Maximum("id");
		if (v.type() == typeid(mantra::NullValue))
			id = 1;
		else
			id = boost::get<boost::uint32_t>(v) + 1;

		MT_CP("New maximum ID will be %1% (%2%).", id % name);
		mantra::Storage::RecordMap rec;
		rec["id"] = id;
		rec["name"] = name;
		if (!storage.InsertRow(rec))
			MT_RET(rv);
	}

	rv = load(id, name);
	ROOT->data.Add(rv);

	MT_RET(rv);
	MT_EE
}

boost::shared_ptr<Committee> Committee::create(const std::string &name,
							const boost::shared_ptr<Committee> &head)
{
	MT_EB
	MT_FUNC("Committee::create" << name << head);

	boost::shared_ptr<Committee> rv;
	static mantra::iequal_to<std::string> cmp;
	if (cmp(name, ROOT->ConfigValue<std::string>("commserv.all.name")) ||
		cmp(name, ROOT->ConfigValue<std::string>("commserv.regd.name")) ||
		cmp(name, ROOT->ConfigValue<std::string>("commserv.sadmin.name")))
		MT_RET(rv);

	boost::uint32_t id;
	{
		boost::mutex::scoped_lock sl(id_lock);
		mantra::StorageValue v = storage.Maximum("id");
		if (v.type() == typeid(mantra::NullValue))
			id = 1;
		else
			id = boost::get<boost::uint32_t>(v) + 1;

		MT_CP("New maximum ID will be %1% (%2%).", id % name);
		mantra::Storage::RecordMap rec;
		rec["id"] = id;
		rec["name"] = name;
		rec["head_committee"] = head->ID();
		if (!storage.InsertRow(rec))
			MT_RET(rv);
	}

	rv = load(id, name);
	ROOT->data.Add(rv);

	MT_RET(rv);
	MT_EE
}

boost::shared_ptr<Committee> Committee::create(const std::string &name,
							const boost::shared_ptr<StoredUser> &head)
{
	MT_EB
	MT_FUNC("Committee::create" << name << head);

	boost::shared_ptr<Committee> rv;
	static mantra::iequal_to<std::string> cmp;
	if (cmp(name, ROOT->ConfigValue<std::string>("commserv.all.name")) ||
		cmp(name, ROOT->ConfigValue<std::string>("commserv.regd.name")) ||
		cmp(name, ROOT->ConfigValue<std::string>("commserv.oper.name")) ||
		cmp(name, ROOT->ConfigValue<std::string>("commserv.sop.name")) ||
		cmp(name, ROOT->ConfigValue<std::string>("commserv.admin.name")) ||
		cmp(name, ROOT->ConfigValue<std::string>("commserv.sadmin.name")))
		MT_RET(rv);

	boost::uint32_t id;
	{
		boost::mutex::scoped_lock sl(id_lock);
		mantra::StorageValue v = storage.Maximum("id");
		if (v.type() == typeid(mantra::NullValue))
			id = 1;
		else
			id = boost::get<boost::uint32_t>(v) + 1;

		MT_CP("New maximum ID will be %1% (%2%).", id % name);
		mantra::Storage::RecordMap rec;
		rec["id"] = id;
		rec["name"] = name;
		rec["head_user"] = head->ID();
		if (!storage.InsertRow(rec))
			MT_RET(rv);
	}

	rv = load(id, name);
	ROOT->data.Add(rv);

	MT_RET(rv);
	MT_EE
}

boost::shared_ptr<Committee> Committee::load(boost::uint32_t id, const std::string &name)
{
	MT_EB
	MT_FUNC("Committee::load" << id << name);

	boost::shared_ptr<Committee> rv(new Committee(id, name));
	rv->self = rv;

	MT_RET(rv);
	MT_EE
}

void Committee::Online(const boost::shared_ptr<LiveUser> &in)
{
	MT_EB
	MT_FUNC("Committee::Online" << in);

	SYNC_WLOCK(online_members_);
	online_members_.insert(in);

	MT_EE
}

void Committee::Offline(const boost::shared_ptr<LiveUser> &in)
{
	MT_EB
	MT_FUNC("Committee::Offline" << in);

	SYNC_WLOCK(online_members_);
	online_members_.erase(in);

	MT_EE
}

std::set<boost::shared_ptr<Committee> > Committee::FindCommittees(const boost::shared_ptr<StoredUser> &in)
{
	MT_EB
	MT_FUNC("Committee::Live_Committees" << in);

	std::set<boost::shared_ptr<Committee> > rv;
	mantra::Storage::DataSet data;
	mantra::Storage::FieldSet fields;
	fields.insert("id");
	storage.RetrieveRow(data, mantra::Comparison<mantra::C_EqualTo>::make("head_user", in->ID()), fields);

	mantra::Storage::DataSet::const_iterator i;
	for (i = data.begin(); i != data.end(); ++i)
	{
		mantra::Storage::RecordMap::const_iterator j = i->find("id");
		if (j == i->end() || j->second.type() == typeid(mantra::NullValue))
			continue;

		boost::shared_ptr<Committee> comm = ROOT->data.Get_Committee(
						boost::get<boost::uint32_t>(j->second));
		if (comm)
			rv.insert(comm);
	}

	data.clear();
	Member::storage.RetrieveRow(data, mantra::Comparison<mantra::C_EqualTo>::make("entry", in->ID()), fields);
	for (i = data.begin(); i != data.end(); ++i)
	{
		mantra::Storage::RecordMap::const_iterator j = i->find("id");
		if (j == i->end() || j->second.type() == typeid(mantra::NullValue))
			continue;

		boost::shared_ptr<Committee> comm = ROOT->data.Get_Committee(
						boost::get<boost::uint32_t>(j->second));
		if (comm)
			rv.insert(comm);
	}

	std::vector<mantra::StorageValue> look;
	std::set<boost::shared_ptr<Committee> >::iterator k;
	for (k = rv.begin(); k != rv.end(); ++k)
		look.push_back((*k)->ID());

	while (!look.empty())
	{
		data.clear();
		storage.RetrieveRow(data, mantra::Comparison<mantra::C_OneOf>::make("head_committee", look), fields);
		look.clear();
		for (i = data.begin(); i != data.end(); ++i)
		{
			mantra::Storage::RecordMap::const_iterator j = i->find("id");
			if (j == i->end() || j->second.type() == typeid(mantra::NullValue))
				continue;

			boost::shared_ptr<Committee> comm = ROOT->data.Get_Committee(
							boost::get<boost::uint32_t>(j->second));
			if (comm)
			{
				look.push_back(comm->ID());
				rv.insert(comm);
			}
		}
	}

	MT_RET(rv);
	MT_EE
}

void Committee::DropInternal()
{
	MT_EB
	MT_FUNC("Committee::DropInternal");


	MT_EE
}

void Committee::Head(const boost::shared_ptr<StoredUser> &head)
{
	MT_EB
	MT_FUNC("Committee::Head" << head);

	if (IsSpecial())
		return;

	mantra::Storage::RecordMap rec;
	rec["head_committee"] = mantra::NullValue();
	rec["head_user"] = head->ID();
	cache.Put(rec);

	MT_EE
}

void Committee::Head(const boost::shared_ptr<Committee> &head)
{
	MT_EB
	MT_FUNC("Committee::Head" << head);

	if (IsSpecial())
		return;

	mantra::Storage::RecordMap rec;
	rec["head_committee"] = head->ID();
	rec["head_user"] = mantra::NullValue();
	cache.Put(rec);

	MT_EE
}

boost::shared_ptr<StoredUser> Committee::HeadUser() const
{
	MT_EB
	MT_FUNC("HeadUser");

	boost::shared_ptr<StoredUser> ret;
	mantra::StorageValue rv = cache.Get("head_user");
	if (rv.type() == typeid(mantra::NullValue))
		MT_RET(ret);
	ret = ROOT->data.Get_StoredUser(boost::get<boost::uint32_t>(rv));

	MT_RET(ret);
	MT_EE
}

boost::shared_ptr<Committee> Committee::HeadCommittee() const
{
	MT_EB
	MT_FUNC("HeadCommittee");

	boost::shared_ptr<Committee> ret;
	mantra::StorageValue rv = cache.Get("head_committee");
	if (rv.type() == typeid(mantra::NullValue))
		MT_RET(ret);
	ret = ROOT->data.Get_Committee(boost::get<boost::uint32_t>(rv));

	MT_RET(ret);
	MT_EE
}

void Committee::Description(const std::string &in)
{
	MT_EB
	MT_FUNC("Committee::Description" << in);

	cache.Put("description", in);

	MT_EE
}

std::string Committee::Description() const
{
	MT_EB
	MT_FUNC("Committee::Description");

	std::string ret;
	mantra::StorageValue rv = cache.Get("description");
	if (rv.type() == typeid(mantra::NullValue))
		MT_RET(ret);
	ret = boost::get<std::string>(rv);

	MT_RET(ret);
	MT_EE
}

void Committee::Comment(const std::string &in)
{
	MT_EB
	MT_FUNC("Committee::Comment" << in);

	cache.Put("comment", in);

	MT_EE
}

std::string Committee::Comment() const
{
	MT_EB
	MT_FUNC("Committee::Comment");

	std::string ret;
	mantra::StorageValue rv = cache.Get("comment");
	if (rv.type() == typeid(mantra::NullValue))
		MT_RET(ret);
	ret = boost::get<std::string>(rv);

	MT_RET(ret);
	MT_EE
}

void Committee::Email(const std::string &in)
{
	MT_EB
	MT_FUNC("Committee::Email" << in);

	if (in.empty())
		cache.Put("email", mantra::NullValue());
	else
		cache.Put("email", in);

	MT_EE
}

std::string Committee::Email() const
{
	MT_EB
	MT_FUNC("Committee::Email");

	std::string ret;
	mantra::StorageValue rv = cache.Get("email");
	if (rv.type() == typeid(mantra::NullValue))
		MT_RET(ret);
	ret = boost::get<std::string>(rv);

	MT_RET(ret);
	MT_EE
}

void Committee::Website(const std::string &in)
{
	MT_EB
	MT_FUNC("Committee::Website" << in);

	if (in.empty())
		cache.Put("website", mantra::NullValue());
	else
		cache.Put("website", in);

	MT_EE
}

std::string Committee::Website() const
{
	MT_EB
	MT_FUNC("Committee::Website");

	std::string ret;
	mantra::StorageValue rv = cache.Get("website");
	if (rv.type() == typeid(mantra::NullValue))
		MT_RET(ret);
	ret = boost::get<std::string>(rv);

	MT_RET(ret);
	MT_EE
}

bool Committee::Private(const boost::logic::tribool &in)
{
	MT_EB
	MT_FUNC("Committee::Private" << in);

	if (LOCK_Private())
		MT_RET(false);

	if (boost::logic::indeterminate(in))
		cache.Put("private", mantra::NullValue::instance());
	else
		cache.Put("private", static_cast<bool>(in));

	MT_RET(true);
	MT_EE
}

bool Committee::Private() const
{
	MT_EB
	MT_FUNC("Committee::Private");

	bool ret;
	if (ROOT->ConfigValue<bool>("commserv.lock.private"))
		ret = ROOT->ConfigValue<bool>("commserv.defaults.private");
	else
	{
		mantra::StorageValue rv = cache.Get("private");
		if (rv.type() == typeid(mantra::NullValue))
			ret = ROOT->ConfigValue<bool>("commserv.defaults.private");
		else
			ret = boost::get<bool>(rv);
	}

	MT_RET(ret);
	MT_EE
}

bool Committee::OpenMemos(const boost::logic::tribool &in)
{
	MT_EB
	MT_FUNC("Committee::OpenMemos" << in);

	if (LOCK_OpenMemos())
		MT_RET(false);

	if (boost::logic::indeterminate(in))
		cache.Put("openmemos", mantra::NullValue::instance());
	else
		cache.Put("openmemos", static_cast<bool>(in));

	MT_RET(true);
	MT_EE
}

bool Committee::OpenMemos() const
{
	MT_EB
	MT_FUNC("Committee::OpenMemos");

	bool ret;
	if (ROOT->ConfigValue<bool>("commserv.lock.openmemos"))
		ret = ROOT->ConfigValue<bool>("commserv.defaults.openmemos");
	else
	{
		mantra::StorageValue rv = cache.Get("openmemos");
		if (rv.type() == typeid(mantra::NullValue))
			ret = ROOT->ConfigValue<bool>("commserv.defaults.openmemos");
		else
			ret = boost::get<bool>(rv);
	}

	MT_RET(ret);
	MT_EE
}

bool Committee::Secure(const boost::logic::tribool &in)
{
	MT_EB
	MT_FUNC("Committee::Secure" << in);

	if (LOCK_Secure())
		MT_RET(false);

	if (boost::logic::indeterminate(in))
		cache.Put("secure", mantra::NullValue::instance());
	else
		cache.Put("secure", static_cast<bool>(in));

	MT_RET(true);
	MT_EE
}

bool Committee::Secure() const
{
	MT_EB
	MT_FUNC("Committee::Secure");

	bool ret;
	if (ROOT->ConfigValue<bool>("commserv.lock.secure"))
		ret = ROOT->ConfigValue<bool>("commserv.defaults.secure");
	else
	{
		mantra::StorageValue rv = cache.Get("secure");
		if (rv.type() == typeid(mantra::NullValue))
			ret = ROOT->ConfigValue<bool>("commserv.defaults.secure");
		else
			ret = boost::get<bool>(rv);
	}

	MT_RET(ret);
	MT_EE
}

bool Committee::LOCK_Private(const bool &in)
{
	MT_EB
	MT_FUNC("Committee::LOCK_Private" << in);

	if (ROOT->ConfigValue<bool>("commserv.lock.private"))
		MT_RET(false);

	if (*this == ROOT->ConfigValue<std::string>("commserv.all.name") ||
		*this == ROOT->ConfigValue<std::string>("commserv.regd.name") ||
		*this == ROOT->ConfigValue<std::string>("commserv.sadmin.name"))
	{
		MT_RET(false);
	}

	cache.Put("lock_private", in);

	MT_RET(true);
	MT_EE
}

bool Committee::LOCK_Private() const
{
	MT_EB
	MT_FUNC("Committee::LOCK_Private");

	if (ROOT->ConfigValue<bool>("commserv.lock.private"))
		MT_RET(true);

	mantra::StorageValue rv = cache.Get("lock_private");
	if (rv.type() == typeid(mantra::NullValue))
		MT_RET(false);
	bool ret = boost::get<bool>(rv);

	MT_RET(ret);
	MT_EE
}

bool Committee::LOCK_OpenMemos(const bool &in)
{
	MT_EB
	MT_FUNC("Committee::LOCK_OpenMemos" << in);

	if (ROOT->ConfigValue<bool>("commserv.lock.openmemos"))
		MT_RET(false);

	if (*this == ROOT->ConfigValue<std::string>("commserv.all.name") ||
		*this == ROOT->ConfigValue<std::string>("commserv.regd.name") ||
		*this == ROOT->ConfigValue<std::string>("commserv.sadmin.name"))
	{
		MT_RET(false);
	}

	cache.Put("lock_openmemos", in);

	MT_RET(true);
	MT_EE
}

bool Committee::LOCK_OpenMemos() const
{
	MT_EB
	MT_FUNC("Committee::LOCK_OpenMemos");

	if (ROOT->ConfigValue<bool>("commserv.lock.openmemos"))
		MT_RET(true);

	mantra::StorageValue rv = cache.Get("lock_openmemos");
	if (rv.type() == typeid(mantra::NullValue))
		MT_RET(false);
	bool ret = boost::get<bool>(rv);

	MT_RET(ret);
	MT_EE
}

bool Committee::LOCK_Secure(const bool &in)
{
	MT_EB
	MT_FUNC("Committee::LOCK_Secure" << in);

	if (ROOT->ConfigValue<bool>("commserv.lock.secure"))
		MT_RET(false);

	if (*this == ROOT->ConfigValue<std::string>("commserv.all.name") ||
		*this == ROOT->ConfigValue<std::string>("commserv.regd.name") ||
		*this == ROOT->ConfigValue<std::string>("commserv.sadmin.name"))
	{
		MT_RET(false);
	}

	cache.Put("lock_secure", in);

	MT_RET(true);
	MT_EE
}

bool Committee::LOCK_Secure() const
{
	MT_EB
	MT_FUNC("Committee::LOCK_Secure");

	if (ROOT->ConfigValue<bool>("commserv.lock.secure"))
		MT_RET(true);

	mantra::StorageValue rv = cache.Get("lock_secure");
	if (rv.type() == typeid(mantra::NullValue))
		MT_RET(false);
	bool ret = boost::get<bool>(rv);

	MT_RET(ret);
	MT_EE
}

bool Committee::IsSpecial() const
{
	MT_EB
	MT_FUNC("Committee::IsSpecial");

	if (*this == ROOT->ConfigValue<std::string>("commserv.all.name") ||
		*this == ROOT->ConfigValue<std::string>("commserv.regd.name") ||
		*this == ROOT->ConfigValue<std::string>("commserv.oper.name") ||
		*this == ROOT->ConfigValue<std::string>("commserv.sop.name") ||
		*this == ROOT->ConfigValue<std::string>("commserv.admin.name") ||
		*this == ROOT->ConfigValue<std::string>("commserv.sadmin.name"))
		MT_RET(true);
	MT_RET(false);

	MT_EE
}

bool Committee::IsPrivileged() const
{
	MT_EB
	MT_FUNC("Committee::IsSpecial");

	if (*this == ROOT->ConfigValue<std::string>("commserv.oper.name") ||
		*this == ROOT->ConfigValue<std::string>("commserv.sop.name") ||
		*this == ROOT->ConfigValue<std::string>("commserv.admin.name") ||
		*this == ROOT->ConfigValue<std::string>("commserv.sadmin.name"))
		MT_RET(true);
	MT_RET(false);

	MT_EE
}

bool Committee::IsMember(const boost::shared_ptr<LiveUser> &user) const
{
	MT_EB
	MT_FUNC("Committee::IsMember" << user);

	if (!user)
		MT_RET(false);

	SYNC_RLOCK(online_members_);
	bool rv = (online_members_.find(user) != online_members_.end());

	MT_RET(rv);
	MT_EE
}

bool Committee::IsHead(const boost::shared_ptr<LiveUser> &user) const
{
	MT_EB
	MT_FUNC("Committee::IsHead" << user);

	if (IsMember(user))
	{
		boost::shared_ptr<StoredNick> nick = user->Stored();
		if (!nick)
			MT_RET(false);

		bool rv = (HeadUser() == nick->User());
		if (!rv)
		{
			boost::shared_ptr<Committee> comm = HeadCommittee();
			if (comm)
				rv = comm->IsMember(user);
		}
		MT_RET(rv);
	}

	MT_RET(false);
	MT_EE
}

bool Committee::IsMember(const boost::shared_ptr<StoredUser> &user) const
{
	MT_EB
	MT_FUNC("Committee::IsMember" << user);

	if (!user)
		MT_RET(false);

	// Live committees, user CAN'T be in them.
	if (name_ == ROOT->ConfigValue<std::string>("commserv.all.name") ||
		name_ == ROOT->ConfigValue<std::string>("commserv.regd.name"))
		MT_RET(false);

	bool rv = false;
	if (HeadUser() == user || MEMBER_Exists(user))
	{
		rv = true;
	}
	else
	{
		boost::shared_ptr<Committee> comm = HeadCommittee();
		if (comm)
			rv = comm->IsMember(user);
	}

	MT_RET(rv);
	MT_EE
}

bool Committee::IsHead(const boost::shared_ptr<StoredUser> &user) const
{
	MT_EB
	MT_FUNC("Committee::IsHead" << user);

	if (!user)
		MT_RET(false);

	bool rv = false;
	if (HeadUser() == user)
	{
		rv = true;
	}
	else
	{
		boost::shared_ptr<Committee> comm = HeadCommittee();
		if (comm)
			rv = comm->IsMember(user);
	}

	MT_RET(rv);
	MT_EE
}

void Committee::Drop()
{
	MT_EB
	MT_FUNC("Committee::Drop");

	if_StorageDeleter<Committee>(ROOT->data).Del(self.lock());

	MT_EE
}

bool Committee::MEMBER_Exists(boost::uint32_t num) const
{
	MT_EB
	MT_FUNC("Committee::MEMBER_Exists" << num);

	bool rv = (Member::storage.RowExists(
				mantra::Comparison<mantra::C_EqualTo>::make("id", id_) &&
				mantra::Comparison<mantra::C_EqualTo>::make("number", num)));

	MT_RET(rv);
	MT_EE
}

bool Committee::MEMBER_Exists(const boost::shared_ptr<StoredUser> &entry) const
{
	MT_EB
	MT_FUNC("Committee::MEMBER_Exists" << entry);

	bool rv = (Member::storage.RowExists(
				mantra::Comparison<mantra::C_EqualTo>::make("id", id_) &&
				mantra::Comparison<mantra::C_EqualTo>::make("entry", entry->ID())));

	MT_RET(rv);
	MT_EE
}

void Committee::MEMBER_Add(const boost::shared_ptr<StoredUser> &entry)
{
	MT_EB
	MT_FUNC("Committee::MEMBER_Add" << entry);

	if (!entry)
		return;

	if (*this != ROOT->ConfigValue<std::string>("commserv.sadmin.name"))
		return;

	boost::uint32_t number = 0;

	SYNC_LOCK(member_number);
	mantra::StorageValue rv = Member::storage.Maximum("number",
				mantra::Comparison<mantra::C_EqualTo>::make("id", id_));
	if (rv.type() == typeid(mantra::NullValue))
		number = 1;
	else
		number = boost::get<boost::uint32_t>(rv) + 1;

	mantra::Storage::RecordMap rec;
	rec["id"] = id_;
	rec["number"] = number;
	rec["entry"] = entry->ID();
	rec["last_updater"] = ROOT->operserv.Primary();
	Member::storage.InsertRow(rec);

	MT_EE
}

Committee::Member Committee::MEMBER_Add(const boost::shared_ptr<StoredUser> &entry,
										const boost::shared_ptr<StoredNick> &updater)
{
	MT_EB
	MT_FUNC("Committee::MEMBER_Add" << entry << updater);

	if (!entry || !updater)
	{
		Member m;
		MT_RET(m);
	}

	if (*this == ROOT->ConfigValue<std::string>("commserv.all.name") ||
		*this == ROOT->ConfigValue<std::string>("commserv.regd.name") ||
		*this == ROOT->ConfigValue<std::string>("commserv.sadmin.name"))
	{
		Member m;
		MT_RET(m);
	}

	boost::uint32_t number = 0;

	SYNC_LOCK(member_number);
	mantra::StorageValue rv = Member::storage.Maximum("number",
				mantra::Comparison<mantra::C_EqualTo>::make("id", id_));
	if (rv.type() == typeid(mantra::NullValue))
		number = 1;
	else
		number = boost::get<boost::uint32_t>(rv) + 1;

	mantra::Storage::RecordMap rec;
	rec["id"] = id_;
	rec["number"] = number;
	rec["entry"] = entry->ID();
	rec["last_updater"] = updater->Name();
	rec["last_updater_id"] = updater->User()->ID();

	if (Member::storage.InsertRow(rec))
	{
		Member m(self.lock(), number);
		MT_RET(m);
	}

	Member m;
	MT_RET(m);
	MT_EE
}

void Committee::MEMBER_Del(boost::uint32_t num)
{
	MT_EB
	MT_FUNC("Committee::MEMBER_Del" << num);

	Member::storage.RemoveRow(
				mantra::Comparison<mantra::C_EqualTo>::make("id", id_) &&
				mantra::Comparison<mantra::C_EqualTo>::make("number", num));

	MT_EE
}

void Committee::MEMBER_Del(const boost::shared_ptr<StoredUser> &entry)
{
	MT_EB
	MT_FUNC("Committee::MEMBER_Del" << entry);

	Member::storage.RemoveRow(
				mantra::Comparison<mantra::C_EqualTo>::make("id", id_) &&
				mantra::Comparison<mantra::C_EqualTo>::make("entry", entry->ID()));

	MT_EE
}

Committee::Member Committee::MEMBER_Get(boost::uint32_t num) const
{
	MT_EB
	MT_FUNC("Committee::MEMBER_Get" << num);

	if (MEMBER_Exists(num))
	{
		Member m(self.lock(), num);
		MT_RET(m);
	}

	Member m;
	MT_RET(m);
	MT_EE
}

Committee::Member Committee::MEMBER_Get(const boost::shared_ptr<StoredUser> &entry) const
{
	MT_EB
	MT_FUNC("Committee::MEMBER_Get" << entry);

	if (MEMBER_Exists(entry))
	{
		Member m(self.lock(), entry);
		MT_RET(m);
	}

	Member m;
	MT_RET(m);
	MT_EE
}

void Committee::MEMBER_Get(std::set<Committee::Member> &fill) const
{
	MT_EB
	MT_FUNC("Committee::MEMBER_Get" << fill);

	mantra::Storage::DataSet data;
	mantra::Storage::FieldSet fields;
	fields.insert("number");

	Member::storage.RetrieveRow(data,
				mantra::Comparison<mantra::C_EqualTo>::make("id", id_),
				fields);

	mantra::Storage::DataSet::const_iterator i;
	for (i = data.begin(); i != data.end(); ++i)
	{
		mantra::Storage::RecordMap::const_iterator j = i->find("number");
		if (j == i->end() || j->second.type() == typeid(mantra::NullValue))
			continue;

		fill.insert(Member(self.lock(), boost::get<boost::uint32_t>(j->second)));
	}

	MT_EE
}


bool Committee::MESSAGE_Exists(boost::uint32_t num) const
{
	MT_EB
	MT_FUNC("Committee::MESSAGE_Exists" << num);

	bool rv = (Message::storage.RowExists(
				mantra::Comparison<mantra::C_EqualTo>::make("id", id_) &&
				mantra::Comparison<mantra::C_EqualTo>::make("number", num)));

	MT_RET(rv);
	MT_EE
}

Committee::Message Committee::MESSAGE_Add(const std::string &message, 
										  const boost::shared_ptr<StoredNick> &updater)
{
	MT_EB
	MT_FUNC("Committee::MESSAGE_Add" << message << updater);

	boost::uint32_t number = 0;

	SYNC_LOCK(message_number);
	mantra::StorageValue rv = Message::storage.Maximum("number",
				mantra::Comparison<mantra::C_EqualTo>::make("id", id_));
	if (rv.type() == typeid(mantra::NullValue))
		number = 1;
	else
		number = boost::get<boost::uint32_t>(rv) + 1;

	mantra::Storage::RecordMap rec;
	rec["id"] = id_;
	rec["number"] = number;
	rec["message"] = message;
	rec["last_updater"] = updater->Name();
	rec["last_updater_id"] = updater->User()->ID();
	if (Message::storage.InsertRow(rec))
	{
		Message m(self.lock(), number);
		MT_RET(m);
	}

	Message m;
	MT_RET(m);
	MT_EE
}

void Committee::MESSAGE_Del(boost::uint32_t num)
{
	MT_EB
	MT_FUNC("Committee::MESSAGE_Del" << num);

	Message::storage.RemoveRow(
				mantra::Comparison<mantra::C_EqualTo>::make("id", id_) &&
				mantra::Comparison<mantra::C_EqualTo>::make("number", num));

	MT_EE
}

Committee::Message Committee::MESSAGE_Get(boost::uint32_t num) const
{
	MT_EB
	MT_FUNC("Committee::MESSAGE_Get" << num);

	if (MESSAGE_Exists(num))
	{
		Message m(self.lock(), num);
		MT_RET(m);
	}

	Message m;
	MT_RET(m);

	MT_EE
}

void Committee::MESSAGE_Get(std::set<Committee::Message> &fill) const
{
	MT_EB
	MT_FUNC("Committee::MESSAGE_Get" << fill);

	mantra::Storage::DataSet data;
	mantra::Storage::FieldSet fields;
	fields.insert("number");

	Message::storage.RetrieveRow(data,
				mantra::Comparison<mantra::C_EqualTo>::make("id", id_),
				fields);

	mantra::Storage::DataSet::const_iterator i;
	for (i = data.begin(); i != data.end(); ++i)
	{
		mantra::Storage::RecordMap::const_iterator j = i->find("number");
		if (j == i->end() || j->second.type() == typeid(mantra::NullValue))
			continue;

		fill.insert(Message(self.lock(), boost::get<boost::uint32_t>(j->second)));
	}

	MT_EE
}

static void check_option(std::string &str, const mantra::Storage::RecordMap &data,
						 const std::string &key, const std::string &description)
{
	MT_EB
	MT_FUNC("check_option");

	if (ROOT->ConfigValue<bool>(("commserv.lock." + key).c_str()))
	{
		if (ROOT->ConfigValue<bool>(("commserv.defaults." + key).c_str()))
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
		else if (ROOT->ConfigValue<bool>(("commserv.defaults." + key).c_str()))
		{
			if (!str.empty())
				str += ", ";
			str += description;
		}
	}

	MT_EE
}

void Committee::SendInfo(const ServiceUser *service,
						 const boost::shared_ptr<LiveUser> &user) const
{
	MT_EB
	MT_FUNC("Committee::SendInfo" << service << user);

	if (!service || !user)
		return;

	bool opersop = (user->InCommittee(ROOT->ConfigValue<std::string>("commserv.oper.name")) ||
					user->InCommittee(ROOT->ConfigValue<std::string>("commserv.sop.name")));

	mantra::Storage::RecordMap data, last_data;
	mantra::Storage::RecordMap::const_iterator i;
	cache.Get(data);

	bool priv = false;
	if (!opersop && !IsMember(user))
	{
		priv = ROOT->ConfigValue<bool>("commserv.defaults.private");
		i = data.find("private");
		if (i != data.end() && !ROOT->ConfigValue<bool>("commserv.lock.private"))
			priv = boost::get<bool>(i->second);
	}

	SEND(service, user, N_("Information on committee \002%1%\017:"), name_);

	SEND(service, user, N_("Registered     : %1% ago"),
		 DurationToString(mantra::duration(boost::get<boost::posix_time::ptime>(data["registered"]),
										   mantra::GetCurrentDateTime()), mantra::Second));

	i = data.find("head_user");
	if (i != data.end())
	{
		boost::shared_ptr<StoredUser> stored = ROOT->data.Get_StoredUser(
			 boost::get<boost::uint32_t>(i->second));
		if (stored)
		{
			StoredUser::my_nicks_t nicks = stored->Nicks();
			std::string str;
			StoredUser::my_nicks_t::const_iterator j;
			for (j = nicks.begin(); j != nicks.end(); ++j)
			{
				if (!*j)
					continue;

				if (!str.empty())
					str += ", ";
				str += (*j)->Name();
			}
			SEND(service, user, N_("Head User      : %1%"), str);
		}
	}

	i = data.find("head_committee");
	if (i != data.end())
	{
		boost::shared_ptr<Committee> comm = ROOT->data.Get_Committee(
			 boost::get<boost::uint32_t>(i->second));
		if (comm)
			SEND(service, user, N_("Head Committee : %1%"), comm->Name());
	}

	if (!priv)
	{
		i = data.find("email");
		if (i != data.end())
			SEND(service, user, N_("E-Mail Address : %1%"),
				 boost::get<std::string>(i->second));
	}

	i = data.find("website");
	if (i != data.end())
		SEND(service, user, N_("Web Site       : %1%"),
			 boost::get<std::string>(i->second));

	i = data.find("description");
	if (i != data.end())
		SEND(service, user, N_("Description    : %1%"),
			 boost::get<std::string>(i->second));

	if (!priv)
	{
		std::string str;
		check_option(str, data, "secure", U_(user, "Secure"));
		check_option(str, data, "openmemos", U_(user, "Open Memos"));
		check_option(str, data, "private", U_(user, "Private"));
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

Committee::Member::Member()
	: cache(storage, ROOT->ConfigValue<mantra::duration>("storage.cache-expire")),
	  number_(0u)
{
	MT_EB
	MT_FUNC("Committee::Member::Member");


	MT_EE
}

Committee::Member::Member(const boost::shared_ptr<Committee> &owner,
						  boost::uint32_t number)
	: cache(storage, ROOT->ConfigValue<mantra::duration>("storage.cache-expire"),
			mantra::Comparison<mantra::C_EqualTo>::make("id", owner->ID()) &&
			mantra::Comparison<mantra::C_EqualTo>::make("number", number)),
	  owner_(owner), number_(number)
{
	MT_EB
	MT_FUNC("Committee::Member::Member" << owner << number);

	mantra::Storage::DataSet data;
	mantra::Storage::FieldSet fields;
	fields.insert("entry");

	storage.RetrieveRow(data,
			mantra::Comparison<mantra::C_EqualTo>::make("id", owner_->ID()) &&
			mantra::Comparison<mantra::C_EqualTo>::make("number", number_), fields);

	if (data.empty() || data.size() > 1)
		return;

	mantra::Storage::RecordMap::iterator i = data[0].find("entry");
	if (i == data[0].end())
		return;

	if (i->second.type() == typeid(mantra::NullValue))
		return;

	entry_ = ROOT->data.Get_StoredUser(boost::get<boost::uint32_t>(i->second));

	MT_EE
}

Committee::Member::Member(const boost::shared_ptr<Committee> &owner,
						  const boost::shared_ptr<StoredUser> &entry)
	: cache(storage, ROOT->ConfigValue<mantra::duration>("storage.cache-expire")),
	  owner_(owner), entry_(entry)
{
	MT_EB
	MT_FUNC("Committee::Member::Member" << owner << entry);

	mantra::Storage::DataSet data;
	mantra::Storage::FieldSet fields;
	fields.insert("number");

	storage.RetrieveRow(data,
			mantra::Comparison<mantra::C_EqualTo>::make("id", owner_->ID()) &&
			mantra::Comparison<mantra::C_EqualTo>::make("entry", entry->ID()), fields);

	if (data.empty() || data.size() > 1)
		return;

	mantra::Storage::RecordMap::iterator i = data[0].find("number");
	if (i == data[0].end())
		return;

	if (i->second.type() == typeid(mantra::NullValue))
		return;

	number_ = boost::get<boost::uint32_t>(i->second);
	cache.Search(mantra::Comparison<mantra::C_EqualTo>::make("id", owner_->ID()) &&
				 mantra::Comparison<mantra::C_EqualTo>::make("number", number_));

	MT_EE
}

std::string Committee::Member::Last_UpdaterName() const
{
	MT_EB
	MT_FUNC("Committee::Member::Last_UpdaterName");

	mantra::StorageValue rv = cache.Get("last_updater");

	std::string ret;
	if (rv.type() != typeid(mantra::NullValue))
		ret = boost::get<std::string>(rv);

	MT_RET(ret);
	MT_EE
}

boost::shared_ptr<StoredUser> Committee::Member::Last_Updater() const
{
	MT_EB
	MT_FUNC("Committee::Member::Last_Updater");

	mantra::StorageValue rv = cache.Get("last_updater_id");

	boost::shared_ptr<StoredUser> ret;
	if (rv.type() != typeid(mantra::NullValue))
		ret = ROOT->data.Get_StoredUser(boost::get<boost::uint32_t>(rv));

	MT_RET(ret);
	MT_EE
}

boost::posix_time::ptime Committee::Member::Last_Update() const
{
	MT_EB
	MT_FUNC("Committee::Member::Last_Update");

	mantra::StorageValue rv = cache.Get("last_update");

	boost::posix_time::ptime ret;
	if (rv.type() != typeid(mantra::NullValue))
		ret = boost::get<boost::posix_time::ptime>(rv);

	MT_RET(ret);
	MT_EE
}

Committee::Message::Message()
	: cache(storage, ROOT->ConfigValue<mantra::duration>("storage.cache-expire")),
	  number_(0)
{
	MT_EB
	MT_FUNC("Committee::Message::Message");


	MT_EE
}

Committee::Message::Message(const boost::shared_ptr<Committee> &owner,
							boost::uint32_t number)
	: cache(storage, ROOT->ConfigValue<mantra::duration>("storage.cache-expire"),
			mantra::Comparison<mantra::C_EqualTo>::make("id", owner->ID()) &&
			mantra::Comparison<mantra::C_EqualTo>::make("number", number)),
	  owner_(owner), number_(number)
{
	MT_EB
	MT_FUNC("Committee::Message::Message");


	MT_EE
}

void Committee::Message::Change(const std::string &entry,
								const boost::shared_ptr<StoredNick> &updater)
{
	MT_EB
	MT_FUNC("Committee::Message::Change" << entry << updater);

	mantra::Storage::RecordMap rec;
	rec["entry"] = entry;
	rec["last_updater"] = updater->Name();
	rec["last_updater_id"] = updater->User()->ID();
	rec["last_update"] = mantra::GetCurrentDateTime();

	cache.Put(rec);

	MT_EE
}

std::string Committee::Message::Entry() const
{
	MT_EB
	MT_FUNC("Committee::Message::Entry");

	mantra::StorageValue rv = cache.Get("entry");

	std::string ret;
	if (rv.type() != typeid(mantra::NullValue))
		ret = boost::get<std::string>(rv);

	MT_RET(ret);
	MT_EE
}

std::string Committee::Message::Last_UpdaterName() const
{
	MT_EB
	MT_FUNC("Committee::Message::Last_UpdaterName");

	mantra::StorageValue rv = cache.Get("last_updater");

	std::string ret;
	if (rv.type() != typeid(mantra::NullValue))
		ret = boost::get<std::string>(rv);

	MT_RET(ret);
	MT_EE
}

boost::shared_ptr<StoredUser> Committee::Message::Last_Updater() const
{
	MT_EB
	MT_FUNC("Committee::Message::Last_Updater");

	mantra::StorageValue rv = cache.Get("last_updater_id");

	boost::shared_ptr<StoredUser> ret;
	if (rv.type() != typeid(mantra::NullValue))
		ret = ROOT->data.Get_StoredUser(boost::get<boost::uint32_t>(rv));

	MT_RET(ret);
	MT_EE
}

boost::posix_time::ptime Committee::Message::Last_Update() const
{
	MT_EB
	MT_FUNC("Committee::Message::Last_Update");

	mantra::StorageValue rv = cache.Get("last_update");

	boost::posix_time::ptime ret;
	if (rv.type() != typeid(mantra::NullValue))
		ret = boost::get<boost::posix_time::ptime>(rv);

	MT_RET(ret);
	MT_EE
}

