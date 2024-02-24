#include <mpi.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include "hashtable.h"

#define TRACKER_RANK 0
#define MAX_FILES 10
#define MAX_FILENAME 15
#define HASH_SIZE 32
#define MAX_CHUNKS 100
#define MAX_TASKS 16

#define INITIAL_MESSAGE 1
#define START_DOWNLOAD 2
#define DOWNLOAD_FILE 3 
#define REQUEST_DOWNLOAD 4
#define ACCEPTED_DOWNLOAD 5
#define UPDATE_TRACKER 6
#define UPDATE_CLIENT 7
#define FINISH_FILE 8
#define FINISH_ALL_FILLES 9
#define ALL_CLIENTS_FINISHED 10

typedef struct {
    char file_name[MAX_FILENAME];
    int num_chunks;
    char chunks[MAX_CHUNKS][HASH_SIZE + 1];
}BitTorrentFile;

typedef struct {
    int rank;
    int num_tasks;
    int num_files_owned;
    BitTorrentFile files[MAX_FILES];
    int num_files_wanted;
    char files_wanted[MAX_FILES][MAX_FILENAME];
}ClientInfo;

typedef struct {
    int position;
    char hash[HASH_SIZE + 1];
}chunk_info;

typedef struct {
    uint8_t hasChunk;
}client_chunk;

typedef struct {
    int rank;
    //int num_conection;
    client_chunk chunks[MAX_CHUNKS];
}tracker_client;

typedef struct {
    // numarul clientilor care detin un fisier
    int num_clients;
    tracker_client clients[MAX_TASKS];
    int num_chunks;
    Hashtable chunks;
}tracker_file;

typedef struct {
    int rank;
    //int num_conection;
}seed_info;

typedef struct {
    char hash[HASH_SIZE + 1];
    uint8_t gained;
    int num_seeds;
    seed_info seeds[MAX_TASKS];
}download_chunk_info;

typedef struct {
    int num_chunks;
    download_chunk_info chunks[MAX_CHUNKS];
}download_file_info;

// Citeste fisierul de intrare
void read_input_file(ClientInfo *infos)
{
    FILE *file;

    char file_name[MAX_FILENAME];
    sprintf(file_name, "in%d.txt", infos->rank);

    file = fopen(file_name, "rt");   

    if(file == NULL) {
        printf("Eroare la deschiderea fisierului.\n");
        exit(-1);
    }

    fscanf(file, "%d", &infos->num_files_owned);

    for(int i = 0; i < infos->num_files_owned; i++) {
        fscanf(file, "%s", infos->files[i].file_name);
        fscanf(file, "%d", &infos->files[i].num_chunks);
        for(int j = 0; j < infos->files[i].num_chunks; j++) {
            fscanf(file, "%s", infos->files[i].chunks[j]);
            infos->files[i].chunks[j][HASH_SIZE] = '\0';
        }
    }

    fscanf(file, "%d", &infos->num_files_wanted);
    
    for(int i = 0; i < infos->num_files_wanted; i++) {
        fscanf(file, "%s", infos->files_wanted[i]);
        infos->files_wanted[i][strlen(infos->files_wanted[i])] = '\0';
    }
    fclose(file);
}

unsigned int generareSeed(int rank) {
    return (unsigned int)(time(NULL) + rank);
}

//creeaza o ordine random de descarcare a chunk-urilor
void generate_vector(int *vector, int num_chunks, int rank) {
    srand(generareSeed(rank));

    for (int i = 0; i < num_chunks; ++i) {
        vector[i] = i;
    }

    for (int i = num_chunks - 1; i > 0; --i) {
        int j = rand() % (i + 1);
        int temp = vector[i];
        vector[i] = vector[j];
        vector[j] = temp;
    }
}

//face o cerere pentru a descarca un fisier
void request_download(download_file_info *file_to_download, int position, int *frecv_tasks, int rank) {
    int num_seeds = file_to_download->chunks[position].num_seeds;
    int min = frecv_tasks[file_to_download->chunks[position].seeds[0].rank];
    int least_used = file_to_download->chunks[position].seeds[0].rank;
    //se incearca sa se descarce fisierul de la seed-ul cel mai putin folosit
    // astfel rezultand intr-o decarcare uniforma
    for (int i = 1; i < num_seeds; i++) {
        if (frecv_tasks[file_to_download->chunks[position].seeds[i].rank] < min) {
            min = frecv_tasks[file_to_download->chunks[position].seeds[i].rank];
            least_used = file_to_download->chunks[position].seeds[i].rank;
        }
    }
    MPI_Status status;
    char message[10];
    MPI_Send(file_to_download->chunks[position].hash, HASH_SIZE + 1,
                                 MPI_CHAR, least_used, REQUEST_DOWNLOAD, MPI_COMM_WORLD);
    MPI_Recv(message, 10, MPI_CHAR, least_used, ACCEPTED_DOWNLOAD, MPI_COMM_WORLD, &status);
    if (strcmp(message, "OK") == 0) {
        file_to_download->chunks[position].gained = 1;
        frecv_tasks[least_used]++;
    }                             
}

void *download_thread_func(void *arg)
{
    ClientInfo infos = *(ClientInfo*) arg;
    char message[10];

    read_input_file(&infos);
    strcpy(message, "initial");
    MPI_Send(message, 10, MPI_CHAR, TRACKER_RANK, INITIAL_MESSAGE, MPI_COMM_WORLD);
    MPI_Send(&infos.num_files_owned, 1, MPI_INT, TRACKER_RANK, INITIAL_MESSAGE, MPI_COMM_WORLD);
    for(int i = 0; i < infos.num_files_owned; i++) {
        MPI_Send(&infos.files[i].file_name, MAX_FILENAME, MPI_CHAR, TRACKER_RANK, INITIAL_MESSAGE, MPI_COMM_WORLD);
        MPI_Send(&infos.files[i].num_chunks, 1, MPI_INT, TRACKER_RANK, INITIAL_MESSAGE, MPI_COMM_WORLD);

        for(int j = 0; j < infos.files[i].num_chunks; j++) {
            MPI_Send(&infos.files[i].chunks[j], HASH_SIZE + 1, MPI_CHAR, TRACKER_RANK, INITIAL_MESSAGE, MPI_COMM_WORLD);
        }
    }

    //asteapta mesajul de incepere
    MPI_Status status;
    MPI_Recv(message, 10, MPI_CHAR, 0, START_DOWNLOAD, MPI_COMM_WORLD, &status);
    for (int i = 0; i < infos.num_files_wanted; i++) {
        strcpy(message, infos.files_wanted[i]);
        MPI_Send(message, 10, MPI_CHAR, TRACKER_RANK, DOWNLOAD_FILE, MPI_COMM_WORLD);
        int num_chunks;
        MPI_Recv(&num_chunks, 1, MPI_INT, TRACKER_RANK, DOWNLOAD_FILE, MPI_COMM_WORLD, &status);
        //cate chunk-uri s-au descarcat
        int download_chunks = 0;
        int receive_chunks = 0;

        download_file_info file_to_download;
        file_to_download.num_chunks = num_chunks;
        int frecv_tasks[MAX_TASKS];
        for (int j = 0; j < MAX_TASKS; j++) {
            frecv_tasks[j] = 0;
        }
        while (receive_chunks < num_chunks) {
            int position;
            MPI_Recv(&position, 1, MPI_INT, TRACKER_RANK, DOWNLOAD_FILE, MPI_COMM_WORLD, &status);
            MPI_Recv(&file_to_download.chunks[position].hash, HASH_SIZE + 1, MPI_CHAR, TRACKER_RANK, DOWNLOAD_FILE, MPI_COMM_WORLD, &status);
            file_to_download.chunks[position].gained = 0;
            MPI_Recv(&file_to_download.chunks[position].num_seeds, 1, MPI_INT, TRACKER_RANK, DOWNLOAD_FILE, MPI_COMM_WORLD, &status);
            for (int j = 0; j < file_to_download.chunks[position].num_seeds; j++) {
                MPI_Recv(&file_to_download.chunks[position].seeds[j].rank, 1, MPI_INT, TRACKER_RANK, DOWNLOAD_FILE, MPI_COMM_WORLD, &status);
            }
            receive_chunks++;
        }

        int random_order[MAX_CHUNKS];
        generate_vector(random_order, num_chunks, infos.rank);
        while(download_chunks < num_chunks) {
            if(file_to_download.chunks[random_order[download_chunks]].gained == 0) {
                request_download(&file_to_download, random_order[download_chunks], frecv_tasks, infos.rank);
                download_chunks++;
            }
            if(download_chunks % 10 == 0) {
                // trimite update catre tracker
                strcpy(message, infos.files_wanted[i]);
                MPI_Send(message, 10, MPI_CHAR, TRACKER_RANK, UPDATE_TRACKER, MPI_COMM_WORLD);
                MPI_Send(&download_chunks, 1, MPI_INT, TRACKER_RANK, UPDATE_TRACKER, MPI_COMM_WORLD);
                for (int j = 0; j < num_chunks; j++) {
                    if (file_to_download.chunks[j].gained == 1) {
                        MPI_Send(file_to_download.chunks[j].hash, HASH_SIZE + 1, 
                                MPI_CHAR, TRACKER_RANK, UPDATE_TRACKER, MPI_COMM_WORLD);
                    }
                }
                for (int j = 0; j < num_chunks; j++) {
                    int position;
                    MPI_Recv(&position, 1, MPI_INT, TRACKER_RANK, UPDATE_CLIENT, MPI_COMM_WORLD, &status);
                    MPI_Recv(&file_to_download.chunks[position].num_seeds, 1, MPI_INT, TRACKER_RANK,
                                                                    UPDATE_CLIENT, MPI_COMM_WORLD, &status);
                    for (int j = 0; j < file_to_download.chunks[position].num_seeds; j++) {
                        MPI_Recv(&file_to_download.chunks[position].seeds[j].rank, 1,
                                        MPI_INT, TRACKER_RANK, UPDATE_CLIENT, MPI_COMM_WORLD, &status);
                    }
                }
            }
        }
        char file_name[MAX_FILENAME];
        sprintf(file_name, "client%d_%s", infos.rank, infos.files_wanted[i]);
        FILE *file;
        file = fopen(file_name, "wt");
        for(int j = 0; j < num_chunks; j++) {
            if (file_to_download.chunks[j].gained == 1) {
                if (j != num_chunks - 1)
                    fprintf(file, "%s\n", file_to_download.chunks[j].hash);
                else
                    fprintf(file, "%s", file_to_download.chunks[j].hash);
            }  
        }
        fclose(file);
        strcpy(message, infos.files_wanted[i]);
        MPI_Send(message, 10, MPI_CHAR, TRACKER_RANK, FINISH_FILE, MPI_COMM_WORLD); 
    }

    strcpy(message, "finish");
    MPI_Send(message, 10, MPI_CHAR, TRACKER_RANK, FINISH_ALL_FILLES, MPI_COMM_WORLD);
    return NULL;
}

void *upload_thread_func(void *arg)
{
    ClientInfo infos = *(ClientInfo*) arg;
    int flag;
    MPI_Status status;
    MPI_Request request;
    int flag_download;
    MPI_Status status_download;
    MPI_Request request_download;
    char message[10];
    char hash[HASH_SIZE + 1];
    // asteapta mesajul de inchidere cand toti clientii au terminat de descarcat
    MPI_Irecv(message, 10, MPI_CHAR, TRACKER_RANK, ALL_CLIENTS_FINISHED, MPI_COMM_WORLD, &request);
    while(!flag) {
        // asteapta cereri de download
        MPI_Irecv(hash, HASH_SIZE + 1, MPI_CHAR, MPI_ANY_SOURCE, REQUEST_DOWNLOAD, MPI_COMM_WORLD, &request_download);
        MPI_Test(&request, &flag, &status);
        MPI_Test(&request_download, &flag_download, &status_download);
        //astepta pana cand primeste ori o cerere de download fie de inchidere
        while(flag_download == 0 && flag == 0) {
            MPI_Test(&request, &flag, &status);
            MPI_Test(&request_download, &flag_download, &status_download);
        }
        if(flag) {
            break;
        }
        if(flag_download) {
            strcpy(message, "OK");
            MPI_Send(message, 10, MPI_CHAR,
                            status_download.MPI_SOURCE, ACCEPTED_DOWNLOAD, MPI_COMM_WORLD);
        }
    }
    return NULL;
}

//procesarea mesajului initial dat de client catre tracker
void receive_initial_message(Hashtable *files, MPI_Status status_recv, int numtasks) {
    MPI_Status status;
    int num_files;
    MPI_Recv(&num_files, 1, MPI_INT, status_recv.MPI_SOURCE, INITIAL_MESSAGE, MPI_COMM_WORLD, &status);
    for(int i = 0; i < num_files; i++) {
        char file_name[MAX_FILENAME];
        // informarea trackerului despre ce fisier este vorba
        MPI_Recv(file_name, MAX_FILENAME, MPI_CHAR, status_recv.MPI_SOURCE, INITIAL_MESSAGE, MPI_COMM_WORLD, &status);
        // verificam daca este prima oara cand se primesc informatii despre acel fisier sau daca doar trebuie sa adaugam 
        // clientul la swarn-ul fisierului  
        if(!hasKey(files, file_name)) {
            tracker_file tracker_file;
            MPI_Recv(&tracker_file.num_chunks, 1, MPI_INT, status_recv.MPI_SOURCE, INITIAL_MESSAGE, MPI_COMM_WORLD, &status);
            tracker_file.num_clients = 1;
            tracker_file.clients[0].rank = status_recv.MPI_SOURCE;
            initializeHashtable(&tracker_file.chunks);
            for(int i = 0; i < tracker_file.num_chunks; i++) {
                tracker_file.clients[0].chunks[i].hasChunk = 1;
                chunk_info chunk;
                chunk.position = i;
                MPI_Recv(&chunk.hash, HASH_SIZE + 1,
                                            MPI_CHAR, status_recv.MPI_SOURCE, INITIAL_MESSAGE, MPI_COMM_WORLD, &status);
                insert(&tracker_file.chunks, chunk.hash, &chunk, strlen(chunk.hash) + 1, sizeof(chunk_info));                                                       
            }
            insert(files, file_name, &tracker_file, strlen(file_name) + 1, sizeof(tracker_file));
        } else {
            tracker_file *file = (tracker_file *)get(files, file_name);
            file->clients[file->num_clients].rank = status_recv.MPI_SOURCE;
            int num_chunks;
            MPI_Recv(&num_chunks, 1, MPI_INT, status_recv.MPI_SOURCE, INITIAL_MESSAGE, MPI_COMM_WORLD, &status);
            for (int i = 0; i < num_chunks; i++) {
                chunk_info chunk;
                MPI_Recv(&chunk.hash, HASH_SIZE + 1,
                                            MPI_CHAR, status_recv.MPI_SOURCE, INITIAL_MESSAGE, MPI_COMM_WORLD, &status);
                chunk = *(chunk_info *)get(&file->chunks, chunk.hash);
                int position = chunk.position;
                file->clients[file->num_clients].chunks[position].hasChunk = 1;
            }
            file->num_clients++;
        }
    }
}

// trimite informatii legate de chunk-uri, cati le detin, cine le detine
void send_file_info(tracker_file file, MPI_Status status) {
    for (int i = 0; i < TABLE_SIZE; i++) {
        if(file.chunks.table[i] != NULL) {
            Entry* current = file.chunks.table[i];
            while(current != NULL) {
                chunk_info info = *(chunk_info *) current->value;
                MPI_Send(&info.position, 1, MPI_INT, status.MPI_SOURCE, DOWNLOAD_FILE, MPI_COMM_WORLD);
                MPI_Send(&info.hash, HASH_SIZE + 1, MPI_CHAR, status.MPI_SOURCE, DOWNLOAD_FILE, MPI_COMM_WORLD);
                int nr_seeds = 0;
                for(int j = 0; j < file.num_clients; j++) {
                    if (file.clients[j].chunks[info.position].hasChunk) {
                        nr_seeds++;
                    }
                }
                MPI_Send(&nr_seeds, 1, MPI_INT, status.MPI_SOURCE, DOWNLOAD_FILE, MPI_COMM_WORLD);
                for(int j = 0; j < file.num_clients; j++) {
                    if (file.clients[j].chunks[info.position].hasChunk) {
                        MPI_Send(&file.clients[j].rank, 1, MPI_INT, status.MPI_SOURCE, DOWNLOAD_FILE, MPI_COMM_WORLD);
                    }
                }
                current = current->next;
            }
        }
    }
}

//primul mesaj in care clientul ii spune trackerului ce fisier doreste sa descarce
void receive_download_file_message(MPI_Status status, char *message, Hashtable files) {
    tracker_file file = *(tracker_file *)get(&files, message);
    MPI_Send(&file.num_chunks, 1, MPI_INT, status.MPI_SOURCE, DOWNLOAD_FILE, MPI_COMM_WORLD);
    send_file_info(file, status);
}

void update_tracker(MPI_Status status, char *message, Hashtable *files) {
    tracker_file *file = (tracker_file *)get(files, message);
    uint8_t sw = 0;
    uint8_t pos_client;
    // adaugam fisierul la swarn-ul de fisiere daca nu exista deja
    for (int i = 0; i < file->num_clients; i++) {
        if (file->clients[i].rank == status.MPI_SOURCE) {
            sw = 1;
            pos_client = i;
        }
    }
    if (!sw) {
        file->clients[file->num_clients].rank = status.MPI_SOURCE;
        pos_client = file->num_clients;
        file->num_clients++;
        for (int i = 0; i < file->num_chunks; i++) {
            file->clients[pos_client].chunks[i].hasChunk = 0;
        }
    }
    int num_downloads;
    MPI_Recv(&num_downloads, 1, MPI_INT, status.MPI_SOURCE, UPDATE_TRACKER,
                                                            MPI_COMM_WORLD, &status);
    //informam traackerul de cele chunk-uri are la momentul actual
    for (int i = 0; i < num_downloads; i++) {                                                        
        chunk_info chunk;
        MPI_Recv(&chunk.hash, HASH_SIZE + 1,
                    MPI_CHAR, status.MPI_SOURCE, UPDATE_TRACKER, MPI_COMM_WORLD, &status);          
        chunk = *(chunk_info *)get(&file->chunks, chunk.hash);
        int position = chunk.position;
        file->clients[pos_client].chunks[position].hasChunk = 1;
    }
    //informam clientul despre starea fisierului la momentul actual
    for (int i = 0; i < TABLE_SIZE; i++) {
        if(file->chunks.table[i] != NULL) {
            Entry* current = file->chunks.table[i];
            while(current != NULL) {
                chunk_info info = *(chunk_info *) current->value;
                MPI_Send(&info.position, 1, MPI_INT, status.MPI_SOURCE, UPDATE_CLIENT,
                                                            MPI_COMM_WORLD);                                            
                int nr_seeds = 0;
                for(int j = 0; j < file->num_clients; j++) {
                    if (file->clients[j].chunks[info.position].hasChunk) {
                        nr_seeds++;
                    }
                }
                MPI_Send(&nr_seeds, 1, MPI_INT, status.MPI_SOURCE, UPDATE_CLIENT,
                                                            MPI_COMM_WORLD);
                for(int j = 0; j < file->num_clients; j++) {
                    if (file->clients[j].chunks[info.position].hasChunk) {
                        MPI_Send(&file->clients[j].rank, 1, MPI_INT, 
                                        status.MPI_SOURCE, UPDATE_CLIENT, MPI_COMM_WORLD);
                    }
                }
                current = current->next;
            }
        }
    }
}

void finish_file(MPI_Status status, char *message, Hashtable *files) {
    tracker_file *file = (tracker_file *)get(files, message);
    uint8_t pos_client = -1;
    for (int i = 0; i < file->num_clients; i++) {
        if (file->clients[i].rank == status.MPI_SOURCE) {
            pos_client = i;
            break;
        }
    }
    // clientul va detine clar toate chunk-urile fisierului
    for (int i = 0; i < file->num_chunks; i++) {
        file->clients[pos_client].chunks[i].hasChunk = 1;
    }
}

// procesarea mesajului primit
void process_message(MPI_Status status, char *message, Hashtable *files, int *num_clients) {
    switch (status.MPI_TAG) {
    case DOWNLOAD_FILE:
        receive_download_file_message(status, message, *files);
        break;
    case UPDATE_TRACKER:
        update_tracker(status, message, files);
        break;
    case FINISH_FILE:
        finish_file(status, message, files);
        break;
    case FINISH_ALL_FILLES:
        (*num_clients)++;
        break;            
    default:
        break;
    }
}

void tracker(int numtasks, int rank) {
    Hashtable files;
    MPI_Status status;
    initializeHashtable(&files);

    char message[10];
    int num_clients = 0;
    // asteptam primirea mesajul initial de la toti clientii
    while(num_clients != numtasks - 1) {
        MPI_Recv(message, 10, MPI_CHAR, MPI_ANY_SOURCE, INITIAL_MESSAGE, MPI_COMM_WORLD, &status);
        if(strcmp(message, "initial") == 0) {
            receive_initial_message(&files, status, numtasks);
            num_clients++;
        }
    }
    // informam toti clienti ca pot incepe descarcarea
    strcpy(message, "ACK");
    for (int i = 1; i < numtasks; i++) {
        MPI_Send(message, 10, MPI_CHAR, i, START_DOWNLOAD, MPI_COMM_WORLD);
    }
    num_clients = 0;
    // primim mesaje pana cand toti clienti termina de descarcat toate fisierele
    while(num_clients != numtasks -1) {
        MPI_Recv(message, 10, MPI_CHAR, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        process_message(status, message, &files, &num_clients);
    }
    // anuntam toti clienti de finalizarea descarcari tuturor fisierelor
    strcpy(message, "finish");
    for (int i = 1; i < numtasks; i++) {
        MPI_Send(message, 10, MPI_CHAR, i, ALL_CLIENTS_FINISHED, MPI_COMM_WORLD);
    }
}

void peer(int numtasks, int rank) {
    pthread_t download_thread;
    pthread_t upload_thread;
    void *status;
    int r;

    ClientInfo infos;
    infos.rank = rank;
    infos.num_tasks = numtasks;
    r = pthread_create(&download_thread, NULL, download_thread_func, (void *) &infos);
    if (r) {
        printf("Eroare la crearea thread-ului de download\n");
        exit(-1);
    }

    r = pthread_create(&upload_thread, NULL, upload_thread_func, (void *) &infos);
    if (r) {
        printf("Eroare la crearea thread-ului de upload\n");
        exit(-1);
    }

    r = pthread_join(download_thread, &status);
    if (r) {
        printf("Eroare la asteptarea thread-ului de download\n");
        exit(-1);
    }

    r = pthread_join(upload_thread, &status);
    if (r) {
        printf("Eroare la asteptarea thread-ului de upload\n");
        exit(-1);
    }
}
 
int main (int argc, char *argv[]) {
    int numtasks, rank;
 
    int provided;
    MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
    if (provided < MPI_THREAD_MULTIPLE) {
        fprintf(stderr, "MPI nu are suport pentru multi-threading\n");
        exit(-1);
    }
    MPI_Comm_size(MPI_COMM_WORLD, &numtasks);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    if (rank == TRACKER_RANK) {
        tracker(numtasks, rank);
    } else {
        peer(numtasks, rank);
    }

    MPI_Finalize();
}
