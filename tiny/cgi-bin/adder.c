/*
 * adder.c - a minimal CGI program that adds two numbers together
 */
/* $begin adder */
#include "csapp.h"

int main(void) {
  char *buf, *p;
  char arg1[MAXLINE], arg2[MAXLINE], content[MAXLINE];
  int n1=0, n2=0;

  /* 2개의 arguments 추출 */
  if ((buf = getenv("QUERY_STRING")) != NULL){
    p = strchr(buf, '&');
    *p = '\0';
    strcpy(arg1, buf);
    strcpy(arg2, p+1);
    n1=atoi(arg1);
    n2=atoi(arg2);
  }

  /* response body를 생성 */
  sprintf(content, "QUERY_STRING=%s", buf);
  sprintf(content, "Welcome to add.com");
  sprintf(content, "%sThe addition web site\r\n<p>", content);
  sprintf(content, "%sThe Answer is: %d + %d = %d\r\n<p>", content, n1, n2, n1+n2);
  sprintf(content, "%sThank you!\r\n", content);

  /* HTTP 응답 생성 */
  printf("Connection: close\r\n");
  printf("Content-length: %d\r\n", (int)strlen(content));
  printf("Content-type: text/html\r\n\r\n");
  printf("%s", content);
  fflush(stdout); // 클라이언트에게 데이터 전송

  exit(0);
}
/* $end adder */
