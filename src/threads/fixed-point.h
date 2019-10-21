#define F 16384 // F = 2^14
#include <stdint.h>
// i is integer
// a and b are fixed point numbers in 17.14 format
//Integer is passed as second argument
 int inttofp(int i);
 int fpadd(int a, int b);
 int fpsub(int a, int b);
 int fpintadd(int a, int i);
 int fpintsub(int a, int i);
 int fpmul(int a,int b);
 int fpdiv(int a,int b);
 int fpintmul(int a, int i);
 int fpintdiv(int a, int i);
 int fptoint(int a);
 int fptointround(int a);

  int inttofp(int i)
  {
  return i * F;
  }
  int fpadd(int a, int b)
  {
  	return a + b;
  }
  int fpsub(int a, int b)
  {
  	return a - b;
  }
  int fpintadd(int a, int i)
  {
  	return a + inttofp(i);
  }
  int fpintsub(int a, int i)
  {
  	return a - inttofp(i);
  }
  int fpmul(int a,int b)
  {
  	return ((int64_t) a)* b / F;
  }
  int fpdiv(int a, int b)
  {
  	return ((int64_t) a)* F / b;
  }
  int fpintmul(int a, int i)
  {
    return a * i;
  }
  int fpintdiv(int a, int i)
  {
    return a / i;
  }
  int fptoint(int a)
  {
    return a / F;
  }
  int fptointround(int a)
  {
  if(a >= 0)
      return (a + F/2) / F;
   else
      return (a - F/2) / F; 
  }