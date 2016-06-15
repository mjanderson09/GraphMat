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
/* Narayanan Sundaram (Intel Corp.), Michael Anderson (Intel Corp.)
 * ******************************************************************************/

#include <cstring>
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <unistd.h>
#include <cstdlib>
#include <sys/time.h>
#include <parallel/algorithm>
#include <omp.h>
#include <cassert>
#include "src/layouts.h"

inline double sec(struct timeval start, struct timeval end)
{
    return ((double)(((end.tv_sec * 1000000 + end.tv_usec) - (start.tv_sec * 1000000 + start.tv_usec))))/1.0e6;
}

typedef __declspec(align(16)) struct EDGE_T
{
  int src;
  int dst;
  int val;
  int partition_id;
} edge_t;

template<class T>
void AddFn(T a, T b, T* c, void* vsp) {
  *c = a + b ;
}

extern int nthreads;

//const int MAX_PARTS = 512; 

template <class V, class E=int>
class Graph {

  public:
    //V* vertexproperty;
    //MatrixDC<E>** mat;
    //MatrixDC<E>** matT; //transpose
    //int nparts;
    //unsigned long long int* id; // vertex id's if any
    //bool* active;
    int nvertices;
    long long int nnz;
    int vertexpropertyowner;
    int tiles_per_dim;
    bool vertexID_randomization;

    //int *start_src_vertices; //start and end of transpose parts
    //int *end_src_vertices;

    GraphPad::SpMat<GraphPad::COOTile<E> > A;
    GraphPad::SpMat<GraphPad::COOTile<E> > AT;
    GraphPad::SpVec<GraphPad::DenseSegment<V> > vertexproperty;
    GraphPad::SpVec<GraphPad::DenseSegment<bool> > active;
//    int start_dst_vertices[MAX_PARTS];
//    int end_dst_vertices[MAX_PARTS];

  public:
    void ReadMTX(const char* filename, int grid_size); 
    void ReadMTX_sort(const char* filename, int grid_size, int alloc=1); 
    void MMapMTX(const char*, std::vector<int>, std::vector<int>, std::vector<int>, int grid_size); 
    void MMapMTX_sort(const char*, std::vector<int>, std::vector<int>, std::vector<int>, int grid_size, int alloc=1); 
    void ReadMTX_sort(edge_t* edges, int m, int n, int nnz, int grid_size, int alloc=1); 
    void setAllActive();
    void setAllInactive();
    int vertexToNative(int vertex, int nsegments, int len) const;
    int nativeToVertex(int vertex, int nsegments, int len) const;
    void setActive(int v);
    void setInactive(int v);
    void setAllVertexproperty(const V& val);
    void setVertexproperty(int v, const V& val);
    V getVertexproperty(int v) const;
    bool vertexNodeOwner(const int v) const;
    void saveVertexproperty(std::string fname) const;
    void saveVertexpropertyBin(std::string fname) const;
    void saveVertexpropertyBinHdfs(std::string fname) const;
    void reset();
    void shareVertexProperty(Graph<V,E>& g);
    int getBlockIdBySrc(int vertexid) const;
    int getBlockIdByDst(int vertexid) const;
    int getNumberOfVertices() const;
    void applyToAllVertices(void (*ApplyFn)(V, V*, void*), void* param=nullptr);
    template<class T> void applyReduceAllVertices(T* val, void (*ApplyFn)(V*, T*, void*), void (*ReduceFn)(T,T,T*,void*)=AddFn<T>, void* param=nullptr);
    ~Graph();
};

// int Read(char *fname, int*** _M, int **_Mg, int** _Mmg ,int* _g, int* m1) {
  // FILE* fp = fopen(fname, "r");
  // int m, n, nnz;
  // fscanf(fp, "%d %d %d", &m, &n, &nnz);
  // *m1 = m;

  // if (*_g > m) {
    // *_g = m;
  // }
  // int g = *_g;

  // *_M = (int**) _mm_malloc(sizeof(int*) * 3, 64);
  // *_Mg = (int*) _mm_malloc(sizeof(int) * (g + 1), 64);
  // *_Mmg = (int*) _mm_malloc(sizeof(int) * (g + 1), 64);

  // int** M = *_M;
  // int *Mmg = *_Mmg;
  // int *Mg = *_Mg;

  // M[0] = (int*) _mm_malloc(sizeof(int) * nnz, 64);
  // M[1] = (int*) _mm_malloc(sizeof(int) * nnz, 64);
  // M[2] = (int*) _mm_malloc(sizeof(int) * nnz, 64);

  // int r = m % g;
  // int q = m / g;

  // Mmg[0] = 1;
  // //printf("%d ", Mmg[0]);
  // for (int i = 1; i <= g; i++) {
    // if (r) {
      // Mmg[i] = Mmg[i - 1] + q + 1;
      // //printf("%d ", Mmg[i]);
      // r--;
    // } else {
      // Mmg[i] = Mmg[i - 1] + q;
      // //printf("%d ", Mmg[i]);
    // }
  // }
  // //printf("\n");

  // int tid = 0;
  // for (int i = 0; i < nnz; i++) {
    // fscanf(fp, "%d %d %d", &M[0][i], &M[1][i], &M[2][i]);
    // while (M[0][i] >= Mmg[tid]) {
      // Mg[tid] = i;
      // //printf("Mg[%d] = %d\n", tid, Mg[tid]);
      // tid++;
    // }
  // }
  // Mg[tid] = nnz;
  // //printf("Mg[%d] = %d\n", tid, Mg[tid]);
  // for (tid = tid + 1; tid <= g; tid++) {
    // Mg[tid] = nnz;
    // //printf("Mg[%d] = %d\n", tid, Mg[tid]);
  // }
  // //printf("--------\n");
  // fclose(fp);

  // return n;
// }


// int ReadBinary(char *fname, int*** _M, int **_Mg, int** _Mmg ,int* _g, int* m1) {
  // FILE* fp = fopen(fname, "rb");
  // int m, n, nnz;
  // //fscanf(fp, "%d %d %d", &m, &n, &nnz);
  // int tmp_[3];
  // fread(tmp_, sizeof(int), 3, fp);
  // m = tmp_[0];
  // n = tmp_[1];
  // nnz = tmp_[2];
  // printf("Graph %d x %d : %d edges \n", m, m, nnz);

  // *m1 = m;

  // if (*_g > m) {
    // *_g = m;
  // }
  // int g = *_g;

  // *_M = (int**) _mm_malloc(sizeof(int*) * 3, 64);
  // *_Mg = (int*) _mm_malloc(sizeof(int) * (g + 1), 64);
  // *_Mmg = (int*) _mm_malloc(sizeof(int) * (g + 1), 64);

  // int** M = *_M;
  // int *Mmg = *_Mmg;
  // int *Mg = *_Mg;

  // size_t nnz_l = nnz;
  // M[0] = (int*) _mm_malloc(sizeof(int) * nnz_l, 64);
  // M[1] = (int*) _mm_malloc(sizeof(int) * nnz_l, 64);
  // M[2] = (int*) _mm_malloc(sizeof(int) * nnz_l, 64);

  // int* data_dump = (int*) _mm_malloc(sizeof(int)*nnz_l*3, 64);
  // fread(data_dump, sizeof(int), nnz_l*3, fp);

  // int r = m % g;
  // int q = m / g;
  // Mmg[0] = 0;
  // //printf("%d ", Mmg[0]);
  // for (int i = 1; i <= g; i++) {
    // if (r) {
      // Mmg[i] = Mmg[i - 1] + q + 1;
      // //printf("%d ", Mmg[i]);
      // r--;
    // } else {
      // Mmg[i] = Mmg[i - 1] + q;
      // //printf("%d ", Mmg[i]);
    // }
  // }
  // for (int i = 0; i <= g; i++) {
    // printf("%d ", Mmg[i]);
  // }
  // printf("\n");
  // //printf("\n");

  // srand(0);
  // int* tmp;
  // int tid = 0;
  // for (int i = 0; i < nnz; i++) {
    // //fscanf(fp, "%d %d %d", &M[0][i], &M[1][i], &M[2][i]);
    // //fread(tmp, sizeof(int), 3, fp);
    // tmp = data_dump + i*3;
 
    // M[0][i] = tmp[0] - 1;
    // M[1][i] = tmp[1] - 1;
    // M[2][i] = tmp[2];
    // //printf("%d %d %d \n", M[0][i], M[1][i], M[2][i]);
    // //M[2][i] = tmp[2] + (int)( (float)rand()/(float)RAND_MAX*32);

    // if (i > 0) {
      // if (tmp[0] < M[0][i-1]) { //not sorted order
        // printf("Found edge %d - %d after %d - %d : position in file %d\n", tmp[0], tmp[1], M[0][i-1], M[1][i-1], i);
        // printf("Input edge list not sorted according to starting vertex. Quitting now. \n");
        // exit(1);
      // }
    // }

    // while (M[0][i] >= Mmg[tid]) {
      // Mg[tid] = i;
      // printf("Mg[%d] = %d\n", tid, Mg[tid]);
      // tid++;
    // }
  // }
  // if (nnz <= 20) for (int i = 0; i < nnz; i++) printf("%d %d %d \n", M[0][i], M[1][i], M[2][i]);

  // Mg[tid] = nnz;
  // //printf("Mg[%d] = %d\n", tid, Mg[tid]);
  // for (tid = tid + 1; tid <= g; tid++) {
    // Mg[tid] = nnz;
    // //printf("Mg[%d] = %d\n", tid, Mg[tid]);
  // }
  // //printf("--------\n");
  // fclose(fp);

  // _mm_free(data_dump);

  // return n;
// }

// int ReadAndTranspose(char *fname, int*** _M, int **_Mg, int** _Mmg ,int* _g, int* m1) {
  // FILE* fp = fopen(fname, "r");
  // int m, n, nnz;
  // fscanf(fp, "%d %d %d", &m, &n, &nnz);
  // *m1 = n;

  // if (*_g > n) {
    // *_g = n;
  // }
  // int g = *_g;

  // *_M = (int**) _mm_malloc(sizeof(int*) * 3, 64);
  // *_Mg = (int*) _mm_malloc(sizeof(int) * (g + 1), 64);
  // *_Mmg = (int*) _mm_malloc(sizeof(int) * (g + 1), 64);

  // int** M = *_M;
  // int *Mmg = *_Mmg;
  // int *Mg = *_Mg;

  // M[0] = (int*) _mm_malloc(sizeof(int) * nnz, 64);
  // M[1] = (int*) _mm_malloc(sizeof(int) * nnz, 64);
  // M[2] = (int*) _mm_malloc(sizeof(int) * nnz, 64);

  // int r = n % g;
  // int q = n / g;
  // Mmg[0] = 1;
  // //printf("%d ", Mmg[0]);
  // for (int i = 1; i <= g; i++) {
    // if (r) {
      // Mmg[i] = Mmg[i - 1] + q + 1;
      // //printf("%d ", Mmg[i]);
      // r--;
    // } else {
      // Mmg[i] = Mmg[i - 1] + q;
      // //printf("%d ", Mmg[i]);
    // }
  // }
  // //printf("\n");
  // /*
  // int **A = (int**) _mm_malloc(sizeof(int*) * 3, 64);
  // A[0] = (int*) _mm_malloc(sizeof(int) * nnz, 64);
  // A[1] = (int*) _mm_malloc(sizeof(int) * nnz, 64);
  // A[2] = (int*) _mm_malloc(sizeof(int) * nnz, 64);

  // int *t = (int*) _mm_malloc(sizeof(int) * (n + 1), 64);
  // */
  // int* A[3];
  // A[0] = new int[nnz];
  // A[1] = new int[nnz];
  // A[2] = new int[nnz];
  // int* t = new int[n + 1];

  // for (int i = 0; i <= n; i++) {
    // t[i] = 0;
  // }

  // for (int i = 0; i < nnz; i++) {
    // fscanf(fp, "%d %d %d", &A[0][i], &A[1][i], &A[2][i]);
    // t[A[1][i]]++;
  // }

  // //int *j = (int*) _mm_malloc(sizeof(int) * (n + 1), 64);
  // int* j = new int[n + 1];

  // j[0] = 0;
  // for (int i = 1; i <= n; i++) {
    // j[i] = 0;
    // t[i] += t[i-1];
  // }

  // for (int i = 0; i < nnz; i++) {
    // int x = t[A[1][i]-1] + j[A[1][i]];
    // M[0][x] = A[1][i];
    // M[1][x] = A[0][i];
    // M[2][x] = A[2][i];
    // j[A[1][i]]++;
  // }
  // delete A[0];
  // delete A[1];
  // delete A[2];
  // delete t;
  // delete j;

  // int tid = 0;
  // for (int i = 0; i < nnz; i++) {
    // while (M[0][i] >= Mmg[tid]) {
      // Mg[tid] = i;
      // //printf("Mg[%d] = %d\n", tid, Mg[tid]);
      // tid++;
    // }
  // }
  // Mg[tid] = nnz;
  // //printf("Mg[%d] = %d\n", tid, Mg[tid]);
  // for (tid = tid + 1; tid <= g; tid++) {
    // Mg[tid] = nnz;
    // //printf("Mg[%d] = %d\n", tid, Mg[tid]);
  // }
  // //printf("--------\n");
  // fclose(fp);

  // return m;
// }

// int ReadAndTransposeBinary(char *fname, int*** _M, int **_Mg, int** _Mmg ,int* _g, int* m1) {
  // FILE* fp = fopen(fname, "rb");
  // int m, n, nnz;
  // //fscanf(fp, "%d %d %d", &m, &n, &nnz);
  // int tmp_[3];
  // fread(tmp_, sizeof(int), 3, fp);
  // m = tmp_[0];
  // n = tmp_[1];
  // nnz = tmp_[2];

  // *m1 = n;

  // if (*_g > n) {
    // *_g = n;
  // }
  // int g = *_g;

  // *_M = (int**) _mm_malloc(sizeof(int*) * 3, 64);
  // *_Mg = (int*) _mm_malloc(sizeof(int) * (g + 1), 64);
  // *_Mmg = (int*) _mm_malloc(sizeof(int) * (g + 1), 64);

  // int** M = *_M;
  // int *Mmg = *_Mmg;
  // int *Mg = *_Mg;

  // size_t nnz_l = nnz;
  // M[0] = (int*) _mm_malloc(sizeof(int) * nnz_l, 64);
  // M[1] = (int*) _mm_malloc(sizeof(int) * nnz_l, 64);
  // M[2] = (int*) _mm_malloc(sizeof(int) * nnz_l, 64);

  // int* data_dump = (int*) _mm_malloc(sizeof(int)*nnz_l*3, 64);
  // fread(data_dump, sizeof(int), nnz_l*3, fp);

  // int r = n % g;
  // int q = n / g;
  // //int q512 = ((q+511)/512)*512; //q rounded up to 512
  // //int r = n - q512*g;
  // int n512 = (n/512)/g;
  // if (n512 == 0) n512 = 1;

  // Mmg[0] = 0;
  // for (int i = 1; i < g; i++) {
    // Mmg[i] = std::min(n512*i*512, n);
    // //printf("%d ", Mmg[i]);
  // }
  // Mmg[g] = n;
  
  // for (int i = 0; i <= g; i++) {
    // printf("%d ", Mmg[i]);
  // }
  // printf("\n");
  // /*Mmg[0] = 1;
  // //printf("%d ", Mmg[0]);
  // for (int i = 1; i <= g; i++) {
    // if (r) {
      // Mmg[i] = Mmg[i - 1] + q + 1;
      // //printf("%d ", Mmg[i]);
      // r--;
    // } else {
      // Mmg[i] = Mmg[i - 1] + q;
    // }
    // printf("%d ", Mmg[i]);
  // }*/
  // //printf("\n");
  // /*
  // int **A = (int**) _mm_malloc(sizeof(int*) * 3, 64);
  // A[0] = (int*) _mm_malloc(sizeof(int) * nnz, 64);
  // A[1] = (int*) _mm_malloc(sizeof(int) * nnz, 64);
  // A[2] = (int*) _mm_malloc(sizeof(int) * nnz, 64);

  // int *t = (int*) _mm_malloc(sizeof(int) * (n + 1), 64);
  // */
  // int* tmp;
  // int* A[3];
  // A[0] = new int[nnz];
  // A[1] = new int[nnz];
  // A[2] = new int[nnz];
  // int* t = new int[n+1];

  // for (int i = 0; i <= n; i++) {
    // t[i] = 0;
  // }

  // srand(0);
  // for (int i = 0; i < nnz; i++) {
    // //fscanf(fp, "%d %d %d", &A[0][i], &A[1][i], &A[2][i]);
    // //fread(tmp, sizeof(int), 3, fp);
    // tmp = data_dump + i*3;

    // A[0][i] = tmp[0] - 1;
    // A[1][i] = tmp[1] - 1;
    // A[2][i] = tmp[2];
    // //A[2][i] = tmp[2] + (int)( (float)rand()/(float)RAND_MAX*32);

    // t[A[1][i]+1]++;
  // }

  // //int *j = (int*) _mm_malloc(sizeof(int) * (n + 1), 64);
  // int* j = new int[n + 1];

  // j[0] = 0;
  // for (int i = 1; i < n; i++) {
    // j[i] = 0;
    // t[i] += t[i-1];
  // }

  // for (int i = 0; i < nnz; i++) {
    // int x = t[A[1][i]] + j[A[1][i]+1];
    // M[0][x] = A[1][i];
    // M[1][x] = A[0][i];
    // M[2][x] = A[2][i];
    // j[A[1][i]+1]++;
  // }
  // delete [] A[0];
  // delete [] A[1];
  // delete [] A[2];
  // delete [] t;
  // delete [] j;

  // int tid = 0;
  // for (int i = 0; i < nnz; i++) {
    // while (M[0][i] >= Mmg[tid] && tid<=g) {
      // Mg[tid] = i;
      // printf("Mg[%d] = %d\n", tid, Mg[tid]);
      // tid++;
    // }
  // }
  // Mg[tid] = nnz;
  // //printf("Mg[%d] = %d\n", tid, Mg[tid]);
  // for (tid = tid + 1; tid <= g; tid++) {
    // Mg[tid] = nnz;
    // //printf("Mg[%d] = %d\n", tid, Mg[tid]);
  // }
  // //printf("--------\n");
  // fclose(fp);
  // _mm_free(data_dump);

  // return m;
// }

void print_edges(edge_t * edges, int nedges)
{
  for(int edge_id = 0 ; edge_id < nedges ; edge_id++)
  {
    std::cout << edges[edge_id].src << ", " << edges[edge_id].dst << ", " << edges[edge_id].val << ", " << edges[edge_id].partition_id << std::endl;
  }
}

void write_edges_binary(edge_t * edges, char * fname, int m, int n, int nnz)
{
  std::ofstream outfile(fname, std::ios::out | std::ios::binary);
  if(outfile.is_open())
  {
    outfile.write((char*) &m, sizeof(int));
    outfile.write((char*) &n, sizeof(int));
    outfile.write((char*) &nnz, sizeof(int));
    for(int edge_id = 0 ; edge_id < nnz ; edge_id++)
    {
      outfile.write((char*) &(edges[edge_id].src), sizeof(int));
      outfile.write((char*) &(edges[edge_id].dst), sizeof(int));
      outfile.write((char*) &(edges[edge_id].val), sizeof(int));
    }
  }
  else
  {
    std::cout << "Could not open binary output file" << std::endl;
    exit(0);
  }
  outfile.close();
}

void dcsc_to_edges(int ** &vals, int ** &row_inds, int ** &col_ptrs, int ** col_indices, 
                int * &nnzs, int * ncols,
                int num_partitions, edge_t * edges, int * row_pointers, 
		int * edge_pointers, int n)
{
  int edge_id = 0;
  for(int p = 0 ; p < num_partitions ; p++)
  {
    int * val = vals[p];
    int * row_ind = row_inds[p];
    int * col_ptr = col_ptrs[p];
    int * col_index = col_indices[p];
    int nnz_partition = nnzs[p];
    int ncol_partition = ncols[p];
    std::cout << "ncol_partition: " << ncol_partition << std::endl;
    for(int j_index = 0 ; j_index < ncol_partition ; j_index++)
    {
      int j = col_index[j_index];
      // for each element in the column
      for(int i = col_ptr[j_index] ; i < col_ptr[j_index+1] ; i++)
      {
        edges[edge_id].src = row_pointers[p] + row_ind[i];
	edges[edge_id].dst = j+1;
	edges[edge_id].val = val[i];
	edges[edge_id].partition_id = p;
	edge_id++;
      }
    }
  }
}

bool compare_notrans(const edge_t & a, const edge_t & b)
{
  if(a.partition_id < b.partition_id) return true;
  else if(a.partition_id > b.partition_id) return false;

  if(a.dst < b.dst) return true;
  else if(a.dst > b.dst) return false;
  
  if(a.src < b.src) return true;
  else if(a.src > b.src) return false;
  return false;
}

bool compare_trans(const edge_t & a, const edge_t & b)
{
  // sort by partition id, dst id, src id
  bool res = a.partition_id < b.partition_id;
  if(a.partition_id == b.partition_id)
  {
    res = a.src < b.src;
    if(a.src == b.src)
    {
      res = a.dst < b.dst;
    }
  }  
  return res;
}

void read_from_binary(const char * fname, int &m, int &n, int &nnz, edge_t * &edges)
{
  std::ifstream fin(fname, std::ios::binary);
  if(fin.is_open())
  {
    // Get header
    fin.read((char*)&m, sizeof(int));
    fin.read((char*)&n, sizeof(int));
    fin.read((char*)&nnz, sizeof(int));
    std::cout << "Got graph with m=" << m << "\tn=" << n << "\tnnz=" << nnz << std::endl;

    // Create edge list
    int * edge_blob = (int*) _mm_malloc(3*(long int)nnz*sizeof(edge_t), 64);
    fin.read((char*)edge_blob, 3*(long int)nnz*sizeof(int));

    //printf("First line %d %d %d \n", edge_blob[0], edge_blob[1], edge_blob[2]);

    edges = (edge_t*) _mm_malloc((long int)nnz * sizeof(edge_t), 64);
    #pragma omp parallel for
    for(unsigned long int edge_id = 0 ; edge_id < nnz ; edge_id++)
    {
      edges[edge_id].src = edge_blob[3*edge_id+0] - 1; //move to 0-based
      edges[edge_id].dst = edge_blob[3*edge_id+1] - 1; //move to 0-based
      edges[edge_id].val = edge_blob[3*edge_id+2];
    }
    _mm_free(edge_blob);
  }
  else
  {
    std::cout << "Could not open file " << fname << std::endl;
    exit(0);
  }
  fin.close();
}

void read_from_txt(char * fname, int &m, int &n, int &nnz, edge_t * &edges)
{
  std::ifstream fin(fname);
  if(fin.is_open())
  {
    std::string ln;

    // Get header
    getline(fin, ln);
    std::istringstream ln_ss(ln);
    ln_ss >> m;
    ln_ss >> n;
    ln_ss >> nnz;
    std::cout << "Got graph with m=" << m << "\tn=" << n << "\tnnz=" << nnz << std::endl;

    // Create edge list
    edges = (edge_t*) _mm_malloc((long int)nnz * sizeof(edge_t), 64);
    int edge_id = 0;
    int max_src = 0;
    while(getline(fin, ln))
    {
      std::istringstream ln_ss(ln);
      ln_ss >> edges[edge_id].src;
      edges[edge_id].src--;
      ln_ss >> edges[edge_id].dst;
      edges[edge_id].dst--;
      ln_ss >> edges[edge_id].val;
      edge_id++;
    }
  }
  else
  {
    std::cout << "File did not open" << std::endl;
    exit(0);
  }
  fin.close();
}

void static_partition(int * &row_pointers, int m, int num_partitions, int round)
{
    row_pointers = new int[num_partitions+1];

    if(round == 1)
    {
      int rows_per_partition = m / num_partitions;
      int rows_leftover = m % num_partitions;
      row_pointers[0] = 0;
      int current_row = row_pointers[0] + rows_per_partition;
      for(int p = 1 ; p < num_partitions+1 ; p++)
      {
        if(rows_leftover > 0)
        {
  	  current_row += 1;
          row_pointers[p] = current_row;
          current_row += rows_per_partition;
	  rows_leftover--;
        }
	else
	{
          row_pointers[p] = current_row;
          current_row += rows_per_partition;
	}
      }
    }
    else
    {
      int n512 = std::max((m / round) / num_partitions, 1);
      int n_round = std::max(0, m/round - n512*num_partitions);
      //printf("n_round = %d\n", n_round);
      assert(n_round < num_partitions);
      row_pointers[0] = 0;
      for(int p = 1 ; p < num_partitions ; p++)
      {
        //row_pointers[p] = std::min(n512*p*round,m);
        row_pointers[p] = row_pointers[p-1] + ((n_round>0)?((n512 + 1)*round):(n512*round));
        row_pointers[p] = std::min(row_pointers[p], m);
        if (n_round > 0) n_round--;
      }
      row_pointers[num_partitions] = m;
    }
  /*for (int i = 0; i <= num_partitions; i++) {
    printf("%d ", row_pointers[i]);
  }
  printf("\n");*/
}

void set_edge_pointers(edge_t * edges, int * row_pointers, int * &edge_pointers, 
                       int nnz, int num_partitions)
{
  // Figure out edge pointers
  edge_pointers = new int[num_partitions+1];
#define BINARY_SEARCH_EDGE_POINTERS
#ifndef BINARY_SEARCH_EDGE_POINTERS
  int p = 0;
  for(int edge_id = 0 ; edge_id < nnz ; edge_id++)
  {
    while(edges[edge_id].src >= row_pointers[p])
    {
      edge_pointers[p] = edge_id;
      p++;
    }
  }
  edge_pointers[p] = nnz;
  for(p = p+1 ; p < num_partitions+1 ; p++)
  {
    edge_pointers[p] = nnz;
  }
#else
  //#pragma omp parallel for
  for(int p = 0 ; p < num_partitions ; p++)
  {
    // binary search
    int e1 = 0;
    int e2 = nnz-1;
    int eh;
    while(e2 >= e1)
    {
      eh = e2 - (e2 - e1) / 2;
      if((edges[eh-1].src < row_pointers[p]) && edges[eh].src >= row_pointers[p])
      {
        break;
      }
      else if(edges[eh].src >= row_pointers[p])
      {
        e2 = eh-1;
      }
      else
      {
        e1 = eh+1;
      }
    }
    edge_pointers[p] = eh;
    //std::cout << edge_pointers[p] << "\t" << eh << std::endl;
  }
  edge_pointers[num_partitions] = nnz;
#endif
}

void build_dcsc(int ** &vals, int ** &row_inds, int ** &col_ptrs, int ** &col_indices,
                int * &nnzs, int * &ncols,
                int num_partitions, edge_t * edges, int * row_pointers, 
		int * edge_pointers, int n)
{
  vals = new int*[num_partitions];
  row_inds = new int*[num_partitions];
  col_ptrs = new int*[num_partitions];
  col_indices = new int*[num_partitions];
  nnzs = new int[num_partitions];
  ncols = new int[num_partitions];

  #pragma omp parallel for
  for(int p = 0 ; p < num_partitions ; p++)
  {
    int nnz_partition = nnzs[p] = edge_pointers[p+1] - edge_pointers[p];
    vals[p] = (int*) _mm_malloc(nnz_partition * sizeof(int), 64);
    row_inds[p] = (int*) _mm_malloc(nnz_partition * sizeof(int), 64);
    int * val = vals[p];
    int * row_ind = row_inds[p];

    int current_column = -1;
    int num_columns = 0;
    for(int edge_id = edge_pointers[p] ; edge_id < edge_pointers[p+1] ; edge_id++)
    {
      if(current_column < edges[edge_id].dst)
      {
        num_columns++;
	current_column = edges[edge_id].dst;
      }
    }
    ncols[p] = num_columns;
    col_indices[p] = (int*) _mm_malloc((num_columns+1) * sizeof(int), 64);
    col_ptrs[p] = (int*) _mm_malloc((num_columns+1) * sizeof(int), 64);
    int * col_index = col_indices[p];
    int * col_ptr = col_ptrs[p];
    current_column = -1;
    int current_column_num = -1;
    for(int edge_id = edge_pointers[p] ; edge_id < edge_pointers[p+1] ; edge_id++)
    {
      val[edge_id - edge_pointers[p]] = edges[edge_id].val;
      row_ind[edge_id - edge_pointers[p]] = edges[edge_id].src; 
      if(current_column < edges[edge_id].dst)
      {
        current_column_num++;
	current_column = edges[edge_id].dst;
	col_index[current_column_num] = current_column;
	col_ptr[current_column_num] = edge_id - edge_pointers[p];
      }
    }
    col_ptr[num_columns] = nnz_partition;
    col_index[num_columns] = n+1;
  }
}

void partition_and_build_dcsc(int * &row_pointers,
                              int * &edge_pointers,
  			      int ** &vals,
			      int ** &row_inds,
			      int ** &col_ptrs,
			      int ** &col_indices,
			      int * &nnzs, 
			      int * &ncols, 
			      edge_t * edges,
			      int m, 
			      int n,
			      int num_partitions,
			      int nnz,
			      int round)
{
  static_partition(row_pointers, m, num_partitions, round);

  struct timeval start, end;

  gettimeofday(&start, NULL);
  // Set partition ids
  #pragma omp parallel for
  for(int edge_id = 0 ; edge_id < nnz ; edge_id++)
  {
#define SET_PARTITION_IDS_BINARY_SEARCH
#ifndef SET_PARTITION_IDS_BINARY_SEARCH
    for(int p = 0 ; p < num_partitions ; p++)
    {
      if(edges[edge_id].src >= row_pointers[p] && edges[edge_id].src < row_pointers[p+1])
      {
        edges[edge_id].partition_id = p;
      }
    }
#else
    int key = edges[edge_id].src;
    int min_p = 0;
    int max_p = num_partitions-1;
    int h_p;
    while(max_p >= min_p)
    {
      h_p = max_p - ((max_p - min_p) / 2);
      if(key >= row_pointers[h_p] && key < row_pointers[h_p+1]) 
      {
        break;
      }
      else if(key >= row_pointers[h_p])
      {
        min_p = h_p+1;
      }
      else
      {
        max_p = h_p-1;
      }
    }
    edges[edge_id].partition_id = h_p;
#endif
  }
  gettimeofday(&end, NULL);
  std::cout << "Finished setting ids, time: " << sec(start,end)  << std::endl;

  unsigned long int nnz_l = nnz;

  // Sort edge list
  std::cout << "Starting sort" << std::endl;
  gettimeofday(&start, NULL);
  __gnu_parallel::sort(edges, edges+nnz_l, compare_notrans);
  gettimeofday(&end, NULL);
  std::cout << "Finished sort, time: " << sec(start,end)  << std::endl;

  //std::cout << "Sorted graph begin" << std::endl;
  //print_edges(edges, 9);
  //std::cout << "Sorted graph end" << std::endl;

  gettimeofday(&start, NULL);
  set_edge_pointers(edges, row_pointers, edge_pointers, nnz, num_partitions);
  gettimeofday(&end, NULL);
  std::cout << "Finished setting edge pointers, time: " << sec(start,end)  << std::endl;

  for(int p = 0 ; p < num_partitions+1 ; p++)
  {
    //std::cout << "p: " << p << "\t edge pointer: " << edge_pointers[p] << std::endl;
  }

  // build DCSC
  std::cout << "Starting build_dcsc" << std::endl;
  gettimeofday(&start, NULL);
  build_dcsc(vals, row_inds, col_ptrs, col_indices, nnzs, ncols, num_partitions, edges, row_pointers, edge_pointers, n);
  gettimeofday(&end, NULL);
  std::cout << "Finished build_dcsc, time: " << sec(start,end)  << std::endl;


}

#define CHECK(a,b,c) { if((a)!=(b)) {std::cout << a << " " << b << " ERROR: " << c << std::endl; exit(0);} }

template<class V, class E>
void Graph<V,E>::ReadMTX(const char* filename, int grid_size) {

  ReadMTX_sort(filename, grid_size); 

  return;
}

template<class V, class E>
void Graph<V,E>::MMapMTX(const char* filename, std::vector<int> ranks, 
                         std::vector<int> partitions,
                         std::vector<int> sizes,
                         int grid_size) {

  MMapMTX_sort(filename, ranks, partitions, sizes, grid_size); 

  return;
}

template<class V, class E>
int Graph<V,E>::vertexToNative(int vertex, int nsegments, int len) const
{
  if (vertexID_randomization) {

    int v = vertex-1;
    int npartitions = omp_get_max_threads() * 16 * nsegments;
    int height = len / npartitions;
    int vmax = height * npartitions;
    if(v >= vmax)
    {
      return v+1;
    }
    int col = v%npartitions;
    int row = v/npartitions;
    return row + col * height+ 1;
  } else {
    return vertex;
  }
}

template<class V, class E>
int Graph<V,E>::nativeToVertex(int vertex, int nsegments, int len) const
{
  if (vertexID_randomization) {
    int v = vertex-1;
    int npartitions = omp_get_max_threads() * 16 * nsegments;
    int height = len / npartitions;
    int vmax = height * npartitions;
    if(v >= vmax)
    {
      return v+1;
    }
    int col = v/height;
    int row = v%height;
    return col + row * npartitions+ 1;
  } else {
    return vertex;
  }
}

template<class V, class E>
void Graph<V,E>::MMapMTX_sort(const char * input_filename,
                              std::vector<int> ranks,
                              std::vector<int> partitions,
                              std::vector<int> sizes, int grid_size, int alloc) {

  if (GraphPad::global_nrank == 1) {
    vertexID_randomization = false;
  } else {
    vertexID_randomization = true;
  }

  struct timeval start, end;
  gettimeofday(&start, 0);
  {
    GraphPad::edgelist_t<E> A_edges;
    GraphPad::MMapEdgesBin(&A_edges, input_filename, ranks, partitions, sizes);
    tiles_per_dim = GraphPad::global_nrank;
    
    #pragma omp parallel for
    for(int i = 0 ; i < A_edges.nnz ; i++)
    {
      A_edges.edges[i].src = vertexToNative(A_edges.edges[i].src, tiles_per_dim, A_edges.m);
      A_edges.edges[i].dst = vertexToNative(A_edges.edges[i].dst, tiles_per_dim, A_edges.m);
    }

    GraphPad::AssignSpMat(A_edges, &A, tiles_per_dim, tiles_per_dim, GraphPad::partition_fn_2d);
    GraphPad::Transpose(A, &AT, tiles_per_dim, tiles_per_dim, GraphPad::partition_fn_2d);

    int m_ = A.m;
    assert(A.m == A.n);
    nnz = A.getNNZ();
    if(alloc) {
      vertexproperty.AllocatePartitioned(A.m, tiles_per_dim, GraphPad::vector_partition_fn);
      V *__v = new V;
      vertexproperty.setAll(*__v);
      delete __v;
      active.AllocatePartitioned(A.m, tiles_per_dim, GraphPad::vector_partition_fn);
      active.setAll(false);
    } else {
      //vertexproperty = NULL; 
      //active = NULL;
    }

    nvertices = m_;
    vertexpropertyowner = 1;
  }
  gettimeofday(&end, 0);
  std::cout << "Finished GraphPad read + construction, time: " << sec(start,end)  << std::endl;


}



template<class V, class E>
void Graph<V,E>::ReadMTX_sort(const char* filename, int grid_size, int alloc) {

  if (GraphPad::global_nrank == 1) {
    vertexID_randomization = false;
  } else {
    vertexID_randomization = true;
  }

  struct timeval start, end;
  gettimeofday(&start, 0);
  {
    GraphPad::edgelist_t<E> A_edges;
    GraphPad::ReadEdgesBin(&A_edges, filename, false);
    tiles_per_dim = GraphPad::global_nrank;
    
    #pragma omp parallel for
    for(int i = 0 ; i < A_edges.nnz ; i++)
    {
      A_edges.edges[i].src = vertexToNative(A_edges.edges[i].src, tiles_per_dim, A_edges.m);
      A_edges.edges[i].dst = vertexToNative(A_edges.edges[i].dst, tiles_per_dim, A_edges.m);
    }

    GraphPad::AssignSpMat(A_edges, &A, tiles_per_dim, tiles_per_dim, GraphPad::partition_fn_2d);
    GraphPad::Transpose(A, &AT, tiles_per_dim, tiles_per_dim, GraphPad::partition_fn_2d);

    int m_ = A.m;
    assert(A.m == A.n);
    nnz = A.getNNZ();
    if(alloc) {
      vertexproperty.AllocatePartitioned(A.m, tiles_per_dim, GraphPad::vector_partition_fn);
      V *__v = new V;
      vertexproperty.setAll(*__v);
      delete __v;
      active.AllocatePartitioned(A.m, tiles_per_dim, GraphPad::vector_partition_fn);
      active.setAll(false);
    } else {
      //vertexproperty = NULL; 
      //active = NULL;
    }

    nvertices = m_;
    vertexpropertyowner = 1;
  }
  gettimeofday(&end, 0);
  std::cout << "Finished GraphPad read + construction, time: " << sec(start,end)  << std::endl;


}

template<class V, class E> 
void Graph<V,E>::setAllActive() {
  //for (int i = 0; i <= nvertices; i++) {
  //  active[i] = true;
  //}
  //memset(active, 0xff, sizeof(bool)*(nvertices));
  //GraphPad::Apply(active, &active, set_all_true);
  active.setAll(true);
}

template<class V, class E> 
void Graph<V,E>::setAllInactive() {
  //memset(active, 0x0, sizeof(bool)*(nvertices));
  //GraphPad::Apply(active, &active, set_all_false);
  active.setAll(false);
  //GraphPad::Clear(&active);
  for(int segmentId = 0 ; segmentId < active.nsegments ; segmentId++)
  {
    if(active.nodeIds[segmentId] == GraphPad::global_myrank)
    {
      GraphPad::DenseSegment<bool>* s1 = &(active.segments[segmentId]);
      GraphPad::clear_dense_segment(s1->properties.value, s1->properties.bit_vector, s1->num_ints);
    }
  }
}

template<class V, class E> 
void Graph<V,E>::setActive(int v) {
  //active[v] = true;
  int v_new = vertexToNative(v, tiles_per_dim, nvertices);
  active.set(v_new, true);
}

template<class V, class E> 
void Graph<V,E>::setInactive(int v) {
  //active[v] = false;
  int v_new = vertexToNative(v, tiles_per_dim, nvertices);
  active.set(v_new, false);
}
template<class V, class E> 
void Graph<V,E>::reset() {
  //memset(active, 0, sizeof(bool)*(nvertices));
  setAllInactive();
  V v;
  vertexproperty.setAll(v);
  //#pragma omp parallel for num_threads(nthreads)
  //for (int i = 0; i < nvertices; i++) {
  //  V v;
  //  vertexproperty[i] = v;
  //}
}

template<class V, class E> 
void Graph<V,E>::shareVertexProperty(Graph<V,E>& g) {
  //delete [] vertexproperty;
  vertexproperty = g.vertexproperty;
  vertexpropertyowner = 0;
}

template<class V, class E> 
void Graph<V,E>::setAllVertexproperty(const V& val) {
  //#pragma omp parallel for num_threads(nthreads)
  //for (int i = 0; i < nvertices; i++) {
  //  vertexproperty[i] = val;
  //}
  vertexproperty.setAll(val);
}

template<class V, class E> 
void Graph<V,E>::setVertexproperty(int v, const V& val) {
  //vertexproperty[v] = val;
  int v_new = vertexToNative(v, tiles_per_dim, nvertices);
  vertexproperty.set(v_new, val);
}

template<class V, class E> 
void Graph<V,E>::saveVertexproperty(std::string fname) const {
  GraphPad::edgelist_t<V> myedges;
  vertexproperty.get_edges(&myedges);
  for(unsigned int i = 0 ; i < myedges.nnz ; i++)
  {
    myedges.edges[i].src = nativeToVertex(myedges.edges[i].src, tiles_per_dim, nvertices);
  }
  GraphPad::SpVec<GraphPad::DenseSegment<V> > vertexproperty2;
  vertexproperty2.AllocatePartitioned(nvertices, tiles_per_dim, GraphPad::vector_partition_fn);
  vertexproperty2.ingestEdgelist(myedges);
  _mm_free(myedges.edges);
  vertexproperty2.save(fname);
}


template<class V, class E> 
void Graph<V,E>::saveVertexpropertyBin(std::string fname) const {
  GraphPad::edgelist_t<V> myedges;
  vertexproperty.get_edges(&myedges);
  for(unsigned int i = 0 ; i < myedges.nnz ; i++)
  {
    myedges.edges[i].src = nativeToVertex(myedges.edges[i].src, tiles_per_dim, nvertices);
  }
  GraphPad::SpVec<GraphPad::DenseSegment<V> > vertexproperty2;
  vertexproperty2.AllocatePartitioned(nvertices, tiles_per_dim, GraphPad::vector_partition_fn);
  vertexproperty2.ingestEdgelist(myedges);
  _mm_free(myedges.edges);
  vertexproperty2.saveBin(fname);
}


template<class V, class E> 
void Graph<V,E>::saveVertexpropertyBinHdfs(std::string fname) const {
  GraphPad::edgelist_t<V> myedges;
  vertexproperty.get_edges(&myedges);
  for(unsigned int i = 0 ; i < myedges.nnz ; i++)
  {
    myedges.edges[i].src = nativeToVertex(myedges.edges[i].src, tiles_per_dim, nvertices);
  }
  GraphPad::SpVec<GraphPad::DenseSegment<V> > vertexproperty2;
  vertexproperty2.AllocatePartitioned(nvertices, tiles_per_dim, GraphPad::vector_partition_fn);
  vertexproperty2.ingestEdgelist(myedges);
  _mm_free(myedges.edges);
  vertexproperty2.saveBinHdfs(fname);
}

template<class V, class E>
bool Graph<V,E>::vertexNodeOwner(const int v) const {
  int v_new = vertexToNative(v, tiles_per_dim, nvertices);
  return vertexproperty.node_owner(v_new);
}

template<class V, class E> 
V Graph<V,E>::getVertexproperty(const int v) const {
  //return vertexproperty[v];
  V vp ;
  int v_new = vertexToNative(v, tiles_per_dim, nvertices);
  vertexproperty.get(v_new, &vp);
  return vp;
}

int getId(const int i, const int* start, const int* end, const int n) {
  for (int j = 0; j < n; j++) {
    if (i >= start[j] && i <= end[j]) {
      return j;
    }
  }
}

template<class V, class E> 
int Graph<V,E>::getBlockIdBySrc(int vertexId) const {
  return getId(vertexId, start_src_vertices, end_src_vertices, nparts);
}

template<class V, class E> 
int Graph<V,E>::getBlockIdByDst(int vertexId) const {
  return getId(vertexId, start_dst_vertices, end_dst_vertices, nparts);
}

template<class V, class E> 
int Graph<V,E>::getNumberOfVertices() const {
  return nvertices;
}

template<class V, class E> 
void Graph<V,E>::applyToAllVertices( void (*ApplyFn)(V, V*, void*), void* param) {
  GraphPad::Apply(vertexproperty, &vertexproperty, ApplyFn, param);
}


template<class V, class E> 
template<class T> 
void Graph<V,E>::applyReduceAllVertices(T* val, void (*ApplyFn)(V*, T*, void*), void (*ReduceFn)(T,T,T*,void*), void* param) {
  GraphPad::MapReduce(&vertexproperty, val, ApplyFn, ReduceFn, param);
}

template<class V, class E> 
Graph<V,E>::~Graph() {
  if (vertexpropertyowner) {
    //if(vertexproperty) delete [] vertexproperty;
  }
  //if (active) delete [] active;
  //if (id) delete [] id;
  //if (start_src_vertices) delete [] start_src_vertices;
  //if (end_src_vertices) delete [] end_src_vertices;

}

