#include <inc/lib.h>
#include <lwip/sockets.h>
#include <lwip/inet.h>

#define PORT 7

#define BUFFSIZE 32
#define MAXPENDING 5    // Max connection requests
#define NUM_OF_PARTICIPANTS 2
#define IPC_PAGE_VA ((char *) 0xA0000000)
#define USER_BUFFER_LEN(name) (BUFFSIZE + strlen(name) + 2)

int sockets[NUM_OF_PARTICIPANTS] = {-1};

static void
die(char *m)
{
	cprintf("%s\n", m);
	exit();
}

void
handle_client(int sock)
{
	char buffer[BUFFSIZE];
	int received = -1;
	
	// Receive message
	char* name_msg = "What is your name?";
	if ((received = write(sock, name_msg, strlen(name_msg))) < 0)
		die("Failed to receive initial bytes from client");
	if ((received = read(sock, buffer, BUFFSIZE)) < 0)
		die("Failed to receive initial bytes from client");

	char name[BUFFSIZE];
	memset(name, 0, BUFFSIZE);
	strcpy(name, buffer);
	char user_message[USER_BUFFER_LEN(name)];

	// Send bytes and check for more incoming data in loop
	 do {
		// Check for more data
		memset(buffer, 0, BUFFSIZE);
		if ((received = read(sock, buffer, BUFFSIZE)) < 0)
			die("Failed to receive additional bytes from client");

		memset(user_message, 0, USER_BUFFER_LEN(name));
		strcpy(user_message, name);
		printf("user_message(name): %s", user_message);
		strcpy(user_message + strlen(user_message) -1, ": ");
		strcpy(user_message + strlen(user_message) -1, buffer);
		printf("user_message: %s", user_message);
		sys_page_alloc(thisenv->env_id, IPC_PAGE_VA, PTE_P | PTE_W | PTE_U);
		memset(IPC_PAGE_VA, 0, PGSIZE);
		memcpy(IPC_PAGE_VA, user_message, strlen(user_message));
		ipc_send(thisenv->env_parent_id, 0, IPC_PAGE_VA, PTE_P | PTE_W | PTE_U);	

	} while (received > 0);

	close(sock);
}

void
umain(int argc, char **argv)
{
	int serversock, clientsock;
	struct sockaddr_in echoserver, echoclient;
	char buffer[BUFFSIZE];
	unsigned int echolen;
	int received = 0;

	// Create the TCP socket
	if ((serversock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
		die("Failed to create socket");

	cprintf("opened socket\n");

	// Construct the server sockaddr_in structure
	memset(&echoserver, 0, sizeof(echoserver));       // Clear struct
	echoserver.sin_family = AF_INET;                  // Internet/IP
	echoserver.sin_addr.s_addr = htonl(INADDR_ANY);   // IP address
	echoserver.sin_port = htons(PORT);		  // server port

	cprintf("trying to bind\n");

	// Bind the server socket
	if (bind(serversock, (struct sockaddr *) &echoserver,
		 sizeof(echoserver)) < 0) {
		die("Failed to bind the server socket");
	}

	// Listen on the server socket
	if (listen(serversock, MAXPENDING) < 0)
		die("Failed to listen on server socket");

	cprintf("bound\n");

	int i = 0;
	for (; i < NUM_OF_PARTICIPANTS; i++) {
		unsigned int clientlen = sizeof(echoclient);
		// Wait for client connection
		if ((clientsock =
		     accept(serversock, (struct sockaddr *) &echoclient, &clientlen)) < 0) {
			die("Failed to accept client connection");
		}

		sockets[i] = clientsock;
		cprintf("Client %d connected to socket %d: %s\n",i, clientsock, inet_ntoa(echoclient.sin_addr));
		if (fork() == 0)
			handle_client(clientsock);
	}

	// Run until canceled

	while(1){
		envid_t env_id;
		ipc_recv(&env_id, IPC_PAGE_VA, 0);
		char buffer[strlen(IPC_PAGE_VA)];
		memset(buffer, 0, strlen(IPC_PAGE_VA));

		memcpy(buffer, IPC_PAGE_VA, strlen(IPC_PAGE_VA));
		for(i=0; i<NUM_OF_PARTICIPANTS; i++){
			assert(sockets[i] >= 0);
				if (write(sockets[i], buffer, strlen(buffer)) != strlen(buffer))
					die("Failed to send bytes to client");
			}
	}
	
	close(serversock);
}
