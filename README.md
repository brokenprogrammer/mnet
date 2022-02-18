# mnet

MNet is an event based networking library built on top of Winsock Overlapped (Asynchronous) I/O.

## Usage

In order to use MNet you use the existing [mnet.h](mnet.h?raw=1) and [mnet.c](mnet.c?raw=1) files. Since it is a windows library you also have to link with `ws2_32`.

Usage examples can be found within the [examples](examples/) folder.

## Server Example

A bare bones TCP echo server that listens to port 5000 and echoes all messages back to clients.

```C
#include "../mnet.h"

#include "util.c"
#include "../mnet.c"

#pragma comment (lib, "ws2_32.lib")

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
                case MNET_EVENT_TYPE_ACCEPT:
                {
                    MNetRecieve(Event.Connection);
                } break;

                case MNET_EVENT_TYPE_RECEIVED:
                {
                    sprintf_s(Buffer, 4096, "%s", Event.Connection->Buffer);
                    MNetSend(Event.Connection, Buffer, strlen(Buffer));
                } break;

                case MNET_EVENT_TYPE_SENT:
                {
                    MNetRecieve(Event.Connection);
                } break;
            }
        }
    }
}
```

## Client Example

A simple client example that connects to a server and sends an incremented test message.

```C
#include "../mnet.h"

#include "util.c"
#include "../mnet.c"

#pragma comment (lib, "ws2_32.lib")

int main()
{
    MNetInitialize();
    mnet_sock Client = MNetCreateSocket();
    MNetConnect(&Client, "127.0.0.1", 5000);

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
                    sprintf_s(Buffer, 1024, "%s%d\r\n", "Test Number ", Number++);
                    MNetSend(Event.Connection, Buffer, strlen(Buffer));
                } break;

                case MNET_EVENT_TYPE_RECEIVED:
                {
                    sprintf_s(Buffer, 1024, "%s%d\r\n", "Test Number ", Number++);
                    MNetSend(Event.Connection, Buffer, strlen(Buffer));
                } break;

                case MNET_EVENT_TYPE_SENT:
                {
                    MNetRecieve(Event.Connection);             
                } break;
            }
        }
    }
}
```

## License

MIT License

Copyright (c) 2022 Oskar Mendel

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
