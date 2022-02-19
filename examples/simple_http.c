#pragma comment (lib, "user32.lib")
#pragma comment (lib, "ws2_32.lib")

#define MNET_CUSTOM_LOGGING
#include "../mnet.h"

#include "util.c"
#include "../mnet.c"

char *HTML = "<html><body>"
             "<h1>Hello World</h1>"
             "<a href=\"/secret\">Secret Page</a>"
             "</html></body>";
char *SecretHTML = "<html><body>"
                   "<h1>Secret Page</h1>"
                   "<p>You found it, now what?</p>"
                   "</html></body>";
char * HTTPOK = "HTTP/1.1 200 OK\r\n\r\n";

int main()
{
    MNetInitialize();
    mnet_sock Server = MNetCreateSocket();
    MNetListen(&Server, 8000);

    while (Server.State == MNET_SOCK_STATE_LISTENING)
    {
        MNetUpdate(&Server);

        if (Server.NumberOfEvents == 0)
        {
            continue;
        }

        for (int Index = 0; Index < Server.NumberOfEvents; ++Index)
        {
            mnet_event Event = Server.Events[Index];

            switch(Event.Type)
            {
                case MNET_EVENT_TYPE_LISTEN:
                {
                    printf("Server listening at %s:%d \n", Event.Address, Event.Port);
                } break;

                case MNET_EVENT_TYPE_ACCEPT:
                {
                    printf("Accepted new connection with Socket %d\n", (int)Event.Connection->Socket);
                    if (MNetRecieve(Event.Connection) == 0)
                    {
                        printf("Queued recieve message.\n");
                    }
                } break;

                case MNET_EVENT_TYPE_RECEIVED:
                {
                    printf("Recieved new message from Socket %d: %s\n", (int)Event.Connection->Socket, Event.Connection->Buffer);
                    
                    char Path[1024];
                    if (sscanf(Event.Connection->Buffer, "GET %s", Path) == 1)
                    {
                        printf("%s %s\n", Server.Address, Path);

                        MNetSend(Event.Connection, HTTPOK, strlen(HTTPOK));

                        if (!strcmp(Path, "/"))
                        {
                            printf("Sending HTML\n");
                            MNetSend(Event.Connection, HTML, strlen(HTML));
                        }
                        else if (!strcmp(Path, "/secret"))
                        {
                            printf("Sending SecretHTML\n");
                            MNetSend(Event.Connection, SecretHTML, strlen(SecretHTML));
                        }
                        else
                        {
                            printf("Bad Request\n");
                            MNetSend(Event.Connection, "Invalid URL", 11);
                        }
                    }
                    
                    MNetQueueClose(Event.Connection);
                } break;

                case MNET_EVENT_TYPE_SENT:
                {
                    printf("Successfully send entire message to socket %d\n", (int)Event.Connection->Socket);                 
                } break;

                case MNET_EVENT_TYPE_CLOSE:
                {
                    printf("Closing socket %d\n", (int)Event.Connection->Socket);
                } break;
            }
        }
    }
}