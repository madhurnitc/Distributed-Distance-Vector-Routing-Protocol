#### Distributed Distance Vector Routing Protocol
__________________________
__________________________

<paragraph> In this assignment we implemented a simplified version of the Distance Vector
Protocol. The protocol will be run on top of servers (behaving as routers) using UDP.
Each server runs on a machine at a predefined
port number. The servers should be
able to output their forwarding tables along with the cost and should be robust to link
changes. We implemented the basic algorithm: count to infinity, NOT poison
reverse.


In addition, a server should send out routing packets only in the following two conditions: 

1. periodic update.
2. the user uses a command asking for one.

This is a little different from the original algorithm which immediately sends out update routing information when routing table changes.

NOTE: We  used UDP Sockets only for our implementation. Further, we used only the select() API for handling multiple socket connections.

We also  need to use select timeout to implement multiple timers. 