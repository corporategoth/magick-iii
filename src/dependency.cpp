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
#define RCSID(x,y) const char *rcsid_magick__dependency_cpp_ ## x () { return y; }
RCSID(magick__dependency_cpp, "@(#)$Id$");

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

#include <mantra/core/trace.h>

void Dependency::operator()()
{
	MT_EB
	MT_FUNC("Dependency::operator()");

	// So we don't try to cancel ourselves anymore.
	event_ = 0;

	if (ROOT->getUplink())
	{
		boost::mutex::scoped_lock scoped_lock(ROOT->getUplink()->de_.lock_);
		std::multimap<Type_t, std::string>::const_iterator i;
		for (i=outstanding_.begin(); i!=outstanding_.end(); ++i)
		{
			std::map<Type_t, std::multimap<std::string, boost::shared_ptr<Dependency> > >::iterator j =
				ROOT->getUplink()->de_.outstanding_.find(i->first);
			if (j != ROOT->getUplink()->de_.outstanding_.end())
			{
				std::multimap<std::string, boost::shared_ptr<Dependency> >::iterator k;
				for (k=j->second.lower_bound(i->second); k!=j->second.upper_bound(i->second); ++k)
				{
					if (this == k->second.get())
					{
						j->second.erase(k);
						break;
					}
				}
				if (j->second.empty())
					ROOT->getUplink()->de_.outstanding_.erase(j);
			}
		}
		outstanding_.clear();
	}

	MT_EE
}

Dependency::Dependency(const Message &msg) : msg_(msg), event_(0)
{
	MT_EB
	MT_FUNC("Dependency::Dependency" << msg);

	Update();
	if (!outstanding_.empty())
		event_ = ROOT->event->Schedule(boost::bind(&Dependency::operator(), this),
									   ROOT->ConfigValue<mantra::duration>("general.message-expire"));

	MT_EE
}

Dependency::~Dependency()
{
	MT_EB
	MT_FUNC("Dependency::~Dependency");

	if (event_)
		ROOT->event->Cancel(event_);

	if (ROOT->getUplink())
		ROOT->getUplink()->Push(msg_);

	MT_EE
}

void Dependency::Update()
{
	MT_EB
	MT_FUNC("Dependency::Update");


	MT_EE
}

void Dependency::Satisfy(Dependency::Type_t type, const std::string &meta)
{
	MT_EB
	MT_FUNC("Satisfy");

	// We don't need to be locked, since this will only
	// be called by DependencyEngine, under its lock.
	std::multimap<Type_t, std::string>::iterator i;
	for (i=outstanding_.lower_bound(type); i != outstanding_.upper_bound(type); ++i)
	{
		if (i->second == meta)
		{
			outstanding_.erase(i);
			break;
		}
	}

	MT_EE
}

void DependencyEngine::Add(const Message &m)
{
	MT_EB
	MT_FUNC("DependencyEngine::Add" << m);

	boost::shared_ptr<Dependency> ptr(new Dependency(m));
	if (!ptr->Outstanding().empty())
	{
		boost::mutex::scoped_lock scoped_lock(lock_);
		std::multimap<Dependency::Type_t, std::string>::const_iterator i;
		for (i=ptr->Outstanding().begin(); i!=ptr->Outstanding().end(); ++i)
			outstanding_[i->first].insert(std::make_pair(i->second, ptr));
	}

	MT_EE
}

void DependencyEngine::Satisfy(Dependency::Type_t type, const std::string &meta)
{
	MT_EB
	MT_FUNC("DependencyEngine::Satisfy" << type << meta);

	boost::mutex::scoped_lock scoped_lock(lock_);
	std::map<Dependency::Type_t, std::multimap<std::string, boost::shared_ptr<Dependency> > >::iterator i =
		outstanding_.find(type);
	if (i != outstanding_.end())
	{
		std::multimap<std::string, boost::shared_ptr<Dependency> >::iterator j =
			i->second.lower_bound(meta);
		while (j != i->second.upper_bound(meta))
		{
			j->second->Satisfy(type, meta);
			i->second.erase(j++);
		}
		if (i->second.empty())
			outstanding_.erase(i);
	}

	MT_EE
}

