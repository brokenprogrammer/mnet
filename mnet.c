#ifndef MNET_CUSTOM_LOGGING
#define Win32OutputWSAErrorCode printf
#define Win32OutputErrorCode printf
#endif

#define MNET_ALLOC(X) HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, X)
#define MNET_FREE(X) if (X != NULL) { HeapFree(GetProcessHeap(), 0, X); X = NULL; }

mnet_event 
_MNetCreateEvent(mnet_event_type Type)
{
    mnet_event Event = {0};
    Event.Type = Type;
    return Event;
}

mnet_event 
_MNetCreateListenEvent(mnet_sock *Socket)
{
    mnet_event Event = _MNetCreateEvent(MNET_EVENT_TYPE_LISTEN);
    Event.Port    = Socket->Port;
    Event.Address = Socket->Address;
    return Event;
}

mnet_event
_MNetCreateConnectEvent(mnet_client_sock *Connection)
{
    mnet_event Event = _MNetCreateEvent(MNET_EVENT_TYPE_CONNECT);
    Event.Connection = Connection;
    return Event;
}

mnet_event
_MNetCreateTimeoutEvent()
{
    mnet_event Event = _MNetCreateEvent(MNET_EVENT_TYPE_TIMEOUT);
    return Event;
}

mnet_event 
_MNetCreateAcceptEvent(mnet_client_sock *Connection)
{
    mnet_event Event = _MNetCreateEvent(MNET_EVENT_TYPE_ACCEPT);
    Event.Connection = Connection;
    return Event;
}

mnet_event 
_MNetCreateReceivedEvent(mnet_client_sock *Connection)
{
    mnet_event Event = _MNetCreateEvent(MNET_EVENT_TYPE_RECEIVED);
    Event.Connection = Connection;
    return Event;
}

mnet_event 
_MNetCreateSentEvent(mnet_client_sock *Connection)
{
    mnet_event Event = _MNetCreateEvent(MNET_EVENT_TYPE_SENT);
    Event.Connection = Connection;
    return Event;
}

// NOTE(Oskar): Perhaps this shouldnt take a connection.
mnet_event 
_MNetCreateCloseEvent(mnet_client_sock *Connection)
{
    mnet_event Event = _MNetCreateEvent(MNET_EVENT_TYPE_CLOSE);
    Event.Connection = Connection;
    return Event;
}

int
MNetInitialize()
{
    WSADATA WSAData;
    if (WSAStartup(MAKEWORD(2, 2), &WSAData) != 0)
    {
        Win32OutputWSAErrorCode("WSAStartup Failed");
        WSACleanup();
        return -1;
    }

    return 0;
}

SOCKET
_MNetCreateSocket(int AddressFamily, int SocketType, int Protocol)
{
    SOCKET Socket = WSASocketW(AddressFamily, 
                               SocketType, 
                               Protocol,
                               NULL, 0, WSA_FLAG_OVERLAPPED);

    if (Socket == INVALID_SOCKET)
    {
        Win32OutputWSAErrorCode("WSASocketW Failed, unable to create new socket.");
        WSACleanup();
        return SOCKET_ERROR;
    }

    return Socket;
}

void
_MNetInitializeAddressMetadata(mnet_sock *Socket)
{
    union
    {
        struct sockaddr         SockAddr;
        struct sockaddr_storage SockAddrStorage;
        struct sockaddr_in      SockAddrIn;
        struct sockaddr_in6     SockAddrIn6;
    } Address;
    ZeroMemory(&Address, sizeof(Address));


    socklen_t AddressSize = sizeof(Address);
    Socket->Address = NULL;

    if (Socket->Type == MNET_SOCK_TYPE_SERVER)
    {
        if (getpeername(Socket->Server->ListenSocket, &Address.SockAddr, &AddressSize) == -1)
        {
            if (getsockname(Socket->Server->ListenSocket, &Address.SockAddr, &AddressSize) == -1)
            {
                return ;
            }
        }
    }
    else if (Socket->Type == MNET_SOCK_TYPE_CLIENT)
    {
        if (getpeername(Socket->Client->Socket, &Address.SockAddr, &AddressSize) == -1)
        {
            if (getsockname(Socket->Client->Socket, &Address.SockAddr, &AddressSize) == -1)
            {
                return ;
            }
        }
    }

    if (Address.SockAddrStorage.ss_family == AF_INET6)
    {
        Socket->Address = (char *)MNET_ALLOC(INET6_ADDRSTRLEN);
        inet_ntop(AF_INET6, &Address.SockAddrIn6.sin6_addr, Socket->Address,
                  INET6_ADDRSTRLEN);
        Socket->Port = ntohs(Address.SockAddrIn6.sin6_port);
    }
    else
    {
        Socket->Address = (char *)MNET_ALLOC(INET_ADDRSTRLEN);
        inet_ntop(AF_INET, &Address.SockAddrIn.sin_addr, Socket->Address,
                  INET6_ADDRSTRLEN);
        Socket->Port = ntohs(Address.SockAddrIn.sin_port);
    }
}

mnet_sock 
MNetCreateSocket()
{
    mnet_sock Socket = {0};
    
    Socket.Type = MNET_SOCK_TYPE_INVALID;
    Socket.State = MNET_SOCK_STATE_CLOSED;
    Socket.TimeoutMS = 500;

    Socket.NumberOfEvents = 0;

    return Socket;
}

int
MNetListen(mnet_sock *Socket, int Port)
{
    Socket->Type = MNET_SOCK_TYPE_SERVER;
    Socket->TimeoutMS = 25;
    Socket->Server = (mnet_server_sock *)MNET_ALLOC(sizeof(mnet_server_sock));
    mnet_server_sock *Server = Socket->Server;

    struct addrinfo *AddressInfo;
    struct addrinfo Hints;
    ZeroMemory(&Hints, sizeof(Hints));
    Hints.ai_family = AF_INET;
    Hints.ai_socktype = SOCK_STREAM;
    Hints.ai_flags = AI_PASSIVE;

    char PortBuffer[64];
    sprintf_s(PortBuffer, 64, "%d", Port);

    if (getaddrinfo(NULL, PortBuffer, &Hints, &AddressInfo) != 0)
    {
        Win32OutputWSAErrorCode("getaddrinfo Failed");
        WSACleanup();
        return -1;
    }

    Server->ListenSocket = _MNetCreateSocket(AddressInfo->ai_family, 
                                             AddressInfo->ai_socktype, 
                                             AddressInfo->ai_protocol);
    if (Server->ListenSocket == SOCKET_ERROR)
    {
        return -1;
    }

    if (bind(Server->ListenSocket, AddressInfo->ai_addr, (int)AddressInfo->ai_addrlen) == SOCKET_ERROR)
    {
        Win32OutputWSAErrorCode("Failed to bind listening socket.");
        WSACleanup();
        return -1;
    }

    // NOTE(Oskar): Listen for incoming requests.
    // TODO(Oskar): Make backlog a parameter
    if (listen(Server->ListenSocket, MNET_SOCK_DEFAULT_BACKLOG) == SOCKET_ERROR)
    {
        Win32OutputWSAErrorCode("Failed to initiate listening on socket.");
        WSACleanup();
        return -1;
    }

    // NOTE(Oskar): Load the AcceptEx extension function
    GUID AcceptExGuid = WSAID_ACCEPTEX;
    DWORD Bytes = 0;
    int LoadStatus = WSAIoctl(Server->ListenSocket, SIO_GET_EXTENSION_FUNCTION_POINTER,
                              &AcceptExGuid, sizeof(AcceptExGuid),
                              &Server->AcceptEx, sizeof(Server->AcceptEx),
                              &Bytes, NULL, NULL);
    if (LoadStatus == SOCKET_ERROR)
    {
        Win32OutputWSAErrorCode("Failed to load AcceptEx.");
        WSACleanup();
        return -1;
    }

    Socket->State = MNET_SOCK_STATE_LISTENING;
    Socket->Port = Port;

    _MNetInitializeAddressMetadata(Socket);
    Socket->Events[Socket->NumberOfEvents++] = _MNetCreateListenEvent(Socket);

    // NOTE(Oskar): This is set to invalid now so that we can perform additional initialize
    // when performing the first update.
    Server->LastAcceptSocket = INVALID_SOCKET;

    // TODO(Oskar): Free this on error as well.
    freeaddrinfo(AddressInfo);

    return 0;
}

int
MNetConnect(mnet_sock *Socket, char *Address, int Port)
{
    Socket->Type = MNET_SOCK_TYPE_CLIENT;
    Socket->TimeoutMS = 25;
    Socket->Client = (mnet_client_sock *)MNET_ALLOC(sizeof(mnet_client_sock));
    mnet_client_sock *Client = Socket->Client;
    Client->QueuedForClose = 0;
    Socket->Initialized = 0;

    Client->Socket = _MNetCreateSocket(AF_INET, SOCK_STREAM, 0);
    if (Client->Socket == SOCKET_ERROR)
    {
        return -1;
    }

    // NOTE(Oskar): ConnectEx requires the socket to be bound.
    {
        struct sockaddr_in BindAddress;
        ZeroMemory(&BindAddress, sizeof(BindAddress));
        BindAddress.sin_family      = AF_INET;
        BindAddress.sin_addr.s_addr = INADDR_ANY;
        BindAddress.sin_port        = 0;
        if (bind(Client->Socket, (SOCKADDR *)&BindAddress, sizeof(BindAddress)) == SOCKET_ERROR)
        {
            Win32OutputWSAErrorCode("Failed to client socket.");
            WSACleanup();
            return -1;
        }
    }

    GUID ConnectExGuid = WSAID_CONNECTEX;
    DWORD Bytes = 0;
    int LoadStatus = WSAIoctl(Client->Socket, SIO_GET_EXTENSION_FUNCTION_POINTER,
                              &ConnectExGuid, sizeof(ConnectExGuid),
                              &Socket->ConnectEx, sizeof(Socket->ConnectEx),
                              &Bytes, NULL, NULL);
    if (LoadStatus == SOCKET_ERROR)
    {
        Win32OutputWSAErrorCode("Failed to load ConnectEx.");
        WSACleanup();
        return -1;
    }

    Socket->State = MNET_SOCK_STATE_CONNECTED;
    _MNetInitializeAddressMetadata(Socket);

    // NOTE(Oskar): Queue ConnectEx
    ZeroMemory(&Client->Overlapped, sizeof(Client->Overlapped));

    struct sockaddr_in ConnectAddress;
    ZeroMemory(&ConnectAddress, sizeof(ConnectAddress));
    ConnectAddress.sin_family      = AF_INET;
    InetPton(AF_INET, Address, &ConnectAddress.sin_addr.s_addr);
    ConnectAddress.sin_port        = htons(Port);

    if (Socket->ConnectEx(Client->Socket, 
                          (SOCKADDR *)&ConnectAddress, sizeof(ConnectAddress),
                          NULL, 0, NULL, &Client->Overlapped) == FALSE)
    {
        if (WSAGetLastError() != ERROR_IO_PENDING)
        {
            Win32OutputWSAErrorCode("Initial ConnectEx failed.");
            return -1;
        }
    }
    else
    {
        int ReturnCode = setsockopt(Client->Socket, SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, NULL, 0);
        if (ReturnCode != 0) 
        {
            Win32OutputWSAErrorCode("Set socket options failed.");
            return -1;
        }

        // NOTE(Oskar): Connected immediately
        Socket->Events[Socket->NumberOfEvents++] = _MNetCreateConnectEvent(Client);
        Client->Operation = MNET_QUEUED_OPERATION_NONE;
    }

    Client->Operation = MNET_QUEUED_OPERATION_CONNECT;

    return 0;
}

void
MNetClose(mnet_sock *Socket)
{
    if (Socket->Type == MNET_SOCK_TYPE_SERVER)
    {
        mnet_server_sock *Server = Socket->Server;
        for (DWORD Index = 1; Index < Server->NumberOfEvents; ++Index)
        {
            mnet_client_sock *Connection = Server->Connections[Index];
            if(INVALID_SOCKET != Connection->Socket) 
            {
                closesocket(Connection->Socket); 
                Connection->Socket = INVALID_SOCKET;
            }
            MNET_FREE(Connection);
            WSACloseEvent(Server->EventArray[Index]);
        }

        closesocket(Server->ListenSocket);
        WSACloseEvent(Server->EventArray[0]);
        MNET_FREE(Socket->Address);
    }
    else if (Socket->Type == MNET_SOCK_TYPE_CLIENT)
    {
        mnet_client_sock *Client = Socket->Client;
        if(INVALID_SOCKET != Client->Socket) 
        {
            closesocket(Client->Socket); 
            Client->Socket = INVALID_SOCKET;
        }

        MNET_FREE(Socket->Address);
        MNET_FREE(Client);
    }
    else
    {
        // NOTE(Oskar): Error or silently do nothing?
    }
}

int 
MNetRecieve(mnet_client_sock *Target)
{
    assert(Target->Operation == MNET_QUEUED_OPERATION_NONE);
    
    Target->Operation = MNET_QUEUED_OPERATION_READ;
    Target->BytesReceived = 0;

    ZeroMemory(Target->Buffer, MNET_SOCK_BUFFER_SIZE);
    Target->DataBuf.len = MNET_SOCK_BUFFER_SIZE;
    Target->DataBuf.buf = Target->Buffer;

    DWORD Flags = 0;
    if (WSARecv(Target->Socket, &Target->DataBuf, 1, 
                NULL, &Flags, &(Target->Overlapped), NULL) == SOCKET_ERROR)
    {
        if (WSAGetLastError() != ERROR_IO_PENDING)
        {
            Target->Operation = MNET_QUEUED_OPERATION_CLOSE;
            Win32OutputWSAErrorCode("WSARecv Failed.");
            return -1;
        }
    }

    return 0;
}

int 
_MNetSendAdditional(mnet_client_sock *Target)
{
    Target->DataBuf.buf = Target->Buffer + Target->BytesSent;
    Target->DataBuf.len = Target->BytesToSend - Target->BytesSent;

    if (WSASend(Target->Socket, &(Target->DataBuf), 1, 
                NULL, 0, &(Target->Overlapped), NULL) == SOCKET_ERROR)
    {
        if (WSAGetLastError() != ERROR_IO_PENDING)
        {
            Target->Operation = MNET_QUEUED_OPERATION_CLOSE;
            Win32OutputWSAErrorCode("WSASend Failed.");
            return -1;
        }
    }

    return 0;
}

int 
MNetSend(mnet_client_sock *Target, char *Buffer, size_t BufferLength)
{
    assert(Target->Operation == MNET_QUEUED_OPERATION_NONE ||
           Target->Operation == MNET_QUEUED_OPERATION_SEND);
    
    // NOTE(Oskar): Copy buffer to Targets' buffer
    _snprintf_s(Target->Buffer, MNET_SOCK_BUFFER_SIZE, BufferLength, "%s", Buffer);
    Target->BytesToSend = (DWORD)BufferLength;

    Target->Operation = MNET_QUEUED_OPERATION_SEND;

    Target->DataBuf.buf = Target->Buffer + Target->BytesSent;
    Target->DataBuf.len = Target->BytesToSend - Target->BytesSent;

    if (WSASend(Target->Socket, &(Target->DataBuf), 1, 
                NULL, 0, &(Target->Overlapped), NULL) == SOCKET_ERROR)
    {
        if (WSAGetLastError() != ERROR_IO_PENDING)
        {
            Target->Operation = MNET_QUEUED_OPERATION_CLOSE;
            Win32OutputWSAErrorCode("WSASend Failed.");
            return -1;
        }
    }

    return 0;
}

void
MNetQueueClose(mnet_client_sock *Target)
{
    Target->QueuedForClose = 1;
}

static void
_MNetCreateServerConnection(mnet_client_sock *Target, SOCKET LastAcceptSocket)
{
    Target->Socket = LastAcceptSocket;
    ZeroMemory(&(Target->Overlapped), sizeof(OVERLAPPED));
    Target->QueuedForClose = 0;
    Target->Operation = MNET_QUEUED_OPERATION_NONE;
    Target->BytesSent = 0;
    Target->BytesReceived = 0;
    Target->DataBuf.len = MNET_SOCK_BUFFER_SIZE;
    Target->DataBuf.buf = Target->Buffer;
}

// TODO(Oskar): Better name for this function?
CHAR AcceptBuffer[2 * (sizeof(SOCKADDR_IN) + 16)];
DWORD Bytes = 0;
int 
_MNetServerSetup(mnet_server_sock *Server, BOOL FirstPass)
{
    // TODO(Oskar): What AF family ??
    Server->LastAcceptSocket = _MNetCreateSocket(AF_INET, SOCK_STREAM, 0);
    if (Server->LastAcceptSocket == SOCKET_ERROR)
    {
        return -1;
    }

    if (FirstPass == TRUE)
    {
        ZeroMemory(&Server->ListenOverlapped, sizeof(OVERLAPPED));
        if ((Server->EventArray[0] = Server->ListenOverlapped.hEvent = WSACreateEvent()) == WSA_INVALID_EVENT)
        {
            Win32OutputWSAErrorCode("Failed to create new event object.");
            return -1;
        }
        Server->NumberOfEvents = 1;
    }
    else
    {
        // NOTE(Oskar): If just queueing new accept we simply reset the event instead of creating a new one.
        WSAResetEvent(Server->EventArray[0]);
        ZeroMemory(&Server->ListenOverlapped, sizeof(OVERLAPPED));
        Server->ListenOverlapped.hEvent = Server->EventArray[0];
    }

    ZeroMemory(&Server->AcceptBuffer, MNET_SOCK_ACCEPT_BUFFER_SIZE);
    if (Server->AcceptEx(Server->ListenSocket, Server->LastAcceptSocket, AcceptBuffer, 
                         0, sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16, 
                         &Bytes, &Server->ListenOverlapped) == FALSE)
    {
        if (WSAGetLastError() != ERROR_IO_PENDING)
        {
            Win32OutputWSAErrorCode("Initial AcceptEx failed.");
            return -1;
        }
    }

    return 0;
}

void 
_MNetUpdateServerConnectionState(mnet_sock *Socket, 
                                 mnet_client_sock *Connection, 
                                 DWORD BytesTransferred)
{
    // NOTE(Oskar): Based on last queued operation we create a new event then set the socket as idle
    if (Connection->Operation == MNET_QUEUED_OPERATION_READ)
    {
        Connection->BytesReceived = BytesTransferred;
        Connection->BytesSent = 0;

        Socket->Events[Socket->NumberOfEvents++] = _MNetCreateReceivedEvent(Connection);
        Connection->Operation = MNET_QUEUED_OPERATION_NONE;
    }
    else if (Connection->Operation == MNET_QUEUED_OPERATION_SEND)
    {
        Connection->BytesSent += BytesTransferred;
        
        if (Connection->BytesToSend <= Connection->BytesSent)
        {
            // NOTE(Oskar): Done sending
            Socket->Events[Socket->NumberOfEvents++] = _MNetCreateSentEvent(Connection);
            Connection->Operation = MNET_QUEUED_OPERATION_NONE;
        }
        else
        {
            // NOTE(Oskar): Not done sending so we queue another send
            _MNetSendAdditional(Connection);
        }
    }
}

void
_MNetServerCloseClient(mnet_sock *Socket, mnet_client_sock *SocketConnection, DWORD ConnectionIndex)
{
    mnet_server_sock *Server = Socket->Server;
    Socket->Events[Socket->NumberOfEvents++] = _MNetCreateCloseEvent(SocketConnection);

    if (closesocket(SocketConnection->Socket) == SOCKET_ERROR)
    {
        Win32OutputWSAErrorCode("Call to closesocket Failed.");
    }

    MNET_FREE(SocketConnection);
    WSACloseEvent(Server->EventArray[ConnectionIndex]);
    
    // NOTE(Oskar): Move event and socket handles to the back of their respective arrays.        
    if (ConnectionIndex + 1 != Server->NumberOfEvents)
    {
        for (DWORD Index = ConnectionIndex; Index < Server->NumberOfEvents; Index++)
        {
            Server->EventArray[Index]  = Server->EventArray[Index + 1];   
            Server->Connections[Index] = Server->Connections[Index + 1];
        }
    }

    Server->NumberOfEvents--;
}

void
_MNetUpdateServer(mnet_sock *Socket)
{
    mnet_server_sock *Server = Socket->Server;
    // NOTE(Oskar): If this is the first update run we perform some additional
    // initialization.
    if (Server->LastAcceptSocket == INVALID_SOCKET)
    {
        _MNetServerSetup(Server, TRUE);
    }
    else
    {
        // NOTE(Oskar): If not first pass we will clear all events.
        Socket->NumberOfEvents = 0;
    }

    // NOTE(Oskar): Check for idle sockets that are queued to close
    for (DWORD Index = 1; Index < Server->NumberOfEvents; ++Index)
    {
        mnet_client_sock *Connection = Server->Connections[Index];
        int SocketUpForClose = (Connection->Operation == MNET_QUEUED_OPERATION_NONE && 
                                Connection->QueuedForClose);
        if (SocketUpForClose)
        {
            _MNetServerCloseClient(Socket, Connection, Index);
        }
    }

    mnet_client_sock *SocketConnection;
    DWORD BytesTransferred;
    DWORD Flags;
    DWORD WSAWaitResult = WSAWaitForMultipleEvents(Server->NumberOfEvents, 
                                                   Server->EventArray, 
                                                   FALSE, 
                                                   Socket->TimeoutMS, 
                                                   FALSE);
    if (WSAWaitResult == WSA_WAIT_FAILED)
    {
        // TODO(Oskar): Handle fatal error
        Win32OutputWSAErrorCode("WSAWait Failed.");
        return;
    }
    else if (WSAWaitResult == WSA_WAIT_TIMEOUT)
    {
        // NOTE(Oskar): Timeout triggered so nothing hapened.
        Socket->Events[Socket->NumberOfEvents++] = _MNetCreateTimeoutEvent();
        return;
    }

    DWORD EventIndex = WSAWaitResult - WSA_WAIT_EVENT_0;

    // NOTE(Oskar): Event was triggered on our listening socket.
    if (EventIndex == 0)
    {
        if (WSAGetOverlappedResult(Server->ListenSocket, &(Server->ListenOverlapped), &BytesTransferred, FALSE, &Flags) == FALSE)
        {
            Win32OutputWSAErrorCode("Failed to get overlapped result for incoming connection.");
            return;
        }

        // NOTE(Oskar): Check if too many connections
        if (Server->NumberOfEvents > WSA_MAXIMUM_WAIT_EVENTS)
        {
            // TODO(Oskar): Do we need to handle this by queueing a new Accept?
            closesocket(Server->LastAcceptSocket);
            return;
        }
        else
        {
            if ((Server->Connections[Server->NumberOfEvents] = 
                (mnet_client_sock *) MNET_ALLOC(sizeof(mnet_client_sock))) == NULL)
            {
                Win32OutputErrorCode("Call to global alloc failed.");
                return;
            }

            mnet_client_sock *TargetConnection = Server->Connections[Server->NumberOfEvents];
            _MNetCreateServerConnection(TargetConnection, Server->LastAcceptSocket);
            if ((TargetConnection->Overlapped.hEvent = Server->EventArray[Server->NumberOfEvents] = WSACreateEvent()) == WSA_INVALID_EVENT)
            {
                Win32OutputWSAErrorCode("Failed to create new event object.");
                return;
            }

            // NOTE(Oskar): New connection was established
            Socket->Events[Socket->NumberOfEvents++] = _MNetCreateAcceptEvent(TargetConnection);
            Server->NumberOfEvents++;
        }

        // NOTE(Oskar): Create new socket and post another AcceptEx operation
        _MNetServerSetup(Server, FALSE);

        return;
    }

    // NOTE(Oskar): Event was triggered on one of the active sockets.
    SocketConnection = Server->Connections[EventIndex];
    WSAResetEvent(Server->EventArray[EventIndex]);

    BOOL OverlappedResult = WSAGetOverlappedResult(SocketConnection->Socket, 
                                                   &SocketConnection->Overlapped, 
                                                   &BytesTransferred, FALSE, &Flags);
    // NOTE(Oskar): Close connection
    if (OverlappedResult == FALSE || BytesTransferred == 0)
    {
        _MNetServerCloseClient(Socket, SocketConnection, EventIndex);
        // Socket->Events[Socket->NumberOfEvents++] = _MNetCreateCloseEvent(SocketConnection);

        // if (closesocket(SocketConnection->Socket) == SOCKET_ERROR)
        // {
        //     Win32OutputWSAErrorCode("Call to closesocket Failed.");
        // }

        // MNET_FREE(SocketConnection);
        // WSACloseEvent(Server->EventArray[EventIndex]);
        
        // // NOTE(Oskar): Move event and socket handles to the back of their respective arrays.        
        // if (EventIndex + 1 != Server->NumberOfEvents)
        // {
        //     for (DWORD Index = EventIndex; Index < Server->NumberOfEvents; Index++)
        //     {
        //         Server->EventArray[Index]  = Server->EventArray[Index + 1];   
        //         Server->Connections[Index] = Server->Connections[Index + 1];
        //     }
        // }

        // Server->NumberOfEvents--;
        return;
    }

    // NOTE(Oskar): Here we create new events based on the last triggered operation.
    _MNetUpdateServerConnectionState(Socket, SocketConnection, BytesTransferred);

    ZeroMemory(&(SocketConnection->Overlapped), sizeof(WSAOVERLAPPED));
    SocketConnection->Overlapped.hEvent = Server->EventArray[EventIndex];
}

void
_MNetUpdateClient(mnet_sock *Socket)
{
    mnet_client_sock *Client = Socket->Client;

    if (Socket->Initialized == 0)
    {
        Socket->Initialized = 1;
    }
    else
    {
        Socket->NumberOfEvents = 0;
    }

    if (Client->Operation == MNET_QUEUED_OPERATION_CLOSE)
    {
        closesocket(Client->Socket);
        Socket->Events[Socket->NumberOfEvents++] = _MNetCreateCloseEvent(Client);
        Socket->State = MNET_SOCK_STATE_CLOSED;
        return;
    }

    DWORD Flags;
    DWORD BytesTransferred;
    BOOL Result = WSAGetOverlappedResult(Client->Socket, &(Client->Overlapped), &BytesTransferred, FALSE, &Flags);
    if (Result == FALSE)
    {
        if (WSAGetLastError() != WSA_IO_INCOMPLETE)
        {
            Socket->Events[Socket->NumberOfEvents++] = _MNetCreateCloseEvent(Client);
            Win32OutputWSAErrorCode("Initial ConnectEx failed.");
            return;
        }
        else
        {
            return;
        }
    }

    switch (Client->Operation)
    {
        case MNET_QUEUED_OPERATION_CONNECT:
        {
            int ReturnCode = setsockopt(Client->Socket, SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, NULL, 0);
            if (ReturnCode != 0) 
            {
                Win32OutputWSAErrorCode("Set socket options failed.");
                return;
            }

            Socket->Events[Socket->NumberOfEvents++] = _MNetCreateConnectEvent(Client);
            Client->Operation = MNET_QUEUED_OPERATION_NONE;
        } break;

        case MNET_QUEUED_OPERATION_READ:
        {
            Client->BytesReceived = BytesTransferred;
            Client->BytesSent = 0;

            Socket->Events[Socket->NumberOfEvents++] = _MNetCreateReceivedEvent(Client);
            Client->Operation = MNET_QUEUED_OPERATION_NONE;
        } break;

        case MNET_QUEUED_OPERATION_SEND:
        {
            Client->BytesSent += BytesTransferred;
        
            if (Client->BytesToSend <= Client->BytesSent)
            {
                // NOTE(Oskar): Done sending
                Socket->Events[Socket->NumberOfEvents++] = _MNetCreateSentEvent(Client);
                Client->Operation = MNET_QUEUED_OPERATION_NONE;
            }
            else
            {
                // NOTE(Oskar): Not done sending so we queue another send
                _MNetSendAdditional(Client);
            }
        } break;

        case MNET_QUEUED_OPERATION_NONE:
        default:
        {
            return;
        } break;
    }
}

void
MNetUpdate(mnet_sock *Socket)
{
    if (Socket->Type == MNET_SOCK_TYPE_SERVER)
    {
        _MNetUpdateServer(Socket);
    }
    else if (Socket->Type == MNET_SOCK_TYPE_CLIENT)
    {
        _MNetUpdateClient(Socket);
    }
    else
    {
        // NOTE(Oskar): Error or silently do nothing?
    }
}