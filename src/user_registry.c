#include "user.h"
#include "user_registry.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <debug.h>

struct user_registry
{
	int numUsers;
	int maxUsers;
	pthread_mutex_t mutex;
	USER **users;
};

USER_REGISTRY *ureg_init(void)
{
	USER_REGISTRY *registry = malloc(sizeof(USER_REGISTRY));
	registry->numUsers = 0;
	registry->maxUsers = 0;
	pthread_mutex_init(&registry->mutex, NULL);
	registry->users = NULL;
	return registry;
}

void ureg_fini(USER_REGISTRY *ureg)
{
	pthread_mutex_lock(&(ureg->mutex));
	for (int i = 0; i < ureg->numUsers; i++)
	{
		user_unref(ureg->users[i], "User registry finializing");
	}
	free(ureg->users);

    pthread_mutex_unlock(&ureg->mutex);
    pthread_mutex_destroy(&ureg->mutex);
    free(ureg);
}

USER *ureg_register(USER_REGISTRY *ureg, char *handle)
{
	pthread_mutex_lock(&ureg->mutex);
	for (int i = 0; i < ureg->numUsers; i++)
	{
		if (strcmp(user_get_handle(ureg->users[i]), handle) == 0)
		{
			user_ref(ureg->users[i], "User found while register");
			pthread_mutex_unlock(&ureg->mutex);
			return ureg->users[i];
		}
	}

	if (ureg->numUsers == ureg->maxUsers)
	{
		ureg->users = realloc(ureg->users, (ureg->numUsers + 10) * sizeof(USER*));
		ureg->maxUsers += 10;
	}

	USER *newUser = user_create(handle);
	user_ref(newUser, "User reference in registry");
    ureg->users[ureg->numUsers] = newUser;
    ureg->numUsers++;
    pthread_mutex_unlock(&ureg->mutex);

    return newUser;
}

void ureg_unregister(USER_REGISTRY *ureg, char *handle)
{
	pthread_mutex_lock(&ureg->mutex);
	for (int i = 0; i < ureg->numUsers; i++)
	{
		if (strcmp(user_get_handle(ureg->users[i]), handle) == 0)
		{
			user_unref(ureg->users[i], "User found while unregister");
			for (int j = i; j < ureg->numUsers - 1; j++)
			{
                ureg->users[j] = ureg->users[j + 1];
            }
    		ureg->numUsers--;
			pthread_mutex_unlock(&ureg->mutex);
			return;
		}
	}
	pthread_mutex_unlock(&ureg->mutex);

}