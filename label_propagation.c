// * ============================================================
// * LIBRARIES & DEFINES & TYPEDEFS
// * ============================================================
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>


#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"


typedef unsigned int uint;

// ! ============================================================
// ! DATA STRUCTURE: COMPRESSED SPARSE ROW (CSR)
// ! ============================================================
typedef struct {
    size_t *offset;    // * Index pointing to start of neighbors
    uint *neighbors;   // * Flat array of all neighbors
    uint *degree;      // * Number of connections per node
    uint N;            // * Total number of nodes
} CSRGraph;


// * ============================================================
// * DEBUG - FUNCTIONS: EVERY PROCESS WRITE TO CORRESPONDING FILE
// * ============================================================
void delete_process_log(const char *base_name, int rank) {
    int len = snprintf(NULL, 0, "%s_%d.txt", base_name, rank);
    char *filename = malloc((len + 1) * sizeof(char));
    if (!filename) { perror("malloc filename"); exit(1); }
    snprintf(filename, len + 1, "%s_%d.txt", base_name, rank);
    remove(filename); // Delete if exists
    free(filename);
}

// * Appends a formatted message to a rank-specific log file
void write_log(const char *base_name, int rank, const char *format, ...) {
    // Construct the filename 
    int len = snprintf(NULL, 0, "%s_%d.txt", base_name, rank);
    char *filename = malloc((len + 1) * sizeof(char));
    if (!filename) { perror("malloc log name"); exit(1); }
    
    snprintf(filename, len + 1, "%s_%d.txt", base_name, rank);

    FILE *f = fopen(filename, "a");
    if (!f) {
        fprintf(stderr, "Rank %d failed to write to %s\n", rank, filename);
        free(filename);
        return;
    }

    // Print the prefix first
    fprintf(f, "Rank %d: ", rank);

    // Write the formatted message
    va_list args;
    va_start(args, format);
    vfprintf(f, format, args); 
    va_end(args);


    // ======================================================
    // COLOR SWITCH FOR TERMINAL OUTPUT
    // ======================================================
    // const char *color_code;
    
    // switch(rank % 4) {
    //     case 0: color_code = ANSI_COLOR_RED;     break;
    //     case 1: color_code = ANSI_COLOR_GREEN;   break;
    //     case 2: color_code = ANSI_COLOR_YELLOW;  break;
    //     case 3: color_code = ANSI_COLOR_BLUE;    break;
    //     default: color_code = ANSI_COLOR_RESET;  break;
    // }

    

    // // Print colorful notification to the terminal
    // printf("%sRank %d logged a message.%s\n", color_code, rank, ANSI_COLOR_RESET);

    // Cleanup
    fclose(f);
    free(filename);
}

// ! ============================================================
// ! FUNCTION: READ GRAPH (FILE IO)
// ! ============================================================
CSRGraph* read_graph(const char *filename) {
    FILE *f = fopen(filename,"r");
    if(!f){perror("fopen"); exit(1);}

    // * Pass 1: Count Nodes
    uint u,v;
    uint maxnode=0;
    size_t edge_count=0;
    while(fscanf(f,"%u %u",&u,&v)==2){
        if(u>maxnode) maxnode=u;
        if(v>maxnode) maxnode=v;
        edge_count++;
    }
    rewind(f);

    uint N = maxnode+1;
    uint *deg = calloc(N,sizeof(uint));
    while(fscanf(f,"%u %u",&u,&v)==2){
        deg[u]++; deg[v]++;
    }

    // * Pass 2: Build CSR
    size_t *offset = malloc((N+1)*sizeof(size_t));
    offset[0]=0;
    for(uint i=0;i<N;i++) offset[i+1]=offset[i]+deg[i];

    uint *neighbors=malloc(2*edge_count*sizeof(uint));
    size_t *cur=malloc(N*sizeof(size_t));
    memcpy(cur,offset,N*sizeof(size_t));
    
    rewind(f);
    while(fscanf(f,"%u %u",&u,&v)==2){
        neighbors[cur[u]++] = v;
        neighbors[cur[v]++] = u;
    }
    free(cur);
    fclose(f);

    CSRGraph *g=malloc(sizeof(CSRGraph));
    g->offset=offset; g->neighbors=neighbors; g->degree=deg; g->N=N;
    return g;
}

// * ============================================================
// * UTILITY FUNCTIONS
// * ============================================================
void shuffle(uint *array, uint n){
    for(uint i=n-1;i>0;i--){
        uint j = rand() % (i+1);
        uint tmp = array[i];
        array[i] = array[j];
        array[j] = tmp;
    }
}

// ! ============================================================
// ! MAIN EXECUTION
// ! ============================================================
int main(int argc,char **argv){
    // * ============================================================
    // * MPI INITIALIZATION
    // * ============================================================
    MPI_Init(&argc,&argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    // * Check Arguments (Only Rank 0)
    if(argc<2 && rank == 0){ 
        fprintf(stderr,"Usage: %s <edge_list.txt> [output_file]\n",argv[0]); 
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    CSRGraph *g = NULL; // * Only used by rank 0 
    uint N = 0; // * Number of nodes (to be broadcasted)       

    const char *outfile_name = (argc>=3) ? argv[2] : NULL;

    // * Clear old logs
    delete_process_log("process", rank);
    srand(time(NULL) + (rank * 1234)); // Seed RNG differently per rank

    // ! ============================================================
    // ! THE MASTER-WORKER SPLIT
    // ! ============================================================
    if (rank == 0) { 
        // * ==========================================
        // * RANK 0 (MASTER)
        // * ==========================================
        g = read_graph(argv[1]);
        N = g->N;
        write_log("process", rank, "Graph loaded with N=%u nodes\n", N);
    }
    // * ==========================================
    // * RANK 0-n (EVERYONE)
    // * ==========================================

    // * Broadcast N from Rank 0
    // ! Broadcast is used for receiving as well
    MPI_Bcast(&N, 1, MPI_UNSIGNED, 0, MPI_COMM_WORLD);
    write_log("process", rank, "Broadcast COMPLETE. I sent N=%u to the team.\n", N);
    write_log("process", rank, "Broadcast RECEIVED. I now know N=%u.\n", N);

    // * ============================================================
    // * BROADCAST THE GRAPH TOPOLOGY
    // * ============================================================
    // Allocate the struct on workers
    if (rank != 0) g = malloc(sizeof(CSRGraph));
    g->N = N;

    // Need the total # of edges (M) to allocate the neighbors array
    size_t num_edges = 0;
    if (rank == 0) num_edges = g->offset[N]; 

    // Share M
    MPI_Bcast(&num_edges, 1, MPI_UNSIGNED_LONG, 0, MPI_COMM_WORLD);
    write_log("process", rank, "Received # of edges: %zu\n", num_edges);

    // Allocate arrays on workers
    if (rank != 0) {
        g->offset = malloc((N + 1) * sizeof(size_t));
        g->neighbors = malloc(num_edges * sizeof(uint));
        g->degree = calloc(N, sizeof(uint)); 
    }

    // Broadcast the data
    MPI_Bcast(g->offset, N + 1, MPI_UNSIGNED_LONG, 0, MPI_COMM_WORLD);
    write_log("process", rank, "Offset array broadcast complete.\n");
    MPI_Bcast(g->neighbors, num_edges, MPI_UNSIGNED, 0, MPI_COMM_WORLD);
    write_log("process", rank, "Neighbors array broadcast complete.\n");



    // * ============================================================
    // * CALCULATE WORKLOAD 
    // * ============================================================
    uint nodes_per_proc = N / size;
    uint remainder = N % size;
    
    // Determine start and end index for current rank
    uint start_node = rank * nodes_per_proc;
    uint end_node = start_node + nodes_per_proc;
    if (rank == size - 1) end_node += remainder; // * Last rank takes the rest
    write_log("process", rank, "Assigned nodes [%u, %u)\n", start_node, end_node);

    uint my_count = end_node - start_node; 
    write_log("process", rank, "My node count: %u\n", my_count);

    // ! Prepare for MPI_Allgatherv 
    // Need to calculate how many items each rank sends/receives
    int *recv_counts = malloc(size * sizeof(int));
    int *displs = malloc(size * sizeof(int));
    
    
    for(int r=0; r<size; r++) {
        recv_counts[r] = nodes_per_proc;

        if(r == size-1) recv_counts[r] += remainder;
        
        displs[r] = r * nodes_per_proc;
    }


    

    //  ============================================================
    //  MEMORY ALLOCATIONS 
    //  ============================================================
    uint *label = malloc(N * sizeof(uint));
    uint *next_label = malloc(N * sizeof(uint));
    
    // Size N, used for counting neighbors
    uint *counts = calloc(N, sizeof(uint));     
    uint *last_seen = calloc(N, sizeof(uint));
    uint *seen_list = malloc(N * sizeof(uint));
    
    // * Initial condition: Everyone starts with their own ID
    for(uint i = 0; i < N; i++) label[i] = i;

    // * ---------------- Local Shuffle Setup ----------------
    uint *node_order = malloc(my_count * sizeof(uint));
    
    // Fill it with only current rank's range of nodes
    for(uint i = 0; i < my_count; i++) {
        node_order[i] = start_node + i;
    }



    // ! ============================================================
    // ! CORE ALGORITHM: LABEL PROPAGATION LOOP
    // ! ============================================================
    uint iterid = 1;
    int stable = 0;

    int silent_log = 1; // 1 = True (Print), 0 = False (Silent)

    while(!stable){
        stable=1;
        int local_stable = 1; // Track stability for current rank

        iterid++; 
        if(iterid==0){ memset(last_seen,0,N*sizeof(uint)); iterid=1; }

        // Shuffle node order for processing order
        shuffle(node_order, my_count);

        for(uint idx = 0; idx < my_count; idx++){
            uint i = node_order[idx]; 

            // ! Only log details for the first 5 nodes to save space
            int should_log = (silent_log && idx < 5); 
            
            size_t s = g->offset[i], e = g->offset[i+1];
            
            // * Skip isolated nodes
            if(s==e){ 
                next_label[i]=label[i]; 
                if (should_log) write_log("process", rank, "Node %u is isolated. Skipping.\n", i);
                continue; 
            }

            // * ===================================================
            // * NEIGHBOR LABEL COUNTING
            // * ===================================================
            uint seen_cnt=0;
            for(size_t nbr_idx=s; nbr_idx<e; nbr_idx++){
                uint nb = g->neighbors[nbr_idx];
                uint lab = label[nb]; 
                
                if(last_seen[lab]!=iterid){
                    last_seen[lab]=iterid;
                    counts[lab]=1;
                    seen_list[seen_cnt++]=lab;
                } else counts[lab]++;
            }

            // * Find max frequency
            uint max_count=0;
            for(uint k=0;k<seen_cnt;k++){
                if(counts[seen_list[k]]>max_count) max_count=counts[seen_list[k]];

                if (should_log) write_log("process", rank, "Node %u: Label %u has count %u\n", i, seen_list[k], counts[seen_list[k]]);
            }

            // * Pick smallest label among those with max count
            uint best_label = label[i]; // Start with current 
            for(uint k=0;k<seen_cnt;k++){
                uint L = seen_list[k];
                if(counts[L]==max_count && L<best_label){
                    best_label = L;
                    
                    if(should_log) write_log("process", rank, "Node %u: New best label candidate %u with count %u\n", i, L, counts[L]);
                }
            }
            next_label[i] = best_label;
            
            // Check LOCAL stability
            if(next_label[i] != label[i]) local_stable = 0;
        }

        silent_log = 1; // Enable silent log for synchronization phase

        // ! ============================================================
        // ! SYNCHRONIZATION
        // ! ============================================================
        // !Check Global Stability
        MPI_Allreduce(&local_stable, &stable, 1, MPI_INT, MPI_LAND, MPI_COMM_WORLD);
    
        if(silent_log)
            write_log("process", rank, "Iter %u: Local stability=%s, Global stability=%s\n", 
                        iterid, 
                        local_stable ? "STABLE" : "UNSTABLE", 
                        stable ? "CONVERGED" : "CONTINUING"
            );

    
    

        // Exchange Labels
        // * Send MY calculated chunk (next_label + start_node)
        MPI_Allgatherv(
            next_label + start_node, // Send Buffer
            my_count,                // Send Count
            MPI_UNSIGNED,            // Send Type
            label,                   // Recv Buffer (Full Array)
            recv_counts,             // Recv Counts Array
            displs,                  // Displacements Array
            MPI_UNSIGNED,            // Recv Type
            MPI_COMM_WORLD
        );
    
        if(silent_log)
            write_log("process", rank, "Iter %u: Label exchange complete. Global state updated for %u nodes.\n", 
                        iterid, 
                        N
            );

    
    
        // Update next_label to match label
        memcpy(next_label, label, N * sizeof(uint));

        silent_log = 0; // Disable silent log after first iteration
    } 


    write_log("process", rank, "Label propagation complete after %u iterations\n", iterid - 1);

    // * ============================================================
    // * OUTPUT & CLEANUP
    // * ============================================================
    if (rank == 0) {
        FILE *fout = stdout;
        if(outfile_name){
            fout = fopen(outfile_name,"w");
            if(!fout){ perror("fopen output"); }
        }
    

        if(fout) {
            for(uint i=0;i<N;i++) fprintf(fout,"%u %u\n",i,label[i]);
            if(fout != stdout) fclose(fout);
        
        }
    }

    // * Free memory
    free(label); free(next_label);
    free(counts); free(last_seen); free(seen_list);
    free(node_order);

    if (g != NULL) {
        free(g->offset); free(g->neighbors); free(g->degree); free(g);
    }
    

    write_log("process", rank, "-----------------------------Memory cleaned up-----------------------------\n");
    // Log before finizalize
    write_log("process", rank, "Process exiting cleanly. Calling MPI_Finalize now.\n");
    
    // * Ensure all processes reach this point before finalizing
    MPI_Barrier(MPI_COMM_WORLD);


    MPI_Finalize(); 
    return 0;
}