# OS Project - Spring 2022

### Background

This program is used as a remote server shell, with a client-server architecture. It is implemented in C, with remote access built using sockets connecting on a specific port. Although Phase 1 of the project only requires a single client connection, this project in fact allows multiple clients to connect through multithreading. However, if the command cd is used, this will affect all clients. This issue may be resolved in future iterations of the project. 

### Usage & Instructions

To use this project, you need to compile the server and client. To do this, cd into the project file, type “make,” and hit enter. 

After this, you can first run the server by typing in “./server” and then run the client in a separate terminal using “./client”. The shell will open, and a prompt will show: `[om]\>`.  You can now type in commands into the client shell, and they will be sent to the server to be executed. The response will be sent back and printed on the client screen.

The full list of commands is: 
•	clear
•	echo
•	cd
•	ls
•	pwd
•	touch
•	mv	
•	mkdir
•	rmdir
•	cat
•	sort
•	uniq
•	grep
•	exit 
•	rm


We also have the composite commands pipes (uncapped, “|”), Input Redirect (<), Output Redirect (>), and Append Redirect (>>). With all commands, including composite commands, there needs to be some form of whitespace between each command. 
