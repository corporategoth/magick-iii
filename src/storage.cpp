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
	  event_(0), handle_(NULL), crypt_handle_(NULL), compress_handle_(NULL),
	  hasher(mantra::Hasher::NONE), SYNC_NRWINIT(LiveUsers_, reader_priority),
	  SYNC_NRWINIT(LiveChannels_, reader_priority),
	  SYNC_NRWINIT(StoredUsers_, reader_priority),
	  SYNC_NRWINIT(StoredNicks_, reader_priority),
	  SYNC_NRWINIT(StoredChannels_, reader_priority),
	  SYNC_NRWINIT(Committees_, reader_priority)
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
	for (size_t i=0; i<stages_.size(); i++)
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

// Yes, its copied from config_parse.cpp.
template<typename T>
inline bool check_old_new(const char *entry, const po::variables_map &old_vm,
						  const po::variables_map &new_vm)
{
	MT_EB
	MT_FUNC("check_old_new" << entry << "(const po::variables_map &) old_vm"
									 << "(const po::variables_map &) new_vm");

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
	MT_FUNC("Storage::init" << "(const po::variables_map &) vm");

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
				for (size_t i=0; i<newstages_.size(); i++)
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
		for (size_t i=0; i<newstages_.size(); i++)
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

			for (size_t i=0; i<newstages_.size(); i++)
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
			for (ri = stages_.rbegin(); ri != stages_.rend(); ri++)
				*finalstage_.first << *ri->first;

			backend_.second = (void (*)(mantra::Storage *)) destroy_IniFileStorage;
			backend_.first = create_IniFileStorage(*finalstage_.first, 
						vm["storage.inifile.tollerant"].as<bool>());
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
				for (size_t i=0; i<newstages_.size(); i++)
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
		for (size_t i=0; i<newstages_.size(); i++)
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

			for (size_t i=0; i<newstages_.size(); i++)
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
			for (ri = stages_.rbegin(); ri != stages_.rend(); ri++)
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
	cp.Assign<boost::posix_time::ptime>(false);
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
	backend_.first->DefineColumn("channels", "suspended_by", cp);
	cp.Assign<boost::uint32_t>(true);
	backend_.first->DefineColumn("users", "suspend_by_id", cp);
	cp.Assign<std::string>(true);
	backend_.first->DefineColumn("channels", "suspended_reason", cp);
	cp.Assign<boost::posix_time::ptime>(true);
	backend_.first->DefineColumn("channels", "suspend_time", cp);

	// TABLE: channels_level
	cp.Assign<std::string>(false, (boost::uint64_t) 0, (boost::uint64_t) 64);
	backend_.first->DefineColumn("channels_level", "name", cp);
	cp.Assign<boost::uint32_t>(false);
	backend_.first->DefineColumn("channels_level", "id", cp);
	cp.Assign<boost::int32_t>(false);
	backend_.first->DefineColumn("channels_level", "value", cp);
	cp.Assign<std::string>(false, (boost::uint64_t) 0, (boost::uint64_t) 32);
	backend_.first->DefineColumn("channels_level", "last_updater", cp);
	cp.Assign<boost::int32_t>(true);
	backend_.first->DefineColumn("channels_level", "last_updater_id", cp);
	cp.Assign<boost::posix_time::ptime>(false, boost::function0<mantra::StorageValue>(&GetCurrentDateTime));
	backend_.first->DefineColumn("channels_level", "last_update", cp);

	// TABLE: channel_access
	cp.Assign<std::string>(false, (boost::uint64_t) 0, (boost::uint64_t) 64);
	backend_.first->DefineColumn("channels_access", "name", cp);
	cp.Assign<boost::uint32_t>(false);
	backend_.first->DefineColumn("channels_access", "number", cp);
	cp.Assign<std::string>(true, (boost::uint64_t) 0, (boost::uint64_t) 350);
	backend_.first->DefineColumn("channels_access", "entry_mask", cp);
	cp.Assign<boost::uint32_t>(true);
	backend_.first->DefineColumn("channels_access", "entry_user", cp);
	cp.Assign<std::string>(true, (boost::uint64_t) 0, (boost::uint64_t) 32);
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
	cp.Assign<std::string>(false, (boost::uint64_t) 0, (boost::uint64_t) 64);
	backend_.first->DefineColumn("channels_akick", "name", cp);
	cp.Assign<boost::uint32_t>(false);
	backend_.first->DefineColumn("channels_akick", "number", cp);
	cp.Assign<std::string>(true, (boost::uint64_t) 0, (boost::uint64_t) 350);
	backend_.first->DefineColumn("channels_akick", "entry_mask", cp);
	cp.Assign<boost::uint32_t>(true);
	backend_.first->DefineColumn("channels_akick", "entry_user", cp);
	cp.Assign<std::string>(true, (boost::uint64_t) 0, (boost::uint64_t) 32);
	backend_.first->DefineColumn("channels_access", "entry_committee", cp);
	cp.Assign<std::string>(false);
	backend_.first->DefineColumn("channels_akick", "reason", cp);
	cp.Assign<std::string>(false, (boost::uint64_t) 0, (boost::uint64_t) 32);
	backend_.first->DefineColumn("channels_akick", "last_updater", cp);
	cp.Assign<boost::int32_t>(true);
	backend_.first->DefineColumn("channels_akick", "last_updater_id", cp);
	cp.Assign<boost::posix_time::ptime>(false, boost::function0<mantra::StorageValue>(&GetCurrentDateTime));
	backend_.first->DefineColumn("channels_akick", "last_update", cp);

	// TABLE: channels_greet
	cp.Assign<std::string>(false, (boost::uint64_t) 0, (boost::uint64_t) 64);
	backend_.first->DefineColumn("channels_greet", "name", cp);
	cp.Assign<boost::int32_t>(true);
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
	cp.Assign<std::string>(false, (boost::uint64_t) 0, (boost::uint64_t) 64);
	backend_.first->DefineColumn("channels_message", "name", cp);
	cp.Assign<boost::uint32_t>(false);
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
	cp.Assign<std::string>(false, (boost::uint64_t) 0, (boost::uint64_t) 64);
	backend_.first->DefineColumn("channels_news", "name", cp);
	cp.Assign<boost::uint32_t>(false);
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
	cp.Assign<std::string>(false, (boost::uint64_t) 0, (boost::uint64_t) 64);
	backend_.first->DefineColumn("channels_news_read", "name", cp);
	cp.Assign<boost::uint32_t>(false);
	backend_.first->DefineColumn("channels_news_read", "number", cp);
	backend_.first->DefineColumn("channels_news_read", "entry", cp);

	// TABLE: committees
	cp.Assign<std::string>(false, (boost::uint64_t) 0, (boost::uint64_t) 32);
	backend_.first->DefineColumn("committees", "name", cp);

	cp.Assign<boost::uint32_t>(true);
	backend_.first->DefineColumn("committees", "head_user", cp);
	cp.Assign<std::string>(false, (boost::uint64_t) 0, (boost::uint64_t) 32);
	backend_.first->DefineColumn("committees", "head_committee", cp);

	cp.Assign<boost::posix_time::ptime>(false, boost::function0<mantra::StorageValue>(&GetCurrentDateTime));
	backend_.first->DefineColumn("committees", "registered", cp);
	backend_.first->DefineColumn("committees", "last_update", cp);

	cp.Assign<std::string>(true);
	backend_.first->DefineColumn("committees", "description", cp);
	cp.Assign<std::string>(false, (boost::uint64_t) 0, (boost::uint64_t) 320);
	backend_.first->DefineColumn("committees", "email", cp);
	cp.Assign<std::string>(false, (boost::uint64_t) 0, (boost::uint64_t) 2048);
	backend_.first->DefineColumn("committees", "webpage", cp);

	cp.Assign<bool>(true);
	backend_.first->DefineColumn("committees", "private", cp);
	backend_.first->DefineColumn("committees", "openmemos", cp);
	backend_.first->DefineColumn("committees", "secure", cp);
	backend_.first->DefineColumn("committees", "lock_private", cp);
	backend_.first->DefineColumn("committees", "lock_openmemos", cp);
	backend_.first->DefineColumn("committees", "lock_secure", cp);

	// TABLE: committees_member
	cp.Assign<std::string>(false, (boost::uint64_t) 0, (boost::uint64_t) 32);
	backend_.first->DefineColumn("committees_member", "name", cp);
	cp.Assign<boost::uint32_t>(false);
	backend_.first->DefineColumn("committees_member", "entry", cp);
	cp.Assign<std::string>(false, (boost::uint64_t) 0, (boost::uint64_t) 32);
	backend_.first->DefineColumn("committees_member", "last_updater", cp);
	cp.Assign<boost::int32_t>(true);
	backend_.first->DefineColumn("committees_member", "last_updater_id", cp);
	cp.Assign<boost::posix_time::ptime>(false, boost::function0<mantra::StorageValue>(&GetCurrentDateTime));
	backend_.first->DefineColumn("committees_member", "last_update", cp);

	// TABLE: committees_message
	cp.Assign<std::string>(false, (boost::uint64_t) 0, (boost::uint64_t) 32);
	backend_.first->DefineColumn("committees_message", "name", cp);
	cp.Assign<boost::uint32_t>(false);
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
	cp.Assign<std::string>(false, (boost::uint64_t) 0, (boost::uint64_t) 64);
	backend_.first->DefineColumn("forbidden", "name", cp);
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
	backend_.first->DefineColumn("ignores", "reason", cp);
	cp.Assign<std::string>(false, (boost::uint64_t) 0, (boost::uint64_t) 32);
	backend_.first->DefineColumn("ignores", "last_updater", cp);
	cp.Assign<boost::int32_t>(true);
	backend_.first->DefineColumn("ignores", "last_updater_id", cp);
	cp.Assign<boost::posix_time::ptime>(false, boost::function0<mantra::StorageValue>(&GetCurrentDateTime));
	backend_.first->DefineColumn("ignores", "last_update", cp);

	// TABLE: killchans
	cp.Assign<std::string>(false, (boost::uint64_t) 0, (boost::uint64_t) 64);
	backend_.first->DefineColumn("killchans", "name", cp);
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
		ROOT->event->Cancel(event_);
	backend_.first->Refresh();
	LOG(Info, _("Database loaded in %1% seconds."), t.elapsed());
	event_ = ROOT->event->Schedule(boost::bind(&Storage::Save, this),
								   ROOT->ConfigValue<mantra::duration>("save-time"));

	MT_EE
}

void Storage::Save()
{
	MT_EB
	MT_FUNC("Storage::Save");

	boost::timer t;
	boost::mutex::scoped_lock scoped_lock(lock_);
	if (event_)
		ROOT->event->Cancel(event_);
	backend_.first->Commit();
	LOG(Info, _("Database saved in %1% seconds."), t.elapsed());
	event_ = ROOT->event->Schedule(boost::bind(&Storage::Save, this),
								   ROOT->ConfigValue<mantra::duration>("save-time"));

	MT_EE
}

std::string Storage::CryptPassword(const std::string &in) const
{
	MT_EB
	MT_FUNC("std::string Storage::CryptPassword" << in);

	return hasher(in.data(), in.size());

	MT_EE
}

unsigned int StorageInterface::RowExists(const mantra::ComparisonSet &search) const throw(mantra::storage_exception)
{
	return ROOT->data.backend_.first->RowExists(table_, search);
}

bool StorageInterface::InsertRow(const mantra::Storage::RecordMap &data) throw(mantra::storage_exception)
{
	if (update_.empty())
		return ROOT->data.backend_.first->InsertRow(table_, data);

	mantra::Storage::RecordMap entry = data;
	entry[update_] = mantra::GetCurrentDateTime();
	return ROOT->data.backend_.first->InsertRow(table_, entry);
}

unsigned int StorageInterface::ChangeRow(const mantra::Storage::RecordMap &data, 
				const mantra::ComparisonSet &search) throw(mantra::storage_exception)
{
	if (update_.empty())
		return ROOT->data.backend_.first->ChangeRow(table_, data, search);

	mantra::Storage::RecordMap entry = data;
	entry[update_] = mantra::GetCurrentDateTime();
	return ROOT->data.backend_.first->ChangeRow(table_, entry, search);
}

void StorageInterface::RetrieveRow(mantra::Storage::DataSet &data,
				const mantra::ComparisonSet &search,
				const mantra::Storage::FieldSet &fields) throw(mantra::storage_exception)
{
	return ROOT->data.backend_.first->RetrieveRow(table_, data, search, fields);
}

unsigned int StorageInterface::RemoveRow(const mantra::ComparisonSet &search) throw(mantra::storage_exception)
{
	return ROOT->data.backend_.first->RemoveRow(table_, search);
}

mantra::StorageValue StorageInterface::Minimum(const std::string &column,
				const mantra::ComparisonSet &search) throw(mantra::storage_exception)
{
	return ROOT->data.backend_.first->Minimum(table_, column, search);
}

mantra::StorageValue StorageInterface::Maximum(const std::string &column,
				const mantra::ComparisonSet &search) throw(mantra::storage_exception)
{
	return ROOT->data.backend_.first->Maximum(table_, column, search);
}


// Aliases that use the above.
bool StorageInterface::GetRow(const mantra::ComparisonSet &search, mantra::Storage::RecordMap &data)
{
	MT_EB
	MT_FUNC("StorageInterface::GetRow" <<  "(const mantra::ComparisonSet &) search" << "(mantra::Storage::RecordMap &) data");

	mantra::Storage::DataSet ds;
	RetrieveRow(ds, search);
	if (ds.size() != 1u)
		MT_RET(false);

	data = ds[0];

	MT_RET(true);
	MT_EE
}

// Single field gets/puts, using the pre-defined key.
mantra::StorageValue StorageInterface::GetField(const mantra::ComparisonSet &search, const std::string &column)
{
	MT_EB
	MT_FUNC("StorageInterface::GetField" <<  "(const mantra::ComparisonSet &) search" << column);

	static mantra::NullValue nv;

	mantra::Storage::FieldSet fields;
	fields.insert(column);

	mantra::Storage::DataSet ds;
	RetrieveRow(ds, search, fields);
	if (ds.size() != 1u)
		MT_RET(nv);

	mantra::Storage::RecordMap::iterator i = ds[0].find(column);
	if (i == ds[0].end())
		MT_RET(nv);

	MT_RET(i->second);
	MT_EE
}

bool StorageInterface::PutField(const mantra::ComparisonSet &search, const std::string &column, const mantra::StorageValue &value)
{
	MT_EB
	MT_FUNC("StorageInterface::PutField" << "(const mantra::ComparisonSet &) search" << column << value);

	mantra::Storage::RecordMap data;
	data[column] = value;
	unsigned int rv = ChangeRow(data, search);

	MT_RET(rv == 1u);
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
	StoredUsers_[entry->ID()] = entry;

	MT_EE
}

void Storage::Add(const boost::shared_ptr<StoredNick> &entry)
{
	MT_EB
	MT_FUNC("Storage::Add" << entry);

	SYNC_WLOCK(StoredNicks_);
	StoredNicks_[entry->Name()] = entry;

	MT_EE
}

void Storage::Add(const boost::shared_ptr<StoredChannel> &entry)
{
	MT_EB
	MT_FUNC("Storage::Add" << entry);

	SYNC_WLOCK(StoredChannels_);
	StoredChannels_[entry->Name()] = entry;

	MT_EE
}

void Storage::Add(const boost::shared_ptr<Committee> &entry)
{
	MT_EB
	MT_FUNC("Storage::Add" << entry);

	SYNC_WLOCK(Committees_);
	Committees_[entry->Name()] = entry;

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

boost::shared_ptr<StoredUser> Storage::Get_StoredUser(boost::uint32_t id, bool deep) const
{
	MT_EB
	MT_FUNC("Storage::Get_StoredUser" << id << deep);

	SYNC_RWLOCK(StoredUsers_);
	StoredUsers_t::const_iterator i = StoredUsers_.find(id);
	boost::shared_ptr<StoredUser> ret;
	if (i == StoredUsers_.end())
	{
		if (deep)
		{
			// look for entry ...
		}
		MT_RET(ret);
	}
	ret = i->second;
	MT_RET(ret);

	MT_EE
}

boost::shared_ptr<StoredNick> Storage::Get_StoredNick(const std::string &name, bool deep) const
{
	MT_EB
	MT_FUNC("Storage::Get_StoredNick" << name << deep);

	SYNC_RWLOCK(StoredNicks_);
	StoredNicks_t::const_iterator i = StoredNicks_.find(name);
	boost::shared_ptr<StoredNick> ret;
	if (i == StoredNicks_.end())
	{
		if (deep)
		{
			// look for entry ...
		}
		MT_RET(ret);
	}
	ret = i->second;
	MT_RET(ret);

	MT_EE
}

boost::shared_ptr<StoredChannel> Storage::Get_StoredChannel(const std::string &name, bool deep) const
{
	MT_EB
	MT_FUNC("Storage::Get_StoredChannel" << name << deep);

	SYNC_RWLOCK(StoredChannels_);
	StoredChannels_t::const_iterator i = StoredChannels_.find(name);
	boost::shared_ptr<StoredChannel> ret;
	if (i == StoredChannels_.end())
	{
		if (deep)
		{
			// look for entry ...
		}
		MT_RET(ret);
	}
	ret = i->second;
	MT_RET(ret);

	MT_EE
}

boost::shared_ptr<Committee> Storage::Get_Committee(const std::string &name, bool deep) const
{
	MT_EB
	MT_FUNC("Storage::Get_Committee" << name << deep);

	SYNC_RWLOCK(Committees_);
	Committees_t::const_iterator i = Committees_.find(name);
	boost::shared_ptr<Committee> ret;
	if (i == Committees_.end())
	{
		if (deep)
		{
			// look for entry ...
		}
		MT_RET(ret);
	}
	ret = i->second;
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

void Storage::Del(const boost::shared_ptr<StoredUser> &entry)
{
	MT_EB
	MT_FUNC("Storage::Del" << entry);

	SYNC_WLOCK(StoredUsers_);
	StoredUsers_t::iterator i = StoredUsers_.find(entry->ID());
	if (i != StoredUsers_.end() && i->second == entry)
	{
		StoredUsers_.erase(i);
		// Now get rid of it from the DB ...
	}

	MT_EE
}

void Storage::Del(const boost::shared_ptr<StoredNick> &entry)
{
	MT_EB
	MT_FUNC("Storage::Del" << entry);

	SYNC_WLOCK(StoredNicks_);
	StoredNicks_t::iterator i = StoredNicks_.find(entry->Name());
	if (i != StoredNicks_.end() && i->second == entry)
	{
		StoredNicks_.erase(i);
		// Now get rid of it from the DB ...
	}

	MT_EE
}

void Storage::Del(const boost::shared_ptr<StoredChannel> &entry)
{
	MT_EB
	MT_FUNC("Storage::Del" << entry);

	SYNC_WLOCK(StoredChannels_);
	StoredChannels_t::iterator i = StoredChannels_.find(entry->Name());
	if (i != StoredChannels_.end() && i->second == entry)
	{
		StoredChannels_.erase(i);
		// Now get rid of it from the DB ...
	}

	MT_EE
}

void Storage::Del(const boost::shared_ptr<Committee> &entry)
{
	MT_EB
	MT_FUNC("Storage::Del" << entry);

	SYNC_WLOCK(Committees_);
	Committees_t::iterator i = Committees_.find(entry->Name());
	if (i != Committees_.end() && i->second == entry)
	{
		Committees_.erase(i);
		// Now get rid of it from the DB ...
	}

	MT_EE
}

