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

class generic_data_base
{
	boost::uint32_t number_;
protected:
	mutable CachedRecord cache;

	generic_data_base(StorageInterface &storage, boost::uint32_t num);
public:
	virtual ~generic_data_base() {}

	boost::uint32_t Number() const { return number_; }

	std::string Mask() const
		{ return boost::get<std::string>(cache.Get("mask")); }
	void Mask(const std::string &in, const boost::shared_ptr<StoredNick> &nick);
	bool Matches(const std::string &in, bool nocase = true) const
		{ return mantra::glob_match(Mask(), in, nocase); }

	std::string Reason() const
		{ return boost::get<std::string>(cache.Get("reason")); }
	void Reason(const std::string &in, const boost::shared_ptr<StoredNick> &nick);

	std::string Last_UpdaterName() const
		{ return boost::get<std::string>(cache.Get("last_updater")); }
	boost::shared_ptr<StoredUser> Last_Updater() const;
	boost::posix_time::ptime Last_Update() const
		{ return boost::get<boost::posix_time::ptime>(cache.Get("last_update")); }

	virtual void DelInternal() {}
};

template<typename T>
class generic_data_base_tmpl : public generic_data_base,
							   public boost::totally_ordered1<T>,
							   public boost::totally_ordered2<T, boost::uint32_t>
{

protected:
	using generic_data_base::cache;
	static StorageInterface storage;
	static boost::mutex number_lock;

	generic_data_base_tmpl(boost::uint32_t num) : generic_data_base(storage, num) {}

	static void sync(std::set<T> &internal);
public:
	virtual ~generic_data_base_tmpl() {}

	inline bool operator<(boost::uint32_t num) const { return (Number() < num); }
	inline bool operator==(boost::uint32_t num) const { return (Number() == num); }
	inline bool operator<(const T &rhs) const { return (*this < rhs.Number()); }
	inline bool operator==(const T &rhs) const { return (*this == rhs.Number()); }
	inline bool operator!() const { return !(Number() && storage.Exists(Number())); }
	inline operator bool() const { return !operator!(); }
};

class Forbidden : public generic_data_base_tmpl<Forbidden>
{
	friend class Storage;
	friend class generic_data_base_tmpl<Forbidden>;

	using generic_data_base_tmpl<Forbidden>::sync;

	Forbidden(boost::uint32_t num) : generic_data_base_tmpl<Forbidden>(num) {}
public:
	Forbidden() : generic_data_base_tmpl<Forbidden>(0) {}
	static Forbidden create(const std::string &mask, const std::string &reason,
							const boost::shared_ptr<StoredNick> &nick);
};

class Akill : public generic_data_base_tmpl<Akill>
{
	friend class Storage;
	friend class generic_data_base_tmpl<Akill>;

	using generic_data_base_tmpl<Akill>::sync;

	Akill(boost::uint32_t num) : generic_data_base_tmpl<Akill>(num) {}
public:
	Akill() : generic_data_base_tmpl<Akill>(0) {}
	static Akill create(const std::string &mask, const std::string &reason,
						const mantra::duration &length,
						const boost::shared_ptr<StoredNick> &nick);
	static Akill create(const std::string &mask, const std::string &reason,
						const mantra::duration &length, const Service *service);

	mantra::duration Length() const
		{ return boost::get<mantra::duration>(cache.Get("length")); }
	void Length(const mantra::duration &in, const boost::shared_ptr<StoredNick> &nick);
};

class Clone : public generic_data_base_tmpl<Clone>
{
	friend class Storage;
	friend class generic_data_base_tmpl<Clone>;

	using generic_data_base_tmpl<Clone>::sync;

	Clone(boost::uint32_t num) : generic_data_base_tmpl<Clone>(num) {}
public:
	Clone() : generic_data_base_tmpl<Clone>(0) {}
	static Clone create(const std::string &mask, const std::string &reason,
						boost::uint32_t value, const boost::shared_ptr<StoredNick> &nick);

	boost::uint32_t Value() const
		{ return boost::get<boost::uint32_t>(cache.Get("value")); }
	void Value(boost::uint32_t in, const boost::shared_ptr<StoredNick> &nick);
};

class OperDeny : public generic_data_base_tmpl<OperDeny>
{
	friend class Storage;
	friend class generic_data_base_tmpl<OperDeny>;

	using generic_data_base_tmpl<OperDeny>::sync;

	OperDeny(boost::uint32_t num) : generic_data_base_tmpl<OperDeny>(num) {}
public:
	OperDeny() : generic_data_base_tmpl<OperDeny>(0) {}
	static OperDeny create(const std::string &mask, const std::string &reason,
						   const boost::shared_ptr<StoredNick> &nick);
};

class Ignore : public generic_data_base_tmpl<Ignore>
{
	friend class Storage;
	friend class generic_data_base_tmpl<Ignore>;

	using generic_data_base_tmpl<Ignore>::sync;

	Ignore(boost::uint32_t num) : generic_data_base_tmpl<Ignore>(num) {}
public:
	Ignore() : generic_data_base_tmpl<Ignore>(0) {}
	static Ignore create(const std::string &mask, const std::string &reason,
						 const boost::shared_ptr<StoredNick> &nick);
	static Ignore create(const std::string &mask, const std::string &reason,
						 const Service *service);
};

class KillChannel : public generic_data_base_tmpl<KillChannel>
{
	friend class Storage;
	friend class generic_data_base_tmpl<KillChannel>;

	using generic_data_base_tmpl<KillChannel>::sync;

	KillChannel(boost::uint32_t num) : generic_data_base_tmpl<KillChannel>(num) {}
public:
	KillChannel() : generic_data_base_tmpl<KillChannel>(0) {}
	static KillChannel create(const std::string &mask, const std::string &reason,
							  const boost::shared_ptr<StoredNick> &nick);
};

// ---------------------------------------------------------------------

// We can get away with this method (ie. without DelInternal or Add calls)
// because all classes based off generic_data_base_tmpl are self-contained
// with no external references to themselves.  If this changes, we will
// have to re-think this methodology of addition/removal.
template<typename T>
void generic_data_base_tmpl<T>::sync(std::set<T> &internal)
{
	mantra::Storage::DataSet data;
	mantra::Storage::FieldSet fields;

	fields.insert("number");
	storage.RetrieveRow(data, mantra::ComparisonSet(), fields);
	std::sort(data.begin(), data.end(), idless);

	std::set<T> newentries;
	typename std::set<T>::iterator i = internal.begin();
	mantra::Storage::DataSet::const_iterator j = data.begin();
	mantra::Storage::RecordMap::const_iterator k;

	while (i != internal.end() && j != data.end())
	{
		k = j->find("number");
		if (k == j->end() || k->second.type() != typeid(boost::uint32_t))
		{
			// This should never happen!
			++j;
			continue;
		}
		boost::uint32_t id = boost::get<boost::uint32_t>(k->second);

		while (*i < id)
		{
			// Old entry, delete it.
			internal.erase(i++);
		}
		if (*i != id)
		{
			// New entry, add it.
			newentries.insert(T(id));
		}
		else
			++i;
		++j;

	}
	// Old entries, remove them.
	while (i != internal.end())
	{
		// Old entry, delete it.
		internal.erase(i++);
	}

	// New entries, add them.
	while (j != data.end())
	{
		k = j->find("number");
		if (k == j->end() || k->second.type() != typeid(boost::uint32_t))
		{
			// This should never happen!
			++j;
			continue;
		}
		boost::uint32_t id = boost::get<boost::uint32_t>(k->second);

		newentries.insert(T(id));
		++j;
	}

	// Add the entries from above ...
	while (!newentries.empty())
		internal.insert(newentries.begin(), newentries.end());
}

#endif // _MAGICK_OTHERDATA_H
