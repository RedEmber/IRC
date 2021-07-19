#include <ncurses.h>
#include <sys/socket.h> 
#include <arpa/inet.h> 
#include <unistd.h> 
#include <string.h> 
#include <time.h>

#define BUFF_SIZE 1024
#define PORT 8080 

using namespace std;

void clean_buffer(char[], int = BUFF_SIZE - 1);
void cleanup();
int init(int&, struct sockaddr_in&);
   
int main(int argc, char const *argv[]) 
{ 
    fd_set readfds;
    struct timeval tv  = {0, 1000}; // Timeout for polling socket to see if message was received; set to 1 millisecond
    struct sockaddr_in serv_addr;
    int sock = 0, place = 0, valread, activity;
    char send_buffer[1024]; // Used to send commands to server
    char recv_buffer[1024]; // Used to get messages from server
    char temp; // Used to get command entered into interface
    bool running = true;

    // Attempt to create socket
    if(init(sock, serv_addr) == -1)
        return -1;

    // Clean buffers cause I like things clean
    clean_buffer(send_buffer);
    clean_buffer(recv_buffer);

    // Main client loop
    while(running)
    {
        // Clear socket list and add socket to list; not necessary, but a good habit
        FD_ZERO(&readfds);
        FD_SET(sock, &readfds);

        // Get character entered into interface
        temp = getch();

        if(temp == 7 || temp == 74) // Backspace/delete
        {
            --place;
            send_buffer[place] = '\0';
            move(LINES-1, 0);
            clrtoeol();
            printw("%s", send_buffer);
        }
        else if(temp == 10) // Enter
        {
            move(LINES-1, 0);
            clrtoeol();

            if(strncmp(send_buffer, "/exit", 5) == 0) // Exit command
                running = false;
            else
                send(sock, send_buffer, place, 0);

            clean_buffer(send_buffer);
            place = 0;
        }
        else if(temp != -1) // If character entered
        {
            send_buffer[place] = temp;
            ++place;
            printw("%c", temp);
        }

        // Wait for activity on socket for 1 millisecond
        activity = select(sock+1, &readfds, NULL, NULL, &tv);
        if(activity < 0)
            printw("Select error.");

        if(FD_ISSET(sock, &readfds))
        {
            // Activity detected
            if((valread = read(sock, recv_buffer, 1024)) == 0) // Check to see if it was closing connection from server (crash or otherwise)
            {
                printw("Server disconnect. Press any key to continue.");
                timeout(-1);
                getch();
                break;
            }
            else // Otherwise it was a message from the server, so print to interface
            {
                move(LINES-1, 0);
                clrtoeol();
                move(LINES-2, 0);
                printw("%s", recv_buffer);
                clean_buffer(recv_buffer);
                scrl(1);
                move(LINES-1, 0);
                printw("%s", send_buffer);
            }
        }

        // Draw to screen and sleep for 1 millisecond
        // Dont actually need to sleep, and probably shouldnt for practical reasons, but letting it go through the loop
        // any faster would drain my laptop's battery
        refresh();
        sleep(0.001);
    }

    // Main client loop ended, meaning it's time to close up shop!
    cleanup();
    shutdown(sock, 2);
    close(sock);
    return 0; 
} 

void clean_buffer(char buffer[], int size) // Set all character in char buffer to '\0' for resetting purposes
{
    for(int i = 0; i < size; ++i)
        buffer[i] = 0;
}

void cleanup() // Let user know client is shutting down before turning off ncurses interface
{
    move(LINES-1, 0);
    clrtoeol();
    printw("Shutting down. Press any key to exit.");
    timeout(-1);
    getch();
    endwin();
}

// Turns ncurses interface on and sets up all the socket info and whatever else the client needs to get started
int init(int& sock, struct sockaddr_in& serv_addr)
{

    // Start ncurses interface
    initscr();
    cbreak();             // Immediate key input
    timeout(0);           // Non-blocking input
    keypad(stdscr, 1);    // Fix keypad
    noecho();             // No automatic printing
    curs_set(0);          // Hide real cursor
    intrflush(stdscr, 0); // Avoid potential graphical issues
    leaveok(stdscr, 1);   // Don't care where cursor is left
    scrollok(stdscr, TRUE);
    move(LINES-1, 0);

    // Attempt to create socket
    if((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printw("\nSocket creation error\n");
        cleanup();
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    // Convert IPv4 and IPv6 addresses from text to binary form
    if(inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0)
    {
        printw("\nInvalid address / Address not supported\n");
        cleanup();
        return -1;
    }

    // Attempt to connect to server
    if(connect(sock, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) < 0)
    {
        printw("\nConnection Failed\n");
        cleanup();
        return -1;
    }

    return 1;
}
