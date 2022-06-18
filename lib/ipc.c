// User-level IPC library routines

#include <inc/lib.h>

// Receive a value via IPC and return it.
// If 'pg' is nonnull, then any page sent by the sender will be mapped at
//	that address.
// If 'from_env_store' is nonnull, then store the IPC sender's envid in
//	*from_env_store.
// If 'perm_store' is nonnull, then store the IPC sender's page permission
//	in *perm_store (this is nonzero iff a page was successfully
//	transferred to 'pg').
// If the system call fails, then store 0 in *fromenv and *perm (if
//	they're nonnull) and return the error.
// Otherwise, return the value sent by the sender


int32_t
ipc_recv(envid_t *from_env_store, void *pg, int *perm_store)
{
	if (pg == NULL)
		pg = (void*) UTOP + 0x1; //address bigger then UTOP means not sending page
	
	int res = sys_ipc_recv(pg);

	if (!res){

		if (from_env_store != NULL)
			*from_env_store = thisenv->env_ipc_from;

		if (perm_store != NULL)
			*perm_store = thisenv->env_ipc_perm;

		return thisenv->env_ipc_value;
	}

	else {
		if (from_env_store != NULL)
			*from_env_store = 0;

		if (perm_store != NULL)
			*perm_store = 0;
		
		return res;
	}

}

// Send 'val' (and 'pg' with 'perm', if 'pg' is nonnull) to 'toenv'.
// This function keeps trying until it succeeds.
// panic() on any error other than -E_IPC_NOT_RECV.


void
ipc_send(envid_t to_env, uint32_t val, void *pg, int perm)
{
	if (pg == NULL)
		pg = (void*) UTOP + 0x1; //address bigger then UTOP means not sending page
	
	int res = sys_ipc_try_send(to_env, val, pg, perm);
	while (res != 0){

		if ((res < 0) && res != -E_IPC_NOT_RECV)// error different then -E_IPC_NOT_RECV
			panic("ipc_send: bad error - not waiting error (thisEnv %x -> toEnv %x) - %e\n", thisenv->env_id ,to_env ,res);


		sys_yield();
		res = sys_ipc_try_send(to_env, val, pg, perm);
	}
}

// Find the first environment of the given type.  We'll use this to
// find special environments.
// Returns 0 if no such environment exists.
envid_t
ipc_find_env(enum EnvType type)
{
	int i;
	for (i = 0; i < NENV; i++)
		if (envs[i].env_type == type)
			return envs[i].env_id;
	return 0;
}
