#include "protocol.h"
#include "user.h"
#include "mailbox.h"
#include "client_registry.h"
#include "client.h"
#include "globals.h"
#include <string.h>
#include <pthread.h>
#include "debug.h"
#include <time.h>
#include <semaphore.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

struct client_registry
{
	CLIENT *clients[MAX_CLIENTS];
	int numClients;
	pthread_mutex_t mutex;
	sem_t sem;
};

struct client
{
	int fd;
	int referenceCount;
	USER *user;
	MAILBOX *mailbox;
	pthread_mutex_t mutex;
};

CLIENT *client_create(CLIENT_REGISTRY *creg, int fd)
{
	debug("CLIENT CREATE");
	CLIENT *client = malloc(sizeof(CLIENT));
	client->fd = fd;
	client->referenceCount = 1;
	client->user = NULL;
	client->mailbox = NULL;
	pthread_mutex_init(&client->mutex, NULL);
	return client;
}

CLIENT *client_ref(CLIENT *client, char *why)
{
	debug("%s", why);
	debug("REFERENCE COUNT:%d %s %d", client->referenceCount, "to ", client->referenceCount + 1);
	pthread_mutex_lock(&(client->mutex));
	client->referenceCount++;
	pthread_mutex_unlock(&(client->mutex));
	return client;
}

void client_unref(CLIENT *client, char *why)
{
	debug("%s", why);
	debug("REFERENCE COUNT:%d %s %d", client->referenceCount, "to ", client->referenceCount - 1);
	pthread_mutex_lock(&(client->mutex));
	client->referenceCount--;
	if (client->referenceCount == 0)
	{
		pthread_mutex_unlock(&(client->mutex));
		pthread_mutex_destroy(&(client->mutex));
		free(client);
		return;
	}
	pthread_mutex_unlock(&(client->mutex));
}

int client_login(CLIENT *client, char *handle)
{
	debug("LOGIN");
	if (client->user != NULL)
	{
		return -1;
	}
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (client_registry->clients[i] != NULL)
		{
			if (client_registry->clients[i]->user != NULL)
			{
				if (strcmp(handle, user_get_handle(client_registry->clients[i]->user)) == 0)
				{
					return -1;
				}
			}
		}
	}
	pthread_mutex_lock(&(client->mutex));
	client->user = ureg_register(user_registry, handle);
	client->mailbox = mb_init(handle);
	pthread_mutex_unlock(&(client->mutex));
	return 0;
}

int client_logout(CLIENT *client)
{
	debug("LOGOUT");
	if (client->user == NULL)
	{
		return -1;
	}
	mb_shutdown(client->mailbox);
	mb_unref(client->mailbox, "Client logging out");
	user_unref(client->user, "Client logging out");
	pthread_mutex_lock(&(client->mutex));
    client->user = NULL;
    client->mailbox = NULL;
    pthread_mutex_unlock(&(client->mutex));
    return 0;
}

USER *client_get_user(CLIENT *client, int no_ref)
{
	debug("GET USER");
	if (client->user == NULL)
	{
		return NULL;
	}
	if (no_ref == 0)
	{
		user_ref(client->user, "Client get user no_ref = 0");
	}
	return client->user;
}

MAILBOX *client_get_mailbox(CLIENT *client, int no_ref)
{
	debug("GET MAILBOX");
	if (client->user == NULL)
	{
		return NULL;
	}
	if (no_ref == 0)
	{
		mb_ref(client->mailbox, "Client get mailbox no_ref = 0");
	}
	return client->mailbox;
}

int client_get_fd(CLIENT *client)
{
	debug("GET FD");
	return client->fd;
}

int client_send_packet(CLIENT *user, CHLA_PACKET_HEADER *pkt, void *data)
{
	debug("SENDING PACKET--------------");
	pthread_mutex_lock(&user->mutex);
	if (proto_send_packet(user->fd, pkt, data) == -1)
	{
		pthread_mutex_unlock(&user->mutex);
		return -1;
	}
	pthread_mutex_unlock(&user->mutex);
	return 0;
}

int client_send_ack(CLIENT *client, uint32_t msgid, void *data, size_t datalen)
{
	struct timespec currentTime;
    clock_gettime(CLOCK_REALTIME, &currentTime);

	CHLA_PACKET_HEADER *header = malloc(sizeof(CHLA_PACKET_HEADER));
	memset(header, 0, sizeof(CHLA_PACKET_HEADER));
    header->type = CHLA_ACK_PKT;
    header->msgid = htonl(msgid);
    header->payload_length = htonl(datalen);
    header->timestamp_sec = currentTime.tv_sec;
    header->timestamp_nsec =  currentTime.tv_nsec;

	debug("SENDING ACK------------------");
	pthread_mutex_lock(&client->mutex);
	if (proto_send_packet(client->fd, header, data) == -1)
	{
		pthread_mutex_unlock(&client->mutex);
		free(header);
		return -1;
	}
	pthread_mutex_unlock(&client->mutex);
	free(header);
	return 0;
}


int client_send_nack(CLIENT *client, uint32_t msgid)
{
	struct timespec currentTime;
    clock_gettime(CLOCK_REALTIME, &currentTime);

	CHLA_PACKET_HEADER *header = malloc(sizeof(CHLA_PACKET_HEADER));
	memset(header, 0, sizeof(CHLA_PACKET_HEADER));
    header->type = CHLA_NACK_PKT;
    header->msgid = htonl(msgid);
    header->payload_length = htonl(0);
    header->timestamp_sec = currentTime.tv_sec;
    header->timestamp_nsec =  currentTime.tv_nsec;

	debug("SENDING NACK------------------");
	pthread_mutex_lock(&client->mutex);
	if (proto_send_packet(client->fd, header, NULL) == -1)
	{
		pthread_mutex_unlock(&client->mutex);
		free(header);
		return -1;
	}
	pthread_mutex_unlock(&client->mutex);
	free(header);
	return 0;
}