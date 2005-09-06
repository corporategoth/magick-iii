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
#define RCSID(x,y) const char *rcsid_magick__storage_cpp_ ## x () { return y; }
RCSID(magick__storage_cpp, "@(#)$Id$");

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

#include "liveuser.h"
#include "livechannel.h"
#include "storeduser.h"
#include "storednick.h"
#include "storedchannel.h"
#include "committee.h"

#include <dlfcn.h>

#include <mantra/core/utils.h>
#include <mantra/storage/filestage.h>
#include <mantra/storage/netstage.h>
#include <mantra/storage/cryptstage.h>
#include <mantra/storage/compressstage.h>
#include <mantra/storage/verifystage.h>

#include <mantra/storage/inifile.h>
#include <mantra/storage/xml.h>
#include <mantra/storage/mysql.h>
#include <mantra/storage/postgresql.h>
#include <mantra/storage/sqlite.h>
#include <mantra/storage/berkeleydb.h>

#include <boost/current_function.hpp>
#include <boost/timer.hpp>

namespace po = boost::program_options;

Storage::Storage()
	: finalstage_(std::pair<mantra::FinalStage *, void (*)(mantra::FinalStage *)>(NULL, NULL)),
	  backend_(std::pair<mantra::Storage *, void (*)(mantra::Storage *)>(NULL, NULL)),
	  have_cascade(false), event_(0), expire_event_(0),
	  handle_(NULL), crypt_handle_(NULL), compress_handle_(NULL),
	  hasher(mantra::Hasher::NONE), SYNC_NRWINIT(LiveUsers_, reader_priority),
	  SYNC_NRWINIT(LiveChannels_, reader_priority),
	  SYNC_NRWINIT(StoredUsers_, reader_priority),
	  SYNC_NRWINIT(StoredNicks_, reader_priority),
	  SYNC_NRWINIT(StoredChannels_, reader_priority),
		  StoredChannels_by_id_(StoredChannels_.get<id>()),
		  StoredChannels_by_name_(StoredChannels_.get<name>()),
	  SYNC_NRWINIT(Committees_, reader_priority),
		  Committees_by_id_(Committees_.get<id>()),
		  Committees_by_name_(Committees_.get<name>()),
	  SYNC_NRWINIT(Forbiddens_, reader_priority),
	  SYNC_NRWINIT(Akills_, reader_priority),
	  SYNC_NRWINIT(Clones_, reader_priority),
	  SYNC_NRWINIT(OperDenies_, reader_priority),
	  SYNC_NRWINIT(Ignores_, reader_priority),
	  SYNC_NRWINIT(KillChannels_, reader_priority)
{
}

Storage::~Storage()
{
	reset();
}

void Storage::reset()
{
	if (event_)
	{
		event_ = 0;
		ROOT->event->Cancel(event_);
	}

	if (backend_.first)
	{
		if (backend_.second)
			(*backend_.second)(backend_.first);
		else
			delete backend_.first;
		backend_ = std::pair<mantra::Storage *, void (*)(mantra::Storage *)>(NULL, NULL);
	}
	if (finalstage_.first)
	{
		if (finalstage_.second)
			(*finalstage_.second)(finalstage_.first);
		else
			delete finalstage_.first;
		finalstage_ = std::pair<mantra::FinalStage *, void (*)(mantra::FinalStage *)>(NULL, NULL);
	}
	for (size_t i=0; i<stages_.size(); ++i)
	{
		if (stages_[i].second)
			(*stages_[i].second)(stages_[i].first);
		else
			delete stages_[i].first;
	}
	stages_.clear();

	if (handle_)
	{
		dlclose(handle_);
		handle_ = NULL;
	}
	if (crypt_handle_)
	{
		dlclose(crypt_handle_);
		crypt_handle_ = NULL;
	}
	if (compress_handle_)
	{
		dlclose(compress_handle_);
		compress_handle_ = NULL;
	}

	{
		SYNC_WLOCK(StoredUsers_);
		StoredUsers_.clear();
	}
	{
		SYNC_WLOCK(StoredNicks_);
		StoredNicks_.clear();
	}
	{
		SYNC_WLOCK(StoredChannels_);
		StoredChannels_.clear();
	}
	{
		SYNC_WLOCK(Committees_);
		Committees_.clear();
	}
}

bool idless(const mantra::Storage::RecordMap &lhs,
			const mantra::Storage::RecordMap &rhs)
{
	mantra::Storage::RecordMap::const_iterator i = lhs.find("id");
	if (i == lhs.end())
		return false;
	mantra::Storage::RecordMap::const_iterator j = rhs.find("id");
	if (j == rhs.end())
		return true;

	return (boost::get<boost::uint32_t>(i->second) <
			boost::get<boost::uint32_t>(j->second));
}

bool nameless(const mantra::Storage::RecordMap &lhs,
			  const mantra::Storage::RecordMap &rhs)
{
	mantra::Storage::RecordMap::const_iterator i = lhs.find("name");
	if (i == lhs.end())
		return false;
	mantra::Storage::RecordMap::const_iterator j = rhs.find("name");
	if (j == rhs.end())
		return true;

	return (boost::get<boost::uint32_t>(i->second) <
			boost::get<boost::uint32_t>(j->second));
}

void Storage::sync()
{
	MT_EB
	MT_FUNC("Storage::sync");

	mantra::Storage::DataSet data;
	mantra::Storage::FieldSet fields;
	{
		data.clear();
		fields.clear();
		fields.insert("id");

		SYNC_WLOCK(StoredUsers_);
		std::queue<boost::shared_ptr<StoredUser> > newentries;
		backend_.first->RetrieveRow("users", data, mantra::ComparisonSet(), fields);
		std::sort(data.begin(), data.end(), idless);
		StoredUsers_t::iterator i = StoredUsers_.begin();
		mantra::Storage::DataSet::const_iterator j = data.begin();
		mantra::Storage::RecordMap::const_iterator k;
		while (i != StoredUsers_.end() && j != data.end())
		{
			k = j->find("id");
			if (k == j->end() || k->second.type() != typeid(boost::uint32_t))
			{
				// This should never happen!
				++j;
				continue;
			}
			boost::uint32_t id = boost::get<boost::uint32_t>(k->second);

			while (**i < id)
			{
				// Old entry, delete it.
				DelInternal(*i);
				StoredUsers_.erase(i++);
			}
			if (**i != id)
			{
				// New entry, add it.
				newentries.push(if_StoredUser_Storage::load(id));
			}
			else
				++i;
			++j;

		}
		// Old entries, remove them.
		while (i != StoredUsers_.end())
		{
			DelInternal(*i);
			StoredUsers_.erase(i++);
		}

		// New entries, add them.
		while (j != data.end())
		{
			k = j->find("id");
			if (k == j->end() || k->second.type() != typeid(boost::uint32_t))
			{
				// This should never happen!
				++j;
				continue;
			}
			boost::uint32_t id = boost::get<boost::uint32_t>(k->second);

			newentries.push(if_StoredUser_Storage::load(id));
			++j;
		}

		// Add the entries from above ...
		while (!newentries.empty())
		{
			StoredUsers_.insert(newentries.front());
			newentries.pop();
		}
	}

	{
		data.clear();
		fields.clear();
		fields.insert("name");
		fields.insert("id");

		SYNC_WLOCK(StoredNicks_);
		std::queue<boost::shared_ptr<StoredNick> > newentries;
		backend_.first->RetrieveRow("nicks", data, mantra::ComparisonSet(), fields);
		std::sort(data.begin(), data.end(), idless);
		StoredNicks_t::iterator i = StoredNicks_.begin();
		mantra::Storage::DataSet::const_iterator j = data.begin();
		mantra::Storage::RecordMap::const_iterator k;
		while (i != StoredNicks_.end() && j != data.end())
		{
			k = j->find("name");
			if (k == j->end() || k->second.type() != typeid(std::string))
			{
				// This should never happen!
				++j;
				continue;
			}
			std::string name = boost::get<std::string>(k->second);

			while (**i < name)
			{
				// Old entry, delete it.
				DelInternal(*i);
				StoredNicks_.erase(i++);
			}
			if (**i != name)
			{
				k = j->find("id");
				if (k == j->end() || k->second.type() != typeid(boost::uint32_t))
				{
					// This should never happen!
					++j;
					continue;
				}
				boost::shared_ptr<StoredUser> user = Get_StoredUser(
								boost::get<boost::uint32_t>(k->second));
				if (user)
				{
					// New entry, add it.
					newentries.push(if_StoredNick_Storage::load(name, user));
				}
			}
			else
				++i;
			++j;

		}
		// Old entries, remove them.
		while (i != StoredNicks_.end())
		{
			DelInternal(*i);
			StoredNicks_.erase(i++);
		}

		// New entries, add them.
		while (j != data.end())
		{
			k = j->find("name");
			if (k == j->end() || k->second.type() != typeid(std::string))
			{
				// This should never happen!
				++j;
				continue;
			}
			std::string name = boost::get<std::string>(k->second);

			k = j->find("id");
			if (k == j->end() || k->second.type() != typeid(boost::uint32_t))
			{
				// This should never happen!
				++j;
				continue;
			}
			boost::shared_ptr<StoredUser> user = Get_StoredUser(
							boost::get<boost::uint32_t>(k->second));
			if (user)
			{
				newentries.push(if_StoredNick_Storage::load(name, user));
			}
			++j;
		}

		// Add the entries from above ...
		while (!newentries.empty())
		{
			StoredNicks_.insert(newentries.front());
			newentries.pop();
		}
	}

	{
		data.clear();
		fields.clear();
		fields.insert("id");
		fields.insert("name");

		SYNC_WLOCK(StoredChannels_);
		std::queue<boost::shared_ptr<StoredChannel> > newentries;
		backend_.first->RetrieveRow("channels", data, mantra::ComparisonSet(), fields);
		std::sort(data.begin(), data.end(), idless);
		StoredChannels_by_id_t::iterator i = StoredChannels_by_id_.begin(),
				end = StoredChannels_by_id_.end();
		mantra::Storage::DataSet::const_iterator j = data.begin();
		mantra::Storage::RecordMap::const_iterator k;
		while (i != end && j != data.end())
		{
			k = j->find("id");
			if (k == j->end() || k->second.type() != typeid(boost::uint32_t))
			{
				// This should never happen!
				++j;
				continue;
			}
			boost::uint32_t id = boost::get<boost::uint32_t>(k->second);

			k = j->find("name");
			if (k == j->end() || k->second.type() != typeid(std::string))
			{
				// This should never happen!
				++j;
				continue;
			}
			std::string name = boost::get<std::string>(k->second);

			while (**i < id && i != end)
			{
				// Old entry, delete it.
				DelInternal(*i);
				StoredChannels_by_id_.erase(i++);
			}
			if (i == end)
				break;

			if (**i == id)
			{
				// New entry, add it.
				newentries.push(if_StoredChannel_Storage::load(id, name));
			}
			else
				++i;
			++j;

		}
		// Old entries, remove them.
		while (i != end)
		{
			DelInternal(*i);
			StoredChannels_by_id_.erase(i++);
		}

		// New entries, add them.
		while (j != data.end())
		{
			k = j->find("id");
			if (k == j->end() || k->second.type() != typeid(boost::uint32_t))
			{
				// This should never happen!
				++j;
				continue;
			}
			boost::uint32_t id = boost::get<boost::uint32_t>(k->second);

			k = j->find("name");
			if (k == j->end() || k->second.type() != typeid(std::string))
			{
				// This should never happen!
				++j;
				continue;
			}
			std::string name = boost::get<std::string>(k->second);

			newentries.push(if_StoredChannel_Storage::load(id, name));
			++j;
		}

		// Add the entries from above ...
		while (!newentries.empty())
		{
			StoredChannels_.insert(newentries.front());
			newentries.pop();
		}
	}

	{
		data.clear();
		fields.clear();
		fields.insert("id");
		fields.insert("name");

		SYNC_WLOCK(Committees_);
		std::queue<boost::shared_ptr<Committee> > newentries;
		backend_.first->RetrieveRow("committees", data, mantra::ComparisonSet(), fields);
		std::sort(data.begin(), data.end(), idless);
		Committees_by_id_t::iterator i = Committees_by_id_.begin(),
				end = Committees_by_id_.end();
		mantra::Storage::DataSet::const_iterator j = data.begin();
		mantra::Storage::RecordMap::const_iterator k;
		while (i != end && j != data.end())
		{
			k = j->find("id");
			if (k == j->end() || k->second.type() != typeid(boost::uint32_t))
			{
				// This should never happen!
				++j;
				continue;
			}
			boost::uint32_t id = boost::get<boost::uint32_t>(k->second);

			k = j->find("name");
			if (k == j->end() || k->second.type() != typeid(std::string))
			{
				// This should never happen!
				++j;
				continue;
			}
			std::string name = boost::get<std::string>(k->second);

			while (**i < id && i != end)
			{
				// Old entry, delete it.
				DelInternal(*i);
				Committees_by_id_.erase(i++);
			}
			if (i == end)
				break;

			if (**i == id)
			{
				// New entry, add it.
				newentries.push(if_Committee_Storage::load(id, name));
			}
			else
				++i;
			++j;

		}
		// Old entries, remove them.
		while (i != end)
		{
			DelInternal(*i);
			Committees_by_id_.erase(i++);
		}

		// New entries, add them.
		while (j != data.end())
		{
			k = j->find("id");
			if (k == j->end() || k->second.type() != typeid(boost::uint32_t))
			{
				// This should never happen!
				++j;
				continue;
			}
			boost::uint32_t id = boost::get<boost::uint32_t>(k->second);

			k = j->find("name");
			if (k == j->end() || k->second.type() != typeid(std::string))
			{
				// This should never happen!
				++j;
				continue;
			}
			std::string name = boost::get<std::string>(k->second);

			newentries.push(if_Committee_Storage::load(id, name));
			++j;
		}

		// Add the entries from above ...
		while (!newentries.empty())
		{
			Committees_.insert(newentries.front());
			newentries.pop();
		}
	}

	MT_EE
}

// Yes, its copied from config_parse.cpp.
template<typename T>
inline bool check_old_new(const char *entry, const po::variables_map &old_vm,
						  const po::variables_map &new_vm)
{
	MT_EB
	MT_FUNC("check_old_new" << entry << old_vm << new_vm);

	if (old_vm.count(entry) != new_vm.count(entry))
		MT_RET(false);

	// No clue why (old_vm[entry].as<T>() != new_vm[entry].as<T>())
	// will not work, but the below is the same anyway.
	if (new_vm.count(entry) &&
		boost::any_cast<T>(old_vm[entry].value()) !=
		boost::any_cast<T>(new_vm[entry].value()))
		MT_RET(false);

	MT_RET(true);
	MT_EE
}

bool Storage::init(const po::variables_map &vm, 
				   const po::variables_map &opt_config)
{
	MT_EB
	MT_FUNC("Storage::init" << vm);

	if (!check_old_new<unsigned int>("storage.password-hash", opt_config, vm))
		hasher = mantra::Hasher((mantra::Hasher::HashType) vm["storage.password-hash"].as<unsigned int>());

	std::string oldstorage;
	if (opt_config.count("storage"))
		oldstorage = opt_config["storage"].as<std::string>();

	std::string storage_module = vm["storage"].as<std::string>();
	bool samestorage = (storage_module == oldstorage);
	if (storage_module == "inifile")
	{
		if (!vm.count("storage.inifile.stage"))
		{
			LOG(Error, _("CFG: No stages_ found for %1%."),
				"storage.inifile");
			MT_RET(false);
		}

		std::vector<std::string> newstages_ = vm["storage.inifile.stage"].as<std::vector<std::string> >();
		if (opt_config.count("storage.inifile.stage"))
		{
			std::vector<std::string> oldstages_ = opt_config["storage.inifile.stage"].as<std::vector<std::string> >();

			if (newstages_.size() == oldstages_.size())
			{
				for (size_t i=0; i<newstages_.size(); ++i)
					if (newstages_[i] != oldstages_[i])
					{
						samestorage = false;
						break;
					}
			}
			else
				samestorage = false;
		}

		if (samestorage)
			samestorage = check_old_new<bool>("storage.inifile.tollerant", opt_config, vm);

		bool b = false;
		for (size_t i=0; i<newstages_.size(); ++i)
		{
			if (newstages_[i] == "file")
			{
				if (b)
				{
					LOG(Error, _("CFG: Multiple final stages_ specified for %1%."),
							"storage.inifile");
					MT_RET(false);
				}
				else
					b = true;

				if (!vm.count("storage.inifile.stage.file.name"))
				{
					LOG(Error, _("CFG: No file name specified for %1%."),
							"storage.inifile.stage.file");
					MT_RET(false);
				}

				if (samestorage)
					samestorage = check_old_new<std::string>("storage.inifile.stage.file.name", opt_config, vm);
			}
			else if (newstages_[i] == "net")
			{
				if (b)
				{
					LOG(Error, _("CFG: Multiple final stages_ specified for %1%."),
							"storage.inifile");
					MT_RET(false);
				}
				else
					b = true;

				if (!vm.count("storage.inifile.stage.net.host") ||
					!vm.count("storage.inifile.stage.net.port"))
				{
					LOG(Error, _("CFG: No host and/or port specified for %1%."),
						"storage.inifile.stage.net");
					MT_RET(false);
				}

				if (samestorage)
					samestorage = check_old_new<std::string>("storage.inifile.stage.net.host", opt_config, vm);
				if (samestorage)
					samestorage = check_old_new<boost::uint16_t>("storage.inifile.stage.net.port", opt_config, vm);
			}
			else if (newstages_[i] == "verify")
			{
				if (!vm.count("storage.inifile.stage.verify.string"))
				{
					LOG(Error, _("CFG: No verify string specified for %1%."),
						"storage.inifile.stage.verify");
					MT_RET(false);
				}

				if (samestorage)
					samestorage = check_old_new<unsigned int>("storage.inifile.stage.verify.offset", opt_config, vm);
				if (samestorage)
					samestorage = check_old_new<std::string>("storage.inifile.stage.verify.string", opt_config, vm);
				if (samestorage)
					samestorage = check_old_new<bool>("storage.inifile.stage.verify.nice", opt_config, vm);
			}
#ifdef MANTRA_STORAGE_STAGE_COMPRESS_SUPPORT
			else if (newstages_[i] == "compress")
			{
				if (samestorage)
					samestorage = check_old_new<unsigned int>("storage.inifile.stage.stage.level", opt_config, vm);
			}
#endif
			else if (newstages_[i] == "crypt")
			{
				size_t cnt = 0;
				if (vm.count("storage.inifile.stage.crypt.key"))
					++cnt;
				if (vm.count("storage.inifile.stage.crypt.keyfile"))
					++cnt;
				if (!cnt)
				{
					LOG(Error, _("CFG: No key or keyfile specified for %1%."),
							"storage.inifile.stage.crypt");
					MT_RET(false);
				}
				else if (cnt > 1)
				{
					LOG(Error, _("CFG: Only key or keyfile may be specified for %1%."),
						"storage.inifile.stage.crypt");
					MT_RET(false);
				}

				if (samestorage)
					samestorage = check_old_new<unsigned int>("storage.inifile.stage.crypt.type", opt_config, vm);
				if (samestorage)
					samestorage = check_old_new<unsigned int>("storage.inifile.stage.crypt.bits", opt_config, vm);
				if (samestorage)
					samestorage = check_old_new<unsigned int>("storage.inifile.stage.crypt.hash", opt_config, vm);
				if (samestorage)
					samestorage = check_old_new<std::string>("storage.inifile.stage.crypt.key", opt_config, vm);
				if (samestorage)
					samestorage = check_old_new<std::string>("storage.inifile.stage.crypt.keyfile", opt_config, vm);
			}
		}

		if (!b)
		{
			LOG(Error, _("CFG: No final stages_ specified for %1%."),
				"storage.inifile");
			MT_RET(false);
		}

		if (!samestorage)
		{
			boost::mutex::scoped_lock lock(lock_);
			reset();

			for (size_t i=0; i<newstages_.size(); ++i)
			{
				if (newstages_[i] == "file")
				{
					mantra::mfile f(vm["storage.inifile.stage.file.name"].as<std::string>(),
							O_RDWR | O_CREAT);
					finalstage_ = std::make_pair(new mantra::FileStage(f, true),
						(void (*)(mantra::FinalStage *)) &destroy_FileStage);
				}
				else if (newstages_[i] == "net")
				{
					std::string str = vm["storage.inifile.stage.net.host"].as<std::string>();
					boost::uint16_t port = vm["storage.inifile.stage.net.port"].as<boost::uint16_t>();

					mantra::Socket s;
					if (vm.count("bind"))
						s = mantra::Socket(mantra::Socket::STREAM, vm["bind"].as<std::string>());
					else
					{
						mantra::Socket::SockDomain_t domain = mantra::Socket::RemoteDomain(str);
						s = mantra::Socket(domain, mantra::Socket::STREAM);
					}
					if (!s.Valid())
					{
						NLOG(Error, _("CFG: Could not open storage socket."));
						MT_RET(false);
					}

					if (!s.StartConnect(str, port) || !s.CompleteConnect())
					{
						LOG(Error, _("CFG: Could not connect to remote storage host ([%1%]:%2%)."), str % port);
						MT_RET(false);
					}
					finalstage_ = std::make_pair(new mantra::NetStage(s, true),
						(void (*)(mantra::FinalStage *)) &destroy_NetStage);
				}
				else if (newstages_[i] == "verify")
				{
					mantra::Stage *s = new mantra::VerifyStage(
						vm["storage.inifile.stage.verify.offset"].as<boost::uint64_t>(),
						vm["storage.inifile.stage.verify.string"].as<std::string>(),
						vm["storage.inifile.stage.verify.nice"].as<bool>());
					stages_.push_back(std::make_pair(s,
						(void (*)(mantra::Stage *)) &destroy_VerifyStage));
				}
#ifdef MANTRA_STORAGE_STAGE_COMPRESS_SUPPORT
				else if (newstages_[i] == "compress")
				{
					if (!compress_handle_)
					{
						compress_handle_ = dlopen("libmantra_storage_stage_compress.so", RTLD_NOW | RTLD_GLOBAL);
						if (!compress_handle_)
						{
							LOG(Error, _("CFG: Failed to open %1% (%2%) - stage not loaded."),
								"libmantra_storage_stage_compress.so" % dlerror());
							MT_RET(false);
						}
					}
					mantra::CompressStage *(*create)(boost::int8_t) = 
						(mantra::CompressStage *(*)(boost::int8_t))
							dlsym(compress_handle_, "create_CompressStage");
					void (*destroy)(mantra::Stage *) = (void (*)(mantra::Stage *))
							dlsym(compress_handle_, "destroy_CompressStage");

					stages_.push_back(std::make_pair(
						create(vm["storage.inifile.stage.compress.level"].as<unsigned int>()),
						destroy));
				}
#endif
				else if (newstages_[i] == "crypt")
				{
					std::string key;
					if (vm.count("storage.inifile.stage.crypt.key"))
						key = vm["storage.inifile.stage.crypt.key"].as<std::string>();
					else if (vm.count("storage.inifile.stage.crypt.keyfile"))
					{
						mantra::mfile f(vm["storage.inifile.stage.crypt.keyfile"].as<std::string>());
						if (!f.IsOpened())
						{
							LOG(Error, _("CFG: Could not read key file %1%."), 
										vm["storage.inifile.stage.crypt.keyfile"].as<std::string>());
							MT_RET(false);
						}
						ssize_t rv;
						do
						{
							char buf[1024];
							rv = f.Read(buf, 1024);
							if (rv > 0)
								key.append(buf, rv);
						}
						while (rv == 1024);
					}
					else
					{
						LOG(Fatal, _("CFG: Logic error at %1%:%2% in %3% (no key specified)."),
							__FILE__ % __LINE__ % BOOST_CURRENT_FUNCTION);
					}

					if (!crypt_handle_)
					{
						crypt_handle_ = dlopen("libmantra_storage_stage_crypt.so", RTLD_NOW | RTLD_GLOBAL);
						if (!crypt_handle_)
						{
							LOG(Error, _("CFG: Failed to open %1% (%2%) - stage not loaded."),
								"libmantra_storage_stage_crypt.so" % dlerror());
							MT_RET(false);
						}
					}
					mantra::CryptStage *(*create)(mantra::CryptStage::CryptType,
						unsigned int, const std::string &, const mantra::Hasher &) =
						(mantra::CryptStage *(*)(mantra::CryptStage::CryptType,
						unsigned int, const std::string &, const mantra::Hasher &))
							dlsym(compress_handle_, "create_CryptStage");
					void (*destroy)(mantra::Stage *) = (void (*)(mantra::Stage *))
							dlsym(compress_handle_, "destroy_CryptStage");

					try
					{
						stages_.push_back(std::make_pair(
							create((mantra::CryptStage::CryptType) vm["storage.inifile.stage.crypt.type"].as<unsigned int>(),
							vm["storage.inifile.stage.crypt.bits"].as<unsigned int>(), key,
							mantra::Hasher((mantra::Hasher::HashType) vm["storage.inifile.stage.crypt.hash"].as<unsigned int>())),
							destroy));
					}
					catch (const mantra::crypt_stage_badbits &e)
					{
						LOG(Error, _("CFG: Failed to initialise crypt stage: %1%"), e.what());
						MT_RET(false);
					}
				}
			}
			std::vector<std::pair<mantra::Stage *, void (*)(mantra::Stage *)> >::reverse_iterator ri;
			for (ri = stages_.rbegin(); ri != stages_.rend(); ++ri)
				*finalstage_.first << *ri->first;

			backend_.second = (void (*)(mantra::Storage *)) destroy_IniFileStorage;
			backend_.first = create_IniFileStorage(*finalstage_.first, 
						vm["storage.inifile.tollerant"].as<bool>());
			have_cascade = false;
			init();
			if (event_)
			{
				backend_.first->Refresh();
				event_ = ROOT->event->Schedule(boost::bind(&Storage::Save, this),
											   ROOT->ConfigValue<mantra::duration>("save-time"));
			}
		}
	}
#ifdef MANTRA_STORAGE_BERKELEYDB_SUPPORT
	else if (storage_module == "berkeleydb")
	{
		if (!vm.count("storage.berkeleydb.db-dir"))
		{
			NLOG(Error, _("CFG: No DB directory specified for storage.berkeldydb."));
			MT_RET(false);
		}

		if (samestorage)
			samestorage = check_old_new<std::string>("storage.berkeleydb.db-dir", opt_config, vm);
		if (samestorage)
			samestorage = check_old_new<bool>("storage.berkeleydb.tollerant", opt_config, vm);
		if (samestorage)
			samestorage = check_old_new<bool>("storage.berkeleydb.private", opt_config, vm);
		if (samestorage)
			samestorage = check_old_new<std::string>("storage.berkeleydb.password", opt_config, vm);
		if (samestorage)
			samestorage = check_old_new<bool>("storage.berkeleydb.btree", opt_config, vm);

		if (!samestorage)
		{
			boost::mutex::scoped_lock lock(lock_);
			reset();

			handle_ = dlopen("libmantra_storage_berkeleydb.so", RTLD_NOW | RTLD_GLOBAL);
			if (!handle_)
			{
				LOG(Error, _("CFG: Failed to open %1% (%2%) - storage mechanism not loaded."),
					"libmantra_storage_berkeleydb.so" % dlerror());
				MT_RET(false);
			}

			backend_.second = (void (*)(mantra::Storage *)) dlsym(handle_, "destroy_BerkeleyDBStorage");
			mantra::BerkeleyDBStorage *(*create)(const char *, bool, bool, const char *, bool) =
						(mantra::BerkeleyDBStorage *(*)(const char *, bool, bool, const char *, bool))
						dlsym(handle_, "create_BerkeleyDBStorage");

			backend_.first = create(vm["storage.berkeleydb.db-dir"].as<std::string>().c_str(),
				vm["storage.berkeleydb.tollerant"].as<bool>(),
				vm["storage.berkeleydb.private"].as<bool>(),
				vm["storage.berkeleydb.password"].as<std::string>().c_str(),
				vm["storage.berkeleydb.btree"].as<bool>());
			have_cascade = false;
			init();
			if (event_)
			{
				backend_.first->Refresh();
				event_ = ROOT->event->Schedule(boost::bind(&Storage::Save, this),
											   ROOT->ConfigValue<mantra::duration>("save-time"));
			}
		}
	}
#endif
#ifdef MANTRA_STORAGE_XML_SUPPORT
	else if (storage_module == "xml")
	{
		if (!vm.count("storage.xml.stage"))
		{
			LOG(Error, _("CFG: No stages_ found for %1%."),
				"storage.xml");
			MT_RET(false);
		}

		std::vector<std::string> newstages_ = vm["storage.xml.stage"].as<std::vector<std::string> >();
		if (samestorage && opt_config.count("storage.xml.stage"))
		{
			std::vector<std::string> oldstages_ = opt_config["storage.xml.stage"].as<std::vector<std::string> >();

			if (newstages_.size() == oldstages_.size())
			{
				for (size_t i=0; i<newstages_.size(); ++i)
					if (newstages_[i] != oldstages_[i])
					{
						samestorage = false;
						break;
					}
			}
			else
				samestorage = false;
		}

		if (samestorage)
			samestorage = check_old_new<bool>("storage.xml.tollerant", opt_config, vm);

		bool b = false;
		for (size_t i=0; i<newstages_.size(); ++i)
		{
			if (newstages_[i] == "file")
			{
				if (b)
				{
					LOG(Error, _("CFG: Multiple final stages_ specified for %1%."),
							"storage.xml");
					MT_RET(false);
				}
				else
					b = true;

				if (!vm.count("storage.xml.stage.file.name"))
				{
					LOG(Error, _("CFG: No file name specified for %1%."),
						"storage.xml.stage.file");
					MT_RET(false);
				}

				if (samestorage)
					samestorage = check_old_new<std::string>("storage.xml.stage.file.name", opt_config, vm);
			}
			else if (newstages_[i] == "net")
			{
				if (b)
				{
					LOG(Error, _("CFG: Multiple final stages_ specified for %1%."),
						"storage.xml");
					MT_RET(false);
				}
				else
					b = true;

				if (!vm.count("storage.xml.stage.net.host") ||
					!vm.count("storage.xml.stage.net.port"))
				{
					LOG(Error, _("CFG: No host and/or port specified for %1%."),
						"storage.xml.stage.net");
					MT_RET(false);
				}

				if (samestorage)
					samestorage = check_old_new<std::string>("storage.xml.stage.net.host", opt_config, vm);
				if (samestorage)
					samestorage = check_old_new<boost::uint16_t>("storage.xml.stage.net.port", opt_config, vm);
			}
			else if (newstages_[i] == "verify")
			{
				if (!vm.count("storage.xml.stage.verify.string"))
				{
					LOG(Error, "No verify string specified for %1%.",
						"storage.xml.stage.verify");
					MT_RET(false);
				}

				if (samestorage)
					samestorage = check_old_new<unsigned int>("storage.xml.stage.verify.offset", opt_config, vm);
				if (samestorage)
					samestorage = check_old_new<std::string>("storage.xml.stage.verify.string", opt_config, vm);
				if (samestorage)
					samestorage = check_old_new<bool>("storage.xml.stage.verify.nice", opt_config, vm);
			}
#ifdef MANTRA_STORAGE_STAGE_COMPRESS_SUPPORT
			else if (newstages_[i] == "compress")
			{
				if (samestorage)
					samestorage = check_old_new<unsigned int>("storage.xml.stage.stage.level", opt_config, vm);
			}
#endif
			else if (newstages_[i] == "crypt")
			{
				size_t cnt = 0;
				if (vm.count("storage.xml.stage.crypt.key"))
					++cnt;
				if (vm.count("storage.xml.stage.crypt.keyfile"))
					++cnt;
				if (!cnt)
				{
					LOG(Error, "No key or keyfile specified for %1%.",
						"storage.xml.stage.crypt");
					MT_RET(false);
				}
				else if (cnt > 1)
				{
					LOG(Error, _("CFG: Only key or keyfile may be specified for %1%."),
						"storage.xml.stage.crypt");
					MT_RET(false);
				}

				if (samestorage)
					samestorage = check_old_new<unsigned int>("storage.xml.stage.crypt.type", opt_config, vm);
				if (samestorage)
					samestorage = check_old_new<unsigned int>("storage.xml.stage.crypt.bits", opt_config, vm);
				if (samestorage)
					samestorage = check_old_new<unsigned int>("storage.xml.stage.crypt.hash", opt_config, vm);
				if (samestorage)
					samestorage = check_old_new<std::string>("storage.xml.stage.crypt.key", opt_config, vm);
				if (samestorage)
					samestorage = check_old_new<std::string>("storage.xml.stage.crypt.keyfile", opt_config, vm);
			}
		}

		if (!b)
		{
			LOG(Error, _("CFG: No final stages_ specified for %1%."),
				"storage.xml");
			MT_RET(false);
		}

		if (!samestorage)
		{
			boost::mutex::scoped_lock lock(lock_);
			reset();

			for (size_t i=0; i<newstages_.size(); ++i)
			{
				if (newstages_[i] == "file")
				{
					mantra::mfile f(vm["storage.xml.stage.file.name"].as<std::string>(),
							O_RDWR | O_CREAT);
					finalstage_ = std::make_pair(new mantra::FileStage(f, true),
						(void (*)(mantra::FinalStage *)) &destroy_FileStage);
				}
				else if (newstages_[i] == "net")
				{
					std::string str = vm["storage.xml.stage.net.host"].as<std::string>();
					boost::uint16_t port = vm["storage.xml.stage.net.port"].as<boost::uint16_t>();

					mantra::Socket s;
					if (vm.count("bind"))
						s = mantra::Socket(mantra::Socket::STREAM, vm["bind"].as<std::string>());
					else
					{
						mantra::Socket::SockDomain_t domain = mantra::Socket::RemoteDomain(str);
						s = mantra::Socket(domain, mantra::Socket::STREAM);
					}
					if (!s.Valid())
					{
						NLOG(Error, _("CFG: Could not open storage socket."));
						MT_RET(false);
					}

					if (!s.StartConnect(str, port) || !s.CompleteConnect())
					{
						LOG(Error, _("CFG: Could not connect to remote storage host ([%1%]:%2%)."), str % port);
						MT_RET(false);
					}
					finalstage_ = std::make_pair(new mantra::NetStage(s, true),
						(void (*)(mantra::FinalStage *)) &destroy_NetStage);
				}
				else if (newstages_[i] == "verify")
				{
					mantra::Stage *s = new mantra::VerifyStage(
						vm["storage.xml.stage.verify.offset"].as<boost::uint64_t>(),
						vm["storage.xml.stage.verify.string"].as<std::string>(),
						vm["storage.xml.stage.verify.nice"].as<bool>());
					stages_.push_back(std::make_pair(s,
						(void (*)(mantra::Stage *)) &destroy_VerifyStage));
				}
#ifdef MANTRA_STORAGE_STAGE_COMPRESS_SUPPORT
				else if (newstages_[i] == "compress")
				{
					if (!compress_handle_)
					{
						compress_handle_ = dlopen("libmantra_storage_stage_compress.so", RTLD_NOW | RTLD_GLOBAL);
						if (!compress_handle_)
						{
							LOG(Error, _("CFG: Failed to open %1% (%2%) - stage not loaded."),
								"libmantra_storage_stage_compress.so" % dlerror());
							MT_RET(false);
						}
					}
					mantra::CompressStage *(*create)(boost::int8_t) = 
						(mantra::CompressStage *(*)(boost::int8_t))
							dlsym(compress_handle_, "create_CompressStage");
					void (*destroy)(mantra::Stage *) = (void (*)(mantra::Stage *))
							dlsym(compress_handle_, "destroy_CompressStage");

					stages_.push_back(std::make_pair(
						create(vm["storage.xml.stage.compress.level"].as<unsigned int>()),
						destroy));
				}
#endif
				else if (newstages_[i] == "crypt")
				{
					std::string key;
					if (vm.count("storage.xml.stage.crypt.key"))
						key = vm["storage.xml.stage.crypt.key"].as<std::string>();
					else if (vm.count("storage.xml.stage.crypt.keyfile"))
					{
						mantra::mfile f(vm["storage.xml.stage.crypt.keyfile"].as<std::string>());
						if (!f.IsOpened())
						{
							LOG(Error, _("CFG: Could not read key file %1%."), 
										vm["storage.xml.stage.crypt.keyfile"].as<std::string>());
							MT_RET(false);
						}
						ssize_t rv;
						do
						{
							char buf[1024];
							rv = f.Read(buf, 1024);
							if (rv > 0)
								key.append(buf, rv);
						}
						while (rv == 1024);
					}
					else
					{
						LOG(Fatal, _("CFG: Logic error at %1%:%2% in %3% (no key specified)."),
							__FILE__ % __LINE__ % BOOST_CURRENT_FUNCTION);
					}

					if (!crypt_handle_)
					{
						crypt_handle_ = dlopen("libmantra_storage_stage_crypt.so", RTLD_NOW | RTLD_GLOBAL);
						if (!crypt_handle_)
						{
							LOG(Error, _("CFG: Failed to open %1% (%2%) - stage not loaded."),
								"libmantra_storage_stage_crypt.so" % dlerror());
							MT_RET(false);
						}
					}
					mantra::CryptStage *(*create)(mantra::CryptStage::CryptType,
						unsigned int, const std::string &, const mantra::Hasher &) =
						(mantra::CryptStage *(*)(mantra::CryptStage::CryptType,
						unsigned int, const std::string &, const mantra::Hasher &))
							dlsym(compress_handle_, "create_CryptStage");
					void (*destroy)(mantra::Stage *) = (void (*)(mantra::Stage *))
							dlsym(compress_handle_, "destroy_CryptStage");

					try
					{
						stages_.push_back(std::make_pair(
							create((mantra::CryptStage::CryptType) vm["storage.xml.stage.crypt.type"].as<unsigned int>(),
							vm["storage.xml.stage.crypt.bits"].as<unsigned int>(), key,
							mantra::Hasher((mantra::Hasher::HashType) vm["storage.xml.stage.crypt.hash"].as<unsigned int>())),
							destroy));
					}
					catch (const mantra::crypt_stage_badbits &e)
					{
						LOG(Error, _("CFG: Failed to initialise crypt stage: %1%"), e.what());
						MT_RET(false);
					}
				}
			}
			std::vector<std::pair<mantra::Stage *, void (*)(mantra::Stage *)> >::reverse_iterator ri;
			for (ri = stages_.rbegin(); ri != stages_.rend(); ++ri)
				*finalstage_.first << *ri->first;

			handle_ = dlopen("libmantra_storage_xml.so", RTLD_NOW | RTLD_GLOBAL);
			if (!handle_)
			{
				LOG(Error, _("CFG: Failed to open %1% (%2%) - storage mechanism not loaded."),
					"libmantra_storage_xml.so" % dlerror());
				MT_RET(false);
			}

			backend_.second = (void (*)(mantra::Storage *)) dlsym(handle_, "destroy_XMLStorage");
			mantra::XMLStorage *(*create)(mantra::FinalStage &, bool t, const char *) =
						(mantra::XMLStorage *(*)(mantra::FinalStage &, bool t, const char *))
						dlsym(handle_, "create_XMLStorage");

			backend_.first = create(*finalstage_.first, vm["storage.xml.tollerant"].as<bool>(),
						vm["storage.xml.encoding"].as<std::string>().c_str());
			have_cascade = false;
			init();
			if (event_)
			{
				backend_.first->Refresh();
				event_ = ROOT->event->Schedule(boost::bind(&Storage::Save, this),
											   ROOT->ConfigValue<mantra::duration>("save-time"));
			}
		}
	}
#endif
#ifdef MANTRA_STORAGE_MYSQL_SUPPORT
	else if (storage_module == "mysql")
	{
		if (!vm.count("storage.mysql.db-name"))
		{
			LOG(Error, _("CFG: No DB name specified for %1%."),
				"storage.mysql");
			MT_RET(false);
		}

		if (samestorage)
			samestorage = check_old_new<std::string>("storage.mysql.db-name", opt_config, vm);
		if (samestorage)
			samestorage = check_old_new<std::string>("storage.mysql.user", opt_config, vm);
		if (samestorage)
			samestorage = check_old_new<std::string>("storage.mysql.password", opt_config, vm);
		if (samestorage)
			samestorage = check_old_new<std::string>("storage.mysql.host", opt_config, vm);
		if (samestorage)
			samestorage = check_old_new<boost::uint16_t>("storage.mysql.port", opt_config, vm);
		if (samestorage)
			samestorage = check_old_new<bool>("storage.mysql.tollerant", opt_config, vm);
		if (samestorage)
			samestorage = check_old_new<unsigned int>("storage.mysql.timeout", opt_config, vm);
		if (samestorage)
			samestorage = check_old_new<bool>("storage.mysql.compression", opt_config, vm);
		if (samestorage)
			samestorage = check_old_new<unsigned int>("storage.mysql.max-conn-count", opt_config, vm);

		if (!samestorage)
		{
			boost::mutex::scoped_lock lock(lock_);
			reset();

			std::string user, password, host;
			boost::uint16_t port = 0;
			if (vm.count("storage.mysql.user"))
				user = vm["storage.mysql.user"].as<std::string>();
			if (vm.count("storage.mysql.password"))
				password = vm["storage.mysql.password"].as<std::string>();
			if (vm.count("storage.mysql.host"))
				host = vm["storage.mysql.host"].as<std::string>();
			if (vm.count("storage.mysql.port"))
				port = vm["storage.mysql.port"].as<boost::uint16_t>();

			handle_ = dlopen("libmantra_storage_mysql.so", RTLD_NOW | RTLD_GLOBAL);
			if (!handle_)
			{
				LOG(Error, _("CFG: Failed to open %1% (%2%) - storage mechanism not loaded."),
					"libmantra_storage_mysql.so" % dlerror());
				MT_RET(false);
			}

			backend_.second = (void (*)(mantra::Storage *)) dlsym(handle_, "destroy_MySQLStorage");
			mantra::MySQLStorage *(*create)(const char *, const char *,
					const char *, const char *, boost::uint16_t, bool,
					unsigned int, bool, unsigned int) =
				(mantra::MySQLStorage *(*)(const char *, const char *,
					const char *, const char *, boost::uint16_t, bool,
					unsigned int, bool, unsigned int))
						dlsym(handle_, "create_MySQLStorage");

			backend_.first = create(vm["storage.mysql.db-name"].as<std::string>().c_str(),
				user.empty() ? NULL : user.c_str(),
				password.empty() ? NULL : password.c_str(),
				host.empty() ? NULL : host.c_str(), port,
				vm["storage.mysql.tollerant"].as<bool>(),
				vm["storage.mysql.timeout"].as<unsigned int>(),
				vm["storage.mysql.compression"].as<bool>(),
				vm["storage.mysql.max-conn-count"].as<unsigned int>());
			have_cascade = false;
			init();
			if (event_)
			{
				backend_.first->Refresh();
				event_ = ROOT->event->Schedule(boost::bind(&Storage::Save, this),
											   ROOT->ConfigValue<mantra::duration>("save-time"));
			}
		}
	}
#endif
#ifdef MANTRA_STORAGE_POSTGRESQL_SUPPORT
	else if (storage_module == "postgresql")
	{
		if (!vm.count("storage.postgresql.db-name"))
		{
			LOG(Error, _("CFG: No DB name specified for %1%."),
				"storage.postgresql");
			MT_RET(false);
		}

		if (samestorage)
			samestorage = check_old_new<std::string>("storage.postgresql.db-name", opt_config, vm);
		if (samestorage)
			samestorage = check_old_new<std::string>("storage.postgresql.user", opt_config, vm);
		if (samestorage)
			samestorage = check_old_new<std::string>("storage.postgresql.password", opt_config, vm);
		if (samestorage)
			samestorage = check_old_new<std::string>("storage.postgresql.host", opt_config, vm);
		if (samestorage)
			samestorage = check_old_new<boost::uint16_t>("storage.postgresql.port", opt_config, vm);
		if (samestorage)
			samestorage = check_old_new<bool>("storage.postgresql.tollerant", opt_config, vm);
		if (samestorage)
			samestorage = check_old_new<unsigned int>("storage.postgresql.timeout", opt_config, vm);
		if (samestorage)
			samestorage = check_old_new<bool>("storage.postgresql.ssl-only", opt_config, vm);
		if (samestorage)
			samestorage = check_old_new<unsigned int>("storage.postgresql.max-conn-count", opt_config, vm);

		if (!samestorage)
		{
			boost::mutex::scoped_lock lock(lock_);
			reset();

			std::string user, password, host;
			boost::uint16_t port = 0;
			if (vm.count("storage.postgresql.user"))
				user = vm["storage.postgresql.user"].as<std::string>();
			if (vm.count("storage.postgresql.password"))
				password = vm["storage.postgresql.password"].as<std::string>();
			if (vm.count("storage.postgresql.host"))
				host = vm["storage.postgresql.host"].as<std::string>();
			if (vm.count("storage.postgresql.port"))
				port = vm["storage.postgresql.port"].as<boost::uint16_t>();

			handle_ = dlopen("libmantra_storage_postgresql.so", RTLD_NOW | RTLD_GLOBAL);
			if (!handle_)
			{
				LOG(Error, _("CFG: Failed to open %1% (%2%) - storage mechanism not loaded."),
					"libmantra_storage_postgresql.so" % dlerror());
				MT_RET(false);
			}

			backend_.second = (void (*)(mantra::Storage *)) dlsym(handle_, "destroy_PostgreSQLStorage");
			mantra::PostgreSQLStorage *(*create)(const char *, const char *,
					const char *, const char *, boost::uint16_t, bool,
					unsigned int, bool, unsigned int) =
				(mantra::PostgreSQLStorage *(*)(const char *, const char *,
					const char *, const char *, boost::uint16_t, bool,
					unsigned int, bool, unsigned int))
						dlsym(handle_, "create_PostgreSQLStorage");

			backend_.first = create(vm["storage.postgresql.db-name"].as<std::string>().c_str(),
				user.empty() ? NULL : user.c_str(),
				password.empty() ? NULL : password.c_str(),
				host.empty() ? NULL : host.c_str(), port,
				vm["storage.postgresql.tollerant"].as<bool>(),
				vm["storage.postgresql.timeout"].as<unsigned int>(),
				vm["storage.postgresql.ssl-only"].as<bool>(),
				vm["storage.postgresql.max-conn-count"].as<unsigned int>());
			have_cascade = true;
			init();
			if (event_)
			{
				backend_.first->Refresh();
				event_ = ROOT->event->Schedule(boost::bind(&Storage::Save, this),
											   ROOT->ConfigValue<mantra::duration>("save-time"));
			}
		}
	}
#endif
#ifdef MANTRA_STORAGE_SQLITE_SUPPORT
	else if (storage_module == "sqlite")
	{
		if (!vm.count("storage.sqlite.db-name"))
		{
			LOG(Error, _("CFG: No DB name specified for %1%."),
				"storage.sqlite");
			MT_RET(false);
		}

		if (samestorage)
			samestorage = check_old_new<std::string>("storage.sqlite.db-name", opt_config, vm);
		if (samestorage)
			samestorage = check_old_new<bool>("storage.sqlite.tollerant", opt_config, vm);
		if (samestorage)
			samestorage = check_old_new<unsigned int>("storage.sqlite.max-conn-count", opt_config, vm);

		if (!samestorage)
		{
			boost::mutex::scoped_lock lock(lock_);
			reset();

			handle_ = dlopen("libmantra_storage_sqlite.so", RTLD_NOW | RTLD_GLOBAL);
			if (!handle_)
			{
				LOG(Error, _("CFG: Failed to open %1% (%2%) - storage mechanism not loaded."),
					"libmantra_storage_sqlite.so" % dlerror());
				MT_RET(false);
			}

			backend_.second = (void (*)(mantra::Storage *)) dlsym(handle_, "destroy_SQLiteStorage");
			mantra::SQLiteStorage *(*create)(const char *, bool, unsigned int) =
				(mantra::SQLiteStorage *(*)(const char *, bool, unsigned int))
						dlsym(handle_, "create_SQLiteStorage");

			backend_.first = create(vm["storage.sqlite.db-name"].as<std::string>().c_str(),
				vm["storage.sqlite.tollerant"].as<bool>(),
				vm["storage.sqlite.max-conn-count"].as<unsigned int>());
			have_cascade = false;
			init();
			if (event_)
			{
				backend_.first->Refresh();
				event_ = ROOT->event->Schedule(boost::bind(&Storage::Save, this),
											   ROOT->ConfigValue<mantra::duration>("save-time"));
			}
		}
	}
#endif

	MT_RET(true);
	MT_EE
}

static mantra::StorageValue GetCurrentDateTime()
{
	return mantra::GetCurrentDateTime();
}

void Storage::init()
{
	MT_EB
	MT_FUNC("Storage::init");

	backend_.first->DefineTable("users");
	backend_.first->DefineTable("users_access");
	backend_.first->DefineTable("users_ignore");
	backend_.first->DefineTable("users_memo");
	backend_.first->DefineTable("nicks");
	backend_.first->DefineTable("channels");
	backend_.first->DefineTable("channels_level");
	backend_.first->DefineTable("channels_access");
	backend_.first->DefineTable("channels_akick");
	backend_.first->DefineTable("channels_greet");
	backend_.first->DefineTable("channels_message");
	backend_.first->DefineTable("channels_news");
	backend_.first->DefineTable("channels_news_read");
	backend_.first->DefineTable("committees");
	backend_.first->DefineTable("committees_member");
	backend_.first->DefineTable("committees_message");
	backend_.first->DefineTable("forbidden");
	backend_.first->DefineTable("akills");
	backend_.first->DefineTable("clones");
	backend_.first->DefineTable("ignores");
	backend_.first->DefineTable("operdenies");
	backend_.first->DefineTable("killchans");

	mantra::Storage::ColumnProperties_t cp;

	// TABLE: users
	cp.Assign<boost::uint32_t>(false);
	backend_.first->DefineColumn("users", "id", cp);

	cp.Assign<boost::posix_time::ptime>(false, boost::function0<mantra::StorageValue>(&GetCurrentDateTime));
	backend_.first->DefineColumn("users", "last_update", cp);

	cp.Assign<std::string>(false, (boost::uint64_t) 0, (boost::uint64_t) 64);
	backend_.first->DefineColumn("users", "password", cp);

	cp.Assign<std::string>(true, (boost::uint64_t) 0, (boost::uint64_t) 320);
	backend_.first->DefineColumn("users", "email", cp);
	cp.Assign<std::string>(true, (boost::uint64_t) 0, (boost::uint64_t) 2048);
	backend_.first->DefineColumn("users", "website", cp);
	cp.Assign<boost::uint32_t>(true);
	backend_.first->DefineColumn("users", "icq", cp);
	cp.Assign<std::string>(true, (boost::uint64_t) 0, (boost::uint64_t) 16);
	backend_.first->DefineColumn("users", "aim", cp);
	cp.Assign<std::string>(true, (boost::uint64_t) 0, (boost::uint64_t) 320);
	backend_.first->DefineColumn("users", "msn", cp);
	cp.Assign<std::string>(true, (boost::uint64_t) 0, (boost::uint64_t) 512);
	backend_.first->DefineColumn("users", "jabber", cp);
	cp.Assign<std::string>(true, (boost::uint64_t) 0, (boost::uint64_t) 32);
	backend_.first->DefineColumn("users", "yahoo", cp);
	cp.Assign<std::string>(true);
	backend_.first->DefineColumn("users", "description", cp);
	backend_.first->DefineColumn("users", "comment", cp);

	cp.Assign<std::string>(true, (boost::uint64_t) 0, (boost::uint64_t) 32);
	backend_.first->DefineColumn("users", "suspend_by", cp);
	cp.Assign<boost::uint32_t>(true);
	backend_.first->DefineColumn("users", "suspend_by_id", cp);
	cp.Assign<std::string>(true);
	backend_.first->DefineColumn("users", "suspend_reason", cp);
	cp.Assign<boost::posix_time::ptime>(true);
	backend_.first->DefineColumn("users", "suspend_time", cp);

	cp.Assign<std::string>(true, (boost::uint64_t) 0, (boost::uint64_t) 16);
	backend_.first->DefineColumn("users", "language", cp);
	cp.Assign<bool>(true);
	backend_.first->DefineColumn("users", "protect", cp);
	backend_.first->DefineColumn("users", "secure", cp);
	backend_.first->DefineColumn("users", "nomemo", cp);
	backend_.first->DefineColumn("users", "private", cp);
	backend_.first->DefineColumn("users", "privmsg", cp);
	backend_.first->DefineColumn("users", "noexpire", cp);
	cp.Assign<boost::uint32_t>(true);
	backend_.first->DefineColumn("users", "picture", cp);
	cp.Assign<std::string>(true, (boost::uint64_t) 0, (boost::uint64_t) 8);
	backend_.first->DefineColumn("users", "picture_ext", cp);
	cp.Assign<bool>(true);
	backend_.first->DefineColumn("users", "lock_language", cp);
	backend_.first->DefineColumn("users", "lock_protect", cp);
	backend_.first->DefineColumn("users", "lock_secure", cp);
	backend_.first->DefineColumn("users", "lock_nomemo", cp);
	backend_.first->DefineColumn("users", "lock_private", cp);
	backend_.first->DefineColumn("users", "lock_privmsg", cp);

	// TABLE: users_access
	cp.Assign<boost::uint32_t>(false);
	backend_.first->DefineColumn("users_access", "id", cp);
	backend_.first->DefineColumn("users_access", "number", cp);
	cp.Assign<std::string>(false, (boost::uint64_t) 0, (boost::uint64_t) 320);
	backend_.first->DefineColumn("users_access", "mask", cp);
	cp.Assign<boost::posix_time::ptime>(false, boost::function0<mantra::StorageValue>(&GetCurrentDateTime));
	backend_.first->DefineColumn("users_access", "last_update", cp);

	// TABLE: users_ignore
	cp.Assign<boost::uint32_t>(false);
	backend_.first->DefineColumn("users_ignore", "id", cp);
	backend_.first->DefineColumn("users_ignore", "number", cp);
	backend_.first->DefineColumn("users_ignore", "entry", cp);
	cp.Assign<boost::posix_time::ptime>(false, boost::function0<mantra::StorageValue>(&GetCurrentDateTime));
	backend_.first->DefineColumn("users_ignore", "last_update", cp);

	// TABLE: users_memo
	cp.Assign<boost::uint32_t>(false);
	backend_.first->DefineColumn("users_memo", "id", cp);
	cp.Assign<boost::uint32_t>(false);
	backend_.first->DefineColumn("users_memo", "number", cp);
	cp.Assign<boost::posix_time::ptime>(false);
	backend_.first->DefineColumn("users_memo", "sent", cp);
	cp.Assign<std::string>(false, (boost::uint64_t) 0, (boost::uint64_t) 32);
	backend_.first->DefineColumn("users_memo", "sender", cp);
	cp.Assign<boost::uint32_t>(true);
	backend_.first->DefineColumn("users_memo", "sender_id", cp);
	cp.Assign<std::string>(false);
	backend_.first->DefineColumn("users_memo", "message", cp);
	cp.Assign<bool>(true);
	backend_.first->DefineColumn("users_memo", "read", cp);
	cp.Assign<boost::uint32_t>(true);
	backend_.first->DefineColumn("users_memo", "attachment", cp);

	// TABLE: nicks
	cp.Assign<std::string>(false, (boost::uint64_t) 0, (boost::uint64_t) 32);
	backend_.first->DefineColumn("nicks", "name", cp);
	cp.Assign<boost::uint32_t>(false);
	backend_.first->DefineColumn("nicks", "id", cp);
	cp.Assign<boost::posix_time::ptime>(false, boost::function0<mantra::StorageValue>(&GetCurrentDateTime));
	backend_.first->DefineColumn("nicks", "registered", cp);
	cp.Assign<std::string>(true, (boost::uint64_t) 0, (boost::uint64_t) 50);
	backend_.first->DefineColumn("nicks", "last_realname", cp);
	cp.Assign<std::string>(true, (boost::uint64_t) 0, (boost::uint64_t) 320);
	backend_.first->DefineColumn("nicks", "last_mask", cp);
	cp.Assign<std::string>(true, (boost::uint64_t) 0, (boost::uint64_t) 300);
	backend_.first->DefineColumn("nicks", "last_quit", cp);
	cp.Assign<boost::posix_time::ptime>(true);
	backend_.first->DefineColumn("nicks", "last_seen", cp);

	// TABLE: channels
	cp.Assign<boost::uint32_t>(false);
	backend_.first->DefineColumn("channels", "id", cp);
	cp.Assign<std::string>(false, (boost::uint64_t) 0, (boost::uint64_t) 64);
	backend_.first->DefineColumn("channels", "name", cp);
	cp.Assign<boost::posix_time::ptime>(false, boost::function0<mantra::StorageValue>(&GetCurrentDateTime));
	backend_.first->DefineColumn("channels", "registered", cp);
	backend_.first->DefineColumn("channels", "last_update", cp);
	cp.Assign<boost::posix_time::ptime>(true);
	backend_.first->DefineColumn("channels", "last_used", cp);
	cp.Assign<std::string>(false, (boost::uint64_t) 0, (boost::uint64_t) 64);
	backend_.first->DefineColumn("channels", "password", cp);
	cp.Assign<boost::uint32_t>(false);
	backend_.first->DefineColumn("channels", "founder", cp);
	cp.Assign<boost::uint32_t>(true);
	backend_.first->DefineColumn("channels", "successor", cp);
	cp.Assign<std::string>(true);
	backend_.first->DefineColumn("channels", "description", cp);
	backend_.first->DefineColumn("channels", "email", cp);
	backend_.first->DefineColumn("channels", "website", cp);
	backend_.first->DefineColumn("channels", "comment", cp);
	backend_.first->DefineColumn("channels", "topic", cp);
	cp.Assign<std::string>(true, (boost::uint64_t) 0, (boost::uint64_t) 32);
	backend_.first->DefineColumn("channels", "topic_setter", cp);
	cp.Assign<boost::posix_time::ptime>(true);
	backend_.first->DefineColumn("channels", "topic_set_time", cp);

	cp.Assign<bool>(true);
	backend_.first->DefineColumn("channels", "keeptopic", cp);
	backend_.first->DefineColumn("channels", "topiclock", cp);
	backend_.first->DefineColumn("channels", "private", cp);
	backend_.first->DefineColumn("channels", "secureops", cp);
	backend_.first->DefineColumn("channels", "secure", cp);
	backend_.first->DefineColumn("channels", "anarchy", cp);
	backend_.first->DefineColumn("channels", "kickonban", cp);
	backend_.first->DefineColumn("channels", "restricted", cp);
	backend_.first->DefineColumn("channels", "cjoin", cp);
	backend_.first->DefineColumn("channels", "noexpire", cp);
	cp.Assign<mantra::duration>(true);
	backend_.first->DefineColumn("channels", "bantime", cp);
	backend_.first->DefineColumn("channels", "parttime", cp);
	cp.Assign<boost::uint8_t>(true);
	backend_.first->DefineColumn("channels", "revenge", cp);
	cp.Assign<std::string>(true, (boost::uint64_t) 0, (boost::uint64_t) 32);
	backend_.first->DefineColumn("channels", "mlock_on", cp);
	backend_.first->DefineColumn("channels", "mlock_off", cp);
	backend_.first->DefineColumn("channels", "mlock_key", cp);
	cp.Assign<boost::uint32_t>(true);
	backend_.first->DefineColumn("channels", "mlock_limit", cp);
	cp.Assign<bool>(true);
	backend_.first->DefineColumn("channels", "lock_keeptopic", cp);
	backend_.first->DefineColumn("channels", "lock_topiclock", cp);
	backend_.first->DefineColumn("channels", "lock_private", cp);
	backend_.first->DefineColumn("channels", "lock_secureops", cp);
	backend_.first->DefineColumn("channels", "lock_secure", cp);
	backend_.first->DefineColumn("channels", "lock_anarchy", cp);
	backend_.first->DefineColumn("channels", "lock_kickonban", cp);
	backend_.first->DefineColumn("channels", "lock_restricted", cp);
	backend_.first->DefineColumn("channels", "lock_cjoin", cp);
	backend_.first->DefineColumn("channels", "lock_bantime", cp);
	backend_.first->DefineColumn("channels", "lock_parttime", cp);
	backend_.first->DefineColumn("channels", "lock_revenge", cp);
	cp.Assign<std::string>(true, (boost::uint64_t) 0, (boost::uint64_t) 32);
	backend_.first->DefineColumn("channels", "lock_mlock_on", cp);
	backend_.first->DefineColumn("channels", "lock_mlock_off", cp);
	backend_.first->DefineColumn("channels", "suspend_by", cp);
	cp.Assign<boost::uint32_t>(true);
	backend_.first->DefineColumn("channels", "suspend_by_id", cp);
	cp.Assign<std::string>(true);
	backend_.first->DefineColumn("channels", "suspend_reason", cp);
	cp.Assign<boost::posix_time::ptime>(true);
	backend_.first->DefineColumn("channels", "suspend_time", cp);

	// TABLE: channels_level
	cp.Assign<boost::uint32_t>(false);
	backend_.first->DefineColumn("channels_level", "id", cp);
	backend_.first->DefineColumn("channels_level", "level", cp);
	cp.Assign<boost::int32_t>(false);
	backend_.first->DefineColumn("channels_level", "value", cp);
	cp.Assign<std::string>(false, (boost::uint64_t) 0, (boost::uint64_t) 32);
	backend_.first->DefineColumn("channels_level", "last_updater", cp);
	cp.Assign<boost::int32_t>(true);
	backend_.first->DefineColumn("channels_level", "last_updater_id", cp);
	cp.Assign<boost::posix_time::ptime>(false, boost::function0<mantra::StorageValue>(&GetCurrentDateTime));
	backend_.first->DefineColumn("channels_level", "last_update", cp);

	// TABLE: channel_access
	cp.Assign<boost::uint32_t>(false);
	backend_.first->DefineColumn("channels_access", "id", cp);
	cp.Assign<boost::uint32_t>(false);
	backend_.first->DefineColumn("channels_access", "number", cp);
	cp.Assign<std::string>(true, (boost::uint64_t) 0, (boost::uint64_t) 350);
	backend_.first->DefineColumn("channels_access", "entry_mask", cp);
	cp.Assign<boost::uint32_t>(true);
	backend_.first->DefineColumn("channels_access", "entry_user", cp);
	backend_.first->DefineColumn("channels_access", "entry_committee", cp);
	cp.Assign<boost::int32_t>(false);
	backend_.first->DefineColumn("channels_access", "level", cp);
	cp.Assign<std::string>(false, (boost::uint64_t) 0, (boost::uint64_t) 32);
	backend_.first->DefineColumn("channels_access", "last_updater", cp);
	cp.Assign<boost::int32_t>(true);
	backend_.first->DefineColumn("channels_access", "last_updater_id", cp);
	cp.Assign<boost::posix_time::ptime>(false, boost::function0<mantra::StorageValue>(&GetCurrentDateTime));
	backend_.first->DefineColumn("channels_access", "last_update", cp);

	// TABLE: channel_akick
	cp.Assign<boost::uint32_t>(false);
	backend_.first->DefineColumn("channels_akick", "id", cp);
	backend_.first->DefineColumn("channels_akick", "number", cp);
	cp.Assign<std::string>(true, (boost::uint64_t) 0, (boost::uint64_t) 350);
	backend_.first->DefineColumn("channels_akick", "entry_mask", cp);
	cp.Assign<boost::uint32_t>(true);
	backend_.first->DefineColumn("channels_akick", "entry_user", cp);
	backend_.first->DefineColumn("channels_akick", "entry_committee", cp);
	cp.Assign<std::string>(false);
	backend_.first->DefineColumn("channels_akick", "reason", cp);
	cp.Assign<boost::posix_time::ptime>(false, boost::function0<mantra::StorageValue>(&GetCurrentDateTime));
	backend_.first->DefineColumn("channels_akick", "creation", cp);
	cp.Assign<mantra::duration>(true);
	backend_.first->DefineColumn("channels_akick", "length", cp);
	cp.Assign<std::string>(false, (boost::uint64_t) 0, (boost::uint64_t) 32);
	backend_.first->DefineColumn("channels_akick", "last_updater", cp);
	cp.Assign<boost::int32_t>(true);
	backend_.first->DefineColumn("channels_akick", "last_updater_id", cp);
	cp.Assign<boost::posix_time::ptime>(false, boost::function0<mantra::StorageValue>(&GetCurrentDateTime));
	backend_.first->DefineColumn("channels_akick", "last_update", cp);

	// TABLE: channels_greet
	cp.Assign<boost::uint32_t>(false);
	backend_.first->DefineColumn("channels_greet", "id", cp);
	backend_.first->DefineColumn("channels_greet", "entry", cp);
	cp.Assign<std::string>(false);
	backend_.first->DefineColumn("channels_greet", "greeting", cp);
	cp.Assign<bool>(true);
	backend_.first->DefineColumn("channels_greet", "locked", cp);
	cp.Assign<std::string>(false, (boost::uint64_t) 0, (boost::uint64_t) 32);
	backend_.first->DefineColumn("channels_greet", "last_updater", cp);
	cp.Assign<boost::int32_t>(true);
	backend_.first->DefineColumn("channels_greet", "last_updater_id", cp);
	cp.Assign<boost::posix_time::ptime>(false, boost::function0<mantra::StorageValue>(&GetCurrentDateTime));
	backend_.first->DefineColumn("channels_greet", "last_update", cp);

	// TABLE: channels_message
	cp.Assign<boost::uint32_t>(false);
	backend_.first->DefineColumn("channels_message", "id", cp);
	backend_.first->DefineColumn("channels_message", "number", cp);
	cp.Assign<std::string>(false);
	backend_.first->DefineColumn("channels_message", "message", cp);
	cp.Assign<std::string>(false, (boost::uint64_t) 0, (boost::uint64_t) 32);
	backend_.first->DefineColumn("channels_message", "last_updater", cp);
	cp.Assign<boost::int32_t>(true);
	backend_.first->DefineColumn("channels_message", "last_updater_id", cp);
	cp.Assign<boost::posix_time::ptime>(false, boost::function0<mantra::StorageValue>(&GetCurrentDateTime));
	backend_.first->DefineColumn("channels_message", "last_update", cp);

	// TABLE: channels_news
	cp.Assign<boost::uint32_t>(false);
	backend_.first->DefineColumn("channels_news", "id", cp);
	backend_.first->DefineColumn("channels_news", "number", cp);
	cp.Assign<std::string>(false, (boost::uint64_t) 0, (boost::uint64_t) 32);
	backend_.first->DefineColumn("channels_news", "sender", cp);
	cp.Assign<boost::uint32_t>(true);
	backend_.first->DefineColumn("channels_news", "sender_id", cp);
	cp.Assign<boost::posix_time::ptime>(false);
	backend_.first->DefineColumn("channels_news", "sent", cp);
	cp.Assign<std::string>(false);
	backend_.first->DefineColumn("channels_news", "message", cp);

	// TABLE: channels_news_read
	cp.Assign<boost::uint32_t>(false);
	backend_.first->DefineColumn("channels_news_read", "id", cp);
	backend_.first->DefineColumn("channels_news_read", "number", cp);
	backend_.first->DefineColumn("channels_news_read", "entry", cp);

	// TABLE: committees
	cp.Assign<boost::uint32_t>(false);
	backend_.first->DefineColumn("committees", "id", cp);
	cp.Assign<std::string>(false, (boost::uint64_t) 0, (boost::uint64_t) 32);
	backend_.first->DefineColumn("committees", "name", cp);

	cp.Assign<boost::uint32_t>(true);
	backend_.first->DefineColumn("committees", "head_user", cp);
	backend_.first->DefineColumn("committees", "head_committee", cp);

	cp.Assign<boost::posix_time::ptime>(false, boost::function0<mantra::StorageValue>(&GetCurrentDateTime));
	backend_.first->DefineColumn("committees", "registered", cp);
	backend_.first->DefineColumn("committees", "last_update", cp);

	cp.Assign<std::string>(true);
	backend_.first->DefineColumn("committees", "description", cp);
	backend_.first->DefineColumn("committees", "comment", cp);
	cp.Assign<std::string>(true, (boost::uint64_t) 0, (boost::uint64_t) 320);
	backend_.first->DefineColumn("committees", "email", cp);
	cp.Assign<std::string>(true, (boost::uint64_t) 0, (boost::uint64_t) 2048);
	backend_.first->DefineColumn("committees", "webpage", cp);

	cp.Assign<bool>(true);
	backend_.first->DefineColumn("committees", "private", cp);
	backend_.first->DefineColumn("committees", "openmemos", cp);
	backend_.first->DefineColumn("committees", "secure", cp);
	backend_.first->DefineColumn("committees", "lock_private", cp);
	backend_.first->DefineColumn("committees", "lock_openmemos", cp);
	backend_.first->DefineColumn("committees", "lock_secure", cp);

	// TABLE: committees_member
	cp.Assign<boost::uint32_t>(false);
	backend_.first->DefineColumn("committees_member", "id", cp);
	backend_.first->DefineColumn("committees_member", "number", cp);
	backend_.first->DefineColumn("committees_member", "entry", cp);
	cp.Assign<std::string>(false, (boost::uint64_t) 0, (boost::uint64_t) 32);
	backend_.first->DefineColumn("committees_member", "last_updater", cp);
	cp.Assign<boost::int32_t>(true);
	backend_.first->DefineColumn("committees_member", "last_updater_id", cp);
	cp.Assign<boost::posix_time::ptime>(false, boost::function0<mantra::StorageValue>(&GetCurrentDateTime));
	backend_.first->DefineColumn("committees_member", "last_update", cp);

	// TABLE: committees_message
	cp.Assign<boost::uint32_t>(false);
	backend_.first->DefineColumn("committees_message", "id", cp);
	backend_.first->DefineColumn("committees_message", "number", cp);
	cp.Assign<std::string>(false);
	backend_.first->DefineColumn("committees_message", "message", cp);
	cp.Assign<std::string>(false, (boost::uint64_t) 0, (boost::uint64_t) 32);
	backend_.first->DefineColumn("committees_message", "last_updater", cp);
	cp.Assign<boost::int32_t>(true);
	backend_.first->DefineColumn("committees_message", "last_updater_id", cp);
	cp.Assign<boost::posix_time::ptime>(false, boost::function0<mantra::StorageValue>(&GetCurrentDateTime));
	backend_.first->DefineColumn("committees_message", "last_update", cp);

	// TABLE: forbidden
	cp.Assign<boost::uint32_t>(false);
	backend_.first->DefineColumn("forbidden", "number", cp);
	cp.Assign<std::string>(false, (boost::uint64_t) 0, (boost::uint64_t) 64);
	backend_.first->DefineColumn("forbidden", "mask", cp);
	cp.Assign<std::string>(false);
	backend_.first->DefineColumn("forbidden", "reason", cp);
	cp.Assign<std::string>(false, (boost::uint64_t) 0, (boost::uint64_t) 32);
	backend_.first->DefineColumn("forbidden", "last_updater", cp);
	cp.Assign<boost::int32_t>(true);
	backend_.first->DefineColumn("forbidden", "last_updater_id", cp);
	cp.Assign<boost::posix_time::ptime>(false, boost::function0<mantra::StorageValue>(&GetCurrentDateTime));
	backend_.first->DefineColumn("forbidden", "last_update", cp);

	// TABLE: akills
	cp.Assign<boost::uint32_t>(false);
	backend_.first->DefineColumn("akills", "number", cp);
	cp.Assign<mantra::duration>(false);
	backend_.first->DefineColumn("akills", "length", cp);
	cp.Assign<std::string>(false, (boost::uint64_t) 0, (boost::uint64_t) 320);
	backend_.first->DefineColumn("akills", "mask", cp);
	cp.Assign<std::string>(false);
	backend_.first->DefineColumn("akills", "reason", cp);
	cp.Assign<std::string>(false, (boost::uint64_t) 0, (boost::uint64_t) 32);
	backend_.first->DefineColumn("akills", "last_updater", cp);
	cp.Assign<boost::int32_t>(true);
	backend_.first->DefineColumn("akills", "last_updater_id", cp);
	cp.Assign<boost::posix_time::ptime>(false, boost::function0<mantra::StorageValue>(&GetCurrentDateTime));
	backend_.first->DefineColumn("akills", "last_update", cp);

	// TABLE: clones
	cp.Assign<boost::uint32_t>(false);
	backend_.first->DefineColumn("clones", "number", cp);
	backend_.first->DefineColumn("clones", "value", cp);
	cp.Assign<std::string>(false, (boost::uint64_t) 0, (boost::uint64_t) 350);
	backend_.first->DefineColumn("clones", "mask", cp);
	cp.Assign<std::string>(false);
	backend_.first->DefineColumn("clones", "reason", cp);
	cp.Assign<std::string>(false, (boost::uint64_t) 0, (boost::uint64_t) 32);
	backend_.first->DefineColumn("clones", "last_updater", cp);
	cp.Assign<boost::int32_t>(true);
	backend_.first->DefineColumn("clones", "last_updater_id", cp);
	cp.Assign<boost::posix_time::ptime>(false, boost::function0<mantra::StorageValue>(&GetCurrentDateTime));
	backend_.first->DefineColumn("clones", "last_update", cp);

	// TABLE: operdenies
	cp.Assign<boost::uint32_t>(false);
	backend_.first->DefineColumn("operdenies", "number", cp);
	cp.Assign<std::string>(false, (boost::uint64_t) 0, (boost::uint64_t) 350);
	backend_.first->DefineColumn("operdenies", "mask", cp);
	cp.Assign<std::string>(false);
	backend_.first->DefineColumn("operdenies", "reason", cp);
	cp.Assign<std::string>(false, (boost::uint64_t) 0, (boost::uint64_t) 32);
	backend_.first->DefineColumn("operdenies", "last_updater", cp);
	cp.Assign<boost::int32_t>(true);
	backend_.first->DefineColumn("operdenies", "last_updater_id", cp);
	cp.Assign<boost::posix_time::ptime>(false, boost::function0<mantra::StorageValue>(&GetCurrentDateTime));
	backend_.first->DefineColumn("operdenies", "last_update", cp);

	// TABLE: ignores
	cp.Assign<boost::uint32_t>(false);
	backend_.first->DefineColumn("ignores", "number", cp);
	cp.Assign<std::string>(false, (boost::uint64_t) 0, (boost::uint64_t) 350);
	backend_.first->DefineColumn("ignores", "mask", cp);
	cp.Assign<std::string>(false);
	backend_.first->DefineColumn("ignores", "reason", cp);
	cp.Assign<std::string>(false, (boost::uint64_t) 0, (boost::uint64_t) 32);
	backend_.first->DefineColumn("ignores", "last_updater", cp);
	cp.Assign<boost::int32_t>(true);
	backend_.first->DefineColumn("ignores", "last_updater_id", cp);
	cp.Assign<boost::posix_time::ptime>(false, boost::function0<mantra::StorageValue>(&GetCurrentDateTime));
	backend_.first->DefineColumn("ignores", "last_update", cp);

	// TABLE: killchans
	cp.Assign<boost::uint32_t>(false);
	backend_.first->DefineColumn("killchans", "number", cp);
	cp.Assign<std::string>(false, (boost::uint64_t) 0, (boost::uint64_t) 64);
	backend_.first->DefineColumn("killchans", "mask", cp);
	cp.Assign<std::string>(false);
	backend_.first->DefineColumn("killchans", "reason", cp);
	cp.Assign<std::string>(false, (boost::uint64_t) 0, (boost::uint64_t) 32);
	backend_.first->DefineColumn("killchans", "last_updater", cp);
	cp.Assign<boost::int32_t>(true);
	backend_.first->DefineColumn("killchans", "last_updater_id", cp);
	cp.Assign<boost::posix_time::ptime>(false, boost::function0<mantra::StorageValue>(&GetCurrentDateTime));
	backend_.first->DefineColumn("killchans", "last_update", cp);

	backend_.first->EndDefine();

	MT_EE
}

void Storage::Load()
{
	MT_EB
	MT_FUNC("Storage::Load");

	boost::timer t;
	boost::mutex::scoped_lock scoped_lock(lock_);
	if (event_)
	{
		ROOT->event->Cancel(event_);
		event_ = 0;
	}
	if (expire_event_)
	{
		ROOT->event->Cancel(expire_event_);
		expire_event_ = 0;
	}

	backend_.first->Refresh();
	sync();

	{
		boost::shared_ptr<Committee> sadmin, admin, comm;
//		SYNC_WLOCK(Committees_);
		Committees_by_name_t::iterator i = Committees_by_name_.find(ROOT->ConfigValue<std::string>("commserv.sadmin.name"));
		if (i == Committees_by_name_.end())
			sadmin = Committee::create(ROOT->ConfigValue<std::string>("commserv.sadmin.name"));
		else
			sadmin = *i;

		// Re-do the committee membership ...
		backend_.first->RemoveRow("committees_member", mantra::Comparison<mantra::C_EqualTo>::make("id", sadmin->ID()));
		if (ROOT->ConfigExists("operserv.services-admin"))
		{
			mantra::Storage::RecordMap rec;
			rec["id"] = sadmin->ID();
			rec["last_updater"] = ROOT->operserv.Primary();

			std::vector<std::string> members = ROOT->ConfigValue<std::vector<std::string> >("operserv.services-admin");
			for (size_t i=0; i<members.size(); ++i)
			{
				boost::shared_ptr<StoredNick> nick = Get_StoredNick(members[i]);
				if (!nick)
					continue;

				rec["number"] = (boost::uint32_t) i+1;
				rec["entry"] = nick->User()->ID();
				backend_.first->InsertRow("committees_member", rec);
			}
		}

		i = Committees_by_name_.find(ROOT->ConfigValue<std::string>("commserv.admin.name"));
		if (i == Committees_by_name_.end())
		{
			admin = Committee::create(ROOT->ConfigValue<std::string>("commserv.admin.name"),
									  sadmin);
		}
		else
			admin = *i;

		i = Committees_by_name_.find(ROOT->ConfigValue<std::string>("commserv.sop.name"));
		if (i == Committees_by_name_.end())
		{
			comm = Committee::create(ROOT->ConfigValue<std::string>("commserv.sop.name"),
									 sadmin);
		}

		i = Committees_by_name_.find(ROOT->ConfigValue<std::string>("commserv.oper.name"));
		if (i == Committees_by_name_.end())
		{
			comm = Committee::create(ROOT->ConfigValue<std::string>("commserv.oper.name"),
									 admin);
		}

		mantra::Storage::RecordMap rec;
		rec["private"] = true;
		rec["openmemos"] = false;
		rec["secure"] = false;
		rec["lock_private"] = true;
		rec["lock_openmemos"] = true;
		rec["lock_secure"] = true;

		i = Committees_by_name_.find(ROOT->ConfigValue<std::string>("commserv.regd.name"));
		if (i == Committees_by_name_.end())
		{
			comm = Committee::create(ROOT->ConfigValue<std::string>("commserv.regd.name"));
		}
		else
			comm = *i;
		backend_.first->ChangeRow("committees", rec, mantra::Comparison<mantra::C_EqualTo>::make("id", comm->ID()));
		backend_.first->RemoveRow("committees_member", mantra::Comparison<mantra::C_EqualTo>::make("id", comm->ID()));

		i = Committees_by_name_.find(ROOT->ConfigValue<std::string>("commserv.all.name"));
		if (i == Committees_by_name_.end())
		{
			comm = Committee::create(ROOT->ConfigValue<std::string>("commserv.all.name"));
		}
		else
			comm = *i;
		backend_.first->ChangeRow("committees", rec, mantra::Comparison<mantra::C_EqualTo>::make("id", comm->ID()));
		backend_.first->RemoveRow("committees_member", mantra::Comparison<mantra::C_EqualTo>::make("id", comm->ID()));
	}

	LOG(Info, _("Database loaded in %1% seconds."), t.elapsed());
	event_ = ROOT->event->Schedule(boost::bind(&Storage::Save, this),
								   ROOT->ConfigValue<mantra::duration>("save-time"));
	expire_event_ = ROOT->event->Schedule(boost::bind(&Storage::ExpireCheck, this),
								   ROOT->ConfigValue<mantra::duration>("general.expire-check"));

	MT_EE
}

void Storage::Save()
{
	MT_EB
	unsigned long codetype = MT_ASSIGN(MAGICK_TRACE_EVENT);
	MT_FUNC("Storage::Save");

	boost::timer t;
	boost::mutex::scoped_lock scoped_lock(lock_);
	if (event_)
		ROOT->event->Cancel(event_);
	backend_.first->Commit();
	LOG(Info, _("Database saved in %1% seconds."), t.elapsed());
	if (ROOT->ConfigValue<bool>("storage.load-after-save"))
	{
		scoped_lock.unlock();
		Load();
	}
	else
		event_ = ROOT->event->Schedule(boost::bind(&Storage::Save, this),
									   ROOT->ConfigValue<mantra::duration>("save-time"));

	MT_ASSIGN(codetype);
	MT_EE
}

std::string Storage::CryptPassword(const std::string &in) const
{
	MT_EB
	MT_FUNC("Storage::CryptPassword" << in);

	std::string rv = mantra::enhex(hasher(in.data(), std::min(in.size(), 32u)));

	MT_RET(rv);
	MT_EE
}

/****************************************************************************
 * The add/del/retrieve functions for all data.
 ****************************************************************************/

void Storage::Add(const boost::shared_ptr<LiveUser> &entry)
{
	MT_EB
	MT_FUNC("Storage::Add" << entry);

	SYNC_WLOCK(LiveUsers_);
	LiveUsers_[entry->Name()] = entry;

	MT_EE
}

void Storage::Add(const boost::shared_ptr<LiveChannel> &entry)
{
	MT_EB
	MT_FUNC("Storage::Add" << entry);

	SYNC_WLOCK(LiveChannels_);
	LiveChannels_[entry->Name()] = entry;

	MT_EE
}

void Storage::Add(const boost::shared_ptr<StoredUser> &entry)
{
	MT_EB
	MT_FUNC("Storage::Add" << entry);

	SYNC_WLOCK(StoredUsers_);
	StoredUsers_.insert(entry);

	MT_EE
}

void Storage::Add(const boost::shared_ptr<StoredNick> &entry)
{
	MT_EB
	MT_FUNC("Storage::Add" << entry);

	SYNC_WLOCK(StoredNicks_);
	StoredNicks_.insert(entry);

	MT_EE
}

void Storage::Add(const boost::shared_ptr<StoredChannel> &entry)
{
	MT_EB
	MT_FUNC("Storage::Add" << entry);

	SYNC_WLOCK(StoredChannels_);
	StoredChannels_.insert(entry);

	MT_EE
}

void Storage::Add(const boost::shared_ptr<Committee> &entry)
{
	MT_EB
	MT_FUNC("Storage::Add" << entry);

	SYNC_WLOCK(Committees_);
	Committees_.insert(entry);

	MT_EE
}

void Storage::Rename(const boost::shared_ptr<LiveUser> &entry,
					 const std::string &name)
{
	MT_EB
	MT_FUNC("Storage::Rename" << entry << name);

	static mantra::iequal_to<std::string> cmp;
	if (cmp(entry->Name(), name))
		if_LiveUser_Storage(entry).Name(name);
	else
	{
		SYNC_WLOCK(LiveUsers_);
		LiveUsers_t::iterator i = LiveUsers_.find(entry->Name());
		if (i != LiveUsers_.end())
			LiveUsers_.erase(i);
		SYNC_UNLOCK(LiveUsers_);
		if_LiveUser_Storage(entry).Name(name);
		Add(entry);
	}

	MT_EE
}

// --------------------------------------------------------------------------

boost::shared_ptr<LiveUser> Storage::Get_LiveUser(const std::string &name) const
{
	MT_EB
	MT_FUNC("Storage::Get_LiveUser" << name);

	SYNC_RLOCK(LiveUsers_);
	LiveUsers_t::const_iterator i = LiveUsers_.find(name);
	boost::shared_ptr<LiveUser> ret;
	if (i == LiveUsers_.end())
		MT_RET(ret);
	ret = i->second;
	MT_RET(ret);

	MT_EE
}

boost::shared_ptr<LiveChannel> Storage::Get_LiveChannel(const std::string &name) const
{
	MT_EB
	MT_FUNC("Storage::Get_LiveChannel" << name);

	SYNC_RLOCK(LiveChannels_);
	LiveChannels_t::const_iterator i = LiveChannels_.find(name);
	boost::shared_ptr<LiveChannel> ret;
	if (i == LiveChannels_.end())
		MT_RET(ret);
	ret = i->second;
	MT_RET(ret);

	MT_EE
}

boost::shared_ptr<StoredUser> Storage::Get_StoredUser(boost::uint32_t id,
										boost::logic::tribool deep) const
{
	MT_EB
	MT_FUNC("Storage::Get_StoredUser" << id << deep);

	SYNC_RWLOCK(StoredUsers_);
	StoredUsers_t::const_iterator i = std::lower_bound(StoredUsers_.begin(),
													   StoredUsers_.end(), id);
	boost::shared_ptr<StoredUser> ret;
	if (i == StoredUsers_.end() || **i != id)
	{
		if (boost::logic::indeterminate(deep))
			deep = ROOT->ConfigValue<bool>("storage.deep-lookup");
		if (deep)
		{
			// look for entry ...
		}
		MT_RET(ret);
	}
	ret = *i;
	MT_RET(ret);

	MT_EE
}

boost::shared_ptr<StoredNick> Storage::Get_StoredNick(const std::string &name,
										boost::logic::tribool deep) const
{
	MT_EB
	MT_FUNC("Storage::Get_StoredNick" << name << deep);

	SYNC_RWLOCK(StoredNicks_);
	StoredNicks_t::const_iterator i = std::lower_bound(StoredNicks_.begin(),
													   StoredNicks_.end(), name);
	boost::shared_ptr<StoredNick> ret;
	if (i == StoredNicks_.end() || **i != name)
	{
		if (boost::logic::indeterminate(deep))
			deep = ROOT->ConfigValue<bool>("storage.deep-lookup");
		if (deep)
		{
			// look for entry ...
		}
		MT_RET(ret);
	}
	ret = *i;
	MT_RET(ret);

	MT_EE
}

boost::shared_ptr<StoredChannel> Storage::Get_StoredChannel(boost::uint32_t id,
										boost::logic::tribool deep) const
{
	MT_EB
	MT_FUNC("Storage::Get_StoredChannel" << id << deep);

	SYNC_RWLOCK(StoredChannels_);
	StoredChannels_by_id_t::const_iterator i = StoredChannels_by_id_.find(id);
	boost::shared_ptr<StoredChannel> ret;
	if (i == StoredChannels_by_id_.end())
	{
		if (boost::logic::indeterminate(deep))
			deep = ROOT->ConfigValue<bool>("storage.deep-lookup");
		if (deep)
		{
			// look for entry ...
		}
		MT_RET(ret);
	}
	ret = *i;
	MT_RET(ret);

	MT_EE
}

boost::shared_ptr<StoredChannel> Storage::Get_StoredChannel(const std::string &name,
										boost::logic::tribool deep) const
{
	MT_EB
	MT_FUNC("Storage::Get_StoredChannel" << name << deep);

	SYNC_RWLOCK(StoredChannels_);
	StoredChannels_by_name_t::const_iterator i = StoredChannels_by_name_.find(name);
	boost::shared_ptr<StoredChannel> ret;
	if (i == StoredChannels_by_name_.end())
	{
		if (boost::logic::indeterminate(deep))
			deep = ROOT->ConfigValue<bool>("storage.deep-lookup");
		if (deep)
		{
			// look for entry ...
		}
		MT_RET(ret);
	}
	ret = *i;
	MT_RET(ret);

	MT_EE
}

boost::shared_ptr<Committee> Storage::Get_Committee(boost::uint32_t id,
										boost::logic::tribool deep) const
{
	MT_EB
	MT_FUNC("Storage::Get_Committee" << id << deep);

	SYNC_RWLOCK(Committees_);
	Committees_by_id_t::const_iterator i = Committees_by_id_.find(id);
	boost::shared_ptr<Committee> ret;
	if (i == Committees_by_id_.end())
	{
		if (boost::logic::indeterminate(deep))
			deep = ROOT->ConfigValue<bool>("storage.deep-lookup");
		if (deep)
		{
			// look for entry ...
		}
		MT_RET(ret);
	}
	ret = *i;
	MT_RET(ret);

	MT_EE
}

boost::shared_ptr<Committee> Storage::Get_Committee(const std::string &name,
										boost::logic::tribool deep) const
{
	MT_EB
	MT_FUNC("Storage::Get_Committee" << name << deep);

	SYNC_RWLOCK(Committees_);
	Committees_by_name_t::const_iterator i = Committees_by_name_.find(name);
	boost::shared_ptr<Committee> ret;
	if (i == Committees_by_name_.end())
	{
		if (boost::logic::indeterminate(deep))
			deep = ROOT->ConfigValue<bool>("storage.deep-lookup");
		if (deep)
		{
			// look for entry ...
		}
		MT_RET(ret);
	}
	ret = *i;
	MT_RET(ret);

	MT_EE
}

// --------------------------------------------------------------------------

void Storage::Del(const boost::shared_ptr<LiveUser> &entry)
{
	MT_EB
	MT_FUNC("Storage::Del" << entry);

	SYNC_WLOCK(LiveUsers_);
	LiveUsers_t::iterator i = LiveUsers_.find(entry->Name());
	if (i != LiveUsers_.end() && i->second == entry)
		LiveUsers_.erase(i);

	MT_EE
}

void Storage::Del(const boost::shared_ptr<LiveChannel> &entry)
{
	MT_EB
	MT_FUNC("Storage::Del" << entry);

	SYNC_WLOCK(LiveChannels_);
	LiveChannels_t::iterator i = LiveChannels_.find(entry->Name());
	if (i != LiveChannels_.end() && i->second == entry)
		LiveChannels_.erase(i);

	MT_EE
}

void Storage::DelInternal(const boost::shared_ptr<StoredUser> &entry)
{
	MT_EB
	MT_FUNC("Storage::DelInternal" << entry);

	mantra::ComparisonSet cs;
	mantra::Storage::DataSet data;
	mantra::Storage::RecordMap rec;
	mantra::Storage::FieldSet fields;
	fields.insert("id");
	fields.insert("successor");
	backend_.first->RetrieveRow("channels", data,
			mantra::Comparison<mantra::C_EqualTo>::make("founder", entry->ID()) &&
			mantra::Comparison<mantra::C_EqualTo>::make("successor",
								mantra::NullValue::instance(), true), fields);

	// Succeed the channel (ie. Successor -> Founder).
	mantra::Storage::DataSet::const_iterator i;
	mantra::Storage::RecordMap::const_iterator j;
	rec["successor"] = mantra::NullValue();
	for (i = data.begin(); i != data.end(); ++i)
	{
		j = i->find("successor");
		if (j == i->end())
			continue; // ??

		StoredUsers_t::iterator l = std::lower_bound(StoredUsers_.begin(),
													 StoredUsers_.end(),
													 boost::get<boost::uint32_t>(j->second));
		if (l == StoredUsers_.end() ||
			**l != boost::get<boost::uint32_t>(j->second))
			continue; // Its gone, remove channel (later).

		rec["founder"] = j->second;
		j = i->find("id");
		if (j == i->end())
			continue; // ??

		boost::shared_ptr<StoredChannel> channel = Get_StoredChannel(boost::get<boost::uint32_t>(j->second));
		if (!channel)
			continue; // Its gone, remove channel (later).

		// Easy come, easy go.
		if_StoredUser_Storage(*l).Add(channel);

		backend_.first->ChangeRow("channels", rec,
					mantra::Comparison<mantra::C_EqualTo>::make("id", j->second));
	}

	std::vector<mantra::StorageValue> entries;
	fields.clear();
	fields.insert("id");
	backend_.first->RetrieveRow("channels", data,
			mantra::Comparison<mantra::C_EqualTo>::make("founder", entry->ID()), fields);
	entries.reserve(data.size());
	SYNC_WLOCK(StoredChannels_);
	for (i = data.begin(); i != data.end(); ++i)
	{
		j = i->find("id");
		if (j == i->end())
			continue; // ??

		StoredChannels_by_id_t::iterator l = StoredChannels_by_id_.find(boost::get<boost::uint32_t>(j->second));
		if (l == StoredChannels_by_id_.end())
			continue;

		// Do non-database part of drop.
		if_StoredChannel_Storage(*l).DropInternal();
		StoredChannels_by_id_.erase(l);
		entries.push_back(j->second);
	}
	SYNC_UNLOCK(StoredChannels_);

	if (!have_cascade)
	{
		if (!entries.empty())
		{
			cs = mantra::Comparison<mantra::C_OneOf>::make("id", entries);
			backend_.first->RemoveRow("channels_level", cs);
			backend_.first->RemoveRow("channels_access", cs);
			backend_.first->RemoveRow("channels_akick", cs);
			backend_.first->RemoveRow("channels_greet", cs);
			backend_.first->RemoveRow("channels_message", cs);
			backend_.first->RemoveRow("channels_news", cs);
			backend_.first->RemoveRow("channels_news_read", cs);
			backend_.first->RemoveRow("channels", cs);
		}

		cs = mantra::Comparison<mantra::C_EqualTo>::make("entry_user", entry->ID());
		backend_.first->RemoveRow("channels_access", cs);
		backend_.first->RemoveRow("channels_akick", cs);
		cs = mantra::Comparison<mantra::C_EqualTo>::make("entry", entry->ID());
		backend_.first->RemoveRow("channels_greet", cs);
		backend_.first->RemoveRow("channels_news_read", cs);

		rec.clear();
		rec["successor"] = mantra::NullValue();
		backend_.first->ChangeRow("channels", rec,
				mantra::Comparison<mantra::C_EqualTo>::make("successor", entry->ID()));
	}

	entries.clear();
	backend_.first->RetrieveRow("committees", data,
			mantra::Comparison<mantra::C_EqualTo>::make("head_user", entry->ID()), fields);
	entries.reserve(data.size());
	SYNC_WLOCK(Committees_);
	for (i = data.begin(); i != data.end(); ++i)
	{
		j = i->find("id");
		if (j == i->end())
			continue; // ??

		Committees_by_id_t::iterator l = Committees_by_id_.find(boost::get<boost::uint32_t>(j->second));
		if (l == Committees_by_id_.end())
			continue;

		// Do non-database part of drop.
		if_Committee_Storage(*l).DropInternal();
		Committees_by_id_.erase(l);
		entries.push_back(j->second);
	}
	SYNC_UNLOCK(Committees_);

	if (!entries.empty())
		DelInternalRecurse(entries);

	if (!have_cascade)
	{
		backend_.first->RemoveRow("committees_member",
				mantra::Comparison<mantra::C_EqualTo>::make("entry", entry->ID()));

		cs = mantra::Comparison<mantra::C_EqualTo>::make("suspend_by_id", entry->ID());
		rec.clear();
		rec["suspend_by_id"] = mantra::NullValue();
		backend_.first->ChangeRow("users", rec, cs);
		backend_.first->ChangeRow("channels", rec, cs);

		cs = mantra::Comparison<mantra::C_EqualTo>::make("sender_id", entry->ID());
		rec.clear();
		rec["sender_id"] = mantra::NullValue();
		backend_.first->ChangeRow("users_memo", rec, cs);
		backend_.first->ChangeRow("channels_news", rec, cs);

		cs = mantra::Comparison<mantra::C_EqualTo>::make("last_updater_id", entry->ID());
		rec.clear();
		rec["last_updater_id"] = mantra::NullValue();
		backend_.first->ChangeRow("committees_member", rec, cs);
		backend_.first->ChangeRow("committees_message", rec, cs);
		backend_.first->ChangeRow("channels_level", rec, cs);
		backend_.first->ChangeRow("channels_access", rec, cs);
		backend_.first->ChangeRow("channels_akick", rec, cs);
		backend_.first->ChangeRow("channels_greet", rec, cs);
		backend_.first->ChangeRow("channels_message", rec, cs);
		backend_.first->ChangeRow("forbidden", rec, cs);
		backend_.first->ChangeRow("akills", rec, cs);
		backend_.first->ChangeRow("clones", rec, cs);
		backend_.first->ChangeRow("operdenies", rec, cs);
		backend_.first->ChangeRow("ignores", rec, cs);
		backend_.first->ChangeRow("killchans", rec, cs);
	}

	entries.clear();
	backend_.first->RetrieveRow("nicks", data,
			mantra::Comparison<mantra::C_EqualTo>::make("id", entry->ID()), fields);
	entries.reserve(data.size());
	SYNC_WLOCK(StoredNicks_);
	for (i = data.begin(); i != data.end(); ++i)
	{
		j = i->find("name");
		if (j == i->end())
			continue; // ??

		StoredNicks_t::iterator l = std::lower_bound(StoredNicks_.begin(),
													 StoredNicks_.end(),
													 boost::get<std::string>(j->second));
		if (l == StoredNicks_.end() ||
			**l != boost::get<std::string>(j->second))
			continue;

		// Do non-database part of drop.
		if_StoredNick_Storage(*l).DropInternal();
		entries.push_back(j->second);
	}
	SYNC_UNLOCK(StoredNicks_);

	if (!have_cascade)
	{
		backend_.first->RemoveRow("nicks",
				mantra::Comparison<mantra::C_OneOfNC>::make("name", entries));

		backend_.first->RemoveRow("users_ignore",
				mantra::Comparison<mantra::C_EqualTo>::make("entry", entry->ID()));
	}

	if_StoredUser_Storage(entry).DropInternal();

	cs = mantra::Comparison<mantra::C_EqualTo>::make("id", entry->ID());
	if (!have_cascade)
	{
		backend_.first->RemoveRow("users_access", cs);
		backend_.first->RemoveRow("users_ignore", cs);
		backend_.first->RemoveRow("users_memo", cs);
	}
	backend_.first->RemoveRow("users", cs);

	MT_EE
}

void Storage::Del(const boost::shared_ptr<StoredUser> &entry)
{
	MT_EB
	MT_FUNC("Storage::Del" << entry);

	SYNC_WLOCK(StoredUsers_);
	StoredUsers_t::iterator i = std::lower_bound(StoredUsers_.begin(),
												 StoredUsers_.end(),
												 entry);
	if (i == StoredUsers_.end() || *i != entry)
		return;

	StoredUsers_.erase(i);

	SYNC_UNLOCK(StoredUsers_);
	DelInternal(entry);

	MT_EE
}

void Storage::DelInternal(const boost::shared_ptr<StoredNick> &entry)
{
	MT_EB
	MT_FUNC("Storage::DelInternal" << entry);

	if_StoredNick_Storage(entry).DropInternal();
	backend_.first->RemoveRow("nicks",
			mantra::Comparison<mantra::C_EqualToNC>::make("name", entry->Name()));

	MT_EE
}

void Storage::Del(const boost::shared_ptr<StoredNick> &entry)
{
	MT_EB
	MT_FUNC("Storage::Del" << entry);

	SYNC_WLOCK(StoredNicks_);
	StoredNicks_t::iterator i = StoredNicks_.find(entry);
	if (i == StoredNicks_.end())
		return;

	StoredNicks_.erase(i);
	SYNC_UNLOCK(StoredNicks_);
	DelInternal(entry);

	MT_EE
}

void Storage::DelInternal(const boost::shared_ptr<StoredChannel> &entry)
{
	MT_EB
	MT_FUNC("Storage::DelInternal" << entry);

	if_StoredChannel_Storage(entry).DropInternal();
	mantra::ComparisonSet cs = mantra::Comparison<mantra::C_EqualTo>::make("id", entry->ID());
	backend_.first->RemoveRow("channels_level", cs);
	backend_.first->RemoveRow("channels_access", cs);
	backend_.first->RemoveRow("channels_akick", cs);
	backend_.first->RemoveRow("channels_greet", cs);
	backend_.first->RemoveRow("channels_message", cs);
	backend_.first->RemoveRow("channels_news", cs);
	backend_.first->RemoveRow("channels_news_read", cs);
	backend_.first->RemoveRow("channels", cs);

	MT_EE
}

void Storage::Del(const boost::shared_ptr<StoredChannel> &entry)
{
	MT_EB
	MT_FUNC("Storage::Del" << entry);

	SYNC_WLOCK(StoredChannels_);
	StoredChannels_t::iterator i = StoredChannels_.find(*entry);
	if (i == StoredChannels_.end())
		return;

	StoredChannels_.erase(i);
	SYNC_UNLOCK(StoredChannels_);
	DelInternal(entry);

	MT_EE
}

/** 
 * @brief Recursively delete committees.
 * This will only do the database deletion for the entries passed in, however
 * it will ALSO do the DelInternal for sub-entries it finds (ie. those where
 * one of the entries its passed is the head_committee of one it finds).
 * 
 * @param entries A vector of entries to recursively remove (name only).
 */
void Storage::DelInternalRecurse(const std::vector<mantra::StorageValue> &entries)
{
	MT_EB
	MT_FUNC("Storage::DelInternalRecurse" << entries);

	if (entries.empty())
		return;

	mantra::Storage::DataSet data;
	mantra::Storage::FieldSet fields;
	std::vector<mantra::StorageValue> subentries;

	fields.insert("id");
	backend_.first->RetrieveRow("committees", data,
			mantra::Comparison<mantra::C_OneOf>::make("head_committee", entries), fields);
	subentries.reserve(data.size());

	mantra::Storage::DataSet::const_iterator i;
	mantra::Storage::RecordMap::const_iterator j;
	SYNC_WLOCK(Committees_);
	for (i = data.begin(); i != data.end(); ++i)
	{
		j = i->find("id");
		if (j == i->end())
			continue; // ??

		Committees_by_id_t::iterator l = Committees_by_id_.find(boost::get<boost::uint32_t>(j->second));
		if (l == Committees_by_id_.end())
			continue;

		// Do non-database part of drop.
		if_Committee_Storage(*l).DropInternal();
		Committees_by_id_.erase(l);
		subentries.push_back(j->second);
	}
	SYNC_UNLOCK(Committees_);

	if (!subentries.empty())
		DelInternalRecurse(subentries);

	if (!have_cascade)
	{
		mantra::ComparisonSet cs = mantra::Comparison<mantra::C_OneOf>::make("id", entries);
		backend_.first->RemoveRow("committees_member", cs);
		backend_.first->RemoveRow("committees_message", cs);
		backend_.first->RemoveRow("committees", cs);
	}

	MT_EE
}

void Storage::DelInternal(const boost::shared_ptr<Committee> &entry)
{
	MT_EB
	MT_FUNC("Storage::DelInternal" << entry);

	if (!have_cascade)
	{
		mantra::ComparisonSet cs = mantra::Comparison<mantra::C_EqualTo>::make("entry_committee", entry->ID());
		backend_.first->RemoveRow("channels_access", cs);
		backend_.first->RemoveRow("channels_akick", cs);
	}

	if_Committee_Storage(entry).DropInternal();
	std::vector<mantra::StorageValue> entries;
	entries.push_back(entry->Name());
	DelInternalRecurse(entries);

	if (have_cascade)
	{
		mantra::ComparisonSet cs = mantra::Comparison<mantra::C_EqualTo>::make("id", entry->ID());
		backend_.first->RemoveRow("committees", cs);
	}

	MT_EE
}

void Storage::Del(const boost::shared_ptr<Committee> &entry)
{
	MT_EB
	MT_FUNC("Storage::Del" << entry);

	SYNC_WLOCK(Committees_);
	Committees_t::iterator i = Committees_.find(*entry);
	if (i == Committees_.end())
		return;

	Committees_.erase(i);
	SYNC_UNLOCK(Committees_);
	DelInternal(entry);

	MT_EE
}

// --------------------------------------------------------------------------

void Storage::Add(const Forbidden &entry)
{
	MT_EB
	MT_FUNC("Storage::Add" << entry);

	if (!entry.Number())
		return;

	SYNC_WLOCK(Forbiddens_);
	Forbiddens_.insert(entry);

	MT_EE
}

void Storage::Del(const Forbidden &entry)
{
	MT_EB
	MT_FUNC("Storage::Del" << entry);

	backend_.first->RemoveRow("forbidden", 
		  mantra::Comparison<mantra::C_EqualTo>::make("number", entry.Number()));
	SYNC_WLOCK(Forbiddens_);
	Forbiddens_.erase(entry);

	MT_EE
}

Forbidden Storage::Get_Forbidden(boost::uint32_t in, boost::logic::tribool deep)
{
	MT_EB
	MT_FUNC("Get_Forbidden" << in << deep);

	SYNC_RWLOCK(Forbiddens_);
	Forbiddens_t::const_iterator i = std::lower_bound(Forbiddens_.begin(),
													  Forbiddens_.end(), in);
	if (i == Forbiddens_.end() || *i != in)
	{
		if (boost::logic::indeterminate(deep))
			deep = ROOT->ConfigValue<bool>("storage.deep-lookup");
		if (deep && backend_.first->RowExists("forbidden", 
			mantra::Comparison<mantra::C_EqualTo>::make("number", in)))
		{
			Forbidden rv(in);
			SYNC_PROMOTE(Forbiddens_);
			Forbiddens_.insert(rv);
			MT_RET(rv);
		}

		Forbidden rv(0);
		MT_RET(rv);
	}

	Forbidden rv(in);

	MT_RET(rv);
	MT_EE
}

void Storage::Get_Forbidden(std::vector<Forbidden> &fill, boost::logic::tribool channel) const
{
	MT_EB
	MT_FUNC("Get_Forbidden" << fill << channel);

	SYNC_RLOCK(Forbiddens_);
	if (boost::logic::indeterminate(channel))
		std::copy(Forbiddens_.begin(), Forbiddens_.end(), fill.end());
	else
	{
		Forbiddens_t::const_iterator i;
		for (i = Forbiddens_.begin(); i != Forbiddens_.end(); ++i)
			if (channel)
			{
				if (ROOT->proto.IsChannel(i->Mask()))
					fill.push_back(*i);
			}
			else
			{
				if (!ROOT->proto.IsChannel(i->Mask()))
					fill.push_back(*i);
			}
	}

	MT_EE
}

bool Storage::Matches_Forbidden(const std::string &in, boost::logic::tribool channel) const
{
	MT_EB
	MT_FUNC("Matches_Forbidden" << in << channel);

	SYNC_RLOCK(Forbiddens_);
	Forbiddens_t::const_iterator i;
	for (i = Forbiddens_.begin(); i != Forbiddens_.end(); ++i)
	{
		if (boost::logic::indeterminate(channel))
			if (i->Matches(in))
				MT_RET(true);
		else if (channel)
			if (ROOT->proto.IsChannel(i->Mask()) && i->Matches(in))
				MT_RET(true);
		else if (!ROOT->proto.IsChannel(i->Mask()) && i->Matches(in))
			MT_RET(true);
	}

	MT_RET(false);
	MT_EE
}

// --------------------------------------------------------------------------

void Storage::ExpireCheck()
{
	MT_EB
	unsigned long codetype = MT_ASSIGN(MAGICK_TRACE_EVENT);
	MT_FUNC("Storage::ExpireCheck");

	boost::mutex::scoped_lock scoped_lock(lock_);

	if_StoredNick_Storage::expire();
	if_StoredChannel_Storage::expire();

	expire_event_ = ROOT->event->Schedule(boost::bind(&Storage::ExpireCheck, this),
								   ROOT->ConfigValue<mantra::duration>("general.expire-check"));
	MT_ASSIGN(codetype);
	MT_EE
}

