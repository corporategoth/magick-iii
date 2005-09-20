#ifndef WIN32
#pragma interface
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
** it may be viewed here: http://www.neuromancy.net/license/gpl.html
**
** ======================================================================= */
#ifndef _MAGICK_OTHERDATA_H
#define _MAGICK_OTHERDATA_H 1

#ifdef RCSID
RCSID(magick__otherdata_h, "@(#) $Id$");
#endif // RCSID
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

#include "config.h"
#include "storageinterface.h"
#include "storednick.h"
#include "storeduser.h"

#include <boost/thread/mutex.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/operators.hpp>

class StoredUser;
class StoredNick;
class Service;

class generic_data_base_proxy
{
protected:
	// This is a proxy for ROOT->data.Get_StoredUser
	boost::shared_ptr<StoredUser> lookup_user(boost::uint32_t id);
};

template<typename T>
class generic_data_base : public generic_data_base_proxy,
						  public boost::totally_ordered1<T>,
						  public boost::totally_ordered2<T, boost::uint32_t>
{
	boost::uint32_t number_;

protected:
	static StorageInterface storage;
	static boost::mutex number_lock;

	generic_data_base(boost::uint32_t num) : number_(num) {}

public:
	virtual ~generic_data_base() {}

	boost::uint32_t Number() const { return number_; }

	std::string Mask() const
		{ return boost::get<std::string>(storage.GetField(number_, "mask")); }
	void Mask(const std::string &in, const boost::shared_ptr<StoredNick> &nick)
	{
		mantra::Storage::RecordMap rec;
		rec["mask"] = in;
		rec["last_updater"] = nick->Name();
		rec["last_updater_id"] = nick->User()->ID();
		storage.PutRow(number_, rec);
	}
	bool Matches(const std::string &in, bool nocase = true) const
		{ return mantra::glob_match(Mask(), in, nocase); }

	std::string Reason() const
		{ return boost::get<std::string>(storage.GetField(number_, "reason")); }
	void Reason(const std::string &in, const boost::shared_ptr<StoredNick> &nick)
	{
		mantra::Storage::RecordMap rec;
		rec["reason"] = in;
		rec["last_updater"] = nick->Name();
		rec["last_updater_id"] = nick->User()->ID();
		storage.PutRow(number_, rec);
	}

	std::string Last_UpdaterName() const
		{ return boost::get<std::string>(storage.GetField(number_, "last_updater")); }
	boost::shared_ptr<StoredUser> Last_Updater() const
	{
		boost::shared_ptr<StoredUser> rv;
		mantra::StorageValue v = storage.GetField(number_, "last_updater_id");
		if (v.type() == typeid(mantra::NullValue))
			return rv;
		return lookup_user(boost::get<boost::uint32_t>(v));
	}
	boost::posix_time::ptime Last_Update() const
		{ return boost::get<boost::posix_time::ptime>(storage.GetField(number_, "last_update")); }

	inline bool operator<(boost::uint32_t num) const { return (number_ < num); }
	inline bool operator==(boost::uint32_t num) const { return (number_ == num); }
	inline bool operator<(const T &rhs) const { return (*this < rhs.number_); }
	inline bool operator==(const T &rhs) const { return (*this == rhs.number_); }

	virtual void DelInternal() {}
};

class Forbidden : public generic_data_base<Forbidden>
{
	friend class Storage;

	Forbidden(boost::uint32_t num) : generic_data_base<Forbidden>(num) {}
public:
	Forbidden() : generic_data_base<Forbidden>(0) {}
	static Forbidden create(const std::string &mask, const std::string &reason,
							const boost::shared_ptr<StoredNick> &nick);
};

class Akill : public generic_data_base<Akill>
{
	friend class Storage;

	Akill(boost::uint32_t num) : generic_data_base<Akill>(num) {}
public:
	Akill() : generic_data_base<Akill>(0) {}
	static Akill create(const std::string &mask, const std::string &reason,
						const mantra::duration &length,
						const boost::shared_ptr<StoredNick> &nick);
	static Akill create(const std::string &mask, const std::string &reason,
						const mantra::duration &length, const Service *service);

	mantra::duration Length() const
		{ return boost::get<mantra::duration>(storage.GetField(Number(), "length")); }
	void Length(const mantra::duration &in, const boost::shared_ptr<StoredNick> &nick)
	{
		mantra::Storage::RecordMap rec;
		rec["length"] = in;
		rec["last_updater"] = nick->Name();
		rec["last_updater_id"] = nick->User()->ID();
		storage.PutRow(Number(), rec);
	}
};

class Clone : public generic_data_base<Clone>
{
	friend class Storage;

	Clone(boost::uint32_t num) : generic_data_base<Clone>(num) {}
public:
	Clone() : generic_data_base<Clone>(0) {}
	static Clone create(const std::string &mask, const std::string &reason,
						boost::uint32_t value, const boost::shared_ptr<StoredNick> &nick);

	boost::uint32_t Value() const
		{ return boost::get<boost::uint32_t>(storage.GetField(Number(), "value")); }
	void Value(const boost::uint32_t &in, const boost::shared_ptr<StoredNick> &nick)
	{
		mantra::Storage::RecordMap rec;
		rec["value"] = in;
		rec["last_updater"] = nick->Name();
		rec["last_updater_id"] = nick->User()->ID();
		storage.PutRow(Number(), rec);
	}
};

class OperDeny : public generic_data_base<OperDeny>
{
	friend class Storage;

	OperDeny(boost::uint32_t num) : generic_data_base<OperDeny>(num) {}
public:
	OperDeny() : generic_data_base<OperDeny>(0) {}
	static OperDeny create(const std::string &mask, const std::string &reason,
						   const boost::shared_ptr<StoredNick> &nick);
};

class Ignore : public generic_data_base<Ignore>
{
	friend class Storage;

	Ignore(boost::uint32_t num) : generic_data_base<Ignore>(num) {}
public:
	Ignore() : generic_data_base<Ignore>(0) {}
	static Ignore create(const std::string &mask, const std::string &reason,
						 const boost::shared_ptr<StoredNick> &nick);
	static Ignore create(const std::string &mask, const std::string &reason,
						 const Service *service);
};

class KillChannel : public generic_data_base<KillChannel>
{
	friend class Storage;

	KillChannel(boost::uint32_t num) : generic_data_base<KillChannel>(num) {}
public:
	KillChannel() : generic_data_base<KillChannel>(0) {}
	static KillChannel create(const std::string &mask, const std::string &reason,
							  const boost::shared_ptr<StoredNick> &nick);
};

#endif // _MAGICK_OTHERDATA_H
