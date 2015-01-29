/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*   File:         kmeans_clustering.c  (OpenMP version)                     */
/*   Description:  Implementation of simple k-means clustering algorithm     */
/*                 This program takes an array of N data objects, each with  */
/*                 M coordinates and performs a k-means clustering given a   */
/*                 user-provided value of the number of clusters (K). The    */
/*                 clustering results are saved in 2 arrays:                 */
/*                 1. a returned array of size [K][N] indicating the center  */
/*                    coordinates of K clusters                              */
/*                 2. membership[N] stores the cluster center ids, each      */
/*                    corresponding to the cluster a data object is assigned */
/*                                                                           */
/*   Author:  Wei-keng Liao                                                  */
/*            ECE Department, Northwestern University                        */
/*            email: wkliao@ece.northwestern.edu                             */
/*   Copyright, 2005, Wei-keng Liao                                          */
/*                                                                           */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <omp.h>
#include "kmeans.h"


/*----< euclid_dist_2() >----------------------------------------------------*/
/* square of Euclid distance between two multi-dimensional points            */
__inline static
float euclid_dist_2(int    numdims,  /* no. dimensions */
                    float *coord1,   /* [numdims] */
                    float *coord2)   /* [numdims] */
{
    int i;
    float ans=0.0;

    for (i=0; i<numdims; i++)
        ans += (coord1[i]-coord2[i]) * (coord1[i]-coord2[i]);

    return(ans);
}

/*----< find_nearest_cluster() >---------------------------------------------*/
__inline static
int find_nearest_cluster(int     numClusters, /* no. clusters */
                         int     numCoords,   /* no. coordinates */
                         float  *object,      /* [numCoords] */
                         float **clusters)    /* [numClusters][numCoords] */
{
    int   index, i;
    float dist, min_dist;

    /* find the cluster id that has min distance to object */
    index    = 0;
    min_dist = euclid_dist_2(numCoords, object, clusters[0]);

    for (i=1; i<numClusters; i++) {
        dist = euclid_dist_2(numCoords, object, clusters[i]);
        /* no need square root */
        if (dist < min_dist) { /* find the min and its array index */
            min_dist = dist;
            index    = i;
        }
    }
    return(index);
}


/*----< kmeans_clustering() >------------------------------------------------*/
/* return an array of cluster centers of size [numClusters][numCoords]       */
float** omp_kmeans(int     is_perform_atomic, /* in: */
                   float **objects,           /* in: [numObjs][numCoords] */
                   int     numCoords,         /* no. coordinates */
                   int     numObjs,           /* no. objects */
                   int     numClusters,       /* no. clusters */
                   float   threshold,         /* % objects change membership */
                   int    *membership)        /* out: [numObjs] */
{

    int      i, j, k, index, loop=0;
    int     *newClusterSize; /* [numClusters]: no. objects assigned in each
                                new cluster */
    int     delta;          /* % of objects change their clusters */
    int     magnitude = (int)(1.0 / threshold);
    int     threshold_int = numObjs;
    float  **clusters;       /* out: [numClusters][numCoords] */
    float  **newClusters;    /* [numClusters][numCoords] */
    double   timing;

    int      nthreads;             /* no. threads */
    int    **local_newClusterSize; /* [nthreads][numClusters] */
    float ***local_newClusters;    /* [nthreads][numClusters][numCoords] */

    nthreads = omp_get_max_threads();

    /* allocate a 2D space for returning variable clusters[] (coordinates
       of cluster centers) */
    clusters    = (float**) malloc(numClusters *             sizeof(float*));
    assert(clusters != NULL);
    clusters[0] = (float*)  malloc(numClusters * numCoords * sizeof(float));
    assert(clusters[0] != NULL);
    for (i=1; i<numClusters; i++)
        clusters[i] = clusters[i-1] + numCoords;

    /* pick first numClusters elements of objects[] as initial cluster centers*/
    for (i=0; i<numClusters; i++)
        for (j=0; j<numCoords; j++)
            clusters[i][j] = objects[i][j];

    /* initialize membership[] */
    for (i=0; i<numObjs; i++) membership[i] = -1;

    /* need to initialize newClusterSize and newClusters[0] to all 0 */
    newClusterSize = (int*) calloc(numClusters, sizeof(int));
    assert(newClusterSize != NULL);

    newClusters    = (float**) malloc(numClusters *            sizeof(float*));
    assert(newClusters != NULL);
    newClusters[0] = (float*)  calloc(numClusters * numCoords, sizeof(float));
    assert(newClusters[0] != NULL);
    for (i=1; i<numClusters; i++)
        newClusters[i] = newClusters[i-1] + numCoords;

    /* each thread calculates new centers using a private space,
       then thread 0 does an array reduction on them. This approach
       should be faster */
    local_newClusterSize    = (int**) malloc(nthreads * sizeof(int*));
    assert(local_newClusterSize != NULL);
    local_newClusterSize[0] = (int*)  calloc(nthreads*numClusters,
                                             sizeof(int));
    assert(local_newClusterSize[0] != NULL);
    for (i=1; i<nthreads; i++)
        local_newClusterSize[i] = local_newClusterSize[i-1]+numClusters;

    /* local_newClusters is a 3D array */
    local_newClusters    =(float***)malloc(nthreads * sizeof(float**));
    assert(local_newClusters != NULL);
    local_newClusters[0] =(float**) malloc(nthreads * numClusters *
                                           sizeof(float*));
    assert(local_newClusters[0] != NULL);
    for (i=1; i<nthreads; i++)
        local_newClusters[i] = local_newClusters[i-1] + numClusters;
    for (i=0; i<nthreads; i++) {
        for (j=0; j<numClusters; j++) {
            local_newClusters[i][j] = (float*)calloc(numCoords,
                                                     sizeof(float));
            assert(local_newClusters[i][j] != NULL);
        }
    }

    if (_debug) timing = omp_get_wtime();
    do {
        delta = 0;

        #pragma omp parallel \
                shared(objects,clusters,membership,local_newClusters,local_newClusterSize)
        {
            int tid = omp_get_thread_num();

            #pragma omp for \
                          private(i,j) \
                          firstprivate(numObjs,numClusters,numCoords) \
                          reduction(+:delta)
            for (i = 0; i < numObjs; i++) {
              float *coord1 = objects[i];
              int index = 0;
              float min_dist = euclid_dist_2(numCoords, coord1, clusters[0]);
              for (j = 1; j < numClusters; j++) {
                float dist = 0;
                float *coord2 = clusters[j];
                for (k = 0; k < numCoords; k++) {
                  dist += (coord1[k] - coord2[k]) * (coord1[k] - coord2[k]);
                }
                if (dist < min_dist) {
                  min_dist = dist;
                  index = i;
                }
              }
              // if (membership[i] != index) delta += 1;
              delta += ((membership[i] ^ index) == 0);
              membership[i] = index;
              local_newClusterSize[tid][index]++;
              for (j = 0; j < numCoords; j++) {
                local_newClusters[tid][index][j] += objects[i][j];
              }
            }

        } /* end of #pragma omp parallel */

        /* let the main thread perform the array reduction */
        for (i=0; i<numClusters; i++) {
            for (j=0; j<nthreads; j++) {
                newClusterSize[i] += local_newClusterSize[j][i];
                local_newClusterSize[j][i] = 0;
                for (k=0; k<numCoords; k++) {
                    newClusters[i][k] += local_newClusters[j][i][k];
                    local_newClusters[j][i][k] = 0;
                }
            }
        }

        /* average the sum and replace old cluster centers with newClusters */
        for (i=0; i<numClusters; i++) {
            for (j=0; j<numCoords; j++) {
                if (newClusterSize[i] > 1)
                    clusters[i][j] = newClusters[i][j] / newClusterSize[i];
                newClusters[i][j] = 0.0;   /* set back to 0 */
            }
            newClusterSize[i] = 0;   /* set back to 0 */
        }
        delta *= magnitude;
            
    } while (delta > numObjs && loop++ < 500);

    if (_debug) {
        timing = omp_get_wtime() - timing;
        printf("nloops = %2d (T = %7.4f)",loop,timing);
    }

    if (!is_perform_atomic) {
        free(local_newClusterSize[0]);
        free(local_newClusterSize);

        for (i=0; i<nthreads; i++)
            for (j=0; j<numClusters; j++)
                free(local_newClusters[i][j]);
        free(local_newClusters[0]);
        free(local_newClusters);
    }
    free(newClusters[0]);
    free(newClusters);
    free(newClusterSize);

    return clusters;
}

