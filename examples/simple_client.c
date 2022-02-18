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
    mnet_sock Client = MNetCreateSocket();
    MNetConnect(&Client, "127.0.0.1", 8000);

    unsigned int Number = 1;
    char Buffer[1024];
    while (Client.State == MNET_SOCK_STATE_CONNECTED)
    {
        MNetUpdate(&Client);

        if (Client.NumberOfEvents == 0)
        {
            continue;
        }

        for (int Index = 0; Index < Client.NumberOfEvents; ++Index)
        {
            mnet_event Event = Client.Events[Index];

            switch(Event.Type)
            {
                case MNET_EVENT_TYPE_CONNECT:
                {
                    printf("Client socket was connected\n");

                    sprintf_s(Buffer, 1024, "%s%d\r\n", "Test Number ", Number++);
                    if (MNetSend(Event.Connection, Buffer, strlen(Buffer)) != 0)
                    {
                        printf("Error queueing send message.\n");
                    }
                } break;

                case MNET_EVENT_TYPE_RECEIVED:
                {
                    printf("Recieved new message from Socket %d: %s\n", (int)Event.Connection->Socket, Event.Connection->Buffer);
                    
                    sprintf_s(Buffer, 1024, "%s%d\r\n", "Test Number ", Number++);
                    if (MNetSend(Event.Connection, Buffer, strlen(Buffer)) != 0)
                    {
                        printf("Error queueing send message.\n");
                    }
                } break;

                case MNET_EVENT_TYPE_SENT:
                {
                    printf("Successfully send entire message to socket %d\n", (int)Event.Connection->Socket);
                    if (MNetRecieve(Event.Connection) != 0)
                    {
                        printf("Error queueing recieve message.\n");
                    }                    
                } break;

                case MNET_EVENT_TYPE_CLOSE:
                {
                    printf("Server closed connection!\n");
                } break;
            }
        }
    }
}