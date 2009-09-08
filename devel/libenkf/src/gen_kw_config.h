#ifndef __GEN_KW_CONFIG_H__
#define __GEN_KW_CONFIG_H__
#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdbool.h>
#include <enkf_util.h>
#include <enkf_macros.h>
#include <scalar_config.h>
#include <scalar.h>
#include <gen_kw_common.h>


void                        gen_kw_config_set_init_file_fmt( gen_kw_config_type * config , const char * init_file_fmt );
const char                * gen_kw_config_get_key(const gen_kw_config_type * config );
gen_kw_type               * gen_kw_config_get_min_std( const gen_kw_config_type * gen_kw_config );
const char         	  * gen_kw_config_get_template_ref(const gen_kw_config_type * );
gen_kw_config_type 	  * gen_kw_config_fscanf_alloc(const char * key , const char * , const char *, const char * min_std_file , const char * init_file_fmt );
void               	    gen_kw_config_free(gen_kw_config_type *);
void               	    gen_kw_config_transform(const gen_kw_config_type * , const double * , double *);
void               	    gen_kw_config_truncate(const gen_kw_config_type * , scalar_type * );
int                	    gen_kw_config_get_data_size(const gen_kw_config_type * );
const char         	  * gen_kw_config_iget_name(const gen_kw_config_type * , int );
const char         	  * gen_kw_config_get_tagged_name(const gen_kw_config_type * , int );
char               	 ** gen_kw_config_get_name_list(const gen_kw_config_type *);
const scalar_config_type  * gen_kw_config_get_scalar_config(const gen_kw_config_type * );
int                         gen_kw_config_get_index(const gen_kw_config_type *  , const char * );
char                      * gen_kw_config_alloc_user_key(const gen_kw_config_type * config , const char * key , int kw_nr);

SAFE_CAST_HEADER(gen_kw_config);
VOID_FREE_HEADER(gen_kw_config);
VOID_GET_DATA_SIZE_HEADER(gen_kw);
#ifdef __cplusplus
}
#endif
#endif
