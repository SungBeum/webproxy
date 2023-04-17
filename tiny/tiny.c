/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
#include "csapp.h"

void *doit(void *vargp);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize, char *method);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs, char *method);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg);
void*thread(void *vargp);

/*
* tiny 서버 main 함수 ( 반복실행 서버로 명령줄에서 넘겨받은 포트로의 연결 요청을 듣는다. )
*/
int main(int argc, char **argv) {
  int listenfd, *connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;
  pthread_t tid;

  // command line 2가 아니면 host, port 번호를 프린트
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }
  // 듣기 소켓을 오픈한다
  listenfd = Open_listenfd(argv[1]);
  // 무한 서버 루프를 실행한다


   while (1)
  {
      clientlen = sizeof(struct sockaddr_storage);
      connfd = Malloc(sizeof(int));
      *connfd = Accept(listenfd, (SA *) &clientaddr, &clientlen);
      Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
      Pthread_create(&tid, NULL, doit, connfd);
  } 
  // while (1) {
  //   clientlen = sizeof(clientaddr);
  //   // 반복적으로 연결 요청을 접수한다
  //   connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
  //   Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
  //   printf("Accepted connection from (%s, %s)\n", hostname, port);
  //   // 트랜잭션(transaction : 정보 처리) 수행
  //   doit(connfd);
  //   // 자신 쪽의 연결 끝을 닫는다
  //   Close(connfd);
  // }
}

/*
* 한 개의 HTTP 트랜잭션을 처리하는 함수
*/
void *doit(void *vargp)
{
  int is_static;
  struct stat sbuf;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char filename[MAXLINE], cgiargs[MAXLINE];
  rio_t rio;

  
  int fd = *((int *)vargp);
  Pthread_detach(pthread_self());
  Free(vargp);
  // rio_t 타입의 읽기 버퍼를 초기화하는 함수 (Rio_readinitb)
  Rio_readinitb(&rio, fd);
  // 요청 라인을 읽고 분석한다
  Rio_readlineb(&rio, buf, MAXLINE);
  printf("Request headers:\n");
  printf("%s", buf);
  sscanf(buf, "%s %s %s", method, uri, version);
  // get이 아닌 다른 메소드를 요청하면, 에러 메세지를 보내고, main 루틴으로 돌아온다 , 그리고 연결은 닫고 다음 연결 요청을 기다린다.
  // if (strcasecmp(method, "GET")) {
  //   clienterror(fd, method, "501", "Not implemented", "Tiny does not implement this method");
  //   return;  
  // }
  // 다른 요청 헤더들은 무시한다
  // HEAD 메소드 구현
    if (strcasecmp(method, "HEAD") && strcasecmp(method, "GET")) {
    clienterror(fd, method, "501", "Not implemented", "Tiny does not implement this method");
    return;
  }
  read_requesthdrs(&rio);

  // URI 를 CGI 인자 스트링으로 분석하고, 요청이 정적 또는 동적 컨텐츠를 위한 것인지 나타내는 플래그를 설정한다.
  is_static = parse_uri(uri, filename, cgiargs); //CGI : 서버쪽에서 수행되는 응용프로그램
  // 이 파일이 디스크 상에 없으면, 에러 메세지를 클라이언트에게 보내고 리턴한다
  if (stat(filename, &sbuf) < 0) {
    clienterror(fd, filename, "404", "Not found", "Tiny couldn't find this file");
    return;
  }
  // is_static == 정적컨텐츠 or 동적컨텐츠
  if (is_static) {
    // 파일이 보통 파일이라는 것과 읽기 권한을 가지고 있는지 검증
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) {
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read the file");
      return;
    }
    // 정적 컨텐츠를 제공
    serve_static(fd, filename, sbuf.st_size, method);
  }
  else {
    // 동적 컨텐츠에 대한 것이면, 이 파일이 실행 가능한지 검증
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) {
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program");
      return;
    }
    // 동적 컨텐츠를 제공
    serve_dynamic(fd, filename, cgiargs, method);
  }
  Close(fd);
}

/*
* 오류를 client 에게 응답해주는 함수
*/
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
/*
* 요청 헤더 내의 정보를 무시하는 함수
*/
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
//HTTP URI 를 분석하는 함수
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
    // else
      // strcat(filename, ".html");
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
/*
* 정적 컨텐츠를 클라이언트에게 서비스한다.
*/
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
/*
* 파일의 이름으로 파일의 타입을 결정
*/
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
/*
* 자식 프로세스를 fork하고, 그 후에 CGI 프로그램을 자식의 컨텍스트에서 실행, 모든 종류의 동적 컨텐츠를 제공한다.
*/
void serve_dynamic(int fd, char *filename, char *cgiargs, char *method)
{
  char buf[MAXLINE], *emptylist[] = { NULL };

  // 클라이언트에게 성공을 알려주는 응답 라인을 보냄
  sprintf(buf,"HTTP/1.0 200 OK\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Server: Tiny Web Server\r\n");
  Rio_writen(fd, buf, strlen(buf));
  // 새로운 자식을 fork
  if (Fork() == 0)
  {
    // 자식은 QUERY_STRING 환경 변수를 요청 URI 의 인자들로 초기화한다. ( setenv(*name, *value, overwrite) : name이 존재하지 않으면 변수 name 을 value값으로 추가한다. 환경에 name이 이미 존재하면, overwrite가 0이 아니면 그 값을 value로 바꾼다.)
    setenv("QUERY_STRING", cgiargs, 1);
    setenv("REQUEST_METHOD", method, 1);

    // 자식은 자식의 표준 출력을 연결 파일 식별자로 재지정 ( Dup2 : fd 의 서술자의 값을 STDOUT_FILENO 로 지정한다. STDOUT이 이미 열려 있으면 fd 를 닫은 후 복제 )
    Dup2(fd, STDOUT_FILENO); 
    // CGI 프로그램을 로드하고 실행 ( Execve : filename 이 가리키는 파일 실행 )
    Execve(filename, emptylist, environ); 
  }
  // 그 후 자식이 종료되어 정리되는 것을 기다리기 위해 wait 함수에서 블록
  Wait(NULL);
}