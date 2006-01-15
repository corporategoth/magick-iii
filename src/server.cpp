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
#include "liveuser.h"

void Server::Disconnect()
{
	MT_EB
	MT_FUNC("Server::Disconnect");

	parent_ = boost::shared_ptr<Server>();

	SYNC_LOCK(children_);
	children_t::iterator i;
	for (i=children_.begin(); i!=children_.end(); ++i)
		(*i)->Disconnect();
	children_.clear();

	// Go through and mark users as SQUIT
	users_.clear();

	MT_EE
}

Server::Server(const std::string &name, const std::string &desc,
			   const std::string &id, const std::string &altname)
			  : name_(name), description_(desc), id_(id),
				altname_(altname.empty() ? name : altname),
				last_ping_(boost::date_time::not_a_date_time),
				ping_event_(0), SYNC_NRWINIT(users_, reader_priority)
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

bool Server::ChildrenAction::operator()(const boost::shared_ptr<Server> &start)
{
	MT_EB
	MT_FUNC("Server::ChildrenAction::operator()" << start);

	SYNCP_LOCK(start, children_);
	children_t::iterator i;
	for (i = start->children_.begin(); i != start->children_.end(); ++i)
	{
		if (!this->visitor(*i))
			MT_RET(false);
		if (recursive_ && !this->operator()(*i))
			MT_RET(false);
	}

	MT_RET(true);
	MT_EE
}

bool Server::UsersAction::operator()(const boost::shared_ptr<Server> &start)
{
	MT_EB
	MT_FUNC("Server::UserAction::operator()" << start);

	{
		SYNCP_RLOCK(start, users_);
		users_t::iterator i;
		for (i = start->users_.begin(); i != start->users_.end(); ++i)
		{
			if (!this->visitor(*i))
				MT_RET(false);
		}
	}

	if (recursive_)
	{
		SYNCP_LOCK(start, children_);
		children_t::iterator i;
		for (i = start->children_.begin(); i != start->children_.end(); ++i)
		{
			if (!this->operator()(*i))
				MT_RET(false);
		}
	}

	MT_RET(true);
	MT_EE
}

boost::posix_time::ptime Server::Last_Ping() const
{
	MT_EB
	MT_FUNC("Server::Last_Ping");

	SYNC_LOCK(lag_);
	MT_RET(last_ping_);
	MT_EE
}

boost::posix_time::time_duration Server::Lag() const
{
	MT_EB
	MT_FUNC("Server::Lag");

	SYNC_LOCK(lag_);
	MT_RET(lag_);
	MT_EE
}

void Server::Signon(const boost::shared_ptr<LiveUser> &lu)
{
	MT_EB
	MT_FUNC("Server::Signon" << lu);

	SYNC_WLOCK(users_);
	users_.insert(lu);

	MT_EE
}

void Server::Signoff(const boost::shared_ptr<LiveUser> &lu)
{
	MT_EB
	MT_FUNC("Server::Signoff" << lu);

	SYNC_WLOCK(users_);
	users_.erase(lu);

	MT_EE
}

Server::children_t Server::getChildren() const
{
	MT_EB
	MT_FUNC("Server::getChildren");

	SYNC_LOCK(children_);
	MT_RET(children_);
	MT_EE
}

Server::users_t Server::getUsers() const
{
	MT_EB
	MT_FUNC("Server::getUsers");

	SYNC_RLOCK(users_);
	MT_RET(users_);
	MT_EE
}

void Server::Connect(const boost::shared_ptr<Server> &s)
{
	MT_EB
	MT_FUNC("Server::Connect" << s);

	SYNC_LOCK(children_);
	s->parent_ = self.lock();
	children_.insert(s);

	MT_EE
}

static bool operator<(const Server::children_t::value_type &lhs,
					  const std::string &rhs)
{
	return (*lhs < rhs);
}

bool Server::Disconnect(const boost::shared_ptr<Server> &s)
{
	MT_EB
	MT_FUNC("Server::Disconnect" << s);

	SYNC_LOCK(children_);
	children_t::iterator i = children_.find(s);
	if (i == children_.end())
		MT_RET(false);

	if_StorageDeleter<Server>(ROOT->data).Del(*i);
	(*i)->Disconnect();
	children_.erase(i);

	MT_RET(true);
	MT_EE
}

bool Server::Disconnect(const std::string &name)
{
	MT_EB
	MT_FUNC("Server::Disconnect" << name);

	SYNC_LOCK(children_);
	children_t::iterator i = std::lower_bound(children_.begin(),
											  children_.end(),
											  name);
	if (i == children_.end() || **i != name)
		MT_RET(false);

	if_StorageDeleter<Server>(ROOT->data).Del(*i);
	(*i)->Disconnect();
	children_.erase(i);

	MT_RET(true);
	MT_EE
}

void Server::PING()
{
	MT_EB
	MT_FUNC("Server::PING");

	SYNC_LOCK(lag_);
	if (ping_event_)
	{
		ROOT->event->Cancel(ping_event_);
		ping_event_ = 0;
	}

	boost::shared_ptr<Server> p = self.lock();
	Jupe *j = dynamic_cast<Jupe *>(p.get());
	// Do not do this if *WE* are a jupe.
	if (j)
		return;

	while (p && !j)
	{
		p = p->Parent();
		j = dynamic_cast<Jupe *>(p.get());
	}

	std::string out;
	if (j)
		ROOT->proto.addline(*j, out, ROOT->proto.tokenise("PING") + " :" + Name());
	else
		ROOT->proto.addline(out, ROOT->proto.tokenise("PING") + " :" + Name());
	ROOT->proto.send(out);
	last_ping_ = mantra::GetCurrentDateTime();

	MT_EE
}

void Server::PONG()
{
	MT_EB
	MT_FUNC("Server::PONG");

	SYNC_LOCK(lag_);
	if (last_ping_.is_special())
		return;

	lag_ = mantra::GetCurrentDateTime() - last_ping_;
	last_ping_ = boost::date_time::not_a_date_time;
	ping_event_ = ROOT->event->Schedule(boost::bind(&Server::PING, this),
										ROOT->ConfigValue<mantra::duration>("general.ping-frequency"));

	MT_EE
}

// This is a REQUEST ...
void Server::SQUIT(const std::string &reason)
{
	MT_EB
	MT_FUNC("Server::SQUIT" << reason);

	std::string out;
	ROOT->proto.addline(out, ROOT->proto.tokenise("SQUIT") +
						' ' + Name() + " :" + reason);
	ROOT->proto.send(out);

	MT_EE
}

size_t Server::Users() const
{
	MT_EB
	MT_FUNC("Server::Users");

	SYNC_RLOCK(users_);
	size_t rv = users_.size();

	MT_RET(rv);
	MT_EE
}

size_t Server::Opers() const
{
	MT_EB
	MT_FUNC("Server::Opers");

	SYNC_RLOCK(users_);
	size_t rv = std::count_if(users_.begin(), users_.end(),
							  boost::bind(&LiveUser::Mode, _1, 'o'));

	MT_RET(rv);
	MT_EE
}

void Jupe::SQUIT(const std::string &reason)
{
	MT_EB
	MT_FUNC("Jupe::SQUIT" << reason);

	std::string out;

	boost::shared_ptr<Server> p = Parent();
	Jupe *j = ((p && p->Parent()) ? dynamic_cast<Jupe *>(p.get()) : NULL);
	if (j)
		ROOT->proto.addline(*j, out, ROOT->proto.tokenise("SQUIT") + Name() +
							" :" + reason);
	else
		ROOT->proto.addline(out, ROOT->proto.tokenise("SQUIT") + Name() +
							" :" + reason);

	ROOT->proto.send(out);
	if (p)
		p->Disconnect(self.lock());

	MT_EE
}

bool Jupe::RAW(const std::string &cmd,
			   const std::vector<std::string> &args,
			   bool forcecolon) const
{
	MT_EB
	MT_FUNC("Jupe::RAW" << cmd << args << forcecolon);

	std::string out;
	ROOT->proto.addline(*this, out, ROOT->proto.tokenise(cmd) +
						ROOT->proto.assemble(args, forcecolon));
	bool rv = ROOT->proto.send(out);
	MT_RET(rv);

	MT_EE
}

Uplink::Uplink(const std::string &password, const std::string &id)
	: Jupe(::ROOT->ConfigValue<std::string>("server-name"),
		   ::ROOT->ConfigValue<std::string>("server-desc"), id,
		   std::string()), password_(password), burst_(false),
	  flack_(ROOT->ConfigValue<std::string>("filesystem.flack-dir"),
			 ROOT->ConfigValue<boost::uint64_t>("filesystem.flack-memory"))
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

void Uplink::Disconnect()
{
	MT_EB
	MT_FUNC("Uplink::Disconnect");

	Server::Disconnect();

	std::list<boost::thread *> workers;
	{
		boost::mutex::scoped_lock scoped_lock(lock_);
		workers.swap(workers_);
		cond_.notify_all();
	}

	// This is not the first time ;)
	if (!workers.empty())
		SQUIT();

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
	MT_ASSIGN(MAGICK_TRACE_WORKER);
	MT_AUTOFLUSH(true);

	MT_EB
	MT_FUNC("Uplink::operator()");

	boost::thread local_thread;
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
					if (*(*i) == local_thread)
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

		if (!m.Process())
		{
			LOG(Warning, "Processing failed for message: %1%", m);
		}
		MT_FLUSH();
	}

	MT_EE
	MT_FLUSH();
}

class FindServer : public Server::ChildrenAction
{
	std::string name_;
	boost::shared_ptr<Server> result_;

	virtual bool visitor(const boost::shared_ptr<Server> &s)
	{
		MT_EB
		MT_FUNC("FindServer::visitor" << s);

		if (s->Name() == name_)
		{
			result_ = s;
			MT_RET(false);
		}

		MT_RET(true);
		MT_EE
	}

public:
	FindServer(const std::string &name) : Server::ChildrenAction(true), name_(name) {}
	virtual ~FindServer() {}
	const boost::shared_ptr<Server> &Result() const { return result_; }
};

boost::shared_ptr<Server> Uplink::Find(const std::string &name) const
{
	MT_EB
	MT_FUNC("Uplink::Find" << name);

	if (name == Name())
		MT_RET(ROOT->getUplink());

	FindServer fs(name);
	fs(self.lock());
	boost::shared_ptr<Server> rv = fs.Result();
	MT_RET(rv);

	MT_EE
}

class FindServerID : public Server::ChildrenAction
{
	std::string id_;
	boost::shared_ptr<Server> result_;

	virtual bool visitor(const boost::shared_ptr<Server> &s)
	{
		MT_EB
		MT_FUNC("FindServerID::visitor" << s);

		if (s->ID() == id_)
		{
			result_ = s;
			MT_RET(false);
		}
		MT_RET(true);
		MT_EE
	}

public:
	FindServerID(const std::string &id) : Server::ChildrenAction(true), id_(id) {}
	virtual ~FindServerID() {}
	const boost::shared_ptr<Server> &Result() const { return result_; }
};

boost::shared_ptr<Server> Uplink::FindID(const std::string &id) const
{
	MT_EB
	MT_FUNC("Uplink::FindID" << id);

	if (id == ID())
		MT_RET(ROOT->getUplink());

	FindServerID fs(id);
	fs(self.lock());
	boost::shared_ptr<Server> rv = fs.Result();
	MT_RET(rv);

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

bool Uplink::CheckPassword(const std::string &password) const
{
	MT_EB
	MT_FUNC("Uplink::CheckPassword");

	if (password == password_)
		MT_RET(true);

	MT_RET(false);
	MT_EE
}

void Uplink::Push(const std::deque<Message> &in)
{
	MT_EB
	MT_FUNC("Uplink::Push" << in);

	std::deque<Message>::const_iterator i;
	for (i=in.begin(); i!=in.end(); ++i)
		de.Add(*i);

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

bool Uplink::KILL(const boost::shared_ptr<LiveUser> &user,
				  const std::string &reason) const
{
	MT_EB
	MT_FUNC("Uplink::KILL" << user << reason);

	std::string out;
	ROOT->proto.addline(*this, out, ROOT->proto.tokenise("KILL") +
						" " + user->Name() + " :" + reason);
	bool rv = ROOT->proto.send(out);
	if (rv)
		user->Kill(boost::shared_ptr<LiveUser>(), reason);

	MT_RET(rv);
	MT_EE
}

bool Uplink::NUMERIC(Numeric_t num, const std::string &target,
					 const std::vector<std::string> &args,
					 bool forcecolon) const
{
	MT_EB
	MT_FUNC("Uplink::NUMERIC" << num << target << args << forcecolon);

	std::string ostr, istr = boost::lexical_cast<std::string>(static_cast<int>(num)) + " " +
								target + ROOT->proto.assemble(args, forcecolon);
	ROOT->proto.addline(*this, ostr, istr);
	bool rv = ROOT->proto.send(ostr);
	MT_RET(rv);

	MT_EE
}

bool Uplink::ERROR(const std::string &arg) const
{
	MT_EB
	MT_FUNC("Uplink::ERROR" << arg);

	std::string ostr;
	ROOT->proto.addline(*this, ostr, ROOT->proto.tokenise("ERROR") +
						" :" + arg);
	bool rv = ROOT->proto.send(ostr);
	MT_RET(rv);

	MT_EE
}

