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
#define RCSID(x,y) const char *rcsid_magick__server_cpp_ ## x () { return y; }
RCSID(magick__server_cpp, "@(#)$Id$");

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

void Server::Disconnect()
{
	MT_EB
	MT_FUNC("Server::Disconnect");

	parent_ = NULL;

	std::list<boost::shared_ptr<Server> >::iterator i;
	for (i=children_.begin(); i!=children_.end(); ++i)
		(*i)->Disconnect();
	children_.clear();

	// Go through and mark users as SQUIT

	MT_EE
}

Server::Server(const std::string &name, const std::string &desc,
			   const std::string &id, const std::string &altname)
			  : name_(name), description_(desc), id_(id),
				altname_(altname.empty() ? name : altname),
				last_ping_(boost::date_time::not_a_date_time),
				lag_(boost::date_time::not_a_date_time), parent_(NULL)
{
	MT_EB
	MT_FUNC("Server::Server" << name << desc << id << altname);


	MT_EE
}

Server::~Server()
{
	MT_EB
	MT_FUNC("Server::~Server");


	MT_EE
}

void Server::Connect(const boost::shared_ptr<Server> &s)
{
	MT_EB
	MT_FUNC("Server::Connect" << "(const boost::shared_ptr<Server> &) s");

	s->parent_ = this;
	children_.push_back(s);

	MT_EE
}

void Server::Disconnect(const std::string &name)
{
	MT_EB
	MT_FUNC("Server::Disconnect" << name);

	std::list<boost::shared_ptr<Server> >::iterator i;
	for (i=children_.begin(); i!=children_.end(); ++i)
		if ((*i)->Name() == name)
		{
			(*i)->Disconnect();
			children_.erase(i);
			break;
		}

	MT_EE
}

void Server::Ping()
{
	MT_EB
	MT_FUNC("Server::Ping");


	MT_EE
}

size_t Server::Users() const
{
	MT_EB
	MT_FUNC("Server::Users");

	MT_RET(0);

	MT_EE
}

size_t Server::Opers() const
{
	MT_EB
	MT_FUNC("Server::Opers");

	MT_RET(0);

	MT_EE
}

Uplink::Uplink(const std::string &id)
	: Server(::ROOT->ConfigValue<std::string>("server-name"),
			 ::ROOT->ConfigValue<std::string>("server-desc"), id),
	  flack_(ROOT->ConfigValue<std::string>("filesystem.flack-dir"),
			 ROOT->ConfigValue<unsigned int>("filesystem.flack-memory"))
{
	MT_EB
	MT_FUNC("Uplink::Uplink" << id);

	if (ROOT->ConfigExists("filesystem.flack-file-max"))
		flack_.Max_Size(ROOT->ConfigValue<boost::uint64_t>("filesystem.flack-file-max"));
	flack_.Clear();

	// The first thread will start the rest.
	boost::mutex::scoped_lock scoped_lock(lock_);
	for (size_t i = 0; i < ROOT->ConfigValue<unsigned int>("general.min-threads"); ++i)
	{
		boost::thread *thr = new boost::thread(boost::bind(&Uplink::operator(), this));
		if (thr)
			workers_.push_back(thr);
	}

	MT_EE
}

Uplink::~Uplink()
{
	MT_EB
	MT_FUNC("Uplink::~Uplink");

	ROOT->proto.SQUIT(*this);
	Disconnect();

	std::list<boost::thread *> workers;
	{
		boost::mutex::scoped_lock scoped_lock(lock_);
		workers.swap(workers_);
		cond_.notify_all();
	}

	std::list<boost::thread *>::iterator i;
	for (i=workers.begin(); i!=workers.end(); ++i)
	{
		(*i)->join();
		delete *i;
	}

	MT_EE
}

void Uplink::operator()()
{
	MT_ASSIGN(MAGICK_TRACE_MAIN);

	MT_EB
	MT_FUNC("Uplink::operator()");

	while (true)
	{
		Message m;
		{
			boost::mutex::scoped_lock scoped_lock(lock_);
			// Uplink is in destructor, so we die now.
			if (workers_.empty())
				break;

			unsigned int wsz = workers_.size();
			unsigned int msgq = pending_.size();
			if (wsz > ROOT->ConfigValue<unsigned int>("general.max-threads") ||
				(wsz > ROOT->ConfigValue<unsigned int>("general.min-threads") &&
				 msgq < ROOT->ConfigValue<unsigned int>("general.low-water-mark") +
				 (std::max(wsz - 2, 0u) * ROOT->ConfigValue<unsigned int>("general.high-water-mark"))))
			{
				NLOG(Info, _("Too many worker threads, killing one."));
				std::list<boost::thread *>::iterator i;
				for (i = workers_.begin(); i != workers_.end(); ++i)
					if (*(*i) == boost::thread())
					{
						delete *i;
						workers_.erase(i);
						break;
					}
				break;
			}

			while (wsz < ROOT->ConfigValue<unsigned int>("general.min-threads") ||
				   msgq > wsz * ROOT->ConfigValue<unsigned int>("general.high-water-mark"))
			{
				NLOG(Info, _("Not enough worker threads, creating one."));
				boost::thread *thr = new boost::thread(boost::bind(&Uplink::operator(), this));
				if (thr)
				{
					workers_.push_back(thr);
					++wsz;
				}
			}

			if (!msgq)
			{
				MT_FLUSH();
				cond_.wait(scoped_lock);
				if (pending_.empty())
					continue;
			}
			m = pending_.front();
			pending_.pop_front();
		}
		if (m.Class() == Message::Ignore)
			continue;

		if (!m.Process())
		{
			LOG(Warning, "Processing failed for message: %1%", m);
		}
		MT_FLUSH();
	}

	MT_EE
	MT_FLUSH();
}

class RecursiveOperation
{
protected:
	virtual bool visitor(const boost::shared_ptr<Server> &) = 0;

public:
	RecursiveOperation() {}
	virtual ~RecursiveOperation() {}

	void operator()(const Uplink &start)
	{
		MT_EB
		MT_FUNC("RecursiveOperation::operator()" << "(const Uplink &) start");

		std::stack<std::pair<std::list<boost::shared_ptr<Server> >::const_iterator,
					std::list<boost::shared_ptr<Server> >::const_iterator> > is;

		std::list<boost::shared_ptr<Server> >::const_iterator i = start.Children().begin();
		std::list<boost::shared_ptr<Server> >::const_iterator ie = start.Children().end();

		while (i != ie && !is.empty())
		{
			if (i == ie)
			{
				i = is.top().first;
				ie = is.top().second;
				is.pop();
				++i;
			}

			if (!this->visitor(*i))
				break;

			if (!(*i)->Children().empty())
			{
				is.push(make_pair(i, ie));
				i = (*i)->Children().begin();
				ie = (*i)->Children().end();
				continue;
			}

			++i;
		}

		MT_EE
	}
};

class FindServer : public RecursiveOperation
{
	std::string name_;
	boost::shared_ptr<Server> result_;

	virtual bool visitor(const boost::shared_ptr<Server> &s)
	{
		MT_EB
		MT_FUNC("FindServer::visitor" << "(const boost::shared_ptr<Server> &) s");

		if (s->Name() == name_)
		{
			result_ = s;
			MT_RET(false);
		}
		MT_RET(true);
		MT_EE
	}

public:
	FindServer(const std::string &name) : name_(name) {}
	virtual ~FindServer() {}
	const boost::shared_ptr<Server> &Result() const { return result_; }
};

boost::shared_ptr<Server> Uplink::Find(const std::string &name) const
{
	MT_EB
	MT_FUNC("Uplink::Find" << name);

	FindServer fs(name);
	fs(*this);
	boost::shared_ptr<Server> rv = fs.Result();
	MT_NRET(boost::shared_ptr<Server>, rv);

	MT_EE
}

class FindServerID : public RecursiveOperation
{
	std::string id_;
	boost::shared_ptr<Server> result_;

	virtual bool visitor(const boost::shared_ptr<Server> &s)
	{
		MT_EB
		MT_FUNC("FindServerID::visitor" << "(const boost::shared_ptr<Server> &) s");

		if (s->ID() == id_)
		{
			result_ = s;
			MT_RET(false);
		}
		MT_RET(true);
		MT_EE
	}

public:
	FindServerID(const std::string &id) : id_(id) {}
	virtual ~FindServerID() {}
	const boost::shared_ptr<Server> &Result() const { return result_; }
};

boost::shared_ptr<Server> Uplink::FindID(const std::string &id) const
{
	MT_EB
	MT_FUNC("Uplink::FindID" << id);

	FindServerID fs(id);
	fs(*this);
	boost::shared_ptr<Server> rv = fs.Result();
	MT_NRET(boost::shared_ptr<Server>, rv);

	MT_EE
}

class ServerPing : public RecursiveOperation
{
	virtual bool visitor(const boost::shared_ptr<Server> &s)
	{
		MT_EB
		MT_FUNC("ServerPing::visitor" << "(const boost::shared_ptr<Server> &) s");

		s->Ping();
		MT_RET(true);

		MT_EE
	}
public:
	ServerPing() {}
	virtual ~ServerPing() {}
};

void Uplink::Ping()
{
	MT_EB
	MT_FUNC("Uplink::Ping");

	ServerPing sp;
	sp(*this);

	MT_EE
}

bool Uplink::Write()
{
	MT_EB
	MT_FUNC("Uplink::Write");

	bool rv = write_;
	write_ = false;
	MT_RET(rv);

	MT_EE
}

void Uplink::Push(const std::deque<Message> &in)
{
	MT_EB
	MT_FUNC("Uplink::Push" << "(const std::deque<Message> &) in");

	std::deque<Message>::const_iterator i;
	for (i=in.begin(); i!=in.end(); ++i)
		de_.Add(*i);

	MT_EE
}

void Uplink::Push(const Message &in)
{
	MT_EB
	MT_FUNC("Uplink::Push" << in);

	boost::mutex::scoped_lock scoped_lock(lock_);
	pending_.push_back(in);
	cond_.notify_one();

	MT_EE
}

