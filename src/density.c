#include "density.h"

/* This file contains routines to generate one-body and two-body density matrices
   The input files are wavefunction files written by BIGSTICK
   The code relies on a factorization between proton and neutron Slater determinants
*/
void two_body_density_spec(speedParams *sp) {
  // Generate two-body density matrix with spectator-energy-dependence
  // Read in data 
  char wfn_file_initial[100];
  strcpy(wfn_file_initial, sp->initial_file_base);
  strcat(wfn_file_initial, ".wfn");
  char wfn_file_final[100];
  strcpy(wfn_file_final, sp->final_file_base);
  strcat(wfn_file_final, ".wfn");
  char basis_file_initial[100];
  strcpy(basis_file_initial, sp->initial_file_base);
  strcat(basis_file_initial, ".bas");
  char basis_file_final[100];
  strcpy(basis_file_final, sp->final_file_base);
  strcat(basis_file_final, ".bas");
  wfnData *wd = read_binary_wfn_data(wfn_file_initial, wfn_file_final, basis_file_initial, basis_file_final);

  int j_op = sp->j_op;
  int t_op = sp->t_op;
  int ns = wd->n_shells;

  // Determine number of intermediate Slater determinants
  // after the action of one or two annihilation operators
  unsigned int n_sds_p_int1 = get_num_sds(wd->n_shells, wd->n_proton_f - 1);
  unsigned int n_sds_p_int2 = get_num_sds(wd->n_shells, wd->n_proton_f - 2);
  unsigned int n_sds_n_int1 = get_num_sds(wd->n_shells, wd->n_neutron_f - 1);
  unsigned int n_sds_n_int2 = get_num_sds(wd->n_shells, wd->n_neutron_f - 2);

  printf("Intermediate SDs: p1: %d n1: %d, p2: %d, n2: %d\n", n_sds_p_int1, n_sds_n_int1, n_sds_p_int2, n_sds_n_int2);
  //printf("%d, %d, %d, %d\n", wd->n_proton_i, wd->n_proton_f, wd->n_neutron_i, wd->n_neutron_f);
  // Determine the min and max mj values of proton/neutron SDs
  float mj_min_p_i = min_mj(ns, wd->n_proton_i, wd->jz_shell);
  float mj_max_p_i = max_mj(ns, wd->n_proton_i, wd->jz_shell);
  float mj_min_n_i = min_mj(ns, wd->n_neutron_i, wd->jz_shell);
  float mj_max_n_i = max_mj(ns, wd->n_neutron_i, wd->jz_shell);
  
//  printf("%g, %g, %g, %g\n", mj_min_p_i, mj_max_p_i, mj_min_n_i, mj_max_n_i);
  // Account for the factor that m_p + m_n = 0 in determining min/max mj
  mj_min_p_i = MAX(mj_min_p_i, -mj_max_n_i);
  mj_max_p_i = MIN(mj_max_p_i, -mj_min_n_i);
  mj_min_n_i = MAX(mj_min_n_i, -mj_max_p_i);
  mj_max_n_i = MIN(mj_max_n_i, -mj_min_p_i);
  // Determine number of sectors (mp, mn)
  int num_mj_i = mj_max_p_i - mj_min_p_i + 1;
  printf("Number of m_j sectors:%d\n", num_mj_i);
  //printf("%g, %g, %g, %g\n", mj_min_p_i, mj_max_p_i, mj_min_n_i, mj_max_n_i);

  int max_n_spec_q = get_max_n_spec_q(ns, wd->n_proton_i, wd->n_shell, wd->l_shell) + get_max_n_spec_q(ns, wd->n_neutron_i, wd->n_shell, wd->l_shell);
  int min_n_spec_q = get_min_n_spec_q(ns, wd->n_proton_i, wd->n_shell, wd->l_shell) + get_min_n_spec_q(ns, wd->n_neutron_i, wd->n_shell, wd->l_shell);

  int n_spec_bins = max_n_spec_q - min_n_spec_q + 1;
  printf("Min spec excitations: %d Max spec excitations: %d\n", min_n_spec_q, max_n_spec_q);
  // Allocate space for jump lists
  wfe_list **p0_list_i = (wfe_list**) calloc(2*num_mj_i, sizeof(wfe_list*));
  wfe_list **n0_list_i = (wfe_list**) calloc(2*num_mj_i, sizeof(wfe_list*));
  sde_list **p1_list_i = (sde_list**) calloc(2*ns*num_mj_i, sizeof(sde_list*));
  sde_list **n1_list_i = (sde_list**) calloc(2*ns*num_mj_i, sizeof(sde_list*));
  sde_list **p2_list_i = (sde_list**) calloc(2*ns*ns*num_mj_i, sizeof(sde_list*));
  sde_list **n2_list_i = (sde_list**) calloc(2*ns*ns*num_mj_i, sizeof(sde_list*));
  sde_list **p2_list_f = (sde_list**) calloc(2*ns*ns*num_mj_i, sizeof(sde_list*));
  sde_list **n2_list_f = (sde_list**) calloc(2*ns*ns*num_mj_i, sizeof(sde_list*));
  int* p1_array_f = (int*) calloc(ns*n_sds_p_int1, sizeof(int));
  int* p2_array_f = (int*) calloc(ns*ns*n_sds_p_int2, sizeof(int));
  int* n1_array_f = (int*) calloc(ns*n_sds_n_int1, sizeof(int));
  int* n2_array_f = (int*) calloc(ns*ns*n_sds_n_int2, sizeof(int));

  // Determine one/two-body jumps, using special routine if initial and final bases are the same
  if (wd->same_basis) {
    printf("Building proton jumps...\n");
    build_two_body_jumps_i_and_f_spec(wd->n_shells, wd->n_proton_i, mj_min_p_i, mj_max_p_i, num_mj_i, wd->n_sds_p_i, n_sds_p_int1, n_sds_p_int2, p1_array_f, p2_array_f, p0_list_i, p1_list_i, p2_list_i, wd->jz_shell, wd->l_shell, wd->n_shell);
    printf("Done.\n");
    printf("Building neutron jumps...\n");
    build_two_body_jumps_i_and_f_spec(wd->n_shells, wd->n_neutron_i, mj_min_n_i, mj_max_n_i, num_mj_i, wd->n_sds_n_i, n_sds_n_int1, n_sds_n_int2, n1_array_f, n2_array_f, n0_list_i, n1_list_i, n2_list_i, wd->jz_shell, wd->l_shell, wd->n_shell);
    printf("Done.\n");
  } else {
    printf("Building initial state proton jumps...\n");
    build_two_body_jumps_i_spec(wd->n_shells, wd->n_proton_i, mj_min_p_i, mj_max_p_i, num_mj_i, wd->n_sds_p_i, p0_list_i, p1_list_i, p2_list_i, wd->jz_shell, wd->l_shell, wd->n_shell);
    printf("Done\n");
    printf("Building final state proton jumps...\n");
    build_two_body_jumps_f_spec(wd->n_shells, wd->n_proton_f, mj_min_p_i, mj_max_p_i, num_mj_i, wd->n_sds_p_f, n_sds_p_int1, n_sds_p_int2, p1_array_f, p2_array_f, p2_list_f, wd->jz_shell, wd->l_shell, wd->n_shell);
    printf("Done\n");
    printf("Building initial state neutron jumps...\n");
    build_two_body_jumps_i_spec(wd->n_shells, wd->n_neutron_i, mj_min_n_i, mj_max_n_i, num_mj_i, wd->n_sds_n_i, n0_list_i, n1_list_i, n2_list_i, wd->jz_shell, wd->l_shell, wd->n_shell);
    printf("Building final state neutron jumps...\n");
    build_two_body_jumps_f_spec(wd->n_shells, wd->n_neutron_f, mj_min_n_i, mj_max_n_i, num_mj_i, wd->n_sds_n_f, n_sds_n_int1, n_sds_n_int2, n1_array_f, n2_array_f, n2_list_f, wd->jz_shell, wd->l_shell, wd->n_shell);
    printf("Done.\n");
  }

  double* cg_fact = (double*) calloc(sp->n_trans, sizeof(double));
  float mti = 0.5*(wd->n_proton_i - wd->n_neutron_i);
  float mtf = 0.5*(wd->n_proton_f - wd->n_neutron_f);
  float mt_op = mtf - mti;
  if (fabs(mt_op) > t_op) {printf("Error: operator iso-spin is insufficient to mediate a transition between the given nuclides.\n"); exit(0);}
  FILE *out_file;
  char output_density_file[100];
  char output_log_file[100];
  char output_suffix[100];
  strcpy(output_log_file, sp->out_file_base);
  strcat(output_log_file, ".log");

  // Loop over transitions
  eigen_list* trans = sp->transition_list;
  int i_trans = 0;
  while (trans != NULL) {
    int psi_i = trans->eig_i;
    int psi_f = trans->eig_f;
    float ji = wd->j_nuc_i[psi_i];
    float ti = wd->t_nuc_i[psi_i];
    double cg_j = 0.0;
    double cg_t = 0.0;
    float jf = wd->j_nuc_f[psi_f];
    float tf = wd->t_nuc_f[psi_f];
    int ijf = 2*jf;
    float mj = 0.0;
    if (ijf % 2) {mj = 0.5;}
    if (fabs(ji - jf) > j_op) {printf("Error: The requested transition #%d Ji = %g Ti = %g -> #%d Jf = %g Tf = %g through the operator Jop = %d Top = %d is impossible. \nPlease remove this transition from the parameter file and try again.\n", psi_i, ji, ti, psi_f, jf, tf, j_op, t_op); exit(0);}
    cg_j = clebsch_gordan(j_op, ji, jf, 0, mj, mj);
    if (cg_j == 0.0) {continue;}
    cg_t = clebsch_gordan(t_op, ti, tf, mt_op, mti, mtf);
    if (cg_t == 0.0) {continue;}
    cg_j *= pow(-1.0, jf - ji)/sqrt(2*jf + 1);
    cg_t *= pow(-1.0, tf - ti)/sqrt(2*tf + 1);
    printf("Initial state: # %d J: %g T: %g Final state: # %d J: %g T: %g\n", psi_i + 1, ji, ti, psi_f + 1, jf, tf);
    strcpy(output_density_file, sp->out_file_base);
    sprintf(output_suffix, "_J%d_T%d_%d_%d.dens", j_op, t_op, psi_i, psi_f);
    strcat(output_density_file, output_suffix);
    out_file = fopen(output_density_file, "w"); 
    cg_fact[i_trans] = cg_j*cg_t;
    i_trans++;
    trans = trans->next;
  } 
  double* j_store = (double*) malloc(4*sizeof(double));
  double* density = (double*) calloc(n_spec_bins*sp->n_trans, sizeof(double));
 
  // Loop over orbital a
  for (int i_orb1 = 0; i_orb1 < wd->n_orbits; i_orb1++) {
    float j1 = wd->j_orb[i_orb1];
    // Loop over orbital b
    for (int i_orb2 = 0; i_orb2 <= i_orb1; i_orb2++) {
      float j2 = wd->j_orb[i_orb2];
      // Loop over orbital c
      for (int i_orb3 = 0; i_orb3 < wd->n_orbits; i_orb3++) {
        float j4 = wd->j_orb[i_orb3];
        // Loop over orbital d
        for (int i_orb4 = 0; i_orb4 <= i_orb3; i_orb4++) {
          if (pow(-1.0, wd->l_orb[i_orb1] + wd->l_orb[i_orb2] + wd->l_orb[i_orb3] + wd->l_orb[i_orb4]) != 1.0) {continue;} // Parity
          float j3 = wd->j_orb[i_orb4];
          // Allocate storage for each j12 and j34
          int j_min_12 = abs(j1 - j2);
          int j_max_12 = j1 + j2;
          int j_dim_12 = j_max_12 - j_min_12 + 1;
          int j_min_34 = abs(j3 - j4);
          int j_max_34 = j3 + j4;
          int j_dim_34 = j_max_34 - j_min_34 + 1;
          int j_dim = j_dim_12*j_dim_34;
          if (j_op > j_max_12 + j_max_34) {continue;}
          int j_tot_min = j_max_12 + j_max_34;
          for (int j12 = j_min_12; j12 <= j_max_12; j12++) {
            for (int j34 = j_min_34; j34 <= j_max_34; j34++) {
              if (abs(j12 - j34) < j_tot_min) {j_tot_min = abs(j12 - j34);}
            }
          }
          if (j_op < j_tot_min) {continue;}
          j_store = realloc(j_store, sizeof(double)*4*j_dim*n_spec_bins*sp->n_trans);
          for (int k = 0; k < 4*j_dim*n_spec_bins*sp->n_trans; k++) {
            j_store[k] = 0.0;
          }
          // Loop over shells for orbit a
          for (int ia = 0; ia < 2*wd->n_shells; ia++) {
            float mt1 = 0.5;
            int a = ia;
            if (a >= ns) {a -= ns; mt1 -= 1;}
            if (wd->l_shell[a] != wd->l_orb[i_orb1]) {continue;}
            if (wd->n_shell[a] != wd->n_orb[i_orb1]) {continue;}
            if (wd->j_shell[a]/2.0 != j1) {continue;}
            float mj1 = wd->jz_shell[a]/2.0;
            // Loop over shells for orbit b
            for (int ib = 0; ib < 2*wd->n_shells; ib++) {
              if (ib == ia) {continue;}
              float mt2 = 0.5;
              int b = ib;
              if (b >= ns) {b -= ns; mt2 -= 1;}
              if (wd->l_shell[b] != wd->l_orb[i_orb2]) {continue;}
              if (wd->n_shell[b] != wd->n_orb[i_orb2]) {continue;}
              if (wd->j_shell[b]/2.0 != j2) {continue;}
              float mj2 = wd->jz_shell[b]/2.0;
              if ((i_orb1 == i_orb2) && (a < b) && (mt1 == mt2)) {continue;}
              for (int ic = 0; ic < 2*wd->n_shells; ic++) {
                float mt4 = 0.5;
                int c = ic;
                if (c >= ns) {c -= ns; mt4 -= 1;}
                if (wd->l_shell[c] != wd->l_orb[i_orb3]) {continue;}
                if (wd->n_shell[c] != wd->n_orb[i_orb3]) {continue;}
                if (wd->j_shell[c]/2.0 != j4) {continue;}
                float mj4 = wd->jz_shell[c]/2.0;
                // Loop over shells for orbit d
                for (int id = 0; id < 2*wd->n_shells; id++) {
                  if (ic == id) {continue;}
                  int d = id;
                  float mt3 = 0.5;
                  if (d >= ns) {d -= ns; mt3 -= 1;}
                  if (wd->l_shell[d] != wd->l_orb[i_orb4]) {continue;}
                  if (wd->n_shell[d] != wd->n_orb[i_orb4]) {continue;}
                  if (wd->j_shell[d]/2.0 != j3) {continue;}
                  float mj3 = wd->jz_shell[d]/2.0;
                  if ((i_orb3 == i_orb4) && (c < d) && (mt3 == mt4)) {continue;}
                  if (mj1 + mj2 != mj3 + mj4) {continue;}
                  if (mt1 + mt2 - mt3 - mt4 != mt_op) {continue;}
                  for (int i = 0; i < sp->n_trans*n_spec_bins; i++) {
                    density[i] = 0;
                  }
                  if ((mt3 == 0.5) && (mt4 == 0.5)) { // 
                    if ((mt1 == 0.5) && (mt2 == 0.5)) { // 2 proton creation operators + 2 proton annihilation operators
                      trace_a4_nodes_spec(a, b, c, d, num_mj_i, n_sds_p_int2, p2_array_f, p2_list_i, n0_list_i, wd, 0, sp->transition_list, density, min_n_spec_q, n_spec_bins);
                    } else if ((mt1 == -0.5) && (mt2 == -0.5)) { // 2 neutron creation operators and two proton ann. operators
                      trace_a22_nodes_spec(a, b, c, d, num_mj_i, p2_list_i, n2_list_f, wd, 0, sp->transition_list, density, min_n_spec_q, n_spec_bins);
                    }  
                  } else if ((mt3 == -0.5) && (mt4 == -0.5)) {
                    if ((mt1 == -0.5) && (mt2 == -0.5)) { //2 n cr. and 2 n ann. operators
                      trace_a4_nodes_spec(a, b, c, d, num_mj_i, n_sds_n_int2, n2_array_f, n2_list_i, p0_list_i, wd, 1, sp->transition_list, density, min_n_spec_q, n_spec_bins);
                    } else if ((mt1 == 0.5) && (mt2 == 0.5)) {// 2 p cr. and 2 n ann. operators
                      trace_a22_nodes_spec(a, b, c, d, num_mj_i, n2_list_i, p2_list_f, wd, 1, sp->transition_list, density, min_n_spec_q, n_spec_bins);
                    }
                  } else if ((mt1 == 0.5) && (mt2 == -0.5) && (mt3 == 0.5) && (mt4 == -0.5)) {
                      trace_a20_nodes_spec(a, d, b, c, num_mj_i, n_sds_p_int1, n_sds_n_int1, p1_list_i, n1_list_i, p1_array_f, n1_array_f, wd, sp->transition_list, density, min_n_spec_q, n_spec_bins);
                      for (int i = 0; i < n_spec_bins*sp->n_trans; i++) {
                        density[i] = -density[i];
                      }
                  } else if ((mt1 == -0.5) && (mt2 == 0.5) && (mt3 == 0.5) && (mt4 == -0.5)) {
                      trace_a20_nodes_spec(b, d, a, c, num_mj_i, n_sds_p_int1, n_sds_n_int1, p1_list_i, n1_list_i, p1_array_f, n1_array_f, wd, sp->transition_list, density, min_n_spec_q, n_spec_bins);
                  } else if ((mt1 == 0.5) && (mt2 == -0.5) && (mt3 == -0.5) && (mt4 == 0.5)) {
                      trace_a20_nodes_spec(a, c, b, d, num_mj_i, n_sds_p_int1, n_sds_n_int1,p1_list_i, n1_list_i, p1_array_f, n1_array_f, wd, sp->transition_list, density, min_n_spec_q, n_spec_bins);
                  } else if ((mt1 == -0.5) && (mt2 == 0.5) && (mt3 == -0.5) && (mt4 == 0.5)) {
                      trace_a20_nodes_spec(b, c, a, d, num_mj_i, n_sds_p_int1, n_sds_n_int1, p1_list_i, n1_list_i, p1_array_f, n1_array_f, wd, sp->transition_list, density, min_n_spec_q, n_spec_bins);
                      for (int i = 0; i < n_spec_bins*sp->n_trans; i++) {
                        density[i] = -density[i];
                      }
                  }
                  for (int j12 = j_min_12; j12 <= j_max_12; j12++) {
                    if ((mj1 + mj2 > j12) || (mj1 + mj2 < -j12)) {continue;}
                    float cg_j12 = clebsch_gordan(j1, j2, j12, mj1, mj2, mj1 + mj2);
                    if (cg_j12 == 0.0) {continue;}
                    for (int t12 = 0; t12 <= 1; t12++) {
                      if ((mt1 + mt2 > t12) || (mt1 + mt2 < -t12)) {continue;}
                      float cg_t12 = clebsch_gordan(0.5, 0.5, t12, mt1, mt2, mt1 + mt2);
                      for (int j34 = j_min_34; j34 <= j_max_34; j34++) {
                        if ((mj3 + mj4 > j34) || (mj3 + mj4 < -j34)){continue;}
                        float cg_j34 = clebsch_gordan(j4, j3, j34, mj4, mj3, mj3 + mj4);
                        if (cg_j34 == 0.0) {continue;}
                        float cg_jop = clebsch_gordan(j_op, j34, j12, 0, mj3 + mj4, mj1 + mj2);
                        if (cg_jop == 0.0) {continue;}
                        for (int t34 = 0; t34 <= 1; t34++) {
                          if ((mt3 + mt4 > t34) || (mt3 + mt4 < -t34)) {continue;}
                          float cg_t34 = clebsch_gordan(0.5, 0.5, t34, mt4, mt3, mt3 + mt4);
                          if (cg_t34 == 0.0) {continue;}
                          float cg_top = clebsch_gordan(t_op, t34, t12, mt_op, mt3 + mt4, mt1 + mt2);
                          if (cg_top == 0.0) {continue;}
                          double d2 = cg_j12*cg_j34*cg_jop;
                          d2 *= cg_t12*cg_t34*cg_top;
                          d2 *= 0.25*pow(-1.0, -j34 + j12 - t34 + t12)/sqrt((2*j12 + 1)*(2*t12 + 1));
                          if (i_orb1 == i_orb2) {d2 *= sqrt(2);}
                          if (i_orb3 == i_orb4) {d2 *= sqrt(2);}
                          if (d2 == 0.0) {continue;}
                          for (int i_spec = 0; i_spec < n_spec_bins; i_spec++) {
                            for (int i_trans = 0; i_trans < sp->n_trans; i_trans++) {
                              j_store[i_spec + n_spec_bins*(i_trans + sp->n_trans*(t12 + 2*(t34 + 2*((j12 - j_min_12)*j_dim_34 + (j34 - j_min_34)))))] += density[i_spec + n_spec_bins*i_trans]*d2/cg_fact[i_trans];;
                              if ((i_orb1 == i_orb2) && (mt1 == mt2)) {
                                j_store[i_spec + n_spec_bins*(i_trans + sp->n_trans*(t12 + 2*(t34 + 2*((j12 - j_min_12)*j_dim_34 + (j34 - j_min_34)))))] += pow(-1.0, j1 + j2 - j12 - t12)*density[i_spec + n_spec_bins*i_trans]*d2/cg_fact[i_trans];
                              }
                              if ((i_orb3 == i_orb4) && (mt3 == mt4)) {
                                j_store[i_spec + n_spec_bins*(i_trans + sp->n_trans*(t12 + 2*(t34 + 2*((j12 - j_min_12)*j_dim_34 + (j34 - j_min_34)))))] += pow(-1.0, j3 + j4 - j34 - t34)*density[i_spec + n_spec_bins*i_trans]*d2/cg_fact[i_trans];
                              }
                              if ((i_orb1 == i_orb2) && (mt1 == mt2) && (i_orb3 == i_orb4) && (mt3 == mt4)) {
                                j_store[i_spec + n_spec_bins*(i_trans + sp->n_trans*(t12 + 2*(t34 + 2*((j12 - j_min_12)*j_dim_34 + (j34 - j_min_34)))))] += pow(-1.0, j1 + j2 + j3 + j4 - j12 - j34 - t12 - t34)*density[i_spec + n_spec_bins*i_trans]*d2/cg_fact[i_trans];
                              }
                            }
                          }
                        }
                      }
                    }
                  }
                }
              }
            }
          } 
          trans = sp->transition_list;
          i_trans = 0;
          while (trans != NULL) {
            int psi_i = trans->eig_i;
            int psi_f = trans->eig_f;
            strcpy(output_density_file, sp->out_file_base);
            sprintf(output_suffix, "_J%d_T%d_%d_%d.dens", j_op, t_op, psi_i, psi_f);
            strcat(output_density_file, output_suffix);
            out_file = fopen(output_density_file, "a"); 

            for (int t12 = 0; t12 <= 1; t12++) {
              for (int t34 = 0; t34 <= 1; t34++) {
                for (int ij12 = 0; ij12 < j_dim_12; ij12++) {
                  for (int ij34 = 0; ij34 < j_dim_34; ij34++) {
                    for (int i_spec = 0; i_spec < n_spec_bins; i_spec++) {
                      int n_spec = min_n_spec_q + i_spec;
                      if (fabs(j_store[i_spec + n_spec_bins*(i_trans + sp->n_trans*(t12 + 2*(t34 + 2*(ij12*j_dim_34 + ij34))))]) < pow(10, -16)) {continue;}
               
                      fprintf(out_file, "%d,%g,%d,%g,%d,%d,%d,%g,%d,%g,%d,%d,%d,%g\n", 2*wd->n_orb[i_orb1] + wd->l_orb[i_orb1], 2*wd->j_orb[i_orb1], 2*wd->n_orb[i_orb2] + wd->l_orb[i_orb2], 2*wd->j_orb[i_orb2], 2*(ij12 + j_min_12), 2*t12, 2*wd->n_orb[i_orb3] + wd->l_orb[i_orb3], 2*wd->j_orb[i_orb3], 2*wd->n_orb[i_orb4] + wd->l_orb[i_orb4], 2*wd->j_orb[i_orb4], 2*(ij34 + j_min_34), 2*t34, n_spec, j_store[i_spec + n_spec_bins*(i_trans + sp->n_trans*(t12 + 2*(t34 + 2*(ij12*j_dim_34 + ij34))))]);
            
                      if (i_orb1 != i_orb2) { 
                        fprintf(out_file, "%d,%g,%d,%g,%d,%d,%d,%g,%d,%g,%d,%d,%d,%g\n", 2*wd->n_orb[i_orb2] + wd->l_orb[i_orb2], 2*wd->j_orb[i_orb2], 2*wd->n_orb[i_orb1] + wd->l_orb[i_orb1], 2*wd->j_orb[i_orb1], 2*(ij12 + j_min_12), 2*t12, 2*wd->n_orb[i_orb3] + wd->l_orb[i_orb3], 2*wd->j_orb[i_orb3], 2*wd->n_orb[i_orb4] + wd->l_orb[i_orb4], 2*wd->j_orb[i_orb4], 2*(ij34 + j_min_34), 2*t34, n_spec, pow(-1.0, j1 + j2 - ij12 - j_min_12 - t12)*j_store[i_spec + n_spec_bins*(i_trans + sp->n_trans*(t12 + 2*(t34 + 2*(ij12*j_dim_34 + ij34))))]);
                      }
            
                      if (i_orb3 != i_orb4) {
                          fprintf(out_file, "%d,%g,%d,%g,%d,%d,%d,%g,%d,%g,%d,%d,%d,%g\n", 2*wd->n_orb[i_orb1] + wd->l_orb[i_orb1], 2*wd->j_orb[i_orb1], 2*wd->n_orb[i_orb2] + wd->l_orb[i_orb2], 2*wd->j_orb[i_orb2], 2*(ij12 + j_min_12), 2*t12, 2*wd->n_orb[i_orb4] + wd->l_orb[i_orb4], 2*wd->j_orb[i_orb4], 2*wd->n_orb[i_orb3] + wd->l_orb[i_orb3], 2*wd->j_orb[i_orb3], 2*(ij34 + j_min_34), 2*t34, n_spec, pow(-1.0, j3 + j4 - ij34 - j_min_34 - t34)*j_store[i_spec + n_spec_bins*(i_trans + sp->n_trans*(t12 + 2*(t34 + 2*(ij12*j_dim_34 + ij34))))]);
                      }
          
                      if ((i_orb1 != i_orb2) && (i_orb3 != i_orb4)) {
                          fprintf(out_file, "%d,%g,%d,%g,%d,%d,%d,%g,%d,%g,%d,%d,%d,%g\n", 2*wd->n_orb[i_orb2] + wd->l_orb[i_orb2], 2*wd->j_orb[i_orb2], 2*wd->n_orb[i_orb1] + wd->l_orb[i_orb1], 2*wd->j_orb[i_orb1], 2*(ij12 + j_min_12), 2*t12, 2*wd->n_orb[i_orb4] + wd->l_orb[i_orb4], 2*wd->j_orb[i_orb4], 2*wd->n_orb[i_orb3] + wd->l_orb[i_orb3], 2*wd->j_orb[i_orb3], 2*(ij34 + j_min_34), 2*t34, n_spec, pow(-1.0, j1 + j2 + j3 + j4 - ij12 - j_min_12 - ij34 - j_min_34 - t12 - t34)*j_store[i_spec + n_spec_bins*(i_trans + sp->n_trans*(t12 + 2*(t34 + 2*(ij12*j_dim_34 + ij34))))]);
                      }
                    }
                  }
                }
              }
            }
            i_trans++;
            trans = trans->next;
            fclose(out_file);
          }
        }
      }
    }
  }
  free(j_store); 
  
  return;
}

void two_body_density_trunc(speedParams *sp) {

  // Read in data 
  char wfn_file_initial[100];
  strcpy(wfn_file_initial, sp->initial_file_base);
  strcat(wfn_file_initial, ".wfn");
  char wfn_file_final[100];
  strcpy(wfn_file_final, sp->final_file_base);
  strcat(wfn_file_final, ".wfn");
  char basis_file_initial[100];
  strcpy(basis_file_initial, sp->initial_file_base);
  strcat(basis_file_initial, ".bas");
  char basis_file_final[100];
  strcpy(basis_file_final, sp->final_file_base);
  strcat(basis_file_final, ".bas");
  wfnData *wd = read_binary_wfn_data(wfn_file_initial, wfn_file_final, basis_file_initial, basis_file_final);

  int j_op = sp->j_op;
  int t_op = sp->t_op;
  int ns = wd->n_shells;

  // Determine number of intermediate Slater determinants
  // after the action of one or two annihilation operators
  unsigned int n_sds_p_int1 = get_num_sds(wd->n_shells, wd->n_proton_f - 1);
  unsigned int n_sds_p_int2 = get_num_sds(wd->n_shells, wd->n_proton_f - 2);
  unsigned int n_sds_n_int1 = get_num_sds(wd->n_shells, wd->n_neutron_f - 1);
  unsigned int n_sds_n_int2 = get_num_sds(wd->n_shells, wd->n_neutron_f - 2);

  printf("Intermediate SDs: p1: %d n1: %d, p2: %d, n2: %d\n", n_sds_p_int1, n_sds_n_int1, n_sds_p_int2, n_sds_n_int2);
  printf("%d, %d, %d, %d\n", wd->n_proton_i, wd->n_proton_f, wd->n_neutron_i, wd->n_neutron_f);
  // Determine the min and max mj values of proton/neutron SDs
  float mj_min_p_i = min_mj(ns, wd->n_proton_i, wd->jz_shell);
  float mj_max_p_i = max_mj(ns, wd->n_proton_i, wd->jz_shell);
  float mj_min_n_i = min_mj(ns, wd->n_neutron_i, wd->jz_shell);
  float mj_max_n_i = max_mj(ns, wd->n_neutron_i, wd->jz_shell);
  
  printf("%g, %g, %g, %g\n", mj_min_p_i, mj_max_p_i, mj_min_n_i, mj_max_n_i);
  if (fabs(((int) (2*wd->j_nuc_i[0])) % 2) > pow(10, -3)) {
    // half-integer mj case
    mj_min_p_i = MAX(mj_min_p_i, -mj_max_n_i + 0.5);
    mj_max_p_i = MIN(mj_max_p_i, -mj_min_n_i + 0.5);
    mj_min_n_i = MAX(mj_min_n_i, -mj_max_p_i + 0.5);
    mj_max_n_i = MIN(mj_max_n_i, -mj_min_p_i + 0.5);
  } else {
  // Account for the factor that m_p + m_n = 0 in determining min/max mj
    mj_min_p_i = MAX(mj_min_p_i, -mj_max_n_i);
    mj_max_p_i = MIN(mj_max_p_i, -mj_min_n_i);
    mj_min_n_i = MAX(mj_min_n_i, -mj_max_p_i);
    mj_max_n_i = MIN(mj_max_n_i, -mj_min_p_i);
  }

  // Determine number of sectors (mp, mn)
  int num_mj_i = mj_max_p_i - mj_min_p_i + 1;
  printf("Number of m_j sectors:%d\n", num_mj_i);
  printf("%g, %g, %g, %g\n", mj_min_p_i, mj_max_p_i, mj_min_n_i, mj_max_n_i);

  // Allocate space for jump lists
  wf_list **p0_list_i = (wf_list**) calloc(2*num_mj_i, sizeof(wf_list*));
  wf_list **n0_list_i = (wf_list**) calloc(2*num_mj_i, sizeof(wf_list*));
  sd_list **p1_list_i = (sd_list**) calloc(2*ns*num_mj_i, sizeof(sd_list*));
  sd_list **n1_list_i = (sd_list**) calloc(2*ns*num_mj_i, sizeof(sd_list*));
  sd_list **p2_list_i = (sd_list**) calloc(2*ns*ns*num_mj_i, sizeof(sd_list*));
  sd_list **n2_list_i = (sd_list**) calloc(2*ns*ns*num_mj_i, sizeof(sd_list*));
  sd_list **p2_list_f = (sd_list**) calloc(2*ns*ns*num_mj_i, sizeof(sd_list*));
  sd_list **n2_list_f = (sd_list**) calloc(2*ns*ns*num_mj_i, sizeof(sd_list*));
  int* p1_array_f = (int*) calloc(ns*n_sds_p_int1, sizeof(int));
  int* p2_array_f = (int*) calloc(ns*ns*n_sds_p_int2, sizeof(int));
  int* n1_array_f = (int*) calloc(ns*n_sds_n_int1, sizeof(int));
  int* n2_array_f = (int*) calloc(ns*ns*n_sds_n_int2, sizeof(int));


  // Determine one/two-body jumps, using special routine if initial and final bases are the same
  if (wd->same_basis) {
    printf("Building proton jumps...\n");
    build_two_body_jumps_i_and_f_trunc(wd->n_shells, wd->n_proton_i, mj_min_p_i, mj_max_p_i, num_mj_i, wd->n_sds_p_i, n_sds_p_int1, n_sds_p_int2, p1_array_f, p2_array_f, p0_list_i, p1_list_i, p2_list_i, wd->jz_shell, wd->l_shell, wd->w_shell, 8);
    printf("Done.\n");
    printf("Building neutron jumps...\n");
    build_two_body_jumps_i_and_f_trunc(wd->n_shells, wd->n_neutron_i, mj_min_n_i, mj_max_n_i, num_mj_i, wd->n_sds_n_i, n_sds_n_int1, n_sds_n_int2, n1_array_f, n2_array_f, n0_list_i, n1_list_i, n2_list_i, wd->jz_shell, wd->l_shell, wd->w_shell, 8);
    printf("Done.\n");
  } else {
    printf("Building initial state proton jumps...\n");
    build_two_body_jumps_i(wd->n_shells, wd->n_proton_i, mj_min_p_i, mj_max_p_i, num_mj_i, wd->n_sds_p_i, p0_list_i, p1_list_i, p2_list_i, wd->jz_shell, wd->l_shell);
    printf("Done\n");
    printf("Building final state proton jumps...\n");
    build_two_body_jumps_f(wd->n_shells, wd->n_proton_f, mj_min_p_i, mj_max_p_i, num_mj_i, wd->n_sds_p_f, n_sds_p_int1, n_sds_p_int2, p1_array_f, p2_array_f, p2_list_f, wd->jz_shell, wd->l_shell);
    printf("Done\n");
    printf("Building initial state neutron jumps...\n");
    build_two_body_jumps_i(wd->n_shells, wd->n_neutron_i, mj_min_n_i, mj_max_n_i, num_mj_i, wd->n_sds_n_i, n0_list_i, n1_list_i, n2_list_i, wd->jz_shell, wd->l_shell);
    printf("Building final state neutron jumps...\n");
    build_two_body_jumps_f(wd->n_shells, wd->n_neutron_f, mj_min_n_i, mj_max_n_i, num_mj_i, wd->n_sds_n_f, n_sds_n_int1, n_sds_n_int2, n1_array_f, n2_array_f, n2_list_f, wd->jz_shell, wd->l_shell);
    printf("Done.\n");
  }

  double* cg_fact = (double*) calloc(sp->n_trans, sizeof(double));
  float mti = 0.5*(wd->n_proton_i - wd->n_neutron_i);
  float mtf = 0.5*(wd->n_proton_f - wd->n_neutron_f);
  float mt_op = mtf - mti;
  if (fabs(mt_op) > t_op) {printf("Error: operator iso-spin is insufficient to mediate a transition between the given nuclides.\n"); exit(0);}
  FILE *out_file;
  char output_density_file[100];
  char output_log_file[100];
  char output_suffix[100];
  strcpy(output_log_file, sp->out_file_base);
  strcat(output_log_file, ".log");

  // Loop over transitions
  eigen_list* trans = sp->transition_list;
  int i_trans = 0;
  while (trans != NULL) {
    int psi_i = trans->eig_i;
    int psi_f = trans->eig_f;
    float ji = wd->j_nuc_i[psi_i];
    float ti = wd->t_nuc_i[psi_i];
    double cg_j = 0.0;
    double cg_t = 0.0;
    float jf = wd->j_nuc_f[psi_f];
    float tf = wd->t_nuc_f[psi_f];
    int ijf = 2*jf;
    float mj = 0.0;
    if (ijf % 2) {
      mj = 0.5;
    }
    cg_j = clebsch_gordan(j_op, ji, jf, 0, mj, mj);
    if (cg_j == 0.0) {trans = trans->next; continue;}
    cg_t = clebsch_gordan(t_op, ti, tf, mt_op, mti, mtf);
    if (cg_t == 0.0) {trans = trans->next; continue;}
    cg_j *= pow(-1.0, jf - ji)/sqrt(2*jf + 1);
    cg_t *= pow(-1.0, tf - ti)/sqrt(2*tf + 1);
    printf("Initial state: # %d J: %g T: %g Final state: # %d J: %g T: %g\n", psi_i + 1, ji, ti, psi_f + 1, jf, tf);
    strcpy(output_density_file, sp->out_file_base);
    sprintf(output_suffix, "_J%d_T%d_%d_%d.dens", j_op, t_op, psi_i, psi_f);
    strcat(output_density_file, output_suffix);
    out_file = fopen(output_density_file, "w"); 
    cg_fact[i_trans] = cg_j*cg_t;
    i_trans++;
    trans = trans->next;
  }
  
  double* j_store = (double*) malloc(4*sizeof(double));
  double* density = (double*) calloc(sp->n_trans, sizeof(double));
  // Loop over orbital a
  for (int i_orb1 = 0; i_orb1 < wd->n_orbits; i_orb1++) {
    float j1 = wd->j_orb[i_orb1];
    // Loop over orbital b
    for (int i_orb2 = 0; i_orb2 <= i_orb1; i_orb2++) {
      float j2 = wd->j_orb[i_orb2];
      // Loop over orbital c
      for (int i_orb3 = 0; i_orb3 < wd->n_orbits; i_orb3++) {
        float j4 = wd->j_orb[i_orb3];
        // Loop over orbital d
        for (int i_orb4 = 0; i_orb4 <= i_orb3; i_orb4++) {
          printf("%d %d %d %d\n", i_orb1, i_orb2, i_orb3, i_orb4);
          if (pow(-1.0, wd->l_orb[i_orb1] + wd->l_orb[i_orb2] + wd->l_orb[i_orb3] + wd->l_orb[i_orb4]) != 1.0) {continue;} // Parity
          float j3 = wd->j_orb[i_orb4];
          // Allocate storage for each j12 and j34
          int j_min_12 = abs(j1 - j2);
          int j_max_12 = j1 + j2;
          int j_dim_12 = j_max_12 - j_min_12 + 1;
          int j_min_34 = abs(j3 - j4);
          int j_max_34 = j3 + j4;
          int j_dim_34 = j_max_34 - j_min_34 + 1;
          int j_dim = j_dim_12*j_dim_34;
          if (j_op > j_max_12 + j_max_34) {printf("max: %g, %g, %g, %g\n", j1, j2, j3, j4); continue;}
          int j_tot_min = j_max_12 + j_max_34;
          for (int j12 = j_min_12; j12 <= j_max_12; j12++) {
            for (int j34 = j_min_34; j34 <= j_max_34; j34++) {
              if (abs(j12 - j34) < j_tot_min) {j_tot_min = abs(j12 - j34);}
            }
          }
          if (j_op < j_tot_min) {printf("min: %g, %g, %g, %g\n", j1, j2, j3, j4); continue;}
          j_store = realloc(j_store, sizeof(double)*4*j_dim*sp->n_trans);
          for (int k = 0; k < 4*j_dim*sp->n_trans; k++) {
            j_store[k] = 0.0;
          }

          // Loop over shells for orbit a
          for (int ia = 0; ia < 2*wd->n_shells; ia++) {
            float mt1 = 0.5;
            int a = ia;
            if (a >= ns) {a -= ns; mt1 -= 1;}
            if (wd->l_shell[a] != wd->l_orb[i_orb1]) {continue;}
            if (wd->n_shell[a] != wd->n_orb[i_orb1]) {continue;}
            if (wd->j_shell[a]/2.0 != j1) {continue;}
            float mj1 = wd->jz_shell[a]/2.0;
            // Loop over shells for orbit b
            for (int ib = 0; ib < 2*wd->n_shells; ib++) {
              if (ib == ia) {continue;}
              float mt2 = 0.5;
              int b = ib;
              if (b >= ns) {b -= ns; mt2 -= 1;}
              if (wd->l_shell[b] != wd->l_orb[i_orb2]) {continue;}
              if (wd->n_shell[b] != wd->n_orb[i_orb2]) {continue;}
              if (wd->j_shell[b]/2.0 != j2) {continue;}
              float mj2 = wd->jz_shell[b]/2.0;
              if ((i_orb1 == i_orb2) && (a < b) && (mt1 == mt2)) {continue;}
              for (int ic = 0; ic < 2*wd->n_shells; ic++) {
                float mt4 = 0.5;
                int c = ic;
                if (c >= ns) {c -= ns; mt4 -= 1;}
                if (wd->l_shell[c] != wd->l_orb[i_orb3]) {continue;}
                if (wd->n_shell[c] != wd->n_orb[i_orb3]) {continue;}
                if (wd->j_shell[c]/2.0 != j4) {continue;}
                float mj4 = wd->jz_shell[c]/2.0;
                // Loop over shells for orbit d
                for (int id = 0; id < 2*wd->n_shells; id++) {
                  if (ic == id) {continue;}
                  int d = id;
                  float mt3 = 0.5;
                  if (d >= ns) {d -= ns; mt3 -= 1;}
                  if (wd->l_shell[d] != wd->l_orb[i_orb4]) {continue;}
                  if (wd->n_shell[d] != wd->n_orb[i_orb4]) {continue;}
                  if (wd->j_shell[d]/2.0 != j3) {continue;}
                  float mj3 = wd->jz_shell[d]/2.0;
                  if ((i_orb3 == i_orb4) && (c < d) && (mt3 == mt4)) {continue;}
                  if (mj1 + mj2 != mj3 + mj4) {continue;}
		  // New test line
		//  if (mt3 == mt2 && i_orb4 == i_orb2 && mj3 == mj2) {continue;}
                //  if (mt4 == mt2 && i_orb3 == i_orb2 && mj4 == mj2) {continue;}
                  if (mt1 + mt2 - mt3 - mt4 != mt_op) {continue;}
                  for (int i = 0; i < sp->n_trans; i++) {density[i] = 0.0;}
                  if ((mt3 == 0.5) && (mt4 == 0.5)) { // 
                    if ((mt1 == 0.5) && (mt2 == 0.5)) { // 2 proton creation operators + 2 proton annihilation operators
                      trace_a4_nodes(a, b, c, d, num_mj_i, n_sds_p_int2, p2_array_f, p2_list_i, n0_list_i, wd, 0, sp->transition_list, density);
                    } else if ((mt1 == -0.5) && (mt2 == -0.5)) { // 2 neutron creation operators and two proton ann. operators
                      if ((fabs(mj1 + mj2) > (mj_max_n_i - mj_min_n_i)) || (fabs(mj3 + mj4) > (mj_max_p_i - mj_min_p_i))) {printf("Saved time\n"); continue;}
                      trace_a22_nodes(a, b, c, d, num_mj_i, p2_list_i, n2_list_f, wd, 0, sp->transition_list, density);
                    }  
                  } else if ((mt3 == -0.5) && (mt4 == -0.5)) {
                    if ((mt1 == -0.5) && (mt2 == -0.5)) { //2 n cr. and 2 n ann. operators
                      trace_a4_nodes(a, b, c, d, num_mj_i, n_sds_n_int2, n2_array_f, n2_list_i, p0_list_i, wd, 1, sp->transition_list, density);
                    } else if ((mt1 == 0.5) && (mt2 == 0.5)) {// 2 p cr. and 2 n ann. operators
                      if ((fabs(mj1 + mj2) > (mj_max_p_i - mj_min_p_i)) || (fabs(mj3 + mj4) > (mj_max_n_i - mj_min_n_i))) {printf("Saved time\n");continue;}
                      trace_a22_nodes(a, b, c, d, num_mj_i, n2_list_i, p2_list_f, wd, 1, sp->transition_list, density);
                    }
                  } else if ((mt1 == 0.5) && (mt2 == -0.5) && (mt3 == 0.5) && (mt4 == -0.5)) {
                      trace_a20_nodes(a, d, b, c, num_mj_i, n_sds_p_int1, n_sds_n_int1, p1_list_i, n1_list_i, p1_array_f, n1_array_f, wd, sp->transition_list, density);
                      for (int i = 0; i < sp->n_trans; i++) {density[i] *= -1.0;}
                  } else if ((mt1 == -0.5) && (mt2 == 0.5) && (mt3 == 0.5) && (mt4 == -0.5)) {
                      trace_a20_nodes(b, d, a, c, num_mj_i, n_sds_p_int1, n_sds_n_int1, p1_list_i, n1_list_i, p1_array_f, n1_array_f, wd, sp->transition_list, density);
                  } else if ((mt1 == 0.5) && (mt2 == -0.5) && (mt3 == -0.5) && (mt4 == 0.5)) {
                      trace_a20_nodes(a, c, b, d, num_mj_i, n_sds_p_int1, n_sds_n_int1,p1_list_i, n1_list_i, p1_array_f, n1_array_f, wd, sp->transition_list, density);
                  } else if ((mt1 == -0.5) && (mt2 == 0.5) && (mt3 == -0.5) && (mt4 == 0.5)) {
                      trace_a20_nodes(b, c, a, d, num_mj_i, n_sds_p_int1, n_sds_n_int1, p1_list_i, n1_list_i, p1_array_f, n1_array_f, wd, sp->transition_list, density);
                      for (int i = 0; i < sp->n_trans; i++) {density[i] *= -1.0;}
                  }
                  for (int j12 = j_min_12; j12 <= j_max_12; j12++) {
                    if ((mj1 + mj2 > j12) || (mj1 + mj2 < -j12)) {continue;}
                    float cg_j12 = clebsch_gordan(j1, j2, j12, mj1, mj2, mj1 + mj2);
                    if (cg_j12 == 0.0) {continue;}
                    for (int t12 = 0; t12 <= 1; t12++) {
                      if ((mt1 + mt2 > t12) || (mt1 + mt2 < -t12)) {continue;}
                      float cg_t12 = clebsch_gordan(0.5, 0.5, t12, mt1, mt2, mt1 + mt2);
                      for (int j34 = j_min_34; j34 <= j_max_34; j34++) {
                        if ((mj3 + mj4 > j34) || (mj3 + mj4 < -j34)){continue;}
                        float cg_j34 = clebsch_gordan(j4, j3, j34, mj4, mj3, mj3 + mj4);
                        if (cg_j34 == 0.0) {continue;}
                        float cg_jop = clebsch_gordan(j_op, j34, j12, 0, mj3 + mj4, mj1 + mj2);
                        if (cg_jop == 0.0) {continue;}
                        for (int t34 = 0; t34 <= 1; t34++) {
                          if ((mt3 + mt4 > t34) || (mt3 + mt4 < -t34)) {continue;}
                          float cg_t34 = clebsch_gordan(0.5, 0.5, t34, mt4, mt3, mt3 + mt4);
                          if (cg_t34 == 0.0) {continue;}
                          float cg_top = clebsch_gordan(t_op, t34, t12, mt_op, mt3 + mt4, mt1 + mt2);
                          if (cg_top == 0.0) {continue;}
                          double d2 = cg_j12*cg_j34*cg_jop;
                          d2 *= cg_t12*cg_t34*cg_top;
                          d2 *= 0.25*pow(-1.0, -j34 + j12 - t34 + t12)/sqrt((2*j12 + 1)*(2*t12 + 1));
                          if (i_orb1 == i_orb2) {d2 *= sqrt(2);}
                          if (i_orb3 == i_orb4) {d2 *= sqrt(2);}
                          if (d2 == 0.0) {continue;}

                          for (int i = 0; i < sp->n_trans; i++) {
                            j_store[i + sp->n_trans*(t12 + 2*(t34 + 2*((j12 - j_min_12)*j_dim_34 + (j34 - j_min_34))))] += density[i]*d2/cg_fact[i];
                            if ((i_orb1 == i_orb2) && (mt1 == mt2)) {
                              j_store[i + sp->n_trans*(t12 + 2*(t34 + 2*((j12 - j_min_12)*j_dim_34 + (j34 - j_min_34))))] += pow(-1.0, j1 + j2 - j12 - t12)*density[i]*d2/cg_fact[i];
                            }
                            if ((i_orb3 == i_orb4) && (mt3 == mt4)) {
                              j_store[i + sp->n_trans*(t12 + 2*(t34 + 2*((j12 - j_min_12)*j_dim_34 + (j34 - j_min_34))))] += pow(-1.0, j3 + j4 - j34 - t34)*density[i]*d2/cg_fact[i];
                            }
                            if ((i_orb1 == i_orb2) && (mt1 == mt2) && (i_orb3 == i_orb4) && (mt3 == mt4)) {
                              j_store[i + sp->n_trans*(t12 + 2*(t34 + 2*((j12 - j_min_12)*j_dim_34 + (j34 - j_min_34))))] += pow(-1.0, j1 + j2 + j3 + j4 - j12 - j34 - t12 - t34)*density[i]*d2/cg_fact[i];
                            }
                          }
                        }
                      }
                    }
                  }
                }
              }
            }
          } 
          trans = sp->transition_list;
          i_trans = 0;
          while (trans != NULL) {
            int psi_i = trans->eig_i;
            int psi_f = trans->eig_f;
            strcpy(output_density_file, sp->out_file_base);
            sprintf(output_suffix, "_J%d_T%d_%d_%d.dens", j_op, t_op, psi_i, psi_f);
            strcat(output_density_file, output_suffix);
            out_file = fopen(output_density_file, "a"); 

            for (int t12 = 0; t12 <= 1; t12++) {
              for (int t34 = 0; t34 <= 1; t34++) {
                for (int ij12 = 0; ij12 < j_dim_12; ij12++) {
                  for (int ij34 = 0; ij34 < j_dim_34; ij34++) {
                    if (fabs(j_store[i_trans + sp->n_trans*(t12 + 2*(t34 + 2*(ij12*j_dim_34 + ij34)))]) < pow(10, -16)) {continue;}

                    fprintf(out_file, "%d,%g,%d,%g,%d,%d,%d,%g,%d,%g,%d,%d,%g\n", 2*wd->n_orb[i_orb1] + wd->l_orb[i_orb1], 2*wd->j_orb[i_orb1], 2*wd->n_orb[i_orb2] + wd->l_orb[i_orb2], 2*wd->j_orb[i_orb2], 2*(ij12 + j_min_12), 2*t12, 2*wd->n_orb[i_orb3] + wd->l_orb[i_orb3], 2*wd->j_orb[i_orb3], 2*wd->n_orb[i_orb4] + wd->l_orb[i_orb4], 2*wd->j_orb[i_orb4], 2*(ij34 + j_min_34), 2*t34, j_store[i_trans + sp->n_trans*(t12 + 2*(t34 + 2*(ij12*j_dim_34 + ij34)))]);

                    if (i_orb1 != i_orb2) { 
                      fprintf(out_file, "%d,%g,%d,%g,%d,%d,%d,%g,%d,%g,%d,%d,%g\n", 2*wd->n_orb[i_orb2] + wd->l_orb[i_orb2], 2*wd->j_orb[i_orb2], 2*wd->n_orb[i_orb1] + wd->l_orb[i_orb1], 2*wd->j_orb[i_orb1], 2*(ij12 + j_min_12), 2*t12, 2*wd->n_orb[i_orb3] + wd->l_orb[i_orb3], 2*wd->j_orb[i_orb3], 2*wd->n_orb[i_orb4] + wd->l_orb[i_orb4], 2*wd->j_orb[i_orb4], 2*(ij34 + j_min_34), 2*t34, pow(-1.0, j1 + j2 - ij12 - j_min_12 - t12)*j_store[i_trans + sp->n_trans*(t12 + 2*(t34 + 2*(ij12*j_dim_34 + ij34)))]);
                    }
                    if (i_orb3 != i_orb4) {
                        fprintf(out_file, "%d,%g,%d,%g,%d,%d,%d,%g,%d,%g,%d,%d,%g\n", 2*wd->n_orb[i_orb1] + wd->l_orb[i_orb1], 2*wd->j_orb[i_orb1], 2*wd->n_orb[i_orb2] + wd->l_orb[i_orb2], 2*wd->j_orb[i_orb2], 2*(ij12 + j_min_12), 2*t12, 2*wd->n_orb[i_orb4] + wd->l_orb[i_orb4], 2*wd->j_orb[i_orb4], 2*wd->n_orb[i_orb3] + wd->l_orb[i_orb3], 2*wd->j_orb[i_orb3], 2*(ij34 + j_min_34), 2*t34, pow(-1.0, j3 + j4 - ij34 - j_min_34 - t34)*j_store[i_trans + sp->n_trans*(t12 + 2*(t34 + 2*(ij12*j_dim_34 + ij34)))]);
                    }
                    if ((i_orb1 != i_orb2) && (i_orb3 != i_orb4)) {
                      fprintf(out_file, "%d,%g,%d,%g,%d,%d,%d,%g,%d,%g,%d,%d,%g\n", 2*wd->n_orb[i_orb2] + wd->l_orb[i_orb2], 2*wd->j_orb[i_orb2], 2*wd->n_orb[i_orb1] + wd->l_orb[i_orb1], 2*wd->j_orb[i_orb1], 2*(ij12 + j_min_12), 2*t12, 2*wd->n_orb[i_orb4] + wd->l_orb[i_orb4], 2*wd->j_orb[i_orb4], 2*wd->n_orb[i_orb3] + wd->l_orb[i_orb3], 2*wd->j_orb[i_orb3], 2*(ij34 + j_min_34), 2*t34, pow(-1.0, j1 + j2 + j3 + j4 - ij12 - j_min_12 - ij34 - j_min_34 - t12 - t34)*j_store[i_trans + sp->n_trans*(t12 + 2*(t34 + 2*(ij12*j_dim_34 + ij34)))]);

                    }
                  }
                }
              }
            }
            trans = trans->next;
            i_trans++;
            fclose(out_file);
          }
        }
      }
    }
  } 
  free(j_store); 

  return;
}


void two_body_density(speedParams *sp) {

  // Read in data 
  char wfn_file_initial[100];
  strcpy(wfn_file_initial, sp->initial_file_base);
  strcat(wfn_file_initial, ".wfn");
  char wfn_file_final[100];
  strcpy(wfn_file_final, sp->final_file_base);
  strcat(wfn_file_final, ".wfn");
  char basis_file_initial[100];
  strcpy(basis_file_initial, sp->initial_file_base);
  strcat(basis_file_initial, ".bas");
  char basis_file_final[100];
  strcpy(basis_file_final, sp->final_file_base);
  strcat(basis_file_final, ".bas");
  wfnData *wd = read_binary_wfn_data(wfn_file_initial, wfn_file_final, basis_file_initial, basis_file_final);

  int j_op = sp->j_op;
  int t_op = sp->t_op;
  int ns = wd->n_shells;

  // Determine number of intermediate Slater determinants
  // after the action of one or two annihilation operators
  unsigned int n_sds_p_int1 = get_num_sds(wd->n_shells, wd->n_proton_f - 1);
  unsigned int n_sds_p_int2 = get_num_sds(wd->n_shells, wd->n_proton_f - 2);
  unsigned int n_sds_n_int1 = get_num_sds(wd->n_shells, wd->n_neutron_f - 1);
  unsigned int n_sds_n_int2 = get_num_sds(wd->n_shells, wd->n_neutron_f - 2);

  printf("Intermediate SDs: p1: %d n1: %d, p2: %d, n2: %d\n", n_sds_p_int1, n_sds_n_int1, n_sds_p_int2, n_sds_n_int2);
  printf("%d, %d, %d, %d\n", wd->n_proton_i, wd->n_proton_f, wd->n_neutron_i, wd->n_neutron_f);
  // Determine the min and max mj values of proton/neutron SDs
  float mj_min_p_i = min_mj(ns, wd->n_proton_i, wd->jz_shell);
  float mj_max_p_i = max_mj(ns, wd->n_proton_i, wd->jz_shell);
  float mj_min_n_i = min_mj(ns, wd->n_neutron_i, wd->jz_shell);
  float mj_max_n_i = max_mj(ns, wd->n_neutron_i, wd->jz_shell);
  
  printf("%g, %g, %g, %g\n", mj_min_p_i, mj_max_p_i, mj_min_n_i, mj_max_n_i);
  if (fabs(((int) (2*wd->j_nuc_i[0])) % 2) > pow(10, -3)) {
    // half-integer mj case
    mj_min_p_i = MAX(mj_min_p_i, -mj_max_n_i + 0.5);
    mj_max_p_i = MIN(mj_max_p_i, -mj_min_n_i + 0.5);
    mj_min_n_i = MAX(mj_min_n_i, -mj_max_p_i + 0.5);
    mj_max_n_i = MIN(mj_max_n_i, -mj_min_p_i + 0.5);
  } else {
  // Account for the factor that m_p + m_n = 0 in determining min/max mj
    mj_min_p_i = MAX(mj_min_p_i, -mj_max_n_i);
    mj_max_p_i = MIN(mj_max_p_i, -mj_min_n_i);
    mj_min_n_i = MAX(mj_min_n_i, -mj_max_p_i);
    mj_max_n_i = MIN(mj_max_n_i, -mj_min_p_i);
  }
  // Determine number of sectors (mp, mn)
  int num_mj_i = mj_max_p_i - mj_min_p_i + 1;
  printf("Number of m_j sectors:%d\n", num_mj_i);
  printf("%g, %g, %g, %g\n", mj_min_p_i, mj_max_p_i, mj_min_n_i, mj_max_n_i);

  // Allocate space for jump lists
  wf_list **p0_list_i = (wf_list**) calloc(2*num_mj_i, sizeof(wf_list*));
  wf_list **n0_list_i = (wf_list**) calloc(2*num_mj_i, sizeof(wf_list*));
  sd_list **p1_list_i = (sd_list**) calloc(2*ns*num_mj_i, sizeof(sd_list*));
  sd_list **n1_list_i = (sd_list**) calloc(2*ns*num_mj_i, sizeof(sd_list*));
  sd_list **p2_list_i = (sd_list**) calloc(2*ns*ns*num_mj_i, sizeof(sd_list*));
  sd_list **n2_list_i = (sd_list**) calloc(2*ns*ns*num_mj_i, sizeof(sd_list*));
  sd_list **p2_list_f = (sd_list**) calloc(2*ns*ns*num_mj_i, sizeof(sd_list*));
  sd_list **n2_list_f = (sd_list**) calloc(2*ns*ns*num_mj_i, sizeof(sd_list*));
  int* p1_array_f = (int*) calloc(ns*n_sds_p_int1, sizeof(int));
  int* p2_array_f = (int*) calloc(ns*ns*n_sds_p_int2, sizeof(int));
  int* n1_array_f = (int*) calloc(ns*n_sds_n_int1, sizeof(int));
  int* n2_array_f = (int*) calloc(ns*ns*n_sds_n_int2, sizeof(int));

  // Determine one/two-body jumps, using special routine if initial and final bases are the same
  if (wd->same_basis) {
    printf("Building proton jumps...\n");
    build_two_body_jumps_i_and_f(wd->n_shells, wd->n_proton_i, mj_min_p_i, mj_max_p_i, num_mj_i, wd->n_sds_p_i, n_sds_p_int1, n_sds_p_int2, p1_array_f, p2_array_f, p0_list_i, p1_list_i, p2_list_i, wd->jz_shell, wd->l_shell);
    printf("Done.\n");
    printf("Building neutron jumps...\n");
    build_two_body_jumps_i_and_f(wd->n_shells, wd->n_neutron_i, mj_min_n_i, mj_max_n_i, num_mj_i, wd->n_sds_n_i, n_sds_n_int1, n_sds_n_int2, n1_array_f, n2_array_f, n0_list_i, n1_list_i, n2_list_i, wd->jz_shell, wd->l_shell);
    printf("Done.\n");
  } else {
    printf("Building initial state proton jumps...\n");
    build_two_body_jumps_i(wd->n_shells, wd->n_proton_i, mj_min_p_i, mj_max_p_i, num_mj_i, wd->n_sds_p_i, p0_list_i, p1_list_i, p2_list_i, wd->jz_shell, wd->l_shell);
    printf("Done\n");
    printf("Building final state proton jumps...\n");
    build_two_body_jumps_f(wd->n_shells, wd->n_proton_f, mj_min_p_i, mj_max_p_i, num_mj_i, wd->n_sds_p_f, n_sds_p_int1, n_sds_p_int2, p1_array_f, p2_array_f, p2_list_f, wd->jz_shell, wd->l_shell);
    printf("Done\n");
    printf("Building initial state neutron jumps...\n");
    build_two_body_jumps_i(wd->n_shells, wd->n_neutron_i, mj_min_n_i, mj_max_n_i, num_mj_i, wd->n_sds_n_i, n0_list_i, n1_list_i, n2_list_i, wd->jz_shell, wd->l_shell);
    printf("Building final state neutron jumps...\n");
    build_two_body_jumps_f(wd->n_shells, wd->n_neutron_f, mj_min_n_i, mj_max_n_i, num_mj_i, wd->n_sds_n_f, n_sds_n_int1, n_sds_n_int2, n1_array_f, n2_array_f, n2_list_f, wd->jz_shell, wd->l_shell);
    printf("Done.\n");
  }

  double* cg_fact = (double*) calloc(sp->n_trans, sizeof(double));
  float mti = 0.5*(wd->n_proton_i - wd->n_neutron_i);
  float mtf = 0.5*(wd->n_proton_f - wd->n_neutron_f);
  float mt_op = mtf - mti;
  if (fabs(mt_op) > t_op) {printf("Error: operator iso-spin is insufficient to mediate a transition between the given nuclides.\n"); exit(0);}
  FILE *out_file;
  char output_density_file[100];
  char output_log_file[100];
  char output_suffix[100];
  strcpy(output_log_file, sp->out_file_base);
  strcat(output_log_file, ".log");

  // Loop over transitions
  eigen_list* trans = sp->transition_list;
  int i_trans = 0;
  while (trans != NULL) {
    int psi_i = trans->eig_i;
    int psi_f = trans->eig_f;
    float ji = wd->j_nuc_i[psi_i];
    float ti = wd->t_nuc_i[psi_i];
    double cg_j = 0.0;
    double cg_t = 0.0;
    float jf = wd->j_nuc_f[psi_f];
    float tf = wd->t_nuc_f[psi_f];
    int ijf = 2*jf;
    float mj = 0.0;
    if (ijf % 2) {
      mj = 0.5;
    }
    cg_j = clebsch_gordan(j_op, ji, jf, 0, mj, mj);
    if (cg_j == 0.0) {trans = trans->next; continue;}
    cg_t = clebsch_gordan(t_op, ti, tf, mt_op, mti, mtf);
    if (cg_t == 0.0) {trans = trans->next; continue;}
    cg_j *= pow(-1.0, jf - ji)/sqrt(2*jf + 1);
    cg_t *= pow(-1.0, tf - ti)/sqrt(2*tf + 1);
    printf("Initial state: # %d J: %g T: %g Final state: # %d J: %g T: %g\n", psi_i + 1, ji, ti, psi_f + 1, jf, tf);
    strcpy(output_density_file, sp->out_file_base);
    sprintf(output_suffix, "_J%d_T%d_%d_%d.dens", j_op, t_op, psi_i, psi_f);
    strcat(output_density_file, output_suffix);
    out_file = fopen(output_density_file, "w"); 
    cg_fact[i_trans] = cg_j*cg_t;
    i_trans++;
    trans = trans->next;
  }
  
  double* j_store = (double*) malloc(4*sizeof(double));
  double* density = (double*) calloc(sp->n_trans, sizeof(double));
  // Loop over orbital a
  for (int i_orb1 = 0; i_orb1 < wd->n_orbits; i_orb1++) {
    float j1 = wd->j_orb[i_orb1];
    // Loop over orbital b
    for (int i_orb2 = 0; i_orb2 <= i_orb1; i_orb2++) {
      float j2 = wd->j_orb[i_orb2];
      // Loop over orbital c
      for (int i_orb3 = 0; i_orb3 < wd->n_orbits; i_orb3++) {
        float j4 = wd->j_orb[i_orb3];
        // Loop over orbital d
        for (int i_orb4 = 0; i_orb4 <= i_orb3; i_orb4++) {
          printf("%d %d %d %d\n", i_orb1, i_orb2, i_orb3, i_orb4);
          if (pow(-1.0, wd->l_orb[i_orb1] + wd->l_orb[i_orb2] + wd->l_orb[i_orb3] + wd->l_orb[i_orb4]) != 1.0) {continue;} // Parity
          float j3 = wd->j_orb[i_orb4];
          // Allocate storage for each j12 and j34
          int j_min_12 = abs(j1 - j2);
          int j_max_12 = j1 + j2;
          int j_dim_12 = j_max_12 - j_min_12 + 1;
          int j_min_34 = abs(j3 - j4);
          int j_max_34 = j3 + j4;
          int j_dim_34 = j_max_34 - j_min_34 + 1;
          int j_dim = j_dim_12*j_dim_34;
          if (j_op > j_max_12 + j_max_34) {printf("max: %g, %g, %g, %g\n", j1, j2, j3, j4); continue;}
          int j_tot_min = j_max_12 + j_max_34;
          for (int j12 = j_min_12; j12 <= j_max_12; j12++) {
            for (int j34 = j_min_34; j34 <= j_max_34; j34++) {
              if (abs(j12 - j34) < j_tot_min) {j_tot_min = abs(j12 - j34);}
            }
          }
          if (j_op < j_tot_min) {printf("min: %g, %g, %g, %g\n", j1, j2, j3, j4); continue;}
          j_store = realloc(j_store, sizeof(double)*4*j_dim*sp->n_trans);
          for (int k = 0; k < 4*j_dim*sp->n_trans; k++) {
            j_store[k] = 0.0;
          }

          // Loop over shells for orbit a
          for (int ia = 0; ia < 2*wd->n_shells; ia++) {
            float mt1 = 0.5;
            int a = ia;
            if (a >= ns) {a -= ns; mt1 -= 1;}
            if (wd->l_shell[a] != wd->l_orb[i_orb1]) {continue;}
            if (wd->n_shell[a] != wd->n_orb[i_orb1]) {continue;}
            if (wd->j_shell[a]/2.0 != j1) {continue;}
            float mj1 = wd->jz_shell[a]/2.0;
            // Loop over shells for orbit b
            for (int ib = 0; ib < 2*wd->n_shells; ib++) {
              if (ib == ia) {continue;}
              float mt2 = 0.5;
              int b = ib;
              if (b >= ns) {b -= ns; mt2 -= 1;}
              if (wd->l_shell[b] != wd->l_orb[i_orb2]) {continue;}
              if (wd->n_shell[b] != wd->n_orb[i_orb2]) {continue;}
              if (wd->j_shell[b]/2.0 != j2) {continue;}
              float mj2 = wd->jz_shell[b]/2.0;
             // if ((i_orb1 == i_orb2) && (a < b) && (mt1 == mt2)) {continue;}
              for (int ic = 0; ic < 2*wd->n_shells; ic++) {
                float mt4 = 0.5;
                int c = ic;
                if (c >= ns) {c -= ns; mt4 -= 1;}
                if (wd->l_shell[c] != wd->l_orb[i_orb3]) {continue;}
                if (wd->n_shell[c] != wd->n_orb[i_orb3]) {continue;}
                if (wd->j_shell[c]/2.0 != j4) {continue;}
                float mj4 = wd->jz_shell[c]/2.0;
                // Loop over shells for orbit d
                for (int id = 0; id < 2*wd->n_shells; id++) {
                  if (ic == id) {continue;}
                  int d = id;
                  float mt3 = 0.5;
                  if (d >= ns) {d -= ns; mt3 -= 1;}
                  if (wd->l_shell[d] != wd->l_orb[i_orb4]) {continue;}
                  if (wd->n_shell[d] != wd->n_orb[i_orb4]) {continue;}
                  if (wd->j_shell[d]/2.0 != j3) {continue;}
                  float mj3 = wd->jz_shell[d]/2.0;
                //  if ((i_orb3 == i_orb4) && (c < d) && (mt3 == mt4)) {continue;}
                  if (mj1 + mj2 != mj3 + mj4) {continue;}
		  // New test line
		//  if (mt3 == mt2 && i_orb4 == i_orb2 && mj3 == mj2) {continue;}
                //  if (mt4 == mt2 && i_orb3 == i_orb2 && mj4 == mj2) {continue;}
                  if (mt1 + mt2 - mt3 - mt4 != mt_op) {continue;}
                  for (int i = 0; i < sp->n_trans; i++) {density[i] = 0.0;}
                  if ((mt3 == 0.5) && (mt4 == 0.5)) { // 
                    if ((mt1 == 0.5) && (mt2 == 0.5)) { // 2 proton creation operators + 2 proton annihilation operators
                      trace_a4_nodes(a, b, c, d, num_mj_i, n_sds_p_int2, p2_array_f, p2_list_i, n0_list_i, wd, 0, sp->transition_list, density);
                    } else if ((mt1 == -0.5) && (mt2 == -0.5)) { // 2 neutron creation operators and two proton ann. operators
                      if ((fabs(mj1 + mj2) > (mj_max_n_i - mj_min_n_i)) || (fabs(mj3 + mj4) > (mj_max_p_i - mj_min_p_i))) {printf("Saved time\n"); continue;}
                      trace_a22_nodes(a, b, c, d, num_mj_i, p2_list_i, n2_list_f, wd, 0, sp->transition_list, density);
                    }  
                  } else if ((mt3 == -0.5) && (mt4 == -0.5)) {
                    if ((mt1 == -0.5) && (mt2 == -0.5)) { //2 n cr. and 2 n ann. operators
                      trace_a4_nodes(a, b, c, d, num_mj_i, n_sds_n_int2, n2_array_f, n2_list_i, p0_list_i, wd, 1, sp->transition_list, density);
                    } else if ((mt1 == 0.5) && (mt2 == 0.5)) {// 2 p cr. and 2 n ann. operators
                      if ((fabs(mj1 + mj2) > (mj_max_p_i - mj_min_p_i)) || (fabs(mj3 + mj4) > (mj_max_n_i - mj_min_n_i))) {printf("Saved time\n");continue;}
                      trace_a22_nodes(a, b, c, d, num_mj_i, n2_list_i, p2_list_f, wd, 1, sp->transition_list, density);
                    }
                  } else if ((mt1 == 0.5) && (mt2 == -0.5) && (mt3 == 0.5) && (mt4 == -0.5)) {
                      trace_a20_nodes(a, d, b, c, num_mj_i, n_sds_p_int1, n_sds_n_int1, p1_list_i, n1_list_i, p1_array_f, n1_array_f, wd, sp->transition_list, density);
                      for (int i = 0; i < sp->n_trans; i++) {density[i] *= -1.0;}
                  } else if ((mt1 == -0.5) && (mt2 == 0.5) && (mt3 == 0.5) && (mt4 == -0.5)) {
                      trace_a20_nodes(b, d, a, c, num_mj_i, n_sds_p_int1, n_sds_n_int1, p1_list_i, n1_list_i, p1_array_f, n1_array_f, wd, sp->transition_list, density);
                  } else if ((mt1 == 0.5) && (mt2 == -0.5) && (mt3 == -0.5) && (mt4 == 0.5)) {
                      trace_a20_nodes(a, c, b, d, num_mj_i, n_sds_p_int1, n_sds_n_int1,p1_list_i, n1_list_i, p1_array_f, n1_array_f, wd, sp->transition_list, density);
                  } else if ((mt1 == -0.5) && (mt2 == 0.5) && (mt3 == -0.5) && (mt4 == 0.5)) {
                      trace_a20_nodes(b, c, a, d, num_mj_i, n_sds_p_int1, n_sds_n_int1, p1_list_i, n1_list_i, p1_array_f, n1_array_f, wd, sp->transition_list, density);
                      for (int i = 0; i < sp->n_trans; i++) {density[i] *= -1.0;}
                  }
                  for (int j12 = j_min_12; j12 <= j_max_12; j12++) {
                    if ((mj1 + mj2 > j12) || (mj1 + mj2 < -j12)) {continue;}
                    float cg_j12 = clebsch_gordan(j1, j2, j12, mj1, mj2, mj1 + mj2);
                    if (cg_j12 == 0.0) {continue;}
                    for (int t12 = 0; t12 <= 1; t12++) {
                      if ((mt1 + mt2 > t12) || (mt1 + mt2 < -t12)) {continue;}
                      float cg_t12 = clebsch_gordan(0.5, 0.5, t12, mt1, mt2, mt1 + mt2);
                      for (int j34 = j_min_34; j34 <= j_max_34; j34++) {
                        if ((mj3 + mj4 > j34) || (mj3 + mj4 < -j34)){continue;}
                        float cg_j34 = clebsch_gordan(j4, j3, j34, mj4, mj3, mj3 + mj4);
                        if (cg_j34 == 0.0) {continue;}
                        float cg_jop = clebsch_gordan(j_op, j34, j12, 0, mj3 + mj4, mj1 + mj2);
                        if (cg_jop == 0.0) {continue;}
                        for (int t34 = 0; t34 <= 1; t34++) {
                          if ((mt3 + mt4 > t34) || (mt3 + mt4 < -t34)) {continue;}
                          float cg_t34 = clebsch_gordan(0.5, 0.5, t34, mt4, mt3, mt3 + mt4);
                          if (cg_t34 == 0.0) {continue;}
                          float cg_top = clebsch_gordan(t_op, t34, t12, mt_op, mt3 + mt4, mt1 + mt2);
                          if (cg_top == 0.0) {continue;}
                          double d2 = cg_j12*cg_j34*cg_jop;
                          d2 *= cg_t12*cg_t34*cg_top;
                          d2 *= pow(-1.0, -j34 + j12 - t34 + t12)/sqrt((2*j12 + 1)*(2*t12 + 1));
                          if (i_orb1 == i_orb2) {d2 *= 1.0/sqrt(2);}
                          if (i_orb3 == i_orb4) {d2 *= 1.0/sqrt(2);}
                          if (d2 == 0.0) {continue;}

                          for (int i = 0; i < sp->n_trans; i++) {
                            j_store[i + sp->n_trans*(t12 + 2*(t34 + 2*((j12 - j_min_12)*j_dim_34 + (j34 - j_min_34))))] += density[i]*d2/cg_fact[i];
               /*             if ((i_orb1 == i_orb2) && (mt1 == mt2)) {
                              j_store[i + sp->n_trans*(t12 + 2*(t34 + 2*((j12 - j_min_12)*j_dim_34 + (j34 - j_min_34))))] += pow(-1.0, j1 + j2 - j12 - t12)*density[i]*d2/cg_fact[i];
                            }
                            if ((i_orb3 == i_orb4) && (mt3 == mt4)) {
                              j_store[i + sp->n_trans*(t12 + 2*(t34 + 2*((j12 - j_min_12)*j_dim_34 + (j34 - j_min_34))))] += pow(-1.0, j3 + j4 - j34 - t34)*density[i]*d2/cg_fact[i];
                            }
                            if ((i_orb1 == i_orb2) && (mt1 == mt2) && (i_orb3 == i_orb4) && (mt3 == mt4)) {
                              j_store[i + sp->n_trans*(t12 + 2*(t34 + 2*((j12 - j_min_12)*j_dim_34 + (j34 - j_min_34))))] += pow(-1.0, j1 + j2 + j3 + j4 - j12 - j34 - t12 - t34)*density[i]*d2/cg_fact[i];
                            }*/
                          }
                        }
                      }
                    }
                  }
                }
              }
            }
          } 
          trans = sp->transition_list;
          i_trans = 0;
          while (trans != NULL) {
            int psi_i = trans->eig_i;
            int psi_f = trans->eig_f;
            strcpy(output_density_file, sp->out_file_base);
            sprintf(output_suffix, "_J%d_T%d_%d_%d.dens", j_op, t_op, psi_i, psi_f);
            strcat(output_density_file, output_suffix);
            out_file = fopen(output_density_file, "a"); 

            for (int t12 = 0; t12 <= 1; t12++) {
              for (int t34 = 0; t34 <= 1; t34++) {
                for (int ij12 = 0; ij12 < j_dim_12; ij12++) {
                  for (int ij34 = 0; ij34 < j_dim_34; ij34++) {
                    if (fabs(j_store[i_trans + sp->n_trans*(t12 + 2*(t34 + 2*(ij12*j_dim_34 + ij34)))]) < pow(10, -16)) {continue;}

                    fprintf(out_file, "%d,%g,%d,%g,%d,%d,%d,%g,%d,%g,%d,%d,%g\n", 2*wd->n_orb[i_orb1] + wd->l_orb[i_orb1], 2*wd->j_orb[i_orb1], 2*wd->n_orb[i_orb2] + wd->l_orb[i_orb2], 2*wd->j_orb[i_orb2], 2*(ij12 + j_min_12), 2*t12, 2*wd->n_orb[i_orb3] + wd->l_orb[i_orb3], 2*wd->j_orb[i_orb3], 2*wd->n_orb[i_orb4] + wd->l_orb[i_orb4], 2*wd->j_orb[i_orb4], 2*(ij34 + j_min_34), 2*t34, j_store[i_trans + sp->n_trans*(t12 + 2*(t34 + 2*(ij12*j_dim_34 + ij34)))]);

             /*       if (i_orb1 != i_orb2) { 
                      fprintf(out_file, "%d,%g,%d,%g,%d,%d,%d,%g,%d,%g,%d,%d,%g\n", 2*wd->n_orb[i_orb2] + wd->l_orb[i_orb2], 2*wd->j_orb[i_orb2], 2*wd->n_orb[i_orb1] + wd->l_orb[i_orb1], 2*wd->j_orb[i_orb1], 2*(ij12 + j_min_12), 2*t12, 2*wd->n_orb[i_orb3] + wd->l_orb[i_orb3], 2*wd->j_orb[i_orb3], 2*wd->n_orb[i_orb4] + wd->l_orb[i_orb4], 2*wd->j_orb[i_orb4], 2*(ij34 + j_min_34), 2*t34, pow(-1.0, j1 + j2 - ij12 - j_min_12 - t12)*j_store[i_trans + sp->n_trans*(t12 + 2*(t34 + 2*(ij12*j_dim_34 + ij34)))]);
                    }
                    if (i_orb3 != i_orb4) {
                        fprintf(out_file, "%d,%g,%d,%g,%d,%d,%d,%g,%d,%g,%d,%d,%g\n", 2*wd->n_orb[i_orb1] + wd->l_orb[i_orb1], 2*wd->j_orb[i_orb1], 2*wd->n_orb[i_orb2] + wd->l_orb[i_orb2], 2*wd->j_orb[i_orb2], 2*(ij12 + j_min_12), 2*t12, 2*wd->n_orb[i_orb4] + wd->l_orb[i_orb4], 2*wd->j_orb[i_orb4], 2*wd->n_orb[i_orb3] + wd->l_orb[i_orb3], 2*wd->j_orb[i_orb3], 2*(ij34 + j_min_34), 2*t34, pow(-1.0, j3 + j4 - ij34 - j_min_34 - t34)*j_store[i_trans + sp->n_trans*(t12 + 2*(t34 + 2*(ij12*j_dim_34 + ij34)))]);
                    }
                    if ((i_orb1 != i_orb2) && (i_orb3 != i_orb4)) {
                      fprintf(out_file, "%d,%g,%d,%g,%d,%d,%d,%g,%d,%g,%d,%d,%g\n", 2*wd->n_orb[i_orb2] + wd->l_orb[i_orb2], 2*wd->j_orb[i_orb2], 2*wd->n_orb[i_orb1] + wd->l_orb[i_orb1], 2*wd->j_orb[i_orb1], 2*(ij12 + j_min_12), 2*t12, 2*wd->n_orb[i_orb4] + wd->l_orb[i_orb4], 2*wd->j_orb[i_orb4], 2*wd->n_orb[i_orb3] + wd->l_orb[i_orb3], 2*wd->j_orb[i_orb3], 2*(ij34 + j_min_34), 2*t34, pow(-1.0, j1 + j2 + j3 + j4 - ij12 - j_min_12 - ij34 - j_min_34 - t12 - t34)*j_store[i_trans + sp->n_trans*(t12 + 2*(t34 + 2*(ij12*j_dim_34 + ij34)))]);

                    }*/
                  }
                }
              }
            }
            trans = trans->next;
            i_trans++;
            fclose(out_file);
          }
        }
      }
    }
  } 
  free(j_store); 

  return;
}
void one_body_density_spec(speedParams *sp) {
  // Reads in BIGSTICK basis/wavefunction (.trwfn) files along with
  // orbit definition (.sp) files and constructs the one-body density matrices
  // for each initial and final state eigenfunction
  // Initial and final wave functions must share the same orbit file
  // J_op and T_op are the total spin and isospin of the operator

  // Read in data 
  char wfn_file_initial[100];
  strcpy(wfn_file_initial, sp->initial_file_base);
  strcat(wfn_file_initial, ".wfn");
  char wfn_file_final[100];
  strcpy(wfn_file_final, sp->final_file_base);
  strcat(wfn_file_final, ".wfn");
  char basis_file_initial[100];
  strcpy(basis_file_initial, sp->initial_file_base);
  strcat(basis_file_initial, ".bas");
  char basis_file_final[100];
  strcpy(basis_file_final, sp->final_file_base);
  strcat(basis_file_final, ".bas");
  wfnData *wd = read_binary_wfn_data(wfn_file_initial, wfn_file_final, basis_file_initial, basis_file_final);
  
  int j_op = sp->j_op;
  int t_op = sp->t_op;
  int ns = wd->n_shells;

  // Determine the number of intermediate proton SDs
  int n_sds_p_int = get_num_sds(wd->n_shells, wd->n_proton_f - 1);
  printf("Num intermediate proton sds: %d\n", n_sds_p_int);
  int n_sds_n_int = get_num_sds(wd->n_shells, wd->n_neutron_f - 1);
  printf("Num intermediate neutron sds: %d\n", n_sds_n_int);
  
  // Determine the min and max mj values of proton/neutron SDs
  float mj_min_p_i = min_mj(ns, wd->n_proton_i, wd->jz_shell);
  float mj_max_p_i = max_mj(ns, wd->n_proton_i, wd->jz_shell);
  float mj_min_n_i = min_mj(ns, wd->n_neutron_i, wd->jz_shell);
  float mj_max_n_i = max_mj(ns, wd->n_neutron_i, wd->jz_shell);

  // Account for the factor that m_p + m_n = 0 in determining min/max mj
  mj_min_p_i = MAX(mj_min_p_i, -mj_max_n_i);
  mj_max_p_i = MIN(mj_max_p_i, -mj_min_n_i);
  mj_min_n_i = MAX(mj_min_n_i, -mj_max_p_i);
  mj_max_n_i = MIN(mj_max_n_i, -mj_min_p_i);

  // Determine number of sectors (mp, mn)
  int num_mj_i = mj_max_p_i - mj_min_p_i + 1;
  printf("Number of m_j sectors:%d\n", num_mj_i);
  int max_n_spec_q = get_max_n_spec_q(ns, wd->n_proton_i, wd->n_shell, wd->l_shell) + get_max_n_spec_q(ns, wd->n_neutron_i, wd->n_shell, wd->l_shell);
  int min_n_spec_q = get_min_n_spec_q(ns, wd->n_proton_i, wd->n_shell, wd->l_shell) + get_min_n_spec_q(ns, wd->n_neutron_i, wd->n_shell, wd->l_shell);

  int n_spec_bins = max_n_spec_q - min_n_spec_q + 1;
  printf("Num spec bins: %d\n", n_spec_bins);
  double* density = calloc(n_spec_bins*sp->n_trans, sizeof(double));
  double* total = calloc(n_spec_bins*sp->n_trans*pow(wd->n_orbits, 2), sizeof(double));
  printf("Min spec excitations: %d Max spec excitations: %d\n", min_n_spec_q, max_n_spec_q);

  // Allocate space for jump lists
  wfe_list **p0_list_i = (wfe_list**) calloc(2*num_mj_i, sizeof(wfe_list*));
  wfe_list **n0_list_i = (wfe_list**) calloc(2*num_mj_i, sizeof(wfe_list*));
  sde_list **p1_list_i = (sde_list**) calloc(2*ns*num_mj_i, sizeof(sde_list*));
  sde_list **n1_list_i = (sde_list**) calloc(2*ns*num_mj_i, sizeof(sde_list*));
  sde_list **p1_list_f = (sde_list**) calloc(2*ns*num_mj_i, sizeof(sde_list*));
  sde_list **n1_list_f = (sde_list**) calloc(2*ns*num_mj_i, sizeof(sde_list*));
  int* p1_array_f = (int*) calloc(ns*n_sds_p_int, sizeof(int));
  int* n1_array_f = (int*) calloc(ns*n_sds_n_int, sizeof(int));

  if (wd->same_basis) {
    printf("Building initial and final state proton jumps...\n");
    build_one_body_jumps_i_and_f_spec(wd->n_shells, wd->n_proton_i, mj_min_p_i, mj_max_p_i, num_mj_i, wd->n_sds_p_i, n_sds_p_int, p1_array_f, p0_list_i, p1_list_i, wd->jz_shell, wd->l_shell, wd->n_shell); 
    printf("Done.\n");

    printf("Building initial and final state neutron jumps...\n");
    build_one_body_jumps_i_and_f_spec(wd->n_shells, wd->n_neutron_i, mj_min_n_i, mj_max_n_i, num_mj_i, wd->n_sds_n_i, n_sds_n_int, n1_array_f, n0_list_i, n1_list_i, wd->jz_shell, wd->l_shell, wd->n_shell); 
    printf("Done.\n");
  } else {
    printf("Building initial state proton jumps...\n");
    build_one_body_jumps_i_spec(wd->n_shells, wd->n_proton_i, mj_min_p_i, mj_max_p_i, num_mj_i, wd->n_sds_p_i, p0_list_i, p1_list_i, wd->jz_shell, wd->l_shell, wd->n_shell);
    printf("Done.\n");
    printf("Building final state proton jumps...\n");
    build_one_body_jumps_f_spec(wd->n_shells, wd->n_proton_f, mj_min_p_i, mj_max_p_i, num_mj_i, wd->n_sds_p_f, n_sds_p_int, p1_array_f, p1_list_f, wd->jz_shell, wd->l_shell, wd->n_shell);
    printf("Done.\n");
    printf("Building initial state neutron jumps...\n");
    build_one_body_jumps_i_spec(wd->n_shells, wd->n_proton_i, mj_min_n_i, mj_max_n_i, num_mj_i, wd->n_sds_p_i, p0_list_i, p1_list_i, wd->jz_shell, wd->l_shell, wd->n_shell);
    printf("Done.\n"); 
    printf("Building final state neutron jumps...\n");
    build_one_body_jumps_f_spec(wd->n_shells, wd->n_neutron_f, mj_min_n_i, mj_max_n_i, num_mj_i, wd->n_sds_n_f, n_sds_n_int, n1_array_f, n1_list_f, wd->jz_shell, wd->l_shell, wd->n_shell);
    printf("Done.\n");
  } 
  double* cg_fact = (double*) calloc(sp->n_trans, sizeof(double));
  float mti = 0.5*(wd->n_proton_i - wd->n_neutron_i);
  float mtf = 0.5*(wd->n_proton_f - wd->n_neutron_f);
  float mt_op = mtf - mti;
  if (fabs(mt_op) > t_op) {printf("Error: operator iso-spin is insufficient to mediate a transition between the given nuclides.\n"); exit(0);}
  // Loop over transitions
  eigen_list* trans = sp->transition_list;
  int i_trans = 0;
  while (trans != NULL) {
    int psi_i = trans->eig_i;
    int psi_f = trans->eig_f;
    float ji = wd->j_nuc_i[psi_i];
    float ti = wd->t_nuc_i[psi_i];
    double cg_j = 0.0;
    double cg_t = 0.0;
    float jf = wd->j_nuc_f[psi_f];
    float tf = wd->t_nuc_f[psi_f];
    int ijf = 2*jf;
    float mj = 0.0;
    if (ijf % 2) {mj = 0.5;}
    cg_j = clebsch_gordan(j_op, ji, jf, 0, mj, mj);
    if (cg_j == 0.0) {continue;}
    cg_t = clebsch_gordan(t_op, ti, tf, mt_op, mti, mtf);
    if (cg_t == 0.0) {continue;}
    cg_j *= pow(-1.0, j_op + ji + jf)*sqrt(2*j_op + 1)/sqrt(2*jf + 1);
    cg_t *= pow(-1.0, t_op + ti + tf)*sqrt(2*t_op + 1)/sqrt(2*tf + 1);
    printf("Initial state: # %d J: %g T: %g Final state: # %d J: %g T: %g\n", psi_i + 1, ji, ti, psi_f + 1, jf, tf);
    cg_fact[i_trans] = cg_j*cg_t;
    i_trans++;
    trans = trans->next;
  } 
  // Loop over final state orbits
  for (int i_orb1 = 0; i_orb1 < wd->n_orbits; i_orb1++) {
    float j1 = wd->j_orb[i_orb1];
    // Loop over initial state orbits
    for (int i_orb2 = 0; i_orb2 < wd->n_orbits; i_orb2++) {
      float j2 = wd->j_orb[i_orb2];
      if ((j_op > j1 + j2) || (j_op < abs(j1 - j2))) {continue;}
      // Loop over initial state SDs
      for (int ia = 0; ia < 2*wd->n_shells; ia++) {
        float mt1 = 0.5;
        int a = ia;
        if (a >= ns) {a -= ns; mt1 -= 1;}
        if (wd->l_shell[a] != wd->l_orb[i_orb1]) {continue;}
        if (wd->n_shell[a] != wd->n_orb[i_orb1]) {continue;}
        if (wd->j_shell[a]/2.0 != j1) {continue;}
        float mj1 = wd->jz_shell[a]/2.0;
        // Loop over initial state shells
        for (int ib = 0; ib < 2*wd->n_shells; ib++) {
          float mt2 = 0.5;
          int b = ib;
          if (b >= ns) {b -= ns; mt2 -= 1;}
          if (wd->l_shell[b] != wd->l_orb[i_orb2]) {continue;}
          if (wd->n_shell[b] != wd->n_orb[i_orb2]) {continue;}
          if (wd->j_shell[b]/2.0 != j2) {continue;}
          float mj2 = wd->jz_shell[b]/2.0;
          float d2 = clebsch_gordan(j1, j2, j_op, mj1, -mj2, 0);
          d2 *= clebsch_gordan(0.5, 0.5, t_op, mt1, -mt2, mt_op);
          d2 *= pow(-1.0, j2 - mj2 + 0.5 - mt2);
          if (d2 == 0.0) {continue;}
          for (int i = 0; i < n_spec_bins*sp->n_trans; i++) {
            density[i] = 0;
          }

          if ((mt1 == 0.5) && (mt2 == 0.5)) {
            trace_1body_t0_nodes_spec(a, b, num_mj_i, n_sds_p_int, p1_array_f, p1_list_i, n0_list_i, wd, 0, sp->transition_list, density, min_n_spec_q, n_spec_bins);
          } else if ((mt1 == -0.5) && (mt2 == -0.5)) {
            trace_1body_t0_nodes_spec(a, b, num_mj_i, n_sds_n_int, n1_array_f, n1_list_i, p0_list_i, wd, 1, sp->transition_list, density, min_n_spec_q, n_spec_bins);
          } else if ((mt1 == 0.5) && (mt2 == -0.5)) {
            trace_1body_t2_nodes_spec(a, b, num_mj_i, n1_list_i, p1_list_f, wd, 0, sp->transition_list, density, min_n_spec_q, n_spec_bins);
          } else {
            trace_1body_t2_nodes_spec(a, b, num_mj_i, p1_list_i, n1_list_f, wd, 1, sp->transition_list, density, min_n_spec_q, n_spec_bins);
          }
          for (int i = 0; i < sp->n_trans; i++) {
            for (int j = 0; j < n_spec_bins; j++) {
              total[j + n_spec_bins*(i + sp->n_trans*(i_orb1 + i_orb2*wd->n_orbits))] += density[j + i*n_spec_bins]*d2/cg_fact[i];
            }
          }
        }
      }
    }
  }
  FILE *out_file;
  char output_density_file[100];
  char output_log_file[100];
  char output_suffix[100];
  strcpy(output_log_file, sp->out_file_base);
  strcat(output_log_file, ".log");
  trans = sp->transition_list;
  i_trans = 0;
  while (trans != NULL) {
    int psi_i = trans->eig_i;
    int psi_f = trans->eig_f;
    strcpy(output_density_file, sp->out_file_base);
    sprintf(output_suffix, "_J%d_T%d_%d_%d.dens", j_op, t_op, psi_i, psi_f);
    strcat(output_density_file, output_suffix);
    out_file = fopen(output_density_file, "w"); 
    for (int i_orb1 = 0; i_orb1 < wd->n_orbits; i_orb1++) {
      for (int i_orb2 = 0; i_orb2 < wd->n_orbits; i_orb2++) {
        for (int j = 0; j < n_spec_bins; j++) {
          int n_spec = min_n_spec_q + j;

          if (fabs(total[j + n_spec_bins*(i_trans + sp->n_trans*(i_orb1 + i_orb2*wd->n_orbits))]) > pow(10, -12)) {
          fprintf(out_file, "%d %d %g %d %d %g %d %g\n", wd->n_orb[i_orb1], wd->l_orb[i_orb1], wd->j_orb[i_orb1], wd->n_orb[i_orb2], wd->l_orb[i_orb2], wd->j_orb[i_orb2], n_spec, total[j + n_spec_bins*(i_trans + sp->n_trans*(i_orb1 + i_orb2*wd->n_orbits))]);
          }
        }
      }
    }
    fclose(out_file);
    i_trans++;
    trans = trans->next;
  }   
  return;
}

void one_body_density_trunc(speedParams* sp) {
  // Reads in BIGSTICK basis/wavefunction (.trwfn) files along with
  // orbit definition (.sp) files and constructs the one-body density matrices
  // for each initial and final state eigenfunction
  // Initial and final wave functions must share the same orbit file
  // J_op and T_op are the total spin and isospin of the operator

  // Read in data 
  char wfn_file_initial[100];
  strcpy(wfn_file_initial, sp->initial_file_base);
  strcat(wfn_file_initial, ".wfn");
  char wfn_file_final[100];
  strcpy(wfn_file_final, sp->final_file_base);
  strcat(wfn_file_final, ".wfn");
  char basis_file_initial[100];
  strcpy(basis_file_initial, sp->initial_file_base);
  strcat(basis_file_initial, ".bas");
  char basis_file_final[100];
  strcpy(basis_file_final, sp->final_file_base);
  strcat(basis_file_final, ".bas");
  wfnData *wd = read_binary_wfn_data(wfn_file_initial, wfn_file_final, basis_file_initial, basis_file_final);

  int j_op = sp->j_op;
  int t_op = sp->t_op;
  int ns = wd->n_shells;

  printf("j_op: %d t_op: %d\n", j_op, t_op);
  // Determine the number of intermediate proton SDs
  int n_sds_p_int = get_num_sds(wd->n_shells, wd->n_proton_f - 1);
  printf("Num intermediate proton sds: %d\n", n_sds_p_int);
  int n_sds_n_int = get_num_sds(wd->n_shells, wd->n_neutron_f - 1);
  printf("Num intermediate neutron sds: %d\n", n_sds_n_int);
  
  // Determine the min and max mj values of proton/neutron SDs
  float mj_min_p_i = min_mj(ns, wd->n_proton_i, wd->jz_shell);
  float mj_max_p_i = max_mj(ns, wd->n_proton_i, wd->jz_shell);
  float mj_min_n_i = min_mj(ns, wd->n_neutron_i, wd->jz_shell);
  float mj_max_n_i = max_mj(ns, wd->n_neutron_i, wd->jz_shell);
  printf("%g, %g, %g, %g\n", mj_min_p_i, mj_max_p_i, mj_min_n_i, mj_max_n_i);

  // Account for the factor that m_p + m_n = 0 in determining min/max mj
  if (fabs(((int) (2*wd->j_nuc_i[0])) % 2) > pow(10, -3)) {
    // half-integer mj case
    mj_min_p_i = MAX(mj_min_p_i, -mj_max_n_i + 0.5);
    mj_max_p_i = MIN(mj_max_p_i, -mj_min_n_i + 0.5);
    mj_min_n_i = MAX(mj_min_n_i, -mj_max_p_i + 0.5);
    mj_max_n_i = MIN(mj_max_n_i, -mj_min_p_i + 0.5);
  } else {
  mj_min_p_i = MAX(mj_min_p_i, -mj_max_n_i);
  mj_max_p_i = MIN(mj_max_p_i, -mj_min_n_i);
  mj_min_n_i = MAX(mj_min_n_i, -mj_max_p_i);
  mj_max_n_i = MIN(mj_max_n_i, -mj_min_p_i);
 }


  // Determine number of sectors (mp, mn)
  int num_mj_p_i = mj_max_p_i - mj_min_p_i + 1;
  int num_mj_n_i = mj_max_n_i - mj_min_n_i + 1;
  printf("Number of m_j sectors:%d %d\n", num_mj_p_i, num_mj_n_i);
  printf("%g, %g, %g, %g\n", mj_min_p_i, mj_max_p_i, mj_min_n_i, mj_max_n_i);

  float mj_min_p_f = min_mj(ns, wd->n_proton_f, wd->jz_shell);
  float mj_max_p_f = max_mj(ns, wd->n_proton_f, wd->jz_shell);
  float mj_min_n_f = min_mj(ns, wd->n_neutron_f, wd->jz_shell);
  float mj_max_n_f = max_mj(ns, wd->n_neutron_f, wd->jz_shell);
  printf("%g, %g, %g, %g\n", mj_min_p_f, mj_max_p_f, mj_min_n_f, mj_max_n_f);

  // Account for the factor that m_p + m_n = 0 in determining min/max mj
  if (fabs(((int) (2*wd->j_nuc_i[0])) % 2) > pow(10, -3)) {
    // half-integer mj case
    mj_min_p_f = MAX(mj_min_p_f, -mj_max_n_f + 0.5);
    mj_max_p_f = MIN(mj_max_p_f, -mj_min_n_f + 0.5);
    mj_min_n_f = MAX(mj_min_n_f, -mj_max_p_f + 0.5);
    mj_max_n_f = MIN(mj_max_n_f, -mj_min_p_f + 0.5);
  } else { 
  mj_min_p_f = MAX(mj_min_p_f, -mj_max_n_f);
  mj_max_p_f = MIN(mj_max_p_f, -mj_min_n_f);
  mj_min_n_f = MAX(mj_min_n_f, -mj_max_p_f);
  mj_max_n_f = MIN(mj_max_n_f, -mj_min_p_f);
  }



  // Determine number of sectors (mp, mn)
  int num_mj_p_f = mj_max_p_f - mj_min_p_f + 1;
  int num_mj_n_f = mj_max_n_f - mj_min_n_f + 1;
  printf("Number of final m_j sectors: %d %d\n", num_mj_p_f, num_mj_n_f);
  printf("%g, %g, %g, %g\n", mj_min_p_f, mj_max_p_f, mj_min_n_f, mj_max_n_f);

  // Allocate space for jump lists
  wf_list **p0_list_i = (wf_list**) calloc(2*num_mj_p_i, sizeof(wf_list*));
  wf_list **n0_list_i = (wf_list**) calloc(2*num_mj_n_i, sizeof(wf_list*));
  sd_list **p1_list_i = (sd_list**) calloc(2*ns*num_mj_p_i, sizeof(sd_list*));
  sd_list **n1_list_i = (sd_list**) calloc(2*ns*num_mj_n_i, sizeof(sd_list*));
  sd_list **p1_list_f = (sd_list**) calloc(2*ns*num_mj_p_i, sizeof(sd_list*));
  sd_list **n1_list_f = (sd_list**) calloc(2*ns*num_mj_n_i, sizeof(sd_list*));
  int* p1_array_f = (int*) calloc(ns*n_sds_p_int, sizeof(int));
  int* n1_array_f = (int*) calloc(ns*n_sds_n_int, sizeof(int));
  int w_cut = 51;
  if (wd->same_basis) {
    printf("Building initial and final state proton jumps...\n");
    build_one_body_jumps_i_and_f_trunc(wd->n_shells, wd->n_proton_i, mj_min_p_i, mj_max_p_i, num_mj_p_i, wd->n_sds_p_i, n_sds_p_int, p1_array_f, p0_list_i, p1_list_i, wd->jz_shell, wd->l_shell, wd->w_shell, w_cut); 
    printf("Done.\n");

    printf("Building initial and final state neutron jumps...\n");
    build_one_body_jumps_i_and_f_trunc(wd->n_shells, wd->n_neutron_i, mj_min_n_i, mj_max_n_i, num_mj_n_i, wd->n_sds_n_i, n_sds_n_int, n1_array_f, n0_list_i, n1_list_i, wd->jz_shell, wd->l_shell, wd->w_shell, w_cut); 
    printf("Done.\n");
  } else {
    printf("Building initial state proton jumps...\n");
    build_one_body_jumps_i_trunc(wd->n_shells, wd->n_proton_i, mj_min_p_i, mj_max_p_i, num_mj_p_i, wd->n_sds_p_i, p0_list_i, p1_list_i, wd->jz_shell, wd->l_shell, wd->w_shell, 15);
    printf("Done.\n");
    printf("Building final state proton jumps...\n");
    build_one_body_jumps_f_trunc(wd->n_shells, wd->n_proton_f, mj_min_p_i, mj_max_p_i, num_mj_p_i, wd->n_sds_p_f, n_sds_p_int, p1_array_f, p1_list_f, wd->jz_shell, wd->l_shell, wd->w_shell, 18);
    printf("Done.\n");
    printf("Building initial state neutron jumps...\n");
    build_one_body_jumps_i_trunc(wd->n_shells, wd->n_neutron_i, mj_min_n_i, mj_max_n_i, num_mj_n_i, wd->n_sds_n_i, n0_list_i, n1_list_i, wd->jz_shell, wd->l_shell, wd->w_shell, 42);
    printf("Done.\n"); 
    printf("Building final state neutron jumps...\n");
    build_one_body_jumps_f_trunc(wd->n_shells, wd->n_neutron_f, mj_min_n_i, mj_max_n_i, num_mj_n_i, wd->n_sds_n_f, n_sds_n_int, n1_array_f, n1_list_f, wd->jz_shell, wd->l_shell, wd->w_shell, 39);
    printf("Done.\n");
  } 
  // Loop over initial eigenstates
  
 
  double* cg_fact = (double*) calloc(sp->n_trans, sizeof(double));
  float mti = 0.5*(wd->n_proton_i - wd->n_neutron_i);
  float mtf = 0.5*(wd->n_proton_f - wd->n_neutron_f);
  float mt_op = mtf - mti;
  if (fabs(mt_op) > t_op) {printf("Error: operator iso-spin is insufficient to mediate a transition between the given nuclides.\n"); exit(0);}
  // Loop over transitions
  eigen_list* trans = sp->transition_list;
  int i_trans = 0;
  while (trans != NULL) {
    int psi_i = trans->eig_i;
    int psi_f = trans->eig_f;
    float ji = wd->j_nuc_i[psi_i];
    float ti = wd->t_nuc_i[psi_i];
    double cg_j = 0.0;
    double cg_t = 0.0;
    float jf = wd->j_nuc_f[psi_f];
    float tf = wd->t_nuc_f[psi_f];
    double mji = 0;
    double mjf = 0;
    int ijf = (int) 2*jf;
    if (ijf % 2) {mjf = 0.5; mji = 0.5;}
    if (fabs(ji - jf) > j_op) {printf("Error: The requested transition Ji = %g Ti = %g -> Jf = %g Tf = %g through the operator Jop = %d Top = %d is impossible. \nPlease remove this transition from the parameter file and try again.\n", ji, ti, jf, tf, j_op, t_op); exit(0);}
    cg_j = clebsch_gordan(j_op, ji, jf, 0, mji,mjf);
    if (cg_j == 0.0) {printf("Error: CG coefficient is zero for allowed transition. \nComputing the density matrix for this operator requires the raising/lowering operators (not currently supported."); exit(0);}
    cg_t = clebsch_gordan(t_op, ti, tf, mt_op, mti, mtf);
    if (cg_t == 0.0) {printf("Error: CG coefficient is zero for allowed transition. \nComputing the density matrix for this operator requires the raising/lowering operators (not currently supported."); exit(0);}
    cg_j *= pow(-1.0, j_op - ji + jf)*sqrt(2*j_op + 1)/sqrt(2*jf + 1);
    cg_t *= pow(-1.0, t_op - ti + tf)*sqrt(2*t_op + 1)/sqrt(2*tf + 1);
    printf("Initial state: # %d J: %g T: %g Final state: # %d J: %g T: %g\n", psi_i + 1, ji, ti, psi_f + 1, jf, tf);
    cg_fact[i_trans] = cg_j*cg_t;
    i_trans++;
    trans = trans->next;
  } 
  // Loop over final state orbits
  double *total = (double*) calloc(sp->n_trans*pow(wd->n_orbits, 2), sizeof(double));
  double *density = (double*) calloc(sp->n_trans, sizeof(double));
  for (int i_orb1 = 0; i_orb1 < wd->n_orbits; i_orb1++) {
    float j1 = wd->j_orb[i_orb1];
    // Loop over initial state orbits
    for (int i_orb2 = 0; i_orb2 < wd->n_orbits; i_orb2++) {
      float j2 = wd->j_orb[i_orb2];
      if ((j_op > j1 + j2) || (j_op < abs(j1 - j2))) {continue;}
      // Loop over initial state SDs
      for (int ia = 0; ia < 2*wd->n_shells; ia++) {
        float mt1 = 0.5;
        int a = ia;
        if (a >= ns) {a -= ns; mt1 -= 1;}
        if (wd->l_shell[a] != wd->l_orb[i_orb1]) {continue;}
        if (wd->n_shell[a] != wd->n_orb[i_orb1]) {continue;}
        if (wd->j_shell[a]/2.0 != j1) {continue;}
        float mj1 = wd->jz_shell[a]/2.0;
        // Loop over initial state shells
        for (int ib = 0; ib < 2*wd->n_shells; ib++) {
          float mt2 = 0.5;
          int b = ib;
          if (b >= ns) {b -= ns; mt2 -= 1;}
          if (wd->l_shell[b] != wd->l_orb[i_orb2]) {continue;}
          if (wd->n_shell[b] != wd->n_orb[i_orb2]) {continue;}
          if (wd->j_shell[b]/2.0 != j2) {continue;}
          float mj2 = wd->jz_shell[b]/2.0;
          float d2 = clebsch_gordan(j1, j2, j_op, mj1, -mj2, 0);
          d2 *= clebsch_gordan(0.5, 0.5, t_op, mt1, -mt2, mt_op);
          d2 *= pow(-1.0, j2 - mj2 + 0.5 - mt2);
          if (d2 == 0.0) {continue;}
          for (int i = 0; i < sp->n_trans; i++) {
            density[i] = 0.0;
          }
          if ((mt1 == 0.5) && (mt2 == 0.5)) {
            trace_1body_t0_nodes(a, b, num_mj_p_i, n_sds_p_int, p1_array_f, p1_list_i, n0_list_i, wd, 0, sp->transition_list, density);
          } else if ((mt1 == -0.5) && (mt2 == -0.5)) {
            trace_1body_t0_nodes(a, b, num_mj_n_i, n_sds_n_int, n1_array_f, n1_list_i, p0_list_i, wd, 1, sp->transition_list, density);
          } else if ((mt1 == 0.5) && (mt2 == -0.5)) {
            trace_1body_t2_nodes(a, b, num_mj_n_i, mj_min_n_i, num_mj_p_i, mj_min_p_i, n1_list_i, p1_list_f, wd, 1, sp->transition_list, density);
          } else {
            trace_1body_t2_nodes(a, b, num_mj_p_i, mj_min_p_i, num_mj_n_i, mj_min_n_i, p1_list_i, n1_list_f, wd, 0, sp->transition_list, density);
          }
          for (int i = 0; i < sp->n_trans; i++) {
            total[i + sp->n_trans*(i_orb1 + i_orb2*wd->n_orbits)] += density[i]*d2/cg_fact[i];
            printf("%g\n", total[i + sp->n_trans*(i_orb1 + i_orb2*wd->n_orbits)]);

	  }

        }
      }
    }
  } 
  FILE *out_file;
  char output_density_file[100];
  char output_log_file[100];
  char output_suffix[100];
  strcpy(output_log_file, sp->out_file_base);
  strcat(output_log_file, ".log");
  trans = sp->transition_list;
  i_trans = 0;
  while (trans != NULL) {
    int psi_i = trans->eig_i;
    int psi_f = trans->eig_f;
    strcpy(output_density_file, sp->out_file_base);
    sprintf(output_suffix, "_J%d_T%d_%d_%d.dens", j_op, t_op, psi_i, psi_f);
    strcat(output_density_file, output_suffix);
    out_file = fopen(output_density_file, "w"); 
    for (int i_orb1 = 0; i_orb1 < wd->n_orbits; i_orb1++) {
      for (int i_orb2 = 0; i_orb2 < wd->n_orbits; i_orb2++) {
        if (fabs(total[i_trans + sp->n_trans*(i_orb1 + i_orb2*wd->n_orbits)]) > pow(10, -12)) {
          fprintf(out_file, "%d, %g, %d, %g, %g\n", 2*wd->n_orb[i_orb1] + wd->l_orb[i_orb1], 2*wd->j_orb[i_orb1], 2*wd->n_orb[i_orb2] + wd->l_orb[i_orb2], 2*wd->j_orb[i_orb2], total[i_trans + sp->n_trans*(i_orb1 + i_orb2*wd->n_orbits)]);
        }
      }
    }
    fclose(out_file);
    i_trans++;
    trans = trans->next;
  }    
  
  return;
}


void one_body_density(speedParams* sp) {
  // Reads in BIGSTICK basis/wavefunction (.trwfn) files along with
  // orbit definition (.sp) files and constructs the one-body density matrices
  // for each initial and final state eigenfunction
  // Initial and final wave functions must share the same orbit file
  // J_op and T_op are the total spin and isospin of the operator

  // Read in data 
  char wfn_file_initial[100];
  strcpy(wfn_file_initial, sp->initial_file_base);
  strcat(wfn_file_initial, ".wfn");
  char wfn_file_final[100];
  strcpy(wfn_file_final, sp->final_file_base);
  strcat(wfn_file_final, ".wfn");
  char basis_file_initial[100];
  strcpy(basis_file_initial, sp->initial_file_base);
  strcat(basis_file_initial, ".bas");
  char basis_file_final[100];
  strcpy(basis_file_final, sp->final_file_base);
  strcat(basis_file_final, ".bas");
  wfnData *wd = read_binary_wfn_data(wfn_file_initial, wfn_file_final, basis_file_initial, basis_file_final);

  int j_op = sp->j_op;
  int t_op = sp->t_op;
  int ns = wd->n_shells;

  printf("j_op: %d t_op: %d\n", j_op, t_op);
  // Determine the number of intermediate proton SDs
  int n_sds_p_int = get_num_sds(wd->n_shells, wd->n_proton_f - 1);
  printf("Num intermediate proton sds: %d\n", n_sds_p_int);
  int n_sds_n_int = get_num_sds(wd->n_shells, wd->n_neutron_f - 1);
  printf("Num intermediate neutron sds: %d\n", n_sds_n_int);
  
  // Determine the min and max mj values of proton/neutron SDs
  float mj_min_p_i = min_mj(ns, wd->n_proton_i, wd->jz_shell);
  float mj_max_p_i = max_mj(ns, wd->n_proton_i, wd->jz_shell);
  float mj_min_n_i = min_mj(ns, wd->n_neutron_i, wd->jz_shell);
  float mj_max_n_i = max_mj(ns, wd->n_neutron_i, wd->jz_shell);

  // Account for the factor that m_p + m_n = 0 in determining min/max mj
  mj_min_p_i = MAX(mj_min_p_i, -mj_max_n_i);
  mj_max_p_i = MIN(mj_max_p_i, -mj_min_n_i);
  mj_min_n_i = MAX(mj_min_n_i, -mj_max_p_i);
  mj_max_n_i = MIN(mj_max_n_i, -mj_min_p_i);

  // Determine number of sectors (mp, mn)
  int num_mj_p_i = mj_max_p_i - mj_min_p_i + 1;
  int num_mj_n_i = mj_max_n_i - mj_min_n_i + 1;
  printf("Number of initial p m_j sectors:%d\n", num_mj_p_i);
  printf("Number of initial n m_j sectors:%d\n", num_mj_n_i);

  float mj_min_p_f = min_mj(ns, wd->n_proton_f, wd->jz_shell);
  float mj_max_p_f = max_mj(ns, wd->n_proton_f, wd->jz_shell);
  float mj_min_n_f = min_mj(ns, wd->n_neutron_f, wd->jz_shell);
  float mj_max_n_f = max_mj(ns, wd->n_neutron_f, wd->jz_shell);

  // Account for the factor that m_p + m_n = 0 in determining min/max mj
  mj_min_p_f = MAX(mj_min_p_f, -mj_max_n_f);
  mj_max_p_f = MIN(mj_max_p_f, -mj_min_n_f);
  mj_min_n_f = MAX(mj_min_n_f, -mj_max_p_f);
  mj_max_n_f = MIN(mj_max_n_f, -mj_min_p_f);

  // Determine number of sectors (mp, mn)
  int num_mj_f = mj_max_p_f - mj_min_p_f + 1;
  int num_mj_n_f = mj_max_p_f - mj_min_p_f + 1;
;
  printf("Number of final p m_j sectors:%d\n", num_mj_f);
  printf("Number of final n m_j sectors:%d\n", num_mj_n_f);


  // Allocate space for jump lists
  wf_list **p0_list_i = (wf_list**) calloc(2*num_mj_p_i, sizeof(wf_list*));
  wf_list **n0_list_i = (wf_list**) calloc(2*num_mj_n_i, sizeof(wf_list*));
  sd_list **p1_list_i = (sd_list**) calloc(2*ns*num_mj_p_i, sizeof(sd_list*));
  sd_list **n1_list_i = (sd_list**) calloc(2*ns*num_mj_n_i, sizeof(sd_list*));
  sd_list **p1_list_f = (sd_list**) calloc(2*ns*num_mj_f, sizeof(sd_list*));
  sd_list **n1_list_f = (sd_list**) calloc(2*ns*num_mj_f, sizeof(sd_list*));
  int* p1_array_f = (int*) calloc(ns*n_sds_p_int, sizeof(int));
  int* n1_array_f = (int*) calloc(ns*n_sds_n_int, sizeof(int));

  if (wd->same_basis) {
    printf("Building initial and final state proton jumps...\n");
    build_one_body_jumps_i_and_f(wd->n_shells, wd->n_proton_i, mj_min_p_i, mj_max_p_i, num_mj_p_i, wd->n_sds_p_i, n_sds_p_int, p1_array_f, p0_list_i, p1_list_i, wd->jz_shell, wd->l_shell); 
    printf("Done.\n");

    printf("Building initial and final state neutron jumps...\n");
    build_one_body_jumps_i_and_f(wd->n_shells, wd->n_neutron_i, mj_min_n_i, mj_max_n_i, num_mj_n_i, wd->n_sds_n_i, n_sds_n_int, n1_array_f, n0_list_i, n1_list_i, wd->jz_shell, wd->l_shell); 
    printf("Done.\n");
  } else {
    printf("Building initial state proton jumps...\n");
    build_one_body_jumps_i(wd->n_shells, wd->n_proton_i, mj_min_p_i, mj_max_p_i, num_mj_p_i, wd->n_sds_p_i, p0_list_i, p1_list_i, wd->jz_shell, wd->l_shell);
    printf("Done.\n");
    printf("Building final state proton jumps...\n");
    build_one_body_jumps_f(wd->n_shells, wd->n_proton_f, mj_min_p_i, mj_max_p_i, num_mj_p_i, wd->n_sds_p_f, n_sds_p_int, p1_array_f, p1_list_f, wd->jz_shell, wd->l_shell);
    printf("Done.\n");
    printf("Building initial state neutron jumps...\n");
    build_one_body_jumps_i(wd->n_shells, wd->n_neutron_i, mj_min_n_i, mj_max_n_i, num_mj_n_i, wd->n_sds_n_i, n0_list_i, n1_list_i, wd->jz_shell, wd->l_shell);
    printf("Done.\n"); 
    printf("Building final state neutron jumps...\n");
    build_one_body_jumps_f(wd->n_shells, wd->n_neutron_f, mj_min_n_f, mj_max_n_i, num_mj_n_i, wd->n_sds_n_i, n_sds_n_int, n1_array_f, n1_list_f, wd->jz_shell, wd->l_shell);
    printf("Done.\n");
  } 
  // Loop over initial eigenstates
  
  
  double* cg_fact = (double*) calloc(sp->n_trans, sizeof(double));
  float mti = 0.5*(wd->n_proton_i - wd->n_neutron_i);
  float mtf = 0.5*(wd->n_proton_f - wd->n_neutron_f);
  float mt_op = mtf - mti;
  printf("mt_op: %g \n", mt_op);
  if (fabs(mt_op) > t_op) {printf("Error: operator iso-spin is insufficient to mediate a transition between the given nuclides.\n"); exit(0);}
  // Loop over transitions
  eigen_list* trans = sp->transition_list;
  int i_trans = 0;
  while (trans != NULL) {
    int psi_i = trans->eig_i;
    int psi_f = trans->eig_f;
    float ji = wd->j_nuc_i[psi_i];
    float ti = wd->t_nuc_i[psi_i];
    double cg_j = 0.0;
    double cg_t = 0.0;
    float jf = wd->j_nuc_f[psi_f];
    float tf = wd->t_nuc_f[psi_f];
    double mji = 0;
    double mjf = 0;
    int ijf = (int) 2*jf;
    if (ijf % 2) {mjf = 0.5; mji = 0.5;}
    if (fabs(ji - jf) > j_op) {printf("Error: The requested transition Ji = %g Ti = %g -> Jf = %g Tf = %g through the operator Jop = %d Top = %d is impossible. \nPlease remove this transition from the parameter file and try again.\n", ji, ti, jf, tf, j_op, t_op); exit(0);}
    cg_j = clebsch_gordan(j_op, ji, jf, 0, mji,mjf);
    if (cg_j == 0.0) {printf("Error: CG coefficient is zero for allowed transition. \nComputing the density matrix for this operator requires the raising/lowering operators (not currently supported."); exit(0);}
    cg_t = clebsch_gordan(t_op, ti, tf, mt_op, mti, mtf);
    if (cg_t == 0.0) {printf("Error: CG coefficient is zero for allowed transition. \nComputing the density matrix for this operator requires the raising/lowering operators (not currently supported."); exit(0);}
    cg_j *= pow(-1.0, j_op + ji + jf)*sqrt(2*j_op + 1)/sqrt(2*jf + 1);
    cg_t *= pow(-1.0, t_op + ti + tf)*sqrt(2*t_op + 1)/sqrt(2*tf + 1);
    printf("Initial state: # %d J: %g T: %g Final state: # %d J: %g T: %g\n", psi_i + 1, ji, ti, psi_f + 1, jf, tf);
    cg_fact[i_trans] = cg_j*cg_t;
    i_trans++;
    trans = trans->next;
  } 
  // Loop over final state orbits
  double *total = (double*) calloc(sp->n_trans*pow(wd->n_orbits, 2), sizeof(double));
  double *density = (double*) calloc(sp->n_trans, sizeof(double));
  for (int i_orb1 = 0; i_orb1 < wd->n_orbits; i_orb1++) {
    float j1 = wd->j_orb[i_orb1];
    // Loop over initial state orbits
    for (int i_orb2 = 0; i_orb2 < wd->n_orbits; i_orb2++) {
      float j2 = wd->j_orb[i_orb2];
      if ((j_op > j1 + j2) || (j_op < abs(j1 - j2))) {continue;}
      // Loop over initial state SDs
      for (int ia = 0; ia < 2*wd->n_shells; ia++) {
        float mt1 = 0.5;
        int a = ia;
        if (a >= ns) {a -= ns; mt1 -= 1;}
        if (wd->l_shell[a] != wd->l_orb[i_orb1]) {continue;}
        if (wd->n_shell[a] != wd->n_orb[i_orb1]) {continue;}
        if (wd->j_shell[a]/2.0 != j1) {continue;}
        float mj1 = wd->jz_shell[a]/2.0;
        // Loop over initial state shells
        for (int ib = 0; ib < 2*wd->n_shells; ib++) {
          float mt2 = 0.5;
          int b = ib;
          if (b >= ns) {b -= ns; mt2 -= 1;}
          if (wd->l_shell[b] != wd->l_orb[i_orb2]) {continue;}
          if (wd->n_shell[b] != wd->n_orb[i_orb2]) {continue;}
          if (wd->j_shell[b]/2.0 != j2) {continue;}
          float mj2 = wd->jz_shell[b]/2.0;
          float d2 = clebsch_gordan(j1, j2, j_op, mj1, -mj2, 0);
          d2 *= clebsch_gordan(0.5, 0.5, t_op, mt1, -mt2, mt_op);
          d2 *= pow(-1.0, j2 - mj2 + 0.5 - mt2);
          if (d2 == 0.0) {continue;}
          for (int i = 0; i < sp->n_trans; i++) {
            density[i] = 0.0;
          }
          if ((mt1 == 0.5) && (mt2 == 0.5)) {
            trace_1body_t0_nodes(a, b, num_mj_p_i, n_sds_p_int, p1_array_f, p1_list_i, n0_list_i, wd, 0, sp->transition_list, density);
          } else if ((mt1 == -0.5) && (mt2 == -0.5)) {
            trace_1body_t0_nodes(a, b, num_mj_n_i, n_sds_n_int, n1_array_f, n1_list_i, p0_list_i, wd, 1, sp->transition_list, density);
          } else if ((mt1 == 0.5) && (mt2 == -0.5)) {
            trace_1body_t2_nodes(a, b, num_mj_n_i, mj_min_n_i, num_mj_p_i, mj_min_p_i, n1_list_i, p1_list_f, wd, 1, sp->transition_list, density);
          } else {
            trace_1body_t2_nodes(a, b, num_mj_p_i, mj_min_p_i, num_mj_n_i, mj_min_n_i, p1_list_i, n1_list_f, wd, 0, sp->transition_list, density);
          }
          for (int i = 0; i < sp->n_trans; i++) {
            total[i + sp->n_trans*(i_orb1 + i_orb2*wd->n_orbits)] += density[i]*d2/cg_fact[i];
          }
        }
      }
    }
  } 
  FILE *out_file;
  char output_density_file[100];
  char output_log_file[100];
  char output_suffix[100];
  strcpy(output_log_file, sp->out_file_base);
  strcat(output_log_file, ".log");
  trans = sp->transition_list;
  i_trans = 0;
  while (trans != NULL) {
    int psi_i = trans->eig_i;
    int psi_f = trans->eig_f;
    strcpy(output_density_file, sp->out_file_base);
    sprintf(output_suffix, "_J%d_T%d_%d_%d.dens", j_op, t_op, psi_i, psi_f);
    strcat(output_density_file, output_suffix);
    out_file = fopen(output_density_file, "w"); 
    for (int i_orb1 = 0; i_orb1 < wd->n_orbits; i_orb1++) {
      for (int i_orb2 = 0; i_orb2 < wd->n_orbits; i_orb2++) {
        if (fabs(total[i_trans + sp->n_trans*(i_orb1 + i_orb2*wd->n_orbits)]) > pow(10, -12)) {
          fprintf(out_file, "%d, %g, %d, %g, %g\n", 2*wd->n_orb[i_orb1] + wd->l_orb[i_orb1], 2*wd->j_orb[i_orb1], 2*wd->n_orb[i_orb2] + wd->l_orb[i_orb2], 2*wd->j_orb[i_orb2], total[i_trans + sp->n_trans*(i_orb1 + i_orb2*wd->n_orbits)]);
        }
      }
    }
    fclose(out_file);
    i_trans++;
    trans = trans->next;
  }    
  
  return;
}

void trace_a4_nodes(int a, int b, int c, int d, int num_mj, int n_sds_int2, int* p2_array_f, sd_list** p2_list_i, wf_list** n0_list_i, wfnData* wd, int i_op, eigen_list* transition, double* density) {
  int ns = wd->n_shells;

  for (int ipar = 0; ipar <= 1; ipar++) {
    for (int imj = 0; imj < num_mj; imj++) {
      sd_list* node1 = p2_list_i[ipar + 2*(imj + num_mj*(c + d*ns))];
      while (node1 != NULL) {
        unsigned int ppn = node1->pn;
        unsigned int ppi = node1->pi;
        int phase1 = node1->phase;
        int ppf = p2_array_f[(ppn - 1) + n_sds_int2*(a + ns*b)];
        if (ppf == 0) {node1 = node1->next; continue;}
        int phase2 = 1;
        node1 = node1->next;
        if (ppf < 0) {
          ppf *= -1;
          phase2 = -1;
        }
        wf_list* node2 = n0_list_i[ipar + 2*(num_mj - imj - 1)];
        while (node2 != NULL) {
          int pn = node2->p;
          int index_i = -1;
          int index_f = -1;

          if (i_op == 0) {
            unsigned int p_hash_i = ppi + wd->n_sds_p_i*(pn % HASH_SIZE);
            wh_list* node3 = wd->wh_hash_i[p_hash_i];
            while (node3 != NULL) {
              if ((pn == node3->pn) && (ppi == node3->pp)) {
                index_i = node3->index;
                break;
              }
              node3 = node3->next;
            }
            if (index_i < 0) {node2 = node2->next; continue;}

            unsigned int p_hash_f = ppf + wd->n_sds_p_f*(pn % HASH_SIZE);
            node3 = wd->wh_hash_f[p_hash_f];
            while (node3 != NULL) {
              if ((pn == node3->pn) && (ppf == node3->pp)) {
                index_f = node3->index;
                break;
              }
              node3 = node3->next;
            }
            if (index_f < 0) {node2 = node2->next; continue;}
            eigen_list* eig_pair = transition;
            int i_trans = 0;
            while (eig_pair != NULL) {
              int psi_i = eig_pair->eig_i;
              int psi_f = eig_pair->eig_f;
	      density[i_trans] += wd->bc_i[psi_i + wd->n_eig_i*index_i]*wd->bc_f[psi_f + wd->n_eig_f*index_f]*phase1*phase2;
              i_trans++;
              eig_pair = eig_pair->next;
            }
           } else {
            unsigned int p_hash_i = pn + wd->n_sds_p_i*(ppi % HASH_SIZE);
            wh_list* node3 = wd->wh_hash_i[p_hash_i];
            while (node3 != NULL) {
              if ((pn == node3->pp) && (ppi == node3->pn)) {
                index_i = node3->index;
                break;
              }
              node3 = node3->next;
            }
            if (index_i < 0) {node2 = node2->next; continue;}

            unsigned int p_hash_f = pn + wd->n_sds_p_f*(ppf % HASH_SIZE);
            node3 = wd->wh_hash_f[p_hash_f];
            while (node3 != NULL) {
              if ((pn == node3->pp) && (ppf == node3->pn)) {
                index_f = node3->index;
                break;
              }
              node3 = node3->next;
            }
            if (index_f < 0) {node2 = node2->next; continue;}
            eigen_list* eig_pair = transition;
            int i_trans = 0;
            while (eig_pair != NULL) {
              int psi_i = eig_pair->eig_i;
              int psi_f = eig_pair->eig_f;
	      density[i_trans] += wd->bc_i[psi_i + wd->n_eig_i*index_i]*wd->bc_f[psi_f + wd->n_eig_f*index_f]*phase1*phase2;
              i_trans++;
              eig_pair = eig_pair->next;
            }
          } 
          node2 = node2->next;
        } 
      }
    }
   }
  return;
}

void trace_a22_nodes(int a, int b, int c, int d, int num_mj, sd_list** a2_list_i, sd_list** a2_list_f, wfnData* wd, int i_op, eigen_list* transition, double* density) {
/* 

*/
  int ns = wd->n_shells;
  for (int ipar = 0; ipar <= 1; ipar++) {
    for (int imj = 0; imj < num_mj; imj++) {
      sd_list* node1 = a2_list_i[ipar + 2*(imj + num_mj*(c + d*ns))];
      // Loop over final states resulting from 2x a_op
      while (node1 != NULL) {
        unsigned int ppf = node1->pn; // Get final state p_f
        unsigned int ppi = node1->pi; // Get initial state p_i
        int phase1 = node1->phase;
        // Get list of n_i associated to p_i
        sd_list* node2 = a2_list_f[ipar + 2*(num_mj - imj - 1 + num_mj*(a + b*ns))]; //hash corresponds to a_op operators
        // Loop over n_f  
        while (node2 != NULL) {
          unsigned int pni = node2->pi;
          unsigned int pnf = node2->pn;
          int phase2 = node2->phase;
          int index_i = -1;
          int index_f = -1;
          if (i_op == 0) {
            unsigned int p_hash_i = ppi + wd->n_sds_p_i*(pni % HASH_SIZE);
            wh_list* node3 = wd->wh_hash_i[p_hash_i];
            while (node3 != NULL) {
              if ((pni == node3->pn) && (ppi == node3->pp)) {
                index_i = node3->index;
                break;
              }
              node3 = node3->next;
            }
            if (index_i < 0) {node2 = node2->next; continue;}

            unsigned int p_hash_f = ppf + wd->n_sds_p_f*(pnf % HASH_SIZE);
            node3 = wd->wh_hash_f[p_hash_f];
            while (node3 != NULL) {
              if ((pnf == node3->pn) && (ppf == node3->pp)) {
                index_f = node3->index;
                break;
              }
              node3 = node3->next;
            }
            if (index_f < 0) {node2 = node2->next; continue;}
            eigen_list* eig_pair = transition;
            int i_trans = 0;
            while (eig_pair != NULL) {
              int psi_i = eig_pair->eig_i;
              int psi_f = eig_pair->eig_f;
	      density[i_trans] += wd->bc_i[psi_i + wd->n_eig_i*index_i]*wd->bc_f[psi_f + wd->n_eig_f*index_f]*phase1*phase2;
              i_trans++;
              eig_pair = eig_pair->next;
            }
           } else {
            unsigned int p_hash_i = pni + wd->n_sds_p_i*(ppi % HASH_SIZE);
            wh_list* node3 = wd->wh_hash_i[p_hash_i];
            while (node3 != NULL) {
              if ((pni == node3->pp) && (ppi == node3->pn)) {
                index_i = node3->index;
                break;
              }
              node3 = node3->next;
            }
            if (index_i < 0) {node2 = node2->next; continue;}

            unsigned int p_hash_f = pnf + wd->n_sds_p_f*(ppf % HASH_SIZE);
            node3 = wd->wh_hash_f[p_hash_f];
            while (node3 != NULL) {
              if ((pnf == node3->pp) && (ppf == node3->pn)) {
                index_f = node3->index;
                break;
              }
              node3 = node3->next;
            }
            if (index_f < 0) {node2 = node2->next; continue;}
            eigen_list* eig_pair = transition;
            int i_trans = 0;
            while (eig_pair != NULL) {
              int psi_i = eig_pair->eig_i;
              int psi_f = eig_pair->eig_f;
	      density[i_trans] += wd->bc_i[psi_i + wd->n_eig_i*index_i]*wd->bc_f[psi_f + wd->n_eig_f*index_f]*phase1*phase2;
              i_trans++;
              eig_pair = eig_pair->next;
            }
          } 
          node2 = node2->next;
        }
        node1 = node1->next;
      }
    }  
  }
  return;
}

void trace_a20_nodes(int a, int b, int c, int d, int num_mj, int n_sds_p_int1, int n_sds_n_int1, sd_list** p1_list_i, sd_list** n1_list_i, int* p1_array_f, int* n1_array_f, wfnData* wd, eigen_list* transition, double* density) {

  int ns = wd->n_shells;
  for (int ipar = 0; ipar <= 1; ipar++) {
    for (int imj = 0; imj < num_mj; imj++) {
      sd_list* node_pi = p1_list_i[ipar + 2*(imj + num_mj*b)]; // Get proton states resulting from p_b |p_i>
      while (node_pi != NULL) {
        int ppi = node_pi->pi; 
        int ppn = node_pi->pn; // Get pn = p_a |p_i>
        int phase1 = node_pi->phase;
        int phase2 = 1;
        int ppf = p1_array_f[(ppn - 1) + n_sds_p_int1*a]; // Get pf such that pn = p_a |p_f>
        if (ppf == 0) {node_pi = node_pi->next; continue;}
        if (ppf < 0) {
          ppf *= -1;
          phase2 = -1;
        }
        sd_list* node_ni = n1_list_i[ipar + 2*(num_mj - imj - 1 + num_mj*d)]; // Get neutron states resulting from n_d |n_i>
        while (node_ni != NULL) {
          int pni = node_ni->pi;
          int pnn = node_ni->pn; // Get nn = n_d |n_i>
          int phase3 = node_ni->phase;
          int phase4 = 1;
          int pnf = n1_array_f[(pnn - 1) + n_sds_n_int1*c];
          if (pnf == 0) {node_ni = node_ni->next; continue;}
          if (pnf < 0) {
            pnf *= -1;
            phase4 = -1;
          }
          unsigned int p_hash_i = ppi + wd->n_sds_p_i*(pni % HASH_SIZE);
          unsigned int p_hash_f = ppf + wd->n_sds_p_f*(pnf % HASH_SIZE);
          wh_list *node3 = wd->wh_hash_i[p_hash_i]; // hash corresponds to a_op operators
   
          int index_i = -1;
          int index_f = -1;
          while (node3 != NULL) {
            if ((pni == node3->pn) && (ppi == node3->pp)) {
              index_i = node3->index;
              break;
            }
            node3 = node3->next;
          }
          if (index_i < 0) {node_ni = node_ni->next; continue;}
          node3 = wd->wh_hash_f[p_hash_f];
          while (node3 != NULL) {
            if ((pnf == node3->pn) && (ppf == node3->pp)) {
              index_f = node3->index;
              break;
            }
            node3 = node3->next;
          }
          if (index_f < 0) {node_ni = node_ni->next; continue;}
          eigen_list* eig_pair = transition;
          int i_trans = 0;
          while (eig_pair != NULL) {
            int psi_i = eig_pair->eig_i;
            int psi_f = eig_pair->eig_f;
            density[i_trans] += wd->bc_i[psi_i + wd->n_eig_i*index_i]*wd->bc_f[psi_f + wd->n_eig_f*index_f]*phase1*phase2*phase3*phase4;
            i_trans++;
            eig_pair = eig_pair->next;
          }
          node_ni = node_ni->next;
        }
        node_pi = node_pi->next;
      }
    } 
  } 
  return;
}

void trace_a4_nodes_spec(int a, int b, int c, int d, int num_mj, int n_sds_int2, int* p2_array_f, sde_list** p2_list_i, wfe_list** n0_list_i, wfnData* wd, int i_op, eigen_list *transition, double* density, int n_q_spec_min, int n_spec_bins) {
  int ns = wd->n_shells;

  for (int ipar = 0; ipar <= 1; ipar++) {
    for (int imj = 0; imj < num_mj; imj++) {
      sde_list* node1 = p2_list_i[ipar + 2*(imj + num_mj*(c + d*ns))];
      while (node1 != NULL) {
        unsigned int ppn = node1->pn;
        unsigned int ppi = node1->pi;
        int phase1 = node1->phase;
        int n_q_spec_p = node1->n_quanta;
        int ppf = p2_array_f[(ppn - 1) + n_sds_int2*(a + ns*b)];
        if (ppf == 0) {node1 = node1->next; continue;}
        int phase2 = 1;
        node1 = node1->next;
        if (ppf < 0) {
          ppf *= -1;
          phase2 = -1;
        }
        wfe_list* node2 = n0_list_i[ipar + 2*(num_mj - imj - 1)];
        while (node2 != NULL) {
          int pn = node2->p;
          int n_q_spec_n = node2->n_quanta;
          int i_spec = n_q_spec_p + n_q_spec_n - n_q_spec_min;
          int index_i = -1;
          int index_f = -1;

          if (i_op == 0) {
            unsigned int p_hash_i = ppi + wd->n_sds_p_i*(pn % HASH_SIZE);
            wh_list* node3 = wd->wh_hash_i[p_hash_i];
            while (node3 != NULL) {
              if ((pn == node3->pn) && (ppi == node3->pp)) {
                index_i = node3->index;
                break;
              }
              node3 = node3->next;
            }
            if (index_i < 0) {node2 = node2->next; continue;}

            unsigned int p_hash_f = ppf + wd->n_sds_p_f*(pn % HASH_SIZE);
            node3 = wd->wh_hash_f[p_hash_f];
            while (node3 != NULL) {
              if ((pn == node3->pn) && (ppf == node3->pp)) {
                index_f = node3->index;
                break;
              }
              node3 = node3->next;
            }
            if (index_f < 0) {node2 = node2->next; continue;}
            eigen_list* eig_pair = transition;
            int i_trans = 0;
            while (eig_pair != NULL) {
              int psi_i = eig_pair->eig_i;
              int psi_f = eig_pair->eig_f;
              density[i_spec + n_spec_bins*i_trans] += wd->bc_i[psi_i + wd->n_eig_i*index_i]*wd->bc_f[psi_f + wd->n_eig_f*index_f]*phase1*phase2;
              i_trans++;
              eig_pair = eig_pair->next;
            }

          } else {
            unsigned int p_hash_i = pn + wd->n_sds_p_i*(ppi % HASH_SIZE);
            wh_list* node3 = wd->wh_hash_i[p_hash_i];
            while (node3 != NULL) {
              if ((pn == node3->pp) && (ppi == node3->pn)) {
                index_i = node3->index;
                break;
              }
              node3 = node3->next;
            }
            if (index_i < 0) {node2 = node2->next; continue;}

            unsigned int p_hash_f = pn + wd->n_sds_p_f*(ppf % HASH_SIZE);
            node3 = wd->wh_hash_f[p_hash_f];
            while (node3 != NULL) {
              if ((pn == node3->pp) && (ppf == node3->pn)) {
                index_f = node3->index;
                break;
              }
              node3 = node3->next;
            }
            if (index_f < 0) {node2 = node2->next; continue;}
            eigen_list* eig_pair = transition;
            int i_trans = 0;
            while (eig_pair != NULL) {
              int psi_i = eig_pair->eig_i;
              int psi_f = eig_pair->eig_f;
              density[i_spec + n_spec_bins*i_trans] += wd->bc_i[psi_i + wd->n_eig_i*index_i]*wd->bc_f[psi_f + wd->n_eig_f*index_f]*phase1*phase2;
              i_trans++;
              eig_pair = eig_pair->next;
            }
          } 
          node2 = node2->next;
        } 
      }
    }
   }
  return;
}

void trace_a22_nodes_spec(int a, int b, int c, int d, int num_mj, sde_list** a2_list_i, sde_list** a2_list_f, wfnData* wd, int i_op, eigen_list *transition, double* density, int n_q_spec_min, int n_spec_bins) {
/* 

*/
  double total = 0.0;
  int ns = wd->n_shells;
  for (int ipar = 0; ipar <=1; ipar++) {
    for (int imj = 0; imj < num_mj; imj++) {
      sde_list* node1 = a2_list_i[ipar + 2*(imj + num_mj*(c + d*ns))];
      // Loop over final states resulting from 2x a_op
      while (node1 != NULL) {
        unsigned int ppf = node1->pn; // Get final state p_f
        unsigned int ppi = node1->pi; // Get initial state p_i
        int phase1 = node1->phase;
        int n_q_spec_p = node1->n_quanta;
        // Get list of n_i associated to p_i
        sde_list* node2 = a2_list_f[ipar + 2*(num_mj - imj - 1 + num_mj*(a + b*ns))]; //hash corresponds to a_op operators
        // Loop over n_f  
        while (node2 != NULL) {
          unsigned int pni = node2->pi;
          unsigned int pnf = node2->pn;
          int phase2 = node2->phase;
          int n_q_spec_n = node2->n_quanta;
          int i_spec = n_q_spec_p + n_q_spec_n - n_q_spec_min;
          int index_i = -1;
          int index_f = -1;
          if (i_op == 0) {
            unsigned int p_hash_i = ppi + wd->n_sds_p_i*(pni % HASH_SIZE);
            wh_list* node3 = wd->wh_hash_i[p_hash_i];
            while (node3 != NULL) {
              if ((pni == node3->pn) && (ppi == node3->pp)) {
                index_i = node3->index;
                break;
              }
              node3 = node3->next;
            }
            if (index_i < 0) {node2 = node2->next; continue;}

            unsigned int p_hash_f = ppf + wd->n_sds_p_f*(pnf % HASH_SIZE);
            node3 = wd->wh_hash_f[p_hash_f];
            while (node3 != NULL) {
              if ((pnf == node3->pn) && (ppf == node3->pp)) {
                index_f = node3->index;
                break;
              }
              node3 = node3->next;
            }
            if (index_f < 0) {node2 = node2->next; continue;}
            eigen_list* eig_pair = transition;
            int i_trans = 0;
            while (eig_pair != NULL) {
              int psi_i = eig_pair->eig_i;
              int psi_f = eig_pair->eig_f;
	      density[i_spec + n_spec_bins*i_trans] += wd->bc_i[psi_i + wd->n_eig_i*index_i]*wd->bc_f[psi_f + wd->n_eig_f*index_f]*phase1*phase2;
              i_trans++;
              eig_pair = eig_pair->next;
            }
           } else {
            unsigned int p_hash_i = pni + wd->n_sds_p_i*(ppi % HASH_SIZE);
            wh_list* node3 = wd->wh_hash_i[p_hash_i];
            while (node3 != NULL) {
              if ((pni == node3->pp) && (ppi == node3->pn)) {
                index_i = node3->index;
                break;
              }
              node3 = node3->next;
            }
            if (index_i < 0) {node2 = node2->next; continue;}

            unsigned int p_hash_f = pnf + wd->n_sds_p_f*(ppf % HASH_SIZE);
            node3 = wd->wh_hash_f[p_hash_f];
            while (node3 != NULL) {
              if ((pnf == node3->pp) && (ppf == node3->pn)) {
                index_f = node3->index;
                break;
              }
              node3 = node3->next;
            }
            if (index_f < 0) {node2 = node2->next; continue;}
            eigen_list* eig_pair = transition;
            int i_trans = 0;
            while (eig_pair != NULL) {
              int psi_i = eig_pair->eig_i;
              int psi_f = eig_pair->eig_f;
	      density[i_spec + n_spec_bins*i_trans] += wd->bc_i[psi_i + wd->n_eig_i*index_i]*wd->bc_f[psi_f + wd->n_eig_f*index_f]*phase1*phase2;
              i_trans++;
              eig_pair = eig_pair->next;
            }
          } 
          node2 = node2->next;
        }
        node1 = node1->next;
      }
    }  
  }
  return;
}

void trace_a20_nodes_spec(int a, int b, int c, int d, int num_mj, int n_sds_p_int1, int n_sds_n_int1, sde_list** p1_list_i, sde_list** n1_list_i, int* p1_array_f, int* n1_array_f, wfnData* wd, eigen_list *transition, double* density, int n_q_spec_min, int n_spec_bins) {

  int ns = wd->n_shells;
  for (int ipar = 0; ipar <= 1; ipar++) {
    for (int imj = 0; imj < num_mj; imj++) {
      sde_list* node_pi = p1_list_i[ipar + 2*(imj + num_mj*b)]; // Get proton states resulting from p_b |p_i>
      while (node_pi != NULL) {
        int ppi = node_pi->pi; 
        int ppn = node_pi->pn; // Get pn = p_a |p_i>
        int phase1 = node_pi->phase;
        int phase2 = 1;
        int n_q_spec_p = node_pi->n_quanta;
        int ppf = p1_array_f[(ppn - 1) + n_sds_p_int1*a]; // Get pf such that pn = p_a |p_f>
        if (ppf == 0) {node_pi = node_pi->next; continue;}
        if (ppf < 0) {
          ppf *= -1;
          phase2 = -1;
        }
        sde_list* node_ni = n1_list_i[ipar + 2*(num_mj - imj - 1 + num_mj*d)]; // Get neutron states resulting from n_d |n_i>
        while (node_ni != NULL) {
          int pni = node_ni->pi;
          int pnn = node_ni->pn; // Get nn = n_d |n_i>
          int phase3 = node_ni->phase;
          int n_q_spec_n = node_ni->n_quanta;
          int phase4 = 1;
          int pnf = n1_array_f[(pnn - 1) + n_sds_n_int1*c];
          if (pnf == 0) {node_ni = node_ni->next; continue;}
          if (pnf < 0) {
            pnf *= -1;
            phase4 = -1;
          }
          int i_spec = n_q_spec_p + n_q_spec_n - n_q_spec_min;
          unsigned int p_hash_i = ppi + wd->n_sds_p_i*(pni % HASH_SIZE);
          unsigned int p_hash_f = ppf + wd->n_sds_p_f*(pnf % HASH_SIZE);
          wh_list *node3 = wd->wh_hash_i[p_hash_i]; // hash corresponds to a_op operators
   
          int index_i = -1;
          int index_f = -1;
          while (node3 != NULL) {
            if ((pni == node3->pn) && (ppi == node3->pp)) {
              index_i = node3->index;
              break;
            }
            node3 = node3->next;
          }
          if (index_i < 0) {node_ni = node_ni->next; continue;}
          node3 = wd->wh_hash_f[p_hash_f];
          while (node3 != NULL) {
            if ((pnf == node3->pn) && (ppf == node3->pp)) {
              index_f = node3->index;
              break;
            }
            node3 = node3->next;
          }
          if (index_f < 0) {node_ni = node_ni->next; continue;}
          eigen_list* eig_pair = transition;
          int i_trans = 0;
          while (eig_pair != NULL) {
            int psi_i = eig_pair->eig_i;
            int psi_f = eig_pair->eig_f;
	    density[i_spec + n_spec_bins*i_trans] += wd->bc_i[psi_i + wd->n_eig_i*index_i]*wd->bc_f[psi_f + wd->n_eig_f*index_f]*phase1*phase2;
            i_trans++;
            eig_pair = eig_pair->next;
          }
          node_ni = node_ni->next;
        }
        node_pi = node_pi->next;
      }
    } 
  } 
  return;
}

void build_two_body_jumps_i_and_f_spec(int n_s, int n_p, float mj_min, float mj_max, int num_mj, int n_sds_i, int n_sds_int1, int n_sds_int2, int*a1_array_f, int* a2_array_f, wfe_list** a0_list_i, sde_list** a1_list_i, sde_list** a2_list_i, int* jz_shell, int* l_shell, int* n_shell) {
/*
  Input(s):
    mj_min_i: 
*/
  for (int j = 1; j <= n_sds_i; j++) {
    int j_min = j_min_from_p(n_s, n_p, j);
    float mj;
    int parity, n_quanta;
    get_m_pi_q(j, n_s, n_p, n_shell, l_shell, jz_shell, &mj, &parity, &n_quanta, j_min);
    if ((mj < mj_min) || (mj > mj_max)) {continue;}
    int i_parity = (parity + 1)/2;
    int i_mj = mj - mj_min;
    if (a0_list_i[i_parity + 2*i_mj] == NULL) {
      a0_list_i[i_parity + 2*i_mj] = create_wfe_node(j, n_quanta, NULL);
    } else {
      wfe_append(a0_list_i[i_parity + 2*i_mj], j, n_quanta);
    }
    for (int b = j_min - 1; b < n_s; b++) {
      int phase1;
      int pn1 = a_op(n_s, n_p, j, b + 1, &phase1, j_min);
      if (pn1 == 0) {continue;}
      int n_spec1 = n_quanta - (2*n_shell[b] + l_shell[b]);
      a1_array_f[(pn1 - 1) + b*n_sds_int1] = j*phase1;
      if (a1_list_i[i_parity + 2*(i_mj + num_mj*b)] == NULL) {
        a1_list_i[i_parity + 2*(i_mj + num_mj*b)] = create_sde_node(j, pn1, phase1, n_spec1, NULL);
      } else {
        sde_append(a1_list_i[i_parity + 2*(i_mj + num_mj*b)], j, pn1, phase1, n_spec1);
      }
      for (int a = j_min - 1; a < b; a++) {
        int phase2;
        int pn2 = a_op(n_s, n_p - 1, pn1, a + 1, &phase2, j_min);
        if (pn2 == 0) {continue;}
        int n_spec2 = n_spec1 - (2*n_shell[a] + l_shell[a]);
        if (a2_list_i[i_parity + 2*(i_mj + num_mj*(b + a*n_s))] == NULL) {
          a2_list_i[i_parity + 2*(i_mj + num_mj*(b + a*n_s))] = create_sde_node(j, pn2, phase1*phase2, n_spec2, NULL);
        } else {
          sde_append(a2_list_i[i_parity + 2*(i_mj + num_mj*(b + a*n_s))], j, pn2, phase1*phase2, n_spec2);
        }
        if (a2_list_i[i_parity + 2*(i_mj + num_mj*(a + b*n_s))] == NULL) {
          a2_list_i[i_parity + 2*(i_mj + num_mj*(a + b*n_s))] = create_sde_node(j, pn2, -phase1*phase2, n_spec2, NULL);
        } else {
          sde_append(a2_list_i[i_parity + 2*(i_mj + num_mj*(a + b*n_s))], j, pn2, -phase1*phase2, n_spec2);
        }
        a2_array_f[(pn2 - 1) + n_sds_int2*(b + a*n_s)] = phase1*phase2*j;
        a2_array_f[(pn2 - 1) + n_sds_int2*(a + b*n_s)] = -phase1*phase2*j;
      }
    }
  } 

  return;
}

void build_two_body_jumps_i_spec(int n_s, int n_p, float mj_min, float mj_max, int num_mj, int n_sds_i, wfe_list** a0_list_i, sde_list** a1_list_i, sde_list** a2_list_i, int* jz_shell, int* l_shell, int* n_shell) {

  for (int j = 1; j <= n_sds_i; j++) {
    int j_min = j_min_from_p(n_s, n_p, j);
    float mj;
    int parity, n_quanta;
    get_m_pi_q(j, n_s, n_p, n_shell, l_shell, jz_shell, &mj, &parity, &n_quanta, j_min);
    if ((mj < mj_min) || (mj > mj_max)) {continue;}
    int i_mj = mj - mj_min;
    int i_parity = (parity + 1)/2;
    if (a0_list_i[i_parity + 2*i_mj] == NULL) {
      a0_list_i[i_parity + 2*i_mj] = create_wfe_node(j, n_quanta, NULL);
    } else {
      wfe_append(a0_list_i[i_parity + 2*i_mj], j, n_quanta);
    }

    for (int b = j_min - 1; b < n_s; b++) {
      int phase1;
      int pn1 = a_op(n_s, n_p, j, b + 1, &phase1, j_min);
      if (pn1 == 0) {continue;}
      int n_spec1 = n_quanta - (2*n_shell[b] + l_shell[b]);
      if (a1_list_i[i_parity + 2*(i_mj + num_mj*b)] == NULL) {
        a1_list_i[i_parity + 2*(i_mj + num_mj*b)] = create_sde_node(j, pn1, phase1, n_spec1, NULL);
      } else {
        sde_append(a1_list_i[i_parity + 2*(i_mj + num_mj*b)], j, pn1, phase1, n_spec1);
      }
      for (int a = j_min - 1; a < b; a++) {
        int phase2;
        int pn2 = a_op(n_s, n_p - 1, pn1, a + 1, &phase2, j_min);
        if (pn2 == 0) {continue;}
        int n_spec2 = n_spec1 - (2*n_shell[a] + l_shell[a]);
        if (a2_list_i[i_parity + 2*(i_mj + num_mj*(b + a*n_s))] == NULL) {
          a2_list_i[i_parity + 2*(i_mj + num_mj*(b + a*n_s))] = create_sde_node(j, pn2, phase1*phase2, n_spec2, NULL);
        } else {
          sde_append(a2_list_i[i_parity + 2*(i_mj + num_mj*(b + a*n_s))], j, pn2, phase1*phase2, n_spec2);
        }
        if (a2_list_i[i_parity + 2*(i_mj + num_mj*(a + b*n_s))] == NULL) {
          a2_list_i[i_parity + 2*(i_mj + num_mj*(a + b*n_s))] = create_sde_node(j, pn2, -phase1*phase2, n_spec2, NULL);
        } else {
          sde_append(a2_list_i[i_parity + 2*(i_mj + num_mj*(a + b*n_s))], j, pn2, -phase1*phase2, n_spec2);
        }
      }
    }
  }

  return;
}

void build_two_body_jumps_f_spec(int n_s, int n_p, float mj_min, float mj_max, int num_mj, int n_sds_f, int n_sds_int1, int n_sds_int2, int* a1_array_f, int* a2_array_f, sde_list** a2_list_f, int* jz_shell, int* l_shell, int* n_shell) {
  
  for (int j = 1; j <= n_sds_f; j++) {
    int j_min = j_min_from_p(n_s, n_p, j);
    for (int b = j_min - 1; b < n_s; b++) {
      int phase1;
      int pn1 = a_op(n_s, n_p, j, b + 1, &phase1, j_min);
      if (pn1 == 0) {continue;}
      a1_array_f[(pn1 - 1) + b*n_sds_int1] = j*phase1;
      for (int a = j_min - 1; a < b; a++) {
        int phase2;
        int pn2 = a_op(n_s, n_p - 1, pn1, a + 1, &phase2, j_min);
        if (pn2 == 0) {continue;}
        a2_array_f[(pn2 - 1) + n_sds_int2*(b + a*n_s)] = phase1*phase2*j;
        a2_array_f[(pn2 - 1) + n_sds_int2*(a + b*n_s)] = -phase1*phase2*j;
        float mj;
        int parity, n_quanta;
        get_m_pi_q(pn2, n_s, n_p - 2, n_shell, l_shell, jz_shell, &mj, &parity, &n_quanta, j_min);
        if ((mj > mj_max) || (mj < mj_min)) {continue;}
        int i_mj = mj - mj_min;
        int i_parity = (parity + 1)/2;
        if (a2_list_f[i_parity + 2*(i_mj + num_mj*(b + a*n_s))] == NULL) {
          a2_list_f[i_parity + 2*(i_mj + num_mj*(b + a*n_s))] = create_sde_node(pn2, j, phase1*phase2, n_quanta, NULL);
        } else {
          sde_append(a2_list_f[i_parity + 2*(i_mj + num_mj*(b + a*n_s))], pn2, j, phase1*phase2, n_quanta);
        }
        if (a2_list_f[i_parity + 2*(i_mj + num_mj*(a + b*n_s))] == NULL) {
          a2_list_f[i_parity + 2*(i_mj + num_mj*(a + b*n_s))] = create_sde_node(pn2, j, -phase1*phase2, n_quanta, NULL);
        } else {
          sde_append(a2_list_f[i_parity + 2*(i_mj + num_mj*(a + b*n_s))], pn2, j, -phase1*phase2, n_quanta);
        }
      }
    }
  }

  return;
}

void build_two_body_jumps_i_and_f_trunc(int n_s, int n_p, float mj_min, float mj_max, int num_mj, int n_sds_i, int n_sds_int1, int n_sds_int2, int*a1_array_f, int* a2_array_f, wf_list** a0_list_i, sd_list** a1_list_i, sd_list** a2_list_i, int* jz_shell, int* l_shell, int* w_shell, int w_max) {
/*
  Input(s):
    mj_min_i: 
*/
  for (int j = 1; j <= n_sds_i; j++) {
    if (w_from_p(j, n_s, n_p, w_shell) > w_max) {continue;}
    int j_min = j_min_from_p(n_s, n_p, j);
    float mj = m_from_p(j, n_s, n_p, jz_shell);
    if ((mj < mj_min) || (mj > mj_max)) {continue;}
    int i_parity = (parity_from_p(j, n_s, n_p, l_shell) + 1)/2;
    int i_mj = mj - mj_min;
    if (a0_list_i[i_parity + 2*i_mj] == NULL) {
      a0_list_i[i_parity + 2*i_mj] = create_wf_node(j, NULL);
    } else {
      wf_append(a0_list_i[i_parity + 2*i_mj], j);
    }
    for (int b = j_min - 1; b < n_s; b++) {
      int phase1;
      int pn1 = a_op(n_s, n_p, j, b + 1, &phase1, j_min);
      if (pn1 == 0) {continue;}
      a1_array_f[(pn1 - 1) + b*n_sds_int1] = j*phase1;
      if (a1_list_i[i_parity + 2*(i_mj + num_mj*b)] == NULL) {
        a1_list_i[i_parity + 2*(i_mj + num_mj*b)] = create_sd_node(j, pn1, phase1, NULL);
      } else {
        sd_append(a1_list_i[i_parity + 2*(i_mj + num_mj*b)], j, pn1, phase1);
      }
      for (int a = j_min - 1; a < b; a++) {
        int phase2;
        int pn2 = a_op(n_s, n_p - 1, pn1, a + 1, &phase2, j_min);
        if (pn2 == 0) {continue;}
        if (a2_list_i[i_parity + 2*(i_mj + num_mj*(b + a*n_s))] == NULL) {
          a2_list_i[i_parity + 2*(i_mj + num_mj*(b + a*n_s))] = create_sd_node(j, pn2, phase1*phase2, NULL);
        } else {
          sd_append(a2_list_i[i_parity + 2*(i_mj + num_mj*(b + a*n_s))], j, pn2, phase1*phase2);
        }
        if (a2_list_i[i_parity + 2*(i_mj + num_mj*(a + b*n_s))] == NULL) {
          a2_list_i[i_parity + 2*(i_mj + num_mj*(a + b*n_s))] = create_sd_node(j, pn2, -phase1*phase2, NULL);
        } else {
          sd_append(a2_list_i[i_parity + 2*(i_mj + num_mj*(a + b*n_s))], j, pn2, -phase1*phase2);
        }
        a2_array_f[(pn2 - 1) + n_sds_int2*(b + a*n_s)] = phase1*phase2*j;
        a2_array_f[(pn2 - 1) + n_sds_int2*(a + b*n_s)] = -phase1*phase2*j;
      }
    }
  } 

  return;
}


void build_two_body_jumps_i_and_f(int n_s, int n_p, float mj_min, float mj_max, int num_mj, int n_sds_i, int n_sds_int1, int n_sds_int2, int*a1_array_f, int* a2_array_f, wf_list** a0_list_i, sd_list** a1_list_i, sd_list** a2_list_i, int* jz_shell, int* l_shell) {
/*
  Input(s):
    mj_min_i: 
*/
  for (int j = 1; j <= n_sds_i; j++) {
    int j_min = j_min_from_p(n_s, n_p, j);
    float mj = m_from_p(j, n_s, n_p, jz_shell);
    if ((mj < mj_min) || (mj > mj_max)) {continue;}
    int i_parity = (parity_from_p(j, n_s, n_p, l_shell) + 1)/2;
    int i_mj = mj - mj_min;
    if (a0_list_i[i_parity + 2*i_mj] == NULL) {
      a0_list_i[i_parity + 2*i_mj] = create_wf_node(j, NULL);
    } else {
      wf_append(a0_list_i[i_parity + 2*i_mj], j);
    }
    for (int b = j_min - 1; b < n_s; b++) {
      int phase1;
      int pn1 = a_op(n_s, n_p, j, b + 1, &phase1, j_min);
      if (pn1 == 0) {continue;}
      a1_array_f[(pn1 - 1) + b*n_sds_int1] = j*phase1;
      if (a1_list_i[i_parity + 2*(i_mj + num_mj*b)] == NULL) {
        a1_list_i[i_parity + 2*(i_mj + num_mj*b)] = create_sd_node(j, pn1, phase1, NULL);
      } else {
        sd_append(a1_list_i[i_parity + 2*(i_mj + num_mj*b)], j, pn1, phase1);
      }
      for (int a = j_min - 1; a < b; a++) {
        int phase2;
        int pn2 = a_op(n_s, n_p - 1, pn1, a + 1, &phase2, j_min);
        if (pn2 == 0) {continue;}
        if (a2_list_i[i_parity + 2*(i_mj + num_mj*(b + a*n_s))] == NULL) {
          a2_list_i[i_parity + 2*(i_mj + num_mj*(b + a*n_s))] = create_sd_node(j, pn2, phase1*phase2, NULL);
        } else {
          sd_append(a2_list_i[i_parity + 2*(i_mj + num_mj*(b + a*n_s))], j, pn2, phase1*phase2);
        }
        if (a2_list_i[i_parity + 2*(i_mj + num_mj*(a + b*n_s))] == NULL) {
          a2_list_i[i_parity + 2*(i_mj + num_mj*(a + b*n_s))] = create_sd_node(j, pn2, -phase1*phase2, NULL);
        } else {
          sd_append(a2_list_i[i_parity + 2*(i_mj + num_mj*(a + b*n_s))], j, pn2, -phase1*phase2);
        }
        a2_array_f[(pn2 - 1) + n_sds_int2*(b + a*n_s)] = phase1*phase2*j;
        a2_array_f[(pn2 - 1) + n_sds_int2*(a + b*n_s)] = -phase1*phase2*j;
      }
    }
  } 

  return;
}

void build_two_body_jumps_i(int n_s, int n_p, float mj_min, float mj_max, int num_mj, int n_sds_i, wf_list** a0_list_i, sd_list** a1_list_i, sd_list** a2_list_i, int* jz_shell, int* l_shell) {

  for (int j = 1; j <= n_sds_i; j++) {
    int j_min = j_min_from_p(n_s, n_p, j);
    float mj = m_from_p(j, n_s, n_p, jz_shell);
    if ((mj < mj_min) || (mj > mj_max)) {continue;}
    int i_mj = mj - mj_min;
    int i_parity = (parity_from_p(j, n_s, n_p, l_shell) + 1)/2;
    if (a0_list_i[i_parity + 2*i_mj] == NULL) {
      a0_list_i[i_parity + 2*i_mj] = create_wf_node(j, NULL);
    } else {
      wf_append(a0_list_i[i_parity + 2*i_mj], j);
    }

    for (int b = j_min - 1; b < n_s; b++) {
      int phase1;
      int pn1 = a_op(n_s, n_p, j, b + 1, &phase1, j_min);
      if (pn1 == 0) {continue;}
      if (a1_list_i[i_parity + 2*(i_mj + num_mj*b)] == NULL) {
        a1_list_i[i_parity + 2*(i_mj + num_mj*b)] = create_sd_node(j, pn1, phase1, NULL);
      } else {
        sd_append(a1_list_i[i_parity + 2*(i_mj + num_mj*b)], j, pn1, phase1);
      }
      for (int a = j_min - 1; a < b; a++) {
        int phase2;
        int pn2 = a_op(n_s, n_p - 1, pn1, a + 1, &phase2, j_min);
        if (pn2 == 0) {continue;}
        if (a2_list_i[i_parity + 2*(i_mj + num_mj*(b + a*n_s))] == NULL) {
          a2_list_i[i_parity + 2*(i_mj + num_mj*(b + a*n_s))] = create_sd_node(j, pn2, phase1*phase2, NULL);
        } else {
          sd_append(a2_list_i[i_parity + 2*(i_mj + num_mj*(b + a*n_s))], j, pn2, phase1*phase2);
        }
        if (a2_list_i[i_parity + 2*(i_mj + num_mj*(a + b*n_s))] == NULL) {
          a2_list_i[i_parity + 2*(i_mj + num_mj*(a + b*n_s))] = create_sd_node(j, pn2, -phase1*phase2, NULL);
        } else {
          sd_append(a2_list_i[i_parity + 2*(i_mj + num_mj*(a + b*n_s))], j, pn2, -phase1*phase2);
        }
      }
    }
  }

  return;
}

void build_two_body_jumps_f(int n_s, int n_p, float mj_min, float mj_max, int num_mj, int n_sds_f, int n_sds_int1, int n_sds_int2, int* a1_array_f, int* a2_array_f, sd_list** a2_list_f, int* jz_shell, int* l_shell) {
  
  for (int j = 1; j <= n_sds_f; j++) {
    int j_min = j_min_from_p(n_s, n_p, j);
    for (int b = j_min - 1; b < n_s; b++) {
      int phase1;
      int pn1 = a_op(n_s, n_p, j, b + 1, &phase1, j_min);
      if (pn1 == 0) {continue;}
      a1_array_f[(pn1 - 1) + b*n_sds_int1] = j*phase1;
      for (int a = j_min - 1; a < b; a++) {
        int phase2;
        int pn2 = a_op(n_s, n_p - 1, pn1, a + 1, &phase2, j_min);
        if (pn2 == 0) {continue;}
        a2_array_f[(pn2 - 1) + n_sds_int2*(b + a*n_s)] = phase1*phase2*j;
        a2_array_f[(pn2 - 1) + n_sds_int2*(a + b*n_s)] = -phase1*phase2*j;
        float mj = m_from_p(pn2, n_s, n_p - 2, jz_shell);
        if ((mj > mj_max) || (mj < mj_min)) {continue;}
        int i_mj = mj - mj_min;
        int i_parity = (parity_from_p(pn2, n_s, n_p - 2, l_shell) + 1)/2;
        if (a2_list_f[i_parity + 2*(i_mj + num_mj*(b + a*n_s))] == NULL) {
          a2_list_f[i_parity + 2*(i_mj + num_mj*(b + a*n_s))] = create_sd_node(pn2, j, phase1*phase2, NULL);
        } else {
          sd_append(a2_list_f[i_parity + 2*(i_mj + num_mj*(b + a*n_s))], pn2, j, phase1*phase2);
        }
        if (a2_list_f[i_parity + 2*(i_mj + num_mj*(a + b*n_s))] == NULL) {
          a2_list_f[i_parity + 2*(i_mj + num_mj*(a + b*n_s))] = create_sd_node(pn2, j, -phase1*phase2, NULL);
        } else {
          sd_append(a2_list_f[i_parity + 2*(i_mj + num_mj*(a + b*n_s))], pn2, j, -phase1*phase2);
        }
      }
    }
  }

  return;
}

void build_one_body_jumps_i_and_f_spec(int n_s, int n_p, float mj_min, float mj_max, int num_mj, int n_sds_i, int n_sds_int, int*a1_array_f, wfe_list** a0_list_i, sde_list** a1_list_i, int* jz_shell, int* l_shell, int* n_shell) {
/*
  Input(s):
    mj_min_i: 
*/
  for (int j = 1; j <= n_sds_i; j++) {
    int j_min = j_min_from_p(n_s, n_p, j);
    int n_quanta, parity;
    float mj;
    get_m_pi_q(j, n_s, n_p, n_shell, l_shell, jz_shell, &mj, &parity, &n_quanta, j_min);
    if ((mj < mj_min) || (mj > mj_max)) {continue;}
    int i_parity = (parity + 1)/2;
    int i_mj = mj - mj_min;
    if (a0_list_i[i_parity + 2*i_mj] == NULL) {
      a0_list_i[i_parity + 2*i_mj] = create_wfe_node(j, n_quanta, NULL);
    } else {
      wfe_append(a0_list_i[i_parity + 2*i_mj], j, n_quanta);
    }
    for (int a = j_min - 1; a < n_s; a++) {
      int phase;
      int pn = a_op(n_s, n_p, j, a + 1, &phase, j_min);
      if (pn == 0) {continue;}
      int n_spec1 = n_quanta - (2*n_shell[a] + l_shell[a]);
      a1_array_f[(pn - 1) + a*n_sds_int] = j*phase;
      if (a1_list_i[i_parity + 2*(i_mj + num_mj*a)] == NULL) {
        a1_list_i[i_parity + 2*(i_mj + num_mj*a)] = create_sde_node(j, pn, phase, n_spec1, NULL);
      } else {
        sde_append(a1_list_i[i_parity + 2*(i_mj + num_mj*a)], j, pn, phase, n_spec1);
      }
    }
  } 

  return;
}

void build_one_body_jumps_i_spec(int n_s, int n_p, float mj_min, float mj_max, int num_mj, int n_sds_i, wfe_list** a0_list_i, sde_list** a1_list_i,  int* jz_shell, int* l_shell, int* n_shell) {

  for (int j = 1; j <= n_sds_i; j++) {
    int j_min = j_min_from_p(n_s, n_p, j);
    float mj;
    int parity, n_quanta;
    get_m_pi_q(j, n_s, n_p, n_shell, l_shell, jz_shell, &mj, &parity, &n_quanta, j_min);
    if ((mj < mj_min) || (mj > mj_max)) {continue;}
    int i_mj = mj - mj_min;
    int i_parity = (parity + 1)/2;
    if (a0_list_i[i_parity + 2*i_mj] == NULL) {
      a0_list_i[i_parity + 2*i_mj] = create_wfe_node(j, n_quanta, NULL);
    } else {
      wfe_append(a0_list_i[i_parity + 2*i_mj], j, n_quanta);
    }

    for (int a = j_min - 1; a < n_s; a++) {
      int phase;
      int pn = a_op(n_s, n_p, j, a + 1, &phase, j_min);
      if (pn == 0) {continue;}
      int n_spec1 = n_quanta - (2*n_shell[a] + l_shell[a]);
      if (a1_list_i[i_parity + 2*(i_mj + num_mj*a)] == NULL) {
        a1_list_i[i_parity + 2*(i_mj + num_mj*a)] = create_sde_node(j, pn, phase, n_spec1, NULL);
      } else {
        sde_append(a1_list_i[i_parity + 2*(i_mj + num_mj*a)], j, pn, phase, n_spec1);
      }
    }
  }

  return;
}

void build_one_body_jumps_f_spec(int n_s, int n_p, float mj_min, float mj_max, int num_mj, int n_sds_f, int n_sds_int, int* a1_array_f, sde_list** a1_list_f, int* jz_shell, int* l_shell, int* n_shell) {
  
  for (int j = 1; j <= n_sds_f; j++) {
    int j_min = j_min_from_p(n_s, n_p, j);
    for (int a = j_min - 1; a < n_s; a++) {
      int phase;
      int pn = a_op(n_s, n_p, j, a + 1, &phase, j_min);
      if (pn == 0) {continue;}
      a1_array_f[(pn - 1) + a*n_sds_int] = j*phase;
      float mj;
      int parity, n_quanta;
      get_m_pi_q(j, n_s, n_p, n_shell, l_shell, jz_shell, &mj, &parity, &n_quanta, j_min);
      if ((mj > mj_max) || (mj < mj_min)) {continue;}
      int i_mj = mj - mj_min;
      int i_parity = (parity + 1)/2;
      if (a1_list_f[i_parity + 2*(i_mj + num_mj*a)] == NULL) {
        a1_list_f[i_parity + 2*(i_mj + num_mj*a)] = create_sde_node(pn, j, phase, n_quanta, NULL);
      } else {
        sde_append(a1_list_f[i_parity + 2*(i_mj + num_mj*a)], pn, j, phase, n_quanta);
      }
    }
  }

  return;
}


void build_one_body_jumps_i_and_f_trunc(int n_s, int n_p, float mj_min, float mj_max, int num_mj, int n_sds_i, int n_sds_int, int*a1_array_f, wf_list** a0_list_i, sd_list** a1_list_i, int* jz_shell, int* l_shell, int* w_shell, int w_max) {
/*
  Input(s):
    mj_min_i: 
*/
  for (int j = 1; j <= n_sds_i; j++) {
    if (w_from_p(j, n_s, n_p, w_shell) > w_max) {continue;}
    int j_min = j_min_from_p(n_s, n_p, j);
    float mj = m_from_p(j, n_s, n_p, jz_shell);
    if ((mj < mj_min) || (mj > mj_max)) {continue;}
    int i_parity = (parity_from_p(j, n_s, n_p, l_shell) + 1)/2;
    int i_mj = mj - mj_min;
    if (a0_list_i[i_parity + 2*i_mj] == NULL) {
      a0_list_i[i_parity + 2*i_mj] = create_wf_node(j, NULL);
    } else {
      wf_append(a0_list_i[i_parity + 2*i_mj], j);
    }
    for (int a = j_min - 1; a < n_s; a++) {
      int phase;
      int pn = a_op(n_s, n_p, j, a + 1, &phase, j_min);
      if (pn == 0) {continue;}
      a1_array_f[(pn - 1) + a*n_sds_int] = j*phase;
      if (a1_list_i[i_parity + 2*(i_mj + num_mj*a)] == NULL) {
        a1_list_i[i_parity + 2*(i_mj + num_mj*a)] = create_sd_node(j, pn, phase, NULL);
      } else {
        sd_append(a1_list_i[i_parity + 2*(i_mj + num_mj*a)], j, pn, phase);
      }
    }
  } 

  return;
}


void build_one_body_jumps_i_and_f(int n_s, int n_p, float mj_min, float mj_max, int num_mj, int n_sds_i, int n_sds_int, int*a1_array_f, wf_list** a0_list_i, sd_list** a1_list_i, int* jz_shell, int* l_shell) {
/*
  Input(s):
    mj_min_i: 
*/
  for (int j = 1; j <= n_sds_i; j++) {
    int j_min = j_min_from_p(n_s, n_p, j);
    float mj = m_from_p(j, n_s, n_p, jz_shell);
    if ((mj < mj_min) || (mj > mj_max)) {continue;}
    int i_parity = (parity_from_p(j, n_s, n_p, l_shell) + 1)/2;
    int i_mj = mj - mj_min;
    if (a0_list_i[i_parity + 2*i_mj] == NULL) {
      a0_list_i[i_parity + 2*i_mj] = create_wf_node(j, NULL);
    } else {
      wf_append(a0_list_i[i_parity + 2*i_mj], j);
    }
    for (int a = j_min - 1; a < n_s; a++) {
      int phase;
      int pn = a_op(n_s, n_p, j, a + 1, &phase, j_min);
      if (pn == 0) {continue;}
      a1_array_f[(pn - 1) + a*n_sds_int] = j*phase;
      if (a1_list_i[i_parity + 2*(i_mj + num_mj*a)] == NULL) {
        a1_list_i[i_parity + 2*(i_mj + num_mj*a)] = create_sd_node(j, pn, phase, NULL);
      } else {
        sd_append(a1_list_i[i_parity + 2*(i_mj + num_mj*a)], j, pn, phase);
      }
    }
  } 

  return;
}

void build_one_body_jumps_i_trunc(int n_s, int n_p, float mj_min, float mj_max, int num_mj, int n_sds_i, wf_list** a0_list_i, sd_list** a1_list_i,  int* jz_shell, int* l_shell, int* w_shell, int w_max) {

  for (int j = 1; j <= n_sds_i; j++) {
    if (w_from_p(j, n_s, n_p, w_shell) > w_max) {continue;}
    int j_min = j_min_from_p(n_s, n_p, j);
    float mj = m_from_p(j, n_s, n_p, jz_shell);
    if ((mj < mj_min) || (mj > mj_max)) {continue;}
    int i_mj = mj - mj_min;
    int i_parity = (parity_from_p(j, n_s, n_p, l_shell) + 1)/2;
    if (a0_list_i[i_parity + 2*i_mj] == NULL) {
      a0_list_i[i_parity + 2*i_mj] = create_wf_node(j, NULL);
    } else {
      wf_append(a0_list_i[i_parity + 2*i_mj], j);
    }

    for (int a = j_min - 1; a < n_s; a++) {
      int phase;
      int pn = a_op(n_s, n_p, j, a + 1, &phase, j_min);
      if (pn == 0) {continue;}

      if (a1_list_i[i_parity + 2*(i_mj + num_mj*a)] == NULL) {
        a1_list_i[i_parity + 2*(i_mj + num_mj*a)] = create_sd_node(j, pn, phase, NULL);
      } else {
        sd_append(a1_list_i[i_parity + 2*(i_mj + num_mj*a)], j, pn, phase);
      }
    }
  }

  return;
}

void build_one_body_jumps_f_trunc(int n_s, int n_p, float mj_min, float mj_max, int num_mj, int n_sds_f, int n_sds_int, int* a1_array_f, sd_list** a1_list_f, int* jz_shell, int* l_shell, int* w_shell, int w_max) {
  
  for (int j = 1; j <= n_sds_f; j++) {
     if (w_from_p(j, n_s, n_p, w_shell) > w_max) {continue;}
     int j_min = j_min_from_p(n_s, n_p, j);

    for (int a = j_min - 1; a < n_s; a++) {
      int phase;
      int pn = a_op(n_s, n_p, j, a + 1, &phase, j_min);
      if (pn == 0) {continue;}
      float mj = m_from_p(pn, n_s, n_p - 1, jz_shell);
      if ((mj > mj_max) || (mj < mj_min)) {continue;}
      int i_mj = mj - mj_min;
      int i_parity = (parity_from_p(pn, n_s, n_p - 1, l_shell) + 1)/2;

      a1_array_f[(pn - 1) + a*n_sds_int] = j*phase;
      if (a1_list_f[i_parity + 2*(i_mj + num_mj*a)] == NULL) {
       a1_list_f[i_parity + 2*(i_mj + num_mj*a)] = create_sd_node(pn, j, phase, NULL);
      } else {
        sd_append(a1_list_f[i_parity + 2*(i_mj + num_mj*a)], pn, j, phase);
      }

    }
  }

  return;
}


void build_one_body_jumps_i(int n_s, int n_p, float mj_min, float mj_max, int num_mj, int n_sds_i, wf_list** a0_list_i, sd_list** a1_list_i,  int* jz_shell, int* l_shell) {

  for (int j = 1; j <= n_sds_i; j++) {
    int j_min = j_min_from_p(n_s, n_p, j);
    float mj = m_from_p(j, n_s, n_p, jz_shell);
    if ((mj < mj_min) || (mj > mj_max)) {continue;}
    int i_mj = mj - mj_min;
    int i_parity = (parity_from_p(j, n_s, n_p, l_shell) + 1)/2;
    if (a0_list_i[i_parity + 2*i_mj] == NULL) {
      a0_list_i[i_parity + 2*i_mj] = create_wf_node(j, NULL);
    } else {
      wf_append(a0_list_i[i_parity + 2*i_mj], j);
    }

    for (int a = j_min - 1; a < n_s; a++) {
      int phase;
      int pn = a_op(n_s, n_p, j, a + 1, &phase, j_min);
      if (pn == 0) {continue;}
      if (a1_list_i[i_parity + 2*(i_mj + num_mj*a)] == NULL) {
        a1_list_i[i_parity + 2*(i_mj + num_mj*a)] = create_sd_node(j, pn, phase, NULL);
      } else {
        sd_append(a1_list_i[i_parity + 2*(i_mj + num_mj*a)], j, pn, phase);
      }
    }
  }

  return;
}

void build_one_body_jumps_f(int n_s, int n_p, float mj_min, float mj_max, int num_mj, int n_sds_f, int n_sds_int, int* a1_array_f, sd_list** a1_list_f, int* jz_shell, int* l_shell) {
  
  for (int j = 1; j <= n_sds_f; j++) {
    int j_min = j_min_from_p(n_s, n_p, j);
    float mj = m_from_p(j, n_s, n_p, jz_shell);
    if ((mj > mj_max) || (mj < mj_min)) {continue;}
    int i_mj = mj - mj_min;
    int i_parity = (parity_from_p(j, n_s, n_p, l_shell) + 1)/2;

    for (int a = j_min - 1; a < n_s; a++) {
      int phase;
      int pn = a_op(n_s, n_p, j, a + 1, &phase, j_min);
      if (pn == 0) {continue;}
      a1_array_f[(pn - 1) + a*n_sds_int] = j*phase;
      //float mj = m_from_p(pn, n_s, n_p - 1, jz_shell);
      //if ((mj > mj_max) || (mj < mj_min)) {continue;}
      //int i_mj = mj - mj_min;
      //int i_parity = (parity_from_p(pn, n_s, n_p - 1, l_shell) + 1)/2;
      if (a1_list_f[i_parity + 2*(i_mj + num_mj*a)] == NULL) {
        a1_list_f[i_parity + 2*(i_mj + num_mj*a)] = create_sd_node(pn, j, phase, NULL);
      } else {
        sd_append(a1_list_f[i_parity + 2*(i_mj + num_mj*a)], pn, j, phase);
      }
    }
  }

  return;
}

void trace_1body_t0_nodes_spec(int a, int b, int num_mj, int n_sds_int, int* a1_array_f, sde_list** a1_list_i, wfe_list** a0_list_i, wfnData* wd, int i_op, eigen_list *transition, double* density, int n_q_spec_min, int n_spec_bins) {

  int ns = wd->n_shells;

  for (int ipar = 0; ipar <= 1; ipar++) {
    for (int imj = 0; imj < num_mj; imj++) {
      sde_list* node1 = a1_list_i[ipar + 2*(imj + num_mj*b)];
      while (node1 != NULL) {
        unsigned int ppn = node1->pn;
        unsigned int ppi = node1->pi;
        int phase1 = node1->phase;
        int ppf = a1_array_f[(ppn - 1) + n_sds_int*a];
        if (ppf == 0) {node1 = node1->next; continue;}
        int phase2 = 1;
        if (ppf < 0) {
          ppf *= -1;
 	  phase2 = -1;
        }
        int n_quanta_1 = node1->n_quanta;
        node1 = node1->next;
        wfe_list* node2 = a0_list_i[ipar + 2*(num_mj - imj - 1)];
        while (node2 != NULL) {
  	  int pn = node2->p;
	  int index_i = -1;
	  int index_f = -1;
          int i_spec = n_quanta_1 + node2->n_quanta - n_q_spec_min;
	  if (i_op == 0) {
	    unsigned int p_hash_i = ppi + wd->n_sds_p_i*(pn % HASH_SIZE);
	    wh_list* node3 = wd->wh_hash_i[p_hash_i];
	    while (node3 != NULL) {
	      if ((pn == node3->pn) && (ppi == node3->pp)) {
	        index_i = node3->index;
	        break;
	      }
	      node3 = node3->next;
	    }
	    if (index_i < 0) {node2 = node2->next; continue;}

	    unsigned int p_hash_f = ppf + wd->n_sds_p_f*(pn % HASH_SIZE);
	    node3 = wd->wh_hash_f[p_hash_f];
	    while (node3 != NULL) {
	      if ((pn == node3->pn) && (ppf == node3->pp)) {
	        index_f = node3->index;
	        break;
	      }
	      node3 = node3->next;
	    }
	    if (index_f < 0) {node2 = node2->next; continue;}
            eigen_list* eig_pair = transition;
            int i_trans = 0;
            while (eig_pair != NULL) {
              int psi_i = eig_pair->eig_i;
              int psi_f = eig_pair->eig_f;
	      density[i_spec + n_spec_bins*i_trans] += wd->bc_i[psi_i + wd->n_eig_i*index_i]*wd->bc_f[psi_f + wd->n_eig_f*index_f]*phase1*phase2;
              i_trans++;
              eig_pair = eig_pair->next;
            }
	  } else {
	    unsigned int p_hash_i = pn + wd->n_sds_p_i*(ppi % HASH_SIZE);
	    wh_list* node3 = wd->wh_hash_i[p_hash_i];
	    while (node3 != NULL) {
	      if ((pn == node3->pp) && (ppi == node3->pn)) {
	        index_i = node3->index;
	        break;
	      }
	      node3 = node3->next;
	    }
	    if (index_i < 0) {node2 = node2->next; continue;}

	    unsigned int p_hash_f = pn + wd->n_sds_p_f*(ppf % HASH_SIZE);
	    node3 = wd->wh_hash_f[p_hash_f];
	    while (node3 != NULL) {
	      if ((pn == node3->pp) && (ppf == node3->pn)) {
	        index_f = node3->index;
	        break;
	      }
	      node3 = node3->next;
	    }
	    if (index_f < 0) {node2 = node2->next; continue;}
            eigen_list* eig_pair = transition;
            int i_trans = 0;
            while (eig_pair != NULL) {
              int psi_i = eig_pair->eig_i;
              int psi_f = eig_pair->eig_f;
	      density[i_spec + n_spec_bins*i_trans] += wd->bc_i[psi_i + wd->n_eig_i*index_i]*wd->bc_f[psi_f + wd->n_eig_f*index_f]*phase1*phase2;
              i_trans++;
              eig_pair = eig_pair->next;
            }
	  } 
	  node2 = node2->next;
        } 
      }
    }
  }
  return;
}

void trace_1body_t2_nodes_spec(int a, int b, int num_mj, sde_list** a1_list_i, sde_list** a1_list_f, wfnData* wd, int i_op, eigen_list *transition, double* density, int n_q_spec_min, int n_spec_bins) {
/* 

*/
  int ns = wd->n_shells;
  for (int ipar = 0; ipar <= 1; ipar++) {
    for (int imj = 0; imj < num_mj; imj++) {
      sde_list* node1 = a1_list_i[ipar + 2*(imj + num_mj*b)];
      // Loop over final states resulting from 2x a_op
      while (node1 != NULL) {
        unsigned int ppf = node1->pn; // Get final state p_f
        unsigned int ppi = node1->pi; // Get initial state p_i
        int phase1 = node1->phase;
        // Get list of n_i associated to p_i
        sde_list* node2 = a1_list_f[ipar + 2*(num_mj - imj - 1 + num_mj*a)]; //hash corresponds to a_op operators
        // Loop over n_f  
        //int m_pf = m_from_p(ppf, ns, npp, wd->jz_shell);
        while (node2 != NULL) {
          unsigned int pni = node2->pi;
          unsigned int pnf = node2->pn;
          int phase2 = node2->phase;
          int i_spec = node1->n_quanta + node2->n_quanta - n_q_spec_min;
          int index_i = -1;
          int index_f = -1;
          if (i_op == 0) {
	    unsigned int p_hash_i = ppi + wd->n_sds_p_i*(pni % HASH_SIZE);
	    wh_list* node3 = wd->wh_hash_i[p_hash_i];
	    while (node3 != NULL) {
	      if ((pni == node3->pn) && (ppi == node3->pp)) {
	        index_i = node3->index;
	        break;
	      }
	      node3 = node3->next;
	    }
	    if (index_i < 0) {node2 = node2->next; continue;}

	    unsigned int p_hash_f = ppf + wd->n_sds_p_f*(pnf % HASH_SIZE);
	    node3 = wd->wh_hash_f[p_hash_f];
	    while (node3 != NULL) {
	      if ((pnf == node3->pn) && (ppf == node3->pp)) {
	        index_f = node3->index;
	        break;
	      }
	      node3 = node3->next;
	    }
	    if (index_f < 0) {node2 = node2->next; continue;}
            eigen_list* eig_pair = transition;
            int i_trans = 0;
            while (eig_pair != NULL) {
              int psi_i = eig_pair->eig_i;
              int psi_f = eig_pair->eig_f;
	      density[i_spec + n_spec_bins*i_trans] += wd->bc_i[psi_i + wd->n_eig_i*index_i]*wd->bc_f[psi_f + wd->n_eig_f*index_f]*phase1*phase2;
              i_trans++;
              eig_pair = eig_pair->next;
            }
	  } else {
	    unsigned int p_hash_i = pni + wd->n_sds_p_i*(ppi % HASH_SIZE);
	    wh_list* node3 = wd->wh_hash_i[p_hash_i];
	    while (node3 != NULL) {
	      if ((pni == node3->pp) && (ppi == node3->pn)) {
	        index_i = node3->index;
	        break;
	      }
	      node3 = node3->next;
	    }
	    if (index_i < 0) {node2 = node2->next; continue;}

	    unsigned int p_hash_f = pnf + wd->n_sds_p_f*(ppf % HASH_SIZE);
	    node3 = wd->wh_hash_f[p_hash_f];
	    while (node3 != NULL) {
	      if ((pnf == node3->pp) && (ppf == node3->pn)) {
	        index_f = node3->index;
	        break;
	      }
	      node3 = node3->next;
	    }
	    if (index_f < 0) {node2 = node2->next; continue;}
            eigen_list* eig_pair = transition;
            int i_trans = 0;
            while (eig_pair != NULL) {
              int psi_i = eig_pair->eig_i;
              int psi_f = eig_pair->eig_f;
	      density[i_spec + n_spec_bins*i_trans] += wd->bc_i[psi_i + wd->n_eig_i*index_i]*wd->bc_f[psi_f + wd->n_eig_f*index_f]*phase1*phase2;
              i_trans++;
              eig_pair = eig_pair->next;
            }
	  }
          node2 = node2->next;
        }
        node1 = node1->next;
      }
    }  
  }
  return;
}


void trace_1body_t0_nodes(int a, int b, int num_mj, int n_sds_int, int* a1_array_f, sd_list** a1_list_i, wf_list** a0_list_i, wfnData* wd, int i_op, eigen_list* transition, double* density) {

  int ns = wd->n_shells;

  for (int ipar1 = 0; ipar1 <= 1; ipar1++) {
    for (int imj = 0; imj < num_mj; imj++) {
      sd_list* node1 = a1_list_i[ipar1 + 2*(imj + num_mj*b)];
      while (node1 != NULL) {
        unsigned int ppn = node1->pn;
        unsigned int ppi = node1->pi;
        int phase1 = node1->phase;
        int ppf = a1_array_f[(ppn - 1) + n_sds_int*a];
        if (ppf == 0) {node1 = node1->next; continue;}
        int phase2 = 1;
        node1 = node1->next;
        if (ppf < 0) {
        ppf *= -1;
 	phase2 = -1;
        }
	int ipar2;
        if (wd->parity_i == '+') {
	  if (ipar1 == 0) {
		  ipar2 = 0;
	  } else {
		  ipar2 = 1;
	  }
	} else if (wd->parity_i == '-') {
		if (ipar1 == 0) {
			ipar2 = 1;
		} else {
			ipar2 = 0;
		}
	} else {printf("Parity error\n"); exit(0);}

        wf_list* node2 = a0_list_i[ipar2 + 2*(num_mj - imj - 1)];
        while (node2 != NULL) {
	  int pn = node2->p;
   	  int index_i = -1;
	  int index_f = -1;

	  if (i_op == 0) {
	    unsigned int p_hash_i = ppi + wd->n_sds_p_i*(pn % HASH_SIZE);
	    wh_list* node3 = wd->wh_hash_i[p_hash_i];
	    while (node3 != NULL) {
	      if ((pn == node3->pn) && (ppi == node3->pp)) {
	        index_i = node3->index;
	        break;
	      }
	      node3 = node3->next;
	    }
	    if (index_i < 0) {node2 = node2->next; continue;}

	    unsigned int p_hash_f = ppf + wd->n_sds_p_f*(pn % HASH_SIZE);
	    node3 = wd->wh_hash_f[p_hash_f];
	    while (node3 != NULL) {
	      if ((pn == node3->pn) && (ppf == node3->pp)) {
	        index_f = node3->index;
	        break;
	      }
	      node3 = node3->next;
	    }
	    if (index_f < 0) {node2 = node2->next; continue;}
            eigen_list* eig_pair = transition;
            int i_trans = 0;
            while (eig_pair != NULL) {
              int psi_i = eig_pair->eig_i;
              int psi_f = eig_pair->eig_f;
	      density[i_trans] += wd->bc_i[psi_i + wd->n_eig_i*index_i]*wd->bc_f[psi_f + wd->n_eig_f*index_f]*phase1*phase2;
              i_trans++;
              eig_pair = eig_pair->next;
            }
	  } else {
	    unsigned int p_hash_i = pn + wd->n_sds_p_i*(ppi % HASH_SIZE);
	    wh_list* node3 = wd->wh_hash_i[p_hash_i];
	    while (node3 != NULL) {
	      if ((pn == node3->pp) && (ppi == node3->pn)) {
	        index_i = node3->index;
	        break;
	      }
	      node3 = node3->next;
	    }
	    if (index_i < 0) {node2 = node2->next; continue;}

	    unsigned int p_hash_f = pn + wd->n_sds_p_f*(ppf % HASH_SIZE);
	    node3 = wd->wh_hash_f[p_hash_f];
	    while (node3 != NULL) {
	      if ((pn == node3->pp) && (ppf == node3->pn)) {
	        index_f = node3->index;
	        break;
	      }
	      node3 = node3->next;
	    }
	    if (index_f < 0) {node2 = node2->next; continue;}
            eigen_list* eig_pair = transition;
            int i_trans = 0;
            while (eig_pair != NULL) {
              int psi_i = eig_pair->eig_i;
              int psi_f = eig_pair->eig_f;
	      density[i_trans] += wd->bc_i[psi_i + wd->n_eig_i*index_i]*wd->bc_f[psi_f + wd->n_eig_f*index_f]*phase1*phase2;
              i_trans++;
              eig_pair = eig_pair->next;
            }
	  } 
	  node2 = node2->next;
        } 
      }
    }
  }
  return;
}

void trace_1body_t2_nodes(int a, int b, int num_mj_1, float mj_min_1, int num_mj_2, float mj_min_2, sd_list** a1_list_i, sd_list** a1_list_f, wfnData* wd, int i_op, eigen_list *transition, double* density) {
/* 

*/
  int ns = wd->n_shells;
  for (int imj1 = 0; imj1 < num_mj_1; imj1++) {
    float mj1 = imj1 + mj_min_1;
    for (int imj2 = 0; imj2 < num_mj_2; imj2++) {
      float mj2 = imj2 + mj_min_2;
      if ((mj1 + mj2 != 0) && (mj1 + mj2 != 0.5)) {continue;}
      for (int ipar1 = 0; ipar1 <= 1; ipar1++) {
     //   for (int ipar2 = 0; ipar2 <= 1; ipar2++){
        int ipar2;
	if (wd->parity_i == '+') {
	  if (ipar1 == 0) {
		  ipar2 = 0;
	  } else {
		  ipar2 = 1;
	  }
	} else if (wd->parity_i == '-') {
		if (ipar1 == 0) {
			ipar2 = 1;
		} else {
			ipar2 = 0;
		}
	} else {printf("Parity error\n"); exit(0);}
        sd_list* node1 = a1_list_i[ipar1 + 2*(imj1 + num_mj_1*b)];
        // Loop over final states resulting from 2x a_op
        while (node1 != NULL) {
          unsigned int ppf = node1->pn; // Get final state p_f
          unsigned int ppi = node1->pi; // Get initial state p_i
          int phase1 = node1->phase;
		//if (ipar == 0) {ipar2 = 0;} else {ipar2 = 1;}
          sd_list* node2 = a1_list_f[ipar2 + 2*(imj2 + num_mj_2*a)]; //hash corresponds to a_op operators

        // Loop over n_f  
        //int m_pf = m_from_p(ppf, ns, npp, wd->jz_shell);
        while (node2 != NULL) {
          unsigned int pni = node2->pi;
          unsigned int pnf = node2->pn;
          int phase2 = node2->phase;
          int index_i = -1;
          int index_f = -1;  
          if (i_op == 0) {
	    unsigned int p_hash_i = ppi + wd->n_sds_p_i*(pni % HASH_SIZE);
	    wh_list* node3 = wd->wh_hash_i[p_hash_i];
	    while (node3 != NULL) {
	      if ((pni == node3->pn) && (ppi == node3->pp)) {
	        index_i = node3->index;
	        break;
	      }
	      node3 = node3->next;
	    }
	    if (index_i < 0) {node2 = node2->next; continue;}

	    unsigned int p_hash_f = ppf + wd->n_sds_p_f*(pnf % HASH_SIZE);
	    node3 = wd->wh_hash_f[p_hash_f];
	    while (node3 != NULL) {
	      if ((pnf == node3->pn) && (ppf == node3->pp)) {
	        index_f = node3->index;
	        break;
	      }
	      node3 = node3->next;
	    }
	    if (index_f < 0) {node2 = node2->next; continue;}
            eigen_list* eig_pair = transition;
            int i_trans = 0;
            while (eig_pair != NULL) {
              int psi_i = eig_pair->eig_i;
              int psi_f = eig_pair->eig_f;
	      density[i_trans] += wd->bc_i[psi_i + wd->n_eig_i*index_i]*wd->bc_f[psi_f + wd->n_eig_f*index_f]*phase1*phase2;
              i_trans++;
              eig_pair = eig_pair->next;
            }
	  } else {
	    unsigned int p_hash_i = pni + wd->n_sds_p_i*(ppi % HASH_SIZE);
	    wh_list* node3 = wd->wh_hash_i[p_hash_i];
	    while (node3 != NULL) {
	      if ((pni == node3->pp) && (ppi == node3->pn)) {
	        index_i = node3->index;
	        break;
	      }
	      node3 = node3->next;
	    }
	    if (index_i < 0) {node2 = node2->next; continue;}

	    unsigned int p_hash_f = pnf + wd->n_sds_p_f*(ppf % HASH_SIZE);
	    node3 = wd->wh_hash_f[p_hash_f];
	    while (node3 != NULL) {
	      if ((pnf == node3->pp) && (ppf == node3->pn)) {
	        index_f = node3->index;
	        break;
	      }
	      node3 = node3->next;
	    }
	    if (index_f < 0) {node2 = node2->next; continue;}
            eigen_list* eig_pair = transition;
            int i_trans = 0;
            while (eig_pair != NULL) {
              int psi_i = eig_pair->eig_i;
              int psi_f = eig_pair->eig_f;
	      density[i_trans] += wd->bc_i[psi_i + wd->n_eig_i*index_i]*wd->bc_f[psi_f + wd->n_eig_f*index_f]*phase1*phase2;
              i_trans++;
              eig_pair = eig_pair->next;
            }
	  } 
          node2 = node2->next;
	}
        node1 = node1->next;
	}
//	}
      }
    }  
  }
  return;
}

