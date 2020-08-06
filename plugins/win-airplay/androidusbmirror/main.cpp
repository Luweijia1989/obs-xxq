#include "server.h"

int main(int argv, char *argc[])
{
	struct server s;
	server_init(&s);
	struct server_params p;
	memset(&p, 0, sizeof(struct server_params));
	server_start(&s, NULL, &p);
	return 0;
}
