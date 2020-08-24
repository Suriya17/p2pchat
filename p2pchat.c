#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/select.h> 
#include <sys/socket.h>
#include <netinet/in.h> 
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>

#define BUFSIZE 300
char MYNAME[BUFSIZE]; 
int NUMUSERS;


void bzero(void *s, size_t n);
int inet_aton(const char *cp, struct in_addr *inp);

void error(char *msg) {
	perror(msg);
	exit(1);
}

int max(int a, int b){
	return (a > b) ? a : b;
}

typedef struct{
	char name[20];
	unsigned short int port;
	struct in_addr sin_addr;
}user_info;

/**
 * A node struct for maintaining connections 
 */

typedef struct conn_node{
	char name[20];
	int fd;
	struct conn_node *next;
}conn_node;

/*
 * Search the connection list by name to see if the connection is already present
 * returns the corresponding fd if present
 * Else returns -1
 */

int searchConnection(conn_node **connections, char *name){
	conn_node *searchPtr = *connections;
	while(searchPtr != NULL){
		
		if(!strcmp(name,searchPtr->name))
			return searchPtr->fd;
		searchPtr = searchPtr->next;
	}
	return -1;
}


/**
 * To get the name of the friend from the connection fd
 */

void searchbyfd(conn_node **connections, int fd, char name[]){
	conn_node *searchPtr = *connections;
	while(searchPtr != NULL){
		// printf("Found name %s\n",searchPtr->name);
		if(fd == searchPtr->fd){
			// printf("Found name in if %s\n",searchPtr->name);
			strcpy(name,searchPtr->name);
			return;
		}
		searchPtr = searchPtr->next;
	}
	return ;
}

/**
 * Update the name for a accepted connection 
 */
void updateName(conn_node **connections, char *name, int fd){
	conn_node *searchPtr = *connections;
	while(searchPtr != NULL){
		if(fd == searchPtr->fd){
			strcpy(searchPtr->name,name);
			return;
		}
		searchPtr = searchPtr->next;
	}
}

/*
 * Deletes a connection from the connection linked list
 */

void deleteConnection(conn_node **connections, int fd){
	conn_node *prev = NULL, *temp = *connections;

	if(temp != NULL && temp->fd == fd){
		*connections = temp->next;
		free(temp);
		return;
	}

	while(temp != NULL){
		if(temp->fd == fd){
			prev->next = temp->next;
			free(temp);
			return;
		}
		prev = temp;
		temp = temp->next;
	}
	return;
}

/*
 * Adds a new connection to the list of present connections, makes the new 
 * connection the head of the connection linked list. Used when connection
 * is initiated by us since we already know the name.
 */

void addConnectionMade(conn_node **connections, int fd, char *friendname){
	conn_node *newconn;
	newconn = (conn_node *)malloc(sizeof(conn_node));
	// printf("%d %d\n",ipaddr,portno );

	strcpy(newconn->name,friendname);
	newconn->fd = fd;
	newconn->next = *connections;
	*connections = newconn;
	return;
}

/*
 * Adds a new connection to the list of present connections, makes the new 
 * connection the head of the connection linked list. Used when a connection 
 * is accepted by us since we do not know the name yet. Name is made UNKNOWN
 */
void addConnectionAccepted(conn_node **connections, int fd){
	conn_node *newconn;
	newconn = (conn_node *)malloc(sizeof(conn_node));
	newconn->fd = fd;
	strcpy(newconn->name,"UNKNOWN");
	newconn->next = *connections;
	*connections = newconn;
}

/*
 * Searches the user info table for name
 * Returns the index of the entry
 */

int searchTable(char *name,user_info table[]){
	for(int i = 0; i < NUMUSERS; i++)
		if(!strcmp(name,table[i].name))
			return i;
	return -1;
}

/**
 * Function to print out the present connections
 */
void printConnections(conn_node **conns){
	conn_node *head = *conns;
	while(head != NULL){
		printf("%s - %d\n",head->name,head->fd);
		head = head->next;
	}
}

/*
 * Function to make a dummy friend list
 * Tested over internet and localhost
 * To add more friends increase the NUMUSERS field
 * And add the credentials of the new user here 
 */

void populateUsers(user_info friends[],FILE *f){
	char s_name[20];
    char s_ip[30];
    int  s_port;
    for(int i=0;i<NUMUSERS;i++)
    {
        fscanf(f,"%s %s %d",s_name,s_ip,&s_port);
        strcpy(friends[i].name,s_name);
        int n = inet_aton(s_ip,&(friends[i].sin_addr));
        if(n < 0)
            error("inet");
        friends[i].port = htons(s_port);
    }
	
	// strcpy(friends[0].name,"port1");
	// int n = inet_aton("127.0.0.1",&(friends[0].sin_addr));
	// if(n < 0)
	// 	error("inet");
	// friends[0].port = htons(9999);


	// strcpy(friends[2].name,"port2");
	// n = inet_aton("127.0.0.1",&(friends[2].sin_addr));
	// if(n < 0)
	// 	error("inet");
	// friends[2].port = htons(9998);

	// strcpy(friends[1].name,"server04");
	// n  = inet_aton("10.5.18.104",&(friends[1].sin_addr));
	// if(n < 0)
	// 	error("inet");
	// friends[1].port = htons(9999);
}

void printTable(user_info friends[]){
	for (int i = 0; i < NUMUSERS; ++i)
	{
		printf("%s - %d - %d\n",friends[i].name,friends[i].sin_addr.s_addr,friends[i].port );
	}
}


/**
 * Message should not be longer than BUFSIZE(200 here)
 * Longer messages are not handled since it seemed obselete
 * 
 */

int main(int argc, char const *argv[])
{
	if(argc != 3)
		error("format: ./a.out <portno> <username>");

	fd_set readset, writeset;
	fd_set readsettemp, writesettemp;
	int listenSocket, childsock;
	int optval = 1;
	int fdmax,n,clientlen;
	int portno = atoi(argv[1]);
    
    strcpy(MYNAME,argv[2]);
    strcat(MYNAME,"~");
	char msg[BUFSIZE];
    FILE *f=fopen("users_list.txt","r");
    fscanf(f,"%d",&NUMUSERS);

	char buf[BUFSIZE];
	struct sockaddr_in myaddr;
	struct sockaddr_in newaddr;
	user_info friends[NUMUSERS];
	int numBytes;
	char *friendname, *typedmsg;
	conn_node *myConns = NULL;
	int t,sentBytes;
	char frndnamebuf[20];

	populateUsers(friends,f);
	// printf("%s\n",friends[0].name );
	printTable(friends);

	listenSocket = socket(AF_INET, SOCK_STREAM, 0);
	
	printf("socket listening - %d\n",listenSocket );
	optval = 1;
    setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, 
         (const void *)&optval , sizeof(int));
    
    bzero((char *) &myaddr, sizeof(myaddr));
    
    clientlen = sizeof(newaddr);


    myaddr.sin_family = AF_INET;
    myaddr.sin_addr.s_addr = htonl(INADDR_ANY);

    myaddr.sin_port = htons((unsigned short)portno);


    if (bind(listenSocket, (struct sockaddr *) &myaddr, 
    	sizeof(myaddr)) < 0) 
    error("ERROR on binding");

	if (listen(listenSocket, 10) < 0) 
		error("ERROR on listen");
	printf("Server Running .... on port %d\n",portno);
	
	FD_ZERO(&readset);
	FD_ZERO(&readsettemp);
	FD_SET(STDIN_FILENO,&readset);
	FD_SET(listenSocket,&readset);

	fdmax = listenSocket;

	while(1){
		// printConnections(&myConns);
		readsettemp = readset;
		// writesettemp = writeset;
		// bcopy(&readset,&readsettemp,sizeof(readset));
		// printf("Entered loop\n");
		n = select(fdmax+1,&readsettemp,NULL,NULL,NULL);

		if(n < 0){
			error("select");
		}
		// printf("%d\n",n);

		if(n > 0){
			for(int i = 0; i <= fdmax; i++){
				if(FD_ISSET(i,&readsettemp)){
					if(i == listenSocket){
						bzero((char *) &newaddr, sizeof(newaddr));
						// printf("value of i - %d\n",i);
						// childsock = socket(AF_INET, SOCK_STREAM, 0);
						childsock = accept(listenSocket,(struct sockaddr *)&newaddr,&clientlen);

						if(childsock < 0)
							error("ERROR on ACCEPT");

						fdmax = max(fdmax,childsock);
						FD_SET(childsock,&readset);
						
						// struct sockaddr_in tempsock;
						// int addrlen = sizeof(tempsock);

						// getpeername(childsock,(struct sockaddr*)&tempsock,&addrlen);

						// printf("accepted local address: %s\n", inet_ntoa(tempsock.sin_addr));
						// printf("accepted local port: %d\n", (int) ntohs(tempsock.sin_port));

						addConnectionAccepted(&myConns,childsock);
					}

					else if(i == STDIN_FILENO){
						// printf("STDIN!!!\n");
						bzero(buf,BUFSIZE);
						numBytes = read(i,buf,BUFSIZE);
						friendname = strtok(buf,"/");
						typedmsg = strtok(NULL,"/");
						// printf("%s - %s\n",friendname,typedmsg);
						t = searchConnection(&myConns,friendname);
						if(t > 0){
							// printf("search success\n");
							sentBytes = send(t,typedmsg,strlen(typedmsg),0);
							if(sentBytes < 0)
								error("Sending to friend");
						}
						else{
							// printf("search failed\n");
							bzero((char *) &newaddr, sizeof(newaddr));
							newaddr.sin_family = AF_INET;
							int frndaddr = searchTable(friendname,friends);

							if(frndaddr == -1){
								printf("No such friend!!\n");
								continue;
							}
						    newaddr.sin_addr.s_addr = friends[frndaddr].sin_addr.s_addr;
						    newaddr.sin_port = friends[frndaddr].port;
						    // printf("connected to %d --- %d \n",newaddr.sin_addr.s_addr,newaddr.sin_port);

						    // printf("friend port - %d\n",newaddr.sin_port );
						    childsock = socket(AF_INET, SOCK_STREAM, 0);
						    if(connect(childsock,(const struct sockaddr*)&newaddr,sizeof(newaddr)) < 0){
						    	perror("connecting to friend, may be offline!");
								continue;
							}
						    FD_SET(childsock,&readset);
						    fdmax = max(childsock,fdmax);
						    addConnectionMade(&myConns,childsock,friendname);
							bzero(msg,BUFSIZE);
							strcpy(msg,MYNAME);
							strcat(msg,typedmsg);
						    sentBytes = send(childsock,msg,strlen(msg),0);

							if(sentBytes < 0){
								perror("Sending to friend");
								continue;
							}
						}
					}

					else {
						bzero(buf,BUFSIZE);
						searchbyfd(&myConns,i,frndnamebuf);
						numBytes = read(i,buf,BUFSIZE);

						if(numBytes == 0){
							close(i);
							FD_CLR(i,&readset);
							deleteConnection(&myConns,i);
						}

						else if(!strcmp(frndnamebuf,"UNKNOWN")){
							char *newName = strtok(buf,"~");
							strcpy(frndnamebuf,newName);
							updateName(&myConns,newName,i);
							char *newmsg = strtok(NULL,"~");
							printf("%s : %s",newName,newmsg);
						}
				
						else{
							// printf("Yo Entered\n");
							// strcpy(frndnamebuf,"Yo Entered");
							printf("%s : %s",frndnamebuf,buf);
						}
					}


				}
			}
		}

	}
    fclose(f);
	return 0;
}