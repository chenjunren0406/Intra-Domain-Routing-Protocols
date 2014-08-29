  #include "RoutingProtocolImpl.h"

  using namespace std;

  RoutingProtocolImpl::RoutingProtocolImpl(Node *n) : RoutingProtocol(n) {

   sys = n;

    dv_forwardingTableChange = false;
  
     //init set_alarm
     for(int i=0;i<3;i++){
         data[i]=i;
     }
     void *sendData1=&data[0];
     sys->set_alarm(this, 1000, sendData1);
     void *sendData2=&data[1];
     sys->set_alarm(this, 10000, sendData2);
     void *sendData3=&data[2];
     sys->set_alarm(this,30000, sendData3);
     
  }

  RoutingProtocolImpl::~RoutingProtocolImpl() {
   // add your own code (if needed)
  }

  void RoutingProtocolImpl::init(unsigned short num_ports, unsigned short router_id, eProtocolType protocol_type) {
      
  // if protocol = 0, DV; protocol = 1,LS;
      protocol = protocol_type;
      
      cout<<protocol <<" ............."<<endl;
      
      sequenceNum = 0;
      
      char* px;
     
      unsigned short size;
      
      for(int i=0; i<sys->link_vector.size();i++){
          unsigned short portNo = sys->get_link_port(sys->link_vector[i]);
          px=getPacket(1, router_id, 0);
          size=htons(*((unsigned short*)(px+sizeof(unsigned short))));
          sys->send(portNo, px, sizeof(unsigned short)*size);
          //cout<<"send ping from "<<sys->id<<" to port "<<i<<endl;
      }
  }

  void RoutingProtocolImpl::handle_alarm(void *data) {
      
      // alarm for 1 sec
     if(*(int *)data==0){
         
         /*
          there are 4 kinds of things we need to handle in 1 sec:
          1.link coming up
          2.link die
          3.time out of pong
          4.time out of DV/LS
          */
         //cout<<"refresh port status"<<endl;
         
         /*check every port
           see is there a link die or link coming up.
          */
         
         bool timeOutHappenOrlinkchange = false;
         char *px;
         for(int i=0; i<sys->link_vector.size();i++){
             //link dying and there is a record in port status; then romove it from port status
             unsigned short portNo = sys->get_link_port(sys->link_vector[i]);
             unsigned short Neighborid = getNeiboridFromPortNo(portNo);
             
             if(!(sys->link_vector[i])->get_is_alive() && port_status.find(portNo) != port_status.end()){
                 
                 remove_record_PortStatus(Neighborid);
                 remove_record_ForwardingTable(Neighborid);
                 timeOutHappenOrlinkchange = true;
             }
     
             else if((sys->link_vector[i])->get_is_alive() && port_status.find(portNo) == port_status.end()){ //link comingup and there is no record in port_status, then send a ping to get RTT and insert it
                   //Also this may change the forwarding_table in the field of direct link
                 if(forwarding_table.find(Neighborid) != forwarding_table.end()){
                     forwarding_table.find(Neighborid)->second.direct = true;
                 }
                 if(port_status.find(portNo) == port_status.end()){
                     px = getPacket(1,sys->id,0);
                     unsigned short size=htons(*((unsigned short*)(px+sizeof(unsigned short))));
                     sys->send(portNo,px,sizeof(unsigned short)*size);
                 }
                 
             }
             
         }// end of link dying and link coming up
         
         // check whether have some time out
         
         //time out of pong
         map<unsigned short,int>::iterator it = lastTimeOfPong.begin();
         while(it != lastTimeOfPong.end()){
             if((sys->time()) >= (it->second) + 15000){
                 //time out
                 
                 cout<<sys->time()<<" "<<it->second;
                 remove_record_PortStatus(it->first);
                 remove_record_ForwardingTable(it->first);
                 lastTimeOfPong.erase(it++);
                 timeOutHappenOrlinkchange = true;
                 cout<<"$$$$"<<endl;
                 continue;
             }
             it++;
         }
         
         //time out of DV/LS
         map<unsigned short,int>::iterator its = lastTimeOfDVLS.begin();
         while((its != lastTimeOfDVLS.end())){
             if(sys->time() >= its->second + 45000){
                 //time out
                 if(protocol == 0){
                     remove_record_ForwardingTable(its->first);
                     lastTimeOfDVLS.erase(its++);
                     timeOutHappenOrlinkchange = true;
                     continue;
                 }
                 else if(protocol == 1){
                     remove_record_ForwardingTable(its->first);
                     nodeVec.erase(its->first);
                     lastTimeOfDVLS.erase(its++);
                     timeOutHappenOrlinkchange = true;
                     continue;
                 }
             }
             its++;
         }
         
         if(timeOutHappenOrlinkchange){
             if(protocol == 0){
                 sendDVInEveryPort();
             }
             else if(protocol == 1){
                 sendLSInEveryPort();
             }
             timeOutHappenOrlinkchange = false;
         }
         
         printPortStatus();
         sys->set_alarm(this, 1000, data);
     }
      
      // alarm for 10 sec
     if(*(int *)data==1){
         //cout<<"send Ping "<<"dangqian "<< sys->id <<endl;
         char *pxx;
         for(int i=0; i<sys->link_vector.size();i++){
             if((sys->link_vector[i])->get_is_alive()){
                 
                 pxx = getPacket(1,sys->id,0);
                 unsigned short size=htons(*((unsigned short*)(pxx+sizeof(unsigned short))));
                 sys->send(i,pxx,sizeof(unsigned short)*size);
                 
             }
             
         }
         sys->set_alarm(this, 10000, data);
     }
      // alarm for 30 sec
     if(*(int *)data==2){
         //cout<<"send DV"<<endl;
         if(protocol == 0){
             sendDVInEveryPort();
         }
         else if(protocol == 1){
             sendLSInEveryPort();
         }
         sys->set_alarm(this, 30000, data);
     }
     
  }

  void RoutingProtocolImpl::recv(unsigned short port, void *packet, unsigned short size) {
      char *p = (char *)packet;
      
      unsigned char type;
      
      type = *((unsigned char*)p);
      
      unsigned short scrid = htons(*((short*)(p+2*sizeof(short))));
                                   
      unsigned short destID = htons(*((unsigned short*)(p+sizeof(unsigned short)*2)));
                                

     //package *p = (package *)packet;
      
      
     if(type == 0){
         //data
         
         char* newP=(char*)malloc(sizeof(unsigned short) * calculateSize(type));
         memcpy(newP, p, sizeof(unsigned short)*calculateSize(type));
         free(p);
         
         if(destID!=sys->id){
             for(map<unsigned short, Forwarding_record>::iterator it = forwarding_table.begin();it != forwarding_table.end(); ++it){
                 if(destID == it->first){
                     for(map<unsigned short, Port_record>::iterator its=port_status.begin(); its!=port_status.end();++its){
                         if(it->second.nextHop == its->second.neiNodeID){
                             //cout<<"the nextHop="<<it->second.nextHop<<endl;
                             //                             cout<<"sending data from "<<sys->id<<" to "<<its->second.neiNodeID<<endl;
                             sys->send(its->first, newP, sizeof(unsigned short)*calculateSize(type));
                             break;
                         }
                     }
                     break;
                 }
             }
         }
         
         
     }
     else if(type == 1){
         //ping

         unsigned char changedT=2;
         *((unsigned char*)p)=changedT;
         *((unsigned short*)(p+sizeof(unsigned short)*3))=htons(destID);
         *((unsigned short*)(p+sizeof(unsigned short)*2))=htons(sys->id);
         unsigned short size;
         size=htons(*((unsigned short*)(p+sizeof(unsigned short))));
         sys->send(port, p, sizeof(unsigned short)*size);
     }
     else if(type == 2){
//         //pong
//         cout<<"recv Pong"<<endl;
//         cout<<"when I get Pong, p size="<<htons(*((unsigned short *)(p+sizeof(unsigned short))))<<endl;
//         cout<<"sys->id="<<sys->id<<endl;
         bool psChange=false;
         unsigned short desc = 0;
         int pTime=htons(*((int *)(p+sizeof(unsigned short)*4)));
         unsigned short cost=sys->time()-pTime;
         if(port_status.find(port)==port_status.end()){
             unsigned short neiNodeID=htons(*(unsigned short*)(p+2*sizeof(unsigned short)));
//             cout<<"neiNodeID="<<neiNodeID<<endl;
             Port_record record=Port_record(neiNodeID, cost);
             port_status.insert(pair<unsigned short, Port_record>(port, record));
             psChange=true;
         }
         else{
             if(port_status.find(port)->second.costRRT!=cost){
                 //cost is changed,reset the value of this record in port_status
                 desc =cost - (port_status.find(port)->second.costRRT);
                 port_status.find(port)->second.costRRT = cost;
                 psChange = true;
             }
         }
         // if port_status change, then check whether the forwarding_table should be changed; if there is no change in
         // port_status, then do nothing
         
       
         if(psChange && protocol == 0){
             //
             checkPSChange_DV(scrid,cost,desc);
             
             psChange = false;
         }
         if(psChange && protocol==1){
             
             sendLSInEveryPort();
             psChange=false;
         }
         cout<<"finish pong"<<endl;
         //
         lastTimeOfPong[scrid] = sys->time();
         printPortStatus();
     }
     else if(type == 3){
         //DV
         DV dd = getDV(p);
         
        package *ppp= getOriginalDVpacket(dd);
         
         unsigned short cost = costfromForwardingTable(scrid);
    
         map<unsigned short,Forwarding_record> recvtable = *((map<unsigned short,Forwarding_record> *)(ppp->table));
       
         for(map<unsigned short, Forwarding_record>::iterator it = recvtable.begin();it != recvtable.end(); ++it)
         {
             std::cout << "recv forwarding table before process" <<endl<<endl<<"senderid :"<< ppp->srcid <<" dest : "<<ppp->dstid<<"current nodeid: "<<sys->id<<" Dest ID="<<it->first << ", NextHop " << it->second.nextHop <<", Cost="<<it->second.cost<<", Direct="<<it->second.direct<<"\n";
         }
         
         updateForwardingTable(recvtable,cost,scrid,sys->id);
         
         if(dv_forwardingTableChange){// see if there is any change in forwardingtable, send dv table to every port
             sendDVInEveryPort();
             dv_forwardingTableChange = false;
         }
         
         //update last time of DV
         lastTimeOfDVLS[scrid] = sys->time();
         printForwardingTable();
         delete[] p;
     }
     else if(type == 4){
         //LS
         
         cout<<"recv LS"<<endl;
         print_PortStatus(port_status);
         LS packetLS=getLS(p);
         //delete[] p;
         //printLSPacket(packetLS);
         
         int LS_change=0;
         if(!nodeVec.count(packetLS.srcid)){
             Vertice v=Vertice();
             v.sequence_number=packetLS.sequenceNum;
             v.neighbours=packetLS.contents;
             v.NodeID=packetLS.srcid;
             nodeVec[packetLS.srcid]=v;
             LS_change=1;
         }
         else if(packetLS.sequenceNum >nodeVec[packetLS.srcid].sequence_number)
         {
             Vertice v=Vertice();
             v.sequence_number=packetLS.sequenceNum;
             v.neighbours=packetLS.contents;
             v.NodeID=packetLS.srcid;
             nodeVec[packetLS.srcid]=v;
             LS_change=1;
         }
         
         if(LS_change!=0){
             
             unsigned short size = 0;
             for(map<unsigned short,Port_record>::iterator it = port_status.begin();it != port_status.end();it++){
                 if(it->second.neiNodeID!=packetLS.srcid){
                     size=htons(*((unsigned short*)(p+sizeof(unsigned short))));
                     char* newP=(char*)malloc(sizeof(short)*size);
                     memcpy(newP, p, sizeof(short)*size);
                     sys->send(it->first, newP, sizeof(unsigned short)*size);
                 }
                 
             }
             
             
             
             Dijsktra(port_status, nodeVec);
             
         }
         delete[] p;
         lastTimeOfDVLS[packetLS.srcid]=(int)sys->time();
         //print_nodeVec(nodeVec);
         
     }

  }



  //at init() function unused!!!!
  void RoutingProtocolImpl::init_forwarding_table(){
        queue<Node *> q;
        vector<int> visited;
        vector<Node *> curNei;
        q.push(sys);
        visited.push_back(sys->id);
        Forwarding_record cur_record;
        while(!q.empty()){
          Node * cur=q.front();
          q.pop();
          curNei=getNeibor(cur);
          for(std::vector<Node *>::iterator it = curNei.begin(); it != curNei.end(); ++it) {
            if(std::find(visited.begin(), visited.end(), (*it)->id) != visited.end()) {
               /* v contains x *///do nothing
            } else {
              /* v does not contain x */
              cout<<"node id is"<<(*it)->id<<endl;
              if(!forwarding_table.count((*it)->id)){
                cur_record=Forwarding_record(INFINITY_COST, 0, false);
                forwarding_table[(*it)->id]=cur_record;
              }
              visited.push_back((*it)->id);
              q.push(*it);
            }
          }
        }

        cout<<"printing table for "<<sys->id<<endl;

        //for test
        for(map<unsigned short, Forwarding_record>::const_iterator it = forwarding_table.begin();it != forwarding_table.end(); ++it)
        {
            cout << it->first << " " << it->second.nextHop <<"\n";
        }


  }

  vector<Node *> RoutingProtocolImpl::getNeibor(Node * n){
     vector<Node *> curNei;
      for(std::vector<Link *>::iterator it = n->link_vector.begin(); it != n->link_vector.end(); ++it) {
        if(n->id ==(*it)->get_node1()->id) curNei.push_back((*it)->get_node2());
        else curNei.push_back((*it)->get_node1());
    }
    return curNei;
    

  }

  //unused!!!
  void RoutingProtocolImpl::update_forwarding_table(unsigned short dest, unsigned short cost){
    if(forwarding_table[dest].nextHop==0) {
      forwarding_table[dest].nextHop=dest;
      forwarding_table[dest].cost=cost;
    }
    else if(cost < forwarding_table[dest].cost){
      forwarding_table[dest].cost=cost;
    }
      forwarding_table[dest].direct=true;
  }

//use for each record in the recv forwarding table
void RoutingProtocolImpl::mergeRecord(RoutingProtocolImpl::Forwarding_record *p,
                                     RoutingProtocolImpl::Forwarding_record *current,unsigned short directcost,unsigned short nodeid){
    
    
    if(current->nextHop == nodeid){// if current record use this node to be its nexthop, then update
        if(current -> cost != p->cost + directcost){
            current->cost = p->cost + directcost;
            dv_forwardingTableChange = true;
        }
    }
    else{// if nexthop is different, then compare. find out the better solution
        if(current->cost > p->cost + directcost){
            current->cost = p->cost + directcost;
            current->nextHop = nodeid;
            dv_forwardingTableChange = true;
        }
    }
    
}

//To update the forwarding table
void RoutingProtocolImpl::updateForwardingTable(map<unsigned short, RoutingProtocolImpl::Forwarding_record> table,unsigned short directcost,unsigned short nodeid, unsigned short localid){
    
    map<unsigned short, Forwarding_record>::iterator recvtable = table.begin();
    map<unsigned short, Forwarding_record>::iterator localtable = forwarding_table.begin();
    cout<<"before gao"<<endl;
    printForwardingTable();
    
 //check is there any destionation exist in local forwarding table && not exist in recv_fd && next hop is srcid
       while(localtable != forwarding_table.end()){
            //exist in local forwarding table
            if(table.find(localtable->first) == table.end() && localtable->second.nextHop == nodeid && localtable->second.nextHop != localtable->first){
                // not exist in recv_fd && next hop is srcid, then remove
                int nxt = localtable->second.nextHop;
                localtable++;
                remove_record_ForwardingTable(nxt);
                continue;
            }
            localtable++;
        }
    cout<<"after gao"<<endl;
    printForwardingTable();
    
    for(recvtable = table.begin();recvtable != table.end();recvtable++){
        if(forwarding_table.find(recvtable->first) == forwarding_table.end() && recvtable->first != localid && recvtable->second.cost != INFINITY_COST){
            //there is no record of this destionation id in current forwarding table, then insert it.
            unsigned short directLinkCost = costfromPortStuats(nodeid);
            Forwarding_record record = Forwarding_record((directLinkCost+ recvtable->second.cost),nodeid,false);
            forwarding_table.insert(pair<unsigned short, Forwarding_record>(recvtable->first,record));
            dv_forwardingTableChange = true;
            continue;
        }
        
        
        //record of same destination exist, then compare.
        for(localtable = forwarding_table.begin();localtable != forwarding_table.end();localtable++){
            if(recvtable->first == localtable -> first){//destination is the same
                mergeRecord(&(recvtable->second), &(localtable->second),directcost,nodeid);
                break;
            }
        }
    }
    printForwardingTable();
    printPortStatus();
    cout<<"eventually gao"<<endl;
    
    checkDirectLink();
    printPortStatus();
    printForwardingTable();
}

void RoutingProtocolImpl::checkDirectLink(){//check those nodes which has direct link with current node
    
    unsigned short port_No;
    map<unsigned short,Forwarding_record>::iterator it = forwarding_table.begin();
    for(it = forwarding_table.begin(); it != forwarding_table.end();it++){
        port_No = getPortnumFromPortStatus (it->first);
        if((port_status.find(port_No) != port_status.end())&&(it->second.cost > costfromPortStuats(it->first))){
            //this node is exist in port_status table
            //Also directlink cost is better than current choice
            //Then, choose direcklink.
                it->second.cost = costfromPortStuats(it->first);
                it->second.nextHop = it->first;
                dv_forwardingTableChange = true;
        }
    }
    for(map<unsigned short,Port_record>::iterator its = port_status.begin();its != port_status.end();its++){
            unsigned short neigborID = its->second.neiNodeID;
            if(forwarding_table.find(neigborID) == forwarding_table.end()){
                 //those node does not exist in forwarding table, but has direct link, then insert into forwardingtable
                unsigned short cost =  its->second.costRRT;
    
                Forwarding_record rec = Forwarding_record(cost,its->second.neiNodeID,true);
    
                forwarding_table.insert(pair<unsigned short,Forwarding_record>(its->second.neiNodeID,rec));
    
                dv_forwardingTableChange = true;
            }
        }
}

unsigned short RoutingProtocolImpl::getPortnumFromPortStatus(unsigned short nodeid){
    for(map<unsigned short,Port_record>::iterator it = port_status.begin(); it != port_status.end(); it++){
        if(it->second.neiNodeID == nodeid)
            return it->first;
    }
    return SPECIAL_PORT;
}
unsigned short RoutingProtocolImpl::costfromPortStuats(unsigned short nodeid){
    
    map<unsigned short, Port_record>::iterator its = port_status.begin();
    for(its = port_status.begin();its != port_status.end();its++){
        if(its->second.neiNodeID == nodeid)
            return its->second.costRRT;
    }
    return INFINITY_COST;
}

//get cost from forwarding table
unsigned short RoutingProtocolImpl::costfromForwardingTable(unsigned short nodeid){
    map<unsigned short,Forwarding_record>::iterator it = forwarding_table.begin();
    for(it = forwarding_table.begin(); it != forwarding_table.end();it++){
        if(it->first == nodeid){
            return it->second.cost;
        }
    }
    
    return INFINITY_COST;
}

// if you know a portid, return NodeId
unsigned short RoutingProtocolImpl::getNeiboridFromPortNo(unsigned short portNo){
    map<unsigned short, Port_record>::iterator it = port_status.begin();
    for(it = port_status.begin();it != port_status.end();it++){
        if(portNo == it->first){
            return it->second.neiNodeID;
        }
    }
    return SPECIAL_PORT;
}

//unused
void RoutingProtocolImpl::sendPacketInEveryPort(package *pck){
    for(unsigned short k = 0; k < port_status.size();k++)
        sys->send(k,pck,sizeof(package));
}
// printing forwarding table
void RoutingProtocolImpl::printForwardingTable(){
    for(map<unsigned short,Forwarding_record>::iterator it = forwarding_table.begin();it != forwarding_table.end();it++){
        cout<<"print local forwarding table "<<"system Id: "<<sys->id <<" destination id: "<<it->first << " cost: "<<it->second.cost
        <<" nextHop: "<<it->second.nextHop<<" direct: "<<it->second.direct<<endl;
    }
}
//send DV in everyPort
void RoutingProtocolImpl::sendDVInEveryPort(){
    char *pac;
    for(map<unsigned short, Port_record>::iterator it = port_status.begin(); it != port_status.end(); it++){
        
        pac = getPacket(3,sys->id,it->second.neiNodeID);
        
        unsigned short size = htons(*((short*)(pac+sizeof(short))));
        
        sys->send(it->first,pac,size*sizeof(unsigned short));
    }
}

void RoutingProtocolImpl::checkPSChange_DV(unsigned short srcid,unsigned short costOfdirectLink,unsigned short diff){
    bool changeInfd = false;
    
    if(forwarding_table.find(srcid) == forwarding_table.end()){
        //there is no record of srcid in forwarding table, so insert it
        Forwarding_record fdrecord = Forwarding_record(costOfdirectLink,srcid,true);
        forwarding_table.insert(pair<unsigned short,Forwarding_record>(srcid,fdrecord));
        changeInfd = true;
    }
    else{
        //there is record of srcid
        //1.next hop is srcid
        //2.compare cost of srcid is destination
        
        //first situation
        for(map<unsigned short,Forwarding_record>::iterator it = forwarding_table.begin(); it != forwarding_table.end();it++){
            if(it->second.nextHop == srcid){
                it->second.cost = it->second.cost + diff;
                changeInfd = true;
            }
        }
        
        //second situation
        if(forwarding_table.find(srcid)->second.cost > costOfdirectLink){
            //cost is bigger than record in forwarding_table
            forwarding_table.find(srcid)->second.cost = costOfdirectLink;
            forwarding_table.find(srcid)->second.direct = true;
            forwarding_table.find(srcid)->second.nextHop = srcid;
            changeInfd = true;
        }
    }
    
    if(changeInfd)
        sendDVInEveryPort();
}

void RoutingProtocolImpl::remove_record_PortStatus(unsigned short neiNodeID){
    map<unsigned short,Port_record>::iterator it = port_status.begin();
    while(it != port_status.end()){
        if (it->second.neiNodeID == neiNodeID) {
            port_status.erase(it++);
            continue;
        }
        it++;
    }
}

void RoutingProtocolImpl::remove_record_ForwardingTable(unsigned short nexthop){
    map<unsigned short, Forwarding_record>::iterator it = forwarding_table.begin();
    while(it != forwarding_table.end()){
        if(it->second.nextHop == nexthop){
            forwarding_table.erase(it++);
            continue;
        }
        it++;
    }
}

void RoutingProtocolImpl::printPortStatus(){
    for(map<unsigned short,Port_record>::iterator it = port_status.begin();it != port_status.end(); it++){
        cout<<"current port statuts ,Node ID: "<< sys->id<<" port No: "<<it->first<<" NeiNodeId "<<it->second.neiNodeID<<" cost "<<it->second.costRRT<<endl;
    }
}

RoutingProtocolImpl::DV RoutingProtocolImpl::getDV(char* p){
    DV packet=DV();
    //cout<<"htons(*((short*)(p+sizeof(unsigned short))))-4="<<htons(*((short*)(p+sizeof(unsigned short))))-4<<endl;
    packet.type=*((unsigned char*)p);
    packet.reserved=htons(*((unsigned char*)(p+sizeof(unsigned char))));
    packet.size=htons(*((unsigned short*)(p+sizeof(unsigned short))));
    packet.srcid=htons(*((unsigned short*)(p+2*sizeof(unsigned short))));
    packet.dstid=htons(*((unsigned short*)(p+3*sizeof(unsigned short))));
    map<unsigned short, unsigned short> content;
    content=getMapFromPacket(p+sizeof(unsigned short)*4, htons(*((unsigned short*)(p+sizeof(unsigned short))))-4);
    ///
    
    
    packet.contents=content;
    cout<<"recv dv package, destination and cost :"<<endl<<"sender: "<<packet.srcid <<" to "<< "receiver:"<< packet.dstid<<endl;
    printMap(content);
    return packet;
}

map<unsigned short, unsigned short> RoutingProtocolImpl::getMapFromPacket(char* packet, unsigned short mapSize){
    map<unsigned short, unsigned short> contents;
    
    for(int i=0;i<mapSize;i+=2){
        unsigned short id=htons(*((unsigned short*)(packet+i*sizeof(unsigned short))));
        
        unsigned short cost=htons(*((unsigned short*)(packet+(i+1)*sizeof(unsigned short))));
        contents.insert(pair<unsigned short, unsigned short>(id, cost));
    }
    return contents;
}

void RoutingProtocolImpl::printMap(map<unsigned short, unsigned short> mymap){
    int min;
    cout<<"printing map"<<endl;
    for(map<unsigned short,unsigned short>::iterator its = mymap.begin();its != mymap.end();its++){
        cout<<its->first<<"  "<<its->second<<endl;
    }
    
}

//calculate Size
unsigned short RoutingProtocolImpl::calculateSize(char type) {
    unsigned short result = 0;
    if(type == 0) result = 6;
    else if(type == 1) result = 6;
    else if(type == 2) result = 6;
    else if(type == 3) result = 2*forwarding_table.size() + 4;
    else if(type == 4) result = 2*port_status.size() + 6;
    return result;
    
}

// this will form a standard packet before sending
char* RoutingProtocolImpl::getPacket(char type, unsigned short srcID, unsigned short destID){
    
    cout<<"type="<< (int)type <<" srcID="<<srcID<<" destID="<<destID<<endl;
    //get the size of the packet (in unsigned short) by the given type
    unsigned short size=calculateSize(type);
    cout<<"size of the pack="<<size<<endl;
    //get the packet pointer
    char* packet=(char*)malloc(sizeof(short)*size);
    
    //set type of the default packet
    *((unsigned char*)packet)=type;
    //set the reserved section of the default packet to be '0'
    unsigned char rreserved='0';
    *((unsigned char*)(packet+sizeof(unsigned char)))=rreserved;
    
    //set the size section of the default packet
    *((unsigned short*)(packet+sizeof(unsigned short)))=htons(size);
    //set the source ID section of the default packet
    *((short*)(packet+2*sizeof(short)))=htons(srcID);
    //set the destination ID section of the default packet
    *((short*)(packet+3*sizeof(short)))=htons(destID);
    
    
    if(type==0){
        //data packet
    }
    else if(type==1){
        //Ping packet
//        cout<<"this is a Ping packet"<<endl;
//        cout<<"srcID="<<htons(*((short*)(packet+2*sizeof(short))))<<endl;
//        cout<<"destID="<<htons(*((short*)(packet+3*sizeof(short))))<<endl;
//        
       int time=(int)sys->time();
//        cout<<"the time = "<<time<<endl;
        *((int*)(packet+4*sizeof(short)))=htons(time);
        
    }
    else if(type==2){
        //Pong packet
        cout<<"this is a Pong packet"<<endl;
        
        //do changes in recv() method
    }
    else if(type==3){
        //DV packet
       // cout<<"this is a DV packet"<<endl;
        
        int count=0;
        //get destId and cost from the forwarding table
        
        for(map<unsigned short, Forwarding_record>::iterator its=forwarding_table.begin();its!=forwarding_table.end();its++){
            
            //set the desintaion ID
            *((unsigned short *)(packet+(4+count)*sizeof(unsigned short)))=htons(its->first);
            //set the cost
            if(its->second.nextHop == destID){
                /*if destination id is the same with nexthop in forwarding table,
                 then posion reverse
                 */
                *((unsigned short*)(packet+(5+count)*sizeof(unsigned short)))=htons(INFINITY_COST);
            }
            else{
                *((unsigned short*)(packet+(5+count)*sizeof(unsigned short)))=htons(its->second.cost);
            }
            count+=2;
        }
    }
    else if(type==4){
        cout<<"this is a LS packet"<<endl;
        //LS packet
        
        int count=0;
        
        //set the sequenceNum
        sequenceNum++;
        *((int*)(packet+4*sizeof(unsigned short)))=htons(sequenceNum);
        
        //get the neigborID and cost from the port status
        for(map<unsigned short, Port_record>::iterator it=port_status.begin();it!=port_status.end();it++){
            *((unsigned short*)(packet+(6+count)*sizeof(unsigned short)))=htons(it->second.neiNodeID);
            *((unsigned short*)(packet+(7+count)*sizeof(unsigned short)))=htons(it->second.costRRT);
            
            count+=2;
        }
        
    }
    return packet;
}

RoutingProtocolImpl::package *RoutingProtocolImpl::getOriginalDVpacket(DV p){
    Forwarding_record record;
    map<unsigned short,Forwarding_record> *dvmap;
    package *packet;
    packet = new package();
    packet->type = p.type;
    packet->size = p.size;
    packet->srcid = p.srcid;
    packet->dstid = p.dstid;
    packet->data = 0;
    dvmap = new map<unsigned short, Forwarding_record>();
    for(map<unsigned short,unsigned short>::iterator its = p.contents.begin(); its != p.contents.end(); its++){
        record = Forwarding_record(its->second,0,false);
        dvmap->insert(pair<unsigned short,Forwarding_record>(its->first,record));
    }
    packet->table = dvmap;
    return packet;
}

void RoutingProtocolImpl::printLSPacket(LS packet){
    cout<<"packet.type= "<<(int)packet.type<<endl;
    cout<<"packet.reserved= "<<(int)packet.reserved<<endl;
    cout<<"packet.srcid= "<<packet.srcid<<endl;
    cout<<"packet.dstid= "<<packet.dstid<<endl;
    cout<<"packet.sequenceNum= "<<packet.sequenceNum<<endl;
    printMap(packet.contents);
}

RoutingProtocolImpl::LS RoutingProtocolImpl::getLS(char* p){
    LS packet=LS();
    
    packet.type=*((unsigned char*)p);
    packet.reserved=htons(*((unsigned char*)(p+sizeof(unsigned char))));
    packet.size=htons(*((unsigned short*)(p+sizeof(unsigned short))));
    packet.srcid=htons(*((unsigned short*)(p+2*sizeof(unsigned short))));
    packet.dstid=htons(*((unsigned short*)(p+3*sizeof(unsigned short))));
    packet.sequenceNum=htons(*((int*)(p+4*sizeof(unsigned short))));
    map<unsigned short, unsigned short> content;
    content=getMapFromPacket(p+sizeof(unsigned short)*6, htons(*((short*)(p+sizeof(unsigned short))))-6);
    
    
    //printMap(content);
    packet.contents=content;
    
    return packet;
    
}

void RoutingProtocolImpl::print_PortStatus(map<unsigned short ,Port_record> status){
    for(map<unsigned short ,Port_record>::iterator its = status.begin();its != status.end();its++){
        cout<<" Neibour node: "<<its->second.neiNodeID<<" cost is: "<<its->second.costRRT<<endl;
    }
}

map<unsigned short,unsigned short> RoutingProtocolImpl::convert_Port_status(){
	map<unsigned short,unsigned short> retMap;
	for(map<unsigned short,Port_record>::iterator it = port_status.begin();it != port_status.end(); it++){
		retMap[it->second.neiNodeID]=it->second.costRRT;
	}
	return retMap;
    
}

void RoutingProtocolImpl::print_nodeVec(map<unsigned short, Vertice> vec){
    cout<<"nodeVEc for node:///////////////////////////////////"<<sys->id<<endl;
    for(map<unsigned short,Vertice>::iterator its = vec.begin();its != vec.end();its++){
        cout<< " hashkey: "<< its->first<<" id: "<<its->second.NodeID<<" seq_num: "<<its->second.sequence_number;
        for(map<unsigned short,unsigned short>::iterator it = its->second.neighbours.begin();it != its->second.neighbours.end();it++){
			cout<<endl;
			cout<<it->first<<"   "<<it->second<<endl;
        }
        //print_PortStatus(its->second.neighbours);
	}
    cout<<endl;
    
}

unsigned short RoutingProtocolImpl::findMin(map<unsigned short, unsigned short> mymap){
    unsigned short min;
    unsigned short minValue=INFINITY_COST;
    for(map<unsigned short,unsigned short>::iterator its = mymap.begin();its != mymap.end();its++){
        if(its->second <= minValue) {
            minValue=its->second;
            min=its->first;
        }
    }
    
    return min;
    
}

void RoutingProtocolImpl::Dijsktra( map<unsigned short,Port_record> Myport, map<unsigned short, Vertice> mynodeVec){
    map<unsigned short, unsigned short> dis;
    map<unsigned short, unsigned short> Q;
    Forwarding_record cur_record;
    map<unsigned short, Forwarding_record> tmpFT;
    map<unsigned short, Forwarding_record> hopFT;
    unsigned short hop;
	map<unsigned short, Vertice> mynodeVec1=nodeVec;
    
    //forwarding_table.clear();
    for(map<unsigned short,Vertice>::iterator its = mynodeVec.begin();its != mynodeVec.end();its++){
        if(its->first != sys->id){
            tmpFT[its->first].cost=INFINITY_COST;
        }
    }
    
    for(map<unsigned short,Port_record>::iterator its = Myport.begin();its != Myport.end();its++){
		cur_record=Forwarding_record(its->second.costRRT, its->second.neiNodeID, true);
		tmpFT[its->second.neiNodeID]=cur_record;
        
        
    }
    
    
    for(map<unsigned short,Forwarding_record>::iterator its = tmpFT.begin();its != tmpFT.end();its++){
        Q[its->first]=its->second.cost;
    }
    
    map<unsigned short,unsigned short> nextHop_vec = mynodeVec[sys->id].neighbours;
    
    //Q=dis;
    // Q.erase(sys->id);
    // cout<<"dij debug:"<<endl;
    // cout<<"Printing port status: "<<endl;
    // print_PortStatus(Myport);
    // cout<<endl;
    
    // print_PortStatus(port_status);
    // cout<<endl;
    // cout<<endl;
    //print_tmpFT();
    // cout<<"first is here"<<endl;
    //   print_forwarding_table1(tmpFT);
    
    
    
    while(!Q.empty()){
        unsigned short w=findMin(Q);
        mynodeVec=mynodeVec1;
        cout<<w<<endl;
        Q.erase(w);
        if(tmpFT[w].cost==INFINITY_COST) {
        	continue;
        }
        map<unsigned short,unsigned short> neibour=mynodeVec[w].neighbours;
        printMap(neibour);
        for(map<unsigned short,unsigned short>::iterator its = neibour.begin();its != neibour.end();its++){
            if(Q.count(its->first)){
                unsigned short c1, c2;
                c1=tmpFT[its->first].cost;
                c2=its->second+tmpFT[w].cost;
                unsigned short minCost= c1>c2?c2:c1;
                tmpFT[its->first].cost=minCost;
                tmpFT[its->first].nextHop=w;
                Q[its->first]=minCost;
            }
        }
    }
    
    hopFT=tmpFT;
    // cout<<"dij debug:"<<endl;
    // cout<<"Printing port status: "<<endl;
    // print_PortStatus(Myport);
    // cout<<endl;
    
    // print_nodeVec(mynodeVec);
    // cout<<endl;
    // print_forwarding_table1(hopFT);
    // cout<<endl;
    
    for(map<unsigned short,Forwarding_record>::iterator its = hopFT.begin();its != hopFT.end();its++){
    	unsigned short cur=its->first;
    	if(hopFT[its->first].cost==INFINITY_COST){
			hopFT[its->first].nextHop=nextHop_vec.begin()->first;
            
    	}
    	else if(!nextHop_vec.count(its->first)){
            hop=its->second.nextHop;
    	    if(hop==its->first) continue;
            
            while(!nextHop_vec.count(hop)){
                // if(!hopFT.count(hop)) {
                // 	continue;}
                
                hop=tmpFT[hop].nextHop;
            }
            hopFT[its->first].nextHop=hop;
    	}
    	else{
            hopFT[its->first].nextHop=its->first;
    	}
    }
    
    
    forwarding_table=hopFT;
    
    
}

void RoutingProtocolImpl::sendLSInEveryPort(){
    char* px;//=getPacket(1, router_id, 0);
    unsigned short size;
    Vertice v;
    sequenceNum++;
    v= Vertice(sequenceNum, sys->id);
    v.neighbours=convert_Port_status();
    v.NodeID=sys->id;
    v.sequence_number= sequenceNum;
    nodeVec[sys->id]=v;
    
    for(int i=0; i<sys->link_vector.size();i++){
        if((sys->link_vector[i])->get_is_alive()){
            
            px=getPacket(4, sys->id, 0);
            sequenceNum++;
            size=htons(*((unsigned short*)(px+sizeof(unsigned short))));
            sys->send(i, px, sizeof(unsigned short)*size);
        }
    }
    cout<<"wagaga"<<sys->id<<endl;
    print_PortStatus(port_status);
    
    Dijsktra(port_status, nodeVec);
}

  // add more of your own code