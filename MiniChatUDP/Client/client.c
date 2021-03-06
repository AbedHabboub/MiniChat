#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include "wrsock.h"

#define BUF_SIZE 512

typedef enum {
	UNDEFINED = 0,
	CONNECT,
	QUIT,
	WHO
} ClientCommand;

struct conn_data {
	char *name;
	char *host;
	int port;
};

static int volatile _s_connected = 0;
static struct sockaddr_in *_s_server_addr;
static int _s_client_sock = 0;
int len = sizeof (struct sockaddr_in);

static ClientCommand parse_user_command(char *str)
{
	ClientCommand ret = UNDEFINED;
	if (strncmp(str, "_connect", 8) == 0)
	{
		ret = CONNECT;
	}
	else if (strncmp(str, "_quit", 5) == 0)
	{
		ret = QUIT;
	}
	else if (strncmp(str, "_who", 4) == 0)
	{
		ret = WHO;
	}
	return ret;
}

static struct conn_data* get_conn_arguments(char *a_data)
{
	char *arg;
	struct conn_data *conn = malloc(sizeof(struct conn_data));
	conn->host = NULL;
	conn->name = NULL;

#define get_string(value)                           \
    if ((arg = strtok (NULL, " ")) != NULL) {       \
        size_t len = strlen(arg);                   \
        char *param = malloc(len + 1);              \
        strncpy(param, arg, len);                   \
        param[len] = '\0';                          \
        value = param;                         \
    } else {                                        \
        goto error;                                 \
    }

	strtok(a_data, " ");
	get_string(conn->name);
	get_string(conn->host);

	if ((arg = strtok (NULL, " ")) != NULL)
	{
		conn->port = atoi(arg);
	}
	else
	{
		goto error;
	}
	return conn;

	error:
    	free(conn->host);
	free(conn->name);
	free(conn);
	conn = NULL;
	return conn;
}

void send_message(char *message)
{
	size_t length = strlen(message);
	sendto (_s_client_sock, &length, sizeof (length), 0, (struct sockaddr *) _s_server_addr, sizeof (struct sockaddr));
	sendto (_s_client_sock, message, length, 0, (struct sockaddr *) _s_server_addr, sizeof (struct sockaddr));
}

static ClientCommand TraitementClavier (struct conn_data **data, char *msg)
{
	char buf[BUF_SIZE] = {0};
	int taillemessage;
	ClientCommand cmd;
	taillemessage = read(0, buf, BUF_SIZE);
	cmd = parse_user_command(buf);
	*data = NULL;
	if (cmd == CONNECT)
	{
		*data = get_conn_arguments(buf);
	}
	else if (cmd == UNDEFINED)
	{
		sprintf(msg, "%s", buf);
	}
	return cmd;
}

void TraitementSock (int sock)
{
	char buf[BUF_SIZE] = {0}; /* Buffer de reception */
	int taillemessage; /* Taille du message a recevoir
	* Chaque emetteur commence par envoyer la taille du message */
	/* On recoit la taile du message puis le message */
	//recvfrom() is used to receive messages from a socket
	recvfrom (sock, &taillemessage, sizeof (taillemessage), 0, (struct sockaddr *) NULL, NULL);
	recvfrom (sock, buf, taillemessage, 0, (struct sockaddr *) NULL, NULL);
	printf("%s\n", buf);
	if (strcmp(buf, "_connected")==0)
	{
		_s_connected=1;
		printf("Client is connected\n");
		fflush(stdout);
	}
	else if (strcmp(buf, "_kill")==0)
	{
		printf("Connection is closed by server.\n");
		close(_s_client_sock);
		free(_s_server_addr);
		_s_client_sock=0;
		_s_connected=0;
	}
}


void send_conn_data(char *name)
{
	char message[BUF_SIZE] = {0};
	sprintf(message, "_connect %s", name);
	send_message(message);
}

void printline()
{
	printf("\n----------------------\n");
}

int main (int argc, char **argv)
{
	char message [BUF_SIZE];
	fd_set readf;
	struct conn_data *data = NULL;

	printline();
	printf("MiniChat (Client) started.\nAllowed commands:"
			"\n\t1) _connect name address port\t: to connect to the chat server by declaring the client name, server address, and port"
			"\n\t2) _who\t\t\t\t: to display a list of connected clients"
			"\n\t3) _quit\t\t\t: to disconnect the client\n"
			"Other words will be treated as messages to all other connected clients");
	printline();

	while(1)
	{
		FD_SET (0, &readf);
		if (_s_connected)
		{
			FD_SET(_s_client_sock, &readf);
		}
		switch (select (_s_client_sock + 1, &readf, 0, 0, 0)) {
		default:
			if (FD_ISSET (0, &readf))
			{
				switch(TraitementClavier(&data, message))
				{
					case CONNECT:
						if (_s_connected)
						{
							printline();
							printf("You are already connected to the MiniChat (Server)");
							printline();
						}
						else if (!data)
						{
							printline();
							printf("Invalid parameters for _connect. Try _connect <name> <address> <port>");
							printline();
						}
						else
						{
							_s_client_sock = SockUdp(NULL, 0);
							_s_server_addr = CreerSockAddr(data->host, data->port);
							//_s_connected = 1;
							send_conn_data(data->name);
							//printline();
							//printf("You are connected to '%s:%d'", data->host, data->port);
							//printline();
							//fflush(stdout); //flush the output buffer to a stream stdout
							FD_SET(_s_client_sock,&readf);
						}
						if (data)
						{
							free(data->host);
							free(data->name);
							free(data);
						}
						data = NULL;
					break;
					case QUIT:
						if (!_s_connected)
						{
							printline();
							printf("You are already disconnected");
							printline();
							break;
						}
						send_message("_quit");
						printline();
						printf("You have disconnected from the MiniChat (Server)");
						printline();
						close(_s_client_sock);
						free(_s_server_addr);
						_s_client_sock = 0;
						_s_connected = 0;
					break;
					case WHO:
						if (!_s_connected)
						{
							printline();
							printf("You are disconnected. Please connect again");
							printline();
							break;
						}
						send_message("_who");
					break;
					case UNDEFINED:
						if (!_s_connected)
						{
							printline();
							printf("You are disconnected. Please connect again");
							printline();
							break;
						}
						/* just simple message. need to send to server */
						message[strlen(message) - 1] = '\0';
						send_message(message);
					break;
					default:
						printline();
						printf("Unknown command.\nAllowed commands:"
							"\n\t1) _connect name address port\t: to connect to the chat server by declaring the client name, server address, and port"
							"\n\t2) _who\t\t\t\t: to display a list of connected clients"
							"\n\t3) _quit\t\t\t: to disconnect the client\n"
							"Other words will be treated as messages to all other connected clients");
						printline();
					break;
				}
			}
			else if (FD_ISSET (_s_client_sock, &readf))
			{
				TraitementSock(_s_client_sock);
			}
		}
	}
}
