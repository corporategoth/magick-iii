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
#define RCSID(x,y) const char *rcsid_magick__main_cpp_ ## x () { return y; }
RCSID(magick__main_cpp, "@(#)$Id$");

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
#include "message.h"

#include <fstream>

Magick *ROOT = NULL;
mantra::FlowControl *flow = NULL;

static int prelim_check(int prv, const boost::function0<bool> &check,
						const std::vector<std::string> &args);
#ifdef MANTRA_TRACING
static int init_trace(int prv, const boost::function0<bool> &check,
						   const std::vector<std::string> &args);
#endif
static int init_messages(int prv, const boost::function0<bool> &check,
						   const std::vector<std::string> &args);
static int create_instance(int prv, const boost::function0<bool> &check,
						   const std::vector<std::string> &args);
static int start_threads(int prv, const boost::function0<bool> &check,
						 const std::vector<std::string> &args);
static int run(int prv, const boost::function0<bool> &check);
static int stop_threads(int prv, const boost::function0<bool> &check);
static int destroy_instance(int prv, const boost::function0<bool> &check);
static void dopause();
static void doresume();

int main(int argc, const char *argv[])
{
	mantra::FlowControl fc("Magick IRC Services", argc, argv);
	flow = &fc;

	bool background = true;
	for (int i=0; i<argc; ++i)
		if (strcmp(argv[i], "--nofork")==0)
			background = false;

	flow->pushPreforkFunction(prelim_check);
#ifdef MANTRA_TRACING
	flow->pushStartFunction(init_trace);
#endif
	flow->pushStartFunction(init_messages);
	flow->pushStartFunction(create_instance);
	flow->pushStartFunction(start_threads);
	flow->pushRunFunction(run);
	flow->pushStopFunction(stop_threads);
	flow->pushStopFunction(destroy_instance);

	flow->pushPauseFunction(dopause);
	flow->pushResumeFunction(doresume);

	return flow->exec(background);
}

static int prelim_check(int prv, const boost::function0<bool> &check,
						const std::vector<std::string> &args)
{
	int rv = prv;

#ifndef WIN32
	flow->Start();
#endif

	return rv;
}

#ifdef MANTRA_TRACING
class TraceHandler
{
	const char *file_;
	boost::shared_ptr<boost::mutex> lock;

public:
	TraceHandler(const char *f) : file_(f), lock(new boost::mutex) {}

	TraceHandler &operator()(std::deque<std::pair<mantra::TraceType_t, std::string> > &out)
	{
		boost::mutex::scoped_lock scoped_lock(*lock);
		// We don't keep the file open, since it may have been deleted,
		// or we may never trace so why create it.
		std::ofstream file(file_, std::ios::out|std::ios::app);
		while (out.size())
		{
			file << out.front().second << '\n';
			out.pop_front();
		}
		file.flush();
		file.close();
		return *this;
	}
};

static int init_trace(int prv, const boost::function0<bool> &check,
						   const std::vector<std::string> &args)
{
	mantra::mtrace::instance().RegisterType(MAGICK_TRACE_LOST, TraceHandler("trace_LOST.log"));
	mantra::mtrace::instance().RegisterType(MAGICK_TRACE_MAIN, TraceHandler("trace_MAIN.log"));
	mantra::mtrace::instance().RegisterType(MAGICK_TRACE_WORKER, TraceHandler("trace_WORKER.log"));
	mantra::mtrace::instance().RegisterType(MAGICK_TRACE_NICKSERV, TraceHandler("trace_NICKSERV.log"));
	mantra::mtrace::instance().RegisterType(MAGICK_TRACE_CHANSERV, TraceHandler("trace_CHANSERV.log"));
	mantra::mtrace::instance().RegisterType(MAGICK_TRACE_MEMOSERV, TraceHandler("trace_MEMOSERV.log"));
	mantra::mtrace::instance().RegisterType(MAGICK_TRACE_COMMSERV, TraceHandler("trace_COMMSERV.log"));
	mantra::mtrace::instance().RegisterType(MAGICK_TRACE_OPERSERV, TraceHandler("trace_OPERSERV.log"));
	mantra::mtrace::instance().RegisterType(MAGICK_TRACE_OTHER, TraceHandler("trace_OTHER.log"));
	mantra::mtrace::instance().RegisterType(MAGICK_TRACE_DCC, TraceHandler("trace_DCC.log"));
	mantra::mtrace::instance().RegisterType(MAGICK_TRACE_EVENT, TraceHandler("trace_EVENT.log"));
	
	MT_ASSIGN(MAGICK_TRACE_MAIN);
	MT_AUTOFLUSH(true);
	return prv;
}
#endif

static int init_messages(int prv, const boost::function0<bool> &check,
						   const std::vector<std::string> &args)
{
	Message::ResetCommands();
	return prv;
}

static int create_instance(int prv, const boost::function0<bool> &check,
						   const std::vector<std::string> &args)
{
	int rv = prv;

	try
	{
		srand(time(NULL) * getpid());
		textdomain("magick");

		ROOT = new Magick;
		if (!check())
			return rv;
		++rv;

		if (!ROOT->parse_config(args))
			return -rv;
	}
	catch (std::exception &e)
	{
		MT_FLUSH();
		std::cout << "Caught exception " << boost::type_info(typeid(e)).name() << ": " << e.what() << std::endl;
		return -rv;
	}

	++rv;
	return rv;
}

static int start_threads(int prv, const boost::function0<bool> &check,
						 const std::vector<std::string> &args)
{
	int rv = prv;
	NLOG(Info, _("Starting other threads ..."));

	ROOT->event = new mantra::Events;

	++rv;
	return rv;
}

static int run(int prv, const boost::function0<bool> &check)
{
	int rv = prv;

	try
	{
		NLOG(Info, _("Loading databases."));
		ROOT->data.Load();
		NLOG(Info, _("Startup complete."));
		ROOT->run(check);
	}
	catch (mantra::exception &e)
	{
		LOG(Critical, "Uncaught mantra::exception (%1% at %2%:%3%) caught: %4%",
						typeid(e).name() % e.file() % e.line() % e.what());
	}
	catch (std::exception &e)
	{
		LOG(Critical, "Uncaught std::exception (%1%) caught: %2%",
						typeid(e).name() % e.what());
	}
	catch (std::string &e)
	{
		LOG(Critical, "Uncaught std::string exception caught: %1%",
						e.c_str());
	}
	catch (const char *e)
	{
		LOG(Critical, "Uncaught string exception caught: %1%", e);
	}
	catch (int e)
	{
		LOG(Critical, "Uncaught integer exception caught: %1%", e);
	}
	catch (...)
	{
		NLOG(Critical, "Uncaught unknown exception caught.");
	}

	MT_FLUSH();
	NLOG(Info, _("Shutting Down."));

	return rv;
}

static int stop_threads(int prv, const boost::function0<bool> &check)
{
	int rv = prv;

	NLOG(Info, _("Stopping other threads ..."));
	if (ROOT->event)
		delete ROOT->event;

	--rv;
	return rv;
}

static int destroy_instance(int prv, const boost::function0<bool> &check)
{
	int rv = prv;

	NLOG(Info, _("Shutdown complete."));
	delete ROOT;
	ROOT = NULL;
	rv -= 2;

	return rv;
}

static void dopause()
{
}

static void doresume()
{
}
