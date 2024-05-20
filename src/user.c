#include "user.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <debug.h>

struct user
{
	char* handle;
	int referenceCount;
	pthread_mutex_t mutex;
};

USER *user_create(char *handle)
{
	USER *user = malloc(sizeof(USER));
	user->handle = strdup(handle);
	user->referenceCount = 1;
	pthread_mutex_init(&user->mutex, NULL);
	debug("%p\n", user);
	return user;
}

USER *user_ref(USER *user, char *why)
{
	debug("%s\n", why);
	pthread_mutex_lock(&(user->mutex));
	user->referenceCount++;
	pthread_mutex_unlock(&(user->mutex));
	return user;
}

void user_unref(USER *user, char *why)
{
	debug("%s\n", why);
	pthread_mutex_lock(&(user->mutex));
	user->referenceCount--;
	if (user->referenceCount == 0)
	{
		free(user->handle);
		pthread_mutex_unlock(&(user->mutex));
		pthread_mutex_destroy(&(user->mutex));
		free(user);
		return;
	}
	pthread_mutex_unlock(&(user->mutex));
}

char *user_get_handle(USER *user)
{
    return user->handle;
}