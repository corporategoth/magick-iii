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
#include "otherdata.h"

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
class LiveChannel;
class Server;

class Storage
{
	friend class StorageInterface;
	template<typename T> friend class if_StorageDeleter;

public:
	typedef std::set<boost::shared_ptr<LiveUser> > LiveUsers_t;
	typedef std::set<boost::shared_ptr<LiveChannel> > LiveChannels_t;

	// server name -> (end-squit event, squit users)
	class SquitEntry : public boost::totally_ordered1<SquitEntry>,
					   public boost::totally_ordered2<SquitEntry, std::string>
	{
		size_t stage_;
		const std::string server_;
		const std::string uplink_;
		unsigned int event_;
		Storage::LiveUsers_t users_;

		void operator()();
	public:
		SquitEntry(const std::string &server, const std::string &uplink, size_t stage)
			: stage_(stage), server_(server), uplink_(uplink), event_(0){}
		~SquitEntry();

		size_t Stage() const { return stage_; }
		const std::string &Server() const { return server_; }
		const std::string &Uplink() const { return uplink_; }
		const LiveUsers_t &Users() const { return users_; }
	
		// Do a swap WITHOUT scheduling a new cleanup timer (cancel existing timer).
		void swap(Storage::LiveUsers_t &users);
		// Do a swap WITH scheduling a new cleanup timer.
		void swap(Storage::LiveUsers_t &users, const mantra::duration &duration);

		// Remove a single user from the users DB (for cleanup).
		void add(const boost::shared_ptr<LiveUser> &user);
		void remove(const boost::shared_ptr<LiveUser> &user);
		boost::shared_ptr<LiveUser> remove(const std::string &user);

		bool operator<(const std::string &s) const
			{ return server_ < s; }
		bool operator==(const std::string &s) const
			{ return server_ == s; }
		bool operator<(const SquitEntry &in) const
			{ return *this < in.server_; }
		bool operator==(const SquitEntry &in) const
			{ return *this == in.server_; }
	};

	typedef std::set<SquitEntry> SquitUsers_t;

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

	// If these change from 'set' you must update otherdata.h's sync()
	typedef std::set<Forbidden> Forbiddens_t;
	typedef std::set<Akill> Akills_t;
	typedef std::set<Clone> Clones_t;
	typedef std::set<OperDeny> OperDenies_t;
	typedef std::set<Ignore> Ignores_t;
	typedef std::set<KillChannel> KillChannels_t;

private:

	mutable boost::mutex lock_;
	std::pair<mantra::FinalStage *, void (*)(mantra::FinalStage *)> finalstage_;
	std::vector<std::pair<mantra::Stage *, void (*)(mantra::Stage *)> > stages_;
	std::pair<mantra::Storage *, void (*)(mantra::Storage *)> backend_;
	bool have_cascade;
	unsigned int event_, expire_event_;
	void *handle_, *crypt_handle_, *compress_handle_;
	mantra::Hasher hasher;

	LiveUsers_t RWSYNC(LiveUsers_);
	LiveChannels_t RWSYNC(LiveChannels_);

	// SQUIT's have three stages (for the purposes of SQUIT protection).
	// 1. Suspected SQUIT.  A user has signed off with a quit message consisting
	//    of two server names where one is the uplink of the other, which is
	//    usually the first sign of a SQUIT on networks without NOQUIT.
	//    ACTION: Move the user into the first stage squit storage for a short
	//            period of time until we get the SQUIT message.  If the timeout
	//            expires before we get the SQUIT message or the user signs back
	//            on again, Quit the user normally.
	// 2. Confirmed SQUIT.  We recieve the SQUIT message from the server, which
	//    will be the first notification on networks that support NOQUIT.
	//    ACTION: Move the users into the second stage squit storage until the
	//            longer squit timer times out.  If the server does not come back
	//            in that time, or the users signs on in that time, Quit the user
	//            normally.
	// 3. End SQUIT.  We receive a reconnect from the server.
	//    ACTION: Move all users in second stage squit storage to the third stage
	//            stage squit storage for a short period of time until they sign
	//            back on again.  If the timeout expires before they sign on or
	//            the user signs back on with different information than they
	//            signed off with, Quit the user normally.
	RWNSYNC(SquitUsers_);
	SquitUsers_t SquitUsers_[3];

	StoredUsers_t RWSYNC(StoredUsers_);
	StoredNicks_t RWSYNC(StoredNicks_);

	StoredChannels_t RWSYNC(StoredChannels_);
	StoredChannels_by_id_t &StoredChannels_by_id_;
	StoredChannels_by_name_t &StoredChannels_by_name_;

	Committees_t RWSYNC(Committees_);
	Committees_by_id_t &Committees_by_id_;
	Committees_by_name_t &Committees_by_name_;

	Forbiddens_t RWSYNC(Forbiddens_);
	Akills_t RWSYNC(Akills_);
	Clones_t RWSYNC(Clones_);
	OperDenies_t RWSYNC(OperDenies_);
	Ignores_t RWSYNC(Ignores_);
	KillChannels_t RWSYNC(KillChannels_);

	void init();
	void reset();
	template<typename T> void sync();

	void Del(const boost::shared_ptr<LiveUser> &entry);
	void Del(const boost::shared_ptr<LiveChannel> &entry);
	void Del(const boost::shared_ptr<Server> &entry); // special case (squit)
	void Del(const SquitEntry &entry);

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
	size_t Users() const;
	size_t Channels() const;

	void Load();
	void Save();
	boost::posix_time::ptime SaveTime() const;
	std::string CryptPassword(const std::string &in) const;

	// The 'server' entry does not actually store data here, merely
	// fixes up the SQUIT protection.
	void Add(const boost::shared_ptr<Server> &entry);
	void Add(const boost::shared_ptr<LiveUser> &entry);
	void Add(const boost::shared_ptr<LiveChannel> &entry);
	void Add(const boost::shared_ptr<StoredUser> &entry);
	void Add(const boost::shared_ptr<StoredNick> &entry);
	void Add(const boost::shared_ptr<StoredChannel> &entry);
	void Add(const boost::shared_ptr<Committee> &entry);

	void Rename(const boost::shared_ptr<LiveUser> &entry,
				const std::string &newname);
	void Squit(const boost::shared_ptr<LiveUser> &entry,
			   const boost::shared_ptr<Server> &server); // Level 1

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

	void Add(const Forbidden &entry);
	void Del(const Forbidden &entry);
	bool Reindex(const Forbidden &entry, boost::uint32_t num);
	Forbidden Get_Forbidden(boost::uint32_t in,
			boost::logic::tribool deep = boost::logic::indeterminate);
	void Get_Forbidden(std::set<Forbidden> &fill,
			boost::logic::tribool channel = boost::logic::indeterminate) const;
	Forbidden Matches_Forbidden(const std::string &in,
			boost::logic::tribool channel = boost::logic::indeterminate) const;

	void Add(const Akill &entry);
	void Del(const Akill &entry);
	bool Reindex(const Akill &entry, boost::uint32_t num);
	Akill Get_Akill(boost::uint32_t in,
			boost::logic::tribool deep = boost::logic::indeterminate);
	void Get_Akill(std::set<Akill> &fill) const;
	Akill Matches_Akill(const std::string &in) const;

	void Add(const Clone &entry);
	void Del(const Clone &entry);
	bool Reindex(const Clone &entry, boost::uint32_t num);
	Clone Get_Clone(boost::uint32_t in,
			boost::logic::tribool deep = boost::logic::indeterminate);
	void Get_Clone(std::set<Clone> &fill) const;
	Clone Matches_Clone(const std::string &in) const;

	void Add(const OperDeny &entry);
	void Del(const OperDeny &entry);
	bool Reindex(const OperDeny &entry, boost::uint32_t num);
	OperDeny Get_OperDeny(boost::uint32_t in,
			boost::logic::tribool deep = boost::logic::indeterminate);
	void Get_OperDeny(std::set<OperDeny> &fill) const;
	OperDeny Matches_OperDeny(const std::string &in) const;

	void Add(const Ignore &entry);
	void Del(const Ignore &entry);
	bool Reindex(const Ignore &entry, boost::uint32_t num);
	Ignore Get_Ignore(boost::uint32_t in,
			boost::logic::tribool deep = boost::logic::indeterminate);
	void Get_Ignore(std::set<Ignore> &fill) const;
	Ignore Matches_Ignore(const std::string &in) const;

	void Add(const KillChannel &entry);
	void Del(const KillChannel &entry);
	bool Reindex(const KillChannel &entry, boost::uint32_t num);
	KillChannel Get_KillChannel(boost::uint32_t in,
			boost::logic::tribool deep = boost::logic::indeterminate);
	void Get_KillChannel(std::set<KillChannel> &fill) const;
	KillChannel Matches_KillChannel(const std::string &in) const;

	// Check on ALL kinds of expirations ...
	void ExpireCheck();
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
