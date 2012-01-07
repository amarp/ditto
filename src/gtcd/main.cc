/*
 * Copyright (c) 2005-2006 Carnegie Mellon University and Intel Corporation.
 * All rights reserved.
 * See the file "LICENSE" for licensing terms.
 */

#include "gtcd.h"
#include "chunker/chunkerPlugin_all.h"
#include "xfer/xferPlugin_all.h"
#include "storage/storagePlugin_all.h"
#include "configfile.h"
#include "sniffer/snifferPlugin_tcp.h"

static str gtcd_listen_sock("/tmp/gtcd.sock");
gtcd_main *m;
qhash<str, sPluginNew_cb> sPluginTab;
qhash<str, xPluginNew_cb> xPluginTab;

static void
accept_connection(int fd)
{
    struct sockaddr_un sun;
    socklen_t sunlen = sizeof(sun);
    bzero(&sun, sizeof(sun));

    int cs = accept(fd, (struct sockaddr *) &sun, &sunlen);
    if (fd < 0) {
        if (errno != EAGAIN)
            warn << "accept; errno = " << errno << "\n";
        return;
    }

    vNew gtcd(cs, sun, m);
}

static void
gtcd_start(str gtcd_listen_sock)
{
    mode_t m = umask(0);

    int fd = unixsocket(gtcd_listen_sock);
    if (fd < 0 && errno == EADDRINUSE) {
        /* XXX - This is a slightly race-prone way of cleaning up after a
         * server bails without unlinking the socket.  If we can't connect
         * to the socket, it's dead and should be unlinked and rebound.
         * Two daemons could do this simultaneously, however. */
        int xfd = unixsocket_connect(gtcd_listen_sock);
        if (xfd < 0) {
            unlink(gtcd_listen_sock);
            fd = unixsocket(gtcd_listen_sock);
        }
        else {
            warn << "closing the socket \n";
            close (xfd);
            errno = EADDRINUSE;
        }
    }
    if (fd < 0)
        fatal ("%s: %m\n", gtcd_listen_sock.cstr ());

    close_on_exec(fd);
    make_async(fd);

    umask(m);

    listen(fd, 150);

    /* XXX: Daemonize */

    warn << progname << " (DOT Generic Transfer Client) version "
        << VERSION << ", pid " << getpid() << "\n";

    fdcb(fd, selread, wrap(accept_connection, fd));
}

static void
print_splugin(const str &s, sPluginNew_cb *cb)
{
    warnx << s << " ";
}

static void
print_xplugin(const str &s, xPluginNew_cb *cb)
{
    warnx << s << " ";
}

static void
print_plugins()
{
    warnx << "Available Storage Plugins: ";
    sPluginTab.traverse(wrap(print_splugin));
    warnx << "\n";

    warnx << "Available Transfer Plugins: ";
    xPluginTab.traverse(wrap(print_xplugin));
    warnx << "\n";
}

static void
usage()
{
    fprintf(stderr, "usage:  gtcd [-h] [-f configfile] [-p gtcd_listen_sock]\n");
}

static void
help()
{
    usage();
    fprintf(stderr,
	    "    -h .......... help (this message)\n"
	    "    -f file ..... configuration file\n"
	    "    -p listen ... socket on which to listen (def /tmp/gtcd.soc)\n"
	    "    -d .......... debug / do not daemonize\n"
	    "\n");
    print_plugins();
}

static void
instantiate_plugins(str configfile)
{
    storagePlugin *sp = NULL;
    xferPlugin *xp = NULL;
    vec<xferPlugin*>  xpvec;
        
    vec<str> sp_list;
    vec<str> xp_list;

    qhash<str, storagePlugin *> sPluginPtr;
    qhash<str, xferPlugin *> xPluginPtr;

    if (parse_config(configfile, &sp_list, &xp_list))
        fatal << "Cannot parse config file\n";

    /* instantiate storage plugin chain */
    while (sp_list.size()) {
        str conf = sp_list.pop_back();
	str plg_list = sp_list.pop_back();
	str plugin = sp_list.pop_back();
	
        warn << "Adding Storage Plugin: " << plugin << " chained to " <<
	     plg_list << " with params " << conf << "\n";
	
        if (!sPluginTab[plugin]) {
            warn("Storage plugin (%s) does not exist\n", plugin.cstr());
            print_plugins();
            exit(-1);
        }

	if (plg_list == "" ||
	    plg_list == "null") {
	    warn << "Passing NULL\n";
	    sp = NULL;
	}
	else {
	    sp = (*sPluginPtr[plg_list]);
	    if (!sp) 
		fatal << "Storage plugin " << plg_list << " not instantiated\n";
	    else
		warn << "passing " << plg_list << "\n";
	}
	
        sp = (*sPluginTab[plugin])(m, sp);

	sPluginPtr.insert(plugin, sp);
	
        if (!sp->configure(conf))
            fatal("Storage plugin (%s) configuration failed\n", plugin.cstr());
    }
    
    m->set_storagePlugin(sp);

    /* instantiate xfer plugin chain */
    while (xp_list.size()) {
        str conf = xp_list.pop_back();
	str plg_list = xp_list.pop_back();
        str plugin = xp_list.pop_back();
	
        warn << "Adding Transfer Plugin: " << plugin << " chained to " <<
	    plg_list << " with params " << conf << "\n";
	
        if (!xPluginTab[plugin]) {
            warn("Transfer plugin (%s) does not exist\n", plugin.cstr());
            print_plugins();
            exit(-1);
        }

	if (plg_list == "" ||
	    plg_list == "null") {
	    warn << "Passing NULL\n";
	    xp = NULL;
	}
	else {
	    if (strchr(plg_list.cstr(), ',')) {
		char *name = strdup(plg_list.cstr());
		char *temp = strtok(name,",");
		
		while (temp != NULL) {
		    str xfer_plg(temp);
		    		    
		    xp = (*xPluginPtr[xfer_plg]);
		    if (!xp) 
			fatal << "Transfer plugin " << xfer_plg << " not instantiated\n";
		    else
			warn << "passing " << xfer_plg << "\n";

		    xpvec.push_back(xp);
		    temp = strtok(NULL,",");
		}
	    }
	    else {
		//just one plugin, not a vec
		xp = (*xPluginPtr[plg_list]);
		if (!xp) 
		    fatal << "Transfer plugin " << plg_list << " not instantiated\n";
		else
		    warn << "passing " << plg_list << "\n";
	    }
	}    
	if (xpvec.size()) {
	    xp = NULL;
	    xp = (*xPluginTab[plugin])(m, xp);
	    xp->set_more_plugins(xpvec);
	}
	else {
	    xp = (*xPluginTab[plugin])(m, xp);
	}
	
	xPluginPtr.insert(plugin, xp);
	
	if (!xp->configure(conf))
	    fatal("Transfer plugin (%s) configuration failed\n", plugin.cstr());
	
    } //while
    
    m->set_xferPlugin(xp);
    
    m->set_chunkerPlugin(New chunkerPlugin_default(m->sp));

    m->set_snifferPlugin(New snifferPlugin_tcp(m, NULL));
    //m->snp->configure();
}

int
main(int argc, char * argv[])
{
    storagePlugin_maker _spm; // populate sPluginTab
    xferPlugin_maker _xpm;    // populate xPluginTab

    char ch;
    bool daemonize = true;
    str configfile;
    bool b_no_dot_proxy = false;
    str first_octet_mask = "2";

    setprogname(argv[0]);

    while ((ch = getopt(argc, argv, "ndhp:f:m:")) != -1)
        switch(ch) {
	case 'd':
	    warn << "Daemonize option not yet implemented\n";
	    daemonize = false;
	    break;
	case 'h':
	    help();
	    exit(0);
        case 'n':
            b_no_dot_proxy = true;
            break;
        case 'p':
            gtcd_listen_sock = optarg;
            break;
        case 'f':
            configfile = optarg;
            break;
        case 'm':
            first_octet_mask = optarg;
            break;
	default:
	    usage();
	    exit(-1);
	}

    gtcd_start(gtcd_listen_sock);

    /* Configure the graph of plugins */
    m = New gtcd_main(configfile);
    m->no_dot_proxy = b_no_dot_proxy;
    m->first_octet_mask = first_octet_mask;


    instantiate_plugins(configfile);
        
    //m->set_chunkerPlugin(New chunkerPlugin_generate());

    // -- code for multipath plugin
    // vec<xferPlugin*>  xplist;
    // xplist.push_back(New xferPlugin_gtc(m,port));
    // xplist.push_back(New xferPlugin_gtc(m,port+1));
    //
    // m->set_xferPlugin_portable(New xferPlugin_portable(m));
    //
    // xferPlugin_mpath *mp = New xferPlugin_mpath(m, xplist);
    // m->set_xferPlugin(mp);
    // --

    amain();
}
