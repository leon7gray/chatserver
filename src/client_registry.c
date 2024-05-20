#include "globals.h"
#include "client_registry.h"
#include "client.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <debug.h>
#include "csapp.h"

struct client_registry
{
	CLIENT *clients[MAX_CLIENTS];
	int numClients;
	pthread_mutex_t mutex;
};

CLIENT_REGISTRY *creg_init()
{
	CLIENT_REGISTRY *registry = malloc(sizeof(CLIENT_REGISTRY));
	pthread_mutex_init(&registry->mutex, NULL);
	registry->numClients = 0;
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
        registry->clients[i] = NULL;
    }
	return registry;
}

void creg_fini(CLIENT_REGISTRY *cr)
{
	debug("CREG FINI");
	pthread_mutex_destroy(&(cr->mutex));
	for (int i = 0; i < cr->numClients; i++)
	{
		// TODO
	}
	free(cr);
}

CLIENT *creg_register(CLIENT_REGISTRY *cr, int fd)
{
	debug("CREG REGISTER");
	CLIENT *client = client_create(cr, fd);
	pthread_mutex_lock(&cr->mutex);
	client_ref(client, "Client reference in registry");
    cr->clients[cr->numClients] = client;
    cr->numClients++;
    pthread_mutex_unlock(&cr->mutex);
    return client;
}

int creg_unregister(CLIENT_REGISTRY *cr, CLIENT *client)
{
	pthread_mutex_lock(&cr->mutex);
	debug("CREG UNREGISTER");
	for (int i = 0; i < cr->numClients; i++)
	{
		if (cr->clients[i] == client)
		{
			client_unref(client, "Client unregistered");
			for (int j = i; j < cr->numClients - 1; j++)
			{
                cr->clients[j] = cr->clients[j + 1];
            }
            cr->clients[cr->numClients] = NULL;
            cr->numClients--;
            pthread_mutex_unlock(&cr->mutex);
			return 0;
		}
	}
	pthread_mutex_unlock(&cr->mutex);
	return -1;
}

CLIENT **creg_all_clients(CLIENT_REGISTRY *cr)
{
	debug("CREG ALL CLIENTS");
	CLIENT **clients = malloc((cr->numClients + 1) * sizeof(CLIENT*));
	pthread_mutex_lock(&cr->mutex);
	for (int i = 0; i < cr->numClients; i++)
	{
		clients[i] = cr->clients[i];
		client_ref(cr->clients[i], "Get all clients");
	}
	pthread_mutex_unlock(&cr->mutex);
	clients[cr->numClients] = NULL;
	return clients;
}

void creg_shutdown_all(CLIENT_REGISTRY *cr)
{
	debug("NUM CLIENTS\n");
	debug("%d\n", cr->numClients);
	int clients = cr->numClients;
	for (int i = 0; i < clients; i++)
	{
		CLIENT *client = cr->clients[0];
		//MAILBOX *mailbox = client_get_mailbox(client, 1);
		debug("SHUTDOWN %p\n", client);
		shutdown(client_get_fd(client), SHUT_RDWR);

		debug("LOGOUT\n");
		client_logout(client);
		debug("UNREGISTER\n");
		creg_unregister(cr, client);

		client_unref(client, "Client no longer in client thread");
		debug("FINISH UNREGISTER\n");
	}
	if (cr->numClients == 0)
    {
    	return;
    }
	debug("SHUTDOWN WAITING\n");
	while (1)
	{
        debug("%d\n", cr->numClients);
        if (cr->numClients == 0)
        {
        	break;
        }
    }
    debug("SHUTDOWN FINISH WAITING\n");
}