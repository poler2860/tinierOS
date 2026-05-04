#include "tinyos.h"
#include "kernel_streams.h"
#include "kernel_sched.h"
#include "kernel_cc.h"

static file_ops reader_fo = {
    .Open = (void*)pipe_read_error,
	  .Read = pipe_read,
	  .Write = pipe_write_error,
	  .Close = pipe_reader_close
};

static file_ops writer_fo = {
    .Open = (void*)pipe_write_error,
	  .Read = pipe_read_error,
	  .Write = pipe_write,
	  .Close = pipe_writer_close
};

pipe_cb *pipe_init() {
    pipe_cb *n_pipe = (pipe_cb*)malloc(sizeof(pipe_cb));
    //Return error if memory allocation failed
    if(n_pipe  == NULL)
        return NULL;

		n_pipe->buffer_node = init_list(PIPE_BUFFER_SIZE, NULL);
		n_pipe->r_pos = n_pipe->buffer_node;
		n_pipe->w_pos = n_pipe->buffer_node;
		n_pipe->bufferBytes = 0;
		n_pipe->hasData = COND_INIT;
		n_pipe->hasSpace = COND_INIT;

    return n_pipe;
}

int sys_Pipe(pipe_t* pipe) {

    FCB* fcb[2];        // Reader [0] and writer [1] FCBs
    Fid_t fid[2];       // Reader [0] and writer [1] FIDs

    //Try to reserve the FCBs
    if(FCB_reserve(2, fid, fcb) == 0)
        return -1;
 	  

    // Create a pipe
    pipe_cb *n_pipe = pipe_init();


    //fid[0] is for reading and fid[1] is for writing data as shown in the presentation
    pipe->read = fid[0];
    pipe->write = fid[1];

    //Reader setup
    fcb[0]->streamfunc = &reader_fo;
    fcb[0]->streamobj = n_pipe;
    n_pipe->reader = fcb[0];

    //Writer setup
    fcb[1]->streamfunc = &writer_fo;
    fcb[1]->streamobj = n_pipe;
    n_pipe->writer = fcb[1];

    return 0;
}

int pipe_write(void* pipecb_t, const char *buf, unsigned int n) {

pipe_cb* pipe_w = (pipe_cb*) pipecb_t; 

    // Check if pipe, reader and writer are open
    if(pipe_w == NULL || pipe_w->writer == NULL || pipe_w->reader == NULL){
        return -1;
    }

    //Check if we have space and continue only if we have space in the buffer (pipe)
    int bytesAvailable = PIPE_BUFFER_SIZE - pipe_w->bufferBytes;
    while(bytesAvailable == 0 && pipe_w->reader != NULL){
        kernel_wait(&pipe_w->hasSpace, SCHED_PIPE);   				//Wait for someone to free up some space
        bytesAvailable = PIPE_BUFFER_SIZE - pipe_w->bufferBytes;
    }

    //Check if reader is closed
    if(pipe_w->reader == NULL){
        return -1;
    }

    //Check for the available space in the buffer and 
    int w_bytes = (n<bytesAvailable) ? n : bytesAvailable;

    //Start writing the bytes
    for(int i=0; i<w_bytes; i++){
		    if(pipe_w->w_pos->c != '\0'){       // Stop if NULL Terminator found
			      return -1;
		    }

        pipe_w->w_pos->c = buf[i];   						    //Write the character in the current pos for write
        pipe_w->w_pos = pipe_w->w_pos->next; 			  //Go to the next available spot
        pipe_w->bufferBytes++;          						//Increment the number of bytes in the buffer
    }

    //Broadcast the waiting readers
    kernel_broadcast(&pipe_w->hasData);

    //return the written bytes
    return w_bytes;

}

int pipe_read(void* pipecb_t, char *buf, unsigned int n) {

    pipe_cb* pipe_r = (pipe_cb*) pipecb_t;

    //Continue if pipe and reader aren't null (closed or don't exist)
    if(pipe_r == NULL || pipe_r->reader == NULL){
        return -1;
    }

    //If we have no writen bytes and the writer is closed return
    if(pipe_r->writer == NULL && pipe_r->bufferBytes == 0){
        return 0;
    }

    //If we have no data to read wait for data
    while(pipe_r->bufferBytes == 0 && pipe_r->writer != NULL){
        kernel_wait(&pipe_r->hasData, SCHED_PIPE);
    }

    //Re-check: If we have no writen bytes and the writer is closed return
    if(pipe_r->writer == NULL && pipe_r->bufferBytes == 0){
        return 0;
    }

    //Read the max #ofbytes
    int r_bytes = (n<pipe_r->bufferBytes) ? n : pipe_r->bufferBytes;

    //Start reading the bytes
    for(int i=0; i<r_bytes; i++){
        buf[i] = pipe_r->r_pos->c;     					//Read each character in the current pos of read
	     	pipe_r->r_pos->c = '\0';							  //Replace each readed character with the NULL terminator
        pipe_r->r_pos = pipe_r->r_pos->next;   	//Go to the next spot to read from
        pipe_r->bufferBytes--;             			//Decrement the counter for #ofbyteswriten
    }

    //Broadcast the waiting writers
    kernel_broadcast(&pipe_r->hasSpace);

    //Return the readed bytes
    return r_bytes;
}


int pipe_writer_close(void* _pipecb) {

    pipe_cb *c_pipe = (pipe_cb*)_pipecb;

    //If the pipe is null then return error code (-1)
    if(c_pipe == NULL){
        return -1;
    }

    c_pipe->writer = NULL;

    //Point the writer to nothing (Null) and if reader is null too, then free the pipe
    if (c_pipe->reader == NULL){
        free(c_pipe->writer);
        free(c_pipe->reader);
        free(c_pipe->buffer_node);
        free(c_pipe);
    }
    else{
    	//If we have data then broadcast to wake up all waiting readers so they can now read
        kernel_broadcast(&c_pipe->hasData);
    }
    return 0; 
}

int pipe_reader_close(void* _pipecb) {
    pipe_cb *c_pipe = (pipe_cb*)_pipecb;

    //If the pipe or the reader is null then return error code (-1)
    if(c_pipe == NULL || c_pipe->reader == NULL){
        return -1;
    }

	//Point the reader to nothing (Null) and if writer is null too, then free the pipe
    if ((c_pipe->reader = NULL) && c_pipe->writer == NULL){
        free(c_pipe->buffer_node);
        free(c_pipe);
    }
    else{
    	//If we have space then broadcast to wake up all waiting writers so they can now write
        kernel_broadcast(&c_pipe->hasSpace);
    }

    return 0;

}

/* Always returns -1 */
int pipe_error() {
	return -1;
}

int pipe_write_error(void* pipecb_t, const char *buf, unsigned int n) {
	return -1;
}

int pipe_read_error(void* pipecb_t, char *buf, unsigned int n) {
	return -1;
}


c_list *init_list(int size, const char* data) {
	
	c_list *start_node = (c_list*)malloc(sizeof(c_list));

	// Setup the first node
	if(data != NULL) {
		start_node->c = data[0];
	} else {
		start_node->c = '\0';
	}

	// Initialize the next and previous links
	start_node->next = NULL;
	start_node->prev = NULL;

  //Save the start node
  c_list *list = start_node;
	
	for(int i=0; i<size; i++) {

    c_list *new_node = (c_list*)malloc(sizeof(c_list));
    if(start_node->next!=NULL){
        new_node->next = start_node->next;
	    } else {
	    	new_node->next = start_node;
	    }
        new_node->prev = start_node;
        new_node->prev->next = new_node;
        new_node->next->prev = new_node;
        
        //if the data is null then we replace it with a NULL terminator	
        if(data!=NULL){
    		    new_node->c = data[i];
	      } else {
	    	    new_node->c = '\0';
	      }

        // set first to the new char node
        start_node = new_node;
    }

    return list;

}

