#include <util.h>
#include <ecl_sum.h>
#include <stat.h>
#include <double_vector.h>
#include <time_t_vector.h>
#include <config.h>
#include <vector.h>
#include <glob.h>
#include <statistics.h>

#define DEFAULT_NUM_INTERP  50
#define SUMMARY_JOIN       ":"


typedef struct {
  ecl_sum_type             * ecl_sum;
  double_vector_type       * interp_data;
  const time_t_vector_type * interp_time;
  time_t                     start_time;
  time_t                     end_time;
} sum_case_type;




typedef struct {
  vector_type         * data;
  time_t_vector_type  * interp_time;
  int                   num_interp;
  time_t                start_time;
  time_t                end_time;
} ensemble_type;




//typedef struct {
//  stringlist_type    * header_list;
//  stringlist_type    * unit_list;
//  vector_type        * data;         /* Vector containing row vectors of double_vector_type; 
//                                        one double_vector_type instance for each interpolated 
//                                        timestep. */
//} result_type;





/*****************************************************************/


sum_case_type * sum_case_fread_alloc( const char * data_file , const time_t_vector_type * interp_time ) {
  sum_case_type * sum_case = util_malloc( sizeof * sum_case , __func__ );

  printf("Loading case: %s\n" , data_file );
  sum_case->ecl_sum     = ecl_sum_fread_alloc_case( data_file , SUMMARY_JOIN );
  sum_case->interp_data = double_vector_alloc(0 , 0);
  sum_case->interp_time = interp_time; 
  sum_case->start_time  = ecl_sum_get_start_time( sum_case->ecl_sum );
  sum_case->end_time    = ecl_sum_get_end_time( sum_case->ecl_sum );
  return sum_case;
}


void sum_case_free( sum_case_type * sum_case) {
  ecl_sum_free( sum_case->ecl_sum );
  double_vector_free( sum_case->interp_data );
  free( sum_case );
}


void sum_case_free__( void * sum_case) {
  sum_case_free( (sum_case_type *) sum_case);
}


/*****************************************************************/


void ensemble_add_case( ensemble_type * ensemble , const char * data_file ) {
  sum_case_type * sum_case = sum_case_fread_alloc( data_file , ensemble->interp_time );
  vector_append_owned_ref( ensemble->data , sum_case , sum_case_free__ );
  if (ensemble->start_time > 0)
    ensemble->start_time = util_time_t_min( ensemble->start_time , sum_case->start_time);
  else
    ensemble->start_time = ecl_sum_get_start_time( sum_case->ecl_sum );
  
  ensemble->end_time   = util_time_t_max( ensemble->end_time   , sum_case->end_time);
}



void ensemble_init_time_interp( ensemble_type * ensemble ) {
  int i;
  for (i = 0; i < ensemble->num_interp; i++)
    time_t_vector_append( ensemble->interp_time , ensemble->start_time + i * (ensemble->end_time - ensemble->start_time) / (ensemble->num_interp - 1));
}



void ensemble_load_from_glob( ensemble_type * ensemble , const char * pattern ) {
  glob_t pglob;
  int    i;
  glob( pattern , GLOB_NOSORT , NULL , &pglob );

  for (i=0; i < pglob.gl_pathc; i++)
    ensemble_add_case( ensemble , pglob.gl_pathv[i]);

  globfree( &pglob );
}
  


ensemble_type * ensemble_alloc( ) {
  ensemble_type * ensemble = util_malloc( sizeof * ensemble , __func__ );

  ensemble->num_interp  = DEFAULT_NUM_INTERP;
  ensemble->start_time  = -1;
  ensemble->end_time    = -1;
  ensemble->data        = vector_alloc_new();
  ensemble->interp_time = time_t_vector_alloc( 0 , -1 );
  
  return ensemble;
}


void ensemble_init( ensemble_type * ensemble , config_type * config) {

  /*1 : Loading ensembles and settings from the config instance */
  /*1a: Loading the eclipse summary cases. */
  {
    int i,j;
    for (i=0; i < config_get_occurences( config , "CASE_LIST"); i++) {
      const stringlist_type * case_list = config_iget_stringlist_ref( config , "CASE_LIST" , i );
      for (j=0; j < stringlist_get_size( case_list ); j++)
        ensemble_load_from_glob( ensemble , stringlist_iget( case_list , j ));
    }
  }

  /*1b: Other config settings */
  if (config_item_set( config , "NUM_INTERP" ))
    ensemble->num_interp  = config_iget_as_int( config , "NUM_INTERP" , 0 , 0 );
  
  
  /*2: Remaining initialization */
  ensemble_init_time_interp( ensemble );
}

void ensemble_free( ensemble_type * ensemble ) {
  vector_free( ensemble->data );
  time_t_vector_free( ensemble->interp_time );
  free( ensemble );
}

/*****************************************************************/

/**
   Each output line should be of the format:

   OUTPUT  output_file key.q    key.q    key.q    key.q    ...
*/

void output_init( hash_type * output , const config_type * config ) {
  int i;
  for (i=0; i < config_get_occurences( config , "OUTPUT" ); i++) {
    const stringlist_type * tokens = config_iget_stringlist_ref( config , "OUTPUT" , i);
    const char * file        = stringlist_iget( tokens , 0 );
    int j;
    stringlist_type * keylist = stringlist_alloc_new( );
    /* Alle the keys are just added - without any check. */
    for (j = 1; j < stringlist_get_size( tokens ); j++)
      stringlist_append_copy( keylist , stringlist_iget( tokens , j ));
    
    hash_insert_hash_owned_ref( output , file , keylist , stringlist_free__ );
  }
}




void output_run_line( const char * output_file , const stringlist_type * keys , ensemble_type * ensemble) {
  const char * float_fmt   = "%24.5f ";
  const char * string_fmt  = "%24s ";
  const char * comment_fmt = "-------------------------";
  
  FILE * stream = util_mkdir_fopen( output_file ,"w");
  int i;

  stringlist_type * sum_keys     = stringlist_alloc_new();
  double_vector_type * quantiles = double_vector_alloc(0,0);

  printf("Creating output file: %s \n",output_file );

  /* Going through the keys. */
  for (i = 0; i < stringlist_get_size( keys ); i++) {
    const char * key = stringlist_iget( keys , i );
    char * sum_key;
    double quantile;
    {
      int tokens;
      char ** tmp;
      util_split_string( key , SUMMARY_JOIN , &tokens , &tmp);
      if (tokens == 1)
        util_exit("Hmmm - the key:%s is malformed - must be of the form SUMMARY_KEY:QUANTILE.\n",key);
      
      sum_key = util_alloc_joined_string( (const char **) tmp , tokens - 1 , SUMMARY_JOIN);
      if (!util_sscanf_double( tmp[tokens - 1] , &quantile))
        util_exit("Hmmmm - failed to interpret:%s as a quantile - must be a number [0,1).\n",tmp[tokens-1]);
      
      util_free_stringlist( tmp, tokens );
    }
    double_vector_append( quantiles , quantile );
    stringlist_append_owned_ref( sum_keys , sum_key );
  }

  
  
  /* The main loop - outer loop is running over time. */
  {
    int tstep;
    int ikey;
    hash_type * interp_data_cache = hash_alloc();
    fprintf(stream , "-- DATE        DAYS     ");
    for (ikey=0; ikey < stringlist_get_size( keys ); ikey++) 
      fprintf(stream , string_fmt , stringlist_iget( keys , ikey ));
    fprintf(stream , "\n");
    fprintf(stream , "--------------------------");

    for (ikey=0; ikey < stringlist_get_size( keys ); ikey++) 
      fprintf(stream , comment_fmt);
    fprintf(stream , "\n");

    
    for (tstep = 0; tstep < time_t_vector_size( ensemble->interp_time ); tstep++) {
      time_t interp_time = time_t_vector_iget( ensemble->interp_time , tstep );
      {
        int mday,month,year;
        util_set_datetime_values(interp_time , NULL , NULL , NULL , &mday , &month , &year);
        fprintf(stream , "%02d.%02d.%4d  ", mday , month , year);
      }
      fprintf(stream , "%10.2f  ", 1.0*(interp_time - ensemble->start_time) / 86400);
      
      
      
      for (ikey = 0; ikey < stringlist_get_size( sum_keys ); ikey++) {
        const char * sum_key = stringlist_iget( sum_keys , ikey );
        double quantile      = double_vector_iget( quantiles , ikey );
        double value;
        double_vector_type * interp_data;

        /* Check if we have the vector in the cache table - if not create it. */
        if (!hash_has_key( interp_data_cache , sum_key)) {
          interp_data = double_vector_alloc(0 , 0);
          hash_insert_hash_owned_ref( interp_data_cache , sum_key , interp_data , double_vector_free__);
        }
        interp_data = hash_get( interp_data_cache , sum_key );

        /* Check if the vector has data - if not initialize it. */
        if (double_vector_size( interp_data ) == 0) {
          /* Fill up the interpolated vector */
          for (int iens = 0; iens < vector_get_size( ensemble->data ); iens++) {
            const sum_case_type * sum_case = vector_iget_const( ensemble->data , iens );
            if ((interp_time >= sum_case->start_time) && (interp_time <= sum_case->end_time))    /* We allow the different simulations to have differing length */
              double_vector_append( interp_data , ecl_sum_get_general_var_from_sim_time( sum_case->ecl_sum , interp_time , sum_key)) ;
          }
        }
        value = statistics_empirical_quantile( interp_data , quantile );
        fprintf(stream , float_fmt , value );
      }
      hash_apply( interp_data_cache , double_vector_reset__ );
      fprintf(stream , "\n");
    }
    hash_free( interp_data_cache );
  }
  fclose( stream );
}



void output_run( hash_type * output , ensemble_type * ensemble ) {
  hash_iter_type * iter = hash_iter_alloc( output );

  while (!hash_iter_is_complete( iter )) {
    const char * output_file     = hash_iter_get_next_key( iter );
    const stringlist_type * keys = hash_get( output , output_file );
    output_run_line( output_file , keys , ensemble );
  }
}




/*****************************************************************/

void config_init( config_type * config ) {
  config_item_type * item;

  item = config_add_item( config , "CASE_LIST"      , true , true );
  item = config_add_key_value( config , "NUM_INTERP" , false , CONFIG_INT);
  item = config_add_item( config , "OUTPUT" , true , true );
  config_item_set_argc_minmax( item , 1 , -1 , NULL );
}


/*****************************************************************/

void usage() {
  fprintf(stderr, "\nUse:\n\n    ecl_quantil config_file\n\n");
  
  printf("Help\n");
  printf("----\n");
  printf("\n");
  printf("The ecl_quantile program will load an ensemble of ECLIPSE summary\n");
  printf("files, it can then output quantiles of summary vectors over the time\n");
  printf("span of the simulation. The program is based on a simple configuration\n");
  printf("file. The configuration file only has three keywords:\n");
  printf("\n");
  printf("\n");
  printf("   CASE_LIST   simulation*X/run*X/CASE*.DATA\n");
  printf("   CASE_LIST   extra_simulation.DATA    even/more/simulations*GG/run*.DATA\n");
  printf("\n");
  printf("\n");
  printf("   OUTPUT      FILE1   WWCT:OP_1:0.10  WWCT:OP_1:0.50   WWCT   WOPR:OP_3\n");
  printf("   OUTPUT      FILE2   FOPT:0.10  FOPT:0.50  FOPT:0.90  GOPT:0.10  GOPT:0.50  GOPT:0.90   FWPT:0.10  FWPT:0.50  FWPT:0.90\n");
  printf("   NUM_INTERP  100\n");
  printf("\n");
  printf("\n");
  printf("CASE_LIST: This keyword is used to give the path to ECLIPSE data files\n");
  printf("  corresponding to summaries which you want to load, observe that the\n");
  printf("  argument given to the CASE_LIST keyword can contain unix-style\n");
  printf("  wildcards like '*'. You can point to several simulation cases with\n");
  printf("  one CASE_LIST keyword. In addition you can several CASE_LIST\n");
  printf("  keywords.\n");
  printf("\n");
  printf("\n");
  printf("OUTPUT: This keyword is used to denote what output you want from the\n");
  printf("  program. The first argument to the OUTPUT keyword is the name output\n");
  printf("  file you want to produce, in the example above we will create two\n");
  printf("  output files (FILE1 and FILE2 respectively). The remaining arguments\n");
  printf("  on the output line corresponds to the summary vector & quantile you\n");
  printf("  are interested in. Each of these values is a \":\" separated string\n");
  printf("  consting of:\n");
  printf("   \n");
  printf("     VAR: The ECLIPSE summary variable we are interested in, (nearly)\n");
  printf("          all variables found in the summary file are available,\n");
  printf("          e.g. RPR, WWCT or GOPT.\n");
  printf("\n");
  printf("     WG?: This is extra information added to the variable to make it\n");
  printf("          unique, e.g. the name of a well or group for rate variables\n");
  printf("          and the region number for a region. Not all variables, in\n");
  printf("          particalar the Fxxx rates, have this string.\n");
  printf("\n");
  printf("     Q: The quantile we are interested in, e.g 0.10 to get the P10\n");
  printf("        quantile and 0.90 to get the P90 quantile.\n");
  printf("\n");
  printf("  Examples are:\n");
  printf("\n");
  printf("     WWCT:OPX:0.75:    The P75 quantile of the watercut in well OPX.\n");
  printf("     BPR:10,10,5:0.50: The P50 quantile of the Block Pressure in block 10,10,5\n");
  printf("     FOPT:0.90:        The P90 quantile of the field oil production total.\n");
  printf("\n");
  printf("\n");
  printf("NUM_INTERP: Before the program can calculate quantiles it must\n");
  printf("  interpolate all the simulated data down on the same time axis. This\n");
  printf("  keyword regulates how many points should be used when interpolating\n");
  printf("  the time axis; the default is 50 which is probably quite OK. Observe\n");
  printf("  that for rate variable the program will not do linear interpolation\n");
  printf("  between ECLIPSE report steps, the might therefore look a bit jagged\n");
  printf("  if NUM_INTERP is set to high. This keyword is optional.\n");
  exit(0);
}



int main( int argc , char ** argv ) {
  if (argc != 2)
    usage();
  else {
    hash_type     * output   = hash_alloc();
    ensemble_type * ensemble = ensemble_alloc();
    {
      config_type   * config   = config_alloc( );
      config_init( config );
      config_parse( config , argv[1] , "--" , NULL , NULL , false , true );
    
      ensemble_init( ensemble , config );
      output_init( output , config);
      config_free( config );
    } 
    output_run( output , ensemble );
    ensemble_free( ensemble );
    hash_free( output );
  }
}