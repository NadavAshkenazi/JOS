#include <inc/lib.h>
#include <lwip/sockets.h>
#include <lwip/inet.h>

#define PORT 7
#define BUFFSIZE 32
#define IRELEVENT_CHARS 2
#define NAMESIZE 16
#define MAXPENDING 5    // Max connection requests
#define NUM_OF_PARTICIPANTS 2
#define IPC_PAGE_VA ((char *) 0xA00000)


#define USER_BUFFER_LEN (BUFFSIZE + NAMESIZE + 4)

#define ENTER_KEY_ASCII 10
#define NAME_ERROR 1
#define ACK_RECEIVED_NAME 100
#define ACK_EVERYBODY_JOINED 200
#define ACK_START_CHAT 300
#define RESET 1
#define NO_RESET 0
#define NO_SOCKET -1
#define NO_ENV -1
#define KILL 1
#define NO_SET_KILL 0


envid_t CHAT_ENV;
int handler_sockets[NUM_OF_PARTICIPANTS] = {NO_SOCKET};
envid_t handler_envs[NUM_OF_PARTICIPANTS] = {NO_ENV};

void serverSendMessage(const char* const msg);


static void
server_die(char *m)
{	
	cprintf("%s\n", m);
	cprintf("Destroying chat...\n");
	sys_kill_monitored_envs();
	exit();
}


static void
handler_die(char *m)
{
	cprintf("BAD USAGE: %s\n", m);
	cprintf("SET KILL FLAG = %d\n", sys_kill_flag(KILL));
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
	strcpy(user_message, "@");
	strcpy(user_message +1, name);
	// printf("user_message(name): %s", user_message);
	strcpy(user_message + strlen(name) +1 , ": ");
	strcpy(user_message + strlen(name) + 3 , buffer);
	strcpy(user_message + strlen(name) + 3 + strlen(buffer), "\n");
	// printf("user_message: %s", user_message);
}

void
handle_client(int sock)
{
	char buffer[BUFFSIZE];
	// sys_page_alloc(thisenv->env_id, IPC_PAGE_VA, PTE_P | PTE_W | PTE_U);
	int received = -1;
	
	// Receive message
	char* name_msg = "What is your name (up to 15 chars)?\n";
	if ((received = write(sock, name_msg, strlen(name_msg))) < 0)
		handler_die("Failed to send initial bytes from client");
	if ((received = read(sock, buffer, BUFFSIZE)) < 0)
		handler_die("Failed to receive initial bytes from client");
	

	//validate name
	bool validation = validateString(buffer, NAMESIZE);
	if (!validation)
		handler_die("invalid name");

	char name[NAMESIZE];
	memset(name, 0, NAMESIZE);
	strcpy(name, buffer);

	name_msg = "You are now logged in, waiting for others to join...\n";
	if ((received = write(sock, name_msg, strlen(name_msg))) < 0)
		handler_die("Failed to send initial bytes from client");

	while (sys_chat_counter_read(NO_RESET) < NUM_OF_PARTICIPANTS);
		sys_yield();
	// cprintf("user %d dec.\n", sock);

	sys_chat_counter_inc();

	while (sys_chat_counter_read(NO_RESET) < 2 * NUM_OF_PARTICIPANTS);
		sys_yield();

	
	char user_message[USER_BUFFER_LEN];
	// Send bytes and check for more incoming data in loop
	 do {
		// Check for more data
		memset(buffer, 0, BUFFSIZE);
		memset(user_message, 0, USER_BUFFER_LEN);

		if ((received = read(sock, buffer, BUFFSIZE)) < 0)
			handler_die("Failed to receive additional bytes from client");

		bool validation = validateString(buffer, BUFFSIZE);
		if (!validation)
			handler_die("invalid msg");

		prepareMsg(user_message, name, buffer);
		// validation = validateString(buffer, USER_BUFFER_LEN);
		// if (!validation)
		// 	die("invalid users msg");

		// int k = 0;
		// for (; k < USER_BUFFER_LEN; k++){
		// 	printf("char is: %c -> %d\n", user_message[k], (int)user_message[k]);
		// }


		sys_page_alloc(thisenv->env_id, IPC_PAGE_VA, PTE_P | PTE_W | PTE_U);
		memset(IPC_PAGE_VA, 0, PGSIZE);
		memcpy(IPC_PAGE_VA, user_message, strlen(user_message));
		ipc_send(thisenv->env_parent_id, 0, IPC_PAGE_VA, PTE_P | PTE_W | PTE_U);	

	} while (received > 0);

	close(sock);
}

void serverSendMessage(const char* const msg){
	char* server_msg = "*SERVER*: "; 
	strcat(server_msg, msg);
	int i;
	for(i =0; i<NUM_OF_PARTICIPANTS; i++){
		assert(handler_sockets[i] >= 0);
		if (write(handler_sockets[i], server_msg, strlen(server_msg)) != strlen(server_msg)){
			server_die("Failed to send bytes to client");
		}
	}	
}

void
umain(int argc, char **argv)
{	
	CHAT_ENV = thisenv->env_id;
	int serversock, clientsock;
	struct sockaddr_in echoserver, echoclient;
	char buffer[BUFFSIZE];
	unsigned int echolen;
	int received = 0;

	// Create the TCP socket
	if ((serversock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
		server_die("Failed to create socket");

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
		server_die("Failed to bind the server socket");
	}

	// Listen on the server socket
	if (listen(serversock, MAXPENDING) < 0)
		server_die("Failed to listen on server socket");

	cprintf("bound\n");

	cprintf("Waiting for users to loggin...\n");

	sys_chat_counter_read(RESET);
	//wait until all accepts
	int i = 0;
	for (; i < NUM_OF_PARTICIPANTS; i++) {
		cprintf("Waiting for %d users\n", NUM_OF_PARTICIPANTS -i);
		unsigned int clientlen = sizeof(echoclient);
		if ((clientsock = accept(serversock, (struct sockaddr *) &echoclient, &clientlen)) < 0) {
			server_die("Failed to accept client connection");
		}

		handler_sockets[i] = clientsock;
		cprintf("Client %d connected to socket %d: %s\n",i, clientsock, inet_ntoa(echoclient.sin_addr));
		envid_t pid = monitoredFork();
		if (pid == 0)
			handle_client(clientsock);
		else{
			cprintf("forked server to %x\n",pid);
			handler_envs[i] = pid;
			sys_chat_counter_inc();
		}
	}

	cprintf("All users logged in, awiting to enter chat...\n");
	// //send ok to handlers
	// for (i = 0; i < NUM_OF_PARTICIPANTS; i++)
	// 	ipc_send(childern_envs[i], ACK_EVERYBODY_JOINED, NULL, 0);
	

	// cprintf("waiting for every handler to receive a username...\n");
	// //wait for every handler to receive a username
	int counter = sys_chat_counter_read(NO_RESET);
	// cprintf("counter: %d\n", counter);
	while (counter < 2 * NUM_OF_PARTICIPANTS){
		counter = sys_chat_counter_read(NO_RESET);
		// cprintf("counter: %d\n", counter);
		sys_yield();
	}

	cprintf("Chat is active.\n");
	char* active_msg = "All users are in, you can start chatting...\n";
	serverSendMessage(active_msg);
	// Run until canceled
	while(1){

		if (sys_kill_flag(NO_SET_KILL) == KILL)
			server_die("All users logged out");

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
			if (sys_kill_flag(NO_SET_KILL) == KILL)
				server_die("All users logged out");

			assert(handler_sockets[i] >= 0);
			// printf("server to socket: msg len %d\n", strlen(buffer));
			// printf("server to socket: msg  %s\n", buffer);
			if (handler_envs[i] != env_id){
				if (write(handler_sockets[i], buffer, strlen(buffer)) != strlen(buffer)){
					server_die("Failed to send bytes to client");
				}
			}
		}	
	}
	
	close(serversock);
}
