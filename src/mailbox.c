#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mailbox.h"
#include <pthread.h>
#include <semaphore.h>
#include <debug.h>

struct mailbox
{
	char *handle;
	MAILBOX_DISCARD_HOOK *hook;
	int referenceCount;
	int online;
	MAILBOX_ENTRY **buffer;
    int front;
    int rear;
    int numEntry;
    pthread_mutex_t mutex;
    sem_t sem;
};

MAILBOX *mb_init(char *handle)
{
	MAILBOX *mb = malloc(sizeof(MAILBOX));
	mb->handle = strdup(handle);
	mb->referenceCount = 1;
	mb->online = 1;
	mb->buffer = malloc(sizeof(MAILBOX_ENTRY*) * 100);
	mb->front = 0;
    mb->rear = -1;
    mb->numEntry = 0;
	pthread_mutex_init(&mb->mutex, NULL);
	sem_init(&mb->sem, 0, 0);
	return mb;
}

void mb_set_discard_hook(MAILBOX *mb, MAILBOX_DISCARD_HOOK *hook)
{
	pthread_mutex_lock(&(mb->mutex));
	mb->hook = hook;
	pthread_mutex_unlock(&(mb->mutex));
}

void discard_hook(MAILBOX_ENTRY *entry)
{
	mb_add_notice(entry->content.message.from, BOUNCE_NOTICE_TYPE, entry->content.message.msgid);
}

void mb_ref(MAILBOX *mb, char *why)
{
	debug("%s", why);
	debug("REFERENCE COUNT:%d %s %d", mb->referenceCount, "to ", mb->referenceCount + 1);
	pthread_mutex_lock(&(mb->mutex));
	mb->referenceCount++;
	pthread_mutex_unlock(&(mb->mutex));
}

void mb_unref(MAILBOX *mb, char *why)
{
	debug("%s", why);
	debug("MAILBOX REFERENCE COUNT:%d %s %d", mb->referenceCount, "to ", mb->referenceCount - 1);
	pthread_mutex_lock(&(mb->mutex));
	mb->referenceCount--;
	if (mb->referenceCount == 0)
	{
		pthread_mutex_unlock(&(mb->mutex));
		pthread_mutex_destroy(&(mb->mutex));
		free(mb->handle);
		free(mb->buffer);
		sem_destroy(&(mb->sem));
		free(mb);
		debug("MAILBOX NO MORE REFERENCE");
		return;
	}
	pthread_mutex_unlock(&(mb->mutex));
}

void mb_shutdown(MAILBOX *mb)
{
	debug("SHUTDOWN");
	pthread_mutex_lock(&(mb->mutex));
	mb->online = 0;
	pthread_mutex_unlock(&(mb->mutex));
	sem_post(&(mb->sem));
}

char *mb_get_handle(MAILBOX *mb)
{
	debug("GET HANDLE");
	return mb->handle;
}

void mb_add_message(MAILBOX *mb, int msgid, MAILBOX *from, void *body, int length)
{
	debug("ADD MESSAGE");
	if (mb->online == 0)
	{
		return;
	}
	pthread_mutex_lock(&(mb->mutex));
	MAILBOX_ENTRY *entry = malloc(sizeof(MAILBOX_ENTRY));
	entry->type = MESSAGE_ENTRY_TYPE;
	entry->content.message.msgid = msgid;
    entry->content.message.from = from;
    entry->content.message.body = body;
    entry->content.message.length = length;

    if (mb != from)
    {
    	mb_ref(from, "Add message");
    }

    mb->rear = (mb->rear + 1) % 100;
    mb->buffer[mb->rear] = entry;
    mb->numEntry++;
    pthread_mutex_unlock(&(mb->mutex));
    sem_post(&(mb->sem));
}

void mb_add_notice(MAILBOX *mb, NOTICE_TYPE ntype, int msgid)
{
	debug("ADD NOTICE");
	if (mb->online == 0)
	{
		return;
	}
	pthread_mutex_lock(&mb->mutex);
	MAILBOX_ENTRY *entry = malloc(sizeof(MAILBOX_ENTRY));
	entry->type = NOTICE_ENTRY_TYPE;
	entry->content.notice.type = ntype;
	entry->content.notice.msgid = msgid;

	mb->rear = (mb->rear + 1) % 100;
    mb->buffer[mb->rear] = entry;
    mb->numEntry++;
    pthread_mutex_unlock(&(mb->mutex));
    sem_post(&(mb->sem));
}

MAILBOX_ENTRY *mb_next_entry(MAILBOX *mb)
{
	debug("NEXT ENTRY");
    sem_wait(&(mb->sem));
    debug("NEXT ENTRY POSTED");
    pthread_mutex_lock(&(mb->mutex));
    MAILBOX_ENTRY *entry = NULL;
    if (mb->online == 0)
    {
    	while (mb->numEntry > 0)
    	{
    		entry = mb->buffer[mb->front];
    		mb->front = (mb->front + 1) % 100;
    		mb->numEntry--;
    		free(entry);
    	}
    	pthread_mutex_unlock(&(mb->mutex));
    	sem_post(&(mb->sem));
    	return NULL;
    }
    else if (mb->numEntry > 0)
    {
        entry = mb->buffer[mb->front];
        mb->front = (mb->front + 1) % 100;
        mb->numEntry--;

        if (entry->type == MESSAGE_ENTRY_TYPE)
        {
        	debug("NEW MESSAGE AVAILABLE");
        	if (entry->content.message.from != NULL)
        	{
        		if (entry->content.message.from != mb)
        		{
        			mb_unref(entry->content.message.from, "Next entry");
        		}
        	}
        	else
        	{
        		discard_hook(entry);
        	}
        }
    }
    pthread_mutex_unlock(&(mb->mutex));
    return entry;
}