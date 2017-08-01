/*
*
* Multi-tråds nett-tjener som benytter posix pthreads.
*
* Enkel nett-tjener som klarer å servere enkel html, 
* jpg, gif & tekst-filer.
*
* @created   20160826-01:22-GMT+1
* @modified  20160826-02:15-GMT+1
*
*/
 
//---Include filer ------------------------------------------------------------
#include <stdio.h>          // for printf()
#include <stdlib.h>         // for exit()
#include <string.h>         // for strcpy(),strerror() og strlen()
#include <fcntl.h>          // for fil i/o konstanter
#include <sys/stat.h>       // for fil i/o konstanter
#include <errno.h>          // Feil-meldinger
//---FOR BSD UNIX/LINUX -------------------------------------------------------
#include <sys/types.h>      //   
#include <netinet/in.h>     //   
#include <sys/socket.h>     // for støpsel system-kall  
#include <arpa/inet.h>      // for støpsel system-kall (bind) 
#include <sched.h>          // for planlegging
#include <pthread.h>        // P-thread implementasjon
#include <signal.h>         // for signalisering
#include <semaphore.h>      // for p-thread semaforer
//---HTTP respons-beskjeder ---------------------------------------------------
#define OK_IMAGE    "HTTP/1.0 200 OKnContent-Type:image/gifnn"
#define OK_TEXT     "HTTP/1.0 200 OKnContent-Type:text/htmlnn"
#define NOTOK_404   "HTTP/1.0 404 Not FoundnContent-Type:text/htmlnn"
#define MESS_404    "<html><body><h1>FILE NOT FOUND</h1></body></html>"
//---definisjoner -------------------------------------------------------------
#define BUF_SIZE            1024     // buffer størrelse i bytes
#define PORT_NUM              80     // Port nummer for nett tjener (TCP 80/443)
#define PEND_CONNECTIONS     100     // ventende forbindelser for å sette på hold 
#define TRUE                   1
#define FALSE                  0
#define NTHREADS 5                     /* antall barnetråder          */ 
#define NUM_LOOPS  10                  /* antall lokale løkker        */
#define SCHED_INTVL 5                  /* tråd-planleggings intervall */
#define HIGHPRIORITY 10
//---globale variabler -------------------------------------------------------
sem_t thread_sem[NTHREADS];  
int  next_thread;  
int  can_run;
int  i_stopped[NTHREADS]; 
unsigned int   client_s; // klient støpsel-deskriptor.
//---barnetråd implementasjon ------------------------------------------------
void *my_thread(void * arg)
{
  unsigned int   myClient_s; // kopier støpsel.	
  //---andre lokale variabler ------------------------------------------------
  char           in_buf[BUF_SIZE];     // Inn-buffer for GET forespørsler.
  char           out_buf[BUF_SIZE];    // Ut-buffer for HTTP-respons.
  char           *file_name;           // Filnavn.
  unsigned int   fh;                   // Fil håndterer (fil deskriptor).
  unsigned int   buf_len;              // Buffer lengde for fil-lesing.
  unsigned int   retcode;              // Retur-kode.
  myClient_s = *(unsigned int *)arg;   // Kopier klient støpsel deskriptor.
  //---mottak av første HTTP forespørsel (HTTP GET) --------------------------
      retcode = recv(client_s, in_buf, BUF_SIZE, 0);
      // hvis mottaksfeil.
      if (retcode < 0)
      {   printf("recv feil oppdaqet ...\n"); }
      // hvis HTTP kommando mottatt.
      else
      {    
        // analyser ut filnavnet fra GET forespørsel.
        strtok(in_buf, " ");
        file_name = strtok(NULL, " ");
		// test for høy prioritets fil.
		if(0 == strcmp(&file_name[1],"test_00.jpg")){
			int diditwork = pthread_setschedprio(pthread_self(), HIGHPRIORITY);
			if(!diditwork)
				printf("prioritet ble ikke korrekt økt ...\n");
			else{
				printf("Høy prioritets tråd laget ...\n");
			}
		}
        // åpne forespurt fil (start på andre karakter for å bli kvitt ledende "").
        fh = open(&file_name[1], O_RDONLY, S_IREAD | S_IWRITE);  
        // generer og send respons (404 hvis fil ikke funnet).
        if (fh == -1)
        {
          printf("Filen %s ble ikke funnet - HTTP 404\n", &file_name[1]);
          strcpy(out_buf, NOTOK_404);
          send(client_s, out_buf, strlen(out_buf), 0);
          strcpy(out_buf, MESS_404);
          send(client_s, out_buf, strlen(out_buf), 0);
        }
        else
        {
          printf("Filen %s blir sendt ...\n", &file_name[1]);
          if ((strstr(file_name, ".jpg") != NULL)||(strstr(file_name, ".gif") != NULL)) 
          { strcpy(out_buf, OK_IMAGE); }
          else
          { strcpy(out_buf, OK_TEXT); }
          send(client_s, out_buf, strlen(out_buf), 0); 
          buf_len = 1;  
          while (buf_len > 0)  
          {
            buf_len = read(fh, out_buf, BUF_SIZE);
            if (buf_len > 0)   
            { 
              send(client_s, out_buf, buf_len, 0);     
              // printf("%d bytes overført ...\n", buf_len);  
            }
          }
 
          close(fh);       // lukk filen.
          close(client_s); // lukk klient forbindelse.
	  pthread_exit(NULL); // avslutt pthread, som dreper tråder / barnetråder.
        }
      } 
}
 
//===== Hovedprogram =========================================================
int main(void)
{
  //---lokale variabler for støpsel forindelse -------------------------------
  unsigned int		server_s;         // Tjener støpsel deskriptor.
  struct sockaddr_in	server_addr;      // Tjener Internett-addresse.
  unsigned int client_s;                  // Klient støpsel deskriptor.
  struct sockaddr_in	client_addr;      // Klient Internett-addresse.
  struct in_addr	client_ip_addr;   // Klient IP-addresse.
  int			addr_len;         // Internett addresse lengde.
  unsigned int		ids;              // holder tråd-arguenter.
  pthread_attr_t	attr;             // pthread attributter.
  pthread_t		threads;          // Tråd-ID (brukt av OS). 
  //---opprett et nytt støpsel -----------------------------------------------
  server_s = socket(AF_INET, SOCK_STREAM, 0);
  //---fyll inn addresse informasjon, og bind det ----------------------------
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(PORT_NUM);
  server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  bind(server_s, (struct sockaddr *)&server_addr, sizeof(server_addr));
  //---lytt etter forbindelser og aksepter -----------------------------------
  listen(server_s, PEND_CONNECTIONS);
  //---nett tjener hoved-løkke ===============================================
  pthread_attr_init(&attr);
  while(TRUE)
  {
    printf("Tjener er klar ...\n");  
    //---vent for neste klient til å ankomme ---------------------------------
    addr_len = sizeof(client_addr);
    client_s = accept(server_s, (struct sockaddr *)&client_addr, &addr_len);
    printf("en ny klient ankommer ...\n");  
    if (client_s == FALSE)
    {
      printf("FEIL - Ikke i stand til å opprette støpsel\n");
      exit(FALSE);
    }
    else
    {
    //---opprett barnetråd ---------------------------------------------------
    ids = client_s;
    pthread_create (		/* opprett en barnetråd         */ 
                   &threads,	/* tråd-ID (system-utlevert)    */     
                   &attr,	/* standard tråd-attributter    */
                   my_thread,	/* Tråd-rutine                  */
                   &ids);	/* Argumenter å videresende     */ 
    }
  }
  //---for å være sikker på at "hovedprogram(main)" returnerer et heltall ----
  close (server_s);  // lukker hoved støpsel.
  return (TRUE);     // retur-kode fra "hovedprogram(main)"
}
