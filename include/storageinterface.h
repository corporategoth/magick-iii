#ifndef win32
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
#ifndef _MAGICK_STORAGEINTERFACE_H
#define _MAGICK_STORAGEINTERFACE_H 1

#ifdef RCSID
RCSID(magick__storageinterface_h, "@(#) $Id$");
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

#include <mantra/storage/interface.h>

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

#endif // _MAGICK_STORAGEINTERFACE_H
