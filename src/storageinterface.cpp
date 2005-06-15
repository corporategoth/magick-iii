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
#define RCSID(x,y) const char *rcsid_magick__storageinterface_cpp_ ## x () { return y; }
RCSID(magick__storageinterface_cpp, "@(#)$Id$");

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
#include "storageinterface.h"

unsigned int StorageInterface::RowExists(const mantra::ComparisonSet &search) const throw(mantra::storage_exception)
{
	MT_EB
	MT_FUNC("StorageInterface::RowExists" << search);

	unsigned int rv = ROOT->data.backend_.first->RowExists(table_, search);

	MT_RET(rv);
	MT_EE
}

bool StorageInterface::InsertRow(const mantra::Storage::RecordMap &data) throw(mantra::storage_exception)
{
	MT_EB
	MT_FUNC("StorageInterface::InsertRow" << data);

	unsigned int rv;
	if (update_.empty())
	{
		rv = ROOT->data.backend_.first->InsertRow(table_, data);
	}
	else
	{
		mantra::Storage::RecordMap entry = data;
		entry[update_] = mantra::GetCurrentDateTime();
		rv = ROOT->data.backend_.first->InsertRow(table_, entry);
	}

	MT_RET(rv);
	MT_EE
}

unsigned int StorageInterface::ChangeRow(const mantra::Storage::RecordMap &data, 
				const mantra::ComparisonSet &search) throw(mantra::storage_exception)
{
	MT_EB
	MT_FUNC("StorageInterface::ChangeRow" << data << search);

	unsigned int rv;
	if (update_.empty())
	{
		rv = ROOT->data.backend_.first->ChangeRow(table_, data, search);
	}
	else
	{
		mantra::Storage::RecordMap entry = data;
		entry[update_] = mantra::GetCurrentDateTime();
		rv = ROOT->data.backend_.first->ChangeRow(table_, entry, search);
	}

	MT_RET(rv);
	MT_EE
}

void StorageInterface::RetrieveRow(mantra::Storage::DataSet &data,
				const mantra::ComparisonSet &search,
				const mantra::Storage::FieldSet &fields) throw(mantra::storage_exception)
{
	MT_EB
	MT_FUNC("StorageInterface::RetrieveRow" << data << search << fields);

	ROOT->data.backend_.first->RetrieveRow(table_, data, search, fields);

	MT_EE
}

unsigned int StorageInterface::RemoveRow(const mantra::ComparisonSet &search) throw(mantra::storage_exception)
{
	MT_EB
	MT_FUNC("StorageInterface::RemoveRow");

	unsigned int rv = ROOT->data.backend_.first->RemoveRow(table_, search);

	MT_RET(rv);
	MT_EE
}

mantra::StorageValue StorageInterface::Minimum(const std::string &column,
				const mantra::ComparisonSet &search) throw(mantra::storage_exception)
{
	MT_EB
	MT_FUNC("StorageInterface::Minimum" << column << search);

	mantra::StorageValue rv = ROOT->data.backend_.first->Minimum(table_, column, search);

	MT_RET(rv);
	MT_EE
}

mantra::StorageValue StorageInterface::Maximum(const std::string &column,
				const mantra::ComparisonSet &search) throw(mantra::storage_exception)
{
	MT_EB
	MT_FUNC("StorageInterface::Maximum" << column << search);

	mantra::StorageValue rv = ROOT->data.backend_.first->Maximum(table_, column, search);

	MT_RET(rv);
	MT_EE
}

// Aliases that use the above.
bool StorageInterface::GetRow(const mantra::ComparisonSet &search, mantra::Storage::RecordMap &data)
{
	MT_EB
	MT_FUNC("StorageInterface::GetRow" << search << data);

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
	MT_FUNC("StorageInterface::GetField" << search << column);

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
	MT_FUNC("StorageInterface::PutField" << search << column << value);

	mantra::Storage::RecordMap data;
	data[column] = value;
	unsigned int rv = ChangeRow(data, search);

	MT_RET(rv == 1u);
	MT_EE
}

