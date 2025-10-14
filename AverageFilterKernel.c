#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>

#define WORK_REQUEST 1
#define WORK_ASSIGNMENT 2
#define RESULT 3
#define TERMINATE 4

typedef struct PGM
{
  char type[3];
  unsigned char **data;
  unsigned int width;
  unsigned int height;
  unsigned int grayvalue;
} PGM;

void ignoreComments(FILE *fp)
{
  int ch;
  char line[100];
  // Ignore any blank lines
  while ((ch = fgetc(fp)) != EOF && isspace(ch))
    ;
  // Recursively ignore comments in a PGM image
  // commented lines start with a '#'
  if (ch == '#')
  {
    fgets(line, sizeof(line), fp);
    // To get cursor to next line
    ignoreComments(fp);
  }
  // check if anymore comments available
  else
  {
    fseek(fp, -1, SEEK_CUR);
  }
  // Beginning of current line
}

int openPGM(PGM *pgm, char fname[])
{
  FILE *pgmfile = fopen(fname, "rb");
  if (pgmfile == NULL)
  {
    printf("File does not exist\n");
    return 0;
  }

  ignoreComments(pgmfile);
  fscanf(pgmfile, "%s", pgm->type);
  ignoreComments(pgmfile);

  fscanf(pgmfile, "%d %d", &(pgm->width),
         &(pgm->height));
  ignoreComments(pgmfile);

  fscanf(pgmfile, "%d", &(pgm->grayvalue));
  ignoreComments(pgmfile);

  pgm->data = malloc(pgm->height * sizeof(unsigned char *));
  for (int i = 0; i < pgm->height; i++)
  {
    pgm->data[i] = malloc(pgm->width *
                          sizeof(unsigned char));
    fread(pgm->data[i], pgm->width * sizeof(unsigned char), 1, pgmfile);
  }

  fclose(pgmfile);
  return 1;
}

void printImageDetails(PGM *pgm, char filename[])
{
  FILE *pgmfile = fopen(filename, "rb");

  // searches the occurrence of '.'
  char *ext = strrchr(filename, '.');

  if (!ext)
    printf("No extension found in file %s", filename);
  else // portion after .
    printf("File format     : %s\n", ext + 1);

  printf("PGM File type   : %s\n", pgm->type);
  printf("Width of img    : %d px\n", pgm->width);
  printf("Height of img   : %d px\n", pgm->height);
  printf("Max Gray value  : %d\n", pgm->grayvalue);

  fclose(pgmfile);
}
void saveImage(PGM *pgm, char fname[])
{
  FILE *fp = fopen(fname, "wb");

  fprintf(fp, "%s\n", pgm->type);
  fprintf(fp, "%d %d\n", pgm->width, pgm->height);
  fprintf(fp, "%d\n", pgm->grayvalue);

  for (int i = 0; i < pgm->height; i++)
  {
    for (int j = 0; j < pgm->width; j++)
    {
      fprintf(fp, "%c", pgm->data[i][j]);
    }
  }

  fclose(fp);
}

void filterLine(PGM *pgm, int line, int filt_size, unsigned char *output)
{
  int s = filt_size / 2;
  
  for (int j = 0; j < pgm->width; j++)
  {
    int d = 0;
    int e = 0;
    for (int k = -s; k <= s; k++)
    {
      for (int l = -s; l <= s; l++)
      {
        if (line + k >= 0 && j + l >= 0 && line + k < pgm->height && j + l < pgm->width)
        {
          d++;
          e = e + (int)pgm->data[line + k][j + l];
        }
      }
    }
    e = e / d;
    output[j] = (unsigned char)e;
  }
}

void master(PGM *pgm, int filt_size, int num_procs)
{
  PGM *filtered = malloc(sizeof(PGM));
  strcpy(filtered->type, pgm->type);
  filtered->height = pgm->height;
  filtered->width = pgm->width;
  filtered->grayvalue = pgm->grayvalue;
  filtered->data = malloc(pgm->height * sizeof(unsigned char *));
  for (int i = 0; i < pgm->height; i++)
  {
    filtered->data[i] = malloc(pgm->width * sizeof(unsigned char));
  }

  int next_line = 0;
  int completed_lines = 0;
  int total_lines = pgm->height;
  int active_slaves = 0;
  int terminated_slaves = 0;
  int total_slaves = num_procs - 1;
  
  // Main loop: handle work requests and results until all slaves are terminated
  while (terminated_slaves < total_slaves)
  {
    MPI_Status status;
    int buffer;
    
    // Probe for any incoming message
    MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
    
    if (status.MPI_TAG == WORK_REQUEST)
    {
      // Receive work request
      MPI_Recv(&buffer, 1, MPI_INT, status.MPI_SOURCE, WORK_REQUEST, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
      
      if (next_line < total_lines)
      {
        // Send line number to slave
        MPI_Send(&next_line, 1, MPI_INT, status.MPI_SOURCE, WORK_ASSIGNMENT, MPI_COMM_WORLD);
        next_line++;
        active_slaves++;
      }
      else
      {
        // No more work, send terminate signal
        int terminate = -1;
        MPI_Send(&terminate, 1, MPI_INT, status.MPI_SOURCE, TERMINATE, MPI_COMM_WORLD);
        terminated_slaves++;
      }
    }
    else if (status.MPI_TAG == RESULT)
    {
      // Receive result: line number first
      int line_num;
      MPI_Recv(&line_num, 1, MPI_INT, status.MPI_SOURCE, RESULT, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
      
      // Then receive the filtered line data
      MPI_Recv(filtered->data[line_num], pgm->width, MPI_UNSIGNED_CHAR, 
               status.MPI_SOURCE, RESULT, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
      
      completed_lines++;
      active_slaves--;
    }
  }
  
  saveImage(filtered, "filtered.pgm");
  printf("The image file has been filtered\n");
  
  // Free filtered image
  for (int i = 0; i < filtered->height; i++)
  {
    free(filtered->data[i]);
  }
  free(filtered->data);
  free(filtered);
}

void slave(PGM *pgm, int filt_size)
{
  unsigned char *output_line = malloc(pgm->width * sizeof(unsigned char));
  
  while (1)
  {
    // Request work from master
    int dummy = 0;
    MPI_Send(&dummy, 1, MPI_INT, 0, WORK_REQUEST, MPI_COMM_WORLD);
    
    // Receive assignment
    MPI_Status status;
    int line_num;
    MPI_Recv(&line_num, 1, MPI_INT, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
    
    // Check if terminate signal
    if (status.MPI_TAG == TERMINATE)
    {
      break;
    }
    
    // Process the line
    filterLine(pgm, line_num, filt_size, output_line);
    
    // Send result back to master
    MPI_Send(&line_num, 1, MPI_INT, 0, RESULT, MPI_COMM_WORLD);
    MPI_Send(output_line, pgm->width, MPI_UNSIGNED_CHAR, 0, RESULT, MPI_COMM_WORLD);
  }
  
  free(output_line);
}


int main(int argc, char *argv[])
{
  int rank, num_procs;
  
  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &num_procs);

  if (argc != 3)
  {
    if (rank == 0)
    {
      printf("Usage: %s <input_file> <filter_size>\n", argv[0]);
    }
    MPI_Finalize();
    exit(EXIT_FAILURE);
  }

  PGM *pgm = malloc(sizeof(PGM));
  char *ch = argv[1];
  int choice = atoi(argv[2]);
  
  // Master loads the image and broadcasts metadata
  if (rank == 0)
  {
    if (!openPGM(pgm, ch))
    {
      printf("Failed to open image\n");
      MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
    }
    printImageDetails(pgm, ch);
    
    if (!(choice >= 1))
    {
      printf("Wrong size for the filter window\n");
      MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
    }
  }
  
  // Broadcast image metadata
  MPI_Bcast(&pgm->width, 1, MPI_UNSIGNED, 0, MPI_COMM_WORLD);
  MPI_Bcast(&pgm->height, 1, MPI_UNSIGNED, 0, MPI_COMM_WORLD);
  MPI_Bcast(&pgm->grayvalue, 1, MPI_UNSIGNED, 0, MPI_COMM_WORLD);
  MPI_Bcast(pgm->type, 3, MPI_CHAR, 0, MPI_COMM_WORLD);
  
  // Allocate memory for image data in slaves
  if (rank != 0)
  {
    pgm->data = malloc(pgm->height * sizeof(unsigned char *));
    for (int i = 0; i < pgm->height; i++)
    {
      pgm->data[i] = malloc(pgm->width * sizeof(unsigned char));
    }
  }
  
  // Broadcast image data line by line
  for (int i = 0; i < pgm->height; i++)
  {
    MPI_Bcast(pgm->data[i], pgm->width, MPI_UNSIGNED_CHAR, 0, MPI_COMM_WORLD);
  }
  
  // Execute master or slave logic
  if (rank == 0)
  {
    master(pgm, choice, num_procs);
  }
  else
  {
    slave(pgm, choice);
  }
  
  // Free memory
  for (int i = 0; i < pgm->height; i++)
  {
    free(pgm->data[i]);
  }
  free(pgm->data);
  free(pgm);
  
  MPI_Finalize();
  return 0;
}
