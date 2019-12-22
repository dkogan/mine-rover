#include <unistd.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <signal.h>
#include <math.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Value_Slider.H>
#include <FL/Fl_Dial.H>
#include <FL/Fl_Toggle_Button.H>

#define HOST            "192.168.3.2"
#define PORT            3322
#define COMM_PERIOD_US  100000
#define UPDOWN_DELTA    100
#define UPDOWN_MIN      -4095
#define UPDOWN_MAX      4095
#define LEFTRIGHT_DELTA 5
#define LEFTRIGHT_MIN   -90
#define LEFTRIGHT_MAX   90

#define WIDGET_TURN_W    500
#define WIDGET_TURN_H    500
#define WIDGET_FORWARD_W 150
#define WIDGET_FORWARD_H WIDGET_TURN_H
#define WIDGET_GO_W      (WIDGET_TURN_W + WIDGET_FORWARD_W)
#define WIDGET_GO_H      150

#define WINDOW_W         WIDGET_GO_W
#define WINDOW_H         (WIDGET_TURN_H + WIDGET_GO_H)

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

static Fl_Dial*          widget_turn    = NULL;
static Fl_Value_Slider*  widget_forward = NULL;
static Fl_Toggle_Button* widget_go      = NULL;

static void* send_current_command_thread(void* cookie)
{
    while(!done)
    {
        char buf[128];
        int len;

        int motor_values[4] = {};

        Fl::lock();
        if(done) return NULL;

        bool   go      = widget_go     ->value();
        double forward = widget_forward->value();
        double turn    = widget_turn   ->value();
        Fl::unlock();

        if( go )
        {
            // As I turn, I keep the outer wheels turning at the same speed, and
            // slow down the inner wheels, linearly.
            // Motor order: top view; forward is UP:
            //
            //    ^^^
            //    1 0
            //    | |
            //    3 2
            double outer_speed = fabs(forward);
            double inner_speed = outer_speed * (1.0 - 2.0*fabs(turn) / LEFTRIGHT_MAX);
            if( turn > 0 )
            {
                motor_values[0] = motor_values[2] = inner_speed;
                motor_values[1] = motor_values[3] = outer_speed;
            }
            else
            {
                motor_values[0] = motor_values[2] = outer_speed;
                motor_values[1] = motor_values[3] = inner_speed;
            }
            if(forward < 0)
            {
                motor_values[0] = -motor_values[0];
                motor_values[1] = -motor_values[1];
                motor_values[2] = -motor_values[2];
                motor_values[3] = -motor_values[3];
            }
            len = snprintf(buf, sizeof(buf),
                           "%d %d %d %d\n",
                           motor_values[0],
                           motor_values[1],
                           motor_values[2],
                           motor_values[3]);
        }
        else
            len = snprintf(buf, sizeof(buf), "\n");

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


class Fl_Window_customhandler : public Fl_Window
{
public:
    Fl_Window_customhandler(int w, int h, const char* title = 0)
        : Fl_Window(w,h,title)
    {
    }

    int handle(int event)
    {
        if(event == FL_KEYDOWN)
        {
            if(Fl::event_key() == ' ')
            {
                if( widget_go->value() )
                    widget_go->value(0);
                else
                    widget_go->value(1);
                return 1;
            }

            if(Fl::event_key() == FL_BackSpace)
            {
                // reset everything to the defaults
                widget_go     ->value(0);
                widget_forward->value(0);
                widget_turn   ->value(0);
                return 1;
            }

            if(Fl::event_key() == FL_Up)
            {
                double x = widget_forward->value();
                x = fmin(UPDOWN_MAX, x + UPDOWN_DELTA);
                widget_forward->value(x);
                return 1;
            }
            if(Fl::event_key() == FL_Down)
            {
                double x = widget_forward->value();
                x = fmax(UPDOWN_MIN, x - UPDOWN_DELTA);
                widget_forward->value(x);
                return 1;
            }
            if(Fl::event_key() == FL_Left)
            {
                double x = widget_turn->value();
                x = fmax(LEFTRIGHT_MIN, x - LEFTRIGHT_DELTA);
                widget_turn->value(x);
                return 1;
            }
            if(Fl::event_key() == FL_Right)
            {
                double x = widget_turn->value();
                x = fmin(LEFTRIGHT_MAX, x + LEFTRIGHT_DELTA);
                widget_turn->value(x);
                return 1;
            }
        }

        return Fl_Window::handle(event);
    }
};

int main(int argc, char* argv[])
{
    if(!set_up_network(&fd, &addr, HOST, PORT))
    {
        fprintf(stderr, "Giving up\n");
        return 1;
    }


    Fl::lock();
    Fl::visual(FL_RGB);

    Fl_Window_customhandler window(WINDOW_W, WINDOW_H);

    widget_turn = new Fl_Dial(0,0, WIDGET_TURN_W, WIDGET_TURN_H);
    widget_turn->type(FL_LINE_DIAL);
    widget_turn->bounds(LEFTRIGHT_MIN, LEFTRIGHT_MAX);
    widget_turn->angle1(90);
    widget_turn->angle2(270);

    widget_forward = new Fl_Value_Slider(WIDGET_TURN_W,0,
                                       WIDGET_FORWARD_W, WIDGET_FORWARD_H);
    widget_forward->type(FL_VERT_NICE_SLIDER);
    widget_forward->bounds(UPDOWN_MAX, UPDOWN_MIN); // backwards: low is at the bottom

    widget_go = new Fl_Toggle_Button(0, WIDGET_TURN_H,
                                     WIDGET_GO_W, WIDGET_GO_H,
                                     "Go!");
    widget_go->selection_color(FL_RED);

    //window.resizable(window);
    window.end();
    window.show();

    // non-focusable; the windown event handler handles the events for everybody
    widget_turn   ->set_output();
    widget_forward->set_output();
    widget_go     ->set_output();

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
