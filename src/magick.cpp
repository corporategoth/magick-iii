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
#define RCSID(x,y) const char *rcsid_magick__magick_cpp_ ## x () { return y; }
RCSID(magick__magick_cpp, "@(#)$Id$");

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

Magick::Magick()
	: start(boost::posix_time::second_clock::local_time()),
	  pid(getpid()), level(0),
	  opt_command_line_only("Available Options"),
	  opt_common("Config File Overrides"),
	  opt_config_file_only("Config File Only Options"),
	  logger_lock(boost::read_write_scheduling_policy::reader_priority),
	  disconnect(false), shutdown(false), uplink(NULL), event(NULL)
{
	init_config();
}

Magick::~Magick()
{
}


void Magick::Log(mantra::LogLevel::Level_t level, const boost::format &out)
{
	MT_EB
	MT_FUNC("Magick::Log" << level << out);

	boost::read_write_mutex::scoped_read_lock lock(logger_lock);
	if (loggers.empty())
		std::cerr << out.str() << std::endl;
	else
	{
		std::list<mantra::Logger<char> *>::iterator iter;
		for (iter=loggers.begin(); iter!=loggers.end(); ++iter)
			
			(*iter)->Write(level, out);
	}

	MT_EE
}

void Magick::LogHex(mantra::LogLevel::Level_t level, const void *buf,
					size_t len, const std::string &prefix)
{
	MT_EB
	MT_FUNC("Magick::LogHex" << level << "(const void *) buf" << len << prefix);

	boost::read_write_mutex::scoped_read_lock lock(logger_lock);
	if (loggers.empty())
	{
		std::vector<std::string> out;
		mantra::hexdump(buf, len, out);
		for (size_t i=0; i<out.size(); ++i)
			std::cerr << out[i] << std::endl;
	}
	else
	{
		std::list<mantra::Logger<char> *>::iterator iter;
		for (iter=loggers.begin(); iter!=loggers.end(); ++iter)
			(*iter)->WriteHex(level, buf, len, prefix);
	}

	MT_EE
}

void Magick::run(const boost::function0<bool> &check)
{
	MT_EB
	MT_FUNC("Magick::run" << "(const boost::function0<bool> &) check");

	while (check())
	{
		size_t next_conn = 0;

		std::vector<remote_connection> all_remote = opt_config["remote"].as<std::vector<remote_connection> >();
		if (next_conn >= all_remote.size())
			next_conn = 0;

		disconnect = false;
		remote_connection remote = all_remote[next_conn];
		LOG(Info, _("Attempting connection to %1%[%2%]."),
					remote.host % remote.port);

		mantra::Socket sock;
		if (opt_config.count("bind"))
			sock = mantra::Socket(mantra::Socket::STREAM, opt_config["bind"].as<std::string>());
		else
		{
			mantra::Socket::SockDomain_t domain = mantra::Socket::RemoteDomain(remote.host);
			sock = mantra::Socket(domain, mantra::Socket::STREAM);
		}
		sock.Blocking(false);

		boost::posix_time::ptime now = mantra::GetCurrentDateTime();
		boost::posix_time::ptime expire = now +
			ConfigValue<mantra::duration>("general.connection-timeout");

		if (!disconnect && sock.StartConnect(remote.host, remote.port))
		{
			std::string in, uin;
			mantra::SocketGroup sg;
			sg.SetWrite(sock, true);

			do
			{
				if (!uplink && expire < now)
				{
					LOG(Error, _("Timed out attempting connection to %1%[%2%]."),
							remote.host % remote.port);
					sock.Close();
					break;
				}
				else if (disconnect)
				{
					LOG(Error, _("Disconnect requested to %1%[%2%]."),
							remote.host % remote.port);
					sock.Close();
					break;
				}

				if (uplink && uplink->Write())
					sg.SetWrite(sock, true);
				mantra::SocketGroup::WaitResultMap res;
				int rv = sg.Wait(res, 500);
				if (rv == SOCK_ETIMEDOUT)
				{
					continue;
				}
				else if (rv != 0)
				{
					LOG(Error, _("Received error code #%1% from primary socket group, closing socket."), rv);
					sock.Close();
					break;
				}
				else if (res.size() == 0)
					continue;
				else if (res.size() > 1 || res.begin()->first != sock)
				{
					NLOG(Error, _("Invalid data in result from primary socket group, closing socket."));
					sock.Close();
					break;
				}
				else if (res.begin()->second & mantra::SocketGroup::Error)
				{
					sock.Retrieve_Error();
					if (sock.Last_Error())
					{
						LOG(Warning, _("Received socket error #%1% on %2%[%3%], closing."),
								sock.Last_Error() % remote.host % remote.port);
					}
					else if (!uplink)
					{
						LOG(Warning, _("Could not establish connection to %1%[%2%]."),
								remote.host % remote.port);
					}
					else
					{
						LOG(Warning, _("Connection to %2%[%3%] closed by foreign host."),
								remote.host % remote.port);
					}
					sock.Close();
					break;
				}

				if (res.begin()->second & mantra::SocketGroup::Write)
				{
					if (!sock.Connected())
					{
						if (!sock.CompleteConnect(0))
						{
							if (sock.Last_Error() != SOCK_EAGAIN)
							{
								LOG(Warning, _("Received socket error #%1% attempting connection to %2%[%3%]."),
										sock.Last_Error() % remote.host % remote.port);
								sock.Close();
								break;
							}
						}
						else
						{
							LOG(Info, _("Connection established to %1%[%2%]."),
										remote.host % remote.port);

							uplink = new Uplink(proto.NumericToID(remote.numeric));
							sg.SetRead(sock, true);
							proto.Connect(*uplink, remote.password);
						}
					}
					else
					{
						if (!uplink)
						{
							sg.SetWrite(sock, false);
							continue;
						}

						char buf[1024];
						int len;
						do
						{
							len = uplink->flack_.Read(buf, 1024);
							if (!len)
							{
								sg.SetWrite(sock, false);
								break;
							}

							rv = sock.send(buf, len);
							if (rv < 0)
							{
								if (sock.Last_Error() != SOCK_EAGAIN)
								{
									LOG(Warning, _("Received socket error #%1% while sending data to %2%[%3%]."),
											sock.Last_Error() % remote.host % remote.port);
									sock.Close();
								}
								break;
							}
							else
								uplink->flack_.Consume(rv);
						}
						while (rv == len);

						if (!sock.Valid())
							break;
					}
				}

				std::deque<Message> msgs;
				if (res.begin()->second & mantra::SocketGroup::Urgent)
				{
					char buf[1024];
					do
					{
						rv = sock.recv(buf, 1024, 0, true);
						if (rv < 0)
						{
							if (sock.Last_Error() != SOCK_EAGAIN)
							{
								LOG(Warning, _("Received socket error #%1% while sending data to %2%[%3%]."),
										sock.Last_Error() % remote.host % remote.port);
								sock.Close();
							}
							break;
						}
						else if (rv == 0)
						{
							LOG(Warning, _("Uplink connection closed by %1%[%2%]."),
									remote.host % remote.port);
							sock.Close();
						}
						else
							uin.append(buf, rv);
					}
					while (rv == 1024);
					if (!sock.Valid())
						break;
					proto.Decode(uin, msgs, uplink->Tokens());
				}

				if (res.begin()->second & mantra::SocketGroup::Read)
				{
					char buf[1024];
					do
					{
						rv = sock.recv(buf, 1024);
						if (rv < 0)
						{
							if (sock.Last_Error() != SOCK_EAGAIN)
							{
								LOG(Warning, _("Received socket error #%1% while sending data to %2%[%3%]."),
										sock.Last_Error() % remote.host % remote.port);
								sock.Close();
							}
							break;
						}
						else if (rv == 0)
						{
							LOG(Warning, _("Uplink connection closed by %1%[%2%]."),
									remote.host % remote.port);
							sock.Close();
						}
						else
							in.append(buf, rv);
					}
					while (rv == 1024);
					if (!sock.Valid())
						break;
					proto.Decode(in, msgs, uplink->Tokens());
				}
				if (!msgs.empty())
					uplink->Push(msgs);

				MT_FLUSH();
				now = mantra::GetCurrentDateTime();
			}
			while (sock.Valid() && check());

			if (uplink)
			{
				LOG(Info, _("Uplink connection to %1%[%2%] dropped."),
						remote.host % remote.port);
				delete uplink;
				uplink = NULL;
			}
		}
		else
		{
			LOG(Warning, _("Could not establish connection to %1%[%2%]."),
						remote.host % remote.port);
			sock.Close();
		}

		++next_conn;
		MT_FLUSH();
		now = mantra::GetCurrentDateTime();
		expire = now + opt_config["general.server-relink"].as<mantra::duration>();
		while (now < expire && check())
		{
			sleep(1);
			now = mantra::GetCurrentDateTime();
		}
	}

	MT_EE
}
