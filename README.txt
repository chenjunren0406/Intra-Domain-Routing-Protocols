Team:
Minsheng Chen, Xihao Zhu, Junren Chen, Yinuo Wang


Two edited file:
RoutingProtocolImpl.cc
RoutingProtocolImpl.h

The test file:
test

The output is:
For DV: DV.out
For LS: LS.out

Before running this program, please use command below to generate excutable files:
make
Then, use command:
./Simulator testFilename DV/LS

For example:
To test the test file named "test" by using DV algorithm, please use:
./Simulator test DV


********Important notes********************
Our group have some misunderstanding at first, therefore our project may lead to some confusion. Here to calcify some points:

We use the method getPacket(), which is in RoutingProtocolImpl.cc, to get the 5 different types of packets which will be sent to other nodes. However, we also has a packet struct, which is independent from the given General packet format in the Handout. After we get a packet, we will use get the contents of the received packet and copy those contents to a packet struct, when implementing the DV, we use the packet struct.