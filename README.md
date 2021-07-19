DESCRIPTION
This is an Internet Relay Chat (IRC) project created in Internetworking Protocols class. This project uses two programs, each coded in C++: the client and the server. The server program contains a list of rooms, which upon creation, only consists of one room called the Master Room; more can be created by users. The program accepts connections from client programs (up to 30) but does not automatically put them in a room. Once they join a room, the client can input a number of commands, listed in the USAGE section. Additionally, the server is able to issue messages to the Master Room and shut itself down via the command line.

USAGE
Note: Requires Linux

Server:
1. Open server folder in terminal
2. Run server using "./server"
3. Wait for client connection
4. Once clients have joined, you can see all sent messages
5. Use "/exit" to close server

Client:
1. On same machine as server, open IRC folder in terminal
2. Run client using "./client"
3. Program will automatically connect to server program if running on same machine
4. Use "/join Master_Room" to join the Master Room
5. Use commands however you wish
6. To close, type "/exit"

Commands:
/setname {X} -- Sets client username to X
/create {X} -- Attempts to create room called X; if room list is maxed out at 20, fails
/listroom -- Lists all active rooms
/join {X} -- Attemps to connect to room X, allowing messages to be sent; if room is full at 20, fails
/leave -- Disconnects client from room, NOT from server
/listmembers {X} -- Lists members of room X
/send {X} {Y} -- Sends message Y to room X
