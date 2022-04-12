#include <stdio.h>
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
#define MMAXLINE 2048
#define MAXHEADER 4096
#define MAXCACHEBLOCK 50

struct cache_block
{
    char header[MAXHEADER];
    char path[MMAXLINE];
    char *body;
    int body_size;
    char visit;
};

struct _cache
{
    struct cache_block *block_slot[MAXCACHEBLOCK];
    int clock_p;
    int used_size;
    int used_block;
} cache;

int read_cnt;
sem_t mutex, write_lock;

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3";

void doit(int connfd);
void request_back_server(rio_t *rio_buf, char *host, char *port, char *path, char *real_path);
void http_error(int fd, char *errnum, char *msg);
int parse_request(char *request, char *host, char *port, char *path);
void deal_request_header(char *header_line, int back_server_fd);
void get_back_response_and_send_to_client(int fd_conn_back_server, int client_fd, char *path);
void html_error(int fd, char *msg);
void *thread(void *vargp);
void init_cache();
int read_cache(char *header, char *body, char *real_path);
int write_cache(char *header, char *body, int body_size, char *real_path);
int next_slot(int need_size);
void evict();

int main(int argc, char **argv)
{
    //printf("%s\n", user_agent_hdr);
    int listenfd, connfd;
    pthread_t tid;
    int *vargs = NULL;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    /* Check command line args */
    if (argc != 2)
    {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }

    init_cache();
    listenfd = Open_listenfd(argv[1]);
    while (1)
    {
        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        //doit(connfd);

        if ((vargs = (int *)malloc(sizeof(int))) == NULL)
        {
            http_error(connfd, "500", "not enough memory!");
            Close(connfd);
        }
        else
        {
            *vargs = connfd;
            pthread_create(&tid, NULL, thread, (void *)vargs);
        }
    }
}

void *thread(void *vargp)
{
    int connfd = *((int *)vargp);
    pthread_detach(pthread_self());
    free(vargp);
    doit(connfd);
    Close(connfd);
    return NULL;
}

void doit(int connfd)
{
    rio_t Riobuf;
    char req_buf[MMAXLINE], host[MMAXLINE], port[MMAXLINE], path[MMAXLINE], real_path[MAXLINE];
    char *header, *body;
    int body_size;
    rio_readinitb(&Riobuf, connfd);
    if (!rio_readlineb(&Riobuf, req_buf, MMAXLINE))
        return;

    if (parse_request(req_buf, host, port, path) < 0)
        http_error(connfd, "403", "Unknown request");
    else
    {
        sprintf(real_path, "%s:%s%s", host, port, path);
        header = (char *)malloc(MAXHEADER);
        body = (char *)malloc(MAX_OBJECT_SIZE);
        if (header != NULL && body != NULL)
        {
            if ((body_size = read_cache(header, body, real_path)) >= 0)
            {
                Rio_writen(connfd, header, strlen(header));
                Rio_writen(connfd, "\r\n", strlen("\r\n"));
                Rio_writen(connfd, body, body_size);
                free(header);
                free(body);
                return;
            }
        }
        free(header);
        free(body);
        request_back_server(&Riobuf, host, port, path, real_path);
    }
}

/*根据第一行的请求解析出host，port与请求path，正确返回0，出错返回-1*/
int parse_request(char *request, char *host, char *port, char *path)
{
    char method[MMAXLINE], path_buf[MMAXLINE], version[MMAXLINE], host_buf[MMAXLINE], port_buf[MMAXLINE];
    char *i, *host_buf_p, *port_buf_p;
    int have_port = 1;
    memset(path_buf, 0, MMAXLINE);
    sscanf(request, "%s %s %s", method, path_buf, version);
    if (strcmp(method, "GET"))
        return -1;
    i = path_buf;
    host_buf_p = host_buf;
    port_buf_p = port_buf;
    while (*i != '/' && *i != '\0')
        i++;
    if (*i != '\0')
        i++;
    if (*i != '\0')
        i++;
    while (*i != ':' && *i != '\0')
    {
        if (*i == '/')
        {
            have_port = 0;
            break;
        }
        *host_buf_p++ = *i++;
    }
    *host_buf_p = '\0';
    if (!have_port)
    {
        sprintf(port_buf_p, "80");
    }
    else
    {
        i++;
        while (*i != '/' && *i != '\0')
        {
            *port_buf_p++ = *i++;
        }
        *port_buf_p = '\0';
    }

    if (*i == '\0')
        return -1;

    sprintf(host, "%s", host_buf);
    sprintf(port, "%s", port_buf);
    sprintf(path, "%s", i);
    //sprintf(request, "%s %s %s\r\n", method, i, "HTTP/1.0");
    return 0;
}

/*解析剩下的头部，构造并发送向后台服务器的请求,最后接受请求并返回给客户端*/
void request_back_server(rio_t *rio_buf, char *host, char *port, char *path, char *real_path)
{
    int fd_conn_back_server, fd_conn_client;
    char buf[MMAXLINE];

    fd_conn_client = rio_buf->rio_fd;
    fd_conn_back_server = Open_clientfd(host, port);

    sprintf(buf, "GET %s HTTP/1.0\r\n", path);
    Rio_writen(fd_conn_back_server, buf, strlen(buf));
    sprintf(buf, "Host: %s:%s\r\n", host, port);
    Rio_writen(fd_conn_back_server, buf, strlen(buf));
    sprintf(buf, "User-Agent: %s\r\n", user_agent_hdr);
    Rio_writen(fd_conn_back_server, buf, strlen(buf));
    sprintf(buf, "Connection: close\r\n");
    Rio_writen(fd_conn_back_server, buf, strlen(buf));
    sprintf(buf, "Proxy-Connection: close\r\n");
    Rio_writen(fd_conn_back_server, buf, strlen(buf));

    while (1)
    {
        Rio_readlineb(rio_buf, buf, MMAXLINE);
        if (!strcmp(buf, "\r\n"))
        {
            Rio_writen(fd_conn_back_server, buf, strlen(buf));
            break;
        }
        deal_request_header(buf, fd_conn_back_server);
    }

    get_back_response_and_send_to_client(fd_conn_back_server, fd_conn_client, real_path);
    //Close(connfd);
    return;
}

/*处理请求头,忽略给定的头（需要修改的头）,并把其余头发送给后台服务器*/
void deal_request_header(char *header_line, int back_server_fd)
{
    char key[MMAXLINE], value[MMAXLINE];
    sscanf(header_line, "%s %s", key, value);
    if (!strcmp(key, "Host:"))
        return;
    else if (!strcmp(key, "User-Agent:"))
    {
        return;
    }
    else if (!strcmp(key, "Connection:"))
    {
        return;
    }
    else if (!strcmp(key, "Proxy-Connection:"))
    {
        return;
    }
    else
    {
        Rio_writen(back_server_fd, header_line, strlen(header_line));
        return;
    }
}

/*读后台的返回，并且写给客户端*/
void get_back_response_and_send_to_client(int fd_conn_back_server, int client_fd, char *real_path)
{
    int body_length, body_length_record;
    ssize_t read_size;
    rio_t rio_buf;
    char buf[MMAXLINE], key[MMAXLINE], value[MMAXLINE];
    char *body_data, *temp_body_data_ptr, *temp_buf_ptr, *header_data;
    Rio_readinitb(&rio_buf, fd_conn_back_server);

    if ((header_data = (char *)malloc(MAXHEADER)) == NULL)
    {
        http_error(client_fd, "500", "not enough memory");
        return;
    }

    Rio_readlineb(&rio_buf, buf, MMAXLINE); //返回码
    Rio_writen(client_fd, buf, strlen(buf));
    sprintf(header_data, "%s", buf);

    while (1)
    { /*读头部*/
        Rio_readlineb(&rio_buf, buf, MMAXLINE);
        if (!strcmp(buf, "\r\n"))
        {
            Rio_writen(client_fd, buf, strlen(buf));
            break;
        }
        //deal_request_header(buf, fd_conn_back_server);
        sscanf(buf, "%s %s", key, value);
        if (!strcmp(key, "Content-length:"))
        {
            body_length = atoi(value);
        }
        Rio_writen(client_fd, buf, strlen(buf));
        strcat(header_data, buf);
    }

    if (body_length > MAX_OBJECT_SIZE) //不能缓存
    {
        while (body_length > 0)
        {
            read_size = Rio_readnb(&rio_buf, buf, MMAXLINE);
            body_length -= read_size;
            Rio_writen(client_fd, buf, read_size);
        }
        free(header_data);
    }
    else //可以缓存
    {
        if ((body_data = (char *)malloc(body_length)) == NULL)
        {
            html_error(client_fd, "not enough memory");
            //Close(client_fd);
            Close(fd_conn_back_server);
            return;
        }
        body_length_record = body_length;
        temp_body_data_ptr = body_data;
        while (body_length > 0)
        {
            read_size = Rio_readnb(&rio_buf, buf, MMAXLINE);
            body_length -= read_size;

            Rio_writen(client_fd, buf, read_size);

            temp_buf_ptr = buf; //写局部缓存
            while (read_size > 0)
            {
                *temp_body_data_ptr++ = *temp_buf_ptr++;
                read_size--;
            }
        }
        write_cache(header_data, body_data, body_length_record, real_path);
        free(header_data);
        free(body_data);
    }

    Close(fd_conn_back_server);
    //Rio_writen(client_fd, )
}
/*如函数名*/
void http_error(int fd, char *errnum, char *msg)
{
    char buf[MMAXLINE];

    /* Print the HTTP response headers */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, msg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n\r\n");
    Rio_writen(fd, buf, strlen(buf));

    /* Print the HTTP response body */
    sprintf(buf, "<html><title>Request Error</title>");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "<body bgcolor="
                 "ffffff"
                 ">\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "%s: %s\r\n", errnum, msg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "<hr><em>The Tiny Proxy server</em>\r\n");
    Rio_writen(fd, buf, strlen(buf));
}

void html_error(int fd, char *msg)
{
    char buf[MMAXLINE];

    sprintf(buf, "<html><title>Request Error</title>");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "<body bgcolor="
                 "ffffff"
                 ">\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "%s\r\n", msg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "<hr><em>The Tiny Proxy server</em>\r\n");
    Rio_writen(fd, buf, strlen(buf));
}

void init_cache()
{
    memset(cache.block_slot, 0, MAXCACHEBLOCK * sizeof(struct cache_block *));
    cache.clock_p = 0;
    cache.used_block = 0;
    cache.used_size = 0;
    read_cnt = 0;
    sem_init(&mutex, 0, 1);
    sem_init(&write_lock, 0, 1);
}

/*找到返回body_size，并且写header与body，找不到返回-1*/
int read_cache(char *header, char *body, char *real_path)
{
    P(&mutex);
    if (++read_cnt == 1)
        P(&write_lock);
    V(&mutex);

    int i, find = 0;
    for (i = 0; i < MAXCACHEBLOCK; i++)
    {
        if (cache.block_slot[i] != NULL && !strcmp(cache.block_slot[i]->path, real_path))
        {
            find = 1;
            break;
        }
    }

    if (find)
    {
        struct cache_block *block = cache.block_slot[i];
        sprintf(header, "%s", block->header);
        memcpy(body, block->body, block->body_size);
        block->visit = 1;
        find = block->body_size;
    }
    else
        find = -1;

    P(&mutex);
    if (--read_cnt == 0)
        V(&write_lock);
    V(&mutex);

    return find;
}

/*成功返回1，失败返回0*/
int write_cache(char *header, char *body, int body_size, char *real_path)
{
    P(&write_lock);

    int slot;
    struct cache_block *block;

    slot = next_slot(body_size);
    block = cache.block_slot[slot] = (struct cache_block *)malloc(sizeof(struct cache_block));

    sprintf((block->path), "%s", real_path);
    sprintf((block->header), "%s", header);
    block->visit = 1;
    block->body_size = body_size;
    cache.used_block++;
    cache.used_size += body_size;
    if ((block->body = (char *)malloc(body_size)) == NULL)
    {
        V(&write_lock);
        return 0;
    }
    memcpy(block->body, body, body_size);
    V(&write_lock);
    return 1;
}

/*write已经获得了写锁，所以next_slot与evict都不用再加锁*/
int next_slot(int need_size)
{
    while (cache.used_size + need_size > MAX_CACHE_SIZE || cache.used_block >= MAXCACHEBLOCK)
        evict();
    while (cache.block_slot[cache.clock_p] != NULL)
        cache.clock_p = (cache.clock_p + 1) % MAXCACHEBLOCK;
    return cache.clock_p;
}

void evict()
{
    struct cache_block *block;
    while (1)
    {
        block = cache.block_slot[cache.clock_p];
        if (block == NULL)
        {
            cache.clock_p = (cache.clock_p + 1) % MAXCACHEBLOCK;
            continue;
        }
        if (block->visit)
        {
            block->visit = 0;
        }
        else
        {
            free(block->body);
            cache.used_size -= block->body_size;
            cache.used_block--;
            free(block);
            cache.block_slot[cache.clock_p] = NULL;
            return;
        }

        cache.clock_p = (cache.clock_p + 1) % MAXCACHEBLOCK;
    }
}