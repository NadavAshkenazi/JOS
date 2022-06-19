#include <inc/lib.h>
#include <lwip/sockets.h>
#include <lwip/inet.h>


//chat configs
#define PORT 7
#define BUFFSIZE 128
#define IRELEVENT_CHARS 2
#define NAMESIZE 16
#define MAXPENDING 5    // Max connection requests
#define MAX_NUM_OF_PARTICIPANTS 9
#define IPC_PAGE_VA ((char *) 0xA00000) //non shared va
#define USER_BUFFER_LEN (BUFFSIZE + NAMESIZE + 4)

//keyword defines
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
#define READ 0
#define BAD_USAGE true
#define NONE -1


//Globals
int usersNum = 0;
envid_t chatEnv;
int handler_sockets[MAX_NUM_OF_PARTICIPANTS] = {NO_SOCKET};
envid_t handler_envs[MAX_NUM_OF_PARTICIPANTS] = {NO_ENV};
void _serverSendMessage(const char* const msg);


 /* ==========================================================
						Chat server code
   ========================================================== */


/* Kills all handlers, closes File descriptors and exits with the proper msg */
static void
_server_die(char *m)
{	
	cprintf("%s\n", m);
	cprintf("Destroying chat...\n");
	sys_kill_monitored_envs();
	exit();
}



/* informs reason of exiting and tells server to die */
static void
_handler_die(char *m, bool badUsage)
{	
	if (badUsage)
		cprintf("[%x]BAD USAGE: %s\n",thisenv->env_id,  m);
	else
		cprintf("[%x]USER REQUEST: %s\n",thisenv->env_id,  m);
	cprintf("[%x]SET KILL FLAG = %d\n",thisenv->env_id, sys_kill_flag(KILL));
}



/* validates that a string does not causes buffer overflow. */
bool _validateString(char* buffer, int wantedSize){
	int i = 0;
	while ((int)buffer[i] != ENTER_KEY_ASCII){
		if (i >= BUFFSIZE -1)
			return false;
		i++;
	}
	
	if (i <= wantedSize -1){
		assert((int)buffer[i] == ENTER_KEY_ASCII);
		buffer[i] = '\0';
		return true;
	}
	else
		return false;
	
}



/* sends msg from handler to user terminal */
static inline void _msgUserFromHandler(int sock, char* msg){
	int received = NONE;
	if ((received = write(sock, msg, strlen(msg))) < 0)
		_handler_die("Failed to send initial bytes from client", BAD_USAGE);
}



/* reads name from user and validates no overflow */
static inline bool _readNameAndValidate(int sock, char* buffer, char* name){
	int received = NONE;
	if ((received = read(sock, buffer, BUFFSIZE)) < 0)
		_handler_die("Failed to receive initial bytes from client", BAD_USAGE);

	//validate name
	bool validation = _validateString(buffer, NAMESIZE);

	if (validation){
		memset(name, 0, NAMESIZE);
		strcpy(name, buffer);
	}

	return validation;
}



/* sync with server over all barrieres */
static inline void _handlerSyncAllBarrieres(){

	// all sockets accepted barrier
	while (sys_chat_counter_read(NO_RESET) < usersNum);
		sys_yield();

	sys_chat_counter_inc();

	// server is ready barrier 
	while (sys_chat_counter_read(NO_RESET) < 2 * usersNum);
		sys_yield();
}



/* prepare msg to the correct sending format */
static inline void _prepareMsg(char* user_message,char* name,char* buffer){
	strcpy(user_message, "@");
	strcpy(user_message +1, name);
	strcpy(user_message + strlen(name) +1 , ": ");
	strcpy(user_message + strlen(name) + 3 , buffer);
	strcpy(user_message + strlen(name) + 3 + strlen(buffer), "\n");
}



/* check for more incoming data in a loop and send to server over IPC*/
static inline int _handlerMsgListener(int sock, char* buffer, char* user_message, char* name){
	int received = NONE;
	memset(buffer, 0, BUFFSIZE);
	memset(user_message, 0, USER_BUFFER_LEN);

	if ((received = read(sock, buffer, BUFFSIZE)) < 0)
		_handler_die("Failed to receive additional bytes from client", BAD_USAGE);

	bool validation = _validateString(buffer, BUFFSIZE);
	if (!validation)
		_handler_die("invalid msg", BAD_USAGE);


	if (strcmp(buffer, "##_EXIT_##") == 0){
		_handler_die("USER KILL", !BAD_USAGE);
	}

	_prepareMsg(user_message, name, buffer);
	
	sys_page_alloc(thisenv->env_id, IPC_PAGE_VA, PTE_P | PTE_W | PTE_U);
	memset(IPC_PAGE_VA, 0, PGSIZE);
	memcpy(IPC_PAGE_VA, user_message, strlen(user_message));
	ipc_send(thisenv->env_parent_id, 0, IPC_PAGE_VA, PTE_P | PTE_W | PTE_U);

	return received;
}



/* Listener enviroment function to mannage sending msgs from users.
   Each user receives its own listener. */
void
handle_client(int sock)
{
	char buffer[BUFFSIZE], name[NAMESIZE], user_message[USER_BUFFER_LEN];
	char* msg = NULL;
	int received = NONE;

	//handle name
	msg = "What is your name (up to 15 chars)?\n";
	_msgUserFromHandler(sock, msg);
	int validation = _readNameAndValidate(sock, buffer, name);
	if (validation)
		msg = "You are now logged in, waiting for others to join...\n";
	else
		msg = "Invalid name, wating to die...\n";
	
	_msgUserFromHandler(sock, msg);
	_handlerSyncAllBarrieres();
	
	if (!validation)
		_handler_die("invalid name", BAD_USAGE);

	// handle msgs
	 do {
		received = _handlerMsgListener(sock, buffer, user_message, name);
	} while (received > 0);

	close(sock);
}



/* send SERVER msg to all users */
void _serverSendMessage(const char* const msg){
	char* server_msg = "*SERVER*: "; 
	strcat(server_msg, msg);
	int i;
	for(i = 0; i < usersNum; i++){
		assert(handler_sockets[i] >= 0);
		if (write(handler_sockets[i], server_msg, strlen(server_msg)) != strlen(server_msg)){
			_server_die("Failed to send bytes to client");
		}
	}	
}



/* checks if server needs to die, and if so - kills it! */
void _lifeAssertion(){
	int k;
	for (k = 0; k < 5; ++k) sys_yield(); //delay
	if (sys_kill_flag(READ) == KILL)	
			_server_die("All users logged out");

}



/* kills server if user crashes */
void _checkUserCrashes(){
	int liveEnvs = sys_get_monitored_env_amount();
	if(liveEnvs != usersNum) 
		_server_die("User crash");
}



/* get users num from keyboard */
static inline void _establishNumOfUsers(){

	int k;
	for (k = 0; k < 100; ++k) sys_yield(); //delay

	cprintf("Enter num of users (upto 9):\n");

	char r = sys_cgetc();
	while((r == 0) || (!(r >= '2' && r <= '9'))){
		if (r != 0)
			cprintf("please enter a valid number between 2 and 9\n");
		r = sys_cgetc();
	};

	usersNum = (uint32_t)strtol(&r, 0, 0);
	cprintf("Num of users: %d\n", usersNum);
}


/* set server configuration and connections */
void _configServer(int *serversock, struct sockaddr_in *echoserver){
	// Create the TCP socket
	if ((*serversock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
		_server_die("Failed to create socket");

	cprintf("opened socket\n");

	// Construct the server sockaddr_in structure
	memset(echoserver, 0, sizeof(*echoserver));       // Clear struct
	echoserver->sin_family = AF_INET;                  // Internet/IP
	echoserver->sin_addr.s_addr = htonl(INADDR_ANY);   // IP address
	echoserver->sin_port = htons(PORT);		  // server port

	cprintf("trying to bind\n");

	// Bind the server socket
	if (bind(*serversock, (struct sockaddr *) echoserver, sizeof(*echoserver)) < 0) {
		_server_die("Failed to bind the server socket");
	}

	// Listen on the server socket
	if (listen(*serversock, MAXPENDING) < 0)
		_server_die("Failed to listen on server socket");

	cprintf("bound\n");
	cprintf("Waiting for users to loggin...\n");
}



/* loop until all users are conneceted */
void _acceptClients(int* clientsock, int* serversock, struct sockaddr_in* echoclient){
	//wait until all accepts
	int i = 0;
	for (; i < usersNum; i++) {
		cprintf("Waiting for %d users\n", usersNum -i);
		unsigned int clientlen = sizeof(*echoclient);
		if ((*clientsock = accept(*serversock, (struct sockaddr *) echoclient, &clientlen)) < 0) {
			_server_die("Failed to accept client connection");
		}

		_lifeAssertion();
					
		handler_sockets[i] = *clientsock;
		cprintf("Client %d connected to socket %d: %s\n",i, *clientsock, inet_ntoa(echoclient->sin_addr));

		envid_t pid = monitoredFork();
		if (pid == 0)
			handle_client(*clientsock);
		else{
			cprintf("forked server to %x\n",pid);
			handler_envs[i] = pid;
			sys_chat_counter_inc();
		}
	}

	_lifeAssertion();
	cprintf("All users logged in, awiting to enter chat...\n");
}



/* sync with handlers over all barrieres */
void _serverSyncAllBarrieres(){
		// //wait for every handler to receive a username
	int counter = sys_chat_counter_read(NO_RESET);
	while (counter < 2 * usersNum){
		counter = sys_chat_counter_read(NO_RESET);
		sys_yield();
	}

	_lifeAssertion();

	cprintf("Chat is active.\n");

	char* active_msg = "All users are in, you can start chatting...\n to close chat, one of the users must send ##_EXIT_##.\n";
	_serverSendMessage(active_msg);

}



/* check for more incoming data from handlers over IPC in a loop and send to all other handlers*/
static inline void _serverMsgListener(){

		_lifeAssertion();
		_checkUserCrashes();

		envid_t env_id;
		ipc_recv(&env_id, IPC_PAGE_VA, 0);
		assert((strlen(IPC_PAGE_VA) <= USER_BUFFER_LEN) && (*(IPC_PAGE_VA + USER_BUFFER_LEN) == 0));

		char buffer[USER_BUFFER_LEN];
		memset(buffer, 0, USER_BUFFER_LEN);
		memcpy(buffer, IPC_PAGE_VA, strlen(IPC_PAGE_VA));
		int i;
		for(i = 0; i < usersNum; i++){

			_lifeAssertion();
			assert(handler_sockets[i] >= 0);

			if (handler_envs[i] != env_id){
				if (write(handler_sockets[i], buffer, strlen(buffer)) != strlen(buffer)){
					_server_die("Failed to send bytes to client");
				}
			}
		}	
}



/* main server enviroment function
   receives msgs from listers via IPC and sends them over nic */
void
umain(int argc, char **argv)
{	
	chatEnv = thisenv->env_id;
	int serversock, clientsock;
	struct sockaddr_in echoserver, echoclient;
	char buffer[BUFFSIZE];
	unsigned int echolen;
	int received = 0;

	//configs
	_establishNumOfUsers();
	_configServer(&serversock, &echoserver);

	//connections and synchronization
	sys_chat_counter_read(RESET); // reset barrier
	_acceptClients(&clientsock, &serversock, &echoclient);
	_serverSyncAllBarrieres();

	// Run until canceled with server cmd
	while(1){
		_serverMsgListener();
	}
	
	close(serversock);
}
