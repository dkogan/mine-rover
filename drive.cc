#include <unistd.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <signal.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Value_Slider.H>
#include <FL/Fl_Dial.H>
#include <FL/Fl_Toggle_Button.H>

#define HOST           "127.0.0.1"
#define PORT           3322
#define COMM_PERIOD_US 200000

#define WIDGET_TURN_W  500
#define WIDGET_TURN_H  500
#define WIDGET_DRIVE_W 150
#define WIDGET_DRIVE_H WIDGET_TURN_H
#define WIDGET_GO_W    (WIDGET_TURN_W + WIDGET_DRIVE_W)
#define WIDGET_GO_H    150

#define WINDOW_W       WIDGET_GO_W
#define WINDOW_H       (WIDGET_TURN_H + WIDGET_GO_H)

static bool set_up_network( // out
                            int* fd,
                            struct sockaddr_in* addr,

                            // in
                            const char* host, int port)
{
    *fd = socket(AF_INET, SOCK_DGRAM, 0);
    if( *fd < 0 )
    {
        fprintf(stderr, "Couldn't socket()\n");
        return false;
    }

    struct hostent* server = gethostbyname(host);
    if(server == NULL)
    {
        fprintf(stderr, "Couldn't gethostbyname(\"%s\")\n", host);
        return false;
    }
    *addr = (struct sockaddr_in){ .sin_family = AF_INET,
                                  .sin_port   = htons(port)};
    memcpy(&addr->sin_addr.s_addr,
           server->h_addr,
           server->h_length);

    return true;
}


static int                fd;
static struct sockaddr_in addr;
static bool               done = false;

static void* send_current_command_thread(void* cookie)
{
    while(!done)
    {
        char buf[128];
        int len;

        Fl::lock();
        if(done) return NULL;
        {
            len = snprintf(buf, sizeof(buf),
                           "%d %d %d %d\n",
                           1,2,3,4);
        }
        Fl::unlock();

        if( len >= (int)sizeof(buf) )
        {
            fprintf(stderr, "buffer overflow\n");
            done = true;
            return NULL;
        }
        int len_sent = sendto(fd, buf, len, 0,
                              (struct sockaddr*)&addr, sizeof(addr));
        if( len_sent != len )
        {
            fprintf(stderr, "sendto(len = %d) returned %d\n", len, len_sent);
            done = true;
            return NULL;
        }

        usleep(COMM_PERIOD_US);
    }

    return NULL;
}

int main(int argc, char* argv[])
{
    if(!set_up_network(&fd, &addr, HOST, PORT))
    {
        fprintf(stderr, "Giving up\n");
        return 1;
    }


    Fl::lock();
    Fl::visual(FL_RGB);

    Fl_Window window(WINDOW_W, WINDOW_H);

    Fl_Dial* widget_turn = new Fl_Dial(0,0, WIDGET_TURN_W, WIDGET_TURN_H);
    widget_turn->type(FL_LINE_DIAL);

    Fl_Value_Slider* widget_drive = new Fl_Value_Slider(WIDGET_TURN_W,0,
                                                        WIDGET_DRIVE_W, WIDGET_DRIVE_H);
    widget_drive->type(FL_VERT_NICE_SLIDER);
    widget_drive->bounds(0, 4095);

    Fl_Toggle_Button* widget_go =
        new Fl_Toggle_Button(0, WIDGET_TURN_H,
                                          WIDGET_GO_W, WIDGET_GO_H,
                                          "Go!");

    window.resizable(window);
    window.end();
    window.show();

    pthread_t send_current_command_thread_id;
    if(pthread_create(&send_current_command_thread_id,
                      NULL,
                      &send_current_command_thread, NULL) != 0)
    {
        fprintf(stderr, "Couldn't create thread\n");
        return 1;
    }

    while (Fl::wait() && !done)
    {
    }

    done = true;
    pthread_kill(send_current_command_thread_id, SIGINT);
    Fl::unlock();

    pthread_join(send_current_command_thread_id, NULL);

    return 0;
}
