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

#include "storageinterface.h"

// *sigh* have to include these to make multi_index happy.
#include "storedchannel.h"
#include "committee.h"

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
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/mem_fun.hpp>

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

	typedef std::set<boost::shared_ptr<StoredUser> > StoredUsers_t;
	typedef std::set<boost::shared_ptr<StoredNick> > StoredNicks_t;

	struct id {};
	struct name {};

	typedef boost::multi_index::multi_index_container<
				boost::shared_ptr<StoredChannel>,
				boost::multi_index::indexed_by<
					boost::multi_index::ordered_unique<boost::multi_index::identity<StoredChannel> >,
					boost::multi_index::ordered_unique<boost::multi_index::tag<id>,
													   BOOST_MULTI_INDEX_CONST_MEM_FUN(StoredChannel, boost::uint32_t, ID)>,
					boost::multi_index::ordered_unique<boost::multi_index::tag<name>,
													   BOOST_MULTI_INDEX_CONST_MEM_FUN(StoredChannel, const std::string &, Name),
													   mantra::iless<std::string> >
				> > StoredChannels_t;
	typedef StoredChannels_t::index<id>::type StoredChannels_by_id_t;
	typedef StoredChannels_t::index<name>::type StoredChannels_by_name_t;
//	typedef std::set<boost::shared_ptr<StoredChannel> > StoredChannels_t;
//
	typedef boost::multi_index::multi_index_container<
				boost::shared_ptr<Committee>,
				boost::multi_index::indexed_by<
					boost::multi_index::ordered_unique<boost::multi_index::identity<Committee> >,
					boost::multi_index::ordered_unique<boost::multi_index::tag<id>,
													   BOOST_MULTI_INDEX_CONST_MEM_FUN(Committee, boost::uint32_t, ID)>,
					boost::multi_index::ordered_unique<boost::multi_index::tag<name>,
													   BOOST_MULTI_INDEX_CONST_MEM_FUN(Committee, const std::string &, Name),
													   mantra::iless<std::string> >
				> > Committees_t;
	typedef Committees_t::index<id>::type Committees_by_id_t;
	typedef Committees_t::index<name>::type Committees_by_name_t;
//	typedef std::set<boost::shared_ptr<Committee> > Committees_t;

	boost::mutex lock_;
	std::pair<mantra::FinalStage *, void (*)(mantra::FinalStage *)> finalstage_;
	std::vector<std::pair<mantra::Stage *, void (*)(mantra::Stage *)> > stages_;
	std::pair<mantra::Storage *, void (*)(mantra::Storage *)> backend_;
	bool have_cascade;
	unsigned int event_, expire_event_;
	void *handle_, *crypt_handle_, *compress_handle_;
	mantra::Hasher hasher;

	LiveUsers_t RWSYNC(LiveUsers_);
	LiveChannels_t RWSYNC(LiveChannels_);

	StoredUsers_t RWSYNC(StoredUsers_);
	StoredNicks_t RWSYNC(StoredNicks_);

	StoredChannels_t RWSYNC(StoredChannels_);
	StoredChannels_by_id_t &StoredChannels_by_id_;
	StoredChannels_by_name_t &StoredChannels_by_name_;

	Committees_t RWSYNC(Committees_);
	Committees_by_id_t &Committees_by_id_;
	Committees_by_name_t &Committees_by_name_;

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
	boost::shared_ptr<StoredChannel> Get_StoredChannel(boost::uint32_t id,
			 boost::logic::tribool deep = boost::logic::indeterminate) const;
	boost::shared_ptr<StoredChannel> Get_StoredChannel(const std::string &name, // Convenience
			 boost::logic::tribool deep = boost::logic::indeterminate) const;
	boost::shared_ptr<Committee> Get_Committee(boost::uint32_t id,
			 boost::logic::tribool deep = boost::logic::indeterminate) const;
	boost::shared_ptr<Committee> Get_Committee(const std::string &name, // Convenience
			 boost::logic::tribool deep = boost::logic::indeterminate) const;

	// Check on ALL kinds of expirations ...
	void ExpireCheck();

	boost::uint32_t Forbid_Add(const std::string &in, const boost::shared_ptr<StoredNick> &nick);
	bool Forbid_Del(boost::uint32_t in);
	std::vector<std::string> Forbid_List_Nick() const;
	std::vector<std::string> Forbid_List_Channel() const;
	bool Forbid_Check(const std::string &in) const;
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
