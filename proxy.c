/*
 * proxy.c - A multi-threading proxy server with LRU cache.
 *
 * Author: Hang Gong
 */

#include "csapp.h"
#include "cache.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
#define HOSTLEN 256
#define SERVLEN 8
#define PORTLEN 8


typedef struct {
    struct sockaddr_in addr;    // Socket address
    socklen_t addrlen;          // Socket address length
    int connfd;                 // Client connection file descriptor
    char host[HOSTLEN];         // Client host
    char serv[SERVLEN];         // Client service (port)
} client_info;


static const char *header_user_agent = "Mozilla/5.0 "
                                       "(X11; Linux x86_64; rv:10.0.3) "
                                       "Gecko/20120305 Firefox/10.0.3";

/*
 * parse_uri - parse the uri; save host, port and path back.
 *
 * return: if success, return 0, else return -1.
 */
int parse_uri(char *uri, char *host, char *port, char *path) {
    char *pos;
    if ((pos = strstr(uri, "://")) == NULL) {
        return -1;  // malformed request
    }

    *pos = '\0';
    if (strncmp(uri, "http", sizeof("http"))) {
        return -1;  // not an http request
    }

    // skip '://' three characters
    uri = pos + 3;
    if ((pos = strchr(uri, '/')) == NULL) {
        if (snprintf(host, MAXLINE, "%s", uri) >= MAXLINE) {
            return -1;  // Overflow!
        }
        // use default path "/"
        snprintf(path, MAXLINE, "%s", "/");
    } else {
        *pos = '\0';
        if (snprintf(host, MAXLINE, "%s", uri) >= MAXLINE) {
            return -1;  // Overflow!
        }
        if (snprintf(path, MAXLINE, "/%s", pos + 1) >= MAXLINE) {
            return -1;  // Overflow!
        }
    }

    // check whether client specifies a port number
    if ((pos = strchr(host, ':')) != NULL) {
        *pos = '\0';

        if (snprintf(port, PORTLEN, "%s", pos + 1) >= PORTLEN) {
            return -1;   // Overflow!
        }

    } else {
        // use default "80" port
        snprintf(port, MAXLINE, "%s", "80");
    }
    return 0;
}

/*
 * save_cache - given request, response, and response size,
 *              try to save these information into the cache.
 */
bool save_cache(char *req, char *res, size_t res_len) {
    size_t req_len = strlen(req) + 1;
    char *heap_req = malloc(req_len);
    if (heap_req == NULL) {
        return false;
    }
    strcpy(heap_req, req);
    write_cache(heap_req, res, res_len);
    return true;
}

/*
 * request_gen - generate a HTTP request that the proxy send to the server.
 */
int request_gen(char *request, size_t limit,
                char *host, char *path, char *header) {
    size_t buflen = snprintf(request, limit,
                            "GET %s HTTP/1.0\r\n"
                            "Host: %s\r\n"
                            "Connection: close\r\n"
                            "Proxy-Connection: close\r\n"
                            "User-Agent: %s\r\n"
                            "%s",
                            path, host, header_user_agent, header);

    // check overflow
    return buflen >= limit ? -1 : 0;
}


/*
 * read_requesthdrs - read HTTP request headers
 *
 * If success, save the the additional header back and return false,
 * else return true;
 */
bool read_requesthdrs(rio_t *rp, char *header) {
    char buf[MAXLINE];
    size_t total = 0;
    size_t line_len;
    ssize_t num_read;
    do {
        if ((num_read = rio_readlineb(rp, buf, MAXLINE)) <= 0) {
            return true;
        }
        line_len = (size_t)num_read;
        if (total + line_len >= MAXLINE) {
            return false;
        }

        // check for header overlap
        if (strstr(buf, "Connection:")
          || strstr(buf, "Proxy-Connection:")
          || strstr(buf, "User-Agent:")
          || strstr(buf, "Host:")) {
            continue;
        }

        memcpy(header + total, buf, line_len);
        total += line_len;

    } while (strncmp(buf, "\r\n", sizeof("\r\n")));

    return false;
}


/*
 * clienterror - returns an error message to the client
 */
void clienterror(int fd, char *cause, char *errnum,
                 char *shortmsg, char *longmsg) {
    char buf[MAXLINE];
    char body[MAXBUF];
    size_t buflen;
    size_t bodylen;

    /* Build the HTTP response body */
    bodylen = snprintf(body, MAXBUF,
        "<!DOCTYPE html>\r\n"
        "<html>\r\n"
        "<head><title>Tiny Error</title></head>\r\n"
        "<body bgcolor=\"ffffff\">\r\n"
        "<h1>%s: %s</h1>\r\n"
        "<p>%s: %s</p>\r\n"
        "<hr /><em>The Proxy Server</em>\r\n"
        "</body></html>\r\n",
        errnum, shortmsg, longmsg, cause);
    if (bodylen >= MAXBUF) {
        return; // Overflow!
    }

    /* Build the HTTP response headers */
    buflen = snprintf(buf, MAXLINE,
        "HTTP/1.0 %s %s\r\n"
        "Content-Type: text/html\r\n"
        "Content-Length: %zu\r\n\r\n",
        errnum, shortmsg, bodylen);
    if (buflen >= MAXLINE) {
        return; // Overflow!
    }

    /* Write the headers */
    if (rio_writen(fd, buf, buflen) < 0) {
        return;
    }

    /* Write the body */
    if (rio_writen(fd, body, bodylen) < 0) {
        return;
    }
}

/*
 * serve - handle one entire HTTP request/response proxy communication
 */
void serve(client_info *client) {
    rio_t rio_client, rio_server;
    int clientfd, serverfd;

    clientfd = client->connfd;
    rio_readinitb(&rio_client, clientfd);

    /* Read request line */
    char buf[MAXLINE];
    if (rio_readlineb(&rio_client, buf, MAXLINE) <= 0) {
        return;
    }

    // print the request header
    // printf("%s", buf);

    /* Parse the request line and check if it's well-formed */
    char method[MAXLINE];
    char uri[MAXLINE];
    char version;

    /* sscanf must parse exactly 3 things
     * for request line to be well-formed */
    /* version must be either HTTP/1.0 or HTTP/1.1 */
    if (sscanf(buf, "%s %s HTTP/1.%c", method, uri, &version) != 3
        || (version != '0' && version != '1')) {
        clienterror(clientfd, buf, "400", "Bad Request",
            "Proxy received a malformed request");
        return;
    }

    /* Check that the method is GET */
    if (strncmp(method, "GET", sizeof("GET"))) {
        clienterror(clientfd, method, "501", "Not Implemented",
            "Proxy does not implement this method");
        return;
    }

    /* parse host address and path from uri */
    char host[MAXLINE], port[PORTLEN], path[MAXLINE], header[MAXLINE];
    if (parse_uri(uri, host, port, path) < 0) {
        clienterror(clientfd, method, "400", "Bad Request",
            "Proxy received a malformed request");
        return;
    }

    /* Check if reading request headers caused an error */
    if (read_requesthdrs(&rio_client, header)) {
        clienterror(clientfd, buf, "400", "Bad Request",
            "Proxy received a malformed request");
        return;
    }

    // All checks are done. First read the cache.

    // Rebuild the request path, format as host:port/path
    char req[MAXLINE];
    snprintf(req, MAXLINE, "%s:%s%s", host, port, path);
    size_t res_size;
    char *res = read_cache_begin(req, &res_size);

    // cache hit!
    if (res) {
        rio_writen(clientfd, res, res_size);
        read_cache_end(req);
        return;
    }

    // Cache miss, generate new request
    read_cache_end(req);
    char request[MAXLINE];
    if (request_gen(request, MAXLINE, host, path, header) < 0) {
        clienterror(clientfd, buf, "500", "Internal Server Error",
            "Proxy cannot generate new request");
        return;
    }

    // Open socket connection to server
    if ((serverfd = open_clientfd(host, port)) < 0) {
        fprintf(stderr, "Error connecting to %s:%s\n", host, port);
        clienterror(clientfd, method, "503", "Service Unavailable",
            "Proxy cannot connect to the server");
        return;
    }

    // Initialize RIO read structure
    rio_readinitb(&rio_server, serverfd);

    // Write to server
    if (rio_writen(serverfd, request, strlen(request)) < 0) {
        // fprintf(stderr, "Error sending request to server\n");
        clienterror(clientfd, method, "503", "Service Unavailable",
            "Proxy cannot send request to the server");
        close(serverfd);
        return;
    }

    // Read back response
    ssize_t num_read = 0;
    size_t res_len = 0;
    size_t line_len = 0;
    bool malloc_success = false;

    // Allocate memory for caching the response
    char *res_buf = malloc(MAX_OBJECT_SIZE);
    if (res_buf) {
        malloc_success = true;
    }

    // keep reading the response
    while ((num_read = rio_readlineb(&rio_server, buf, MAXLINE)) > 0) {
        line_len = (size_t)num_read;
        if (rio_writen(clientfd, buf, line_len) < 0) {
            // client has closed the socket.
            free(res_buf);
            close(serverfd);
            return;
        }

        // check the response size and append buf after res_buf
        if (malloc_success && res_len + line_len < MAX_OBJECT_SIZE ) {
            // buf may contains '\0'!!! Can't use anything other than memcpy!!!
            memcpy(res_buf + res_len, buf, line_len);
        }
        res_len += line_len;
    }

    close(serverfd);

    // save cache successfully
    if (malloc_success
        && res_len < MAX_OBJECT_SIZE
        && save_cache(req, res_buf, res_len)) {
        return;
    }
    // fail to save cache, free the memory
    free(res_buf);
}

/*
 * run - a thread runner function
 */
void *run(void *arg) {
    // get the argument
    client_info *client = arg;

    // detach itself
    pthread_detach(pthread_self());

    // call the serve function
    serve(client);

    // close the client socket connection
    close(client->connfd);

    // free the memory
    free(arg);
    return NULL;
}


int main(int argc, char **argv) {

    /* Check command line args */
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }

    // initialize the cache
    cache_init(MAX_CACHE_SIZE);

    // listen to the port
    int listenfd = Open_listenfd(argv[1]);

    // ignore SIGPIPE error
    Signal(SIGPIPE, SIG_IGN);

    while (1) {
        // Allocate space on the heap for client info
        client_info *client = malloc(sizeof(*client));

        /* handle malloc() error */
        if (client == NULL) {
            continue;
        }

        /* Initialize the length of the address */
        client->addrlen = sizeof(client->addr);

        /* accept() will block until a client connects to the port */
        client->connfd = accept(listenfd,
                                (SA *) &client->addr, &client->addrlen);

        // handle accept() error
        if (client->connfd < 0) {
            free(client);
            continue;
        }

        // The thread we're about to create
        pthread_t thread;

        /* Connection is established; serve client */
        pthread_create(&thread, NULL, run, client);
    }
}
