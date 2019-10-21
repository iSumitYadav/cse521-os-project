#define fp_coeff (1<<14)


int inline int_to_fixed_point(int y_int){
  return y_int * fp_coeff;
}

int inline fixed_point_to_int_round(int x_fp){
  if(x_fp >= 0){
      return (x_fp + fp_coeff / 2) / fp_coeff;
    }else{
      return (x_fp - fp_coeff / 2) / fp_coeff;
    }
}

int inline fixed_point_to_int(int x_fp){
  return x_fp / fp_coeff;
}

int inline add_fixed_point(int x_fp, int y_fp){
  return x_fp + y_fp;
}

int inline sub_fixed_point(int x_fp, int y_fp){
  return x_fp - y_fp;
}

int inline add_fixed_point_int(int x_fp, int y_int){
  return x_fp + int_to_fixed_point(y_int);
}

int inline sub_fixed_point_int(int x_fp, int y_int){
  return x_fp - int_to_fixed_point(y_int);
}

int inline multiply_fixed_point(int x_fp, int y_fp){
  return ((int64_t) x_fp) * y_fp / fp_coeff;
}

int inline multiply_fixed_point_int(int x_fp, int y_int){
  return x_fp * y_int;
}

int inline divide_fixed_point(int x_fp, int y_fp){
  return ((int64_t) x_fp) * fp_coeff / y_fp;
}

int inline divide_fixed_point_int(int x_fp, int y_int){
  return x_fp / y_int;
}