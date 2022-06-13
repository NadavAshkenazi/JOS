#include <inc/lib.h>
#include <lwip/sockets.h>
#include <lwip/inet.h>

#define PORT 7
#define BUFFSIZE 32
#define IRELEVENT_CHARS 2
#define NAMESIZE 16
#define MAXPENDING 5    // Max connection requests
#define NUM_OF_PARTICIPANTS 2
#define IPC_PAGE_VA ((char *) 0xA0000000)
#define USER_BUFFER_LEN (BUFFSIZE + NAMESIZE + 3)

#define ENTER_KEY_ASCII 10
#define NAME_ERROR 1

int sockets[NUM_OF_PARTICIPANTS] = {-1};

static void
die(char *m)
{
	cprintf("%s\n", m);
	exit();
}

//this function assumes buffer consists of letter only
bool validateString(char* buffer, int wantedSize){
	int i = 0;
	while ((int)buffer[i] != ENTER_KEY_ASCII){
		// printf("char is %c\n", buffer[i]);
		if (i >= BUFFSIZE -1)
			return false;
		i++;
	}
	
	if (i <= wantedSize -1){
		assert((int)buffer[i] == ENTER_KEY_ASCII);
		buffer[i] = '\0';
		return true;

	}

	else{
		return false;
	}
}

void prepareMsg(char* user_message,char* name,char* buffer){
	// memset(user_message, 0, USER_BUFFER_LEN);
	strcpy(user_message, name);
	// printf("user_message(name): %s", user_message);
	strcpy(user_message + strlen(name) , ": ");
	strcpy(user_message + strlen(name) + 2 , buffer);
	strcpy(user_message + strlen(name) + 2 + strlen(buffer), "\n");
	// printf("user_message: %s", user_message);
}

void
handle_client(int sock)
{
	char buffer[BUFFSIZE];
	// sys_page_alloc(thisenv->env_id, IPC_PAGE_VA, PTE_P | PTE_W | PTE_U);
	int received = -1;
	
	// Receive message
	char* name_msg = "What is your name (up to 15 chars)?";
	if ((received = write(sock, name_msg, strlen(name_msg))) < 0)
		die("Failed to send initial bytes from client");
	if ((received = read(sock, buffer, BUFFSIZE)) < 0)
		die("Failed to receive initial bytes from client");
	

	//validate name
	bool validation = validateString(buffer, NAMESIZE);
	if (!validation)
		die("invalid name");

	char name[NAMESIZE];
	memset(name, 0, NAMESIZE);
	strcpy(name, buffer);

	// //ack of login to parent
	// ipc_send(thisenv->env_parent_id, 1, NULL, 0);
	// // printf("%s logged in (size: %d)", name, strlen(name));


	char user_message[USER_BUFFER_LEN];
	// Send bytes and check for more incoming data in loop
	 do {
		// Check for more data
		memset(buffer, 0, BUFFSIZE);
		memset(user_message, 0, USER_BUFFER_LEN);

		if ((received = read(sock, buffer, BUFFSIZE)) < 0)
			die("Failed to receive additional bytes from client");

		bool validation = validateString(buffer, BUFFSIZE);
		if (!validation)
			die("invalid msg");

		prepareMsg(user_message, name, buffer);
		// validation = validateString(buffer, USER_BUFFER_LEN);
		// if (!validation)
		// 	die("invalid users msg");

		int k = 0;
		for (; k < USER_BUFFER_LEN; k++){
			printf("char is: %c -> %d\n", user_message[k], (int)user_message[k]);
		}


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

	cprintf("Waiting for users to loggin...");
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


	
	// int usrCounter = 0;
	// while (usrCounter < NUM_OF_PARTICIPANTS) {
	// 	envid_t env_id;
	// 	usrCounter += ipc_recv(&env_id, IPC_PAGE_VA, 0);
	// }


	// Run until canceled
	while(1){
		envid_t env_id;
		ipc_recv(&env_id, IPC_PAGE_VA, 0);
		assert(strlen(IPC_PAGE_VA) <= USER_BUFFER_LEN);
		assert(*(IPC_PAGE_VA + USER_BUFFER_LEN) == 0);

		// printf("server: msg len %d\n", strlen(IPC_PAGE_VA));

		char buffer[USER_BUFFER_LEN];
		memset(buffer, 0, USER_BUFFER_LEN);
		memcpy(buffer, IPC_PAGE_VA, strlen(IPC_PAGE_VA));
		// printf("server buffer: msg len %d\n", strlen(buffer));
		for(i=0; i<NUM_OF_PARTICIPANTS; i++){
			assert(sockets[i] >= 0);
			// printf("server to socket: msg len %d\n", strlen(buffer));
			// printf("server to socket: msg  %s\n", buffer);
			if (write(sockets[i], buffer, strlen(buffer)) != strlen(buffer))
				die("Failed to send bytes to client");
			}
	}
	
	close(serversock);
}
