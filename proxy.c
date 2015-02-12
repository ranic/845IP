/* proxy.c: Proxy server using multithreading that supports concurrency caching. 
 *
 * Authors: Vijay Jayaram (vijayj@andrew.cmu.edu)
 * 			Anand Pattabiraman (apattabi@andrew.cmu.edu)
 *
 * Our proxy server creates and detaches a thread for each client GET request.
 * While worker threads process requests, the main thread waits for new ones.
 * If the request is for less than MAX_OBJECT_SIZE amount of data, we cache it.
 * Our cache is a FIFO linked list; here, LRU eviction is NOT implemented.
 * A readers-writer lock is used to handle multiple calls to read/write in cache.
 * The lock functions are wrapped around rwlocks in the POSIX interface.
 */

#include <stdio.h>
#include <stdlib.h>
#include "csapp.h"

#define MAX_CACHE_SIZE 1049000
#define MAX_HEADERS_SIZE 50000
#define MAX_OBJECT_SIZE 102400
#define GIVEN_PORT 32726

/* Here, we implement our cache as a queue of cache objects. 
 * 		key = 'hostname + path'
 *		data is a ptr to requested data in memory (Malloc-ed)
 *		
 * The queue works as follows:
 *      => cache->front = dummy_node
 *	    => cache->front->next = LRU object
 *		=> Add to the back of the queue
 *		=> 'Remove' function can remove from the middle
 */

typedef struct cache_object* cache_obj;

/* Cache struct */
struct cache_object {
	char* key;
    char* hdrs;
	char* data;
	cache_obj next;
	int size;
};

struct cache_queue {
	cache_obj front;
	cache_obj back;
	int size;
	pthread_rwlock_t lock;
};

/*Global cache variable that is initialized with init_cache()*/
struct cache_queue* cache;

sem_t mutex;

/* 	Cache functions
 *
 * 		void init_cache():
 *			=> Mallocs cache, sets dummy_node at front/back
 *			=> Initializes lock
 *
 *		void add_to_cache(char* key, char* data, int size):
 *			=> Only called with size < MAX_OBJECT_SIZE
 *			=> Evicts as many LRU nodes as necessary to make space
 *			=> Mallocs cache_object, initializes it with args
 *			=> Puts at end of queue, sets cache->back to it
 *			=> Increments cache->size
 *
 *		void free_obj(cache_obj object):
 *			=> Free data associated with a cache object
 *
 *		int search_cache(char* key, int clientfd):
 *			=> Searches cache for an object with key 'key'
 *			=> Return ptr to object if found, else NULL
 *
*/
void init_cache();
void add_to_cache(char* key, char* hdrs, char* data, int size);
void free_obj(cache_obj object);
int search_cache(char* key, int clientfd);

int check_cache_single();
int Check_cache_single();

/*Lock wrapper functions*/
void init_lock(pthread_rwlock_t* lock);
void read_lock();
void write_lock();
void unlock();


/*Parses request*/
void* handle_request(void* fd_addr);
void parseit(void* fd_addr);

/*In the case of exit, closes two fd's*/
void cleanup(int firstfd, int secondfd);

/*Forwards the client request to the appropriate host*/
int forwardit(char * host, char * path, int port, int fd, rio_t *rio, char* key); 

/*Extracts the additional request headers.*/
void read_requesthdrs(rio_t * client_rio, char * full_request, char *host, 
                        int hostfd);

/*Extracts the additional response headers.*/
void read_responsehdrs(rio_t *rp, int clientfd, char* key);

/*Global constants regarding client*/
static const char *user_agent = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static const char *accept_s = "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n";
static const char *accept_encoding = "Accept-Encoding: gzip, deflate\r\n";
static const char *connection_s = "Connection: close\r\n";
static const char *proxy_s = "Proxy-Connection: close\r\n";
static const char *finish_request = "\r\n";
int connectioncount = 0;

int main(int argc, char **argv)
{
    int listenfd, port, *connfd;
	socklen_t clientlen = sizeof(struct sockaddr_in);
    struct sockaddr_in clientaddr;
	pthread_t tid;

    printf("%s%s%s", user_agent, accept_s, accept_encoding);

    Signal(SIGPIPE, SIG_IGN);

    /* Check command line args */
	if(argc == 1) port = GIVEN_PORT;
	else if (argc == 2) port = atoi(argv[1]);
	else {
		fprintf(stderr, "usage: ./proxy <port>\n");
		exit(1);
	}
		
	if (port < 0 || port > 65535) {
		fprintf(stderr, "Port Invalid: Must be between 0 and 65535\n");
		exit(-1);
	}

	sem_init(&mutex, 0 , 1);
    init_cache();	
    listenfd = Open_listenfd(port);
    while (1) {
        clientlen = sizeof(clientaddr);
		connfd = Malloc(sizeof(int));
        *connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        printf("Connection made. #%d\n", connectioncount++);
		Pthread_create(&tid, NULL, handle_request, (void*) connfd);
    }
    return 0;
}
/*Small wrapper to parse/forward the request that fits Pthread specs*/
void* handle_request(void* connfd) {
	parseit(connfd);
	return NULL;
}

void request_error(char *msg) /* application error */
{
    fprintf(stderr, "%s\n", msg);
}

/*
 * parseit - expects a socketfd to get the GET requests.
 *      Request format:
 *          GET http://www.google.com<:port>/path HTTP/<version #> 
 *          
 *      Note: 
 *          <> - indicates optional fields.
 */
void parseit(void* fd_addr) {
	int fd = *((int*) fd_addr);
	Free(fd_addr);
    int port = -1;
    int numParsed;
    char buf[MAXLINE] = {0}; 
    char method[MAXLINE] = {0};
    char host[MAXLINE] = {0};
    char path[MAXLINE] = {0};
    char version[MAXLINE] = {0};
    char protocol[7] = {0};
    const char* http = "http://";
    const char* versIntro = "HTTP/";
    char* movingbuf;

	Pthread_detach(Pthread_self());

    rio_t rio;
    Rio_readinitb(&rio, fd);

    /* Reads the GET request from the fd */
    Rio_readlineb(&rio, buf, MAXLINE);

    if((numParsed = sscanf(buf, "%s %s %s", method, host, version)) < 2){
        request_error("Error parsing request: Not enough arguments.");
        printf("%s\n", buf);
        printf("Numparsed: %d\n", numParsed);
		cleanup(fd, -1);
        return;
    }

  /* Checks the version starts with "HTTP/" */
    strncpy(protocol, version, 5);
    if (strcmp(protocol, versIntro)) {
        request_error("Error parsing request: Invalid HTTP version.");
		cleanup(fd, -1);
		return;
    }

    /* Checks that it's a GET request */
    if (strcasecmp(method, "GET")) {
        request_error("Error parsing request: Not a GET request.");
		cleanup(fd, -1);
        return;
    }

    /* Checks the buf uses protocol "http://" */
    strncpy(protocol, host, 7);
    if (strcmp(protocol, http)) {
        request_error("Error parsing request: Not http:// domain.");
		cleanup(fd, -1);
        return;
    }

    /* Extracts path. */
    movingbuf = strchr((host + 7), '/');
    if(movingbuf == NULL){
        request_error("Error parsing request: No path found.");
		cleanup(fd, -1);
        return;
    }
    /* Need to set '/' to '\0' to help extract port/server*/
    strcpy(path,movingbuf);
    *movingbuf = '\0';

    /* Checks for port number. */
    movingbuf = strchr((host + 7), ':');
    if(movingbuf != NULL){
        printf("%s\n", movingbuf + 1);
        port = atoi(movingbuf + 1);
        if((port < 0) || (port > 65535)){
			cleanup(fd, -1);
            request_error("Error parsing request: Invalid port found.");
            return; 
        }
        /* Need to set '/' to '\0' to help extract server*/
        *movingbuf = '\0';
    }

	/*Creates the key that will identify the request in the cache*/
	char* key = Calloc(strlen(host) + strlen(path) + 1, sizeof(char));
	strncpy(key, host, strlen(host));
	strncpy(key+strlen(host), path, strlen(path)+1);

    /* Use to ensure the state of the cache is correct. */
    /*
	if((numParsed = Check_cache_single())){ 
        printf("Cache formatting error.%d\n\n", numParsed);
        exit(-1);
    }*/

	/*If requested object not in cache, forward request to host*/
	if (search_cache(key, fd)) {
		forwardit(host, path, port, fd, &rio, key);
	}
	cleanup(fd, -1);
	Free(key);
    return;
}

/*
 * forwardit - forwards an http request to the host server.
 *          host should be http://host...
 *          path should be /...
 *          port should be -1 if no port found, otherwise port in url
 *          fd should the the connection file descriptor
 */
int forwardit(char *host, char *path, int port, int fd, rio_t *rio, char* key){
    int hostfd;
    int pathlength;
    char buf[MAXLINE] = {0};
    char *buf_ptr;
    char full_request[MAX_OBJECT_SIZE] = {0};
    rio_t hostrio;

    /* Reform request in buf */
    strcpy(buf, "GET ");
    buf_ptr = buf + 4;
    pathlength = strlen(path);
    strcpy(buf_ptr, path);
    buf_ptr = buf_ptr + pathlength;
    strcpy(buf_ptr, " HTTP/1.0\r\n");

    /* If no port provided, use standard internet port. */
    if(port == -1){
        port = 80;
    }

    /* Open the connection to the web server using the host. */
    hostfd = Open_clientfd(host+7, port);
    if(hostfd < 0){
		request_error("Error getting host.\n");
		cleanup(fd, hostfd);
        return hostfd;
    }
    
    /* Write the request to the server */
    strcpy(full_request, buf);
           // printf("Formed string to send: %s\n", buf);  

    /* This function sends all the appropriate headers to the server. */
    read_requesthdrs(rio, full_request, host + 7, hostfd);
    
    /* Use for debugging: Prints the fully formed request. */
    //printf("\n\nFull Request:\n\n%s\n\n", full_request);

    /* Send all of it as one big request*/
    Rio_writen(hostfd, full_request, strlen(full_request));

    /* Reads the response from the hostfd (the host server) */
    Rio_readinitb(&hostrio, hostfd);

    /* This function extracts all of the appropriate response headers. */
    read_responsehdrs(&hostrio, fd, key);
    Close(hostfd);

    return 0;
}
    
/*
 * read_requesthdrs - reads all of the additional request headers and ADDs 
 *                    them to the char * full_request. Full_request should
 *                    already have the GET request in it.
 *                    Ignore all of the headers sent by browser and send our 
 *                    own.
 */
void read_requesthdrs(rio_t * client_rio, char *full_request,
                       char *host, int hostfd)
{
    char hdrHost[MAXLINE] = {0};

    strcpy(hdrHost, "Host: ");
    strcpy(hdrHost+6, host);
    strcat(full_request, hdrHost);
    strcat(full_request, finish_request);
    strcat(full_request, (char *) user_agent);
    strcat(full_request, (char *) accept_s);
    strcat(full_request, (char *) accept_encoding);
    strcat(full_request, (char *) connection_s);
    strcat(full_request, (char *) proxy_s);
    strcat(full_request, finish_request);

    return; 
}

/*
 *  read_responsehdrs - read and parse HTTP request headers
 *                      at this point, the HTTP 200.OK has already been read
 *                      out of the buffer.
 *   */
void read_responsehdrs(rio_t *rp, int clientfd, char* key)
{
    char buf[MAXLINE];
    char content[10000];
	char cachebuf[MAX_OBJECT_SIZE + 1] = {0};
	char headersbuf[MAX_HEADERS_SIZE + 1] = {0};
    int cache_buf_valid = 1;
    int headers_buf_valid = 1;
	int size = 0;
    int numbytesread = 0;

    numbytesread = Rio_readlineb(rp, buf, MAXLINE);
    if(numbytesread >= 0){
        size = numbytesread;
	    strncpy(headersbuf, buf, numbytesread);
        Rio_writen(clientfd, buf, numbytesread);
    }

    while(strcmp(buf, "\r\n")) {
        numbytesread = Rio_readlineb(rp, buf, MAXLINE);

        if(numbytesread < 0){break;}

		if ((headers_buf_valid) && (size <= MAX_HEADERS_SIZE)){
			memcpy(headersbuf+size, buf, numbytesread); 
        }
        else
            headers_buf_valid = 0;

        size += numbytesread;
        Rio_writen(clientfd, buf, numbytesread);
    }

    size = 0; 
    while((numbytesread = Rio_readnb(rp, content, 10000))){
        if(numbytesread < 0){ break; }

        Rio_writen(clientfd, content, numbytesread);

		if (cache_buf_valid && (size <= MAX_OBJECT_SIZE)){
			memcpy(cachebuf+size, content, numbytesread);
        }
        else{
            cache_buf_valid = 0;
        }

        size += numbytesread;
    }

	if(headers_buf_valid && cache_buf_valid && size > 0) {
		add_to_cache(key, headersbuf, cachebuf, size);

        /* DEBUG: Use to check the state of the cache after add. */
		/*if((numbytesread = Check_cache_single())){
            printf("Size: %d\n",size); 
            printf("Cache formatting error responsehdrs.%d\n\n", numbytesread);
            exit(-1);
        }
        */
	}
    return;
}

/*Frees up descriptors in use*/
void cleanup(int firstfd, int secondfd) {
	if (firstfd >= 0) Close(firstfd);
	if (secondfd >= 0) Close(secondfd);
	Pthread_exit(NULL);
}

/******* CACHE FUNCTIONS ******/
void init_cache() {
	/*Create dummy node, initialize all fields to NULL or 0 */
	cache_obj dummy_node = Calloc(1, sizeof(struct cache_object));
    dummy_node->key = NULL;
    dummy_node->hdrs = NULL;
    dummy_node->data = NULL;
    dummy_node->next = NULL;
    dummy_node->size = 0;
	
	cache = Malloc(sizeof(struct cache_queue));
	cache->front = dummy_node;
	cache->back = dummy_node;
	cache->size = 0;
	init_lock(&cache->lock);
}

void add_to_cache(char* key, char* hdrs, char* data, int size) {
	P(&mutex);
	write_lock();
	/*Evicts if necessary until there is enough space to cache */
	while ((MAX_CACHE_SIZE - cache->size) < size) {
		cache_obj first = cache->front->next;
		cache->front->next = first->next;
		cache->size -= first->size;
		free_obj(first);
    }
	/*Initialize fields of new_node */
	cache_obj new_node = Malloc(sizeof(struct cache_object));
	new_node->key = Calloc(strlen(key) + 1, sizeof(char)); 
    strcpy(new_node->key, key);
	new_node->hdrs = Calloc(strlen(hdrs) + 1, sizeof(char));
    strcpy(new_node->hdrs, hdrs);	
	new_node->data = Calloc(size + 1, sizeof(char));
    memcpy(new_node->data, data, size);
	new_node->size = size;
	new_node->next = NULL;
	
	/*Add it to the back of cache*/
	cache->back->next = new_node;
	cache->back = new_node;
	cache->size += size;
	unlock();
	V(&mutex);
}

int search_cache(char* key, int clientfd) {
	read_lock();
	cache_obj cur = cache->front->next;
	cache_obj prev = cache->front;
	for (; cur; cur = cur->next) {
		/*If next matches key, object is found */
		if (!strcmp(cur->key,key)) {
			/*Serve object to client*/
			Rio_writen(clientfd, cur->hdrs, strlen(cur->hdrs));
		    Rio_writen(clientfd, cur->data, cur->size);
			P(&mutex);
			unlock();
			/*Crazy stuff here*/
			write_lock();
			prev->next = cur->next;
			cache->back->next = cur;
			cache->back = cur;
			cur->next = NULL;
			unlock();
			V(&mutex);
			return 0;
		}
		prev = cur;
	}
	/*If reached here, key not found in cache. */
	unlock();
	return -1;
}

void free_obj(cache_obj object) {
	free(object->key);
	free(object->hdrs);
	free(object->data);
	free(object);
}

int Check_cache_single() {
	int mochi;
	read_lock();
	mochi = check_cache_single();
	unlock();
	return mochi;
}


int check_cache_single(){
    int accum_size = 0;
    int back_seen = 0;
    struct cache_object* temp;
	if (cache == NULL) return -1;
	if (cache->front == NULL || cache->back == NULL) return -2;

    // Check if dummy node is well formed.
	if (cache->front->size != 0) return -3;
	if (cache->front->key != NULL) return -4;
	if (cache->front->hdrs != NULL) return -5;
	if (cache->front->data != NULL) return -6;

    // Check if there are nodes after the back.
	if (cache->back->next != NULL) return -7;

    // If there is nothing cached, size should be 0.
	if ((cache->front == cache->back) && (cache->size != 0)) return -8;

    if (cache->size != 0){
        // If something cached, front != back.
		if(cache->front == cache->back) return -9;
        temp = cache->front->next;
        while(temp != NULL){
            // Check if the cached objects are well formed.
			if (temp->key == NULL) return -10;
			if (temp->data == NULL) return -11;
            if (temp->size == 0) return -12;
            if (temp->size > MAX_OBJECT_SIZE) return -14;

            // Checks if we've exceeded cache size or infinite loop.
            accum_size += temp->size;
            if (accum_size > MAX_CACHE_SIZE) return -15;

            if (temp == cache->back) back_seen = 1;
            temp = temp->next; 
        }
        if (!back_seen) return -16;
    }

    // Checks cache size is correct.
    if (accum_size != cache->size){ return -17; }
    
    return 0;
}


/******* LOCK WRAPPER FUNCTIONS ******/
void init_lock(pthread_rwlock_t* lock) {
	if (pthread_rwlock_init(lock, NULL) != 0) {
		fprintf(stderr, "Error: Init lock failed.\n");
		exit(-1);
	}
}

void write_lock() {
	if (pthread_rwlock_wrlock((pthread_rwlock_t*) (&cache->lock)) != 0) {
		fprintf(stderr, "Error: Write lock failed.\n");
		exit(-1);
	}
}

void read_lock() {
	if (pthread_rwlock_rdlock((pthread_rwlock_t*) (&cache->lock)) != 0) {
		fprintf(stderr, "Error: Read lock failed.\n");
		exit(-1);
	}
}

void unlock() {
	if (pthread_rwlock_unlock((pthread_rwlock_t*) (&cache->lock)) != 0) {
		fprintf(stderr, "Error: Unlock failed.\n");
		exit(-1);
	}
}
