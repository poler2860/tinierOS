#ifndef KERNEL_SOCKET_H
#define KERNEL_SOCKET_H

#include "kernel_streams.h"
#include "tinyos.h"

/**
        @brief Socket types (states)
*/
typedef enum SOCKET_TYPE {
  SOCKET_LISTENER, // Ready for connections
  SOCKET_UNBOUND,  // Not currently in use
  SOCKET_PEER      // For peer-to-peer connections
} socket_type;

/**
        @brief Listener Socket
*/
typedef struct listener_socket {
  rlnode queue;          // queue for sockets than want to connect to this one
  CondVar req_available; // cond var to wake up the listener
} listener_socket;

/**
        @brief Unbound Socket
*/
typedef struct unbound_socket {
  rlnode unbound_socket; // a node for connection with the queue
} unbound_socket;

/**
        @brief Peer Socket
*/
typedef struct peer_socket {
  struct peer_socket *peer; // the peer (socket) we are connected to
  pipe_cb *write_pipe;      // the write pipe
  pipe_cb *read_pipe;       // the read pipe
} peer_socket;

/**
        @brief The socket control block

        Holds all the information required
        for each socket to function, such as a refcount,
        fcb,it's type and port and finally a union of either
        a listener, unbound or peer socket.
*/
typedef struct socket_control_block {
  uint refcount; // A reference count so we won't delete the socket when in use
  FCB *fcb;      // The sockets FCB. If NULL, socket is closed.
  socket_type type; // The type of the socket (state), which is the same as the
                    // other socket (union)
  port_t port;      // The socket's port
  union {
    listener_socket listener_s;
    unbound_socket unbound_s;
    peer_socket peer_s;
  };
} socket_cb;

/**
        @brief The connection request object.

        Used for the connection to the listener.
        This connection request is sent using Connect() to
        the listening socket to be connected with.
*/
typedef struct connection_request {
  int admitted; // A simple flag, that when 1 means the request is being handled
  socket_cb *peer;      // The socket that made the request
  CondVar connected_cv; // A cond var to sleep the connecting socket till
                        // connection occurs or it's timed out
  rlnode queue_node;    // A node to insert to the listerner queue
} connection_request;

/**
        Initialization of a socket cb metadata.
*/
socket_cb *socket_cb_init(port_t port);

/**
        @brief Write operation for the socket.
*/
int socket_write(void *socket, const char *buf, unsigned int n);

/**
        @brief Read operation for a socket.
*/
int socket_read(void *socket, char *buf, unsigned int n);

/**
        @brief Close given socket
*/
int socket_close(void *socket);

/**
        @brief
*/
Fid_t sys_Socket(port_t port);

/**
 @breif listen to a given socket
 **/
int sys_Listen(Fid_t sock);

Fid_t sys_Accept(Fid_t lsock);

#endif // !KERNEL_STREAMS
