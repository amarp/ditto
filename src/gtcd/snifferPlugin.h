#ifndef _PLUGIN_SNIFFER_H_
#define _PLUGIN_SNIFFER_H_

#include "gtc_prot.h"
#include "amisc.h"
#include "async.h"
#include "arpc.h"

class gtcd_main;

class snifferPlugin {

public:
    //virtual bool configure (str s) = 0;
    virtual ~snifferPlugin () {}
  
};

#endif /* _PLUGIN_SNIFFER_H_ */

