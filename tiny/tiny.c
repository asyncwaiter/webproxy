#include "csapp.h"

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg);

int main(int argc, char **argv) {
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  /* Check command line args */
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]);
  while (1) {
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr,
                    &clientlen);  // line:netp:tiny:accept
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE,
                0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    doit(connfd);   // line:netp:tiny:doit
    Close(connfd);  // line:netp:tiny:close
  }
}

/* 핵심 로직 함수: Header 읽고, URI 파싱해서, 정적/동적 통신 결정 */
void doit(int fd)
{
  int is_static;
  struct stat sbuf; // 파일 정보 저장을 위한 구조체
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char filename[MAXLINE], cgiargs[MAXLINE];
  rio_t rio;
  
  /* Header 읽기 */
  Rio_readinitb(&rio, fd);
  Rio_readlineb(&rio, buf, MAXLINE);
  printf("Request Headers:\n");
  printf("%s", buf); // fd로 읽은 buf를 출력
  sscanf(buf, "%s %s %s", method, uri, version); // request 요청 받기
  if(strcasecmp(method, "GET")){
    clienterror(fd, method, "501", "Not implemented", "Tiny does not implement this method");
    return;
  }
  read_requesthdrs(&rio); // 추가 헤더가 있을 경우, 읽는 과정
  
  /* URI 파싱 */
  is_static = parse_uri(uri, filename, cgiargs);
  // filename의 정보를 가져와 sbuf에 저장하는 함수 
  if(stat(filename, &sbuf) < 0){ // 성공시 0, 실패시 -1
    clienterror(fd, filename, "404", "Not found", "Tiny couldn't find this file");
    return;
  }
  if (is_static) { // 정적 컨텐츠일 경우
      // st_mode 메타데이터로 파일의 속성과 권한을 확인
      // 정규파일 확인(텍스트, 바이너리...) 반대는 directory, symbolic links && 읽기 권한 확인 
      if(!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)){
        clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program");
        return;
      }
      serve_static(fd, filename, sbuf.st_size);
  }
  else { // 동적 컨텐츠인 경우, 실행 권한 확인
    if(!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)){
        clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program");
        return;
      }
    serve_dynamic(fd, filename, cgiargs);
  }




}

void read_requesthdrs(rio_t *rp)
{
  char buf[MAXLINE];

  Rio_readlineb(rp, buf, MAXLINE);
  while (strcmp(buf, "\r\n")) {
    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
  }
  return;
}

void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
{
  char buf[MAXLINE], body[MAXBUF];

  /* HTTP 응답 body 생성 */
  sprintf(body, "<html><title>Tiny ERROR</title>");
  sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
  sprintf(body, "%s<hr><em>Tiny Web Server</em>\r\n", body);

  /* HTTP 응답 출력 */
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
  if (!strstr(uri, "cgi-bin")) { // 값이 없을 경우
    strcpy(cgiargs, "");
    strcpy(filename, ".");
    strcat(filename, uri);
    if (uri[strlen(uri)-1] == '/'){
      strcat(filename, "home.html");
    }
    return 1;
  }
  else { // 값이 존재하는 경우
    ptr = index(uri, '?'); // -> 포인터 반환 OR NULL
    if(ptr){ // 쿼리스트링 존재⭕️
      strcpy(cgiargs, ptr+1);
      *ptr = '\0'; // '?'를 NULL로 바꿔서 URI에서 쿼리 스트링 부분을 잘라냄
    }
    else // 쿼리스트링 존재❌
      strcpy(cgiargs, "");
    strcpy(filename, ".");
    strcat(filename, uri); // filename에 .과 uri를 붙임 /cgi-bin/script라면 filename은 ./cgi-bin/script
    return 0;
  }
}

void serve_static(int fd, char *filename, int filesize)
{
  int srcfd;
  char *srcp, filetype[MAXLINE], buf[MAXBUF];

  /* Client에게 응답 헤더 전송 */
  get_filetype(filename, filetype);
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
  sprintf(buf, "%sConnection: close\r\n", buf);
  sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
  sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
  Rio_writen(fd, buf, strlen(buf)); // 클라에게 헤더 전송
  printf("Response header:\n");
  printf("%s", buf);

  /* Client에게 응답 바디 전송 */
  srcfd = Open(filename, O_RDONLY, 0); // 파일 열고 해당 파일 디스크립터 할당
  // 파일을 메모리 공간에 매핑 -> 반환은 시작 주소
  // read 하지 않고 이렇게 하는 이유는? -> 시스템이 파일을 직접 메모리에 바로 올려서 읽을 수 있기에 더 빠름
  srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
  Close(srcfd); // 파일 디스크립터 닫기
  Rio_writen(fd, srcp, filesize); // 클라에게 파일 전송
  Munmap(srcp, filesize); // 메모리에 매핑된 파일 데이터 해제 memory unmap
}

void get_filetype(char *filename, char *filetype)
{
  if (strstr(filename, ".html"))
    strcpy(filetype, "text/html");
  else if (strstr(filename, ".gif"))
    strcpy(filetype, "image/gif");
  else if (strstr(filename, "./png"))
    strcpy(filetype, "image/png");
  else if (strstr(filename, ".jpg"))
    strcpy(filetype, "image/jpeg");
  else if (strstr(filename, "./mp4"))
    strcpy(filetype, "video/mp4");
  else
    strcpy(filetype, "text/plain");
}

void serve_dynamic(int fd, char *filename, char *cgiargs)
{
  char buf[MAXLINE], *emptylist[] = { NULL };
  
  /* HTTP 응답의 첫번째 부분 */
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  Rio_writen(fd, buf, strlen(buf)); // 응답 헤더 전송
  sprintf(buf, "Server: Tiny Web Server\r\n");
  Rio_writen(fd, buf, strlen(buf));

  if (Fork() == 0){ // 자식에서 CGI 프로그램을 실행하는 이유는? -> 웹서버의 안정성과 동시성 처리 때문
    setenv("QUERY_STRING", cgiargs, 1);
    Dup2(fd, STDOUT_FILENO); // 클라이언트 소켓 fd를 표준 출력으로 리다이렉트 -> 클라이언트에게 바로 전송
    Execve(filename, emptylist, environ); // 현재 자식 프로세스의 메모리 공간을 새로운 프로그램으로 완전히 덮어쓰는 시스템 호출, filename에 해당하는 CGI 프로그램 실행
    // CGI는 실행 중에 표준 출력을 통해 데이터를 출력함
    // 하지만 Dup2를 먼저 호출했기에, 클라이언트 fd로 직접 전송됌
    // 동적 웹페이지나 데이터 생성 후, printf 같은 함수로 출력을 하면 -> 리다이렉트된 클라이언트 소켓을 통해 클라이언트에게 전송됌
  }
  Wait(NULL); // 자식 기다림

}