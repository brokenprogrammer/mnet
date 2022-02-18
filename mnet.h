#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <Windows.h>
#include <WS2tcpip.h>
#include <ws2def.h>
#include <Wspiapi.h>
#include <mswsock.h>
#include <strsafe.h>
#include <stdio.h>
#include <assert.h>

#define MNET_SOCK_BUFFER_SIZE 4096
#define MNET_SOCK_DEFAULT_BACKLOG 5
#define MNET_SOCK_ACCEPT_BUFFER_SIZE (2 * (sizeof(SOCKADDR_IN)) + 16)
#define MNET_MAX_NUMBER_OF_EVENTS WSA_MAXIMUM_WAIT_EVENTS

typedef enum _mnet_queued_operation
{
    MNET_QUEUED_OPERATION_NONE,
    MNET_QUEUED_OPERATION_CONNECT,
    MNET_QUEUED_OPERATION_READ,
    MNET_QUEUED_OPERATION_SEND,
    MNET_QUEUED_OPERATION_CLOSE,
} mnet_queued_operation;

typedef struct _mnet_client_sock
{
    CHAR Buffer[MNET_SOCK_BUFFER_SIZE];
    WSABUF DataBuf;
    DWORD BytesSent;
    DWORD BytesToSend;
    DWORD BytesReceived;

    SOCKET Socket;
    WSAOVERLAPPED Overlapped;

    mnet_queued_operation Operation;
} mnet_client_sock;

typedef struct _mnet_server_sock
{
    SOCKET ListenSocket;
    WSAOVERLAPPED ListenOverlapped;

    SOCKET LastAcceptSocket;
    CHAR AcceptBuffer[MNET_SOCK_ACCEPT_BUFFER_SIZE];
    DWORD AcceptBufferReadBytes;

    DWORD NumberOfEvents;
    WSAEVENT EventArray[WSA_MAXIMUM_WAIT_EVENTS];
    mnet_client_sock *Connections[WSA_MAXIMUM_WAIT_EVENTS]; // TODO(Oskar): Rename

    LPFN_ACCEPTEX AcceptEx;
} mnet_server_sock;

typedef enum _mnet_event_type
{
    MNET_EVENT_TYPE_INVALID,
    MNET_EVENT_TYPE_CONNECT,
    MNET_EVENT_TYPE_ACCEPT,
    MNET_EVENT_TYPE_SENT,
    MNET_EVENT_TYPE_RECEIVED,
    MNET_EVENT_TYPE_CLOSE,
    MNET_EVENT_TYPE_LISTEN,
    MNET_EVENT_TYPE_TIMEOUT,
} mnet_event_type;

typedef struct _mnet_event
{
    // TODO(Oskar): Do we specify buffers here?
    mnet_event_type Type;

    // TODO(Oskar): Figure out what we want in case of a client socket.
    union
    {
        struct
        {
            int Port;
            char *Address;
        };

        mnet_client_sock *Connection;
    };

} mnet_event;

typedef enum _mnet_sock_type
{
    MNET_SOCK_TYPE_INVALID,
    MNET_SOCK_TYPE_CLIENT,
    MNET_SOCK_TYPE_SERVER,
} mnet_sock_type;

typedef enum _mnet_sock_state
{
    MNET_SOCK_STATE_CLOSED,
    MNET_SOCK_STATE_CONNECTING,
    MNET_SOCK_STATE_CONNECTED,
    MNET_SOCK_STATE_LISTENING,
} mnet_sock_state;

typedef struct _mnet_sock
{
    mnet_sock_type Type;
    mnet_sock_state State;
    DWORD TimeoutMS;

    // NOTE(Oskar): Metadata
    int Port;
    char *Address;

    // NOTE(Oskar): Events
    int NumberOfEvents;
    mnet_event Events[MNET_MAX_NUMBER_OF_EVENTS];

    union
    {
        mnet_client_sock *Client;
        mnet_server_sock *Server;
    };

    // NOTE(Oskar): Client socket specific
    LPFN_CONNECTEX ConnectEx;
    int Initialized;
} mnet_sock;

int MNetInitialize();
mnet_sock MNetCreateSocket();
int MNetListen(mnet_sock *Socket, int Port);
int MNetConnect(mnet_sock *Socket, char *Address, int Port);
void MNetClose(mnet_sock *Socket);

void MNetUpdate(mnet_sock *Socket);

int MNetRecieve(mnet_client_sock *Target);
int MNetSend(mnet_client_sock *Target, char *Buffer, size_t BufferLength);

// TODO(Oskar): Functions to get states / data and modify them for example timeout.
// TODO(Oskar): Turn some errors into non-fatal events.