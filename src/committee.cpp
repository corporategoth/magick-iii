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

StorageInterface Committee::storage("committees", "name", "last_update");
StorageInterface Committee::Member::storage("committees_member", std::string(), "last_update");
StorageInterface Committee::Message::storage("committees_message", std::string(), "last_update");

Committee::Committee(const std::string &name)
	: name_(name), SYNC_NRWINIT(online_members_, reader_priority)
{
	MT_EB
	MT_FUNC("Committee::Committee" << name);


	MT_EE
}

boost::shared_ptr<Committee> Committee::create(const std::string &name)
{
	MT_EB
	MT_FUNC("Committee::create" << name);

	mantra::Storage::RecordMap rec;
	rec["name"] = name;
	storage.InsertRow(rec);

	boost::shared_ptr<Committee> rv = load(name);

	MT_RET(rv);
	MT_EE
}

boost::shared_ptr<Committee> Committee::create(const std::string &name,
							const boost::shared_ptr<Committee> &head)
{
	MT_EB
	MT_FUNC("Committee::create" << name << head);

	mantra::Storage::RecordMap rec;
	rec["name"] = name;
	rec["head_committee"] = head->Name();
	storage.InsertRow(rec);

	boost::shared_ptr<Committee> rv = load(name);

	MT_RET(rv);
	MT_EE
}

boost::shared_ptr<Committee> Committee::create(const std::string &name,
							const boost::shared_ptr<StoredUser> &head)
{
	MT_EB
	MT_FUNC("Committee::create" << name << head);

	mantra::Storage::RecordMap rec;
	rec["name"] = name;
	rec["head_user"] = head->ID();
	storage.InsertRow(rec);

	boost::shared_ptr<Committee> rv = load(name);

	MT_RET(rv);
	MT_EE
}

boost::shared_ptr<Committee> Committee::load(const std::string &name)
{
	MT_EB
	MT_FUNC("Committee::load" << name);

	boost::shared_ptr<Committee> rv(new Committee(name));
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
	fields.insert("name");
	storage.RetrieveRow(data, mantra::Comparison<mantra::C_EqualTo>::make("head_user", in->ID()), fields);

	mantra::Storage::DataSet::const_iterator i = data.begin();
	for (i = data.begin(); i != data.end(); ++i)
	{
		mantra::Storage::RecordMap::const_iterator j = i->find("name");
		if (j == i->end() || j->second.type() == typeid(mantra::NullValue))
			continue;

		boost::shared_ptr<Committee> comm = ROOT->data.Get_Committee(
						boost::get<std::string>(j->second));
		if (comm)
			rv.insert(comm);
	}

	data.clear();
	Member::storage.RetrieveRow(data, mantra::Comparison<mantra::C_EqualTo>::make("entry", in->ID()), fields);
	for (i = data.begin(); i != data.end(); ++i)
	{
		mantra::Storage::RecordMap::const_iterator j = i->find("name");
		if (j == i->end() || j->second.type() == typeid(mantra::NullValue))
			continue;

		boost::shared_ptr<Committee> comm = ROOT->data.Get_Committee(
						boost::get<std::string>(j->second));
		if (comm)
			rv.insert(comm);
	}

	std::vector<mantra::StorageValue> look;
	std::set<boost::shared_ptr<Committee> >::iterator k;
	for (k = rv.begin(); k != rv.end(); ++k)
		look.push_back((*k)->Name());

	while (!look.empty())
	{
		data.clear();
		storage.RetrieveRow(data, mantra::Comparison<mantra::C_OneOfNC>::make("head_committee", look), fields);
		look.clear();
		for (i = data.begin(); i != data.end(); ++i)
		{
			mantra::Storage::RecordMap::const_iterator j = i->find("name");
			if (j == i->end() || j->second.type() == typeid(mantra::NullValue))
				continue;

			boost::shared_ptr<Committee> comm = ROOT->data.Get_Committee(
							boost::get<std::string>(j->second));
			if (comm)
			{
				look.push_back(comm->Name());
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

	mantra::Storage::RecordMap rec;
	rec["head_committee"] = mantra::NullValue();
	rec["head_user"] = head->ID();
	storage.ChangeRow(rec, mantra::Comparison<mantra::C_EqualToNC>::make("name", name_));

	MT_EE
}

void Committee::Head(const boost::shared_ptr<Committee> &head)
{
	MT_EB
	MT_FUNC("Committee::Head" << head);

	mantra::Storage::RecordMap rec;
	rec["head_committee"] = head->Name();
	rec["head_user"] = mantra::NullValue();
	storage.ChangeRow(rec, mantra::Comparison<mantra::C_EqualToNC>::make("name", name_));

	MT_EE
}

boost::shared_ptr<StoredUser> Committee::HeadUser() const
{
	MT_EB
	MT_FUNC("HeadUser");

	boost::shared_ptr<StoredUser> ret;
	mantra::StorageValue rv = storage.GetField(name_, "head_user");
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
	mantra::StorageValue rv = storage.GetField(name_, "head_committee");
	if (rv.type() == typeid(mantra::NullValue))
		MT_RET(ret);
	ret = ROOT->data.Get_Committee(boost::get<std::string>(rv));

	MT_RET(ret);
	MT_EE
}

void Committee::Description(const std::string &in)
{
	MT_EB
	MT_FUNC("Committee::Description" << in);

	storage.PutField(name_, "description", in);

	MT_EE
}

std::string Committee::Description() const
{
	MT_EB
	MT_FUNC("Committee::Description");

	std::string ret;
	mantra::StorageValue rv = storage.GetField(name_, "description");
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
		storage.PutField(name_, "email", mantra::NullValue());
	else
		storage.PutField(name_, "email", in);

	MT_EE
}

std::string Committee::Email() const
{
	MT_EB
	MT_FUNC("Committee::Email");

	std::string ret;
	mantra::StorageValue rv = storage.GetField(name_, "email");
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
		storage.PutField(name_, "website", mantra::NullValue());
	else
		storage.PutField(name_, "website", in);

	MT_EE
}

std::string Committee::Website() const
{
	MT_EB
	MT_FUNC("Committee::Website");

	std::string ret;
	mantra::StorageValue rv = storage.GetField(name_, "website");
	if (rv.type() == typeid(mantra::NullValue))
		MT_RET(ret);
	ret = boost::get<std::string>(rv);

	MT_RET(ret);
	MT_EE
}

void Committee::Private(const boost::logic::tribool &in)
{
	MT_EB
	MT_FUNC("Committee::Private" << in);

	if (LOCK_Private())
		return;

	if (boost::logic::indeterminate(in))
		storage.PutField(name_, "private", mantra::NullValue::instance());
	else
		storage.PutField(name_, "private", (bool) in);

	MT_EE
}

bool Committee::Private() const
{
	MT_EB
	MT_FUNC("Committee::Private");

	bool ret;
	mantra::StorageValue rv = storage.GetField(name_, "private");
	if (rv.type() == typeid(mantra::NullValue))
		ret = ROOT->ConfigValue<bool>("commserv.defaults.private");
	else
		ret = boost::get<bool>(rv);

	MT_RET(ret);
	MT_EE
}

void Committee::OpenMemos(const boost::logic::tribool &in)
{
	MT_EB
	MT_FUNC("Committee::OpenMemos" << in);

	if (LOCK_OpenMemos())
		return;

	if (boost::logic::indeterminate(in))
		storage.PutField(name_, "openmemos", mantra::NullValue::instance());
	else
		storage.PutField(name_, "openmemos", (bool) in);

	MT_EE
}

bool Committee::OpenMemos() const
{
	MT_EB
	MT_FUNC("Committee::OpenMemos");

	bool ret;
	mantra::StorageValue rv = storage.GetField(name_, "openmemos");
	if (rv.type() == typeid(mantra::NullValue))
		ret = ROOT->ConfigValue<bool>("commserv.defaults.openmemos");
	else
		ret = boost::get<bool>(rv);

	MT_RET(ret);
	MT_EE
}

void Committee::Secure(const boost::logic::tribool &in)
{
	MT_EB
	MT_FUNC("Committee::Secure" << in);

	if (LOCK_Secure())
		return;

	if (boost::logic::indeterminate(in))
		storage.PutField(name_, "secure", mantra::NullValue::instance());
	else
		storage.PutField(name_, "secure", (bool) in);

	MT_EE
}

bool Committee::Secure() const
{
	MT_EB
	MT_FUNC("Committee::Secure");

	bool ret;
	mantra::StorageValue rv = storage.GetField(name_, "secure");
	if (rv.type() == typeid(mantra::NullValue))
		ret = ROOT->ConfigValue<bool>("commserv.defaults.secure");
	else
		ret = boost::get<bool>(rv);

	MT_RET(ret);
	MT_EE
}

void Committee::LOCK_Private(const bool &in)
{
	MT_EB
	MT_FUNC("Committee::LOCK_Private" << in);

	if (ROOT->ConfigValue<bool>("commserv.lock.private"))
		return;

	storage.PutField(name_, "lock_private", in);

	MT_EE
}

bool Committee::LOCK_Private() const
{
	MT_EB
	MT_FUNC("Committee::LOCK_Private");

	if (ROOT->ConfigValue<bool>("commserv.lock.private"))
		MT_RET(true);

	mantra::StorageValue rv = storage.GetField(name_, "lock_private");
	if (rv.type() == typeid(mantra::NullValue))
		MT_RET(false);
	bool ret = boost::get<bool>(rv);

	MT_RET(ret);
	MT_EE
}

void Committee::LOCK_OpenMemos(const bool &in)
{
	MT_EB
	MT_FUNC("Committee::LOCK_OpenMemos" << in);

	if (ROOT->ConfigValue<bool>("commserv.lock.openmemos"))
		return;

	storage.PutField(name_, "lock_openmemos", in);

	MT_EE
}

bool Committee::LOCK_OpenMemos() const
{
	MT_EB
	MT_FUNC("Committee::LOCK_OpenMemos");

	if (ROOT->ConfigValue<bool>("commserv.lock.openmemos"))
		MT_RET(true);

	mantra::StorageValue rv = storage.GetField(name_, "lock_openmemos");
	if (rv.type() == typeid(mantra::NullValue))
		MT_RET(false);
	bool ret = boost::get<bool>(rv);

	MT_RET(ret);
	MT_EE
}

void Committee::LOCK_Secure(const bool &in)
{
	MT_EB
	MT_FUNC("Committee::LOCK_Secure" << in);

	if (ROOT->ConfigValue<bool>("commserv.lock.secure"))
		return;

	storage.PutField(name_, "lock_secure", in);

	MT_EE
}

bool Committee::LOCK_Secure() const
{
	MT_EB
	MT_FUNC("Committee::LOCK_Secure");

	if (ROOT->ConfigValue<bool>("commserv.lock.secure"))
		MT_RET(true);

	mantra::StorageValue rv = storage.GetField(name_, "lock_secure");
	if (rv.type() == typeid(mantra::NullValue))
		MT_RET(false);
	bool ret = boost::get<bool>(rv);

	MT_RET(ret);
	MT_EE
}

bool Committee::IsMember(const boost::shared_ptr<LiveUser> &user) const
{
	MT_EB
	MT_FUNC("Committee::IsMember" << user);

	bool rv = false;
    boost::shared_ptr<StoredNick> nick = user->Stored();
    if (nick)
        rv = MEMBER_Exists(nick->User());

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



bool Committee::MEMBER_Exists(const boost::shared_ptr<StoredUser> &entry) const
{
	MT_EB
	MT_FUNC("Committee::MEMBER_Exists" << entry);

	bool rv = (Member::storage.RowExists(
				mantra::Comparison<mantra::C_EqualToNC>::make("name", name_) &&
				mantra::Comparison<mantra::C_EqualTo>::make("entry", entry->ID())));

	MT_RET(rv);
	MT_EE
}

Committee::Member Committee::MEMBER_Add(const boost::shared_ptr<StoredUser> &entry,
										const boost::shared_ptr<StoredNick> &updater)
{
	MT_EB
	MT_FUNC("Committee::MEMBER_Add" << entry << updater);

	mantra::Storage::RecordMap rec;
	rec["name"] = name_;
	rec["entry"] = entry->ID();
	rec["last_updater"] = updater->Name();
	rec["last_updater_id"] = updater->User()->ID();
	if (Member::storage.InsertRow(rec))
	{
		Member m(self.lock(), entry);
		MT_RET(m);
	}

	Member m;
	MT_RET(m);
	MT_EE
}

void Committee::MEMBER_Del(const boost::shared_ptr<StoredUser> &entry)
{
	MT_EB
	MT_FUNC("Committee::MEMBER_Del" << entry);

	Member::storage.RemoveRow(
				mantra::Comparison<mantra::C_EqualToNC>::make("name", name_) &&
				mantra::Comparison<mantra::C_EqualTo>::make("entry", entry->ID()));

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

void Committee::MEMBER_Fill(std::set<Committee::Member> &fill) const
{
	MT_EB
	MT_FUNC("Committee::MEMBER_Fill" << fill);

	std::set<Member> rv;

	mantra::Storage::DataSet data;
	mantra::Storage::FieldSet fields;
	fields.insert("entry");

	Member::storage.RetrieveRow(data,
				mantra::Comparison<mantra::C_EqualToNC>::make("name", name_),
				fields);

	mantra::Storage::DataSet::const_iterator i = data.begin();
	for (i = data.begin(); i != data.end(); ++i)
	{
		mantra::Storage::RecordMap::const_iterator j = i->find("entry");
		if (j == i->end() || j->second.type() == typeid(mantra::NullValue))
			continue;

		boost::shared_ptr<StoredUser> user = ROOT->data.Get_StoredUser(
								boost::get<boost::uint32_t>(j->second));
		if (!user)
			continue;

		rv.insert(Member(self.lock(), user));
	}

	MT_EE
}


bool Committee::MESSAGE_Exists(boost::uint32_t num) const
{
	MT_EB
	MT_FUNC("Committee::MESSAGE_Exists" << num);

	bool rv = (Message::storage.RowExists(
				mantra::Comparison<mantra::C_EqualToNC>::make("name", name_) &&
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
				mantra::Comparison<mantra::C_EqualToNC>::make("name", name_));
	if (rv.type() == typeid(mantra::NullValue))
		number = 1;
	else
		number = boost::get<boost::uint32_t>(rv) + 1;

	mantra::Storage::RecordMap rec;
	rec["name"] = name_;
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

	Member::storage.RemoveRow(
				mantra::Comparison<mantra::C_EqualToNC>::make("name", name_) &&
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

	std::set<Message> rv;

	mantra::Storage::DataSet data;
	mantra::Storage::FieldSet fields;
	fields.insert("number");

	Member::storage.RetrieveRow(data,
				mantra::Comparison<mantra::C_EqualToNC>::make("name", name_),
				fields);

	mantra::Storage::DataSet::const_iterator i = data.begin();
	for (i = data.begin(); i != data.end(); ++i)
	{
		mantra::Storage::RecordMap::const_iterator j = i->find("number");
		if (j == i->end() || j->second.type() == typeid(mantra::NullValue))
			continue;

		rv.insert(Message(self.lock(), boost::get<boost::uint32_t>(j->second)));
	}

	MT_EE
}


