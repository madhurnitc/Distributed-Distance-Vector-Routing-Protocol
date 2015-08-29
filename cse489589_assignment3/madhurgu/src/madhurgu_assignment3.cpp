/**
 * @madhurgu_assignment3
 * @author  MADHUR GUPTA <madhurgu@buffalo.edu>
 * @version 1.0
 *
 * @section LICENSE
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details at
 * http://www.gnu.org/copyleft/gpl.html
 *
 * @section DESCRIPTION
 *
 * This contains the main function. Add further description here....
 */
#include <iostream>
#include <stdio.h>
#include <string.h>
#include <fstream>
#include <cstdlib>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <limits.h>
#include <sys/select.h>
#include <sys/types.h>
#include <stdint.h>
#include <netdb.h>
#include <time.h>
#include<algorithm>
#include <arpa/inet.h>
#include "../include/global.h"
#include "../include/logger.h"


#define STDIN 0
#define SIZE 1024
using namespace std;

//function prototyping
void printCostMatrix();
void parseTopologyFile(char* pathtoTopologyFile);
void sortServerListbyID();
void runStepCommand(char *cmd);
void runUpdateCommand(char* cmd,uint16_t sid1,uint16_t sid2,uint16_t cost);
void initializeCostMatrix();
void printPacket();
void runBellmanFord();
void runDisplayCommand(char* cmd);
void runDumpCommand(char* cmd);
void parseRecievedPacket();
void runDisableCommand(char* cmd,uint16_t sid);
void sendPeriodicUpdates();
void runAcademicIntegrityCommand(char* cmd);
void runCrashCommand();


struct peerinfo
{
	uint32_t peerip;    // IP
	uint16_t peerPort;  // port number of the peer server
	uint16_t dummyfield; // corresponds to 0x0 field
	uint16_t peerID;     // Unique Identifier for the peer
	uint16_t peerCost;   //cost to this peer from the given server
};
//[PA3] Update Packet Start
struct rtPacket
{
	uint16_t noOfUpdates;
	uint16_t port;
	uint32_t ip;
	peerinfo pInfo[5];
};
//[PA3] Update Packet End

struct serverList
{
	uint16_t id;
	uint32_t ip;
	uint16_t port;
};

struct mytimers
{
	bool firstupdateReceived;
	int noOfUpdatesMissed;
};
mytimers mytm[6]; //declaring 6 as I will index the array of timers by Server ID e.g mytm[2] means timer with server ID 2
/*struct neighborList
{
	uint16_t myid;
	uint16_t peerid;
	uint16_t cost;
}; */

//[PA3] Routing Table Start
struct routingtable
{
	uint16_t server_id;
	uint16_t server_cost;
	uint16_t next_hop_id;
};
//[PA3] Routing Table End.
serverList myServerList[6];
int sortedserverID[5];
int sock_fd;
int sock_fd1;
//neighborList myNeighborList[5];
routingtable rtable[4];
peerinfo  pInfo[4];
uint16_t costmatrix[6][6];
uint16_t myID,myPort;
int neighborID[5];
int neighborCost[5];
int currentCountOfDVReceived=0;
uint16_t INFINITY = 65535;
uint16_t NUMBER_OF_SERVER = 0;
uint16_t NUMBER_OF_NEIGHBOR = 0;
rtPacket packet,receivedPacket;

/**
 * main function
 *
 * @param  argc Number of arguments
 * @param  argv The argument list
 * @return 0 EXIT_SUCCESS
 */
int main(int argc, char **argv)
{
	/*Init. Logger*/
        cse4589_init_log();

	/* Clear LOGFILE and DUMPFILE */
        fclose(fopen(LOGFILE, "w"));
        fclose(fopen(DUMPFILE, "wb"));

	/*Start Here*/
    	struct addrinfo hints, *res;
    	int s=0;
    	int selret;
    	int sock_index;
    	char command[SIZE];
		char *p;
		char *argv1[6];
    	timeval structtimeval;
    	char port[32];
    	/*master file descriptor list */
    	fd_set master_list;
    	/* temp file descriptor list for select() */
    	fd_set watch_list;
    	/* maximum file descriptor number */
    	int head_socket;
    	// timeout value for the socket
    	int timeout;

    	if (argc < 5)
    	{
    		// Number of parameters are less. Mark an error
    		cse4589_print_and_log("%s:%s\n",argv[0],"Invalid Command.In-sufficient Parameters. Please refer to command.Usage :  ./assignment3 ­-t <path­to­topology­file> ­-i <routing­update­interval> ");
    		return 0;
    	}
    	if ((argc == 5) && ( strcasecmp(argv[1], "-t") == 0) && (strcasecmp(argv[3],"-i") ==0) && atoi(argv[4]))
    	{
             parseTopologyFile(argv[2]);
    	}
    	else
    	{
    		// all the parameters are not of correct type
    		cse4589_print_and_log("%s:%s\n",argv[0],"Invalid Command.Check Values of the parameters. Please refer to command.Usage :  ./assignment3 ­-t <path­to­topology­file> ­-i <routing­update­interval> ");
    		return 0;
    	}
    	// set the time out value that we read from CLI
    	timeout = atoi(argv[4]);
    	memset(&hints, 0, sizeof(hints));
    	hints.ai_family = AF_INET;
    	hints.ai_socktype = SOCK_DGRAM;
    	hints.ai_flags = AI_PASSIVE;     // fill in my IP for me

    	// find my listening port and
        //convert the port to char* so that it can be used in getaddrinfo
    	for(int i =0 ;i<NUMBER_OF_SERVER;i++)
    	{
    		if(myID == myServerList[i].id)
    		{
    			sprintf(port,"%d",myServerList[i].port);
    			break;
    		}
    	}

    	s=getaddrinfo(NULL,port,&hints,&res);
    	cout<<"Port = "<<port<<endl;
    	//create a sending socket
		sock_fd1 = socket(AF_INET, SOCK_DGRAM, 0);
		if (sock_fd1 < 0)
		{
			perror("socket");
			return -1;
		}

    	// create listening socket
    	sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    	if (sock_fd < 0)
    	{
    		cerr<<"Could not create socket"<<endl;
    		return -1;
    	}
    	if (bind(sock_fd, res->ai_addr, res->ai_addrlen) < 0)
    	{
    		cerr<<"Could not bind the socket"<<endl;
    		return -1;
    	}
    	/* add the sock_fd to the master set */

    	/* keep track of the highest file descriptor */

    	head_socket = sock_fd; /* so far, it's this one*/
	    if (sock_fd1 > head_socket)
		{
		   head_socket = sock_fd1;
		}
	    //set the file descriptor
    	FD_ZERO(&master_list);
    	FD_SET(sock_fd, &master_list);
		FD_SET(sock_fd1, &master_list);
    	FD_SET(STDIN, &master_list);
    	//initialize timer structure
		structtimeval.tv_sec = timeout;
		structtimeval.tv_usec = 0;
    	while(1)
    	{
    		//clear the watch_list
    		FD_ZERO(&watch_list);
    		watch_list = master_list;
    		printf("assignment_bash$");
    		fflush(stdout);
    		selret = select(head_socket + 1, &watch_list, NULL, NULL, &structtimeval);
    		if (selret < 0)
    		{
    			perror("select failed");
    			return -1;
    		}
     		if (selret > 0)
     		{
    			for (sock_index = 0; sock_index <= head_socket; sock_index++)
    			{
    				if (FD_ISSET(sock_index, &watch_list))
    				{
    					if (sock_index == STDIN)
    					{
    						fgets(command, 256, stdin);
    						//char command1[256];
    						//strcpy(command1,command);
    						if ((p = strchr(command, '\n')) != NULL)
    							*p = '\0';

    						//Tokenize the command
    						char *pch;

    						int argcount = 0;
    						pch = strtok(command, " ");
    						while (pch != NULL)
    						{
    							argv1[argcount] = (char*)malloc(strlen(pch));
    							strcpy(argv1[argcount], pch);
    							argcount = argcount + 1;
    							pch = strtok(NULL, " ");
    						}
    						if ((strcasecmp(argv1[0],"update") == 0) && (argcount==4))
    						{
    							if(atoi(argv1[1]) && atoi(argv1[2]) && atoi(argv1[3]))
    							{
                                     runUpdateCommand(command,atoi(argv1[1]),atoi(argv1[2]),atoi(argv1[3]));
    							}
    							else
    							{
    								//Value for the arguments are not integer. Mark and error
    								cse4589_print_and_log("%s:%s\n",command,"Invalid value for arguments.");
    							}

    						}
    						else if ((strcasecmp(argv1[0],"step") == 0) && (argcount==1))
    						{
        						//execute step command
    							runStepCommand(command);
    						}

    						else if ((strcasecmp(argv1[0],"display") == 0) && (argcount==1))
    						{
        						// execute display command
    							runDisplayCommand(command);
    						}
    						else if((strcasecmp(argv1[0],"dump") == 0) && (argcount==1))
    						{
        						// execute dump command
    							runDumpCommand(command);
    						}
    						else if((strcasecmp(argv1[0],"packets") == 0) && (argcount==1))
    						{
                                   cse4589_print_and_log("%s:SUCCESS\n",command);
                                   cse4589_print_and_log("%d\n",currentCountOfDVReceived);
                                   //reset it to zero so that we can count the number of DV received for next cycle
                                   currentCountOfDVReceived =0;
    						}
    						else if((strcasecmp(argv1[0],"disable") == 0) && (argcount==2) && atoi(argv1[1]))
    						{
        						// execute disable command
    							runDisableCommand(command,atoi(argv1[1]));
    						}
    						else if((strcasecmp(argv1[0],"crash") == 0) && (argcount==1))
    						{
    							//run crash command
    							runCrashCommand();
    						}
    						else if((strcasecmp(argv1[0],"academic_integrity") == 0) && (argcount==1))
    						{
    							// run academic integrity command
    							runAcademicIntegrityCommand(command);
    						}
    						else
    						{
    							//invalid command
    							cse4589_print_and_log("%s:%s\n",command,"Invalid command");
    						}
    					}
    					else if (sock_index == sock_fd)
    					{
                             cout<<"Receiving data from the other machine"<<endl;
                             int n= recvfrom(sock_fd,&receivedPacket,sizeof(receivedPacket),0,NULL,NULL);
                             if (n>0)
                             {
                                 // parse the received packet
                                 parseRecievedPacket();
                                 //run the bellman ford
                                 runBellmanFord();
                                 printCostMatrix();
                             }

    					}
    				}
    			}

     		}
     		if (selret == 0)
     		{
     			//reset the values in timer.
     			structtimeval.tv_sec = timeout;
     			structtimeval.tv_usec = 0;
     			cout<<"Timeout happened"<<endl;
     			//check whether we have received first update from the neighbor
     			// If yes, increase the noOfMissedUpdates by one for that neighbor
     			for(int i=1;i<=NUMBER_OF_SERVER;i++)
     			{
     				for(int j=0;j<NUMBER_OF_NEIGHBOR;j++)
     				{
     					//check if we have received first update from the neighbor.
     					if( i== neighborID[j] && mytm[i].firstupdateReceived)
     					{
     						mytm[i].noOfUpdatesMissed++;
     						cout<<"Number of Updates missed from "<<i<<" "<<mytm[i].noOfUpdatesMissed<<endl;
     					}
     				}
     			}
     			//check if noofUpdatesMissed for any server is greater or equal to 3
     			int count =0;
     			for(int i=1;i<=NUMBER_OF_SERVER;i++)
     			{
     				if(mytm[i].noOfUpdatesMissed >= 3)
     				{
     					for(int j=0;j<NUMBER_OF_NEIGHBOR;j++)
     					{
     					    if(i==neighborID[j])
     					    {
     					    	//count keeps track of number of neighbor to be removed.
     					    	count++;
     					    	//update cost matrix for this neighbor to infinity
     					    	cout<<"Updating the cost to "<<i<<" infinity"<<endl;
     					    	costmatrix[myID][i] = INFINITY;
     					    	for(int k=j;k<NUMBER_OF_NEIGHBOR;k++)
     					    	{
     					    		neighborID[k] = neighborID[k+1];
     					    		neighborCost[k] = neighborCost[k+1];
     					    	}
     					    }
     					}
     				}
     			}
     			//update neighbor count
     			NUMBER_OF_NEIGHBOR =  NUMBER_OF_NEIGHBOR - count;
     			// run bellman ford
     			runBellmanFord();
     			// send my Distance vector to neighbors
     			sendPeriodicUpdates();

     		}
    	}

	
	return 0;
}
// This function parses the given topology file.
// Based on topology file, it fills initializes total number of servers in the topology,
// total number of neighbors of this server,
// details of each server in the topology,
// initial cost matrix for this server
// @param pathtoTopologyFile  --- Path to the topology file.
// Reference for below function: http://www.learncpp.com/cpp-tutorial/136-basic-file-io/
void parseTopologyFile(char* pathtoTopologyFile)
{
	int servercount;
	int neighborcount;
	int lineCount = 0;
	int j=0,k=0;
    ifstream topologyfile(pathtoTopologyFile);
    if (!topologyfile)
    {
        // Print an error and exit
        cse4589_print_and_log("%s:%s\n","./assignment3","Could not open topology file");
        exit(0);
    }
    while (topologyfile)
    {
        // read data from the file into a string
    	char strInput[32];
        topologyfile>>strInput;
    	lineCount++;
    	//Line# 1 corresponds to number of server in topology
    	if (lineCount == 1)
    	{
    		servercount = atoi(strInput);
    		NUMBER_OF_SERVER = servercount;
        	// Sine we got NUMBER of Servers in topology initialize cost matrix
        	initializeCostMatrix();
    	}
    	else if (lineCount == 2)
    	{
    		neighborcount = atoi(strInput);
    		NUMBER_OF_NEIGHBOR = neighborcount;
    	}
        //lines corresponding to the list of server in topology file
    	else if(lineCount>2 && lineCount<=2+3*NUMBER_OF_SERVER)
    	{
    		// fill the server details like ID,IP and Port
           if((lineCount+3)%3 == 0)
           {
        	   myServerList[j].id = atoi(strInput);
           }
           else if((lineCount+3)%3 == 1)
           {
        	   in_addr addr;
        	   inet_aton(strInput,&addr);
        	   myServerList[j].ip=addr.s_addr;

           }
           else if((lineCount+3)%3 == 2)
           {
        	   myServerList[j].port = atoi(strInput);
        	   j++;
           }
    	}
        //lines corresponding to the neighbor list in toplogy file
    	else if(lineCount>2+3*NUMBER_OF_SERVER && lineCount<=(2+3*NUMBER_OF_SERVER)+3*NUMBER_OF_NEIGHBOR)
    	{
    		//fill the neighbor details like MyID,Neighbor's ID and cost to this neighbor
            if((lineCount+3)%3 == 0)
            {
         	   myID=atoi(strInput);
            }
            else if((lineCount+3)%3 == 1)
            {
         	   neighborID[k]=atoi(strInput);
            }
            else if((lineCount+3)%3 == 2)
            {
               neighborCost[k] = atoi(strInput);
         	   costmatrix[myID][neighborID[k]]=atoi(strInput);
         	   rtable[neighborID[k]].server_id = neighborID[k];
         	   rtable[neighborID[k]].server_cost = costmatrix[myID][neighborID[k]];
         	   rtable[neighborID[k]].next_hop_id = rtable[neighborID[k]].server_id;
         	   k++;
            }

    	}

    }
    //sort the server list based on the server ID.
    sortServerListbyID();
    for (int i=0;i<servercount;i++)
    {
    	if(myServerList[i].id==myID)
    		myPort = myServerList[i].port;
    	cout<<myServerList[i].id<< " "<<myServerList[i].ip<<" "<<myServerList[i].port<<endl;
    }
    printCostMatrix();
    cout<<"Neighborlist"<<endl;
    for(int i=0;i<NUMBER_OF_NEIGHBOR;i++)
    {
    	cout<<neighborID[i]<<" "<<neighborCost[i]<<endl;
    }
}
/*This function sorts the server list read from the topology file based on their server ID
 *
 */
void sortServerListbyID()
{
	serverList temp;
	for(int i=0;i<NUMBER_OF_SERVER;i++)
	{
		for(int j=0;j<NUMBER_OF_SERVER;j++)
		{
			if(myServerList[i].id < myServerList[j].id)
			{
				temp = myServerList[i];
				myServerList[i] = myServerList[j];
				myServerList[j] = temp;
			}
		}
	}
}
/* This function just check the index of a server from the list of servers in the topology.
 *
 */
int findIndex(int serverID)
{
   for (int i=0;i<NUMBER_OF_SERVER;i++)
   {
	   if(sortedserverID[i]==serverID)
		   return i;
   }
   return -1;
}

/* This function initialize the cost matrix.
 * this function is called before we read the topology file.
 * It sets the values to INFINITY excepts for the entries where where row# == column# which is set to zero.
 */
void initializeCostMatrix()
{
   int i,j,k;
   for(i=1;i<=NUMBER_OF_SERVER;i++)
   {
	   for(j=1;j<=NUMBER_OF_SERVER;j++)
	   {
		   // cost to itself is set to zero
		   if(i ==j)
			   costmatrix[i][j] = 0;
		   else
		       costmatrix[i][j] = INFINITY;
	   }
   }
   printCostMatrix();
}
// This function is just used for debugging purpose to see the cost matrix.
void printCostMatrix()
{
	for(int i=1;i<=NUMBER_OF_SERVER;i++)
	{
		for(int j=1;j<=NUMBER_OF_SERVER;j++)
		{
			cout<<costmatrix[i][j]<<" ";
		}
		cout<<endl;
	}
}
/* This function runs the Bell Ford equation on the cost matrix,
 * And tries to find a shorter path between two nodes.
 * Reference : http://www.thelearningpoint.net/computer-science/c-program-distance-vector-routing-algorithm-using-bellman-ford-s-algorithm
 */
void runBellmanFord()
{
	int count=0;
	int i,j,k;
	do
	{
		count=0;
       for(i=1;i<=NUMBER_OF_SERVER;i++)
       {
    	   for(j=1;j<=NUMBER_OF_SERVER;j++)
    	   {
    		   for(k=1;k<=NUMBER_OF_SERVER;k++)
    		   {
    			   if(costmatrix[i][j]>costmatrix[i][k]+costmatrix[k][j])
    			   {
    				   costmatrix[i][j]=costmatrix[i][k]+costmatrix[k][j];
    				   count++;
    				   rtable[j].server_id = j;
    				   rtable[j].server_cost = costmatrix[i][j];
    				   // this k will the next_hop to reach j
    				   rtable[j].next_hop_id = k;
    			   }
    			   if(costmatrix[i][j]>INFINITY)
    				   costmatrix[i][j]=INFINITY;

    		   }
    	   }
       }

	}while(count != 0);
}
/*This function excutes the update command.
 * @param sid1 server ID of machine on which this program is running
 * @param sid2 server ID of neighbor.
 * @param cost new cost between sid1 and sid2.
 */
void runUpdateCommand(char* cmd,uint16_t sid1,uint16_t sid2,uint16_t cost)
{
	bool b1=false, b2=false;
	//verify the server ID
	if (sid1 == myID)
	{
			b1=true;
	}
	//check if the sid2 is actually a neighbor of this server
	for(int i=0;i<NUMBER_OF_NEIGHBOR;i++)
	{
		//check if  sid2 is actually a neighbor of sid1
		if(sid2==neighborID[i])
		{
			b2 = true;
			break;
		}
	}
  //if both the IDs are valid , update the cost matrix.
  if (b1 && b2)
  {
   //update the cost matrix
   costmatrix[sid1][sid2]=cost;
   //run bellman ford
   runBellmanFord();
   cse4589_print_and_log("%s:SUCCESS\n",cmd);
  }
  else
  {
	  cse4589_print_and_log("%s:%s\n",cmd,"Invalid Server ID");
  }

}
/* This function runs the "step" command.
 * It sends the routing packet to all the neighbors which are directly connected to this server.
 */
void runStepCommand(char *cmd)
{
    int k=0;
	//add number or update fields
	packet.noOfUpdates =htons(NUMBER_OF_SERVER);
	//add server port
	packet.port = htons(myPort);
    // fill the server details
	for(int i=0;i<NUMBER_OF_SERVER;i++)
	{
		//get the IP of local machine from list of server's using its ID
		if(myServerList[i].id == myID)
		{
			packet.ip = myServerList[i].ip;
		}
		// fill the details for each server that needs to be used in routing packet.
			packet.pInfo[k].peerip = myServerList[i].ip;
			packet.pInfo[k].peerPort = htons(myServerList[i].port);
			packet.pInfo[k].dummyfield = htons(0);
			packet.pInfo[k].peerID = htons(myServerList[i].id);
			packet.pInfo[k].peerCost = htons(costmatrix[myID][myServerList[i].id]);
			k++;
	}
	//Just print the packet data for debugging purposes.
	printPacket();
	//send packet to all neighbors
	for(int i=0;i<NUMBER_OF_NEIGHBOR;i++)
	{
		for (int j=0;j<NUMBER_OF_SERVER;j++)
		{

			if(neighborID[i] == myServerList[j].id)
			{

				cout<<"Sending packet to neighbor "<<neighborID[i]<<endl;
				sockaddr_in servaddr;
				in_addr addr;
				bzero(&servaddr,sizeof(servaddr));
				addr.s_addr=myServerList[j].ip;
                char* s = inet_ntoa(addr);
                servaddr.sin_family = AF_INET;
				servaddr.sin_addr.s_addr = myServerList[j].ip;
                servaddr.sin_port = htons(myServerList[j].port);
                cout<<"converted IP and Port : "<<s<<" "<<servaddr.sin_port<<endl;
                cout<<"server struct"<<servaddr.sin_addr.s_addr<<servaddr.sin_port<<endl;
				sendto(sock_fd1,&packet,sizeof(packet),0,(struct sockaddr *)&servaddr,sizeof(servaddr));
			}
		}
	}
	cse4589_print_and_log("%s:SUCCESS\n",cmd);
}
/* This function is just for debugging purpose.
 * To print the data being sent in a packet
 */
void printPacket()
{
	cout<<"Packet Data"<<endl;
	cout<<packet.noOfUpdates<< " "<< packet.port<<endl;
	cout<<packet.ip<<endl;
	for(int k=0;k<NUMBER_OF_SERVER-1;k++)
	{
		cout<<packet.pInfo[k].peerip<<endl;
		cout<<packet.pInfo[k].peerPort<<" "<<packet.pInfo[k].dummyfield<<endl;
		cout<<packet.pInfo[k].peerID<<" "<<packet.pInfo[k].peerCost<<endl;
	}
}

/* This function runs the "Display" command.
 * It prints the routing table of the local server.
 * This routing table is sorted by Server ID.
 */
void runDisplayCommand(char* cmd)
{
	// run the bellman ford algo
	runBellmanFord();
	cse4589_print_and_log("%s:SUCCESS\n",cmd);
	//print the routing table;
	for(int i=1;i<=NUMBER_OF_SERVER;i++)
	{
		if(i != myID)
		{
		   if(costmatrix[myID][i]>=INFINITY)
		   {
			  int j = -1;
			  cse4589_print_and_log("%-15d%-15d%-15d\n",i,j,costmatrix[myID][i]);
		   }
		   else
		   {
              cse4589_print_and_log("%-15d%-15d%-15d\n",i,rtable[i].next_hop_id,costmatrix[myID][i]);
		   }
		 }
		 else if (i == myID)
		 {
		    cse4589_print_and_log("%-15d%-15d%-15d\n",i,i,costmatrix[i][i]);
		 }
	}
}
/* This function writes a packet in to  dump file when dump command is entered.
 *
 */
void runDumpCommand(char* cmd)
{
	//this printpacket is just for debuffing purpose.
	printPacket();
	int n = cse4589_dump_packet(&packet,sizeof(packet));
	if (n>0)
	{
	   cse4589_print_and_log("%s:SUCCESS\n",cmd);
	}
	else
	{
		cse4589_print_and_log("%s:%s\n",cmd,"Error in creating dump");
	}
}
/* This function parses a received packet and updates the cost matrix accordingly;
 *
 */
void parseRecievedPacket()
{
	uint16_t peerID;
	// get the ID of the machine from which we received this packet
	for(int i=0;i<NUMBER_OF_SERVER;i++)
	{
		if(ntohl(receivedPacket.ip) == ntohl(myServerList[i].ip))
		{
			for(int j=0;j<NUMBER_OF_NEIGHBOR;j++)
			{
				// verify that we are receiving the updates from the neighbors only.
				if(neighborID[j] == myServerList[i].id)
				{
					peerID =  myServerList[i].id;
					cse4589_print_and_log("RECEIVED A MESSAGE FROM SERVER %d\n",peerID);
                    //update the counters for DV packets. This will be used in "packets" command.
                     currentCountOfDVReceived++;
					//set the noOfMissedUpdates to zero
					mytm[peerID].noOfUpdatesMissed = 0;
					//set the boolean firstUpdateReceived to true
				    mytm[peerID].firstupdateReceived = true;
					// update cost matrix i.e. distance vector of this peer, with the received values.
					for(int k=0;k<NUMBER_OF_SERVER;k++)
					{
					   //cout<<"Updating Cost matrix row "<<peerID<<" column"<<ntohs(receivedPacket.pInfo[k].peerID)<< " to "<<ntohs(receivedPacket.pInfo[k].peerCost)<<endl;
				       costmatrix[peerID][ntohs(receivedPacket.pInfo[k].peerID)] = ntohs(receivedPacket.pInfo[k].peerCost);
				       int tempID = ntohs(receivedPacket.pInfo[k].peerID);
				       int tempCost = ntohs(receivedPacket.pInfo[k].peerCost);
				       //cout<<"tempID ="<<tempID<<" TempCost= "<<tempCost<<endl;
				       cse4589_print_and_log("%-15d%-15d\n",tempID,tempCost);
					}
				}
			}

		}
	}

}


/* This function implements Disable Command
 * @param cmd //Actual command given from the main program
 * @param sid //server ID of the neighbor
 */
void runDisableCommand(char* cmd,uint16_t sid)
{
	bool b=false;
	int index;
	for (int i=0;i<NUMBER_OF_NEIGHBOR;i++)
	{
		// check the given sid matches the neighbor list
		if(sid == neighborID[i])
		{
			index=i;
			costmatrix[myID][neighborID[i]] = INFINITY;
			b = true;
			break;
		}
	}
	if(b)
	{
		for(int i=index;i<NUMBER_OF_NEIGHBOR;i++)
		{
			neighborID[i] = neighborID[i+1];
			neighborCost[i] = neighborCost[i+1];
		}
		//reduce the number of neighbors
		NUMBER_OF_NEIGHBOR--;
		// run the bellman ford
		runBellmanFord();
		cout<<"Print CostMatrix"<<endl;
		printCostMatrix();
		cse4589_print_and_log("%s:SUCCESS\n",cmd);
	}
	else
	{
		cse4589_print_and_log("%s:%s\n",cmd,"Invalid Neighbor");
	}

}
/* This function send the periodic updates of Distance vectors to the neighbors.
 * This is called on each timeout when socket hasn't any received updates from any of the neighbors.
 */

void sendPeriodicUpdates()
{
    int k=0;
	//add number or update fields
	packet.noOfUpdates =htons(NUMBER_OF_SERVER);
	//add server port
	packet.port = htons(myPort);
    // fill the server details
	for(int i=0;i<NUMBER_OF_SERVER;i++)
	{
		//get the IP of local machine from list of server's using its ID
		if(myServerList[i].id == myID)
		{
			packet.ip = myServerList[i].ip;
		}
		// fill the details for each server that needs to be used in routing packet.
			packet.pInfo[k].peerip = myServerList[i].ip;
			packet.pInfo[k].peerPort = htons(myServerList[i].port);
			packet.pInfo[k].dummyfield = htons(0);
			packet.pInfo[k].peerID = htons(myServerList[i].id);
			packet.pInfo[k].peerCost = htons(costmatrix[myID][myServerList[i].id]);
			k++;
	}
	printPacket();
	//send packet to all neighbors
	for(int i=0;i<NUMBER_OF_NEIGHBOR;i++)
	{
		for (int j=0;j<NUMBER_OF_SERVER;j++)
		{

			if(neighborID[i] == myServerList[j].id)
			{

				cout<<"Sending packet to neighbor "<<neighborID[i]<<endl;
				sockaddr_in servaddr;
				in_addr addr;
				bzero(&servaddr,sizeof(servaddr));
				addr.s_addr=myServerList[j].ip;
                char* s = inet_ntoa(addr);
                servaddr.sin_family = AF_INET;
				servaddr.sin_addr.s_addr = myServerList[j].ip;
                servaddr.sin_port = htons(myServerList[j].port);
                cout<<"converted IP and Port : "<<s<<" "<<servaddr.sin_port<<endl;
                cout<<"server struct"<<servaddr.sin_addr.s_addr<<servaddr.sin_port<<endl;
				sendto(sock_fd1,&packet,sizeof(packet),0,(struct sockaddr *)&servaddr,sizeof(servaddr));
			}
		}
	}
}
/* This function implements the crash command
 * It just runs the program in infinite loop making it unresponsive for any user input or any distance vector updates.
 *
 */
void runCrashCommand()
{
	while(1)
	{
       //infinite loop
	}
}
void  runAcademicIntegrityCommand(char* cmd)
{
   cse4589_print_and_log("%s:SUCCESS\n",cmd);
   cse4589_print_and_log("I have read and understood the course academic integrity policy located at http://www.cse.buffalo.edu/faculty/dimitrio/courses/cse4589_f14/index.html#integrity");
}
