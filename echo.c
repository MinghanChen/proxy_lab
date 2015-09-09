#include "csapp.h"
#include "cache.h"

static int byte_cnt;
static sem_t mutex;
/*the flag is used to judge whether a 'Host' request header has been sent by client.*/
static int flag = 0; 

/* You won't lose style points for including these long lines in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static const char *accept_hdr = "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n";
static const char *accept_encoding_hdr = "Accept-Encoding: gzip, deflate\r\n";
static const char *conn_hdr = "Connection: close\r\n";
static const char *prox_hdr = "Proxy-Connection: close\r\n";

/*req struct is used to store the information it parses from the request line. 
Hostname_port store the domain together with the port number. Hostname is used to 
store the domain. Path is used to store the path in the request line.*/
typedef struct {
    char *hostname_port;
    char *hostname;
    char *path;
    char *header;
    char *port;
} req;

/*the init_echo_cnt function will be only called when the thread handles the first request*/
static void init_echo_cnt(void) {
    Sem_init(&mutex, 0, 1);
    byte_cnt = 0;
}

static void free_request(req *req_addr) {
	Free(req_addr -> hostname_port);
	Free(req_addr -> hostname);
	Free(req_addr -> path);
}



/*parse_reqline function is used to parse the request line and then store the result into
corresponding part of a req structure. There are mainly three circumstances to be considered:
1.  The request line provides both the domain and the path
e.g. GET http://www.cmu.edu/hub/index.html HTTP/1.1
2.  The request line only provides the domain without a slash '/' behind it
e.g. GET http://www.cmu.edu HTTP/1.1
3.  The request line only provides the domain with a slash '/' behind it
e.g. GET http://www.cmu.edu/ HTTP/1.1*/
static void parse_reqline(char *str, req *req_addr) {
    char *p, *pathstr, *hostportstr, *temp;
    char *hoststr;

    p = strstr(str, "http://");
    p += strlen("http://");
    temp = strstr(p, " ");
    *temp = '\0';
    /*only domain no path*/
    if (strstr(p, "/") == NULL) {
        hostportstr = (char *)Malloc(strlen(p) + 1);
        strcpy(hostportstr, p);
        req_addr -> hostname_port = hostportstr;
        req_addr -> path = "/";
    } else if (strlen(strstr(p, "/")) == 1) {
        *(temp - 1) = '\0';
        hostportstr = (char *)Malloc(strlen(p) + 1);
        strcpy(hostportstr, p);
        req_addr -> hostname_port = hostportstr;
        req_addr -> path = "/";
    } else {
        temp = strstr(p, "/");
        *temp = '\0';
        hostportstr = (char *)Malloc(strlen(p) + 1);
        strcpy(hostportstr, p);
        temp ++;
        pathstr = (char *)Malloc(strlen(temp) + 2);
        sprintf(pathstr, "/%s", temp);
        req_addr -> hostname_port = hostportstr;
        req_addr -> path = pathstr;
    }

    hoststr = (char *)Malloc(strlen(hostportstr) + 1);
    strcpy(hoststr, hostportstr);
    /*set the port to 80 by default if the request line doesn't designate the port*/
    if (strstr(hoststr, ":") == NULL) {
        req_addr -> hostname = hoststr;
        req_addr -> port = "80";
    } else {
        p = strstr(hoststr, ":");
        *p = '\0';
        req_addr -> hostname = hoststr;
        req_addr -> port = p + 1;
    }
}

/*parse_reqhdr function is used to handle the information in the request header from clients.
At first, it judge if the format is correct. If the format has no problem, the function will
judge whether the request header has the same key with the static constants given above. If so,
the function would simply ignore it. Or it would send it to server. In addition, the function
judges whether the client has designated the Host information. If so, the function would set
the flag to 1.*/
static char *parse_reqhdr(char *buf) {
    char *temp_buf = NULL; // temp_buf is used to store the content in the buf.
    char *p = NULL;
    //printf("enter parse_reqhdr\n");
    temp_buf = (char *)Malloc(strlen(buf) + 1);
    strcpy(temp_buf, buf);
    //printf("the temp_buf : %s\n", temp_buf);
    p = strstr(temp_buf, ":");
    //printf("p is %s\n", p);
    if (p == NULL) {
        printf("parse failure!\n");
        return NULL;
    }
    *p = '\0';
    //printf("after designate null terminator to p\n");
    /*same one would simply return*/
    if (strcmp(temp_buf, "User-Agent") == 0) {
        Free(temp_buf);
        return NULL;
    }
    if (strcmp(temp_buf, "Accept") == 0) {
        Free(temp_buf);
        return NULL;
    }
    if (strcmp(temp_buf, "Accept-Encoding") == 0) {
        Free(temp_buf);
        return NULL;
    }
    if (strcmp(temp_buf, "Connection") == 0) {
        Free(temp_buf);
        return NULL;
    }
    if (strcmp(temp_buf, "Proxy-Connection") == 0) {
        Free(temp_buf);
        return NULL;
    }
    if (strcmp(temp_buf, "Host") == 0) {
        flag = 1;
		//printf("set the flag\n");
    }

    //printf("here...%s\n", buf);
  /*other request header*/
    free(temp_buf);
    return buf;

}

/*forward_req function is used to forward the request information to the server. Firstly,
it will judge whether the information the client requests is stored in the cache. If so,
the function simply return the response it has stored. If not, it will send the information to
the server and then send the response from server to the client. Besides, it will judge whether
the response from server is suitable to store(less than MAX_OBJECT_SIZE). If so, the function
will store it into the cache.*/
static void forward_req(int connfd, char *req_buf, req *req_addr) {
    int clientfd, n;
    int total = 0;
    char receive_buf[MAXLINE];
    char cache[MAX_OBJECT_SIZE];
    proxy_line *p;
    rio_t rio;
    char *name = (char *)Malloc(strlen(req_addr -> hostname_port) + strlen(req_addr -> path) + 1);
    sprintf(name, "%s%s", req_addr -> hostname_port, req_addr -> path);
    memset(cache, '\0', MAX_OBJECT_SIZE);
    /*Lock when read*/
    P(&mutex);
    if ((p = search_cache(name)) != NULL) {
        Rio_writen(connfd, p -> buf, strlen(p -> buf) + 1);
        return;
    }
    V(&mutex);

    clientfd = Open_clientfd(req_addr -> hostname, req_addr -> port);
    Rio_readinitb(&rio, clientfd);
    Rio_writen(clientfd, req_buf, strlen(req_buf) + 1);
    while ((n = Rio_readlineb(&rio, receive_buf, MAXLINE)) != 0) {
        total += n;
        if (total <= MAX_OBJECT_SIZE) {
            sprintf(cache, "%s%s", cache, receive_buf);
        }
        Rio_writen(connfd, receive_buf, n);
    }

     if (total <= MAX_OBJECT_SIZE) {
     		/*Lock when write.*/
            P(&mutex);
            insert(cache, name);
            V(&mutex);
        }
 }
/*deal_req function do the major work. Firstly, it parses the request line from client and
store it into a req struction by calling parse_reqline function. Secondly, it add the five
headers given above into the forward_buf, which stores all the request information to the server.
Thirdly, the function will handle all the request headers sent from client by calling the 
parse_reqhdr(char *buf) function. Finally, when it catch the terminator(\r\n), it will forward
all the information to the server by calling forward_req function.*/
void deal_req(int connfd) {
    int n;
    char buf[MAXLINE];
    char *hdr_pointer = NULL;
    char *forward_buf = NULL; //send to the server
    req request;
    rio_t rio;
    static pthread_once_t once = PTHREAD_ONCE_INIT;

    Pthread_once(&once, init_echo_cnt);
    Rio_readinitb(&rio, connfd);
    /*parse the request line*/

    if (Rio_readlineb(&rio, buf, MAXLINE) > 0) {
        parse_reqline(buf, &request);
    } else {
        printf("wrong with request line\n");
    }

    /*initilization*/
    forward_buf = (char *)Malloc(MAXLINE);
    memset(forward_buf, '\0', strlen(buf) + 1);
    printf("set the forward_buf at the first time\n");

    sprintf(forward_buf, "GET %s HTTP/1.0\r\n", request.path);
    forward_buf = (char *)realloc(forward_buf, strlen(forward_buf) + 1 + strlen(user_agent_hdr)
                    + strlen(accept_hdr) + strlen(accept_encoding_hdr) + strlen(conn_hdr)
                     + strlen(prox_hdr));
    sprintf(forward_buf, "%s%s%s%s%s%s", forward_buf, user_agent_hdr, accept_hdr, accept_encoding_hdr, conn_hdr, prox_hdr);
   // Rio_writen(connfd, forward_buf, strlen(forward_buf) + 1);
    while ((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0) {
        /*request header terminates*/
        if ((strcmp ("\r\n", buf) == 0) || (strcmp("\r", buf) == 0) || (strcmp("\n", buf) == 0)) {
            if (flag == 0) {
                char *host = (char *)Malloc(strlen(request.hostname) + strlen("Host: ") + 3);
                sprintf(host, "Host: %s\r\n", request.hostname);
                forward_buf = realloc(forward_buf, strlen(forward_buf) + strlen(host) + 3);
                sprintf(forward_buf, "%s%s\r\n", forward_buf, host);
            } else {
                forward_buf = realloc(forward_buf, strlen(forward_buf) + 3);
                sprintf(forward_buf, "%s\r\n", forward_buf);
            }
            /*delete the code when test finished*/
           // Rio_writen(connfd, forward_buf, strlen(forward_buf) + 1);
            /*don't forget to free the forward_buf*/
            break;
        } else {
            if ((hdr_pointer = parse_reqhdr(buf)) != NULL) {
                forward_buf = realloc(forward_buf, strlen(forward_buf) + strlen(hdr_pointer) + 1);
                sprintf(forward_buf, "%s%s", forward_buf, hdr_pointer);
            }

        }
    }

    forward_req(connfd, forward_buf, &request);
    Free(forward_buf);
    free_request(&request);

}





