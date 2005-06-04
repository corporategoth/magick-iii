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
#define RCSID(x,y) const char *rcsid_magick__storeduser_cpp_ ## x () { return y; }
RCSID(magick__storeduser_cpp, "@(#)$Id$");

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
#include "storeduser.h"
#include "storednick.h"
#include "liveuser.h"
#include "storedchannel.h"
#include "committee.h"

#include <mantra/core/trace.h>

StorageInterface StoredUser::storage("users", "id", "last_update");
StorageInterface StoredUser::storage_access("users_access", std::string(), "last_update");
StorageInterface StoredUser::storage_ignore("users_ignore", std::string(), "last_update");

StoredUser::StoredUser(boost::uint32_t id)
	: id_(id), SYNC_NRWINIT(online_users_, reader_priority)
{
	MT_EB
	MT_FUNC("StoredUser::StoredUser");


	MT_EE
}

boost::shared_ptr<StoredUser> StoredUser::create(const std::string &password)
{
	MT_EB
	MT_FUNC("StoredUser::create" << password);

	static boost::mutex id_lock;

	boost::uint32_t id;
	{
		boost::mutex::scoped_lock sl(id_lock);
		mantra::StorageValue rv = storage.Maximum("id");
		if (rv.type() == typeid(mantra::NullValue))
			id = 1;
		else
			id = boost::get<boost::uint32_t>(rv) + 1;

		mantra::Storage::RecordMap rec;
		rec["id"] = id;
		rec["password"] = ROOT->data.CryptPassword(password);
		storage.InsertRow(rec);
	}

	boost::shared_ptr<StoredUser> rv = load(id);

	MT_RET(rv);
	MT_EE
}

boost::shared_ptr<StoredUser> StoredUser::load(boost::uint32_t id)
{
	MT_EB
	MT_FUNC("StoredUser::load");

	boost::shared_ptr<StoredUser> rv(new StoredUser(id));
	rv->self = rv;
	MT_RET(rv);

	MT_EE
}

/** 
 * @brief Set the downloaded picture in the database.
 * This may only be called from the DCC class after the file has been
 * completed.  To un-set the image, you muse use StoredUser::ClearPicture().
 * 
 * @param in ID of the picture we have downloaded.
 */
void StoredUser::Picture(boost::uint32_t in, const std::string &ext)
{
	MT_EB
	MT_FUNC("StoredUser::Picture" << in << ext);

	mantra::Storage::RecordMap rec;
	rec["picture"] = in;
	rec["picture_ext"] = ext;
	storage.ChangeRow(rec, mantra::Comparison<mantra::C_EqualTo>::make("id", id_));

	MT_EE
}

void StoredUser::Online(const boost::shared_ptr<LiveUser> &user)
{
	MT_EB
	MT_FUNC("StoredUser::Online" << user);

	SYNC_WLOCK(online_users_);
	online_users_.insert(user);

	MT_EE
}

void StoredUser::Offline(const boost::shared_ptr<LiveUser> &user)
{
	MT_EB
	MT_FUNC("StoredUser::Offline" << user);

	SYNC_WLOCK(online_users_);
	online_users_.erase(user);

	MT_EE
}

void StoredUser::Add(const boost::shared_ptr<StoredNick> &nick)
{
	MT_EB
	MT_FUNC("StoredUser::Add" << nick);

	SYNC_LOCK(my_nicks_);
	my_nicks_.insert(nick);

	MT_EE
}

void StoredUser::Del(const boost::shared_ptr<StoredNick> &nick)
{
	MT_EB
	MT_FUNC("StoredUser::Del" << nick);

	SYNC_LOCK(my_nicks_);
	my_nicks_.erase(nick);
	if (my_nicks_.empty())
		if_StorageDeleter<StoredUser>(ROOT->data).Del(self.lock());

	MT_EE
}

void StoredUser::Add(const boost::shared_ptr<StoredChannel> &channel)
{
	MT_EB
	MT_FUNC("StoredUser::Add" << channel);

	SYNC_LOCK(my_channels_);
	my_channels_.insert(channel);

	MT_EE
}

void StoredUser::Del(const boost::shared_ptr<StoredChannel> &channel)
{
	MT_EB
	MT_FUNC("StoredUser::Del" << channel);

	SYNC_LOCK(my_channels_);
	my_channels_.erase(channel);

	MT_EE
}

boost::shared_ptr<StoredNick> StoredUser::Last_Online() const
{
	MT_EB
	MT_FUNC("StoredUser::Last_Online");

	boost::shared_ptr<StoredNick> rv = if_StoredNick_StoredUser::Last_Seen(self.lock());

	MT_RET(rv);
	MT_EE
}

StoredUser::online_users_t StoredUser::Online() const
{
	MT_EB
	MT_FUNC("StoredUser::Online");

	SYNC_RLOCK(online_users_);

	MT_RET(online_users_);
	MT_EE
}

StoredUser::my_nicks_t StoredUser::Nicks() const
{
	MT_EB
	MT_FUNC("StoredUser::Nicks");

	SYNC_LOCK(my_nicks_);

	MT_RET(my_nicks_);
	MT_EE
}

void StoredUser::Password(const std::string &password)
{
	MT_EB
	MT_FUNC("StoredUser::Password" << password);

	storage.PutField(id_, "password",
					 ROOT->data.CryptPassword(password));

	MT_EE
}

bool StoredUser::CheckPassword(const std::string &password) const
{
	MT_EB
	MT_FUNC("StoredUser::CheckPassword" << password);

	mantra::StorageValue rv = storage.GetField(id_, "password");
	if (rv.type() == typeid(mantra::NullValue))
		MT_RET(false);
	bool ret = (boost::get<std::string>(rv) == ROOT->data.CryptPassword(password));

	MT_RET(ret);
	MT_EE
}

void StoredUser::Email(const std::string &in)
{
	MT_EB
	MT_FUNC("StoredUser::Email" << in);

	if (in.empty())
		storage.PutField(id_, "email", mantra::NullValue());
	else
		storage.PutField(id_, "email", in);

	MT_EE
}

std::string StoredUser::Email() const
{
	MT_EB
	MT_FUNC("StoredUser::Email");

	std::string ret;
	mantra::StorageValue rv = storage.GetField(id_, "email");
	if (rv.type() == typeid(mantra::NullValue))
		MT_RET(ret);
	ret = boost::get<std::string>(rv);

	MT_RET(ret);
	MT_EE
}

void StoredUser::Website(const std::string &in)
{
	MT_EB
	MT_FUNC("StoredUser::Website" << in);

	if (in.empty())
		storage.PutField(id_, "website", mantra::NullValue());
	else
		storage.PutField(id_, "website", in);

	MT_EE
}

std::string StoredUser::Website() const
{
	MT_EB
	MT_FUNC("StoredUser::Website");

	std::string ret;
	mantra::StorageValue rv = storage.GetField(id_, "website");
	if (rv.type() == typeid(mantra::NullValue))
		MT_RET(ret);
	ret = boost::get<std::string>(rv);

	MT_RET(ret);
	MT_EE
}

void StoredUser::ICQ(const boost::uint32_t &in)
{
	MT_EB
	MT_FUNC("StoredUser::ICQ" << in);

	if (!in)
		storage.PutField(id_, "icq", mantra::NullValue());
	else
		storage.PutField(id_, "icq", in);

	MT_EE
}

boost::uint32_t StoredUser::ICQ() const
{
	MT_EB
	MT_FUNC("StoredUser::ICQ");

	boost::uint32_t ret;
	mantra::StorageValue rv = storage.GetField(id_, "icq");
	if (rv.type() == typeid(mantra::NullValue))
		MT_RET(0u);
	ret = boost::get<boost::uint32_t>(rv);

	MT_RET(ret);
	MT_EE
}

void StoredUser::AIM(const std::string &in)
{
	MT_EB
	MT_FUNC("StoredUser::AIM" << in);

	if (in.empty())
		storage.PutField(id_, "aim", mantra::NullValue());
	else
		storage.PutField(id_, "aim", in);

	MT_EE
}

std::string StoredUser::AIM() const
{
	MT_EB
	MT_FUNC("StoredUser::AIM");

	std::string ret;
	mantra::StorageValue rv = storage.GetField(id_, "aim");
	if (rv.type() == typeid(mantra::NullValue))
		MT_RET(ret);
	ret = boost::get<std::string>(rv);

	MT_RET(ret);
	MT_EE
}

void StoredUser::MSN(const std::string &in)
{
	MT_EB
	MT_FUNC("StoredUser::MSN" << in);

	if (in.empty())
		storage.PutField(id_, "msn", mantra::NullValue());
	else
		storage.PutField(id_, "msn", in);

	MT_EE
}

std::string StoredUser::MSN() const
{
	MT_EB
	MT_FUNC("StoredUser::MSN");

	std::string ret;
	mantra::StorageValue rv = storage.GetField(id_, "msn");
	if (rv.type() == typeid(mantra::NullValue))
		MT_RET(ret);
	ret = boost::get<std::string>(rv);

	MT_RET(ret);
	MT_EE
}

void StoredUser::Jabber(const std::string &in)
{
	MT_EB
	MT_FUNC("StoredUser::Jabber" << in);

	if (in.empty())
		storage.PutField(id_, "jabber", mantra::NullValue());
	else
		storage.PutField(id_, "jabber", in);

	MT_EE
}

std::string StoredUser::Jabber() const
{
	MT_EB
	MT_FUNC("StoredUser::Jabber");

	std::string ret;
	mantra::StorageValue rv = storage.GetField(id_, "jabber");
	if (rv.type() == typeid(mantra::NullValue))
		MT_RET(ret);
	ret = boost::get<std::string>(rv);

	MT_RET(ret);
	MT_EE
}

void StoredUser::Yahoo(const std::string &in)
{
	MT_EB
	MT_FUNC("StoredUser::Yahoo" << in);

	if (in.empty())
		storage.PutField(id_, "yahoo", mantra::NullValue());
	else
		storage.PutField(id_, "yahoo", in);

	MT_EE
}

std::string StoredUser::Yahoo() const
{
	MT_EB
	MT_FUNC("StoredUser::Yahoo");

	std::string ret;
	mantra::StorageValue rv = storage.GetField(id_, "yahoo");
	if (rv.type() == typeid(mantra::NullValue))
		MT_RET(ret);
	ret = boost::get<std::string>(rv);

	MT_RET(ret);
	MT_EE
}

void StoredUser::Description(const std::string &in)
{
	MT_EB
	MT_FUNC("StoredUser::Description" << in);

	if (in.empty())
		storage.PutField(id_, "description", mantra::NullValue());
	else
		storage.PutField(id_, "description", in);

	MT_EE
}

std::string StoredUser::Description() const
{
	MT_EB
	MT_FUNC("StoredUser::Description");

	std::string ret;
	mantra::StorageValue rv = storage.GetField(id_, "description");
	if (rv.type() == typeid(mantra::NullValue))
		MT_RET(ret);
	ret = boost::get<std::string>(rv);

	MT_RET(ret);
	MT_EE
}

void StoredUser::Comment(const std::string &in)
{
	MT_EB
	MT_FUNC("StoredUser::Comment" << in);

	if (in.empty())
		storage.PutField(id_, "comment", mantra::NullValue());
	else
		storage.PutField(id_, "comment", in);

	MT_EE
}

std::string StoredUser::Comment() const
{
	MT_EB
	MT_FUNC("StoredUser::Comment");

	std::string ret;
	mantra::StorageValue rv = storage.GetField(id_, "comment");
	if (rv.type() == typeid(mantra::NullValue))
		MT_RET(ret);
	ret = boost::get<std::string>(rv);

	MT_RET(ret);
	MT_EE
}

void StoredUser::Suspend(const boost::shared_ptr<StoredNick> &nick,
						 const std::string &reason)
{
	MT_EB
	MT_FUNC("StoredUser::Suspend" << nick << reason);

	mantra::Storage::RecordMap rec;
	rec["suspend_by"] = nick->Name();
	rec["suspend_by_id"] = nick->User()->ID();
	rec["suspend_reason"] = reason;
	rec["suspend_time"] = mantra::GetCurrentDateTime();
	storage.ChangeRow(rec, mantra::Comparison<mantra::C_EqualTo>::make("id", id_));

	MT_EE
}

void StoredUser::Unsuspend()
{
	MT_EB
	MT_FUNC("StoredUser::Unsuspend");

	mantra::Storage::RecordMap rec;
	rec["suspend_by"] = mantra::NullValue();
	rec["suspend_by_id"] = mantra::NullValue();
	rec["suspend_reason"] = mantra::NullValue();
	rec["suspend_time"] = mantra::NullValue();
	storage.ChangeRow(rec, mantra::Comparison<mantra::C_EqualTo>::make("id", id_));

	MT_EE
}

std::string StoredUser::Suspended_By() const
{
	MT_EB
	MT_FUNC("StoredUser::Suspended_By");

	std::string ret;
	mantra::StorageValue rv = storage.GetField(id_, "suspend_by");
	if (rv.type() == typeid(mantra::NullValue))
		MT_RET(ret);
	ret = boost::get<std::string>(rv);

	MT_RET(ret);
	MT_EE
}

boost::shared_ptr<StoredUser> StoredUser::Suspended_ByShared() const
{
	MT_EB
	MT_FUNC("StoredUser::Suspended_ByShared");

	boost::shared_ptr<StoredUser> ret;
	mantra::StorageValue rv = storage.GetField(id_, "suspend_by_id");
	if (rv.type() == typeid(mantra::NullValue))
		MT_RET(ret);
	ret = ROOT->data.Get_StoredUser(boost::get<boost::uint32_t>(rv));

	MT_RET(ret);
	MT_EE
}

std::string StoredUser::Suspend_Reason() const
{
	MT_EB
	MT_FUNC("StoredUser::Suspend_Reason");

	std::string ret;
	mantra::StorageValue rv = storage.GetField(id_, "suspend_reason");
	if (rv.type() == typeid(mantra::NullValue))
		MT_RET(ret);
	ret = boost::get<std::string>(rv);

	MT_RET(ret);
	MT_EE
}

boost::posix_time::ptime StoredUser::Suspend_Time() const
{
	MT_EB
	MT_FUNC("StoredUser::Suspend_Time");

	boost::posix_time::ptime ret(boost::date_time::not_a_date_time);
	mantra::StorageValue rv = storage.GetField(id_, "suspend_time");
	if (rv.type() == typeid(mantra::NullValue))
		MT_RET(ret);
	ret = boost::get<boost::posix_time::ptime>(rv);

	MT_RET(ret);
	MT_EE
}

bool StoredUser::Language(const std::string &in)
{
	MT_EB
	MT_FUNC("StoredUser::Language" << in);

	if (LOCK_Language())
		MT_RET(false);

	if (in.empty())
		storage.PutField(id_, "language", mantra::NullValue::instance());
	else
		storage.PutField(id_, "language", in);

	MT_RET(true);
	MT_EE
}

std::string StoredUser::Language() const
{
	MT_EB
	MT_FUNC("StoredUser::Language");

	std::string ret;
	mantra::StorageValue rv = storage.GetField(id_, "language");
	if (rv.type() == typeid(mantra::NullValue))
		MT_RET(ret);
	ret = boost::get<std::string>(rv);

	MT_RET(ret);
	MT_EE
}

bool StoredUser::Protect(const boost::logic::tribool &in)
{
	MT_EB
	MT_FUNC("StoredUser::Protect" << in);

	if (LOCK_Protect())
		MT_RET(false);

	if (boost::logic::indeterminate(in))
		storage.PutField(id_, "protect", mantra::NullValue::instance());
	else
		storage.PutField(id_, "protect", (bool) in);

	MT_RET(true);
	MT_EE
}

bool StoredUser::Protect() const
{
	MT_EB
	MT_FUNC("StoredUser::Protect");

	bool ret;
	mantra::StorageValue rv = storage.GetField(id_, "protect");
	if (rv.type() == typeid(mantra::NullValue))
		ret = ROOT->ConfigValue<bool>("nickserv.defaults.protect");
	else
		ret = boost::get<bool>(rv);

	MT_RET(ret);
	MT_EE
}

bool StoredUser::Secure(const boost::logic::tribool &in)
{
	MT_EB
	MT_FUNC("StoredUser::Secure" << in);

	if (LOCK_Secure())
		MT_RET(false);

	if (boost::logic::indeterminate(in))
		storage.PutField(id_, "secure", mantra::NullValue::instance());
	else
		storage.PutField(id_, "secure", (bool) in);

	MT_RET(true);
	MT_EE
}

bool StoredUser::Secure() const
{
	MT_EB
	MT_FUNC("StoredUser::Secure");

	bool ret;
	mantra::StorageValue rv = storage.GetField(id_, "secure");
	if (rv.type() == typeid(mantra::NullValue))
		ret = ROOT->ConfigValue<bool>("nickserv.defaults.secure");
	else
		ret = boost::get<bool>(rv);

	MT_RET(ret);
	MT_EE
}

bool StoredUser::NoMemo(const boost::logic::tribool &in)
{
	MT_EB
	MT_FUNC("StoredUser::NoMemo" << in);

	if (LOCK_NoMemo())
		MT_RET(false);

	if (boost::logic::indeterminate(in))
		storage.PutField(id_, "nomemo", mantra::NullValue::instance());
	else
		storage.PutField(id_, "nomemo", (bool) in);

	MT_RET(true);
	MT_EE
}

bool StoredUser::NoMemo() const
{
	MT_EB
	MT_FUNC("StoredUser::NoMemo");

	bool ret;
	mantra::StorageValue rv = storage.GetField(id_, "nomemo");
	if (rv.type() == typeid(mantra::NullValue))
		ret = ROOT->ConfigValue<bool>("nickserv.defaults.nomemo");
	else
		ret = boost::get<bool>(rv);

	MT_RET(ret);
	MT_EE
}

bool StoredUser::Private(const boost::logic::tribool &in)
{
	MT_EB
	MT_FUNC("StoredUser::Private" << in);

	if (LOCK_Private())
		MT_RET(false);

	if (boost::logic::indeterminate(in))
		storage.PutField(id_, "private", mantra::NullValue::instance());
	else
		storage.PutField(id_, "private", (bool) in);

	MT_RET(true);
	MT_EE
}

bool StoredUser::Private() const
{
	MT_EB
	MT_FUNC("StoredUser::Private");

	bool ret;
	mantra::StorageValue rv = storage.GetField(id_, "private");
	if (rv.type() == typeid(mantra::NullValue))
		ret = ROOT->ConfigValue<bool>("nickserv.defaults.private");
	else
		ret = boost::get<bool>(rv);

	MT_RET(ret);
	MT_EE
}

bool StoredUser::PRIVMSG(const boost::logic::tribool &in)
{
	MT_EB
	MT_FUNC("StoredUser::PRIVMSG" << in);

	if (LOCK_PRIVMSG())
		MT_RET(false);

	if (boost::logic::indeterminate(in))
		storage.PutField(id_, "privmsg", mantra::NullValue::instance());
	else
		storage.PutField(id_, "privmsg", (bool) in);

	MT_RET(true);
	MT_EE
}

bool StoredUser::PRIVMSG() const
{
	MT_EB
	MT_FUNC("StoredUser::PRIVMSG");

	bool ret;
	mantra::StorageValue rv = storage.GetField(id_, "privmsg");
	if (rv.type() == typeid(mantra::NullValue))
		ret = ROOT->ConfigValue<bool>("nickserv.defaults.privmsg");
	else
		ret = boost::get<bool>(rv);

	MT_RET(ret);
	MT_EE
}

bool StoredUser::NoExpire(const boost::logic::tribool &in)
{
	MT_EB
	MT_FUNC("StoredUser::NoExpire" << in);

	if (ROOT->ConfigValue<bool>("nickserv.lock.noexpire"))
		MT_RET(false);

	if (boost::logic::indeterminate(in))
		storage.PutField(id_, "noexpire", mantra::NullValue::instance());
	else
		storage.PutField(id_, "noexpire", (bool) in);

	MT_RET(true);
	MT_EE
}

bool StoredUser::NoExpire() const
{
	MT_EB
	MT_FUNC("StoredUser::NoExpire");

	bool ret;
	mantra::StorageValue rv = storage.GetField(id_, "noexpire");
	if (rv.type() == typeid(mantra::NullValue))
		ret = ROOT->ConfigValue<bool>("nickserv.defaults.noexpire");
	else
		ret = boost::get<bool>(rv);

	MT_RET(ret);
	MT_EE
}

void StoredUser::ClearPicture()
{
	MT_EB
	MT_FUNC("StoredUser::ClearPicture");

	mantra::Storage::RecordMap rec;
	rec["picture"] = mantra::NullValue();
	rec["picture_ext"] = mantra::NullValue();
	storage.ChangeRow(rec, mantra::Comparison<mantra::C_EqualTo>::make("id", id_));

	MT_EE
}

std::string StoredUser::PictureExt() const
{
	MT_EB
	MT_FUNC("StoredUser::PictureExt");

	std::string ret;
	mantra::StorageValue rv = storage.GetField(id_, "picture_ext");
	if (rv.type() == typeid(mantra::NullValue))
		MT_RET(ret);
	ret = boost::get<std::string>(rv);

	MT_RET(ret);
	MT_EE
}

bool StoredUser::LOCK_Language(const bool &in)
{
	MT_EB
	MT_FUNC("StoredUser::LOCK_Language" << in);

	if (ROOT->ConfigValue<bool>("nickserv.lock.language"))
		MT_RET(false);

	storage.PutField(id_, "lock_language", in);

	MT_RET(true);
	MT_EE
}

bool StoredUser::LOCK_Language() const
{
	MT_EB
	MT_FUNC("StoredUser::LOCK_Language");

	if (ROOT->ConfigValue<bool>("nickserv.lock.language"))
		MT_RET(true);

	mantra::StorageValue rv = storage.GetField(id_, "lock_language");
	if (rv.type() == typeid(mantra::NullValue))
		MT_RET(false);
	bool ret = boost::get<bool>(rv);

	MT_RET(ret);
	MT_EE
}

bool StoredUser::LOCK_Protect(const bool &in)
{
	MT_EB
	MT_FUNC("StoredUser::LOCK_Protect" << in);

	if (ROOT->ConfigValue<bool>("nickserv.lock.protect"))
		MT_RET(false);

	storage.PutField(id_, "lock_protect", in);

	MT_RET(true);
	MT_EE
}

bool StoredUser::LOCK_Protect() const
{
	MT_EB
	MT_FUNC("StoredUser::LOCK_Protect");

	if (ROOT->ConfigValue<bool>("nickserv.lock.protect"))
		MT_RET(true);

	mantra::StorageValue rv = storage.GetField(id_, "lock_protect");
	if (rv.type() == typeid(mantra::NullValue))
		MT_RET(false);
	bool ret = boost::get<bool>(rv);

	MT_RET(ret);
	MT_EE
}

bool StoredUser::LOCK_Secure(const bool &in)
{
	MT_EB
	MT_FUNC("StoredUser::LOCK_Secure" << in);

	if (ROOT->ConfigValue<bool>("nickserv.lock.secure"))
		MT_RET(false);

	storage.PutField(id_, "lock_secure", in);

	MT_RET(true);
	MT_EE
}

bool StoredUser::LOCK_Secure() const
{
	MT_EB
	MT_FUNC("StoredUser::LOCK_Secure");

	if (ROOT->ConfigValue<bool>("nickserv.lock.secure"))
		MT_RET(true);

	mantra::StorageValue rv = storage.GetField(id_, "lock_secure");
	if (rv.type() == typeid(mantra::NullValue))
		MT_RET(false);
	bool ret = boost::get<bool>(rv);

	MT_RET(ret);
	MT_EE
}

bool StoredUser::LOCK_NoMemo(const bool &in)
{
	MT_EB
	MT_FUNC("StoredUser::LOCK_NoMemo" << in);

	if (ROOT->ConfigValue<bool>("nickserv.lock.nomemo"))
		MT_RET(false);

	storage.PutField(id_, "lock_nomemo", in);

	MT_RET(true);
	MT_EE
}

bool StoredUser::LOCK_NoMemo() const
{
	MT_EB
	MT_FUNC("StoredUser::LOCK_NoMemo");

	if (ROOT->ConfigValue<bool>("nickserv.lock.nomemo"))
		MT_RET(true);

	mantra::StorageValue rv = storage.GetField(id_, "lock_nomemo");
	if (rv.type() == typeid(mantra::NullValue))
		MT_RET(false);
	bool ret = boost::get<bool>(rv);

	MT_RET(ret);
	MT_EE
}

bool StoredUser::LOCK_Private(const bool &in)
{
	MT_EB
	MT_FUNC("StoredUser::LOCK_Private" << in);

	if (ROOT->ConfigValue<bool>("nickserv.lock.private"))
		MT_RET(false);

	storage.PutField(id_, "lock_private", in);

	MT_RET(true);
	MT_EE
}

bool StoredUser::LOCK_Private() const
{
	MT_EB
	MT_FUNC("StoredUser::LOCK_Private");

	if (ROOT->ConfigValue<bool>("nickserv.lock.private"))
		MT_RET(true);

	mantra::StorageValue rv = storage.GetField(id_, "lock_private");
	if (rv.type() == typeid(mantra::NullValue))
		MT_RET(false);
	bool ret = boost::get<bool>(rv);

	MT_RET(ret);
	MT_EE
}

bool StoredUser::LOCK_PRIVMSG(const bool &in)
{
	MT_EB
	MT_FUNC("StoredUser::LOCK_PRIVMSG" << in);

	if (ROOT->ConfigValue<bool>("nickserv.lock.privmsg"))
		MT_RET(false);

	storage.PutField(id_, "lock_privmsg", in);

	MT_RET(true);
	MT_EE
}

bool StoredUser::LOCK_PRIVMSG() const
{
	MT_EB
	MT_FUNC("StoredUser::LOCK_PRIVMSG");

	if (ROOT->ConfigValue<bool>("nickserv.lock.privmsg"))
		MT_RET(true);

	mantra::StorageValue rv = storage.GetField(id_, "lock_privmsg");
	if (rv.type() == typeid(mantra::NullValue))
		MT_RET(false);
	bool ret = boost::get<bool>(rv);

	MT_RET(ret);
	MT_EE
}

bool StoredUser::ACCESS_Matches(const std::string &in) const
{
	MT_EB
	MT_FUNC("StoredUser::ACCESS_Matches" << in);

	mantra::Storage::DataSet data;
	mantra::Storage::FieldSet fields;
	fields.insert("mask");
	storage_access.RetrieveRow(data, mantra::Comparison<mantra::C_EqualTo>::make("id", id_), fields);

	mantra::Storage::DataSet::iterator i;
	for (i = data.begin(); i != data.end(); ++i)
	{
		mantra::Storage::RecordMap::iterator j = i->find("mask");
		if (j == i->end())
			continue; // ??
		if (mantra::glob_match(boost::get<std::string>(j->second), in, true))
			MT_RET(true);
	}
	MT_RET(false);

	MT_EE
}

bool StoredUser::ACCESS_Matches(const boost::shared_ptr<LiveUser> &in) const
{
	MT_EB
	MT_FUNC("StoredUser::ACCESS_Matches" << in);

	bool rv = ACCESS_Matches(in->User() + "@" + in->Host());
	MT_RET(rv);

	MT_EE
}

bool StoredUser::ACCESS_Exists(boost::uint32_t num) const
{
	MT_EB
	MT_FUNC("StoredUser::ACCESS_Exists" << num);

	bool rv = (storage_access.RowExists(
				mantra::Comparison<mantra::C_EqualTo>::make("id", id_) &&
				mantra::Comparison<mantra::C_EqualTo>::make("number", num)));

	MT_RET(rv);
	MT_EE
}

boost::uint32_t StoredUser::ACCESS_Add(const std::string &in)
{
	MT_EB
	MT_FUNC("StoredUser::ACCESS_Add" << in);

	boost::uint32_t number = 0;

	SYNC_LOCK(access_number);
	mantra::StorageValue rv = storage_access.Maximum("number",
				mantra::Comparison<mantra::C_EqualTo>::make("id", id_));
	if (rv.type() == typeid(mantra::NullValue))
		number = 1;
	else
		number = boost::get<boost::uint32_t>(rv) + 1;

	mantra::Storage::RecordMap rec;
	rec["id"] = id_;
	rec["number"] = number;
	rec["mask"] = in;
	if (storage_access.InsertRow(rec))
		MT_RET(number);
	MT_RET(0u);

	MT_EE
}

void StoredUser::ACCESS_Del(const boost::uint32_t num)
{
	MT_EB
	MT_FUNC("StoredUser::ACCESS_Del" << num);

	storage_access.RemoveRow(
				mantra::Comparison<mantra::C_EqualTo>::make("id", id_) &&
				mantra::Comparison<mantra::C_EqualTo>::make("number", num));

	MT_EE
}

void StoredUser::ACCESS_Change(boost::uint32_t num, const std::string &in)
{
	MT_EB
	MT_FUNC("StoredUser::ACCESS_Change" << num << in);

	mantra::Storage::RecordMap data;
	data["mask"] = in;
	storage_access.ChangeRow(data,
				mantra::Comparison<mantra::C_EqualTo>::make("id", id_) &&
				mantra::Comparison<mantra::C_EqualTo>::make("number", num));

	MT_EE
}

std::pair<std::string, boost::posix_time::ptime> StoredUser::ACCESS_Get(boost::uint32_t num) const
{
	MT_EB
	MT_FUNC("StoredUser::ACCESS_Get" << num);

	mantra::Storage::DataSet data;
	mantra::Storage::FieldSet fields;
	fields.insert("mask");
	fields.insert("last_update");
	storage_access.RetrieveRow(data,
				mantra::Comparison<mantra::C_EqualTo>::make("id", id_) &&
				mantra::Comparison<mantra::C_EqualTo>::make("number", num), fields);

	std::pair<std::string, boost::posix_time::ptime> rv;
			// (std::string(), boost::posix_time::ptime(not_a_date_time));
	if (data.size() != 1)
		MT_RET(rv);
	rv = std::make_pair(boost::get<std::string>(data.front()["mask"]),
						boost::get<boost::posix_time::ptime>(data.front()["last_update"]));

	MT_RET(rv);
	MT_EE
}

void StoredUser::ACCESS_Get(std::map<boost::uint32_t, std::pair<std::string, boost::posix_time::ptime> > &fill) const
{
	MT_EB
	MT_FUNC("StoredUser::ACCESS_Get" << fill);

	mantra::Storage::DataSet data;
	storage_access.RetrieveRow(data, mantra::Comparison<mantra::C_EqualTo>::make("id", id_));
	mantra::Storage::DataSet::iterator i;
	for (i = data.begin(); i != data.end(); ++i)
		fill[boost::get<boost::uint32_t>((*i)["number"])] =
			std::make_pair(boost::get<std::string>((*i)["mask"]),
						   boost::get<boost::posix_time::ptime>((*i)["last_update"]));

	MT_EE
}

bool StoredUser::IGNORE_Matches(const boost::shared_ptr<StoredUser> &in) const
{
	MT_EB
	MT_FUNC("StoredUser::IGNORE_Matches" << in);

	return (storage_ignore.RowExists(
				mantra::Comparison<mantra::C_EqualTo>::make("id", id_) &&
				mantra::Comparison<mantra::C_EqualTo>::make("entry", in->ID())) != 0);

	MT_EE
}

bool StoredUser::IGNORE_Exists(boost::uint32_t num) const
{
	MT_EB
	MT_FUNC("StoredUser::IGNORE_Exists" << num);

	return (storage_ignore.RowExists(
				mantra::Comparison<mantra::C_EqualTo>::make("id", id_) &&
				mantra::Comparison<mantra::C_EqualTo>::make("number", num)));

	MT_EE
}

boost::uint32_t StoredUser::IGNORE_Add(const boost::shared_ptr<StoredUser> &in)
{
	MT_EB
	MT_FUNC("StoredUser::IGNORE_Add" << in);

	boost::uint32_t number = 0;

	SYNC_LOCK(ignore_number);
	mantra::StorageValue rv = storage_ignore.Maximum("number",
				mantra::Comparison<mantra::C_EqualTo>::make("id", id_));
	if (rv.type() == typeid(mantra::NullValue))
		number = 1;
	else
		number = boost::get<boost::uint32_t>(rv) + 1;

	mantra::Storage::RecordMap rec;
	rec["id"] = id_;
	rec["number"] = number;
	rec["entry"] = in->ID();
	if (storage_ignore.InsertRow(rec))
		MT_RET(number);
	MT_RET(0);

	MT_EE
}

void StoredUser::IGNORE_Del(const boost::uint32_t num)
{
	MT_EB
	MT_FUNC("StoredUser::IGNORE_Del" << num);

	storage_ignore.RemoveRow(
				mantra::Comparison<mantra::C_EqualTo>::make("id", id_) &&
				mantra::Comparison<mantra::C_EqualTo>::make("number", num));

	MT_EE
}

void StoredUser::IGNORE_Change(boost::uint32_t num, const boost::shared_ptr<StoredUser> &in)
{
	MT_EB
	MT_FUNC("StoredUser::IGNORE_Change" << num << in);

	mantra::Storage::RecordMap data;
	data["entry"] = in->ID();
	storage_ignore.ChangeRow(data,
				mantra::Comparison<mantra::C_EqualTo>::make("id", id_) &&
				mantra::Comparison<mantra::C_EqualTo>::make("number", num));

	MT_EE
}

std::pair<boost::shared_ptr<StoredUser>, boost::posix_time::ptime> StoredUser::IGNORE_Get(boost::uint32_t num) const
{
	MT_EB
	MT_FUNC("StoredUser::IGNORE_Get" << num);

	mantra::Storage::DataSet data;
	mantra::Storage::FieldSet fields;
	fields.insert("entry");
	fields.insert("last_update");
	storage_ignore.RetrieveRow(data,
				mantra::Comparison<mantra::C_EqualTo>::make("id", id_) &&
				mantra::Comparison<mantra::C_EqualTo>::make("number", num), fields);
	std::pair<boost::shared_ptr<StoredUser>, boost::posix_time::ptime> rv;
			// (boost::shared_ptr<StoredUser>(), boost::posix_time::ptime(boost::date_time::not_a_date_time));
	if (data.size() != 1)
		MT_RET(rv);
	boost::uint32_t uid = boost::get<boost::uint32_t>(data.front()["entry"]);
	boost::shared_ptr<StoredUser> user = ROOT->data.Get_StoredUser(uid);
	if (!user)
		MT_RET(rv);
	rv = std::make_pair(user, boost::get<boost::posix_time::ptime>(data.front()["last_update"]));

	MT_RET(rv);
	MT_EE
}

void StoredUser::IGNORE_Get(std::map<boost::uint32_t, std::pair<boost::shared_ptr<StoredUser>, boost::posix_time::ptime> > &fill) const
{
	MT_EB
	MT_FUNC("StoredUser::IGNORE_Get" << fill);

	mantra::Storage::DataSet data;
	storage_ignore.RetrieveRow(data, mantra::Comparison<mantra::C_EqualTo>::make("id", id_));
	mantra::Storage::DataSet::iterator i;
	for (i = data.begin(); i != data.end(); ++i)
	{
		boost::uint32_t uid = boost::get<boost::uint32_t>((*i)["entry"]);
		boost::shared_ptr<StoredUser> user = ROOT->data.Get_StoredUser(uid);
		if (!user)
			continue;
		fill[boost::get<boost::uint32_t>((*i)["number"])] =
			std::make_pair(user, boost::get<boost::posix_time::ptime>((*i)["last_update"]));
	}

	MT_EE
}

void StoredUser::DropInternal()
{
	MT_EB
	MT_FUNC("StoredUser::DropInternal");

	// This function is pretty much a no-op right now.  Its all handled
	// by the DB function anyway (it will already drop nicknames for us).

	{
		SYNC_WLOCK(online_users_);
		online_users_.clear();
	}
	{
		SYNC_LOCK(my_nicks_);
		my_nicks_.clear();
	}
	{
		SYNC_LOCK(my_channels_);
		my_channels_.clear();
	}

	MT_EE
}

static void check_option(std::string &str, const mantra::Storage::RecordMap &data,
						 const std::string &key, const std::string &description)
{
	MT_EB
	MT_FUNC("check_option");

	if (ROOT->ConfigValue<bool>(("nickserv.lock." + key).c_str()))
	{
		if (ROOT->ConfigValue<bool>(("nickserv.defaults." + key).c_str()))
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
		else if (ROOT->ConfigValue<bool>(("nickserv.defaults." + key).c_str()))
		{
			if (!str.empty())
				str += ", ";
			str += description;
		}
	}

	MT_EE
}

void StoredUser::SendInfo(const boost::shared_ptr<LiveUser> &service,
						  const boost::shared_ptr<LiveUser> &user) const
{
	MT_EB
	MT_FUNC("StoredUser::SendInfo" << service << user);

	if (!service || !user || !service->GetService())
		return;

	bool opersop = (user->InCommittee(ROOT->ConfigValue<std::string>("commserv.oper.name")) ||
					user->InCommittee(ROOT->ConfigValue<std::string>("commserv.sop.name")));

	mantra::Storage::RecordMap data;
	storage.GetRow(id_, data);

	mantra::Storage::RecordMap::const_iterator i = data.find("email");
	if (i != data.end())
		SEND(service, user, N_("E-Mail Address : %1%"),
			 boost::get<std::string>(i->second));

	i = data.find("website");
	if (i != data.end())
		SEND(service, user, N_("Web Site       : %1%"),
			 boost::get<std::string>(i->second));

	i = data.find("icq");
	if (i != data.end())
		SEND(service, user, N_("ICQ UIN        : %1%"),
			 boost::get<boost::uint32_t>(i->second));

	i = data.find("aim");
	if (i != data.end())
		SEND(service, user, N_("AIM Screen Name: %1%"),
			 boost::get<std::string>(i->second));

	i = data.find("msn");
	if (i != data.end())
		SEND(service, user, N_("MSN account    : %1%"),
			 boost::get<std::string>(i->second));

	i = data.find("jabber");
	if (i != data.end())
		SEND(service, user, N_("Jabber ID      : %1%"),
			 boost::get<std::string>(i->second));

	i = data.find("yahoo");
	if (i != data.end())
		SEND(service, user, N_("Yahoo! ID      : %1%"),
			 boost::get<std::string>(i->second));

	i = data.find("description");
	if (i != data.end())
		SEND(service, user, N_("Description    : %1%"),
			 boost::get<std::string>(i->second));

	i = data.find("suspend_by");
	if (i != data.end())
	{
		SEND(service, user, N_("Suspended By   : %1% at %2%"),
			 boost::get<std::string>(i->second) %
			 boost::get<boost::posix_time::ptime>(data["suspend_time"]));
		i = data.find("suspend_reason");
		SEND(service, user, N_("Suspended For  : %1%"),
			 boost::get<std::string>(i->second));
	}

	std::string str;
	check_option(str, data, "protect", N_("Nick Protection"));
	check_option(str, data, "secure", N_("Secure"));
	check_option(str, data, "nomemo", N_("No Memos"));
	check_option(str, data, "private", N_("Private"));
	if (opersop)
		check_option(str, data, "noexpire", N_("No Expire"));
	if (!str.empty())
		SEND(service, user, N_("Options        : %1%"), str);

	str.clear();
	std::set<boost::shared_ptr<Committee> > comm =
			Committee::FindCommittees(self.lock());
	std::set<boost::shared_ptr<Committee> >::iterator j;
	for (j = comm.begin(); j != comm.end(); ++j)
	{
		if ((*j)->Private())
			continue;
		else if ((*j)->HeadUser() == self.lock())
		{
			if (!str.empty())
				str += ", ";
			str += '\002' + (*j)->Name() + '\017';
		}
		else if ((*j)->MEMBER_Exists(self.lock()))
		{
			if (!str.empty())
				str += ", ";
			str += (*j)->Name();
		}
	}
	if (!str.empty())
		SEND(service, user, N_("Committees     : %1%"), str);

	if (opersop)
	{
		i = data.find("comment");
		if (i != data.end())
			SEND(service, user, N_("OPER Comment   : %1%"),
				 boost::get<std::string>(i->second));
	}

	i = data.find("picture");
	if (i != data.end())
	{
		// TODO Info about picture ...
	}

	MT_EE
}

