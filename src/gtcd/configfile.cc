/*
 * Copyright (c) 2005-2006 Carnegie Mellon University and Intel Corporation.
 * All rights reserved.
 * See the file "LICENSE" for licensing terms.
 */

#include "configfile.h"

enum config_state_t { CONFIG_NONE, CONFIG_STORAGE, CONFIG_XFER, CONFIG_ROUTING, CONFIG_SNIFFER };

bool
parse_config(str file, vec<str> *sp_list, vec<str> *xp_list)
{
    assert(sp_list);
    assert(xp_list); 

    if (!file) {
        warn << "No config file specified!\n";
        return true;
    }

    parseargs pa(file);
    int line;
    vec<str> av;

    enum config_state_t state = CONFIG_NONE;
    bool saw_storage = false;
    bool saw_xfer = false;
    bool saw_routing = false;
    bool saw_sniffer = false;
    bool errors = false;

    while (pa.getline(&av, &line)) {
        if (!strcasecmp(av[0], "[storage]")) {
            if (saw_storage) {
                errors = true;
                warn << file << ":" << line << ": Duplicate [storage] section\n";
            }
            else {
                state = CONFIG_STORAGE;
                saw_storage = true;
            }
        }
        else if (!strcasecmp(av[0], "[transfer]")) {
            if (saw_xfer) {
                errors = true;
                warn << file << ":" << line << ": Duplicate [transfer] section\n";
            }
            else {
                state = CONFIG_XFER;
                saw_xfer = true;
            }
        }
        else if (!strcasecmp(av[0], "[routing]")) {
            if (saw_routing) {
                errors = true;
                warn << file << ":" << line << ": Duplicate [routing] section\n";
            }
            else {
                state = CONFIG_ROUTING;
                saw_routing = true;
            }
        }
        else if (!strcasecmp(av[0], "[sniffer]")) {
            if (saw_sniffer) {
                errors = true;
                warn << file << ":" << line << ": Duplicate [sniffer] section\n";
            }
            else {
                state = CONFIG_SNIFFER;
                saw_sniffer = true;
            }
        }
        else {
            if (state == CONFIG_NONE) {
                errors = true;
                warn << file << ":" << line << ": Not inside a plugin section\n";
            }
            else if (state == CONFIG_STORAGE) {
                sp_list->push_back(av.pop_front());
                if (av.size()) {
		    //for plg_list
		    sp_list->push_back(av.pop_front());
		    //for conf
                    sp_list->push_back(join(" ", av));
		}
                else {
		    //for plg_list and conf
                    sp_list->push_back("");
		    sp_list->push_back("");
		}
            }
            else if (state == CONFIG_XFER) {
                xp_list->push_back(av.pop_front());
                if (av.size()) {
		    //for plg_list
		    xp_list->push_back(av.pop_front());
		    //for conf
                    xp_list->push_back(join(" ", av));
		}
                else {
		    //for plg_list and conf
		    xp_list->push_back("");
		    xp_list->push_back("");
		}
            }
            else if (state == CONFIG_ROUTING) {
	      // do nothing
	    }
	}
    }

    if (!saw_storage) {
        warn << file << ": Missing [storage] section\n";
        errors = true;
    }
    if (!saw_xfer) {
        warn << file << ": Missing [transfer] section\n";
        errors = true;
    }

    if (errors)
        warn << "Errors processing file: " << file << "\n";

    return errors;
}

//#define USE_LOOPBACK_ROUTING
bool 
parse_config_routing_info (str file, vec<str> *routing_list)
{
  assert (routing_list);

  if (!file) {
    warn << "No config file specified \n";
    return true;
  }
  parseargs pa(file);
  int line;
  vec<str> av;
  
  enum config_state_t state = CONFIG_NONE;
  bool saw_routing = false;
  bool errors = false;
#ifdef USE_LOOPBACK_ROUTING
  char *routing_info_header = "[routing]";
#else 
  char *routing_info_header = "Destination";
#endif 
  while (pa.getline(&av, &line)) {
    if (!strcasecmp(av[0], routing_info_header)) {
      if (saw_routing) {
	errors = true;
	warn << file << ":" << line << ": Duplicate routing information \n";
      }
      else {
	state = CONFIG_ROUTING;
	saw_routing = true;
      }
    }
    else {
      if (state == CONFIG_ROUTING) {
	while (av.size ())
	  routing_list->push_back(av.pop_front());
      }
    } 
  }
  if (!saw_routing) {
    warn << file << ": Missing routing information \n";
    errors = true;
  }
  if (errors) 
    warn << "Errors processing file :" << file << "\n";
  
  return errors;
}

bool 
parse_config_sniffer_info (str file, vec<str> *sniffer_unix_socket_list)
{
  assert (sniffer_unix_socket_list);

  if (!file) {
    warn << "No config file specified \n";
    return true;
  }
  parseargs pa(file);
  int line;
  vec<str> av;
  
  enum config_state_t state = CONFIG_NONE;
  bool saw_sniffer = false;
  bool errors = false;

  while (pa.getline(&av, &line)) {
    if (!strcasecmp(av[0], "[sniffer]")) {
      if (saw_sniffer) {
	errors = true;
	warn << file << ":" << line << ": Duplicate [sniffer] section\n";
      }
      else {
	state = CONFIG_SNIFFER;
	saw_sniffer = true;
      }
    }
    else { 
      if (state == CONFIG_SNIFFER) {
	sniffer_unix_socket_list->push_back(av.pop_front());
	sniffer_unix_socket_list->push_back(av.pop_front());
        break;
      }
    }
  }
  
  if (!saw_sniffer) {
    warn << file << ": Missing [sniffer] section\n";
    errors = true;
  }
  if (errors)
    warn << "Errors processing file: " << file << "\n";
  
  return errors;
}
