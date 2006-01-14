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
#define RCSID(x,y) const char *rcsid_magick__otherdata_cpp_ ## x () { return y; }
RCSID(magick__otherdata_cpp, "@(#)$Id$");

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
#include "otherdata.h"

#include <mantra/core/trace.h>

template<> StorageInterface generic_data_base_tmpl<Forbidden>::storage("forbidden", "number", "last_update");
template boost::mutex generic_data_base_tmpl<Forbidden>::number_lock;
template<> boost::mutex generic_data_base_tmpl<Forbidden>::number_lock;

template<> StorageInterface generic_data_base_tmpl<Akill>::storage("akills", "number", "last_update");
template boost::mutex generic_data_base_tmpl<Akill>::number_lock;
template<> boost::mutex generic_data_base_tmpl<Akill>::number_lock;

template<> StorageInterface generic_data_base_tmpl<Clone>::storage("clones", "number", "last_update");
template boost::mutex generic_data_base_tmpl<Clone>::number_lock;
template<> boost::mutex generic_data_base_tmpl<Clone>::number_lock;

template<> StorageInterface generic_data_base_tmpl<OperDeny>::storage("operdenies", "number", "last_update");
template boost::mutex generic_data_base_tmpl<OperDeny>::number_lock;
template<> boost::mutex generic_data_base_tmpl<OperDeny>::number_lock;

template<> StorageInterface generic_data_base_tmpl<Ignore>::storage("ignores", "number", "last_update");
template boost::mutex generic_data_base_tmpl<Ignore>::number_lock;
template<> boost::mutex generic_data_base_tmpl<Ignore>::number_lock;

template<> StorageInterface generic_data_base_tmpl<KillChannel>::storage("killchans", "number", "last_update");
template boost::mutex generic_data_base_tmpl<KillChannel>::number_lock;
template<> boost::mutex generic_data_base_tmpl<KillChannel>::number_lock;

generic_data_base::generic_data_base(StorageInterface &storage,
									 boost::uint32_t number)
	: number_(number), cache(storage,
			ROOT->ConfigValue<mantra::duration>("storage.cache-expire"),
			mantra::Comparison<mantra::C_EqualTo>::make("number", number))
{
	MT_EB
	MT_FUNC("generic_data_base::generic_data_base" << storage << number);

	MT_EE
}

void generic_data_base::Mask(const std::string &in, const boost::shared_ptr<StoredNick> &nick)
{
	MT_EB
	MT_FUNC("generic_data_base::Mask" << in << nick);

	mantra::Storage::RecordMap rec;
	rec["mask"] = in;
	rec["last_updater"] = nick->Name();
	rec["last_updater_id"] = nick->User()->ID();
	cache.Put(rec);

	MT_EE
}

void generic_data_base::Reason(const std::string &in, const boost::shared_ptr<StoredNick> &nick)
{
	MT_EB
	MT_FUNC("generic_data_base::Reason" << in << nick);

	mantra::Storage::RecordMap rec;
	rec["reason"] = in;
	rec["last_updater"] = nick->Name();
	rec["last_updater_id"] = nick->User()->ID();
	cache.Put(rec);

	MT_EE
}

boost::shared_ptr<StoredUser> generic_data_base::Last_Updater() const
{
	MT_EB
	MT_FUNC("generic_data_base::Last_Updater");

	boost::shared_ptr<StoredUser> rv;
	mantra::StorageValue v = cache.Get("last_updater_id");
	if (v.type() != typeid(mantra::NullValue))
		rv = ROOT->data.Get_StoredUser(boost::get<boost::uint32_t>(v));

	MT_RET(rv);
	MT_EE
}

Forbidden Forbidden::create(const std::string &mask, const std::string &reason,
							const boost::shared_ptr<StoredNick> &nick)
{
	MT_EB
	MT_FUNC("Forbidden::create" << mask << reason << nick);

	boost::uint32_t id;
	{
		boost::mutex::scoped_lock sl(number_lock);
		mantra::StorageValue v = storage.Maximum("number");
		if (v.type() == typeid(mantra::NullValue))
			id = 1;
		else
			id = boost::get<boost::uint32_t>(v) + 1;

		mantra::Storage::RecordMap rec;
		rec["number"] = id;
		rec["mask"] = mask;
		rec["reason"] = reason;
		rec["last_updater"] = nick->Name();
		rec["last_updater_id"] = nick->User()->ID();
		if (!storage.InsertRow(rec))
		{
			Forbidden rv(0);
			MT_RET(rv);
		}
	}

	Forbidden rv(id);
	ROOT->data.Add(rv);

	MT_RET(rv);
	MT_EE
}

Akill Akill::create(const std::string &mask, const std::string &reason,
					const mantra::duration &length,
					const boost::shared_ptr<StoredNick> &nick)
{
	MT_EB
	MT_FUNC("Akill::create" << mask << reason << length << nick);

	boost::uint32_t id;
	{
		boost::mutex::scoped_lock sl(number_lock);
		mantra::StorageValue v = storage.Maximum("number");
		if (v.type() == typeid(mantra::NullValue))
			id = 1;
		else
			id = boost::get<boost::uint32_t>(v) + 1;

		mantra::Storage::RecordMap rec;
		rec["number"] = id;
		rec["mask"] = mask;
		rec["reason"] = reason;
		rec["creation"] = mantra::GetCurrentDateTime();
		if (length)
			rec["length"] = length;
		rec["last_updater"] = nick->Name();
		rec["last_updater_id"] = nick->User()->ID();
		if (!storage.InsertRow(rec))
		{
			Akill rv(0);
			MT_RET(rv);
		}
	}

	Akill rv(id);
	ROOT->data.Add(rv);

	MT_RET(rv);
	MT_EE
}

Akill Akill::create(const std::string &mask, const std::string &reason,
					const mantra::duration &length, const Service *service)
{
	MT_EB
	MT_FUNC("Akill::create" << mask << reason << length << service);

	boost::uint32_t id;
	{
		boost::mutex::scoped_lock sl(number_lock);
		mantra::StorageValue v = storage.Maximum("number");
		if (v.type() == typeid(mantra::NullValue))
			id = 1;
		else
			id = boost::get<boost::uint32_t>(v) + 1;

		mantra::Storage::RecordMap rec;
		rec["number"] = id;
		rec["mask"] = mask;
		rec["reason"] = reason;
		rec["creation"] = mantra::GetCurrentDateTime();
		if (length)
			rec["length"] = length;
		rec["last_updater"] = service->Primary();
		if (!storage.InsertRow(rec))
		{
			Akill rv(0);
			MT_RET(rv);
		}
	}

	Akill rv(id);
	ROOT->data.Add(rv);

	MT_RET(rv);
	MT_EE
}

void Akill::Length(const mantra::duration &in, const boost::shared_ptr<StoredNick> &nick)
{
	MT_EB
	MT_FUNC("Akill::Length" << in << nick);

	mantra::Storage::RecordMap rec;
	rec["length"] = in;
	rec["last_updater"] = nick->Name();
	rec["last_updater_id"] = nick->User()->ID();
	cache.Put(rec);

	MT_EE
}

Clone Clone::create(const std::string &mask, const std::string &reason,
					boost::uint32_t value, const boost::shared_ptr<StoredNick> &nick)
{
	MT_EB
	MT_FUNC("Clone::create" << mask << reason << value << nick);

	boost::uint32_t id;
	{
		boost::mutex::scoped_lock sl(number_lock);
		mantra::StorageValue v = storage.Maximum("number");
		if (v.type() == typeid(mantra::NullValue))
			id = 1;
		else
			id = boost::get<boost::uint32_t>(v) + 1;

		mantra::Storage::RecordMap rec;
		rec["number"] = id;
		rec["mask"] = mask;
		rec["reason"] = reason;
		rec["value"] = value;
		rec["last_updater"] = nick->Name();
		rec["last_updater_id"] = nick->User()->ID();
		if (!storage.InsertRow(rec))
		{
			Clone rv(0);
			MT_RET(rv);
		}
	}

	Clone rv(id);
	ROOT->data.Add(rv);

	MT_RET(rv);
	MT_EE
}

void Clone::Value(boost::uint32_t in, const boost::shared_ptr<StoredNick> &nick)
{
	MT_EB
	MT_FUNC("Clone::Value" << in << nick);

	mantra::Storage::RecordMap rec;
	rec["value"] = in;
	rec["last_updater"] = nick->Name();
	rec["last_updater_id"] = nick->User()->ID();
	cache.Put(rec);

	MT_EE
}

OperDeny OperDeny::create(const std::string &mask, const std::string &reason,
						  const boost::shared_ptr<StoredNick> &nick)
{
	MT_EB
	MT_FUNC("OperDeny::create" << mask << reason << nick);

	boost::uint32_t id;
	{
		boost::mutex::scoped_lock sl(number_lock);
		mantra::StorageValue v = storage.Maximum("number");
		if (v.type() == typeid(mantra::NullValue))
			id = 1;
		else
			id = boost::get<boost::uint32_t>(v) + 1;

		mantra::Storage::RecordMap rec;
		rec["number"] = id;
		rec["mask"] = mask;
		rec["reason"] = reason;
		rec["last_updater"] = nick->Name();
		rec["last_updater_id"] = nick->User()->ID();
		if (!storage.InsertRow(rec))
		{
			OperDeny rv(0);
			MT_RET(rv);
		}
	}

	OperDeny rv(id);
	ROOT->data.Add(rv);

	MT_RET(rv);
	MT_EE
}

Ignore Ignore::create(const std::string &mask, const std::string &reason,
					  const boost::shared_ptr<StoredNick> &nick)
{
	MT_EB
	MT_FUNC("Ignore::create" << mask << reason << nick);

	boost::uint32_t id;
	{
		boost::mutex::scoped_lock sl(number_lock);
		mantra::StorageValue v = storage.Maximum("number");
		if (v.type() == typeid(mantra::NullValue))
			id = 1;
		else
			id = boost::get<boost::uint32_t>(v) + 1;

		mantra::Storage::RecordMap rec;
		rec["number"] = id;
		rec["mask"] = mask;
		rec["reason"] = reason;
		rec["last_updater"] = nick->Name();
		rec["last_updater_id"] = nick->User()->ID();
		if (!storage.InsertRow(rec))
		{
			Ignore rv(0);
			MT_RET(rv);
		}
	}

	Ignore rv(id);
	ROOT->data.Add(rv);

	MT_RET(rv);
	MT_EE
}

Ignore Ignore::create(const std::string &mask, const std::string &reason,
					  const Service *service)
{
	MT_EB
	MT_FUNC("Ignore::create" << mask << reason << service);

	boost::uint32_t id;
	{
		boost::mutex::scoped_lock sl(number_lock);
		mantra::StorageValue v = storage.Maximum("number");
		if (v.type() == typeid(mantra::NullValue))
			id = 1;
		else
			id = boost::get<boost::uint32_t>(v) + 1;

		mantra::Storage::RecordMap rec;
		rec["number"] = id;
		rec["mask"] = mask;
		rec["reason"] = reason;
		rec["last_updater"] = service->Primary();
		if (!storage.InsertRow(rec))
		{
			Ignore rv(0);
			MT_RET(rv);
		}
	}

	Ignore rv(id);
	ROOT->data.Add(rv);

	MT_RET(rv);
	MT_EE
}

KillChannel KillChannel::create(const std::string &mask, const std::string &reason,
								const boost::shared_ptr<StoredNick> &nick)
{
	MT_EB
	MT_FUNC("KillChannel::create" << mask << reason << nick);

	boost::uint32_t id;
	{
		boost::mutex::scoped_lock sl(number_lock);
		mantra::StorageValue v = storage.Maximum("number");
		if (v.type() == typeid(mantra::NullValue))
			id = 1;
		else
			id = boost::get<boost::uint32_t>(v) + 1;

		mantra::Storage::RecordMap rec;
		rec["number"] = id;
		rec["mask"] = mask;
		rec["reason"] = reason;
		rec["last_updater"] = nick->Name();
		rec["last_updater_id"] = nick->User()->ID();
		if (!storage.InsertRow(rec))
		{
			KillChannel rv(0);
			MT_RET(rv);
		}
	}

	KillChannel rv(id);
	ROOT->data.Add(rv);

	MT_RET(rv);
	MT_EE
}

