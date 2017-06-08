#include "../include/dropboxServer.h"
#include "../include/dropboxUtil.h"


void receive_file(char *file, int client_socket)
{
	int fd, verification_fd;
	struct stat file_stat;
	ssize_t len;
	char buffer[256];
	int file_size;

	// Open file
    fd = open(file, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
	if (fd == -1)
    {
		printf("Error opening file");
		exit(EXIT_FAILURE);
    }

	// Receive file size
	len = recv(client_socket, buffer, sizeof(buffer), 0);
	if (len < 0)
	{
		printf("Error on receiving file size");
		exit(EXIT_FAILURE);
	}
	file_size = atoi(buffer);
	printf("Server receive %d bytes for the file size\n", len);

	// Receive file data
	while(lseek(fd, 0, SEEK_END) < file_size)
	{
		ssize_t read_len = recv(client_socket, buffer, sizeof(buffer), 0);
		if (read_len < 0)
		{
			printf("Error on receiving file data");
			exit(EXIT_FAILURE);
		}

		// Write file data
		len = write(fd, buffer, read_len);
		if (len < 0)
		{
			printf("Error on writing file");
			exit(EXIT_FAILURE);
		}
	}

	close(fd);
}

void send_file(char *file, int client_socket)
{
	int fd, verification_fd;
	struct stat file_stat;
	ssize_t len;
	int remain_data;
	char file_size[256];
	int offset;
	int sent_bytes = 0;

	// Open file
	fd = open(file, O_RDONLY);
	if (fd == -1)
    {
		printf("Error opening file");
		exit(EXIT_FAILURE);
    }

	// Get file stats
	if (fstat(fd, &file_stat) < 0)
	{
		printf("Error fstat");
		exit(EXIT_FAILURE);
	}

	// Send file size
	remain_data = file_stat.st_size;
	itoa(remain_data, file_size, 10);
	len = send(client_socket, file_size, sizeof(file_size), 0);
	if (len < 0)
	{
		printf("Error on sending file size");
		exit(EXIT_FAILURE);
	}
	printf("Server sent %d bytes for the file size\n", len);

	// Send file data
	offset = 0;
	while (((sent_bytes = sendfile(client_socket, fd, &offset, BUFSIZ)) > 0) && (remain_data > 0))
	{
		remain_data -= sent_bytes;
		printf("Server sent %d bytes from file's data, offset is now : %d and remaining data = %d\n", sent_bytes, offset, remain_data);
	}

	close(fd);
}

void run_thread(void *socket_client)
{
	char buffer[BUFFER_SIZE];
	int socketfd = *(int*)socket_client;
	int message;
	printf("i created a thread\n");
	

	while(1) {
		bzero(buffer, BUFFER_SIZE);
		message = read(socketfd, buffer, BUFFER_SIZE);
		if (message < 0) 
			printf("ERROR reading from socket");
		else {
			printf("Here is the message: %s\n", buffer);
			// write
			message = write(socketfd,"I got your message", 18);
		}
		
	}

	free(socket_client);

	close(socketfd);
}

int main(int argc, char *argv[])
{
	int socket_connection, socket_client;
	socklen_t client_len;
	struct sockaddr_in serv_addr, client_addr;

	
	if(argc <= MIN_ARG)
	{
		printf("Not enough arguments passed.");
		exit(1);
	}
	
	int PORT = atoi(argv[1]);
	
	if ((socket_connection = socket(AF_INET, SOCK_STREAM, 0)) == -1) 
        printf("ERROR opening socket");
	
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(PORT);
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	bzero(&(serv_addr.sin_zero), 8);     
    
	if (bind(socket_connection, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) 
		printf("ERROR on binding");
	
	listen(socket_connection, MAX_CONNECTIONS);
	client_len = sizeof(struct sockaddr_in);
	
	while(1)
	{
		if( (socket_client = accept(socket_connection, (struct sockaddr *) &client_addr, &client_len)) )

		{
			printf("accepted a client\n");
			
			int *new_sock;
			new_sock = malloc(1);
        	*new_sock = socket_client;
			pthread_t client_thread;
		
			pthread_create(&client_thread, NULL, run_thread, (void*)new_sock);
		
			pthread_detach(client_thread);

			//free(new_sock);
		}
		
	}

	printf("saí do meu while (server)\n");
	
	return 0; 
}
