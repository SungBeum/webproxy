#include <stdio.h>
#include "csapp.h"
#include <stdlib.h>

void *doit(void *vargp);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize, char *method);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs, char *method);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg);
void *thread(void *vargp);
typedef struct cache_storage
{
  char *path;
  char *body;
  int size;
  struct cache_storage *next;
  struct cache_storage *prev;
} cache;
/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";
    
cache *root = NULL;
int total_cache_size = 0;

int main(int argc, char **argv) {
  // root = (cache *)Malloc(sizeof(cache));
  printf("%s", user_agent_hdr);
  int listenfd, *connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;
  pthread_t tid;
  
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }
  listenfd = Open_listenfd(argv[1]);

   while (1)
  {
      clientlen = sizeof(struct sockaddr_storage);
      connfd = Malloc(sizeof(int));
      *connfd = Accept(listenfd, (SA *) &clientaddr, &clientlen);
      Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
      Pthread_create(&tid, NULL, doit, connfd);
  }
}


void *doit(void *vargp)
{   // rio_client : 클라이언트, rio : tiny 서버, proxyfd : 프록시
  int is_static;
  
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE], real_tiny_port[MAXLINE];  
  int proxyfd , con_length = 0, tiny_port;
  char *host, *port, *p, *sbuf, path[MAXLINE], tiny_name[MAXLINE], *tmp;
  rio_t rio, rio_client;
  // cache cbody, cpath, csize;
  int fd = *((int *)vargp);
  Pthread_detach(pthread_self());
  Free(vargp);

  // host = "localhost";
  // port = "3000";




  // proxyfd = Open_clientfd(host, port);
  Rio_readinitb(&rio_client, fd);
  Rio_readlineb(&rio_client, buf, MAXLINE);
  // printf("rio_client_buf : %s\n", buf);
  
  sscanf(buf, "%s %s %s", method, uri, version);
  printf("method : %s \n uri : %s \n  version : %s \n", method, uri, version);
  tmp = strchr(uri, ':');
  strcpy(tmp, tmp+3);
  strncpy(tiny_name, tmp, 9);
  tiny_name[9] = '\0';
  printf("tiny_name : %s\n", tiny_name);
  printf("tmp : %s\n", tmp);
  tmp = strchr(tmp, ':');
  strcpy(tmp, tmp+1);
  printf("tmp : %s\n", tmp);
  tiny_port = atoi(tmp);
  sprintf(real_tiny_port, "%d", tiny_port);
  printf("real_tiny_port : %s\n", real_tiny_port);
  // sprintf(read_tiny_port, tiny_port, MAXLINE);
  // printf("tiny_port : %d\n", tiny_port);
  tmp = strchr(tmp, '/');
  strcpy(path, tmp);

  // 캐시 있는지 체크하는 함수
  // if (find_cache(path))   //있으면
  // {
  //  return;
  // }

  proxyfd = Open_clientfd(tiny_name, real_tiny_port);
  Rio_readinitb(&rio, proxyfd);

  sprintf(buf, "%s %s HTTP/1.0\r\n", method, path);
  Rio_writen(proxyfd, buf, strlen(buf));
  // printf("buf : %s", buf);
  // Rio_readlineb(&rio_client, buf, MAXLINE);
  // Rio_writen(proxyfd, buf, strlen(buf));
  // printf("buf : %s", buf);

  // printf("i'm here\n");  
  while(strcmp(buf, "\r\n"))
  {
    Rio_readlineb(&rio_client, buf, MAXLINE);    
    // if (strstr(buf, "Proxy-Connection:"))
    // {
    //   tmp = strchr(buf,':');
    //   strcpy(buf, tmp+1);
    //   strcpy(buf, "Proxy-Connection: Close");      
    // }
    Rio_writen(proxyfd, buf, strlen(buf));
    // printf(("buf : %s", buf));
  }
  // printf("actually i'm here\n");
  Rio_readlineb(&rio, buf, MAXLINE);
  // printf("buf : %s", buf);

  Rio_writen(fd, buf, strlen(buf));
  // printf("1\n");
  if (strcasecmp(method, "HEAD") && strcasecmp(method, "GET")) 
  {
    clienterror(fd, method, "501", "Not implemented", "Tiny does not implement this method");
    return;
  }
  // printf("2\n");

  while(strcmp(buf, "\r\n"))
  {
    Rio_readlineb(&rio, buf, MAXLINE);
    Rio_writen(fd, buf, strlen(buf));
    printf("buf : %s", buf);
    if (strstr(buf, "Content-length"))
    {
      p = strchr(buf, ':');
      
      con_length = atoi(p+1);
      continue;
    }
  }
  // printf("3\n");

  // printf("con_length : %d\n", con_length);
  // printf("i'm here\n");
  sbuf = Malloc(con_length);
  printf("con_length: %d\n", con_length);

  // srcfd 의 정보를 읽어서 srcp 에 저장!
  // csize.size = con_length;
  // printf("csize: %d\n", &csize);
  Rio_readnb(&rio, sbuf, con_length);
  // printf("payload: %s\n", sbuf);
  // cbody.body = sbuf;
  printf("path: %s, con_len(size): %d\n", path, con_length);
  
  // 캐시 크기가 부족하면, delete_cache()
  // while(con_length + total_cache_size > MAX_CACHE_SIZE)
  // {
  //   delete_cache();
  // }
  insert_cache(path, sbuf, con_length);


  Rio_writen(fd, sbuf, con_length);
  free(sbuf);
  Close(proxyfd);
  Close(fd);
}  
void insert_cache(char *path, char *body, int size)
{
  cache *S1 = (cache *)malloc(sizeof(cache));  
  total_cache_size += size;
  S1->path = path;
  S1->size = size;
  // printf("s_1path: %s\n",S1->path);
  // printf("s_1size: %d\n",S1->size);
  S1->body = body;
  // printf("root: %p\n",root);
  if (root == NULL)
  {
    // printf("i'm here\n");
    root = S1;
    S1->next = root;
    S1->prev = root;
    // printf("root_path : %s\n", root->path);
    // printf("S1 : %p\n", S1);
  }
  else
  {
    root->prev->next = S1;
    S1->prev = root->prev;
    root->prev = S1;
    S1 -> next = root;
    root = S1;
  }
}

int find_cache(path)
{
  
}

void delete_cache()
{ // 캐시가 하나
 if (root->next = root)
 {
  free(root);
  root = NULL;
 }
 else
 {
  root->prev->prev->next = root;
  root->prev = root->prev->prev;
  free(root->prev);
 } 
}

void read_requesthdrs(rio_t *rp)
{
  char buf[MAXLINE];

  Rio_readlineb(rp, buf, MAXLINE);
  // carriage return 과 line feed 쌍을 체크 ( \r : CR - 종이를 고정시키고 커서를 맨 앞줄로 이동시키는 것 , \n : LF - 커서 위치는 정지한 상태에서 종이를 한 줄 올리는 것)
  while(strcmp(buf, "\r\n")) {
    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
  }
  return;
}

void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
{
  char buf[MAXLINE], body[MAXBUF];
  // Build the HTTP response body - html 파일
  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
  sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);
  // Print the HTTP response =
  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, body, strlen(body));
}

void get_filetype(char *filename, char *filetype)
{
  if (strstr(filename, ".html"))
    strcpy(filetype, "text/html");
  else if (strstr(filename, ".gif"))
    strcpy(filetype, "image/gif");
  else if (strstr(filename, ".png"))
    strcpy(filetype, "image/png");
  else if (strstr(filename, ".jpg"))
    strcpy(filetype, "image/jpeg");
  else if (strstr(filename, ".mp4"))
    strcpy(filetype, "video/mp4");
  else
    strcpy(filetype, "text/plain");
}