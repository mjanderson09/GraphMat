/******************************************************************************
** Copyright (c) 2015, Intel Corporation                                     **
** All rights reserved.                                                      **
**                                                                           **
** Redistribution and use in source and binary forms, with or without        **
** modification, are permitted provided that the following conditions        **
** are met:                                                                  **
** 1. Redistributions of source code must retain the above copyright         **
**    notice, this list of conditions and the following disclaimer.          **
** 2. Redistributions in binary form must reproduce the above copyright      **
**    notice, this list of conditions and the following disclaimer in the    **
**    documentation and/or other materials provided with the distribution.   **
** 3. Neither the name of the copyright holder nor the names of its          **
**    contributors may be used to endorse or promote products derived        **
**    from this software without specific prior written permission.          **
**                                                                           **
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS       **
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT         **
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR     **
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT      **
** HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,    **
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED  **
** TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR    **
** PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF    **
** LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING      **
** NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS        **
** SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
* ******************************************************************************/
/* Narayanan Sundaram (Intel Corp.)
 * ******************************************************************************/
#include "GraphMatRuntime.cpp"

#include "Degree.cpp"

class PR {
  public:
    float pagerank;
    int degree;
  public:
    PR() {
      pagerank = 0.3;
      degree = 0;
    }
    int operator!=(const PR& p) {
      return (fabs(p.pagerank-pagerank)>1e-5);
    }
    friend std::ostream &operator<<(std::ostream &outstream, const PR & val)
    {
      outstream << val.pagerank; 
      return outstream;
    }
};

template <class E>
class PageRank : public GraphProgram<float, float, PR, E> {
  public:
    float alpha;

  public:

  PageRank(float a=0.3) {
    alpha = a;
    this->activity = ALL_VERTICES;
  }

  void reduce_function(float& a, const float& b) const {
    a += b;
  }
  void process_message(const float& message, const E edge_val, const PR& vertexprop, float& res) const {
    res = message;
  }
  bool send_message(const PR& vertexprop, float& message) const {
    if (vertexprop.degree == 0) {
      message = 0.0;
    } else {
      message = vertexprop.pagerank/(float)vertexprop.degree;
    }
    return true;
  }
  void apply(const float& message_out, PR& vertexprop) {
    vertexprop.pagerank = alpha + (1.0-alpha)*message_out; //non-delta update
  }

};


template <class edge>
void run_pagerank(const char * input_filename,
                  std::vector<int> ranks,
                  std::vector<int> partitions,
		  std::vector<int> sizes,
		  int nthreads) {

  Graph<PR, edge> G;
  PageRank<edge> pr;
  Degree<PR, edge> dg;

 
  G.MMapMTX(input_filename, ranks, partitions, sizes, nthreads*8); //nthread pieces of matrix
  //G.ReadMTX(filename, nthreads*8); //nthread pieces of matrix

  //auto dg_tmp = graph_program_init(dg, G);

  struct timeval start, end;
  gettimeofday(&start, 0);

  G.setAllActive();
  //run_graph_program(&dg, G, 1, &dg_tmp);
  run_graph_program(&dg, G, 1);

  gettimeofday(&end, 0);
  double time = (end.tv_sec-start.tv_sec)*1e3+(end.tv_usec-start.tv_usec)*1e-3;
  printf("Degree Time = %.3f ms \n", time);

  //graph_program_clear(dg_tmp);
  
  //auto pr_tmp = graph_program_init(pr, G);

  gettimeofday(&start, 0);

  G.setAllActive();
  //run_graph_program(&pr, G, -1, &pr_tmp);
  run_graph_program(&pr, G, -1);
  
  gettimeofday(&end, 0);
  time = (end.tv_sec-start.tv_sec)*1e3+(end.tv_usec-start.tv_usec)*1e-3;
  printf("PR Time = %.3f ms \n", time);

  //graph_program_clear(pr_tmp);

  MPI_Barrier(GraphPad::GRAPHMAT_COMM);
  for (int i = 1; i <= std::min((unsigned long long int)25, (unsigned long long int)G.getNumberOfVertices()); i++) { 
    if (G.vertexNodeOwner(i)) {
      printf("%d : %d %f\n", i, G.getVertexproperty(i).degree, G.getVertexproperty(i).pagerank);
    }
    fflush(stdout);
    MPI_Barrier(GraphPad::GRAPHMAT_COMM);
  }

  //G.saveVertexproperty("/dev/shm/pagerank_out");
  G.saveVertexpropertyBinHdfs("/blah/pagerank_out");

/*
  FILE * f = fopen("/dev/shm/partitions_out", "w");
  for(int i = 0 ; i < GraphPad::global_nrank ; i++)
  {
    fprintf(f, "%d,%d\n", i, i);
  }
  fclose(f);
  */

}

int main(int argc, char* argv[]) {
  if (argc < 3) {
    printf("Correct format: %s A.mtx partitions.txt\n", argv[0]);
    return 0;
  }
  const char* input_filename = argv[1];
  const char* partition_filename = argv[2];

  MPI_Init(&argc, &argv);

  int rank, size;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);

  std::vector<int> ranks;
  std::vector<int> partitions;
  std::vector<int> sizes;
  if(rank == 0)
  {
    FILE * partition_file = fopen(partition_filename, "r");
    assert(partition_file);

    int rank, partition_num, partition_size;
    while(fscanf(partition_file, "%d,%d,%d\n", &rank, &partition_num, &partition_size) > 0)
    {
      ranks.push_back(rank);
      partitions.push_back(partition_num);
      sizes.push_back(partition_size);
      std::cout << rank << "\t" << partition_num << "\t" << partition_size << std::endl;
    }
    fclose(partition_file);
  }
  int num_partitions = ranks.size();
  MPI_Bcast(&num_partitions, 1, MPI_INT, 0, MPI_COMM_WORLD);
  if(rank != 0)
  {
    ranks = std::vector<int>(num_partitions);
    partitions = std::vector<int>(num_partitions);
    sizes = std::vector<int>(num_partitions);
  }
  MPI_Bcast(ranks.data(), num_partitions, MPI_INT, 0, MPI_COMM_WORLD);
  MPI_Bcast(partitions.data(), num_partitions, MPI_INT, 0, MPI_COMM_WORLD);
  MPI_Bcast(sizes.data(), num_partitions, MPI_INT, 0, MPI_COMM_WORLD);


  if(rank != 0)
  {
    GraphPad::GB_Init(std::vector<int>({0}));

  #pragma omp parallel
    {
#pragma omp single
      {
        nthreads = omp_get_num_threads();
        printf("num threads got: %d\n", nthreads);
      }
    }
  
    run_pagerank<int>(input_filename, ranks, partitions, sizes, nthreads);
  }

  MPI_Finalize();
}

