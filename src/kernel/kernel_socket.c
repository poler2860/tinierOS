#include "kernel_socket.h"
#include "kernel_cc.h"
#include "kernel_streams.h"
#include "tinyos.h"

socket_cb *PORT_MAP[MAX_PORT];

static file_ops socket_fo = {
    .Read = socket_read, .Write = socket_write, .Close = socket_close};

// initialization of a socket(cb)
socket_cb *socket_cb_init(port_t port) {
  socket_cb *socket = (socket_cb *)xmalloc(sizeof(socket_cb));

  socket->refcount = 0; // since no one is currently using the socket
  socket->type = SOCKET_UNBOUND;
  socket->port = port; // allocate memory for the socket

  return socket;
}

Fid_t sys_Socket(port_t port) {
  // check if the port is negative or bigger than the max port(non valid)
  if (port < 0 || port > MAX_PORT) {
    return NOFILE;
  }

  Fid_t fid;
  FCB *fcb;

  // If FCB reservation fails return error code (-1)
  if (!FCB_reserve(1, &fid, &fcb)) {
    return NOFILE;
  }

  // new socket setup and allocation of memory
  socket_cb *socket = socket_cb_init(port);

  // if uncorrectly setup unreserve the fid and fcb and return error
  if (socket == NULL) {
    FCB_unreserve(1, &fid, &fcb);
    return NOFILE;
  }

  // continue setting up socket
  socket->fcb = fcb;
  fcb->streamobj = socket;
  fcb->streamfunc = &socket_fo;

  return fid;
}

int sys_Listen(Fid_t sock) {
  // If the id is invalid or the fid not legal return error code (-1)
  if (sock < 0 || sock > MAX_FILEID - 1 || get_fcb(sock) == NULL) {
    return -1;
  }

  // Setup the listener socket
  socket_cb *listener = (socket_cb *)get_fcb(sock)->streamobj;

  // If socket is null or port is none return error code (-1)
  if (listener == NULL || listener->port == NOPORT) {
    return -1;
  }

  // If port is used by another listener
  // Or if listener is initialized return error code (-1)
  if (PORT_MAP[listener->port] != NULL || listener->type != SOCKET_UNBOUND) {
    return -1;
  }

  PORT_MAP[listener->port] = listener; // Declare port to listener
  listener->type = SOCKET_LISTENER; // Listener should now be of type listener

  // Initialization of listener(union) of the listener cb
  rlnode_init(&listener->listener_s.queue, NULL); // Request queue is empty
  listener->listener_s.req_available =
      COND_INIT; // A cond var to sleep the listener

  return 0;
}

Fid_t sys_Accept(Fid_t lsock) {

  // If the id is invalid or the fid not legal return error code (-1)
  if (lsock < 0 || lsock > MAX_FILEID - 1 || get_fcb(lsock) == NULL) {
    return -1;
  }

  // Setup the listener socket
  socket_cb *listener = (socket_cb *)get_fcb(lsock)->streamobj;

  // If not properly initialized as a listener return error code (-1)
  if (listener == NULL || listener->type != SOCKET_LISTENER) {
    return -1;
  }

  listener->refcount++; // listener is now being used so increment refcount so
                        // as no to be deleted
  int port = listener->port;
  // Start listening
  while (is_rlist_empty(&listener->listener_s.queue) &&
         PORT_MAP[port] != NULL) {
    kernel_wait(&listener->listener_s.req_available,
                SCHED_USER); // user request to listen
  }
  if (PORT_MAP[port] == NULL) {
    listener->refcount--;
    return -1;
  }

  // Request so proceed
  // If we are still operating or in a non valid port continue else return error
  // code (-1)
  if (listener == NULL || listener->type != SOCKET_LISTENER ||
      PORT_MAP[listener->port] != listener) {
    listener->refcount--;
    return -1;
  }

  // Pop the first request available in the listener
  connection_request *request =
      (connection_request *)rlist_pop_front(&listener->listener_s.queue)->obj;

  // Initialize a peer
  Fid_t peer_fid = sys_Socket(listener->port);

  // If invalid or non legal fid return error code (-1)
  if (peer_fid == -1 || get_fcb(peer_fid) == NULL) {
    listener->refcount--;
    return -1;
  }

  request->admitted = 1; // since the request is now being handled

  // The 2 peers
  socket_cb *peer_socket = (socket_cb *)get_fcb(peer_fid)->streamobj;
  socket_cb *req__socket = request->peer;

  // Initialization of the peers
  peer_socket->type = SOCKET_PEER;
  req__socket->type = SOCKET_PEER;

  // Initialize the pipes
  pipe_cb *pipe1 = pipe_init();
  pipe_cb *pipe2 = pipe_init();

  // Setup readers and writers
  pipe1->reader = peer_socket->fcb;
  pipe1->writer = req__socket->fcb;
  peer_socket->peer_s.read_pipe = pipe1;
  peer_socket->peer_s.write_pipe = pipe2;

  pipe2->reader = req__socket->fcb;
  pipe2->writer = peer_socket->fcb;
  req__socket->peer_s.read_pipe = pipe2;
  req__socket->peer_s.write_pipe = pipe1;

  // signal for connection and decrement refcount since we are not using it no
  // more
  kernel_signal(&request->connected_cv);
  listener->refcount--;

  return peer_fid;
}

int sys_Connect(Fid_t sock, port_t port, timeout_t timeout) {

  // If the id is invalid or the fid not legal return error code (-1)
  if (sock < 0 || sock > MAX_FILEID - 1 || get_fcb(sock) == NULL) {
    return -1;
  }

  // check if port is non valid
  if (port <= NOPORT || port >= MAX_PORT - 1) {
    return NOFILE;
  }

  // Check if port wasn't properly setup as listener or at all
  if (PORT_MAP[port] == NULL || PORT_MAP[port]->type != SOCKET_LISTENER) {
    return -1;
  }

  FCB *sock_fcb = get_fcb(sock);
  if (sock_fcb == NULL) {
    return -1;
  }

  // The request socket
  socket_cb *r_socket = sock_fcb->streamobj;
  if (r_socket->type != SOCKET_UNBOUND) {
    return -1;
  }

  socket_cb *c_socket = PORT_MAP[port]; // The connection socket
  c_socket->refcount++;                 // Increment refcount

  // Setup the request to connect
  connection_request *request =
      (connection_request *)xmalloc(sizeof(connection_request));
  request->admitted = 0;
  request->connected_cv = COND_INIT;
  request->peer = r_socket;
  rlnode_init(&request->queue_node, request);

  // Push request to his queue and notify the listener
  rlist_push_back(&c_socket->listener_s.queue, &request->queue_node);
  kernel_signal(&c_socket->listener_s.req_available);

  // Wait until timeout
  int wait_time;
  while (request->admitted == 0) {
    wait_time = kernel_timedwait(&request->connected_cv, SCHED_USER, timeout);
    if (wait_time == 0) {
      break;
    }
  }

  c_socket->refcount--;

  // return 0 if succesfull request or -1 (error code) if timed out
  return (request->admitted == 1) ? 0 : -1;
}

int sys_ShutDown(Fid_t sock, shutdown_mode how) {
  // If the id is invalid or the fid not legal return error code (-1)
  if (sock < 0 || sock > MAX_FILEID - 1 || get_fcb(sock) == NULL) {
    return -1;
  }

  // The socket to close
  socket_cb *c_socket = (socket_cb *)get_fcb(sock)->streamobj;

  // Close depending on the request shutdown mode
  switch (how) {
  case SHUTDOWN_READ:
    // Reader close
    pipe_reader_close(c_socket->peer_s.read_pipe);
    c_socket->peer_s.read_pipe = NULL;
    break;
  case SHUTDOWN_WRITE:
    // Writer close
    pipe_writer_close(c_socket->peer_s.write_pipe);
    c_socket->peer_s.write_pipe = NULL;
    break;
  case SHUTDOWN_BOTH:
    // Close reader and writer
    pipe_writer_close(c_socket->peer_s.write_pipe);
    pipe_reader_close(c_socket->peer_s.read_pipe);
    c_socket->peer_s.read_pipe = NULL;
    c_socket->peer_s.write_pipe = NULL;
    break;
  default:
    return -1;
  }

  return 0;
}

int socket_write(void *socket, const char *buf, unsigned int n) {

  // The socket to write to
  socket_cb *w_socket = (socket_cb *)socket;

  // If not peer or null return error code (-1)
  if (w_socket == NULL || w_socket->type != SOCKET_PEER) {
    return -1;
  }

  // Write with the correct pipe
  return pipe_write(w_socket->peer_s.write_pipe, buf, n);
}

int socket_read(void *socket, char *buf, unsigned int n) {

  // The socket to read from
  socket_cb *r_socket = (socket_cb *)socket;

  // If not peer or null return error code (-1)
  if (r_socket == NULL || r_socket->type != SOCKET_PEER) {
    return -1;
  }

  // Read with the correct pipe
  return pipe_read(r_socket->peer_s.read_pipe, buf, n);
}

int socket_close(void *socket) {

  // The socket to close
  socket_cb *c_socket = (socket_cb *)socket;

  // If null (meaning it's closed) return error code (-1)
  if (c_socket == NULL) {
    return -1;
  }

  // Close the socket depending on it's type
  switch (c_socket->type) {
  case SOCKET_UNBOUND:
    break;
  case SOCKET_LISTENER:
    PORT_MAP[c_socket->port] = NULL; // Remove from port map
    kernel_broadcast(
        &c_socket->listener_s.req_available); // Wakeup waiting sockets
    break;
  case SOCKET_PEER:
    // Close reader
    pipe_reader_close(c_socket->peer_s.read_pipe);
    c_socket->peer_s.read_pipe = NULL;

    // Close writer
    pipe_writer_close(c_socket->peer_s.write_pipe);
    c_socket->peer_s.write_pipe = NULL;
    break;
  default:
    return -1;
  }

  // if we are not using it anymore free from memory
  if (c_socket->refcount == 0) {
    free(socket);
  }

  return 0;
}
