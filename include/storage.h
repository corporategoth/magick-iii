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

#include <mantra/storage/interface.h>
#include <mantra/storage/stage.h>

#include <boost/noncopyable.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/read_write_mutex.hpp>

class LiveUser;
class StoredUser;
class StoredNick;
class LiveChannel;
class StoredChannel;
class Committee;

class Storage
{
	friend class StorageInterface;

	boost::mutex lock_;
	std::pair<mantra::FinalStage *, void (*)(mantra::FinalStage *)> finalstage_;
	std::vector<std::pair<mantra::Stage *, void (*)(mantra::Stage *)> > stages_;
	std::pair<mantra::Storage *, void (*)(mantra::Storage *)> backend_;
	unsigned int event_;
	void *handle_, *crypt_handle_, *compress_handle_;

	boost::read_write_mutex LiveUsers_lock_;
	std::map<std::string, boost::shared_ptr<LiveUser> > LiveUsers_;
	boost::read_write_mutex LiveChannels_lock_;
	std::map<std::string, boost::shared_ptr<LiveChannel> > LiveChannels_;

	boost::read_write_mutex StoredUsers_lock_;
	std::map<boost::uint32_t, boost::shared_ptr<StoredUser> > StoredUsers_;
	boost::read_write_mutex StoredNicks_lock_;
	std::map<std::string, boost::shared_ptr<StoredNick> > StoredNicks_;
	boost::read_write_mutex StoredChannels_lock_;
	std::map<std::string, boost::shared_ptr<StoredChannel> > StoredChannels_;
	boost::read_write_mutex Committees_lock_;
	std::map<std::string, boost::shared_ptr<Committee> > Committees_;

	void init();
	void reset();
public:
	Storage();
	~Storage();

	bool init(const boost::program_options::variables_map &vm,
			  const boost::program_options::variables_map &opt_config);
	void ClearLive();
	void Load();
	void Save();

	void Add_LiveUser(const boost::shared_ptr<LiveUser> &entry);
	void Del_LiveUser(const std::string &name);
	boost::shared_ptr<LiveUser> Get_LiveUser(const std::string &name) const;

	void Add_LiveChannel(const boost::shared_ptr<LiveChannel> &entry);
	void Del_LiveChannel(const std::string &name);
	boost::shared_ptr<LiveChannel> Get_LiveChannel(const std::string &name) const;

	void Add_StoredUser(const boost::shared_ptr<StoredUser> &entry);
	void Del_StoredUser(boost::uint32_t id);
	boost::shared_ptr<StoredUser> Get_StoredUser(boost::uint32_t id) const;

	void Add_StoredNick(const boost::shared_ptr<StoredNick> &entry);
	void Del_StoredNick(const std::string &name);
	boost::shared_ptr<StoredNick> Get_StoredNick(const std::string &name) const;

	void Add_StoredChannel(const boost::shared_ptr<StoredChannel> &entry);
	void Del_StoredChannel(const std::string &name);
	boost::shared_ptr<StoredChannel> Get_StoredChannel(const std::string &name) const;

	void Add_Committee(const boost::shared_ptr<Committee> &entry);
	void Del_Committee(const std::string &name);
	boost::shared_ptr<Committee> Get_Committee(const std::string &name) const;

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
inline std::ostream &operator<<(std::ostream &os, const boost::shared_ptr<T> &in)
{
	return (os << *in);
}

#endif // _MAGICK_STORAGE_H
