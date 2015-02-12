/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the 
 *     GET method to serve static and dynamic content.
 */
#include "csapp.h"

#define MAX_CACHE_SIZE 10000
typedef struct cache_object* cache_obj;

/* Cache struct */
struct cache_object {
	char* name;
    void* handle;
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
char scratch[MAXLINE];
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

void* handle_request(void* arg);
void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *function_name, char *cgiargs);
void serve_static(int fd, char *function_name, int filesize);
void get_filetype(char *function_name, char *filetype);
void serve_dynamic(int fd, char *function_name, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, 
        char *shortmsg, char *longmsg);
void cleanup(int fd);
void init_cache();
void* add_to_cache(char* name, void* handle, int size);
void* search_cache(char* name, int fd, char* cgiargs);
cache_obj create_node(char* name, void* handle, int size);
void evict_lru();
/*Lock wrapper functions*/
void init_lock(pthread_rwlock_t* lock);
void read_lock();
void write_lock();
void unlock();


int main(int argc, char **argv) 
{
    int listenfd, port, clientlen;
    int* connfd_ptr;
    struct sockaddr_in clientaddr;
    pthread_t tid;
    
    printf("%x\n", mutex);

    init_cache();
    /* Check command line args */
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }
    port = atoi(argv[1]);

    listenfd = Open_listenfd(port);
    while (1) {
        clientlen = sizeof(clientaddr);
        connfd_ptr = (int*) Malloc(sizeof(int));
        *connfd_ptr = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        printf("Connection made.\n");
        Pthread_create(&tid, NULL, handle_request, (void*) connfd_ptr);
    }
}
/* $end tinymain */

/*Small wrapper to parse/forward the request that fits Pthread specs*/
void* handle_request(void* connfd_ptr) {
    int fd = *((int*) connfd_ptr);
    Free(connfd_ptr);
	doit(fd);
	return NULL;
}

/*
 * doit - handle one HTTP request/response transaction
 */
/* $begin doit */
void doit(int fd) 
{
    int is_static;
    struct stat sbuf;
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char function_name[MAXLINE], cgiargs[MAXLINE];
    rio_t rio;

    /* This thread doesn't have to be joined */
    Pthread_detach(Pthread_self());

    /* Read request line and headers */
    Rio_readinitb(&rio, fd);
    Rio_readlineb(&rio, buf, MAXLINE);
    sscanf(buf, "%s %s %s", method, uri, version);

    printf("Scanned input. %s\n", buf);
    if (strcasecmp(method, "GET")) { 
        clienterror(fd, method, "501", "Not Implemented",
                "Tiny does not implement this method");
        cleanup(fd);
        return;
    }
    read_requesthdrs(&rio);

    /* Parse URI from GET request */
    is_static = parse_uri(uri, function_name, cgiargs);
    #ifdef OLD
    if (stat(function_name, &sbuf) < 0) {
        clienterror(fd, function_name, "404", "Not found",
                "Tiny couldn't find this file");
        cleanup(fd);
        return;
    }
    #endif
    if (is_static) { /* Serve static content */
        if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) {
            clienterror(fd, function_name, "403", "Forbidden",
                    "Tiny couldn't read the file");
            cleanup(fd);
            return;
        }
        serve_static(fd, function_name, sbuf.st_size);
    }
    else { /* Serve dynamic content */
        #ifdef OLD
        if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) {
            clienterror(fd, function_name, "403", "Forbidden",
                    "Tiny couldn't run the CGI program");
            cleanup(fd);
            return;
        }
        #endif
        serve_dynamic(fd, function_name, cgiargs);
    }
    cleanup(fd);
}
/* $end doit */

/*
 * read_requesthdrs - read and parse HTTP request headers
 */
/* $begin read_requesthdrs */
void read_requesthdrs(rio_t *rp) 
{
    char buf[MAXLINE];

    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
    while(strcmp(buf, "\r\n")) {
        Rio_readlineb(rp, buf, MAXLINE);
        printf("%s", buf);
    }
    return;
}
/* $end read_requesthdrs */

/*
 * parse_uri - parse URI into function_name and CGI args
 *             return 0 if dynamic content, 1 if static
 */
/* $begin parse_uri */
int parse_uri(char *uri, char *function_name, char *cgiargs) 
{
    char *ptr;

    if (!strstr(uri, "cgi-bin")) {  /* Static content */
        strcpy(cgiargs, "");
        strcpy(function_name, ".");
        strcat(function_name, uri);
        if (uri[strlen(uri)-1] == '/')
            strcat(function_name, "home.html");
        return 1;
    }
    else {  /* Dynamic content */
        ptr = index(uri, '?');
        if (ptr) {
            strcpy(cgiargs, ptr+1);
            *ptr = '\0';
        }
        else 
            strcpy(cgiargs, "");
        //strcpy(function_name, ".");

        // Skip the "/cgi-bin/" portion and jump to the program
        strcat(function_name, uri+9);
        return 0;
    }
}
/* $end parse_uri */

/*
 * serve_static - copy a file back to the client 
 */
/* $begin serve_static */
void serve_static(int fd, char *function_name, int filesize) 
{
    int srcfd;
    char *srcp, filetype[MAXLINE], buf[MAXBUF];

    /* Send response headers to client */
    get_filetype(function_name, filetype);
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
    sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
    sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
    Rio_writen(fd, buf, strlen(buf));

    /* Send response body to client */
    srcfd = Open(function_name, O_RDONLY, 0);
    srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
    Close(srcfd);
    Rio_writen(fd, srcp, filesize);
    Munmap(srcp, filesize);
}

/*
 * get_filetype - derive file type from file name
 */
void get_filetype(char *function_name, char *filetype) 
{
    if (strstr(function_name, ".html"))
        strcpy(filetype, "text/html");
    else if (strstr(function_name, ".gif"))
        strcpy(filetype, "image/gif");
    else if (strstr(function_name, ".jpg"))
        strcpy(filetype, "image/jpeg");
    else
        strcpy(filetype, "text/plain");
}  
/* $end serve_static */

size_t getfilesize(char* filename) {
    struct stat st;
    stat(filename, &st);
    return st.st_size;
}

/*
 * serve_dynamic - run a CGI program on behalf of the client
 */
/* $begin serve_dynamic */
void serve_dynamic(int fd, char *function_name, char *cgiargs) 
{
    char buf[MAXLINE], *emptylist[] = { NULL };

    void *handle;
    void (*function)(int, char*);
    char *error; 
    size_t size;

    /* Return first part of HTTP response */
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Server: Tiny Web Server\r\n");
    Rio_writen(fd, buf, strlen(buf));
    
    #ifdef OLD
    /* search_cache finds and evaluates function if cached */
    if ((function = search_cache(function_name, fd, cgiargs)) == NULL) {
        /* else... add to cache and execute here */
        pthread_mutex_lock(&mutex);
        sleep(5);
        printf("Didn't find in cache, opening file\n");
        /* DL_Open the corresponding .so file */
        sprintf(buf, "./lib/%s.so", function_name);
        size = getfilesize(buf);
        if ((handle = dlopen(buf, RTLD_LAZY)) == NULL) {
            sprintf(buf, "%s\n", dlerror());
            Rio_writen(fd, buf, strlen(buf));
            return;
        }

        printf("Opened file and got handle to function\n");
        /* Get the function (from dlysm) and add to cache */
        function = (void (*)(int, char*)) add_to_cache(function_name, handle, size);

        if ((error = dlerror()) != NULL) {
            printf("Invalid function error: %s %s\n", function_name, error);
            sprintf(buf, "Invalid function %s\n", function_name);
            Rio_writen(fd, buf, strlen(buf));
            return;
        }
        
        printf("About to execute function\n");
        /* Execute the function */
        if (function != NULL) {
           function(fd, cgiargs);
        }

        printf("done with function, about to release mutex\n");

        /* At this point the function is complete and in the cache */
        pthread_mutex_unlock(&mutex);
        printf("released mutex, served client\n");
    }
    #endif
}
/* $end serve_dynamic */

/* cleanup -- Frees up descriptors in use and ends thread */
void cleanup(int fd) {
    Close(fd);
	Pthread_exit(NULL);
}

/*
 * clienterror - returns an error message to the client
 */
/* $begin clienterror */
void clienterror(int fd, char *cause, char *errnum, 
        char *shortmsg, char *longmsg) 
{
    char buf[MAXLINE], body[MAXBUF];

    /* Build the HTTP response body */
    sprintf(body, "<html><title>Tiny Error</title>");
    sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

    /* Print the HTTP response */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
    Rio_writen(fd, buf, strlen(buf));
    Rio_writen(fd, body, strlen(body));
}
/* $end clienterror */

/******* CACHE FUNCTIONS ******/
void init_cache() {
	/*Create dummy node, initialize all fields to NULL or 0 */
	cache_obj dummy_node = Calloc(1, sizeof(struct cache_object));
    dummy_node->name = NULL;
    dummy_node->handle = NULL;
    dummy_node->next = NULL;
    dummy_node->size = 0;
	
	cache = Malloc(sizeof(struct cache_queue));
	cache->front = dummy_node;
	cache->back = dummy_node;
	cache->size = 0;
	init_lock(&cache->lock);
}

/* Always called under protection of mutex */
void evict_lru() {
    cache_obj first = cache->front->next;
    cache->front->next = first->next;
    if (first == cache->back)
        cache->back = cache->front;
    cache->size -= first->size;

    /* unload the shared library */
    if (dlclose(first->handle) < 0) {
        fprintf(stderr, "%s\n", dlerror());
    }

    free(first->name);
    free(first);
}

cache_obj create_node(char* name, void* handle, int size) {
	cache_obj new_node = Malloc(sizeof(struct cache_object));

    new_node->name = Calloc(strlen(name)+1, sizeof(char));
    strncpy(new_node->name, name, strlen(name));

	new_node->handle = handle;
	new_node->size = size;
	new_node->next = NULL;
	
    return new_node;
}

/* Always called under protection of mutex */
void* add_to_cache(char* name, void* handle, int size) {
    void* function;
	write_lock();
    printf("Took write lock in add_to_cache\n");
	/*Evicts if necessary until there is enough space to cache */
	while (cache->size > MAX_CACHE_SIZE) 
        evict_lru();

    /* Create new node and add to back of cache */
    cache_obj new_node = create_node(name, handle, size);
	cache->back->next = new_node;
	cache->back = new_node;
	cache->size += size;
	unlock();

    /* Open the shared library containing function name */
    sprintf(scratch, "./lib/%s.so", name);
    
    /* Resolve the function */
    function = dlsym(handle, name);
    printf("Released write lock in add to cache and exiting\n");
    return function;
}

void* search_cache(char* name, int fd, char* cgiargs) {
    void (*function)(int, char*);

	read_lock();
	cache_obj cur = cache->front->next;
    cache_obj prev = cache->front;
	for (; cur; cur = cur->next) {
		/*If next matches key, object is found */
		if (!strcmp(cur->name, name)) {
			pthread_mutex_lock(&mutex);
			unlock(); /* Unlocks the read lock*/
            if (cur != cache->back) {
                /* Takes the write lock to move to the end of the cache */
                write_lock();
                prev->next = cur->next;
                cache->back->next = cur;
                cache->back = cur;
                cur->next = NULL;
                unlock(); /* Unlocks the write lock */
            }
            
            /* Found in the cache, get and execute function */
            function = dlsym(cur->handle, name);

            if (dlerror() != NULL) {
                sprintf(scratch, "Invalid function %s\n", name);
                Rio_writen(fd, scratch, strlen(scratch));
                return NULL;
            }

            function(fd, cgiargs);
            pthread_mutex_unlock(&mutex);
			return cur->handle;
		}
        prev = cur;
	}
	/*If reached here, key not found in cache. */
	unlock();
	return NULL;
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
