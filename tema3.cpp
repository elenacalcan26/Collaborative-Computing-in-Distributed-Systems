#include "mpi.h"
#include <iostream>
#include <stdlib.h>
#include <vector>
#include <string>
#include <fstream>

#define COORD_0 0
#define COORD_1 1
#define COORD_2 2
#define NUM_CLUSTERS 3

#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))

using namespace std;

// verifica daca procesul curent este coordonator sau worker
bool isCoordinatorProc(int rank)
{
    return (rank == COORD_0) || (rank == COORD_1) || (rank == COORD_2);
}

// procesul coordonator citeste din fisierul corespunzator procesele worker din cluster-ul sau
void get_workers_from_file(vector<int> *topology, int coord)
{
    string file;
    file = "cluster" + to_string(coord) + ".txt";
    int worker, num_of_workers;

    ifstream input_file(file);

    if (!input_file.is_open())
    {
        cout << "Could not oopen file " << file << endl;
        exit(-1);
    }

    input_file >> num_of_workers;

    for (int i = 0; i < num_of_workers; i++)
    {
        input_file >> worker;

        // se salveaza in lisra de adiacenta legatura dintre coordonator si worker
        topology[coord].push_back(worker);
        topology[worker].push_back(coord);
    }

    input_file.close();
}

// se face logarea procelelor worker din fiecare cluster
void worker_log(vector<int> *topology, int rank)
{

    // se verifca coordonatorii
    if (isCoordinatorProc(rank))
    {

        // se face send catre workeri si se primeste raspunsul lor
        for (auto worker : topology[rank])
        {
            printf("M (%d, %d)\n", rank, worker);

            MPI_Send(&rank, 1, MPI_INT, worker, 0, MPI_COMM_WORLD);

            MPI_Status status;
            int val;
            MPI_Recv(&val, 1, MPI_INT, worker, 0, MPI_COMM_WORLD, &status);
        }
    }
    else
    {

        // procesul worker primeste id-ul coordonatorului sau si ii raspunde
        MPI_Status status;
        int boss;
        MPI_Recv(&boss, 1, MPI_INT, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &status);

        topology[rank].push_back(boss);

        printf("M (%d, %d)\n", rank, boss);

        MPI_Send(&rank, 1, MPI_INT, boss, 0, MPI_COMM_WORLD);
    }
}

// un porces coordonator ii trimite altui proces coordonator procesle din cluster-ul sau
void complete_topology(vector<int> *topology, int coord1, int coord2, int rank)
{
    if (rank == coord1)
    {

        int size = topology[rank].size();
        int *workers = &topology[rank][0];

        // se trimit procesele worker
        MPI_Send(&size, 1, MPI_INT, coord2, 1, MPI_COMM_WORLD);
        MPI_Send(workers, size, MPI_INT, coord2, 1, MPI_COMM_WORLD);

        printf(" M (%d, %d)\n", rank, coord2);
    }
    else if (rank == coord2)
    {

        MPI_Status status;
        int recv_size;

        printf("M (%d, %d)\n", rank, coord1);

        // se primesc procesele worker ale primului coordonator si le salveaza in lista sa
        MPI_Recv(&recv_size, 1, MPI_INT, coord1, 1, MPI_COMM_WORLD, &status);

        int recv_workers[recv_size];
        MPI_Recv(recv_workers, recv_size, MPI_INT, coord1, 1, MPI_COMM_WORLD, &status);

        for (int i = 0; i < recv_size; i++)
        {
            topology[coord1].push_back(recv_workers[i]);
            topology[recv_workers[i]].push_back(coord1);
        }
    }
}

// coordonatorul trimite catre workerii sai informatiile despre celelalte clustere pe care le cunoaste
void send_topology_to_workers(vector<int> *topology, int rank)
{

    if (isCoordinatorProc(rank))
    {
        for (auto worker : topology[rank])
        {

            for (int i = 0; i < NUM_CLUSTERS; i++)
            {
                int send_coord = i;
                int size = topology[i].size();
                int *workers = &topology[send_coord][0];

                // se trimit catre worker coordonaorul cluster-lui, dimensiunea clusetr-ului si lista cu workeri
                MPI_Send(&send_coord, 1, MPI_INT, worker, 2, MPI_COMM_WORLD);
                MPI_Send(&size, 1, MPI_INT, worker, 2, MPI_COMM_WORLD);
                MPI_Send(workers, size, MPI_INT, worker, 2, MPI_COMM_WORLD);

                printf("M (%d, %d)\n", rank, worker);
            }
        }
    }
    else
    {

        for (int i = 0; i < NUM_CLUSTERS; i++)
        {

            MPI_Status status;
            int recv_coord, recv_size;

            // worker-ul primeste informatiile despre celelalte cclustere
            MPI_Recv(&recv_coord, 1, MPI_INT, topology[rank][0], 2, MPI_COMM_WORLD, &status);
            MPI_Recv(&recv_size, 1, MPI_INT, topology[rank][0], 2, MPI_COMM_WORLD, &status);

            int recv_workers[recv_size];

            MPI_Recv(recv_workers, recv_size, MPI_INT, topology[rank][0], 2, MPI_COMM_WORLD, &status);

            // salveaza informatille in lista sa
            for (int i = 0; i < recv_size; i++)
            {
                topology[recv_coord].push_back(recv_workers[i]);
                topology[recv_workers[i]].push_back(recv_coord);
            }
        }
    }
}

// fiecare proces afiseaza topologia aflata
void print_topology(vector<int> *topology, int rank)
{
    printf("%d -> ", rank);

    for (int j = 0; j < NUM_CLUSTERS; j++)
    {
        printf("%d:", j);

        int size = topology[j].size();
        for (int i = 0; i < size - 1; i++)
        {
            printf("%d,", topology[j][i]);
        }
        printf("%d ", topology[j][size - 1]);
    }

    printf("\n");
}

// calculeaza numarul de iteratii pe care il efectueaza workerii din fiecare cluster
void compute_end_indexes(int n, int *end_indexes, vector<int> *topology)
{
    int total_workers = 0;
    int requierd_iterations, rest;
    for (int i = 0; i < NUM_CLUSTERS; i++)
    {
        total_workers += topology[i].size();
    }

    // se calculeaza numarul de iteratii obligatorii pentru fiecare worker
    requierd_iterations = n / total_workers;
    rest = n % total_workers;

    if (rest != 0 && rest / NUM_CLUSTERS != 0)
    {
        requierd_iterations += (rest / NUM_CLUSTERS);
        rest = rest % NUM_CLUSTERS;
    }

    for (int i = 0; i < NUM_CLUSTERS; i++)
    {
        // se calculeaza numarul de iteratii per cluster
        end_indexes[i] = requierd_iterations * topology[i].size();

        if (rest > 0)
        {
            end_indexes[i]++;
            rest--;
        }
    }

    // se calculeaza indecsii de final din vector corespunzatori fiecarui cluster
    for (int i = 1; i < NUM_CLUSTERS; i++)
    {
        end_indexes[i] = MIN(end_indexes[i] + end_indexes[i - 1], n);
    }
}

// imparte array-ul dupa numarul de workeri din fiecare cluster
void divide_array(vector<int> *topology, int rank, int n, int *v, int *end_indexes, vector<int> &cluster_array)
{
    if (rank == COORD_0)
    {

        compute_end_indexes(n, end_indexes, topology);

        // se salveaza portiune din vector corespunzator cluster-ului 0
        for (int i = 0; i < end_indexes[0]; i++)
        {
            cluster_array.push_back(v[i]);
        }

        // pentru fiecare cluster se trimite bucata din vector prin care pot itera workerii respectivi
        for (int coord_id = 1; coord_id < NUM_CLUSTERS; coord_id++)
        {
            int start = end_indexes[coord_id - 1];

            int arr[end_indexes[coord_id] - start] = {0};

            for (int k = start; k < end_indexes[coord_id]; k++)
            {
                arr[k - start] = v[k];
            }

            MPI_Send(end_indexes, NUM_CLUSTERS, MPI_INT, coord_id, 3, MPI_COMM_WORLD);
            MPI_Send(arr, (end_indexes[coord_id] - start), MPI_INT, coord_id, 3, MPI_COMM_WORLD);

            printf("M (%d, %d)\n", rank, coord_id);
        }
    }
    else
    {
        // coordonatorii celorlalte clustere primesc de la procesul 0 bucata din vector 
        // pe care workerii lor trebuie sa o prelucreze
        MPI_Status status;
        int recv_arr[NUM_CLUSTERS];

        MPI_Recv(&recv_arr, NUM_CLUSTERS, MPI_INT, COORD_0, 3, MPI_COMM_WORLD, &status);

        for (int i = 0; i < NUM_CLUSTERS; i++)
        {
            end_indexes[i] = recv_arr[i];
        }

        int size = end_indexes[rank] - end_indexes[rank - 1];
        int recv_divided_arr[size];

        MPI_Recv(recv_divided_arr, size, MPI_INT, COORD_0, 3, MPI_COMM_WORLD, &status);

        for (int i = 0; i < size; i++)
        {
            cluster_array.push_back(recv_divided_arr[i]);
        }
    }
}

// coordonatorii trimit proceselor worker bucatile din vector ce trebuie prelucrate
void multiply_array(int rank, vector<int> *topology, vector<int> &cluster_array)
{
    if (isCoordinatorProc(rank))
    {
        // procesul coordonator imparte vectorul asignat cluster-ului proceselor worker
        int size = (int)cluster_array.size();
        int num_workers = (int)topology[rank].size();

        for (int worker_id = 0; worker_id < num_workers; worker_id++)
        {
            // se calculeaza indicii de start si de end din vector, in care worker-ul realizeaza calculele
            int start = worker_id * ((double)size / num_workers);
            int end = MIN(size, (worker_id + 1) * ((double)size / num_workers));
            int task_size = end - start;

            // se trimite portiunea din vector pe care o prelucreaza worker-ul
            int task[task_size];

            for (int i = start; i < end; i++)
            {
                task[i - start] = cluster_array[i];
            }

            MPI_Send(&task_size, 1, MPI_INT, topology[rank][worker_id], 0, MPI_COMM_WORLD);
            MPI_Send(task, task_size, MPI_INT, topology[rank][worker_id], 0, MPI_COMM_WORLD);

            printf("M (%d, %d)\n", rank, topology[rank][worker_id]);

            // coordonatorul primeste rezultatele de la worker si actualizeaza vectorul
            MPI_Status status;
            int recv_size;

            MPI_Recv(&recv_size, 1, MPI_INT, topology[rank][worker_id], 0, MPI_COMM_WORLD, &status);

            int recv_task[recv_size];

            MPI_Recv(recv_task, recv_size, MPI_INT, topology[rank][worker_id], 0, MPI_COMM_WORLD, &status);

            for (int i = start; i < end; i++)
            {
                cluster_array[i] = recv_task[i - start];
            }
        }
    }
    else
    {

        // worker-ul primeste partea sa din vector pe care o prelucreaza
        MPI_Status status;
        int recv_size;

        MPI_Recv(&recv_size, 1, MPI_INT, topology[rank][0], 0, MPI_COMM_WORLD, &status);

        int recv_task[recv_size];

        MPI_Recv(recv_task, recv_size, MPI_INT, topology[rank][0], 0, MPI_COMM_WORLD, &status);

        int task[recv_size];

        for (int i = 0; i < recv_size; i++)
        {
            task[i] = recv_task[i] * 2;
        }
        
        // trimite coordonatorului rezultatele obtinute
        MPI_Send(&recv_size, 1, MPI_INT, topology[rank][0], 0, MPI_COMM_WORLD);
        MPI_Send(task, recv_size, MPI_INT, topology[rank][0], 0, MPI_COMM_WORLD);

        printf("M (%d, %d)\n", rank, topology[rank][0]);
    }
}

// se asambleaza rezultatele obtinute de procele worker
void merge_array(int rank, vector<int> cluster_array, int *end_indexes, int *v)
{
    if (rank == COORD_0)
    {
        // se actualizeaza rezultatele obtinute de workerii din cluster-ul coordonatorului 0
        for (int i = 0; i < end_indexes[rank]; i++)
        {
            v[i] = cluster_array[i];
        }
        cout << endl;
    }

    if (rank != COORD_0)
    {

        // se trimit rezultatele obtinute de celelalte clustere coordonatorului 0
        int size = end_indexes[rank] - end_indexes[rank - 1];

        int arr[size];

        for (int i = 0; i < size; i++)
        {
            arr[i] = cluster_array[i];
        }

        MPI_Send(&size, 1, MPI_INT, COORD_0, 5, MPI_COMM_WORLD);
        MPI_Send(arr, size, MPI_INT, COORD_0, 5, MPI_COMM_WORLD);

        printf("M (%d, %d)\n", rank, COORD_0);
    }
    else
    {
        // coordonatorul 0 primeste rezultatele de la celelalte clustere si le asambleaza
        for (int i = 1; i < NUM_CLUSTERS; i++)
        {
            MPI_Status status;
            int recv_size;

            MPI_Recv(&recv_size, 1, MPI_INT, i, 5, MPI_COMM_WORLD, &status);

            int recv_arr[recv_size];

            MPI_Recv(recv_arr, recv_size, MPI_INT, i, 5, MPI_COMM_WORLD, &status);

            int start = end_indexes[i - 1];

            for (int j = start; j < end_indexes[i]; j++)
            {
                v[j] = recv_arr[j - start];
            }
        }
    }
}

int main(int argc, char *argv[])
{
    int numtasks, rank;

    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &numtasks);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    int n;
    int *v;
    int num_of_workers;
    vector<int> topology[numtasks]; // coordonatorii nu sunt adiacenti intre ei aici
    int end_indexes[NUM_CLUSTERS];  // retine indecsii de end din array pentru fiecare cluster
    vector<int> cluster_array;      // retine partea de array pentru fiecare cluster

    if (rank == COORD_0)
    {
        // generare vector de catre procesul 0
        n = atoi(argv[1]);
        v = (int *)malloc(n * sizeof(int));

        if (v == NULL)
        {
            printf("Malloc failed\n");
            exit(-1);
        }

        for (int i = 0; i < n; i++)
        {
            v[i] = i;
        }
    }

    if (isCoordinatorProc(rank))
    {
        get_workers_from_file(topology, rank);
    }

    worker_log(topology, rank);
    complete_topology(topology, COORD_0, COORD_1, rank);
    complete_topology(topology, COORD_1, COORD_0, rank);
    complete_topology(topology, COORD_0, COORD_2, rank);
    complete_topology(topology, COORD_2, COORD_0, rank);
    complete_topology(topology, COORD_1, COORD_2, rank);
    complete_topology(topology, COORD_2, COORD_1, rank);

    send_topology_to_workers(topology, rank);

    print_topology(topology, rank);

    if (isCoordinatorProc(rank))
    {
        divide_array(topology, rank, n, v, end_indexes, cluster_array);
    }

    multiply_array(rank, topology, cluster_array);

    if (isCoordinatorProc(rank))
    {
        merge_array(rank, cluster_array, end_indexes, v);
    }

    // procesul 0 afiseaza rezultatul final
    if (rank == COORD_0)
    {
        printf("Rezultat: ");
        for (int i = 0; i < n; i++)
        {
            printf("%d ", v[i]);
        }
        printf("\n");
        free(v);
    }

    MPI_Finalize();
}