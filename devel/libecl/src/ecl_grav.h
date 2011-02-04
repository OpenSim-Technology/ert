#ifndef  __ECL_GRAV_H__
#define  __ECL_GRAV_H__
#ifdef __cplusplus
extern "C" {
#endif

#include <ecl_kw.h>
#include <ecl_grid.h>
#include <ecl_file.h>

double ecl_grav_phase_deltag( double utm_x ,
                              double utm_y , 
                              double tvd,
                              const ecl_grid_type * grid,
                              const ecl_file_type * init_file ,
                              const ecl_kw_type   * sat_kw1,
                              const ecl_kw_type   * rho_kw1,
                              const ecl_kw_type   * porv_kw1,
                              const ecl_kw_type   * sat_kw2,
                              const ecl_kw_type   * rho_kw2,
                              const ecl_kw_type   * porv_kw2);



#ifdef __cplusplus
}
#endif
#endif

