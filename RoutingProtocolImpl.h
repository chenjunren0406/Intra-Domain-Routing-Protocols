#ifndef ROUTINGPROTOCOLIMPL_H
#define ROUTINGPROTOCOLIMPL_H

#include "RoutingProtocol.h"
#include <queue>
#include <map>
#include <string.h>
#include <netinet/in.h>
#include <vector>
#include <stdio.h>
#include "Node.h"
#include "global.h"

class RoutingProtocolImpl : public RoutingProtocol {
  public:
    RoutingProtocolImpl(Node *n);
    ~RoutingProtocolImpl();

    /*
     record in forwarding table
     */
    struct Forwarding_record{
        unsigned short cost;
        unsigned short nextHop;
        bool direct;
        Forwarding_record() {};
        Forwarding_record(unsigned short cost, unsigned nextHop, bool direct) : cost(cost),nextHop(nextHop),direct(direct){};
    };
    /*
     record in port status
     */
    struct Port_record{
        unsigned short neiNodeID;
        unsigned short costRRT;
        Port_record(){};
        Port_record(unsigned short neiNodeID, unsigned short costRRT) : neiNodeID(neiNodeID), costRRT(costRRT){};
    };
    /*
     the inside package which is convinent to be handle in our system
     */
    struct package{
        unsigned short type;
        unsigned short size;
        unsigned short srcid;
        unsigned short dstid;
        int data;
        void *table;
        package(){};
        package(unsigned short type, unsigned short size, unsigned short srcid, unsigned short dstid, int data, void *table):type(type),size(size),srcid(srcid),dstid(dstid),data(data),table(table){};
    };
    
    struct LS{
        char type;
        char reserved;
        unsigned short size;
        unsigned short srcid;
        unsigned short dstid;
        int sequenceNum;
        map<unsigned short, unsigned short> contents;
        LS(){};
        LS(char type, char reserved, unsigned short size, unsigned short srcid,unsigned short dstid, int sequenceNum, map<unsigned short, unsigned short> contents):type(type),reserved(reserved), size(size),srcid(srcid),dstid(dstid),sequenceNum(sequenceNum),contents(contents){};
    };
    
    struct DV{
        char type;
        char reserved;
        unsigned short size;
        unsigned short srcid;
        unsigned short dstid;
        map<unsigned short, unsigned short> contents;
        DV(){};
        DV(char type, char reserved, unsigned short size, unsigned short srcid,unsigned short dstid, map<unsigned short, unsigned short> contents):type(type),reserved(reserved), size(size),srcid(srcid),dstid(dstid),contents(contents){};
    };
    
    struct Vertice{
        int sequence_number;
        unsigned short NodeID;
        map<unsigned short, unsigned short> neighbours;
        Vertice(){};
        Vertice(int sequence_number, unsigned short NodeId) : sequence_number(sequence_number), NodeID(NodeID){};
    };
    
    /*
     this is a hash map to save the Forwarding Record.
     */
    map<unsigned short, Forwarding_record> forwarding_table;
    /*
     this is a hash map to store the record of port status(what is direck link node, cost of direct link node, and you can access with this node from which port at current node )
     */
    map<unsigned short,Port_record> port_status;
    /*
     this is a hash map to store the whole topology of graph
     */
    map<unsigned short, Vertice> nodeVec;
    
    void init(unsigned short num_ports, unsigned short router_id, eProtocolType protocol_type);
    // As discussed in the assignment document, your RoutingProtocolImpl is
    // first initialized with the total number of ports on the router,
    // the router's ID, and the protocol type (P_DV or P_LS) that
    // should be used. See global.h for definitions of constants P_DV
    // and P_LS.

    void handle_alarm(void *data);
    // As discussed in the assignment document, when an alarm scheduled by your
    // RoutingProtoclImpl fires, your RoutingProtocolImpl's
    // handle_alarm() function will be called, with the original piece
    // of "data" memory supplied to set_alarm() provided. After you
    // handle an alarm, the memory pointed to by "data" is under your
    // ownership and you should free it if appropriate.

    void recv(unsigned short port, void *packet, unsigned short size);
    // When a packet is received, your recv() function will be called
    // with the port number on which the packet arrives from, the
    // pointer to the packet memory, and the size of the packet in
    // bytes. When you receive a packet, the packet memory is under
    // your ownership and you should free it if appropriate. When a
    // DATA packet is created at a router by the simulator, your
    // recv() function will be called for such DATA packet, but with a
    // special port number of SPECIAL_PORT (see global.h) to indicate
    // that the packet is generated locally and not received from
    // a neighbor router.



    //our own functions
    void init_forwarding_table();
    
    vector<Node *> getNeibor(Node * n);
    
    void update_forwarding_table(unsigned short dest, unsigned short cost);
    
    void mergeRecord(Forwarding_record *p, Forwarding_record *current, unsigned short directcost,unsigned short nodeid);
    
    void updateForwardingTable(map<unsigned short,Forwarding_record> table,unsigned short directcost, unsigned short nodeid, unsigned short localid);
    
    void checkDirectLink();
    
    unsigned short costfromPortStuats(unsigned short nodeid);
    
    unsigned short costfromForwardingTable(unsigned short nodeid);
    
    unsigned short getNeiboridFromPortNo(unsigned short portNo);
    
    void sendPacketInEveryPort(package *pck);
    
    void printForwardingTable();
    
    void sendDVInEveryPort();
    
    void checkPSChange_DV(unsigned short scrid ,unsigned short costOfdirectLink,unsigned short diff);
    
    void remove_record_ForwardingTable(unsigned short nexthop);
    
    void remove_record_PortStatus(unsigned short neiNodeID);
    
    unsigned short getPortnumFromPortStatus(unsigned short nodeid);
    
    void printPortStatus();
    
    /*
     get LS information from transimtion package
     */
    DV getDV(char *p);
    
    map<unsigned short, unsigned short>getMapFromPacket(char* packet, unsigned short mapSize);
    
    void printMap(map<unsigned short, unsigned short> mymap);
    
    char *getPacket(char type, unsigned short srcID, unsigned short destID);
    
    unsigned short calculateSize(char type);
    
    package *getOriginalDVpacket(DV p);
    //
    void printLSPacket(LS packet);
    
    /*
     get LS information from transimtion package
     */
    LS getLS(char* p);
    
    void print_PortStatus(map<unsigned short,Port_record> status);
    
    map<unsigned short,unsigned short> convert_Port_status();
    
     void print_nodeVec(map<unsigned short, Vertice> vec);
    
     unsigned short findMin(map<unsigned short, unsigned short> mymap);
    
     void Dijsktra( map<unsigned short,Port_record> Myport, map<unsigned short, Vertice> mynodeVec);
    
    void sendLSInEveryPort();
 private:
    Node *sys; // To store Node object; used to access GSR9999 interfaces
    
    map<unsigned short,int>lastTimeOfPong;//key: nodeid ; value: last time of update
    
    map<unsigned short, int>lastTimeOfDVLS;// key: nodeid; value: last time of update
    
    bool dv_forwardingTableChange;
    
    int data[3];
    
    int sequenceNum;
    
    int protocol;//0:DV 1:LS
    

};

#endif

