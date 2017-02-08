/*******************************************************************************
#                                                                              #
#      MJPG-streamer allows to stream JPG frames from an input-plugin          #
#      to several output plugins                                               #
#                                                                              #
#      Copyright (C) 2007 busybox-project (base64 function)                    #
#      Copyright (C) 2007 Tom Stöveken                                         #
#                                                                              #
# This program is free software; you can redistribute it and/or modify         #
# it under the terms of the GNU General Public License as published by         #
# the Free Software Foundation; version 2 of the License.                      #
#                                                                              #
# This program is distributed in the hope that it will be useful,              #
# but WITHOUT ANY WARRANTY; without even the implied warranty of               #
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                #
# GNU General Public License for more details.                                 #
#                                                                              #
# You should have received a copy of the GNU General Public License            #
# along with this program; if not, write to the Free Software                  #
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA    #
#                                                                              #
*******************************************************************************/
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <syslog.h>
#include <netdb.h>
#include <errno.h>
#include <limits.h>

#include "../../wave_streamer.h"
#include "../../utils.h"
#include "../../tools/tcp.h"
#include "../../tools/alsa.h"

#include "httpd.h"

#define MAX_CLIENTS 100

static globals *pglobal;
int port = 8080,queue = 20;

/******************************************************************************
Description.: initializes the iobuffer structure properly
Input Value.: pointer to already allocated iobuffer
Return Value: iobuf
******************************************************************************/
void init_iobuffer(iobuffer *iobuf)
{
    memset(iobuf->buffer, 0, sizeof(iobuf->buffer));
    iobuf->level = 0;
}

/******************************************************************************
Description.: initializes the request structure properly
Input Value.: pointer to already allocated req
Return Value: req
******************************************************************************/
void init_request(request *req)
{
    req->type        = A_UNKNOWN;
    req->parameter   = NULL;
    req->client      = NULL;
    req->credentials = NULL;
}

/******************************************************************************
Description.: If strings were assigned to the different members free them
              This will fail if strings are static, so always use strdup().
Input Value.: req: pointer to request structure
Return Value: -
******************************************************************************/
void free_request(request *req)
{
    if(req->parameter != NULL) free(req->parameter);
    if(req->client != NULL) free(req->client);
    if(req->credentials != NULL) free(req->credentials);
    if(req->query_string != NULL) free(req->query_string);
}

/******************************************************************************
Description.: read with timeout, implemented without using signals
              tries to read len bytes and returns if enough bytes were read
              or the timeout was triggered. In case of timeout the return
              value may differ from the requested bytes "len".
Input Value.: * fd.....: fildescriptor to read from
              * iobuf..: iobuffer that allows to use this functions from multiple
                         threads because the complete context is the iobuffer.
              * buffer.: The buffer to store values at, will be set to zero
                         before storing values.
              * len....: the length of buffer
              * timeout: seconds to wait for an answer
Return Value: * buffer.: will become filled with bytes read
              * iobuf..: May get altered to save the context for future calls.
              * func().: bytes copied to buffer or -1 in case of error
******************************************************************************/
int _read(int fd, iobuffer *iobuf, void *buffer, size_t len, int timeout)
{
    int copied = 0, rc, i;
    fd_set fds;
    struct timeval tv;

    memset(buffer, 0, len);

    while((copied < len)) {
        i = MIN(iobuf->level, len - copied);
        memcpy(buffer + copied, iobuf->buffer + IO_BUFFER - iobuf->level, i);

        iobuf->level -= i;
        copied += i;
        if(copied >= len)
            return copied;

        /* select will return in case of timeout or new data arrived */
        tv.tv_sec = timeout;
        tv.tv_usec = 0;
        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        if((rc = select(fd + 1, &fds, NULL, NULL, &tv)) <= 0) {
            if(rc < 0)
                exit(EXIT_FAILURE);

            /* this must be a timeout */
            return copied;
        }

        init_iobuffer(iobuf);

        /*
         * there should be at least one byte, because select signalled it.
         * But: It may happen (very seldomly), that the socket gets closed remotly between
         * the select() and the following read. That is the reason for not relying
         * on reading at least one byte.
         */
        if((iobuf->level = read(fd, &iobuf->buffer, IO_BUFFER)) <= 0) {
            /* an error occured */
            return -1;
        }

        /* align data to the end of the buffer if less than IO_BUFFER bytes were read */
        memmove(iobuf->buffer + (IO_BUFFER - iobuf->level), iobuf->buffer, iobuf->level);
    }

    return 0;
}

/******************************************************************************
Description.: Read a single line from the provided fildescriptor.
              This funtion will return under two conditions:
              * line end was reached
              * timeout occured
Input Value.: * fd.....: fildescriptor to read from
              * iobuf..: iobuffer that allows to use this functions from multiple
                         threads because the complete context is the iobuffer.
              * buffer.: The buffer to store values at, will be set to zero
                         before storing values.
              * len....: the length of buffer
              * timeout: seconds to wait for an answer
Return Value: * buffer.: will become filled with bytes read
              * iobuf..: May get altered to save the context for future calls.
              * func().: bytes copied to buffer or -1 in case of error
******************************************************************************/
/* read just a single line or timeout */
int _readline(int fd, iobuffer *iobuf, void *buffer, size_t len, int timeout)
{
    char c = '\0', *out = buffer;
    int i;

    memset(buffer, 0, len);

    for(i = 0; i < len && c != '\n'; i++) {
        if(_read(fd, iobuf, &c, 1, timeout) <= 0) {
            /* timeout or error occured */
            return -1;
        }
        *out++ = c;
    }

    return i;
}

/******************************************************************************
Description.: Decodes the data and stores the result to the same buffer.
              The buffer will be large enough, because base64 requires more
              space then plain text.
Hints.......: taken from busybox, but it is GPL code
Input Value.: base64 encoded data
Return Value: plain decoded data
******************************************************************************/
void decodeBase64(char *data)
{
    const unsigned char *in = (const unsigned char *)data;
    /* The decoded size will be at most 3/4 the size of the encoded */
    unsigned ch = 0;
    int i = 0;

    while(*in) {
        int t = *in++;

        if(t >= '0' && t <= '9')
            t = t - '0' + 52;
        else if(t >= 'A' && t <= 'Z')
            t = t - 'A';
        else if(t >= 'a' && t <= 'z')
            t = t - 'a' + 26;
        else if(t == '+')
            t = 62;
        else if(t == '/')
            t = 63;
        else if(t == '=')
            t = 0;
        else
            continue;

        ch = (ch << 6) | t;
        i++;
        if(i == 4) {
            *data++ = (char)(ch >> 16);
            *data++ = (char)(ch >> 8);
            *data++ = (char) ch;
            i = 0;
        }
    }
    *data = '\0';
}

/******************************************************************************
Description.: convert a hexadecimal ASCII character to integer
Input Value.: ASCII character
Return Value: corresponding value between 0 and 15, or -1 in case of error
******************************************************************************/
int hex_char_to_int(char in)
{
    if(in >= '0' && in <= '9')
        return in - '0';

    if(in >= 'a' && in <= 'f')
        return (in - 'a') + 10;

    if(in >= 'A' && in <= 'F')
        return (in - 'A') + 10;

    return -1;
}

/******************************************************************************
Description.: Send a complete HTTP response and a stream of JPG-frames.
Input Value.: fildescriptor fd to send the answer to
Return Value: -
******************************************************************************/
void send_stream(int context_fd)
{
    //alsa init parameter
    char *dev = "default";
    snd_pcm_format_t format = SND_PCM_FORMAT_S16_LE;
    unsigned int rate = 16000;
    unsigned int channels = 1;

    snd_pcm_t *capture_handle;
    init_alsa(&capture_handle,dev,rate,format,channels);
    int err;
    int buffer_frames = 512;
    int buffer_length = buffer_frames * snd_pcm_format_width(format)/8 * channels;
    char buffer[buffer_length];

    char head[BUFFER_SIZE] = {0};
    struct timeval timestamp;
    gettimeofday(&timestamp, NULL);

    DBG("preparing header\n");
    sprintf(head, "HTTP/1.0 200 OK\r\n" \
            STD_HEADER \
            "Content-Type: audio/wav;boundary=" BOUNDARY "\r\n" \
            "\r\n" \
            "--" BOUNDARY "\r\n");

    if(write(context_fd, head, strlen(head)) < 0) {
        return;
    }

    DBG("Headers send, sending stream now\n");

    while(!pglobal->stop) {
        /*
         * print the individual mimetype and the length
         * sending the content-length fixes random stream disruption observed
         * with firefox
         */
        sprintf(head, "Content-Type: audio/wav\r\n" \
                "Content-Length: %d\r\n" \
                "X-Timestamp: %d.%06d\r\n" \
                "\r\n", buffer_length, (int)timestamp.tv_sec, (int)timestamp.tv_usec);
        DBG("sending intemdiate header\n");
        if(write(context_fd, head, strlen(head)) < 0) break;

        if ((err = snd_pcm_readi (capture_handle, buffer, buffer_frames)) != buffer_frames) {
            fprintf (stderr, "read from audio interface failed %d:(%s)\n",
                     err, snd_strerror (err));
            break;
        }

        DBG("sending frame\n");
        if(write(context_fd, buffer, buffer_length) < 0) break;

        DBG("sending boundary\n");
        sprintf(head, "\r\n--" BOUNDARY "\r\n");
        if(write(context_fd, head, strlen(head)) < 0) break;
    }
}

/******************************************************************************
Description.: Send error messages and headers.
Input Value.: * fd.....: is the filedescriptor to send the message to
              * which..: HTTP error code, most popular is 404
              * message: append this string to the displayed response
Return Value: -
******************************************************************************/
void send_error(int fd, int which, char *message)
{
    char buffer[BUFFER_SIZE] = {0};

    if(which == 401) {
        sprintf(buffer, "HTTP/1.0 401 Unauthorized\r\n" \
                "Content-type: text/plain\r\n" \
                STD_HEADER \
                "WWW-Authenticate: Basic realm=\"MJPG-Streamer\"\r\n" \
                "\r\n" \
                "401: Not Authenticated!\r\n" \
                "%s", message);
    } else if(which == 404) {
        sprintf(buffer, "HTTP/1.0 404 Not Found\r\n" \
                "Content-type: text/plain\r\n" \
                STD_HEADER \
                "\r\n" \
                "404: Not Found!\r\n" \
                "%s", message);
    } else if(which == 500) {
        sprintf(buffer, "HTTP/1.0 500 Internal Server Error\r\n" \
                "Content-type: text/plain\r\n" \
                STD_HEADER \
                "\r\n" \
                "500: Internal Server Error!\r\n" \
                "%s", message);
    } else if(which == 400) {
        sprintf(buffer, "HTTP/1.0 400 Bad Request\r\n" \
                "Content-type: text/plain\r\n" \
                STD_HEADER \
                "\r\n" \
                "400: Not Found!\r\n" \
                "%s", message);
    } else if (which == 403) {
        sprintf(buffer, "HTTP/1.0 403 Forbidden\r\n" \
                "Content-type: text/plain\r\n" \
                STD_HEADER \
                "\r\n" \
                "403: Forbidden!\r\n" \
                "%s", message);
    } else {
        sprintf(buffer, "HTTP/1.0 501 Not Implemented\r\n" \
                "Content-type: text/plain\r\n" \
                STD_HEADER \
                "\r\n" \
                "501: Not Implemented!\r\n" \
                "%s", message);
    }

    if(write(fd, buffer, strlen(buffer)) < 0) {
        DBG("write failed, done anyway\n");
    }
}

/******************************************************************************
Description.: Serve a connected TCP-client. This thread function is called
              for each connect of a HTTP client like a webbrowser. It determines
              if it is a valid HTTP request and dispatches between the different
              response options.
Input Value.: arg is the filedescriptor and server-context of the connected TCP
              socket. It must have been allocated so it is freeable by this
              thread function.
Return Value: always NULL
******************************************************************************/
/* thread for clients that connected to this server */
void *client_thread(void *arg)
{
    int cnt;
    char query_suffixed = 0;
    char buffer[BUFFER_SIZE] = {0}, *pb = buffer;
    iobuffer iobuf;
    request req;
    int client_fd = (int) arg;

    /* initializes the structures */
    init_iobuffer(&iobuf);
    init_request(&req);

    /* What does the client want to receive? Read the request. */
    memset(buffer, 0, sizeof(buffer));
    if((cnt = _readline(client_fd, &iobuf, buffer, sizeof(buffer) - 1, 5)) == -1) {
        close(client_fd);
        return NULL;
    }

    req.query_string = NULL;

    /* determine what to deliver */
    if(strstr(buffer, "GET /?action=stream") != NULL) {
        req.type = A_STREAM;
        query_suffixed = 255;
    }
    else {
        int len;

        DBG("try to serve a file\n");
        req.type = A_FILE;

        if((pb = strstr(buffer, "GET /")) == NULL) {
            DBG("HTTP request seems to be malformed\n");
            send_error(client_fd, 400, "Malformed HTTP request");
            close(client_fd);
            return NULL;
        }

        pb += strlen("GET /");
        len = MIN(MAX(strspn(pb, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ._-1234567890"), 0), 100);
        req.parameter = malloc(len + 1);
        if(req.parameter == NULL) {
            exit(EXIT_FAILURE);
        }

        memset(req.parameter, 0, len + 1);
        strncpy(req.parameter, pb, len);

        if (strstr(pb, ".cgi") != NULL) {
            req.type = A_CGI;
            pb = strchr(pb, '?');
            if (pb != NULL) {
                pb++; // skip the ?
                len = strspn(pb, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ._-1234567890=&");
                req.query_string = malloc(len + 1);
                if (req.query_string == NULL)
                    exit(EXIT_FAILURE);
                strncpy(req.query_string, pb, len);
            } else {
                req.query_string = malloc(2);
                if (req.query_string == NULL)
                    exit(EXIT_FAILURE);
                sprintf(req.query_string, " ");
            }
        }
        DBG("parameter (len: %d): \"%s\"\n", len, req.parameter);
    }

    /*
     * parse the rest of the HTTP-request
     * the end of the request-header is marked by a single, empty line with "\r\n"
     */
    do {
        memset(buffer, 0, sizeof(buffer));

        if((cnt = _readline(client_fd, &iobuf, buffer, sizeof(buffer) - 1, 5)) == -1) {
            free_request(&req);
            close(client_fd);
            return NULL;
        }

        if(strstr(buffer, "User-Agent: ") != NULL) {
            req.client = strdup(buffer + strlen("User-Agent: "));
        } else if(strstr(buffer, "Authorization: Basic ") != NULL) {
            req.credentials = strdup(buffer + strlen("Authorization: Basic "));
            decodeBase64(req.credentials);
            DBG("username:password: %s\n", req.credentials);
        }

    } while(cnt > 2 && !(buffer[0] == '\r' && buffer[1] == '\n'));


    switch(req.type) {
    case A_STREAM:
        DBG("Request for stream from input: \n");
        send_stream(client_fd);
        break;
    default:
        DBG("unknown request\n");
    }

    close(client_fd);
    free_request(&req);

    DBG("leaving HTTP client thread\n");
    return NULL;
}

/******************************************************************************
Description.: This function cleans up ressources allocated by the server_thread
Input Value.: arg is not used
Return Value: -
******************************************************************************/
void server_cleanup(void *arg)
{

    OPRINT("cleaning up ressources allocated by server thread\n");

}

/******************************************************************************
Description.: Open a TCP socket and wait for clients to connect. If clients
              connect, start a new thread for each accepted connection.
Input Value.: arg is a pointer to the globals struct
Return Value: always NULL, will only return on exit
******************************************************************************/
void *server_thread(void *arg)
{
    pglobal = (globals*) arg;
    pthread_t client;
    /*create socket fd*/
    int server_sockfd = passive_server(port,queue);
DBG("1\n");
    struct sockaddr_in client_addr;
    socklen_t length = sizeof(client_addr);
    /* set cleanup handler to cleanup allocated ressources */
    pthread_cleanup_push(server_cleanup, NULL);
    DBG("2\n");
    while(!pglobal->stop)
    {
        DBG("等待客户端连接\n");

        ///成功返回非负描述字，出错返回-1
        int conn = accept(server_sockfd, (struct sockaddr*)&client_addr, &length);
        if(conn<0)
        {
            perror("connect");
            exit(1);
        }
        DBG("客户端成功连接,socketID=%d\n",conn);
        pthread_create(&client, NULL, &client_thread, (void*)conn);
        pthread_detach(client);
//        pglobal->in.add(conn);
    }
    close(server_sockfd);
    pthread_cleanup_pop(1);
    return NULL;
}
