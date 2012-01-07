/*
 * Copyright (c) 2005-2006 Carnegie Mellon University and Intel Corporation.
 * All rights reserved.
 * See the file "LICENSE" for licensing terms.
 */

#include "parseopt.h"
#include "rxx.h"

bool
parse_config(str file, vec<str> *sp_list, vec<str> *xp_list);

bool 
parse_config_routing_info(str file, vec<str> *routing_list);

bool 
parse_config_sniffer_info(str file, vec<str> *sniffer_unix_socket_list);
