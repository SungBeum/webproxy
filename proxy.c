#include <stdio.h>
#include "csapp.h"

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize, char *method);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs, char *method);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg);

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";

int main(int argc, char **argv) {
  printf("%s", user_agent_hdr);
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }
  listenfd = Open_listenfd(argv[1]);
  while (1) {
    clientlen = sizeof(clientaddr);

    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE,
                0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    doit(connfd);
    Close(connfd);
  }
}


void doit(int fd)
{   // rio_client : 클라이언트, rio : tiny 서버, proxyfd : 프록시
  int is_static;
  struct stat sbuf;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char filename[MAXLINE], cgiargs[MAXLINE];
  
  int proxyfd , con_length = 0;
  char *host, *port, *p, *con_type;
  rio_t rio, rio_client;

  host = "localhost";
  port = "3000";

  proxyfd = Open_clientfd(host, port);
  Rio_readinitb(&rio_client, fd);
  Rio_readinitb(&rio, proxyfd);
  Rio_readlineb(&rio_client, buf, MAXLINE);
  // printf("rio_client_buf : %s", buf);
  sscanf(buf, "%s %s %s", method, uri, version);
  // printf("method : %s \n uri : %s \n  version : %s \n", method, uri, version);
  Rio_writen(proxyfd, buf, strlen(buf));
  while(strcmp(buf, "\r\n"))
  {
    Rio_readlineb(&rio_client, buf, MAXLINE);
    Rio_writen(proxyfd, buf, strlen(buf));
    printf(("buf : %s", buf));
  }
  Rio_readlineb(&rio, buf, MAXLINE);
  Rio_writen(fd, buf, strlen(buf));
  if (strcasecmp(method, "HEAD") && strcasecmp(method, "GET")) 
  {
    clienterror(fd, method, "501", "Not implemented", "Tiny does not implement this method");
    return;
  }
  while(strcmp(buf, "\r\n"))
  {
    Rio_readlineb(&rio, buf, MAXLINE);
    Rio_writen(fd, buf, strlen(buf));
    printf("buf : %s", buf);
    if (strstr(buf, "Content-length"))
    {
      p = strchr(buf, ':');
      
      con_length = atoi(p+1);
      // printf("con_length : %d\n", con_length);
    }
    if (strstr(buf, "Content-type"))
    {
      p = strchr(buf, ':');
      strcpy(con_type, p+1);
      printf("con_type : %s", con_type);
    }
  }
//   is_static = parse_uri(uri, con_type, cgiargs);
//   printf("is_static : %d\n", is_static);
//   if (stat(con_type, &sbuf) < 0) {
//     clienterror(fd, con_type, "404", "Not found", "Tiny couldn't find this file");
//     return;
//   }
// //   // is_static == 정적컨텐츠 or 동적컨텐츠
//   if (is_static) {
//     // 파일이 보통 파일이라는 것과 읽기 권한을 가지고 있는지 검증
//     if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) {
//       clienterror(fd, con_type, "403", "Forbidden", "Tiny couldn't read the file");
//       return;
//     }
//     // 정적 컨텐츠를 제공
//     serve_static(fd, con_type, sbuf.st_size, method);
//   }






//   else {
//     // 동적 컨텐츠에 대한 것이면, 이 파일이 실행 가능한지 검증
//     if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) {
//       clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program");
//       return;
//     }
//     // 동적 컨텐츠를 제공
//     serve_dynamic(fd, filename, cgiargs, method);
//   }
  
  
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

int parse_uri(char *uri, char *filename, char *cgiargs)
{
  char *ptr;
  // 스트링"cgi-bin"을 포함하는 모든 uri는 동적 컨텐츠를 요청
  if (!strstr(uri, "cgi-bin")) // 만약 정적 컨텐츠를 위한 것이면,
  { // CGI 인자 스트링을 지우고
    strcpy(cgiargs, "");
    // uri 를 ./index.html 같은 상대 리눅스 경로이름으로 변환한다.
    strcpy(filename, ".");
    strcat(filename, uri);
    // 만약 uri 가 '/' 문자로 끝난다면, 기본 파일 이름을 추가한다.
    if (uri[strlen(uri)-1] == '/')
      strcat(filename, "home.html");
    else
      strcat(filename, ".html");
    return 1;
  }
  // 만약 동적 컨텐츠를 위한 것이라면,
  else 
  {
    // 모든 CGI 인자들을 추출하고
    ptr = index(uri, '?');
    if (ptr) {
      strcpy(cgiargs, ptr+1);
      *ptr = '\0';
    }
    else{
      strcpy(cgiargs, "");
    }
    // 나머지 uri 부분을 상대 리눅스 파일이름으로 변환한다.
    strcpy(filename, ".");
    strcat(filename, uri);
    return 0;
  }
}

void serve_static(int fd, char *filename, int filesize, char *method)
{
  int srcfd;
  char *srcp, filetype[MAXLINE], buf[MAXBUF];

  // 파일 이름의 접미어 부분을 검사해서 파일 타입을 결정
  get_filetype(filename, filetype);
  // 클라이언트에 응답 줄과 응답 헤더를 보낸다
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
  sprintf(buf, "%sConnection: close\r\n", buf);
  sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
  sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
  Rio_writen(fd, buf, strlen(buf));
  printf("Response headers:\n");
  printf("%s", buf);

  if (!strcasecmp(method, "HEAD"))
  {
    return;
  }

  // 읽기위해 filename 을 오픈하고 식별자를 얻어온다
  srcfd = Open(filename, O_RDONLY, 0);
  srcp = Malloc(filesize);
  // srcfd 의 정보를 읽어서 srcp 에 저장!
  Rio_readn(srcfd, srcp, filesize);
  Rio_writen(fd, srcp, filesize);
  // 요청한 파일을 가상메모리 영역으로 매핑 - mmap : 파일 srcfd의 첫 번째 filesize 바이트를 주소 srcp에서 시작하는 사적 읽기-허용 가상메모리 영역으로 매핑한다.
  // srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
  // 파일을 메모리로 매핑한 후 더 이상 이 식별자는 필요 없기 때문에, 파일을 닫는다 - 메모리 누수 방지
  Close(srcfd);
  // 실제로 파일을 클라이언트에게 전송한다
  // Rio_writen(fd, srcp, filesize);
  // 매핑된 가상메모리 주소를 반환한다 - 메모리 누수 방지
  free(srcp);
  // Munmap(srcp, filesize);
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