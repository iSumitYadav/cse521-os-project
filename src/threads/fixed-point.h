#define fp_coeff 1<<14


int integer_to_fixed_point(int y_int){
	return (y_int * fp_coeff);
}

int fixed_point_to_integer(int x_fp){
	return (x_fp / fp_coeff);
}

int fixed_point_to_integer_round(int x_fp){
	if(x_fp >= 0){
		return ((x_fp + fp_coeff/2) / fp_coeff);
	}else{
		return ((x_fp - fp_coeff/2) / fp_coeff);
	}
}

int add_fixed_point(int x_fp, int y_fp){
	return (x_fp + y_fp);
}

int subtract_fixed_point(int x_fp, int y_fp){
	return (x_fp - y_fp);
}

int add_fixed_point_int(int x_fp, int y_int){
	return (x_fp + (y_int * fp_coeff));
}

int subtract_fixed_point_int(int x_fp, int y_int){
	return (x_fp - (y_int * fp_coeff));
}

int multiply_fixed_point_int(int x_fp, int y_int){
	return (x_fp * y_int);
}

int divide_fixed_point_int(int x_fp, int y_int){
	return (x_fp / y_int);
}

int multiply_fixed_point(int x_fp, int y_fp){
	return ((int64_t)x_fp) * y_fp / fp_coeff;
}

int divide_fixed_point(int x_fp, int y_fp){
	return ((int64_t)x_fp) * fp_coeff / y_fp;
}