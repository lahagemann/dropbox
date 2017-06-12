//#include "../include/dropboxServer.h"
//#include "../include/dropboxClient.h"

#include "dropboxServer.h"
#include "dropboxUtil.h"
//#include "../../include/fila2.h"
#include "fila2.h"

pthread_mutex_t queue;
FILA2 connected_clients;
char home[256];

/*
	TODO:
		- NO SYNC_SERVER:
			- deletar arquivo da pasta sync_dir_<user> do servidor
			- chamar função de deletar arquivo n da estrutura do cliente
		- @LARISSA: colocar tuas funções aqui dentro do dropboxServer.c
		- criar no loop o comando list
		
		DEPOIS QUE ARRUMAR ISSO TUDO:
		- ver os mutex!
*/

void list_files(file_info files[256]){
    int i;
    char filename[256];      
    for(i=0;i<256;i++){
        if (strcmp(files[i].name,"") == 0)
            break;
        else{
            strcpy(filename, "");
            strcat(filename, files[i].name);
            strcat(filename, files[i].extension);

            printf("%s", filename);
        }
            
    }

}

int return_client(char* user_name, client *new_client){
    client *tempinfo;
	pthread_mutex_lock(&queue);

	FirstFila2(&connected_clients);
	
	if(connected_clients.it)
	{
		new_client = (client *)GetAtIteratorFila2(&connected_clients);
		if(new_client)
		{
			if (strcmp(new_client->userid, user_name) == 0)
			{
				printf("found client\n");
				printf("%s\n", new_client->userid);
				pthread_mutex_unlock(&queue);
				return ACCEPTED;
			}
		}
		

		while(NextFila2(&connected_clients) == 0)
		{
			new_client = (client *)GetAtIteratorFila2(&connected_clients);
			if(new_client)
			{
				if (strcmp(new_client->userid, user_name) == 0)
				{
					pthread_mutex_unlock(&queue);
					return ACCEPTED;
				}
			}
		}		
	}

	// não achou na fila
	new_client = NULL;
	pthread_mutex_unlock(&queue);
	return NOT_VALID;
}

void disconnect_client(client *clientinfo){
	pthread_mutex_lock(&queue);
	
	client *tempinfo;
	tempinfo = (client*)(GetAtIteratorFila2(&connected_clients));
	while(strcmp(tempinfo->userid, clientinfo->userid) != 0){
		NextFila2(&connected_clients);
		tempinfo = (client*)(GetAtIteratorFila2(&connected_clients));
	}
		
	//se tem dois conectados, disconecta um
	if(tempinfo->devices[1] == 1)
		tempinfo->devices[1] = 0;
	else //se não, remove a estrutura
		DeleteAtIteratorFila2(&connected_clients);
		
	pthread_mutex_unlock(&queue);
}



int insert_client(client *clientinfo){
	pthread_mutex_lock(&queue);
	
	//busca se já existe
	FirstFila2(&connected_clients);
	
	if(connected_clients.it)
	{
		client *tempinfo = (client *)GetAtIteratorFila2(&connected_clients);
		if(tempinfo)
		{
			if (strcmp(tempinfo->userid, clientinfo->userid) == 0)
			{
				if(tempinfo->devices[1] == 1) //terceiro cliente não pode logar
				{	
					pthread_mutex_unlock(&queue);
					return NOT_VALID;	
				}
				else
				{
					tempinfo->devices[1] = 1;
					pthread_mutex_unlock(&queue);
					return ACCEPTED;
				}
			}
		}

		while(NextFila2(&connected_clients) == 0)
		{
			tempinfo = (client *)GetAtIteratorFila2(&connected_clients);
			if(tempinfo)
			{
				if (strcmp(tempinfo->userid, clientinfo->userid) == 0)
				{
					if(tempinfo->devices[1] == 1) //terceiro cliente não pode logar
					{
						pthread_mutex_unlock(&queue);
						return NOT_VALID;	
					}					
					else
					{
						tempinfo->devices[1] = 1;
						pthread_mutex_unlock(&queue);
						return ACCEPTED;
					}
				}
			}
		}

		//nao achou cliente na fila, insere no fim
		AppendFila2(&connected_clients, (void*)clientinfo);
		tempinfo->devices[0] = 1;
		pthread_mutex_unlock(&queue);

		return ACCEPTED;
		
	}
	else
	{
		//fila vazia.
		
		clientinfo->devices[0] = 1;
		AppendFila2(&connected_clients, (void*)clientinfo);
		pthread_mutex_unlock(&queue);
		return ACCEPTED;
	}

	pthread_mutex_unlock(&queue);

	return NOT_VALID;
}


void* run_client(void *conn_info)
{
	char buffer[BUFFER_SIZE];
	char message;
	connection_info ci = *(connection_info*)conn_info;
	int socketfd = ci.socket_client;

	printf("entrei no run client\n");

	// VAI RECEBER O ID DO CLIENTE ANTES DE CRIAR O SYNC
	// AQUI ELE TEM QUE ACEITAR O CLIENTE E ENVIAR MENSAGEM DE OK
	char clientid[MAXNAME];

	bzero(buffer, BUFFER_SIZE);
	read(socketfd, buffer, BUFFER_SIZE);
	memcpy(clientid, buffer, MAXNAME);

	client *cli = malloc(sizeof(client));
	init_client(cli, home, clientid);
	
	if(insert_client(cli) == ACCEPTED)
	{
		bzero(buffer, BUFFER_SIZE);
		buffer[0] = 'A';		
		write(socketfd, buffer, BUFFER_SIZE);
	}
	else
	{
		bzero(buffer, BUFFER_SIZE);
		buffer[0] = 'N';
		write(socketfd, buffer, BUFFER_SIZE);
		
		close(socketfd);
		pthread_exit(0);
	}

	printf("EEEEEEEEE\n");
	
	// fica esperando a segunda conexão do sync e quando recebe, cria outro socket/thread.
	int socket_connection, socket_sync;
	socklen_t sync_len;
	struct sockaddr_in serv_addr, sync_addr;

	int PORT = ci.port+1;
	
	if ((socket_connection = socket(AF_INET, SOCK_STREAM, 0)) == -1) 
        printf("ERROR opening sync socket");
	
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(PORT);
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	bzero(&(serv_addr.sin_zero), 8);     
    
	if (bind(socket_connection, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) 
		printf("ERROR on binding sync");
	
	listen(socket_connection, 1);
	sync_len = sizeof(struct sockaddr_in);
	
	if( (socket_sync = accept(socket_connection, (struct sockaddr *) &sync_addr, &sync_len)) )
	{
		int *new_sock;
		new_sock = malloc(1);
       	*new_sock = socket_sync;
		pthread_t sync_thread;
		pthread_create(&sync_thread, NULL, run_sync, (void*)new_sock);
		pthread_detach(sync_thread);

	}
	
	// terminou de criar a thread de sync. agora pode executar o loop normal.
 
	while(1) {
		bzero(buffer, BUFFER_SIZE);
		message = read(socketfd, buffer, BUFFER_SIZE);
		if (message < 0) 
			printf("ERROR reading from socket");
		else {
			memcpy(&message, buffer, 1);

			//nao sei se é exatamente assim
			switch(message){
				case EXIT:
					disconnect_client(cli);
					pthread_exit(0);
					break;
				case DOWNLOAD:
					bzero(buffer, BUFFER_SIZE);
					read(socketfd, buffer, BUFFER_SIZE);
					send_file(buffer, socketfd);
					break;
				case UPLOAD:
					bzero(buffer, BUFFER_SIZE);
					read(socketfd, buffer, BUFFER_SIZE);
					receive_file(buffer, socketfd);
					break;
			}
		}
	}

	//free(socket_sync);

	close(socketfd);
}

void* run_sync(void *socket_sync)
{
	printf("running sync!\n");
	char buffer[BUFFER_SIZE];
	int socketfd = *(int*)socket_sync;
	char message;

	while(1) {
		printf("while do sync\n");
		bzero(buffer, BUFFER_SIZE);
		message = read(socketfd, buffer, BUFFER_SIZE);

		if (message < 0) 
			printf("ERROR reading from socket");
		else 
		{
			memcpy(&message, buffer, 1);

			if(message == SYNC)
			{
				printf("message == sync\n");
				char client_id[MAXNAME];
				//recebe id do cliente. ---> VER SE NÃO É MELHOR ELE RECEBER ANTES???
				//pegar os dados do buffer
				bzero(buffer, BUFFER_SIZE);
				read(socketfd, buffer, BUFFER_SIZE);
				memcpy(client_id, buffer, MAXNAME);

				printf("buff: %s\n", buffer);
				
				// pega cliente na lista de clientes e envia o mirror para o cliente.
				client *cli = malloc(sizeof(client));
				return_client(client_id, cli); 

				update_client(cli, home);
			
				printf("got client! %s\n", cli->userid);

				bzero(buffer,BUFFER_SIZE);
				memcpy(buffer, cli, sizeof(client *));
				write(socketfd, buffer, BUFFER_SIZE);

				printf("ok\n");

				// agora fica em um while !finished, fica recebendo comandos de download/delete
				while(1)
				{	
					printf("while dos arquivos\n");
					
					char command;
					char fname[MAXNAME];

					bzero(buffer,BUFFER_SIZE);
					read(socketfd, buffer, BUFFER_SIZE);
					command = buffer[0];

					printf("command: %c\n", command);

					if(command == DOWNLOAD)
					{
						printf("downloading\n");
						// recebe nome do arquivo
						bzero(buffer,BUFFER_SIZE);
						read(socketfd, buffer, BUFFER_SIZE);
						memcpy(fname, buffer, MAXNAME);
						
						// procura arquivo
						int index = search_files(cli, fname);
						file_info f;

						if(index >= 0)
							f = cli->fileinfo[index];

						// manda struct
						bzero(buffer,BUFFER_SIZE);
						memcpy(buffer, &f, sizeof(file_info));
						write(socketfd, buffer, BUFFER_SIZE);

						// receive file funciona com full path
						char *fullpath;
						strcat(fullpath, home);
						strcat(fullpath, "/sync_dir_");
						strcat(fullpath, cli->userid);
						strcat(fullpath, "/");
						strcat(fullpath, f.name);
						strcat(fullpath, ".");
						strcat(fullpath, f.extension);

						// manda arquivo				
						send_file(fullpath, socketfd);
					}
					else if(command == DELETE)
					{
						bzero(buffer,BUFFER_SIZE);

						// recebe nome do arquivo
						read(socketfd, buffer, BUFFER_SIZE);
						memcpy(fname, buffer, MAXNAME);

						// procura arquivo
						int index = search_files(cli, fname);
						file_info f;

						if(index >= 0)
							f = cli->fileinfo[index];

						// deleta arquivo da pasta sync do server
						char *fullpath;
						strcpy(fullpath, home);
						strcpy(fullpath, "/sync_dir_");
						strcpy(fullpath, cli->userid);
						strcpy(fullpath, "/");
						strcpy(fullpath, f.name);
						strcpy(fullpath, ".");
						strcpy(fullpath, f.extension);
			
						remove_file(fullpath);

						// deleta estrutura da lista de arquivos do cliente
						delete_file_from_client_list(cli, fname);
					}
					else
						break;
				}
				
				printf("saí do while\n");
				// aí executa aqui o sync_server.
				sync_server(socketfd);
			}
		}
	}
}

void sync_server(int socketfd)
{

	printf("entrei no sync_server\n");
	struct client client_mirror;
	char buffer[BUFFER_SIZE];

	//server fica esperando o cliente enviar seu mirror
	bzero(buffer,BUFFER_SIZE);
	read(socketfd, buffer, BUFFER_SIZE);
	memcpy(&client_mirror, buffer, sizeof(struct client));

	printf("%s\n", client_mirror.userid);

	// TODO: função que recupera o cliente com client_mirror->userid da lista de clientes.
	client *cli = malloc(sizeof(client));
	return_client(client_mirror.userid, cli);
	update_client(cli, home);

	printf("%s\n", cli->userid);
  
  	// pra cada arquivo do cliente:
  	int i;
  	for(i = 0; i < MAXFILES; i++) 
    {
		printf("files\n");
      	if(strcmp(client_mirror.fileinfo[i].name, "\0") == 0)
		{
			printf("break\n");
           break;
		}
    	else
        {
			printf("stcmp != 0\n");
        	// arquivo existe no servidor?
			int index = search_files(cli, client_mirror.fileinfo[i].name);
			
			if(index >= 0)		// arquivo existe no servidor
			{
				//verifica se o arquivo no cliente tem commit_created/modified > state do servidor.
				if(client_mirror.fileinfo[i].commit_modified > cli->current_commit)
				{
					//isso quer dizer que o arquivo no servidor é de um commit mais novo que o estado atual do cliente.
					// pede para o cliente mandar o arquivo
					bzero(buffer,BUFFER_SIZE);
					buffer[0] = DOWNLOAD;
					write(socketfd, buffer, BUFFER_SIZE);

					bzero(buffer,BUFFER_SIZE);
					memcpy(buffer, &client_mirror.fileinfo[i].name, MAXNAME);
					write(socketfd, buffer, BUFFER_SIZE);
		
					struct file_info f;
					//fica esperando receber struct
					bzero(buffer,BUFFER_SIZE);
					read(socketfd, buffer, BUFFER_SIZE);
					memcpy(&f, buffer, sizeof(struct file_info));
				
					// receive file funciona com full path
					char *fullpath;
					strcat(fullpath, home);
					strcat(fullpath, "/sync_dir_");
					strcat(fullpath, cli->userid);
					strcat(fullpath, "/");
					strcat(fullpath, f.name);
					strcat(fullpath, ".");
					strcat(fullpath, f.extension);
			
					//recebe arquivo
					receive_file(fullpath, socketfd);

					// atualiza na estrutura do cliente no servidor.
					cli->fileinfo[index] = f;
				}
			}
			else				// arquivo não existe no servidor
			{
				printf("arquivo não tem no servidor\n");
				// verifica se o arquivo no cliente tem um commit_modified > state do servidor
				if(client_mirror.fileinfo[i].commit_modified > cli->current_commit)
				{
					//isso quer dizer que é um arquivo novo colocado no servidor em outro pc.
					// pede para o cliente mandar o arquivo
					bzero(buffer,BUFFER_SIZE);
					buffer[0] = DOWNLOAD;
					write(socketfd, buffer, BUFFER_SIZE);

					bzero(buffer,BUFFER_SIZE);

					memcpy(buffer, &client_mirror.fileinfo[i].name, MAXNAME);
					write(socketfd, buffer, BUFFER_SIZE);

					struct file_info f;
					//fica esperando receber struct
					bzero(buffer,BUFFER_SIZE);
					read(socketfd, buffer, BUFFER_SIZE);
					memcpy(&f, buffer, sizeof(struct file_info));
				
					// receive file funciona com full path
					char *fullpath;
					strcat(fullpath, home);
					strcat(fullpath, "/sync_dir_");
					strcat(fullpath, cli->userid);
					strcat(fullpath, "/");
					strcat(fullpath, f.name);
					strcat(fullpath, ".");
					strcat(fullpath, f.extension);
		
					printf("receiving path: %s\n", fullpath);
			
					//recebe arquivo
					receive_file(fullpath, socketfd);

					//bota f na estrutura self
					insert_file_into_client_list(cli, f);
				}
			}
        }
    }

	//avança o estado de commit do cliente no servidor.
	cli->current_commit += 1;

	// avisa que acabou o seu sync.
	bzero(buffer, BUFFER_SIZE);
	buffer[0] = SYNC_END;
	write(socketfd, buffer, BUFFER_SIZE);
}


int main(int argc, char *argv[])
{
	strcpy(home,"/home/");
	strcat(home, getlogin());
	strcat(home, "/server");

	struct stat st;
	if (stat(home, &st) != 0) {
		  mkdir(home, 0777);
	}
		
	CreateFila2(&connected_clients);

	int socket_connection, socket_client;
	socklen_t client_len;
	struct sockaddr_in serv_addr, client_addr;


    //inicializa mutex da fila de clientes
    if (pthread_mutex_init(&queue, NULL) != 0)
    {
        printf("\nMutex (queue) init failed\n");
        return;
    }

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

			connection_info *ci = malloc(sizeof(connection_info));
			ci->socket_client = socket_client;
			ci->port = PORT;
		
			pthread_create(&client_thread, NULL, run_client, (void*)ci);
		
			pthread_detach(client_thread);

			//free(new_sock);
		}
		
	}

	printf("saí do meu while (server)\n");
	
	return 0; 
}
