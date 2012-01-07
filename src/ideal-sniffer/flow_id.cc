#include <map>
#include <vector>
#include "flow_id.h"

// Call using FlowId fid = new FlowId(inet_ntoa(ip->ip_src), inet_ntoa(ip->ip_dst), inet_ntoa(ip->ip_src), inet_ntoa(ip->ip_dst));

FlowId::FlowId(const char *sip, const char *dip, unsigned int sp, unsigned int dp) { 
    srcIp = (char *)malloc(sizeof(char) * (strlen(sip) + 1));
    dstIp = (char *)malloc(sizeof(char) * (strlen(dip) + 1));

    strcpy(srcIp, sip);
    strcpy(dstIp, dip);

    srcPort = sp;
    dstPort = dp;
}

FlowId::FlowId(const FlowId& fid) { 

    srcIp = (char *)malloc(sizeof(char) * (strlen(fid.srcIp) + 1));
    dstIp = (char *)malloc(sizeof(char) * (strlen(fid.dstIp) + 1));

    strcpy(srcIp, fid.srcIp);
    strcpy(dstIp, fid.dstIp);

    srcPort = fid.srcPort;
    dstPort = fid.dstPort;
}

FlowId::~FlowId() { 
    //cout << "d'tor called" << endl;
    if (srcIp != NULL) {
        delete srcIp; 
    }
    if (dstIp != NULL) {
        delete dstIp; 
    }
}

bool FlowId::operator==(FlowId fid2) {
    bool retVal = false;
    if ( (strcmp(this->srcIp, fid2.srcIp) == 0) && (strcmp(this->dstIp, fid2.dstIp) == 0)
         && (this->srcPort == fid2.srcPort) && (this->dstPort == fid2.dstPort) ) {
        retVal = true;
    }
    return retVal;
}

bool FlowId::operator<(const FlowId& fid2) {
    bool retVal = false;
    if ( (this->srcPort + this->dstPort) < (fid2.srcPort + fid2.dstPort) ) {
        retVal = true;
    }
    return retVal;
}

void FlowId::print() {
    cout << this->srcIp << ":" << this->srcPort << " => " << this->dstIp << ":" << this->dstPort << endl;
}

string* FlowId::toString() {
    string* ps = new string();

    char buffer[32];

    ps->append(this->srcIp);
    ps->append(":");
    sprintf(buffer, "%d", this->srcPort);
    ps->append(buffer);
    ps->append(" => ");
    ps->append(this->dstIp);
    ps->append(":");
    sprintf(buffer, "%d", this->dstPort);
    ps->append(buffer);

    return ps;
}


/*
struct FlowIdCmp {
    bool operator()(FlowId* f1, FlowId* f2) const {
        if (*f1 < *f2) {
            return true;
        } else {
            return false;
        }
    }
};

int main(void) {

    string s1 = "1.1.1.1";
    string s2 = "2.2.2.2";

    FlowId *fid1 = new FlowId(s1.c_str(), s2.c_str(), 11, 22);
    FlowId *fid2 = new FlowId(s1.c_str(), s2.c_str(), 11, 23);

    if (*fid1==*fid2) {
        cout << "yoohoo" << endl;
    } else {
        cout << "nada" << endl;
    }

    vector< pair< FlowId*, vector<string*> > > flow_list;

    vector<string*> v;
    string* s = new string("a");
    v.push_back(s);
    pair< FlowId*, vector<string*> >* p = new pair< FlowId*, vector<string*> >(fid1, v);
    flow_list.push_back(*p);

    for( unsigned int i = 0; i < flow_list.size(); i++ ) {
        if ( *(flow_list[i].first) ==  *fid1 ) {
            cout << "yay!" << endl;
        }
        else {
            cout << "boo!" << endl;
        }
    }

    map<FlowId*, string*, FlowIdCmp> flow_map;
    flow_map[fid1] = new string("hola!");
    if (NULL == flow_map[fid2]) {
        flow_map[fid2] = new string("hola again!");
    }

    map<FlowId*, string*>::iterator mi;
    
    cout << "fid1 => " << *(flow_map[fid1]) << endl;
    cout << "fid2 => " << *(flow_map[fid2]) << endl;

    delete fid1;
    delete fid2;

    return 0;
}
*/
