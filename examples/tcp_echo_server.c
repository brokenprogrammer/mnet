#pragma comment (lib, "gdi32.lib")
#pragma comment (lib, "user32.lib")
#pragma comment (lib, "dxguid.lib")
#pragma comment (lib, "d3d11.lib")
#pragma comment (lib, "d3dcompiler.lib")
#pragma comment (lib, "ws2_32.lib")

#define MNET_CUSTOM_LOGGING
#include "../mnet.h"

#include "util.c"
#include "../mnet.c"

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

        char Buffer[4096];
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
                    
                    sprintf_s(Buffer, 4096, "%s", Event.Connection->Buffer);
                    if (MNetSend(Event.Connection, Buffer, strlen(Buffer)) == 0)
                    {
                        printf("Queued send message.\n");
                    }
                } break;

                case MNET_EVENT_TYPE_SENT:
                {
                    printf("Successfully send entire message to socket %d\n", (int)Event.Connection->Socket);

                    // NOTE(Oskar): Queue Recieve
                    if (MNetRecieve(Event.Connection) == 0)
                    {
                        printf("Queued recieve message.\n");
                    }                    
                } break;

                case MNET_EVENT_TYPE_CLOSE:
                {
                    printf("Closing socket %d\n", (int)Event.Connection->Socket);
                } break;
            }
        }
    }
}