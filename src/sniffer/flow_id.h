#ifndef _FLOW_ID
#define _FLOW_ID 1

#include <iostream>
#include <stdio.h>
#include <sstream>
#include <string>

using namespace std;

class FlowId {
protected:
    char *srcIp, *dstIp;
    unsigned int srcPort, dstPort;

public:
    FlowId(const char *sip, const char *dip, unsigned int sp, unsigned int dp);
    FlowId(const FlowId& fid);
    virtual ~FlowId();
    virtual bool operator==(FlowId);
    virtual bool operator<(const FlowId&);
    virtual void print();
    virtual string* toString();
};

#endif //_FLOW_ID
