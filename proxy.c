#include <stdio.h>
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";

void proxy(int clientfd);
void *thread(void *vargp);

int main(int argc, char **argv) {

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
  if (listenfd < 0) {
    fprintf(stderr, "Error: Failed to open listening socket on port %s\n", argv[1]);
    exit(1);
  }

  while (1) {
      clientlen = sizeof(clientaddr);
      connfd = Malloc(sizeof(int));
      *connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);

      Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
      printf("Accepted connection from (%s, %s)\n", hostname, port);

      Pthread_create(&tid, NULL, thread, connfd);
  }
  return 0;
}

void *thread(void *vargp)
{
  int connfd = *((int *)vargp);
  Pthread_detach(pthread_self());
  Free(vargp);
  proxy(connfd);
  Close(connfd);
  return NULL;
}

void proxy(int clientfd)
{
  int serverfd, n;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char hostname[MAXLINE], path[MAXLINE], server_port[6];
  rio_t rio_client, rio_server;

  /* Proxy 헤더 읽기 */
  Rio_readinitb(&rio_client, clientfd);
  Rio_readlineb(&rio_client, buf, MAXLINE); // 여기서 한 줄 읽음
  sscanf(buf, "%s %s %s", method, uri, version); // 읽은 줄을 변수에 저장

  /* HTTP/1.1 -> HTTP/1.0으로 변경 */
  if (strcmp(version, "HTTP/1.1") == 0) {
    strcpy(version, "HTTP/1.0"); 
  }

  /* URI에서 path랑 hostname 추출 */
  sscanf(uri, "http://%[^:]:%[^/]%s", hostname, server_port, path);

  /* Tiny 서버에 req 전달하기 */
  serverfd = Open_clientfd(hostname, server_port);
  if (serverfd < 0) {
      fprintf(stderr, "Connection to server failed\n");
      return;
  }

  
  /* 첫 줄 전달 */
  sprintf(buf, "%s %s %s\r\n", method, path, version);
  Rio_writen(serverfd, buf, strlen(buf));

  /* 기존 헤더 덮어씌우기 */
  sprintf(buf, "%s", user_agent_hdr); // UserAgent 정보
  Rio_writen(serverfd, buf, strlen(buf));

  /* Connection 헤더 추가 */
  sprintf(buf, "Connection: close\r\n");
  Rio_writen(serverfd, buf, strlen(buf));

  // 헤더 끝을 알리는 줄바꿈
  Rio_writen(serverfd, "\r\n", 2);

  Rio_readinitb(&rio_server, serverfd);
  /* 원격 서버의 응답을 클라이언트에 전달 */
  while ((n = Rio_readnb(&rio_server, buf, MAXLINE)) > 0) {
      Rio_writen(clientfd, buf, n);
  }
  Close(serverfd);
}