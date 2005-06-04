#ifndef WIN32
#pragma interface
#endif

/* Magick IRC Services
**
** (c) 2004 The Neuromancy Society <info@neuromancy.net>
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
#ifndef _MAGICK_STORAGE_H
#define _MAGICK_STORAGE_H 1

#ifdef RCSID
RCSID(magick__storage_h, "@(#) $Id$");
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

#include <string>
#include <map>
#include <ostream>

#include <mantra/core/hasher.h>
#include <mantra/core/sync.h>
#include <mantra/storage/interface.h>
#include <mantra/storage/stage.h>

#include <boost/noncopyable.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/read_write_mutex.hpp>
#include <boost/mpl/identity.hpp>

class LiveUser;
class StoredUser;
class StoredNick;
class LiveChannel;
class StoredChannel;
class Committee;

class Storage
{
	friend class StorageInterface;
	template<typename T> friend class if_StorageDeleter;

	typedef std::map<std::string, boost::shared_ptr<LiveUser>, mantra::iless<std::string> > LiveUsers_t;
	typedef std::map<std::string, boost::shared_ptr<LiveChannel>, mantra::iless<std::string> > LiveChannels_t;
	typedef std::map<boost::uint32_t, boost::shared_ptr<StoredUser> > StoredUsers_t;
	typedef std::map<std::string, boost::shared_ptr<StoredNick>, mantra::iless<std::string> > StoredNicks_t;
	typedef std::map<std::string, boost::shared_ptr<StoredChannel>, mantra::iless<std::string> > StoredChannels_t;
	typedef std::map<std::string, boost::shared_ptr<Committee>, mantra::iless<std::string> > Committees_t;

	boost::mutex lock_;
	std::pair<mantra::FinalStage *, void (*)(mantra::FinalStage *)> finalstage_;
	std::vector<std::pair<mantra::Stage *, void (*)(mantra::Stage *)> > stages_;
	std::pair<mantra::Storage *, void (*)(mantra::Storage *)> backend_;
	bool have_cascade;
	unsigned int event_;
	void *handle_, *crypt_handle_, *compress_handle_;
	mantra::Hasher hasher;

	LiveUsers_t RWSYNC(LiveUsers_);
	LiveChannels_t RWSYNC(LiveChannels_);

	StoredUsers_t RWSYNC(StoredUsers_);
	StoredNicks_t RWSYNC(StoredNicks_);
	StoredChannels_t RWSYNC(StoredChannels_);
	Committees_t RWSYNC(Committees_);

	void init();
	void reset();
	void sync();

	void Del(const boost::shared_ptr<LiveUser> &entry);
	void Del(const boost::shared_ptr<LiveChannel> &entry);

	void DelInternal(const boost::shared_ptr<StoredUser> &entry);
	void Del(const boost::shared_ptr<StoredUser> &entry);
	void DelInternal(const boost::shared_ptr<StoredNick> &entry);
	void Del(const boost::shared_ptr<StoredNick> &entry);
	void DelInternal(const boost::shared_ptr<StoredChannel> &entry);
	void Del(const boost::shared_ptr<StoredChannel> &entry);
	void DelInternalRecurse(const std::vector<mantra::StorageValue> &ent);
	void DelInternal(const boost::shared_ptr<Committee> &entry);
	void Del(const boost::shared_ptr<Committee> &entry);

public:
	Storage();
	~Storage();

	bool init(const boost::program_options::variables_map &vm,
			  const boost::program_options::variables_map &opt_config);
	void ClearLive();
	void Load();
	void Save();
	std::string CryptPassword(const std::string &in) const;

	void Add(const boost::shared_ptr<LiveUser> &entry);
	void Add(const boost::shared_ptr<LiveChannel> &entry);
	void Add(const boost::shared_ptr<StoredUser> &entry);
	void Add(const boost::shared_ptr<StoredNick> &entry);
	void Add(const boost::shared_ptr<StoredChannel> &entry);
	void Add(const boost::shared_ptr<Committee> &entry);

	void Rename(const boost::shared_ptr<LiveUser> &entry,
				const std::string &newname);

	boost::shared_ptr<LiveUser> Get_LiveUser(const std::string &name) const;
	boost::shared_ptr<LiveChannel> Get_LiveChannel(const std::string &name) const;
	boost::shared_ptr<StoredUser> Get_StoredUser(boost::uint32_t id,
			 boost::logic::tribool deep = boost::logic::indeterminate) const;
	boost::shared_ptr<StoredNick> Get_StoredNick(const std::string &name,
			 boost::logic::tribool deep = boost::logic::indeterminate) const;
	boost::shared_ptr<StoredChannel> Get_StoredChannel(const std::string &name,
			 boost::logic::tribool deep = boost::logic::indeterminate) const;
	boost::shared_ptr<Committee> Get_Committee(const std::string &name,
			 boost::logic::tribool deep = boost::logic::indeterminate) const;

	bool Forbid_Add(const std::string &in, const boost::shared_ptr<StoredNick> &nick);
	bool Forbid_Del(const std::string &in);
	std::vector<std::string> Forbid_List_Nick() const;
	std::vector<std::string> Forbid_List_Channel() const;
	bool Forbid_Check(const std::string &in) const;
};

// Perfect way to do single-table access :)
class StorageInterface : private boost::noncopyable
{
	std::string table_;
	std::string key_;
	std::string update_;

protected:
	virtual bool GetRow(const mantra::ComparisonSet &search, mantra::Storage::RecordMap &data);
	virtual mantra::StorageValue GetField(const mantra::ComparisonSet &search, const std::string &column);
	virtual bool PutField(const mantra::ComparisonSet &search, const std::string &column, const mantra::StorageValue &value);

public:
	StorageInterface(const std::string &table,
					 const std::string &key = std::string(),
					 const std::string &update = std::string())
		: table_(table), key_(key), update_(update) {}
	virtual ~StorageInterface() {}

	virtual unsigned int RowExists(const mantra::ComparisonSet &search = mantra::ComparisonSet()) const throw(mantra::storage_exception);
	virtual bool InsertRow(const mantra::Storage::RecordMap &data) throw(mantra::storage_exception);
	virtual unsigned int ChangeRow(const mantra::Storage::RecordMap &data, 
				const mantra::ComparisonSet &search = mantra::ComparisonSet()) throw(mantra::storage_exception);
	virtual void RetrieveRow(mantra::Storage::DataSet &data,
				const mantra::ComparisonSet &search = mantra::ComparisonSet(),
				const mantra::Storage::FieldSet &fields = mantra::Storage::FieldSet()) throw(mantra::storage_exception);
	virtual unsigned int RemoveRow(const mantra::ComparisonSet &search = mantra::ComparisonSet()) throw(mantra::storage_exception);
	virtual mantra::StorageValue Minimum(const std::string &column,
				const mantra::ComparisonSet &search = mantra::ComparisonSet()) throw(mantra::storage_exception);
	virtual mantra::StorageValue Maximum(const std::string &column,
				const mantra::ComparisonSet &search = mantra::ComparisonSet()) throw(mantra::storage_exception);

	// Aliases that use the above.
	inline bool GetRow(const std::string &key, mantra::Storage::RecordMap &data)
		{ return GetRow(mantra::Comparison<mantra::C_EqualToNC>::make(key_, key), data); }
	inline bool GetRow(const boost::uint32_t &key, mantra::Storage::RecordMap &data)
		{ return GetRow(mantra::Comparison<mantra::C_EqualTo>::make(key_, key), data); }

	// Single field gets/puts, using the pre-defined key.
	inline mantra::StorageValue GetField(const std::string &key, const std::string &column)
		{ return GetField(mantra::Comparison<mantra::C_EqualToNC>::make(key_, key), column); }
	inline mantra::StorageValue GetField(const boost::uint32_t &key, const std::string &column)
		{ return GetField(mantra::Comparison<mantra::C_EqualTo>::make(key_, key), column); }
	inline bool PutField(const std::string &key, const std::string &column, const mantra::StorageValue &value)
		{ return PutField(mantra::Comparison<mantra::C_EqualToNC>::make(key_, key), column, value); }
	inline bool PutField(const boost::uint32_t &key, const std::string &column, const mantra::StorageValue &value)
		{ return PutField(mantra::Comparison<mantra::C_EqualTo>::make(key_, key), column, value); }
};

template<typename T>
class if_StorageDeleter
{
	// This is a hack, friend class T doesn't work.
	friend class boost::mpl::identity<T>::type;
	Storage &base;

	if_StorageDeleter(Storage &b) : base(b) {}
	if_StorageDeleter(const Storage *b) : base(*b) {}

	inline void Del(const boost::shared_ptr<T> &in) const
		{ base.Del(in); }
};

template<typename T>
inline std::ostream &operator<<(std::ostream &os, const boost::shared_ptr<T> &in)
{
	if (!in)
		return (os << "(NULL)");
	else
		return (os << *in);
}

namespace std
{

// Used for modes (and mode args respectively).
inline ostream &operator<<(ostream &os, const set<char> &in)
{
	for_each(in.begin(), in.end(), mantra::StreamObj<char>(os));
	return os;
}

inline ostream &operator<<(ostream &os, const vector<string> &in)
{
	os << "[";
	for_each(in.begin(), in.end(), mantra::StreamObj<char>(os, ", "));
	os << "]";
	return os;
}

}

#endif // _MAGICK_STORAGE_H
