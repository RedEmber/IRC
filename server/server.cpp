#include <ncurses.h>
#include <stdio.h>  
#include <string.h>
#include <stdlib.h>  
#include <errno.h>  
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>  
#include <sys/socket.h>  
#include <netinet/in.h>  
#include <time.h>

#define BUFF_SIZE 1024
#define MAX_CLIENTS 30
#define GEN_SIZE 20
#define PORT 8080

struct room
{
    char name[GEN_SIZE];
    int members[GEN_SIZE];
    int id; // Will be 0 if room does not technically exist, used for room creation
};

void pre_print();
void post_print(char* buffer);
void clean_buffer(char[], int = BUFF_SIZE - 1);
void cleanup();
void init(int&, int&, int&, int*, int, int, struct sockaddr_in&);

int main(int argc , char *argv[])   
{
    fd_set readfds;
    struct timeval tv = {0, 1000}; // Socket polling timeout -- set to 1000 microseconds, or 1 millisecond
    struct sockaddr_in address; // Socket
    struct room rooms[GEN_SIZE]; // Set of rooms
    int master_socket , addrlen , new_socket , client_socket[30], place = 0, activity, valread, sd, max_sd;
    int i, j, k, l; // Loop variables
    char message[] = "Welcome to Ruby's IRC!\0"; // Welcome message
    char client_names[MAX_CLIENTS][GEN_SIZE];
    char send_buffer[BUFF_SIZE]; // Used to get server commands from own interface -- currently only used to shut down server
    char recv_buffer[BUFF_SIZE]; // Used to get commands from clients
    char temp; // Used to get server commands
    bool running = true;

    init(master_socket, addrlen, new_socket, client_socket, MAX_CLIENTS, 1, address); // Starts server and display interface

    // Make all rooms empty and create one Master Room
    for(i = 0; i < GEN_SIZE; ++i)
    {
        clean_buffer(rooms[i].name, GEN_SIZE);
        for(j = 0; j < GEN_SIZE; ++j)
            rooms[i].members[j] = 0;
        rooms[i].id = 0;
    }
    strncpy(rooms[0].name, "Master_Room\0", 12);
    rooms[0].id = 1;

    // Set data in buffers to null characters, cause I like things clean
    for(i = 0; i < MAX_CLIENTS; ++i)
        clean_buffer(client_names[i], GEN_SIZE);
    clean_buffer(send_buffer);
    clean_buffer(recv_buffer);

    // Accept an incoming connection
    addrlen = sizeof(address);
    printw("Waiting for connections ...");
    scrl(2);
    move(LINES-1, 0);

    // Main Server Loop
    while(running)
    {
        // Clear the socket set and add master socket
        FD_ZERO(&readfds);
        FD_SET(master_socket, &readfds);
        max_sd = master_socket;

        // Add client sockets to set
        for(i = 0; i < MAX_CLIENTS; i++)
        {
            sd = client_socket[i];

            // If valid socket descriptor then add to read list
            if(sd > 0)
                FD_SET(sd , &readfds);

            // Highest file descriptor number
            if(sd > max_sd)
                max_sd = sd;
        }

        // Gets characters input into server interface
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
            pre_print();

            if(strncmp(send_buffer, "/exit", 5) == 0) // Exit command
                running = false;
            else
                printw(send_buffer);

            clean_buffer(send_buffer);
            post_print(send_buffer);
            place = 0;
        }
        else if(temp != -1) // If character entered
        {
            send_buffer[place] = temp;
            ++place;
            printw("%c", temp);
        }

        // Wait for activity on one of the sockets
        activity = select( max_sd + 1 , &readfds , NULL , NULL , &tv);

        if(activity < 0 && errno!=EINTR)
        {
            pre_print();
            printw("select error");
            post_print(send_buffer);
        }

        if(activity != 0)
        {
            // Activity detected
            if(FD_ISSET(master_socket, &readfds)) // If something happened on the master socket, it is an incoming connection
            {
                // Accept connection
                if ((new_socket = accept(master_socket, (struct sockaddr*) &address, (socklen_t*) &addrlen)) < 0)
                {
                    cleanup();
                    perror("accept");
                    exit(EXIT_FAILURE);
                }

                // Add new socket to array of sockets
                for (i = 0; i < MAX_CLIENTS; i++)
                {
                    if(client_socket[i] == 0)
                    {
                        client_socket[i] = new_socket;
                        pre_print();
                        printw("Adding to list of sockets as %d\n" , i);
                        post_print(send_buffer);
                        break;
                    }
                }

                // Generate client username
                strncpy(client_names[i], "User", 4);
                client_names[i][4] = rand() % 10 + 48;
                client_names[i][5] = rand() % 10 + 48;

                // Log client connection information on server interface
                pre_print();
                printw("New connection: %s, SockFD %d, IP %s, Port %d\n", client_names[i], new_socket, inet_ntoa(address.sin_addr), ntohs(address.sin_port));
                post_print(send_buffer);

                // Send welcome message
                if(send(new_socket, message, strlen(message), 0) != strlen(message))
                    perror("send");

                pre_print();
                printw("Welcome message sent successfully");
                post_print(send_buffer);

            }
            else // Otherwise its a message from a client
            {
                // Search for client that sent message
                for (i = 0; i < MAX_CLIENTS; i++)
                {
                    sd = client_socket[i];

                    if (FD_ISSET(sd , &readfds))
                    {
                        if ((valread = read(sd, recv_buffer, BUFF_SIZE)) == 0) // Check if message was for closing (crashing or otherwise)
                        {
                            // Log disconnected client information on server interface
                            getpeername(sd, (struct sockaddr*)&address, (socklen_t*)&addrlen);
                            pre_print();
                            printw("Host disconnected, ip %s, port %d\n", inet_ntoa(address.sin_addr), ntohs(address.sin_port));
                            post_print(send_buffer);

                            // Remove client from any rooms if applicable
                            for(j = 0; j < GEN_SIZE; ++j)
                                for(k = 0; k < GEN_SIZE; ++k)
                                    if(rooms[j].members[k] == sd)
                                        rooms[j].members[k] == 0;

                            // Close the socket and mark as 0 in list for reuse
                            shutdown(sd, 2);
                            close(sd);
                            client_socket[i] = 0;
                            clean_buffer(client_names[i], GEN_SIZE);
                        }
                        else // Otherwise message is a command
                        {
                            // Command process
                            if(strncmp(recv_buffer, "/setname ", 9) == 0) // Client requested username to be changed
                            {
                                if(recv_buffer[9] == '\0') // Nothing is found where name should be found
                                    send(sd, "Server> Error: No name found.\0", 30, 0);
                                else if(recv_buffer[8 + GEN_SIZE] != '\0') // Name is too long for server to hold; limit is GEN_SIZE
                                    send(sd, "Server> Error: Name too long.\0", 30, 0);
                                else
                                {
                                    // Purge old name
                                    clean_buffer(client_names[i], GEN_SIZE);

                                    // Copy new name into client_names list
                                    for(j = 0; j < GEN_SIZE; ++j)
                                        client_names[i][j] = recv_buffer[j+9];

                                    send(sd, "Server> Name changed.\0", 21, 0);
                                }
                            }
                            else if(strncmp(recv_buffer, "/create ", 8) == 0) // Client requested to create a room
                            {
                                if(recv_buffer[8] == '\0')
                                    send(sd, "Server> Error: No name found.\0", 30, 0);
                                else if(recv_buffer[7 + GEN_SIZE] != '\0')
                                    send(sd, "Server> Error: Name too long.\0", 30, 0);
                                else
                                {
                                    // Search for a closed room to create
                                    for(j = 1; j < GEN_SIZE; ++j)
                                        if(rooms[j].id == 0)
                                        {
                                            int new_id = 2;

                                            // Apply room name
                                            for(k = 0; k < GEN_SIZE; ++k)
                                                if(recv_buffer[k+8] != ' ')
                                                    rooms[j].name[k] = recv_buffer[k+8];
                                                else // If there is a space character, break because that is not allowed
                                                {
                                                    j = GEN_SIZE * 2;
                                                    break;
                                                }

                                            if(j == GEN_SIZE * 2) // Detects if the else condition was met in the for loop to break out of this loop too
                                                break;

                                            // Create room ID; must be unique (doesnt need to be)
                                            for(k = 1; k < GEN_SIZE; ++k)
                                                if(rooms[j].id == new_id)
                                                {
                                                    ++new_id;
                                                    k = 0;
                                                }
                                            rooms[j].id = new_id;
                                            break;
                                        }

                                    if(j == GEN_SIZE * 2) // Space was detected in the name parameter, which is not allowed
                                        send(sd, "Server> Error: Must not contain a space.\0", 41, 0);
                                    else if(j == GEN_SIZE) // break never occured, which means there was no closed room
                                        send(sd, "Server> Error: No rooms available.\0", 35, 0);
                                    else
                                        send(sd, "Server> Room created.\0", 22, 0);
                                }
                            }
                            else if(strncmp(recv_buffer, "/listrooms", 10) == 0) // Client requested to list all rooms
                            {
                                // This should be pretty straightforward...
                                send(sd, "--------------------\n", 21, 0);
                                for(j = 0; j < GEN_SIZE; ++j)
                                    if(rooms[j].id != 0)
                                    {
                                        send(sd, rooms[j].name, strlen(rooms[j].name), 0);
                                        send(sd, "\n", 1, 0);
                                    }

                                send(sd, "--------------------\n", 21, 0);
                                send(sd, "Server> Rooms displayed.\n", 25, 0);
                            }
                            else if(strncmp(recv_buffer, "/join ", 6) == 0) // Client requested to join a room
                            {
                                if(recv_buffer[6] == '\0')
                                    send(sd, "Server> Error: No name found.\0", 30, 0);
                                else if(recv_buffer[5 + GEN_SIZE] != '\0')
                                    send(sd, "Server> Error: Name too long.\0", 30, 0);
                                else
                                {
                                    // Variables used to get name for server to join
                                    int len = strlen(recv_buffer) - 5;
                                    char room[len];

                                    // Getting room name
                                    for(j = 0; j < len; ++j)
                                        room[j] = recv_buffer[j + 6];

                                    // Search for room with that name
                                    for(j = 0; j < GEN_SIZE; ++j)
                                        if(strncmp(room, rooms[j].name, len-1) == 0) // Room found
                                        {
                                            // Search room availability
                                            for(k = 0; k < GEN_SIZE; ++k)
                                                if(rooms[j].members[k] == sd) // Client already in room
                                                {
                                                    send(sd, "Server> Error: Already in room.\0", 32, 0);
                                                    break;
                                                }
                                                else if(rooms[j].members[k] == 0 && rooms[j].members[k] != sd) // Room has space
                                                {
                                                    send(sd, "Server> Room joined.\0", 21, 0);
                                                    rooms[j].members[k] = sd;
                                                    break;
                                                }

                                            if(k == GEN_SIZE) // break was never reached, meaning room had no availability
                                                send(sd, "Server> Error: Room is full.\0", 29, 0);

                                            break;
                                        }

                                    if(j == GEN_SIZE)
                                        send(sd, "Server> Error: Room not found.\0", 31, 0);
                                }
                            }
                            else if(strncmp(recv_buffer, "/leave ", 7) == 0) // Client requested to leave room
                            {
                                if(recv_buffer[7] == '\0')
                                    send(sd, "Server> Error: No name found.\0", 30, 0);
                                else if(recv_buffer[6 + GEN_SIZE] != '\0')
                                    send(sd, "Server> Error: Name too long.\0", 30, 0);
                                else
                                {
                                    int len = strlen(recv_buffer) - 6;
                                    char room[len];

                                    // Get room name
                                    for(j = 0; j < len; ++j)
                                        room[j] = recv_buffer[j + 7];

                                    // Search for room with that name
                                    for(j = 0; j < GEN_SIZE; ++j)
                                        if(strncmp(room, rooms[j].name, len-1) == 0) // Room found
                                        {
                                            // Search room member list for this client
                                            for(k = 0; k < GEN_SIZE; ++k)
                                                if(rooms[j].members[k] == sd) // Remove client
                                                {
                                                    rooms[j].members[k] = 0;
                                                    send(sd, "Server> Left room.\0", 19, 0);
                                                    break;
                                                }

                                            if(k == GEN_SIZE) // break was never reached, meaning client was not in room
                                                send(sd, "Server> Error: Not in room.\0", 28, 0);

                                            break;
                                        }

                                    if(j == GEN_SIZE)
                                        send(sd, "Server> Error: Room not found.\0", 31, 0);
                                }
                            }
                            else if(strncmp(recv_buffer, "/listmembers ", 13) == 0) // Client requested to list members of a room
                            {
                                if(recv_buffer[13] == '\0')
                                    send(sd, "Server> Error: No name found.\0", 30, 0);
                                else if(recv_buffer[12 + GEN_SIZE] != '\0')
                                    send(sd, "Server> Error: Name too long.\0", 30, 0);
                                else
                                {
                                    int len = strlen(recv_buffer) - 12;
                                    char room[len];

                                    // Get room name
                                    for(j = 0; j < len; ++j)
                                        room[j] = recv_buffer[j + 13];

                                    // Search for room with that name
                                    for(j = 0; j < GEN_SIZE; ++j)
                                    {
                                        if(strncmp(room, rooms[j].name, len-1) == 0) // Room found
                                        {
                                            // Begin listing of members in room
                                            send(sd, "--------------------\n", 21, 0);
                                            for(k = 0; k < GEN_SIZE; ++k) // Cycle through room member list
                                                if(rooms[j].members[k] != 0)
                                                    for(l = 0; l < MAX_CLIENTS; ++l) // Search client_names list with corresponding socket number to members list
                                                        if(rooms[j].members[k] == client_socket[l])
                                                        {
                                                            send(sd, client_names[l], strlen(client_names[l]), 0);
                                                            send(sd, "\n", 1, 0);
                                                            break;
                                                        }
                                            send(sd, "--------------------\n", 21, 0);
                                            break;
                                        }
                                    }

                                    if(j == GEN_SIZE)
                                        send(sd, "Server> Error: Room not found.\0", 30, 0);
                                    else
                                        send(sd, "Server> Members listed.\n", 24, 0);
                                }
                            }
                            else if(strncmp(recv_buffer, "/send ", 6) == 0) // Client requested to send message to a room TODO
                            {
                                if(recv_buffer[6] == '\0')
                                    send(sd, "Server> Error: No name found.\0", 30, 0);
                                else
                                {
                                    int count = 0;

                                    for(j = 0; j < GEN_SIZE; ++j) // Count the size of the room name parameter
                                        if(recv_buffer[j+6] != ' ') // Separator between parameters is a space
                                            ++count;
                                        else
                                            break;

                                    if(j == GEN_SIZE && recv_buffer[j+6] != ' ') // Check to make sure room name parameter isnt too long
                                        send(sd, "Server> Error: Name too long.\0", 30, 0);
                                    else
                                    {
                                        char room[count];

                                        for(j = 0; j < count; ++j) // Get the actual room name
                                            room[j] = recv_buffer[j+6];

                                        for(j = 0; j < GEN_SIZE; ++j) // Find the corresponding room
                                            if(strncmp(rooms[j].name, room, count) == 0) // Room found
                                            {
                                                int name = strlen(client_names[i]); // This is used a lot so it is saved
                                                int len = strlen(&recv_buffer[7 + count]) + name + count + 6;
                                                char message[len];

                                                // Building the message
                                                strncpy(message, client_names[i], name); // Input name of client sending message
                                                strncpy(&message[name], " to ", 4); // Separates client and room names
                                                strncpy(&message[name+4], room, count); // Input name of room
                                                strncpy(&message[name+4+count], "> ", 2); // Separate message preface from actual message

                                                for(k = 0; k < len; ++k) // Get actual message
                                                    message[k+name+count+6] = recv_buffer[7+count+k];

                                                for(k = 0; k < GEN_SIZE; ++k) // Cycle through room member list and send message to all clients in that room
                                                    if(rooms[j].members[k] != 0)
                                                        send(rooms[j].members[k], message, len, 0);

                                                break;
                                            }

                                        if(j == GEN_SIZE)
                                            send(sd, "Server> Error: Room not found.\0", 31, 0);
                                    }
                                }


                                /* This deprecated code allows clients to send messages to the whole server. It was used for debugging,
                                    but is kept in case a new /announce command is desired.

                                   int len = strlen(client_names[i]);
                                   char message[len + 2 + strlen(recv_buffer) - 5];

                                   strncpy(message, client_names[i], len);
                                   message[len] = '>';
                                   message[len+1] = ' ';

                                   for(j = 0; j < strlen(recv_buffer) - 5; ++j)
                                   message[len+2+j] = recv_buffer[j+6];
                                   message[len+1+j] = '\0';

                                   for(j = 0; j < MAX_CLIENTS; ++j)
                                   if(client_socket[j] != 0)
                                   send(client_socket[j], message, strlen(message) + 1, 0);
                                 */
                            }
                            else // Client requested invalid command
                                send(sd, "Server> Error: Command not found.\0", 34, 0);

                            clean_buffer(recv_buffer);
                        }
                    }
                }
            }
        }

        sleep(0.001); // Used to prevent server from running too fast -- mostly to save my laptop's battery for testing
    }

    // Server loop has ended, which means its closing time!
    cleanup();
    for(int i = 0; i < MAX_CLIENTS; ++i) // Close all client connections
    {
        shutdown(client_socket[i], 2);
        close(client_socket[i]);
    }
    shutdown(master_socket, 2);
    close(master_socket);
    return 0;
}

void pre_print() // First function used to print message to server interface without destroying interface input
{
    move(LINES-1, 0);
    clrtoeol();
    move(LINES-2, 0);
}

void post_print(char* buffer) // Second function corresponding to above
{
    scrl(1);
    move(LINES-1, 0);
    printw("%s", buffer);
}

/*
NOTE: The reason I have these two functions is because I am using the ncurses library.
It has its own print function which can take a variable range of arguments. However, I
do not know how to replicate this, so I created these two functions to put before and
after the print function. The reason I have to do this at all is because if I want to
print to the interface screen WHILE ALSO take input from the command line, I have to
destroy whats on the command line, print, and restore what was on the command line.
 */

void clean_buffer(char buffer[], int size) // Cycle through char buffer and set data to '\0' for cleanliness/resetting purposes
{
    for(int i = 0; i < size; ++i)
        buffer[i] = '\0';
}

void cleanup() // Let client know server is shutting down before turning off ncurses interface
{
    move(LINES-1, 0);
    clrtoeol();
    printw("Shutting down. Press any key to continue.");
    timeout(-1);
    getch();
    endwin();
}

// Turns ncurses interface on and sets up all the socket info and whatever else the server needs to get started
void init(int& master_socket, int& addrlen, int& new_socket, int* client_socket, int max_clients, int opt, struct sockaddr_in& address)
{
    int i;

    // Start ncurses interface and enable certain properties
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
    srand(time(NULL));

    // Set client sockets to empty to represent that there are no clients currently connected
    for (i = 0; i < max_clients; i++)
        client_socket[i] = 0;

    // Create a master socket for client connection
    if((master_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        cleanup();
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Set master socket to allow multiple connections
    if(setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR, (char*) &opt, sizeof(opt)) < 0)
    {
        cleanup();
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    // Type of socket created  
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons( PORT );

    // Bind the socket to localhost port, which in this case is 8080
    if(bind(master_socket, (struct sockaddr*) &address, sizeof(address)) < 0)
    {
        cleanup();
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    printw("Listener on port %d \n", PORT);

    // Try to specify maximum of 3 pending connections for the master socket
    if(listen(master_socket, 3) < 0)
    {
        cleanup();
        perror("listen");
        exit(EXIT_FAILURE);
    }
}
