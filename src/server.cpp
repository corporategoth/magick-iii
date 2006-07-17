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

static void pre_process()
{
	MT_ASSIGN(MAGICK_TRACE_WORKER);
}

static void process(const Message &m)
{
	MT_EB
	MT_FUNC("process" << m);

	if (!m.Process())
		LOG(Warning, "Processing failed for message: %1%", m);

	MT_EE
}

bool Uplink::Write(const char *buf, size_t sz)
{
	MT_EB
	MT_FUNC("Uplink::Write" << buf << sz);

	bool rv = flack_.Write(buf, sz);
	if (rv)
		group_.SetWrite(sock_, true);

	MT_RET(rv);
	MT_EE
}

Uplink::Uplink(const mantra::Socket &sock, const std::string &password,
			   const std::string &id)
	: Jupe(::ROOT->ConfigValue<std::string>("server-name"),
		   ::ROOT->ConfigValue<std::string>("server-desc"), id,
		   std::string()),
	  sock_(sock), group_((mantra::SocketGroup::Implement_t)
						  ROOT->ConfigValue<unsigned int>("general.multiplex-mode")),
	  disconnect_(false), password_(password), burst_(false),
	  queue_(boost::bind(&process, _1),
			 ROOT->ConfigValue<unsigned int>("general.min-threads"),
			 ROOT->ConfigValue<unsigned int>("general.max-threads")),
	  flack_(ROOT->ConfigValue<std::string>("filesystem.flack-dir"),
			 ROOT->ConfigValue<boost::uint64_t>("filesystem.flack-memory"))
{
	MT_EB
	MT_FUNC("Uplink::Uplink" << sock << password << id);

	queue_.Start(&pre_process);

	if (ROOT->ConfigExists("filesystem.flack-file-max"))
		flack_.Max_Size(ROOT->ConfigValue<boost::uint64_t>("filesystem.flack-file-max"));
	flack_.Clear();

	group_.SetRead(sock_, true);

	MT_EE
}

boost::shared_ptr<Uplink> Uplink::create(const remote_connection &rc,
										 const boost::function0<bool> &check)
{
	MT_EB
	MT_FUNC("Uplink::create" << rc << check);

	mantra::Socket sock;
	if (!ROOT->ConfigExists("bind"))
		sock = mantra::Socket(mantra::Socket::STREAM,
							  ROOT->ConfigValue<std::string>("bind"));
	else
	{
		mantra::Socket::SockDomain_t domain = mantra::Socket::RemoteDomain(rc.host);
		sock = mantra::Socket(domain, mantra::Socket::STREAM);
	}
	sock.Blocking(false);

	boost::posix_time::ptime now = mantra::GetCurrentDateTime();
	boost::posix_time::ptime expire = now +
		ROOT->ConfigValue<mantra::duration>("general.connection-timeout");

	LOG(Info, _("Attempting connection to %1%[%2%]."), rc.host % rc.port);
	boost::shared_ptr<Uplink> rv;
	if (!sock.StartConnect(rc.host, rc.port))
		MT_RET(rv);

	mantra::SocketGroup sg;
	sg.SetWrite(sock, true);

	do
	{
		if (expire < now)
		{
			LOG(Error, _("Timed out attempting connection to %1%[%2%]."),
					rc.host % rc.port);
			sock.Close();
			break;
		}

		mantra::SocketGroup::WaitResultMap res;
		int rv = sg.Wait(res, 500);
		if (rv == SOCK_ETIMEDOUT)
		{
			continue;
		}
		else if (rv != 0)
		{
			LOG(Error, _("Received error code #%1% attempting connection to socket %2%[%3%]."),
					rv % rc.host % rc.port);
			sock.Close();
			break;
		}
		else if (res.size() == 0)
			continue;
		else if (res.size() > 1 || res.begin()->first != sock)
		{
			LOG(Error, _("Invalid data in result while attmpting connection to socket %1%[%2%]"),
					rc.host % rc.port);
			sock.Close();
			break;
		}
		else if (res.begin()->second & mantra::SocketGroup::Error)
		{
			sock.Retrieve_Error();
			if (sock.Last_Error())
			{
				LOG(Warning, _("Received socket error #%1% (%2%) on %3%[%4%], closing."),
						sock.Last_Error() % sock.Last_Error_String() % rc.host % rc.port);
			}
			else
			{
				LOG(Warning, _("Could not establish connection to %1%[%2%]."),
						rc.host % rc.port);
			}
			break;
		}
		else if (res.begin()->second & mantra::SocketGroup::Write)
		{
			if (sock.CompleteConnect(0))
				break;

			if (sock.Last_Error() != SOCK_EAGAIN)
			{
				LOG(Warning, _("Received socket error #%1% (%2%) attempting connection to %3%[%4%]."),
						sock.Last_Error() % sock.Last_Error_String() % rc.host % rc.port);
				sock.Close();
				break;
			}
		}

		MT_FLUSH();
		now = mantra::GetCurrentDateTime();
	}
	while (sock.Valid() && check());

	if (!sock.Valid())
		MT_RET(rv);

	LOG(Info, _("Connection established to %1%[%2%]."), rc.host % rc.port);
	rv.reset(new Uplink(sock, rc.password, ROOT->proto.NumericToID(rc.numeric)));
	rv->self = rv;

	MT_RET(rv);
	MT_EE
}

void Uplink::Disconnect()
{
	MT_EB
	MT_FUNC("Uplink::Disconnect");

	Server::Disconnect();

	queue_.Clear();
	SQUIT();

	MT_EE
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

bool Uplink::CheckPassword(const std::string &password) const
{
	MT_EB
	MT_FUNC("Uplink::CheckPassword");

	if (password == password_)
		MT_RET(true);

	MT_RET(false);
	MT_EE
}

/*
void Uplink::Push(const std::deque<Message> &in)
{
	MT_EB
	MT_FUNC("Uplink::Push" << in);

	std::deque<Message>::const_iterator i;
	for (i=in.begin(); i!=in.end(); ++i)
		de.Add(*i);

	MT_EE
}
*/

bool Uplink::operator()(const boost::function0<bool> &check)
{
	MT_EB
	MT_FUNC("Server::operator()" << check);

	std::string in, uin;

	do
	{
		if (disconnect_)
		{
			LOG(Error, _("Disconnect requested to %1%[%2%]."),
					sock_.Remote_Str() % sock_.Remote_Port());
			sock_.Close();
			break;
		}

		mantra::SocketGroup::WaitResultMap res;
		int rv = group_.Wait(res, 500);
		if (rv == SOCK_ETIMEDOUT)
		{
			continue;
		}
		else if (rv != 0)
		{
			LOG(Error, _("Received error code #%1% from socket %2%[%3%]."),
					rv % sock_.Remote_Str() % sock_.Remote_Port());
			sock_.Close();
			break;
		}
		else if (res.size() == 0)
			continue;
		else if (res.size() > 1 || res.begin()->first != sock_)
		{
			LOG(Error, _("Invalid data in result for socket %1%[%2%]."),
					sock_.Remote_Str() % sock_.Remote_Port());
			sock_.Close();
			break;
		}
		else if (res.begin()->second & mantra::SocketGroup::Error)
		{
			sock_.Retrieve_Error();
			if (sock_.Last_Error())
			{
				LOG(Warning, _("Received socket error #%1% (%2%) on socket %3%[%4%], closing."),
						sock_.Last_Error() % sock_.Last_Error_String() % sock_.Remote_Str() % sock_.Remote_Port());
			}
			else
			{
				LOG(Warning, _("Connection to %1%[%2%] closed by foreign host."),
						sock_.Remote_Str() % sock_.Remote_Port());
			}
			sock_.Close();
			break;
		}

		if (res.begin()->second & mantra::SocketGroup::Write)
		{
			char buf[1024];
			int len;
			do
			{
				len = flack_.Read(buf, 1024);
				if (!len)
				{
					group_.SetWrite(sock_, false);
					break;
				}

				rv = sock_.send(buf, len);
				if (rv < 0)
				{
					if (sock_.Last_Error() != SOCK_EAGAIN)
					{
						LOG(Warning, _("Received socket error #%1% (%2%) while sending data to %3%[%4%]."),
								sock_.Last_Error() % sock_.Last_Error_String() %
								sock_.Remote_Str() % sock_.Remote_Port());
						sock_.Close();
					}
					break;
				}
				else
					flack_.Consume(rv);
			}
			while (rv == len);

			if (!sock_.Valid())
				break;
		}

		std::deque<Message> msgs;
		if (res.begin()->second & mantra::SocketGroup::Urgent)
		{
			char buf[1024];
			do
			{
				rv = sock_.recv(buf, 1024, 0, true);
				if (rv < 0)
				{
					if (sock_.Last_Error() != SOCK_EAGAIN)
					{
						LOG(Warning, _("Received socket error #%1% (%2%) while sending data to %3%[%4%]."),
								sock_.Last_Error() % sock_.Last_Error_String() % sock_.Remote_Str() % sock_.Remote_Port());
						sock_.Close();
					}
					break;
				}
				else if (rv == 0)
				{
					LOG(Warning, _("Uplink connection closed by %1%[%2%]."),
							sock_.Remote_Str() % sock_.Remote_Port());
					sock_.Close();
					break;
				}
				else
					uin.append(buf, rv);
			}
			while (rv == 1024);
			if (!sock_.Valid())
				break;
			ROOT->proto.Decode(uin, msgs);
		}

		if (res.begin()->second & mantra::SocketGroup::Read)
		{
			char buf[1024];
			do
			{
				rv = sock_.recv(buf, 1024);
				if (rv < 0)
				{
					if (sock_.Last_Error() != SOCK_EAGAIN)
					{
						LOG(Warning, _("Received socket error #%1% (%2%) while sending data to %3%[%4%]."),
								sock_.Last_Error() % sock_.Last_Error_String() % sock_.Remote_Str() % sock_.Remote_Port());
						sock_.Close();
					}
					break;
				}
				else if (rv == 0)
				{
					LOG(Warning, _("Uplink connection closed by %1%[%2%]."),
							sock_.Remote_Str() % sock_.Remote_Port());
					sock_.Close();
					break;
				}
				else
					in.append(buf, rv);
			}
			while (rv == 1024);
			if (!sock_.Valid())
				break;
			ROOT->proto.Decode(in, msgs);
		}

		std::deque<Message>::const_iterator i;
		for (i=msgs.begin(); i!=msgs.end(); ++i)
			de.Add(*i);

		MT_FLUSH();
	}
	while (sock_.Valid() && check());

	bool rv = check();
	MT_RET(rv);
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

