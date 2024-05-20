#include "server.h"
#include "globals.h"
#include "client.h"
#include "mailbox.h"
#include "user.h"
#include "user_registry.h"
#include "csapp.h"
#include <string.h>
#include <debug.h>
#include <signal.h>
#include <pthread.h>
#include <time.h>

volatile sig_atomic_t sighup_thread_flag = 0;
int fd;
void sighup_thread_handler(int signum)
{
	sighup_thread_flag = 1;
}

void *chla_client_service(void *arg)
{
	struct sigaction action;
	action.sa_handler = sighup_thread_handler;
	sigemptyset(&action.sa_mask);
	action.sa_flags = 0;
	sigaction(SIGHUP, &action, NULL);

	int connfd = *((int*) arg);
	free(arg);
	fd = connfd;
	debug("CONNECTED\n");
	CLIENT *client = creg_register(client_registry, connfd);
	CHLA_PACKET_HEADER hdr;
	pthread_t tid = -1;
	void *payload = NULL;
	while (sighup_thread_flag == 0)
	{
		if (sighup_thread_flag == 1)
		{
			break;
		}
		if (proto_recv_packet(connfd, &hdr, &payload) < 0)
		{
			break;
		}
		hdr.msgid = ntohl(hdr.msgid);
        //printf("%d\n", hdr.payload_length);
		switch (hdr.type)
		{
		case 1:
			debug("LOGIN\n");
			char* handle = (char*) payload;
			handle[ntohl(hdr.payload_length) - 2] = '\0';
			if (client_login(client, handle) == 0)
			{
				client_send_ack(client, hdr.msgid, NULL, 0);
				pthread_create(&tid, NULL, chla_mailbox_service, client);
			}
			else
			{
				client_send_nack(client, hdr.msgid);
			}
			free(handle);
			break;
		case 2:
			debug("LOGOUT\n");
			if (client_logout(client) == 0)
			{
				client_send_ack(client, hdr.msgid, NULL, 0);
			}
			else
			{
				client_send_nack(client, hdr.msgid);
			}
			break;
		case 3:
			debug("USERS\n");
			CLIENT **allClients = creg_all_clients(client_registry);
			int userCount = 0;
			int i = 0;
			ssize_t size = 0;
			while(allClients[i] != NULL)
			{
				if (client_get_user(allClients[i] , 1) != NULL)
				{
					userCount++;
					size += strlen(user_get_handle(client_get_user(allClients[i] , 1))) + 1;
				}
				i++;
			}
			if (userCount == 0)
			{
				client_send_ack(client, hdr.msgid, NULL, 0);
				break;
			}
			char *allUsers = malloc((size * sizeof(char)) + 1);
			memset(allUsers, 0, (size * sizeof(char)) + 1);
			i = 0;
			ssize_t currentSize = 0;
			while(allClients[i] != NULL)
			{
				if (client_get_user(allClients[i] , 1) != NULL)
				{
					char* userHandle = user_get_handle(client_get_user(allClients[i] , 1));
					sprintf(allUsers + currentSize, "%s\n", userHandle);
					currentSize += strlen(userHandle) + 1;
				}
				client_unref(allClients[i], "Reference discarded");
				i++;
			}
			allUsers[size] = '\0';
			free(allClients);
			client_send_ack(client, hdr.msgid, allUsers, size + 1);

			free(allUsers);
			break;
		case 4:
			debug("SEND\n");
			char* targetHandle = (char*) payload;
			char* newline = strchr(targetHandle, '\n');
			*newline = '\0';

			CLIENT **clientsList = creg_all_clients(client_registry);
			CLIENT *targetClient = NULL;
			int k = 0;
			while(clientsList[k] != NULL)
			{
				if (client_get_user(clientsList[k], 1) != NULL)
				{
					if (strncmp(user_get_handle(client_get_user(clientsList[k], 1)), targetHandle, strlen(targetHandle) - 1) == 0)
					{
						targetClient = clientsList[k];
						break;
					}
				}
				k++;
			}
			if (targetClient == NULL)
			{
				client_send_nack(client, hdr.msgid);
				break;
			}
			*newline = '\n';
			mb_add_message(client_get_mailbox(targetClient, 1), hdr.msgid, client_get_mailbox(client, 1), payload, ntohl(hdr.payload_length));
			client_send_ack(client, hdr.msgid, NULL, 0);

			k = 0;
			while(clientsList[k] != NULL)
			{
				client_unref(clientsList[k], "Reference discarded");
				k++;
			}
			free(clientsList);
			break;
		}
	}
	debug("CLIENT SERVICE RETURNING");
	if (tid != -1)
	{
		pthread_join(tid, NULL);
	}
	debug("MAILBOX SERVICE TERMINATED");
	return NULL;
}

void *chla_mailbox_service(void *arg)
{
	//pthread_detach(pthread_self());
	CLIENT *client = (CLIENT*) arg;
	MAILBOX *mailbox = client_get_mailbox(client, 0);
	client_ref(client, "Client in mailbox service thread");
	struct timespec getTime;
	while (sighup_thread_flag == 0)
	{
		MAILBOX_ENTRY *entry = mb_next_entry(mailbox);
		if (entry != NULL)
		{
			switch (entry->type)
			{
			case 0:
				debug("MESSAGE TYPE");
				clock_gettime(CLOCK_REALTIME, &getTime);

				CHLA_PACKET_HEADER *messageHeader = malloc(sizeof(CHLA_PACKET_HEADER));
				memset(messageHeader, 0, sizeof(CHLA_PACKET_HEADER));
				messageHeader->type = CHLA_MESG_PKT;
				messageHeader->msgid = htonl(entry->content.message.msgid);
				messageHeader->payload_length = htonl(entry->content.message.length);
				messageHeader->timestamp_sec = getTime.tv_sec;
				messageHeader->timestamp_nsec =  getTime.tv_nsec;

				client_send_packet(client, messageHeader, entry->content.message.body);

				mb_add_notice(entry->content.message.from, RRCPT_NOTICE_TYPE, htonl(entry->content.message.msgid));

				free(messageHeader);
				free(entry->content.message.body);
				free(entry);

				break;
			case 1:
				debug("NOTICE TYPE");

				clock_gettime(CLOCK_REALTIME, &getTime);

				CHLA_PACKET_HEADER *receivedHeader = malloc(sizeof(CHLA_PACKET_HEADER));
				memset(receivedHeader, 0, sizeof(CHLA_PACKET_HEADER));
				if (entry->content.notice.type == RRCPT_NOTICE_TYPE)
				{
					receivedHeader->type = CHLA_RCVD_PKT;
				}
				else if (entry->content.notice.type == BOUNCE_NOTICE_TYPE)
				{
					receivedHeader->type = CHLA_BOUNCE_PKT;
				}
				receivedHeader->msgid = entry->content.notice.msgid;
				receivedHeader->payload_length = htonl(0);
				receivedHeader->timestamp_sec = getTime.tv_sec;
				receivedHeader->timestamp_nsec =  getTime.tv_nsec;

				client_send_packet(client, receivedHeader, NULL);
				free(receivedHeader);
				free(entry);
				break;
			}
		}
	}
	client_unref(client, "Client no longer in mailbox thread");
	mb_unref(mailbox, "Mailbox no longer in mailbox thread");
	debug("MAILBOX SERVICE RETURNING");
	return NULL;
}