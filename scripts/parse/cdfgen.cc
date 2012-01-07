#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define MAX_NUM_BINS 10000
#define noDEBUG

void usage()
{
  printf("./cdfgen <filename> <bin size> <column> <output file>\n");
  exit(1);
}

double tot_value = 0;
int num_samples = 0;
double bin_size = 0;
int CDF[MAX_NUM_BINS+1];
int column;

void parsefile(FILE *fp)
{
  char buf[1024]; 
  char tmp[1024]; 
  
  struct token_struct{
    char buffer[100];
  };
  struct token_struct token[100];
  char *temp;

  double curr_value;
  int count;

  while (fgets(buf, 1024, fp) != NULL) 
    {
      for(int z=0; z<100;z++)
	bzero(&token[z].buffer,sizeof(token[z].buffer));
	  
      strcpy(tmp,buf);
      count =0;
      temp = strtok(buf," :,\t");
	  
      while(temp != NULL)
	{
	  bzero(&token[count].buffer,sizeof(token[count].buffer));
	  strcpy(token[count].buffer, temp);
	  count++;
	  temp = strtok(NULL," :,\t");
	}

      curr_value = atof(token[column].buffer);

      if (curr_value < 0) {
	continue;
	//curr_value *= -1;
      }
      
      if(curr_value > 0) {
	tot_value+= curr_value;
	num_samples++;
      }
#ifdef DEBUG
      printf("curr_value is %f\n",curr_value);
#endif
      int index;
      double quot;
      
      index = (int) (curr_value/bin_size);
      quot = curr_value/bin_size;
            
      if(quot != (double) index)
      {
	  //there is a remainder
#ifdef DEBUG      
	  printf("index is  is %d\n",index);
#endif
	  index = index+1;
      }
      
      if(index>=MAX_NUM_BINS)
	{
#ifdef DEBUG
	  printf("INDEX %d\n",index);
#endif
	  index = MAX_NUM_BINS;
	}
      
      CDF[index]++;
    }
}


main(int argc,char *argv[])
{
  FILE *fp;
  char output[1024];
  char filename[1024];

  if(argc != 5)
    {
      usage();
      exit(1);
    }

  for(int i =0; i<MAX_NUM_BINS +1 ;i++)
    {
      CDF[i]=0;
    }

  printf("File to be analysed %s\n", argv[1]);
  
  bin_size = atof(argv[2]);
  column = atoi(argv[3]);
  //printf("Max O/H %d\n",max_oh);

  //  bin_size = max_oh/NUM_BINS;  
  printf("bin_size is %f\n",bin_size);

  bzero(filename,sizeof(filename));
  strcpy(filename,argv[1]);

  fp = fopen(filename, "r");
  if(fp == NULL)
    {
      printf("could not open trace file\n");
      exit(1);
    }
  parsefile(fp);
  fclose(fp);

  fp = fopen(argv[4],"w");

  int cdfvalue = 0;
  
  for(int i=0; i<MAX_NUM_BINS +1 ;i++) {
    cdfvalue+=CDF[i];
  }

  int m=0;
  for(int i=0;i<MAX_NUM_BINS +1 ;i++) {
      m+=CDF[i];
      printf("%.2f %.2f\n",i*bin_size,((float)m/(float)cdfvalue)*100);
      fprintf(fp,"%.2f %.2f\n",i*bin_size,((float)m/(float)cdfvalue)*100);
  }

  printf("average value is %f\n",tot_value/num_samples);

  fclose(fp);
  printf("---------------------------------------------------------------------------\n");  
}
