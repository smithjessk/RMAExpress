/******************************************************************
 **
 ** file: read_mps.cpp
 ** 
 ** Aim: implement parsing of MPS format files
 **
 ** Copyright (C) 2008    B. M. Bolstad
 **
 ** Created on Mar 17, 2008
 **
 ** History
 ** Mar 17, 2008 - Initial version
 ** June 24, 2008 - change char * to const char * where appropriate 
 ** May 19, 2009 - Fix how level0 lines are parsed
 ** 
 ** 
 ******************************************************************/
#include <cstdio>
#include <cstdlib>
#include <cstring> 
#include <wx/dynarray.h>
#include <wx/arrstr.h>
#define BUFFERSIZE 4096  //1024

#include <wx/string.h>


#include <vector>
#include <algorithm>


#include "read_mps.h"

using namespace std;


static void error(const char *msg, const char *msg2="", const char *msg3=""){
  wxString Error = wxString((const char*)msg,wxConvUTF8) +_T(" ") + wxString((const char*)msg2,wxConvUTF8)  +_T(" ") + wxString((const char*)msg3,wxConvUTF8)+ _T("\n");
  throw Error;
}

/*******************************************************************
 *******************************************************************
 **
 ** Structures for dealing with pgf file information
 **
 **
 **
 *******************************************************************
 ******************************************************************/

/*******************************************************************
 *******************************************************************
 **
 ** Starting off with the headers
 **
 *******************************************************************
 ******************************************************************/

/* integer (from 0 to n-1) indicates position of header (-1 means header is not present) */

typedef struct{
  int probeset_id;
  int transcript_cluster_id;
  int probeset_list;
  int probe_count;

} header_0;


/*

  #%chip_type=HuEx-1_0-st-v2
  #%chip_type=HuEx-1_0-st-v1
  #%chip_type=HuEx-1_0-st-ta1
  #%lib_set_name=HuEx-1_0-st
  #%lib_set_version=r2
  #%create_date=Tue Sep 19 15:18:05 PDT 2006
  #%genome-species=Homo sapiens
  #%genome-version=hg18
  #%genome-version-ucsc=hg18
  #%genome-version-ncbi=36
  #%genome-version-create_date=2006 March
  #%guid=0000008635-1158704285-1969988809-0053097456-1768345064

*/

typedef struct{
  char **chip_type;
  int n_chip_type;
  char *lib_set_name;
  char *lib_set_version;
  char *header0_str;
  header_0 *header0;
  char *create_date;
  char *guid;
  char **other_headers_keys;
  char **other_headers_values;
  int n_other_headers;
} mps_headers;



/*******************************************************************
 *******************************************************************
 **
 ** Structure for holding the data stored in the file 
 ** in this case we ignore anything that is not a probeset id
 **
 *******************************************************************
 *******************************************************************/

typedef struct{
  std::vector<int> probeset_id;
  std::vector<int> transcript_cluster_id;
  std::vector<std::vector<int> > probeset_list;   
  std::vector<int> probe_count;
} mps_data;



/*******************************************************************
 *******************************************************************
 **
 ** Structure for storing pgf file (after it is read from file)
 **
 *******************************************************************
 ******************************************************************/

struct mps_file{
  mps_headers *headers;
  mps_data *probesets;
};

/*******************************************************************
 *******************************************************************
 **
 **
 ** Code for splitting a string into a series of tokens
 **
 **
 *******************************************************************
 *******************************************************************/


/***************************************************************
 **
 ** tokenset
 ** 
 ** char **tokens  - a array of token strings
 ** int n - number of tokens in this set.
 **
 ** a structure to hold a set of tokens. Typically a tokenset is
 ** created by breaking a character string based upon a set of 
 ** delimiters.
 **
 **
 **************************************************************/

typedef struct{
  char **tokens;
  int n;
} tokenset;



/******************************************************************
 **
 ** tokenset *tokenize(char *str, char *delimiters)
 **
 ** char *str - a string to break into tokens
 ** char *delimiters - delimiters to use in breaking up the line
 **
 **
 ** RETURNS a new tokenset
 **
 ** Given a string, split into tokens based on a set of delimitors
 **
 *****************************************************************/

static tokenset *tokenize(char *str, const char *delimiters){

#if USE_PTHREADS  
  char *tmp_pointer;
#endif  
  int i=0;

  char *current_token;
  tokenset *my_tokenset = (tokenset *)calloc(1,sizeof(tokenset));
  my_tokenset->n=0;
  
  my_tokenset->tokens = NULL;
#if USE_PTHREADS
  current_token = strtok_r(str,delimiters,&tmp_pointer);
#else
  current_token = strtok(str,delimiters);
#endif
  while (current_token != NULL){
    my_tokenset->n++;
    my_tokenset->tokens = (char **)realloc(my_tokenset->tokens,(my_tokenset->n)*sizeof(char*));
    my_tokenset->tokens[i] = (char *)calloc(strlen(current_token)+1,sizeof(char));
    strcpy(my_tokenset->tokens[i],current_token);
    my_tokenset->tokens[i][(strlen(current_token))] = '\0';
    i++;
#if USE_PTHREADS
    current_token = strtok_r(NULL,delimiters,&tmp_pointer);
#else
    current_token = strtok(NULL,delimiters);
#endif
  }
  return my_tokenset; 
}


/******************************************************************
 **
 ** int tokenset_size(tokenset *x)
 **
 ** tokenset *x - a tokenset
 ** 
 ** RETURNS the number of tokens in the tokenset 
 **
 ******************************************************************/

static int tokenset_size(tokenset *x){
  return x->n;
}


/******************************************************************
 **
 ** char *get_token(tokenset *x, int i)
 **
 ** tokenset *x - a tokenset
 ** int i - index of the token to return
 ** 
 ** RETURNS pointer to the i'th token
 **
 ******************************************************************/

static char *get_token(tokenset *x,int i){
  return x->tokens[i];
}

/******************************************************************
 **
 ** void delete_tokens(tokenset *x)
 **
 ** tokenset *x - a tokenset
 ** 
 ** Deallocates all the space allocated for a tokenset 
 **
 ******************************************************************/

static void delete_tokens(tokenset *x){
  
  int i;

  for (i=0; i < x->n; i++){
    free(x->tokens[i]);
  }
  free(x->tokens);
  free(x);
}

/*******************************************************************
 **
 ** int token_ends_with(char *token, char *ends)
 ** 
 ** char *token  -  a string to check
 ** char *ends_in   - we are looking for this string at the end of token
 **
 **
 ** returns  0 if no match, otherwise it returns the index of the first character
 ** which matchs the start of *ends.
 **
 ** Note that there must be one additional character in "token" beyond 
 ** the characters in "ends". So
 **
 **  *token = "TestStr"
 **  *ends = "TestStr"   
 **  
 ** would return 0 but if 
 **
 ** ends = "estStr"
 **
 ** we would return 1.
 **
 ** and if 
 ** 
 ** ends= "stStr"
 ** we would return 2 .....etc
 **
 **
 ******************************************************************/

static int token_ends_with(char *token, const char *ends_in){
  
  int tokenlength = strlen(token);
  int ends_length = strlen(ends_in);
  int start_pos;
  char *tmp_ptr;
  
  if (tokenlength <= ends_length){
    /* token string is too short so can't possibly end with ends */
    return 0;
  }
  
  start_pos = tokenlength - ends_length;
  
  tmp_ptr = &token[start_pos];

  if (strcmp(tmp_ptr,ends_in)==0){
    return start_pos;
  } else {
    return 0;
  }
}


/*******************************************************************
 *******************************************************************
 **
 ** Code for Reading from file
 **
 *******************************************************************
 *******************************************************************/



/****************************************************************
 **
 ** void ReadFileLine(char *buffer, int buffersize, FILE *currentFile)
 **
 ** char *buffer  - place to store contents of the line
 ** int buffersize - size of the buffer
 ** FILE *currentFile - FILE pointer to an opened CEL file.
 **
 ** Read a line from a file, into a buffer of specified size.
 ** otherwise die.
 **
 ***************************************************************/

static int ReadFileLine(char *buffer, int buffersize, FILE *currentFile){

  if (fgets(buffer, buffersize, currentFile) == NULL){
    return 0;
    //error("End of file reached unexpectedly. Perhaps this file is truncated.\n");
  }  
  
  return 1;
}  




/****************************************************************
 **
 ** Code for identifying what type of information is stored in 
 ** the current line
 **
 ***************************************************************/

/****************************************************************
 **
 ** static int IsHeaderLine(char *buffer)
 **
 ** char *buffer - contains line to evaluate
 **
 ** Checks whether supplied line is a header line (ie starts with #%)
 **
 ** return 1 (ie true) if header line. 0 otherwise
 **
 ***************************************************************/


static int IsHeaderLine(char *buffer){

  if (strncmp("#%",buffer,2) == 0){
    return 1;
  }
  return 0;
}

/****************************************************************
 **
 ** static int IsHeaderLine(char *buffer)
 **
 ** char *buffer - contains line to evaluate
 **
 ** Checks whether supplied line is a comment line (ie starts with #)
 **
 **
 ***************************************************************/

static int IsCommentLine(char *buffer){
  if (strncmp("#",buffer,1) == 0){
    return 1;
  }
  return 0;
}


/*****************************************************************
 **
 ** static int IsLevel2(char *buffer)
 **
 ** char *buffer - contains line to evaluate
 **
 ** checks whether supplied line begins with two tab characters it \t\t
 **
 ** Return 1 if true, 0 otherwise
 **
 ***************************************************************/

static int IsLevel2(char *buffer){
  if (strncmp("\t\t",buffer,2) == 0){
    return 1;
  }
  return 0;
}



/*****************************************************************
 **
 ** static int IsLevel1(char *buffer)
 **
 ** char *buffer - contains line to evaluate
 **
 ** checks whether supplied line begins with a single tab characters it \t
 **
 ** Return 1 if true, 0 otherwise
 **
 ***************************************************************/

static int IsLevel1(char *buffer){
  if (strncmp("\t",buffer,1) == 0){
    if (strncmp("\t\t",buffer,2) != 0){
      return 1;
    }
    return 0;
  }
  return 0;
}


/****************************************************************
 ****************************************************************
 **
 ** Code for deallocating or initializing header data structures
 **
 ****************************************************************
 ****************************************************************/

void dealloc_mps_headers(mps_headers *header){
  int i;

  if (header->n_chip_type > 0){
    for (i = 0; i < header->n_chip_type; i++){
      free(header->chip_type[i]);
    }
    free(header->chip_type);
  }
      
  if (header->lib_set_name != NULL){
    free(header->lib_set_name);
  }

  if (header->lib_set_version != NULL){
    free(header->lib_set_version);
  }


  if (header->header0_str != NULL){
    free(header->header0_str);
    free(header->header0);
  }

  if (header->create_date != NULL){
    free(header->create_date);
  }

  if (header->guid != NULL){
    free(header->guid);
  }

  if (header->n_other_headers > 0){
    for (i = 0; i < header->n_other_headers; i++){
      free(header->other_headers_keys[i]);
      free(header->other_headers_values[i]);
    }
    free(header->other_headers_keys);
    free(header->other_headers_values);
  }
}



void initialize_mps_header(mps_headers *header){

  header->chip_type = NULL;
  header->n_chip_type = 0;
  
  header->lib_set_name= NULL;
  header->lib_set_version= NULL;
  header->header0_str= NULL;
  header->header0= NULL;
  header->create_date= NULL;
  header->guid= NULL;
  header->other_headers_keys= NULL;
  header->other_headers_values= NULL;
  header->n_other_headers=0;
}


void dealloc_mps_file(mps_file* my_ps){


  if (my_ps->headers != NULL){
    dealloc_mps_headers(my_ps->headers);
    //   free(my_ps->headers);
    delete my_ps->headers;
  }
  delete my_ps->probesets;
  delete my_ps;
}


/****************************************************************
 ****************************************************************
 **
 ** Code for figuring out column ordering 
 ** 
 ****************************************************************
 ***************************************************************/


static void determine_order_header0(char *header_str, header_0 *header0){

  tokenset *cur_tokenset;
  int i;
  char *temp_str = (char *)calloc(strlen(header_str) +1, sizeof(char));


  strcpy(temp_str,header_str);

  header0->probeset_id = -1;
  header0->transcript_cluster_id = -1;
  header0->probeset_list = -1;
  header0->probe_count = -1;
  
  cur_tokenset = tokenize(temp_str,"\t\r\n");
  
  for (i=0; i < tokenset_size(cur_tokenset); i++){
    if (strcmp(get_token(cur_tokenset,i),"probeset_id")==0){
      header0->probeset_id = i;
    } else if (strcmp(get_token(cur_tokenset,i),"transcript_cluster_id")==0){
      header0->transcript_cluster_id = i;
    } else if (strcmp(get_token(cur_tokenset,i),"probeset_list") == 0){
      header0->probeset_list  = i;
    } else if (strcmp(get_token(cur_tokenset,i),"probe_count") == 0){
      header0->probe_count  = i;
    }
  }
  delete_tokens(cur_tokenset);

  free(temp_str);

}


/****************************************************************
 **
 ** Validate that required headers are present in file. 
 **
 ** Return 0 if an expected header is not present.
 ** Returns 1 otherwise (ie everything looks fine)
 **
 ***************************************************************/

static int validate_mps_header(mps_headers *header){


  /* check that required headers are all there (have been read) */
  if (header->chip_type == NULL)
    return 0;

  if (header->lib_set_name == NULL)
    return 0;

  if (header->lib_set_version == NULL)
    return 0;

  if (header->header0_str == NULL)
    return 0;




  /* check that header0 has required fields */

  if (header->header0->probeset_id == -1)
    return 0;

  if (header->header0->probeset_list == -1)
    return 0;
 

  return 1;
}

/****************************************************************
 ****************************************************************
 **
 ** Code for actually reading from the file
 ** 
 ****************************************************************
 ***************************************************************/

static FILE *open_mps_file(const char *filename){
  
  const char *mode = "r";
  FILE *currentFile = NULL; 

  currentFile = fopen(filename,mode);
  if (currentFile == NULL){
     error("Could not open file %s", filename);
  }   
  return currentFile;

}

/****************************************************************
 **
 ** Reading the header
 **
 ***************************************************************/


void read_mps_header(FILE *cur_file, char *buffer, mps_headers *header){


  tokenset *cur_tokenset;
  int i;
  char *temp_str;
  
  
  initialize_mps_header(header);
  do {
    ReadFileLine(buffer, BUFFERSIZE, cur_file);
    /* Rprintf("%s\n",buffer); */
    if (IsHeaderLine(buffer)){
      cur_tokenset = tokenize(&buffer[2],"=\r\n");
      /* hopefully token 0 is Key 
	 and token 1 is Value */
      /*   Rprintf("Key is: %s\n",get_token(cur_tokenset,0));
	   Rprintf("Value is: %s\n",get_token(cur_tokenset,1)); */
      /* Decode the Key/Value pair */
      if (strcmp(get_token(cur_tokenset,0),"chip_type") == 0){
	if (header->n_chip_type == 0){
	  header->chip_type = (char **)calloc(1, sizeof(char *));
	} else {
	  header->chip_type = (char **)realloc(header->chip_type, (header->n_chip_type+1)*sizeof(char *));
	}
	temp_str = (char *)calloc(strlen(get_token(cur_tokenset,1))+1,sizeof(char));
	strcpy(temp_str,get_token(cur_tokenset,1));
	header->chip_type[header->n_chip_type] = temp_str;
	header->n_chip_type++;
      } else if (strcmp(get_token(cur_tokenset,0), "lib_set_name") == 0){
	temp_str = (char *)calloc(strlen(get_token(cur_tokenset,1)) + 1,sizeof(char));
	strcpy(temp_str,get_token(cur_tokenset,1));
	header->lib_set_name = temp_str;
      } else if (strcmp(get_token(cur_tokenset,0), "lib_set_version") == 0){
	temp_str = (char *)calloc(strlen(get_token(cur_tokenset,1)) + 1,sizeof(char));
	strcpy(temp_str,get_token(cur_tokenset,1));
	header->lib_set_version = temp_str;
      } else if (strcmp(get_token(cur_tokenset,0), "create_date") == 0) {
	temp_str = (char *)calloc(strlen(get_token(cur_tokenset,1)) + 1,sizeof(char));
	strcpy(temp_str,get_token(cur_tokenset,1));
	header->create_date = temp_str;
      } else if (strcmp(get_token(cur_tokenset,0), "header0") == 0) {
	temp_str = (char *)calloc(strlen(get_token(cur_tokenset,1)) + 1,sizeof(char));
	strcpy(temp_str,get_token(cur_tokenset,1));
	header->header0_str = temp_str;
	header->header0 = (header_0 *)calloc(1,sizeof(header_0));
	determine_order_header0(header->header0_str,header->header0);
      } else if (strcmp(get_token(cur_tokenset,0), "guid") == 0) {
	temp_str = (char *)calloc(strlen(get_token(cur_tokenset,1)) + 1,sizeof(char));
	strcpy(temp_str,get_token(cur_tokenset,1));
	header->guid = temp_str;
      } else {
	/* not one of the recognised header types */
	if ( header->n_other_headers == 0){
	  header->other_headers_keys = (char **)calloc(1, sizeof(char *));
	  header->other_headers_values = (char **)calloc(1, sizeof(char *));
	} else {
	  header->other_headers_keys = (char **)realloc(header->other_headers_keys,(header->n_other_headers+1)*sizeof(char *));
	  header->other_headers_values = (char **)realloc(header->other_headers_values,(header->n_other_headers+1)*sizeof(char *));
	  header->chip_type = (char **)realloc(header->chip_type, (header->n_chip_type+1)*sizeof(char *));
	}
	temp_str = (char *)calloc(strlen(get_token(cur_tokenset,1)) + 1,sizeof(char));
	strcpy(temp_str,get_token(cur_tokenset,1));
	header->other_headers_values[header->n_other_headers] = temp_str;
	temp_str = (char *)calloc(strlen(get_token(cur_tokenset,0)) + 1,sizeof(char));
	strcpy(temp_str,get_token(cur_tokenset,0));
	header->other_headers_keys[header->n_other_headers] = temp_str;
	header->n_other_headers++;
	
      }
      
      delete_tokens(cur_tokenset);
    }
  } while (IsHeaderLine(buffer));
 
  /* Check to see that we read header0, if we did then fine to proceed
     otherwise buffer should have headers that we can use to figure it out */

  if (header->header0 == NULL){
    cur_tokenset = tokenize(&buffer[0],"=\r\n");
    temp_str = (char *)calloc(strlen(get_token(cur_tokenset,0)) + 1,sizeof(char));
    strcpy(temp_str,get_token(cur_tokenset,0));
    header->header0_str = temp_str;
    header->header0 = (header_0 *)calloc(1,sizeof(header_0));
    determine_order_header0(header->header0_str,header->header0);
  }



}



void insert_level0(char *buffer, mps_data *probesets, header_0 *header0){

  char *temp_str;
  tokenset *cur_tokenset;
  tokenset *cur_tokenset2;
  int i;
  
  vector<int> ids;

  cur_tokenset = tokenize(buffer,"\t\r\n");

  //wxPrintf(_T("%d \n"),tokenset_size(cur_tokenset));
  
  if (tokenset_size(cur_tokenset) == 4){
    probesets->probeset_id.push_back(atoi(get_token(cur_tokenset,header0->probeset_id)));
    probesets->transcript_cluster_id.push_back(atoi(get_token(cur_tokenset,header0->transcript_cluster_id)));
    probesets->probe_count.push_back(atoi(get_token(cur_tokenset,header0->probe_count)));
  
    // wxPrintf(_T("%d %d %d\n"), atoi(get_token(cur_tokenset,header0->probeset_id)),
    //     atoi(get_token(cur_tokenset,header0->transcript_cluster_id)),
    //     atoi(get_token(cur_tokenset,header0->probe_count)));
    


    cur_tokenset2 = tokenize(get_token(cur_tokenset,header0->probeset_list)," ");
  
    for (i=0; i < tokenset_size(cur_tokenset2); i++){
      ids.push_back(atoi(get_token(cur_tokenset2,i)));
    }
    probesets->probeset_list.push_back(ids);
    delete_tokens(cur_tokenset2);
  }
  delete_tokens(cur_tokenset);
}



void read_mps_probesets(FILE *cur_file, char *buffer, mps_data *probesets, mps_headers *header){
  while(ReadFileLine(buffer, BUFFERSIZE, cur_file)){
    if (IsCommentLine(buffer)){
      /*Ignore */
    } else {
      insert_level0(buffer, probesets, header->header0);
    }
  }
}


mps_file *read_mps(char *filename){

  FILE *cur_file;
  mps_file *my_mps;
  char *buffer = (char *)calloc(BUFFERSIZE, sizeof(char));
  
  cur_file = open_mps_file(filename);
  

  my_mps = new mps_file; //(ps_file *)calloc(1,sizeof(ps_file));

  my_mps->headers = new mps_headers;//(ps_headers *)calloc(1, sizeof(ps_headers));
  my_mps->probesets = new mps_data; //(ps_data *)calloc(1, sizeof(ps_data));
  
  
  read_mps_header(cur_file,buffer,my_mps->headers);
  if (validate_mps_header(my_mps->headers)){
    read_mps_probesets(cur_file, buffer, my_mps->probesets, my_mps->headers);
  } else {
    free(buffer);
    dealloc_mps_file(my_mps);
 
    
    wxString error = _T("PS file does not contain all the required headers (defined by version 1.0)\n");
    
    throw error;
  }

  free(buffer);
  fclose(cur_file);

  return my_mps;
}



wxString mps_get_libsetversion(mps_file *my_mps){

  
  wxString result;
 
  result = wxString(my_mps->headers->lib_set_version,wxConvUTF8);

  return result;

}

wxString mps_get_libsetname(mps_file *my_mps){
  
  wxString result;
 
  result = wxString(my_mps->headers->lib_set_name,wxConvUTF8);

  return result;

}



int mps_get_number_probesets(mps_file *my_mps){


  return my_mps->probesets->probeset_id.size();

}

int mps_get_probeset_id(mps_file *my_mps,int index){
  return my_mps->probesets->probeset_id[index];
}

vector<int> mps_get_probeset_list(mps_file *my_mps,int index){
  return my_mps->probesets->probeset_list[index];
}

int mps_get_probe_count(mps_file *my_mps,int index){
  return my_mps->probesets->probe_count[index];
}
